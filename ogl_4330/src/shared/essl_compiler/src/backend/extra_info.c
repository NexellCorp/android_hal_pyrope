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

#include "backend/extra_info.h"


static node_extra *new_extra_info(mempool *pool)
{
	node_extra *ne;
	ESSL_CHECK(ne = _essl_mempool_alloc(pool, sizeof(node_extra)));
	ne->earliest_use = -999999;
	ne->latest_use = 9999999;
	ne->visited = 0;
	ne->u.m200_modifiers.exponent_adjust = 0;
	ne->u.m200_modifiers.trans_node = NULL;
	ne->u.m200_modifiers.swizzle = _essl_create_identity_swizzle(N_COMPONENTS);

	ne->u.m200_modifiers.mode = M200_OUTPUT_NORMAL;

	return ne;
}

/*@null@*/ node_extra *_essl_create_extra_info(mempool *pool, node *n)
{
	node_extra *ne = new_extra_info(pool);
	assert(n->expr.info == 0);
	n->expr.info = ne;
	return ne;
}

static memerr init_info(extra_info_context *ctx, node *n, node_extra *ne) 
{
	/* Initialize use count and depth */
	ne->original_use_count = ne->unscheduled_use_count = n->hdr.is_control_dependent ? 1 : 0;
	ne->scheduled_use_count = 0;
	ne->operation_depth = ctx->op_weight(n);
	ne->visited = 1;
	return MEM_OK;
}

static node_extra *create_init_info_if_necessary(extra_info_context *ctx, node *n)
{
	node_extra *ne;
	if(!HAS_EXTRA_INFO(n))
	{
		ESSL_CHECK(CREATE_EXTRA_INFO(ctx->pool, n));
	}
	ne = EXTRA_INFO(n);
	if(ne->visited == 0)
	{
		ESSL_CHECK(init_info(ctx, n, ne));
	}
	return ne;
}

static memerr handle_dependency_pass_1(extra_info_context *ctx, node *source, node *dest, int use) 
{
	node_extra *sne, *dne;
	essl_bool continue_visit = ESSL_FALSE;
	int weight = ctx->op_weight(dest);
	if (!HAS_EXTRA_INFO(source))
	{
		ESSL_CHECK(CREATE_EXTRA_INFO(ctx->pool, source));
	}
	sne = EXTRA_INFO(source);

	if(sne->visited == 0)
	{
		/* Fill in initial information */
		ESSL_CHECK(init_info(ctx, source, sne));
		/* Recurse on all sources of the source */
		continue_visit = ESSL_TRUE;
	}
	dne = EXTRA_INFO(dest);

	if(continue_visit)
	{
		size_t i;
		/* Recurse on children */
		for (i = 0 ; i < GET_N_CHILDREN(source) ; i++)
		{
			node *ss = GET_CHILD(source, i);
			if(ss)
			{
				ESSL_CHECK(handle_dependency_pass_1(ctx, ss, source, 1));
			}
		}
		/* Recurse on control dependencies */
		if (source->hdr.is_control_dependent)
		{
			control_dependent_operation *cd_op;
			operation_dependency *dep;
			cd_op = _essl_ptrdict_lookup(ctx->control_dependence, source);
			assert(cd_op != NULL);
			assert(cd_op->block != NULL && cd_op->block->postorder_visit_number >= 0);
			if (cd_op->dependencies != 0)
			{
				/* Explicit dependencies on other operations */
				for (dep = cd_op->dependencies ; dep != 0 ; dep = dep->next)
				{
					node *ss = dep->dependent_on->op;
					ESSL_CHECK(handle_dependency_pass_1(ctx, ss, source, 1));
				}
			}
			else
			{
				/* Implicitly dependent on block top */
				ESSL_CHECK(handle_dependency_pass_1(ctx, ctx->current_block->top_depth, source, 0));
			}
		}
	}

	/* Update use count and operation depth */
	{
		sne->unscheduled_use_count += use;
		sne->original_use_count = sne->unscheduled_use_count;
		if (sne->operation_depth + weight > dne->operation_depth)
		{
			dne->operation_depth = sne->operation_depth + weight;
		}
	}

	return MEM_OK;
}


