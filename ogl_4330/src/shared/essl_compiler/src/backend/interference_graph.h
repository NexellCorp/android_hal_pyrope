/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef BACKEND_INTERFERENCE_GRAPH_H
#define BACKEND_INTERFERENCE_GRAPH_H

#include "common/essl_mem.h"
#include "common/ptrdict.h"

typedef struct interference_graph_context {
	mempool *pool;
	ptrdict dict;
} interference_graph_context;

typedef struct interference_graph_iter {
	struct interference_graph_context *ctx;
	ptrdict_iter it1,it2;
	void *node1;
} interference_graph_iter;


memerr _essl_interference_graph_init(interference_graph_context *ctx, mempool *pool);

memerr _essl_interference_graph_register_edge(interference_graph_context *ctx, void *pa, void *pb);

memerr _essl_interference_graph_register_wildcard_edge(interference_graph_context *ctx, void *pa);


essl_bool _essl_interference_graph_has_edge(interference_graph_context *ctx, void *pa, void *pb);

/** Iterate over the edges of the graph */
void _essl_interference_graph_iter_init(interference_graph_iter *it, interference_graph_context *ctx);

/** Next edge in iteration.
 *  @param it ierator
 *  @param node2 returns the second node of the edge,
 *         or NULL to indicate a wildcard edge.
 *  @return the first node of the edge, or NULL to indicate the end of the edge list.
 */
void *_essl_interference_graph_next(interference_graph_iter *it, void **node2);

ptrdict *_essl_interference_graph_get_edges(interference_graph_context *ctx, void *p);


#endif /* BACKEND_INTERFERENCE_GRAPH_H */
