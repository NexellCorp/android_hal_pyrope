/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */


#include "sw_profiling.h"

#include "mali_instrumented_context.h"
#include "mali_counters.h"

#include "shared/mali_linked_list.h"

/* Defaults to a 2 MB buffer  */
static int buffer_size = 2*1024*1024/sizeof(profiling_event);

/* Restrict buffer size to 1 GB */
static const int max_buffer_size = 1*1024*1024*1024/sizeof(profiling_event);

MALI_EXPORT
mali_err_code _mali_profiling_init(mali_instrumented_context *ctx)
{
	profiling_buffer *_mali_profiling_buffer;
	mali_instrumented_tls_struct *tls_struct = _instrumented_get_tls_with_context(ctx);
	if ( NULL == tls_struct ) return MALI_ERR_FUNCTION_FAILED;
	_mali_profiling_buffer = &(tls_struct->_mali_profiling_buffer);

	tls_struct->realloc_count = 0;
	buffer_size = (int)_mali_sys_config_string_get_s64("MALI_PROFILING_BUFFER_SIZE", buffer_size, 0, max_buffer_size);

	/* Early-out before allocating memory if we're not doing any profiling (yet). */
	if (! ctx->sampling) MALI_SUCCESS;

	/* Early-out if there is already a buffer present (i.e. there exists two contexts in
	   the same thread). This prevents a memory leak. */
	if ( NULL != _mali_profiling_buffer->buffer ) MALI_SUCCESS;

	_mali_profiling_buffer->buffer = _mali_sys_malloc(buffer_size*sizeof(profiling_event));

	MALI_CHECK(_mali_profiling_buffer->buffer != NULL,  MALI_ERR_OUT_OF_MEMORY);

	_mali_profiling_buffer->size = buffer_size;

	_mali_profiling_buffer->current = _mali_profiling_buffer->buffer;

	MALI_SUCCESS;
}

MALI_EXPORT
 void clear_profiling_buffer(profiling_buffer *prof_buffer)
{
	MALI_DEBUG_ASSERT_POINTER(prof_buffer);

	if(NULL != prof_buffer->buffer)
	{
		_mali_sys_free(prof_buffer->buffer);
		prof_buffer->buffer = NULL;
	}
	prof_buffer->current = NULL;
	prof_buffer->size = 0;

	/* Don't need to delete the buffer itself as it is part of a mali_instrumented_tls_struct
	   structure which has its own destructor */
}

/**
 * Find the elapsed time between an "enter" event and a "leave" event.
 *
 * Tracks changes to all enter/leave events' values to detect wrap-arounds
 * in value.
 *
 * \param enter_event Event to start searching from, must be with type PROFILING_EVENT_ENTER
 * \return The time interval if successful, zero if we didn't find a matching LEAVE.
 */
MALI_STATIC s64 find_interval(profiling_event *enter_event)
{
	profiling_event *i;
	u32 cur_time = enter_event->value;
	u32 start_time = enter_event->value;
	s64 accum_time = 0;

	profiling_buffer *_mali_profiling_buffer;
	mali_instrumented_tls_struct *tls_struct = _instrumented_get_tls();
	if ( NULL == tls_struct ) return 0;
	_mali_profiling_buffer = &(tls_struct->_mali_profiling_buffer);

	/* We loop over all events between the ENTER and LEAVE events, and check their
	   timestamp values so we can detect overflow. */
	for (i = enter_event + 1; i < _mali_profiling_buffer->current; ++i)
	{
		/* Ignore COUNT events, as their values are not time. */
		if (i->type == PROFILING_EVENT_COUNT) continue;

		if (i->value < cur_time)
		{
			accum_time += (0x100000000LL - start_time);
			start_time = 0;
		}

		cur_time = i->value;

		/* Found the matching LEAVE event? */
		if (i->counter_id == enter_event->counter_id)
		{
			accum_time += cur_time - start_time;
			i->type = PROFILING_EVENT_NONE; /* set LEAVE event's type to NONE to detect spurios LEAVEs */
			break;
		}
	}

	if (i == _mali_profiling_buffer->current)
	{
		MALI_DEBUG_PRINT(0, ("Failed to find a LEAVE event for ENTER event with cid %d", enter_event->counter_id));
		return 0;
	}

	return accum_time;
}

MALI_EXPORT
void _mali_profiling_analyze(mali_instrumented_frame *frame)
{
	profiling_event *i;

	profiling_buffer *_mali_profiling_buffer;
	mali_instrumented_tls_struct *tls_struct = _instrumented_get_tls();
	if ( NULL == tls_struct ) return;
	_mali_profiling_buffer = &(tls_struct->_mali_profiling_buffer);

	if (NULL == frame) return;

	for ( i = _mali_profiling_buffer->buffer; i < _mali_profiling_buffer->current; ++i)
	{
		switch (i->type)
		{
		case PROFILING_EVENT_COUNT:
			_instrumented_add_to_counter(i->counter_id, frame, i->value);
			break;

		case PROFILING_EVENT_ENTER:
			_instrumented_add_to_counter(i->counter_id, frame,
				(s64)(find_interval(i) * (_mali_sys_get_timestamp_scaling_factor() * 1000000.0)));
			break;

		case PROFILING_EVENT_LEAVE:
			MALI_DEBUG_PRINT(0, ("Unexpected LEAVE event, counter_id %d. LEAVE without ENTER?\n", i->counter_id));
			break;
		}
	}

	if (tls_struct->realloc_count > 0)
	{
		_instrumented_frame_set_realloced(frame);
	}

	_mali_profiling_buffer->current = _mali_profiling_buffer->buffer;
	tls_struct->realloc_count = 0;
}

