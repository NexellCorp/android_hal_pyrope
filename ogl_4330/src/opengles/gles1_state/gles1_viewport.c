/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <gles_state.h>
#include <gles_fb_interface.h>
#include <gles_context.h>
#include <gles_common_state/gles_viewport.h>

#ifdef NEXELL_LEAPFROG_FEATURE_VIEWPORT_SCALE_EN /* nexell for leapfrog */
static gles_context *__gCtxForViewport;
#endif

#include "gles1_viewport.h"
void _gles1_viewport_init( struct gles_context *ctx )
{
	struct gles_state *state;
	GLenum err;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	
	state = &ctx->state;

	/* these states do not depend on anything else, and can be initialized directly by calling the internal functions */
	err = _gles1_viewport( state, 0, 0, 0, 0);
	/* This is not expected to fail for the default values */
	MALI_DEBUG_ASSERT(err == GL_NO_ERROR, ("_gles1_viewport failed unexpectedly") );
	MALI_IGNORE(err);

	_gles_depth_range( ctx, FLOAT_TO_FTYPE(0.f), FLOAT_TO_FTYPE(1.f));

#ifdef NEXELL_LEAPFROG_FEATURE_VIEWPORT_SCALE_EN /* nexell for leapfrog */
	__gCtxForViewport = ctx;
#endif	
}

MALI_INTER_MODULE_API GLenum _gles1_viewport_for_egl( struct gles_context *ctx, GLint x, GLint y, GLint width, GLint height )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	return _gles1_viewport(&ctx->state, x, y, width, height);
}

MALI_INTER_MODULE_API void _gles1_update_viewport_state_for_egl ( struct gles_context *ctx )
{
	mali_statebit_set( & ctx->state.common, MALI_STATE_VS_VIEWPORT_UPDATE_PENDING );
}

GLenum _gles1_viewport( struct gles_state *state, GLint x, GLint y, GLint width, GLint height )
{
	MALI_DEBUG_ASSERT_POINTER( state );

	/* handle errors */
	if (width < 0 || height < 0) MALI_ERROR( GL_INVALID_VALUE );
	
#ifdef NEXELL_LEAPFROG_FEATURE_VIEWPORT_SCALE_EN /* nexell for leapfrog */
	if(__gCtxForViewport && width && height && __gCtxForViewport->scale_surf_width && __gCtxForViewport->scale_surf_height)
	{
		if(width != __gCtxForViewport->scale_surf_width || height != __gCtxForViewport->scale_surf_height)
		{
			width = __gCtxForViewport->scale_surf_width;
			height = __gCtxForViewport->scale_surf_height;
		}
	}
#endif

	/* early out scenario */
	if(x == state->common.viewport.x && y == state->common.viewport.y && width == state->common.viewport.width && height == state->common.viewport.height) return GL_NO_ERROR;


	/* update state */
	state->common.viewport.x      = x;
	state->common.viewport.y      = y;
	state->common.viewport.width  = width;
	state->common.viewport.height = height;

	/* update dirty flags */
	mali_statebit_set( & state->common, MALI_STATE_VS_VIEWPORT_UPDATE_PENDING );
	mali_statebit_set( & state->common, MALI_STATE_PLBU_VIEWPORT_UPDATE_PENDING );
	mali_statebit_set( & state->common, MALI_STATE_PROJECTION_VIEWPORT_MATRIX_UPDATE_PENDING );

	GL_SUCCESS;
}

