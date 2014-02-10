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
#include "common/essl_vector.h"


memerr _essl_vector_init(vector *v, mempool *pool, size_t elem_size, /*@null@*/ vector_copy_elem_fun copy_fun)
{
	size_t default_start_alloc_elems = 4;
	v->pool = pool;
	v->elem_size = elem_size;
	v->copy_fun = copy_fun;
	v->n_elems_allocated = default_start_alloc_elems;
	ESSL_CHECK(v->data = _essl_mempool_alloc(v->pool, v->n_elems_allocated*v->elem_size));
	v->n_elems = 0;
	return MEM_OK;
}
size_t _essl_vector_size(vector *v)
{
	return v->n_elems;
}
memerr _essl_vector_resize(vector *v, size_t new_n_elems)
{
	if(new_n_elems > v->n_elems_allocated)
	{
		/* okay, need more space */
		char *new_data;
		size_t new_n_elems_to_alloc = new_n_elems;
		if(new_n_elems_to_alloc < 2*v->n_elems) new_n_elems_to_alloc = 2*v->n_elems;

		ESSL_CHECK(new_data = _essl_mempool_alloc(v->pool, new_n_elems_to_alloc*v->elem_size));
		if(v->copy_fun != NULL)
		{
			size_t i;
			for(i = 0; i < v->n_elems; ++i)
			{
				ESSL_CHECK(v->copy_fun(v->pool, (char*)new_data + (i*v->elem_size), (char*)v->data + (i*v->elem_size)));
			}
		} else {
			memcpy(new_data, v->data, v->n_elems*v->elem_size);
		}
		v->n_elems_allocated = new_n_elems_to_alloc;
		v->data = new_data;
	}
	v->n_elems = new_n_elems;
	return MEM_OK;
}

void _essl_vector_pop_last(vector *v)
{
	assert(v->n_elems > 0);
	--v->n_elems;
}

void *_essl_vector_get_entry(vector *v, size_t idx)
{
	assert(idx < v->n_elems);
	return &v->data[idx*v->elem_size];
}

void *_essl_vector_get_last_entry(vector *v)
{
	return _essl_vector_get_entry(v, v->n_elems-1);
}
void _essl_vector_set_entry(vector *v, size_t idx, void *data)
{
	void *ptr;
	ptr = _essl_vector_get_entry(v, idx);
	memmove(ptr, data, v->elem_size);
	
	return;
}

void *_essl_vector_append_entry_space(vector *v)
{
	size_t new_size = _essl_vector_size(v)+1;
	ESSL_CHECK(_essl_vector_resize(v, new_size));
	return _essl_vector_get_entry(v, new_size-1);
}



#ifdef UNIT_TEST

int main()
{
	memerr err;
	mempool_tracker tracker;
	mempool pool;
	vector int_vec;
	int *tmp;
	int i, j;
	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	err = _essl_mempool_init(&pool, 0, &tracker);
	assert(err == MEM_OK);
	err = _essl_vector_init(&int_vec, &pool, sizeof(int), NULL);
	assert(err == MEM_OK);
	assert(_essl_vector_size(&int_vec) == 0);
	
	for(j = 0; j < 17; ++j)
	{
		err = _essl_vector_resize(&int_vec, 0);
		assert(err == MEM_OK);
		assert(_essl_vector_size(&int_vec) == 0);
		for(i = 0; i < j; ++i)
		{
			tmp = _essl_vector_append_entry_space(&int_vec);
			assert(tmp != NULL);
			*tmp = i;
		}
		assert(_essl_vector_size(&int_vec) == j);
		for(i = 0; i < j; ++i)
		{
			tmp = _essl_vector_get_entry(&int_vec, i);
			assert(tmp != NULL);
			assert(*tmp == i);
		}

	}
	err = _essl_vector_resize(&int_vec, 24);
	assert(err == MEM_OK);
	assert(_essl_vector_size(&int_vec) == 24);

	err = _essl_vector_resize(&int_vec, 124);
	assert(err == MEM_OK);
	assert(_essl_vector_size(&int_vec) == 124);

	err = _essl_vector_resize(&int_vec, 12);
	assert(err == MEM_OK);
	assert(_essl_vector_size(&int_vec) == 12);


	err = _essl_vector_resize(&int_vec, 23);
	assert(err == MEM_OK);
	assert(_essl_vector_size(&int_vec) == 23);

	for(i = 12; i < 23; ++i)
	{
		int v = i*2;
		_essl_vector_set_entry(&int_vec, i, &v);
	}
	for(i = 0; i < 12; ++i)
	{
		tmp = _essl_vector_get_entry(&int_vec, i);
		assert(tmp != NULL);
		assert(*tmp == i);
	}
	for(i = 12; i < 23; ++i)
	{
		tmp = _essl_vector_get_entry(&int_vec, i);
		assert(tmp != NULL);
		assert(*tmp == i*2);
	}
	for(i = 0; i < 9; ++i)
	{
		_essl_vector_pop_last(&int_vec);
	}
	assert(_essl_vector_size(&int_vec) == 23-9);

	printf("All tests OK!\n");
	_essl_mempool_destroy(&pool);
	return 0;
}


#endif
