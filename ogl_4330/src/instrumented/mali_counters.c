/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_counters.h"

#include "mali_system.h"
#include "base/mali_runtime.h"
#include "base/mali_macros.h"
#include "base/mali_debug.h"

#include "shared/mali_named_list.h"

#include "mali_instrumented_context.h"
#include "mali_instrumented_frame.h"
#include "mali_instrumented_context_types.h"

#include "mali_pp_instrumented.h"
#include "mali_gp_instrumented.h"

/**
 * Set the "activated" flag for selected counters
 * ctx->counters[index].actuivated = activate_type
 *
 * \param ctx                the instrumented context
 * \param counter_indices    indices in the counter array to activate/deactivate
 * \param counter_amount     the size of the \a counter_indices array
 * \param activate_type         whether to deactivate, activate for use in derived - or really activate
 */
MALI_STATIC mali_err_code activate_counters(mali_instrumented_context* ctx,
		const unsigned int *counter_indices,
		int unsigned counter_amount, mali_activate_type activ_type) MALI_CHECK_RESULT;

/**
 * Get a pointer to a mali_counter by its unique ID
 *
 * \param id                 id of counter to fetch
 * \param frame              pointer to frame to fetch counter from.
 * \returns                  a pointer to the counter value if successful,
 *                           or NULL if the counter could not be found or is not activated
 */
MALI_STATIC s64 *get_counter_value_by_id(u32 id, mali_instrumented_frame *frame);

/**
 * Add counter to header tree
 *
 * \param index              index in the counter array
 */
MALI_STATIC mali_err_code add_counter_to_tree(mali_instrumented_context* ctx, u32 index);

mali_err_code  add_counter_to_tree(mali_instrumented_context* ctx, u32 index)
{
	mali_counter_group *header;
	void *realloc;
	MALI_DEBUG_ASSERT_POINTER(ctx);
	header = ctx->current_header;
	MALI_DEBUG_ASSERT_POINTER(header);

	if (( MALI_FALSE == header->leaf) && (header->num_children > 0))
	{
		MALI_DEBUG_PRINT(0, ( "Trying to add leaves to a mali_counter_group [%s] with %d branches\n",
		  header->name,
		  header->num_children ));
		return MALI_ERR_FUNCTION_FAILED;
	}


	realloc = _mali_sys_realloc(
			header->children.counter_indices,
			sizeof(u32) * (header->num_children + 1));
	MALI_CHECK(NULL != realloc, MALI_ERR_OUT_OF_MEMORY);
	header->children.counter_indices = realloc;

	header->leaf = MALI_TRUE;
	header->num_children++;
	header->children.counter_indices[header->num_children - 1] = index;
	MALI_SUCCESS;
}

MALI_EXPORT
const mali_counter_group *_instrumented_get_counter_headers(mali_instrumented_context* ctx)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);
	return ctx->header_root;
}

MALI_EXPORT
int _instrumented_get_number_of_counters(mali_instrumented_context* ctx)
{
	MALI_DEBUG_ASSERT(NULL != ctx, ("mali_instrumented_context is NULL"));
	return ctx->num_counters;
}

MALI_EXPORT
const mali_counter_info *_instrumented_get_counter_information(mali_instrumented_context* ctx,
		u32 counter_index)
{
	MALI_DEBUG_ASSERT(NULL != ctx, ("mali_instrumented_context is NULL"));
	MALI_DEBUG_ASSERT(ctx->num_counters == 0 || NULL != ctx->counters, ("ctx->counters is NULL"));
	if (counter_index >= ctx->num_counters) return NULL;
	return ctx->counters[counter_index].info;
}

