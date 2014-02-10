/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles2_rasterization.h
 * @brief API functions to setup rasterization state
 */

#ifndef _GLES2_RASTERIZATION_H_
#define _GLES2_RASTERIZATION_H_

#include "../gles_base.h"
#include "../gles_context.h"

/**
 * @brief Initialize the rasterization-state
 * @param raster The pointer to the rasterization-state
 */
void _gles2_rasterization_init( struct gles_rasterization *raster );

/**
 * @brief Set the current front face
 * @param raster The pointer to the rasterization-state
 * @param mode The new front-face
 * @return An errorcode.
 * @note This is a wrapper function for the GLES entrypoint glFrontFace().
 */
GLenum _gles2_front_face( struct gles_rasterization *raster, GLenum mode ) MALI_CHECK_RESULT;

/** Set what face(s) to cull */
/**
 * @brief Set which face to cull
 * @param raster The pointer to the rasterization-state
 * @param mode The new face to cull
 * @return An errorcode.
 * @note This is a wrapper function for the GLES entrypoint glCullFace().
 */
GLenum _gles2_cull_face( struct gles_rasterization *raster, GLenum mode ) MALI_CHECK_RESULT;

/**
 * @brief Set the current line-width
 * @param ctx The pointer to the GLES context
 * @param width The new line-width
 * @return An errorcode.
 * @note This is a wrapper function for the GLES entrypoint glLineWidth().
 */
GLenum _gles2_line_width( struct gles_context *ctx, GLftype width ) MALI_CHECK_RESULT;


#endif /* _GLES2_RASTERIZATION_H_ */

