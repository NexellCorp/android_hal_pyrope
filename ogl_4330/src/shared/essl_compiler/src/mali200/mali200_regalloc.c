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
#include "backend/eliminate_phi_nodes.h"
#include "backend/graph_coloring.h"
#include "backend/extra_info.h"
#include "mali200/mali200_instruction.h"
#include "mali200/mali200_slot.h"
#include "mali200/mali200_regalloc.h"
#include "mali200/mali200_register_integration.h"
#include "mali200/mali200_spilling.h"
#include "mali200/mali200_word_insertion.h"
#include "mali200/mali200_word_splitting.h"
#include "common/error_reporting.h"
#include "common/essl_profiling.h"

#ifdef DEBUGPRINT
#include "mali200/mali200_printer.h"
#include "backend/liveness_printer.h"
#endif

static memerr mark_fixed_register_variables(mempool *pool, symbol *function)
{
	basic_block *exit_block = function->control_flow_graph->exit_block;
	/* Put return value in a fixed register */
	if (exit_block->source != NULL)
	{
		preallocated_var *prealloc;
		ESSL_CHECK(prealloc = LIST_NEW(pool, preallocated_var));
		prealloc->var = exit_block->source;
		prealloc->allocation.reg = M200_R0;
		prealloc->allocation.swizzle = _essl_create_identity_swizzle(GET_NODE_VEC_SIZE(prealloc->var));
		LIST_INSERT_FRONT(&exit_block->preallocated_uses, prealloc);
	}
	return MEM_OK;
}

static int words_spanned_by_range(live_range *range) {
	int first_cycle = POSITION_TO_CYCLE(range->start_position);
	int last_cycle;
	live_delimiter *delim;
	for (delim = range->points ; delim->next != 0 ; delim = delim->next);
	last_cycle = POSITION_TO_CYCLE(delim->position);
	return first_cycle - last_cycle + 1;
}

memerr _essl_mali200_allocate_reg(regalloc_context *ctx, live_range *range, int reg, swizzle_pattern *swz) {
	/* Is this an ordinary register? */
	if (reg < M200_MAX_REGS) {
		ESSL_CHECK(_essl_reservation_allocate_reg(ctx->res_ctx, range, reg, swz));
		if (reg >= ctx->used_regs) {
			ctx->used_regs = reg+1;
		}
	}

	/* Record the allocation in the node info */
	{
		node_extra *info;
		ESSL_CHECK(info = EXTRA_INFO(range->var));
		info->out_reg = reg;
		info->reg_swizzle = *swz;
		info->reg_allocated = 1;
		range->allocated = 1;
	}

	return MEM_OK;
}

/** Find a register for the given variable and allocate it. */
static memerr allocate_range(regalloc_context *ctx, live_range *range) {
	assert (range->allocated || range->points != 0);
	if (!range->allocated && !range->spilled) {
		swizzle_pattern swz;
		int reg = _essl_reservation_find_available_reg(ctx->res_ctx, range, &swz);
		if (reg == -1) {
			/* Mark range for spilling */
			assert(range->potential_spill);
			ESSL_CHECK(_essl_ptrset_insert(&ctx->unallocated_ranges, range));
		} else {
			ESSL_CHECK(_essl_mali200_allocate_reg(ctx, range, reg, &swz));
		}
	}
	return MEM_OK;
}

static memerr init_regalloc_context(regalloc_context *ctx, mempool *pool, symbol *function, int numregs, liveness_context *liv_ctx, mali200_relocation_context *rel_ctx) {
	int first_position = START_OF_CYCLE(function->control_flow_graph->entry_block->top_cycle);
	ctx->pool = pool;
	ctx->function = function;
	ctx->cfg = function->control_flow_graph;
	ctx->numregs = numregs;
	ctx->used_regs = 0;
	ctx->liv_ctx = liv_ctx;
	ctx->rel_ctx = rel_ctx;
	ESSL_CHECK(_essl_ptrset_init(&ctx->unallocated_ranges, pool));
	ESSL_CHECK(_essl_ptrdict_init(&ctx->hash_load_range, pool));
	ESSL_CHECK(ctx->res_ctx = _essl_create_reservation_context(ctx->pool, numregs, first_position, NULL));
	return MEM_OK;
}

