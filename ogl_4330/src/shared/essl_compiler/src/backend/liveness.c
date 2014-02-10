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
#include "backend/liveness.h"
#include "common/ptrset.h"
#include "common/ptrdict.h"
#include "common/basic_block.h"
#include "backend/extra_info.h"
#include "backend/instruction.h"


/** Follow transfer node links to get to the real node */
static /*@null@*/ node *get_node(/*@null@*/node *n)
{
	while(n && n->hdr.kind == EXPR_KIND_TRANSFER)
		{
			n = GET_CHILD(n, 0);
		}
	return n;
}


live_delimiter *_essl_liveness_new_delimiter(mempool *pool, node **var_ref, live_delimiter_kind kind, int position) {
	live_delimiter *delim;
	ESSL_CHECK(delim = _essl_mempool_alloc(pool, sizeof(live_delimiter)));
	delim->kind = kind;
	delim->position = position;
	switch (kind) {
	case LIVE_DEF:
	case LIVE_USE:
		delim->var_ref = var_ref;
		break;
	default:
		delim->var_ref = 0;
		break;
	}
	return delim;
}

live_range *_essl_liveness_new_live_range(mempool *pool, node *var, live_delimiter *points) {
	live_range *range;
	live_delimiter *delim;
	assert(points != 0);
	ESSL_CHECK(range = _essl_mempool_alloc(pool, sizeof(live_range)));
	range->var = var;
	range->start_position = points->position;
	range->points = points;
	range->locked = range->points->locked;
	for (delim = range->points->next ; delim != 0 ; delim = delim->next) {
		range->mask |= delim->live_mask;
		range->locked |= delim->locked;
		/*assert(delim->kind != LIVE_DEF || (delim->next != 0 && (delim->mask & ~delim->next->live_mask) == 0));*/
	}
	return range;
}

#if 0
live_range *_essl_liveness_make_simple_range(mempool *pool, control_flow_graph *cfg, node *var,
											 int pos1, int pos2, unsigned int live_mask, unsigned int locked) {
	live_delimiter *def_delim, *use_delim;
	live_range *range;
	node *temp;
	ESSL_CHECK(temp = _essl_new_unary_expression(pool, EXPR_OP_IDENTITY, var));
	_essl_ensure_compatible_node(temp, var);
	ESSL_CHECK(_essl_get_or_create_extra_info(pool, cfg, temp));
	ESSL_CHECK(def_delim = _essl_liveness_new_delimiter(pool, 0, LIVE_DEF, pos1));
	ESSL_CHECK(use_delim = _essl_liveness_new_delimiter(pool, 0, LIVE_USE, pos2));
	def_delim->next = use_delim;
	use_delim->next = 0;
	use_delim->live_mask = live_mask;
	use_delim->locked = locked;
	ESSL_CHECK(range = _essl_liveness_new_live_range(pool, temp, def_delim));
	return range;
}
#endif

/** Create a liveness delimiter and add it to the start of the liveness range for its variable */
static live_delimiter *add_delimiter(liveness_context *ctx, node **var_ref, live_delimiter_kind kind, int position) {
	live_delimiter *delim;
	node *var;
	*var_ref = get_node(*var_ref);
	var = *var_ref;
	ESSL_CHECK(delim = _essl_liveness_new_delimiter(ctx->pool, var_ref, kind, position));
	delim->next = (live_delimiter *)_essl_ptrdict_lookup(&ctx->var_to_range, var);
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->var_to_range, var, delim));

	/* Consistency of delimiter list */
	assert(delim->next == 0 || delim->next->position <= position);

	return delim;
}

/** Update the liveness mask for the given variable in the given dictionaty mapping
 *  from variables to liveness masks. Used for updating the
 *  block->var_live_mask_at_end dictionary.
 *  @param merge if ESSL_TRUE, merge the given liveness mask into the existing one, producing
 *         a 1 (live) bit where at least one of the two masks had a 1 bit.
 */ 
static memerr update_live_mask(ptrdict *d, node *var, long live_mask, essl_bool merge) {
	if (live_mask != 0) {
		if (merge) {
			long old_live_mask = (long)_essl_ptrdict_lookup(d, var);
			live_mask |= old_live_mask;
		}
		ESSL_CHECK(_essl_ptrdict_insert(d, var, (void *)live_mask));
	} else if (!merge) {
		_essl_ptrdict_remove(d, var);
	}
	return MEM_OK;
}

