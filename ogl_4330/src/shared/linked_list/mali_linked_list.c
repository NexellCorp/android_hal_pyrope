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
 * @file mali_linked_list.h
 * @brief interface for mali_linked_list
 */

#include <shared/mali_linked_list.h>
#include <mali_system.h>

/**
 * Insert an entry into a list
 * @param list the list to insert into
 * @param entry the pre-initialized entry to insert
 */
MALI_STATIC void __mali_linked_list_insert_entry( mali_linked_list *list, mali_linked_list_entry *entry );

MALI_EXPORT mali_err_code __mali_linked_list_init( mali_linked_list *list )
{
	MALI_DEBUG_ASSERT_POINTER( list );

	list->first = NULL;
	list->last  = NULL;

	list->list_mutex = _mali_sys_mutex_create();

	MALI_CHECK_NON_NULL(list->list_mutex, MALI_ERR_OUT_OF_MEMORY);

	MALI_SUCCESS;
}

MALI_EXPORT void  __mali_linked_list_deinit( mali_linked_list *list )
{
	mali_linked_list_entry *curr;
	if( (NULL == list) || (NULL == list->list_mutex) ) return;

	_mali_sys_mutex_destroy( list->list_mutex );
	list->list_mutex = NULL;

	curr = __mali_linked_list_get_first_entry( list );
	while( curr )
	{
		curr = __mali_linked_list_remove_entry( list, curr );
	}

	MALI_DEBUG_ASSERT(list->first == NULL, ("List has not been emptied correctly\n") );
	MALI_DEBUG_ASSERT(list->last  == NULL, ("List is in inconsistent state\n") );
}

MALI_EXPORT mali_linked_list* __mali_linked_list_alloc(void)
{
	mali_err_code err;
	mali_linked_list* list = _mali_sys_malloc(sizeof(mali_linked_list));
	if ( NULL == list) return NULL;

	err = __mali_linked_list_init( list );
	if ( err != MALI_ERR_NO_ERROR )
	{
		_mali_sys_free( list );
		list = NULL;
	}
	return list;
}

MALI_EXPORT void __mali_linked_list_free( mali_linked_list *list )
{
	__mali_linked_list_deinit( list );
	_mali_sys_free( list );
}


MALI_EXPORT void __mali_linked_list_empty( mali_linked_list *list, mali_linked_list_cb_func callback )
{
	mali_linked_list_entry *e;
	MALI_DEBUG_ASSERT_POINTER(list);

	e = __mali_linked_list_get_first_entry(list);
	while (NULL != e)
	{
		/* remove data */
		if (NULL != callback) callback((mali_linked_list_cb_param)e->data);
		e->data = NULL;

		/* remove entry */
		e = __mali_linked_list_remove_entry(list, e);
	}
}

MALI_STATIC void __mali_linked_list_insert_entry( mali_linked_list *list, mali_linked_list_entry *entry )
{
	MALI_DEBUG_ASSERT_POINTER( list );
	MALI_DEBUG_ASSERT_POINTER( entry );

	if (NULL == list->last)
	{
		/* this is the first entry to be inserted, add to both back and front */
		MALI_DEBUG_ASSERT(list->first == NULL, ("List is in inconsistent state\n") );
		list->last = list->first = entry;
		entry->next = NULL;
		entry->prev = NULL;
	}
	else
	{
		/* append to the end of the list */
		list->last->next = entry;
		entry->next = NULL;
		entry->prev = list->last;
		list->last = entry;
	}
}

MALI_EXPORT mali_linked_list_entry *__mali_linked_list_remove_entry( mali_linked_list *list, mali_linked_list_entry *entry )
{
	mali_linked_list_entry *next;
	MALI_DEBUG_ASSERT_POINTER( list );
	MALI_DEBUG_ASSERT_POINTER( entry );

	/* take a pointer to the next element, so we can return it */
	next = entry->next;

	/* link prev and next instead */
	if( NULL != entry->next ) entry->next->prev = entry->prev;
	if( NULL != entry->prev ) entry->prev->next = entry->next;

	/* update fist and last pointers in the list if this was one of them */
	if( list->first == entry ) list->first = entry->next;
	if( list->last  == entry ) list->last  = entry->prev;

	/* this entry is not linked any more*/
	entry->next = NULL;
	entry->prev = NULL;

	/* free entry */
	_mali_sys_free( entry );
	entry = NULL;

	/* here we return the next-pointer, so traversal can continue */
	return next;
}

MALI_EXPORT mali_linked_list_entry *__mali_linked_list_get_first_entry( mali_linked_list *list )
{
	MALI_DEBUG_ASSERT_POINTER( list );
	return list->first;
}

MALI_EXPORT mali_linked_list_entry *__mali_linked_list_get_last_entry( mali_linked_list *list )
{
	MALI_DEBUG_ASSERT_POINTER( list );
	return list->last;
}

MALI_EXPORT mali_linked_list_entry *__mali_linked_list_get_next_entry( mali_linked_list_entry *entry )
{
	MALI_DEBUG_ASSERT_POINTER( entry );
	return entry->next;
}

MALI_EXPORT mali_err_code __mali_linked_list_insert_data( mali_linked_list *list, void *data )
{
	mali_linked_list_entry *entry;
	MALI_DEBUG_ASSERT_POINTER( list );

	/* allocate an object for insertion */

	entry = _mali_sys_malloc( sizeof( mali_linked_list_entry ) );
	MALI_CHECK_NON_NULL(entry, MALI_ERR_OUT_OF_MEMORY);

	/* fill in the entry and add it */
	entry->data = data;
	__mali_linked_list_insert_entry( list, entry );

	MALI_SUCCESS;
}

MALI_EXPORT void __mali_linked_list_lock( mali_linked_list *list )
{
	MALI_DEBUG_ASSERT_POINTER( list );
	MALI_DEBUG_ASSERT_POINTER( list->list_mutex );
	_mali_sys_mutex_lock(list->list_mutex);
}

MALI_EXPORT void __mali_linked_list_unlock( mali_linked_list *list )
{
	MALI_DEBUG_ASSERT_POINTER( list );
	MALI_DEBUG_ASSERT_POINTER( list->list_mutex );
	_mali_sys_mutex_unlock(list->list_mutex);
}
