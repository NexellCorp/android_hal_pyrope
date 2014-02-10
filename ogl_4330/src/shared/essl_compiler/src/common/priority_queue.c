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
#include "common/priority_queue.h"


#define PRIQUEUE_DEFAULT_SIZE 10

static memerr updatedict(priqueue *pq, unsigned long index) {
	ESSL_CHECK(_essl_ptrdict_insert(&pq->position, pq->array[index].element, (void *)index));
	return MEM_OK;
}

static memerr swap(priqueue *pq, unsigned long i1, unsigned long i2) {
	pq_element temp = pq->array[i1];
	pq->array[i1] = pq->array[i2];
	pq->array[i2] = temp;
	ESSL_CHECK(updatedict(pq, i1));
	ESSL_CHECK(updatedict(pq, i2));
	return MEM_OK;
}

static memerr up(priqueue *pq, unsigned long idx) {
	int index = (int)idx;
	while (index > 0) {
		int parent_index = (index-1)/2;
		if (pq->array[parent_index].priority >= pq->array[index].priority) {
			return MEM_OK;
		}
		ESSL_CHECK(swap(pq, (unsigned)index, (unsigned)parent_index));
		index = parent_index;
	}
	return MEM_OK;
}

static memerr down(priqueue *pq, unsigned long index) {
	while (index*2+1 < pq->n_elements) {
		unsigned long child_index = index*2+1;
		if (child_index+1 < pq->n_elements &&
			pq->array[child_index+1].priority > pq->array[child_index].priority) {
			child_index = child_index+1;
		}
		if (pq->array[child_index].priority <= pq->array[index].priority) {
			return MEM_OK;
		}
		ESSL_CHECK(swap(pq, index, child_index));
		index = child_index;
	}
	return MEM_OK;
}

static /*@null@*/ pq_elem_type remove_index(priqueue *pq, unsigned long index) {
	pq_elem_type element;
	assert(index < pq->n_elements);
	element = pq->array[index].element;
	if (index != --pq->n_elements) {
		pq->array[index] = pq->array[pq->n_elements];
		ESSL_CHECK(updatedict(pq, index));
		ESSL_CHECK(down(pq, index));
	}

	assert(_essl_ptrdict_has_key(&pq->position, element));
	(void)_essl_ptrdict_remove(&pq->position, element);

	return element;
}

memerr _essl_priqueue_init(priqueue *pq, mempool *pool) {
	pq->pool = pool;
	pq->n_elements = 0;
	pq->array_size = PRIQUEUE_DEFAULT_SIZE;
	ESSL_CHECK(pq->array = _essl_mempool_alloc(pool, pq->array_size * sizeof(pq_element)));
	ESSL_CHECK(_essl_ptrdict_init(&pq->position, pool));
	return MEM_OK;
}

memerr _essl_priqueue_insert(priqueue *pq, int priority, pq_elem_type element) {
	if (_essl_priqueue_has_element(pq, element)) {
		ESSL_CHECK(_essl_priqueue_remove(pq, element));
	}

	if (pq->n_elements == pq->array_size) {
		unsigned int new_size = pq->array_size * 2;
		pq_element *new_array;
		ESSL_CHECK(new_array = _essl_mempool_alloc(pq->pool, new_size * sizeof(pq_element)));
		assert(new_array != 0); /* shut up lint */
		memcpy(new_array, pq->array, pq->n_elements * sizeof(pq_element));
		pq->array_size = new_size;
		pq->array = new_array;
	}

	pq->array[pq->n_elements].priority = priority;
	pq->array[pq->n_elements].element = element;
	ESSL_CHECK(updatedict(pq, pq->n_elements));
	ESSL_CHECK(up(pq, pq->n_elements));
	pq->n_elements++;

	return MEM_OK;
}

pq_elem_type _essl_priqueue_remove(priqueue *pq, pq_elem_type element) {
	assert(_essl_priqueue_has_element(pq, element));
	return remove_index(pq, (unsigned long)_essl_ptrdict_lookup(&pq->position, element));
}

pq_elem_type _essl_priqueue_remove_first(priqueue *pq) {
	return remove_index(pq, 0);
}

pq_elem_type _essl_priqueue_peek_first(priqueue *pq) {
	assert(pq->n_elements > 0);
	return pq->array[0].element;
}

unsigned int _essl_priqueue_n_elements(priqueue *pq) {
	return pq->n_elements;
}

essl_bool _essl_priqueue_has_element(priqueue *pq, pq_elem_type element) {
	return _essl_ptrdict_has_key(&pq->position, element);
}

int _essl_priqueue_get_priority(priqueue *pq, pq_elem_type element) {
	assert(_essl_priqueue_has_element(pq, element));
	return pq->array[(unsigned long)_essl_ptrdict_lookup(&pq->position, element)].priority;
}


#ifdef UNIT_TEST

int main(void) {
	mempool_tracker tracker;
	mempool pool;
	priqueue pq;
	char *a = "a", *b = "b", *c = "c", *d = "d", *e = "e";

	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	assert(_essl_mempool_init(&pool, 4096, &tracker) != 0);
	assert(_essl_priqueue_init(&pq, &pool));
	
	assert(_essl_priqueue_insert(&pq, 10, a));
	assert(_essl_priqueue_insert(&pq, 15, b));
	assert(_essl_priqueue_insert(&pq, 5, c));
	assert(_essl_priqueue_insert(&pq, 3, d));

	assert(_essl_priqueue_n_elements(&pq) == 4);
	assert(_essl_priqueue_has_element(&pq, a));
	assert(_essl_priqueue_get_priority(&pq, a) == 10);

	assert(_essl_priqueue_insert(&pq, 20, a));
	assert(_essl_priqueue_insert(&pq, 25, d));

	assert(_essl_priqueue_n_elements(&pq) == 4);
	assert(_essl_priqueue_has_element(&pq, a));
	assert(!_essl_priqueue_has_element(&pq, e));
	assert(_essl_priqueue_get_priority(&pq, a) == 20);

	assert(_essl_priqueue_insert(&pq, 23, e));
	assert(_essl_priqueue_n_elements(&pq) == 5);
	assert(_essl_priqueue_remove_first(&pq) == d);
	assert(_essl_priqueue_n_elements(&pq) == 4);
	assert(_essl_priqueue_remove(&pq, e) == e);
	assert(_essl_priqueue_n_elements(&pq) == 3);
	assert(_essl_priqueue_remove_first(&pq) == a);
	assert(_essl_priqueue_n_elements(&pq) == 2);
	assert(_essl_priqueue_remove_first(&pq) == b);
	assert(_essl_priqueue_n_elements(&pq) == 1);
	assert(_essl_priqueue_remove_first(&pq) == c);
	assert(_essl_priqueue_n_elements(&pq) == 0);

	assert(!_essl_priqueue_has_element(&pq, a));
	assert(!_essl_priqueue_has_element(&pq, b));
	assert(!_essl_priqueue_has_element(&pq, c));
	assert(!_essl_priqueue_has_element(&pq, d));
	assert(!_essl_priqueue_has_element(&pq, e));

	_essl_mempool_destroy(&pool);

	fprintf(stderr, "All tests OK!\n");
	return 0;
}

#endif
