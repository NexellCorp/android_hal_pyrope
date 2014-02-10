/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <gles_util.h>
#include "gles_vertex_array.h"
#include "gles_buffer_object.h"
#include "gles_gb_interface.h"

void _gles_vertex_array_deinit(struct gles_vertex_array *vertex_array)
{
	unsigned i;
	MALI_DEBUG_ASSERT_POINTER( vertex_array );

	/* deinitialize all vertex attribute arrays */
	for (i = 0; i < GLES_VERTEX_ATTRIB_COUNT; ++i)
	{
		if( NULL != vertex_array->attrib_array[i].vbo) _gles_buffer_object_deref( vertex_array->attrib_array[i].vbo );

		vertex_array->attrib_array[i].buffer_binding = 0;
		vertex_array->attrib_array[i].vbo            = NULL;
	}

	vertex_array->array_buffer_binding         = 0;
	vertex_array->element_array_buffer_binding = 0;

	if( NULL != vertex_array->vbo) _gles_buffer_object_deref( vertex_array->vbo );
	vertex_array->vbo         = NULL;
	if( NULL != vertex_array->element_vbo) _gles_buffer_object_deref( vertex_array->element_vbo );
	vertex_array->element_vbo = NULL;
}

void _gles_push_vertex_attrib_type( struct gles_context * const ctx, const GLuint index, const GLenum type )
{
	struct gles_vertex_array *vertex_array;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT( index < GLES_VERTEX_ATTRIB_COUNT, ("Index is outside the acceptable range (0 <= index < GLES_VERTEX_ATTRIB_COUNT)") );
	MALI_DEBUG_ASSERT(
		(type == GL_BYTE) ||
		(type == GL_UNSIGNED_BYTE) ||
		(type == GL_SHORT) ||
		(type == GL_UNSIGNED_SHORT) ||
		(type == GL_FIXED) ||
		(type == GL_FLOAT),
		("invalid type (0x%X)", type));

	vertex_array = &ctx->state.common.vertex_array;

	MALI_DEBUG_ASSERT_POINTER(vertex_array);

	vertex_array->attrib_array[index].type = type;

	/* Convert GL types into the contiguous range (0, 5).  The GLES
	 * token values are;
	 *
	 * Token				GLES Value		elem_type value
	 * GL_BYTE				0x1400				0
	 * GL_UNSIGNED_BYTE		0x1401				1
	 * GL_SHORT				0x1402				2
	 * GL_UNSIGNED_SHORT	0x1403				3
	 * GL_FLOAT				0x1406				4
	 * GL_FIXED				0x140C				5
	 *
	 */
	{
		u8 elem_type = vertex_array->attrib_array[index].type - GL_BYTE;

		if( elem_type > 3 )
		{
			elem_type -= 2;
		}
		if( elem_type > 5 )
		{
			elem_type  = 5;
		}

		MALI_DEBUG_ASSERT( elem_type < 6, ("Illegal type value") );

		vertex_array->attrib_array[index].elem_type  = elem_type;

		/* Calculate the number of bytes required to represent
		 * the GL type.  Maps the range (0, 5) to values (1, 1, 2,
		 * 2, 4, 4).
		 *
		 * Use this as part of stride calculations.
		 */
		vertex_array->attrib_array[index].elem_bytes = 1 << (elem_type >> 1);

		mali_statebit_set( &ctx->state.common, MALI_STATE_GLESATTRIB0_STREAM_SPEC_DIRTY + index );
	}
}

