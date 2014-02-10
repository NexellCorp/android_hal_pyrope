/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include <base/mali_debug.h>
#include <shared/mali_egl_types.h>
#include <shared/mali_egl_api_globals.h>
#include <util/mali_mem_ref.h>
#include <shared/m200_gp_frame_builder_inlines.h>
#include <util/mali_names.h>

#include "gles_context.h"
#include "gles_state.h"
#include "gles_util.h"
#include "gles_gb_interface.h"
#include "gles_fb_interface.h"
#include "gles_buffer_object.h"
#include "gles_framebuffer_object.h"
#include "gles_texture_object.h"
#include "gles_share_lists.h"
#include "gles_object.h"

#if MALI_USE_GLES_2
    #include "gles_renderbuffer_object.h"
#endif

#include "gles_config_extension.h"

#define MALI_PP_PRODUCT_ID_CORE_TYPE_OFFSET 8

MALI_STATIC egl_gles_global_data * _gles_global_data;

void _gles_internal_unbind_buffer_objects(gles_context *ctx)
{
	GLenum err = GL_NO_ERROR;
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);

	/* un-bind element array buffer */
	err = _gles_bind_buffer( ctx, GL_ELEMENT_ARRAY_BUFFER, 0 );
	MALI_DEBUG_ASSERT(err == GL_NO_ERROR, ("_gles_bind_buffer failed unexpectedly") );

	/* un-bind array buffer */
	err = _gles_bind_buffer( ctx, GL_ARRAY_BUFFER, 0 );
	MALI_DEBUG_ASSERT(err == GL_NO_ERROR, ("_gles_bind_buffer failed unexpectedly") );

	MALI_IGNORE(err);
}

mali_err_code _gles_begin_frame(gles_context *ctx)
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.common.framebuffer.current_object );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.common.framebuffer.current_object->draw_frame_builder );

	_mali_frame_builder_wait_frame( ctx->state.common.framebuffer.current_object->draw_frame_builder );

#if HARDWARE_ISSUE_7571_7572
	ctx->state.common.framebuffer.current_object->framebuffer_has_triangles = MALI_TRUE;
#endif

	/* check if there are any pending rendering errors that have not been reported */
	_gles_check_for_rendering_errors(ctx);

	mali_statebit_set( &ctx->state.common, MALI_STATE_PLBU_FRAME_OR_DEPTH_PENDING );
	mali_statebit_set( &ctx->state.common, MALI_STATE_PLBU_FRAME_DIRTY );
	mali_statebit_set( &ctx->state.common, MALI_STATE_FRAME_MEM_POOL_RESET );

	ctx->frame_pool = NULL;

	_gles_fbo_reset_num_verts_drawn_per_frame(ctx->state.common.framebuffer.current_object);

	return MALI_ERR_NO_ERROR;
}

mali_err_code _gles_reset_frame(gles_context *ctx)
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.common.framebuffer.current_object );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.common.framebuffer.current_object->draw_frame_builder );

	_mali_frame_builder_reset( ctx->state.common.framebuffer.current_object->draw_frame_builder );

#if HARDWARE_ISSUE_7571_7572
	ctx->state.common.framebuffer.current_object->framebuffer_has_triangles = MALI_FALSE;
#endif

	/* check if there are any pending rendering errors that have not been reported */
	_gles_check_for_rendering_errors(ctx);

	mali_statebit_set( &ctx->state.common, MALI_STATE_PLBU_FRAME_OR_DEPTH_PENDING );
	mali_statebit_set( &ctx->state.common, MALI_STATE_PLBU_FRAME_DIRTY );
	mali_statebit_set( &ctx->state.common, MALI_STATE_FRAME_MEM_POOL_RESET );

	ctx->frame_pool = NULL;

	MALI_SUCCESS;
}


mali_err_code _gles_clean_frame(gles_context *ctx)
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.common.framebuffer.current_object );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.common.framebuffer.current_object->draw_frame_builder );

	_mali_frame_builder_clean( ctx->state.common.framebuffer.current_object->draw_frame_builder );

#if HARDWARE_ISSUE_7571_7572
	ctx->state.common.framebuffer.current_object->framebuffer_has_triangles = MALI_FALSE;
#endif

	/* check if there are any pending rendering errors that have not been reported */
	_gles_check_for_rendering_errors(ctx);

	mali_statebit_set( &ctx->state.common, MALI_STATE_PLBU_FRAME_OR_DEPTH_PENDING );
	mali_statebit_set( &ctx->state.common, MALI_STATE_PLBU_FRAME_DIRTY );
	 /* mali_statebit_set( &ctx->state.common, MALI_STATE_FRAME_MEM_POOL_RESET ); */

	ctx->frame_pool = NULL;

	MALI_SUCCESS;
}