static memerr init_top_bottom_depth(extra_info_context *ctx, basic_block *block)
{
	ESSL_CHECK(create_init_info_if_necessary(ctx, block->top_depth));
	ESSL_CHECK(create_init_info_if_necessary(ctx, block->bottom_depth));
	return MEM_OK;
}

static memerr handle_block_pass_1(extra_info_context *ctx, basic_block *block)
{

	node_extra *top_info;
	/* Set current block */
	ctx->current_block = block;

	assert(block->cost >= 0.0);
	assert(block->top_depth != NULL && block->bottom_depth != NULL);

	/* Init pseudo-nodes representing the block top and bottom operations depths */
	if (!HAS_EXTRA_INFO(block->top_depth))
	{
		ESSL_CHECK(init_top_bottom_depth(ctx, block));
	}
	top_info = EXTRA_INFO(block->top_depth);


	/* Set top exit depth to max of the bottom exit depths of already processed predecessor blocks */
	{
		predecessor_list *pred_list;
		for (pred_list = block->predecessors ; pred_list != 0 ; pred_list = pred_list->next) {
			if (pred_list->block->postorder_visit_number > block->postorder_visit_number) {
				node_extra *pred_bottom_info = EXTRA_INFO(pred_list->block->bottom_depth);
				if (pred_bottom_info->operation_depth > top_info->operation_depth) {
					top_info->operation_depth = pred_bottom_info->operation_depth;
				}
			}
		}
	}

	/* Initialize phi nodes to have block top depth */
	{
		phi_list *phi;
		for (phi = block->phi_nodes ; phi != 0 ; phi = phi->next) {
			node_extra *phi_extra;
			ESSL_CHECK(phi_extra = create_init_info_if_necessary(ctx, phi->phi_node));
			phi_extra->operation_depth = top_info->operation_depth;
		}
	}

	/* Handle all phi nodes of successor blocks */
	{
		unsigned i;
		for(i = 0; i < block->n_successors; ++i)
		{
			basic_block *succ = block->successors[i];
			phi_list *phi;
			for (phi = succ->phi_nodes ; phi != 0 ; phi = phi->next) {
				phi_source *phi_s;
				for (phi_s = phi->phi_node->expr.u.phi.sources ; phi_s != 0 ; phi_s = phi_s->next) {
					if (phi_s->join_block == block) {
						ESSL_CHECK(handle_dependency_pass_1(ctx, phi_s->source, block->bottom_depth, 1));
						break;
					}
				}
			}
		}
	}


	/* Handle block source */
	if (block->source != 0) {
		ESSL_CHECK(handle_dependency_pass_1(ctx, block->source, block->bottom_depth, 1));
	}

	{
		control_dependent_operation *cd_op;
		for (cd_op = block->control_dependent_ops ; cd_op != 0 ; cd_op = cd_op->next) {
			ESSL_CHECK(handle_dependency_pass_1(ctx, cd_op->op, block->bottom_depth, 0));
		}
	}

	/* Bottom depends on top */
	ESSL_CHECK(handle_dependency_pass_1(ctx, block->top_depth, block->bottom_depth, 0));

	return MEM_OK;
}



memerr _essl_calculate_extra_info(control_flow_graph *cfg, op_weight_fun op_weight, mempool *pool)
{
	extra_info_context ctx;
	int i;
	
	ctx.pool = pool;
	ctx.cfg = cfg;
	ctx.op_weight = op_weight;
	ctx.control_dependence = &cfg->control_dependence;

	for (i = cfg->n_blocks-1 ; i >= 0 ; i--)
	{
		ESSL_CHECK(handle_block_pass_1(&ctx, cfg->postorder_sequence[i]));
	}

	return MEM_OK;
}



essl_bool _essl_address_symbol_lists_equal(const symbol_list *a, const symbol_list *b)
{
	while(a != NULL && b != NULL)
	{
		if(a->sym != b->sym) return ESSL_FALSE;
		a = a->next;
		b = b->next;
	}
	return a == NULL && b == NULL; /* test if equal length */
}
