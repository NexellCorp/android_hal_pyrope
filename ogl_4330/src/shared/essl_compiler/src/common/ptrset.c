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
#include "common/ptrset.h"
#include "common/essl_mem.h"
#include "common/ptrdict.h"



memerr _essl_ptrset_init(ptrset *s, mempool *pool)
{
	return _essl_ptrdict_init(&s->dict, pool);
}



memerr _essl_ptrset_clear(ptrset *s)
{
	return _essl_ptrdict_clear(&s->dict);
	


}

essl_bool _essl_ptrset_has(ptrset *s, void *value)
{
	return _essl_ptrdict_has_key(&s->dict, value);

}

memerr _essl_ptrset_insert(ptrset *s, void *value)
{
	return _essl_ptrdict_insert(&s->dict, value, value);
}

int _essl_ptrset_remove(ptrset *s, void *value)
{
	return _essl_ptrdict_remove(&s->dict, value);
	
}

unsigned _essl_ptrset_size(ptrset *s)
{
	return _essl_ptrdict_size(&s->dict);
}



void _essl_ptrset_iter_init(ptrset_iter *it, ptrset *s)
{
	_essl_ptrdict_iter_init(&it->it, &s->dict);
}

void *_essl_ptrset_next(ptrset_iter *it)
{
	return _essl_ptrdict_next(&it->it, 0);
	
}

void _essl_ptrset_reverse_iter_init(ptrset_reverse_iter *it, ptrset *s)
{
	_essl_ptrdict_reverse_iter_init(&it->it, &s->dict);
}

void *_essl_ptrset_reverse_next(ptrset_reverse_iter *it)
{
	return _essl_ptrdict_reverse_next(&it->it, 0);
	
}


memerr _essl_ptrset_union(ptrset *s, ptrset *b)
{
	void *val = 0;
	ptrset_iter it;
	_essl_ptrset_iter_init(&it, b);
	while((val = _essl_ptrset_next(&it)) != 0)
	{
		ESSL_CHECK(_essl_ptrset_insert(s, val));

	}
	return MEM_OK;

}

memerr _essl_ptrset_difference(ptrset *s, ptrset *b)
{
	void *val = 0;
	ptrset_iter it;
	_essl_ptrset_iter_init(&it, b);
	while((val = _essl_ptrset_next(&it)) != 0)
	{
		(void)_essl_ptrset_remove(s, val);

	}
	return MEM_OK;

}

essl_bool _essl_ptrset_is_subset(ptrset *subset, ptrset *set)
{
	void *val = 0;
	ptrset_iter it;
	(void)_essl_ptrset_iter_init(&it, subset);
	while((val = _essl_ptrset_next(&it)) != 0)
	{
		if(!_essl_ptrset_has(set, val)) return ESSL_FALSE;

	}
	return ESSL_TRUE;


}

essl_bool _essl_ptrset_equal(ptrset *a, ptrset *b)
{
	return _essl_ptrset_is_subset(a, b) && _essl_ptrset_is_subset(b, a);
}




#ifdef UNIT_TEST


void fill_set(ptrset *s, void **data)
{
	_essl_ptrset_clear(s);
	while(*data)
	{
		_essl_ptrset_insert(s, *data++);

	}
}


int main(void)
{
	ptrset set;
	ptrset *s = &set;
	int i, j;
	long k;
	memerr ret;
#define N_CASES 128
	void  *test_cases[N_CASES];
	
	mempool_tracker tracker;
	mempool pool;

	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	_essl_mempool_init(&pool, 0, &tracker);
	ret = _essl_ptrset_init(s, &pool);
	assert(ret);
	for(k = -6; k < 6; ++k)
	{
		int used_cases = N_CASES>>(k < 0? -k: k);
		for(i = 0; i < used_cases; ++i)
		{
			test_cases[i] = (void *)((k+10)*10000 + i);
		}

		for(i = 0; i < used_cases; ++i)
		{
			for(j = 0; j < i; ++j)
			{
				assert(_essl_ptrset_has(s, test_cases[j]));
			}
			for(; j < used_cases; ++j)
			{
				assert(!_essl_ptrset_has(s, test_cases[j]));

			}


			assert(_essl_ptrset_size(s) == i);
			ret = _essl_ptrset_insert(s, test_cases[i]);
			assert(ret);
		}

		{
			ptrset_iter iterator, *it = &iterator;
			int c = 0;
			_essl_ptrset_iter_init(it, s);
			while(_essl_ptrset_next(it))
			{
				++c;
			}
			assert(c == _essl_ptrset_size(s));
		}

		{
			ptrset_reverse_iter iterator, *it = &iterator;
			int c = 0;
			_essl_ptrset_reverse_iter_init(it, s);
			while(_essl_ptrset_reverse_next(it))
			{
				++c;
			}
			assert(c == _essl_ptrset_size(s));
		}

		for(i = 0; i < used_cases; ++i)
		{
			for(j = 0; j < i; ++j)
			{
				assert(!_essl_ptrset_has(s, test_cases[j]));
			}
			for(; j < used_cases; ++j)
			{
				assert(_essl_ptrset_has(s, test_cases[j]));

			}


			assert(_essl_ptrset_size(s) == used_cases - i);
			ret = _essl_ptrset_remove(s, test_cases[i]);
			assert(ret);
			ret = _essl_ptrset_remove(s, test_cases[i]);
			assert(!ret);
		}

		
	}


	/* okay, basic insertion and removal works, now test set union and difference */
	{
		void *data1[] = {(void *)1,(void *)2,(void *)3,(void *)4,(void *)5,(void *)0 };
	    void *data2[] = {(void *)2,(void *)4,(void *)6,(void *)8,(void *)10,(void *)0 };
		void *union_data[] = {(void *)1,(void *)2,(void *)3,(void *)4,(void *)5, (void *)6, (void *)8, (void *)10, (void *)0};

		void *diff_data[] = {(void*)1, (void*)3, (void*)5, (void*)0};
		ptrset a, b, c;
		_essl_ptrset_init(&a, &pool);
		_essl_ptrset_init(&b, &pool);
		_essl_ptrset_init(&c, &pool);

		fill_set(&a, data1);
		fill_set(&b, data2);
		fill_set(&c, union_data);
		_essl_ptrset_union(&a, &b);
		assert(_essl_ptrset_equal(&a, &c));

		fill_set(&a, data1);
		fill_set(&b, data2);
		fill_set(&c, diff_data);
		_essl_ptrset_difference(&a, &b);
		assert(_essl_ptrset_equal(&a, &c));

	}
	

	_essl_mempool_destroy(&pool);

	fprintf(stderr, "All tests OK!\n");
	return 0;

}


#endif /* UNIT_TEST */

