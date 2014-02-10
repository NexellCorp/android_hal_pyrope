/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_object.h"


struct gles_wrapper* _gles_wrapper_alloc(enum gles_wrappertype type)
{
	struct gles_wrapper* retval;

	retval = _mali_sys_malloc(sizeof(struct gles_wrapper));
	if(NULL == retval) return NULL;

	retval->type = type;
	retval->ptr.generic = NULL;

	return retval;
}

void _gles_wrapper_free(struct gles_wrapper* wrapper)
{
	MALI_DEBUG_ASSERT_POINTER(wrapper);

	/* before calling this function, the object held by the wrapper must
	 * be dereferenced, and the wrapper pointer must be set to NULL. */
	MALI_DEBUG_ASSERT( NULL == wrapper->ptr.generic, ("Wrapper object must be deleted by other means"));

	_mali_sys_free(wrapper);
}

static void _gles_gen_objects_cleanup( mali_named_list *list, GLsizei n, GLuint *buffers )
{
	int i;
	MALI_DEBUG_ASSERT_POINTER( list );
	for (i = 0; i < n; ++i)
	{
		struct gles_wrapper* name_wrapper = (struct gles_wrapper*)__mali_named_list_remove(list, buffers[i]);
		_gles_wrapper_free(name_wrapper);
	}
}

GLenum _gles_gen_objects( mali_named_list *list, GLsizei n, GLuint *buffers, enum gles_wrappertype type )
{
	GLsizei i;
	MALI_DEBUG_ASSERT_POINTER( list );

	/* no output parameter means we cannot return anything. Early out */
	if (NULL == buffers) return GL_NO_ERROR;

	/* n must be >= 0 */
	if (n < 0) return GL_INVALID_VALUE ;

	/* create all requested names */
	for (i = 0; i < n; ++i)
	{
		mali_err_code error_code;
		struct gles_wrapper* name_wrapper;

		u32 name = __mali_named_list_get_unused_name( list );

		/* if this happens someone has allocated 2^32 - 1 names... */
		if (0 == name)
		{
			/* remove previously allocated objects */
			_gles_gen_objects_cleanup(list, i, buffers);

			return GL_OUT_OF_MEMORY ;
		}

		/* create a wrapper to hold the future buffer object */
		name_wrapper = _gles_wrapper_alloc(type);
		if ( NULL == name_wrapper )
		{
			/* remove previously allocated objects */
			_gles_gen_objects_cleanup(list, i, buffers);

			return GL_OUT_OF_MEMORY ;
		}

		error_code = __mali_named_list_insert( list, name, name_wrapper);
		if ( MALI_ERR_NO_ERROR != error_code )
		{
			_gles_wrapper_free( name_wrapper );
			name_wrapper = NULL;

			/* remove previously allocated objects */
			_gles_gen_objects_cleanup(list, i, buffers);

			return GL_OUT_OF_MEMORY ;
		}

		buffers[ i ] = ( GLuint ) name;
	}
	return GL_NO_ERROR;

}

GLboolean _gles_is_object( mali_named_list *list, GLuint name )
{
	struct gles_wrapper *wrapper = NULL;
	GLboolean ret = GL_FALSE;
	MALI_DEBUG_ASSERT_POINTER(list);

	wrapper = __mali_named_list_get( list, name );

	/* if wrapper is NULL, the name has not been allocated - and so 'name' is not a buffer object
	 * if wrapper is non-NULL and wrapper->obj is NULL, the name has been reserved, but the buffer
	 * object has never been bound - and 'name' is not a buffer object */
	if ((NULL != wrapper) && (NULL != wrapper->ptr.generic)) ret = GL_TRUE;
	return ret;
}