void _gles_set_vertex_attrib_pointer(struct gles_context *ctx, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer)
{

	struct gles_vertex_array* vertex_array;
	MALI_DEBUG_ASSERT_POINTER(ctx);

	/*
	this function should allways be called from inside
	the driver, so any errors in the parameters are driver-bugs.
	*/
	MALI_DEBUG_ASSERT_RANGE(size, 1, 4);
	MALI_DEBUG_ASSERT(stride >= 0, ("negative stride (%d)", stride));
	MALI_DEBUG_ASSERT(
		(type == GL_BYTE) ||
		(type == GL_UNSIGNED_BYTE) ||
		(type == GL_SHORT) ||
		(type == GL_UNSIGNED_SHORT) ||
		(type == GL_FIXED) ||
		(type == GL_FLOAT),
		("invalid type (0x%X)", type));

	MALI_DEBUG_ASSERT( index < GLES_VERTEX_ATTRIB_COUNT, ("Index is outside the acceptable range (0 < index < GLES_VERTEX_ATTRIB_COUNT)") );

	vertex_array = &ctx->state.common.vertex_array;
	MALI_DEBUG_ASSERT_POINTER(vertex_array);

	/* changing buffer object? */
	if (vertex_array->array_buffer_binding != vertex_array->attrib_array[index].buffer_binding)
	{
		/* update refcounts */
		if (NULL != vertex_array->vbo)                     _gles_buffer_object_addref(vertex_array->vbo);
		if (NULL != vertex_array->attrib_array[index].vbo) _gles_buffer_object_deref(vertex_array->attrib_array[index].vbo);

		/* copy the buffer binding */
		vertex_array->attrib_array[index].buffer_binding = vertex_array->array_buffer_binding;
		vertex_array->attrib_array[index].vbo            = vertex_array->vbo;
	}

	/* fill in state */
	_gles_push_vertex_attrib_type( ctx, index, type );

	vertex_array->attrib_array[index].given_stride = stride;
	if( 0 == stride ) stride = vertex_array->attrib_array[index].elem_bytes * size;

	vertex_array->attrib_array[index].size       = size;
	vertex_array->attrib_array[index].normalized = normalized;
	vertex_array->attrib_array[index].stride     = stride;
	vertex_array->attrib_array[index].pointer    = pointer;

	/* tell the geometry backend that this pointer has been modified */
	_gles_gb_modify_attribute_stream(ctx, vertex_array->attrib_array, index);
}

void _gles_vertex_array_get_binding(gles_vertex_array *vertex_array, GLenum target, GLuint *binding, struct gles_buffer_object **buffer_object)
{
	MALI_DEBUG_ASSERT_POINTER( vertex_array );
	MALI_DEBUG_ASSERT_POINTER( binding );
	MALI_DEBUG_ASSERT_POINTER( buffer_object );

	switch (target)
	{
		case GL_ARRAY_BUFFER:
			*buffer_object = vertex_array->vbo;
			*binding = vertex_array->array_buffer_binding;
		break;

		case GL_ELEMENT_ARRAY_BUFFER:
			*buffer_object = vertex_array->element_vbo;
			*binding = vertex_array->element_array_buffer_binding;
		break;

		default:
			/* This should have been checked by the caller */
			MALI_DEBUG_ASSERT(0, ("Invalid target"));
			break;
	}
}

void _gles_vertex_array_remove_binding_by_ptr(gles_vertex_array *vertex_array, const void* ptr)
{
	int i;
	MALI_DEBUG_ASSERT_POINTER( vertex_array );

	if (ptr == (const void*)vertex_array->vbo)
	{
		_gles_buffer_object_deref( vertex_array->vbo );

		vertex_array->array_buffer_binding = 0;
		vertex_array->vbo                  = NULL;
	}

	if (ptr == (const void*)vertex_array->element_vbo)
	{
		/* deref */
		_gles_buffer_object_deref( vertex_array->element_vbo );

		vertex_array->element_array_buffer_binding = 0;
		vertex_array->element_vbo                  = NULL;
	}

	for (i = 0; i < GLES_VERTEX_ATTRIB_COUNT; ++i)
	{
		if (ptr == (const void*) vertex_array->attrib_array[i].vbo)
		{
			/* deref */
			_gles_buffer_object_deref( vertex_array->attrib_array[i].vbo );

			vertex_array->attrib_array[i].buffer_binding = 0;
			vertex_array->attrib_array[i].vbo            = NULL;
		}
	}
}

void _gles_vertex_array_set_binding(gles_vertex_array *vertex_array, GLenum target, GLuint binding, struct gles_buffer_object *buffer_object)
{
	struct gles_buffer_object *old_buffer_object = NULL;
	MALI_DEBUG_ASSERT_POINTER( vertex_array );

	switch (target)
	{
		case GL_ARRAY_BUFFER:
			/* store away old binding */
			old_buffer_object = vertex_array->vbo;

			/* update state */
			vertex_array->vbo                  = buffer_object;
			vertex_array->array_buffer_binding = binding;

			/* addref new binding */
			if (NULL != buffer_object) _gles_buffer_object_addref(buffer_object);
			/* deref old binding */
			if (NULL != old_buffer_object) _gles_buffer_object_deref(old_buffer_object);
		break;

		case GL_ELEMENT_ARRAY_BUFFER:
			/* store away old binding */
			old_buffer_object = vertex_array->element_vbo;

			vertex_array->element_vbo = buffer_object;
			vertex_array->element_array_buffer_binding = binding;

			/* addref new binding */
			if (NULL != buffer_object) _gles_buffer_object_addref(buffer_object);
			/* deref old binding */
			if (NULL != old_buffer_object) _gles_buffer_object_deref(old_buffer_object);
		break;

		default:
			/* This should have been checked by the caller */
			MALI_DEBUG_ASSERT(0, ("Invalid target"));
			break;
	}
}

