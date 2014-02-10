/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_instrumented_context.h"
#include "mali_instrumented_frame.h"
#include "mali_instrumented_context_types.h"
#include "mali_counters.h"
#include "mali_counters_dump.h"
#include "mali_frame_dump.h"
#include "mali_log.h"
#include "mri_interface.h"
#include "mali_pp_instrumented.h"
#include "mali_gp_instrumented.h"
#include "mali_instrumented_plugin.h"

MALI_EXPORT
mali_err_code _instrumented_begin_new_frame(mali_instrumented_frame **old_frame)
{
	mali_instrumented_frame *frame;
	mali_instrumented_context *ctx = _instrumented_get_context();

	if (NULL == ctx)
	{
		return MALI_ERR_NO_ERROR; /* no instrumentation, so nothing to do! */
	}

	MALI_DEBUG_ASSERT(ctx->inited, ("Cannot start a frame with an uninitialized context" ));
	MALI_CHECK(ctx->inited, MALI_ERR_FUNCTION_FAILED);

	frame = _mali_sys_malloc( sizeof(mali_instrumented_frame) );
	MALI_CHECK(NULL != frame, MALI_ERR_OUT_OF_MEMORY);

	frame->counter_values = _mali_sys_calloc(ctx->num_counters, sizeof(s64));
	if (NULL == frame->counter_values)
	{
		_mali_sys_free(frame);
		return MALI_ERR_OUT_OF_MEMORY;
	}

	_mali_sys_atomic_set(&frame->ref_count, 1); /* This ref must be released by caller (EGL) */
	frame->instrumented_ctx = ctx;
	frame->profiling_buffer_realloced = MALI_FALSE;
#if !defined(USING_MALI200)
	frame->l2_perf_counters_count = 0;
#endif

	_mali_sys_lock_lock(ctx->lock);

	if (NULL != old_frame)
	{
		/* return old frame */
		*old_frame = ctx->current_frame;
	}

	if (NULL != ctx->current_frame)
	{
		mali_err_code err;

		/* Move current active frame to working frames */
		__mali_linked_list_lock(&ctx->working_frames);
		err = __mali_linked_list_insert_data(&ctx->working_frames, ctx->current_frame);
		if (MALI_ERR_NO_ERROR != err)
		{
			__mali_linked_list_unlock(&ctx->working_frames);
			_mali_sys_lock_unlock(ctx->lock);
			_mali_sys_free(frame->counter_values);
			_mali_sys_free(frame);
			return err;
		}
		__mali_linked_list_unlock(&ctx->working_frames);
	}

	frame->frame_no = ctx->current_frame_no;
	ctx->current_frame_no += 1; /* It's ok if this one wraps from MAX_INT to 0. */
	ctx->current_frame = frame;

	_mali_sys_lock_unlock(ctx->lock);

	MALI_SUCCESS;
}

