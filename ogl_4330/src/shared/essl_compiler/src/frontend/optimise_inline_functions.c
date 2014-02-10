/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "frontend/optimise_inline_functions.h"
#include "common/symbol_table.h"
#include "frontend/callgraph.h"

/** A list of call nodes for each function */
typedef struct _tag_function_call {
	struct _tag_function_call *next; /**< Next phi source in phi node list */
	control_dependent_operation *function; /**< The expression containing the source definition */
} function_call;
ASSUME_ALIASING(function_call, generic_list);

typedef struct _tag_function_data {
	int use_count;	/** number of times this function is called from the function being optimised */
	int node_count; /** number of nodes in the function body */
	int block_count; /** number of basic blocks in the function body */
	function_call* calls; /** Linked list of pointers to the call nodes */
} function_data;


#if 0 /* disable unused functions due to inlining heuristics not finished yet */

/** count_function_nodes
 * Return a nested accumulation of the number of nodes under the node specified
 */
static int count_nodes(/*@null@*/node* body)
{
	int count = 0;	
	unsigned int children;
	unsigned int child;
	/* ignore TRANSFER nodes, as these are effectively no-ops */
	if(body != 0 && body->hdr.kind != EXPR_KIND_TRANSFER)
	{
		count = 1; /* Start with this node itself */
		children = GET_N_CHILDREN(body);
		for(child = 0; child < children; child++)
			count += count_nodes(GET_CHILD(body, child));
	}
	return count;
}

static memerr calculate_function_size(function_data* data, symbol *function)
{
	basic_block *b;
	control_flow_graph *graph = function->control_flow_graph;
	control_dependent_operation *g;
	phi_list* phi;

	ESSL_CHECK(graph);
	
	if(graph != 0)
	{
		/* First, build a dict of all function calls within this function */
		for(b = graph->entry_block; b != 0; b = b->next)
		{
			/* increment the number of blocks in the body */
			data->block_count++;

			/* count any nodes used by the source */
			if(b->source != 0)
				data->node_count += count_nodes(b->source);

			/* iterate over the phi nodes accumulating the contributing node count */
			for(phi = b->phi_nodes; phi != 0; phi = phi->next)
				data->node_count += count_nodes(phi->phi_node);

			/* iterate over the control_dependent_ops accumulating the node count */
			for(g = b->control_dependent_ops; g != 0; g = g->next)
				data->node_count += count_nodes(g->op);
		}
	}
	return MEM_OK;
}

#endif /* end disable functions */

/** Initialise the function inlining optimiser
  */
  
memerr _essl_optimise_inline_functions_init(optimise_inline_functions_context *ctx, 
				    error_context *err, mempool *pool)
{
	ctx->err = err;
	ctx->pool = pool;
	ctx->function = 0;

	return MEM_OK;
} 

/** Do a deep clone of the specified node. Fills in the cloned_nodes dict in ctx.
  */
