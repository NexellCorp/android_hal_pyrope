/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "common/find_blocks_for_operations.h"
#include "common/ptrdict.h"


/** Internal context struct for keeping needed data */
typedef struct _tag_find_blocks_context
{
	mempool *pool;
	control_flow_graph *cfg;
	basic_block *current_block;
	ptrdict visited_nodes;
} find_blocks_context;



typedef enum
{
	NS_NONE, NS_CLEARED, NS_PASS_1, NS_PASS_2
} node_status;


/**
 * Set the status of a node.
 * @param ctx Context structure with visited dictionary.
 * @param n Node to set status for.
 * @param status Status to set.
 * @return MEM_OK on success, MEM_ERROR on failure.
 */
static memerr set_node_status(find_blocks_context *ctx, node *n, node_status status)
{
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->visited_nodes, n, (void*)status));
	return MEM_OK;
}


/**
 * Get the status of a node.
 * @param ctx Context structure with visited dictionary.
 * @param n Node to get status for.
 * @return status for node.
 */
static node_status get_node_status(find_blocks_context *ctx, node *n)
{
	void *data = _essl_ptrdict_lookup(&ctx->visited_nodes, n);
	
	if (data == NULL)
	{
		return NS_NONE;
	}

	return (node_status)data;
}


/**
 * Return the earliest basic block of the two specified basic blocks
 * @param a A basic block
 * @param b Another basic block
 * @return The earliest basic block of the two specified blocks (a and b)
 */
static basic_block *get_earliest_block(basic_block *a, basic_block *b)
{
	if (a == NULL)
	{
		return b;
	}
	
	if (b == NULL)
	{
		return a;
	}

	return _essl_common_dominator(a, b);
}



/**
 * Return the latest basic block of the two specified basic blocks
 * @param a A basic block
 * @param b Another basic block
 * @return The latest basic block of the two specified blocks (a and b)
 */
static basic_block *get_latest_block(find_blocks_context *ctx, basic_block *a, basic_block *b)
{
	basic_block *dom;

	if (a == NULL)
	{
		a = ctx->cfg->entry_block;
	}

	if (b == NULL)
	{
		b = ctx->cfg->entry_block;
	}

	dom = _essl_common_dominator(a, b);

	if (dom == a)
	{
		return b;
	}
	else
	{
		assert(dom == b);
		return a;
	}
}



/**
 * Used to calculate the earliest and latest basic block for a given operation.
 * Function is called recursively to handle all childs and dependent operations.
 * @param ctx Context structure with needed data.
 * @param source The node to find earliest and latest basic block for (childs will also be checked with recursion).
 * @param dest Parent operation (node) for source. Earliest block might be updated.
 * @param record_block ESSL_FALSE is used when checking control dependent ops, to avoid updating earliest/latest block where it shouldn't.
 * @return MEM_OK on success, MEM_ERROR on failure.
 */
static memerr handle_dependency_pass_1(find_blocks_context *ctx, node *source, node *dest) 
{
	unsigned int i;
	essl_bool continue_visit = ESSL_FALSE;
	node_status ns = get_node_status(ctx, source);
	basic_block *old_src_latest_block;

	if (ns != NS_PASS_1)
	{
		if (ns != NS_CLEARED)
		{
			source->expr.earliest_block = NULL;
			source->expr.latest_block = NULL;
		}

		ESSL_CHECK(set_node_status(ctx, source, NS_PASS_1));

		/* Fill in initial information */
		if (source->hdr.is_control_dependent)
		{
			control_dependent_operation *cd_op;
			cd_op = _essl_ptrdict_lookup(&ctx->cfg->control_dependence, source);

			assert(cd_op != NULL);
			assert(cd_op->block != NULL && cd_op->block->postorder_visit_number >= 0);

			source->expr.latest_block = source->expr.earliest_block = cd_op->block;
			assert(source->expr.latest_block->output_visit_number != -1);
		}

		/* Recurse on all sources of the source */
		continue_visit = ESSL_TRUE;
	}

	old_src_latest_block = source->expr.latest_block;
	source->expr.latest_block = get_earliest_block(dest->expr.latest_block, source->expr.latest_block);
	if(source->expr.latest_block != old_src_latest_block)
	{
		continue_visit = ESSL_TRUE;
	}

	if (continue_visit)
	{
		/* Recurse on children */

		for (i = 0; i < GET_N_CHILDREN(source); i++)
		{
			node *ss = GET_CHILD(source, i);
			if (ss)
			{
				ESSL_CHECK(handle_dependency_pass_1(ctx, ss, source));
			}
		}
	}

	dest->expr.earliest_block = get_latest_block(ctx, dest->expr.earliest_block, source->expr.earliest_block);

	return MEM_OK;
}