#if MALI_API_TRACE

void _gles_gb_get_non_vbo_buffers(struct gles_context *ctx, u32 nvert, char* buffers[GLES_VERTEX_ATTRIB_COUNT], u32 sizes[GLES_VERTEX_ATTRIB_COUNT], u32* count)
{
	unsigned int i;
	u32 out_index = 0;
	struct gles_vertex_array* vertex_array;
	u32 already_included = 0; /* bit field for marking which attrib array we have included */

	MALI_DEBUG_ASSERT_POINTER(ctx);

	if (nvert == 0)
	{
		*count = 0;
		return;
	}

	vertex_array = &ctx->state.common.vertex_array;
	MALI_DEBUG_ASSERT_POINTER(vertex_array);

	for ( i = 0; i < GLES_VERTEX_ATTRIB_COUNT; i++ )
	{
		if ( MALI_TRUE == vertex_array->attrib_array[i].enabled &&
		     NULL == vertex_array->attrib_array[i].vbo &&
		     NULL != vertex_array->attrib_array[i].pointer &&
		     0 == (already_included & (1 << i)) )
		{
			/* This is a pointer to some user data we need to copy it */

			char* buffer1_start;
			u32 buffer1_size;
			char* buffer1_end;
			unsigned int j;
			mali_bool did_a_merge;

			buffer1_start = (char*)vertex_array->attrib_array[i].pointer;
			if (vertex_array->attrib_array[i].stride == 0)
			{
				buffer1_size = vertex_array->attrib_array[i].size * vertex_array->attrib_array[i].elem_bytes * nvert;
			}
			else
			{
				buffer1_size = (vertex_array->attrib_array[i].stride * (nvert - 1)) + (vertex_array->attrib_array[i].size * vertex_array->attrib_array[i].elem_bytes);
			}
			buffer1_end = buffer1_start + buffer1_size;

			do
			{
				did_a_merge = MALI_FALSE; /* support case like nvert == 1 and arrays defined in order like 0, 2, 1 */

				/*
				 * Now check if this buffer is interlieved with any of the other buffers.
				 * In that case; merge them together, so we only get one memory block instead.
				 */
				for ( j = i + 1; j < GLES_VERTEX_ATTRIB_COUNT; j++ )
				{
					if ( MALI_TRUE == vertex_array->attrib_array[j].enabled &&
					     NULL == vertex_array->attrib_array[j].vbo &&
					     NULL != vertex_array->attrib_array[j].pointer &&
					     0 == (already_included & (1 << j)) )
					{
						char* buffer2_start;
						u32 buffer2_size;
						char* buffer2_end;

						buffer2_start = (char*)vertex_array->attrib_array[j].pointer;
						if (vertex_array->attrib_array[j].stride == 0)
						{
							buffer2_size = vertex_array->attrib_array[j].size * vertex_array->attrib_array[j].elem_bytes * nvert;
						}
						else
						{
							buffer2_size = (vertex_array->attrib_array[j].stride * (nvert - 1)) + (vertex_array->attrib_array[j].size * vertex_array->attrib_array[j].elem_bytes);
						}
						buffer2_end = buffer2_start + buffer2_size;

						if ( (buffer2_start >= buffer1_start && buffer2_start <= buffer1_end) ||
							 (buffer2_end >= buffer1_start && buffer2_end <= buffer1_end) )
						{
							if (buffer2_end > buffer1_end)
							{
								buffer1_end = buffer2_end;
							}

							if (buffer2_start < buffer1_start)
							{
								buffer1_start = buffer2_start;
							}

							already_included |= (1 << j); /* mark this block as already handled */
							did_a_merge = MALI_TRUE;
						}
					}
				}
			} while (did_a_merge);

			/* buffer1 might now be extended, update size; */

			buffer1_size = (u32)buffer1_end - (u32)buffer1_start;
			buffers[out_index] = (void*)buffer1_start;
			sizes[out_index] = buffer1_size;
			out_index++;

			/* No need to do a; already_included |= (1 << i); because I will not consider the buffer i again */
		}
	}

	*count = out_index;
}

#endif