static memerr clone_node(optimise_inline_functions_context *ctx, node* orig, /*@out@*/node** clone)
{
	unsigned int n;
	
	/* check if we've already cloned this node */
	(*clone) = _essl_ptrdict_lookup(&ctx->cloned_nodes, orig);
	
	if((*clone) != 0)
		return MEM_OK;

	ESSL_CHECK(*clone = _essl_clone_node(ctx->pool, orig));

	/* store the cloned node in the ptrdict */
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->cloned_nodes, orig, (*clone)));

	/* this is a deep clone, so clone the children */
	for(n = 0; n < GET_N_CHILDREN(orig); n++)
	{
		node* child = GET_CHILD(orig, n);
		if(child != 0)
		{
			node* child_clone;
			ESSL_CHECK(clone_node(ctx, child, &child_clone));
			SET_CHILD((*clone), n, child_clone);
		}
	}	
	
	/* If a phi node, clone its sources too */
	if(orig->hdr.kind == EXPR_KIND_PHI)
	{
		phi_source *source;
		(*clone)->expr.u.phi.sources = 0;
		source = orig->expr.u.phi.sources;
		while(source != 0)
		{
			phi_source* src;
			node *s_node = 0;
			src = LIST_NEW(ctx->pool, phi_source);
			ESSL_CHECK(src);
			ESSL_CHECK(clone_node(ctx, source->source, &s_node));
			ESSL_CHECK(src->join_block = _essl_ptrdict_lookup(&ctx->cloned_blocks, source->join_block));
			src->source = s_node;
			LIST_INSERT_FRONT(&(*clone)->expr.u.phi.sources, src);
			source = source->next;
		}
	}
	
	/* If a function call node, clone the parameter STORE/LOAD list */
	if(orig->hdr.kind == EXPR_KIND_FUNCTION_CALL)
	{
		parameter *arg;
		(*clone)->expr.u.fun.arguments = 0;
		arg = orig->expr.u.fun.arguments;
		while(arg != 0)
		{
			parameter* new_arg;
			storeload_list *l_list = 0;
			storeload_list *s_list = 0;
			new_arg = LIST_NEW(ctx->pool, parameter);
			ESSL_CHECK(new_arg);
			if(arg->store != 0)
			{
				storeload_list *p = arg->store;
				while(0 != p)
				{
					node *s_node;
					storeload_list *sl;
					ESSL_CHECK(sl = _essl_mempool_alloc(ctx->pool, sizeof(*sl)));
					ESSL_CHECK(clone_node(ctx, p->n, &s_node));
					LIST_INSERT_BACK(&s_list, sl);
					p = p->next;
				}
			}
			if(arg->load != 0)
			{
				storeload_list *p = arg->load;
				while(0 != p)
				{
					node *l_node;
					storeload_list *ll;
					ESSL_CHECK(ll = _essl_mempool_alloc(ctx->pool, sizeof(*ll)));
					ESSL_CHECK(clone_node(ctx, p->n, &l_node));
					LIST_INSERT_BACK(&l_list, ll);
					p = p->next;
				}
			}
			new_arg->store = s_list;
			new_arg->load = s_list;
			new_arg->sym = arg->sym;
			LIST_INSERT_BACK(&(*clone)->expr.u.fun.arguments, new_arg);
			arg = arg->next;
		}
	}
	
	return MEM_OK;
}

/** Perform a deep clone of a basic block tree. Clones nodes and structures.
  * \note Does not fixup control dependency graph, should only be called prior to setting
  *        up the CDG.
  */
  
static memerr clone_basic_block(optimise_inline_functions_context *ctx, basic_block* block, basic_block** clone)
{
	phi_list* phi;
	control_dependent_operation* cto;
	unsigned i;
	ESSL_CHECK(*clone = _essl_new_basic_block_with_n_successors(ctx->pool, block->n_successors));
	(*clone)->n_successors = block->n_successors;

	/* Store an entry in the clone map for this newly cloned block */
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->cloned_blocks, block, (*clone)));

	(*clone)->termination = block->termination;

	/* Clone the block list first, as the phi nodes will need the 
	 * cloned block information to fixup the join_block members. */
	if(block->next != 0)
	{
		basic_block *next, *next_clone;
		next = block->next;
		ESSL_CHECK(clone_basic_block(ctx, next, &next_clone));
		(*clone)->next = next_clone;
	}
	
	/* Clone the control dependent operations */
	for(cto = block->control_dependent_ops; cto != 0; cto = cto->next)
	{
		control_dependent_operation *new_cto;
		node *cto_node;
		ESSL_CHECK(new_cto = LIST_NEW(ctx->pool, control_dependent_operation));
		ESSL_CHECK(clone_node(ctx, cto->op, &cto_node));
		ESSL_CHECK(cto_node);
		new_cto->block = *clone;
		new_cto->op = cto_node;
		ESSL_CHECK(_essl_ptrdict_insert(ctx->control_dependence, cto_node, new_cto));
		LIST_INSERT_BACK(&(*clone)->control_dependent_ops, new_cto);
	}	
	
	/* Clone the phi nodes */
	phi = block->phi_nodes;
	while(phi != 0)
	{
		node *phi_node;
		phi_list *lst = LIST_NEW(ctx->pool, phi_list);
		ESSL_CHECK(lst);
		ESSL_CHECK(clone_node(ctx, phi->phi_node, &phi_node));
		ESSL_CHECK(phi_node);
		lst->phi_node = phi_node;
		phi_node->expr.u.phi.block = (*clone);
		lst->sym = phi->sym;
		LIST_INSERT_FRONT(&(*clone)->phi_nodes, lst);
		phi = phi->next;
	}
	
	/* Fill in the immediate dominator */
	if(block->immediate_dominator != 0)
		(*clone)->immediate_dominator = _essl_ptrdict_lookup(&ctx->cloned_blocks, block->immediate_dominator);
	
	/* Fill in the targets */
	for(i = 0; i < block->n_successors; ++i)
	{
		ESSL_CHECK((*clone)->successors[i] = _essl_ptrdict_lookup(&ctx->cloned_blocks, block->successors[i]));
	}

	if(block->source != 0)
	{
		node* source;
		ESSL_CHECK(clone_node(ctx, block->source, &source));
		(*clone)->source = source;
	}

	/* Fill in block cost */
	(*clone)->cost = block->cost;

	return MEM_OK;
}