static memerr reset_allocations(regalloc_context *ctx) {
	int first_position = START_OF_CYCLE(ctx->function->control_flow_graph->entry_block->top_cycle);
	live_range *range;
	_essl_ptrset_clear(&ctx->unallocated_ranges);
	ESSL_CHECK(ctx->res_ctx = _essl_create_reservation_context(ctx->pool, ctx->numregs, first_position, NULL));
	ctx->used_regs = 0;

	for (range = ctx->liv_ctx->var_ranges ; range != 0 ; range = range->next) {
		node_extra *info;
		ESSL_CHECK(info = EXTRA_INFO(range->var));
		if (range->allocated && info->out_reg < M200_MAX_REGS) {
			/* Undo allocation */
			info->reg_allocated = 0;
			range->allocated = 0;
		}
		range->potential_spill = 0;
	}

	return MEM_OK;
}

static memerr allocate_all_ranges(regalloc_context *ctx) {
	/* Allocate pre-allocated ranges */
	{
		basic_block *block;
		for (block = ctx->cfg->entry_block ; block != NULL ; block = block->next)
		{
			preallocated_var *prealloc;
			for (prealloc = block->preallocated_uses ; prealloc != NULL ; prealloc = prealloc->next)
			{
				live_range *range = _essl_ptrdict_lookup(&ctx->liv_ctx->var_to_range, prealloc->var);
				if (!range->allocated)
				{
					ESSL_CHECK(_essl_mali200_allocate_reg(ctx, range, prealloc->allocation.reg, &prealloc->allocation.swizzle));
				}
			}
			for (prealloc = block->preallocated_defs ; prealloc != NULL ; prealloc = prealloc->next)
			{
				live_range *range = _essl_ptrdict_lookup(&ctx->liv_ctx->var_to_range, prealloc->var);
				if (!range->allocated)
				{
					ESSL_CHECK(_essl_mali200_allocate_reg(ctx, range, prealloc->allocation.reg, &prealloc->allocation.swizzle));
				}
			}
		}
	}

	/* Allocate all remaining ranges */
	{
		live_range *range;
		for (range = ctx->liv_ctx->var_ranges ; range != 0 ; range = range->next) {
			ESSL_CHECK(allocate_range(ctx, range));
		}
	}
	return MEM_OK;
}

static essl_bool any_unallocated_ranges(regalloc_context *ctx) {
	return _essl_ptrset_size(&ctx->unallocated_ranges) != 0;
}

static essl_bool any_unallocated_spill_ranges(regalloc_context *ctx) {
	live_range *range;
	for (range = ctx->liv_ctx->var_ranges ; range != 0 ; range = range->next) {
		if (range->unspillable &&
			_essl_ptrset_has(&ctx->unallocated_ranges, range))
		{
			ctx->failed_range = range;
			return ESSL_TRUE;
		}
	}
	return ESSL_FALSE;
}

static memerr prepare_ranges_for_coloring(regalloc_context *ctx)
{
	/* Mark spill ranges and one-word ranges as unspillable */
	{
		live_range *range;
		for (range = ctx->liv_ctx->var_ranges ; range != 0 ; range = range->next) {
			if (range->spill_range || words_spanned_by_range(range) == 1) {
				range->unspillable = 1;
			}
		}
	}

	ESSL_CHECK(_essl_liveness_mark_fixed_ranges(ctx->liv_ctx));

	return MEM_OK;
}

#ifdef DEBUGPRINT
#define DEBUGP(exp) \
	if ((pfile = fopen("out-mali200.txt", "a")) != 0) { \
		ESSL_CHECK(exp); \
		fclose(pfile); \
	}
#else
#define DEBUGP(exp)
#endif

