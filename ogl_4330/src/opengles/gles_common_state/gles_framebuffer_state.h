/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_framebuffer_state.h
 * @brief gles state structures
 */

#ifndef GLES_FRAMEBUFFER_STATE_H
#define GLES_FRAMEBUFFER_STATE_H

#include "gles_base.h"

/* Include the config headers for the depth and stencil maxvalues */
#include "gles_config.h"

#include <shared/mali_egl_types.h>

struct gles_framebuffer_object;

typedef struct gles_framebuffer_state
{
	struct gles_framebuffer_object *current_object;
	GLuint current_object_id;

	mali_egl_surface_type default_read_surface_type;

} gles_framebuffer_state;

/**
 * Initializes the framebuffer-state
 * @param state The framebuffer-state that is to be initialized
 * @param default_object The default FBO
 * @note This function initializes the framebuffer-object-state.
 */
void _gles_framebuffer_state_init( struct gles_framebuffer_state *state, struct gles_framebuffer_object *default_object);

#endif /* GLES_FRAMEBUFFER_STATE_H */
