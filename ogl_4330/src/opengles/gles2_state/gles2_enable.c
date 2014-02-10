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
#include <gles_fb_interface.h>

#include "gles2_enable.h"

GLenum _gles2_enable( struct gles_context *ctx, GLenum cap, GLboolean enable )
{
	gles_common_state  *state_common;
	MALI_DEBUG_ASSERT_POINTER( ctx );
	state_common = &(ctx->state.common);

	/* boolean inputs are either false or true - in case some weird version of true, adjust to true */
	if(enable != GL_TRUE && enable != GL_FALSE) enable = GL_TRUE;

	switch (cap)
	{
		case GL_BLEND:
			_gles_fb_set_blend( ctx, enable );
		break;

		case GL_DEPTH_TEST:
			_gles_fb_set_depth_test( ctx, enable );
		break;

		case GL_DITHER:
			_gles_fb_set_dither( ctx, enable );
		break;

		case GL_SAMPLE_ALPHA_TO_COVERAGE:
			_gles_fb_set_sample_alpha_to_coverage( ctx, enable );
		break;

		case GL_SAMPLE_COVERAGE:
			_gles_fb_set_sample_coverage( ctx, enable );
		break;

		case GL_STENCIL_TEST:
			_gles_fb_set_stencil_test( ctx, enable );
		break;

		case GL_CULL_FACE:
			state_common->rasterization.cull_face = enable;
		break;

		case GL_POLYGON_OFFSET_FILL:
			_gles_fb_set_polygon_offset_fill( ctx, enable );
			mali_statebit_set( state_common, MALI_STATE_VS_POLYGON_OFFSET_UPDATE_PENDING );
		break;

		case GL_SCISSOR_TEST:
			(void) mali_statebit_exchange( state_common, GLES_STATE_SCISSOR_ENABLED, enable );
			mali_statebit_set( state_common, MALI_STATE_SCISSOR_DIRTY );
		break;

		default:
			return GL_INVALID_ENUM;
	}
	return GL_NO_ERROR;
}

