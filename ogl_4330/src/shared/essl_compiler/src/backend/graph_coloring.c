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
#include "backend/graph_coloring.h"
#include "common/priority_queue.h"
#include "common/essl_list.h"
#include "common/unique_names.h"
#include "common/essl_target.h"
#include "common/essl_profiling.h"

static const int mask_n_comps[16] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};

static graph_node *get_or_create_node(graph_coloring_context *ctx, live_range *range)
{
	graph_node *vnode;
	vnode = _essl_ptrdict_lookup(&ctx->var_to_node, range->var);
	if (vnode == 0)
	{
		ESSL_CHECK(vnode = _essl_mempool_alloc(ctx->pool, sizeof(graph_node)));
		LIST_INSERT_FRONT(&ctx->nodes, vnode);
		ESSL_CHECK(_essl_ptrdict_insert(&ctx->var_to_node, range->var, vnode));
		vnode->range = range;
	}
	return vnode;
}

static graph_edge *interfere(graph_coloring_context *ctx, graph_node *n1, graph_node *n2)
{
	graph_edge *edge;
	/* Find existing edge */
	for (edge = n1->edges ; edge != 0 && edge->target != n2 ; edge = edge->next);
	if (edge == 0) {
		ESSL_CHECK(edge = _essl_mempool_alloc(ctx->pool, sizeof(graph_edge)));
		edge->target = n2;
		LIST_INSERT_FRONT(&n1->edges, edge);
		n1->n_edges++;
	}
	edge->this_mask |= n1->live_mask;
	edge->other_mask |= n2->live_mask;
	return edge;
}

static essl_bool include_range_in_interference_graph(live_range *range)
{
	return !range->spilled && !range->allocated;
}

#ifndef NDEBUG
static essl_bool ranges_overlap(live_range *range1, live_range *range2)
{
	live_delimiter *delim1 = range1->points;
	live_delimiter *delim2 = range2->points;
	while (delim1 && delim2) {
		/* Invariant: overlap has been checked up to the latest of delim1 and delim2 */
		if (delim2->position > delim1->position) {
			live_delimiter *temp = delim1;
			delim1 = delim2;
			delim2 = temp;
		}
		/* delim1 is earliest */
		while (delim1->next && delim1->next->position >= delim2->position) {
			delim1 = delim1->next;
		}
		if (delim1->next == 0 || delim2->next == 0) return ESSL_FALSE;
		/* Interval from delim1 to delim1->next overlaps interval from delim2 to delim2->next */
		if (delim1->next->live_mask != 0 && delim2->next->live_mask != 0) {
			return ESSL_TRUE;
		}
		/* Move the delimiter whose interval ends earliest */
		if (delim1->next->position >= delim2->next->position)
		{
			delim1 = delim1->next;
		} else {
			delim2 = delim2->next;
		}
	}
	return ESSL_FALSE;
}
#endif

