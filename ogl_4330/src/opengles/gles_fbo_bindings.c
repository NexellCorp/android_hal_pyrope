/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_base.h"
#include "gles_fbo_bindings.h"
#include "gles_framebuffer_object.h"
#include <shared/mali_surface.h>

/* A linked list of gles_fbo_bindings are placed on a texture object mipmap level or a renderbuffer object.
 * Every time a FBO is set up rendering to the object in question, this structure will store which FBOs are
 * rendering to it. When the texture object or renderbuffer is respecified through glTexImage2D or glRenderbuffer
 * Storage, the FBO in question need to be constraint dirty-flagged. By simply iterating through this list on the
 * object being respecified at the time of respecification, that is easy.
 */
typedef struct gles_fbo_binding
{
	struct gles_framebuffer_object *fbo;                   /*< The framebuffer object that this connection is referring to */
	struct gles_framebuffer_attachment* attachment_point;  /*< The attachment point this connection is referring to. */
	struct mali_surface *surface;                          /*< The attachment point surface */
} gles_fbo_binding;


MALI_CHECK_RESULT mali_linked_list *_gles_fbo_bindings_alloc( void )
{
	mali_linked_list *retval = __mali_linked_list_alloc( );

	return retval;
}

void _gles_fbo_bindings_free( mali_linked_list *list )
{
	MALI_DEBUG_ASSERT_POINTER( list );
	/* since the FBOs rendering to this texture/renderbuffer will also addref the object AND mali_surface in question,
	 * it is not possible to delete a fbo bindings list before the FBOs have been detached */
	MALI_DEBUG_ASSERT( __mali_linked_list_get_first_entry( list ) == NULL, ("list should now be empty"));

	__mali_linked_list_free( list );
}

MALI_STATIC void _gles_fbo_bindings_surface_changed_update_bindings( mali_linked_list *list )
{
	mali_linked_list_entry *entry;
	MALI_DEBUG_ASSERT_POINTER( list );

	entry = __mali_linked_list_get_first_entry(list);
	while (NULL != entry)
	{
		struct gles_fbo_binding *fbo_con = ( struct gles_fbo_binding * ) entry->data;

		mali_surface* old_surface = fbo_con->surface;
		mali_surface* new_surface = _gles_get_attachment_surface(fbo_con->attachment_point);

		/* assert that the surfaces are different; either one is null and not the other or both are not null and different */
		if(new_surface != NULL && old_surface != NULL) 
		{
			if(old_surface == new_surface) return; /* do nothing */
		}

		if( NULL != new_surface )
		{
			_mali_surface_addref(new_surface);
		}
		if( NULL != old_surface )
		{
			_mali_surface_deref(old_surface);
		}

		fbo_con->surface = new_surface;

		entry = __mali_linked_list_get_next_entry( entry );
	}
}

void _gles_fbo_bindings_surface_changed( mali_linked_list *list )
{
	_gles_fbo_bindings_surface_changed_update_bindings(list);
	_gles_fbo_bindings_flag_completeness_dirty(list);
}

void _gles_fbo_bindings_flag_completeness_dirty( mali_linked_list *list )
{
	mali_linked_list_entry *entry;
	MALI_DEBUG_ASSERT_POINTER( list );

	entry = __mali_linked_list_get_first_entry(list);
	while (NULL != entry)
	{
		struct gles_fbo_binding *fbo_con = ( struct gles_fbo_binding * ) entry->data;

		fbo_con->fbo->completeness_dirty = MALI_TRUE;
		fbo_con->attachment_point->completeness_dirty = MALI_TRUE;

		entry = __mali_linked_list_get_next_entry( entry );
	}
}

MALI_CHECK_RESULT mali_err_code _gles_fbo_bindings_add_binding( mali_linked_list *list, struct gles_framebuffer_object *fbo, struct gles_framebuffer_attachment* attachment_point )
{
	mali_err_code err = MALI_ERR_NO_ERROR;
	struct gles_fbo_binding *fbo_con = ( struct gles_fbo_binding * ) _mali_sys_malloc( sizeof( struct gles_fbo_binding ) );
	if( NULL == fbo_con ) return MALI_ERR_OUT_OF_MEMORY;

	MALI_DEBUG_ASSERT_POINTER(fbo_con);
	MALI_DEBUG_ASSERT_POINTER(attachment_point);

	fbo_con->fbo = fbo;
	fbo_con->attachment_point = attachment_point;
	fbo_con->surface = _gles_get_attachment_surface(attachment_point);

	if( NULL != fbo_con->surface )
	{
		_mali_surface_addref(fbo_con->surface);
	}

	err = __mali_linked_list_insert_data( list, ( void * ) fbo_con );
	if( MALI_ERR_NO_ERROR != err )
	{
		if ( NULL != fbo_con->surface )
		{
			_mali_surface_deref( fbo_con->surface );
		}
		_mali_sys_free( fbo_con );
	}

	return err;
}

void _gles_fbo_bindings_remove_binding( mali_linked_list *list, struct gles_framebuffer_object *fbo, struct gles_framebuffer_attachment* attachment_point )
{
	mali_linked_list_entry *entry;

	MALI_DEBUG_ASSERT_POINTER( list );
	MALI_DEBUG_ASSERT_POINTER( fbo );
	MALI_DEBUG_ASSERT_POINTER( attachment_point );

	entry = __mali_linked_list_get_first_entry(list);
	while (NULL != entry)
	{
		struct gles_fbo_binding *fbo_con = ( struct gles_fbo_binding * ) entry->data;

		if ( fbo_con->fbo == fbo && fbo_con->attachment_point == attachment_point )
		{
			fbo->completeness_dirty = MALI_TRUE;
			attachment_point->completeness_dirty = MALI_TRUE;

			if( NULL != fbo_con->surface )
			{
				mali_surface* surface = fbo_con->surface;
				_mali_surface_deref(surface);
			}

			_mali_sys_free( fbo_con );

			__mali_linked_list_remove_entry( list, entry );
			break;
		}

		entry = __mali_linked_list_get_next_entry( entry );
	}

	MALI_DEBUG_ASSERT( entry != NULL, ( "The fbo was not found in the list" ) );
}
