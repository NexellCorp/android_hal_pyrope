/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "middle/optimise_basic_blocks.h"
#include "middle/dominator.h"
#include "common/essl_list.h"
#include "common/find_blocks_for_operations.h"

typedef struct _tag_optimise_basic_blocks_context {
	mempool *pool;
	target_descriptor *desc;
	control_flow_graph *cfg;
	ptrset visited;
} optimise_basic_blocks_context;

/* Correct predecessor lists and phi join blocks */
static void correct_predecessor_and_phi(basic_block *block, basic_block *old_block, basic_block *new_block)
{
	if (block != 0)
	{
		predecessor_list *pred;
		phi_list *phi;

		for (pred = block->predecessors ; pred != 0 ; pred = pred->next)
		{
			if (pred->block == old_block)
			{
				pred->block = new_block;
			}
		}

		for (phi = block->phi_nodes ; phi != 0 ; phi = phi->next)
		{
			phi_source *phi_s;
			for (phi_s = phi->phi_node->expr.u.phi.sources ; phi_s != 0 ; phi_s = phi_s->next)
			{
				if (phi_s->join_block == old_block)
				{
					phi_s->join_block = new_block;
				}
			}
		}
	}

}

static void merge_block_sequence(basic_block *block, basic_block *succ_block, control_flow_graph *cfg)
{
	control_dependent_operation **control_dependent_ops_end = LIST_FIND(&block->control_dependent_ops, 0, control_dependent_operation);
	control_dependent_operation *cd_op;
	phi_list *phi;
	unsigned i;

	assert(block->output_visit_number != -1);
	assert(succ_block->output_visit_number != -1);

#ifndef NDEBUG
	{
		control_dependent_operation *cd_op;
		for(cd_op = block->control_dependent_ops; cd_op != NULL; cd_op = cd_op->next)
		{
			assert(cd_op->block == block);
		}
	}
#endif

	/* Rewrite phi nodes to transfers */
	for (phi = succ_block->phi_nodes ; phi != NULL ; phi = phi->next)
	{
		node *phi_node = phi->phi_node;
		phi_source *source = phi_node->expr.u.phi.sources;
		assert(source != NULL && source->next == NULL);
		_essl_rewrite_node_to_transfer(phi_node, source->source);
	}

	for(cd_op = succ_block->control_dependent_ops; cd_op != NULL; cd_op = cd_op->next)
	{
		assert(cd_op->block == succ_block);
		cd_op->block = block;
	}

	assert(*control_dependent_ops_end == NULL);
	*control_dependent_ops_end = succ_block->control_dependent_ops;
	succ_block->control_dependent_ops = NULL;

	block->termination = succ_block->termination;
	assert(succ_block->n_successors <= MIN_SUCCESSORS_ALLOCATED);
	block->n_successors = succ_block->n_successors;
	for(i = 0; i < succ_block->n_successors; ++i)
	{
		block->successors[i] = succ_block->successors[i];
	}

	block->source = succ_block->source;

	for(i = 0; i < succ_block->n_successors; ++i)
	{
		correct_predecessor_and_phi(succ_block->successors[i], succ_block, block);
	}

	/* Mark block as removed */
	succ_block->output_visit_number = -1;
	if (succ_block == cfg->exit_block)
	{
		cfg->exit_block = block;
	}

}

static essl_bool only_one_predecessor(basic_block *block)
{
	return block->predecessors != 0 && block->predecessors->next == 0;
}


/**
 * Check if one basic block follows another (handle deleted blocks in between)
 *
 * block2 follows block1 if there are no remaining blocks in between (output order).
 * block2 is in this case said to be a "local" block for block1.
 *
 * @param block1 First basic block
 * @param block2 Second basic block
 * @param cfg Control flow graph
 * @return Returns ESSL_TRUE if block1 is directly followed by block1
 */
static essl_bool in_output_order(basic_block *block1, basic_block *block2, control_flow_graph *cfg)
{
	int i;

	if (block1->output_visit_number >= block2->output_visit_number)
	{
		return ESSL_FALSE;
	}

	for (i = block1->output_visit_number + 1; i < block2->output_visit_number; i++)
	{
		if (cfg->output_sequence[i]->output_visit_number != -1)
		{
			return ESSL_FALSE; /* found a non-deleted block in between */
		}
	}

	return ESSL_TRUE; /* no blocks found in between, ergo block1 is directly followed by block2 */
}

static essl_bool can_be_merged_with_successor(basic_block *block, basic_block **succ_block_out, control_flow_graph *cfg)
{
	return
		block->termination == TERM_KIND_JUMP &&
		block->source == NULL &&
		(*succ_block_out = block->successors[BLOCK_DEFAULT_TARGET]) != NULL &&
		only_one_predecessor(*succ_block_out) &&
		(in_output_order(block, *succ_block_out, cfg) || /* successor must follow (output order)... */
		 (*succ_block_out)->n_successors < 2);        /* ... or not be a conditional jump */
}

