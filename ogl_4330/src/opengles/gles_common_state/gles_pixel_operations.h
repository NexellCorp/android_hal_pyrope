/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_pixel_operations.h
 * @brief Pixel operation functionality that is common to both the OpenGL ES 1.1 and 2.0 APIs.
 */

#ifndef _GLES_PIXEL_OPERATIONS_H_
#define _GLES_PIXEL_OPERATIONS_H_

#include <mali_system.h>
#include "gles_base.h"

struct gles_context;

/**
 * Specifies a scissoring box for the framebuffer
 */
typedef struct gles_scissor
{
	GLint	x, y;             /**< The offset coordinates for the scissor, in screen-coordinates */
	GLuint	width, height;    /**< The size of the box (0, 0) --> (1, 1), handled in GB */
	u32		last_state;		  /**< The control-flow state when PLBU scissor was last recalculated */

} gles_scissor;

/**
 * @brief Set the stencil-test parameters.
 * @param ctx The pointer to the GLES context
 * @param face Specifies which set of state is affected. If face is GL_FRONT_AND_BACK then the
 *             front and back state are set identical
 * @param func The function to be used for the stencil-testing
 * @param ref The reference value used for the stencil-testing
 * @param mask The mask to be applied when writing to the stencil-buffer
 * @return An errorcode.
 * @note This is a wrapper function for the GLES entrypoint glStencilFunc() and glStencilFuncSeparate().
 */
MALI_IMPORT GLenum _gles_stencil_func( struct gles_context *ctx, GLenum face, GLenum func, GLint ref, GLuint mask) MALI_CHECK_RESULT;

/**
 * @brief Initializes the framebuffer's scissoring box.
 * @param ctx The pointer to the GLES context
 */
MALI_IMPORT void _gles_pixel_operations_init_scissor( struct gles_context * const ctx );

/**
 * @brief Set the scissor-box parameters
 * @param ctx The pointer to the GLES context
 * @param x The new x-offset, in screen coordinates
 * @param y The new y-offset, in screen coordinates
 * @param width The new width, in screen coordinates
 * @param height The new height, in screen coordinates
 * @return An errorcode.
 * @note This implements the GLES entrypoint glScissor().
*/
MALI_IMPORT GLenum _gles_scissor( struct gles_context * const ctx, GLint x, GLint y, GLsizei width, GLsizei height ) MALI_CHECK_RESULT;

/**
 * @brief Set the depth-test parameters
 * @param ctx The pointer to the GLES context
 * @param func The function to be used for depth-testing
 * @return An errorcode.
 * @note This implements the GLES entrypoint glDepthFunc().
 */
MALI_IMPORT GLenum _gles_depth_func( struct gles_context *ctx, GLenum func ) MALI_CHECK_RESULT;


#endif	/* _GLES_PIXEL_OPERATIONS_H_ */

