/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles2_vertex_array.h
 * @brief All routines related to settings for vertex arrays
 */

#ifndef GLES2_VERTEX_ARRAY_H
#define GLES2_VERTEX_ARRAY_H

#include <mali_system.h>
#include <GLES2/mali_gl2.h>
#include "../gles_ftype.h"
#include "../gles_config.h"

struct gles_context;
struct gles_vertex_array;
struct gles_buffer_object;

/**
 * Sets the vertex position pointer
 * @param ctx The GLES context pointer
 * @note This is an initialization method for the vertex-array-state.
 */
void _gles2_vertex_array_init( struct gles_context *ctx );

/**
 * Sets the vertex attribute pointers
 * @param ctx GLES context pointer
 * @param index the attribute index
 * @param size elements of array
 * @param type datatype of array
 * @param normalized a boolean indicating if the parameter should be loaded in normalized range or not (0..1 or 0..DATATYPE_MAX)
 * @param stride stride between each element
 * @param pointer to the actual data-pointer
 * @note This is a wrapper around the GLES-entrypoint glVertexAttribPointer().
 */
GLenum _gles2_vertex_attrib_pointer(struct gles_context *ctx, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer) MALI_CHECK_RESULT;

/**
 * @brief disables vertex attribute arrays
 * @param ctx The GLES context
 * @param index The index to disable
 * @note This function is a wrapper around glDisableVertexAttribArray
 */
GLenum _gles2_disable_vertex_attrib_array(struct gles_context* ctx, GLuint index) MALI_CHECK_RESULT;

/**
 * @brief enables vertex attribute arrays
 * @param ctx The GLES context
 * @param index The index to disable
 * @note This function is a wrapper around glEnableVertexAttribArray
 */
GLenum _gles2_enable_vertex_attrib_array(struct gles_context* ctx, GLuint index) MALI_CHECK_RESULT;

/**
 * @brief sets the vertex value parameter
 * @param vertex_array the vertex array to alter
 * @param index index of the vertex array to alter
 * @param num_values the number of values - between 1 and 4 (inclusive)
 * @param values an array of values
 * @note this function is a wrapper aroung glVertexAttrib[1234]f[v]
 */
GLenum _gles2_vertex_attrib(struct gles_vertex_array *vertex_array, GLuint index, GLuint num_values, const float* values) MALI_CHECK_RESULT;

#endif /* _GLES2_VERTEX_ARRAY_H_ */
