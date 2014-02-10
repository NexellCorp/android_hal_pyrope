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
 * @file gles_rasterization.h
 * @brief Structures and functions to set up those parts of the rasterization
 *        state that is that is common to both OpenGL ES APIs.
 */

#ifndef _GLES_RASTERIZATION_H_
#define _GLES_RASTERIZATION_H_

#include "gles_base.h"
#include "gles_ftype.h"

struct gles_context;

/**
 * Structure containing state that controls how the rasterization of
 * primitives should be performed
 */
typedef struct gles_rasterization
{
	GLenum    front_face;             /**< Specifies what is the current front-face */
	GLboolean cull_face;              /**< Is GL_CULL_FACE enabled? */
	GLenum    cull_face_mode;         /**< Specifies which face we are culling */

	GLftype   point_size;             /**< The current point-size */
	GLftype   point_size_min;         /**< The minimum point-size */
	GLftype   point_size_max;         /**< The maximum point-size */

	GLftype   line_width;             /**< The current line-width */
} gles_rasterization;


/**
 * @brief Set the polygon-offset parameters
 * @param ctx The pointer to the GLES context
 * @param factor The new polygon-offset-factor
 * @param units The new polygon-offset-units
 * @return An errorcode.
 * @note This is a wrapper function for the GLES entrypoint glPolygonOffset().
 */
GLenum _gles_polygon_offset( struct gles_context *ctx, GLftype factor, GLftype units ) MALI_CHECK_RESULT;


#endif /* _GLES_RASTERIZATION_H_ */

