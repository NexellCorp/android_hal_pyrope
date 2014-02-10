/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "middle/optimise_constant_fold.h"
#include "middle/dominator.h"
#include "common/ptrdict.h"
#include "common/basic_block.h"
#include "frontend/constant_fold.h"
#include "middle/optimise_basic_blocks.h"

/* Special visit number encodings used for this pass:
   output postorder
   >= 0   >= 0      block has not yet been confirmed reachable
   >= 0   -1        block has been confirmed reachable
   -1     >= 0      block has been confirmed unreachable
*/

typedef struct _tag_optimise_constant_fold_context {
	constant_fold_context cf_ctx;
	mempool *temp_pool;
	control_flow_graph *cfg;
	ptrdict node_map;
} optimise_constant_fold_context;


static node *constant_fold_single(optimise_constant_fold_context *ctx, node *n)
{
	return _essl_constant_fold_single_node(&ctx->cf_ctx, n);
}

static essl_bool block_edge(basic_block *from, basic_block *to)
{
	unsigned i;
	if (from->output_visit_number == -1)
	{
		/* Block unreachable */
		return ESSL_FALSE;
	}
	for(i = 0; i < from->n_successors; ++i)
	{
		if(from->successors[i] == to) return ESSL_TRUE;
	}
	return ESSL_FALSE;
}

static int remove_dead_phi_sources(node *phi_node)
{
	phi_source **phi_s_p;
	int n_preds = 0;
	for (phi_s_p = &phi_node->expr.u.phi.sources ; *phi_s_p != NULL ;)
	{
		if (!block_edge((*phi_s_p)->join_block, phi_node->expr.u.phi.block))
		{
			/* Source block edge gone */
			*phi_s_p = (*phi_s_p)->next;
		} else {
			phi_s_p = &(*phi_s_p)->next;
			n_preds++;
		}
	}
	return n_preds;
}

static node *constant_fold(optimise_constant_fold_context *ctx, node *n)
{
	unsigned i;
	node *new_node;

	if (_essl_ptrdict_has_key(&ctx->node_map, n))
	{
		return _essl_ptrdict_lookup(&ctx->node_map, n);
	}

	if (n->hdr.kind == EXPR_KIND_PHI)
	{
		/* Remove sources in unreachable blocks */
		int n_preds = remove_dead_phi_sources(n);
		
		/* If only one source, thread the source through */
		assert(n_preds > 0);
		if (n_preds == 1)
		{
			/* Shortcut reference to phi node */
			phi_source *phi_s = n->expr.u.phi.sources;
			assert(phi_s != NULL && phi_s->next == NULL);
			new_node = phi_s->source;

			/* Remove phi node from block */
			{
				phi_list **phi_p;
				for (phi_p = &n->expr.u.phi.block->phi_nodes ; *phi_p != NULL ; phi_p = &(*phi_p)->next)
				{
					if ((*phi_p)->phi_node == n)
					{
						*phi_p = (*phi_p)->next;
						break;
					}
				}
			}
		}
		else
		{
			/* Keep phi node */
			new_node = n;
		}
	}
	else if (n->hdr.kind == EXPR_KIND_TRANSFER)
	{
		ESSL_CHECK(new_node = constant_fold(ctx, GET_CHILD(n, 0)));
	}
	else
	{
		for (i = 0 ; i < GET_N_CHILDREN(n) ; i++)
		{
			node *child = GET_CHILD(n, i);
			if (child != NULL)
			{
				ESSL_CHECK(child = constant_fold(ctx, child));
				SET_CHILD(n, i, child);
			}
		}

		ESSL_CHECK(new_node = constant_fold_single(ctx, n));
	}


	ESSL_CHECK(_essl_ptrdict_insert(&ctx->node_map, n, new_node));
	return new_node;
}


