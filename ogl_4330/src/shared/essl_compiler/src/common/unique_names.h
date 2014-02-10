/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef COMMON_UNIQUE_NAMES_H
#define COMMON_UNIQUE_NAMES_H

#include "common/essl_system.h"
#include "common/ptrdict.h"
#include "common/ptrset.h"
#include "common/essl_common.h"

typedef struct
{
	mempool *pool;
	ptrdict names;
	int counter;
	const char *prefix;
} unique_name_context;

memerr _essl_unique_name_init(unique_name_context *ctx, mempool *pool, const char *prefix);
/*@null@*/ const char *_essl_unique_name_get(unique_name_context *ctx, void *n);
memerr _essl_unique_name_set(unique_name_context *ctx, void *n, const char *name);
/*@null@*/ const char *_essl_unique_name_get_or_create(unique_name_context *ctx, void *n);

#endif /* COMMON_UNIQUE_NAMES_H */
