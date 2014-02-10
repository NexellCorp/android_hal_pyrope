/*
 * This confidential and proprietary software may be used only as
 * authorized by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <mali_system.h>

#include <shared/m200_gp_frame_builder.h>
#include <shared/m200_gp_frame_builder_struct.h>
#include <shared/m200_gp_frame_builder_inlines.h>
#include <shared/m200_gp_frame_builder_dependency.h>

#include <util/mali_math.h>
#include <base/mali_debug.h>
#include <base/mali_macros.h>
#include <base/mali_byteorder.h>
#include <base/mali_memory.h>
#include <base/mali_profiling.h>
#include <regs/MALIGP2/mali_gp_core.h>
#include <regs/MALI200/mali_rsw.h>

#include "m200_regs.h"
#include "m200_frame.h"

#include <shared/mali_surface.h>
#include "shared/m200_incremental_rendering.h"
#include "m200_readback_util.h"

#include "mali_counters.h"
#include "mali_log.h"
#include "mali_instrumented_context.h"
#include "mali_pp_instrumented.h"
#include "mali_gp_instrumented.h"

#if MALI_INSTRUMENTED
#include <base/instrumented/mali_gp_instrumented_internal.h>
#include <base/common/instrumented/base_common_pp_instrumented.h>
#endif  /* MALI_INSTRUMENTED */

#if MALI_TIMELINE_PROFILING_ENABLED
#include <base/mali_profiling.h>
#endif

#if MALI_FRAMEBUFFER_DUMP_ENABLED
#include <shared/m200_fbdump.h>
#endif

/* the number of flushes between resets before incremental rendering is requested */
#ifdef HARDWARE_ISSUE_3526
/* make incremental rendering trigger less often to reduce the chance of
 * rendering-errors due to this issue. Removing it completely is not
 * possible.
 */
#define INCREMENTAL_RENDERING_THRESHOLD 2000
#else
#define INCREMENTAL_RENDERING_THRESHOLD 50
#endif

#if !defined(HARDWARE_ISSUE_3251)
/* default heap size must be 1024 aligned as this is the maximum tile list block bytesize */
#ifndef __SYMBIAN32__
#define HEAP_DEFAULT_SIZE (1024 * 32)
#define HEAP_GROW_SIZE    (1024 * 256)
#else
/* Symbian devices prefer to conserve memory, and also make less use of these heaps in practice */
#define HEAP_DEFAULT_SIZE (1024 * 64)
#define HEAP_GROW_SIZE    (1024 * 128)
#endif
#define HEAP_MAX_SIZE     (1024 * 1024 *16 * 4)
#define HEAP_THRESHOLD_SIZE     (1024 * 32)
#else
/* use one large static heap block */
#define HEAP_STATIC_SIZE (1024 * 1024 * 8)
#endif

/* make up FB identifier from type speficier and unique id */
#define MAKE_FRAME_BUILDER_ID(type,uniq_id)		((((type) & 0xFF) << 24) | ((uniq_id) & 0xFFFFFF))

/* make up flush identifier from internal frame number , flush type (reset_on_finish) and unique id */
#define MAKE_FLUSH_ID(frame,reset,uniq_id)		((((frame) & 0xFF) << 24) | ((reset&1)<<23) | ((uniq_id) & 0x7FFFFF))

#if MALI_PERFORMANCE_KIT
static int mali_complete_jobs_instantly = -1;
static int mali_simple_hw_perf = -1;
static int mali_custom_hw_perf = -1;
static int mali_print_custom_hw_perf = -1;
#endif

MALI_STATIC mali_err_code _mali_frame_builder_plbu_init( mali_internal_frame *frame );
MALI_STATIC void _mali_frame_builder_update_heap_address( mali_internal_frame *frame );

/* forward declaration */
MALI_STATIC mali_ds_release _flush_fragment_callback(mali_base_ctx_handle base_ctx, mali_internal_frame *frame, mali_ds_error status);

MALI_STATIC void _start_pp_processing_callback(mali_internal_frame *frame);

MALI_STATIC_FORCE_INLINE u32 suggest_pp_split_count(u32 width, u32 height)
{
	/* round up to nearest 16 pixels, then divide by it */
	u32 m_tile_width  = (width + (MALI200_TILE_SIZE-1)) / MALI200_TILE_SIZE;
	u32 m_tile_height = (height + (MALI200_TILE_SIZE-1)) /MALI200_TILE_SIZE;
	u32 num_tiles = m_tile_width * m_tile_height;

	return MIN( _mali_pp_get_num_cores(), num_tiles );
}

MALI_EXPORT void _mali_frame_set_inc_render_on_flush( mali_frame_builder* frame_builder, mali_bool value )
{
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	frame_builder->inc_render_on_flush = value;
}

#ifdef DEBUG

/* Debug function to print gp_job & pp_job errorcodes in an understandable format */
MALI_STATIC void _debug_print_mali_jobstat( u32 job_stat )
{
	const char *job_verbose[] =
	{
		"MALI_JOB_STATUS_END_SUCCESS",
		"MALI_JOB_STATUS_END_OOM",
		"MALI_JOB_STATUS_END_ABORT",
		"MALI_JOB_STATUS_END_TIMEOUT_SW",
		"MALI_JOB_STATUS_END_HANG",
		"MALI_JOB_STATUS_END_SEG_FAULT",
		"MALI_JOB_STATUS_END_ILLEGAL_JOB",
		"MALI_JOB_STATUS_END_UNKNOWN_ERR",
		"MALI_JOB_STATUS_END_SHUTDOWN",
		"MALI_JOB_STATUS_END_SYSTEM_UNUSABLE"
	};
	u32 tmp;
	u32 status_index = 0;

	tmp = job_stat>>16;
	while (tmp>>=1) status_index++;

	MALI_DEBUG_ASSERT_LEQ( status_index, (sizeof(job_verbose)/sizeof(char *))-1 );
	MALI_DEBUG_PRINT(0, ("jobstat: %d = %s \n", job_stat, job_verbose[status_index]));
}

#endif /* DEBUG */

#if MALI_USE_DEFERRED_FRAME_RESET
void _mali_frame_reset_deferred(mali_internal_frame *frame)
{
	MALI_DEBUG_ASSERT_POINTER( frame );

	_mali_frame_reset(frame);
	_mali_sys_lock_unlock(frame->lock);
}

MALI_STATIC void _mali_queue_frame_reset(mali_internal_frame *frame)
{
	mali_frame_builder *frame_builder;

	MALI_DEBUG_ASSERT_POINTER( frame );

#if MALI_INSTRUMENTED
	if (NULL != frame->instrumented_frame)
	{
		_instrumented_set_active_frame(frame->instrumented_frame);
	}
#endif

	frame_builder = frame->frame_builder;
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	if (MALI_ERR_NO_ERROR != _mali_base_cleanup_thread_enqueue(frame_builder->base_ctx, MALI_REINTERPRET_CAST(mali_base_worker_task_proc)_mali_frame_reset_deferred, (void *)frame))
	{
		_mali_frame_reset_deferred(frame);
	}

#if MALI_INSTRUMENTED
	if (NULL != frame->instrumented_frame)
	{
		_instrumented_set_active_frame(NULL);
	}
#endif
}
#endif /* MALI_USE_DEFERRED_FRAME_RESET */

mali_ds_resource_handle _mali_frame_builder_surface_do_copy_on_write(void* resource_owner, void* consumer_owner)
{
	mali_ds_resource_handle retval;

	mali_surface_deep_cow_descriptor *cow_desc = NULL;
	mali_surface            *surface = (mali_surface*)resource_owner;
	mali_internal_frame     *frame   = (mali_internal_frame*)consumer_owner;

	/* Do we have to handle a deep copy later ? */
	if ( FRAME_COW_DEEP == frame->cow_flavour )
	{
		/* setup a read dependency on the old surface so that we don't start too early with the PP job */
		if ( MALI_ERR_NO_ERROR != mali_ds_connect(frame->ds_consumer_pp_render, surface->ds_resource, MALI_DS_READ) )
		{
			MALI_DEBUG_PRINT(1, ("FB CoW - Resource connection failed") );
			return NULL;
		}
		cow_desc = &frame->cow_desc;
		frame->cow_flavour = FRAME_COW_DEEP_COPY_PENDING;
	}

	/* Re-allocate a new resource immediately and store the previous memory pointers for a copy operation later */
	_mali_surface_access_lock(surface);
	retval = _mali_surface_clear_dependencies( surface, cow_desc );
	_mali_surface_access_unlock(surface);

	/* In case the reallocation failed we abandon the DeepCOW too. */
	if ( NULL == retval && frame->cow_flavour == FRAME_COW_DEEP_COPY_PENDING )
	{
		frame->cow_flavour = FRAME_COW_REALLOC;
	}

	return retval;
}

MALI_STATIC mali_err_code _add_flush_dependencies_on_buffers(mali_internal_frame *frame, mali_frame_builder* fbuilder, mali_bool frame_swap)
{
	int i;
	mali_err_code err;

	/* The default framebuilder flush setting is the RELEASE_WRITE, READ_GOTO_UNFLUSHED mode.
	 * This mode must be set prior to all flushes (since it can be changed by incremental rendering),
	 * and this is a good place to set it. The RELEASE WRITE, READ_GOTO UNFLUSHED mode will release all
	 * write dependencies after the flush is over, but retain all read dependencies, returning them to unflushed
	 * mode.
	 *
	 * The exception is if this frame has been flagged with frame swap. In this case, no new flushes can occur,
	 * and as a result, all the frame dependencies will be released upon flush finished */
	if(frame_swap) {
		mali_ds_consumer_release_set_mode(frame->ds_consumer_pp_render, MALI_DS_RELEASE_ALL);
	} else {
		mali_ds_consumer_release_set_mode(frame->ds_consumer_pp_render, MALI_DS_RELEASE_WRITE_GOTO_UNFLUSHED);
	}

	for (i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; ++i)
	{
		mali_surface *target_surface = fbuilder->output_buffers[i].buffer;
		if (NULL != target_surface)
		{
			/* Set up event callbacks if there is any event handler for the surface GPU WRITE DONE event */
			if(target_surface->eventfunc[MALI_SURFACE_EVENT_GPU_WRITE_DONE] != NULL)
			{
				/* adding the output buffer to surfacetracking */
				err = _mali_surfacetracking_add(frame->surfacetracking, target_surface, MALI_SURFACE_TRACKING_WRITE);
				if(MALI_ERR_NO_ERROR != err) return err;
			}

			/* add the surface memref to the frame, preventing the memref from vanishing before the frame is done */
			err = _mali_frame_builder_add_callback( fbuilder,
									          (mali_frame_cb_func)_mali_shared_mem_ref_owner_deref,
											  (mali_frame_cb_param) target_surface->mem_ref) ;
			if(MALI_ERR_NO_ERROR != err) return err;
			_mali_shared_mem_ref_owner_addref( target_surface->mem_ref );

			/* add write dependency. This must happen after the memory is addref'ed */
			err =  mali_ds_connect(frame->ds_consumer_pp_render, target_surface->ds_resource, MALI_DS_WRITE);
			if(MALI_ERR_NO_ERROR != err) return err;

		}
	}

#if MALI_FRAMEBUFFER_DUMP_ENABLED
	err = _mali_fbdump_setup_callbacks(fbuilder);		
	if(MALI_ERR_NO_ERROR != err) return err;
#endif


	return MALI_ERR_NO_ERROR;
}

MALI_EXPORT MALI_CHECK_RESULT mali_err_code _mali_frame_callback_list_set_room(mali_internal_frame *frame, int room)
{
	/* allocate more memory */
	mali_frame_callback_wrapper *new_list = _mali_sys_realloc(frame->callback_list,  sizeof(mali_frame_callback_wrapper) * room);
	if (NULL == new_list) return MALI_ERR_OUT_OF_MEMORY;

	/* update state */
	frame->callback_list_room = room;
	frame->callback_list = new_list;
	return MALI_ERR_NO_ERROR;
}

void null_callback(u32 dummy)
{
	MALI_IGNORE(dummy);
	return;
}

void _mali_frame_builder_frame_execute_callbacks_internal( mali_internal_frame *frame, mali_bool execute_deferred_callbacks )
{
	int i;
#if MALI_INSTRUMENTED
	mali_instrumented_frame* acquired_instrumented_frame = NULL;
#endif

	MALI_DEBUG_ASSERT_POINTER( frame );

	/* The callback list can be NULL if we failed to allocate it in _alloc_internal_frames */
	if( NULL == frame->callback_list ) return;
	
	if(!execute_deferred_callbacks && MALI_FALSE == frame->have_non_deferred_callbacks) return;

#if MALI_INSTRUMENTED
	if (NULL == _instrumented_get_active_frame())
	{
		if (NULL == frame->instrumented_frame)
		{
			/*
			 * There are currently no active frame and the framebuilder haven't setup a frame yet.
			 * Use the current frame as active frame for the callback.
			 * (the callback uses the active instrumented frame)
			 */
			acquired_instrumented_frame = _instrumented_acquire_current_frame();
			MALI_DEBUG_ASSERT_POINTER(acquired_instrumented_frame);
			_instrumented_set_active_frame(acquired_instrumented_frame);
		}
		else
		{
			_instrumented_set_active_frame(frame->instrumented_frame);
		}
	}
#endif

	if(execute_deferred_callbacks)
	{
		/* execute callbacks unconditionally */
		for (i = frame->callback_list_size -1 ; i >= 0; i--)
		{
			mali_frame_callback_wrapper *cb_wrapper = &frame->callback_list[i];
			(cb_wrapper->func)(cb_wrapper->param);
		}
	}
	else
	{
		/* execute only the ones that can't be deferred, we know we have some because of the above early out test */
		for (i = frame->callback_list_size -1 ; i >= 0; i--)
		{
			mali_frame_callback_wrapper *cb_wrapper = &frame->callback_list[i];
			if ( MALI_FALSE == cb_wrapper->can_be_deferred )
			{
			 	(cb_wrapper->func)(cb_wrapper->param);
			 	cb_wrapper->func = null_callback;
			 	cb_wrapper->can_be_deferred = MALI_TRUE;
			}
			 	
		}
		frame->have_non_deferred_callbacks = MALI_FALSE; 
	}

#if MALI_INSTRUMENTED
	_instrumented_set_active_frame(NULL); /* don't leave dangling pointers in TLS */
	if (NULL != acquired_instrumented_frame)
	{
		/* Release the reference we acquired */
		_instrumented_release_frame(acquired_instrumented_frame);
	}
#endif
	
	if ( MALI_FALSE == execute_deferred_callbacks ) return;

	/* if the list has grown too much, shrink it to reduce memory usage */
	if ((frame->callback_list_size > INITIAL_LIST_ROOM) && /* memory reallocation will fail if list size is 0. */
	    (frame->callback_list_room > frame->callback_list_size * 2) /* we allow 2x spillage since we grow by a factor of two */
	   )
	{
		mali_err_code err = _mali_frame_callback_list_set_room(frame, frame->callback_list_size);
		/* The error does not matter since we use realloc - and if realloc fails the existing buffer is untouched.
		 * That means we will continue to use the existing (larger) callback list.
		 */
		MALI_IGNORE(err);

	}

	frame->callback_list_size = 0; /* reset the list-size */
	frame->have_non_deferred_callbacks = MALI_FALSE;
}

void _mali_frame_builder_frame_execute_callbacks( mali_internal_frame *frame )
{
   _mali_frame_builder_frame_execute_callbacks_internal( frame, MALI_TRUE );
}

#if MALI_EXTERNAL_SYNC

MALI_STATIC void _mali_frame_call_pp_job_start_callback(mali_internal_frame *frame, mali_bool success, mali_fence_handle fence)
{
	mali_frame_builder *frame_builder;
	MALI_DEBUG_ASSERT_POINTER( frame );
	frame_builder = frame->frame_builder;

	frame_builder->egl_funcptrs->_sync_notify_job_start(success, fence, frame->fence_sync);
	frame->fence_sync = NULL;

	if (NULL != frame->cb_func_pp_job_start)
	{
		frame->cb_func_pp_job_start(success, fence, frame->cb_param_pp_job_start);
		frame->cb_func_pp_job_start = NULL;
	}
}

#endif /* MALI_EXTERNAL_SYNC */

void _mali_frame_builder_frame_execute_callbacks_non_deferred( mali_internal_frame *frame )
{
	if ( MALI_TRUE == frame->have_non_deferred_callbacks )
	{
		_mali_frame_builder_frame_execute_callbacks_internal( frame, MALI_FALSE );
	}
}

MALI_STATIC_INLINE void _mali_frame_builder_set_consumer_errors_with_activate_pp( mali_internal_frame *frame )
{
	mali_ds_consumer_set_error( frame->ds_consumer_pp_render );

	/* This will call the activation callback for the frame.
	- Since it has an error state, this will call:
	_abort_pp_processing( frame, base_ctx ); */
#if MALI_INSTRUMENTED
	_instrumented_set_active_frame(frame->instrumented_frame);
#endif
	mali_ds_consumer_activation_ref_count_change( frame->ds_consumer_pp_render, MALI_DS_REF_COUNT_RELEASE );
#if MALI_INSTRUMENTED
	_instrumented_set_active_frame(NULL); /* Don't leave a dangling pointer */
#endif

}

MALI_STATIC_INLINE void _mali_frame_builder_set_consumer_errors_with_force_release_pp( mali_internal_frame *frame )
{
	mali_ds_consumer_set_error( frame->ds_consumer_pp_render );

	/* This will call the release callback for the frame. */
	mali_ds_consumer_release_all_connections( frame->ds_consumer_pp_render);

}

#if !defined(HARDWARE_ISSUE_3251)
MALI_STATIC_INLINE mali_err_code _mali_frame_plbu_heap_size_change( mali_base_ctx_handle base_ctx, mali_internal_frame* frame )
{
	MALI_DEBUG_ASSERT_POINTER( frame );

	if( NULL == frame->plbu_heap_held )
	{
		return MALI_ERR_NO_ERROR;
	}

	if( MALI_NO_HANDLE != frame->plbu_heap_held->plbu_heap )
	{
		if( frame->plbu_heap_held->use_count != 0)
		{
			u32 heap_size = 0;
			MALI_DEBUG_ASSERT_POINTER( frame->frame_builder );

			heap_size =	MAX(	MAX(frame->frame_builder->past_plbu_size[0], frame->frame_builder->past_plbu_size[1]),
						MAX(frame->frame_builder->past_plbu_size[2], frame->frame_builder->past_plbu_size[3]));

			heap_size = MAX(((u32)(heap_size+1023)/1024) * 1024, HEAP_DEFAULT_SIZE);

			if((u32) _mali_sys_abs(heap_size - frame->plbu_heap_held->init_size) < (frame->plbu_heap_held->init_size >> 4) )
			{
				_mali_mem_heap_reset(frame->plbu_heap_held->plbu_heap);
			}
			else
			{
				MALI_CHECK_NO_ERROR( _mali_mem_heap_resize( base_ctx, frame->plbu_heap_held->plbu_heap, heap_size ));
				frame->plbu_heap_held->init_size = heap_size;
			}
			frame->plbu_heap_held->use_count = 0;
		}
	}
	return MALI_ERR_NO_ERROR;
}
#endif

void _mali_frame_builder_activate_gp_consumer_callback ( mali_base_ctx_handle base_ctx, void * owner, mali_ds_error status )
{
	mali_internal_frame *frame = ( mali_internal_frame * ) owner;
	mali_gp_job_handle current_gp_job = frame->current_gp_job;
	mali_err_code error = MALI_ERR_FUNCTION_FAILED;  /* Set default value to error. */

	MALI_IGNORE( base_ctx );

        /* The codeblock below synchronizes the order of when the internal frame starts rendering.
           The code manipulates the order_synch variable in two different frames taking a mutex in the
           previous frame. This is necessary since there are codepaths that call this function or
           _mali_activate_gp_consumer_callback without taking the Dependency System mutex.
           This can cause r/w races on order_sync with _mali_activate_gp_consumer_callback */
	{
		mali_internal_frame * previous_frame;
		previous_frame = frame->order_synch.prev;
		if( NULL!=previous_frame )
		{
			_mali_sys_mutex_lock(previous_frame->order_synch.frame_order_mutex);

			MALI_DEBUG_ASSERT( NULL==previous_frame->order_synch.release_on_finish , ("Previous frame have already got a connection to this frame, missing cleanup in reset?"));

			if ( (MALI_TRUE == previous_frame->order_synch.in_flight) &&
				 (previous_frame->order_synch.swap_nr < frame->order_synch.swap_nr )
			   )
			{
				/* Adding one reference to the current frame activation count.
				(Same as the GP job does) The current frame will not be activated
				before previous rendering is finished. (and the GP job)*/
				mali_ds_consumer_activation_ref_count_change(frame->ds_consumer_pp_render, MALI_DS_REF_COUNT_GRAB);
				/* Setting this will tell the previous frame to decrement this frame
				activation ref count when it is finished */
				MALI_DEBUG_ASSERT( FRAME_RENDERING == previous_frame->state, ("Err: The previous frame %p is not rendering: state=%d.", previous_frame, previous_frame->state) );
				previous_frame->order_synch.release_on_finish = frame->ds_consumer_pp_render;
			}
			_mali_sys_mutex_unlock(previous_frame->order_synch.frame_order_mutex);
		}
	}
	MALI_DEBUG_PRINT(3, ("GPS[%d]", frame->order_synch.swap_nr));

#if !defined(HARDWARE_ISSUE_3251)
	/* On the flush on a frame after a complete reset (@see _mali_frame_builder_reset)
	 * we reset the PLBU heap.
	 */
	_mali_sys_mutex_lock( frame->mutex );

	if ( frame->plbu_heap_reset_on_job_start || (frame->num_flushes_since_reset == 1) )
	{
		if( MALI_ERR_NO_ERROR != _mali_frame_plbu_heap_size_change( base_ctx, frame ))
		{
			status = MALI_DS_ERROR;
		}
		else
		{
			_mali_frame_builder_update_heap_address( frame );
		}
		frame->plbu_heap_reset_on_job_start = MALI_FALSE;
	}

	_mali_sys_mutex_unlock( frame->mutex );
#endif

	switch (status)
	{
		case MALI_DS_OK:
#if MALI_PERFORMANCE_KIT
			if (mali_complete_jobs_instantly == -1)
			{
				mali_complete_jobs_instantly = (int)_mali_sys_config_string_get_s64("MALI_COMPLETE_JOBS_INSTANTLY", 0, 0, 1);
			}

			if (mali_complete_jobs_instantly == 1)
			{
				error = MALI_ERR_OUT_OF_MEMORY; /* don't start job and trigger cleanup */
			}
			else
			{
#if MALI_TIMELINE_PROFILING_ENABLED
				_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_SINGLE | VR_PROFILING_EVENT_CHANNEL_SOFTWARE | VR_PROFILING_EVENT_REASON_SINGLE_SW_GP_ENQUEUE, 0, 0, frame->frame_builder->identifier, frame->flush_id, 0);
#endif
				error = _mali_gp_job_start( current_gp_job, MALI_JOB_PRI_NORMAL );
			}
#else
#if MALI_TIMELINE_PROFILING_ENABLED
			_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_SINGLE | VR_PROFILING_EVENT_CHANNEL_SOFTWARE | VR_PROFILING_EVENT_REASON_SINGLE_SW_GP_ENQUEUE, 0, 0, frame->frame_builder->identifier, frame->flush_id, 0);
