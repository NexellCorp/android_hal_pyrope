/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES_ARRAY_CONVERT_H_
#define _GLES_ARRAY_CONVERT_H_

#include "gles_context.h"

/**
 * @file gles_array_convert.h
 * @brief Functions to put client data into VBOs to assist drawcall merging
 */

struct gles_buffer_object;

/**
 * @brief Copy a user vertex array into a buffer object.
 * The data are copied in a tight packing, even if the original data had an arbitrary stride
 *
 * @param buffer_object The pointer to the buffer object to populate
 * @param first_elem The first element from the user array to copy
 * @param upload_offset Byte offset into ARRAY_BUFFER to copy
 * @param n_vertices Number of vertices to take from array
 * @param arr User attribute array
 * @note The data is loaded straight in without checking whether the GPU is using the VBO,
 * and without validating that the buffer object is large enough etc.
 */
void _gles_array_convert_copy_attribute(struct gles_buffer_object *buffer_object, GLint first_elem, unsigned int upload_offset, unsigned int n_vertices, gles_vertex_attrib_array *arr);

/**
 * @brief Explicitly generate the indices that are implement in a draw_arrays call.
 *
 * @param buffer_object The pointer to the buffer object to populate
 * @param mode The primitive type
 * @param n_in_indices The number of vertices in the draw call, before any unstripping/unfanning
 * @param index_offset Byte offset into the ELEMENT_ARRAY_BUFFER to start writing
 * @param vertex_offset The vertex number to use for the first vertex
 * @note The data is loaded straight in without checking whether the GPU is using the VBO,
 * and without validating that the buffer object is large enough etc.
 */
void _gles_array_convert_generate_indices_for_draw_arrays(struct gles_buffer_object *buffer_object, GLenum mode, unsigned int n_in_indices, unsigned int index_offset, unsigned int vertex_offset);

/**
 * @brief Expand stripped or fanned indices to triangles/lines
 *
 * @param buffer_object The pointer to the buffer object to populate
 * @param mode The primitive type
 * @param type The type of indices (GL_UNSIGNED_SHORT or GL_UNSIGNED_BYTE)
 * @param n_in_indices The number of indices before unstripping/unfanning
 * @param in_indices Pointer to indices to expand
 * @param index_offset Byte offset into buffer_object to write
 * @param vertex_offset A constant which is added to all the indices on the way through
 * @note The data is loaded straight in without checking whether the GPU is using the VBO,
 * and without validating that the buffer object is large enough etc.
 */
void _gles_array_convert_expand_indices_for_draw_elements(struct gles_buffer_object *buffer_object, GLenum mode, GLenum type, unsigned int n_in_indices, const GLvoid *in_indices, unsigned int index_offset, unsigned int vertex_offset);

#endif /* _GLES_ARRAY_CONVERT_H_ */