MALI_EXPORT
void _instrumented_frame_completed(mali_instrumented_frame *frame, mali_surface *draw_surface)
{
	char framebuffer_dump_filename[512];
	mali_instrumented_context *ctx = _instrumented_get_context();

	MALI_DEBUG_ASSERT_POINTER(ctx);

	if (ctx->options_mask & INSTRUMENT_DUMP_FRAMEBUFFER)
	{
#if defined(__SYMBIAN32__)
#ifdef BUILD_TECHVIEW
		_mali_sys_snprintf(framebuffer_dump_filename, 512, "e:\\dump%04u.ppm", frame->frame_no + 1);
#else
		_mali_sys_snprintf(framebuffer_dump_filename, 512, "d:\\dump%04u.ppm", frame->frame_no + 1);
#endif
#else
		const char* dump_path_env = _mali_sys_config_string_get("MALI_OUTPUT_FOLDER");
		if (NULL != dump_path_env)
		{
			_mali_sys_snprintf(framebuffer_dump_filename, 512, "%s/dump%04u.ppm", dump_path_env, frame->frame_no + 1);
			_mali_sys_config_string_release(dump_path_env);
		}
		else
		{
			_mali_sys_snprintf(framebuffer_dump_filename, 512, "dump%04u.ppm", frame->frame_no + 1);
		}
#endif
		_instrumented_write_frame_dump(framebuffer_dump_filename, draw_surface );
	}
	
	if (ctx->options_mask & INSTRUMENT_DUMP_FRAMEBUFFER_HEX)
	{
#if defined(__SYMBIAN32__)
#ifdef BUILD_TECHVEIW
		_mali_sys_snprintf(framebuffer_dump_filename, 512, "e:\\dump%04u", frame->frame_no + 1);
#else
		_mali_sys_snprintf(framebuffer_dump_filename, 512, "d:\\dump%04u", frame->frame_no + 1);
#endif
#else
		const char* dump_path_env = _mali_sys_config_string_get("MALI_OUTPUT_FOLDER");
		if (NULL != dump_path_env)
		{
			_mali_sys_snprintf(framebuffer_dump_filename, 512, "%s/dump%04u", dump_path_env, frame->frame_no + 1);
			_mali_sys_config_string_release(dump_path_env);
		}
		else
		{
			_mali_sys_snprintf(framebuffer_dump_filename, 512, "dump%04u", frame->frame_no + 1);
		}
#endif
		_instrumented_write_frame_dump_hex(framebuffer_dump_filename, draw_surface );
	}
}

MALI_EXPORT
void _instrumented_finish_frame(mali_instrumented_frame *frame)
{
	mali_instrumented_context *ctx;

	MALI_DEBUG_ASSERT_POINTER(frame);

	ctx = frame->instrumented_ctx;
	MALI_DEBUG_ASSERT_POINTER(ctx);

	MALI_DEBUG_ASSERT(ctx->inited, ("Cannot finish_frame in an uninitialized context\n" ));
	if (!ctx->inited)
	{
		return;
	}

	if (ctx->sampling)
	{
		_mali_profiling_analyze(frame);
		_mali_gp_derive_counts(frame->instrumented_ctx, frame);
		_mali_pp_derive_counts(frame->instrumented_ctx, frame);

#if MALI_INSTRUMENTED_PLUGIN_ENABLED
		/* Report SW counters for this frame to the plugin */
		if (MALI_INSTRUMENTED_FEATURE_IS_ENABLED(CINSTR_CLIENTAPI_COMMON, CINSTR_FEATURE_COUNTERS_SW_ALL))
		{
			_mali_instrumented_plugin_send_event_counters(VR_CINSTR_COUNTER_SOURCE_EGL, ctx->egl_num_counters, ctx->counters + ctx->egl_start_index, frame->counter_values + ctx->egl_start_index);
#if defined(EGL_MALI_GLES)
			_mali_instrumented_plugin_send_event_counters(VR_CINSTR_COUNTER_SOURCE_OPENGLES, ctx->gles_num_counters, ctx->counters + ctx->gles_start_index, frame->counter_values + ctx->gles_start_index);
#endif
#if defined(EGL_MALI_VG)
			_mali_instrumented_plugin_send_event_counters(VR_CINSTR_COUNTER_SOURCE_OPENVG, ctx->vg_num_counters, ctx->counters + ctx->vg_start_index, frame->counter_values + ctx->vg_start_index);
#endif
		}

		/* Report HW counters for this frame to the plugin */
		if (MALI_INSTRUMENTED_FEATURE_IS_ENABLED(CINSTR_CLIENTAPI_COMMON, CINSTR_FEATURE_COUNTERS_HW_CORE))
		{
			int i;

			_mali_instrumented_plugin_send_event_counters(VR_CINSTR_COUNTER_SOURCE_GP, ctx->gp_num_counters, ctx->counters + ctx->gp_start_index, frame->counter_values + ctx->gp_start_index);
			for (i = 0; i < MALI_MAX_PP_SPLIT_COUNT; i++)
			{
				/* The counter IDs are intentionally reported as they where from PP0, since the plug-in doesn't have different PP counters for the different PP cores */
				_mali_instrumented_plugin_send_event_counters(VR_CINSTR_COUNTER_SOURCE_PP, ctx->pp_num_counters[i], ctx->counters + ctx->pp_start_index[0], frame->counter_values + ctx->pp_start_index[i]);
			}
		}
#endif

		if (NULL != frame->counter_values && (ctx->options_mask & INSTRUMENT_PRINT_COUNTERS))
		{
			_instrumented_print_counters(frame);
		}

		if (NULL != frame->counter_values && (ctx->options_mask & INSTRUMENT_DUMP_COUNTERS)) /* check whether we're dumping to file */
		{
			_instrumented_write_frame(ctx, frame);
		}
	}

#if MALI_INSTRUMENTED_PLUGIN_ENABLED
	if (MALI_INSTRUMENTED_FEATURE_IS_ENABLED(CINSTR_CLIENTAPI_COMMON, CINSTR_FEATURE_FRAME_COMPLETE))
	{
		/* For now, we report all render passes as a single render pass */
		_mali_instrumented_plugin_send_event_render_pass_complete(CINSTR_EVENT_RENDER_PASS_COMPLETE, 0, frame->frame_no);
		_mali_instrumented_plugin_send_event_frame_complete(CINSTR_EVENT_FRAME_COMPLETE, frame->frame_no);
	}
#endif

	if (ctx->options_mask & INSTRUMENT_WAIT_FOR_MRI && ctx->mrp_context != NULL)
	{
		MALI_DEBUG_ASSERT_POINTER(ctx->mrp_context);
		_loop_receive_MRI(frame);
	}

	_destroy_instrumented_frame(frame);  /* also frees counter_values and unmaps fb_handle */
}