memerr _essl_liveness_mark_use(liveness_context *ctx, node **var_ref, int position, unsigned mask, essl_bool locked, essl_bool accepts_lut) {
	live_delimiter *delim;
	ESSL_CHECK(delim = add_delimiter(ctx, var_ref, LIVE_USE, position));
	delim->mask = mask;
	delim->live_mask = (delim->next != 0 ? delim->next->live_mask : 0) | mask;
	delim->locked = locked;
	delim->use_accepts_lut = accepts_lut;
	return MEM_OK;
}

memerr _essl_liveness_mark_def(liveness_context *ctx, node **var_ref, int position, unsigned mask, essl_bool locked) {
	live_delimiter *delim;
	ESSL_CHECK(delim = add_delimiter(ctx, var_ref, LIVE_DEF, position));
	delim->mask = mask;
	delim->live_mask = (delim->next != 0 ? delim->next->live_mask : 0) & ~mask;
	delim->locked = locked;
	return MEM_OK;
}

/** Create a STOP delimiter and add it to the variable liveness range */
static memerr mark_stop(liveness_context *ctx, node *var, int position, int mask) {
	live_delimiter *delim;
	ESSL_CHECK(delim = add_delimiter(ctx, &var, LIVE_STOP, position));
	delim->mask = mask;
	delim->live_mask = mask;
	return MEM_OK;
}

/** Create a RESTART delimiter and add it to the variable liveness range */
static memerr mark_restart(liveness_context *ctx, node *var, int position, int mask) {
	live_delimiter *delim;
	ESSL_CHECK(delim = add_delimiter(ctx, &var, LIVE_RESTART, position));
	delim->mask = mask;
	delim->live_mask = 0;
	return MEM_OK;
}

/** Build the list of variables that are live at the end of a block.
 *  These variables comprise liveness transfered from successor blocks as well as
 *  sources to phi nodes in successor blocks.
 */
static memerr build_live_var_list(liveness_context *ctx, basic_block *block) {
	/* Successor block phis */
	unsigned i;
	int bottom_point = END_OF_CYCLE(block->bottom_cycle);
	
	for(i = 0; i < block->n_successors; ++i)
	{
		basic_block *succ = block->successors[i];
		phi_list *phi;
		for (phi = succ->phi_nodes ; phi != 0 ; phi = phi->next) {
			phi_source *phi_s;
			phi->phi_node = get_node(phi->phi_node);
			for (phi_s = phi->phi_node->expr.u.phi.sources ; phi_s != 0 ; phi_s = phi_s->next) {
				if (phi_s->join_block == block) {
					phi_s->source = get_node(phi_s->source);
					ESSL_CHECK(_essl_liveness_mark_use(ctx, &phi_s->source, bottom_point,
												  ctx->mask_from_node(phi_s->source),
												  0, 0));
					break;
				}
			}
		}
	}

	/* Pre-allocated uses */
	{
		preallocated_var *prealloc;
		for (prealloc = block->preallocated_uses ; prealloc != NULL ; prealloc = prealloc->next)
		{
			prealloc->var = get_node(prealloc->var);
			ESSL_CHECK(_essl_liveness_mark_use(ctx, &prealloc->var, bottom_point, ctx->mask_from_node(prealloc->var), 1, 0));
		}
	}

	return MEM_OK;
}

/** Mark the definitions incurred by phi nodes and preallocated variables at the start of a block. */
static memerr mark_top_defs(liveness_context *ctx, basic_block *block) {
	int top_point = START_OF_CYCLE(block->top_cycle);
	phi_list *phi;
	for (phi = block->phi_nodes ; phi != 0 ; phi = phi->next) {
		phi->phi_node = get_node(phi->phi_node);
		ESSL_CHECK(_essl_liveness_mark_def(ctx, &phi->phi_node, top_point,
									  ctx->mask_from_node(phi->phi_node),
									  0));
	}

	/* Pre-allocated defs */
	{
		preallocated_var *prealloc;
		for (prealloc = block->preallocated_defs ; prealloc != NULL ; prealloc = prealloc->next)
		{
			prealloc->var = get_node(prealloc->var);
			ESSL_CHECK(_essl_liveness_mark_def(ctx, &prealloc->var, top_point, ctx->mask_from_node(prealloc->var), 1));
		}
	}

	return MEM_OK;
}

