/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles1_vertex_array.h"
#include "gles_buffer_object.h"
#include "gles1_state.h"
#include <gles_common_state/gles_vertex_array.h>
#include <opengles/gles_sg_interface.h>

/* Internal function declaration */
MALI_STATIC void _gles1_vertex_attrib_array_init( gles_context * const ctx, const GLuint index, const GLint size, const GLenum type );


/* External function definitions */
void _gles1_vertex_array_init( gles_context * const ctx )
{
	GLuint i;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	{
		struct gles_vertex_array * const vertex_array = &ctx->state.common.vertex_array;

		MALI_DEBUG_ASSERT_POINTER( vertex_array );

		vertex_array->client_active_texture = (u8)0;

		_gles1_vertex_attrib_array_init( ctx, GLES_VERTEX_ATTRIB_POSITION,		4, GL_FTYPE);
		_gles1_vertex_attrib_array_init( ctx, GLES_VERTEX_ATTRIB_NORMAL,		3, GL_FTYPE);
		_gles1_vertex_attrib_array_init( ctx, GLES_VERTEX_ATTRIB_COLOR,			4, GL_FTYPE);
		_gles1_vertex_attrib_array_init( ctx, GLES_VERTEX_ATTRIB_POINT_SIZE,	1, GL_FTYPE);

		for (i = GLES_VERTEX_ATTRIB_TEX_COORD0; i < GLES_VERTEX_ATTRIB_COUNT; ++i)
		{
			_gles1_vertex_attrib_array_init( ctx, i, 4, GL_FTYPE);
		}

#if EXTENSION_PALETTE_MATRIX_OES_ENABLE
		/* Must be run last, because GLES_VERTEX_ATTRIB_MATRIX_INDEX
		 * has higher value than GLES_VERTEX_ATTRIB_TEX_COORD0
		 */
		_gles1_vertex_attrib_array_init( ctx, GLES_VERTEX_ATTRIB_MATRIX_INDEX,	0, GL_UNSIGNED_BYTE);
		_gles1_vertex_attrib_array_init( ctx, GLES_VERTEX_ATTRIB_WEIGHT,		0, GL_FTYPE);
#endif

		vertex_array->array_buffer_binding  = 0;
		vertex_array->element_array_buffer_binding = 0;

		vertex_array->vbo         = NULL;
		vertex_array->element_vbo = NULL;
	}
}


GLenum _gles1_vertex_pointer(struct gles_context *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);

	/* check any input-errors */
	if (size > 4 || size < 2) MALI_ERROR( GL_INVALID_VALUE );

	switch (type)
	{
		case GL_BYTE:
		case GL_SHORT:
		case GL_FIXED:
		case GL_FLOAT:
		break;
		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}

	if (stride < 0) MALI_ERROR( GL_INVALID_VALUE );

	_gles_set_vertex_attrib_pointer(ctx, (GLuint)GLES_VERTEX_ATTRIB_POSITION, size, type, (GLboolean)GL_FALSE, stride, pointer);

	GL_SUCCESS;
}


GLenum _gles1_normal_pointer(struct gles_context *ctx, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);

	switch (type)
	{
		case GL_BYTE:
		case GL_SHORT:
		case GL_FIXED:
		case GL_FLOAT:
		break;
		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}

	if (stride < 0) MALI_ERROR( GL_INVALID_VALUE );

	_gles_set_vertex_attrib_pointer(ctx, (GLuint)GLES_VERTEX_ATTRIB_NORMAL, 3, type, (GLboolean)GL_TRUE, stride, pointer);

	GL_SUCCESS;
}


GLenum _gles1_color_pointer(struct gles_context *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);

	/* check any input-errors */
	if (size != 4) MALI_ERROR( GL_INVALID_VALUE );

	switch (type)
	{
		case GL_UNSIGNED_BYTE:
		case GL_FIXED:
		case GL_FLOAT:
		break;
		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}

	if (stride < 0) MALI_ERROR( GL_INVALID_VALUE );

	_gles_set_vertex_attrib_pointer(ctx, (GLuint)GLES_VERTEX_ATTRIB_COLOR, size, type, (GLboolean)GL_TRUE, stride, pointer);

	GL_SUCCESS;
}


