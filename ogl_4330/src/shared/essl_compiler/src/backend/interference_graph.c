/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include "backend/interference_graph.h"

static const char *wildcard = "wildcard";

memerr _essl_interference_graph_init(interference_graph_context *ctx, mempool *pool)
{
	ctx->pool = pool;
	return _essl_ptrdict_init(&ctx->dict, pool);
}

static memerr insert_edge(interference_graph_context *ctx, void *a, void *b)
{
	void *p = _essl_ptrdict_lookup(&ctx->dict, a);
	ptrdict *d = p;
	if(p == wildcard) return MEM_OK;
	if(d == NULL) 
	{
		ESSL_CHECK(d = _essl_mempool_alloc(ctx->pool, sizeof(*d)));
		ESSL_CHECK(_essl_ptrdict_init(d, ctx->pool));
		ESSL_CHECK(_essl_ptrdict_insert(&ctx->dict, a, d));
	}
	
	ESSL_CHECK(_essl_ptrdict_insert(d, b, b)); /* the value doesn't matter, so we simply use the key */

	return MEM_OK;
}

memerr _essl_interference_graph_register_edge(interference_graph_context *ctx, void *a, void *b)
{
	ESSL_CHECK(insert_edge(ctx, a, b));
	ESSL_CHECK(insert_edge(ctx, b, a));
	return MEM_OK;
}

memerr _essl_interference_graph_register_wildcard_edge(interference_graph_context *ctx, void *a)
{
	return _essl_ptrdict_insert(&ctx->dict, a, (void*)wildcard);
}


essl_bool _essl_interference_graph_has_edge(interference_graph_context *ctx, void *a, void *b)
{
	void *p = _essl_ptrdict_lookup(&ctx->dict, a);
	ptrdict *d = p;
	if(p == wildcard) return ESSL_TRUE;
	if(d == NULL) return ESSL_FALSE;

	if(_essl_ptrdict_has_key(d, b)) return ESSL_TRUE;

	return _essl_ptrdict_lookup(&ctx->dict, b) == wildcard;

}


void _essl_interference_graph_iter_init(interference_graph_iter *it, interference_graph_context *ctx)
{
	it->ctx = ctx;
	_essl_ptrdict_iter_init(&it->it1, &ctx->dict);
	it->node1 = NULL;
}

void *_essl_interference_graph_next(interference_graph_iter *it, void **node2)
{
	if (it->node1 != NULL)
	{
		void *dummy;
		void *n2 = _essl_ptrdict_next(&it->it2, &dummy);
		if (n2 != NULL)
		{
			*node2 = n2;
			return it->node1;
		}
		it->node1 = NULL;
	}

	{
		void *n1, *p;
		n1 = _essl_ptrdict_next(&it->it1, &p);
		if (n1 == NULL)
		{
			return NULL;
		}
		if (p == wildcard)
		{
			*node2 = NULL;
			return n1;
		}
		it->node1 = n1;
		_essl_ptrdict_iter_init(&it->it2, p);
		return _essl_interference_graph_next(it, node2);
	}
}

ptrdict *_essl_interference_graph_get_edges(interference_graph_context *ctx, void *a)
{
	void *p = _essl_ptrdict_lookup(&ctx->dict, a);
	ptrdict *d = p;
	assert(p != wildcard);
	return d;
}
