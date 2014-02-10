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
#include "common/find_blocks_for_operations.h"
#include "middle/conditional_select.h"
#include "middle/dominator.h"

static memerr optimise_conditional_selects(mempool *pool, mempool *local_pool, control_flow_graph *cfg, target_descriptor *desc);
static void fix_next_links(control_flow_graph *cfg, basic_block *block1, basic_block *block2, basic_block *block3);
static void fix_postorder_sequence(control_flow_graph* cfg, basic_block *block1, basic_block *block2, basic_block *block3);
static void fix_output_sequence(control_flow_graph* cfg, basic_block *block1, basic_block *block2, basic_block *block3);



/** Suitable constructions will be transformed into a conditional select (if profitable)
 *  @param pool Memory pool for memory allocations
 *  @param cfg Control flow graph to check/modify
 *  @param desc Target descriptor
 */
memerr _essl_optimise_conditional_selects(pass_run_context *pr_ctx, symbol *func)
{
	memerr ret;
	ret = optimise_conditional_selects(pr_ctx->pool, pr_ctx->tmp_pool, func->control_flow_graph, pr_ctx->desc);
	ESSL_CHECK(_essl_compute_dominance_information(pr_ctx->pool, func));
	_essl_validate_control_flow_graph(func->control_flow_graph);
	return ret;
}


static essl_bool can_be_optimised(basic_block *ablock,
							 basic_block **branch_true_out, basic_block **branch_false_out, basic_block **target_out)
{
	/*
	 * The pattern we are looking for are like this:
	 *
	 *            ablock
	 *            /    \
	 * true_branch      false_branch
	 *            \    /
	 *            target
	 *
	 * or:
	 *
	 *            ablock
	 *            /    |
	 * true_branch     |
	 *            \    |
	 *            target
	 *
	 * or:
	 *
	 *            ablock
	 *            |    \
	 *            |     false_branch
	 *            |    /
	 *            target
	 *
	 * These are candidates for a conditional select
	 */

	if (ablock->termination == TERM_KIND_JUMP && ablock->source != NULL)
	{
		/* ok, it's a condition, lets see if we have a common target */
		basic_block *branch_true = ablock->successors[BLOCK_TRUE_TARGET];
		basic_block *branch_false = ablock->successors[BLOCK_DEFAULT_TARGET];
		basic_block *target;
		basic_block *branch_true_default_target = (branch_true->n_successors >= 1) ? branch_true->successors[BLOCK_DEFAULT_TARGET] : NULL;
		basic_block *branch_false_default_target = (branch_false->n_successors >= 1) ? branch_false->successors[BLOCK_DEFAULT_TARGET] : NULL;


		if (branch_true_default_target == branch_false)
		{
			/* Single-sided true branch */
			target = branch_false;
			branch_false = NULL;
		}
		else if (branch_false_default_target == branch_true)
		{
			/* Single-sided false branch */
			target = branch_true;
			branch_true = NULL;
		}
		else if (branch_true_default_target == branch_false_default_target)
		{
			target = branch_true_default_target;
		}
		else
		{
			/* No common target */
			return ESSL_FALSE;
		}

		/* Now, see if there are any control dependent ops in the branch(es),
		   or if they have more than one predecessor or successor.
		*/
		if (branch_true != NULL &&
			(branch_true->termination != TERM_KIND_JUMP ||
			 branch_true->source != NULL ||
			 branch_true->control_dependent_ops != NULL ||
			 branch_true->predecessors->next != NULL))
		{
			return ESSL_FALSE;
		}

		if (branch_false != NULL &&
			(branch_false->termination != TERM_KIND_JUMP ||
			 branch_false->source != NULL ||
			 branch_false->control_dependent_ops != NULL ||
			 branch_false->predecessors->next != NULL))
		{
			return ESSL_FALSE;
		}

		/* verify that target have only two predecessors */
		if (target->predecessors->next->next == NULL)
		{
			*branch_true_out = branch_true;
			*branch_false_out = branch_false;
			*target_out = target;
			return ESSL_TRUE;
		}
	}
	return ESSL_FALSE;
}

static memerr calculate_phi_weights(ptrset *visited,
									basic_block *branch_true, basic_block *branch_false, basic_block *target,
									target_descriptor *desc, int *weight_out)
{
	phi_list *cur_phi;
	int phi_weight = 0;

	/* calculate op weight for all phi nodes */

	ESSL_CHECK(_essl_ptrset_clear(visited));

	cur_phi = target->phi_nodes;
	while (cur_phi != NULL && phi_weight < desc->csel_weight_limit)
	{
		phi_source *phi1 = cur_phi->phi_node->expr.u.phi.sources;
		phi_source *phi2 = phi1->next;
		int weight;

		if (phi1->join_block == branch_true || phi1->join_block == branch_false)
		{
			ESSL_CHECK(_essl_calc_op_weight(phi1->source, phi1->join_block, desc, visited, &weight));
			phi_weight += weight;
		}

		if (phi2->join_block == branch_true || phi2->join_block == branch_false)
		{
			ESSL_CHECK(_essl_calc_op_weight(phi2->source, phi2->join_block, desc, visited, &weight));
			phi_weight += weight;
		}

		cur_phi = cur_phi->next;
	}

	*weight_out = phi_weight;
	return MEM_OK;
}