#if 0
void _essl_liveness_insert_delimiter(live_delimiter *first_delim, live_delimiter *new_delim) {
	live_delimiter *delim = first_delim;
	assert(first_delim->position >= new_delim->position);
	while (delim->next != 0 && delim->next->position >= new_delim->position) {
		delim = delim->next;
	}
	new_delim->next = delim->next;
	delim->next = new_delim;
}
#endif


live_delimiter *_essl_liveness_find_preceding_liveness(live_range *range, int position, int mask) {
	live_delimiter *delim, *found_delim = 0;
	for (delim = range->points ; delim != 0 ; delim = delim->next) {
		if (delim->position > position && delim->live_mask != mask) {
			found_delim = delim;
		}
	}
	return found_delim;
}

/** Update an existing liveness range for a variable based on some extra liveness.
 *  @param block only the delimiters in this block are updated
 *  @param delim the liveness range to update - it is assumed that the start of the range
 *         is no later that the end of the block
 *  @param live_mask the new liveness mask of the variable at the end of the block
 *  @param res_live_mask pointing to a variable which will receive the new liveness mask of
 *         the variable at the start of the block
 *  @param preceding_is_predecessor is the preceding block in the output order one of
 *         this blocks predecessors?
 */
static memerr update_liveness_in_block(liveness_context *ctx, basic_block *block, live_delimiter *delim,
									   int live_mask, int *res_live_mask, essl_bool preceding_is_predecessor)
{
	int start_position = START_OF_CYCLE(block->top_cycle);
	int end_position = END_OF_CYCLE(block->bottom_cycle);
	assert(delim->position >= end_position);
	if (delim->next != 0 && (delim->next->position > end_position ||
							 (delim->next->position == end_position && delim->next->kind == LIVE_USE)))
	{
		/* Not there yet */
		ESSL_CHECK(update_liveness_in_block(ctx, block, delim->next, live_mask, res_live_mask, preceding_is_predecessor));
	} else if (delim->next == 0 || delim->next->position < end_position) {
		/* Insert stop delimiter */
		live_delimiter *stop_delim;
		ESSL_CHECK(stop_delim = _essl_liveness_new_delimiter(ctx->pool, 0, LIVE_STOP, end_position));
		stop_delim->live_mask = live_mask;
		stop_delim->mask = delim->next == 0 ? live_mask : live_mask & ~delim->next->live_mask;
		stop_delim->next = delim->next;
		delim->next = stop_delim;
	} else {
		/* Existing stop delimiter - update live mask */
		live_delimiter *stop_delim = delim->next;
		assert(stop_delim->position == end_position);
		assert(stop_delim->kind == LIVE_STOP);
		stop_delim->live_mask |= live_mask;
		stop_delim->mask = stop_delim->next == 0 ? stop_delim->live_mask : stop_delim->live_mask & ~stop_delim->next->live_mask;
	}

	if (delim->position > start_position) {
		/* Done with block */
		assert(delim->next->position != start_position || delim->next->kind == LIVE_RESTART);
		if (delim->next->position < start_position) {
			/* Start of block */
			*res_live_mask = delim->next->live_mask;
			if (!preceding_is_predecessor && delim->next->live_mask != 0) {
				/* Insert restart */
				live_delimiter *restart_delim;
				ESSL_CHECK(restart_delim = _essl_liveness_new_delimiter(ctx->pool, 0, LIVE_RESTART, start_position));
				restart_delim->live_mask = 0;
				restart_delim->mask = delim->next->live_mask;
				restart_delim->next = delim->next;
				delim->next = restart_delim;
				/* Correct preceding STOP delimiter, if any */
				if (delim->kind == LIVE_STOP)
				{
					delim->mask = delim->live_mask;
				}
			}
		}
		return MEM_OK;
	}

	if (delim->kind == LIVE_RESTART) {
		/* Restart marks the start of the block */
		*res_live_mask = delim->next->live_mask;
		return MEM_OK;
	}

	assert(delim->kind == LIVE_USE || delim->kind == LIVE_DEF);
	delim->live_mask |= delim->next->live_mask;
	if (delim->kind == LIVE_DEF) {
		delim->live_mask &= ~delim->mask;
	}
	*res_live_mask = delim->live_mask;
	return MEM_OK;
}

