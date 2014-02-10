/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_PTRDICT_H
#define COMMON_PTRDICT_H

#include "common/essl_mem.h"
#include "common/essl_dict.h"


#define PTRDICT_DEFAULT_LOG2_SIZE 5
#define PTRDICT_DEFAULT_SIZE (1<<PTRDICT_DEFAULT_LOG2_SIZE)

/**
  dictionary entry.
  key == 0: empty. also, hash == 0 and value == 0 in the case
  key == dummy: formerly used, now unused.
  else: key contains an entry, hash is the hashed value of key

 */
typedef struct
{
	void *key;
	/*@null@*/ void *value;
	int next;
	int prev;
} ptrdict_entry;


/**
  Dictionary with strings as keys. collisions are handled by adjacent entries (idx = (idx+1)&mask).
  Dummy slots are used when entries are deleted in order to avoid rehashing.

  If the lengths of the keys are negative, they are treated as pure pointers and comparison is done with pointer compare, and hashing is done by casting the void* value to unsigned int. This is so that ptrdict can be a facade for this dictionary.
 */


typedef struct {
	unsigned n_filled; /**< number of filled slots (active + dummy) */
	unsigned n_active; /**< number of active slots (i.e. actual inserted keys) */

	unsigned mask; /**< size of entry table - 1. size must always be a power of two, so this can be used as a bitmask */
	unsigned log2_size;
	ptrdict_entry *entries;

	mempool *pool;
	int first;
	int last;
} ptrdict;



/**
  Dictionary iterator.
  This struct is used for iterating through a dictionary.
  The items are returned in no particular order, but the order is consistent as long as you don't insert new items.
  It is safe to remove and replace items while iterating through the dictionary, but not safe to insert new items during an iteration.

*/
typedef struct {
	const ptrdict *dictionary;
	int idx;
} ptrdict_iter;

/* reverse order iterator */
typedef struct {
	const ptrdict *dictionary;
	int idx;
} ptrdict_reverse_iter;




/**
  Initialize a dictionary
  @param  d is the dictionary to initialize
  @param size_hint specifies the number of entries to reserve space for. size_hint is optional, set to 0 to get the default dictionary size.
  @param pool The memory pool to use for the entry table
  */
memerr _essl_ptrdict_init(/*@out@*/ ptrdict *d, mempool *pool);

/** Returns the value associated with the specified key, or NULL if it doesn't exit. */
/*@null@*/ void *_essl_ptrdict_lookup(ptrdict *d, void *key);

/** Returns nonzero if the dictionary contains the specified key */
essl_bool _essl_ptrdict_has_key(ptrdict *d, void *key);

/** Inserts the specified key/value pair into the dictionary. */
memerr _essl_ptrdict_insert(ptrdict *d, void *key, void *value);

/** Removes a given key from the dictionary. Returns one if the key was removed, zero if there was no such key */
int _essl_ptrdict_remove(ptrdict *d, void *key);

/** Removes all entries from a dictionary */
memerr _essl_ptrdict_clear(ptrdict *d);

/** Retrieves the number of entries in the dictionary */
unsigned _essl_ptrdict_size(ptrdict *d);

/** Initializes a dictionary iterator for a given dictionary */
void _essl_ptrdict_iter_init(/*@out@*/ ptrdict_iter *it, const ptrdict *d);


/** Get the next item in the dictionary. The key is the return value, 'value', if non-zero, contains the value for the key. If the returned key has a null pointer, you have reached the end. */
/*@null@*/ void *_essl_ptrdict_next(ptrdict_iter *it, /*@null@*/ void_ptr *value);

/** Initializes a reverse dictionary iterator for a given dictionary */
void _essl_ptrdict_reverse_iter_init(/*@out@*/ ptrdict_reverse_iter *it, const ptrdict *d);


/** Get the next item in the dictionary (reverse order). The key is the return value, 'value', if non-zero, contains the value for the key. If the returned key has a null pointer, you have reached the end. */
/*@null@*/ void *_essl_ptrdict_reverse_next(ptrdict_reverse_iter *it, /*@null@*/ void_ptr *value);

void _essl_ptrdict_set_value(ptrdict_entry *entry, void *value);
void *_essl_ptrdict_get_value(ptrdict_entry *entry);
ptrdict_entry * _essl_ptrdict_next_entry(ptrdict_iter *it);
void *_essl_ptrdict_get_key(ptrdict_entry *entry);


#endif /* COMMON_PTRDICT_H */
