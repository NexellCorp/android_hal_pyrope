/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_DICT_H
#define COMMON_DICT_H

#include "common/essl_str.h"

#define DICT_DEFAULT_SIZE 32

typedef unsigned long hash_type;

/**
  dictionary entry.
  key == 0: empty. also, hash == 0 and value == 0 in the case
  key == dummy: formerly used, now unused.
  else: key contains an entry, hash is the hashed value of key

 */
typedef struct
{
	hash_type hash;
	string key;
	/*@null@*/ void *value;
} dict_entry;


/**
  Dictionary with strings as keys. collisions are handled by adjacent entries (idx = (idx+1)&mask).
  Dummy slots are used when entries are deleted in order to avoid rehashing.

  If the lengths of the keys are negative, they are treated as pure pointers and comparison is done with pointer compare, and hashing is done by casting the void* value to unsigned int. This is so that ptrdict can be a facade for this dictionary.
 */


typedef struct {
	unsigned n_filled; /**< number of filled slots (active + dummy) */
	unsigned n_active; /**< number of active slots (i.e. actual inserted keys) */

	unsigned mask; /**< size of entry table - 1. size must always be a power of two, so this can be used as a bitmask */
	dict_entry *entries;

	mempool *pool;
} dict;



/**
  Dictionary iterator.
  This struct is used for iterating through a dictionary.
  The items are returned in no particular order, but the order is consistent as long as you don't insert new items.
  It is safe to remove and replace items while iterating through the dictionary, but not safe to insert new items during an iteration.

*/
typedef struct {
	const dict *dictionary;
	unsigned idx;
} dict_iter;



/**
  Initialize a dictionary
  @param  d is the dictionary to initialize
  @param size_hint specifies the number of entries to reserve space for. size_hint is optional, set to 0 to get the default dictionary size.
  @param pool The memory pool to use for the entry table
 */
memerr _essl_dict_init(/*@out@*/ dict *d, mempool *pool);


/** Returns the value associated with the specified key, or NULL if it doesn't exit. */
/*@null@*/ void *_essl_dict_lookup(dict *d, string key);

/** Returns nonzero if the dictionary contains the specified key */
essl_bool _essl_dict_has_key(dict *d, string key);

/** Inserts the specified key/value pair into the dictionary. */
memerr _essl_dict_insert(dict *d, string key, void *value);

/** Removes a given key from the dictionary. Returns non-zero if the key was removed, zero if there was no such key */
int _essl_dict_remove(dict *d, string key);

/** Removes all entries from a dictionary */
memerr _essl_dict_clear(dict *d);

/** Retrieves the number of entries in the dictionary */
unsigned _essl_dict_size(dict *d);

/** Initializes a dictionary iterator for a given dictionary */
void _essl_dict_iter_init(/*@out@*/ dict_iter *it, const dict *d);

typedef /*@null@*/ void *void_ptr;

/** Get the next item in the dictionary. The key is the return value, 'value', if non-zero, contains the value for the key. If the returned key has a null pointer, you have reached the end. */
string _essl_dict_next(dict_iter *it, /*@null@*/ void_ptr *value);


#endif /* COMMON_DICT_H */
