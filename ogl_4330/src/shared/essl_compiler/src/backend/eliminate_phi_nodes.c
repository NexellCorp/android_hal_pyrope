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
#include "backend/extra_info.h"
#include "backend/eliminate_phi_nodes.h"
#include "mali200/mali200_instruction.h"
#include "common/ptrset.h"

#ifndef NDEBUG
static essl_bool range_ok(live_range *range) {
	live_delimiter *delim;
	for (delim = range->points ; delim != 0 && delim->next != 0 ; delim = delim->next) {
		if (delim->next->position > delim->position) return ESSL_FALSE;
		switch (delim->kind) {
		case LIVE_DEF:
			assert(delim->live_mask == (delim->next->live_mask & ~delim->mask));
			break;
		case LIVE_USE:
			assert(delim->live_mask == (delim->next->live_mask | delim->mask));
			break;
		default:
			break;
		}
	}
	return ESSL_TRUE;
}

static essl_bool phi_node_complete(node *phi_node)
{
	int n_pred, n_src = 0;
	basic_block *block = phi_node->expr.u.phi.block;
	predecessor_list *pred;
	n_pred = 0;
	for (pred = block->predecessors ; pred != 0 ; pred = pred->next)
	{
		essl_bool found_source = ESSL_FALSE;
		phi_source *phi_s;
		n_src = 0;
		for (phi_s = phi_node->expr.u.phi.sources ; phi_s != 0 ; phi_s = phi_s->next)
		{
			if (phi_s->join_block == pred->block) found_source = ESSL_TRUE;
			n_src++;
		}
		if (!found_source) return ESSL_FALSE;

		n_pred++;
	}
	return n_pred == n_src;
}
#endif

static node *make_move_target(mempool *pool, node *source) {
	node *target;
	ESSL_CHECK(target = _essl_new_unary_expression(pool, EXPR_OP_IDENTITY, source));
	_essl_ensure_compatible_node(target, source);
	ESSL_CHECK(CREATE_EXTRA_INFO(pool, target));
	return target;
}

static essl_bool is_phi_delimiter(live_delimiter *delim, ptrdict *var_ref_to_phi) {
	return delim->var_ref != 0 && _essl_ptrdict_has_key(var_ref_to_phi, delim->var_ref);
}

#ifndef NDEBUG
static essl_bool phi_in_block(phi_list *phi, basic_block *block)
{
	phi_list *pl;
	for (pl = block->phi_nodes ; pl != 0 ; pl = pl->next)
	{
		if (pl == phi) return ESSL_TRUE;
	}
	return ESSL_FALSE;
}
#endif

static essl_bool preceding_is_predecessor(phi_list *phi, ptrdict *phi_to_node)
{
	node *phi_node = _essl_ptrdict_lookup(phi_to_node, phi);
	basic_block *block = phi_node->expr.u.phi.block;
	predecessor_list *pred;
	assert(phi_in_block(phi, block));
	for (pred = block->predecessors ; pred != 0 ; pred = pred->next)
	{
		if (pred->block->output_visit_number + 1 == block->output_visit_number) return ESSL_TRUE;
	}
	return ESSL_FALSE;
}

static live_delimiter *remove_phi_from_delimiters(live_delimiter *delim, ptrdict *var_ref_to_phi, ptrdict *phi_to_node) {
	int next_mask;

	if (delim == 0) return 0;

	delim->next = remove_phi_from_delimiters(delim->next, var_ref_to_phi, phi_to_node);
	next_mask = delim->next != 0 ? delim->next->live_mask : 0;

	if (is_phi_delimiter(delim, var_ref_to_phi))
	{
		switch (delim->kind) {
		case LIVE_DEF:
		{
			phi_list *phi = _essl_ptrdict_lookup(var_ref_to_phi, delim->var_ref);
			if (next_mask != 0 && !preceding_is_predecessor(phi, phi_to_node))
			{
				delim->kind = LIVE_RESTART;
				delim->mask = next_mask;
				delim->live_mask = 0;
				delim->var_ref = 0;
				return delim;
			} else {
				/* Remove */
				return delim->next;
			}
		}
		case LIVE_USE:
		{
			int stop_mask = delim->live_mask & ~next_mask;
			if (stop_mask != 0) {
				delim->kind = LIVE_STOP;
				delim->mask = stop_mask;
				delim->var_ref = 0;
				return delim;
			} else {
				/* Remove */
				return delim->next;
			}
		}
		default:
			break;
		}
	}

	return delim;
}

