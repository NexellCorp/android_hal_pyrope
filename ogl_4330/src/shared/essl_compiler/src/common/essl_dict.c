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
#include "common/essl_dict.h"
#include "common/essl_mem.h"

/* max fill factor is DICT_MAX_FILL_MUL/(float)(1<<DICT_MAX_FILL_SHIFT) */
#define DICT_MAX_FILL_MUL 5
#define DICT_MAX_FILL_SHIFT 3

#define mem_free(s)  /* no memory freeing done */

/*
  dummy key used to flag entries that have been used and now are unused.
  needed by the lookup algorithm so it still handles collisions correctly.
 */
static const string dummy = { "<dummy>", 7 };


/*
  must be anything *but* the hash value of dummy, since we should never return a dummy entry from the lookup function.
 */
#define DICT_DUMMY_HASH_VAL 0

memerr _essl_dict_init(dict *d, mempool *pool)
{
	unsigned size = DICT_DEFAULT_SIZE;

	d->n_filled = 0;
	d->n_active = 0;
	d->mask = size-1;
	d->pool = pool;
	ESSL_CHECK(d->entries = _essl_mempool_alloc(pool, sizeof(dict_entry)*size));
	/*PROFILE_MEMORY("Dictionary", sizeof(dict_entry)*size, 0);*/
	return MEM_OK;
}



static hash_type str_hash(string key)
{
	int i;
	hash_type hash = 1337;
	assert(key.ptr != 0);
	for (i = 0; i < key.len; ++i)
	{
		hash = hash*5 + (unsigned)key.ptr[i];
	}
	return hash;
}


/*
  internal lookup function. used both for user-visible string lookups and lookups used for finding free entries. never returns 0.

 */
static dict_entry *lookup(dict *d, string key, hash_type hash)
{
	unsigned idx = hash & d->mask;
	dict_entry *free_slot = 0;
	/* 
	   need at least one empty slot for failing searches to terminate.
	   this condition ought to never happen, since dict_insert checks 
	   for dictionaries over the maximum fill factor.
	 */
	assert(d->n_active <= d->n_filled); 
	assert(d->n_active <= d->mask); 
	
	for(;;)
	{
		dict_entry *e = &d->entries[idx];
		if(hash == e->hash
		   && (key.ptr == e->key.ptr /* in case of interned strings or pointer compares */
			   || (key.len >= 0 && e->key.len >= 0 && !_essl_string_cmp(key, e->key)))) /* make sure that we are dealing with strings before attempting a string compare */
		{
			return e;
		}			
		if(e->key.ptr == 0)
		{
			return free_slot ? free_slot : e;
		}
		
		if(!free_slot && e->key.ptr == dummy.ptr)
		{
			free_slot = e;
		}
		idx = (idx+1) & d->mask; /* if a better collision handling is desired, this is the place to change */

	}

}

static void insert(dict *d, string key, hash_type hash, /*@null@*/ void *value)
{
	dict_entry *e = lookup(d, key, hash);
	if(e->key.ptr == 0)
	{
		++d->n_filled;
	}
	if(e->key.ptr == 0 || e->key.ptr == dummy.ptr)
	{
		++d->n_active;
	}
	e->key = key;
	e->hash = hash;
	e->value = value;


}

static memerr resize(dict *d, unsigned new_size)
{
	dict_entry *old_entries = d->entries;
	unsigned old_mask = d->mask;
	unsigned int i;
	assert((new_size&(new_size-1)) == 0); /* new_size must be power of two */
	if(!(d->entries = _essl_mempool_alloc(d->pool, sizeof(dict_entry)*new_size)))
	{
		/* out of memory, preserve the old dictionary */
		d->entries = old_entries;
		return MEM_ERROR;
	}
	/*PROFILE_MEMORY("Dictionary", sizeof(dict_entry)*new_size, 0);*/
	d->mask = new_size-1;
	d->n_active = 0; 
	d->n_filled = 0;
	for(i = 0; i <= old_mask; ++i)
	{
		if(old_entries[i].key.ptr != 0 && old_entries[i].key.ptr != dummy.ptr)
		{
			insert(d, old_entries[i].key, old_entries[i].hash, old_entries[i].value); 
		}
	}
	mem_free(old_entries);
	return MEM_OK;
}

void *_essl_dict_lookup(dict *d, string key)
{
	dict_entry *e = lookup(d, key, str_hash(key));

	assert((e->key.ptr != 0 && e->key.ptr != dummy.ptr) || e->value == 0);
	return e->value;
}

essl_bool _essl_dict_has_key(dict *d, string key)
{
	dict_entry *e = lookup(d, key, str_hash(key));
	return e->key.ptr != 0 && e->key.ptr != dummy.ptr;
}

static void dict_insert_noresize(dict *d, string key, void *value)
{
	hash_type hash = str_hash(key);
	insert(d, key, hash, value);
}

