/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles2_viewport.h
 * @brief All routines related to viewport-state.
 */

#ifndef _GLES2_VIEWPORT_H_
#define _GLES2_VIEWPORT_H_

#include <GLES2/mali_gl2.h>
#include "../gles_ftype.h"
#include "../gles_config.h"
#include "../gles_util.h"

struct gles_state;
struct gles_context;

/**
 * Initializes the OpenGL ES 2.x viewport with default values
 * @param ctx The pointer to the GLES context
 */
void _gles2_viewport_init( struct gles_context *context );

/**
 * Sets the current viewport
 * @param state A pointer to the gles_state
 * @param x bottom left corner x coordinate
 * @param y bottom left corner y coordinate
 * @param width width of viewport
 * @param height height of viewport
 * @note This is a wrapper around the GLES-entrypoint glViewport().
 */
GLenum _gles2_viewport( struct gles_state *state, GLint x, GLint y, GLint width, GLint height ) MALI_CHECK_RESULT;

/**
 * Sets the current viewport
 * @param ctx A pointer to the gles_context
 * @param x bottom left corner x coordinate
 * @param y bottom left corner y coordinate
 * @param width width of viewport
 * @param height height of viewport
 * @note This is a wrapper around the GLES-entrypoint glViewport().
 */
GLenum _gles2_viewport_for_egl( struct gles_context *ctx, GLint x, GLint y, GLint width, GLint height ) MALI_CHECK_RESULT;


#endif /* _GLES2_VIEWPORT_H_ */

