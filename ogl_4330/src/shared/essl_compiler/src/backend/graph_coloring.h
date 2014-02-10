/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef BACKEND_GRAPH_COLORING_H
#define BACKEND_GRAPH_COLORING_H

#include "backend/liveness.h"
#include "backend/interference_graph.h"
#include "common/essl_mem.h"
#include "common/ptrdict.h"
#include "common/unique_names.h"
#include "common/essl_list.h"

struct _tag_graph_edge;
struct _tag_graph_coloring_context;

typedef int (*spillcost_func)(struct _tag_graph_coloring_context *ctx, live_range *range);

typedef struct _tag_graph_node {
	struct _tag_graph_node *next;
	struct _tag_graph_edge *edges;
	live_range *range;
	unsigned n_edges:16;
	unsigned live_mask:N_COMPONENTS;
	unsigned on_worklist:1;
	unsigned removed:1;
} graph_node;
ASSUME_ALIASING(graph_node, generic_list);

typedef struct _tag_graph_edge {
	struct _tag_graph_edge *next;
	graph_node *target;
	unsigned this_mask:N_COMPONENTS;
	unsigned other_mask:N_COMPONENTS;
} graph_edge;
ASSUME_ALIASING(graph_edge, generic_list);

typedef essl_bool (*is_definitely_colorable_fun)(struct _tag_graph_coloring_context *ctx, graph_node *n);

typedef struct _tag_graph_coloring_context {
	mempool *pool;
	liveness_context *liv_ctx;
	int n_regs;
	spillcost_func spillcost;
	void *userdata;
	graph_node *nodes;
	ptrdict var_to_node;
	graph_node *removed_nodes;
	int n_fixed_ranges;
	is_definitely_colorable_fun is_definitely_colorable;
} graph_coloring_context;

memerr _essl_sort_live_ranges_by_graph_coloring(mempool *pool, liveness_context *liv_ctx, int n_regs,
												spillcost_func spillcost, void *userdata, essl_bool *ok,
												is_definitely_colorable_fun is_definitely_colorable,
												interference_graph_context *conflict_vars, target_descriptor *desc, unique_name_context *names);

essl_bool _essl_graph_coloring_default_is_definitely_colorable(graph_coloring_context *ctx, graph_node *n);

#endif