void _gles_check_for_rendering_errors(gles_context *ctx)
{
	mali_job_completion_status status;
	GLenum errorcode = GL_NO_ERROR;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->default_framebuffer_object);
	MALI_DEBUG_ASSERT_POINTER(ctx->default_framebuffer_object->draw_frame_builder);

	/* Check whether an error has occurred while rendering to the default (screen) frame builder */
	status = _mali_frame_builder_get_framebuilder_completion_status( ctx->default_framebuffer_object->draw_frame_builder );
	if ( status != MALI_JOB_STATUS_END_SUCCESS ) errorcode = GL_OUT_OF_MEMORY;

	/* Check whether an error has occurred while rendering to one of the FBOs */
	if ( NULL != ctx->share_lists && NULL != ctx->share_lists->framebuffer_object_list )
	{
		gles_wrapper  *name_wrapper;
		u32 iterator = 0;

		name_wrapper = (gles_wrapper *)__mali_named_list_iterate_begin(ctx->share_lists->framebuffer_object_list, &iterator);
		while ( NULL != name_wrapper )
		{
			if (NULL != name_wrapper->ptr.fbo)
			{
				status = _mali_frame_builder_get_framebuilder_completion_status( name_wrapper->ptr.fbo->draw_frame_builder );
				if ( status != MALI_JOB_STATUS_END_SUCCESS ) errorcode = GL_OUT_OF_MEMORY;
			}

			name_wrapper = (gles_wrapper *)__mali_named_list_iterate_next(ctx->share_lists->framebuffer_object_list, &iterator);
		}
	}

	/* only the first error should be reported */
	if ( GL_NO_ERROR == ctx->state.common.errorcode )
	{
		ctx->state.common.errorcode = errorcode;
	}
}

mali_err_code _gles_drawcall_begin(gles_context *ctx)
{
	u8 buffer_dirtiness_mask = 0;
	mali_bool depth_test_enabled;
	mali_bool stencil_test_enabled;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.common.framebuffer.current_object );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.common.framebuffer.current_object->draw_frame_builder );

	if( ctx->state.common.framebuffer_control.color_is_writeable[0] ) buffer_dirtiness_mask |= MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_R;	
	if( ctx->state.common.framebuffer_control.color_is_writeable[1] ) buffer_dirtiness_mask |= MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_G;	
	if( ctx->state.common.framebuffer_control.color_is_writeable[2] ) buffer_dirtiness_mask |= MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_B;	
	if( ctx->state.common.framebuffer_control.color_is_writeable[3] ) buffer_dirtiness_mask |= MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_A;	

	depth_test_enabled = _gles_fb_get_depth_test(ctx);
	if(depth_test_enabled && ctx->state.common.framebuffer_control.depth_is_writeable) buffer_dirtiness_mask |= MALI_FRAME_BUILDER_BUFFER_BIT_DEPTH;


	stencil_test_enabled = _gles_fb_get_stencil_test(ctx);
	if(stencil_test_enabled && ctx->state.common.framebuffer_control.stencil_writemask) buffer_dirtiness_mask |= MALI_FRAME_BUILDER_BUFFER_BIT_STENCIL;

	if( _gles_fb_get_multisample(ctx) ) buffer_dirtiness_mask |= MALI_FRAME_BUILDER_BUFFER_BIT_MULTISAMPLE;

	MALI_CHECK_NO_ERROR( _mali_frame_builder_write_lock( ctx->state.common.framebuffer.current_object->draw_frame_builder, buffer_dirtiness_mask ) );

	ctx->frame_pool = _mali_frame_builder_frame_pool_get( ctx->state.common.framebuffer.current_object->draw_frame_builder);

	if ( ctx->frame_pool == NULL )
	{
		_mali_frame_builder_write_unlock( ctx->state.common.framebuffer.current_object->draw_frame_builder );
		MALI_ERROR( MALI_ERR_OUT_OF_MEMORY );
	}

	return MALI_ERR_NO_ERROR;
}

void _gles_drawcall_end(gles_context *ctx)
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.common.framebuffer.current_object );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.common.framebuffer.current_object->draw_frame_builder );

	ctx->frame_pool = NULL;

	_mali_frame_builder_write_unlock( ctx->state.common.framebuffer.current_object->draw_frame_builder );
}

/**
 * Note: this function appears to race _gles_make_current for access to fields of _gles_global_data.
 * Helgrind generates a warning (unless it is suppressed), but this is a false positive.
 * See doc "OpenGL ES driver design", section "Context pointer retrieval".
 */
struct gles_context * _gles_get_context(void)
{
	if (NULL == _gles_global_data)
	{
		/* The program called GL before calling any EGL function */
		return NULL;
	}
	else if (_gles_global_data->multi_context)
	{
		return MALI_STATIC_CAST(gles_context*)_mali_sys_thread_key_get_data(MALI_THREAD_KEY_GLES_CONTEXT);
	}
	else
	{
		return _gles_global_data->current_context;
	}
}