/**
 * Calculate the earliest and latest basic block for each operation for specified basic block.
 * @param ctx Context stucture with data to process.
 * @return MEM_OK on success, MEM_ERROR on failure.
 */
static memerr handle_block_pass_1(find_blocks_context *ctx)
{
	phi_list *phi;
	unsigned i;
	control_dependent_operation *cd_op;

	/* Create/init pseudo-nodes representing the block top and bottom operations depths */

	if (ctx->current_block->top_depth == NULL)
	{
		ESSL_CHECK(ctx->current_block->top_depth = _essl_new_phi_expression(ctx->pool, ctx->current_block));
	}

	ctx->current_block->top_depth->expr.earliest_block = ctx->current_block->top_depth->expr.latest_block = ctx->current_block;
	ESSL_CHECK(set_node_status(ctx, ctx->current_block->top_depth, NS_CLEARED));

	if (ctx->current_block->bottom_depth == NULL)
	{
		ESSL_CHECK(ctx->current_block->bottom_depth = _essl_new_phi_expression(ctx->pool, ctx->current_block));
	}

	ctx->current_block->bottom_depth->expr.earliest_block = ctx->current_block->bottom_depth->expr.latest_block = ctx->current_block;
	ESSL_CHECK(set_node_status(ctx, ctx->current_block->bottom_depth, NS_CLEARED));

	/* handle the phi nodes */

	for (phi = ctx->current_block->phi_nodes; phi != NULL; phi = phi->next)
	{
		phi->phi_node->expr.earliest_block = phi->phi_node->expr.latest_block = ctx->current_block;
		ESSL_CHECK(set_node_status(ctx, phi->phi_node, NS_CLEARED));
	}

	/* check both default_target and true_target (if any) */
	for(i = 0; i < ctx->current_block->n_successors; ++i)
	{
		basic_block *succ = ctx->current_block->successors[i];
		for (phi = succ->phi_nodes; phi != NULL; phi = phi->next)
		{
			phi_source *phi_s;
			for (phi_s = phi->phi_node->expr.u.phi.sources; phi_s != NULL; phi_s = phi_s->next)
			{
				if (phi_s->join_block == ctx->current_block)
				{
					ESSL_CHECK(handle_dependency_pass_1(ctx, phi_s->source, ctx->current_block->bottom_depth));
					break;
				}
			}
		}
	}

	/* Handle block source */

	if (ctx->current_block->source != NULL)
	{
		ESSL_CHECK(handle_dependency_pass_1(ctx, ctx->current_block->source, ctx->current_block->bottom_depth));
	}

	/* handle control depentent ops */

	for (cd_op = ctx->current_block->control_dependent_ops; cd_op != NULL; cd_op = cd_op->next)
	{
		assert(cd_op->block == ctx->current_block);
		ESSL_CHECK(handle_dependency_pass_1(ctx, cd_op->op, ctx->current_block->bottom_depth));
	}

	/* Bottom depends on top */

	ESSL_CHECK(handle_dependency_pass_1(ctx, ctx->current_block->top_depth, ctx->current_block->bottom_depth));

	return MEM_OK;
}



/**
 * Find best basic block for the given node, based on already calculated earliest and latest basic block
 * @param ctx Context stucture (need basic blocks from control flow graph)
 * @param n Node to find best basic block for
 */
