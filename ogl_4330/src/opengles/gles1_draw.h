/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles1_draw.h
 * @brief OpenGL ES 1.x Functions to draw primitives
 */

#ifndef _GLES1_DRAW_H_
#define _GLES1_DRAW_H_

#include "gles_context.h"
#include "gles_gb_interface.h"

/**
 * Draw indexed geometry
 * @param ctx The pointer to the GLES context
 * @param mode Which primitive is to be drawn(GL_TRIANGLES, GL_POINTS, etc)
 * @param count The number of indices
 * @param type The data-type of the indices(GL_FLOAT; GL_INT, etc)
 * @param indices The indices for the vertices to be used for drawing
 * @note This is a wrapper around the GLES-entrypoint glDrawElements().
 */
GLenum MALI_CHECK_RESULT _gles1_draw_elements( gles_context *ctx, GLenum mode, GLint count, GLenum type, const void *indices );

/**
 * Draw non-indexed geometry
 * @param ctx The pointer to the GLES context
 * @param mode Which primitive is to be drawn
 * @param first The index of the first vertex to be used
 * @param count The number of vertices to be used
 * @note This is a wrapper around the GLES-entrypoint glDrawArray().
 */
GLenum MALI_CHECK_RESULT _gles1_draw_arrays( gles_context *ctx, GLenum mode, GLint first, GLint count );

/**
 * @brief Draw a texturized rectangle specified in screen-coordinates
 * @param ctx The pointer to the GLES context
 * @param x The x-coordinate of the upper-left corner
 * @param y The y-coordinate of the upper-left corner
 * @param z The depth-value, specified in framebuffer-coordinates(0 to 1, other values are accepted, but not recognised).
 * @param width The width of the rectangle
 * @param height The height of the rectangle
 * @return GL_NO_ERROR if successful, GL_INVALID_VALUE if width or height less than or equal to 0.
 */
GLenum MALI_CHECK_RESULT _gles1_draw_tex_oes( struct gles_context *ctx, GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height );

#endif /* _GLES1_DRAW_H_ */

