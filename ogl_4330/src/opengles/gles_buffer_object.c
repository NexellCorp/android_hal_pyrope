/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_buffer_object.h"
#include "gles_share_lists.h"
#include <gles_common_state/gles_vertex_array.h>
#include "gles_gb_interface.h"
#include "gles_object.h"
#include "gles_instrumented.h"


void _gles_buffer_object_init(gles_buffer_object *buffer_object)
{
	MALI_DEBUG_ASSERT_POINTER(buffer_object);
	buffer_object->gb_data   = NULL;
	buffer_object->size      = 0;
	buffer_object->usage     = GL_STATIC_DRAW;
	buffer_object->access    = GL_WRITE_ONLY_OES;
	buffer_object->mapped    = GL_FALSE;
	buffer_object->is_deleted = MALI_FALSE;

	_mali_sys_atomic_initialize(&buffer_object->ref_count, 1);/* initial reference count is 1, because (obviously) a pointer to it exists. we just got it in here. */
}

gles_buffer_object *_gles_buffer_object_new( void )
{
	gles_buffer_object *obj = _mali_sys_malloc(sizeof(gles_buffer_object));
	if ( NULL == obj ) return NULL;

	_gles_buffer_object_init(obj);
	return obj;
}

void _gles_buffer_object_deref(gles_buffer_object *buffer_object)
{
	u32 ref_count = 0;
	MALI_DEBUG_ASSERT_POINTER(buffer_object);
	MALI_DEBUG_ASSERT(_mali_sys_atomic_get( &buffer_object->ref_count ) > 0, ("Negative ref count"));

	ref_count = _mali_sys_atomic_dec_and_return( &buffer_object->ref_count );

	if (0 == ref_count)
	{
		/* free underlying data */
		if (buffer_object->gb_data != NULL)
		{
			_gles_gb_free_buffer_data(buffer_object->gb_data);
			buffer_object->gb_data = NULL;
		}

		/* free object itself */
		_mali_sys_free(buffer_object);
		buffer_object = NULL;
	}
}

GLenum _gles_delete_buffers( mali_named_list *buffer_object_list, struct gles_vertex_array *vertex_array, GLsizei size, const GLuint * buffers )
{
	GLsizei index;
	MALI_DEBUG_ASSERT_POINTER( buffer_object_list );
	MALI_DEBUG_ASSERT_POINTER( vertex_array );

	if (NULL == buffers) GL_SUCCESS;
	if (size < 0) MALI_ERROR( GL_INVALID_VALUE );

	for (index = 0; index < size; ++index)
	{
		struct gles_wrapper *name_wrapper;
		const GLuint name = buffers[index];

		/* silently ignore 0 */
		if( 0 == name ) continue;

		name_wrapper = __mali_named_list_get( buffer_object_list, name );
		if ( NULL == name_wrapper ) continue; /* name not found, silently ignore */
		if ( NULL != name_wrapper->ptr.vbo )
		{
			/* remove any bindings to this buffer object */
			_gles_vertex_array_remove_binding_by_ptr(vertex_array, name_wrapper->ptr.vbo);

			/* delete object from name wrapper */
			name_wrapper->ptr.vbo->is_deleted = MALI_TRUE;
			_gles_buffer_object_deref(name_wrapper->ptr.vbo);
			name_wrapper->ptr.vbo = NULL;
		}

		/* remove name and free name wrapper */
		__mali_named_list_remove(buffer_object_list, name);
		_gles_wrapper_free( name_wrapper );
		name_wrapper = NULL;
	}

	GL_SUCCESS;
}

void _gles_buffer_object_list_entry_delete(struct gles_wrapper *wrapper)
{
	MALI_DEBUG_ASSERT_POINTER(wrapper);

	if ( NULL != wrapper )
	{
		if ( NULL != wrapper->ptr.vbo )
		{
			/* dereference buffer object */
			_gles_buffer_object_deref(wrapper->ptr.vbo);
			wrapper->ptr.vbo= NULL;
		}

		/* delete wrapper */
		_gles_wrapper_free( wrapper );
		wrapper = NULL;
	}
}

