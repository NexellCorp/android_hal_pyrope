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
 * @file gles1_current.h
 * @brief Defines the structures that build up the current-state of the gles-context.
 */

#ifndef _GLES1_CURRENT_H_
#define _GLES1_CURRENT_H_

#include <gles_ftype.h>
#include <gles_config.h>

struct gles1_lighting;
struct gles_context;

/** this structure is all that is left of the Begin()/End()-paradigm in gles 1.x */
typedef struct gles1_current
{
	GLftype color[4];                                 /**< current color. rgba-data. */
	GLftype tex_coord[GLES_MAX_TEXTURE_UNITS][4];     /**< current texture coordinate. strq-data, one pr possible texture unit. */
	GLftype normal[3];                                /**< current normal. xyz-data. */
} gles1_current;

/**
 * Initializes the current state
 * @param current The pointer to the current-state
 */
void _gles1_current_init(gles1_current *current);

/**
 * Set the current-color
 * @param ctx The pointer to the GLES context
 * @param red The red component
 * @param green The green component
 * @param blue The blue component
 * @param alpha The alpha component
 * @note This implements the GLES-entrypoint glColor4 ().
 * @return a GLES error code, GL_NO_ERROR if state-change was successful
 */
GLenum _gles1_color4( struct gles_context *ctx, GLftype red, GLftype green, GLftype blue, GLftype alpha ) MALI_CHECK_RESULT;

/**
 * Set the current-normal
 * @param current The pointer to the current-state
 * @param nx The x component of the normal
 * @param ny The y component of the normal
 * @param nz The z component of the normal
 * @note This implements the GLES-entrypoint glNormal3 ().
 * @return a GLES error code, GL_NO_ERROR if state-change was successful
 */
GLenum _gles1_normal3( gles1_current *current, GLftype nx, GLftype ny, GLftype nz ) MALI_CHECK_RESULT;

/**
 * Set the current-texture-coordinate for the active client-texture
 * @param current The pointer to the current-state
 * @param target Can only be GL_TEXTURE_2D
 * @param s The s component of the texture coordinate
 * @param t The t component of the texture coordinate
 * @param r The r component of the texture coordinate
 * @param q the q component of the texture coordinate
 * @note This implements the GLES-entrypoint glMultiTexCoord4 ().
 * @return a GLES error code, GL_NO_ERROR if state-change was successful
 */
GLenum _gles1_multi_tex_coord4( gles1_current *current, GLenum target, GLftype s, GLftype t, GLftype r, GLftype q ) MALI_CHECK_RESULT;


#endif /* _GLES1_CURRENT_H_ */
