/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_VECTOR_H
#define COMMON_VECTOR_H

#include "common/essl_mem.h"

typedef memerr (*vector_copy_elem_fun)(mempool *pool, void *dest, void *src);

typedef struct vector
{
	mempool *pool;
	char *data;
	size_t elem_size;
	size_t n_elems;
	size_t n_elems_allocated;
	vector_copy_elem_fun copy_fun;
} vector;


/** Initialize a vector. if copy_fun is NULL, elements are copied using regular memcpy instead */
memerr _essl_vector_init(vector *v, mempool *pool, size_t elem_size, /*@null@*/ vector_copy_elem_fun copy_fun);


size_t _essl_vector_size(vector *v);
memerr _essl_vector_resize(vector *v, size_t new_n_elems);
void *_essl_vector_get_entry(vector *v, size_t idx);
void *_essl_vector_get_last_entry(vector *v);
void _essl_vector_set_entry(vector *v, size_t idx, void *data);
void *_essl_vector_append_entry_space(vector *v);
void _essl_vector_pop_last(vector *v);






#endif /* COMMON_VECTOR_H */