MALI_CHECK_RESULT gles_buffer_object *_gles_get_buffer_object( struct mali_named_list *buffer_object_list, GLint name )
{
	gles_buffer_object *obj = NULL;
	struct gles_wrapper *name_wrapper = NULL;

	/* search the list */
	name_wrapper = __mali_named_list_get(buffer_object_list, name);
	if ( name_wrapper != NULL )
	{
		/* the name has been reserved.
		 * Note that the buffer object might not have been created yet, so name_wrapper->tex_obj
		 * could still be NULL
		 */
		obj = name_wrapper->ptr.vbo;
	}

	/* the texture object was not found in the list, try to create it. Note that both paths above can return NULL  */
	if (NULL == obj)
	{
		mali_err_code err;

		/* buffer object not yet allocated, let's do that now. */
		obj = _gles_buffer_object_new();
		if ( NULL == obj ) return NULL;

		MALI_DEBUG_ASSERT(_mali_sys_atomic_get( &obj->ref_count ) == 1, ("refcount not as expected"));

		if ( NULL != name_wrapper )
		{
			/* name has already been reserved - e.g. glBindBuffer after glGenBuffers */
			name_wrapper->ptr.vbo = obj;
		}
		else
		{
			/* no name has been reserved - e.g. glBindBuffer _without_ glGenBuffers */
			name_wrapper = _gles_wrapper_alloc(WRAPPER_VERTEXBUFFER);
			if ( NULL == name_wrapper )
			{
				/* out of memory, clean up */
				_gles_buffer_object_deref(obj);
				obj = NULL;

				return NULL;
			}

			name_wrapper->ptr.vbo = obj;
			err = __mali_named_list_insert(buffer_object_list, name, name_wrapper);

			if (MALI_ERR_NO_ERROR != err)
			{
				/* out of memory, clean up */

				MALI_DEBUG_ASSERT(err == MALI_ERR_OUT_OF_MEMORY, ("unexpected (and incorrectly promoted) error code! (0x%X, expected 0x%X)", err, MALI_ERR_OUT_OF_MEMORY));
				_gles_buffer_object_deref(obj);
				obj = NULL;
				name_wrapper->ptr.vbo=NULL;

				_gles_wrapper_free( name_wrapper );
				name_wrapper = NULL;

				return NULL;
			}
		}
	}

	return obj;
}

GLenum _gles_bind_buffer(gles_context *ctx, GLenum target, GLuint name )
{

	gles_buffer_object  *new_obj = NULL;
	gles_buffer_object  *old_obj = NULL;
	mali_named_list 	*buffer_object_list;
	gles_vertex_array 	*vertex_array;
	GLuint               old_name;


	MALI_DEBUG_ASSERT_POINTER( ctx);
	MALI_DEBUG_ASSERT_POINTER( ctx->share_lists);
	MALI_DEBUG_ASSERT_POINTER( ctx->share_lists->vertex_buffer_object_list);
	MALI_DEBUG_ASSERT_POINTER( &ctx->state.common.vertex_array );

	buffer_object_list 	= ctx->share_lists->vertex_buffer_object_list;
	vertex_array		= &ctx->state.common.vertex_array;

	if ((target != GL_ARRAY_BUFFER) && (target != GL_ELEMENT_ARRAY_BUFFER)) MALI_ERROR( GL_INVALID_ENUM );

	_gles_vertex_array_get_binding(vertex_array, target, &old_name, &old_obj);

	/* binding the same buffer as already bound, treat as NOP */
	if ( old_name == name && old_obj != NULL )
	{
		if( old_obj->is_deleted == MALI_FALSE && _mali_sys_atomic_get(&ctx->share_lists->ref_count) < 2) return GL_NO_ERROR;
	}

	/* buffer name of 0 has a special meaning in Open GL ES to remove the binding */
	if (0 == name)
	{
		_gles_vertex_array_set_binding(vertex_array, target, 0, NULL);
		GL_SUCCESS;
	}

	/* find the buffer object to bind */
	new_obj = _gles_get_buffer_object(buffer_object_list, name);
	MALI_CHECK_NON_NULL( new_obj, GL_OUT_OF_MEMORY );

	/* set binding */
	_gles_vertex_array_set_binding(vertex_array, target, name, new_obj);

	GL_SUCCESS;
}