static unsigned dict_resize_size(dict *d)
{
	if(d->n_filled<<DICT_MAX_FILL_SHIFT >= (d->mask+1)*DICT_MAX_FILL_MUL)
		return (d->mask+1)*2;
	else
		return 0;
}

static memerr dict_check_resize(dict *d)
{
	unsigned new_size = dict_resize_size(d);
	if(new_size)
	{
		return resize(d, new_size);
	}
	return MEM_OK;
}


memerr _essl_dict_insert(dict *d, string key, void *value)
{
	dict_insert_noresize(d, key, value);
	return dict_check_resize(d);	
}

int _essl_dict_remove(dict *d, string key)
{
	hash_type hash = str_hash(key);
	dict_entry *e = lookup(d, key, hash);
	if(e->key.ptr == dummy.ptr || e->key.ptr == 0) return 0; /* no such key */
	assert(dummy.ptr != 0);
	e->key = dummy;
	e->hash = DICT_DUMMY_HASH_VAL;
	e->value = 0;
	--d->n_active;
	return 1;
	
}


memerr _essl_dict_clear(dict *d)
{
	d->n_filled = 0;
	d->n_active = 0;
	memset(d->entries, 0, sizeof(dict_entry)*(d->mask+1));
	return MEM_OK;
}
unsigned _essl_dict_size(dict *d)
{
	return d->n_active;
}



void _essl_dict_iter_init(dict_iter *it, const dict *d)
{
	assert(it != 0);
	assert(d != 0);
	it->dictionary = d;
	it->idx = 0;
}

string _essl_dict_next(dict_iter *it, void_ptr *value)
{
	string empty;
	const dict *d = it->dictionary;
	for(; it->idx <= d->mask && !(d->entries[it->idx].key.ptr != 0 && d->entries[it->idx].key.ptr != dummy.ptr); ++it->idx)
	{
	}
	if(it->idx <= d->mask)
	{
		if(value)
		{
			*value = d->entries[it->idx].value;
		}
		return d->entries[it->idx++].key;

	}

	/* reached end of iteration */
	if(value)
	{
		*value = 0;
	}
	empty.ptr = 0;
	empty.len = 0;
	return empty;
	
}




#ifdef UNIT_TEST

int main(void)
{
	dict dictionary;
	dict *d = &dictionary;
	int i, j, k;
	int ret;
#define N_CASES 128
	string test_keys[N_CASES];
	char *test_values[N_CASES] = {0};

	mempool_tracker tracker;
	mempool pool;

	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	_essl_mempool_init(&pool, 0, &tracker);
	ret = _essl_dict_init(d, &pool);
	assert(ret);
	for(k = -6; k < 6; ++k)
	{
		int used_cases = N_CASES>>(k < 0? -k: k);
		for(i = 0; i < used_cases; ++i)
		{
			char buf[100];
			sprintf(buf, "%dkey%d", k, i);
			test_keys[i] = _essl_cstring_to_string(&pool, buf);
			if(!test_values[i])
			{
				test_values[i] = _essl_mempool_alloc(&pool, 100);
				assert(test_values[i]);
			}
			sprintf(test_values[i], "%dvalue%d", k, i);
		}

		for(i = 0; i < used_cases; ++i)
		{
			for(j = 0; j < i; ++j)
			{
				assert(_essl_dict_lookup(d, test_keys[j]) == test_values[j]);
			}
			for(; j < used_cases; ++j)
			{
				assert(!_essl_dict_has_key(d, test_keys[j]));

			}


			assert(_essl_dict_size(d) == i);
			ret = _essl_dict_insert(d, test_keys[i], (void*)test_values[i]);
			assert(ret);
		}

		{
			dict_iter iterator, *it = &iterator;
			int c = 0;
			_essl_dict_iter_init(it, d);
			while(_essl_dict_next(it, 0).ptr)
			{
				++c;
			}
			assert(c == _essl_dict_size(d));
		}

		for(i = 0; i < used_cases; ++i)
		{
			for(j = 0; j < i; ++j)
			{
				assert(!_essl_dict_has_key(d, test_keys[j]));
			}
			for(; j < used_cases; ++j)
			{
				assert(_essl_dict_lookup(d, test_keys[j]) == test_values[j]);

			}


			assert(_essl_dict_size(d) == used_cases - i);
			ret = _essl_dict_remove(d, test_keys[i]);
			assert(ret);
			ret = _essl_dict_remove(d, test_keys[i]);
			assert(!ret);
		}

		
	}
	if(d->entries)
	{
 		mem_free(d->entries);
		d->entries = 0;
	}

	_essl_mempool_destroy(&pool);

	fprintf(stderr, "All tests OK!\n");
	return 0;

}


#endif /* UNIT_TEST */