static memerr find_optimal_block_for_node(find_blocks_context *ctx, node *n)
{
	assert(n->expr.latest_block != NULL);

	if (n->expr.earliest_block == NULL)
	{
		n->expr.earliest_block = ctx->cfg->entry_block;
	}

	assert(_essl_common_dominator(n->expr.latest_block, n->expr.earliest_block) == n->expr.earliest_block);

	/* Keep constants in the same block as they are used.  We will otherwise
	 * waste registers, e.g. when the constant's live range get split by phi
	 * elimination.  */
	if (n->hdr.kind == EXPR_KIND_CONSTANT)
	{
		n->expr.best_block = n->expr.latest_block;
		return MEM_OK;
	}

	{
		basic_block *best_block = n->expr.latest_block;
		basic_block *curr_block = n->expr.latest_block;

	    while (curr_block != n->expr.earliest_block)
		{
			assert(curr_block->immediate_dominator != NULL);
			
			curr_block = curr_block->immediate_dominator;
			if (curr_block->cost < best_block->cost)
			{
				best_block = curr_block;
			}
		}

		n->expr.best_block = best_block;
	}

	return MEM_OK;
}



/**
 * Used to calculate the best block for a given operation.
 * Function is called recursively to handle all childs and depentent operations.
 * @param ctx Context structure with needed data.
 * @param source The node to find best block for (childs will also be checked with recursion)
 * @return MEM_OK on success, MEM_ERROR on failure.
 */
static memerr handle_dependency_pass_2(find_blocks_context *ctx, node *source) 
{
	if (get_node_status(ctx, source) != NS_PASS_2)
	{
		size_t i;

		ESSL_CHECK(set_node_status(ctx, source, NS_PASS_2));

		source->expr.best_block = NULL;

		/* Fill in initial information */
		ESSL_CHECK(find_optimal_block_for_node(ctx, source));

		/* Recurse on children */
		for (i = 0; i < GET_N_CHILDREN(source); i++) 
		{
			node *ss = GET_CHILD(source, i);
			if (ss)
			{
				ESSL_CHECK(handle_dependency_pass_2(ctx, ss));
			}
		}

		/* Recurse on control dependencies */
		if (source->hdr.is_control_dependent) 
		{
			control_dependent_operation *cd_op;
			operation_dependency *dep;
			
			cd_op = _essl_ptrdict_lookup(&ctx->cfg->control_dependence, source);

			assert(cd_op != NULL);
			assert(cd_op->block != NULL && cd_op->block->postorder_visit_number >= 0);

			if (cd_op->dependencies != NULL)
			{
				/* Explicit dependencies on other operations */
				for (dep = cd_op->dependencies; dep != NULL; dep = dep->next)
				{
					node *ss = dep->dependent_on->op;
					ESSL_CHECK(handle_dependency_pass_2(ctx, ss));
				}
			}
			else
			{
				/* Implicitly dependent on block top */
				ESSL_CHECK(handle_dependency_pass_2(ctx, ctx->current_block->top_depth));
			}
		}
	}

	return MEM_OK;
}



/**
 * Calculate the best basic block for each operation for specified basic block.
 * @param xtx Context structure with data to process.
 * @return MEM_OK on success, MEM_ERROR on failure.
 */
static memerr handle_block_pass_2(find_blocks_context *ctx)
{
	control_dependent_operation *cd_op;
	unsigned i;

	/* Handle all phi nodes of successor blocks */
	for(i = 0; i < ctx->current_block->n_successors; ++i)
	{
		basic_block *succ = ctx->current_block->successors[i];
		phi_list *phi;
		for (phi = succ->phi_nodes; phi != NULL; phi = phi->next)
		{
			phi_source *phi_s;
			for (phi_s = phi->phi_node->expr.u.phi.sources; phi_s != NULL; phi_s = phi_s->next)
			{
				if (phi_s->join_block == ctx->current_block)
				{
					ESSL_CHECK(handle_dependency_pass_2(ctx, phi_s->source));
					break;
				}
			}
		}
	}

	/* Handle block source */
	if (ctx->current_block->source != NULL)
	{
		ESSL_CHECK(handle_dependency_pass_2(ctx, ctx->current_block->source));
	}

	/* Handle control-dependent ops */
	for (cd_op = ctx->current_block->control_dependent_ops; cd_op != NULL; cd_op = cd_op->next)
	{
		ESSL_CHECK(handle_dependency_pass_2(ctx, cd_op->op));
	}

	/* Bottom depends on top */
	ESSL_CHECK(handle_dependency_pass_2(ctx, ctx->current_block->top_depth));

	return MEM_OK;
}