GLenum _gles_buffer_data(
	mali_base_ctx_handle base_ctx,
	struct gles_vertex_array *vertex_array,
	enum gles_api_version api_version,
	GLenum target,
	GLsizeiptr size,
	const GLvoid * data,
	GLenum usage)
{
	gles_buffer_object *buffer_object = NULL;
	GLuint              binding;
	struct gles_gb_buffer_object_data *old;

	MALI_DEBUG_ASSERT_POINTER( vertex_array );

	if (size < 0 ) MALI_ERROR( GL_INVALID_VALUE );
	if ((target != GL_ARRAY_BUFFER) && (target != GL_ELEMENT_ARRAY_BUFFER)) MALI_ERROR( GL_INVALID_ENUM );

	switch (usage)
	{
		case GL_STATIC_DRAW:
		case GL_DYNAMIC_DRAW:
		break;

#if MALI_USE_GLES_2
		/* GLES 2.0 supports one extra usage mode compared to GLES 1.1 */
		case GL_STREAM_DRAW:
			/* If the currently used API version is OpenGL ES 2.0 then accept the
			   usage by breaking. If not then fallthrough to the default case, which
			   returns an invalid enum error */
			IF_API_IS_GLES2(api_version)
			{
				break;
			}
#endif

		default: /* (Potential) fallthrough */
			MALI_ERROR( GL_INVALID_ENUM );
	}

	_gles_vertex_array_get_binding(vertex_array, target, &binding, &buffer_object);

	if ((NULL == buffer_object) || (0 == binding))
	{
		/* buffer name of 0 has a special meaning in Open GL ES to remove the binding */
		/* it's invalid to do anything on buffer object 0 */
		MALI_ERROR( GL_INVALID_OPERATION );
	}

	old = buffer_object->gb_data;

	_profiling_enter( INST_GL_UPLOAD_VBO_TIME);
	buffer_object->gb_data = _gles_gb_buffer_data(base_ctx, target, size, data, usage);
	_profiling_leave( INST_GL_UPLOAD_VBO_TIME);

	if (NULL == buffer_object->gb_data)
	{
		buffer_object->gb_data = old;
		MALI_ERROR( GL_OUT_OF_MEMORY );
	}

	/* free any previous data */
	if (NULL != old)
	{
		_gles_gb_free_buffer_data(old);
		old = NULL;
	}

	/* update states */
	buffer_object->usage = usage;
	buffer_object->size = size;

	GL_SUCCESS;
}


GLenum _gles_buffer_sub_data(mali_base_ctx_handle base_ctx, struct gles_vertex_array *vertex_array, GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data)
{
	gles_buffer_object *buffer_object = NULL;
	GLuint              binding;

	MALI_DEBUG_ASSERT_POINTER( vertex_array );

	if ((target != GL_ARRAY_BUFFER) && (target != GL_ELEMENT_ARRAY_BUFFER)) MALI_ERROR( GL_INVALID_ENUM );

	_gles_vertex_array_get_binding(vertex_array, target, &binding, &buffer_object);

	if ((NULL == buffer_object) || (0 == binding))
	{
		/* buffer name of 0 has a special meaning in Open GL ES to remove the binding */
		/* it's invalid to do anything on buffer object 0 */
		MALI_ERROR( GL_INVALID_OPERATION );
	}

	/* GLsizeiptr can not be negative, so checking for negative size is not needed. Strangely the GL spec
	 * says that invalid value should be returned for negative size...so we keep this check for now  */
	if (offset<0 || size<0) MALI_ERROR( GL_INVALID_VALUE );

	if ((u32)(offset + size) > buffer_object->size) MALI_ERROR( GL_INVALID_VALUE );

	if(buffer_object->gb_data != NULL && data != NULL)
	{
		struct gles_gb_buffer_object_data *new_buffer_data = _gles_gb_buffer_sub_data(base_ctx, buffer_object->gb_data, buffer_object->size, target, offset, size, data);
		if (NULL == new_buffer_data) MALI_ERROR( GL_OUT_OF_MEMORY );
		buffer_object->gb_data = new_buffer_data;
	}

	GL_SUCCESS;
}