/** Propagate the liveness of a variable through its existing liveness range because of a back edge.
 *  Liveness will propagate from block to block, only stopped by non-changing liveness or reaching
 *  beyond the blocks that have been processed by the liveness calculator.
 *  @param var the variable
 *  @param live_mask the new liveness mask of the variable
 *  @param block the block in which to propagete the liveness
 *  @param from_block the block which is the target of the back edge. Liveness will not be
 *         propagated earlier than this block (except for updating of the end-of-block liveness masks),
 *         since the liveness calculator has not processed earlier blocks yet.
 */
static memerr propagate_wrapped_liveness(liveness_context *ctx, node *var, long live_mask, basic_block *block, basic_block *from_block) {
	/* If not handled in this block, propagate liveness */
	long live_mask_at_end = (long)_essl_ptrdict_lookup(&block->var_live_mask_at_end, var);
	if (live_mask & ~live_mask_at_end)
	{
		live_delimiter *delim;
		predecessor_list *pred;
		essl_bool preceding_is_predecessor = ESSL_FALSE;
		int res_live_mask;
		live_mask |= live_mask_at_end;
		ESSL_CHECK(update_live_mask(&block->var_live_mask_at_end, var, live_mask, ESSL_FALSE));

		/* Propagate through block */
		for (pred = block->predecessors ; pred != 0 ; pred = pred->next) {
			if (pred->block->output_visit_number + 1 == block->output_visit_number) {
				preceding_is_predecessor = ESSL_TRUE;
			}
		}
		delim = _essl_ptrdict_lookup(&ctx->var_to_range, var);
		if (delim == 0 || delim->position < END_OF_CYCLE(block->bottom_cycle)) {
			/* Nothing in block - just put stop delimiter */
			ESSL_CHECK(mark_stop(ctx, var, END_OF_CYCLE(block->bottom_cycle), live_mask));
			res_live_mask = live_mask;
		} else {
			/* Update throughout block delimiters */
			ESSL_CHECK(update_liveness_in_block(ctx, block, delim, live_mask, &res_live_mask, preceding_is_predecessor));
		}

		/* Propagate to predecessors */
		for (pred = block->predecessors ; pred != 0 ; pred = pred->next) {
			if (pred->block->output_visit_number >= from_block->output_visit_number) {
				/* Still wrapped */
				ESSL_CHECK(propagate_wrapped_liveness(ctx, var, res_live_mask, pred->block, from_block));
			} else {
				/* Reached liveness calculation frontier */
				ESSL_CHECK(update_live_mask(&pred->block->var_live_mask_at_end, var, res_live_mask, ESSL_TRUE));
			}
		}
	}
	return MEM_OK;
}

/** Transfer variable liveness at the start of the given block to all its predecessors. */
static memerr transfer_live_vars_to_predecessors(liveness_context *ctx, basic_block *block, basic_block *preceding_block) {
	int top_point = START_OF_CYCLE(block->top_cycle);
	int prec_bottom_point = END_OF_CYCLE(preceding_block->bottom_cycle);

	/* Put all live vars onto the live lists of all predecessor blocks */
	ptrdict_iter it;
	node *var;
	essl_bool preceding_is_predecessor = ESSL_FALSE;
	live_delimiter *delim;
	int live_mask;
	_essl_ptrdict_iter_init(&it, &ctx->var_to_range);
	while ((var = _essl_ptrdict_next(&it, (void **)(void*)&delim)) != 0) {
		predecessor_list *pred;
		live_mask = delim->live_mask;
		for (pred = block->predecessors ; pred != 0 ; pred = pred->next) {
			if (pred->block->termination != TERM_KIND_EXIT) {
				if (pred->block->output_visit_number >= block->output_visit_number) {
					/* Liveness wraps around */
					ESSL_CHECK(propagate_wrapped_liveness(ctx, var, live_mask, pred->block, block));
				} else {
					ESSL_CHECK(update_live_mask(&pred->block->var_live_mask_at_end, var, live_mask, ESSL_TRUE));
				}
				if (pred->block == preceding_block) {
					preceding_is_predecessor = ESSL_TRUE;
				}
			}
		}
	}

	/* Mark end of lifetime hole if there is no fallthrough from preceding block */
	if (!preceding_is_predecessor) {
		_essl_ptrdict_iter_init(&it, &ctx->var_to_range);
		while ((var = _essl_ptrdict_next(&it, (void **)(void*)&delim)) != 0) {
			live_mask = delim->live_mask;
			if (live_mask != 0) {
				ESSL_CHECK(mark_restart(ctx, var, top_point, live_mask));
			}
		}
	}

	/* Mark start of lifetime hole for non-live vars that are live in preceding block */
	_essl_ptrdict_iter_init(&it, &preceding_block->var_live_mask_at_end);
	while ((var = _essl_ptrdict_next(&it, (void **)(void*)&live_mask)) != 0) {
		int curr_live_mask = ((live_delimiter *)_essl_ptrdict_lookup(&ctx->var_to_range, var))->live_mask;
		if (live_mask != 0 && (!preceding_is_predecessor || live_mask != curr_live_mask)) {
			ESSL_CHECK(mark_stop(ctx, var, prec_bottom_point, live_mask));
		}
	}

	return MEM_OK;
}

