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
#include "maligp2/maligp2_virtual_regs.h"
#include "maligp2/maligp2_instruction.h"
#include "backend/liveness.h"
#include "backend/reservation.h"
#include "backend/graph_coloring.h"

#ifdef DEBUGPRINT
#include "backend/liveness_printer.h"
#endif

#define INITIAL_REG_CAPACITY 10

virtual_reg_context *_essl_maligp2_virtual_reg_init(mempool *pool, compiler_options *opts) {
	virtual_reg_context *ctx;
	ESSL_CHECK(ctx = _essl_mempool_alloc(pool, sizeof(virtual_reg_context)));
	ctx->pool = pool;
	ctx->n_regs = opts->n_maligp2_work_registers;
	ctx->n_virtual_regs = 0;
	ctx->regs_capacity = INITIAL_REG_CAPACITY;
	ESSL_CHECK(ctx->regs = (virtual_reg **)_essl_mempool_alloc(pool, INITIAL_REG_CAPACITY*sizeof(virtual_reg *)));
	ESSL_CHECK(_essl_ptrdict_init(&ctx->var_to_reg, pool));
	ESSL_CHECK(_essl_ptrdict_init(&ctx->virtual_var_to_reg, pool));
	ESSL_CHECK(ctx->vr_type = _essl_new_type(pool));
	ctx->vr_type->basic_type = TYPE_FLOAT;
	ctx->vr_type->u.basic.vec_size = MALIGP2_NATIVE_VECTOR_SIZE;
	ctx->conflict_graph = NULL;
	return ctx;
}

static virtual_reg *new_virtual_reg(virtual_reg_context *ctx) {
	if (ctx->n_virtual_regs >= ctx->regs_capacity) {
		unsigned int new_capacity = ctx->regs_capacity*2;
		virtual_reg **new_regs;
		ESSL_CHECK(new_regs = _essl_mempool_alloc(ctx->pool, new_capacity*sizeof(virtual_reg *)));
		memcpy(new_regs, ctx->regs, ctx->n_virtual_regs*sizeof(virtual_reg *));
		ctx->regs = new_regs;
		ctx->regs_capacity = new_capacity;
	}

	{
		unsigned int index = ctx->n_virtual_regs++;
		node *virtual_var;
		virtual_reg *vreg;
		ESSL_CHECK(vreg = _essl_mempool_alloc(ctx->pool, sizeof(virtual_reg)));
		ESSL_CHECK(virtual_var = _essl_new_dont_care_expression(ctx->pool, ctx->vr_type));
		ctx->regs[index] = vreg;
		vreg->index = index;
		vreg->virtual_var = virtual_var;
		vreg->coalesced = ESSL_FALSE;
		vreg->alloc_reg = -1;
		ESSL_CHECK(_essl_ptrdict_insert(&ctx->virtual_var_to_reg, virtual_var, vreg));
		return vreg;
	}
}

essl_bool _essl_maligp2_virtual_reg_coalesce(virtual_reg_context *ctx, int reg1, int reg2) {
	virtual_reg *vreg1 = _essl_maligp2_virtual_reg_get(ctx, reg1);
	virtual_reg *vreg2 = _essl_maligp2_virtual_reg_get(ctx, reg2);
	int i;
	for (i = 0 ; i < MALIGP2_NATIVE_VECTOR_SIZE ; i++) {
		if (vreg1->vars[i] != 0 && vreg2->vars[i] != 0) {
			return ESSL_FALSE;
		}
	}
	vreg2->coalesced = ESSL_TRUE;
	vreg2->index = vreg1->index;
	for (i = 0 ; i < MALIGP2_NATIVE_VECTOR_SIZE ; i++) {
		if (vreg2->vars[i] != 0) {
			vreg1->vars[i] = vreg2->vars[i];
		}
	}
	return ESSL_TRUE;
}

essl_bool _essl_maligp2_virtual_reg_allocated(virtual_reg_context *ctx, node *var) {
	return _essl_ptrdict_has_key(&ctx->var_to_reg, var);
}

void _essl_maligp2_virtual_reg_get_allocation(virtual_reg_context *ctx, node *var, int *reg, int *comp) {
	virtual_reg *vreg;
	int i;
	assert(_essl_maligp2_virtual_reg_allocated(ctx, var));
	*reg = (long)_essl_ptrdict_lookup(&ctx->var_to_reg, var);
	vreg = _essl_maligp2_virtual_reg_get(ctx, *reg);
	for (i = 0 ; i < 4 ; i++) {
		if (vreg->vars[i] == var) {
			*comp = i;
			return;
		}
	}
}