#endif
			error = _mali_gp_job_start( current_gp_job, MALI_JOB_PRI_NORMAL );
#endif
		break;
		case MALI_DS_ERROR:
			/* Free all resources and do nothing:
			 * This is a NOOP, since this is handled by the PP-consumer */

			/* Continue on to the error block below.*/
			break;
		default:
			return;
	}

	if ( MALI_ERR_NO_ERROR != error )
	{
		mali_ds_consumer_handle ds_consumer_pp_render;

		/* grab the mutex again since we need to do more processing on the frame */
		_mali_sys_mutex_lock( frame->mutex );

		#if MALI_INSTRUMENTED
			_instrumented_gp_abort( frame->current_gp_job );
		#endif

		if( frame->reset_on_finish == MALI_FALSE )
		{
			_mali_gp_job_free( frame->gp_job );
		}
		frame->gp_job = frame->current_gp_job;

		/* Don't set error-state for the gp-consumer, this will happen when
		 * we activate the pp-consumer with an error set
		 */

		/* But set an error on the pp_render consumer */
		mali_ds_consumer_set_error( frame->ds_consumer_pp_render );

		/* Cache the consumer, so we can unlock the frame before we post the error */
		ds_consumer_pp_render = frame->ds_consumer_pp_render;

		/* ungrab the mutex again since we now are done changing the frame in this function */
		_mali_sys_mutex_unlock( frame->mutex );

		/* This will call the activation callback for the frame.
		- It will not start the PP jobs, since the error state is set on the consumer.
		- It will however wait with the callback until its dependencies are met, and previous
		frame is also finished. This is necessary to keep the ordering of events correct. */
#if MALI_INSTRUMENTED
		_instrumented_set_active_frame(frame->instrumented_frame);
#endif
		mali_ds_consumer_activation_ref_count_change( ds_consumer_pp_render, MALI_DS_REF_COUNT_RELEASE );
#if MALI_INSTRUMENTED
		_instrumented_set_active_frame(NULL); /* Don't leave a dangling pointer */
#endif

		/* We do deliberetely not return error, since the frame could considered started,
		but failed after it came into the dependency system, and will be swapped with
		an error state*/
	}
}

/**
 * Undo the work done in _alloc_internal_frames
 **/
MALI_STATIC void _free_internal_frames(mali_internal_frame **frame_array, u32 swap_count)
{
	u32 i;

	MALI_DEBUG_ASSERT_POINTER(frame_array);

	for( i = 0; i < swap_count; i++ )
	{
		mali_internal_frame *frame = frame_array[ i ];
		if( frame != NULL )
		{
			_mali_frame_free( frame );
			frame = NULL;
			frame_array[i] = NULL;
		}
	}
	_mali_sys_free( frame_array );
}


/**
 * Creates a number of mali_internal_frame structures and initializes them.
 * Only called when allocating a new frame_builder.
 * @param base_ctx The base context to use for mali resource allocations
 * @swap_count The number of frames to create ( == the number of entries in the buffer swap chain)
 * @return An array of pointers to the new frames
 */
MALI_STATIC mali_internal_frame **_alloc_internal_frames( mali_base_ctx_handle base_ctx,
                                                         mali_frame_builder *frame_builder,
                                                         u32 swap_count )
{
	u32                   i;
	mali_bool             init_succeeded;
	mali_internal_frame **frame_array;

	MALI_IGNORE(frame_builder);

	MALI_DEBUG_ASSERT_HANDLE( base_ctx );
	MALI_DEBUG_ASSERT( swap_count > 0, ("illegal swap count\n") );


	frame_array = (mali_internal_frame **) _mali_sys_calloc( swap_count, sizeof( mali_internal_frame *) );
	if( frame_array == NULL ) return NULL;

	init_succeeded = MALI_TRUE;
	for( i = 0; i < swap_count; i++ )
	{
		mali_internal_frame *frame = _mali_frame_alloc( base_ctx, frame_builder);
		frame_array[ i ] = frame;
		if (NULL == frame) init_succeeded = MALI_FALSE;
	}

    /* Internal frames finishing Out Of Order Protection */
	/* If swap_count is higher than one and all frame allocations so far are
	 * successful, we need to force ooo protection.  If the initialization
	 * failed, we will destroy the allocated frames anyway. */
	if ( swap_count > 1 && init_succeeded )
	{
		u32 last_idx = swap_count-1;
		/* Setting up the ooo protector in the frame */
		for( i = 0; i < swap_count; i++ )
		{
			mali_internal_frame *frame;
			frame = frame_array[ i ];
			frame->order_synch.prev =    (i!=0)     ? frame_array[ i-1 ] : frame_array[ last_idx ];
			frame->order_synch.next = (i!=last_idx) ? frame_array[ i+1 ] : frame_array[ 0 ];
			frame->order_synch.swap_nr           = 0;
			frame->order_synch.in_flight         = MALI_FALSE;
			frame->order_synch.release_on_finish = NULL;
		}
	}

	/* all allocations succeeded */
	if( init_succeeded == MALI_TRUE ) return frame_array;

	/* allocations failed, so we have to clean up */
	_free_internal_frames(frame_array, swap_count);
	return NULL;
}


MALI_STATIC void __m200_rsw_dummy( m200_rsw *rsw, u32 dummy_shader_addr, s32 first_instr_len )
{
	MALI_DEBUG_ASSERT_POINTER( rsw );
	MALI_DEBUG_ASSERT( dummy_shader_addr != 0, ("Invalid shader_addr\n" ) );
	MALI_DEBUG_ASSERT( first_instr_len > 0, ("Invalid first instruction length\n") );

	_mali_sys_memset( rsw, 0, sizeof( m200_rsw ) );
	__m200_rsw_encode( rsw, M200_RSW_MULTISAMPLE_WRITE_MASK, 0xf );
	__m200_rsw_encode( rsw, M200_RSW_MULTISAMPLE_GRID_ENABLE, 1 );
	__m200_rsw_encode( rsw, M200_RSW_SHADER_ADDRESS, dummy_shader_addr );
	__m200_rsw_encode( rsw, M200_RSW_HINT_NO_SHADER_PRESENT, 0 );
	__m200_rsw_encode( rsw, M200_RSW_ALLOW_EARLY_Z_AND_STENCIL_TEST, 1 );
	__m200_rsw_encode( rsw, M200_RSW_Z_COMPARE_FUNC, M200_TEST_ALWAYS_FAIL);
	__m200_rsw_encode( rsw, M200_RSW_STENCIL_FRONT_COMPARE_FUNC, M200_TEST_ALWAYS_FAIL);
	__m200_rsw_encode( rsw, M200_RSW_STENCIL_BACK_COMPARE_FUNC, M200_TEST_ALWAYS_FAIL);

	/* See P00086 5.1 The header which says 4:0 == 1st instruction word length, this needs
	to be encoded into the dummy program */
	#if defined(USING_MALI400) || defined(USING_MALI450)
	__m200_rsw_encode( rsw, M200_RSW_FIRST_SHADER_INSTRUCTION_LENGTH, 0 );
	#else
	__m200_rsw_encode( rsw, M200_RSW_FIRST_SHADER_INSTRUCTION_LENGTH, (u32)first_instr_len );
	#endif
}


/**
 * Init the required dummy RSW and insert it into the rsw cache.
 * This is allocated once and re-used over all frames in the framebuilder. 
 */
MALI_STATIC mali_err_code _mali_frame_builder_alloc_dummy_rsw( mali_frame_builder *frame_builder )
{
	static const u32     dummy_shader[] = { 0x00020425, 0x0000000C, 0x01E007CF, 0xB0000000, 0x000005F5 };
	m200_rsw dummy_rsw;

	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	MALI_DEBUG_ASSERT_POINTER( frame_builder->base_ctx );

	frame_builder->dummy_rsw_mem = _mali_mem_alloc( frame_builder->base_ctx, MALI_ALIGN(sizeof(m200_rsw), 64) + MALI_ALIGN(sizeof( dummy_shader ),64) , 64, MALI_PP_READ | MALI_CPU_WRITE );
	if(frame_builder->dummy_rsw_mem == MALI_NO_HANDLE) return MALI_ERR_OUT_OF_MEMORY;

	/* copy the dummy shader to mali mem and store the length of the first instruction */
	_mali_mem_write_cpu_to_mali_32( frame_builder->dummy_rsw_mem, MALI_ALIGN(sizeof(m200_rsw), 64), (void *)dummy_shader, sizeof( dummy_shader ),  sizeof( dummy_shader[0] ) );

	/* set the dummy shader parameters in the rsw and set the other default params */
	__m200_rsw_dummy(&dummy_rsw, _mali_mem_mali_addr_get( frame_builder->dummy_rsw_mem, MALI_ALIGN(sizeof(m200_rsw), 64)), dummy_shader[0] & 0x1f );

	/* copy the RSW to mali mem */
	_mali_mem_write_cpu_to_mali_32( frame_builder->dummy_rsw_mem, 0, dummy_rsw, sizeof(dummy_rsw), sizeof(m200_rsw));

	return MALI_ERR_NO_ERROR;
}

MALI_EXPORT mali_frame_builder *_mali_frame_builder_alloc(
	enum mali_frame_builder_type type,
	mali_base_ctx_handle base_ctx,
	u32 swap_count,
	enum mali_frame_builder_properties properties,
	egl_api_shared_function_ptrs *egl_funcptrs )
{
	s32                  i;
	u32                  heap_size;
	mali_err_code        error;
	mali_frame_builder  *frame_builder;

	MALI_DEBUG_ASSERT_HANDLE( base_ctx );
	MALI_DEBUG_ASSERT( swap_count > 0, ("Need a minimum swapcount of 1\n") );

	/* allocate the frame_builder struct to initialize*/
	frame_builder = (mali_frame_builder *) _mali_sys_calloc( 1, sizeof( mali_frame_builder ) );

	if( NULL == frame_builder ) return NULL;

	/* set base ctx which will be used for all mali resource allocations */
	frame_builder->base_ctx                           = base_ctx;
	frame_builder->egl_funcptrs                       = egl_funcptrs;
	frame_builder->properties                         = properties;
	frame_builder->incremental_render_flush_threshold = INCREMENTAL_RENDERING_THRESHOLD;
	frame_builder->buffer_state_per_plane             = 0;
	frame_builder->preserve_multisample               = MALI_FALSE;

	/* Only allow a maximum of 2 PLBU-heaps, one for working with GP and one for working with PP */
	frame_builder->plbu_heap_count = MIN( 2, swap_count );

	/* set swap variables in frame builder */
	frame_builder->iframe_current = 0;
	frame_builder->iframe_count   = swap_count;

	/* alloc mali memory for dummy rsw + shader. They share one memblock, at 64 byte aligned offsets */
	error = _mali_frame_builder_alloc_dummy_rsw(frame_builder);
	if( error != MALI_ERR_NO_ERROR )
	{
		_mali_frame_builder_free( frame_builder );
		frame_builder = NULL;
		return NULL;
	}

#if HARDWARE_ISSUE_7320
	/* allocate flush commandlist for issue 7320 workaround */
	frame_builder->flush_commandlist_subroutine = _mali_frame_builder_create_flush_command_list_for_bug_7320( base_ctx );
	if ( frame_builder->flush_commandlist_subroutine == MALI_NO_HANDLE )
	{
		_mali_frame_builder_free( frame_builder );
		frame_builder = NULL;
		return NULL;
	}
#endif

	/* allocate and initialize the internal frames */
	frame_builder->curr_pp_split_count = _mali_pp_get_num_cores();
	frame_builder->iframes = _alloc_internal_frames( base_ctx, frame_builder, swap_count);
	if( frame_builder->iframes == NULL )
	{
		_mali_frame_builder_free( frame_builder );
		frame_builder = NULL;
		return NULL;
	}

	/* allocate and set up PLBU heaps */
	frame_builder->plbu_heap_current = 0;
	frame_builder->plbu_heaps = _mali_sys_calloc( frame_builder->plbu_heap_count, sizeof( mali_heap_holder ) );
	if ( NULL == frame_builder->plbu_heaps )
	{
		_mali_frame_builder_free( frame_builder );
		frame_builder = NULL;
		return NULL;
	}

	#if !defined(HARDWARE_ISSUE_3251)
	heap_size = (u32)_mali_sys_config_string_get_s64("MALI_FRAME_HEAP_SIZE", HEAP_DEFAULT_SIZE, 4096, HEAP_MAX_SIZE);
	if ( frame_builder->properties & MALI_FRAME_BUILDER_PROPS_NOT_ALLOCATE_HEAP ) 
	{
	 	#if defined(USING_MALI200) && (MALI200_HWVER == 0x0004)
		/* Can't have zero size heap on r0p4. See bugzilla 9379 */
		heap_size = 128;
		#else
		heap_size = 0;
		#endif
	}
	#endif
	
	for ( i = 0; i < (s32) frame_builder->plbu_heap_count; i++ )
	{
		frame_builder->plbu_heaps[i].plbu_ds_resource = mali_ds_resource_allocate( base_ctx, frame_builder, NULL );

		frame_builder->plbu_heaps[ i ].init_size		= heap_size;
		frame_builder->plbu_heaps[i].use_count			= 0;

		if ( MALI_NO_HANDLE == frame_builder->plbu_heaps[i].plbu_ds_resource )
		{
			_mali_frame_builder_free( frame_builder );
			frame_builder = NULL;
			return NULL;
		}

		if( 0 != heap_size )
		{
			#if !defined(HARDWARE_ISSUE_3251)
				frame_builder->plbu_heaps[ i ].plbu_heap = _mali_mem_heap_alloc( frame_builder->base_ctx, heap_size, HEAP_MAX_SIZE, HEAP_GROW_SIZE);
			#else
				frame_builder->plbu_heaps[ i ].plbu_heap = _mali_mem_alloc( frame_builder->base_ctx, heap_size, 1024, MALI_PP_READ | MALI_GP_WRITE );
			#endif
			if ( MALI_NO_HANDLE == frame_builder->plbu_heaps[ i ].plbu_heap )
			{
				_mali_frame_builder_free( frame_builder );
				frame_builder = NULL;
				return NULL;
			}
		}
	}

	/* no attachments at this point */
	_mali_sys_memset(frame_builder->output_buffers, 0, sizeof(frame_builder->output_buffers));

	/* In case we are working with FBOs, we set color, depth and stencil buffer state to UNDEFINED */ 
	if( MALI_FRAME_BUILDER_TYPE_GLES_FRAMEBUFFER_OBJECT == type )
	{
		frame_builder->buffer_state_per_plane = MALI_COLOR_PLANE_UNDEFINED | MALI_DEPTH_PLANE_UNDEFINED | MALI_STENCIL_PLANE_UNDEFINED;
	}

	/* intial scissor params */
	frame_builder->scissor.top = frame_builder->scissor.bottom = frame_builder->scissor.left = frame_builder->scissor.right = 0;
	frame_builder->vpbox.top   = frame_builder->vpbox.bottom   = frame_builder->vpbox.left   = frame_builder->vpbox.right   = 0.f;

	/* dimensions */
	frame_builder->tilebuffer_color_format = 0x00008888;

	/* Clear values */
	_mali_frame_builder_set_clear_value( frame_builder, MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_R, 0x0 );
	_mali_frame_builder_set_clear_value( frame_builder, MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_G, 0x0 );
	_mali_frame_builder_set_clear_value( frame_builder, MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_B, 0x0 );
	_mali_frame_builder_set_clear_value( frame_builder, MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_A, 0xFFFF );
	_mali_frame_builder_set_clear_value( frame_builder, MALI_FRAME_BUILDER_BUFFER_BIT_DEPTH, 0xFFFFFF );
	_mali_frame_builder_set_clear_value( frame_builder, MALI_FRAME_BUILDER_BUFFER_BIT_STENCIL, 0x0 );

	frame_builder->swap_performed = 0;

#if MALI_TIMELINE_PROFILING_ENABLED
	/* generate unique framebuilder ID */
	frame_builder->identifier = MAKE_FRAME_BUILDER_ID(type, _mali_base_frame_builder_id_get_new(frame_builder->base_ctx));

	/* initialize the flush id generator */
	frame_builder->flush_count = 0;
#endif

#if MALI_EXTERNAL_SYNC
	frame_builder->output_fence = MALI_FENCE_INVALID_HANDLE;
#endif /* MALI_EXTERNAL_SYNC */

	return frame_builder;
}

MALI_EXPORT void _mali_frame_builder_free( mali_frame_builder *frame_builder )
{
	u32 i;
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	/* no leave trace for this one, as the frame builder will be gone then */

	_mali_frame_builder_wait_all( frame_builder );


#if MALI_EXTERNAL_SYNC
	if (MALI_FENCE_INVALID_HANDLE != frame_builder->output_fence)
	{
		mali_fence_release(frame_builder->output_fence);
		frame_builder->output_fence = MALI_FENCE_INVALID_HANDLE;
	}
#endif /* MALI_EXTERNAL_SYNC */


	/* deref attached surfaces */
	for ( i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; i++ )
	{
		if(frame_builder->output_buffers[i].buffer)
		{
			_mali_surface_deref( frame_builder->output_buffers[i].buffer );
			frame_builder->output_buffers[i].buffer	= NULL;
		}
	}

	/* free all internal frames. This implicitly also waits Mali processing to finish. */
	if( frame_builder->iframes != NULL )
	{
		_free_internal_frames(frame_builder->iframes, frame_builder->iframe_count);
		frame_builder->iframes = NULL;
	}

	/* free all heaps */
	if(NULL != frame_builder->plbu_heaps )
	{
		for(i = 0; i < frame_builder->plbu_heap_count; i++)
		{
			if( MALI_NO_HANDLE != frame_builder->plbu_heaps[ i ].plbu_heap )
			{
				_mali_mem_free( frame_builder->plbu_heaps[ i ].plbu_heap );
			}

	 		if( MALI_NO_HANDLE != frame_builder->plbu_heaps[ i ].plbu_ds_resource )
			{
				mali_ds_resource_release_connections( frame_builder->plbu_heaps[ i ].plbu_ds_resource, MALI_DS_RELEASE, MALI_DS_ABORT_ALL );
			}
		}
		_mali_sys_free( frame_builder->plbu_heaps );
		frame_builder->plbu_heaps = NULL;
	}
	
#if HARDWARE_ISSUE_7320
	/* free the flush command list subroutine */
	if ( frame_builder->flush_commandlist_subroutine != MALI_NO_HANDLE )
	{
		_mali_mem_free(frame_builder->flush_commandlist_subroutine);
		frame_builder->flush_commandlist_subroutine = MALI_NO_HANDLE;
	}
#endif

	/* Free the dummy RSW */
	if(frame_builder->dummy_rsw_mem != MALI_NO_HANDLE)
	{
		_mali_mem_free( frame_builder->dummy_rsw_mem );
		frame_builder->dummy_rsw_mem = MALI_NO_HANDLE;
	}

	/* deref attached surfaces */
	for ( i = 0; i < MALI_READBACK_SURFACES_COUNT; i++ )
	{
		if(frame_builder->readback_buffers[i].buffer)
		{
			_mali_surface_deref( frame_builder->readback_buffers[i].buffer );
			frame_builder->readback_buffers[i].buffer = NULL;
		}
	}

	/* free the frame builder */
	_mali_sys_free( frame_builder );
}

/**
 * Insert various dummy PLBU commands in the gp job.
 * The commands will be replaced by the right values inside __internal_flush.
 * Part of the once-per-frame GP init
 */