/** Do all liveness calculations for a block */
static memerr calculate_live_ranges_for_block(liveness_context *ctx, int output_index) {
	basic_block *block = ctx->cfg->output_sequence[output_index];
	ESSL_CHECK(build_live_var_list(ctx, block));
	ESSL_CHECK(ctx->block_func(ctx, block, ctx->block_func_data));
	ESSL_CHECK(mark_top_defs(ctx, block));
	if (output_index > 0) {
		basic_block *preceding_block = ctx->cfg->output_sequence[output_index-1];
		ESSL_CHECK(transfer_live_vars_to_predecessors(ctx, block, preceding_block));
	}

	return MEM_OK;
}

liveness_context *_essl_liveness_create_context(mempool *pool, control_flow_graph *cfg, target_descriptor *desc,
												block_liveness_handler block_func, void *block_func_data, mask_from_node_fun mask_from_node,
												error_context *error_context) {
	liveness_context *ctx;
	ESSL_CHECK(ctx = _essl_mempool_alloc(pool, sizeof(liveness_context)));
	ctx->pool = pool;
	ctx->cfg = cfg;
	ctx->desc = desc;
	ctx->var_ranges = 0;
	ESSL_CHECK(_essl_ptrdict_init(&ctx->var_to_range, pool));
	ctx->block_func = block_func;
	ctx->block_func_data = block_func_data;
	ctx->mask_from_node = mask_from_node;
	ctx->error_context = error_context;
	return ctx;
}

memerr _essl_liveness_insert_range(liveness_context *ctx, live_range *range)
{
	range->next = ctx->var_ranges;
	ctx->var_ranges = range;
	return _essl_ptrdict_insert(&ctx->var_to_range, range->var, range);
}

void _essl_liveness_remove_range(liveness_context *ctx, live_range *range)
{
	live_range **rangep;
	for (rangep = &ctx->var_ranges ; *rangep != 0 ; rangep = &(*rangep)->next)
	{
		if (*rangep == range)
		{
			*rangep = range->next;
			break;
		}
	}
	_essl_ptrdict_remove(&ctx->var_to_range, range->var);
}


memerr _essl_liveness_calculate_live_ranges(liveness_context *ctx) {
	int block_i;

	/* Initialize live var lists */
	for (block_i = (int)ctx->cfg->n_blocks-1 ; block_i >= 0 ; block_i--) {
		ESSL_CHECK(_essl_ptrdict_init(&ctx->cfg->output_sequence[block_i]->var_live_mask_at_end, ctx->pool));
	}

	/* Trace live vars backwards through blocks */
	for (block_i = (int)ctx->cfg->n_blocks-1 ; block_i >= 0 ; block_i--) {
		ESSL_CHECK(calculate_live_ranges_for_block(ctx, block_i));
	}

	/* Create ranges */
	{
		ptrdict_iter var_it;
		node *var;
		live_delimiter *delim;
		_essl_ptrdict_iter_init(&var_it, &ctx->var_to_range);
		while ((var = _essl_ptrdict_next(&var_it, (void **)(void*)&delim)) != 0) {
			live_range *range;
			live_delimiter **delimp = &delim;

			/* Variables must not be live at the function entry */
			assert(delim->live_mask == 0);
			if (delim->live_mask != 0)
			{
				return MEM_ERROR;
			}

			/* Clear out redundant STOP delimiters */
			while (*delimp != 0) {
				if ((*delimp)->kind == LIVE_STOP && (*delimp)->next != 0 &&
					(*delimp)->live_mask == (*delimp)->next->live_mask)
				{
					*delimp = (*delimp)->next;
				} else {
					delimp = &(*delimp)->next;
				}
			}

			ESSL_CHECK(range = _essl_liveness_new_live_range(ctx->pool, var, delim));
			range->spill_range = (var->hdr.kind == EXPR_KIND_UNARY && var->expr.operation == EXPR_OP_SPILL);
			assert(range->var == var);
			ESSL_CHECK(_essl_liveness_insert_range(ctx, range));
		}
	}

	LIVENESS_ASSERT_IDENTICAL(ctx, ctx);

	return MEM_OK;
}