static void remove_phi_from_range(live_range *range, ptrdict *var_ref_to_phi, ptrdict *phi_to_node) {
	range->points = remove_phi_from_delimiters(range->points, var_ref_to_phi, phi_to_node);
}


/** Split a liveness range in two, corresponding to insertion of a move instruction to move the
 *  variable from one location to another.
 *  @param pos1 the use position of the move instruction; will become the end position of
 *         the first part of the split
 *  @param pos2 the definition position of the move instruction; will become the start
 *         position of the second part of the split
 *  @param var1_ref reference to the source of the move instruction; will become the var_ref of the
 *         end delimiter of the first part of the split
 *  @param var2_ref reference to the destination of the move instruction; will become the var_ref of the
 *         start delimiter of the second part of the split
 *  @return the range corresponding to the second part if the split; the original range contains
 *          the first part
 */
static live_range *split_liveness_range(mempool *pool, live_range *range,
										int pos1, int pos2, node **var1_ref, node **var2_ref,
										basic_block *block, liveness_split_mode mode,
										node **stop_use_var_ref)
{
	live_delimiter *delim, *delim1, *delim2, **delimp, **top_delimp;
	live_delimiter *range_start = 0, *range2_start = 0;
	assert(*var2_ref);
	assert(pos1 > pos2);
	assert(pos1 < range->points->position);
	ESSL_CHECK(delim1 = _essl_liveness_new_delimiter(pool, var1_ref, LIVE_USE, pos1));
	ESSL_CHECK(delim2 = _essl_liveness_new_delimiter(pool, var2_ref, LIVE_DEF, pos2));

	top_delimp = &range->points;
	for (delim = range->points ; delim != 0 ; delim = delim->next)
	{
		if (mode == LIVENESS_SPLIT_KEEP_TO_START &&
			delim->position > START_OF_CYCLE(block->top_cycle))
		{
			if (delim->var_ref != 0)
			{
				*delim->var_ref = *var2_ref;
			}
			top_delimp = &delim->next;
		}

		if (delim->next != 0 && delim->next->position < pos1)
		{
			/* Split this subrange */
			live_range *range2;
			int live_mask = delim->next->live_mask;
			assert(delim->next->position < pos2);
			assert(delim->next->live_mask != 0);
			delim1->mask = live_mask;
			delim1->live_mask = live_mask;
			delim2->mask = live_mask;
			delim2->live_mask = 0;

			switch (mode) {
			case LIVENESS_SPLIT_KEEP_TO_START:
				assert((*top_delimp)->position == START_OF_CYCLE(block->top_cycle));
				delim2->next = delim->next;
				delim->next = delim1;
				delim1->next = 0;
				range_start = *top_delimp;
				*top_delimp = delim2;
				range2_start = range->points;

				/* Update all variable references in the second range */
				for (delimp = &delim2 ; *delimp != 0 ; delimp = &(*delimp)->next) {
					if ((*delimp)->var_ref != 0) {
						*(*delimp)->var_ref = *var2_ref;
					}
				}
				break;
			case LIVENESS_SPLIT_KEEP_ALL_EXCEPT_PHI_USE:
				delim1->next = delim->next;
				delim->next = delim1;
				range_start = range->points;
				range2_start = delim2;
				delim2->next = 0;

				/* Move over phi use delimiter */
				for (delimp = &delim1->next ; *delimp != 0 ; delimp = &(*delimp)->next) {
					if ((*delimp)->position == END_OF_CYCLE(block->bottom_cycle)
						&& (*delimp)->var_ref == stop_use_var_ref)
					{
						assert((*delimp)->kind == LIVE_USE);
						delim2->next = *delimp;
						*delimp = delim2->next->next;
						delim2->next->next = 0;
						*delim2->next->var_ref = *var2_ref;
						break;
					}
				}
				assert(delim2->next != 0);
				break;
			}

			range->points = range_start;
			ESSL_CHECK(range2 = _essl_liveness_new_live_range(pool, *var2_ref, range2_start));
			_essl_liveness_correct_live_range(range);
			return range2;
		}
	}
	assert(0); /* Range does not cover the split point */
	return 0;
}

