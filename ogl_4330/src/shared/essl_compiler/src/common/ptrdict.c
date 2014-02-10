/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "common/ptrdict.h"
#include "common/essl_mem.h"


/* max fill factor is DICT_MAX_FILL_MUL/(float)(1<<DICT_MAX_FILL_SHIFT) */
#define DICT_MAX_FILL_MUL 1
#define DICT_MAX_FILL_SHIFT 1


/* #define SUPER_DEBUG */

/*
  dummy key used to flag entries that have been used and now are unused.
  needed by the lookup algorithm so it still handles collisions correctly.
 */
static const void *dummy = "<dummy>";


memerr _essl_ptrdict_init(ptrdict *d, mempool *pool)
{
	unsigned size = PTRDICT_DEFAULT_SIZE;
	d->mask = size-1;
	d->log2_size = PTRDICT_DEFAULT_LOG2_SIZE;
	
	d->n_filled = 0;
	d->n_active = 0;
	d->pool = pool;
	d->first = -1;
	d->last = -1;
	ESSL_CHECK(d->entries = _essl_mempool_alloc(pool, sizeof(ptrdict_entry)*size));
	/*PROFILE_MEMORY("Dictionary", sizeof(dict_entry)*size, 0);*/
	return MEM_OK;
}

#ifdef SUPER_DEBUG
static essl_bool sanity_test(const ptrdict *d)
{
	unsigned idx;
	unsigned n_entries = 0;
	if(d->n_active == 0)
	{
		assert(d->first == -1 && d->last == -1);
	} else {
		assert(d->first != -1 && d->last != -1);
	}
	for(idx = 0; idx <= d->mask; ++idx)
	{
		ptrdict_entry *e = &d->entries[idx];
		if(e->key != 0 && e->key != dummy)
		{
			++n_entries;
			assert(e->next != idx);
			assert(e->prev != idx);
			if(e->prev == -1)
			{
				assert(d->first == idx);
			} else {
				assert(d->entries[e->prev].next == idx);
			}
			if(e->next == -1)
			{
				assert(d->last == idx);
			} else {
				assert(d->entries[e->next].prev == idx);
			}

		}

	}
	assert(n_entries == d->n_active);
	return ESSL_TRUE;
}
#endif

/*
  internal lookup function. used both for user-visible string lookups and lookups used for finding free entries. never returns 0.

 */
static int ptrdict_lookup(ptrdict *d, void *key)
{
	/* fibonacci hashing, courtesy of TAOCP */
	hash_type hash = (((hash_type)key) * 0x9e406cb5U) >> (32 - d->log2_size);
	int idx = (int)(hash & d->mask);
	int free_slot_idx = -1;
	/* 
	   need at least one empty slot for failing searches to terminate.
	   this condition ought to never happen, since dict_insert checks 
	   for dictionaries over the maximum fill factor.
	 */
	assert(key != 0);
	assert(d->n_active <= d->n_filled); 
	assert(d->n_active <= d->mask); 

	for(;;)
	{
		ptrdict_entry *e = &d->entries[idx];
		if(key == e->key)
		{
			return idx;
		}			
		if(e->key == 0)
		{
			return free_slot_idx != -1 ? free_slot_idx : idx;
		}
		
		if(free_slot_idx == -1 && e->key == dummy)
		{
			free_slot_idx = idx;
		}
		{ 
			hash_type hash2 = ((hash_type)key)>>6;
			int step = (int)(hash2 & d->mask);
			if(!(step & 1)) step = (step + 1) & d->mask;
			idx = (int)((idx+step) & d->mask);
		}

	}

}

static void ptrdict_insert(ptrdict *d, void * key, /*@null@*/ void *value)
{
	int idx = ptrdict_lookup(d, key);
	ptrdict_entry *e = &d->entries[idx];
	if(e->key == 0)
	{
		++d->n_filled;
	}
	if(e->key == 0 || e->key == dummy)
	{
		++d->n_active;
		e->next = -1;
		e->prev = -1;
		if(d->first == -1) d->first = idx;
		if(d->last != -1) 
		{ 
			d->entries[d->last].next = idx; 
			e->prev = d->last; 
		}
		d->last = idx;
	}
	e->key = key;
	e->value = value;
	
#ifdef SUPER_DEBUG
	sanity_test(d);
#endif
}

