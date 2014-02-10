/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_LIST_H
#define COMMON_LIST_H

#include "common/essl_system.h"
#include "common/essl_mem.h"
#include "common/essl_common.h"

/**
   generic list implementation.

*/

typedef struct _tag_generic_list generic_list;
typedef /*@null@*/ struct _tag_generic_list *generic_list_ptr;
struct _tag_generic_list {
	generic_list *next;
};

#define ASSUME_ALIASING(NAME1, NAME2) \
union _tag_##NAME1##_##NAME2##_tell_the_compiler_that_aliasing_is_possible { \
	NAME1 n1; \
	NAME2 n2; \
}


/*@null@*/ void *_essl_list_new(mempool *pool, size_t size);
void _essl_list_insert_front(generic_list_ptr *lst, generic_list *elem);
void _essl_list_insert_back(generic_list_ptr *lst, generic_list *elem);
void _essl_list_remove(generic_list_ptr *ptr);
unsigned int _essl_list_length(generic_list *lst);
/*@null@*/ generic_list *_essl_list_reverse(generic_list *ptr);
generic_list *_essl_list_sort(generic_list *lst, int (*compare)(generic_list *e1, generic_list *e2));
generic_list **_essl_list_find(generic_list_ptr *lst, generic_list *elem);


/** macros so we don't have to cast to generic_list pointers manually */

/** Create a new list element */
#define LIST_NEW(pool, type) 	((type *)_essl_list_new((pool), sizeof(type)))

/** Insert the given list element at the front of the list */
#define LIST_INSERT_FRONT(lst, elm) _essl_list_insert_front((generic_list_ptr *)(void*)(lst), (generic_list*)(elm))

/** Insert the given list element at the back of the list */
#define LIST_INSERT_BACK(lst, elm) _essl_list_insert_back((generic_list_ptr *)(void*)(lst), (generic_list*)(elm))

/** Remove a list element, given by the double pointer pointing at it from the previous element */
#define LIST_REMOVE(prev) _essl_list_remove((generic_list_ptr *)(void*)(prev))

/** In-place reversal of a list */
#define LIST_REVERSE(ptr, type) ((type*)_essl_list_reverse((generic_list *)(ptr)))

/** Sort a list */
#define LIST_SORT(lst, comp, type) ((type*)_essl_list_sort((generic_list*)(lst), (comp)))

/** Find an element in a list */
#define LIST_FIND(lst, elm, type) ((type**)(void*)_essl_list_find((generic_list**)(void*)(lst), (generic_list*)(elm)))

/** Returns the list length*/
#define LIST_LENGTH(lst) _essl_list_length((generic_list*)(void*)(lst))

#endif /* COMMON_LIST_H */


