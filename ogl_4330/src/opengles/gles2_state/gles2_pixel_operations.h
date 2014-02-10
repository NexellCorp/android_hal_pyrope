/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles2_pixel_operations.h
 * @brief Pixel operation functionality
 */

#ifndef GLES2_PIXEL_OPERATIONS_H
#define GLES2_PIXEL_OPERATIONS_H

#include "../gles_ftype.h"
#include "../gles_util.h"

struct gles_pixel_operations;
struct gles_stencil_test;
struct gles_blend;
struct gles_context;

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
MALI_IMPORT GLenum _gles2_stencil_op( struct gles_context *ctx, GLenum face, GLenum fail, GLenum zfail, GLenum zpass ) MALI_CHECK_RESULT;

/**
 * @brief Set the blend parameters
 * @param ctx The pointer to the GLES context
 * @param srcRGB Blending source RGB function
 * @param dstRGB Blending destination RGB function
 * @param srcAlpha Blending source Alpha function
 * @param dstAlpha Blending destination Alpha function
 * @return An errorcode.
 * @note This is a wrapper function for the GLES entrypoint glBlendFunc() and glBlendFuncSeparate().
*/
MALI_IMPORT GLenum _gles2_blend_func( struct gles_context *ctx, GLenum src_rgb, GLenum dst_rgb, GLenum src_alpha, GLenum dst_alpha ) MALI_CHECK_RESULT;

/**
 * @brief Set the constant color to be used in blending
 * @param ctx The pointer to the GLES context
 * @param red Constant blend color
 * @param green Constant blend color
 * @param blue Constant blend color
 * @param alpha Constant blend color
 * @return An errorcode.
 * @note This is a wrapper function for the GLES entrypoint glBlendColor().
 */
MALI_IMPORT GLenum _gles2_blend_color( struct gles_context *ctx, GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha ) MALI_CHECK_RESULT;

/**
 * @brief Set the blend equation
 * @param ctx The pointer to the GLES context
 * @param mode_rgb The new RGB blend function
 * @param mode_alpha The new alpha blend equation
 * @return An errorcode.
 * @note This is a wrapper function for the GLES entrypoint glBlendEquation() and glBlendEquationSeparate.
*/
MALI_IMPORT GLenum _gles2_blend_equation( struct gles_context *ctx, GLenum mode_rgb, GLenum mode_alpha ) MALI_CHECK_RESULT;

#endif	/* GLES2_PIXEL_OPERATIONS_H */

