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
#include "common/basic_block.h"
#include "common/ptrdict.h"

/*@null@*/ basic_block *_essl_new_basic_block_with_n_successors(mempool *pool, unsigned n_successors)
{

	basic_block *b;
	if(n_successors < MIN_SUCCESSORS_ALLOCATED) n_successors = MIN_SUCCESSORS_ALLOCATED;
	b = _essl_mempool_alloc(pool, sizeof(basic_block) + n_successors*sizeof(basic_block*)); /* allocate place for an array of two successor pointers as well */
	if (b == 0)  return 0;

	b->successors = (basic_block**)(b + 1); /* fetch the allocated array */

	b->n_successors = 0;
	b->next = NULL;
	b->predecessors = 0;
	b->predecessor_count = 0;
	b->phi_nodes = 0;
	b->local_ops = 0;
	b->control_dependent_ops = 0;
	b->termination = TERM_KIND_UNKNOWN;
	b->source = 0;
	b->immediate_dominator = 0;
	b->postorder_visit_number = 0;
	b->cost = -1.0f;


	return b;
}


/*@null@*/ basic_block *_essl_new_basic_block(mempool *pool)
{
	return _essl_new_basic_block_with_n_successors(pool, MIN_SUCCESSORS_ALLOCATED);
}

control_dependent_operation *_essl_clone_control_dependent_op(node *orig, node *clone, control_flow_graph *cfg, mempool *pool)
{
	control_dependent_operation *orig_cd_op, *clone_cd_op;
	unsigned i;
	ESSL_CHECK(orig_cd_op = _essl_ptrdict_lookup(&cfg->control_dependence, orig));

	ESSL_CHECK(clone_cd_op = _essl_mempool_alloc(pool, sizeof(*clone_cd_op)));
	clone_cd_op->op = clone;
	clone_cd_op->dependencies = 0;
	clone_cd_op->block = orig_cd_op->block;
	LIST_INSERT_FRONT(&orig_cd_op->next, clone_cd_op);
	ESSL_CHECK(_essl_ptrdict_insert(&cfg->control_dependence, clone, clone_cd_op));

	/* clone upward dependencies */
	{
		operation_dependency **clone_dep_ptr = &clone_cd_op->dependencies;
		operation_dependency *orig_dep;
		operation_dependency *clone_dep;
		for(orig_dep = orig_cd_op->dependencies; orig_dep != 0; orig_dep = orig_dep->next)
		{
			ESSL_CHECK(clone_dep = _essl_mempool_alloc(pool, sizeof(*clone_dep)));
			clone_dep->dependent_on = orig_dep->dependent_on;
			LIST_INSERT_FRONT(clone_dep_ptr, clone_dep);
			clone_dep_ptr = &clone_dep->next;

		}

	}

	/* now identify and clone downward dependencies */
	for(i = 0; i < cfg->n_blocks; ++i)
	{
		control_dependent_operation *cd_it;
		operation_dependency *dep;
		basic_block *block = cfg->output_sequence[i];
		for(cd_it = block->control_dependent_ops; cd_it != 0; cd_it = cd_it->next)
		{
			for(dep = cd_it->dependencies; dep != 0; dep = dep->next)
			{
				if(dep->dependent_on == orig_cd_op)
				{
					operation_dependency *clone_dep;
					ESSL_CHECK(clone_dep = _essl_mempool_alloc(pool, sizeof(*clone_dep)));
					clone_dep->dependent_on = clone_cd_op;
					LIST_INSERT_FRONT(&dep->next, clone_dep);
				}
			}

		}

	}

	return clone_cd_op;
}

basic_block *_essl_common_dominator(basic_block *b1, basic_block *b2)
{
	basic_block *finger1 = b1;
	basic_block *finger2 = b2;
	while(finger1 != finger2)
	{
		while(finger1->postorder_visit_number < finger2->postorder_visit_number)
		{
			assert(finger1->immediate_dominator != NULL);
			finger1 = finger1->immediate_dominator;
		}

		while(finger2->postorder_visit_number < finger1->postorder_visit_number)
		{
			assert(finger2->immediate_dominator != NULL);
			finger2 = finger2->immediate_dominator;
		}

	}
	return finger1;
}

/** Split a basic block at the specified control dependent operation.
  * \note Does not attempt to fixup the local operations.
  */
basic_block * _essl_split_basic_block(mempool *pool, basic_block* block, control_dependent_operation* split_point)
{
	basic_block* b2;
	control_dependent_operation *cd_op;
	unsigned i;
	
	ESSL_CHECK(b2 = _essl_new_basic_block_with_n_successors(pool, block->n_successors));
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

	return b2;;
}

/** Remove the control dependent operation that references the specified node from the list.
 */
void _essl_remove_control_dependent_op_node(control_dependent_operation **list, node *n)
{
	/* Find the CDO on the graph */
	if((*list) != 0 && (*list)->op == n)
		(*list) = (*list)->next;
	else
	{
		while((*list) != 0 && (*list)->next != 0 && (*list)->next->op != n)	list = &((*list)->next);
		assert((*list) != 0 && (*list)->next->op == n);
		LIST_REMOVE(&((*list)->next));
	}

	return;
}


#ifndef NDEBUG
void _essl_validate_control_flow_graph(control_flow_graph *cfg)
{
	int block_i;
	basic_block *prev_block = NULL;
	for (block_i = 0 ; block_i < (int)cfg->n_blocks ; block_i++)
	{
		basic_block *block = cfg->output_sequence[block_i];
		control_dependent_operation *cd_op;
		phi_list *phi;

		/* Validate block sequence */
		assert(block->output_visit_number == block_i);
		assert(prev_block == NULL || prev_block->next == block);

		/* Validate global ops */
		for(cd_op = block->control_dependent_ops; cd_op != NULL; cd_op = cd_op->next)
		{
			assert(cd_op->block == block);
		}

		/* Validate phi nodes */
		for (phi = block->phi_nodes ; phi != NULL ; phi = phi->next)
		{
			node *phi_node = phi->phi_node;
			phi_source *phi_s;
			assert(phi_node->expr.u.phi.block == block);
			for (phi_s = phi_node->expr.u.phi.sources ; phi_s != NULL ; phi_s = phi_s->next)
			{
				predecessor_list *pred;
				phi_source *phi_s2;
				essl_bool join_block_found = ESSL_FALSE;
				for (pred = block->predecessors ; pred != NULL ; pred = pred->next)
				{
					if (pred->block == phi_s->join_block)
					{
						join_block_found = ESSL_TRUE;
					}
				}
				assert(join_block_found);

				for (phi_s2 = phi_s->next ; phi_s2 != NULL ; phi_s2 = phi_s2->next)
				{
					assert(phi_s2->join_block != phi_s->join_block);
				}
			}
		}

		prev_block = block;
	}
}
#endif