static memerr build_interference_graph(graph_coloring_context *ctx)
{
	priqueue delim_queue;
	ptrdict delim_to_range;
	live_range *range;
	graph_node *live_nodes = 0;

	/* Fill in first delimiters of ranges by priority */
	ESSL_CHECK(_essl_priqueue_init(&delim_queue, ctx->pool));
	ESSL_CHECK(_essl_ptrdict_init(&delim_to_range, ctx->pool));
	for (range = ctx->liv_ctx->var_ranges ; range != 0 ; range = range->next)
	{
		if (include_range_in_interference_graph(range))
		{
			ESSL_CHECK(_essl_priqueue_insert(&delim_queue, range->start_position, range->points));
			ESSL_CHECK(_essl_ptrdict_insert(&delim_to_range, range->points, range));

			if (range->fixed)
			{
				ctx->n_fixed_ranges++;
			}
		}
	}

	while (_essl_priqueue_n_elements(&delim_queue) > 0)
	{
		graph_node *vnode, **vnodep;
		unsigned live_mask, old_live_mask;

		/* Get and update delimiter */
		live_delimiter *delim = _essl_priqueue_remove_first(&delim_queue);
		range = _essl_ptrdict_lookup(&delim_to_range, delim);
		_essl_ptrdict_remove(&delim_to_range, delim);
		if (delim->next != 0)
		{
			ESSL_CHECK(_essl_priqueue_insert(&delim_queue, delim->next->position, delim->next));
			ESSL_CHECK(_essl_ptrdict_insert(&delim_to_range, delim->next, range));
		}

		/* Get graph node */
		ESSL_CHECK(vnode = get_or_create_node(ctx, range));
		vnodep = LIST_FIND(&live_nodes, vnode, graph_node);

		/* Update live mask */
		live_mask = delim->next != 0 ? delim->next->live_mask : 0;
		old_live_mask = vnode->live_mask;
		vnode->live_mask = live_mask;

		if ((live_mask & ~old_live_mask) != 0)
		{
			/* Range liveness has increased - put interference edges */
			graph_node *lnode;
			for (lnode = live_nodes ; lnode != 0 ; lnode = lnode->next)
			{
				if (lnode != vnode)
				{
					ESSL_CHECK(interfere(ctx, lnode, vnode));
					ESSL_CHECK(interfere(ctx, vnode, lnode));
				}
			}
		}

		if (live_mask != 0)
		{
			if (vnodep == 0)
			{
				/* Node is not in live list - insert it */
				graph_node **nodep = LIST_FIND(&ctx->nodes, vnode, graph_node);
				LIST_REMOVE(nodep);
				LIST_INSERT_FRONT(&live_nodes, vnode);
			}
		} else {
			/* Range is not live */
			if (vnodep != 0)
			{
				/* Remove node from live list */
				LIST_REMOVE(vnodep);
				LIST_INSERT_FRONT(&ctx->nodes, vnode);
			}
		}

	}

#ifndef NDEBUG
	/* Verify interference graph */
	{
		live_range *r1, *r2;
		for (r1 = ctx->liv_ctx->var_ranges ; r1 != 0 ; r1 = r1->next) {
			if (include_range_in_interference_graph(r1)) {
				for (r2 = ctx->liv_ctx->var_ranges ; r2 != 0 ; r2 = r2->next) {
					if (r1 != r2 && include_range_in_interference_graph(r2)) {
						/* Is there an edge from r1 to r2? */
						essl_bool interference = ESSL_FALSE;
						graph_node *n1 = _essl_ptrdict_lookup(&ctx->var_to_node, r1->var);
						graph_edge *edge;
						for (edge = n1->edges ; edge != 0 ; edge = edge->next) {
							if (edge->target->range == r2) {
								interference = ESSL_TRUE;
								break;
							}
						}

						if (interference) {
							assert(ranges_overlap(r1,r2));
						} else {
							assert(!ranges_overlap(r1,r2));
						}
					}
				}
			}
		}
	}
#endif

	return MEM_OK;
}

static void remove_node(graph_coloring_context *ctx, graph_node **np)
{
	graph_node *n = *np;
	n->removed = 1;
	LIST_REMOVE(np);
	LIST_INSERT_FRONT(&ctx->removed_nodes, n);

	/* Decrement neighbour edge counts */
	{
		graph_edge *edge;
		for (edge = n->edges ; edge != 0 ; edge = edge->next)
		{
			edge->target->n_edges--;
		}
	}
}

essl_bool _essl_graph_coloring_default_is_definitely_colorable(graph_coloring_context *ctx, graph_node *n)
{
	if (n->range->fixed)
	{
		return ESSL_FALSE;
	}

	if (n->range->locked)
	{
		int ccount = 0;
		graph_edge **edgep;
		for (edgep = &n->edges ; *edgep != 0 ;)
		{
			graph_edge *edge = *edgep;
			if (edge->target->removed)
			{
				/* Edge to removed node - clean it out */
				*edgep = edge->next;
			} else {
				if (edge->target->range->locked)
				{
					/* Both nodes locked - interfere if the live masks overlap */
					ccount += (edge->this_mask & edge->other_mask) != 0;
				} else {
					ccount += 1;
				}
				edgep = &edge->next;
			}
		}

		return ccount < ctx->n_regs;

	} else {
		int ccounts[5] = {0,0,0,0,0};
		graph_edge **edgep;
		for (edgep = &n->edges ; *edgep != 0 ;)
		{
			graph_edge *edge = *edgep;
			if (edge->target->removed)
			{
				/* Edge to removed node - clean it out */
				*edgep = edge->next;
			} else {
				ccounts[mask_n_comps[edge->other_mask]]++;
				edgep = &edge->next;
			}
		}

		/* Determine whether there is room for this variable no matter how
		   neighbours are allocated, based on the number of neighbours having
		   each of the possible number of components.
		*/
		switch (mask_n_comps[n->range->mask])
		{
		case 1:
			/* There is room for a scalar if the total size of the neighbours
			   is less than the total register space.
			*/
			return ccounts[1] + 2*ccounts[2] + 3*ccounts[3] + 4*ccounts[4] < 4*ctx->n_regs;

		case 2:
		{
			/* First calculate how many registers are not clobbered by
			   vec3 or vec4 values.
			*/
			int fullregs = ctx->n_regs - (ccounts[3] + ccounts[4]);
			if (ccounts[2] < fullregs)
			{
				/* In the worst case, ccounts[2] registers contain one vec2 value,
				   and fullregs - ccounts[2] registers contain no vec2 values.
				*/
				return ccounts[1] < ccounts[2] + 3*(fullregs - ccounts[2]);
			} else {
				/* In the worst case, fullregs*2 - ccounts[2] registers contain one vec2 value,
				   and the rest contain two vec2 values or a vec3/vec4 value.
				*/
				return ccounts[1] < fullregs*2 - ccounts[2];
			}
		}

		case 3:
			/* A register is clobbered by a vec2 or larger value.
			   It takes twice the number of remaining registers worth of scalar values
			   to clobber the rest.
			*/
			return ccounts[1] + 2*ccounts[2] + 2*ccounts[3] + 2*ccounts[4] < 2*ctx->n_regs;

		case 4:
			/* Any value prevents a vec4 from being in the same reguster. */
			return ccounts[1] + ccounts[2] + ccounts[3] + ccounts[4] < ctx->n_regs;

		}
	}

	return ESSL_TRUE;
}

