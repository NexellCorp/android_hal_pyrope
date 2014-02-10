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
 * @file gles1_coloring.h
 * @brief Defines the structure and prototype functions for fogging and coloring states.
 */

#ifndef _GLES1_COLORING_H_
#define _GLES1_COLORING_H_

#include <gles_base.h>
#include <gles_float_type.h>
#include <gles_util.h>

struct gles_context;

typedef struct gles1_coloring
{
	/* fogging parameters */
	GLftype   fog_color[4]; /**< The color of the fog */
	GLftype   fog_density;  /**< The density of the fog */
	GLftype   fog_start;    /**< Z-coordinate for where the fog starts */
	GLftype   fog_end;      /**< Z-coordinate for where the fog ends */
	GLenum    fog_mode;     /**< The fog-mode(GL_EXP, GL_EXP2, GL_LINEAR */

} gles1_coloring;

/**
 * Initializes the color-state
 * @param coloring The pointer to the coloring-state
 * @note This function initializes the coloring-state.
 */
void _gles1_coloring_init(gles1_coloring *coloring);

/**
 * Set the current shade model
 * @param ctx The gles context
 * @param mode the shade model to use
 * @note This implements the GLES-entrypoint glShadeModel()
 * @return a GLES error code, GL_NO_ERROR if state-change was successful
 */
GLenum _gles1_shade_model( struct gles_context *ctx, GLenum mode ) MALI_CHECK_RESULT;

/**
 * Set up the fog-properties
 * @param ctx Pointer to the context
 * @param pname The enum for the different fog-properties
 * @param params The new values to be set
 * @param type Which type the entries in <params> are
 * @note This implements the GLES-entrypoint glFogv()
 * @return a GLES error code, GL_NO_ERROR if state-change was successful
 */
GLenum _gles1_fogv( struct gles_context *ctx, GLenum pname, const GLvoid *params, gles_datatype type ) MALI_CHECK_RESULT;

/**
 * Set up the fog-properties
 * @param ctx Pointer to the context
 * @param pname The enum for the different fog-properties
 * @param param The new value to be set
 * @param type Which type <param> is
 * @note This implements the GLES-entrypoint glFogv()
 * @return a GLES error code, GL_NO_ERROR if state-change was successful
 */
GLenum _gles1_fog( struct gles_context *ctx, GLenum pname, const GLvoid *param, gles_datatype type ) MALI_CHECK_RESULT;

#endif /* _GLES1_COLORING_H_ */