GLenum _gles1_point_size_pointer(struct gles_context *ctx, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);


	switch (type)
	{
		case GL_FIXED:
		case GL_FLOAT:
		break;
		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}

	if (stride < 0) MALI_ERROR( GL_INVALID_VALUE );

	_gles_set_vertex_attrib_pointer(ctx, (GLuint)GLES_VERTEX_ATTRIB_POINT_SIZE, 1, type, (GLboolean)GL_FALSE, stride, pointer);

	GL_SUCCESS;
}


GLenum _gles1_tex_coord_pointer(struct gles_context *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);

	/* check any input-errors */
	if (size > 4 || size < 2) MALI_ERROR( GL_INVALID_VALUE );

	switch (type)
	{
		case GL_BYTE:
		case GL_SHORT:
		case GL_FIXED:
		case GL_FLOAT:
		break;
		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}

	if (stride < 0) MALI_ERROR( GL_INVALID_VALUE );

	_gles_set_vertex_attrib_pointer(ctx, (GLuint)GLES_VERTEX_ATTRIB_TEX_COORD0 + (GLuint)(ctx->state.common.vertex_array.client_active_texture), size, type, (GLboolean)GL_FALSE, stride, pointer);

	GL_SUCCESS;
}


GLenum _gles1_weight_pointer_oes(struct gles_context *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);

	/* check any input-errors */
	if (size <= 0 || size > GLES1_MAX_VERTEX_UNITS_OES) MALI_ERROR( GL_INVALID_VALUE );

	switch (type)
	{
		case GL_FIXED:
		case GL_FLOAT:
		break;
		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}

	if (stride < 0) MALI_ERROR( GL_INVALID_VALUE );
	_gles_set_vertex_attrib_pointer(ctx, (GLuint)GLES_VERTEX_ATTRIB_WEIGHT, size, type, (GLboolean)GL_TRUE, stride, pointer);

	GL_SUCCESS;
}


GLenum _gles1_matrix_index_pointer_oes(struct gles_context *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);

	/* check any input-errors */
	if (size <= 0 || size > GLES1_MAX_VERTEX_UNITS_OES) MALI_ERROR( GL_INVALID_VALUE );

	switch (type)
	{
		case GL_UNSIGNED_BYTE:
		break;
		default:
			MALI_ERROR( GL_INVALID_ENUM );
	}

	if (stride < 0) MALI_ERROR( GL_INVALID_VALUE );

	_gles_set_vertex_attrib_pointer(ctx, (GLuint)GLES_VERTEX_ATTRIB_MATRIX_INDEX, size, type, (GLboolean)GL_FALSE, stride, pointer);

	MALI_GLES_SG_WRITE_FLOAT(NumBones, (float)size);

	GL_SUCCESS;
}


GLenum _gles1_client_active_texture( struct gles_vertex_array *vertex_array, GLenum texture )
{
	s32 tex;

	MALI_DEBUG_ASSERT_POINTER( vertex_array );

	tex = (s32) texture - GL_TEXTURE0;

	if (( tex < 0 ) || ( tex >= GLES_MAX_TEXTURE_UNITS )) MALI_ERROR( GL_INVALID_ENUM );

	vertex_array->client_active_texture = (u8) tex;

	GL_SUCCESS;
}


/* Internal function definitions */
MALI_STATIC void _gles1_vertex_attrib_array_init( gles_context * const ctx, const GLuint index, const GLint size, const GLenum type )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	{
		struct gles_vertex_attrib_array * const attrib = &ctx->state.common.vertex_array.attrib_array[index];

		MALI_DEBUG_ASSERT_POINTER( attrib );

		_gles_push_vertex_attrib_type( ctx, index, type );

		attrib->size           = size;
		attrib->given_stride   = 0;
		attrib->stride         = 0;
		attrib->pointer        = NULL;
		attrib->enabled        = (GLboolean)GL_FALSE;
		attrib->buffer_binding = 0;
		attrib->vbo            = NULL;
	}
}
