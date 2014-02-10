/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "maligp2/maligp2_constant_register_spilling.h"
#include "maligp2/maligp2_instruction.h"
#include "common/symbol_table.h"
#include "maligp2/maligp2_relocation.h"
#include "maligp2/maligp2_slot.h"
#include "maligp2/maligp2_liveness.h"


#define MALIGP2_SCALAR_SIZE SIZE_FP32

typedef struct _tag_reg_usage {
	int n_loads;
	int n_stores;
	int last_loaded_cycle;
	int last_stored_cycle;
	symbol *constreg;
	int new_reg;
} reg_usage;

typedef struct _tag_constreg_context {
	mempool *pool;
	virtual_reg_context *vr_ctx;
	control_flow_graph *cfg;
	typestorage_context *ts_ctx;
	maligp2_relocation_context *rel_ctx;
	liveness_context *liv_ctx;
	reg_usage *reg_usage;
	ptrdict load_words;
	ptrdict store_words;
} constreg_context;

static int get_instruction_reg(constreg_context *ctx, maligp2_instruction *inst)
{
	int vreg = inst->address_offset / MALIGP2_NATIVE_VECTOR_SIZE;
	int reg = _essl_maligp2_virtual_reg_get(ctx->vr_ctx, vreg)->alloc_reg;
	assert(reg >= 0 && reg < ctx->vr_ctx->n_regs_used);
	return reg;
}

static int reg_usage_cost(constreg_context *ctx, int r)
{
	return ctx->reg_usage[r].n_loads + ctx->reg_usage[r].n_stores;
}

static void count_load(constreg_context *ctx, maligp2_instruction *load, int cycle)
{
	if (load != 0 && load->opcode == MALIGP2_LOAD_WORK_REG)
	{
		int reg = get_instruction_reg(ctx, load);
		if (ctx->reg_usage[reg].last_loaded_cycle != cycle)
		{
			ctx->reg_usage[reg].n_loads++;
		}
		ctx->reg_usage[reg].last_loaded_cycle = cycle;
	}
}

static void count_store(constreg_context *ctx, maligp2_instruction *store, int cycle)
{
	if (store != 0 && store->opcode == MALIGP2_STORE_WORK_REG)
	{
		int reg = get_instruction_reg(ctx, store);
		if (ctx->reg_usage[reg].last_stored_cycle != cycle)
		{
			ctx->reg_usage[reg].n_stores++;
		}
		ctx->reg_usage[reg].last_stored_cycle = cycle;
	}
}

static memerr calculate_reg_usage(constreg_context *ctx) {
	basic_block *block;
	unsigned i;
	control_flow_graph *cfg = ctx->cfg;
	ESSL_CHECK(ctx->reg_usage = _essl_mempool_alloc(ctx->pool, ctx->vr_ctx->n_regs_used * sizeof(reg_usage)));
	for (i = 0; i < cfg->n_blocks; ++i)
	{
		maligp2_instruction_word *word;
		block = cfg->output_sequence[i];
		for (word = block->earliest_instruction_word ; word != 0 ; word = word->successor)
		{
			int i;
			for (i = 0 ; i < 4 ; i++)
			{
				count_load(ctx, word->u.real_slots.load0[i], word->cycle);
				count_load(ctx, word->u.real_slots.load1[i], word->cycle);
			}
			count_store(ctx, word->u.real_slots.store_xy, word->cycle);
			count_store(ctx, word->u.real_slots.store_zw, word->cycle);
		}
	}

	return MEM_OK;
}

static const char spillname[] = "spill";
static symbol *make_spill_symbol(constreg_context *ctx)
{
	string s = { spillname, 5 };
	const type_specifier *type;
	ESSL_CHECK(type = _essl_get_type_with_size(ctx->ts_ctx, TYPE_FLOAT, 4, MALIGP2_SCALAR_SIZE));
	return _essl_new_variable_symbol_with_default_qualifiers(ctx->pool, s, type, SCOPE_LOCAL, ADDRESS_SPACE_THREAD_LOCAL, UNKNOWN_SOURCE_OFFSET);
}

