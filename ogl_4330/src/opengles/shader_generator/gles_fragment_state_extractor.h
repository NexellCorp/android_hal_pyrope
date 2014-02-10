/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES_FRAGMENT_STATE_EXTRACTOR_H_
#define _GLES_FRAGMENT_STATE_EXTRACTOR_H_

#include <shared/shadergen_interface.h>

struct gles_context;
/**
 * Extracts the state required for the fragment shader generator from the current GLES state
 * @param ctx A pointer to the GLES context. Must be != NULL
 * @param sgstate A pointer to the fragment shader generator state. Must be != NULL
 */
void _gles_sg_extract_fragment_state(struct gles_context *ctx, GLenum primitive_type);

/**
 * Extracts the uniforms required for the fragment shader based on the current GLES state
 * @param ctx A pointer to the GLES context. Must be != NULL
 * @param sgstate A pointer to the fragment shader generator state. Must be != NULL
 * @param uniforms A pointer to where the uniform values should be stored/cached. Must be != NULL
 * @param program_binary A pointer to the program-binary-state
 */
void _gles_sg_extract_fragment_uniforms(gles_context *ctx, const fragment_shadergen_state *sgstate, gles_fp16 *uniforms, bs_program *program_binary);

/**
 * Get the number of uniforms used by the fragment shader based on the given state
 */
int _gles_sg_get_fragment_uniform_array_size(const fragment_shadergen_state *sgstate);

#endif /* _GLES_FRAGMENT_STATE_EXTRACTOR_H_ */
