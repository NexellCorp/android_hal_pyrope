/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_context.h"

#include <mali_system.h>

#include <base/mali_debug.h>

#include "gles_state.h"
#include "gles_flush.h"
#include "gles_convert.h"
#include "gles_framebuffer_object.h"
#include "gles_share_lists.h"
#include <shared/m200_gp_frame_builder_struct.h>
#include <shared/m200_gp_frame_builder_inlines.h>

MALI_STATIC GLenum _gles_flush_internal( gles_context *ctx, mali_bool flush_all )
{
	mali_err_code        error;
	mali_frame_builder  *frame_builder = NULL;

	MALI_DEBUG_ASSERT_POINTER(ctx);

	frame_builder = _gles_get_current_draw_frame_builder( ctx );
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	/* cannot early-out if this is fence sync flush */
	if ( !frame_builder->inc_render_on_flush )
	{
		if ( 0 == ctx->state.common.framebuffer.current_object_id &&
			 MALI_EGL_WINDOW_SURFACE == ctx->state.common.framebuffer.default_read_surface_type)
		{
			/* There is no need to flush window surfaces. The framebuffer will only be visible
			 * following a glReadPixels, glCopyTex[Sub]Image, or eglSwapBuffers and these functions
			 * will all cause the framebuffer to be flushed.
			 */
			GL_SUCCESS;
		}
	}

	error = _mali_frame_builder_flush( frame_builder );


	if( error != MALI_ERR_NO_ERROR )
	{
		mali_err_code        err;
		err =  _gles_reset_frame( ctx );
		if( err != MALI_ERR_NO_ERROR ) MALI_ERROR( _gles_convert_mali_err( err ) );

		MALI_ERROR( _gles_convert_mali_err( error ) );
	}
	
	/* the flush_all flag is a temp workaround that also adds a 
	 * flush command to the main framebuilder. If set, we should 
	 * flush both the current and the FBO0 framebuilder. 
	 * The current FBO is flushed above, leaving the primary one. */
	if(flush_all)
	{
		frame_builder = ctx->default_framebuffer_object->draw_frame_builder;
		MALI_DEBUG_ASSERT_POINTER( frame_builder );

		error = _mali_frame_builder_flush( frame_builder );
		if( error != MALI_ERR_NO_ERROR )
		{
			mali_err_code        err;
			err =  _gles_reset_frame( ctx );
			if( err != MALI_ERR_NO_ERROR ) MALI_ERROR( _gles_convert_mali_err( err ) );

			MALI_ERROR( _gles_convert_mali_err( error ) );
		}
	
	}

	GL_SUCCESS;
}

MALI_INTER_MODULE_API GLenum _gles_flush( gles_context *ctx, mali_bool flush_all )
{
	GLenum ret;
	_gles_share_lists_lock( ctx->share_lists );
	ret = _gles_flush_internal(ctx, flush_all);
	_gles_share_lists_unlock( ctx->share_lists );
	return ret;
}

MALI_STATIC GLenum _gles_finish_internal( gles_context *ctx )
{
	mali_err_code       error;
	mali_frame_builder *frame_builder = NULL;

	MALI_DEBUG_ASSERT_POINTER(ctx);

	frame_builder = _gles_get_current_draw_frame_builder( ctx );
	if (NULL == frame_builder) GL_SUCCESS; /* Nothing to do */

	error = _mali_frame_builder_flush( frame_builder );
	
	if( error != MALI_ERR_NO_ERROR )
	{
		mali_err_code        err;
		err =  _gles_reset_frame( ctx );
		if( err != MALI_ERR_NO_ERROR ) MALI_ERROR( _gles_convert_mali_err( err ) );

		MALI_ERROR( _gles_convert_mali_err( error ) );
	}

	_mali_frame_builder_wait( frame_builder );
	_gles_check_for_rendering_errors( ctx );

	GL_SUCCESS;
}

MALI_INTER_MODULE_API GLenum _gles_finish( gles_context *ctx )
{
	GLenum ret;
	_gles_share_lists_lock( ctx->share_lists );
	ret = _gles_finish_internal(ctx);
	_gles_share_lists_unlock( ctx->share_lists );
	return ret;
}
