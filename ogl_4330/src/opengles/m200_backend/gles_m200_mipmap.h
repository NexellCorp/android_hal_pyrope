/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_M200_MIPMAP_H
#define GLES_M200_MIPMAP_H

#include "../gles_base.h"

/* forward defines */
struct gles_texture_object;
struct gles_context;

/**
 * Generate mipmaps for single mipmap face, in HW.
 * @param tex_obj A pointer to the texture object
 * @param ctx A pointer to the GLES context
 * @param target GL_TEXTURE_2D or one of the cubemap faces
 * @return A GLES error code. GL_NO_ERROR on success
 */
GLenum _gles_generate_mipmap_chain_hw(
	struct gles_texture_object* tex_obj,
	struct gles_context* ctx,
	GLenum target,
	unsigned int base_miplvl);


#endif /* GLES_M200_MIPMAP_H */