MALI_INTER_MODULE_API mali_err_code _gles_make_current( struct gles_context *ctx )
{
	const gles_context *old_ctx;
	egl_gles_global_data * const global_data = _gles_global_data;

	MALI_DEBUG_ASSERT_POINTER(global_data);
	/* Note: don't use _gles_get_current_context to get the old value. If there was no previous
	 * context, the value may be bogus.
	 */
	old_ctx = MALI_STATIC_CAST(const gles_context *)_mali_sys_thread_key_get_data(MALI_THREAD_KEY_GLES_CONTEXT);
	MALI_CHECK_NO_ERROR(_mali_sys_thread_key_set_data(MALI_THREAD_KEY_GLES_CONTEXT, MALI_STATIC_CAST(void*)ctx));

	_mali_sys_mutex_lock(_gles_global_data->context_mutex);
	if (NULL != old_ctx)
	{
		global_data->num_contexts--;
		if (global_data->num_contexts == 0)
		{
			global_data->multi_context = MALI_FALSE;
			global_data->current_context = NULL;
		}
	}
	if (NULL != ctx)
	{
		global_data->num_contexts++;
		if (global_data->num_contexts > 1)
		{
			global_data->multi_context = MALI_TRUE;
		}
		if (!global_data->multi_context)
		{
			global_data->current_context = ctx;
		}
	}
/* Gles should hit a path where this is done on the flush on unbind of the previous context */	
#if 0
	if( ctx != NULL )
	{
		const u32 mask = 	(1 << MALI_STATE_SCISSOR_DIRTY) |
							(1 << MALI_STATE_VS_VIEWPORT_UPDATE_PENDING) |
							(1 << MALI_STATE_PLBU_VIEWPORT_UPDATE_PENDING) |
							(1 << MALI_STATE_VIEWPORT_SCISSOR_UPDATE_PENDING) |
							(1 << MALI_STATE_PLBU_FRAME_OR_DEPTH_PENDING) |
							(1 << MALI_STATE_VS_DEPTH_RANGE_UPDATE_PENDING) |
							(1 << MALI_STATE_VS_POLYGON_OFFSET_UPDATE_PENDING);

		mali_statebit_raw_set( &ctx->state.common, mask, mask);
	}
#endif
	_mali_sys_mutex_unlock(_gles_global_data->context_mutex);

	MALI_SUCCESS;
}

MALI_INTER_MODULE_API mali_err_code _gles_initialize(egl_gles_global_data *global_data)
{
        _gles_global_data = global_data;

	/* If multiple versions of GLES are present | the structure may already
	 * have been initialized. Use the mutex handle to determine this.
	 */
	if (MALI_NO_HANDLE == global_data->context_mutex)
	{
		global_data->context_mutex = _mali_sys_mutex_create();
		if (MALI_NO_HANDLE == global_data->context_mutex)
		{
			return MALI_ERR_FUNCTION_FAILED;
		}

		global_data->num_contexts = 0;
		global_data->multi_context = MALI_FALSE;
		global_data->current_context = NULL;
	}

	MALI_SUCCESS;
}

MALI_INTER_MODULE_API void _gles_shutdown(egl_gles_global_data *global_data)
{
	if (global_data->context_mutex != MALI_NO_HANDLE)
	{
		_mali_sys_mutex_destroy(global_data->context_mutex);
		global_data->context_mutex = NULL;
	}
}

/**
 * Sets the read frame builder values
 */
MALI_INTER_MODULE_API mali_err_code _gles_set_read_frame_builder( gles_context *ctx, mali_frame_builder *frame_builder, mali_egl_surface_type surface_type )
{
	mali_err_code err = MALI_ERR_NO_ERROR;

	_gles_share_lists_lock( ctx->share_lists );

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	MALI_DEBUG_ASSERT_POINTER( ctx->default_framebuffer_object );

	if( _gles_get_current_read_frame_builder(ctx) == ctx->default_framebuffer_object->read_frame_builder )
	{
		/* tag the framebuilder as dirty, since we might be switching between "clean" direct rendered surfaces */
		_mali_frame_builder_acquire_output( frame_builder );
		/* return with error code if frame_builder output is not set properly*/
		if(frame_builder->output_valid != MALI_TRUE)
		{
			err = MALI_ERR_OUT_OF_MEMORY;
			goto cleanup;
		}
		err = _mali_frame_builder_use( frame_builder );
	}

	ctx->state.common.framebuffer.default_read_surface_type = surface_type;
	ctx->default_framebuffer_object->read_frame_builder = frame_builder;

	if ( surface_type == MALI_EGL_PBUFFER_SURFACE )
	{
		ctx->default_framebuffer_object->read_flip_y = MALI_FALSE;
	} else
	{
		ctx->default_framebuffer_object->read_flip_y = MALI_TRUE;
	}
cleanup:
	_gles_share_lists_unlock( ctx->share_lists );

	return err;
}

