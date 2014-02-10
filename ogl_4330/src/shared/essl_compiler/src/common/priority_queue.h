/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_PRIORITY_QUEUE
#define COMMON_PRIORITY_QUEUE

#include "common/essl_common.h"
#include "common/essl_mem.h"
#include "common/ptrdict.h"

typedef void *pq_elem_type;

typedef struct {
	int priority;
	pq_elem_type element;
} pq_element;

typedef struct {
	mempool *pool;
	unsigned int n_elements;
	unsigned int array_size;
	pq_element *array;
	ptrdict position;
} priqueue;


/** Initialize the priority queue */
memerr _essl_priqueue_init(/*@out@*/ priqueue *pq, mempool *pool);

/** Insert an element into the queue */
memerr _essl_priqueue_insert(priqueue *pq, int priority, pq_elem_type element);

/** Remove an element from the queue */
/*@null@*/ pq_elem_type _essl_priqueue_remove(priqueue *pq, pq_elem_type element);

/** Remove the element with the highest priority from the queue */
/*@null@*/ pq_elem_type _essl_priqueue_remove_first(priqueue *pq);

/** Peek at the element with the highest priority from the queue */
/*@null@*/ pq_elem_type _essl_priqueue_peek_first(priqueue *pq);

/** How many element are there in the queue? */
unsigned int _essl_priqueue_n_elements(priqueue *pq);

/** Is this element in the queue? */
essl_bool _essl_priqueue_has_element(priqueue *pq, pq_elem_type element);

/** What is the priority of this element? */
int _essl_priqueue_get_priority(priqueue *pq, pq_elem_type element);

#endif