/** Split a basic block at the specified control dependent operation.
  * \note Does not attempt to fixup the local operations.
  */
static memerr split_basic_block(optimise_inline_functions_context *ctx, basic_block* block, control_dependent_operation* split_point)
{
	basic_block* b2;
	control_dependent_operation *cd_op;
	unsigned i;
	
	ESSL_CHECK(b2 = _essl_new_basic_block_with_n_successors(ctx->pool, block->n_successors));
	b2->n_successors = block->n_successors;
	for(i = 0; i < block->n_successors; ++i)
	{
		b2->successors[i] = block->successors[i];
	}


	LIST_INSERT_FRONT(&block->next, b2);

	b2->termination = block->termination;
	b2->source = block->source;
	b2->cost = block->cost;
	block->n_successors = 1;
	block->successors[BLOCK_DEFAULT_TARGET] = b2;
	block->termination = TERM_KIND_JUMP;
	block->source = 0;

	/* Copy the remaining control dependent ops after the 'call' to the new block */
	b2->control_dependent_ops = split_point->next;
	split_point->next = 0;
	for (cd_op = b2->control_dependent_ops ; cd_op != NULL ; cd_op = cd_op->next)
	{
		cd_op->block = b2;
	}

	return MEM_OK;
}

static essl_bool called_only_once(symbol *function)
{
	assert(function->call_count > 0);
	return function->call_count == 1;
}

static void decrement_call_count(symbol *function)
{
	assert(function->call_count > 0);
	function->call_count -= 1;
}

static node *get_node_or_clone(optimise_inline_functions_context *ctx, node *n, symbol *function)
{
	if (called_only_once(function))
	{
		return n;
	} else {
		node *clone = _essl_ptrdict_lookup(&ctx->cloned_nodes, n);
		return clone;
	}
}

/** Perform inlining of the given function at the specified call point.
 */