/**
 * Sets the draw frame builder values (NOTE: assumes list lock is taken)
 */
MALI_STATIC mali_err_code _gles_set_draw_frame_builder_internal( gles_context *ctx, mali_frame_builder *frame_builder, struct egl_gles_surface_capabilities surface_capabilities)
{
	mali_err_code err;
	struct gles_framebuffer_attachment *color_attachment;
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	MALI_DEBUG_ASSERT_POINTER( ctx->default_framebuffer_object );

	color_attachment = &ctx->default_framebuffer_object->color_attachment;
	MALI_DEBUG_ASSERT_POINTER( color_attachment );
 
	color_attachment->ptr.surface_capabilities = surface_capabilities;

	if ( surface_capabilities.surface_type == MALI_EGL_PBUFFER_SURFACE )
	{
		ctx->default_framebuffer_object->draw_flip_y = MALI_FALSE;
	} else
	{
		ctx->default_framebuffer_object->draw_flip_y = MALI_TRUE;
	}

	/* set the new frame_builder as default */
	ctx->default_framebuffer_object->draw_frame_builder = frame_builder;
	if(surface_capabilities.samples > 4 && surface_capabilities.sample_buffers > 0)
	{
		ctx->default_framebuffer_object->draw_supersample_scalefactor = 2;
	} else 
	{
		ctx->default_framebuffer_object->draw_supersample_scalefactor = 1;
	}


	/* the previous default frame_builder is current, so we must switch the current as well */
	_gles_fb_api_buffer_change(ctx);

	err = _gles_begin_frame(ctx);

	if (MALI_ERR_NO_ERROR != err)
	{
		MALI_DEBUG_ASSERT(err == MALI_ERR_OUT_OF_MEMORY, ("unexpected error code %x", err));
		ctx->state.common.errorcode = GL_OUT_OF_MEMORY;
		MALI_ERROR( err );
	}

	MALI_SUCCESS;
}

/**
 * Sets the draw frame builder values
 */
MALI_INTER_MODULE_API mali_err_code _gles_set_draw_frame_builder( gles_context *ctx, mali_frame_builder *frame_builder, struct egl_gles_surface_capabilities surface_capabilities)
{
	mali_err_code err;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	_gles_share_lists_lock( ctx->share_lists );
	err = _gles_set_draw_frame_builder_internal(ctx, frame_builder, surface_capabilities);
	_gles_share_lists_unlock( ctx->share_lists );

	return err;
}

void _gles_create_renderer_string( char *renderer, int max_len )
{
	u32 renderer_num = _mali_pp_get_core_product_id() >> MALI_PP_PRODUCT_ID_CORE_TYPE_OFFSET;

#ifdef NEXELL_FEATURE_ENABLE
	if (205 == renderer_num) _mali_sys_snprintf(renderer, max_len, "VR");
	else if (206 == renderer_num) _mali_sys_snprintf(renderer, max_len, "VR");
	else if (207 == renderer_num) _mali_sys_snprintf(renderer, max_len, "VR");
	else					 _mali_sys_snprintf(renderer, max_len, "VR-%d", renderer_num);
	renderer[max_len - 1] = '\0'; /* ensure zero-termination, even if the string was capped. */
#else
	/* Mali-400 MP returns 205, 300 return 206 and 450 returns 207 */
	if (205 == renderer_num) _mali_sys_snprintf(renderer, max_len, MALI_NAME_PRODUCT_MALI400);
	else if (206 == renderer_num) _mali_sys_snprintf(renderer, max_len, MALI_NAME_PRODUCT_MALI300);
	else if (207 == renderer_num) _mali_sys_snprintf(renderer, max_len, MALI_NAME_PRODUCT_MALI450);
	else                     _mali_sys_snprintf(renderer, max_len, "Mali-%d", renderer_num);
	renderer[max_len - 1] = '\0'; /* ensure zero-termination, even if the string was capped. */
#endif	
}

#if EXTENSION_ROBUSTNESS_EXT_ENABLE
/* According to the spec, when the GPU generates errors, the context should be reset and application will create a new context to continue operation, */
/* In one side if we want to reset the context,  we cannot know from the driver which context causes GP/PP job errors, so we will reset current context in this function, which requires much work. */
/* In the other side, if the context isn't reset, it won't affect the continue work by the application, so just return no_error */
GLenum _gles_get_graphics_reset_status_ext( gles_context *ctx )
{
	MALI_IGNORE(ctx);
	return GL_NO_ERROR;
}
#endif