static node *make_spill_var_ref(constreg_context *ctx, int reg)
{
	symbol *var_symbol = ctx->reg_usage[reg].constreg;
	symbol_list *lst;
	node *var;
	node_extra *info;
	ESSL_CHECK(var = _essl_new_variable_reference_expression(ctx->pool, var_symbol));
	ESSL_CHECK(info = _essl_create_extra_info(ctx->pool, var));
	ESSL_CHECK(lst = _essl_mempool_alloc(ctx->pool, sizeof(*lst)));
	lst->sym = var_symbol;
	info->address_symbols = lst;
	return var;
}

static memerr mark_spilled_regs(constreg_context *ctx)
{
	int n_spills = ctx->vr_ctx->n_regs_used - ESSL_MAX(ctx->vr_ctx->n_regs - 2, 0);
	int r;
	int cost_threshold = 3;
	int n_below_threshold;
	int n_spilled;
	do {
		cost_threshold++;
		n_below_threshold = 0;
		for (r = 0 ; r < ctx->vr_ctx->n_regs_used ; r++)
		{
			int cost = reg_usage_cost(ctx, r);
			if (cost < cost_threshold) n_below_threshold++;
		}
	} while (n_below_threshold < n_spills);

	n_spilled = 0;
	for (r = 0 ; n_spilled < n_spills && r < ctx->vr_ctx->n_regs_used ; r++)
	{
		if (reg_usage_cost(ctx, r) < cost_threshold)
		{
			/* Mark spill */
			ESSL_CHECK(ctx->reg_usage[r].constreg = make_spill_symbol(ctx));
			n_spilled++;
		}
	}

	return MEM_OK;
}

static maligp2_instruction *put_instruction(constreg_context *ctx, maligp2_instruction_word *word,
									 maligp2_schedule_classes sc, maligp2_opcode opcode,
									 node *out, int comp)
{
	int subcycle;
	maligp2_instruction *inst;
	essl_bool failed_alloc = ESSL_FALSE;
	ESSL_CHECK(inst = _essl_maligp2_create_slot_instruction(ctx->pool, word, 
													   opcode == MALIGP2_CONSTANT ? 0/* index */ : sc,
													   opcode, out, &subcycle, comp, &failed_alloc));
	if(failed_alloc) return NULL;
	word->used_slots |= sc;
	inst->instr_node = out;
	return inst;
}

static void unspill_node(node *n)
{
	n->hdr.kind = EXPR_KIND_UNARY;
	n->expr.operation = EXPR_OP_IDENTITY;
}

static memerr spill_load_instruction(constreg_context *ctx, maligp2_instruction **loadp, maligp2_instruction_word *spill_word, int reg, int comp)
{
	if (*loadp && (*loadp)->opcode == MALIGP2_LOAD_WORK_REG && get_instruction_reg(ctx, *loadp) == reg)
	{
		maligp2_instruction *const_load;
		unspill_node((*loadp)->instr_node);
		if (spill_word->u.real_slots.load_const[comp] == 0)
		{
			/* Create a load instruction to load from the constant register */
			ESSL_CHECK(const_load = put_instruction(ctx, spill_word, MALIGP2_SC_LOAD_CONST, MALIGP2_LOAD_CONSTANT, (*loadp)->instr_node, comp));
			const_load->address_offset = comp;
			ESSL_CHECK(_essl_maligp2_add_address_offset_relocation(ctx->rel_ctx, ctx->reg_usage[reg].constreg, const_load));
			DO_ASSERT(_essl_maligp2_reserve_move(ctx->cfg, ctx->liv_ctx->desc, spill_word, (*loadp)->instr_node));
		} else {
			/* Load instruction already present - redirect references to this variable to the loaded one */
			node *old_var = (*loadp)->instr_node;
			node *loaded_var = spill_word->u.real_slots.load_const[comp]->instr_node;
			live_range *var_range = _essl_ptrdict_lookup(&ctx->liv_ctx->var_to_range, old_var);
			live_delimiter *delim;
			for (delim = var_range->points ; delim != 0 ; delim = delim->next)
			{
				assert(*delim->var_ref == old_var);
				*delim->var_ref = loaded_var;
			}
		}

		*loadp = 0;
	}
	return MEM_OK;
}