static memerr inline_function(optimise_inline_functions_context *ctx, control_dependent_operation* call, basic_block* insert_point,
							  control_flow_graph *parent_func)
{
	symbol* func = call->op->expr.u.sym;
	control_flow_graph* graph = func->control_flow_graph;
	basic_block *b2, *b3, *b4, *last_block = 0;
	parameter *arg, *param;

	/* Now split the block that contains this call in two at the call */
	ESSL_CHECK(split_basic_block(ctx, insert_point, call));
	/* Point to the new block created by the split */
	b2 = insert_point->next;
		
	/* Bring the blocks from the function body into the caller */
	if (called_only_once(func))
	{
		/* Get callee blocks */
		b3 = graph->entry_block;
	} else {
		/* Called more than once - clone it */
		ESSL_CHECK(_essl_ptrdict_init(&ctx->cloned_nodes, &ctx->temp_pool));
		ESSL_CHECK(_essl_ptrdict_init(&ctx->cloned_blocks, &ctx->temp_pool));
		ESSL_CHECK(clone_basic_block(ctx, graph->entry_block, &b3));
	}
	assert(b3 != 0);

	/* Update block costs and control dependent op map, and find last block in list */
	for(b4 = b3; b4 != 0; b4 = b4->next)
	{
		/* Insert control dependent operations in the parent map */
		control_dependent_operation *cd_op;
		for (cd_op = b4->control_dependent_ops ; cd_op != 0 ; cd_op = cd_op->next)
		{
			ESSL_CHECK(_essl_ptrdict_insert(&parent_func->control_dependence, cd_op->op, cd_op));
		}

		/* Correct block cost */
		b4->cost *= insert_point->cost;
		assert(b4->cost >= 0.0);

		last_block = b4;
	}

	/* Link exit block of inlined function to insert point */
	last_block->termination = TERM_KIND_JUMP;
	last_block->n_successors = 1;
	last_block->successors[BLOCK_DEFAULT_TARGET] = insert_point->successors[BLOCK_DEFAULT_TARGET];
	last_block->next = insert_point->next;
	insert_point->next = b3;
	insert_point->successors[BLOCK_DEFAULT_TARGET] = b3;
	
	/* Fixup the immediate dominator pointers for the split and new blocks */
	/* entry point of cloned function body is immediately dominated by the first half of the original block */
	b3->immediate_dominator = insert_point; 
	/* second half of the original block is immediately dominated by the exit block of the cloned function body */
	b2->immediate_dominator = last_block;

	/* fixup phi node references that used to point to block but now need to point to b2 */
	{
		unsigned i;
		for(i = 0; i < b2->n_successors; ++i)
		{
			basic_block *c = b2->successors[i];
			phi_list *phi;
			for (phi = c->phi_nodes ; phi != 0 ; phi = phi->next) {
				phi_source *ps;
				for (ps = phi->phi_node->expr.u.phi.sources ; ps != 0 ; ps = ps->next) {
					if(ps->join_block == insert_point)
					{
						ps->join_block = b2;
					}
				}
			}
		}
	}
		
	/* forward all the argument STORE's to the parameter LOAD's */
	arg = call->op->expr.u.fun.arguments;
	param = graph->parameters;
	while(arg != 0 && param != 0)
	{
		/* If the parameter is in or inout, link up the LOAD from the callee with the STORE from the caller */
		if(param->load !=0 && arg->store != 0)
		{
			/* Link the second child of the STORE to the LOAD */
			node* load_clone;
			storeload_list* store = arg->store;
			storeload_list* load = param->load;
			while(0 != store)
			{
				node* child;
				assert(0 != load);	/* Check that the store and load lists match up safely */
				child = GET_CHILD(store->n, 1);
				if(store->n->hdr.is_control_dependent)
				{
					_essl_remove_control_dependent_op_node(&insert_point->control_dependent_ops, store->n);
					store->n->hdr.is_control_dependent = ESSL_FALSE;
				}
				load_clone = get_node_or_clone(ctx, load->n, func);

				if(load_clone) /* only process if the load isn't dead */
				{
					_essl_rewrite_node_to_transfer(load_clone, child);
					/* remove the TRANSFER node from the CDG of the entry block */
					if(load_clone->hdr.is_control_dependent)
					{
						_essl_remove_control_dependent_op_node(&b3->control_dependent_ops, load_clone);
						load_clone->hdr.is_control_dependent = ESSL_FALSE;
					}
				}
				store=store->next;
				load=load->next;
			}
		} else if(arg->store != 0 && param->load == 0)
		{
			/* store to a control dependent variable - rewrite the store */
			storeload_list* store;
			for(store = arg->store; store != 0; store = store->next)
			{
				node *var_ref = GET_CHILD(store->n, 0);
				while(var_ref != 0 && var_ref->hdr.kind != EXPR_KIND_VARIABLE_REFERENCE)
				{
					var_ref = GET_CHILD(var_ref, 0);
				}
				assert(var_ref != 0);
				var_ref->expr.u.sym = param->sym;
				store->n->expr.u.load_store.address_space = param->sym->address_space;
			}
		}
		assert(! ( arg->store == 0 && param->load != 0));

		
		/* if the param is out or inout, it will have a matching store/load on exit. */
		/* link the load to the child of the store and rewrite it as a transfer to eliminate the load */
		if(param->store != 0 && arg->load != 0)
		{
			storeload_list* load = arg->load;
			storeload_list* store = param->store;
			while(0 != load)
			{
				node* store_clone;
				node* child;
				assert(0 != store);	/* Check that the store and load lists match up safely */
				store_clone = get_node_or_clone(ctx, store->n, func);
				child = GET_CHILD(store_clone, 1);
				_essl_rewrite_node_to_transfer(load->n, child);
				if(store_clone->hdr.is_control_dependent)
				{
					_essl_remove_control_dependent_op_node(&last_block->control_dependent_ops, store_clone);
					store_clone->hdr.is_control_dependent = ESSL_FALSE;
				}
				if(load->n->hdr.is_control_dependent)
				{
					_essl_remove_control_dependent_op_node(&last_block->next->control_dependent_ops, load->n);
					load->n->hdr.is_control_dependent = ESSL_FALSE;
				}
				load = load->next;
				store = store->next;
			}
		}
		else if(arg->load != 0 && param->store == 0)
		{
			/* load of a control dependent variable - rewrite the load */
			storeload_list* load;
			for(load = arg->load; load != 0; load = load->next)
			{
				node *var_ref = GET_CHILD(load->n, 0);
				while(var_ref != 0 && var_ref->hdr.kind != EXPR_KIND_VARIABLE_REFERENCE)
				{
					var_ref = GET_CHILD(var_ref, 0);
				}
				assert(var_ref != 0);
				var_ref->expr.u.sym = param->sym;
				load->n->expr.u.load_store.address_space = param->sym->address_space;
			}
		}
		assert(! ( arg->load == 0 && param->store != 0));

		arg = arg->next;
		param = param->next;
	}

	/* Remove the CALL from the CDO list for the calling block */
	if(call->op->hdr.is_control_dependent)
	{
		_essl_remove_control_dependent_op_node(&insert_point->control_dependent_ops, call->op);
		call->op->hdr.is_control_dependent = ESSL_FALSE;
	}

	/* and rewrite it to a transfer to the phi node of the exit block of the inlined function */
	if (last_block->source)
	{
		_essl_rewrite_node_to_transfer(call->op, last_block->source);
		last_block->source = 0;
	} else {
		/* void function - make call node invalid */
		call->op->hdr.kind = EXPR_KIND_UNKNOWN;
	}

	/* Function is now called in one place less */
	decrement_call_count(func);

	return MEM_OK;
}