virtual_reg *_essl_maligp2_virtual_reg_get(virtual_reg_context *ctx, int reg) {
	assert(reg < ctx->n_virtual_regs);
	while (ctx->regs[reg]->coalesced) {
		reg = ctx->regs[reg]->index;
	}
	return ctx->regs[reg];
}

virtual_reg *_essl_maligp2_virtual_reg_allocate(virtual_reg_context *ctx) {
	return new_virtual_reg(ctx);
}

memerr _essl_maligp2_virtual_reg_assign(virtual_reg_context *ctx, node *var, int reg, int comp) {
	virtual_reg *vreg = _essl_maligp2_virtual_reg_get(ctx, reg);
	long lreg = reg;
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->var_to_reg, var, (void *)lreg));
	vreg->vars[comp] = var;
	return MEM_OK;
}

static memerr reserve_ranges(virtual_reg_context *ctx, liveness_context *liv_ctx, int n_regs, essl_bool *ok) {
	reservation_context *res_ctx;
	live_range *range;
	swizzle_pattern swz = _essl_create_identity_swizzle(MALIGP2_NATIVE_VECTOR_SIZE);
	int first_position = START_OF_CYCLE(liv_ctx->cfg->entry_block->top_cycle);
	ESSL_CHECK(res_ctx = _essl_create_reservation_context(ctx->pool, n_regs, first_position, ctx->conflict_graph));

	ctx->n_regs_used = 0;
	for (range = liv_ctx->var_ranges ; range != 0 ; range = range->next) {
		int alloc_reg = _essl_reservation_find_available_reg(res_ctx, range, &swz);
		virtual_reg *vreg;
		if (alloc_reg == -1) {
			/* Out of work registers */
			assert(range->potential_spill);
			*ok = ESSL_FALSE;
			return MEM_OK;
		}
		vreg = _essl_ptrdict_lookup(&ctx->virtual_var_to_reg, range->var);
		assert(!vreg->coalesced);
		vreg->alloc_reg = alloc_reg;
		ESSL_CHECK(_essl_reservation_allocate_reg(res_ctx, range, alloc_reg, &swz));
		if (alloc_reg >= ctx->n_regs_used) {
			ctx->n_regs_used = alloc_reg+1;
		}
	}
	*ok = ESSL_TRUE;
	return MEM_OK;
}

static void integrate_address(virtual_reg_context *ctx, maligp2_instruction *inst) {
	if (inst != 0 && (inst->opcode == MALIGP2_LOAD_WORK_REG || inst->opcode == MALIGP2_STORE_WORK_REG)) {
		int reg = inst->address_offset / MALIGP2_NATIVE_VECTOR_SIZE;
		virtual_reg *vreg = _essl_maligp2_virtual_reg_get(ctx, reg);
		int new_address = vreg->alloc_reg * MALIGP2_NATIVE_VECTOR_SIZE + (inst->address_offset & 3);
		inst->address_offset = new_address;
		assert(vreg->alloc_reg != -1);
		assert(inst->address_offset != -1);
	}
}

static void integrate_allocations(virtual_reg_context *ctx, control_flow_graph *cfg) {
	basic_block *block;
	unsigned i;
	for (i = 0; i < cfg->n_blocks; ++i)
	{
		maligp2_instruction_word *word;
		block = cfg->output_sequence[i];
		for (word = block->earliest_instruction_word ; word != 0 ; word = word->successor) {
			int i;
			for (i = 0 ; i < MALIGP2_NATIVE_VECTOR_SIZE ; i++) {
				integrate_address(ctx, word->u.real_slots.load0[i]);
				integrate_address(ctx, word->u.real_slots.load1[i]);
			}
			integrate_address(ctx, word->u.real_slots.store_xy);
			integrate_address(ctx, word->u.real_slots.store_zw);
		}
	}
}

static memerr mark_store(virtual_reg_context *ctx, liveness_context *liv_ctx, maligp2_instruction *store, int position) {
	if (store != 0 && store->opcode == MALIGP2_STORE_WORK_REG) {
		int reg = store->address_offset / MALIGP2_NATIVE_VECTOR_SIZE;
		virtual_reg *vreg = _essl_maligp2_virtual_reg_get(ctx, reg);
		int mask = ((store->args[0].arg != 0) | ((store->args[1].arg != 0)*2)) << (store->address_offset & 2);
		ESSL_CHECK(_essl_liveness_mark_def(liv_ctx, &vreg->virtual_var, position, mask, 1));
	}
	return MEM_OK;
}