static memerr constant_fold_phi_sources(optimise_constant_fold_context *ctx, basic_block *phi_block, basic_block *source_block)
{
	phi_list *phi;

	if (phi_block == NULL)
	{
		return MEM_OK;
	}

	for (phi = phi_block->phi_nodes ; phi != NULL ; phi = phi->next)
	{
		phi_source *phi_s;
		for (phi_s = phi->phi_node->expr.u.phi.sources ; phi_s != NULL ; phi_s = phi_s->next)
		{
			if (phi_s->join_block == source_block)
			{
				ESSL_CHECK(phi_s->source = constant_fold(ctx, phi_s->source));
			}
		}
	}

	return MEM_OK;
}

static memerr constant_fold_block(optimise_constant_fold_context *ctx, basic_block *block)
{
	unsigned i;
	if (block->postorder_visit_number != -1)
	{
		/* Block is unreachable */
		block->output_visit_number = -1;
		return MEM_OK;
	}
	/* Fold phi sources */
	for(i = 0; i < block->n_successors; ++i)
	{
		ESSL_CHECK(constant_fold_phi_sources(ctx, block->successors[i], block));
	}

	/* Fold control dependent ops */
	{
		control_dependent_operation **cd_op_p;
		for (cd_op_p = &block->control_dependent_ops ; *cd_op_p != NULL ;)
		{
			ESSL_CHECK((*cd_op_p)->op = constant_fold(ctx, (*cd_op_p)->op));
			if ((*cd_op_p)->op->hdr.kind == EXPR_KIND_CONSTANT)
			{
				/* Operation folded away */
				*cd_op_p = (*cd_op_p)->next;
			} else {
				cd_op_p = &(*cd_op_p)->next;
			}
		}
	}

	if (block->source != NULL)
	{
		ESSL_CHECK(block->source = constant_fold(ctx, block->source));

		switch (block->termination)
		{
		case TERM_KIND_JUMP:
			/* Conditional jump - fold condition */
			if (block->source->hdr.kind == EXPR_KIND_CONSTANT)
			{
				float val = ctx->cf_ctx.desc->scalar_to_float(block->source->expr.u.value[0]);
				if (val != 0.0f)
				{
					/* Always true - optimise if forward jump */
					if (block->successors[BLOCK_TRUE_TARGET]->output_visit_number > block->output_visit_number)
					{
						block->successors[BLOCK_DEFAULT_TARGET] = block->successors[BLOCK_TRUE_TARGET];
						block->n_successors = 1;
						block->source = NULL;
					}
				} else {
					/* Always false - optimise if forward jump */
					if (block->successors[BLOCK_DEFAULT_TARGET]->output_visit_number > block->output_visit_number)
					{
						block->n_successors = 1;
						block->source = NULL;
					}
				}
			}
			break;

		case TERM_KIND_DISCARD:
			/* Conditional discard - fold condition */
			if (block->source->hdr.kind == EXPR_KIND_CONSTANT)
			{
				float val = ctx->cf_ctx.desc->scalar_to_float(block->source->expr.u.value[0]);
				if (val != 0.0f)
				{
					/* Always true */
					block->successors[BLOCK_DEFAULT_TARGET] = ctx->cfg->exit_block;
					block->n_successors = 1;
				} else {
					/* Always false */
					block->termination = TERM_KIND_JUMP;
					block->n_successors = 1;
				}
				block->source = NULL;
			}
			break;

		default:
			break;
		}

	}

	/* Mark successors reachable */
	for(i = 0; i < block->n_successors; ++i)
	{
		assert(block->successors[i]->output_visit_number != -1);
		block->successors[i]->postorder_visit_number = -1;
	}

	return MEM_OK;
}

