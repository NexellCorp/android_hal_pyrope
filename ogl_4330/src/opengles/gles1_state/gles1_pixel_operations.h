/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005, 2007-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles1_pixel_operations.h
 * @brief Pixel operation functionality
 */

#ifndef _GLES1_PIXEL_OPERATIONS_H_
#define _GLES1_PIXEL_OPERATIONS_H_

#include "gles_ftype.h"
#include "gles_util.h"
#include "gles_config_extension.h"

struct gles_context;
struct gles_pixel_operations;

/**
 * @brief Set the alpha-test parameters
 * @param ctx The pointer to the GLES context
 * @param func The function to be used for alpha-testing
 * @param ref The reference value to be used in the alpha-testing
 * @return An errorcode.
 * @note This implements the GLES entrypoint glAlphaFunc().
 */
MALI_IMPORT GLenum _gles1_alpha_func( struct gles_context *ctx, GLenum func, GLftype ref ) MALI_CHECK_RESULT;

/**
 * @brief Set the stencil-test operations.
 * @param ctx The pointer to the GLES context
 * @param face Specifies which set of state is affected. If face is GL_FRONT_AND_BACK then the
 *             front and back state are set identical
 * @param fail Specifies what happens is to be used when the stencil-test fails
 * @param zfail Specifies what happens is to be used when the depth-test fails
 * @param zpass Specifies what happens when the depth-test passes
 * @return An errorcode.
 * @note This is a wrapper function for the GLES entrypoint glStencilOp() and glStencilOpSeparate().
 */
MALI_IMPORT GLenum _gles1_stencil_op( struct gles_context *ctx, GLenum face, GLenum fail, GLenum zfail, GLenum zpass ) MALI_CHECK_RESULT;

/**
 * @brief Set the blend parameters
 * @param ctx The pointer to the GLES context
 * @param srcRGB Blending source RGB function
 * @param dstRGB Blending destination RGB function
 * @param srcAlpha Blending source Alpha function
 * @param dstAlpha Blending destination Alpha function
 * @return An errorcode.
 * @note This is a wrapper function for the GLES entrypoint glBlendFunc().
 */
MALI_IMPORT GLenum _gles1_blend_func( struct gles_context *ctx, GLenum src_rgb, GLenum dst_rgb, GLenum src_alpha, GLenum dst_alpha ) MALI_CHECK_RESULT;

/**
 * @brief Set the logicop-test parameters
 * @param ctx The pointer to the GLES context
 * @param opcode The new value for the opcode
 * @return An errorcode.
 * @note This implements the GLES entrypoint glLogicOp().
 */
MALI_IMPORT GLenum _gles1_logic_op( struct gles_context *ctx, GLenum opcode ) MALI_CHECK_RESULT;

#endif /* _GLES1_PIXEL_OPERATIONS_H_ */
