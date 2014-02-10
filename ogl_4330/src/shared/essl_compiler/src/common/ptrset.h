/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_PTRSET_H
#define COMMON_PTRSET_H

#include "common/essl_mem.h"
#include "common/ptrdict.h"

/**
   Set of pointers. collisions are handled by adjacent entries (idx = (idx+1)&mask).
   Dummy slots are used when entries are deleted in order to avoid rehashing.
   Null pointers cannot be inserted into the set.
 */
typedef struct {
	ptrdict dict;
} ptrset;




typedef struct {
	ptrdict_iter it;
} ptrset_iter;

typedef struct {
	ptrdict_reverse_iter it;
} ptrset_reverse_iter;



memerr _essl_ptrset_init(/*@out@*/ ptrset *s, mempool *pool);


essl_bool _essl_ptrset_has(ptrset *s, void *value);


essl_bool _essl_ptrset_is_subset(ptrset *subset, ptrset *set);

essl_bool _essl_ptrset_equal(ptrset *a, ptrset *b);

memerr _essl_ptrset_union(ptrset *s, ptrset *b);

memerr _essl_ptrset_difference(ptrset *s, ptrset *b);

memerr _essl_ptrset_insert(ptrset *s, void *value);

/**
   Remove value from set. Returns 1 if a value was removed, and 0 if no such value existed.

*/
int _essl_ptrset_remove(ptrset *s, void *value);

memerr _essl_ptrset_clear(ptrset *s);

unsigned _essl_ptrset_size(ptrset *s);

void _essl_ptrset_iter_init(/*@out@*/ ptrset_iter *it, ptrset *s);

/*@null@*/ void *_essl_ptrset_next(ptrset_iter *it);


void _essl_ptrset_reverse_iter_init(/*@out@*/ ptrset_reverse_iter *it, ptrset *s);

/*@null@*/ void *_essl_ptrset_reverse_next(ptrset_reverse_iter *it);


#endif /* COMMON_PTRSET_H */
