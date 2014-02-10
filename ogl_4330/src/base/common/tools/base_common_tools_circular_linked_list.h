/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */


#ifndef _BASE_COMMON_TOOLS_CIRCULAR_LINKED_LIST_H_
#define _BASE_COMMON_TOOLS_CIRCULAR_LINKED_LIST_H_

#include <stddef.h>
#include <base/mali_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file base_common_tools_circular_linked_list.h
 *
 * Circular linked list support
 * The circular linked list(CLL) module supports creating CLLs of user defined types.
 * This is done by embedding a mali_embedded_list_link object inside the struct which wants to be linked.
 * Example:
 * struct user_struct
 * {
 * 	int normal_member;
 *  mali_embedded_list_link link_member;
 *  int other_member;
 * };
 *
 * Then the user_struct can be included on a CLL.
 * The head of a CLL is usually just a plain mali_embedded_list_link, example:
 *
 * struct data_tracker
 * {
 *    mali_embedded_list_link head_for_list_of_objects;
 * };
 *
 * The a user_struct object can be added to this list
 *
 * _mali_embedded_list_insert_after(&data_tracker_object.head_for_list_of_objects, &new_member->link_member);
 *
 * To extract an object afterwards one can use the MALI_EMBEDDED_LIST_GET_CONTAINER macro
 *
 * user_struct * pObject = MALI_EMBEDDED_LIST_GET_CONTAINER(user_struct, link_member, _mali_embedded_list_get_next(&data_tracker_object.head_for_list_of_objects));
 *
 * Or if walking the whole list:
 *
 * user_struct * pObject;
 *
 * MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(pObject, &data_tracker_object.head_for_list_of_objects, user_struct, link_member)
 * {
 *    ... For each pObject in the list the body will be evaluated...
 * }
 */

/**
 * Definition of the link type.
 * Only contains the next and prev pointers
 */
typedef struct mali_embedded_list_link
{
	struct mali_embedded_list_link * next;
	struct mali_embedded_list_link * prev;
} mali_embedded_list_link;

/**
 * Initialize the head of a list
 * Usage:
 * MALI_EMBEDDED_LIST_HEAD_INIT_DYN(head);
 */
#define MALI_EMBEDDED_LIST_HEAD_INIT_RUNTIME(list) do { (list)->next = (list); (list)->prev = (list); } while (0)

/**
 * Initialize a list
 * Usage:
 * mali_embedded_list_link head = LIST_HEAD_INIT(head);
 */
#define MALI_EMBEDDED_LIST_HEAD_INIT_COMPILETIME(list) { &(list), &(list) }

/**
 * Get container object of given link
 * Usage my_type * ptr = MALI_EMBEDDED_LIST_GET_CONTAINER(my_type, link_member_in_type, a_link_entry);
 */
/*
	Can't wrap all macro arguments in () because it'll generate invalid code
	OK since most of them are supposed to be type names and not statements.
*/
#define MALI_EMBEDDED_LIST_GET_CONTAINER(container_type, member, ptr) ((container_type*)(((u8*)(ptr)) - offsetof(container_type, member)))

/**
 * Loop over all items in a list.
 * In the compound statement following the macro the entry variable points to the given entry of requested type
 * Usage:
 * MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(my_ptr, my_list_head, my_type, link_member_in_type)
 * {
 *    ...do something with entry pointed to by my_ptr...
 * }
 */
#define MALI_EMBEDDED_LIST_FOR_EACH_ENTRY(entry, head, container_type, member) \
 	for ( \
	(entry) = MALI_EMBEDDED_LIST_GET_CONTAINER(container_type, member, (head)->next); \
	&entry->member != (head); \
	(entry) = MALI_EMBEDDED_LIST_GET_CONTAINER(container_type, member, (entry)->member.next) \
	)

#define MALI_EMBEDDED_LIST_FOR_EACH_ENTRY_REVERSE(entry, head, container_type, member) \
 	for ( \
	(entry) = MALI_EMBEDDED_LIST_GET_CONTAINER(container_type, member, (head)->prev); \
	&entry->member != (head); \
	(entry) = MALI_EMBEDDED_LIST_GET_CONTAINER(container_type, member, (entry)->member.prev) \
	)

#define MALI_EMBEDDED_LIST_FOR_EACH_ENTRY_SAFE(entry, temp, head, container_type, member) \
	for ( \
	(entry) = MALI_EMBEDDED_LIST_GET_CONTAINER(container_type, member, (head)->next), (temp) = (head)->next->next; \
	&entry->member != (head); \
	(entry) = MALI_EMBEDDED_LIST_GET_CONTAINER(container_type, member, (temp)), (temp) = (temp)->next \
	)