MALI_STATIC mali_err_code _mali_frame_builder_plbu_preinit( mali_internal_frame *frame )
{
	mali_gp_plbu_cmd* cmds;

	/* Init of viewportbox[] to stop an incorrect warning from Coverity as it doesn't handle unions correctly */
	union {
		float f;
		u32   i;
	} viewportbox[4] = { {0}, {0}, {0}, {0}};

	MALI_DEBUG_ASSERT_POINTER( frame );
	MALI_DEBUG_ASSERT_POINTER( frame->gp_job );

	/* try to get the cmd_list buffer directly */
	cmds = _mali_gp_job_plbu_cmds_reserve( frame->gp_job, 11 );

	/*store it for later */
	frame->pre_plbu_cmd_head = cmds;

    MALI_CHECK_NON_NULL(cmds, MALI_ERR_OUT_OF_MEMORY);

	/* Setting initial viewportbox
	 * Using unions since the binary representation of the floats have to be written to the PLBUs config registers
	 */
	viewportbox[ 0 ].f = 0.f;
	viewportbox[ 1 ].f = frame->vpbox.right;
	viewportbox[ 2 ].f = 0.f;
	viewportbox[ 3 ].f = frame->vpbox.bottom;

	cmds[ 5 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( viewportbox[0].i, GP_PLBU_CONF_REG_SCISSOR_LEFT );
	cmds[ 6 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( viewportbox[1].i, GP_PLBU_CONF_REG_SCISSOR_RIGHT );
	cmds[ 7 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( viewportbox[2].i, GP_PLBU_CONF_REG_SCISSOR_TOP );
	cmds[ 8 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( viewportbox[3].i, GP_PLBU_CONF_REG_SCISSOR_BOTTOM );

MALI_DEBUG_CODE(
	cmds[ 0 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( 1, GP_PLBU_CONF_REG_PARAMS);
	cmds[ 1 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( 2, GP_PLBU_CONF_REG_TILE_SIZE);
	cmds[ 2 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( 3, GP_PLBU_CONF_REG_VIEWPORT );
	cmds[ 3 ] = GP_PLBU_COMMAND_PREPARE_FRAME( 4 );
	cmds[ 4 ] = GP_PLBU_COMMAND_POINTER_ARRAY_BASE_ADDR_TILE_COUNT(0x5555dead, 0);
	cmds[ 9 ]  = GP_PLBU_COMMAND_WRITE_CONF_REG( 0xaaaadead, GP_PLBU_CONF_REG_HEAP_START_ADDR );
	cmds[ 10 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( 0xbbbbdead, GP_PLBU_CONF_REG_HEAP_END_ADDR );
)
	/* Commit the commands to the PLBU list */
	_mali_gp_job_plbu_cmds_confirm( frame->gp_job, 11 );


	return MALI_ERR_NO_ERROR;
}

/**
 * Insert various PLBU commands in the gp job to set up screen sizes etc.
 * Part of the once-per-frame GP init
 */
MALI_STATIC mali_err_code _mali_frame_builder_plbu_init( mali_internal_frame *frame )
{
	mali_gp_plbu_cmd* cmds;

	MALI_DEBUG_ASSERT_POINTER( frame );
	MALI_DEBUG_ASSERT_POINTER( frame->gp_job );
	MALI_DEBUG_ASSERT_POINTER( frame->tilelists);

	MALI_DEBUG_ASSERT(frame->job_inited == 1, ( "job not Inited in _mali_frame_builder_plbu_init") );

	/* try to get the cmd_list buffer directly */
	cmds = frame->pre_plbu_cmd_head;

    MALI_CHECK_NON_NULL(cmds, MALI_ERR_OUT_OF_MEMORY);

	/* the tile list scale must be set before any tile list commands are generated */
	cmds[ 0 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( MALI_TILELIST_CHUNK_FORMAT << 8, GP_PLBU_CONF_REG_PARAMS);

	/* tile binning downscale and polygon list primitve format */
	{
#if defined(USING_MALI200)
		cmds[ 1 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( GP_PLBU_CONF_REGS_TILE_SIZE(frame->tilelists->binning_scale_x, frame->tilelists->binning_scale_y), GP_PLBU_CONF_REG_TILE_SIZE);
#else
		cmds[ 1 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( GP_PLBU_CONF_REGS_TILE_SIZE_WITH_FORMAT(frame->tilelists->binning_scale_x, frame->tilelists->binning_scale_y, frame->tilelists->polygonlist_format), GP_PLBU_CONF_REG_TILE_SIZE);
#endif
	}

	/* starting from r0p2 the screen size is set using master tile dimensions */
	cmds[ 2 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( 0 | ( (frame->tilelists->master_tile_height - 1) << 8 ) | ( 0 << 16 ) |
	                                            ( (frame->tilelists->master_tile_width - 1) << 24 ), GP_PLBU_CONF_REG_VIEWPORT );

	/* width for tile iterator */
	cmds[ 3 ] = GP_PLBU_COMMAND_PREPARE_FRAME( frame->tilelists->slave_tile_width );

	/* load the pointer array into the PLBU */
	cmds[ 4 ] = GP_PLBU_COMMAND_POINTER_ARRAY_BASE_ADDR_TILE_COUNT( _mali_mem_mali_addr_get( frame->tilelists->pointer_array_mem, 0 ), frame->tilelists->tile_pointers_to_load );

	/* NOTE: cmds[5 to 8] are the initial viewport and are set in _mali_frame_builder_plbu_preinit */

	/* cmds [9 to 10] are the initial heap parameters, set in _mali_frame_builder_update_heap_address */
	MALI_DEBUG_ASSERT(frame->job_inited == 1, ( "mali_frame_builder_plbu_init job inited stage should be 1 but is %d", frame->job_inited) );
	frame->job_inited = 2;

	return MALI_ERR_NO_ERROR;
}

MALI_STATIC void _mali_frame_builder_update_heap_address( mali_internal_frame *frame )
{
	mali_gp_plbu_cmd* cmds;

	MALI_DEBUG_ASSERT_POINTER(frame);

	cmds = frame->pre_plbu_cmd_head;

	if( NULL != cmds )
	{
		if ( NULL != frame->plbu_heap_held && NULL != frame->plbu_heap_held->plbu_heap)
		{
#if !defined(HARDWARE_ISSUE_3251)
			cmds[ 9 ]  = GP_PLBU_COMMAND_WRITE_CONF_REG(  _mali_mem_heap_get_start_address( frame->plbu_heap_held->plbu_heap ), GP_PLBU_CONF_REG_HEAP_START_ADDR );
			cmds[ 10 ] = GP_PLBU_COMMAND_WRITE_CONF_REG(  _mali_mem_heap_get_end_address_of_first_block( frame->plbu_heap_held->plbu_heap ), GP_PLBU_CONF_REG_HEAP_END_ADDR );
#else
			cmds[ 9 ]  = GP_PLBU_COMMAND_WRITE_CONF_REG(  _mali_mem_mali_addr_get( frame->plbu_heap_held->plbu_heap, 0), GP_PLBU_CONF_REG_HEAP_START_ADDR );
			cmds[ 10 ] = GP_PLBU_COMMAND_WRITE_CONF_REG(  _mali_mem_mali_addr_get( frame->plbu_heap_held->plbu_heap, _mali_mem_size_get(frame->plbu_heap_held->plbu_heap) ), GP_PLBU_CONF_REG_HEAP_END_ADDR );
#endif
		}
		else
		{
			cmds[ 9 ]  = GP_PLBU_COMMAND_WRITE_CONF_REG(  0, GP_PLBU_CONF_REG_HEAP_START_ADDR );
			cmds[ 10 ] = GP_PLBU_COMMAND_WRITE_CONF_REG(  0, GP_PLBU_CONF_REG_HEAP_END_ADDR );
		}
	}

}


MALI_STATIC void _reset_scissor_and_viewport_state( mali_frame_builder *frame_builder )
{
	u32 width, height;

	mali_internal_frame* frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	width = frame_builder->output_width;
	height = frame_builder->output_height;

	frame_builder->scissor.left   = 0;
	frame_builder->scissor.top    = 0;
	frame_builder->scissor.right  = width - 1;
	frame_builder->scissor.bottom = height - 1;

	frame_builder->vpbox.left   = 0.f;
	frame_builder->vpbox.top    = 0.f;
	frame->vpbox.left   = 0.f;
	frame->vpbox.top    = 0.f;
#if HARDWARE_ISSUE_3396
	/* need the settings to be inclusive due to the interface change */
	frame_builder->vpbox.right  = (float) (width) - 0.001f;
	frame_builder->vpbox.bottom = (float) (height) - 0.001f;
	frame->vpbox.right  = (float) (width) - 0.001f;
	frame->vpbox.bottom = (float) (height) - 0.001f;
#else /* HARDWARE_ISSUE_3396 */
	frame_builder->vpbox.right  = (float) (width);
	frame_builder->vpbox.bottom = (float) (height);
	frame->vpbox.right  = (float) (width);
	frame->vpbox.bottom = (float) (height);
#endif /* HARDWARE_ISSUE_3396 */
}

/**
 * Per-frame GP initialization such as heap setup, pointer array init +++
 */
MALI_STATIC mali_err_code _mali_frame_builder_per_frame_gp_init_pre( mali_frame_builder *frame_builder )
{
	mali_internal_frame* frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	/* finish the old frame id and get a new one */
	frame->frame_id = _mali_base_frame_id_get_new( frame_builder->base_ctx );

	/* reset the job so we're able to associate it with a new frame id */
	_mali_gp_job_reset( frame->gp_job );

	/* associating the gp job with the correct frame id */
	_mali_gp_job_set_frame_id( frame->gp_job, frame->frame_id );

	/* reset scissor params */
	_reset_scissor_and_viewport_state( frame_builder );

	MALI_CHECK_NO_ERROR(_mali_frame_builder_plbu_preinit(frame));

/*	MALI_DEBUG_ASSERT( frame->num_flushes_since_reset == 0, ("per_frame_gp_init_pre called twice"));*/

	frame->job_inited = 1;

	return MALI_ERR_NO_ERROR;
}

/**
 * GP initialization for the _internal_flush
 */
MALI_STATIC mali_err_code _mali_frame_builder_per_frame_gp_init( mali_frame_builder *frame_builder )
{
	mali_err_code err;
	mali_internal_frame* frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	MALI_DEBUG_ASSERT(frame->job_inited == 1, ( "mali_frame_builder_per_frame_gp_init job init stage should be 1 but is %d\n", frame->job_inited) );

	/* associate the heap with the gp job */
	if( NULL != frame->plbu_heap_held && MALI_NO_HANDLE != frame->plbu_heap_held->plbu_heap )
	{
		_mali_gp_job_set_tile_heap( frame->gp_job, frame->plbu_heap_held->plbu_heap );
	}

	_mali_frame_builder_set_split_count( frame_builder, suggest_pp_split_count( frame_builder->output_width, frame_builder->output_height ) );

	/* Allocate the tilelists if not yet done or if sizes changed*/
	if(frame->tilelists == NULL ||
	   frame_builder->output_width != frame->tilelists->width ||
	   frame_builder->output_height != frame->tilelists->height ||
	   frame_builder->curr_pp_split_count != frame->tilelists->split_count)
	{
		if(frame->tilelists) _mali_tilelist_free(frame->tilelists);
		frame->tilelists = _mali_tilelist_alloc( frame_builder->output_width,
		                                         frame_builder->output_height,
		                                         frame_builder->curr_pp_split_count, 
		                                         frame_builder->base_ctx);
		if(!frame->tilelists) return MALI_ERR_OUT_OF_MEMORY;
	} else {
		/* reinitialize the pointer array */
		/* TODO: only legal if the tilelist is not in use!!! Verify this!*/
		err = _mali_tilelist_reset(frame->tilelists);
		if(err != MALI_ERR_NO_ERROR) return err;

		/* and maybe re-align the tilelists to work on a new splitcount */
		if(frame_builder->curr_pp_split_count != frame->tilelists->split_count)
		{
			err = _mali_tilelist_change_splitcount(frame->tilelists, frame_builder->curr_pp_split_count);
			if(err != MALI_ERR_NO_ERROR) return err;
		}
	}
	frame->curr_pp_split_count = frame_builder->curr_pp_split_count;

	/* tile list dimensions, pointer array, initial viewportbox and heap setup */
	MALI_CHECK_NO_ERROR(_mali_frame_builder_plbu_init( frame ));

	return MALI_ERR_NO_ERROR;
}

MALI_STATIC mali_err_code _mali_frame_builder_init_per_frame( mali_frame_builder *frame_builder )
{
	mali_internal_frame *frame;
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	MALI_DEBUG_ASSERT(frame_builder->output_valid == MALI_TRUE, ("Framebuilder outputs set up incorrectly"));

	frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	MALI_DEBUG_ASSERT( MALI_ERR_NO_ERROR != _mali_sys_mutex_try_lock(frame->mutex),
	                   ("frame->mutex not locked by caller!") );

	MALI_DEBUG_ASSERT( frame->state != FRAME_CLEAN,("FRAME_CLEAN state inside _mali_frame_builder_init_per_frame!"));
	MALI_DEBUG_ASSERT( frame->state != FRAME_UNMODIFIED,("FRAME_UNMODIFIED state inside _mali_frame_builder_init_per_frame!"));
	MALI_DEBUG_ASSERT( frame->state != FRAME_COMPLETE  ,("FRAME_COMPLETE state inside _mali_frame_builder_init_per_frame!"));

	/* do per-frame init if this is the first _use() after a reset */
	if( frame->state == FRAME_DIRTY)
	{
		mali_err_code err;
		MALI_DEBUG_ASSERT( frame->plbu_heap_held == NULL, ("Per frame init encountered a plbu_heap held by the frame!"));

		/* Swap around the plbu-heaps like we did with the frames, there might not be a 1-2-1 ratio of heaps vs frames */
		frame_builder->plbu_heap_current = (frame_builder->plbu_heap_current + 1 == frame_builder->plbu_heap_count ) ? 0 : frame_builder->plbu_heap_current + 1;
		frame->plbu_heap_held = &frame_builder->plbu_heaps[ frame_builder->plbu_heap_current ];

		frame->order_synch.swap_nr = frame_builder->swap_performed;
		MALI_DEBUG_ASSERT( NULL == frame->order_synch.release_on_finish , ("Err: The new and clean frame has an action pending.") );

		/* initialize plbu */
		err = _mali_frame_builder_per_frame_gp_init( frame_builder );
		if (err != MALI_ERR_NO_ERROR)
		{
			frame->plbu_heap_held = NULL;
			return err;
		}

	}
	else
	{
		MALI_DEBUG_ASSERT( MALI_FALSE ,("Frame state inside _mali_frame_builder_init_per_frame is %d!", (int)frame->state ));
		return MALI_ERR_OUT_OF_MEMORY;
	}

	return MALI_ERR_NO_ERROR;
}

/**
 * Internals of the _mali_frame_builder_use() entrypoint.
 * Allocates and maps a frame pool if not yet done, and sets the frame to dirty. 
 * Also deals with automatic readbacks.
 *
 * Long term plan: honestly, this entire function should only happen as the first drawcall in the frame. 
 * We may want to rename it. 
 */
MALI_STATIC mali_err_code _frame_builder_use_internal( mali_frame_builder *frame_builder, mali_internal_frame *frame )
{

	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	MALI_DEBUG_ASSERT_POINTER( frame );

	/* early out for most common conditions */
	if( frame->state == FRAME_DIRTY && frame->pool_mapped )
	{
		return MALI_ERR_NO_ERROR;
	}

#if MALI_USE_DEFERRED_FRAME_RESET
	if (FRAME_COMPLETE == frame->state)
	{
		if (MALI_TRUE == frame->reset_on_finish)
		{
			_mali_sys_mutex_unlock(frame->mutex);
			_mali_frame_reset(frame);
			_mali_sys_mutex_lock(frame->mutex);
		}
	}
#endif /* MALI_USE_DEFERRED_FRAME_RESET */

	/* NOTE: The calling function _must_ hold the frame->mutex */
	MALI_DEBUG_ASSERT( MALI_ERR_NO_ERROR != _mali_sys_mutex_try_lock(frame->mutex),
	                   ("frame->mutex not locked by caller!") );

	/* Early out if dimensions are 0 */
	MALI_DEBUG_ASSERT(frame == GET_CURRENT_INTERNAL_FRAME( frame_builder ), ("frame builder use_internal frame is not the current frame!"));
	MALI_DEBUG_ASSERT(0 != frame_builder->output_valid,("USE'ing a framebuilder that is not valid"));

	/* do per-frame init if this is the first _use() after a reset */
	if( (frame->state == FRAME_CLEAN) ||
	    (frame->state == FRAME_UNMODIFIED))
	{
		_mali_frame_builder_per_frame_gp_init_pre(frame_builder);
	}

	/* make sure we have a mapped frame pool: a lot of things, including the vertices streams, will be allocated from the frame pool memory */
	if( !frame->pool_mapped )
	{
		/* map the mem pool */
		MALI_CHECK_NO_ERROR( _mali_mem_pool_map( &frame->frame_pool ) );
		frame->pool_mapped = MALI_TRUE;
	}

	/* A clean frame should not read back stuff, as it will destroy its clear values */
	if ( frame->state != FRAME_CLEAN )
	{
		int index;

		/* frame will soon contain unrendered primitives / unprocessed commands */
		frame->state = FRAME_DIRTY;

		/* Using readback_first_drawcall we make sure that we only do a readback the first time it hits per frame.
		 * This "frame" member is set to MALI_TRUE in _mali_frame_alloc and _mali_frame_reset. 
		 */
		if ( frame->readback_first_drawcall )
		{
			/* Do readback where necessary */
			for ( index = 0; index < MALI_READBACK_SURFACES_COUNT; index++ )
			{
				mali_err_code error;
				mali_surface* surface;
				u32 usage;
				surface = _mali_frame_builder_get_readback(frame_builder, index, &usage);
				if( surface == NULL) continue;

				error = _mali_frame_builder_readback( frame_builder, surface, usage, 0, 0, _mali_frame_builder_get_width( frame_builder ), _mali_frame_builder_get_height( frame_builder ) );
				if( MALI_ERR_NO_ERROR != error )
				{
					return error;
				}
			}
			frame->readback_first_drawcall = MALI_FALSE;
		}
	}
	
	/* frame will soon contain unrendered primitives / unprocessed commands */
	frame->state = FRAME_DIRTY;

	return MALI_ERR_NO_ERROR;
}

MALI_EXPORT mali_err_code _mali_frame_builder_use( mali_frame_builder *frame_builder )
{
	mali_internal_frame *frame;
	mali_err_code error = MALI_ERR_NO_ERROR;
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	MALI_DEBUG_ASSERT(frame_builder->output_valid == MALI_TRUE, ("Framebuilder outputs set up incorrectly"));

	frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	/* wait for current frame to finish any mali processing */
	_mali_frame_wait_and_take_mutex( frame );

	error = _frame_builder_use_internal( frame_builder, frame );
	_mali_sys_mutex_unlock( frame->mutex );

	if ( MALI_ERR_NO_ERROR != error )
	{
		return error;
	}

	MALI_SUCCESS;
}

MALI_EXPORT void _mali_frame_builder_clean( mali_frame_builder *frame_builder )
{
	u32 i;
	mali_internal_frame* frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	/* wait for all mali hw processing of the frame to finish before taking the mutex*/
	_mali_frame_wait_and_take_mutex( frame );
	_mali_sys_mutex_unlock( frame->mutex );

	/* set the owners of each framebuilder attachment we are clearing. This prevents a future
	 * readback, overriding the clear */
	for (i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; ++i)
	{
		u32 usage;
		mali_surface *surf = _mali_frame_builder_get_output(frame_builder, i, &usage);

		if (surf == NULL || (usage & MALI_OUTPUT_COLOR)) continue;

		_mali_surface_access_lock(surf);

		if (surf->flags & MALI_SURFACE_FLAG_READ_PENDING) 
		{
			_mali_surface_access_unlock(surf);
			continue;
		}

		if ((usage & MALI_OUTPUT_DEPTH) || (usage & MALI_OUTPUT_STENCIL))
		{
			_mali_frame_builder_discard_surface_write_back(frame_builder, surf, i);
		}

		_mali_surface_access_unlock(surf);
	}

	/* reset the frame. A full clean will remove (overwrite) all contents of the frame */
	_mali_frame_reset( frame );

	/* then set the frame state to CLEAN */
	frame->state = FRAME_CLEAN;

	/* fragment shader stack size */
	frame->fs_stack.start = 0;
	frame->fs_stack.grow  = 0;

	frame->plbu_heap_reset_on_job_start = MALI_TRUE;
	_mali_frame_set_inc_render_on_flush( frame_builder, MALI_FALSE );
}

MALI_EXPORT void _mali_frame_builder_set_clearstate( mali_frame_builder *frame_builder, u32 buffer_mask )
{
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	/* Setting the cleared buffers specified in buffer_mask to the buffer state to "clear to current clear color" */
	if( buffer_mask & MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_ALL_CHANNELS )
	{
		frame_builder->buffer_state_per_plane &= ~MALI_COLOR_PLANE_MASK;
		frame_builder->buffer_state_per_plane |= MALI_COLOR_PLANE_CLEAR_TO_COLOR;
	}
	if( buffer_mask & MALI_FRAME_BUILDER_BUFFER_BIT_DEPTH )
	{
		frame_builder->buffer_state_per_plane &= ~MALI_DEPTH_PLANE_MASK;
 		frame_builder->buffer_state_per_plane |= MALI_DEPTH_PLANE_CLEAR_TO_COLOR;
	}
	if( buffer_mask & MALI_FRAME_BUILDER_BUFFER_BIT_STENCIL )
	{
		frame_builder->buffer_state_per_plane &= ~MALI_STENCIL_PLANE_MASK;
		frame_builder->buffer_state_per_plane |= MALI_STENCIL_PLANE_CLEAR_TO_COLOR;
	}
}

MALI_EXPORT u32 _mali_frame_builder_get_clearstate( mali_frame_builder *frame_builder )
{
	u32 bit_mask = 0;

	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	if( (frame_builder->buffer_state_per_plane & MALI_COLOR_PLANE_CLEAR_TO_COLOR) || (frame_builder->buffer_state_per_plane & MALI_COLOR_PLANE_UNDEFINED) ) 	bit_mask |= MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_ALL_CHANNELS;
	if( (frame_builder->buffer_state_per_plane & MALI_DEPTH_PLANE_CLEAR_TO_COLOR) || (frame_builder->buffer_state_per_plane & MALI_DEPTH_PLANE_UNDEFINED) ) 	bit_mask |= MALI_FRAME_BUILDER_BUFFER_BIT_DEPTH;
	if( (frame_builder->buffer_state_per_plane & MALI_STENCIL_PLANE_CLEAR_TO_COLOR) || (frame_builder->buffer_state_per_plane & MALI_STENCIL_PLANE_UNDEFINED) ) 	bit_mask |= MALI_FRAME_BUILDER_BUFFER_BIT_STENCIL;

	return bit_mask;
}

MALI_EXPORT void _mali_frame_builder_reset( mali_frame_builder *frame_builder )
{
	mali_internal_frame* frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	/* wait for all mali hw processing of the frame to finish before taking the mutex*/
	_mali_frame_wait_and_take_mutex( frame );
	_mali_sys_mutex_unlock( frame->mutex );

	/* reset the frame */
	_mali_frame_reset( frame );

	/* fragment shader stack size */
	frame->fs_stack.start = 0;
	frame->fs_stack.grow  = 0;

	frame->plbu_heap_reset_on_job_start = MALI_TRUE;
	_mali_frame_set_inc_render_on_flush( frame_builder, MALI_FALSE );

	frame_builder->preserve_multisample = MALI_FALSE;
}

/**
 * Update the frame builders fragment stack allocation, i.e. grow or shrink the existing stack buffer
 * depending on the requirements of the current frame.
 * Called once per frame.
 */
MALI_STATIC mali_err_code _fragment_stack_alloc( mali_frame_builder *frame_builder )
{
	int                  stack_entries, stack_size, prev_stack_size, num_stacks;
	mali_internal_frame* frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	/* total number of stack entries needed */
	stack_entries = frame->fs_stack.start + frame->fs_stack.grow;

	if( 0 == stack_entries )
	{
		/* no stack needed. Free any existing stack mem and return */
		_mali_mem_free( frame->fs_stack.fs_stack_mem );
		frame->fs_stack.fs_stack_mem = MALI_NO_HANDLE;

		return MALI_ERR_NO_ERROR;
	}

#if defined(USING_MALI450)
	/* one stack per core */
	num_stacks = _mali_pp_get_num_cores();
#else
	/* one stack per master tile list */
	num_stacks = frame_builder->curr_pp_split_count;
#endif /* defined(USING_MALI450) */

	/* each stack entry is 8bytes and we have 128 threads */
	stack_size = stack_entries * 8 * 128 * num_stacks;

	/* get the size of the already allocated stack */
	prev_stack_size = ( MALI_NO_HANDLE == frame->fs_stack.fs_stack_mem ) ? 0 : _mali_mem_size_get( frame->fs_stack.fs_stack_mem );

	/* if the new stack is larger than the old, or less than 50% of the old size, we allocate a new */
	if( (stack_size > prev_stack_size) || (prev_stack_size > 2*stack_size) )
	{
		/* free old */
		_mali_mem_free( frame->fs_stack.fs_stack_mem );
		frame->fs_stack.fs_stack_mem = MALI_NO_HANDLE;

		/* allocate new */
		frame->fs_stack.fs_stack_mem = _mali_mem_alloc( frame_builder->base_ctx, stack_size, 64, MALI_PP_READ | MALI_PP_WRITE );
		if ( MALI_NO_HANDLE == frame->fs_stack.fs_stack_mem ) return MALI_ERR_OUT_OF_MEMORY;
	}

	return MALI_ERR_NO_ERROR;
}


/**
 * Sets the writeback "clip" box in the registers of a pp_job
 */
MALI_STATIC void _set_writeback_bounding_box( mali_pp_job_handle pp_job, u32 width, u32 height )
{
	MALI_DEBUG_ASSERT_HANDLE( pp_job );
	MALI_DEBUG_ASSERT( (width != 0) && (height != 0), ("zero-sized buffers not allowed\n") );
	/* assuring that the sizes are within the limits set in 3.5.11 and .12 of the M200 TRM */
	MALI_DEBUG_ASSERT( (width & ~((1<<14)-1)) == 0, ("width too large\n") );
	MALI_DEBUG_ASSERT( (height & ~((1<<14)-1)) == 0, ("height too large\n") );

	_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_BOUNDING_BOX_LEFT_RIGHT, width-1);
	_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_BOUNDING_BOX_BOTTOM,     height-1);
}

/**
 * Converts a usage containing multiple MALI_OUTPUT_DOWNSCALE bits set, to scale factor fitting a WBX AA_FORMAT register value
 *
 * Example:
 * MALI_OUTPUT_DOWNSAMPLE_X_2X | MALI_OUTPUT_DOWNSAMPLE_Y_2X ==> M200_WBx_AA_FORMAT_4X (aka 2)
 *
 */
MALI_STATIC u32 usage_to_wbx_register_aa_format( u32 usage )
{
	u32 x = 0;
	u32 y = 0;

	MALI_DEBUG_ASSERT(
	   (!! (usage & MALI_OUTPUT_DOWNSAMPLE_X_2X )) +
	   (!! (usage & MALI_OUTPUT_DOWNSAMPLE_X_4X )) +
	   (!! (usage & MALI_OUTPUT_DOWNSAMPLE_X_8X ))  <= 1, ("Only one x scale bit may be set!"));
	MALI_DEBUG_ASSERT(
	   (!! (usage & MALI_OUTPUT_DOWNSAMPLE_Y_2X )) +
	   (!! (usage & MALI_OUTPUT_DOWNSAMPLE_Y_4X )) +
	   (!! (usage & MALI_OUTPUT_DOWNSAMPLE_Y_8X )) +
	   (!! (usage & MALI_OUTPUT_DOWNSAMPLE_Y_16X )) <= 1, ("Only one y scale bit may be set!"));

	/* fill out log2 scale factors for x and y */
	if(usage & MALI_OUTPUT_DOWNSAMPLE_X_2X) x=1;
	if(usage & MALI_OUTPUT_DOWNSAMPLE_X_4X) x=2;
	if(usage & MALI_OUTPUT_DOWNSAMPLE_X_8X) x=3;
	if(usage & MALI_OUTPUT_DOWNSAMPLE_Y_2X) y=1;
	if(usage & MALI_OUTPUT_DOWNSAMPLE_Y_4X) y=2;
	if(usage & MALI_OUTPUT_DOWNSAMPLE_Y_8X) y=3;
	if(usage & MALI_OUTPUT_DOWNSAMPLE_Y_16X) y=4;


#if defined(USING_MALI400) || defined(USING_MALI450)
	/* mali400 support any combination of these */
	return (x << 8) | (y << 12);
#endif
#ifdef USING_MALI200
	/* mali200 defines the last 3 bits according to these rules:
	 * (Taken from the TRM, under the AA_FORMAT register state)
	 *
	 *  AA format       AA_X   AA_Y
	 *  ---------------------------
	 *          0          0      0
	 *          1          0      1
	 *          2          1      1
	 *          3          1      2
	 *          4          2      2
	 *          5          2      3
	 *          6          3      3
	 *          7          3      4
	 *
	 * This means aa_format = aa_x + aa_y, but with some heavy combination limits:
	 * (aa_y - aa_x) must be 0 or 1.
	 */
	MALI_DEBUG_ASSERT( (y - x == 1) || (y - x == 0), ("M200 impose limits of AA differences"));
	return x + y;
#endif

}


/**
 * Configures one of Mali200s writeback units by setting register values in a pp_job based on render
 * attachment settings.
 * Called once for each writeback unit (3) per frame.
 * @param render_attachment is the governing render attachment for this writeback unit, it is allowed to be NULL
 */
MALI_STATIC u32 _writeback_unit_setup( mali_pp_job_handle           pp_job,
                                       u32                          wb_unit,
                                       struct mali_surface         *render_target,
									   u32                          usage,
                                       mali_bool                    enable_bbox )
{
	u32                                   aa_format = 0;
	u32                                   fb_ptr, wb_flags = 0;

	MALI_DEBUG_ASSERT_HANDLE( pp_job );
	MALI_DEBUG_ASSERT_RANGE( (int)wb_unit, 0, 2 );

	if( render_target == NULL )
	{
		_m200_wb_reg_write( pp_job, wb_unit, M200_WBx_REG_SOURCE_SELECT, M200_WBx_SOURCE_NONE );
		return wb_unit;
	}

	_mali_surface_access_lock( render_target );

#if HARDWARE_ISSUE_4582
	/* layout INTERLEAVED BLOCKS does not support a bounding box, and it must be disabled for this rendertarget */
	if( render_target->format.pixel_layout == MALI_PIXEL_LAYOUT_INTERLEAVED_BLOCKS ) enable_bbox = MALI_FALSE;
#endif

	/* find aa format. */
	aa_format = usage_to_wbx_register_aa_format(usage);

	fb_ptr = (u32) _mali_mem_mali_addr_get( render_target->mem_ref->mali_memory, render_target->mem_offset );
	MALI_DEBUG_ASSERT_ALIGNMENT( fb_ptr, 8 ); /* the WBx unit requires this alignment */
	MALI_DEBUG_ASSERT_ALIGNMENT( fb_ptr, MALI200_SURFACE_ALIGNMENT); /* but all read surfaces requires this alignment */

	_m200_wb_reg_write(pp_job, wb_unit, M200_WBx_REG_TARGET_ADDR,   fb_ptr);
	_m200_wb_reg_write(pp_job, wb_unit, M200_WBx_REG_TARGET_LAYOUT, render_target->format.pixel_layout );

	if( MALI_PIXEL_LAYOUT_INTERLEAVED_BLOCKS == render_target->format.pixel_layout )
	{
		/* in block-interleaved mode scanline length = number of blocks */
		_m200_wb_reg_write(pp_job, wb_unit, M200_WBx_REG_TARGET_SCANLINE_LENGTH, (render_target->format.width + 0xf) >> 4 );
	}
	else
	{
		MALI_DEBUG_ASSERT(render_target->format.pixel_layout == MALI_PIXEL_LAYOUT_LINEAR, ("The only 'else' part we have is linear mode"));
		MALI_DEBUG_ASSERT(MALI200_SURFACE_PITCH_ALIGNMENT_LINEAR_LAYOUT % 8 == 0, ("MALI200_SURFACE_PITCH_ALIGNMENT_LINEAR_LAYOUT must be divisible by 8 on M200"));
		_m200_wb_reg_write(pp_job, wb_unit, M200_WBx_REG_TARGET_SCANLINE_LENGTH, render_target->format.pitch / 8);
	}

	_m200_wb_reg_write(pp_job, wb_unit, M200_WBx_REG_TARGET_AA_FORMAT,    aa_format);
	_m200_wb_reg_write(pp_job, wb_unit, M200_WBx_REG_TARGET_PIXEL_FORMAT, render_target->format.pixel_format);

	MALI_DEBUG_ASSERT( usage & (MALI_OUTPUT_COLOR | MALI_OUTPUT_DEPTH | MALI_OUTPUT_STENCIL), ("At least one MALI_OUTPUT usage bit must be set"));
	MALI_DEBUG_ASSERT(
		!!(usage & (MALI_OUTPUT_COLOR)) +
		!!(usage & (MALI_OUTPUT_DEPTH | MALI_OUTPUT_STENCIL))
		< 2, ("Either COLOR or DEPTH/STENCIL must be set"));

	if(usage & MALI_OUTPUT_COLOR)
	{
		_m200_wb_reg_write(pp_job, wb_unit, M200_WBx_REG_SOURCE_SELECT, M200_WBx_SOURCE_ARGB);
	} else {
		_m200_wb_reg_write(pp_job, wb_unit, M200_WBx_REG_SOURCE_SELECT, M200_WBx_SOURCE_Z_STENCIL);
	}

	if ( render_target->format.red_blue_swap )
	{
		wb_flags |= M200_WBx_TARGET_FLAGS_SWAP_RED_BLUE_ENABLE;
	}
	if ( render_target->format.reverse_order )
	{
		wb_flags |= M200_WBx_TARGET_FLAGS_INV_COMPONENT_ORDER_ENABLE;
	}
	if ( usage & MALI_OUTPUT_WRITE_DIRTY_PIXELS_ONLY )
	{
		wb_flags |= M200_WBx_TARGET_FLAGS_DIRTY_BIT_ENABLE;
	}

	if( enable_bbox ) wb_flags |= M200_WBx_TARGET_FLAGS_BOUNDING_BOX_ENABLE;

	_m200_wb_reg_write(pp_job, wb_unit, M200_WBx_REG_TARGET_FLAGS, wb_flags);
	_m200_wb_reg_write(pp_job, wb_unit, M200_WBx_REG_GLOBAL_TEST_ENABLE, 0);

	if ( !(usage & MALI_OUTPUT_MRT) )
	{
		/* No MRT so disable it */
		_m200_wb_reg_write(pp_job, wb_unit, M200_WBx_REG_MRT_ENABLE, 0);
	}
	else
	{
		/* Set up multiple render targets (MRT) */
		u32 offset = render_target->datasize;
		u32 mrt_mask = 0xF; /* enable all four outputs */

		_m200_wb_reg_write(pp_job, wb_unit, M200_WBx_REG_MRT_ENABLE, mrt_mask);
		_m200_wb_reg_write(pp_job, wb_unit, M200_WBx_REG_MRT_OFFSET, offset);
	}
	_mali_surface_access_unlock(render_target);

	return wb_unit + 1;
}

/**
 * Sets all the pp_job registers that depend on render_attachment settings (clear values,
 * framebuffer formats, dimensions etc).
 * Called once per frame.
 */
MALI_STATIC void _render_attachment_setup( mali_frame_builder          *frame_builder,
                                                mali_pp_job_handle           pp_job )
{
	mali_bool use_bbox;
	u32       next_unused_wb_unit = 0;
	u32       i;

	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	MALI_DEBUG_ASSERT_HANDLE( pp_job );

	/* set the different clear values in the pp job registers */
	if(_mali_frame_builder_get_fp16_flag (frame_builder) )
	{
		/* In FP16 mode, the first two of the four clear values hold the FP16 clear value for all channels.
		 * Note that the value in color_clear_value is already a FP16 bitvalue in this case. */
		u32 rg_color = (frame_builder->color_clear_value[0]) | (frame_builder->color_clear_value[1] << 16);
		u32 ba_color = (frame_builder->color_clear_value[2]) | (frame_builder->color_clear_value[3] << 16);

		_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_ABGR_CLEAR_VALUE_0,  rg_color);
		_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_ABGR_CLEAR_VALUE_1,  ba_color);
		_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_ABGR_CLEAR_VALUE_2,  0x0);
		_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_ABGR_CLEAR_VALUE_3,  0x0);
		_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_Z_CLEAR_VALUE,       frame_builder->depth_clear_value );
		_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_STENCIL_CLEAR_VALUE, frame_builder->stencil_clear_value );
	} else {
		/* In non-FP16 mode, the four clear colors each set the clear color of a subsample. 
		 * Each setter accepts an u32 value holding 8 bits of each channel.
		 * Note that the value in color_clear_value is a 16bit value in this case.
		 * It must be shifted down by 8 bits to fit. */
		u32 color =   (((frame_builder->color_clear_value[0] >> 8) & 0xFF)      )     /* red */
		            | (((frame_builder->color_clear_value[1] >> 8) & 0xFF) << 8 )     /* green */
		            | (((frame_builder->color_clear_value[2] >> 8) & 0xFF) << 16)     /* blue */
		            | (((frame_builder->color_clear_value[3] >> 8) & 0xFF) << 24)     /* alpha */
					;
	
		_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_ABGR_CLEAR_VALUE_0,  color);
		_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_ABGR_CLEAR_VALUE_1,  color);
		_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_ABGR_CLEAR_VALUE_2,  color);
		_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_ABGR_CLEAR_VALUE_3,  color);
		_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_Z_CLEAR_VALUE,       frame_builder->depth_clear_value );
		_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_STENCIL_CLEAR_VALUE, frame_builder->stencil_clear_value );
	}

	use_bbox = frame_builder->bounding_box_used;

	if( use_bbox ) _set_writeback_bounding_box( pp_job, frame_builder->bounding_box_width, frame_builder->bounding_box_height);

	/* set up the 3 writeback units.  We always use the writeback units in order (wb0 before wb1, etc.) */
	for ( i=0; i < MALI200_WRITEBACK_UNIT_COUNT; i++ )
	{
		mali_surface* surface = frame_builder->output_buffers[i].buffer;
		u32 usage = frame_builder->output_buffers[i].usage;

		mali_bool use_bbox_local = frame_builder->bounding_box_enable[i] & use_bbox;

		#if HARDWARE_ISSUE_4472
		if( usage & (
		    MALI_OUTPUT_DOWNSAMPLE_X_2X |
			MALI_OUTPUT_DOWNSAMPLE_X_4X |
			MALI_OUTPUT_DOWNSAMPLE_X_8X |
			MALI_OUTPUT_DOWNSAMPLE_Y_2X |
			MALI_OUTPUT_DOWNSAMPLE_Y_4X |
			MALI_OUTPUT_DOWNSAMPLE_Y_8X |
			MALI_OUTPUT_DOWNSAMPLE_Y_16X
			       )
		  )
		{
			use_bbox_local = MALI_FALSE;
		}
		#endif

		next_unused_wb_unit = _writeback_unit_setup( pp_job,
		                                             next_unused_wb_unit,
													 surface, usage,
		                                             use_bbox_local);
	}

	MALI_DEBUG_ASSERT_RANGE( (int)next_unused_wb_unit, 0, 3 );

}