static memerr constant_fold_function(optimise_constant_fold_context *ctx)
{
	int i;

	ESSL_CHECK(_essl_ptrdict_init(&ctx->node_map, ctx->temp_pool));

	/* Mark entry block reachable */
	ctx->cfg->entry_block->postorder_visit_number = -1;

	for (i = ctx->cfg->n_blocks-1 ; i >= 0 ; i--)
	{
		basic_block *block = ctx->cfg->postorder_sequence[i];
		ESSL_CHECK(constant_fold_block(ctx, block));
	}

	/* Exit block must be reachable */
	assert(ctx->cfg->exit_block->output_visit_number != -1);

	/* Clean out remaining dead phi sources */
	for (i = ctx->cfg->n_blocks-1 ; i >= 0 ; i--)
	{
		basic_block *block = ctx->cfg->postorder_sequence[i];
		phi_list *phi;
		for (phi = block->phi_nodes ; phi != NULL ; phi = phi->next)
		{
			remove_dead_phi_sources(phi->phi_node);
		}
	}

	/* Remove dead blocks */
	_essl_correct_output_sequence_list(ctx->cfg);

	/* The cfg is now in an inconsistent state, but this will be fixed up
	   by the subsequent dominator pass.
	*/

	return MEM_OK;
}

memerr _essl_optimise_constant_fold_nodes_and_blocks(pass_run_context *pr_ctx, symbol *func)
{
	memerr result = MEM_ERROR;
	optimise_constant_fold_context ocf_ctx;
	ESSL_CHECK(_essl_constant_fold_init(&ocf_ctx.cf_ctx, pr_ctx->pool, pr_ctx->desc, pr_ctx->err_ctx));

	ocf_ctx.temp_pool = pr_ctx->tmp_pool;
	ocf_ctx.cfg = func->control_flow_graph;

	result = constant_fold_function(&ocf_ctx);

	ESSL_CHECK(_essl_compute_dominance_information(pr_ctx->pool, func));
	_essl_validate_control_flow_graph(func->control_flow_graph);

	return result;
}



static memerr constant_fold_function_nodes(optimise_constant_fold_context *ctx)
{
	int j;

	ESSL_CHECK(_essl_ptrdict_init(&ctx->node_map, ctx->temp_pool));

	for (j = ctx->cfg->n_blocks-1 ; j >= 0 ; j--)
	{
		basic_block *block = ctx->cfg->postorder_sequence[j];
		unsigned i;
		/* Fold phi sources */
		for(i = 0; i < block->n_successors; ++i)
		{
			ESSL_CHECK(constant_fold_phi_sources(ctx, block->successors[i], block));
		}
		
		/* Fold control dependent ops */
		{
			control_dependent_operation **cd_op_p;
			for (cd_op_p = &block->control_dependent_ops ; *cd_op_p != NULL ;)
			{
				ESSL_CHECK((*cd_op_p)->op = constant_fold(ctx, (*cd_op_p)->op));
				if ((*cd_op_p)->op->hdr.kind == EXPR_KIND_CONSTANT)
				{
					/* Operation folded away */
					*cd_op_p = (*cd_op_p)->next;
				} else {
					cd_op_p = &(*cd_op_p)->next;
				}
			}
		}
		
		if (block->source != NULL)
		{
			ESSL_CHECK(block->source = constant_fold(ctx, block->source));
		}
	}

	return MEM_OK;
}

memerr _essl_optimise_constant_fold_nodes(mempool *pool, symbol *function, target_descriptor *desc, error_context *e_ctx)
{
	mempool temp_pool;
	memerr result = MEM_ERROR;
	optimise_constant_fold_context ocf_ctx;
	ESSL_CHECK(_essl_constant_fold_init(&ocf_ctx.cf_ctx, pool, desc, e_ctx));

	ESSL_CHECK(_essl_mempool_init(&temp_pool, 0, pool->tracker));
	ocf_ctx.temp_pool = &temp_pool;
	ocf_ctx.cfg = function->control_flow_graph;

	result = constant_fold_function_nodes(&ocf_ctx);

	_essl_mempool_destroy(&temp_pool);

	return result;
}
