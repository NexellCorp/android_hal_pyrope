/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <gles_context.h>
#include <gles_state.h>

#include "gles2_viewport.h"

void _gles2_viewport_init( gles_context *ctx )
{
	GLenum err;
	struct gles_state *state;
	MALI_DEBUG_ASSERT_POINTER( ctx );

	state = &ctx->state;

	/* reset viewport and depth range states by calling its gles-equivalent controller function */
	err = _gles2_viewport( state, 0, 0, 0, 0 );
	/* This is not expected to fail for the default values */
	MALI_DEBUG_ASSERT(err == GL_NO_ERROR, ("_gles2_viewport failed unexpectedly") );
	MALI_IGNORE(err);

	_gles_depth_range( ctx, FLOAT_TO_FTYPE(0.f), FLOAT_TO_FTYPE(1.f) );
}

MALI_INTER_MODULE_API GLenum _gles2_viewport_for_egl( struct gles_context *ctx, GLint x, GLint y, GLint width, GLint height )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	return _gles2_viewport(&ctx->state, x, y, width, height);
}

MALI_INTER_MODULE_API void _gles2_update_viewport_state_for_egl ( struct gles_context *ctx )
{
	mali_statebit_set( & ctx->state.common, MALI_STATE_VS_VIEWPORT_UPDATE_PENDING );
}

GLenum _gles2_viewport(gles_state *state, GLint x, GLint y, GLint width, GLint height)
{
	MALI_DEBUG_ASSERT_POINTER( state );

	/* handle errors */
	if (width < 0 || height < 0) return GL_INVALID_VALUE;

	/* clamp width if necessary */
	if ( width > GLES2_MAX_VIEWPORT_DIM_X )
	{
		width = GLES2_MAX_VIEWPORT_DIM_X;
	}

	/* clamp height if necessary */
	if ( height > GLES2_MAX_VIEWPORT_DIM_Y )
	{
		height = GLES2_MAX_VIEWPORT_DIM_Y;
	}

	/* early out scenario */
	if(x == state->common.viewport.x && y == state->common.viewport.y && width == state->common.viewport.width && height == state->common.viewport.height) return GL_NO_ERROR;

	/* update state */
	state->common.viewport.x      = x;
	state->common.viewport.y      = y;
	state->common.viewport.width  = width;
	state->common.viewport.height = height;

	mali_statebit_set( & state->common, MALI_STATE_VS_VIEWPORT_UPDATE_PENDING );
	mali_statebit_set( & state->common, MALI_STATE_PLBU_VIEWPORT_UPDATE_PENDING );
	return GL_NO_ERROR;
}

