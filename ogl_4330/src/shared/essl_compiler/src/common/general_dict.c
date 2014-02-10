/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "common/general_dict.h"
#include "common/essl_mem.h"
#include "common/essl_common.h"

/* max fill factor is GENERAL_DICT_MAX_FILL_MUL/(float)(1<<GENERAL_DICT_MAX_FILL_SHIFT) */
#define GENERAL_DICT_MAX_FILL_MUL 5
#define GENERAL_DICT_MAX_FILL_SHIFT 3

#define mem_free(s)  /* no memory freeing done */

/*
  dummy key used to flag entries that have been used and now are unused.
  needed by the lookup algorithm so it still handles collisions correctly.
 */
static const void *dummy = "<dummy>";


/*
  must be anything *but* the hash value of dummy, since we should never return a dummy entry from the lookup function.
 */
#define GENERAL_DICT_DUMMY_HASH_VAL 0


memerr _essl_general_dict_init(general_dict *d, mempool *pool, target_descriptor *desc, general_dict_equals_fun equals_fun, general_dict_hash_fun hash_fun)
{
	unsigned size = GENERAL_DICT_DEFAULT_SIZE;

	d->n_filled = 0;
	d->n_active = 0;
	d->mask = size-1;
	d->pool = pool;
	d->equals_fun = equals_fun;
	d->hash_fun = hash_fun;
	d->desc = desc;
	ESSL_CHECK(d->entries = _essl_mempool_alloc(pool, sizeof(general_dict_entry)*size));
	/*PROFILE_MEMORY("Dictionary", sizeof(dict_entry)*size, 0);*/
	return MEM_OK;
}





/*
  internal lookup function. used both for user-visible string lookups and lookups used for finding free entries. never returns 0.

 */
static general_dict_entry *lookup(general_dict *d, string *key, general_dict_hash_type hash)
{
	unsigned idx = hash & d->mask;
	general_dict_entry *free_slot = 0;
	/* 
	   need at least one empty slot for failing searches to terminate.
	   this condition ought to never happen, since general_dict_insert checks 
	   for dictionaries over the maximum fill factor.
	 */
	assert(d->n_active <= d->n_filled); 
	assert(d->n_active <= d->mask); 
	
	for(;;)
	{
		general_dict_entry *e = &d->entries[idx];
		if(hash == e->hash
		   && d->equals_fun(d->desc, key, e->key))
		{
			return e;
		}			
		if(e->key == 0)
		{
			return free_slot ? free_slot : e;
		}
		
		if(!free_slot && e->key == dummy)
		{
			free_slot = e;
		}
		idx = (idx+1) & d->mask; /* if a better collision handling is desired, this is the place to change */

	}

}

static void insert(general_dict *d, void *key, general_dict_hash_type hash, /*@null@*/ void *value)
{
	general_dict_entry *e = lookup(d, key, hash);
	if(e->key == 0)
	{
		++d->n_filled;
	}
	if(e->key == 0 || e->key == dummy)
	{
		++d->n_active;
	}
	e->key = key;
	e->hash = hash;
	e->value = value;


}

static memerr resize(general_dict *d, unsigned new_size)
{
	general_dict_entry *old_entries = d->entries;
	unsigned old_mask = d->mask;
	unsigned int i;
	assert((new_size&(new_size-1)) == 0); /* new_size must be power of two */
	if(!(d->entries = _essl_mempool_alloc(d->pool, sizeof(general_dict_entry)*new_size)))
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
		if(old_entries[i].key != 0 && old_entries[i].key != dummy)
		{
			insert(d, old_entries[i].key, old_entries[i].hash, old_entries[i].value); 
		}
	}
	mem_free(old_entries);
	return MEM_OK;
}

void *_essl_general_dict_lookup(general_dict *d, void *key)
{
	general_dict_entry *e = lookup(d, key, d->hash_fun(d->desc, key));

	assert((e->key != 0 && e->key != dummy) || e->value == 0);
	return e->value;
}

essl_bool _essl_general_dict_has_key(general_dict *d, void *key)
{
	general_dict_entry *e = lookup(d, key, d->hash_fun(d->desc, key));
	return (int)(e->key != 0 && e->key != dummy);
}

