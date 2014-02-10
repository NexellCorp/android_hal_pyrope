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
 * @file gles1_vertex_array.h
 * @brief All routines related to settings for vertex arrays
 */

#ifndef _GLES1_VERTEX_ARRAY_H_
#define _GLES1_VERTEX_ARRAY_H_

#include <gles_base.h>
#include <gles_config.h>

struct gles_buffer_object;
struct gles_context;
struct gles_vertex_array;

/**
  * enum for table lookup for the different GL-arrays
  */
typedef enum
{
	GLES_VERTEX_ATTRIB_POSITION     = 0,
	GLES_VERTEX_ATTRIB_NORMAL       = 1,
	GLES_VERTEX_ATTRIB_COLOR        = 2,
	GLES_VERTEX_ATTRIB_POINT_SIZE   = 3,
	GLES_VERTEX_ATTRIB_WEIGHT       = 4,
	GLES_VERTEX_ATTRIB_MATRIX_INDEX = 5,
	GLES_VERTEX_ATTRIB_TEX_COORD0   = 6,
	GLES_VERTEX_ATTRIB_TEX_COORD1   = 7,
#if !HARDWARE_ISSUE_4326
	GLES_VERTEX_ATTRIB_TEX_COORD2   = 8,
	GLES_VERTEX_ATTRIB_TEX_COORD3   = 9,
	GLES_VERTEX_ATTRIB_TEX_COORD4   = 10,
	GLES_VERTEX_ATTRIB_TEX_COORD5   = 11,
	GLES_VERTEX_ATTRIB_TEX_COORD6   = 12,
	GLES_VERTEX_ATTRIB_TEX_COORD7   = 13,

	#if GLES_VERTEX_ATTRIB_COUNT < 14 /* GLES1_VERTEX_ATTRIB_COUNT - but the C standard does not allow us to access enums from the preprocessor */
	#error "Error in gles1_config.h: GLES_VERTEX_ATTRIB_COUNT must be at least 14 on HW > r0p1"
	#endif /* GLES_VERTEX_ATTRIB_COUNT < 14 */
#else
	#if GLES_VERTEX_ATTRIB_COUNT < 8 /* GLES1_VERTEX_ATTRIB_COUNT - but the C standard does not allow us to access enums from the preprocessor */
	#error "Error in gles1_config.h: GLES_VERTEX_ATTRIB_COUNT must be at least 8 on r0p1"
	#endif /* GLES_VERTEX_ATTRIB_COUNT < 8 */
#endif
	GLES1_VERTEX_ATTRIB_COUNT /**< the number of vertex-streams */
} gles1_vertex_attrib;

/**
 * Sets the vertex position pointer
 * @param ctx	Pointer to the GLES context
 * @note This is an initialization method for the vertex-array-state.
 */
void _gles1_vertex_array_init( struct gles_context * const ctx );

/**
 * Sets the vertex position pointer
 * @param ctx a pointer to the context
 * @param size elements of array
 * @param type datatype of array
 * @param stride stride between each element
 * @param pointer to the actual data-pointer
 * @note This implements the GLES-entrypoint glVertexPointer().
 */
GLenum _gles1_vertex_pointer(struct gles_context *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) MALI_CHECK_RESULT;

/**
 * Sets the vertex normal pointer
 * @param ctx The gles context
 * @param type datatype of array
 * @param stride stride between each element
 * @param pointer to the actual data-pointer
 * @note This implements the GLES-entrypoint glNormalPointer().
 */
GLenum _gles1_normal_pointer(struct gles_context *ctx, GLenum type, GLsizei stride, const GLvoid *pointer) MALI_CHECK_RESULT;

/**
 * Sets the vertex color pointer
 * @param ctx The gles context
 * @param size elements of array
 * @param type datatype of array
 * @param stride stride between each element
 * @param pointer to the actual data-pointer
 * @note This implements the GLES-entrypoint glColorPointer().
 */
GLenum _gles1_color_pointer(struct gles_context *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) MALI_CHECK_RESULT;

/**
 * Sets the point size pointer
 * @param ctx The gles context
 * @param type datatype of array
 * @param stride stride between each element
 * @param pointer to the actual data-pointer
 * @note This implements the GLES-entrypoint glPointSizePointerOES().
 */
GLenum _gles1_point_size_pointer(struct gles_context *context, GLenum type, GLsizei stride, const GLvoid *pointer) MALI_CHECK_RESULT;

/**
 * Sets the vertex texture coordinate pointer
 * @param ctx The gles context
 * @param size elements of array
 * @param type datatype of array
 * @param stride stride between each element
 * @param pointer to the actual data-pointer
 * @note This implements the GLES-entrypoint glTexCoordPointer().
 */
GLenum _gles1_tex_coord_pointer(struct gles_context *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) MALI_CHECK_RESULT;

/**
 * Sets the vertex weight pointer
 * @param ctx The gles context
 * @param size elements of array
 * @param type datatype of array
 * @param stride stride between each element
 * @param pointer to the actual data-pointer
 * @note This implements the GLES-entrypoint glWeightPointerOES().
 */
GLenum _gles1_weight_pointer_oes(struct gles_context *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) MALI_CHECK_RESULT;

/**
 * Sets the vertex matrix-index pointer
 * @param ctx The gles context
 * @param size elements of array
 * @param type datatype of array
 * @param stride stride between each element
 * @param pointer to the actual data-pointer
 * @note This implements the GLES-entrypoint glMatrixIndexPointerOES().
 */
GLenum _gles1_matrix_index_pointer_oes(struct gles_context *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) MALI_CHECK_RESULT;

/**
 * Sets the active texture for the client-state
 * @param vertex_array The vertex-array of our GLES context
 * @param texture Which texture to set as active for the client-state
 * @note This implements the GLES-entrypoint glClientActiveTexture().
 */
GLenum _gles1_client_active_texture( struct gles_vertex_array *vertex_array, GLenum texture ) MALI_CHECK_RESULT;

#endif /* _GLES1_VERTEX_ARRAY_H_ */
