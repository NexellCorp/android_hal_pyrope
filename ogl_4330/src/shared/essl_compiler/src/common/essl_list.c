/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "common/essl_list.h"

void *_essl_list_new(mempool *pool, size_t size)
{
	generic_list *elem;
	assert(size >= sizeof(generic_list));
	elem = _essl_mempool_alloc(pool, size);
	if(!elem) return 0;
	elem->next = 0;
	return elem;
}


void _essl_list_insert_front(generic_list **lst, generic_list *elem) 
{ 
	elem->next = *lst; 
	*lst = elem; 
}

void _essl_list_insert_back(generic_list **lst, generic_list *elem) 
{ 
	while(*lst) lst = &((*lst)->next); 
	*lst = elem; 
}

void _essl_list_remove(generic_list **ptr) 
{ 
	if(!*ptr) return; 
	*ptr = (*ptr)->next; 
}

unsigned int _essl_list_length(generic_list *lst) {
	unsigned int l = 0;
	while (lst != 0) {
		l++;
		lst = lst->next;
	}
	return l;
}


generic_list *_essl_list_reverse(generic_list *ptr)
{
	generic_list *prev = 0;
	generic_list *next = 0;
	while(ptr)
	{
		next = ptr->next;
		ptr->next = prev;

		prev = ptr;
		ptr = next;
	}
	return prev;
}

generic_list **_essl_list_find(generic_list **lst, generic_list *elem)
{
	while (*lst != 0 && *lst != elem)
	{
		lst = &(*lst)->next;
	}
	if (*lst == elem) return lst;
	return 0;
}


static generic_list *split_and_merge(generic_list *lst, int n, generic_list **rest, 
									 int (*compare)(generic_list *e1, generic_list *e2)) {
	if (n <= 2) {
		if (n == 1) {
			*rest = lst->next;
			lst->next = 0;
			return lst;
		} else {
			generic_list *second = lst->next;
			*rest = second->next;
			if (compare(lst, second) > 0) {
				/* Swap elements */
				second->next = lst;
				lst->next = 0;
				return second;
			} else {
				/* Return in order */
				second->next = 0;
				return lst;
			}
		}
	}

	{
		generic_list *l1, *l2;
		generic_list *result, **res_ptr = &result;
		l1 = split_and_merge(lst, (n+1)/2, rest, compare);
		l2 = split_and_merge(*rest, n/2, rest, compare);

		while (l1 != 0 && l2 != 0) {
			if (compare(l1,l2) > 0) {
				*res_ptr = l2;
				res_ptr = &l2->next;
				l2 = l2->next;
			} else {
				*res_ptr = l1;
				res_ptr = &l1->next;
				l1 = l1->next;
			}
		}
		if (l1 != 0) {
			*res_ptr = l1;
		} else {
			*res_ptr = l2;
		}

		return result;
	}
}

generic_list *_essl_list_sort(generic_list *lst, int (*compare)(generic_list *e1, generic_list *e2)) {
	generic_list *dummy;
	if (lst) {
		return split_and_merge(lst, _essl_list_length(lst), &dummy, compare);
	} else {
		return 0;
	}
}



#ifdef UNIT_TEST


typedef struct _tag_test_list {
	struct _tag_test_list *next;
	int a; int b;
} test_list;
ASSUME_ALIASING(test_list, generic_list);

int compare(generic_list *l1, generic_list *l2) {
	test_list *tl1 = (test_list *)l1;
	test_list *tl2 = (test_list *)l2;
	return tl1->a - tl2->a;
}

int main()
{
	test_list *lst = 0, *elem = 0, *iter = 0;
	int i;
	int last_a;
	int expected[] = {8, 4, 2, 0, 1, 3, 5, 7, 9, 11};
	int expected_sorted[] = {0, 1, 2, 3, 4, 5, 7, 7, 9, 11};

	mempool_tracker tracker;
	mempool p, *pool = &p;

	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	if(!_essl_mempool_init(pool, 0, &tracker)) assert(0);
	for(i = 0; i < 12; ++i)
	{
		elem = LIST_NEW(pool, test_list);
		assert(elem);
		elem->a = i;
		elem->b = i+1;
		if(i&1)
		{
			LIST_INSERT_BACK(&lst, elem);
		} else {
			LIST_INSERT_FRONT(&lst, elem);
		}

	}
	LIST_REMOVE(&elem->next); /* removes nothing, since elem->next is 0 */
	LIST_REMOVE(&lst); 
	LIST_REMOVE(&lst->next);
	
	for(iter = lst, i = 0; i < 10; ++i, iter = iter->next)
	{
		assert(iter->a == expected[i]);
		assert(iter->b == expected[i]+1);
	}
	assert(!iter);
	
	lst = LIST_REVERSE(lst, test_list);
	for(iter = lst, i = 0; i < 10; ++i, iter = iter->next)
	{
		assert(iter->a == expected[10-1-i]);
		assert(iter->b == expected[10-1-i]+1);
	}
	assert(!iter);
	lst = LIST_REVERSE(lst, test_list);

	for(iter = lst, i = 0; i < 10; ++i, iter = iter->next)
	{
		assert(iter->a == expected[i]);
		assert(iter->b == expected[i]+1);
	}
	assert(!iter);

	lst->a = 7;
	lst = (test_list *)_essl_list_sort((generic_list *)lst, compare);

	for(iter = lst, i = 0; i < 10; ++i, iter = iter->next)
	{
		assert(iter->a == expected_sorted[i]);
		if (iter->next && iter->a == iter->next->a) {
			assert(iter->b == expected_sorted[i]+2);
		} else {
			assert(iter->b == expected_sorted[i]+1);
		}
	}
	assert(!iter);

	lst = LIST_REVERSE(lst, test_list);
	lst = (test_list *)_essl_list_sort((generic_list *)lst, compare);
	last_a = -1;
	for(iter = lst, i = 0; i < 10; ++i, iter = iter->next)
	{
		assert(iter->a == expected_sorted[i]);
		if (iter->a == last_a) {
			assert(iter->b == expected_sorted[i]+2);
		} else {
			assert(iter->b == expected_sorted[i]+1);
		}
		last_a = iter->a;
	}
	assert(!iter);



	_essl_mempool_destroy(pool);
	fprintf(stderr, "All tests OK!\n");

	return 0;
}

#endif /* UNIT_TEST */