mali_err_code activate_counters(mali_instrumented_context* ctx, const unsigned int *counter_indices,
		unsigned int indices_count,  mali_activate_type activate_type)
{
	unsigned int nr, ix;
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->counters);
	if (NULL == counter_indices)
	{
		MALI_DEBUG_PRINT(0, ("activate_counters received invalid counter_indices\n") );
		return MALI_ERR_FUNCTION_FAILED;
	}

	/* Check that all indices are valid */
	for (nr = 0; nr < indices_count; ++nr)
	{
		ix = counter_indices[nr];
		if (ix >= ctx->num_counters)  return MALI_ERR_FUNCTION_FAILED;
	}

	/* Activate or turn off the counters given in the parameter list */
	for (nr = 0; nr < indices_count; ++nr)
	{
		ix = counter_indices[nr];
		ctx->counters[ix].activated = activate_type;
	}

	/* Turn off all hw counters used for computing derived 'counters' */
	for (ix = 0; ix < ctx->num_counters; ++ix)
	{
	    if (MALI_ACTIVATE_DERIVED == ctx->counters[ix].activated)
	    {
		ctx->counters[ix].activated = MALI_ACTIVATE_NOT;
	    }
	}

	/* and next: turn on the ones still needed for computing derived */
	_mali_gp_activate_derived(ctx);
	_mali_pp_activate_derived(ctx);

	/* make sure sampling is properly turned on or off */
	ctx->sampling = MALI_FALSE;
	for (ix = 0; ix < ctx->num_counters; ++ix)
	{
		if (MALI_ACTIVATE_NOT != ctx->counters[ix].activated)
		{
			ctx->sampling = MALI_TRUE;
			MALI_SUCCESS;
		}
	}
	MALI_SUCCESS;
} /* activate_counters */

MALI_EXPORT
mali_err_code _instrumented_activate_counters(mali_instrumented_context* ctx,
	const unsigned int *counter_indices, unsigned int indices_count)
{
	return activate_counters(ctx, counter_indices, indices_count, MALI_ACTIVATE_EXPLICIT);
}

MALI_EXPORT
mali_err_code _instrumented_deactivate_counters(mali_instrumented_context* ctx,
	const unsigned int *counter_indices, unsigned int indices_count)
{
	return activate_counters(ctx, counter_indices, indices_count, MALI_ACTIVATE_NOT);
}

MALI_EXPORT
void _instrumented_activate_counter_derived(mali_instrumented_context* ctx, u32 id)
{
	u32 index;
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->counters);
	MALI_DEBUG_ASSERT_POINTER(ctx->counter_map);
	/* The ctx->counter_map named list stores index + 1 */
	index = (u32)(MALI_REINTERPRET_CAST(s32)__mali_named_list_get(ctx->counter_map, id) - 1);
	MALI_DEBUG_ASSERT(index < ctx->num_counters, ("Counter index out of range"));
	if (MALI_ACTIVATE_NOT == ctx->counters[index].activated)
	{
		ctx->counters[index].activated = MALI_ACTIVATE_DERIVED;
	}
	ctx->sampling = MALI_TRUE;
}

MALI_EXPORT
mali_bool _instrumented_is_counter_active(mali_instrumented_context* ctx, u32 id)
{
	s32 index;
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->counters);
	MALI_DEBUG_ASSERT_POINTER(ctx->counter_map);

	/* The ctx->counter_map named list stores index + 1 */
	index = MALI_REINTERPRET_CAST(s32)__mali_named_list_get(ctx->counter_map, id)-1;
	return index >= 0 && ctx->counters[index].activated != MALI_ACTIVATE_NOT;
}

MALI_EXPORT
mali_bool _instrumented_is_sampling_enabled(mali_instrumented_context *ctx)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);
	return ctx->sampling;
}