static memerr check_spill_load(constreg_context *ctx, maligp2_instruction *inst, maligp2_instruction_word *word, basic_block *block)
{
	if (inst != 0 && inst->opcode == MALIGP2_LOAD_WORK_REG)
	{
		int reg = get_instruction_reg(ctx, inst);
		if (ctx->reg_usage[reg].constreg != 0)
		{
			/* Register is spilled */
			maligp2_instruction_word *spill_word;
			int i;
			if (_essl_maligp2_inseparable_from_predecessor(word))
			{
				ESSL_CHECK(spill_word = _essl_maligp2_insert_word_before(ctx->pool, ctx->liv_ctx, word->predecessor, block));
			} else {
				ESSL_CHECK(spill_word = _essl_maligp2_insert_word_before(ctx->pool, ctx->liv_ctx, word, block));
			}

			ESSL_CHECK(_essl_ptrdict_insert(&ctx->load_words, spill_word, &ctx->reg_usage[reg]));

			for (i = 0 ; i < 4 ; i++)
			{
				ESSL_CHECK(spill_load_instruction(ctx, &word->u.real_slots.load0[i], spill_word, reg, i));
				ESSL_CHECK(spill_load_instruction(ctx, &word->u.real_slots.load1[i], spill_word, reg, i));
			}

		}
	}

	return MEM_OK;
}

static memerr spill_store_instruction(constreg_context *ctx, maligp2_instruction **storep, maligp2_instruction_word *spill_word, int reg, int comp)
{
	maligp2_instruction *const_store;
	ESSL_CHECK(const_store = put_instruction(ctx, spill_word,
										comp == 0 ? MALIGP2_SC_STORE_XY : MALIGP2_SC_STORE_ZW,
										MALIGP2_STORE_CONSTANT,
										NULL, 0));
	const_store->address_offset = 0;
	const_store->address_reg = MALIGP2_ADDR_A0_X;

	if (*storep && (*storep)->opcode == MALIGP2_STORE_WORK_REG && get_instruction_reg(ctx, *storep) == reg)
	{
		int i;
		for (i = 0 ; i < 2 ; i++)
		{
			const_store->args[i] = (*storep)->args[i];
			if ((*storep)->args[i].arg)
			{
				unspill_node((*storep)->args[i].arg);
				DO_ASSERT(_essl_maligp2_reserve_move(ctx->cfg, ctx->liv_ctx->desc, spill_word, (*storep)->args[i].arg));
			}
		}

		*storep = 0;
	}
	return MEM_OK;
}

static memerr check_spill_store(constreg_context *ctx, maligp2_instruction *inst, maligp2_instruction_word *word, basic_block *block)
{
	if (inst != 0 && inst->opcode == MALIGP2_STORE_WORK_REG)
	{
		int reg = get_instruction_reg(ctx, inst);
		if (ctx->reg_usage[reg].constreg != 0)
		{
			/* Register is spilled */
			maligp2_instruction_word *spill_word;
			if (_essl_maligp2_inseparable_from_successor(word))
			{
				ESSL_CHECK(spill_word = _essl_maligp2_insert_word_after(ctx->pool, ctx->liv_ctx, word->successor, block));
			} else {
				ESSL_CHECK(spill_word = _essl_maligp2_insert_word_after(ctx->pool, ctx->liv_ctx, word, block));
			}
			if (spill_word->u.real_slots.branch)
			{
				/* There is a branch instruction here already, so we have to insert yet another word */
				ESSL_CHECK(spill_word = _essl_maligp2_insert_word_before(ctx->pool, ctx->liv_ctx, spill_word, block));
			}

			ESSL_CHECK(_essl_ptrdict_insert(&ctx->store_words, spill_word, &ctx->reg_usage[reg]));

			/* Set address */
			{
				node *var;
				maligp2_instruction *set_address;
				maligp2_instruction *branch;
				ESSL_CHECK(var = make_spill_var_ref(ctx, reg));

				assert(spill_word->embedded_constants[0] == 0);
				assert(spill_word->n_embedded_constants == 0);
				spill_word->embedded_constants[0] = var;
				spill_word->n_embedded_constants = 1;
				ESSL_CHECK(put_instruction(ctx, spill_word, MALIGP2_SC_LOAD_CONST, MALIGP2_CONSTANT, var, 0));

				ESSL_CHECK(set_address = put_instruction(ctx, spill_word, MALIGP2_SC_LUT, MALIGP2_SET_ADDRESS, NULL, 0));
				set_address->args[0].arg = var;
				set_address->address_reg = MALIGP2_ADDR_A0_X;
				spill_word->used_slots |= MALIGP2_SC_A0_X;

				ESSL_CHECK(branch = put_instruction(ctx, spill_word, MALIGP2_SC_BRANCH, MALIGP2_STORE_SELECT_ADDRESS_REG, NULL, 0));
				branch->address_reg = MALIGP2_ADDR_A0_X;
			}

			ESSL_CHECK(spill_store_instruction(ctx, &word->u.real_slots.store_xy, spill_word, reg, 0));
			ESSL_CHECK(spill_store_instruction(ctx, &word->u.real_slots.store_zw, spill_word, reg, 2));

		}
	}

	return MEM_OK;
}