#ifndef NDEBUG
void _essl_liveness_assert_identical(liveness_context *ctx1, liveness_context *ctx2) {
	live_range *range1;
	unsigned int i1 = 0;
	for (range1 = ctx1->var_ranges ; range1 != 0 ; range1 = range1->next) {
		live_range *range2;
		live_delimiter *delim1, *delim2;
		if (range1->spilled) continue;
		assert(_essl_ptrdict_has_key(&ctx2->var_to_range, range1->var));
		range2 = _essl_ptrdict_lookup(&ctx2->var_to_range, range1->var);
		for (delim1 = range1->points, delim2 = range2->points ;
			 delim1 != 0 && delim2 != 0 ;)
		{
			essl_bool found_match = ESSL_FALSE;
			live_delimiter *delim2_temp = delim2;
			assert(delim1->position == delim2->position);
			while (delim2_temp && delim2_temp->position == delim1->position)
			{
				if (delim2_temp->mask == delim1->mask &&
					delim2_temp->var_ref == delim1->var_ref &&
					delim2_temp->locked == delim1->locked)
				{
					found_match = ESSL_TRUE;
					break;
				}
				delim2_temp = delim2_temp->next;
			}
			assert(found_match);

			/* Only advance delim2 if delim1 is changing position */
			if (delim1->next == 0 || delim1->next->position != delim1->position)
			{
				while (delim2->next && delim2->next->position == delim2->position)
				{
					delim2 = delim2->next;
				}
				delim2 = delim2->next;
			}
			delim1 = delim1->next;
		}
		assert(delim1 == 0 && delim2 == 0);
		i1++;
	}
	assert(i1 == _essl_ptrdict_size(&ctx2->var_to_range));
}
#endif

/** Skip delimiters with identical positions */
static live_delimiter *advance_delimiter(live_delimiter *delim) {
	while (delim->next != 0 && delim->next->position == delim->position) {
		delim = delim->next;
	}
	return delim->next;
}

essl_bool _essl_liveness_merge_live_ranges(live_range *range1, live_range *range2) {
	/* Check that the two ranges are disjoint */
	{
		live_delimiter *delim1 = range1->points;
		live_delimiter *delim2 = range2->points;

		if (range1 == range2) return ESSL_TRUE;

		while (delim1 != 0 && delim2 != 0) {
			if (delim1->live_mask != 0 && delim2->live_mask != 0) {
				return ESSL_FALSE;
			}
			if (delim1->position > delim2->position) {
				delim1 = advance_delimiter(delim1);
			} else if (delim2->position > delim1->position) {
				delim2 = advance_delimiter(delim2);
			} else {
				delim1 = advance_delimiter(delim1);
				delim2 = advance_delimiter(delim2);
			}
		}
	}

	/* Join the ranges */
	{
		live_delimiter **delim1p = &range1->points;
		live_delimiter *delim2 = range2->points;
		range2->points = 0;

		while (delim2 != 0) {
			if (*delim1p == 0) {
				*delim1p = delim2;
				break;
			}
			if ((*delim1p)->position > delim2->position) {
				delim1p = &(*delim1p)->next;
			} else if (delim2->position > (*delim1p)->position || delim2->live_mask != 0) {
				/* Insert delimiter */
				live_delimiter *delim = delim2;
				delim2 = delim2->next;
				delim->next = *delim1p;
				*delim1p = delim;
				delim1p = &delim->next;
			} else {
				delim1p = &(*delim1p)->next;
			}
		}
	}

	/* Summarize range information */
	{
		live_delimiter *delim;
		assert(range1->var != 0);
		for (delim = range1->points ; delim != 0 ; delim = delim->next) {
			range1->mask |= delim->live_mask;
			range1->locked |= delim->locked;
			if (delim->var_ref) {
				assert(*delim->var_ref != 0);
				*delim->var_ref = range1->var;
			}
		}
		range1->start_position = range1->points->position;
	}

	return ESSL_TRUE;
}