static memerr ptrdict_resize(ptrdict *d)
{
	ptrdict_entry *old_entries = d->entries;
	int i = d->first;
	unsigned log2_new_size = d->log2_size + 1;
	unsigned new_size = 1<<log2_new_size;
#ifndef NDEBUG
	unsigned int n_active = d->n_active;
#endif

#ifdef SUPER_DEBUG
	sanity_test(d);
#endif
	if(!(d->entries = _essl_mempool_alloc(d->pool, sizeof(ptrdict_entry)*new_size)))
	{
		/* out of memory, preserve the old dictionary */
		d->entries = old_entries;
		return MEM_ERROR;
	}
	/*PROFILE_MEMORY("Dictionary", sizeof(dict_entry)*new_size, 0);*/
	d->mask = new_size-1;
	d->log2_size = log2_new_size;
	d->n_active = 0; 
	d->n_filled = 0;
	d->first = -1;
	d->last = -1;
	for( ; i != -1; i = old_entries[i].next)
	{
		ptrdict_insert(d, old_entries[i].key, old_entries[i].value); 
	}
	assert(n_active == d->n_active);
#ifdef SUPER_DEBUG
	sanity_test(d);
#endif
	return MEM_OK;
}

void *_essl_ptrdict_lookup(ptrdict *d, void *key)
{
	ptrdict_entry *e = &d->entries[ptrdict_lookup(d, key)];

	assert((e->key != 0 && e->key != dummy) || e->value == 0);
	return e->value;
}

essl_bool _essl_ptrdict_has_key(ptrdict *d, void * key)
{
	ptrdict_entry *e = &d->entries[ptrdict_lookup(d, key)];
	return e->key != 0 && e->key != dummy;
}

static void ptrdict_insert_noresize(ptrdict *d, void * key, void *value)
{
	ptrdict_insert(d, key, value);
}


static memerr ptrdict_check_resize(ptrdict *d)
{
	if(d->n_filled<<DICT_MAX_FILL_SHIFT >= (d->mask+1)*DICT_MAX_FILL_MUL)
	{
		return ptrdict_resize(d);
	}
	return MEM_OK;
}


memerr _essl_ptrdict_insert(ptrdict *d, void * key, void *value)
{
	ptrdict_insert_noresize(d, key, value);
	return ptrdict_check_resize(d);	
}

int _essl_ptrdict_remove(ptrdict *d, void * key)
{
	int idx = ptrdict_lookup(d, key);
	ptrdict_entry *e = &d->entries[idx];
	if(e->key == dummy || e->key == 0) return 0; /* no such key */
	assert(dummy != 0);
	if(d->first == idx) d->first = e->next;
	if(d->last == idx) d->last = e->prev;
	if(e->next != -1) d->entries[e->next].prev = e->prev;
	if(e->prev != -1) d->entries[e->prev].next = e->next;
	e->key = (void *)dummy;
	e->value = 0;
	e->next = -1;
	e->prev = -1;
	--d->n_active;
#ifdef SUPER_DEBUG
	sanity_test(d);
#endif

	return 1;
	
}


memerr _essl_ptrdict_clear(ptrdict *d)
{
	d->n_filled = 0;
	d->n_active = 0;
	d->first = -1;
	d->last = -1;
	memset(d->entries, 0, sizeof(ptrdict_entry)*(d->mask+1));
#ifdef SUPER_DEBUG
	sanity_test(d);
#endif
	return MEM_OK;
}
unsigned _essl_ptrdict_size(ptrdict *d)
{
	return d->n_active;
}



void _essl_ptrdict_iter_init(ptrdict_iter *it, const ptrdict *d)
{
	assert(it != 0);
	assert(d != 0);
	it->dictionary = d;
	it->idx = d->first;
#ifdef SUPER_DEBUG
	sanity_test(d);
#endif
}

void * _essl_ptrdict_next(ptrdict_iter *it, void_ptr *value)
{
	if(it->idx != -1)
	{
		const ptrdict *d = it->dictionary;
		void *key;
		if(value)
		{
			*value = d->entries[it->idx].value;
		}
		key = d->entries[it->idx].key;
		it->idx = d->entries[it->idx].next;
		return key;
	}
	/* reached end of iteration */
	if(value)
	{
		*value = 0;
	}
	return 0;
	
}

void _essl_ptrdict_set_value(ptrdict_entry *entry, void *value)
{
	assert(entry != NULL);
	entry->value = value;
	return;
}