/**
 * Find the earlist, best and latest basic block for every node in specified control flow graph.
 * @param pool Mempool to use for memory allocations.
 * @param locak_pool Mempool used for local allocations (can be deleted after function return).
 * @param cfg Control flow graph to calculate earlist, best and latest basic block for
 * @return MEM_OK on success, MEM_ERROR on failure.
 */
static memerr find_blocks_for_operations_helper(mempool *pool, mempool *local_pool, control_flow_graph *cfg)
{
	find_blocks_context ctx;
	unsigned int i;

	ctx.pool = pool;
	ctx.cfg = cfg;

	ESSL_CHECK(_essl_ptrdict_init(&ctx.visited_nodes, local_pool));

	/* First pass is to calculate the earliest and latest basic block */

	for (i = cfg->n_blocks; i > 0; i--)
	{
		ctx.current_block = cfg->postorder_sequence[i - 1];
		ESSL_CHECK(handle_block_pass_1(&ctx));
	}

	/* Second (and last) pass it to calculate the best basic block */

	ESSL_CHECK(_essl_ptrdict_clear(&ctx.visited_nodes));

	for (i = 0; i < cfg->n_blocks; i++)
	{
		ctx.current_block = cfg->postorder_sequence[i];
		ESSL_CHECK(handle_block_pass_2(&ctx));
	}

	return MEM_OK;
}


memerr _essl_find_blocks_for_operations_func(mempool *pool, symbol *func)
{
	memerr ret;
	mempool local_pool;
	ESSL_CHECK(_essl_mempool_init(&local_pool, 0, _essl_mempool_get_tracker(pool)));
	ret = find_blocks_for_operations_helper(pool, &local_pool, func->control_flow_graph);
	_essl_mempool_destroy(&local_pool);
	return ret;
}


/**
 * Find the earlist, best and latest basic block for every node in specified control flow graph.
 * @param pool Mempool to use for memory allocations.
 * @param cfg Control flow graph to calculate earlist, best and latest basic block for
 * @return MEM_OK on success, MEM_ERROR on failure.
 */
memerr _essl_find_blocks_for_operations(pass_run_context *pr_ctx, symbol *func)
{
	memerr ret;
	ret = find_blocks_for_operations_helper(pr_ctx->pool, pr_ctx->tmp_pool, func->control_flow_graph);
	return ret;
}


/**
 * The idea behind this factor is that this should be used to penalise nodes with few children, since deep graphs are worse to schedule than wide graphs.
 * @param no_childs Number of childs.
 * @return Penalty factor for the given number of childs.
 */
static int get_penalty_factor(unsigned int no_childs)
{
	switch(no_childs)
	{
	case 0:
		return 1;
	case 1:
		return 3;
	case 2:
		return 2;
	}

	return 1;
}



/**
 * Calculate a cost factor (based on op weight and number of children) for specified node (including childs).
 * @param n Node to calculate cost for.
 * @param best_block Count only nodes with best block matching this.
 * @param desc Target descriptor containing function to retrieve operation weight for a given node
 * @param visited_nodes Visited nodes will be added to this ptrset. Nodes already in ptrset will not be checked.
 * @param weight A cost factor. Higher number means that a csel transformation is less likely to be beneficial.
 * @return MEM_OK on success, MEM_ERROR on failure.
 */
int _essl_calc_op_weight(node *n, basic_block* best_block, target_descriptor *desc, ptrset *visited_nodes, int *weight)
{
	unsigned int i;
	int my_weight = 0;

	if (!_essl_ptrset_has(visited_nodes, n) && n->expr.best_block == best_block)
	{
		unsigned int no_childs = GET_N_CHILDREN(n);
		
		ESSL_CHECK(_essl_ptrset_insert(visited_nodes, n));

		my_weight = desc->get_op_weight_realistic(n) * get_penalty_factor(no_childs);

		for (i = 0; i < no_childs; i++)
		{
			node *ss = GET_CHILD(n, i);
			if (ss)
			{
				int child_weight;
				ESSL_CHECK(_essl_calc_op_weight(ss, best_block, desc, visited_nodes, &child_weight));
				my_weight += child_weight;
			}
		}
	}

	*weight = my_weight;

	return MEM_OK;
}