#define MALI_EMBEDDED_LIST_FOR_EACH_ENTRY_SAFE_REVERSE(entry, temp, head, container_type, member) \
	for ( \
	(entry) = MALI_EMBEDDED_LIST_GET_CONTAINER(container_type, member, (head)->prev), (temp) = (head)->prev->prev; \
	&entry->member != (head); \
	(entry) = MALI_EMBEDDED_LIST_GET_CONTAINER(container_type, member, (temp)), (temp) = (temp)->prev \
	)

/**
 * Check whether the list is empty.
 * Returns true if outward pointers from link only points to itself
 */
MALI_STATIC_FORCE_INLINE mali_bool _mali_embedded_list_is_empty(mali_embedded_list_link* link)
{
	/*check_link(link);*/
	/* if link->next and link->prev points to the same or NULL, list is considered empty */
	return (((link->next == link) && (link->prev == link)) || ((link->next == NULL) && (link->prev == NULL)));
}

/**
 * Check if the given link has an entry behind it.
 */
MALI_STATIC_FORCE_INLINE mali_bool _mali_embedded_list_has_next(mali_embedded_list_link * head, mali_embedded_list_link * link)
{
	/*check_link(link);*/
	return (link->next != head);
}

/**
 * Check if the given link has an entry in front of it.
 */
MALI_STATIC_FORCE_INLINE mali_bool _mali_embedded_list_has_prev(mali_embedded_list_link * head, mali_embedded_list_link * link)
{
	/*check_link(link);*/
	return (link->prev != head);
}

/**
 * Return the entry behind given link.
 * Entry must have a next (@see _mali_embedded_list_has_next).
 */
MALI_STATIC_FORCE_INLINE mali_embedded_list_link * _mali_embedded_list_get_next(mali_embedded_list_link * link)
{
	/*check_link(link);*/
	return link->next;
}

/**
 * Return the entry in fron of given link.
 * Entry must have a prev (@see _mali_embedded_list_has_prev).
 */
MALI_STATIC_FORCE_INLINE mali_embedded_list_link * _mali_embedded_list_get_prev(mali_embedded_list_link * link)
{
	/*check_link(link);*/
	return link->prev;
}

/**
 * Insert a new entry behind the given link
 */
MALI_STATIC_FORCE_INLINE void _mali_embedded_list_insert_after(mali_embedded_list_link * head, mali_embedded_list_link * new_entry)
{
	/*check_link(head);*/
	MALI_DEBUG_ASSERT((new_entry->next == NULL) && (new_entry->prev == NULL), ("List insert of dirty element"));

	new_entry->prev = head;
	new_entry->next = head->next;
	head->next = new_entry;
	new_entry->next->prev = new_entry;

	/*check_link(head);
	check_link(new_entry);*/
}

/**
 * Insert a new entry before the given link
 */
MALI_STATIC_FORCE_INLINE void _mali_embedded_list_insert_before(mali_embedded_list_link * head, mali_embedded_list_link * new_entry)
{
	/*check_link(head);*/
	MALI_DEBUG_ASSERT((new_entry->next == NULL) && (new_entry->prev == NULL), ("List insert of dirty element 0x%X: next = 0x%X, prev = 0x%X", new_entry, new_entry->next, new_entry->prev));

	new_entry->next = head;
	new_entry->prev = head->prev;
	head->prev = new_entry;
	new_entry->prev->next = new_entry;

	/*check_link(head);
	check_link(new_entry);*/
}

/**
 * Insert a new entry at tail of list given by head.
 * Just an alias for _mali_embedded_list_insert_before(head, new_entry);
 */
MALI_STATIC_FORCE_INLINE void _mali_embedded_list_insert_tail(mali_embedded_list_link * head, mali_embedded_list_link * new_entry)
{
	/* just an alias */
	_mali_embedded_list_insert_before(head, new_entry);
}

/**
 * Remove an entry from the list it belongs to.
 */
MALI_STATIC_FORCE_INLINE void _mali_embedded_list_remove(mali_embedded_list_link * link)
{
	MALI_DEBUG_ASSERT_POINTER(link);
	MALI_DEBUG_ASSERT_POINTER(link->next);
	MALI_DEBUG_ASSERT_POINTER(link->prev);
	/*check_link(link);*/

	link->next->prev = link->prev;
	link->prev->next = link->next;

	link->next = link->prev = (mali_embedded_list_link *) NULL;
}

#ifdef __cplusplus
}
#endif

#endif /*_BASE_COMMON_TOOLS_CIRCULAR_LINKED_LIST_H_*/