static int get_sort_weight(live_range *range)
{
	if (range->sort_weight != 0)
	{
		return range->sort_weight;
	}

	{
		live_delimiter *delim;
		int subranges = 0, total_span = 0;
		int weight;
		for (delim = range->points ; delim != 0 && delim->next != 0 ; delim = delim->next) {
			if (delim->next->live_mask != 0) {
				subranges += 1;
				total_span += delim->position - delim->next->position;
			}
		}
		weight = subranges > 0 ? total_span / subranges : 0;
		if (weight <= 7) range->sort_weight = weight;
		return weight;
	}

}

static int range_compare(generic_list *vr1, generic_list *vr2) {
	live_range *r1 = (live_range *)vr1;
	live_range *r2 = (live_range *)vr2;
	int w1 = get_sort_weight(r1);
	int w2 = get_sort_weight(r2);
	return w1 - w2;
}

live_range *_essl_liveness_sort_live_ranges(live_range *ranges) {
	live_range *range;
	for (range = ranges ; range != 0 ; range = range->next)
	{
		range->sort_weight = 0;
	}
	return LIST_SORT(ranges, range_compare, live_range);
}

static memerr insert_stop_delimiter_after(mempool *pool, live_delimiter *delim, int position, int mask) {
	live_delimiter *stop_delim;
	ESSL_CHECK(stop_delim = _essl_liveness_new_delimiter(pool, 0, LIVE_STOP, position));
	stop_delim->mask = mask;
	stop_delim->live_mask = delim->next != 0
		? delim->next->live_mask | mask
		: mask;
	stop_delim->next = delim->next;
	delim->next = stop_delim;
	return MEM_OK;
}

memerr _essl_liveness_fix_dead_definitions(mempool *pool, live_range *ranges, ptrset *var_refs) {
	live_range *range;
	for (range = ranges ; range != 0 ; range = range->next) {
		live_delimiter *delim;
		for (delim = range->points ; delim != 0 ; delim = delim->next) {
			if (delim->kind == LIVE_DEF && delim->mask != 0 &&
				(delim->next == 0 ||
				 (delim->mask & ~delim->next->live_mask) != 0) &&
				(var_refs == 0 || _essl_ptrset_has(var_refs, delim->var_ref)))
			{
				/* One or more components of this definitions are dead */
				int dead_mask = delim->next != 0
					? delim->mask & ~delim->next->live_mask
					: delim->mask;
				live_delimiter *delim2 = delim;
				essl_bool handled = ESSL_FALSE;
				while (delim2->next != 0 && delim2->next->position == delim2->position) {
					if (delim2->next->next != 0 && (delim2->next->next->live_mask & dead_mask) != 0) {
						/* Dead interval squeezed in between co-located delimiters */
						ESSL_CHECK(insert_stop_delimiter_after(pool, delim2, delim->position, dead_mask));
						handled = ESSL_TRUE;
						break;
					}
					delim2 = delim2->next;
					delim2->live_mask |= dead_mask;
				}
				if (!handled) {
					/* There is space for a non-empty liveness interval for the defined components */
					ESSL_CHECK(insert_stop_delimiter_after(pool, delim2, delim->position-1, dead_mask));
				}
				range->mask |= dead_mask;
			}
		}
	}
	return MEM_OK;
}