static void general_dict_insert_noresize(general_dict *d, void *key, void *value)
{
	general_dict_hash_type hash = d->hash_fun(d->desc, key);
	insert(d, key, hash, value);
}

static unsigned general_dict_resize_size(general_dict *d)
{
	if(d->n_filled<<GENERAL_DICT_MAX_FILL_SHIFT >= (d->mask+1)*GENERAL_DICT_MAX_FILL_MUL)
		return (d->mask+1)*2;
	else
		return 0;
}

static memerr general_dict_check_resize(general_dict *d)
{
	unsigned new_size = general_dict_resize_size(d);
	if(new_size)
	{
		return resize(d, new_size);
	}
	return MEM_OK;
}


memerr _essl_general_dict_insert(general_dict *d, void *key, void *value)
{
	general_dict_insert_noresize(d, key, value);
	return general_dict_check_resize(d);	
}

int _essl_general_dict_remove(general_dict *d, void *key)
{
	general_dict_hash_type hash = d->hash_fun(d->desc, key);
	general_dict_entry *e = lookup(d, key, hash);
	if(e->key == dummy || e->key == 0) return 0; /* no such key */
	assert(dummy != 0);
	e->key = (void *)dummy;
	e->hash = GENERAL_DICT_DUMMY_HASH_VAL;
	e->value = 0;
	--d->n_active;
	return 1;
	
}


memerr _essl_general_dict_clear(general_dict *d)
{
	d->n_filled = 0;
	d->n_active = 0;
	memset(d->entries, 0, sizeof(general_dict_entry)*(d->mask+1));
	return MEM_OK;
}
unsigned _essl_general_dict_size(general_dict *d)
{
	return d->n_active;
}



#ifdef UNIT_TEST

general_dict_hash_type hash(target_descriptor *desc, const void *a)
{
	general_dict_hash_type h = 1337;
	const char *sa = a;
	for(; *sa != '\0'; sa++)
	{
		h = h*37 + *sa;
	}
	return h;

} 

essl_bool equals(target_descriptor *desc, const void *a, const void *b)
{
	return !strcmp(a, b);
}

int main(void)
{
	mempool_tracker tracker;
	mempool pool;
	general_dict dictionary;
	general_dict *d = &dictionary;
	int i, j;
	long k;
	memerr ret;
#define N_CASES 128
	char *test_keys[N_CASES] = {0};
	char *test_values[N_CASES] = {0};
	
	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	_essl_mempool_init(&pool, 0, &tracker);
	ret = _essl_general_dict_init(d, &pool, 0, equals, hash);
	assert(ret);
	for(k = -6; k < 6; ++k)
	{
		int used_cases = N_CASES>>(k < 0? -k: k);
		for(i = 0; i < used_cases; ++i)
		{
			
			if(!test_keys[i])
			{
				test_keys[i] = _essl_mempool_alloc(&pool, 100);
				assert(test_keys[i]);
			}
			sprintf(test_keys[i], "%ldkey%d", k, i);
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
				assert(_essl_general_dict_lookup(d, test_keys[j]) == test_values[j]);
			}
			for(; j < used_cases; ++j)
			{
				assert(!_essl_general_dict_has_key(d, test_keys[j]));

			}


			assert(_essl_general_dict_size(d) == i);
			ret = _essl_general_dict_insert(d, test_keys[i], (void*)test_values[i]);
			assert(ret);
		}

		for(i = 0; i < used_cases; ++i)
		{
			for(j = 0; j < i; ++j)
			{
				assert(!_essl_general_dict_has_key(d, test_keys[j]));
			}
			for(; j < used_cases; ++j)
			{
				assert(_essl_general_dict_lookup(d, test_keys[j]) == test_values[j]);

			}


			assert(_essl_general_dict_size(d) == used_cases - i);
			ret = _essl_general_dict_remove(d, test_keys[i]);
			assert(ret);
			ret = _essl_general_dict_remove(d, test_keys[i]);
			assert(!ret);
		}

		
	}

	_essl_mempool_destroy(&pool);

	fprintf(stderr, "All tests OK!\n");
	return 0;

}


#endif /* UNIT_TEST */

