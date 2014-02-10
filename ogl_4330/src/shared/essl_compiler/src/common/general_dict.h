/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_GENERAL_DICT_H
#define COMMON_GENERAL_DICT_H

#include "common/essl_common.h"
#include "common/essl_target.h"

#define GENERAL_DICT_DEFAULT_SIZE 32

typedef unsigned long general_dict_hash_type;

/**
  general_dictionary entry.
  key == 0: empty. also, hash == 0 and value == 0 in the case
  key == dummy: formerly used, now unused.
  else: key contains an entry, hash is the hashed value of key

 */
typedef struct
{
	general_dict_hash_type hash;
	void *key;
	/*@null@*/ void *value;
} general_dict_entry;


/**
  General_Dictionary with strings as keys. collisions are handled by adjacent entries (idx = (idx+1)&mask).
  Dummy slots are used when entries are deleted in order to avoid rehashing.

  If the lengths of the keys are negative, they are treated as pure pointers and comparison is done with pointer compare, and hashing is done by casting the void* value to unsigned int. This is so that ptrdict can be a facade for this dictionary.
 */

typedef essl_bool (*general_dict_equals_fun)(target_descriptor *desc, const void *a, const void *b);
typedef general_dict_hash_type (*general_dict_hash_fun)(target_descriptor *desc, const void *a);


typedef struct {
	unsigned n_filled; /**< number of filled slots (active + dummy) */
	unsigned n_active; /**< number of active slots (i.e. actual inserted keys) */

	unsigned mask; /**< size of entry table - 1. size must always be a power of two, so this can be used as a bitmask */
	general_dict_entry *entries;
	general_dict_equals_fun equals_fun;
	general_dict_hash_fun hash_fun;

	mempool *pool;
	target_descriptor *desc;
} general_dict;



/**
  Initialize a dictionary
  @param  d is the dictionary to initialize
  @param pool The memory pool to use for the entry table
 */
memerr _essl_general_dict_init(/*@out@*/ general_dict *d, mempool *pool, target_descriptor *desc, general_dict_equals_fun equals_fun, general_dict_hash_fun hash_fun);


/** Returns the value associated with the specified key, or NULL if it doesn't exit. */
/*@null@*/ void *_essl_general_dict_lookup(general_dict *d, void *key);

/** Returns nonzero if the dictionary contains the specified key */
essl_bool _essl_general_dict_has_key(general_dict *d, void *key);

/** Inserts the specified key/value pair into the dictionary. */
memerr _essl_general_dict_insert(general_dict *d, void *key, void *value);

/** Removes a given key from the dictionary. Returns non-zero if the key was removed, zero if there was no such key */
int _essl_general_dict_remove(general_dict *d, void *key);

/** Removes all entries from a dictionary */
memerr _essl_general_dict_clear(general_dict *d);

/** Retrieves the number of entries in the dictionary */
unsigned _essl_general_dict_size(general_dict *d);

#endif /* COMMON_DICT_H */