static essl_bool move_reservation_necessary(constreg_context *ctx, maligp2_instruction_word *word, node *var)
{
	/* Used for a load or store? */
	if (
		(word->u.real_slots.load0[0] && word->u.real_slots.load0[0]->instr_node == var) ||
		(word->u.real_slots.load0[1] && word->u.real_slots.load0[1]->instr_node == var) ||
		(word->u.real_slots.load0[2] && word->u.real_slots.load0[2]->instr_node == var) ||
		(word->u.real_slots.load0[3] && word->u.real_slots.load0[3]->instr_node == var) ||
		(word->u.real_slots.load1[0] && word->u.real_slots.load1[0]->instr_node == var) ||
		(word->u.real_slots.load1[1] && word->u.real_slots.load1[1]->instr_node == var) ||
		(word->u.real_slots.load1[2] && word->u.real_slots.load1[2]->instr_node == var) ||
		(word->u.real_slots.load1[3] && word->u.real_slots.load1[3]->instr_node == var) ||
		(word->u.real_slots.load_const[0] && word->u.real_slots.load_const[0]->instr_node == var) ||
		(word->u.real_slots.load_const[1] && word->u.real_slots.load_const[1]->instr_node == var) ||
		(word->u.real_slots.load_const[2] && word->u.real_slots.load_const[2]->instr_node == var) ||
		(word->u.real_slots.load_const[3] && word->u.real_slots.load_const[3]->instr_node == var) ||
		(word->u.real_slots.store_xy && word->u.real_slots.store_xy->args[0].arg == var) ||
		(word->u.real_slots.store_xy && word->u.real_slots.store_xy->args[1].arg == var) ||
		(word->u.real_slots.store_zw && word->u.real_slots.store_zw->args[0].arg == var) ||
		(word->u.real_slots.store_zw && word->u.real_slots.store_zw->args[1].arg == var)
		)
	{
		return ESSL_TRUE;
	}

	/* Fixed-point range? */
	{
		live_range *range = _essl_ptrdict_lookup(&ctx->liv_ctx->var_to_range, var);
		if (_essl_maligp2_is_fixedpoint_range(range))
		{
			return ESSL_TRUE;
		}
	}

	return ESSL_FALSE;
}

static memerr remove_unused_move_reservations(constreg_context *ctx, maligp2_instruction_word *word)
{
	int i;
	for (i = 0  ; i < MALIGP2_MAX_MOVES ; i++)
	{
		if (word->reserved_moves[i] != 0 &&
			!move_reservation_necessary(ctx, word, word->reserved_moves[i]))
		{
			word->reserved_moves[i] = 0;
			word->n_moves_available++;
			word->n_moves_reserved--;
		}
	}
	return MEM_OK;
}

