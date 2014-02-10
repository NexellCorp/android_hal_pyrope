/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_share_lists.h"
#include "gles_texture_object.h"
#include "gles_buffer_object.h"
#include "gles_framebuffer_object.h"
#include "gles_renderbuffer_object.h"

#include "gles_share_lists_backend.h"

/**
 * Initialize the share lists that are common between the OpenGL ES APIs
 * @param share_lists A pointer to a structure containing the share lists. This function
 *                    will initialize the lists in the structure that are common between
 *                    the OpenGL ES APIs
 * @return An error code if something went wrong, MALI_ERR_NO_ERROR otherwise
 */
MALI_STATIC mali_err_code _gles_share_lists_init(struct gles_share_lists *share_lists,  enum gles_api_version api_version)
{
	MALI_DEBUG_ASSERT_POINTER( share_lists );

	/* texture list */
	share_lists->texture_object_list = __mali_named_list_allocate();
	if(NULL == share_lists->texture_object_list) return MALI_ERR_OUT_OF_MEMORY;

	/* buffer object list */
	share_lists->vertex_buffer_object_list = __mali_named_list_allocate();
	if(NULL == share_lists->vertex_buffer_object_list) return MALI_ERR_OUT_OF_MEMORY;

	/* framebuffer object list */
	share_lists->framebuffer_object_list  = __mali_named_list_allocate();
	if(NULL == share_lists->framebuffer_object_list) return MALI_ERR_OUT_OF_MEMORY;

	/* renderbuffer object list */
	share_lists->renderbuffer_object_list  = __mali_named_list_allocate();
	if(NULL ==  share_lists->renderbuffer_object_list) return MALI_ERR_OUT_OF_MEMORY;

	/* program object list list */
	share_lists->program_object_list = __mali_named_list_allocate();
	if(NULL == share_lists->program_object_list) return MALI_ERR_OUT_OF_MEMORY;


	/* initialize ref count */
	_mali_sys_atomic_initialize(&share_lists->ref_count, 1);
	if(api_version == GLES_API_VERSION_2)
	{
		_mali_sys_atomic_initialize(&share_lists->v2_ref_count, 1);
	} else {
		_mali_sys_atomic_initialize(&share_lists->v2_ref_count, 0);
	}

#if CSTD_TOOLCHAIN_RVCT_GCC_MODE || __GNUC__ > 4 || ( __GNUC__ == 4 && __GNUC_MINOR__ > 4) || ( __GNUC__ == 4 && __GNUC_MINOR__ == 4 && __GNUC_PATCHLEVEL__ > 3)
	share_lists->lock_lists = _mali_sys_spinlock_create();
#else
	share_lists->lock_lists = _mali_sys_mutex_create();
#endif
	MALI_CHECK_NON_NULL( share_lists->lock_lists, MALI_ERR_OUT_OF_MEMORY );

	/* all is good */
	return MALI_ERR_NO_ERROR;
}

/**
 * @brief De-initializes and deletes a set of share lists
 * @param share_lists a pointer to the set of share lists to delete
 */
MALI_STATIC void _gles_share_lists_delete(struct gles_share_lists *share_lists)
{
	/* texture list */
	if (NULL != share_lists->texture_object_list)       __mali_named_list_free( share_lists->texture_object_list,       _gles_texture_object_list_entry_delete );
	share_lists->texture_object_list       = NULL;

	/* buffer object list */
	if (NULL != share_lists->vertex_buffer_object_list) __mali_named_list_free( share_lists->vertex_buffer_object_list, _gles_buffer_object_list_entry_delete );
	share_lists->vertex_buffer_object_list = NULL;

	/* framebuffer object list */
	if (NULL != share_lists->framebuffer_object_list)	__mali_named_list_free( share_lists->framebuffer_object_list, _gles_framebuffer_object_list_entry_delete );
	share_lists->framebuffer_object_list   = NULL;

	/* renderbuffer object list */
	if (NULL != share_lists->renderbuffer_object_list)	__mali_named_list_free( share_lists->renderbuffer_object_list, _gles_renderbuffer_object_list_entry_delete ); 
	share_lists->renderbuffer_object_list  = NULL;

	/* program object list */
	if (NULL != share_lists->program_object_list)
	{
		MALI_DEBUG_ASSERT(__mali_named_list_size( share_lists->program_object_list ) == 0, ("No more elements should remain"));
		__mali_named_list_free(share_lists->program_object_list, NULL ); /* no elements to delete */
	}
	share_lists->program_object_list = NULL;

	/* Free the lock */
#if CSTD_TOOLCHAIN_RVCT_GCC_MODE || __GNUC__ > 4 || ( __GNUC__ == 4 && __GNUC_MINOR__ > 4) || ( __GNUC__ == 4 && __GNUC_MINOR__ == 4 && __GNUC_PATCHLEVEL__ > 3)
	if (NULL != share_lists->lock_lists) _mali_sys_spinlock_destroy( share_lists->lock_lists );
#else
	if (NULL != share_lists->lock_lists) _mali_sys_mutex_destroy( share_lists->lock_lists );
#endif
	share_lists->lock_lists = NULL;

	/* free the structure itself */
	_mali_sys_free(share_lists);
	share_lists = NULL;
}

struct gles_share_lists *_gles_share_lists_alloc( enum gles_api_version api_version )
{
	mali_err_code err;

	struct gles_share_lists *share_lists = _mali_sys_malloc(sizeof(struct gles_share_lists));
	if (NULL == share_lists) return NULL;
	_mali_sys_memset(share_lists, 0, sizeof(struct gles_share_lists));

	err = _gles_share_lists_init(share_lists, api_version);
	if (MALI_ERR_NO_ERROR != err)
	{
		_gles_share_lists_delete(share_lists);
		share_lists = NULL;

		return NULL;
	}

	return share_lists;
}

void _gles_share_lists_addref(struct gles_share_lists *share_lists,  enum gles_api_version api_version)
{
	MALI_DEBUG_ASSERT_POINTER( share_lists );
	MALI_DEBUG_ASSERT( _mali_sys_atomic_get( &share_lists->ref_count ) > 0, ( "invalid ref count ( %d )", _mali_sys_atomic_get( &share_lists->ref_count )) );
	if( GLES_API_VERSION_2 == api_version )  _mali_sys_atomic_inc( &share_lists->v2_ref_count );
	_mali_sys_atomic_inc( &share_lists->ref_count );
}

void _gles_share_lists_deref(struct gles_share_lists *share_lists,  enum gles_api_version api_version)
{
	MALI_DEBUG_ASSERT_POINTER( share_lists );

	if( GLES_API_VERSION_2 == api_version && 0 == _mali_sys_atomic_dec_and_return( &share_lists->v2_ref_count ))
	{
		_gles_share_lists_clear_v2_content(share_lists);
	}

	if( 0 == _mali_sys_atomic_dec_and_return( &share_lists->ref_count ))
	{
		MALI_DEBUG_ASSERT( _mali_sys_atomic_get( &share_lists->v2_ref_count) == 0, ("this refcount should be zero now"));
		_gles_share_lists_delete(share_lists);
		share_lists = NULL;
	}
}