/**
 * Set all pp_job registers necessary to render a frame.
 * Called once per frame.
 */
MALI_STATIC void _pp_job_setup_specific_registers(mali_internal_frame *frame, mali_pp_job_handle pp_job, int job_index)
{
	MALI_DEBUG_ASSERT_POINTER( frame );
	MALI_DEBUG_ASSERT_POINTER( frame->tilelists );
	MALI_DEBUG_ASSERT_HANDLE( pp_job );
	MALI_DEBUG_ASSERT_RANGE( job_index, 0, MALI_MAX_PP_SPLIT_COUNT - 1 );

/* DLBU used instead of master tile list for Mali-450 */
#if !defined(USING_MALI450)
	/* set master tile list */
	_m200_frame_reg_write_specific(pp_job,
		job_index,
		M200_FRAME_REG_REND_LIST_ADDR,
		_mali_mem_mali_addr_get(frame->tilelists->master_tile_list_mem, frame->tilelists->pp_master_tile_list_offsets[job_index] * sizeof(mali_tilelist_cmd))
	);
#endif /* !defined(USING_MALI450) */

	/* set fragment stack address */
	if ( MALI_NO_HANDLE != frame->fs_stack.fs_stack_mem )
	{
		int stack_entries = frame->fs_stack.start + frame->fs_stack.grow;
		int stack_size = stack_entries * 8 * 128;

		_m200_frame_reg_write_specific(
			pp_job,
			job_index,
			M200_FRAME_REG_FS_STACK_ADDR,
			_mali_mem_mali_addr_get( frame->fs_stack.fs_stack_mem, stack_size * job_index)
		);

		/* @@@@ Really a common, but falls under the if test, so best kept here to avoid multiple if tests? */
		_m200_frame_reg_write_common(
			pp_job,
			M200_FRAME_REG_FS_STACK_SIZE_AND_INIT_VAL,
			( frame->fs_stack.start << 16 ) | (frame->fs_stack.start + frame->fs_stack.grow)
		);
	}
}

MALI_STATIC void _pp_job_setup_common_registers( mali_frame_builder *frame_builder, mali_internal_frame *frame, mali_pp_job_handle pp_job )
{
	MALI_DEBUG_ASSERT_POINTER( frame );
	MALI_DEBUG_ASSERT_HANDLE( pp_job );

	/* setup render attachments */
	_render_attachment_setup(frame_builder, pp_job);

	/* set rsw base */
	_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_REND_RSW_BASE, _mali_mem_mali_addr_get( frame_builder->dummy_rsw_mem, 0) );

	{
		mali_bool earlyz_enable = MALI_TRUE;
		mali_bool earlyz_disable2 = MALI_FALSE;

#if HARDWARE_ISSUE_4924
		earlyz_enable = MALI_FALSE;
#endif

#if HARDWARE_ISSUE_3781
		earlyz_disable2 = MALI_TRUE;
#endif
		_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_FEATURE_ENABLE,
			((frame_builder->properties & MALI_FRAME_BUILDER_PROPS_ORIGO_LOWER_LEFT) ? M200_FEATURE_ORIGIN_LOWER_LEFT : 0) |
			(earlyz_enable ? M200_FEATURE_EARLYZ_ENABLE :  0) |
			(earlyz_disable2 ? M200_FEATURE_EARLYZ_DISABLE2 : 0) |
			(_mali_frame_builder_get_fp16_flag (frame_builder) ? M200_FEATURE_FP_TILEBUF_ENABLE : 0)
		);
#if !defined(USING_MALI200)
		/*
		** Mali-400 MP introduced HW fixes, so we don't need SW workarounds:
		** M400_SCALING_CONFIG_FLIP_FRAGCOORD - this is actually the same as M200_FEATURE_ORIGIN_LOWER_LEFT above
		** M400_SCALING_CONFIG_FRAGCOORD_SCALE_ENABLE - instead of workaround for bugzilla 4374 (gl_mali_FragCoordScale)
		** M400_SCALING_CONFIG_FLIP_POINT_SPRITES - instead of workaround for bugzilla 3823 (gl_mali_PointCoordScaleBias)
		** M400_SCALING_CONFIG_DERIVATIVE_SCALE_ENABLE - instead of workaround for bugzilla 4658 (gl_mali_DerivativeScale)
		** M400_SCALING_CONFIG_FLIP_DERIVATIVE_Y - instead of workaround for bugzilla 4658 (gl_mali_DerivativeScale)
		*/

		/* Note: The scalefactor is pulled from the largest WBx scalefactor. For regular use-cases, this makes sense
		 * as all targets will carry the same supersample factor. If you ever figure that this should rather be
		 * the common ground or something, feel free to do so. */

		MALI_DEBUG_ASSERT( frame_builder->output_log2_scale_x < 8, ("x scale factor too large!") );
		MALI_DEBUG_ASSERT( frame_builder->output_log2_scale_y < 8, ("y scale factor too large!") );

		_m200_frame_reg_write_common(pp_job, M400_FRAME_REG_SCALING_CONFIG,
			(frame_builder->output_log2_scale_x << 16) |
			(frame_builder->output_log2_scale_y << 20) |
			M400_SCALING_CONFIG_FRAGCOORD_SCALE_ENABLE |
			M400_SCALING_CONFIG_DERIVATIVE_SCALE_ENABLE |
			((frame_builder->properties & MALI_FRAME_BUILDER_PROPS_ORIGO_LOWER_LEFT) ? M400_SCALING_CONFIG_FLIP_FRAGCOORD | M400_SCALING_CONFIG_FLIP_DERIVATIVE_Y : M400_SCALING_CONFIG_FLIP_POINT_SPRITES)
		);
#endif
	}

#if HARDWARE_ISSUE_4490
	/* set the subpixel specifier to 4 bit */
	_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_SUBPIXEL_SPECIFIER, 127-4 );
#else
	/* set the subpixel specifier to the highest value that is guaranteed to work in all resolutions; 8 bits */
	_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_SUBPIXEL_SPECIFIER, 127-8 );
#endif

	/* Set the pixel center for the Position Register operation to ( 0.5, height - 0.5) for OpenGLES according to 3.4.17 of M200 TRM */
	{
		/* fix for defect 4374 - the offset must be set to height -0.5, but height is given by the supersample factor */
		_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_ORIGIN_OFFSET_X, 1 );
		_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_ORIGIN_OFFSET_Y,
			(frame_builder->properties & MALI_FRAME_BUILDER_PROPS_ORIGO_LOWER_LEFT) ? (2*frame_builder->output_height)-1 : 1
		);
	}

	_m200_frame_reg_write_common(pp_job, M200_FRAME_REG_TIEBREAK_MODE, (frame_builder->properties & MALI_FRAME_BUILDER_PROPS_ORIGO_LOWER_LEFT) ? 1 : 0);

#if !defined(USING_MALI200)
	MALI_DEBUG_ASSERT_POINTER( frame->tilelists );
	/* set up tile list primitive format, must match what is used by the PLBU */
	_m200_frame_reg_write_common(pp_job, M400_FRAME_REG_PLIST_CONFIG,
	                               GP_PLBU_CONF_REGS_TILE_SIZE_WITH_FORMAT(frame->tilelists->binning_scale_x, frame->tilelists->binning_scale_y, frame->tilelists->polygonlist_format));

	/* set up tilebuffer format to match output format. This means the per-primitive dithering targets the right color space. */
	{
		int i;
		for ( i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; i++)
		{
			struct mali_surface* render_target = frame_builder->output_buffers[i].buffer;
			u32 usage = frame_builder->output_buffers[i].usage;

			/* use the format of the first color target in the writeback units */
			if(	render_target && ( MALI_OUTPUT_COLOR & usage ) )
			{
				/* the default tilebuffer format is 8888 */
				u32 code = 0x00008888;
#if 1
				/* Handling the correct tile buffers for dithering */
				switch (render_target->format.pixel_format)
				{
					case	MALI_PIXEL_FORMAT_R5G6B5:
						code = 0x00008565;
						break;
					case	MALI_PIXEL_FORMAT_A1R5G5B5:
						code = 0x00008555;
						break;
					case	MALI_PIXEL_FORMAT_A4R4G4B4:
						code = 0x00008444;
						break;
					default:
						;
				}
#endif
				frame_builder->tilebuffer_color_format = code;
				_m200_frame_reg_write_common(pp_job, M400_FRAME_REG_TILEBUFFER_BITS, code);
				break;
			}
		}
	}