MALI_EXPORT
mali_bool _mali_profiling_grow_buffer(void)
{
	profiling_event *new_buffer = NULL;
	u32 new_size;
	u32 current_offset;
	mali_instrumented_context *ctx = _instrumented_get_context();

	profiling_buffer *_mali_profiling_buffer;
	mali_instrumented_tls_struct *tls_struct = _instrumented_get_tls_with_context(ctx);
	if ( NULL == tls_struct ) return 0;
	_mali_profiling_buffer = &(tls_struct->_mali_profiling_buffer);

	new_size = _mali_profiling_buffer->size + buffer_size;
	current_offset = _mali_profiling_buffer->current - _mali_profiling_buffer->buffer;

	/* If we're not collecting any performance data we can early-out here, before
	 * allocating memory for the profiling buffer.
	 */
	if ( !ctx || !ctx->sampling ) return MALI_FALSE;

	new_buffer = _mali_sys_realloc(_mali_profiling_buffer->buffer, new_size * sizeof(profiling_event));
	MALI_CHECK(NULL != new_buffer, MALI_FALSE);
	_mali_profiling_buffer->buffer = new_buffer;
	_mali_profiling_buffer->current = _mali_profiling_buffer->buffer + current_offset;
	_mali_profiling_buffer->size = new_size;

	return MALI_TRUE;
}

MALI_EXPORT
void _profiling_enter(u32 counter_id)
{
	u32 time;
	profiling_buffer *_mali_profiling_buffer;
	mali_instrumented_context *ctx = _instrumented_get_context();
	mali_instrumented_tls_struct *tls_struct = _instrumented_get_tls();
	if ( NULL == tls_struct ) return;
	if (!ctx || !ctx->sampling) return;
	_mali_profiling_buffer = &(tls_struct->_mali_profiling_buffer);


	/* This test will return true if:
	 * - There is an allocated buffer, and it has to be grown to fit the profiling data
	 * - No buffer is allocated for this thread (both pointers are NULL)
	 */
	if (MALI_STATIC_CAST(unsigned int)(_mali_profiling_buffer->current - _mali_profiling_buffer->buffer)
		>= _mali_profiling_buffer->size)
	{
		if (!_mali_profiling_grow_buffer()) return;
		/* Tell the instrumented frame that the profiling buffer has been realloced. */
		tls_struct->realloc_count++;
	}

	time = _mali_sys_get_timestamp();

	_mali_profiling_buffer->current->type = PROFILING_EVENT_ENTER;
	_mali_profiling_buffer->current->counter_id = counter_id;
	_mali_profiling_buffer->current->value = time;
	++_mali_profiling_buffer->current;
}

MALI_EXPORT
void _profiling_leave(u32 counter_id)
{
	u32 time;
	profiling_buffer *_mali_profiling_buffer;
	mali_instrumented_tls_struct *tls_struct = _instrumented_get_tls();
	mali_instrumented_context *ctx = _instrumented_get_context();
	if ( NULL == tls_struct ) return;
	if (!ctx || !ctx->sampling) return;
	_mali_profiling_buffer = &(tls_struct->_mali_profiling_buffer);

	time = _mali_sys_get_timestamp();

	/* This test will return true if:
	 * - There is an allocated buffer, and it has to be grown to fit the profiling data
	 * - No buffer is allocated for this thread (both pointers are NULL)
	 */
	if (MALI_STATIC_CAST(unsigned int)(_mali_profiling_buffer->current - _mali_profiling_buffer->buffer)
		>= _mali_profiling_buffer->size)
	{
		if (!_mali_profiling_grow_buffer()) return;
		/* Tell the instrumented frame that the profiling buffer has been realloced. */
		tls_struct->realloc_count++;
	}
	_mali_profiling_buffer->current->type = PROFILING_EVENT_LEAVE;
	_mali_profiling_buffer->current->counter_id = counter_id;
	_mali_profiling_buffer->current->value = time;
	++_mali_profiling_buffer->current;
}

/**
 * Count a profiling event.
 *
 * Adjusts the counter's value by +increment.
 * @param counter_id id number of the counter
 */
MALI_EXPORT
void _profiling_count(u32 counter_id, u32 increment)
{
	profiling_buffer *_mali_profiling_buffer;
	mali_instrumented_tls_struct *tls_struct = _instrumented_get_tls();
	if ( NULL == tls_struct ) return;
	_mali_profiling_buffer = &(tls_struct->_mali_profiling_buffer);

	/* This test will return true if:
	 * - There is an allocated buffer, and it has to be grown to fit the profiling data
	 * - No buffer is allocated for this thread (both pointers are NULL)
	 */
	if (MALI_STATIC_CAST(unsigned int)(_mali_profiling_buffer->current - _mali_profiling_buffer->buffer)
		>= _mali_profiling_buffer->size)
	{
		if (!_mali_profiling_grow_buffer()) return;
		/* Tell the instrumented frame that the profiling buffer has been realloced. */
		tls_struct->realloc_count++;
	}
	_mali_profiling_buffer->current->type = PROFILING_EVENT_COUNT;
	_mali_profiling_buffer->current->counter_id = counter_id;
	_mali_profiling_buffer->current->value = increment;
	++_mali_profiling_buffer->current;
}