static memerr create_conditional_select(mempool *pool, control_flow_graph *cfg, unsigned int *block_no,
										basic_block *ablock, basic_block *branch_true, basic_block *branch_false, basic_block *target)
{
	predecessor_list *pred_entry;
	control_dependent_operation *curr_dep_op;

	/* convert all phi nodes */
	phi_list *cur_phi = target->phi_nodes;
	while (cur_phi != NULL)
	{
		phi_source *phi_false = NULL;
		phi_source *phi_true = NULL;
		phi_source *phi_s;

		for (phi_s = cur_phi->phi_node->expr.u.phi.sources ; phi_s != NULL ; phi_s = phi_s->next)
		{
			if (phi_s->join_block == branch_true || (branch_true == NULL && phi_s->join_block == ablock))
			{
				assert(phi_true == NULL);
				phi_true = phi_s;
			}

			if (phi_s->join_block == branch_false || (branch_false == NULL && phi_s->join_block == ablock))
			{
				assert(phi_false == NULL);
				phi_false = phi_s;
			}
		}
		assert(phi_true != NULL && phi_false != NULL && phi_false != phi_true);

		/* rewrite the phi node to a contidional select */

		cur_phi->phi_node->hdr.kind = EXPR_KIND_TERNARY;
		cur_phi->phi_node->expr.operation = EXPR_OP_CONDITIONAL_SELECT;
		ESSL_CHECK(_essl_node_set_n_children(cur_phi->phi_node, 3, pool));
		SET_CHILD(cur_phi->phi_node, 0, ablock->source);
		SET_CHILD(cur_phi->phi_node, 1, phi_true->source);
		SET_CHILD(cur_phi->phi_node, 2, phi_false->source);

		cur_phi = cur_phi->next;
	}

	/* Check if ablock is still needed to preserve block contiguity */
	{
		essl_bool ablock_needed = ESSL_FALSE;
		int ablock_target_dist = target->output_visit_number - ablock->output_visit_number;
		if (branch_true != NULL &&
			branch_true->output_visit_number > ablock->output_visit_number &&
			branch_true->output_visit_number < target->output_visit_number)
		{
			ablock_target_dist -= 1;
		}
		if (branch_false != NULL &&
			branch_false->output_visit_number > ablock->output_visit_number &&
			branch_false->output_visit_number < target->output_visit_number)
		{
			ablock_target_dist -= 1;
		}

		if (ablock_target_dist > 1)
		{
			/* ablock, branches and target do not form a contiguous block,
			   hence predecessors of ablock will not be predecessors of
			   target after removing ablock and branches.
			   Check if any predecessors of ablock require such contiguity.
			*/
			predecessor_list *pred;
			for (pred = ablock->predecessors ; pred != NULL ; pred = pred->next)
			{
				if (pred->block->output_visit_number + 1 == ablock->output_visit_number && pred->block->n_successors > 1)
				{
					ablock_needed = ESSL_TRUE;
					break;
				}
			}
		}

		if (ablock_needed)
		{
			/* We can only remove the branch blocks */
			target->predecessors->block = ablock;
			target->predecessors->next = NULL;

			ablock->source = NULL;
			ablock->n_successors = 1;
			ablock->successors[BLOCK_DEFAULT_TARGET] = target;

			target->phi_nodes = NULL;

			ablock = NULL;
		}
		else
		{
			/* remove the blocks which we don't need, and "join" ablock and target */

			/* fix the predecessors */

			target->predecessors = ablock->predecessors;
			target->predecessor_count = ablock->predecessor_count;

			/* fix the true_target and default_target */

			for(pred_entry = ablock->predecessors; pred_entry != NULL; pred_entry = pred_entry->next)
			{
				unsigned i;
				basic_block *pred_block = pred_entry->block;
				for(i = 0; i < pred_block->n_successors; ++i)
				{
					if(pred_block->successors[i] == ablock)
					{
						pred_block->successors[i] = target;
					}
				}
			}

			/* copy phi nodes (if any) from ablock to target (and update the block they belong to) */

			target->phi_nodes = ablock->phi_nodes;
			for(cur_phi = target->phi_nodes; cur_phi != NULL; cur_phi = cur_phi->next)
			{
				cur_phi->phi_node->expr.u.phi.block = target;
			}

			/* append the control dependent ops (if any) from ablock to target */

			curr_dep_op = ablock->control_dependent_ops;

			while (curr_dep_op != NULL)
			{
				curr_dep_op->block = target;

				if (curr_dep_op->next != NULL)
				{
					curr_dep_op = curr_dep_op->next;
				}
				else
				{
					break;
				}
			}

			if (curr_dep_op != NULL)
			{
				curr_dep_op->next = target->control_dependent_ops;
				target->control_dependent_ops = ablock->control_dependent_ops;
			}
		}
	}

	/* Now, fix the postorder_sequence and output_sequence */

	fix_next_links(cfg, ablock, branch_true, branch_false);
	fix_postorder_sequence(cfg, ablock, branch_true, branch_false);
	fix_output_sequence(cfg, ablock, branch_true, branch_false);

	/* Now, update the number of blocks counter, and get ready for more */
	{
		int n_blocks_removed = (ablock != NULL) + (branch_false != NULL) + (branch_true != NULL);
		cfg->n_blocks -= n_blocks_removed;
		*block_no -= n_blocks_removed;
	}

	return MEM_OK;
}


