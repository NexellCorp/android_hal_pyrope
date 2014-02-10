/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_share_lists.h
 * @brief Contains structure and function prototypes for shared data.
 * */
#ifndef _GLES_SHARE_LISTS_H_
#define _GLES_SHARE_LISTS_H_

#include <base/mali_types.h>
#include "gles_base.h"
#include "gles_context.h"
#include "shared/mali_named_list.h"

/** The structure holding the data that might be shared between different gles context */
struct gles_share_lists
{
	mali_atomic_int ref_count;

	mali_named_list *texture_object_list;               /**< The list of the generated textures */
	mali_named_list *vertex_buffer_object_list;         /**< The list of the generated buffers */

	/* Share lists that are only relevant to the OpenGL ES 2.x API.
	 * These lists are emptied when the amount of v2 refcounts reach zero.
	*/
	mali_atomic_int v2_ref_count;
	mali_named_list *framebuffer_object_list;           /**< The list of framebuffer-objects */
	mali_named_list *renderbuffer_object_list;          /**< The list of renderbuffer-objects */
	mali_named_list *program_object_list;               /**< The list of shader-objects */

#if CSTD_TOOLCHAIN_RVCT_GCC_MODE || __GNUC__ > 4 || ( __GNUC__ == 4 && __GNUC_MINOR__ > 4) || ( __GNUC__ == 4 && __GNUC_MINOR__ == 4 && __GNUC_PATCHLEVEL__ > 3)
	mali_spinlock_handle lock_lists;
#else
	mali_mutex_handle lock_lists;					/**< The shared mutex to lock all lists */
#endif

};

/**
 * @brief Allocates a new set of share lists
 * @param the API version to allocate a shared list for
 * @return a pointer to the set of share lists
 */
struct gles_share_lists *_gles_share_lists_alloc( enum gles_api_version api_version );

/**
 * @brief Add a reference to a set of share lists
 * @param share_lists a pointer to the set of share lists to add the reference to
 **/
void _gles_share_lists_addref(struct gles_share_lists *share_lists, enum gles_api_version api_version);

/**
 * @brief Remove a reference from  a set of share lists and if the refcount reaches zero
 *        then this function will also delete that list.
 * @param share_lists a pointer to the set of share lists to remove the reference from
 * @note If you need to deref a list for an OpenGL ES 2.x context then use the function
 *       _gles2_share_lists_deref in gles2_share_lists.h instead
 */
void _gles_share_lists_deref(struct gles_share_lists *share_lists, enum gles_api_version api_version);

/**
 * @brief Locks the share-lists
 * @param share_lists a pointer to the set of share lists to lock
 */
MALI_STATIC_FORCE_INLINE void _gles_share_lists_lock( struct gles_share_lists *share_lists )
{
#if CSTD_TOOLCHAIN_RVCT_GCC_MODE || __GNUC__ > 4 || ( __GNUC__ == 4 && __GNUC_MINOR__ > 4) || ( __GNUC__ == 4 && __GNUC_MINOR__ == 4 && __GNUC_PATCHLEVEL__ > 3)
	_mali_sys_spinlock_lock( share_lists->lock_lists );
#else
	_mali_sys_mutex_lock( share_lists->lock_lists );
#endif
}

/**
 * @brief Unlocks the share lists
 * @param share_lists a pointer to the set of share lists to unlock
 */
MALI_STATIC_FORCE_INLINE void _gles_share_lists_unlock( struct gles_share_lists *share_lists )
{
#if CSTD_TOOLCHAIN_RVCT_GCC_MODE || __GNUC__ > 4 || ( __GNUC__ == 4 && __GNUC_MINOR__ > 4) || ( __GNUC__ == 4 && __GNUC_MINOR__ == 4 && __GNUC_PATCHLEVEL__ > 3)
	_mali_sys_spinlock_unlock( share_lists->lock_lists );
#else
	_mali_sys_mutex_unlock( share_lists->lock_lists );
#endif
}

#endif /* _GLES_SHARE_LISTS_H_ */
