/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <gles_fb_interface.h>
#include <gles_context.h>
#include <gles_state.h>
#include "gles_viewport.h"


void _gles_depth_range( struct gles_context *ctx, GLftype z_near, GLftype z_far)
{
	struct gles_state *state;
	MALI_DEBUG_ASSERT_POINTER( ctx );

	state = &ctx->state;

	/* clamp input */
	if( z_near > FLOAT_TO_FTYPE(1.0f) ) z_near = FLOAT_TO_FTYPE(1.0f);
	if( z_near < FLOAT_TO_FTYPE(0.0f) ) z_near = FLOAT_TO_FTYPE(0.0f);
	if( z_far > FLOAT_TO_FTYPE(1.0f) ) z_far = FLOAT_TO_FTYPE(1.0f);
	if( z_far < FLOAT_TO_FTYPE(0.0f) ) z_far = FLOAT_TO_FTYPE(0.0f);

	{
		mali_float near_plane = z_near;
		mali_float far_plane = z_far;
		
		state->common.viewport.z_nearfar[0] = near_plane;
		state->common.viewport.z_nearfar[1] = far_plane;
		state->common.viewport.z_minmax[0] = near_plane;
		state->common.viewport.z_minmax[1] = far_plane;

		/* All internal uses of near/far planes require that
		 * "near <= far" so we order it correctly here for all
		 * subsequent uses.
		 */
		if( near_plane > far_plane )
		{
			state->common.viewport.z_minmax[0] = far_plane;
			state->common.viewport.z_minmax[1] = near_plane;
		}

		mali_statebit_set( & state->common, MALI_STATE_VS_DEPTH_RANGE_UPDATE_PENDING );
		_gles_fb_depth_range( ctx, state->common.viewport.z_minmax[0], state->common.viewport.z_minmax[1] );
	}

	/* Update dirty flags
	 *
	 * Note that the PLBU setup of viewport is independent of depth-range.
	 */
	mali_statebit_set( & state->common, MALI_STATE_VS_VIEWPORT_UPDATE_PENDING );
	mali_statebit_set( & state->common, MALI_STATE_PLBU_FRAME_OR_DEPTH_PENDING );

	/* this state will only be consumed by the GLES11 shader generator, ignored for GLES20 */
	mali_statebit_set( & state->common, MALI_STATE_PROJECTION_VIEWPORT_MATRIX_UPDATE_PENDING );
}

