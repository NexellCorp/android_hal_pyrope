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
 * @file gles2_draw.h
 * @brief OpenGL ES 2.0 Functions to draw primitives
 */

#ifndef _GLES2_DRAW_H_
#define _GLES2_DRAW_H_

#include "gles_context.h"

/**
 * Draw indexed geometry
 * @param ctx The pointer to the GLES context
 * @param mode Which primitive is to be drawn(GL_TRIANGLES, GL_POINTS, etc)
 * @param count The number of indices
 * @param type The data-type of the indices(GL_FLOAT; GL_INT, etc)
 * @param indices The indices for the vertices to be used for drawing
 * @note This is a wrapper around the GLES-entrypoint glDrawElements().
 */
GLenum MALI_CHECK_RESULT _gles2_draw_elements( gles_context *ctx, GLenum mode, GLint count, GLenum type, const void *indices );

/**
 * Draw non-indexed geometry
 * @param ctx The pointer to the GLES context
 * @param mode Which primitive is to be drawn
 * @param first The index of the first vertex to be used
 * @param count The number of vertices to be used
 * @note This is a wrapper around the GLES-entrypoint glDrawArray().
 */
GLenum MALI_CHECK_RESULT _gles2_draw_arrays( gles_context *ctx, GLenum mode, GLint first, GLint count );

#endif /* _GLES2_DRAW_H_ */

