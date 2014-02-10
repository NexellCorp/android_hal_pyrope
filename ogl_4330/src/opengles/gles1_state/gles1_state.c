/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles1_state.h"

#include <mali_system.h>
#include <gles_state.h>
#include <gles_util.h>
#include <gles_context.h>
#include <gles_common_state/gles_framebuffer_control.h>
#include "gles1_pixel.h"
#include "gles1_viewport.h"

void _gles1_state_init( struct gles_context *ctx )
{
	gles_common_state *state_common;
	gles1_state       *state_gles1;
	gles_state        *state;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( &ctx->state );
	MALI_DEBUG_ASSERT_POINTER( ctx->state.api.gles1 );

	state        = &ctx->state;
	state_common = &(state->common);
	state_gles1  = state->api.gles1;

	mali_state_init( &state->common );

	/* initialize sub-states */
	_gles1_current_init( &state_gles1->current );
	_gles1_vertex_array_init( ctx );
	_gles1_viewport_init( ctx );
	_gles1_transform_init( &state_gles1->transform );
	_gles1_coloring_init( &state_gles1->coloring );
	_gles1_lighting_init( state );
	_gles1_rasterization_init( ctx );
	_gles_framebuffer_control_init( ctx );
	_gles_pixel_operations_init_scissor( ctx );
	_gles1_pixel_init( &state_common->pixel );
	_gles1_hint_init( &state_gles1->hint );
	_gles1_texture_env_init( ctx, ctx->default_texture_object );
	_gles_framebuffer_state_init( &state->common.framebuffer, ctx->default_framebuffer_object );

	state->common.current_program_rendering_state = NULL;

	state->common.errorcode = GL_NO_ERROR;
	/* enable color dirty bits to 1, since they have been assigned in initialization*/
	mali_statebit_set( &ctx->state.common, GLES_STATE_CURRENT_COLOR_DIRTY);
	mali_statebit_set( &ctx->state.common, GLES_STATE_FOG_COLOR_DIRTY);

}

void _gles1_state_deinit( struct gles_context *ctx )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	_gles_vertex_array_deinit( &ctx->state.common.vertex_array );
}
