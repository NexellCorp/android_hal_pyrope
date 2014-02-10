/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <shared/m200_gp_frame_builder.h>
#include "gles_framebuffer_state.h"
#include "gles_framebuffer_object.h"

void _gles_framebuffer_state_init( gles_framebuffer_state *state, struct gles_framebuffer_object *default_fbo )
{
	MALI_DEBUG_ASSERT_POINTER( state );
	MALI_DEBUG_ASSERT_POINTER( default_fbo );

	/* there is no attached FBO initially, and addref it since it's being bound */
	_gles_framebuffer_object_addref(default_fbo);
	state->current_object = default_fbo;
	state->current_object_id = 0;

	state->default_read_surface_type = MALI_EGL_WINDOW_SURFACE;
}


