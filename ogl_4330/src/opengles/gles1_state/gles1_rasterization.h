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
 * @file gles1_rasterization.h
 * @brief API functions to setup rasterization state
 */

#ifndef _GLES1_RASTERIZATION_H_
#define _GLES1_RASTERIZATION_H_

#include "gles_base.h"
#include <gles_ftype.h>
#include <gles_util.h>

struct gles_rasterization;
struct gles_context;

typedef struct gles1_rasterization
{
	GLftype   point_fade_threshold_size;		/**< The maximum attenuation value */
	GLftype   point_distance_attenuation[3];	/**< The attenuation coefficients for the point-size */

} gles1_rasterization;

/**
 * @brief Initialize the rasterization-state
 * @param ctx The pointer to the GLES context
 */
void _gles1_rasterization_init( struct gles_context *ctx );

/**
 * @brief Set the current front face
 * @param raster The pointer to the rasterization-state
 * @param mode The new front-face
 * @return An errorcode.
 * @note This implements the GLES entrypoint glFrontFace().
 */
GLenum _gles1_front_face( struct gles_rasterization *raster, GLenum mode ) MALI_CHECK_RESULT;

/** Set what face(s) to cull */
/**
 * @brief Set which face to cull
 * @param raster The pointer to the rasterization-state
 * @param mode The new face to cull
 * @return An errorcode.
 * @note This implements the GLES entrypoint glCullFace().
 */
GLenum _gles1_cull_face( struct gles_rasterization *raster, GLenum mode ) MALI_CHECK_RESULT;

/**
 * @brief Set the current point-size
 * @param raster The pointer to the rasterization-state
 * @param size The new point-size
 * @return An errorcode.
 * @note This implements the GLES entrypoint glPointSize().
 */
GLenum _gles1_point_size( struct gles_rasterization *raster, GLftype size ) MALI_CHECK_RESULT;

/**
 * @brief Set the current line-width
 * @param ctx The pointer to the GLES context
 * @param width The new line-width
 * @return An errorcode.
 * @note This implements the GLES entrypoint glLineWidth().
 */
GLenum _gles1_line_width( struct gles_context *ctx, GLftype width ) MALI_CHECK_RESULT;

/**
 * @brief Set the point parameters
 * @param ctx The pointer to the GLES context
 * @param pname The identifier for which parameter to alter
 * @param params the new values for the parameter
 * @param type The data-type of the new values
 * @return An errorcode.
 * @note This implements the GLES entrypoint glPointParameter().
*/
GLenum _gles1_point_parameterv(
	struct gles_context *ctx,
	GLenum pname,
	const GLvoid *params,
	gles_datatype type ) MALI_CHECK_RESULT;

/**
 * @brief Set the point parameters
 * @param ctx The pointer to the GLES context
 * @param pname The identifier for which parameter to alter
 * @param param the new value for the parameter
 * @param type The data-type of the new value
 * @return An errorcode.
 * @note This implements the GLES entrypoint glPointParameter().
 */
GLenum _gles1_point_parameter(
	struct gles_context *ctx,
	GLenum pname,
	const GLvoid *param,
	gles_datatype type ) MALI_CHECK_RESULT;

#endif /* _GLES1_RASTERIZATION_H_ */