void *_essl_ptrdict_get_value(ptrdict_entry *entry)
{
	assert(entry != NULL);
	return entry->value;
}

void *_essl_ptrdict_get_key(ptrdict_entry *entry)
{
	assert(entry != NULL);
	return entry->key;
}

ptrdict_entry * _essl_ptrdict_next_entry(ptrdict_iter *it)
{
	if(it->idx != -1)
	{
		const ptrdict *d = it->dictionary;
		ptrdict_entry *entry = &d->entries[it->idx];
		it->idx = d->entries[it->idx].next;
		return entry;
	}
	return NULL;
}


void _essl_ptrdict_reverse_iter_init(ptrdict_reverse_iter *it, const ptrdict *d)
{
	assert(it != 0);
	assert(d != 0);
	it->dictionary = d;
	it->idx = d->last;
#ifdef SUPER_DEBUG
	sanity_test(d);
#endif
}

void * _essl_ptrdict_reverse_next(ptrdict_reverse_iter *it, void_ptr *value)
{
	if(it->idx != -1)
	{
		const ptrdict *d = it->dictionary;
		void *key;
		if(value)
		{
			*value = d->entries[it->idx].value;
		}
		key = d->entries[it->idx].key;
		it->idx = d->entries[it->idx].prev;
		return key;
	}
	/* reached end of iteration */
	if(value)
	{
		*value = 0;
	}
	return 0;
	
}




#ifdef UNIT_TEST

int main(void)
{
	ptrdict dictionary;
	ptrdict *d = &dictionary;
	int i, j;
	long k;
	memerr ret;
#define N_CASES 128
	void *test_keys[N_CASES];
	char *test_values[N_CASES] = {0};
	
	mempool_tracker tracker;
	mempool pool;

	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	_essl_mempool_init(&pool, 0, &tracker);
	ret = _essl_ptrdict_init(d, &pool);
	assert(ret);
	for(k = -6; k < 6; ++k)
	{
		int used_cases = N_CASES>>(k < 0? -k: k);
		for(i = 0; i < used_cases; ++i)
		{
			test_keys[i] = (void*)((k+10)*1000 + i);
			if(!test_values[i])
			{
				test_values[i] = _essl_mempool_alloc(&pool, 100);
				assert(test_values[i]);
			}
			sprintf(test_values[i], "%ldvalue%d", k, i);
		}

		for(i = 0; i < used_cases; ++i)
		{
			for(j = 0; j < i; ++j)
			{
				assert(_essl_ptrdict_lookup(d, test_keys[j]) == test_values[j]);
			}
			for(; j < used_cases; ++j)
			{
				assert(!_essl_ptrdict_has_key(d, test_keys[j]));

			}


			assert(_essl_ptrdict_size(d) == i);
			ret = _essl_ptrdict_insert(d, test_keys[i], (void*)test_values[i]);
			assert(ret);
		}

		{
			ptrdict_iter iterator, *it = &iterator;
			int c = 0;
			void *val;
			_essl_ptrdict_iter_init(it, d);
			while(_essl_ptrdict_next(it, &val))
			{
				assert(val == test_values[c]);
				++c;
			}
			assert(c == _essl_ptrdict_size(d));
		}

		{
			ptrdict_reverse_iter iterator, *it = &iterator;
			int c = _essl_ptrdict_size(d);
			void *val;
			_essl_ptrdict_reverse_iter_init(it, d);
			while(_essl_ptrdict_reverse_next(it, &val))
			{
				assert(val == test_values[c-1]);
				--c;
			}
			assert(c == 0);
		}

		for(i = 0; i < used_cases; ++i)
		{
			for(j = 0; j < i; ++j)
			{
				assert(!_essl_ptrdict_has_key(d, test_keys[j]));
			}
			for(; j < used_cases; ++j)
			{
				assert(_essl_ptrdict_lookup(d, test_keys[j]) == test_values[j]);

			}


			assert(_essl_ptrdict_size(d) == used_cases - i);
			ret = _essl_ptrdict_remove(d, test_keys[i]);
			assert(ret);
			ret = _essl_ptrdict_remove(d, test_keys[i]);
			assert(!ret);
		}

		
	}

	_essl_mempool_destroy(&pool);

	fprintf(stderr, "All tests OK!\n");
	return 0;

}


#endif /* UNIT_TEST */

