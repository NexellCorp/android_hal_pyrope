/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles1_viewport.h
 * @brief All routines related to viewport-state.
 */

#ifndef _GLES1_VIEWPORT_H_
#define _GLES1_VIEWPORT_H_

#include "../gles_base.h"
#include "../gles_ftype.h"

struct gles_state;
struct gles_context;

/**
 * Initializes the OpenGL ES 1.x viewport with default values
 * @param ctx The pointer to the GLES context
 */
void _gles1_viewport_init( struct gles_context *ctx );

/**
 * Sets the current viewport
 * @param state A pointer to the gles_state to update
 * @param x bottom left corner x coordinate
 * @param y bottom left corner y coordinate
 * @param width width of viewport
 * @param height height of viewport
 * @note This implements the GLES-entrypoint glViewport().
 */
GLenum _gles1_viewport( struct gles_state *state, GLint x, GLint y, GLint width, GLint height) MALI_CHECK_RESULT;

/**
 * Sets the current viewport
 * @param ctx A pointer to the gles_context to update
 * @param x bottom left corner x coordinate
 * @param y bottom left corner y coordinate
 * @param width width of viewport
 * @param height height of viewport
 * @note This implements the GLES-entrypoint glViewport().
 */
GLenum _gles1_viewport_for_egl( struct gles_context *ctx, GLint x, GLint y, GLint width, GLint height ) MALI_CHECK_RESULT;


#endif /* _GLES1_VIEWPORT_H_ */