#endif
}

/**
 * Store the necessary PLBU state data so that a later gp_job can continue working where the current
 * job left off.
 * Called once for each "incremental render", i.e. when M200 is set up to render the tile lists that
 * have been produced so far, before we continue building on the same lists.
 */
MALI_STATIC mali_err_code _gp_job_context_switch_out( mali_frame_builder *frame_builder, mali_gp_job_handle gp_job )
{
	mali_mem_handle  stack;
	mali_gp_plbu_cmd* cmds;
	u32 cmd_index = 0;

#if HARDWARE_ISSUE_7320
#define MAX_NUM_CMDS (12 + (2*HARDWARE_ISSUE_7320_OUTSTANDING_WRITES))
#else
#define MAX_NUM_CMDS 12
#endif


	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	MALI_DEBUG_ASSERT_HANDLE( gp_job );

	cmds = _mali_gp_job_plbu_cmds_reserve( gp_job, MAX_NUM_CMDS );

    MALI_CHECK_NON_NULL(cmds, MALI_ERR_OUT_OF_MEMORY);

	stack = GET_CURRENT_INTERNAL_FRAME( frame_builder )->gp_context_stack;
	MALI_DEBUG_ASSERT_HANDLE( stack );

#if HARDWARE_ISSUE_4175
	/**
	 * Dummy commands are required since the heap start address that we read
	 * in the first 'proper' command is not guaranteed to be updated unless the
	 * PLBU pipeline is flushed.
	 *
	 * In other words: we need to waste min. 5 cycles here.
	 *
	 * Set RSW/Vertex Base Address commands will not have any effect on output.
	 * (they only have effect when a vertex is processed - and we are done with
	 * vertex processing at this point.

	 * Each Set Base Address command will take 1 cycle per tile, so we need to
	 * add 5 commands when rendering to a single tile. If the number of tiles is >= 5,
	 * we just need a single command.
	 */
	cmds[ cmd_index++ ] = GP_PLBU_COMMAND_RSW_VERTEX_ADDR( 0, 0 );
	if ( GET_CURRENT_INTERNAL_FRAME( frame_builder )->tilelists->tile_pointers_to_load < 5 )
	{
		cmds[ cmd_index++ ] = GP_PLBU_COMMAND_RSW_VERTEX_ADDR( 0, 0 );
		cmds[ cmd_index++ ] = GP_PLBU_COMMAND_RSW_VERTEX_ADDR( 0, 0 );
		cmds[ cmd_index++ ] = GP_PLBU_COMMAND_RSW_VERTEX_ADDR( 0, 0 );
		cmds[ cmd_index++ ] = GP_PLBU_COMMAND_RSW_VERTEX_ADDR( 0, 0 );
	}
#endif /* HARDWARE_ISSUE_4175 */

	/* store current heap start - end addresses */
	cmds[ cmd_index++ ] = GP_PLBU_COMMAND_STORE_CONF_REG( _mali_mem_mali_addr_get( stack, (0*4)), GP_PLBU_CONF_REG_HEAP_START_ADDR );
	cmds[ cmd_index++ ] = GP_PLBU_COMMAND_STORE_CONF_REG( _mali_mem_mali_addr_get( stack, (1*4)), GP_PLBU_CONF_REG_HEAP_END_ADDR );

	/* store the viewportbox settings */
	cmds[ cmd_index++ ] = GP_PLBU_COMMAND_STORE_CONF_REG( _mali_mem_mali_addr_get( stack, (2*4)), GP_PLBU_CONF_REG_SCISSOR_LEFT );
	cmds[ cmd_index++ ] = GP_PLBU_COMMAND_STORE_CONF_REG( _mali_mem_mali_addr_get( stack, (3*4)), GP_PLBU_CONF_REG_SCISSOR_RIGHT );
	cmds[ cmd_index++ ] = GP_PLBU_COMMAND_STORE_CONF_REG( _mali_mem_mali_addr_get( stack, (4*4)), GP_PLBU_CONF_REG_SCISSOR_TOP );
	cmds[ cmd_index++ ] = GP_PLBU_COMMAND_STORE_CONF_REG( _mali_mem_mali_addr_get( stack, (5*4)), GP_PLBU_CONF_REG_SCISSOR_BOTTOM );
#if HARDWARE_ISSUE_7320
	{
		int i;
		for(i = 0; i < HARDWARE_ISSUE_7320_OUTSTANDING_WRITES; i++)
		{
			cmds[ cmd_index++ ] = GP_PLBU_COMMAND_STORE_CONF_REG( _mali_mem_mali_addr_get( stack, (5*4)), GP_PLBU_CONF_REG_SCISSOR_BOTTOM );
		}
	}
#endif

	/* and flush the pointer array */
	cmds[ cmd_index++ ] = GP_PLBU_COMMAND_FLUSH_POINTER_CACHE();
#if HARDWARE_ISSUE_7320
	{
		/* there are 21 writes per FLUSH_POINTER_CACHE. */
		int i;
		for(i = 0; i < ((HARDWARE_ISSUE_7320_OUTSTANDING_WRITES + 20) / 21); i++)
		{
			cmds[ cmd_index++ ] = GP_PLBU_COMMAND_FLUSH_POINTER_CACHE();
		}
	}
#endif
	/* currently the minimum number of cmds is 7 */
	MALI_DEBUG_ASSERT_RANGE( cmd_index, 7, MAX_NUM_CMDS );

	/* Commit the commands to the PLBU list */
	_mali_gp_job_plbu_cmds_confirm(gp_job, cmd_index );

#undef MAX_NUM_CMDS

	return MALI_ERR_NO_ERROR;
}

/**
 * Retrieve the necessary PLBU state data stored by _context_switch_out() so that the current gp_job
 * can continue working where a previous job left off.
 * Called once for each "incremental render", i.e. when M200 is set up to render the tile lists that
 * have been produced so far, before we continue building on the same lists.
 */
