/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file sg_interface.h
 * @brief Interface for shader generators, set up shader generator context, set new shader state based on fixed-function GLES1 state.
 */

#ifndef _GLES_SG_INTERFACE_H_
#define _GLES_SG_INTERFACE_H_

#include <mali_system.h>
#include <base/mali_macros.h>
#include "shader_generator/gles_shader_generator_context.h"
#include "gles_context.h"
#include <shared/shadergen_interface.h>

struct gles_context;
struct bs_program;

/**
 * Allocates a shader generator context
 * @param Handle to the base context
 * @return The shader generator context
 */
struct gles_sg_context *_gles_sg_alloc( mali_base_ctx_handle base_ctx );

/**
 * Deletes the shader generator context
 * @param sg_ctx The SG context to delete
 */
void _gles_sg_free( struct gles_sg_context *sg_ctx );

/**
 * Initialize all state present in the shader generator context. This mimics the
 * functionality found in gles_fb_init, and should be called at the same time.
 * @param sg_ctx The SG context in which to initialize state
 */
void _gles_sg_state_init(struct gles_sg_context* sg_ctx);

/**
 * Initializes shaders for drawing.
 * @param ctx The pointer to the GLES context
 * @param sg_ctx The pointer to the shader generator context
 * @return GL_NO_ERROR if succesful, errorcode if not
 */
mali_err_code MALI_CHECK_RESULT _gles_sg_init_draw_call( struct gles_context *ctx, struct gles_sg_context *sg_ctx, GLenum primitive_type);


#endif /* _GLES_SG_INTERFACE_H_ */