static live_range *split_range(mempool *pool, liveness_context *ctx, live_range *source_range,
							   int earliest, int latest, basic_block *block, liveness_split_mode mode,
							   node **stop_use_var_ref,
							   insert_move_callback insert_move, void *callback_ctx, essl_bool backward) {
	/* Insert split move */
	node *target;
	int src_position, dst_position;
	node **src_ref, **dst_ref;
	live_range *range2;
	if (earliest > START_OF_CYCLE(block->top_cycle))
	{
		earliest = START_OF_CYCLE(block->top_cycle);
	}
	if (latest < END_OF_CYCLE(block->bottom_cycle))
	{
		latest = END_OF_CYCLE(block->bottom_cycle);
	}

	/* The move insertion function may infer the mode from the direction, so make sure they match */
	assert((mode == LIVENESS_SPLIT_KEEP_ALL_EXCEPT_PHI_USE && backward) ||
		   (mode == LIVENESS_SPLIT_KEEP_TO_START && !backward));

	ESSL_CHECK(target = make_move_target(pool, source_range->var));
	ESSL_CHECK(insert_move(callback_ctx, source_range->var, target, earliest, latest, block, backward,
					  &src_position, &dst_position, &src_ref, &dst_ref));

	assert(range_ok(source_range));

	/* Split range */
	ESSL_CHECK(range2 = split_liveness_range(ctx->pool, source_range, src_position, dst_position, src_ref, dst_ref, block, mode, stop_use_var_ref));

	assert(range_ok(source_range));
	assert(range_ok(range2));
	return range2;
}

/** Component mask for variable size */
static unsigned int mask_from_node(node *n) {
	return (1U << GET_NODE_VEC_SIZE(n))-1;
}

static live_range *split_source(mempool *pool, liveness_context *ctx, phi_source *phi_s,
								live_range *phi_range, live_range *source_range,
								insert_move_callback insert_move, void *callback_ctx) {
	/* Find earliest possible split position.
	   The source must be live, and the phi node must not be live. */
	live_range *range2;
	int phi_use_pos = END_OF_CYCLE(phi_s->join_block->bottom_cycle);
	live_delimiter *phi_live_end = _essl_liveness_find_preceding_liveness(phi_range, phi_use_pos, 0);
	live_delimiter *source_live_begin = _essl_liveness_find_preceding_liveness(source_range, phi_use_pos, mask_from_node(phi_s->source));
	int earliest_split = source_live_begin->position;
	if (phi_live_end != 0 && phi_live_end->position < earliest_split) {
		earliest_split = phi_live_end->position;
	}
	
	/* Insert split move */
	ESSL_CHECK(range2 = split_range(pool, ctx, source_range, earliest_split, phi_use_pos,
							   phi_s->join_block, LIVENESS_SPLIT_KEEP_ALL_EXCEPT_PHI_USE,
							   &phi_s->source,
							   insert_move, callback_ctx, ESSL_TRUE));

	return range2;
}

static memerr add_range(liveness_context *ctx, live_range *range) {
	/* Insert new range in list and map */
	range->next = ctx->var_ranges;
	ctx->var_ranges = range;
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->var_to_range, range->var, range));
	return MEM_OK;
}

static essl_bool range_is_live_between(live_range *range, int pos1, int pos2) {
	live_delimiter *delim;
	for (delim = range->points ; delim != 0 && delim->position > pos2 ; delim = delim->next)
	{
		if (delim->next != 0 &&
			delim->position > pos2 &&
			delim->next->position <= pos1 &&
			delim->next->live_mask != 0)
		{
			return ESSL_TRUE;
		}
	}
	return ESSL_FALSE;
}

static int source_position(liveness_context *ctx, basic_block *join_block) {
	return START_OF_SUBCYCLE(CYCLE_TO_SUBCYCLE(join_block->bottom_cycle, ctx->desc->branch_condition_subcycle));
}

static essl_bool range_covers_source(liveness_context *ctx, live_range *range, phi_source *phi_s)
{
	return range_is_live_between(range, source_position(ctx, phi_s->join_block), END_OF_CYCLE(phi_s->join_block->bottom_cycle));
}

static live_range *get_range_for_var(liveness_context *ctx, node *var, ptrdict *range_replace)
{
	live_range *range = _essl_ptrdict_lookup(&ctx->var_to_range, var);
	live_range *rep_range;
	assert(range);
	while ((rep_range = _essl_ptrdict_lookup(range_replace, range)) != 0)
	{
		assert(range->points == 0);
		range = rep_range;
	}
	return range;
}