MALI_EXPORT
mali_err_code _instrumented_register_counter(mali_instrumented_context* ctx, u32 id,
		const char *name, const char *description, const char *unit,
		mali_frequency_scale scale_to_hz)
{
	unsigned int index; /* Index in counter array */
	mali_counter *counter;
	void* realloc = NULL;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->counter_map);
	MALI_DEBUG_ASSERT(!ctx->inited, ("Cannot register counter in already initialized context.\n" ));
	index = ctx->num_counters;


	/* reassign ctx->counters only if realloc succeeds */
	realloc = _mali_sys_realloc(ctx->counters, (ctx->num_counters + 1) * sizeof(mali_counter));
	MALI_CHECK(NULL != realloc, MALI_ERR_OUT_OF_MEMORY);
	ctx->counters = realloc;

	counter = &ctx->counters[index];
	MALI_DEBUG_ASSERT_POINTER(counter);
	MALI_CHECK(NULL != (counter->info = _mali_sys_malloc(sizeof(mali_counter_info))),
			MALI_ERR_OUT_OF_MEMORY);

	/* first initialize to NULL, in case strdup is not called for some reason */
	counter->info->name = NULL;
	counter->info->description = NULL;
	counter->info->unit = NULL;

	counter->info->id = id;
	counter->info->name = _mali_sys_strdup(name);
	counter->info->description = _mali_sys_strdup(description);
	counter->info->unit = _mali_sys_strdup(unit);
	counter->info->scale_to_hz = scale_to_hz;
	MALI_CHECK_GOTO(counter->info->name != NULL
			&& counter->info->description != NULL
			&& counter->info->unit != NULL, outofmem);
	counter->activated = MALI_ACTIVATE_NOT;
	MALI_CHECK_GOTO(MALI_ERR_NO_ERROR == add_counter_to_tree(ctx, index), outofmem);
	MALI_CHECK_GOTO(MALI_ERR_NO_ERROR == __mali_named_list_insert( ctx->counter_map, id, (void*)(index+1)), outofmem);

	ctx->num_counters++;

	MALI_SUCCESS;

outofmem:

	/* out of memory after one or more of the strings have been
	 * attempted initialized, so clean them up */
	if (counter->info->name != NULL) _mali_sys_free(counter->info->name);
	if (counter->info->description != NULL) _mali_sys_free(counter->info->description);
	if (counter->info->unit != NULL) _mali_sys_free(counter->info->unit);
	_mali_sys_free(counter->info);

	MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);

}

MALI_EXPORT
mali_err_code _instrumented_start_group(mali_instrumented_context* ctx,
		const char *name, const char *description, u32 frequency)
{
	mali_counter_group *new_group;
	mali_counter_group *old_group;
	void *realloc;
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(name);
	MALI_DEBUG_ASSERT_POINTER(description);

	old_group = ctx->current_header;
	MALI_DEBUG_ASSERT_POINTER(old_group);
	MALI_DEBUG_ASSERT(!ctx->inited, ("Cannot add group to an already initialized context.\n" ));

	if (old_group->leaf)
	{
		MALI_DEBUG_ASSERT( MALI_FALSE,("Can't add counter group [%s] to "
				"a leaf counter group [%s].",
				name, old_group->name));
		return MALI_ERR_FUNCTION_FAILED;
	}
	new_group = _mali_sys_malloc(sizeof(mali_counter_group));
	if (NULL == new_group) return MALI_ERR_OUT_OF_MEMORY;

	new_group->name = NULL;
	new_group->description = NULL;
	new_group->name = _mali_sys_strdup(name);
	new_group->description = _mali_sys_strdup(description);
	new_group->frequency = frequency;
	new_group->leaf = MALI_FALSE;
	new_group->num_children = 0;
	new_group->children.groups = NULL;
	new_group->parent = old_group;

	MALI_CHECK_GOTO(NULL != new_group->name && NULL != new_group->description, outofmem);

	realloc = _mali_sys_realloc(old_group->children.groups,
			sizeof(mali_counter_group*)*(old_group->num_children + 1));
	MALI_CHECK_GOTO(NULL != realloc, outofmem);
	old_group->num_children++;
	old_group->children.groups = realloc;

	/* Insert the new group in the old group's list of children */
	old_group->children.groups[old_group->num_children - 1] = new_group;

	ctx->current_header = new_group;
	MALI_SUCCESS;

outofmem:
	if (new_group->name != NULL) _mali_sys_free(new_group->name);
	if (new_group->description != NULL) _mali_sys_free(new_group->description);
	_mali_sys_free(new_group);
	MALI_ERROR(MALI_ERR_OUT_OF_MEMORY);
}