MALI_STATIC mali_err_code _gp_job_context_switch_in( mali_frame_builder *frame_builder, mali_gp_job_handle gp_job )
{
	mali_mem_handle      stack;
	mali_gp_plbu_cmd*     cmds;

	/* Init of viewportbox[] to stop an incorrect warning from Coverity as it doesn't handle unions correctly */
	union {
		float f;
		u32   i;
	} viewportbox[4] = { {0}, {0}, {0}, {0}};

	mali_internal_frame* frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	MALI_DEBUG_ASSERT_HANDLE( gp_job );

	cmds = _mali_gp_job_plbu_cmds_reserve( gp_job, 16 );

    MALI_CHECK_NON_NULL(cmds, MALI_ERR_OUT_OF_MEMORY);

	stack = frame->gp_context_stack;
	MALI_DEBUG_ASSERT_HANDLE( stack );

	/* the tile list scale must be set before any tile list commands are generated */
	cmds[ 0 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( MALI_TILELIST_CHUNK_FORMAT << 8, GP_PLBU_CONF_REG_PARAMS);

	/* tile binning downscale */
	{
		MALI_DEBUG_ASSERT_POINTER(frame->tilelists);
#if defined(USING_MALI200)
		cmds[ 1 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( GP_PLBU_CONF_REGS_TILE_SIZE(frame->tilelists->binning_scale_x, frame->tilelists->binning_scale_y), GP_PLBU_CONF_REG_TILE_SIZE);
#else
		cmds[ 1 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( GP_PLBU_CONF_REGS_TILE_SIZE_WITH_FORMAT(frame->tilelists->binning_scale_x, frame->tilelists->binning_scale_y, frame->tilelists->polygonlist_format), GP_PLBU_CONF_REG_TILE_SIZE);
#endif
	}

	/* starting from r0p2 the screen size is set using master tile dimensions */
	cmds[ 2 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( 0 | ( (frame->tilelists->master_tile_height - 1) << 8 ) | ( 0 << 16 ) |
	                                            ( (frame->tilelists->master_tile_width - 1) << 24 ), GP_PLBU_CONF_REG_VIEWPORT );

	/* width for tile iterator */
	cmds[ 3 ] = GP_PLBU_COMMAND_PREPARE_FRAME( frame->tilelists->slave_tile_width );

	/* load the pointer array into the PLBU */
	cmds[ 4 ] = GP_PLBU_COMMAND_POINTER_ARRAY_BASE_ADDR_TILE_COUNT( _mali_mem_mali_addr_get( frame->tilelists->pointer_array_mem, 0 ), frame->tilelists->tile_pointers_to_load );

	/* load current heap start - end addresses */
	cmds[ 5 ] = GP_PLBU_COMMAND_LOAD_CONF_REG( _mali_mem_mali_addr_get( stack, (0*4)), GP_PLBU_CONF_REG_HEAP_START_ADDR );
	cmds[ 6 ] = GP_PLBU_COMMAND_LOAD_CONF_REG( _mali_mem_mali_addr_get( stack, (1*4)), GP_PLBU_CONF_REG_HEAP_END_ADDR );

	/* Setting initial viewportbox
	 * Using unions since the binary representation of the floats have to be written to the PLBUs config registers
	 */
	viewportbox[ 0 ].f = 0.f;
	viewportbox[ 1 ].f = frame_builder->vpbox.right;
	viewportbox[ 2 ].f = 0.f;
	viewportbox[ 3 ].f = frame_builder->vpbox.bottom;

	cmds[ 7 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( viewportbox[0].i, GP_PLBU_CONF_REG_SCISSOR_LEFT );
	cmds[ 8 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( viewportbox[1].i, GP_PLBU_CONF_REG_SCISSOR_RIGHT );
	cmds[ 9 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( viewportbox[2].i, GP_PLBU_CONF_REG_SCISSOR_TOP );
	cmds[ 10 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( viewportbox[3].i, GP_PLBU_CONF_REG_SCISSOR_BOTTOM );

	/* load the contexts viewportbox settings */
	cmds[ 11 ] = GP_PLBU_COMMAND_LOAD_CONF_REG( _mali_mem_mali_addr_get( stack, (2*4)), GP_PLBU_CONF_REG_SCISSOR_LEFT );
	cmds[ 12 ] = GP_PLBU_COMMAND_LOAD_CONF_REG( _mali_mem_mali_addr_get( stack, (3*4)), GP_PLBU_CONF_REG_SCISSOR_RIGHT );
	cmds[ 13 ] = GP_PLBU_COMMAND_LOAD_CONF_REG( _mali_mem_mali_addr_get( stack, (4*4)), GP_PLBU_CONF_REG_SCISSOR_TOP );
	cmds[ 14 ] = GP_PLBU_COMMAND_LOAD_CONF_REG( _mali_mem_mali_addr_get( stack, (5*4)), GP_PLBU_CONF_REG_SCISSOR_BOTTOM );
	/* and the scissorbox */
	cmds[ 15 ] = GP_PLBU_COMMAND_SET_SCISSOR( frame_builder->scissor.top,
	                                          frame_builder->scissor.bottom,
	                                          frame_builder->scissor.left,
	                                          frame_builder->scissor.right );

	/* Commit the commands to the PLBU list */
	_mali_gp_job_plbu_cmds_confirm( gp_job, 16 );

	return MALI_ERR_NO_ERROR;
}
/**
 * Finalize the current tile lists for incremental rendering. I.e. finalize them so that they
 * can processed by Mali200 before being appended with further primitives from GP2.
 * (Think; draw -> readpixels -> draw -> swapbuffers)
 */
MALI_STATIC mali_err_code _finalize_gp_job_with_context_switch( mali_frame_builder *frame_builder, mali_gp_job_handle *next_gp_job_ptr )
{
	mali_gp_job_handle   prev_gp_job;
	mali_gp_job_handle   next_gp_job;
	mali_err_code        error;

	mali_internal_frame* frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	MALI_DEBUG_ASSERT_POINTER( next_gp_job_ptr );
	prev_gp_job = frame->gp_job;

	*next_gp_job_ptr = MALI_NO_HANDLE;

	next_gp_job = _mali_gp_job_new( frame_builder->base_ctx );
	if( MALI_NO_HANDLE == next_gp_job ) return MALI_ERR_OUT_OF_MEMORY;

	_mali_gp_job_set_frame_id( next_gp_job, frame->frame_id );
	if( NULL != frame->plbu_heap_held && MALI_NO_HANDLE != frame->plbu_heap_held->plbu_heap ) _mali_gp_job_set_tile_heap( next_gp_job, frame->plbu_heap_held->plbu_heap );

	error = _gp_job_context_switch_in( frame_builder, next_gp_job );
	if( MALI_ERR_NO_ERROR != error )
	{
		_mali_gp_job_free( next_gp_job );
		return error;
	}

	error = _gp_job_context_switch_out( frame_builder, prev_gp_job );
	if( MALI_ERR_NO_ERROR == error )
	{
#if HARDWARE_ISSUE_3396
		MALI_DEBUG_ASSERT_POINTER(frame->tilelists);
		/* prior to r0p2 the "viewport" is set using slave tile dimensions */
		error = _mali_gp_job_add_plbu_cmd(prev_gp_job, GP_PLBU_COMMAND_WRITE_CONF_REG( 0 | ( (frame->tilelists->slave_tile_height - 1) << 8 ) | ( 0 << 16 ) |
											( (frame->tilelists->slave_tile_width - 1) << 24 ), GP_PLBU_CONF_REG_VIEWPORT ) );
		if( MALI_ERR_NO_ERROR != error )
		{
			_mali_gp_job_free( next_gp_job );
			return error;
		}
#endif
		error = _mali_gp_job_add_plbu_cmd( prev_gp_job, GP_PLBU_COMMAND_END_FRAME());
	}
	if( MALI_ERR_NO_ERROR != error )
	{
		_mali_gp_job_free( next_gp_job );
		return error;
	}

	*next_gp_job_ptr = next_gp_job;
	return MALI_ERR_NO_ERROR;
}


#if MALI_INSTRUMENTED
MALI_STATIC void _instrumented_pp_job_callback(void * cb_param, mali_pp_job_handle pp_job)
{
	mali_instrumented_frame *instrumented_frame = (mali_instrumented_frame *)cb_param;

	if ( NULL != instrumented_frame && _instrumented_is_sampling_enabled(_instrumented_get_context() ) )
	{
		/* let the instrumented module pull out performance counters from the pp job */
		_instrumented_pp_done( instrumented_frame, pp_job );
	}
	
}
#endif

/* It is only allowed to call this when the frame depencency is activated, but no pp jobs started. */
MALI_STATIC void _abort_pp_processing( mali_internal_frame *frame )
{
#if MALI_EXTERNAL_SYNC
	_mali_frame_call_pp_job_start_callback(frame, MALI_FALSE, MALI_FENCE_INVALID_HANDLE);
#endif /* MALI_EXTERNAL_SYNC */

	if ( NULL != frame->cb_func_complete_output )
	{
		(frame->cb_func_complete_output)(frame->cb_param_complete_output);
		frame->cb_func_complete_output = NULL; /* reset the callback after calling it once */
	}

	if ( NULL != frame->pp_job )
	{
#if MALI_INSTRUMENTED
		_instrumented_pp_abort(frame->pp_job);
#endif
		_mali_pp_job_free(frame->pp_job);
	}

	_mali_frame_builder_set_consumer_errors_with_force_release_pp( frame );

}

/* Callback function for the GP job that finalizes the tile lists. */
MALI_STATIC mali_bool _flush_geometry_callback( mali_base_ctx_handle base_ctx, void * cb_param, u32 job_stat, mali_gp_job_handle job_handle )
{
	mali_internal_frame *frame;

	MALI_DEBUG_ASSERT_HANDLE( base_ctx );
	MALI_DEBUG_ASSERT_POINTER( cb_param );

	frame = MALI_REINTERPRET_CAST(mali_internal_frame *) ( cb_param );

	MALI_DEBUG_ASSERT_POINTER(frame);

	/* record plbu_heap usage */
	if( NULL != frame->plbu_heap_held &&
		MALI_NO_HANDLE != frame->plbu_heap_held->plbu_heap)
	{
		MALI_DEBUG_ASSERT_POINTER(frame->frame_builder);
		frame->frame_builder->past_plbu_size[frame->frame_builder->current_period++ % 4] = _mali_mem_heap_used_bytes_get( frame->plbu_heap_held->plbu_heap ); 
		frame->plbu_heap_held->use_count++;
	}


#if MALI_INSTRUMENTED
	if (NULL != frame->instrumented_frame && _instrumented_is_sampling_enabled( _instrumented_get_context() ) )
	{
		/* let the instrumented module pull out performance counters from the gp job */
		_instrumented_gp_done( frame->instrumented_frame, job_handle );
	}
#else
	MALI_IGNORE(job_handle);
#endif

	if ( MALI_JOB_STATUS_END_SUCCESS == job_stat )
	{
		MALI_DEBUG_PRINT(3, ("GPE[%d]", frame->order_synch.swap_nr));

		MALI_DEBUG_ASSERT( NULL != frame->pp_job, ("Illegal state"));

		/* This removes the reference count held by the GP job.
			The Dependency System will call the PP jobs start function inside this function
		   if all dependencies are already met. If not it will start them when the
		   dependencies are met. */
#if MALI_INSTRUMENTED
		_instrumented_set_active_frame(frame->instrumented_frame);
#endif
		mali_ds_consumer_activation_ref_count_change(frame->ds_consumer_pp_render, MALI_DS_REF_COUNT_RELEASE);
#if MALI_INSTRUMENTED
		_instrumented_set_active_frame(NULL); /* Don't leave a dangling pointer */
#endif
	}
	else
	{
		if ( MALI_JOB_STATUS_END_OOM == job_stat )
		{
			MALI_INSTRUMENTED_LOG( (_instrumented_get_context(), MALI_LOG_LEVEL_ERROR,
			                   "Vertex processing: Out of tile list memory and was not able to recover - Aborting rendering") );
		}

#ifdef DEBUG
		{
			MALI_DEBUG_PRINT(0, ("%s() : Abnormal Mali GP job completion : ", MALI_FUNCTION));
			_debug_print_mali_jobstat( job_stat );
		}
#endif /* DEBUG */

		/* Update frame completion/error status */
		_mali_sys_atomic_set(&(frame->completion_status), job_stat);


		/* This will set an error state for the frame - And trigger the activation callback
		   The activation callback will call the release function which again will release the PP job
		   and do swap on the frame.*/
		_mali_frame_builder_set_consumer_errors_with_activate_pp( frame );
	}

	return MALI_TRUE;
}

void _mali_frame_builder_frame_dependency_activated(mali_base_ctx_handle base_ctx, void * owner, mali_ds_error status)
{
	mali_internal_frame *frame;

	MALI_IGNORE(base_ctx);

	frame = MALI_REINTERPRET_CAST(mali_internal_frame *) ( owner );
	MALI_DEBUG_PRINT(3, ("PPS[%d]", frame->order_synch.swap_nr));

	switch (status)
	{
		case MALI_DS_OK:
			if(_mali_projob_have_drawcalls(frame))
			{
				/* Start all projobs */
				_mali_projob_pp_flush(frame);

				/* Insert a barrier on the main job job so we can submit it right away */
				MALI_DEBUG_ASSERT( NULL != frame->pp_job, ("ERROR"));
				_mali_pp_job_set_barrier(frame->pp_job);

				_start_pp_processing_callback(frame);
			} else {
				/* no projobs, start PP job directly (without barrier) */
				_start_pp_processing_callback(frame);
			}
			break;
		case MALI_DS_ERROR:
			/* Fall through */
		default:
			_abort_pp_processing( frame );
	}
}


mali_ds_release _mali_frame_builder_frame_dependency_release(mali_base_ctx_handle base_ctx, void * owner, mali_ds_error status)
{
	mali_internal_frame *frame;

	frame = MALI_REINTERPRET_CAST(mali_internal_frame *) ( owner );

	return _flush_fragment_callback(base_ctx, frame, status);
}


MALI_STATIC void _pp_callback(mali_base_ctx_handle ctx, void * cb_param, mali_job_completion_status completion_status, mali_pp_job_handle job_handle)
{
	mali_internal_frame *frame;

	MALI_IGNORE(ctx);
	MALI_IGNORE(job_handle);

	frame = MALI_STATIC_CAST(mali_internal_frame *)cb_param;

	if ( NULL != frame->cb_func_complete_output )
	{
		(frame->cb_func_complete_output)(frame->cb_param_complete_output);
		frame->cb_func_complete_output = NULL; /* reset the callback after calling it once */
	}

#if MALI_PERFORMANCE_KIT
	if (mali_simple_hw_perf == -1)
	{
		mali_simple_hw_perf = (int)_mali_sys_config_string_get_s64("MALI_SIMPLE_HW_PERF", 0, 0, 1);
	}
	if (mali_custom_hw_perf == -1)
	{
		mali_custom_hw_perf = (int)_mali_sys_config_string_get_s64("MALI_CUSTOM_HW_PERF", 0, 0, 1);
		if(mali_custom_hw_perf == 1)
		{
			mali_print_custom_hw_perf = (int)_mali_sys_config_string_get_s64("MALI_PRINT_CUSTOM_HW_PERF", 0, 0, 1);
		}
		else
		{
			mali_print_custom_hw_perf = 0;
		}
	}

	if (mali_simple_hw_perf == 1 || mali_print_custom_hw_perf == 1)
	{
		_mali_sys_printf("Internal frame %u\n", frame);
	}
#endif

#if MALI_INSTRUMENTED
	_instrumented_pp_job_callback(frame->instrumented_frame, job_handle);
#endif

	if ( MALI_JOB_STATUS_END_SUCCESS != completion_status)
	{
		mali_ds_consumer_set_error(frame->ds_consumer_pp_render);
		mali_ds_consumer_set_error( frame->ds_consumer_gp_job );
	}

	MALI_DEBUG_PRINT(3, ("Internal frame %u, job render_time\n", frame));

	mali_ds_consumer_release_ref_count_dec( frame->ds_consumer_pp_render );
}

MALI_STATIC void _start_pp_processing_callback(mali_internal_frame *frame)
{
	mali_pp_job_handle pp_job;
	pp_job = frame->pp_job;
	MALI_DEBUG_ASSERT( NULL != pp_job, ("ERROR"));

	/* We are ready to run the PP job, now check for a pending deep CoW */
	if ( FRAME_COW_DEEP_COPY_PENDING == frame->cow_flavour )
	{
		_mali_mem_copy( frame->cow_desc.dest_mem_ref->mali_memory,frame->cow_desc.mem_offset,
		                frame->cow_desc.src_mem_ref->mali_memory ,frame->cow_desc.mem_offset,
						frame->cow_desc.data_size);

		_mali_shared_mem_ref_owner_deref(frame->cow_desc.src_mem_ref);
		frame->cow_flavour = FRAME_COW_REALLOC;
	}

	/* Set up a the ref_count variable - Doing this BEFORE we start the jobs which can finish quickly */
	mali_ds_consumer_release_ref_count_set_initial(frame->ds_consumer_pp_render, 1);

	/* call event GPU_WRITE on all write-tracked surfaces */
	/* call event GPU_READ on all read-tracked surfaces */
	_mali_surfacetracking_start_track(frame->surfacetracking);
	
	/* GP processing completed successfully, PP processing can be started */
	_mali_pp_job_set_callback(pp_job, _pp_callback, frame );
#if MALI_TIMELINE_PROFILING_ENABLED
	_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_SINGLE | VR_PROFILING_EVENT_CHANNEL_SOFTWARE | VR_PROFILING_EVENT_REASON_SINGLE_SW_PP_ENQUEUE, 0, 0, frame->frame_builder->identifier, frame->flush_id, 0);
#endif

#if MALI_EXTERNAL_SYNC
	{
		mali_fence_handle fence;

		_mali_pp_job_start(pp_job, MALI_JOB_PRI_NORMAL, &fence);
		_mali_frame_call_pp_job_start_callback(frame, MALI_FENCE_INVALID_HANDLE != fence, fence);

		if (MALI_FENCE_INVALID_HANDLE != fence) mali_fence_release(fence);
	}
#else
	_mali_pp_job_start(pp_job, MALI_JOB_PRI_NORMAL, NULL);
#endif /* MALI_EXTERNAL_SYNC */

	/* The pp job can be freed by pp_job_start(), or later by the postprocessing callback, so we
	 * should assume at this point that it is no longer valid. */
	frame->pp_job = MALI_NO_HANDLE;
}

/* Callback function from the PP job that renders the frame */
MALI_STATIC mali_ds_release _flush_fragment_callback(mali_base_ctx_handle base_ctx, mali_internal_frame *frame, mali_ds_error status)
{
	mali_ds_consumer_handle next_frame_consumer = MALI_NO_HANDLE;
	mali_bool reset_on_finish;
	mali_job_completion_status completion_status;
	mali_bool exit_with_unlock = MALI_TRUE;

	MALI_DEBUG_ASSERT_HANDLE( base_ctx );

	_mali_sys_mutex_lock(frame->order_synch.frame_order_mutex);
	MALI_DEBUG_PRINT(3, ("PPE[%d]", frame->order_synch.swap_nr));

	frame->order_synch.in_flight = MALI_FALSE;
	next_frame_consumer = frame->order_synch.release_on_finish;
	frame->order_synch.release_on_finish = NULL;

	_mali_sys_mutex_unlock(frame->order_synch.frame_order_mutex);

	if ( MALI_DS_OK != status )
	{
		completion_status = MALI_JOB_STATUS_END_OOM;
		_mali_sys_atomic_set(&(frame->completion_status), completion_status );
	}
	else
	{
		completion_status = MALI_JOB_STATUS_END_SUCCESS;
	}

	if( MALI_NO_HANDLE != next_frame_consumer )
	{
		/* Releasing one reference to the next frame activation count.
		The next frame is now allowed to start (when its GP jobs is also finished). */
#if MALI_INSTRUMENTED
		_instrumented_set_active_frame(frame->instrumented_frame);
#endif
		mali_ds_consumer_activation_ref_count_change(next_frame_consumer, MALI_DS_REF_COUNT_RELEASE);
#if MALI_INSTRUMENTED
		_instrumented_set_active_frame(NULL); /* Don't leave a dangling pointer */
#endif
	}

	/* take the mutex as we'll modify the frame state */
	_mali_sys_mutex_lock( frame->mutex );

	reset_on_finish = frame->reset_on_finish;

#if MALI_SW_COUNTERS_ENABLED
	/* Dump the counters only if one or more of them is enabled. */
	if(reset_on_finish && (0 != _mali_base_get_setting(MALI_SETTING_SW_COUNTER_ENABLED)))
	{
		_mali_sw_counters_dump( frame->sw_counters );
	}
#endif

	/* do the heap_reset and the release of the plbu dependantcy before the frame reset as the reset can take some time */
#if !defined(HARDWARE_ISSUE_3251)
	/* reset PLBU heap */
	if( reset_on_finish  || MALI_JOB_STATUS_END_OOM == completion_status)
	{
		frame->plbu_heap_held = NULL;
	}
#endif

	frame->current_plbu_ds_resource = MALI_NO_HANDLE;

	/* rendering has ended (in one way or the other) so the frame state is complete */
	frame->state = FRAME_COMPLETE;

	_mali_sys_mutex_unlock( frame->mutex );

	mali_ds_consumer_release_all_connections(frame->ds_consumer_gp_job);

	/* call the GPU_READ_DONE callback on all read-tracked surfaces, if any */
	/* call the GPU_WRITE_DONE callback on all write-tracked surfaces, if any */
	_mali_surfacetracking_stop_track(frame->surfacetracking);

	/* and remove the write-surfacetracked surfaces as they are never written to again! */
	_mali_surfacetracking_reset(frame->surfacetracking, MALI_SURFACE_TRACKING_WRITE);

	/* the frame wont be used for any incremental rendering (it's most probably flush by a buffer swap),
	 * so it is reset after rendering has finished */
	if( reset_on_finish || MALI_JOB_STATUS_END_OOM == completion_status )
	{
#if MALI_USE_DEFERRED_FRAME_RESET
		_mali_frame_builder_frame_execute_callbacks_non_deferred( frame );
		/* remove all dependencies on this frame */
		mali_ds_consumer_release_set_mode(frame->ds_consumer_pp_render, MALI_DS_RELEASE_ALL);
		mali_ds_consumer_release_all_connections(frame->ds_consumer_pp_render);
		exit_with_unlock = MALI_FALSE;
		_mali_queue_frame_reset(frame);
#else
		_mali_frame_reset(frame);
#endif /* MALI_USE_DEFERRED_FRAME_RESET */
	}

#if MALI_INSTRUMENTED
	if (NULL != frame->instrumented_frame)
	{
		/* Release the reference we acquired in _internal_flush() */
		_instrumented_release_frame(frame->instrumented_frame);
		frame->instrumented_frame = NULL;
	}
#endif

	if (exit_with_unlock) _mali_sys_lock_unlock(frame->lock);

	return MALI_DS_RELEASE;
}

/* Rotate the swap chains for all attachments one step */
MALI_STATIC void _rotate_swap_chain( mali_frame_builder *frame_builder )

{
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	/* rotate the frame chain */
	frame_builder->iframe_current = ROTATE_INTERNAL_FRAMES(frame_builder);
	frame_builder->swap_performed++;

}

MALI_STATIC mali_err_code _pp_jobs_create( mali_frame_builder     *frame_builder,
                                           mali_internal_frame    *frame,
                                           mali_base_ctx_handle    base_ctx )
{
	u32 i, num_cores;
	mali_pp_job_handle pp_job;
	mali_stream_handle stream;

	MALI_DEBUG_ASSERT_HANDLE(  base_ctx );
	MALI_DEBUG_ASSERT_POINTER( frame );
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

#if defined(USING_MALI450)
	/* Use zero cores for Mali-450, indicating that we should use the DLBU. */
	num_cores = 0;
#else
	num_cores = frame->curr_pp_split_count;
#endif /* defined(USING_MALI450) */

#if MALI_EXTERNAL_SYNC
	stream = frame->stream;
#else
	stream = NULL;
#endif /* MALI_EXTERNAL_SYNC */

	pp_job = _mali_pp_job_new(base_ctx, num_cores, stream);

	if (MALI_NO_HANDLE == pp_job)
	{
		MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
	}
	else
	{
		_pp_job_setup_common_registers(frame_builder, frame, pp_job);
		_mali_pp_job_set_frame_id( pp_job, frame->frame_id );

		for (i = 0; i < frame->curr_pp_split_count; ++i)
		{
			_pp_job_setup_specific_registers(frame, pp_job, i);
		}

		if ( frame_builder->egl_funcptrs && frame_builder->egl_funcptrs->get_synchandle )
		{
			mali_sync_handle local = frame_builder->egl_funcptrs->get_synchandle();
			_mali_pp_job_add_to_sync_handle( local, pp_job);
		}

#if MALI_INSTRUMENTED
		{
			int j;

			for ( j = 0; j < MALI200_WRITEBACK_UNIT_COUNT; j++ )
			{
				struct mali_surface *render_target = frame_builder->output_buffers[j].buffer;

				if ( NULL != render_target)
				{
					_mali_base_common_instrumented_pp_job_set_wbx_mem(pp_job, j, render_target->mem_ref->mali_memory);
				} else {
					_mali_base_common_instrumented_pp_job_set_wbx_mem(pp_job, j, MALI_NO_HANDLE);
				}
			}
		}
#endif
	}

#if defined(USING_MALI450)
	/* Setting the info needed for the Dynamic Load Balancing unit */
	{
		m450_pp_job_frame_info info;
		info.slave_tile_list_mali_address = _mali_mem_mali_addr_get(frame->tilelists->slave_tile_list_mem, 0);
		MALI_DEBUG_ASSERT( 0!= info.slave_tile_list_mali_address, ("ERROR: slave tile list has Null address! \n")) ;
		info.master_x_tiles = frame->tilelists->master_tile_width;
		info.master_y_tiles = frame->tilelists->master_tile_height;
		info.size = M400_TILE_BLOCK_SIZE_128 + MALI_TILELIST_CHUNK_FORMAT;
		info.binning_pow2_x = frame->tilelists->binning_scale_x;
		info.binning_pow2_y = frame->tilelists->binning_scale_y;
		/* HACK: Hardcoding that we have 8 PP cores for M450 */
		if ( frame->fs_stack.fs_stack_mem )
		{
			info.stack_size_per_core = (_mali_mem_size_get( frame->fs_stack.fs_stack_mem ))/8;
		}
		else
		{
			info.stack_size_per_core = 0;
		}

		_mali_pp_job_450_set_frame_info(pp_job, &info);
	}
#endif /* defined(USING_MALI450) */

	frame->pp_job = pp_job;

	MALI_SUCCESS;
}

/**
 * The internals of the _flush() and _swap() frame_builder interface entrypoints
 */
MALI_STATIC mali_err_code _internal_flush( mali_frame_builder *frame_builder,
                                           mali_bool frame_swap )

{
	mali_base_ctx_handle base_ctx = _mali_frame_builder_get_base_ctx(frame_builder);
	mali_err_code error;
	mali_gp_job_handle   next_gp_job = MALI_NO_HANDLE, current_gp_job;
	mali_internal_frame* frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	MALI_DEBUG_PRINT(3, ("F[%d]", frame->order_synch.swap_nr));
	MALI_DEBUG_ASSERT_POINTER(frame_builder);
	MALI_DEBUG_ASSERT_POINTER(frame);

	/* early out if the FBO is set up wrong */
	if(frame_builder->output_valid == MALI_FALSE)
	{
		if(frame->state == FRAME_RENDERING)
		{
			_mali_frame_wait( frame );
		}

		if(frame->state != FRAME_UNMODIFIED)
		{
			_mali_frame_reset( frame );
		}

		return MALI_ERR_NO_ERROR;
	}

	_mali_sys_mutex_lock(frame->mutex);

	/* If the frame is clean it might've been "fully cleared" (reset), so we must ensure
	 * we can render the framebuffer with the clearcolor */
	if( frame->state == FRAME_CLEAN )
	{
		error = _frame_builder_use_internal( frame_builder, frame );
		if( MALI_ERR_NO_ERROR != error )
		{
			_mali_sys_mutex_unlock(frame->mutex);
			return error;
		}
	}

	/* if there's nothing (new) to render we can just execute the callbacks */
	if( frame->state == FRAME_COMPLETE || frame->state == FRAME_UNMODIFIED )
	{
		/* if the frame has been flushed but should be swapped, we must reset it here */
		_mali_sys_mutex_unlock(frame->mutex);

		/* Wait for the frame to really be COMPLETE before resetting it */
#if MALI_TIMELINE_PROFILING_ENABLED
		_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_SUSPEND|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_SUSPEND_RESUME_SW_FB_IFRAME_SYNC, 0, 0, frame->frame_builder->identifier, (u32)frame, 0);
#endif
		_mali_sys_lock_lock(frame->lock);
		_mali_sys_lock_unlock(frame->lock);
#if MALI_TIMELINE_PROFILING_ENABLED
		_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_RESUME|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_SUSPEND_RESUME_SW_FB_IFRAME_SYNC, 0, 0, frame->frame_builder->identifier, (u32)frame, 0);
#endif
		if( MALI_TRUE == frame_swap && frame->state == FRAME_COMPLETE ) 
		{
			_mali_frame_reset( frame );
		}

		return MALI_ERR_EARLY_OUT;
	}
	else if( frame->state == FRAME_RENDERING )
	{
		/* If the frame is currently rendering we don't need to perform any changes on it,
		 * however, if the currently rendering frame has an existing callback different from the
		 * one we want to set, we must make sure both are called.
		 */
		if(frame_swap) frame->reset_on_finish = MALI_TRUE;
		_mali_sys_mutex_unlock(frame->mutex);
		return MALI_ERR_NO_ERROR;
	}

	MALI_DEBUG_ASSERT( MALI_FALSE != frame_builder->output_valid, ("Invalid output in flush. Clean frame called _internal_use and became invalid") );

	/* the init should be done only once per frame in case we are in incremental rendering */
	if( 0 == frame->num_flushes_since_reset )
	{
		error = _mali_frame_builder_init_per_frame(frame_builder);
		if( MALI_ERR_NO_ERROR != error )
		{
			_mali_sys_mutex_unlock(frame->mutex);
			return error;
		}
	}

	_mali_sys_mutex_unlock(frame->mutex);

	/* we track the number of flushes since last reset as a heuristic for incremental rendering
	 * (tracked after the early-outs on purpose)
	 */
	frame->num_flushes_since_reset++;

	/* make sure the pool is unmapped */
	if( frame->pool_mapped )
	{
		/* unmap the mem pool */
		_mali_mem_pool_unmap( &frame->frame_pool );
		frame->pool_mapped = MALI_FALSE;
	}

	/* setup the CoW behaviour before we might trigger it when making the DS_WRITE connect call */
	frame->cow_flavour = FRAME_COW_REALLOC;

	if ( frame_builder->output_buffers[MALI_DEFAULT_COLOR_WBIDX].usage & MALI_OUTPUT_WRITE_DIRTY_PIXELS_ONLY )
	{
		frame->cow_flavour = FRAME_COW_DEEP;
	}

	/* Makes a Dependency System connection between frame->ds_consumer_pp_render and
	each render target. This makes sure that the PP jobs are not started before all
	these WRITE dependencies are met. */
	error = _add_flush_dependencies_on_buffers(frame, frame_builder, frame_swap);

	if( MALI_ERR_NO_ERROR != error )
	{
		/* The dependency system will release connections and reset the consumer,
		but not call callbacks. Since the consumer is not flushed. */

		_mali_frame_builder_set_consumer_errors_with_force_release_pp( frame );
		_mali_frame_builder_reset( frame_builder );
		return error;
	}

	/* update fragment stack memory allocation in the frame */
	error = _fragment_stack_alloc( frame_builder );
	if( MALI_ERR_NO_ERROR != error )
	{
		/* The dependency system will release connections and reset the consumer,
		but not call callbacks. Since the consumer is not flushed. */
		_mali_frame_builder_set_consumer_errors_with_force_release_pp( frame );
		_mali_frame_builder_reset( frame_builder );
		return error;
	}

	/* Initialize the PP Job(s) */
	error = _pp_jobs_create(frame_builder, frame, base_ctx);
	if ( MALI_ERR_NO_ERROR != error )
	{
		/* The dependency system will release connections and reset the consumer,
		but not call callbacks. Since the consumer is not flushed. */
		_mali_frame_builder_set_consumer_errors_with_force_release_pp( frame );
		_mali_frame_builder_reset( frame_builder );
		return MALI_ERR_OUT_OF_MEMORY;
	}

#if MALI_EXTERNAL_SYNC

	if (MALI_FENCE_INVALID_HANDLE != frame_builder->output_fence)
	{
		mali_fence_handle fence;

		fence = mali_fence_dup(frame_builder->output_fence);

		MALI_DEBUG_ASSERT(MALI_FENCE_INVALID_HANDLE != fence, ("fence dup failed on pp_job_set_fence"));

		if (MALI_FENCE_INVALID_HANDLE == fence)
		{
			/* The dependency system will release connections and reset the consumer,
			   but not call callbacks. Since the consumer is not flushed. */
			_mali_frame_builder_set_consumer_errors_with_force_release_pp( frame );
			_mali_frame_builder_reset( frame_builder );
			return MALI_ERR_OUT_OF_MEMORY;
		}

		_mali_pp_job_set_fence(frame->pp_job, fence);
	}

	frame_builder->egl_funcptrs->_sync_notify_job_create(frame_builder);

	/* WARNING: after the sync_notify_job_create callback has been called, it is VERY IMPORTANT
	   that the job_start callbacks are called with MALI_FALSE for started, if anything fails
	   before mali_ds_consumer_flush(frame->ds_consumer_pp_render). */

#endif /* MALI_EXTERNAL_SYNC */

	MALI_DEBUG_ASSERT(frame->job_inited == 2, ("job Init state in flush should be 2 but is %d framestate %d numflushes %d \n", frame->job_inited, frame->state, frame->num_flushes_since_reset ) );

	/* set up the necessary list finalization and context switching to the new gp job */
	if( frame_swap == MALI_FALSE )
	{
		/* set up the necessary list finalization and context switching to the new gp job */
		error = _finalize_gp_job_with_context_switch( frame_builder, &next_gp_job );

		/* the job will be automatically released on completion */
		_mali_gp_job_set_auto_free_setting( frame->gp_job, MALI_TRUE );
	}
	else
	{
		/* no need for a new gp job as this frame will be reset on completion anyway */
#if HARDWARE_ISSUE_3396
		/* prior to r0p2 the "viewport" is set using slave tile dimensions */

		error = _mali_gp_job_add_plbu_cmd(frame->gp_job, GP_PLBU_COMMAND_WRITE_CONF_REG( 0 | ( (frame->tilelists->slave_tile_height - 1) << 8 ) | ( 0 << 16 ) |
											( (frame->tilelists->slave_tile_width - 1) << 24 ), GP_PLBU_CONF_REG_VIEWPORT ) );
		if( MALI_ERR_NO_ERROR != error )
		{
#if MALI_EXTERNAL_SYNC
			_mali_frame_call_pp_job_start_callback(frame, MALI_FALSE, MALI_FENCE_INVALID_HANDLE);
#endif /* MALI_EXTERNAL_SYNC */
			_mali_gp_job_free( next_gp_job );
			/* The dependency system will release connections and reset the consumer,
			but not call callbacks. Since the consumer is not flushed. */
			_mali_frame_builder_set_consumer_errors_with_force_release_pp( frame );
			_mali_frame_builder_reset( frame_builder );
			return error;
		}
#endif /* HARDWARE_ISSUE_3396 */
		error = _mali_gp_job_add_plbu_cmd( frame->gp_job, GP_PLBU_COMMAND_END_FRAME());

		/* the job will not be released on completion */
		_mali_gp_job_set_auto_free_setting( frame->gp_job, MALI_FALSE );
	}

	if( MALI_ERR_NO_ERROR != error )
	{
#if MALI_EXTERNAL_SYNC
		_mali_frame_call_pp_job_start_callback(frame, MALI_FALSE, MALI_FENCE_INVALID_HANDLE);
#endif /* MALI_EXTERNAL_SYNC */
		_mali_pp_job_free( frame->pp_job );
		frame->pp_job = MALI_NO_HANDLE;

		/* The dependency system will release connections and reset the consumer,
		but not call callbacks. Since the consumer is not flushed. */
		_mali_frame_builder_set_consumer_errors_with_force_release_pp( frame );
		_mali_frame_builder_reset( frame_builder );
		return error;
	}

#if MALI_INSTRUMENTED
	frame->instrumented_frame = _instrumented_acquire_current_frame();

	if (NULL != frame->instrumented_frame && _instrumented_is_sampling_enabled( _instrumented_get_context() ))
	{
		/* instrument the PP and GP jobs */
		/* Setup the pp job for instrumentation */
		_instrumented_pp_setup( _instrumented_get_context(), frame->pp_job, 0 );

		_instrumented_gp_setup( _instrumented_get_context(), frame->gp_job );
		_mali_instrumented_gp_set_plbu_stack_handle( frame->gp_job, frame->gp_context_stack );
	}
#endif /* MALI_INSTRUMENTED */

	MALI_DEBUG_ASSERT_POINTER(frame->tilelists);
	_mali_gp_job_set_plbu_pointer_array( frame->gp_job, frame->tilelists->pointer_array_mem );

	/*set the GP job callback which will trigger the dependency system to start the PP job */
	_mali_gp_job_set_callback( frame->gp_job, MALI_REINTERPRET_CAST(mali_cb_gp) _flush_geometry_callback, MALI_REINTERPRET_CAST(void *) frame );

	/* add memory to be free'd when the job is completed */
	if( frame->gp_job_mem_list != MALI_NO_HANDLE )
	{
		_mali_gp_job_add_mem_to_free_list( frame->gp_job, frame->gp_job_mem_list );
		frame->gp_job_mem_list = MALI_NO_HANDLE;
	}

#if MALI_TIMELINE_PROFILING_ENABLED
	/* set the GP job identity */
	{
		frame->flush_id = MAKE_FLUSH_ID(frame_builder->iframe_current, frame_swap, frame_builder->flush_count);
		_mali_gp_job_set_identity(frame->gp_job, frame_builder->identifier, frame->flush_id);

		/* set PP jobs identities */
		_mali_pp_job_set_identity(frame->pp_job, frame_builder->identifier, frame->flush_id);

		_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_SINGLE | VR_PROFILING_EVENT_CHANNEL_SOFTWARE | VR_PROFILING_EVENT_REASON_SINGLE_SW_FLUSH, 0, 0, frame_builder->identifier, frame->flush_id, 0);

		/* flush is considered successful from this point on (no returns with error), safe to increase
			the flush count. */
		frame_builder->flush_count++;
	}
#endif

	current_gp_job = frame->gp_job ;
	if( frame_swap == MALI_FALSE ) frame->gp_job = next_gp_job;

	_mali_sys_mutex_lock(frame->mutex);

	/* frame should be reset on completion if we're swapping buffers */
	frame->reset_on_finish = frame_swap;

	frame->state = FRAME_RENDERING;
	/* Setting in flight - means that it is in the rendering pipeline */
	frame->order_synch.in_flight = MALI_TRUE;

	_mali_sys_mutex_unlock(frame->mutex);

	/* mark the frame as in use by Mali */
	_mali_sys_lock_lock( frame->lock );

	/* Add an additional ref count for the GP job, so that we can flush the consumer now */
	mali_ds_consumer_activation_ref_count_change(frame->ds_consumer_pp_render,1);

	/* Flushing consumer; PP job will start when all the surfaces it depends on are
	   available AND the ref_count set above for the GP job is decreased */
	mali_ds_consumer_flush(frame->ds_consumer_pp_render);

	MALI_DEBUG_ASSERT_POINTER( frame->ds_consumer_gp_job );

	frame->current_gp_job = current_gp_job;

	/* If this frame has not been assigned a plbu-resource yet, then give it one and add dependency on it */
	if ( MALI_NO_HANDLE == frame->current_plbu_ds_resource && frame->plbu_heap_held != NULL && frame->plbu_heap_held->plbu_heap != NULL)
	{
		frame->current_plbu_ds_resource = frame_builder->plbu_heaps[ frame_builder->plbu_heap_current ].plbu_ds_resource;

		MALI_DEBUG_ASSERT_POINTER( frame->current_plbu_ds_resource );
		MALI_DEBUG_ASSERT( frame->plbu_heap_held == &frame_builder->plbu_heaps[ frame_builder->plbu_heap_current ], ("Out of sync between DS resource and associated heap") );

		/* Connect the plbu-heap with a resource to the gp-consumer, this connection will be released in
		 * the _flush_fragment_callback function */
		if ( MALI_ERR_NO_ERROR !=
		     mali_ds_connect( frame->ds_consumer_gp_job, frame->current_plbu_ds_resource, MALI_DS_WRITE ))
		{
			frame->current_plbu_ds_resource = MALI_NO_HANDLE;
			mali_ds_consumer_set_error(frame->ds_consumer_gp_job);
		}
	}

#if MALI_INSTRUMENTED
	_instrumented_set_active_frame(frame->instrumented_frame);
#endif

	/* execute the lock output callback if specified */
	if ( NULL != frame->cb_func_lock_output )
	{
		(frame->cb_func_lock_output)(frame->cb_param_lock_output);
	}

	mali_ds_consumer_flush( frame->ds_consumer_gp_job );

#if MALI_INSTRUMENTED
		_instrumented_set_active_frame(NULL); /* don't leave dangling pointers in TLS */
#endif

	return MALI_ERR_NO_ERROR;
}

MALI_STATIC void _mali_incremental_render_on_flush( mali_frame_builder *frame_builder )
{
	if ( frame_builder->inc_render_on_flush )
	{

		/* This flush is triggered by eglClientWaitSync for fence sync */
		mali_err_code err = MALI_ERR_NO_ERROR;
		_mali_frame_set_inc_render_on_flush( frame_builder, MALI_FALSE );

		 err = _mali_incremental_render( frame_builder );

		/* if err == MALI_ERR_EARLY_OUT this means that there is nothing to render, reset the fb for the callbacks to be called */
		if ( MALI_ERR_EARLY_OUT == err )
		{
			_mali_frame_builder_reset( frame_builder );
		}
	}
}

MALI_STATIC mali_err_code _mali_frame_builder_flush_common( mali_frame_builder *frame_builder,
                                                            mali_bool swap_behaviour )
{
	mali_err_code error;

	MALI_DEBUG_ASSERT_POINTER( frame_builder );

#if MALI_SW_COUNTERS_ENABLED
	{
		mali_internal_frame *frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );
		mali_sw_counters *counters = frame->sw_counters;
		MALI_DEBUG_ASSERT_POINTER(counters);
		MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, NUM_FLUSHES, 1);
	}
#endif /* MALI_SW_COUNTERS_ENABLED */

	if ( frame_builder->properties & MALI_FRAME_BUILDER_PROPS_ROTATE_ON_FLUSH ) swap_behaviour = MALI_TRUE;  /* if rotate on flush, all flushes act as swaps */

	/* consider incremental rendering only when not swapping. */
	if(swap_behaviour == MALI_FALSE) _mali_incremental_render_on_flush( frame_builder );

	error = _internal_flush( frame_builder, swap_behaviour );

	if(swap_behaviour)
	{
		if ( frame_builder->properties & MALI_FRAME_BUILDER_PROPS_UNDEFINED_AFTER_SWAP)
		{
			frame_builder->buffer_state_per_plane = MALI_COLOR_PLANE_UNDEFINED | MALI_DEPTH_PLANE_UNDEFINED | MALI_STENCIL_PLANE_UNDEFINED; 
		}
		else
		{
			if(!(frame_builder->buffer_state_per_plane & MALI_COLOR_PLANE_UNDEFINED)) 
			{
				frame_builder->buffer_state_per_plane &= ~MALI_COLOR_PLANE_MASK;
				frame_builder->buffer_state_per_plane |= MALI_COLOR_PLANE_UNCHANGED;
			}
			if(!(frame_builder->buffer_state_per_plane & MALI_DEPTH_PLANE_UNDEFINED)) 
			{
				frame_builder->buffer_state_per_plane &= ~MALI_DEPTH_PLANE_MASK;
				frame_builder->buffer_state_per_plane |= MALI_DEPTH_PLANE_UNCHANGED;
			}
			if(!(frame_builder->buffer_state_per_plane & MALI_STENCIL_PLANE_UNDEFINED)) 
			{
				frame_builder->buffer_state_per_plane &= ~MALI_STENCIL_PLANE_MASK;
				frame_builder->buffer_state_per_plane |= MALI_STENCIL_PLANE_UNCHANGED;
			}
		}

		frame_builder->preserve_multisample = MALI_FALSE;
	}

	/* if swapping, rotate the frame */
	if ( error == MALI_ERR_NO_ERROR && swap_behaviour != MALI_FALSE )	_rotate_swap_chain( frame_builder );

#ifndef NDEBUG
	if(error != MALI_ERR_NO_ERROR && error != MALI_ERR_EARLY_OUT)
	{
		mali_internal_frame *frame;
		frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

		_mali_sys_mutex_lock(frame->mutex);
		MALI_DEBUG_ASSERT(frame->state != FRAME_RENDERING, ("if flush failed, no rendering allowed"));
		MALI_DEBUG_ASSERT(frame->state != FRAME_COMPLETE, ("if flush failed, no complete allowed"));
		_mali_sys_mutex_unlock(frame->mutex);
	}
#endif

	if ( error == MALI_ERR_EARLY_OUT ) return MALI_ERR_NO_ERROR; /* don't return that early out */
	return error;
}

MALI_EXPORT mali_err_code _mali_frame_builder_flush( mali_frame_builder *frame_builder )
{
	return _mali_frame_builder_flush_common( frame_builder, MALI_FALSE );
}

MALI_EXPORT mali_err_code _mali_frame_builder_swap( mali_frame_builder *frame_builder )
{
	return _mali_frame_builder_flush_common( frame_builder, MALI_TRUE );
}

MALI_EXPORT mali_bool _mali_frame_builder_is_modified( mali_frame_builder *frame_builder )
{
	mali_internal_frame *frame;
	frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	return frame->state == FRAME_UNMODIFIED ? MALI_FALSE : MALI_TRUE;
}

MALI_EXPORT void _mali_frame_builder_add_frame_mem( mali_frame_builder *frame_builder,
                                                    mali_mem_handle mem_handle )
{
	mali_internal_frame* frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	MALI_DEBUG_ASSERT_HANDLE( mem_handle );

	_mali_frame_builder_add_internal_frame_mem( frame, mem_handle );

}

MALI_EXPORT void _mali_frame_builder_wait_frame( mali_frame_builder *frame_builder )
{
	mali_internal_frame* frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	_mali_frame_wait( frame );

}

MALI_EXPORT void _mali_frame_builder_wait( mali_frame_builder *frame_builder )
{

	/* If the are rotating on calls to _mali_frame_builder_flush, we need to wait
	 * on previous frames as well since the current frame is no longer the one that
	 * was flushed.
	 */
	if ( frame_builder->properties & MALI_FRAME_BUILDER_PROPS_ROTATE_ON_FLUSH )
	{
		_mali_frame_builder_wait_all( frame_builder );
	}
	else
	{
		mali_internal_frame* frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );
		_mali_frame_wait( frame );
	}

}

MALI_EXPORT void _mali_frame_builder_wait_all( mali_frame_builder *frame_builder )
{
	mali_internal_frame *frame_next;
	u32 i;
	u32 frame_idx;
	u32 swap_count;

	MALI_DEBUG_ASSERT_POINTER(frame_builder);

	frame_idx = frame_builder->iframe_current;
	swap_count = frame_builder->iframe_count;

	if( NULL != frame_builder->iframes )
	{
		for ( i = 0; i < swap_count; i++ )
		{
			frame_idx = (frame_idx + 1) == ( swap_count ) ? 0 : frame_idx + 1;
			frame_next = frame_builder->iframes[ frame_idx ];
			_mali_frame_wait( frame_next );
		}
	}
}

MALI_EXPORT void _mali_frame_builder_set_output( mali_frame_builder *frame_builder,
                                                  u32 wbx_id,
                                                  struct mali_surface *surface,
                                                  u32 usage )
{
	int i;

	/* output parameters */
	u32 output_width = 0;
	u32 output_height = 0;
	mali_bool output_valid = MALI_TRUE;
	u32 largest_xscale = 1;
	u32 largest_yscale = 1;

	/* bounding box parameters */
	int bb_width = 0;
	int bb_height = 0;
	mali_bool bb_used = MALI_FALSE;
	mali_bool bb_enable[MALI200_WRITEBACK_UNIT_COUNT]; /* per wbx */
	struct mali_frame_builder_output_chain *output;

	MALI_DEBUG_ASSERT_POINTER( frame_builder );


	MALI_DEBUG_ASSERT_RANGE( (int)wbx_id, 0, MALI200_WRITEBACK_UNIT_COUNT - 1);


	/* attach surfaces to the fbuilder, detach old surfaces */
	if ( NULL != surface )
	{
		_mali_surface_addref(surface);

		/* verify that the newly attached surface adhere to the WBx Target AA limitations */
		#ifndef NDEBUG

		/* FP16 formats do not support ANY downscaling */
		if( usage &
		 ( MALI_OUTPUT_DOWNSAMPLE_X_2X | MALI_OUTPUT_DOWNSAMPLE_X_4X | MALI_OUTPUT_DOWNSAMPLE_X_8X |
		   MALI_OUTPUT_DOWNSAMPLE_Y_2X | MALI_OUTPUT_DOWNSAMPLE_Y_4X | MALI_OUTPUT_DOWNSAMPLE_Y_8X |
		   MALI_OUTPUT_DOWNSAMPLE_Y_16X ))
		{
			MALI_DEBUG_ASSERT(surface->format.pixel_format != MALI_PIXEL_FORMAT_ARGB_FP16 &&
			                  surface->format.pixel_format != MALI_PIXEL_FORMAT_B_FP16 &&
			                  surface->format.pixel_format != MALI_PIXEL_FORMAT_GB_FP16,
							  ("FP16 output formats require: No downscaling bits set"));
		}

		/* 8 bit formats do not support 4x or 8x downscaling */
		if( usage &
		 ( MALI_OUTPUT_DOWNSAMPLE_X_4X | MALI_OUTPUT_DOWNSAMPLE_X_8X ))
		{
			MALI_DEBUG_ASSERT( surface->format.pixel_format != MALI_PIXEL_FORMAT_B8 &&
			                   surface->format.pixel_format != MALI_PIXEL_FORMAT_S8,
							 ("8bit formats require: No 4x or 8x downscaling in the x axis"));
		}

		/* 16 bit formats do not support 8x downscaling */
		if( usage & MALI_OUTPUT_DOWNSAMPLE_X_8X )
		{
			MALI_DEBUG_ASSERT( surface->format.pixel_format != MALI_PIXEL_FORMAT_R5G6B5 &&
			                   surface->format.pixel_format != MALI_PIXEL_FORMAT_A1R5G5B5 &&
			                   surface->format.pixel_format != MALI_PIXEL_FORMAT_A4R4G4B4 &&
			                   surface->format.pixel_format != MALI_PIXEL_FORMAT_G8B8 &&
			                   surface->format.pixel_format != MALI_PIXEL_FORMAT_Z16,
							 ("16 bit formats require: No 8x downscaling in the x axis"));
		}
		#endif

	}

	output = &frame_builder->output_buffers[wbx_id];
	if(output->buffer)
	{
		_mali_surface_deref(output->buffer);
	}

	output->buffer = surface;
	output->usage  = usage;

	frame_builder->fp16_format_used = MALI_FALSE;


	/**
	 * Bounding box headaches:
	 *
	 * Mali always output full tiles - the bounding box allows you to cull some of that tile output.
	 * This is vital for linear outputs where the output buffer has a width divisible by 8, failure to use
	 * the bounding box will lead the unused parts of a tile to bleed over to the next scanline.
	 *
	 * The bounding box is enabled/disabled per writeback unit, but can only have one value. This value is set as
	 * a frame register, specified once per job. If enabled, the pixels outside of the bbox boundaries are culled.
	 *
	 * If using supersampling, this scheme is complicated, as each WBx can have a different supersample scalefactor.
	 * The HW will automatically *multiply* the bounding box register value with the supersample scalefactor.
	 * The idea is that if the bbox is set to the output size (f.ex 640x480), then the bbox will be doubled up
	 * to 1280x960 or whatever on wbunits using supersampling automatically. This scheme works perfectly as
	 * long as the framebuilder dimensions are actually 1280x960 and the WBx is set up to render to a 640x480
	 * supersampled output.
	 *
	 * But it is not always this well-working. A simple texture mipgen render operation where we want the HW
	 * to generate three mipmap levels of a texture can really mess this up. Assume you want to make miplevels
	 * for a 256x256 texture.
	 *  - WB0 is set up to render a 128x128 miplevel (2x downscale)
	 *  - WB1 is set up to render a 64x64 miplevel   (4x downscale)
	 *  - WB2 is set up to render a 32x32 miplevel   (8x downscale)
	 *
	 * In this case, the bounding box simply cannot be used. If the bbox is set to 32x32 in this case, then
	 *  - WB0 will assume the bounding box is (32x32)*2 = 64x64 (4x too small!)
	 *  - WB1 will assume the bounding box is (32x32)*4 = 256x256 (perfect)
	 *  - WB2 will assume the bounding box is (32x32)*8 = 1024x1024 (4x too large!)
	 *
	 * Concluding - we cannot use the bounding box in this use-case, and when detecting the usecase we need to
	 * disable the bounding box. For each writeback unit using ...
	 *
	 *  - #1: Block-interleaved output:
	 *      Disable the bounding box.
	 *      It doesn't matter if we render to the full tiles. No one will read the extra pixels anyway.
	 *
	 *  - #2: Linear outputs that are framebuilder tile-aligned
	 *      Disable the bounding box.
	 *      It doesn't matter if the bounding box is enabled or not, no pixels should be culled anyway.
	 *
	 *  - #3: Non-tilealigned linear outputs, where the superscale factor is identical to the common ground bbox:
	 *      The bbox size is set to the dimensions of the frame divided by the supersample scalefactor
	 *      The bounding box is enabled unless hardware issues disallow it.
	 *      This should have no problems, and will cover the common case of one output buffer.
	 *
	 *  - #4: Non-tilealigned linear outputs, where the superscale factor is not identical to the common ground bbox:
	 *      Assert crash - treat the output setup as invalid.
	 *      The usecase is not possible to handle and should never be allowed to be set up.
	 *
	 * The solution is to set up a bounding box right here, and a bitmask telling whether it is used on each WBx unit.
	 *
	 */

	/* validate all outputs */
	for(i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; i++)
	{
		u32 w, h;
		u32 xscale = 1;
		u32 yscale = 1;
		mali_surface* surf;
		u32 usage;

		bb_enable[i] = MALI_FALSE;

		surf = frame_builder->output_buffers[i].buffer;
		if(surf == NULL) continue;
		usage = frame_builder->output_buffers[i].usage;

		if(usage & MALI_OUTPUT_DOWNSAMPLE_X_2X) xscale = 2;
		if(usage & MALI_OUTPUT_DOWNSAMPLE_X_4X) xscale = 4;
		if(usage & MALI_OUTPUT_DOWNSAMPLE_X_8X) xscale = 8;
		if(usage & MALI_OUTPUT_DOWNSAMPLE_Y_2X) yscale = 2;
		if(usage & MALI_OUTPUT_DOWNSAMPLE_Y_4X) yscale = 4;
		if(usage & MALI_OUTPUT_DOWNSAMPLE_Y_8X) yscale = 8;
		if(usage & MALI_OUTPUT_DOWNSAMPLE_Y_16X) yscale = 16;

		if(xscale > largest_xscale) largest_xscale = xscale;
		if(yscale > largest_yscale) largest_yscale = yscale;

		w = surf->format.width * xscale;
		h = surf->format.height * yscale;

		if(output_width == 0) output_width = w;
		if(output_height == 0) output_height = h;
		if(output_width != w || output_height != h) output_valid = MALI_FALSE;

		/* For non-linear layouts, the bbox is always disabled. For linear layouts, it will likely be enabled */
		if(surf->format.pixel_layout == MALI_PIXEL_LAYOUT_LINEAR)
		{
			/* grab the common ground bbox dimension */
			if(bb_width == 0) bb_width = surf->format.width;
			if(bb_height == 0) bb_height = surf->format.height;

			/* if the dimensions are not divisible by a tile, enable the bbox */
			if( (bb_width & 0xf) != 0 || (bb_height & 0xf) != 0)
			{
				bb_enable[i] = MALI_TRUE;
				bb_used = MALI_TRUE;
			}

			/* if the bbox dimensions don't add up, the setup is not legal (see #4 in the huge comment earlier) */
			if(bb_width != surf->format.width || bb_height != surf->format.height) output_valid = MALI_FALSE;
		}

		if ( MALI_PIXEL_FORMAT_ARGB_FP16 == surf->format.pixel_format
		 || MALI_PIXEL_FORMAT_B_FP16 == surf->format.pixel_format
		 || MALI_PIXEL_FORMAT_GB_FP16 == surf->format.pixel_format )
		{
			frame_builder->fp16_format_used = MALI_TRUE;
		}

	}

	/* HW only support rendering to non-0-sized outputs */
	if(output_width == 0 || output_height == 0) output_valid = MALI_FALSE;

	/* store new state in framebuilder */
	frame_builder->output_valid = output_valid;
	if(output_valid)
	{
		if(output_width != frame_builder->output_width || output_height != frame_builder->output_height)
		{
			frame_builder->output_width = output_width;
			frame_builder->output_height = output_height;
		}
		frame_builder->output_log2_scale_x = _mali_log_base2(largest_xscale);
		frame_builder->output_log2_scale_y = _mali_log_base2(largest_yscale);
		frame_builder->bounding_box_width = bb_width;
		frame_builder->bounding_box_height = bb_height;
		frame_builder->bounding_box_used = bb_used;
		for(i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; i++)
		{
			frame_builder->bounding_box_enable[i] = bb_enable[i];
		}
	} else{
		frame_builder->output_width = 0;
		frame_builder->output_height = 0;
		frame_builder->output_log2_scale_x = 0;
		frame_builder->output_log2_scale_y = 0;
		frame_builder->bounding_box_width = 0;
		frame_builder->bounding_box_height = 0;
		frame_builder->bounding_box_used = MALI_FALSE;
		for(i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; i++)
		{
			frame_builder->bounding_box_enable[i] = MALI_FALSE;
		}
	}

}

MALI_EXPORT struct mali_surface* _mali_frame_builder_get_output( mali_frame_builder *frame_builder,
                                                                    u32 wbx_id, u32* out_usage)
{
	MALI_DEBUG_ASSERT_POINTER( frame_builder );


	MALI_DEBUG_ASSERT_RANGE( (int)wbx_id, 0, MALI200_WRITEBACK_UNIT_COUNT - 1);
	if(out_usage) *out_usage = frame_builder->output_buffers[wbx_id].usage;

	return frame_builder->output_buffers[wbx_id].buffer;
}

MALI_EXPORT void _mali_frame_builder_set_readback( mali_frame_builder *frame_builder,
                                                  u32 readback_surface_id,
                                                  struct mali_surface *surface,
                                                  u32 usage )
{
	struct mali_frame_builder_output_chain *readback;

	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	MALI_DEBUG_ASSERT_RANGE( (int)readback_surface_id, 0, MALI_READBACK_SURFACES_COUNT - 1);

	/* attach surfaces to the fbuilder, detach old surfaces */
	if ( NULL != surface )
	{
		_mali_surface_addref(surface);
	}

	readback = &frame_builder->readback_buffers[readback_surface_id];

	if(readback->buffer)
	{
		_mali_surface_deref(readback->buffer);
	}

	readback->buffer = surface;
	readback->usage  = usage;

}

MALI_EXPORT struct mali_surface* _mali_frame_builder_get_readback( mali_frame_builder *frame_builder,
                                                                    u32 readback_surface_id, u32* readback_usage)
{
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	MALI_DEBUG_ASSERT_RANGE( (int)readback_surface_id, 0, MALI_READBACK_SURFACES_COUNT - 1);
	if(readback_usage) *readback_usage = frame_builder->readback_buffers[readback_surface_id].usage;

	return frame_builder->readback_buffers[readback_surface_id].buffer;
}

MALI_EXPORT void _mali_frame_builder_get_consumer_pp_render( mali_frame_builder *frame_builder, mali_ds_consumer_handle *ds_consumer_pp_render )
{
	mali_internal_frame* frame = NULL;
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );
	*ds_consumer_pp_render = frame->ds_consumer_pp_render;
}