memerr _essl_liveness_insert_cycle(liveness_context *ctx,
								   int position, basic_block *insertion_block,
								   insert_cycle_callback insert_into_instructions) {
	/* Adjust instructions */
	unsigned i;
	for (i = 0 ; i < ctx->cfg->n_blocks ; i++) {
		basic_block *block = ctx->cfg->output_sequence[i];
		/* Adjust top_cycle and bottom_cycle */
		if ((int)i <= insertion_block->output_visit_number) block->top_cycle += 1;
		if ((int)i < insertion_block->output_visit_number) block->bottom_cycle += 1;
		insert_into_instructions(block, position);
	}

	if (((instruction_word *)ctx->cfg->entry_block->earliest_instruction_word)->cycle > MAX_COMPILER_INSTRUCTION_WORDS)
	{
		REPORT_ERROR(ctx->error_context, ERR_RESOURCES_EXHAUSTED, 0, "Maximum number of compiler supported instructions (%d) exceeded.\n", MAX_COMPILER_INSTRUCTION_WORDS);
		return MEM_ERROR;
	}

	/* Adjust liveness intervals */
	{
		live_range *range;
		for (range = ctx->var_ranges ; range != 0 ; range = range->next) {
			live_delimiter *delim;
			if (range->start_position >= position) {
				range->start_position += POSITIONS_PER_CYCLE;
			}
			for (delim = range->points ; delim != 0 ; delim = delim->next) {
				if (delim->position >= position) {
					delim->position += POSITIONS_PER_CYCLE;
				}
			}
		}
	}

	/* Correct phi source use positions in the insertion block
	   (which have been erroneously adjusted) */
	{
		unsigned i;
		for(i = 0; i < insertion_block->n_successors; ++i)
		{
			basic_block *succ = insertion_block->successors[i];
			phi_list *phi;
			for (phi = succ->phi_nodes ; phi != 0 ; phi = phi->next) {
				phi_source *phi_s;
				for (phi_s = phi->phi_node->expr.u.phi.sources ; phi_s != 0 ; phi_s = phi_s->next) {
					if (phi_s->join_block == insertion_block) {
						/* Correct this source */
						live_range *range = _essl_ptrdict_lookup(&ctx->var_to_range, phi_s->source);
						live_delimiter **delimp;
						for (delimp = &range->points ; *delimp != 0 ; delimp = &(*delimp)->next) {
							if ((*delimp)->var_ref == &phi_s->source) {
								(*delimp)->position = END_OF_CYCLE(insertion_block->bottom_cycle);
								break;
							}
						}
						_essl_liveness_correct_live_range(range);
					}
				}
			}
		}
	}

	return MEM_OK;
}

static int delimiter_comparator(generic_list *e1, generic_list *e2)
{
	live_delimiter *d1 = (live_delimiter *)e1;
	live_delimiter *d2 = (live_delimiter *)e2;
	/*assert(d1->position >= d2->position || (d1->kind == LIVE_USE && d2->kind == LIVE_USE));*/
	return d2->position - d1->position;
}

static int correct_live_range(live_delimiter *delim) {
	int total_mask, live_after;

	if (delim == 0) {
		return 0;
	}

	total_mask = correct_live_range(delim->next);
	live_after = delim->next != 0 ? delim->next->live_mask : 0;

	switch (delim->kind) {
	case LIVE_USE:
	case LIVE_STOP:
		delim->live_mask = live_after | delim->mask;
		break;
	case LIVE_DEF:
	case LIVE_RESTART:
		delim->live_mask = live_after & ~delim->mask;
		break;
	default:
		assert(0);
		break;
	}
	
	return total_mask | delim->live_mask;
}

void _essl_liveness_correct_live_range(live_range *range) {
	range->points = LIST_SORT(range->points, delimiter_comparator, live_delimiter);
	range->mask = correct_live_range(range->points);
}

memerr _essl_liveness_mark_fixed_ranges(liveness_context *ctx)
{
	ptrset fixed_var_refs;
	unsigned block_i;
	live_range *range;

	/* Collect all anchoring points for fixed ranges */
	ESSL_CHECK(_essl_ptrset_init(&fixed_var_refs, ctx->pool));
	for (block_i = 0 ; block_i < ctx->cfg->n_blocks ; block_i++)
	{
		basic_block *block = ctx->cfg->output_sequence[block_i];
		preallocated_var *prealloc;
		for (prealloc = block->preallocated_defs ; prealloc != NULL ; prealloc = prealloc->next)
		{
			ESSL_CHECK(_essl_ptrset_insert(&fixed_var_refs, &prealloc->var));
		}
		for (prealloc = block->preallocated_uses ; prealloc != NULL ; prealloc = prealloc->next)
		{
			ESSL_CHECK(_essl_ptrset_insert(&fixed_var_refs, &prealloc->var));
		}
	}

	for (range = ctx->var_ranges ; range != 0 ; range = range->next)
	{
		live_delimiter *delim;
		for (delim = range->points ; delim != 0 ; delim = delim->next)
		{
			if (delim->var_ref != NULL && _essl_ptrset_has(&fixed_var_refs, delim->var_ref))
			{
				range->fixed = 1;
				break;
			}
		}
	}

	return MEM_OK;
}