MALI_EXPORT
void _instrumented_end_group(mali_instrumented_context* ctx)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT(!ctx->inited, ("Cannot end group in an already initialized context.\n" ));
	if (NULL == ctx->current_header->parent)
	{
		MALI_DEBUG_PRINT(0, ("Too many _instrumented_end_group "
				"compared to _instrumented_start_groups\n"));
		return;
	}
	ctx->current_header = ctx->current_header->parent;
}

s64 *get_counter_value_by_id(u32 id, mali_instrumented_frame *frame)
{
	s32 index;
	s64 *counter_values;
	mali_instrumented_context *ctx;

	if (NULL == frame) return NULL;

	ctx = frame->instrumented_ctx;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->counters);
	MALI_DEBUG_ASSERT_POINTER(ctx->counter_map);

	/* The ctx->counter_map named list stores index + 1 */
	index = MALI_REINTERPRET_CAST(s32)__mali_named_list_get(ctx->counter_map, id)-1;
	if (index < 0) return NULL;
	if (!ctx->counters[index].activated)  return NULL;
	counter_values = frame->counter_values;
	if (NULL == counter_values) return NULL;

	return &counter_values[index];
}

MALI_EXPORT
void _instrumented_set_counter(u32 id, mali_instrumented_frame *frame, s64 value)
{
	mali_instrumented_context *ctx;
	s64 *counter_value;

	if (NULL == frame) return;

	ctx = frame->instrumented_ctx;
	MALI_DEBUG_ASSERT_POINTER(ctx);

	if (!ctx->inited)
	{
		MALI_DEBUG_ASSERT(0, ("Cannot add to a counter in an uninitialized context" ));
		return;
	}
	if (!ctx->sampling) return;
	counter_value = get_counter_value_by_id(id, frame);
	if (NULL != counter_value)
	{
		*counter_value = value;
	}
}

MALI_EXPORT
void _instrumented_add_to_counter(u32 id, mali_instrumented_frame *frame, s64 value)
{
	mali_instrumented_context *ctx;
	s64 *counter_value, prev_value;

	if (NULL == frame) return;

	ctx = frame->instrumented_ctx;
	MALI_DEBUG_ASSERT_POINTER(ctx);

	if (!ctx->inited)
	{
		MALI_DEBUG_ASSERT(0, ("Cannot add to a counter in an uninitialized context" ));
		return;
	}

	if (!ctx->sampling) return;

	counter_value = get_counter_value_by_id(id, frame);

	if (NULL != counter_value)
	{
		prev_value = *counter_value;
		*counter_value += value;

		/* check wrapping conditions */
		if (prev_value > 0 && value > 0 && *counter_value < 0)
		{
			*counter_value = MALI_S64_MAX;
		}
		else if (prev_value < 0 && value < 0 && *counter_value > 0)
		{
			*counter_value = MALI_S64_MIN;
		}
	}
}

MALI_EXPORT
s64 _instrumented_get_counter(u32 id, mali_instrumented_frame *frame)
{
	s64 *counter_value;

	if (NULL == frame) return 0;

	counter_value = get_counter_value_by_id(id, frame);

	if (NULL == counter_value) return 0;
	return *counter_value;
}

MALI_EXPORT
void _instrumented_counter_free_group(mali_counter_group *group)
{
	unsigned int i;

	_mali_sys_free(MALI_CONST_CAST(char*)group->name);
	_mali_sys_free(MALI_CONST_CAST(char*)group->description);
	if (group->leaf)
	{
		_mali_sys_free(group->children.counter_indices);
	}
	else
	{
		for (i = 0; i < group->num_children; i++)
		{
			_instrumented_counter_free_group(group->children.groups[i]);
			_mali_sys_free(group->children.groups[i]);
		}
		_mali_sys_free(group->children.groups);
	}
}