memerr _essl_mali200_allocate_registers(mempool *pool, symbol *function, target_descriptor *desc, int numregs, mali200_relocation_context *rel_ctx, unique_name_context *names) {
	regalloc_context ra_ctx, *ctx = &ra_ctx;
	error_context *err = rel_ctx->err_ctx;
	liveness_context *liv_ctx;
	essl_bool overpressure = ESSL_FALSE;
	essl_bool coloring_ok;
	int n_iterations;
#ifdef DEBUGPRINT
	FILE *pfile;
#endif
	IGNORE_PARAM(names);

	ESSL_CHECK(mark_fixed_register_variables(pool, function));

	TIME_PROFILE_START("live_ranges");
	ESSL_CHECK(liv_ctx = _essl_mali200_calculate_live_ranges(pool, function->control_flow_graph, desc, err));
	TIME_PROFILE_STOP("live_ranges");
	DEBUGP(_essl_liveness_printer(pfile, liv_ctx->var_ranges, names, M200_NATIVE_VECTOR_SIZE));

	

	TIME_PROFILE_START("_regalloc");

	TIME_PROFILE_START("eliminate_phi_nodes");
	ESSL_CHECK(_essl_eliminate_phi_nodes(pool, liv_ctx, _essl_mali200_phielim_insert_move, liv_ctx));
	TIME_PROFILE_STOP("eliminate_phi_nodes");
	ESSL_CHECK(init_regalloc_context(ctx, pool, function, numregs, liv_ctx, rel_ctx));

	n_iterations = 0;
	do {
		DEBUGP(_essl_m200_print_function(pfile, pool, function, &names));

		if (n_iterations > MALI200_MAX_REGALLOC_ITERATIONS)
		{
			/* Give up */
			(void)REPORT_ERROR(err, ERR_INTERNAL_COMPILER_ERROR, 0, "%s register allocation failed for fragment shader.\n",
							   _essl_mali_core_name(desc->core));
			return MEM_ERROR;
		}

		overpressure = ESSL_FALSE;
		ESSL_CHECK(prepare_ranges_for_coloring(ctx));
		TIME_PROFILE_START("graph_coloring");
		ESSL_CHECK(_essl_sort_live_ranges_by_graph_coloring(pool, liv_ctx, ctx->numregs, _essl_mali200_spill_cost, NULL, &coloring_ok, _essl_graph_coloring_default_is_definitely_colorable, NULL, desc, names));
		TIME_PROFILE_STOP("graph_coloring");
		DEBUGP(_essl_liveness_printer(pfile, liv_ctx->var_ranges, names, M200_NATIVE_VECTOR_SIZE));
		TIME_PROFILE_START("allocate_ranges");
		ESSL_CHECK(allocate_all_ranges(ctx));
		TIME_PROFILE_STOP("allocate_ranges");
		while (any_unallocated_ranges(ctx)) {
			if (any_unallocated_spill_ranges(ctx)) {
				/* Overpressured word somewhere */
				m200_instruction_word *word;
				basic_block *block;
				ESSL_CHECK(word = _essl_mali200_find_word_for_spill(ctx, ctx->failed_range, &block));
				ESSL_CHECK(_essl_mali200_split_word(ctx, word, block));

				ESSL_CHECK(reset_allocations(ctx));
				
				DEBUGP(_essl_liveness_printer(pfile, liv_ctx->var_ranges, names, M200_NATIVE_VECTOR_SIZE));

				TIME_PROFILE_START("live_ranges");
				ESSL_CHECK(liv_ctx = _essl_mali200_calculate_live_ranges(pool, function->control_flow_graph, desc, err));
				TIME_PROFILE_STOP("live_ranges");
				ESSL_CHECK(init_regalloc_context(ctx, pool, function, numregs, liv_ctx, rel_ctx));

				overpressure = ESSL_TRUE;
				break;
			}
			/*printf("Spilling!\n");*/
			TIME_PROFILE_START("create_spills");
			ESSL_CHECK(_essl_mali200_create_spill_ranges(ctx));
			TIME_PROFILE_STOP("create_spills");
			ESSL_CHECK(reset_allocations(ctx));
			ESSL_CHECK(prepare_ranges_for_coloring(ctx));
			TIME_PROFILE_START("graph_coloring");
			ESSL_CHECK(_essl_sort_live_ranges_by_graph_coloring(pool, liv_ctx, ctx->numregs, _essl_mali200_spill_cost, NULL, &coloring_ok, _essl_graph_coloring_default_is_definitely_colorable, NULL, desc, names));
			TIME_PROFILE_STOP("graph_coloring");
			/* OPT: When a range defined by a uniform load is spilled, eliminate original load and transform all spill loads into uniform loads */
			/* OPT: Create spill ranges across solid blocks of instructions rather than individual instructions */
			DEBUGP(_essl_liveness_printer(pfile, liv_ctx->var_ranges, names, M200_NATIVE_VECTOR_SIZE));
			TIME_PROFILE_START("allocate_ranges");
			ESSL_CHECK(allocate_all_ranges(ctx));
			TIME_PROFILE_STOP("allocate_ranges");
		}

		n_iterations++;
	} while (overpressure);

	/* Write allocations into instructions */
	DEBUGP(_essl_liveness_printer(pfile, liv_ctx->var_ranges, names, M200_NATIVE_VECTOR_SIZE));
	TIME_PROFILE_START("insert_spills");
	ESSL_CHECK(_essl_mali200_insert_spills(ctx));
	TIME_PROFILE_STOP("insert_spills");
	DEBUGP(_essl_m200_print_function(pfile, pool, function, &names));
	TIME_PROFILE_START("integrate_allocations");
	_essl_mali200_integrate_allocations(ctx->cfg);
	TIME_PROFILE_STOP("integrate_allocations");

	TIME_PROFILE_STOP("_regalloc");

	return MEM_OK;
}