/** Suitable constructions will be transformed into a conditional select (if profitable)
 *  @param pool Memory pool for memory allocations
 *  @param local_pool Memory pool for local allocations (will be freed on return)
 *  @param cfg Control flow graph to check/modify
 *  @param desc Target descriptor
 */
static memerr optimise_conditional_selects(mempool *pool, mempool *local_pool, control_flow_graph *cfg, target_descriptor *desc)
{
	unsigned int block_no = 0;
	ptrset visited_nodes;

	ESSL_CHECK(_essl_ptrset_init(&visited_nodes, local_pool));

	while (block_no < cfg->n_blocks)
	{
		/* first, look for basic blocks with a condition (true_target) */

		basic_block *ablock = cfg->postorder_sequence[block_no];
		basic_block *branch_true = NULL, *branch_false = NULL, *target = NULL;
		if (can_be_optimised(ablock, &branch_true, &branch_false, &target))
		{
			/* check if we should replace these phi node(s) with conditional select(s) */
			int phi_weight = 0;
			ESSL_CHECK(calculate_phi_weights(&visited_nodes, branch_true, branch_false, target, desc, &phi_weight));

			if (phi_weight < desc->csel_weight_limit)
			{
				ESSL_CHECK(create_conditional_select(pool, cfg, &block_no, ablock, branch_true, branch_false, target));

				if (cfg->n_blocks < 3)
				{
					return MEM_OK; /* not enough blocks left to satisfy criteria, we are done! */
				}

			}
		}

		block_no++;
	}

	return MEM_OK;
}


/**
 * Fix the next pointers in the control flow graph after removing the specified blocks.
 * @param cfg Control flow graph with next pointers to fix.
 * @param block1 Block to remove.
 * @param block2 Another block to remove.
 * @param block3 And even another block to remove.
 */
static void fix_next_links(control_flow_graph *cfg, basic_block *block1, basic_block *block2, basic_block *block3)
{
	/* fix all the next pointers (including entry_block and exit_block in cfg) */
	basic_block *curr_block;
	basic_block *prev_block;
	prev_block = NULL;
	curr_block = cfg->entry_block;
	while (curr_block != NULL)
	{
		if (curr_block == block1 ||
			curr_block == block2 ||
			curr_block == block3)
		{
			if (prev_block != NULL)
			{
				prev_block->next = curr_block->next;
			}
			else
			{
				cfg->entry_block = curr_block->next;
			}
		}
		else
		{
			prev_block = curr_block;
		}

		curr_block = curr_block->next;
	}
}


/**
 * Fix the postorder sequence in the control flow graph after removing the specified blocks.
 * @param cfg Control flow graph with postorder sequence to fix.
 * @param block1 Block to remove.
 * @param block2 Another block to remove.
 * @param block3 And even another block to remove.
 */
static void fix_postorder_sequence(control_flow_graph* cfg, basic_block *block1, basic_block *block2, basic_block *block3)
{
	unsigned int ins_pos = 0;
	unsigned int read_pos = 0;

	while (read_pos < cfg->n_blocks)
	{
		cfg->postorder_sequence[read_pos]->postorder_visit_number = ins_pos;

		if (cfg->postorder_sequence[read_pos] != block1 &&
		    cfg->postorder_sequence[read_pos] != block2 &&
		    cfg->postorder_sequence[read_pos] != block3)
		{
			if (read_pos != ins_pos)
			{
				cfg->postorder_sequence[ins_pos] = cfg->postorder_sequence[read_pos];
			}

			ins_pos++;
		}

		read_pos++;
	}
}


/**
 * Fix the output sequence in the control flow graph after removing the specified blocks.
 * @param cfg Control flow graph with output sequence to fix.
 * @param block1 Block to remove.
 * @param block2 Another block to remove.
 * @param block3 And even another block to remove.
 */
static void fix_output_sequence(control_flow_graph* cfg, basic_block *block1, basic_block *block2, basic_block *block3)
{
	unsigned int ins_pos = 0;
	unsigned int read_pos = 0;

	while (read_pos < cfg->n_blocks)
	{
		cfg->output_sequence[read_pos]->output_visit_number = ins_pos;

		if (cfg->output_sequence[read_pos] != block1 &&
		    cfg->output_sequence[read_pos] != block2 &&
		    cfg->output_sequence[read_pos] != block3)
		{
			if (read_pos != ins_pos)
			{
				cfg->output_sequence[ins_pos] = cfg->output_sequence[read_pos];
			}

			ins_pos++;
		}

		read_pos++;
	}
}

