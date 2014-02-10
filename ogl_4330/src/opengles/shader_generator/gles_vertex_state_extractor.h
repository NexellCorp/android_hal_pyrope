/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES_VERTEX_STATE_EXTRACTOR_H_
#define _GLES_VERTEX_STATE_EXTRACTOR_H_

#include <shared/shadergen_interface.h>
#include "../gles_context.h"

/**
 * Builds the remapping table for vertex shader attributes.
 * @param sgstate The vertex shader generator state bitfield
 * @param remap_table A pointer to the remapping table to update. Must be != NULL
 */
void _gles_sg_make_attribute_remap_table(struct vertex_shadergen_state * sgstate, int *remap_table);

/**
 * Updates the current GLES state (vertex_array) with the current bound names
 * @param glstate A pointer to the GLES state. Must be != NULL
 */
void _gles_sg_update_current_attribute_values(gles_state *glstate);

/**
 * Extracts the uniforms required for the vertex shader based on the current GLES state
 * @param ctx A pointer to the GLES context. Must be != NULL
 * @param sgstate The vertex shader generator state bitfield
 * @param uniforms A pointer to where the uniform values should be stored. Must be != NULL
 * @param program_binary A pointer to the program-binary-state
 */
void _gles_sg_extract_vertex_uniforms(gles_context *ctx, const struct vertex_shadergen_state * sgstate, float *uniforms, bs_program *program_binary);

/**
 * Get the number of uniforms used by the vertex shader based on the given state
 * @param sgstate The vertex shader generator state bitfield
 */
int _gles_sg_get_vertex_uniform_array_size(const struct vertex_shadergen_state * sgstate);

#endif /* _GLES_VERTEX_STATE_EXTRACTOR_H_ */
