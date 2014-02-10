/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_mipmap.h
 * @brief gles mipmap related function prototypes
 */

#ifndef _GLES_MIPMAP_H_
#define _GLES_MIPMAP_H_

#include "gles_texture_object.h"
#include <shared/mali_named_list.h>
#include "gles_state.h"
#include "gles_util.h"

/**
 * This is the internal function handling the glGenerateMipmaps entrypoint.
 * Generate all required mipmaps based on the current GLES state.
 * @param ctx A pointer to the GLES context
 * @param tex_env The texture environment, so we can know which texture is bound at the moment
 * @param texture_object_list The list containing all texture objects, used for locking
 * @param target GL_TEXTURE_2D or GL_CUBEMAP for GLES2
 * @note This function is a wrapper around generate_mipmaps
 */
GLenum _gles_generate_mipmap( struct gles_context *ctx,
                               gles_texture_environment* tex_env,
                               mali_named_list* texture_object_list,
                               GLenum target) MALI_CHECK_RESULT;
/**
 * Generates all required mipmaps based on the texel data for the level 0 mipmap. Will be done in SW. Has optimizations for blocked layout
 * @param tex_obj A pointer to the texture object
 * @param ctx A pointer to the GLES context
 * @param target The target, must be GL_TEXTURE_2D or a cube face enum
 * @param base_level The level to make reductions of. Typically 0, but may be any existing index. Levels greater than the base_level will be created
 *
 * @return an error code. GL_NO_ERROR on success.
 **/
GLenum _gles_generate_mipmap_chain_sw_16x16blocked(
	struct gles_texture_object *tex_obj,
	struct gles_context *ctx,
	GLenum target,
	unsigned int base_level ) MALI_CHECK_RESULT;
/**
 * Generates all required mipmaps based on the texel data for the level 0 mipmap. Will be done in SW
 * @param tex_obj A pointer to the texture object
 * @param ctx A pointer to the GLES context
 * @param target The target, must be GL_TEXTURE_2D
 * @param base_level The level to make reductions of. Typically 0, but may be any existing index. Levels greater than the base_level will be created
 *
 * @return an error code. GL_NO_ERROR on success.
 **/
GLenum _gles_generate_mipmap_chain_sw(
		struct gles_texture_object *tex_obj,
		struct gles_context *ctx,
		GLenum target,
		unsigned int base_level
		) MALI_CHECK_RESULT;

/**
 * Generates all required mipmaps based on the texel data for the level 0 mipmap. May be done in HW or SW depending on what is more efficient.
 * @param tex_obj A pointer to the texture object
 * @param ctx A pointer to the GLES context
 * @param target The target, must be GL_TEXTURE_2D
 * @param base_level The level to make reductions of. Typically 0, but may be any existing index. Levels greater than the base_level will be created
 *
 * @return an error code. GL_NO_ERROR on success.
 **/
GLenum _gles_generate_mipmap_chain(
		struct gles_texture_object *tex_obj,
		struct gles_context *ctx,
		GLenum target,
		unsigned int base_level
		) MALI_CHECK_RESULT;



#endif /* _GLES_MIPMAP_H_ */