void _essl_correct_output_sequence_list(control_flow_graph *cfg)
{
	/* Correct output sequence list */
	int n = 1;
	basic_block **next = &cfg->entry_block;
	basic_block *block;
	for (block = cfg->entry_block ; block != 0 ; block = block->next)
	{
		if (block->output_visit_number != -1 && block != cfg->exit_block)
		{
			/* Block is still here */
			*next = block;
			next = &block->next;
			n++;
		}
	}
	/* Exit block last */
	*next = cfg->exit_block;
	cfg->exit_block->next = 0;
	cfg->n_blocks = n;
}

memerr _essl_optimise_basic_block_sequences(pass_run_context *pr_ctx, symbol *func)
{
	control_flow_graph *cfg = func->control_flow_graph;
	basic_block *block;

	for (block = cfg->entry_block ; block != 0 ; block = block->next)
	{
		if (block->output_visit_number != -1)
		{
			basic_block *succ_block = NULL;
			while (can_be_merged_with_successor(block, &succ_block, cfg))
			{
				/* Merge blocks */
				merge_block_sequence(block, succ_block, cfg);
			}
		}
	}

	_essl_correct_output_sequence_list(cfg);
	ESSL_CHECK(_essl_compute_dominance_information(pr_ctx->pool, func));
	_essl_validate_control_flow_graph(func->control_flow_graph);

	return MEM_OK;

}

static essl_bool can_be_joined_into_successor(optimise_basic_blocks_context *ctx, basic_block *block, basic_block **succ_block_out)
{
	basic_block *succ;
	predecessor_list *pred;

	/* Check that block has no control-dependent operations */
	if (block->control_dependent_ops != NULL) return ESSL_FALSE;

	/* Check that block has a single successor */
	if (!(block->termination == TERM_KIND_JUMP &&
		  block->source == NULL &&
		  block->n_successors == 1))
	{
		return ESSL_FALSE;
	}

	succ = block->successors[BLOCK_DEFAULT_TARGET];
	*succ_block_out = succ;

	/* Check that succ has more than one predecessor */
	if (only_one_predecessor(succ)) return ESSL_FALSE;

	/* All predecessors of the block with two successors must have at least one successor
	   directly following it after the transformation */
	for (pred = block->predecessors ; pred != 0 ; pred = pred->next)
	{
		basic_block *pred_block = pred->block;
		basic_block *sibling_block = NULL;
		if(pred_block->n_successors < 2) continue;
		if(pred_block->n_successors > 2) return ESSL_FALSE;
		
		sibling_block = pred_block->successors[BLOCK_DEFAULT_TARGET] == block
			? pred_block->successors[BLOCK_TRUE_TARGET]
			: pred_block->successors[BLOCK_DEFAULT_TARGET];

		if (sibling_block != NULL &&
			!(in_output_order(pred_block, sibling_block, ctx->cfg) ||
			  (in_output_order(pred_block, block, ctx->cfg) &&
			   (in_output_order(block, succ, ctx->cfg) || in_output_order(block, sibling_block, ctx->cfg)))))
		{
			return ESSL_FALSE;
		}
	}

	/* Check that the blocks have no common predecessors */
	{
		predecessor_list *block_pred;
		for (block_pred = block->predecessors ; block_pred != NULL ; block_pred = block_pred->next)
		{
			predecessor_list *succ_pred;
			for (succ_pred = succ->predecessors ; succ_pred != NULL ; succ_pred = succ_pred->next)
			{
				if (block_pred->block == succ_pred->block) return ESSL_FALSE;
			}
		}
	}

	/* If block has exactly one predecessor and no phi nodes, we are good */
	if (block->predecessors != NULL && block->predecessors->next == NULL && block->phi_nodes == NULL)
	{
		/* We could eliminate this block, but it is probably a bad idea if some operations only need
		   to be executed if this block is executed. Only eliminate the block if no phi sources for
		   the block have this block as the best block.
		*/
		int weight = 0;
		phi_list *phi;
		ESSL_CHECK(_essl_ptrset_clear(&ctx->visited));
		for (phi = succ->phi_nodes ; phi != NULL ; phi = phi->next)
		{
			phi_source *phi_s;
			for (phi_s = phi->phi_node->expr.u.phi.sources ; phi_s != NULL ; phi_s = phi_s->next)
			{
				if (phi_s->join_block == block)
				{
					int op_weight;
					ESSL_CHECK(_essl_calc_op_weight(phi_s->source, block, ctx->desc, &ctx->visited, &op_weight));
					weight += op_weight;
				}
			}
		}

		if (weight < ctx->desc->blockelim_weight_limit)
		{
			return ESSL_TRUE;
		}
	}

	if(block->phi_nodes != NULL && succ->phi_nodes == NULL)
	{
		/* succ has more than 1 predecessor, checked earlier
		 * and doesn't have phi_nodes,
		 * block has phi_nodes. It unclear how to move block phi_nodes to succ because
		 * it increases the number of required phi sources*/
		return ESSL_FALSE;
	}


	/* Check that all phi sources of succ with join-block block are phi nodes in block */
	{
		phi_list *phi;
		for (phi = succ->phi_nodes ; phi != NULL ; phi = phi->next)
		{
			phi_source *phi_s;
			for (phi_s = phi->phi_node->expr.u.phi.sources ; phi_s != NULL ; phi_s = phi_s->next)
			{
				if (phi_s->join_block == block)
				{
					node *source = phi_s->source;
					if (source->hdr.kind != EXPR_KIND_PHI ||
						source->expr.u.phi.block != block)
					{
						return ESSL_FALSE;
					}
				}
			}
		}
	}

	return ESSL_TRUE;
}

