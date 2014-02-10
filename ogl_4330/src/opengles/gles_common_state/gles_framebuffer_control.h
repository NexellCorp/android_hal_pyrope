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
 * @file gles_framebuffer_control.h
 * @brief Functions and structures that are used by both OpenGL ES APIs for controlling
 *        the framebuffer settings
 */

#ifndef _GLES_FRAMEBUFFER_CONTROL_H_
#define _GLES_FRAMEBUFFER_CONTROL_H_

#include <gles_ftype.h>

struct gles_context;

typedef struct gles_framebuffer_control
{
	GLboolean color_is_writeable[4];  /**< The color-writemask[0..4] == RGBA, GL_FALSE if no writing should occur, GL_TRUE otherwise */
	GLboolean depth_is_writeable;     /**< Can the depth-buffer be altered? */
	GLuint    stencil_writemask;      /**< The mask to apply to the values written to the stencil-buffer */
	GLftype   color_clear_value[4];   /**< The color that we clear the color-buffer to */
	GLftype   depth_clear_value;      /**< The value that we clear the depth-buffer to */
	GLint     stencil_clear_value;    /**< The value that we clear the stencil-buffer to */

	/* Framebuffer control variable that is only used by the OpenGL ES 2.0 API */
	GLuint    stencil_back_writemask; /**< The mask to apply to the values written to the stencil-back-buffer */
} gles_framebuffer_control;

/**
 * init the framebuffer control structure
 * @param ctx The pointer to the GLES context
 */
void _gles_framebuffer_control_init( struct gles_context *ctx );

/**
 * set the framebuffer clear color
 * @param fb_control The pointer to the framebuffer-control-state
 * @param red The red color
 * @param green The green color
 * @param blue The blue color
 * @param alpha The alpha color
 * @note This implements the GLES-entrypoint glClearColor().
 */
void _gles_clear_color( struct gles_framebuffer_control *fb_control, GLftype red, GLftype green, GLftype blue, GLftype alpha );

/**
 * set the framebuffer depth clear value
 * @param fb_control The pointer to the framebuffer-control-state
 * @param depth The value to clear the depth-buffer to
 * @note This implements the GLES-entrypoint glClearDepth().
 */
void _gles_clear_depth( struct gles_framebuffer_control *fb_control, GLftype depth );

/**
 * set the framebuffer stencil clear value
 * @param fb_control The pointer to the framebuffer-control-state
 * @param s The value to clear the stencil-buffer to
 * @note This implements the GLES-entrypoint glClearDepth().
 */
void _gles_clear_stencil( struct gles_framebuffer_control *fb_control, GLint s );

/**
 * set write mask for the different color channels
 * @param ctx The pointer to the GLES context
 * @param red Sets if it is possible to write to the red channel in the framebuffer
 * @param green Sets if it is possible to write to the green channel in the framebuffer
 * @param blue Sets if it is possible to write to the blue channel in the framebuffer
 * @param alpha Sets if it is possible to write to the alpha channel in the framebuffer
 * @note This implements the GLES-entrypoint glColorMask().
 */
void _gles_color_mask( struct gles_context *ctx, GLboolean red,  GLboolean green,  GLboolean blue,  GLboolean alpha );

/**
 * set write mask for depth
 * @param ctx The pointer to the GLES context
 * @param flag The flag indicating if it is possible to write to the depth-buffer
 * @note This implements the GLES-entrypoint glDepthMask().
 */
void _gles_depth_mask( struct gles_context *ctx, GLboolean flag );

/**
 * set write mask for stencil
 * @param ctx The pointer to the GLES context
 * @param mask The mask that should be used when writing to the stencil buffer(values calculated for writing to the stencil buffer is &'ed with this mask)
 * @note This is a wrapper around the GLES-entrypoints glStencilMask() and glStencilMaskSeparate().
 */
GLenum _gles_stencil_mask( struct gles_context *ctx, GLenum face, GLuint mask ) MALI_CHECK_RESULT;

#endif	/* _GLES_FRAMEBUFFER_CONTROL_H_ */
