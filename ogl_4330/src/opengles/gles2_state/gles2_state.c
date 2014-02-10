/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles2_state.h"

#include "gles_context.h"
#include <base/mali_debug.h>
#include "../gles_state.h"
#include "../gles_base.h"
#include "../gles_util.h"
#include <gles_common_state/gles_framebuffer_control.h>
#include "gles2_pixel.h"
#include "gles2_vertex_array.h"
#include "gles2_tex_state.h"
#include "gles2_viewport.h"
#include "gles2_rasterization.h"

void _gles2_state_init( struct gles_context *ctx )
{

	gles_state *state;
	MALI_DEBUG_ASSERT_POINTER( ctx );
	state = &ctx->state;
	MALI_DEBUG_ASSERT_POINTER( state->api.gles2 );

	mali_state_init( &state->common );

	_gles2_texture_env_init(&state->common.texture_env, ctx->default_texture_object);
	_gles_framebuffer_state_init( &state->common.framebuffer, ctx->default_framebuffer_object );

	/* Initialize all the substates */
	state->common.errorcode = GL_NO_ERROR;
	_gles2_vertex_array_init( ctx );
	_gles2_viewport_init( ctx );
	_gles2_rasterization_init(&state->common.rasterization);
	_gles_framebuffer_control_init( ctx );
	_gles2_vertex_shader_init(&state->api.gles2->vertex_shader);
	_gles2_hint_init(&state->api.gles2->hint);
	_gles_pixel_operations_init_scissor( ctx );
	_gles2_program_env_init(&state->api.gles2->program_env);
	_gles2_pixel_init(&state->common.pixel);

	_gles_renderbuffer_state_init( &state->common.renderbuffer );

	/* Setting a default value which is not true, but will work as expected */
	state->api.gles2->previous_draw_mode = GL_POINTS;

	state->common.current_program_rendering_state = NULL;
}

void _gles2_state_deinit( struct gles_context *ctx )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	_gles_vertex_array_deinit(&ctx->state.common.vertex_array);
}