static memerr join_block_into_successor(optimise_basic_blocks_context *ctx, basic_block *block, basic_block *succ)
{
	predecessor_list *pred, **pred_p;

	assert(block->output_visit_number != -1);
	assert(succ->output_visit_number != -1);

	/* Mark block as removed */
	block->output_visit_number = -1;

	/* Update predecessor list and targets */
	for (pred_p = &succ->predecessors ; *pred_p != NULL ; pred_p = &(*pred_p)->next)
	{
		if ((*pred_p)->block == block)
		{
			*pred_p = (*pred_p)->next;
			if (*pred_p == NULL) break;
		}
	}
	*pred_p = block->predecessors;
	for (pred = *pred_p ; pred != NULL ; pred = pred->next)
	{
		unsigned i;
		basic_block *pred_block = pred->block;
		for(i = 0; i < pred_block->n_successors; ++i)
		{
			if(pred_block->successors[i] == block)
			{
				pred_block->successors[i] = succ;
			}
		}

		assert(pred_block->termination != TERM_KIND_JUMP ||
			   pred_block->source == NULL ||
			   in_output_order(pred_block, pred_block->successors[BLOCK_DEFAULT_TARGET], ctx->cfg) ||
			   in_output_order(pred_block, pred_block->successors[BLOCK_TRUE_TARGET], ctx->cfg));
	}

	/* Insert phi sources of block into phi nodes of succ */
	{
		phi_list *phi;
		for (phi = succ->phi_nodes ; phi != NULL ; phi = phi->next)
		{
			phi_source **phi_s_p;
			for (phi_s_p = &phi->phi_node->expr.u.phi.sources ; *phi_s_p != NULL ; phi_s_p = &(*phi_s_p)->next)
			{
				if ((*phi_s_p)->join_block == block)
				{
					phi_source *next_phi_s = (*phi_s_p)->next;
					phi_source *phi2_s;
					if ((*phi_s_p)->source->hdr.kind == EXPR_KIND_PHI && (*phi_s_p)->source->expr.u.phi.block == block)
					{
						/* merge phi sources into list */
						for (phi2_s = (*phi_s_p)->source->expr.u.phi.sources ; phi2_s != NULL ; phi2_s = phi2_s->next)
						{
							phi_source *new_phi_s;
							ESSL_CHECK(new_phi_s = LIST_NEW(ctx->pool, phi_source));
							new_phi_s->source = phi2_s->source;
							new_phi_s->join_block = phi2_s->join_block;
							*phi_s_p = new_phi_s;
							phi_s_p = &new_phi_s->next;
						}
						*phi_s_p = next_phi_s;
					} else {
						/* Single-predecessor block with no phis */
						assert(block->predecessors != NULL && block->predecessors->next == NULL && block->phi_nodes == NULL);
						(*phi_s_p)->join_block = block->predecessors->block;
					}
					break;
				}
			}
		}
	}

	return MEM_OK;
}

memerr _essl_optimise_basic_block_joins(pass_run_context *pr_ctx, symbol *func)
{
	unsigned i;
	optimise_basic_blocks_context obb_ctx, *ctx = &obb_ctx;
	ctx->pool = pr_ctx->pool;
	ctx->cfg = func->control_flow_graph;
	ctx->desc = pr_ctx->desc;
	ESSL_CHECK(_essl_ptrset_init(&ctx->visited, pr_ctx->tmp_pool));

	for (i = 0 ; i < ctx->cfg->n_blocks ; i++)
	{
		basic_block *block = ctx->cfg->output_sequence[i];
		basic_block *succ_block = NULL;
		if (can_be_joined_into_successor(ctx, block, &succ_block))
		{
			ESSL_CHECK(join_block_into_successor(ctx, block, succ_block));
		}
	}

	_essl_correct_output_sequence_list(ctx->cfg);

	ESSL_CHECK(_essl_compute_dominance_information(ctx->pool, func));
	_essl_validate_control_flow_graph(func->control_flow_graph);

	return MEM_OK;
}