static graph_node **select_node_for_spilling(graph_coloring_context *ctx)
{
	/* Select node with highest number of edges times components */
	graph_node **best_node = 0;
	float best_weight = 1000000000.0f;
	graph_node **np;
	for (np = &ctx->nodes ; *np != 0 ; np = &(*np)->next)
	{
		if (!(*np)->range->fixed) {
			int cost;
			cost = ctx->spillcost(ctx, (*np)->range);
			if (cost >= 0) {
				int n_comps = 0;
				unsigned mask = (*np)->range->mask;
				int degree;
				float weight;
				if(mask & 0x000F) n_comps += mask_n_comps[(mask>> 0) & 0x000F];
				if(mask & 0x00F0) n_comps += mask_n_comps[(mask>> 4) & 0x000F];
				if(mask & 0x0F00) n_comps += mask_n_comps[(mask>> 8) & 0x000F];
				if(mask & 0xF000) n_comps += mask_n_comps[(mask>>12) & 0x000F];

				degree = (*np)->n_edges * n_comps;
				weight = cost/(float)degree;

				if (weight < best_weight)
				{
					best_node = np;
					best_weight = weight;
				}
			}
		}
	}

	return best_node;
}

static essl_bool any_nonfixed_ranges(graph_coloring_context *ctx)
{
	graph_node *n;
	for (n = ctx->nodes ; n != 0 ; n = n->next)
	{
		if (!n->range->fixed) return ESSL_TRUE;
	}
	return ESSL_FALSE;
}

static essl_bool color_interference_graph(graph_coloring_context *ctx)
{
	while (any_nonfixed_ranges(ctx))
	{
		graph_node **nodep;
		essl_bool removed_any = ESSL_FALSE;
		for (nodep = &ctx->nodes ; *nodep != 0 ;)
		{
			if (ctx->is_definitely_colorable(ctx, *nodep))
			{
				remove_node(ctx, nodep);
				removed_any = ESSL_TRUE;
			} else {
				nodep = &(*nodep)->next;
			}
		}
		if (!removed_any)
		{
			/* No definitely colorable nodes - spill one */
			nodep = select_node_for_spilling(ctx);
			if (nodep == 0) {
				return ESSL_FALSE;
			}
			if ((*nodep)->range->fixed) ctx->n_fixed_ranges--;
			(*nodep)->range->potential_spill = 1;
			remove_node(ctx, nodep);
		}
	}

	/* Remove fixed ranges */
	while (ctx->nodes)
	{
		remove_node(ctx, &ctx->nodes);
	}

	return ESSL_TRUE;
}

static void extract_ordering(graph_coloring_context *ctx)
{
	live_range **rangep = &ctx->liv_ctx->var_ranges;
	graph_node *n;

	/* Remove all ranges that were included in the graph */
	while (*rangep != 0)
	{
		if (include_range_in_interference_graph(*rangep))
		{
			*rangep = (*rangep)->next;
		} else {
			rangep = &(*rangep)->next;
		}
	}

	/* Add ordered ranges in order */
	for (n = ctx->removed_nodes ; n != 0 ; n = n->next)
	{
		*rangep = n->range;
		rangep = &n->range->next;
	}
	*rangep = 0;
}