MALI_EXPORT void _mali_frame_builder_set_lock_output_callback( mali_frame_builder *frame_builder,
                                                               mali_frame_cb_func cb_func,
                                                               mali_frame_cb_param cb_param,
                                                               mali_ds_consumer_handle *ds_consumer_pp_render )
{
	mali_internal_frame* frame = NULL;
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );
	frame->cb_func_lock_output = cb_func;
	frame->cb_param_lock_output = cb_param;
	if ( ds_consumer_pp_render ) *ds_consumer_pp_render = frame->ds_consumer_pp_render;

}

MALI_EXPORT void _mali_frame_builder_set_complete_output_callback( mali_frame_builder *frame_builder,
                                                                   mali_frame_cb_func cb_func,
                                                                   mali_frame_cb_param cb_param )
{
	mali_internal_frame *frame = NULL;
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );
	frame->cb_func_complete_output = cb_func;
	frame->cb_param_complete_output = cb_param;
}

MALI_EXPORT void _mali_frame_builder_set_acquire_output_callback( mali_frame_builder *frame_builder, 
                                                                 mali_frame_cb_func cb_func, 
                                                                 mali_frame_cb_param cb_param )
{
	mali_internal_frame* frame = NULL;
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );
	frame->cb_func_acquire_output = cb_func;
	frame->cb_param_acquire_output = cb_param;
}

