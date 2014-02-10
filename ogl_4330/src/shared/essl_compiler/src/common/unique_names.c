/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*
 * This file defines a method for providing guaranteed unique names based on a 
 * unique identifier for the named item as a void* pointer.
 */

#define NEEDS_STDIO  /* for snprintf */
 
#include "common/essl_system.h"
#include "unique_names.h"
#include "common/essl_common.h"

memerr _essl_unique_name_init(unique_name_context *ctx, mempool *pool, const char *prefix)
{
	ctx->pool = pool;
	ESSL_CHECK(_essl_ptrdict_init(&ctx->names, pool));
	ctx->counter = 1;
	ctx->prefix = prefix;
	return MEM_OK;
}

/*@null@*/ const char *_essl_unique_name_get(unique_name_context *ctx, void *n)
{
	return _essl_ptrdict_lookup(&ctx->names, n);

}

memerr _essl_unique_name_set(unique_name_context *ctx, void *n, const char *name)
{
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->names, n, (void*)name));
	return MEM_OK;

}

/*@null@*/ const char *_essl_unique_name_get_or_create(unique_name_context *ctx, void *n)
{
	char *name = (char*)_essl_unique_name_get(ctx, n);
	size_t size = strlen(ctx->prefix) + 16;
	if(name) return name;
	
	ESSL_CHECK(name = _essl_mempool_alloc(ctx->pool, size));

	(void)snprintf(name, size, "%s%03d", ctx->prefix, ctx->counter);
	/*(void)snprintf(name, size, "%p", n);*/
	++ctx->counter;
	ESSL_CHECK(_essl_unique_name_set(ctx, n, name));
	return name;
}