static memerr insert_spills(constreg_context *ctx) {
	basic_block *block;
	unsigned i;
	control_flow_graph *cfg = ctx->cfg;
	for (i = 0; i < cfg->n_blocks; ++i)
	{
		maligp2_instruction_word *word;
		block = cfg->output_sequence[i];
		for (word = block->earliest_instruction_word ; word != 0 ; word = word->successor)
		{
			int i;
			for (i = 0 ; i < 4 ; i++)
			{
				ESSL_CHECK(check_spill_load(ctx, word->u.real_slots.load0[i], word, block));
				ESSL_CHECK(check_spill_load(ctx, word->u.real_slots.load1[i], word, block));
			}
			ESSL_CHECK(check_spill_store(ctx, word->u.real_slots.store_xy, word, block));
			ESSL_CHECK(check_spill_store(ctx, word->u.real_slots.store_zw, word, block));

			ESSL_CHECK(remove_unused_move_reservations(ctx, word));
		}
	}

	return MEM_OK;
}

static memerr insert_extra_delay_words(constreg_context *ctx)
{
	int block_i;
	ptrdict last_load_word;
	ESSL_CHECK(_essl_ptrdict_init(&last_load_word, ctx->pool));
	for (block_i = ctx->cfg->n_blocks-1 ; block_i >= 0 ; block_i--)
	{
		basic_block *block = ctx->cfg->output_sequence[block_i];
		maligp2_instruction_word *word;
		for (word = block->latest_instruction_word ; word != 0 ; word = word->predecessor)
		{
			if (word->u.real_slots.branch && word->u.real_slots.branch->opcode == MALIGP2_JMP)
			{
				/* Unconditional jump - flush delays */
				ESSL_CHECK(_essl_ptrdict_clear(&last_load_word));
			}

			if (_essl_ptrdict_has_key(&ctx->store_words, word))
			{
				reg_usage *ru = _essl_ptrdict_lookup(&ctx->store_words, word);
				if (_essl_ptrdict_has_key(&last_load_word, ru))
				{
					maligp2_instruction_word *load_word = _essl_ptrdict_lookup(&last_load_word, ru);
					if (word->cycle - load_word->cycle < 4)
					{
						/* Too close */
						ESSL_CHECK(word = _essl_maligp2_insert_word_after(ctx->pool, ctx->liv_ctx, word, block));
					}
				}
			}

			if (_essl_ptrdict_has_key(&ctx->load_words, word))
			{
				reg_usage *ru = _essl_ptrdict_lookup(&ctx->load_words, word);
				ESSL_CHECK(_essl_ptrdict_insert(&last_load_word, ru, word));
			}
		}
	}

	return MEM_OK;
}

memerr _essl_maligp2_constant_register_spilling(mempool *pool, virtual_reg_context *vr_ctx, control_flow_graph *cfg,
												typestorage_context *ts_ctx, maligp2_relocation_context *rel_ctx,
												liveness_context *liv_ctx)
{
	constreg_context cr_ctx, *ctx = &cr_ctx;
	cr_ctx.pool = pool;
	cr_ctx.vr_ctx = vr_ctx;
	cr_ctx.cfg = cfg;
	cr_ctx.ts_ctx = ts_ctx;
	cr_ctx.rel_ctx = rel_ctx;
	cr_ctx.liv_ctx = liv_ctx;
	ESSL_CHECK(_essl_ptrdict_init(&cr_ctx.load_words, pool));
	ESSL_CHECK(_essl_ptrdict_init(&cr_ctx.store_words, pool));
	ESSL_CHECK(calculate_reg_usage(ctx));
	ESSL_CHECK(mark_spilled_regs(ctx));
/*
	{
		int i;
		for (i = 0 ; i < ctx->vr_ctx->n_regs_used ; i++)
		{
			printf("%2d: %3d loads + %3d stores = %3d%s\n", i,
				   ctx->reg_usage[i].n_loads, ctx->reg_usage[i].n_stores,
				   ctx->reg_usage[i].n_loads + ctx->reg_usage[i].n_stores,
				   ctx->reg_usage[i].constreg != 0 ? " *" : "");
		}
	}
*/
	ESSL_CHECK(insert_spills(ctx));
	ESSL_CHECK(insert_extra_delay_words(ctx));

	return MEM_OK;
}