MALI_EXPORT
void _instrumented_addref_frame(mali_instrumented_frame *frame)
{
	_mali_sys_atomic_inc(&frame->ref_count);
}

MALI_EXPORT
void _instrumented_release_frame(mali_instrumented_frame *frame)
{
	u32 refs = _mali_sys_atomic_dec_and_return(&frame->ref_count);

	if (0 == refs)
	{
		/*
		 * We can now finish all completed frame (frames which no longer has a reference).
		 * Make sure we complete frames in the correct order (the order they where added to the working_frames list.
		 */
		_instrumented_context_finish_frames(frame->instrumented_ctx);
	}
}


MALI_EXPORT
void _instrumented_set_active_frame( mali_instrumented_frame *frame )
{
	mali_err_code err;
    err = _mali_sys_thread_key_set_data(MALI_THREAD_KEY_INSTRUMENTED_ACTIVE_FRAME, (void*)frame);
	MALI_IGNORE(err);
}

MALI_EXPORT
mali_instrumented_frame *_instrumented_get_active_frame(void)
{
        return (mali_instrumented_frame *)_mali_sys_thread_key_get_data(MALI_THREAD_KEY_INSTRUMENTED_ACTIVE_FRAME);
}

MALI_EXPORT
void _instrumented_frame_set_realloced(mali_instrumented_frame *frame)
{
	MALI_DEBUG_ASSERT_POINTER(frame);
	MALI_INSTRUMENTED_LOG((frame->instrumented_ctx, MALI_LOG_LEVEL_WARNING,
			  "The buffer used for software profiling has been reallocated. "
			  "This may affect the values from one or more timing counters."));
	frame->profiling_buffer_realloced = MALI_TRUE;
}

MALI_EXPORT
mali_bool _instrumented_frame_get_realloced(mali_instrumented_frame *frame)
{
	MALI_DEBUG_ASSERT_POINTER(frame);
	return frame->profiling_buffer_realloced;
}