MALI_EXPORT
void _instrumented_free_counter_info(mali_counter *counter)
{
	MALI_DEBUG_ASSERT_POINTER(counter);
	_mali_sys_free(counter->info->name);
	_mali_sys_free(counter->info->description);
	_mali_sys_free(counter->info->unit);
	_mali_sys_free(counter->info);
}

MALI_STATIC void print_counters(mali_instrumented_frame *frame, mali_counter_group *group, unsigned int indent)
{
	unsigned int i, j;
	mali_instrumented_context *ctx = frame->instrumented_ctx;
	MALI_DEBUG_ASSERT_POINTER(ctx);

	/* Don't print the Root group (it's implicit in <countertree>). */
	if (group != ctx->header_root)
	{

		for ( i = 0; i < indent; i++) { _mali_sys_printf("+"); }
		_mali_sys_printf(" %s", group->name);
		if (group->frequency > 0)
		{
			_mali_sys_printf(" @ %dMHz", group->frequency);
		}
		_mali_sys_printf("\n");
	}

	for (i = 0; i < group->num_children; i++)
	{
		if (group->leaf)
		{
			const mali_counter_info *info;
			s64 value;
			u32 counter_id = group->children.counter_indices[i];

			if (! ctx->counters[counter_id].activated) continue;

			for ( j = 0; j < indent; j++) { _mali_sys_printf(" "); }

			info = _instrumented_get_counter_information(ctx, counter_id);
			value = frame->counter_values[counter_id];
			_mali_sys_printf("  %9llu\t%s\t%s\n", value, info->unit, info->name);
		}
		else
		{
			print_counters(frame, group->children.groups[i], indent + 1);
		}
	}
}

MALI_EXPORT
void _instrumented_print_counters(mali_instrumented_frame *frame)
{
	MALI_DEBUG_ASSERT_POINTER(frame);
	_mali_sys_printf("\nCounters from frame %d:\n", frame->frame_no);
	if (_instrumented_frame_get_realloced(frame))
	{
		_mali_sys_printf("\n!! NOTE! Profiling buffer was reallocated while profiling this frame!\n"
			"!! One or more timers may be include time to reallocate the buffer.\n\n");
	}

	print_counters(frame, frame->instrumented_ctx->header_root, 0);
}

MALI_EXPORT void _instrumented_register_counter_egl_begin(mali_instrumented_context* ctx)
{
	ctx->egl_start_index = ctx->num_counters;
}

MALI_EXPORT void _instrumented_register_counter_egl_end(mali_instrumented_context* ctx)
{
	ctx->egl_num_counters = ctx->num_counters - ctx->egl_start_index;
}

MALI_EXPORT void _instrumented_register_counter_gles_begin(mali_instrumented_context* ctx)
{
	ctx->gles_start_index = ctx->num_counters;
}

MALI_EXPORT void _instrumented_register_counter_gles_end(mali_instrumented_context* ctx)
{
	ctx->gles_num_counters = ctx->num_counters - ctx->gles_start_index;
}

MALI_EXPORT void _instrumented_register_counter_vg_begin(mali_instrumented_context* ctx)
{
	ctx->vg_start_index = ctx->num_counters;
}

MALI_EXPORT void _instrumented_register_counter_vg_end(mali_instrumented_context* ctx)
{
	ctx->vg_num_counters = ctx->num_counters - ctx->vg_start_index;
}

MALI_EXPORT void _instrumented_register_counter_gp_begin(mali_instrumented_context* ctx)
{
	ctx->gp_start_index = ctx->num_counters;
}

MALI_EXPORT void _instrumented_register_counter_gp_end(mali_instrumented_context* ctx)
{
	ctx->gp_num_counters = ctx->num_counters - ctx->gp_start_index;
}

MALI_EXPORT void _instrumented_register_counter_pp_begin(mali_instrumented_context* ctx, u32 core)
{
	ctx->pp_start_index[core] = ctx->num_counters;
}

MALI_EXPORT void _instrumented_register_counter_pp_end(mali_instrumented_context* ctx, u32 core)
{
	ctx->pp_num_counters[core] = ctx->num_counters - ctx->pp_start_index[core];
}