/** Perform function inlining for all appropriate function calls within the specified function body.
 */

memerr _essl_optimise_inline_functions(optimise_inline_functions_context *ctx, symbol *function)
{
	basic_block *b;
	control_flow_graph *graph = function->control_flow_graph;

	ESSL_CHECK(graph);
	ctx->control_dependence = &function->control_flow_graph->control_dependence;
	/* Iterate the identified function calls checking thier size, and looking
	 * for candidates for inlining
	 */
	
	for(b = graph->entry_block; b != 0; b = b->next)
	{
		/* Look in control_dependent_ops for function calls, as this is the
		 * only place they can exist after the ssa transformation
		 */
		control_dependent_operation *g;
		for(g = b->control_dependent_ops; g != 0; g = g->next)
		{
			/* Check if it's a FUNCTION_CALL node */
			if(g->op->hdr.kind == EXPR_KIND_FUNCTION_CALL)
			{
				/* If the function is deemed to be 'inlineable', then we can do that now */
				/** NOTE: This is a fake test which should be changed when not everything is inlined. */
				if(g->op->expr.u.sym->calls_from != 0)	
				{
					memerr ret;
					ESSL_CHECK(_essl_mempool_init(&ctx->temp_pool, 0, _essl_mempool_get_tracker(ctx->pool)));
					ret = inline_function(ctx, g, b, graph);
					_essl_mempool_destroy(&ctx->temp_pool);
					ESSL_CHECK(ret);
					break;
				}
			}
		}
	}
	
	return MEM_OK;
}