static memerr phi_sources_covered_by_range(liveness_context *ctx, node *phi_node, live_range *range, essl_bool *needs_split, ptrset *visited)
{
	phi_source *phi_s;

	if (_essl_ptrset_has(visited, phi_node)) return MEM_OK;
	ESSL_CHECK(_essl_ptrset_insert(visited, phi_node));

	for (phi_s = phi_node->expr.u.phi.sources ; phi_s != 0 ; phi_s = phi_s->next)
	{
		/* Is this source in conflict with the range? */
		if (phi_s->source != phi_node && range_covers_source(ctx, range, phi_s)) {
			*needs_split = ESSL_TRUE;
			return MEM_OK;
		}

		/* If the source is a phi node, visit that as well */
		if (phi_s->source->hdr.kind == EXPR_KIND_PHI)
		{
			ESSL_CHECK(phi_sources_covered_by_range(ctx, phi_s->source, range, needs_split, visited));
			if (*needs_split) return MEM_OK;
		}
	}

	/* If any phi nodes have this as source, visit them */
	{
		unsigned i;
		control_flow_graph *cfg = ctx->cfg;
		for(i = 0; i < cfg->n_blocks; ++i)
		{
			basic_block *block = cfg->output_sequence[i];
			phi_list *phi;
			for (phi = block->phi_nodes ; phi != 0 ; phi = phi->next)
			{
				phi_source *phi_s;
				for (phi_s = phi->phi_node->expr.u.phi.sources ; phi_s != 0 ; phi_s = phi_s->next)
				{
					if (phi_s->source == phi_node)
					{
						ESSL_CHECK(phi_sources_covered_by_range(ctx, phi->phi_node, range, needs_split, visited));
						if (*needs_split) return MEM_OK;
						break;
					}
				}
			}
		}
	}

	return MEM_OK;
}

static memerr phi_needs_split(liveness_context *ctx, phi_list *phi, essl_bool *needs_split)
{
	ptrset visited;
	live_range *phi_range = _essl_ptrdict_lookup(&ctx->var_to_range, phi->phi_node);
	*needs_split = ESSL_FALSE;
	ESSL_CHECK(_essl_ptrset_init(&visited, ctx->pool));
	ESSL_CHECK(phi_sources_covered_by_range(ctx, phi->phi_node, phi_range, needs_split, &visited));
	return MEM_OK;
}

static memerr split_phi_nodes(mempool *pool, liveness_context *ctx, insert_move_callback insert_move, void *callback_ctx)
{
	unsigned i;
	control_flow_graph *cfg = ctx->cfg;
	for(i = 0; i < cfg->n_blocks; ++i)
	{
		basic_block *block = cfg->output_sequence[i];
		phi_list *phi;
		for (phi = block->phi_nodes ; phi != 0 ; phi = phi->next)
		{
			essl_bool needs_split;
			assert(phi_node_complete(phi->phi_node));
			ESSL_CHECK(phi_needs_split(ctx, phi, &needs_split));
			if (needs_split)
			{
				live_range *phi_range = _essl_ptrdict_lookup(&ctx->var_to_range, phi->phi_node);
				int earliest_position = phi_range->start_position;
				int latest_position = source_position(ctx, block);
				live_range *range2;
				assert(earliest_position == START_OF_CYCLE(block->top_cycle));
				ESSL_CHECK(range2 = split_range(pool, ctx, phi_range, earliest_position, latest_position,
										   block, LIVENESS_SPLIT_KEEP_TO_START, 0,
										   insert_move, callback_ctx, ESSL_FALSE));
				ESSL_CHECK(add_range(ctx, range2));
			}
		}
	}
	return MEM_OK;
}