#if MALI_EXTERNAL_SYNC

MALI_EXPORT void _mali_frame_builder_set_pp_job_start_callback(
	mali_frame_builder *frame_builder,
	mali_frame_pp_job_start_cb_func cb_func,
	mali_frame_cb_param cb_param )
{
	mali_internal_frame *frame = NULL;
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );
	frame->cb_func_pp_job_start = cb_func;
	frame->cb_param_pp_job_start = cb_param;
}

MALI_EXPORT void _mali_frame_builder_set_output_fence( mali_frame_builder *frame_builder, mali_fence_handle fence )
{
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	if (MALI_FENCE_INVALID_HANDLE != frame_builder->output_fence)
	{
		mali_fence_release(frame_builder->output_fence);
	}

	frame_builder->output_fence = fence;
}

MALI_EXPORT void _mali_frame_builder_set_fence_sync_data( mali_frame_builder *frame_builder, void *data )
{
	mali_internal_frame *frame;
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	frame->fence_sync = data;
}

#endif /* MALI_EXTERNAL_SYNC */

MALI_EXPORT void _mali_frame_builder_acquire_output( mali_frame_builder *frame_builder )
{
	mali_internal_frame* frame = NULL;
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	if ( NULL != frame->cb_func_acquire_output )
	{
		(frame->cb_func_acquire_output)(frame->cb_param_acquire_output);
	}
}

MALI_EXPORT void _mali_frame_builder_set_clear_value( mali_frame_builder *frame_builder,
                                                      u32 buffer_type,
                                                      u32 clear_value )
{

	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	switch( buffer_type )
	{
		case MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_R:
			MALI_DEBUG_ASSERT(clear_value <= 0xFFFF, ("Legal clear range for color is 0 - 0xFFFF inclusive"));
			if(frame_builder->color_clear_value[0] != clear_value)
			{
				frame_builder->buffer_state_per_plane &= ~MALI_COLOR_PLANE_MASK;
				frame_builder->buffer_state_per_plane |= MALI_COLOR_PLANE_DIRTY;
			}
			frame_builder->color_clear_value[0] = clear_value;
			break;
		case MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_G:
			MALI_DEBUG_ASSERT(clear_value <= 0xFFFF, ("Legal clear range for color is 0 - 0xFFFF inclusive"));
			if(frame_builder->color_clear_value[1] != clear_value)
			{
				frame_builder->buffer_state_per_plane &= ~MALI_COLOR_PLANE_MASK;
				frame_builder->buffer_state_per_plane |= MALI_COLOR_PLANE_DIRTY;
			}
			frame_builder->color_clear_value[1] = clear_value;
			break;
		case MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_B:
			MALI_DEBUG_ASSERT(clear_value <= 0xFFFF, ("Legal clear range for color is 0 - 0xFFFF inclusive"));
			if(frame_builder->color_clear_value[2] != clear_value)
			{
				frame_builder->buffer_state_per_plane &= ~MALI_COLOR_PLANE_MASK;
				frame_builder->buffer_state_per_plane |= MALI_COLOR_PLANE_DIRTY;
			}
			frame_builder->color_clear_value[2] = clear_value;
			break;
		case MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_A:
			MALI_DEBUG_ASSERT(clear_value <= 0xFFFF, ("Legal clear range for color is 0 - 0xFFFF inclusive"));
			if(frame_builder->color_clear_value[3] != clear_value)
			{
				frame_builder->buffer_state_per_plane &= ~MALI_COLOR_PLANE_MASK;
				frame_builder->buffer_state_per_plane |= MALI_COLOR_PLANE_DIRTY;
			}
			frame_builder->color_clear_value[3] = clear_value;
			break;
		case MALI_FRAME_BUILDER_BUFFER_BIT_DEPTH:
			MALI_DEBUG_ASSERT(clear_value <= 0xFFFFFF, ("Legal clear range for depth is 0 - 0xFFFFFF inclusive"));
			if(frame_builder->depth_clear_value != clear_value)
			{
				frame_builder->buffer_state_per_plane &= ~MALI_DEPTH_PLANE_MASK;
				frame_builder->buffer_state_per_plane |= MALI_DEPTH_PLANE_DIRTY;
			}
			frame_builder->depth_clear_value = clear_value;
			break;
		case MALI_FRAME_BUILDER_BUFFER_BIT_STENCIL:
			MALI_DEBUG_ASSERT(clear_value <= 0xFF, ("Legal clear range for stencil is 0 - 0xFF inclusive"));
			if(frame_builder->stencil_clear_value != clear_value)
			{
				frame_builder->buffer_state_per_plane &= ~MALI_STENCIL_PLANE_MASK;
				frame_builder->buffer_state_per_plane |= MALI_STENCIL_PLANE_DIRTY;
			}
			frame_builder->stencil_clear_value = clear_value;
			break;
		default:
			MALI_DEBUG_ASSERT( 0, ("invalid buffer type\n") );
			break;
	}
}

MALI_EXPORT u32 _mali_frame_builder_get_clear_value( mali_frame_builder *frame_builder,
                                                     u32 buffer_type )
{
	u32 clear_value;

	switch( buffer_type )
	{
	case MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_R:
		clear_value = frame_builder->color_clear_value[0];
		break;
	case MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_G:
		clear_value = frame_builder->color_clear_value[1];
		break;
	case MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_B:
		clear_value = frame_builder->color_clear_value[2];
		break;
	case MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_A:
		clear_value = frame_builder->color_clear_value[3];
		break;
	case MALI_FRAME_BUILDER_BUFFER_BIT_DEPTH:
		clear_value = frame_builder->depth_clear_value;
		break;
	case MALI_FRAME_BUILDER_BUFFER_BIT_STENCIL:
		clear_value = frame_builder->stencil_clear_value;
		break;
	default:
		clear_value = 0;
		MALI_DEBUG_ASSERT( 0, ("invalid buffer type\n") );
		break;
	}

	return clear_value;
}

MALI_EXPORT mali_job_completion_status _mali_frame_builder_get_framebuilder_completion_status( mali_frame_builder *frame_builder )
{
	mali_job_completion_status frame_builder_error = MALI_JOB_STATUS_END_SUCCESS;
	mali_job_completion_status err;
	u32 i;

	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	/* Cycles through and clears the error state of all frames. The error of the last erroneous
	 * frame is returned */
	for (i = 0; i < frame_builder->iframe_count ; ++i)
	{
		if ( (frame_builder->iframes != NULL) && (frame_builder->iframes[i] != NULL) )
		{
			err = (mali_job_completion_status)_mali_sys_atomic_get( &(frame_builder->iframes[i]->completion_status) );
			if ( MALI_JOB_STATUS_END_SUCCESS != err )
			{
				/* Clear frame state */
				frame_builder_error = err;
				_mali_sys_atomic_set( &(frame_builder->iframes[i]->completion_status), (u32)MALI_JOB_STATUS_END_SUCCESS );
			}
		}
	}

#if MALI_PERFORMANCE_KIT
	if (mali_complete_jobs_instantly == 1)
	{
		frame_builder_error = MALI_JOB_STATUS_END_SUCCESS; /* fake success */
	}
#endif

	return frame_builder_error;
}


/* Mali200 frame builder specific functions ---------------------------------------------------------------- */
MALI_EXPORT void _mali_frame_builder_update_fragment_stack( mali_frame_builder *frame_builder, u32 stack_start, u32 stack_size )
{
	u32 stack_grow;
	mali_internal_frame* frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	/* store the maximum stack requirements for the fragment shader */
	stack_grow  = stack_size - stack_start;

	frame->fs_stack.start = MAX( frame->fs_stack.start, stack_start );
	frame->fs_stack.grow  = MAX( frame->fs_stack.grow,  stack_grow );

}

MALI_EXPORT mali_err_code _mali_frame_builder_scissor( mali_frame_builder *frame_builder, u32 left, u32 top, u32 right, u32 bottom,
                                                       mali_gp_plbu_cmd * const buffer, u32 * const index, const u32 buffer_len )
{
	mali_err_code err = MALI_ERR_NO_ERROR;
	mali_gp_job_handle gp_job;
	mali_internal_frame* frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	MALI_DEBUG_ASSERT( (frame->state != FRAME_CLEAN),
	                   ("must call _mali_frame_builder_use() before this function\n") );

	MALI_DEBUG_ASSERT( (( (u32) frame_builder->output_width  > left) &&
	                    ( (u32) frame_builder->output_width  > right) &&
	                    ( (u32) frame_builder->output_height > top) &&
	                    ( (u32) frame_builder->output_height > bottom)),
	                   ("scissorbox (%d, %d, %d, %d) is not within the frame_builder dimensions ( %d, %d)\n",
	                   left, top, right, bottom, frame_builder->output_width, frame_builder->output_height ) );

	/* early out if there is no change in the scissor state */
	if( (left == frame_builder->scissor.left) && (right  == frame_builder->scissor.right) &&
		(top  == frame_builder->scissor.top)  && (bottom == frame_builder->scissor.bottom) ) return MALI_ERR_NO_ERROR;

	/* store the new current scissorbox */
	frame_builder->scissor.left   = left;
	frame_builder->scissor.top    = top;
	frame_builder->scissor.right  = right;
	frame_builder->scissor.bottom = bottom;

	gp_job = frame->gp_job;
	MALI_DEBUG_ASSERT_HANDLE( gp_job );

	if ( buffer != NULL )
	{
		u32 idx = *index;
		MALI_DEBUG_ASSERT( idx < buffer_len, ( "plbu stream buffer overflow" ) );
		buffer[idx++] = GP_PLBU_COMMAND_SET_SCISSOR( top, bottom, left, right);
		MALI_DEBUG_ASSERT( idx <= buffer_len, ( "plbu stream buffer overflow" ) );
		*index = idx;
	} else
	{
		MALI_IGNORE( buffer_len );
		MALI_IGNORE( index );
		err = _mali_gp_job_add_plbu_cmd( gp_job, GP_PLBU_COMMAND_SET_SCISSOR( top, bottom, left, right ) );
	}

	return err;
}

MALI_EXPORT mali_err_code _mali_frame_builder_viewport( mali_frame_builder *frame_builder, float left, float top, float right, float bottom,
                                                        mali_gp_plbu_cmd * const buffer, u32 * const index, const u32 buffer_len )
{
	mali_err_code err = MALI_ERR_NO_ERROR;

	/* Init of viewportbox[] to stop an incorrect warning from Coverity as it doesn't handle unions correctly */
	union {
		float f;
		u32   i;
	} viewportbox[4] = { {0}, {0}, {0}, {0}};
	mali_internal_frame* frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	MALI_DEBUG_ASSERT( (frame->state != FRAME_CLEAN),
	                   ("must call _mali_frame_builder_use() before this function\n") );

	/* early out if there is no change in the scissor state */
	if( (left == frame_builder->vpbox.left) && (right  == frame_builder->vpbox.right) &&
		(top  == frame_builder->vpbox.top)  && (bottom == frame_builder->vpbox.bottom) ) return MALI_ERR_NO_ERROR;

	/* store the new current viewportbox */
	frame_builder->vpbox.left   = left;
	frame_builder->vpbox.top    = top;
	frame_builder->vpbox.right  = right;
	frame_builder->vpbox.bottom = bottom;

	frame->vpbox.left   = left;
	frame->vpbox.top    = top;
	frame->vpbox.right  = right;
	frame->vpbox.bottom = bottom;

	/* Setting initial viewportbox to fullscreen
	 * Using unions since the binary representation of the floats have to be written to the PLBUs config registers
	 */
#if HARDWARE_ISSUE_3396
	/* the interface had a different behavior prior to this fix. Must handle inclusive bounds by adding x < 1 */
	viewportbox[ 0 ].f = (float) left;
	viewportbox[ 1 ].f = (float) right  + 0.99;
	viewportbox[ 2 ].f = (float) top;
	viewportbox[ 3 ].f = (float) bottom + 0.99;
#else
	viewportbox[ 0 ].f = left;
	viewportbox[ 1 ].f = right;
	viewportbox[ 2 ].f = top;
	viewportbox[ 3 ].f = bottom;
#endif

	if ( buffer != NULL )
	{
		u32 idx = *index;
		MALI_DEBUG_ASSERT( idx < buffer_len, ( "plbu stream buffer overflow" ) );
		buffer[idx++] = GP_PLBU_COMMAND_WRITE_CONF_REG( viewportbox[0].i, GP_PLBU_CONF_REG_SCISSOR_LEFT );
		buffer[idx++] = GP_PLBU_COMMAND_WRITE_CONF_REG( viewportbox[1].i, GP_PLBU_CONF_REG_SCISSOR_RIGHT );
		buffer[idx++] = GP_PLBU_COMMAND_WRITE_CONF_REG( viewportbox[2].i, GP_PLBU_CONF_REG_SCISSOR_TOP );
		buffer[idx++] = GP_PLBU_COMMAND_WRITE_CONF_REG( viewportbox[3].i, GP_PLBU_CONF_REG_SCISSOR_BOTTOM );
		MALI_DEBUG_ASSERT( idx <= buffer_len, ( "plbu stream buffer overflow" ) );
		*index = idx;
	} else
	{
		mali_gp_plbu_cmd cmds[ 4 ];
		mali_gp_job_handle gp_job;

		MALI_IGNORE( buffer_len );
		MALI_IGNORE( index );

		gp_job = frame->gp_job;
		MALI_DEBUG_ASSERT_HANDLE( gp_job );

		cmds[ 0 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( viewportbox[0].i, GP_PLBU_CONF_REG_SCISSOR_LEFT );
		cmds[ 1 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( viewportbox[1].i, GP_PLBU_CONF_REG_SCISSOR_RIGHT );
		cmds[ 2 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( viewportbox[2].i, GP_PLBU_CONF_REG_SCISSOR_TOP );
		cmds[ 3 ] = GP_PLBU_COMMAND_WRITE_CONF_REG( viewportbox[3].i, GP_PLBU_CONF_REG_SCISSOR_BOTTOM );

		err = _mali_gp_job_add_plbu_cmds( gp_job, cmds, sizeof( cmds ) / sizeof( mali_gp_plbu_cmd ) );
	}

	return err;
}

MALI_EXPORT mali_base_ctx_handle _mali_frame_builder_get_base_ctx( mali_frame_builder *frame_builder )
{
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	return frame_builder->base_ctx;
}

MALI_EXPORT void _mali_frame_builder_set_split_count( mali_frame_builder *frame_builder, u32 split_cnt )
{
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	if (split_cnt != frame_builder->curr_pp_split_count)
	{
		MALI_DEBUG_ASSERT_RANGE(split_cnt, 1, MALI_MAX_PP_SPLIT_COUNT);

		frame_builder->curr_pp_split_count = split_cnt;
	}
}

MALI_EXPORT mali_bool _mali_frame_builder_incremental_rendering_requested( mali_frame_builder *frame_builder )
{
	mali_internal_frame* frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	return (frame->num_flushes_since_reset > frame_builder->incremental_render_flush_threshold);
}

MALI_EXPORT mali_err_code _mali_frame_builder_write_lock( mali_frame_builder *frame_builder, u8 dirtiness_mask  )
{
	u32 index;
	mali_internal_frame *frame;
	mali_err_code error = MALI_ERR_NO_ERROR;

	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	MALI_DEBUG_ASSERT(frame_builder->output_valid == MALI_TRUE, ("Framebuilder outputs set up incorrectly"));

	_mali_frame_builder_acquire_output( frame_builder );

	if (dirtiness_mask & MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_ALL_CHANNELS)
	{
		frame_builder->buffer_state_per_plane &= ~MALI_COLOR_PLANE_MASK;
		frame_builder->buffer_state_per_plane |= MALI_COLOR_PLANE_DIRTY;  
	}
	if (dirtiness_mask & MALI_FRAME_BUILDER_BUFFER_BIT_DEPTH)
	{
		frame_builder->buffer_state_per_plane &= ~MALI_DEPTH_PLANE_MASK;
		frame_builder->buffer_state_per_plane |= MALI_DEPTH_PLANE_DIRTY;  
	}
	if (dirtiness_mask & MALI_FRAME_BUILDER_BUFFER_BIT_STENCIL)
	{
		frame_builder->buffer_state_per_plane &= ~MALI_STENCIL_PLANE_MASK;
		frame_builder->buffer_state_per_plane |= MALI_STENCIL_PLANE_DIRTY;  
	}
	if (dirtiness_mask & MALI_FRAME_BUILDER_BUFFER_BIT_MULTISAMPLE) frame_builder->preserve_multisample = MALI_TRUE;	

	/* take the frame lock */
	frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	/* wait for current frame to finish any mali processing */
	_mali_frame_wait_and_take_mutex( frame );

	error = _frame_builder_use_internal( frame_builder, frame );

	_mali_sys_mutex_unlock( frame->mutex );

	if ( MALI_ERR_NO_ERROR != error )
	{
		return error;
	}

	for ( index = 0; index < MALI200_WRITEBACK_UNIT_COUNT; index++)
	{
		struct mali_surface* surface = frame_builder->output_buffers[index].buffer;
		if( surface == NULL ) continue;

	}


	/* At this point we have acquired locks on the relevant surfaces. */
	MALI_SUCCESS;
}

MALI_EXPORT void _mali_frame_builder_write_unlock( mali_frame_builder *frame_builder )
{
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	MALI_IGNORE( frame_builder );
}



MALI_EXPORT void _mali_frame_builder_discard_surface_write_back(mali_frame_builder* frame_builder, mali_surface* surf, u32 wb_unit)
{
	u32 i;
	u32 fb_ptr;

	MALI_DEBUG_ASSERT_POINTER(frame_builder);
	MALI_DEBUG_ASSERT_POINTER(surf);
	MALI_DEBUG_ASSERT_POINTER(surf->mem_ref);

	fb_ptr = (u32) _mali_mem_mali_addr_get( surf->mem_ref->mali_memory, surf->mem_offset );
	for(i=0; i<frame_builder->iframe_count; i++)
	{
		mali_internal_frame* iframe = frame_builder->iframes[i];
		MALI_DEBUG_ASSERT_POINTER(iframe);
		_mali_sys_mutex_lock(iframe->mutex);

		if(FRAME_RENDERING==iframe->state)
		{
			mali_pp_job_handle pp_job = iframe->pp_job;
			
			if ( pp_job )
			{
				if( fb_ptr == _m200_wb_attachment_address( pp_job, wb_unit ) )
				{
					if( _mali_pp_job_is_updateable(pp_job) )
					{
						_m200_wb_discard_attachment( pp_job, wb_unit );
					}
				}
			}
		}
		_mali_sys_mutex_unlock(iframe->mutex);
	}
}

MALI_EXPORT mali_err_code _mali_frame_builder_add_surface_read_dependency(mali_frame_builder* frame_builder, mali_surface* surface)
{
	mali_err_code err;
	mali_internal_frame* frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	MALI_DEBUG_ASSERT( frame->state == FRAME_DIRTY , ("Adding a surface_read_dependency to an undirty frame! state = %d \n", frame->state));

	/**
	 * Comment on this if condition: 
	 * Bug 6400 prevents unmovable surfaces from being Copy-on-writed
	 * Copy on Write is the currently only way to avoid dependency deadlocks. 
	 * To avoid deadlocks with pixmaps and other non-movable surfaces, adding a read dependency is not possible 
	 */
	if(! (surface->flags & MALI_SURFACE_FLAG_DONT_MOVE) )
	{
		/* only add READ dependency on movable surfaces */
		err = mali_ds_connect(frame->ds_consumer_pp_render, surface->ds_resource, MALI_DS_READ);
		surface->flags |= MALI_SURFACE_FLAG_READ_PENDING;
		if(err != MALI_ERR_NO_ERROR) return err;
	}

	err = _mali_surfacetracking_add(frame->surfacetracking, surface, MALI_SURFACE_TRACKING_READ);

	return err;
}

MALI_EXPORT mali_incremental_render_capabilities _mali_frame_builder_get_targets_to_preserve( mali_frame_builder* frame_builder)
{
	int i;
	mali_incremental_render_capabilities capabilities = 0;

	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	/* Preserve COLOR, DEPTH or STENCIL bits? 
	 * if the buffer bit is "cleared", we need to output the buffer later in the incremental render, but we don't need the readback. We could just clear again.
	 */
	if( (frame_builder->buffer_state_per_plane & (MALI_COLOR_PLANE_DIRTY | MALI_COLOR_PLANE_CLEAR_TO_COLOR))) capabilities |= MALI_INCREMENTAL_RENDER_PRESERVE_COLOR_BUFFER;
	if( (frame_builder->buffer_state_per_plane & (MALI_DEPTH_PLANE_DIRTY | MALI_DEPTH_PLANE_CLEAR_TO_COLOR))) capabilities |= MALI_INCREMENTAL_RENDER_PRESERVE_DEPTH_BUFFER;
	if( (frame_builder->buffer_state_per_plane & (MALI_STENCIL_PLANE_DIRTY | MALI_STENCIL_PLANE_CLEAR_TO_COLOR))) capabilities |= MALI_INCREMENTAL_RENDER_PRESERVE_STENCIL_BUFFER;
		
	/* Preserve SUPERSAMPLE bit? */
	for ( i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; i++ )
	{
		if(frame_builder->output_buffers[i].buffer)
		{
			if(frame_builder->output_buffers[i].usage & (
						MALI_OUTPUT_DOWNSAMPLE_X_2X |
						MALI_OUTPUT_DOWNSAMPLE_X_4X |
						MALI_OUTPUT_DOWNSAMPLE_X_8X |
						MALI_OUTPUT_DOWNSAMPLE_Y_2X |
						MALI_OUTPUT_DOWNSAMPLE_Y_4X |
						MALI_OUTPUT_DOWNSAMPLE_Y_8X |
						MALI_OUTPUT_DOWNSAMPLE_Y_16X
						)
			  )
				capabilities |= MALI_INCREMENTAL_RENDER_PRESERVE_SUPERSAMPLING;
			break;
		}
	}

	/* Preserve MULTISAMPLE bit? */
	if(frame_builder->preserve_multisample) capabilities |= MALI_INCREMENTAL_RENDER_PRESERVE_MULTISAMPLING;

	return capabilities;
}

#if MALI_TEST_API
MALI_EXPORT void _mali_frame_builder_set_incremental_render_threshold( mali_frame_builder *frame_builder,
                                                                       u32                 threshold )
{
	frame_builder->incremental_render_flush_threshold = threshold;
}
#endif
