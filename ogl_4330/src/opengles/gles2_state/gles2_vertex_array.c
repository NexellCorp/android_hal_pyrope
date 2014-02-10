/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles2_vertex_array.h"
#include "gles_buffer_object.h"
#include <gles_common_state/gles_vertex_array.h>
#include "../gles_config.h"
#include "gles_gb_interface.h"

MALI_STATIC void _gles2_vertex_attrib_array_init( struct gles_context *ctx, u32 index )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	{
		gles_vertex_attrib_array * const attrib = &ctx->state.common.vertex_array.attrib_array[index];

		MALI_DEBUG_ASSERT_POINTER( attrib );

		attrib->size 			= 4;
		attrib->given_stride    = 0;
		attrib->stride			= 0;
		attrib->pointer			= NULL;
		attrib->enabled			= (GLboolean)GL_FALSE;
		attrib->normalized		= (GLboolean)GL_FALSE;
		attrib->buffer_binding	= 0;
		attrib->vbo				= NULL;

		_gles_push_vertex_attrib_type( ctx, index, GL_FTYPE );

		attrib->value[0] = 0.0f;
		attrib->value[1] = 0.0f;
		attrib->value[2] = 0.0f;
		attrib->value[3] = 1.0f;
	}
}

void _gles2_vertex_array_init( struct gles_context *ctx )
{
	u32 i;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	{
		struct gles_vertex_array * const vertex_array = & ctx->state.common.vertex_array;

		MALI_DEBUG_ASSERT_POINTER( vertex_array );

		/* initialize all vertex attribute arrays */
		for (i = 0; i < GLES_VERTEX_ATTRIB_COUNT; ++i)
		{
			_gles2_vertex_attrib_array_init( ctx, i );
		}

		vertex_array->array_buffer_binding         = 0;
		vertex_array->element_array_buffer_binding = 0;

		vertex_array->vbo         = NULL;
		vertex_array->element_vbo = NULL;
	}
}

GLenum _gles2_vertex_attrib_pointer(struct gles_context *ctx, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);

	/* check any input-errors */
	if (index >= GLES_VERTEX_ATTRIB_COUNT) return GL_INVALID_VALUE;
	if (size > 4 || size < 1) return GL_INVALID_VALUE;
	if (stride < 0) return GL_INVALID_VALUE;

	switch (type)
	{
		case GL_BYTE:
		case GL_UNSIGNED_BYTE:
		case GL_SHORT:
		case GL_UNSIGNED_SHORT:
		case GL_FIXED:
		case GL_FLOAT:
		break;
		default:
			return GL_INVALID_ENUM;
	}

	 if (type == GL_FLOAT) normalized = MALI_FALSE; /* normalization can be applied to integer types only. should be handled by HW, but just for safety*/

	_gles_set_vertex_attrib_pointer(ctx, index, size, type, normalized, stride, pointer);

	return GL_NO_ERROR;
}

GLenum _gles2_disable_vertex_attrib_array(struct gles_context* ctx, GLuint index)
{
	
	struct gles_vertex_array *vertex_array;
	MALI_DEBUG_ASSERT_POINTER(ctx);
	vertex_array = &ctx->state.common.vertex_array;
	
	if(index >= GLES_VERTEX_ATTRIB_COUNT) {
		return GL_INVALID_VALUE;
	}
	vertex_array->attrib_array[index].enabled = GL_FALSE;
	_gles_gb_modify_attribute_stream( ctx, vertex_array->attrib_array, index);
	return GL_NO_ERROR;
}

GLenum _gles2_enable_vertex_attrib_array(struct gles_context* ctx, GLuint index)
{
	struct gles_vertex_array *vertex_array;
	MALI_DEBUG_ASSERT_POINTER(ctx);
	vertex_array = &ctx->state.common.vertex_array;
	
	if(index >= GLES_VERTEX_ATTRIB_COUNT) {
		return GL_INVALID_VALUE;
	}
	vertex_array->attrib_array[index].enabled = GL_TRUE;
	_gles_gb_modify_attribute_stream( ctx, vertex_array->attrib_array, index);
	return GL_NO_ERROR;
}

GLenum _gles2_vertex_attrib(struct gles_vertex_array *vertex_array, GLuint index, GLuint num_values, const float* values)
{

	float* f4vquad;
	GLuint i;

	MALI_DEBUG_ASSERT_POINTER(vertex_array);
	if(index >= GLES_VERTEX_ATTRIB_COUNT)
	{
		return GL_INVALID_VALUE;
	}

    if( values == NULL ) return GL_NO_ERROR;

	f4vquad = vertex_array->attrib_array[index].value;

	/* set up quad to contain values, padded with 0,0,0,1 for undefined values. */
	for(i = 0; i < num_values; i++)
	{
		f4vquad[i] = values[i];
	}
	for(i = num_values; i < 3; i++)
	{
		f4vquad[i] = 0.0f;
	}
	if(num_values < 4)
	{
		f4vquad[3] = 1.0f;
	}

	return GL_NO_ERROR;
}