static memerr init_graph_coloring_context(graph_coloring_context *ctx, liveness_context *liv_ctx, int n_regs,
										  spillcost_func spillcost, void *userdata,
										  mempool *pool, is_definitely_colorable_fun is_definitely_colorable)
{
	ctx->pool = pool;
	ESSL_CHECK(_essl_ptrdict_init(&ctx->var_to_node, pool));
	ctx->liv_ctx = liv_ctx;
	ctx->n_regs = n_regs;
	ctx->spillcost = spillcost;
	ctx->userdata = userdata;
	ctx->nodes = 0;
	ctx->removed_nodes = 0;
	ctx->n_fixed_ranges = 0;
	ctx->is_definitely_colorable = is_definitely_colorable;
	return MEM_OK;
}

#ifdef DEBUGPRINT
static memerr print_node(node *n, unique_name_context *names, FILE *f)
{
	const char *name;
	ESSL_CHECK(name = _essl_unique_name_get_or_create(names, n));
	fprintf(f, "%s", name);
	return MEM_OK;
}

static int mask_decim(unsigned mask)
{
	return (mask & 8)/8 + (mask & 4)/4*10 + (mask & 2)/2*100 + (mask & 1)*1000;
}

static memerr print_interference_graph(graph_coloring_context *ctx, unique_name_context *names, FILE *out)
{
	ptrdict_iter it;
	node *var;
	graph_node *vnode;
	_essl_ptrdict_iter_init(&it, &ctx->var_to_node);
	while ((var = _essl_ptrdict_next(&it, (void **)(void *)&vnode)) != 0)
	{
		graph_edge *edge;
		ESSL_CHECK(print_node(var, names, out));
		fprintf(out, " -> ");
		for (edge = vnode->edges ; edge != 0 ; edge = edge->next)
		{
			ESSL_CHECK(print_node(edge->target->range->var, names, out));
			fprintf(out, " (%04d-%04d)%s",
					mask_decim(edge->this_mask),
					mask_decim(edge->other_mask),
					edge->next ? ", " : "");
			/*fprintf(out, "%s", edge->next ? ", " : "");*/
		}
		fprintf(out, "\n");
	}
	fprintf(out, "\n");
	return MEM_OK;
}
#endif

memerr _essl_sort_live_ranges_by_graph_coloring(mempool *pool, liveness_context *liv_ctx, int n_regs,
												spillcost_func spillcost, void *userdata, essl_bool *ok,
												is_definitely_colorable_fun is_definitely_colorable,
												interference_graph_context *conflict_vars,
												target_descriptor *desc, unique_name_context *names)
{
	graph_coloring_context gc_ctx, *ctx = &gc_ctx;
	IGNORE_PARAM(desc);
	IGNORE_PARAM(names);
	ESSL_CHECK(init_graph_coloring_context(ctx, liv_ctx, n_regs, spillcost, userdata, pool, is_definitely_colorable));

	TIME_PROFILE_START("-build_igraph");
	ESSL_CHECK(build_interference_graph(ctx));
	TIME_PROFILE_STOP("-build_igraph");

	if (conflict_vars != NULL)
	{
		interference_graph_iter it;
		node *var1, *var2;
		_essl_interference_graph_iter_init(&it, conflict_vars);
		while ((var1 = _essl_interference_graph_next(&it, (void **)(void *)&var2)) != 0)
		{
			graph_node *node1, *node2;
			graph_edge *edge;
			assert(var2 != NULL);
			node1 = _essl_ptrdict_lookup(&ctx->var_to_node, var1);
			node2 = _essl_ptrdict_lookup(&ctx->var_to_node, var2);
			ESSL_CHECK(edge = interfere(ctx, node1, node2));
			edge->other_mask = 0xf; /* full mask */
		}
	}

#ifdef DEBUGPRINT
	{
		FILE *pfile;
		char *filename = desc->kind == TARGET_FRAGMENT_SHADER ? "out-mali200.txt" : "out-maligp2.txt";
		if ((pfile = fopen(filename, "a")) != 0)
		{
			ESSL_CHECK(print_interference_graph(ctx, names, pfile));
			fclose(pfile);
		}
	}
#endif

	TIME_PROFILE_START("-color_igraph");
	*ok = color_interference_graph(ctx);
	TIME_PROFILE_STOP("-color_igraph");

	if (*ok) extract_ordering(ctx);

	return MEM_OK;
}