memerr _essl_eliminate_phi_nodes(mempool *pool, liveness_context *ctx, insert_move_callback insert_move, void *callback_ctx) {
	unsigned i;
	control_flow_graph *cfg = ctx->cfg;
	ptrdict phi_to_node;
	ptrdict var_ref_to_phi;
	ptrdict range_replace;
	
	ESSL_CHECK(split_phi_nodes(pool, ctx, insert_move, callback_ctx));

	/* Record the sources list for all phis */
	ESSL_CHECK(_essl_ptrdict_init(&phi_to_node, ctx->pool));
	ESSL_CHECK(_essl_ptrdict_init(&var_ref_to_phi, ctx->pool));
	ESSL_CHECK(_essl_ptrdict_init(&range_replace, ctx->pool));

	for(i = 0; i < cfg->n_blocks; ++i)
	{
		basic_block *block = cfg->output_sequence[i];
		phi_list *phi;
		for (phi = block->phi_nodes ; phi != 0 ; phi = phi->next) {
			phi_source *phi_s;
			ESSL_CHECK(_essl_ptrdict_insert(&phi_to_node, phi, phi->phi_node));
			ESSL_CHECK(_essl_ptrdict_insert(&var_ref_to_phi, &phi->phi_node, phi));
			for (phi_s = phi->phi_node->expr.u.phi.sources ; phi_s != 0 ; phi_s = phi_s->next) {
				ESSL_CHECK(_essl_ptrdict_insert(&var_ref_to_phi, &phi_s->source, phi));
			}
		}
	}

	/* Coalesce all phi nodes with their sources. If the live range of a source
	   overlaps that of the phi node, split the source. */
	for(i = 0; i < cfg->n_blocks; ++i)
	{
		basic_block *block = cfg->output_sequence[i];
		phi_list *phi;
		for (phi = block->phi_nodes ; phi != 0 ; phi = phi->next) {
			node *phi_node = _essl_ptrdict_lookup(&phi_to_node, phi);
			live_range *phi_range = get_range_for_var(ctx, phi_node, &range_replace);
			phi_source *phi_s;

			/* Split every phi source whose live range overlaps
			   the end of a source block of the same phi node. */
			for (phi_s = phi_node->expr.u.phi.sources ; phi_s != 0 ; phi_s = phi_s->next) {
				node *source = phi_s->source;
				live_range *source_range = get_range_for_var(ctx, source, &range_replace);
				essl_bool split = ESSL_FALSE;
				phi_source *phi_s2;
				for (phi_s2 = phi_node->expr.u.phi.sources ; phi_s2 != 0 ; phi_s2 = phi_s2->next) {
					if (phi_s2->source != phi_s->source && range_covers_source(ctx, source_range, phi_s2)) {
						split = ESSL_TRUE;
						break;
					}
				}
				if (split) {
					live_range *range2;
					ESSL_CHECK(range2 = split_source(pool, ctx, phi_s, phi_range, source_range, insert_move, callback_ctx));
					ESSL_CHECK(add_range(ctx, range2));
				}
			}


			for (phi_s = phi_node->expr.u.phi.sources ; phi_s != 0 ; phi_s = phi_s->next) {
				node *source = phi_s->source;
				live_range *source_range = get_range_for_var(ctx, source, &range_replace);

				/* Merge source if possible */
				assert(range_ok(phi_range));
				assert(range_ok(source_range));
				if (_essl_liveness_merge_live_ranges(phi_range, source_range)) {
					/* Ranges have been merged */
					assert(range_ok(phi_range));
					assert(range_ok(source_range));
					if (source_range != phi_range) {
						ESSL_CHECK(_essl_ptrdict_insert(&range_replace, source_range, phi_range));
					}
				} else {
					live_range *range2;
					ESSL_CHECK(range2 = split_source(pool, ctx, phi_s, phi_range, source_range, insert_move, callback_ctx));

					/* Merge new range into the phi node range */
					DO_ASSERT(_essl_liveness_merge_live_ranges(phi_range, range2));
					assert(range_ok(phi_range));
					assert(range_ok(range2));
				}
			}

		}
	}

	/* Clean up unused ranges */
	{
		ptrdict_iter iter;
		node *var;
		live_range *range, **rangep;
		_essl_ptrdict_iter_init(&iter, &ctx->var_to_range);
		while ((var = _essl_ptrdict_next(&iter, (void **)(void*)&range)) != 0) {
			if (range->points == 0) {
				/* Range is trash */
				_essl_ptrdict_remove(&ctx->var_to_range, var);
			}
		}

		for (rangep = &ctx->var_ranges ; *rangep != 0 ;) {
			if ((*rangep)->points == 0) {
				/* Range is trash */
				*rangep = (*rangep)->next;
			} else {
				rangep = &(*rangep)->next;
			}
		}
	}

	/* Remove or rewrite phi delimiters from ranges */
	{
		live_range *range;
		for (range = ctx->var_ranges ; range != 0 ; range = range->next) {
			remove_phi_from_range(range, &var_ref_to_phi, &phi_to_node);
		}
	}

	/* Remove all phi nodes */
	for(i = 0; i < cfg->n_blocks; ++i)
	{
		basic_block *block = cfg->output_sequence[i];
		block->phi_nodes = 0;
	}

	return MEM_OK;
}