static memerr mark_load(virtual_reg_context *ctx, liveness_context *liv_ctx, maligp2_instruction **load, int position) {
	int reg = -1;
	int mask = 0;
	int i;
	for (i = 0 ; i < MALIGP2_NATIVE_VECTOR_SIZE ; i++) {
		if (load[i] != 0 && load[i]->opcode == MALIGP2_LOAD_WORK_REG && load[i]->instr_node != 0) {
			assert(reg == -1 || reg == load[i]->address_offset / MALIGP2_NATIVE_VECTOR_SIZE);
			reg = load[i]->address_offset / MALIGP2_NATIVE_VECTOR_SIZE;
			mask |= 1 << i;
		}
	}
	if (mask != 0) {
		virtual_reg *vreg = _essl_maligp2_virtual_reg_get(ctx, reg);
		ESSL_CHECK(_essl_liveness_mark_use(liv_ctx, &vreg->virtual_var, position, mask, 1, 0));
	}
	return MEM_OK;
}

static memerr mark_virtual_reg_defs_and_uses(liveness_context *liv_ctx, basic_block *block, void *vctx) {
	virtual_reg_context *ctx = vctx;
	maligp2_instruction_word *word;

	for (word = block->latest_instruction_word ; word != 0 ; word = word->predecessor) {
		int store_pos = END_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 0));
		int load_pos = START_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 2));
		ESSL_CHECK(mark_store(ctx, liv_ctx, word->u.real_slots.store_xy, store_pos));
		ESSL_CHECK(mark_store(ctx, liv_ctx, word->u.real_slots.store_zw, store_pos));
		ESSL_CHECK(mark_load(ctx, liv_ctx, word->u.real_slots.load0, load_pos));
		ESSL_CHECK(mark_load(ctx, liv_ctx, word->u.real_slots.load1, load_pos));
	}

	return MEM_OK;
}

static int no_spilling(graph_coloring_context *gc_ctx, live_range *range)
{
	IGNORE_PARAM(gc_ctx);
	IGNORE_PARAM(range);
	/* No sensible spilling weight yet */
	return 1;
}

/** Component mask for variable size */
static unsigned int mask_from_node(node *n) {
	return (1U << GET_NODE_VEC_SIZE(n))-1;
}

memerr _essl_maligp2_allocate_work_registers(virtual_reg_context *ctx, control_flow_graph *cfg, target_descriptor *desc, error_context *err, unique_name_context *names) {
	liveness_context *liv_ctx;
	essl_bool coloring_ok;

	IGNORE_PARAM(names);

	if (ctx->n_virtual_regs <= ctx->n_regs) {
		/* No more virtual registers than actual registers.
		   No need for expensive allocation.
		*/
		ctx->n_regs_used = ctx->n_virtual_regs;
		return MEM_OK;
	}

	ESSL_CHECK(liv_ctx = _essl_liveness_create_context(ctx->pool, cfg, desc, mark_virtual_reg_defs_and_uses, ctx, mask_from_node, err));
	ESSL_CHECK(_essl_liveness_calculate_live_ranges(liv_ctx));
	ESSL_CHECK(_essl_liveness_fix_dead_definitions(ctx->pool, liv_ctx->var_ranges, 0));
	ESSL_CHECK(_essl_sort_live_ranges_by_graph_coloring(ctx->pool, liv_ctx, ctx->n_regs, no_spilling, NULL, &coloring_ok, _essl_graph_coloring_default_is_definitely_colorable, ctx->conflict_graph, desc, names));

	/* Since we permit spilling, coloring will always be ok */
	assert(coloring_ok);

#ifdef DEBUGPRINT
	{
		FILE *out;
		if ((out = fopen("out-maligp2.txt", "a")) != 0) {
			ESSL_CHECK(_essl_liveness_printer(out, liv_ctx->var_ranges, names, 4));
			fclose(out);
		}
	}
#endif

	{
		int n_regs = ctx->n_regs;
		do {
			ESSL_CHECK(reserve_ranges(ctx, liv_ctx, n_regs, &coloring_ok));
			n_regs *= 2;
		} while (!coloring_ok);
	}

	if (ctx->n_regs_used <= ctx->n_regs) {
		integrate_allocations(ctx, liv_ctx->cfg);
	}

	return MEM_OK;
}

void _essl_maligp2_virtual_reg_set_conflict_graph(virtual_reg_context *ctx, interference_graph_context *conflict_graph)
{
	ctx->conflict_graph = conflict_graph;
}
