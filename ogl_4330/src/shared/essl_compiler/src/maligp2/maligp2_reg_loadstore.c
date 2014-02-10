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
#include "maligp2/maligp2_reg_loadstore.h"
#include "maligp2/maligp2_slot.h"
#include "maligp2/maligp2_virtual_regs.h"
#include "maligp2/maligp2_liveness.h"
#include "backend/extra_info.h"
#include "backend/instruction.h"

typedef struct _tag_maligp2_instruction_word_conflict_list {
	struct _tag_maligp2_instruction_word_conflict_list *next;
	maligp2_instruction_word *sword;
	maligp2_instruction_word *lword;
	basic_block *lblock;
} maligp2_instruction_word_conflict_list;

#ifndef NDEBUG
static essl_bool range_ok(live_range *range) {
	live_delimiter *delim;
	for (delim = range->points ; delim != 0 && delim->next != 0 ; delim = delim->next) {
		if (delim->next->position > delim->position) return ESSL_FALSE;
/*
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
*/
	}
	return ESSL_TRUE;
}

static void assert_all_ranges_ok(liveness_context *liv_ctx)
{
	live_range *range;
	for (range = liv_ctx->var_ranges ; range != 0 ; range = range->next) {
		assert(range_ok(range));
	}
}
#endif

static node *make_temp(loadstore_context *ctx, node *source) {
	node *temp;
	ESSL_CHECK(temp = _essl_new_unary_expression(ctx->pool, EXPR_OP_SPILL, source));
	_essl_ensure_compatible_node(temp, source);
	ESSL_CHECK(CREATE_EXTRA_INFO(ctx->pool, temp));
	return temp;
}

static memerr enqueue_delimiter(loadstore_context *ctx, live_delimiter *delimiter) {
	int priority = delimiter->position;
	assert(delimiter->next == 0 || delimiter->next->position <= delimiter->position);
	assert(!_essl_priqueue_has_element(&ctx->var_defs, delimiter));
	ESSL_CHECK(_essl_priqueue_insert(&ctx->var_defs, priority, delimiter));
	return MEM_OK;
}

static essl_bool more_delimiters(loadstore_context *ctx) {
	return _essl_priqueue_n_elements(&ctx->var_defs) > 0;
}

static live_delimiter *next_delimiter(loadstore_context *ctx) {
	if (more_delimiters(ctx)) {
		return (live_delimiter *)_essl_priqueue_remove_first(&ctx->var_defs);
	} else {
		return 0;
	}
}

static memerr rewrite_var_ref_for_spill(loadstore_context *ctx, node **var_ref, node *to, live_delimiter_kind kind, int position, int locked)
{
	live_delimiter *delim;
	/* Rewrite */
	*var_ref = to;
	/* Make liveness delimiter */
	ESSL_CHECK(delim = _essl_liveness_new_delimiter(ctx->pool, var_ref, kind, position));
	delim->mask = 0x1;
	delim->locked = locked;

	if (_essl_ptrdict_has_key(&ctx->liv_ctx->var_to_range, to)) {
		live_range *range = _essl_ptrdict_lookup(&ctx->liv_ctx->var_to_range, to);
		live_delimiter **delimp = &range->points;

		/* Find place in delimiter list */
		assert(kind == LIVE_DEF || position < (*delimp)->position);
		while (*delimp && (*delimp)->position > position)
		{
			delimp = &(*delimp)->next;
		}
		/* Check for existing delimiter for this var ref */
		{
			essl_bool exists = ESSL_FALSE;
			live_delimiter *check_delim;
			for (check_delim = *delimp ; check_delim != 0 && check_delim->position == delim->position ; check_delim = check_delim->next)
			{
				if (check_delim->var_ref == delim->var_ref)
				{
					exists = ESSL_TRUE;
					break;
				}
			}

			if (!exists)
			{
				/* Insert delimiter */
				delim->next = *delimp;
				*delimp = delim;
				range->start_position = range->points->position;
				assert(delim->next == 0 || delim->next->position <= position);
			}
		}
		assert(range_ok(range));
	} else {
		live_range *range;
		ESSL_CHECK(range = _essl_liveness_new_live_range(ctx->pool, to, delim));
		range->spill_range = 1;
		LIST_INSERT_FRONT(&ctx->liv_ctx->var_ranges, range);
		ESSL_CHECK(_essl_ptrdict_insert(&ctx->liv_ctx->var_to_range, to, range));
		assert(range_ok(range));
	}


	return MEM_OK;
}

static memerr rewrite_move_reservations(loadstore_context *ctx, maligp2_instruction_word *word, node *from, node *to) {
	assert(from != 0);
	assert(to != 0);
	if (word != 0) {
		int i;
		for (i = 0 ; i < MALIGP2_MAX_MOVES ; i++) {
			if (word->reserved_moves[i] == from || word->reserved_moves[i] == to) {
				int position = START_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 1));
				ESSL_CHECK(rewrite_var_ref_for_spill(ctx, &word->reserved_moves[i], to, LIVE_USE, position, 1));
			}
		}
	}
	return MEM_OK;
}

static void unreserve_move(maligp2_instruction_word *word, node *n) {
	int i;
	assert(n != 0);
	for (i = 0 ; i < MALIGP2_MAX_MOVES ; i++) {
		if (word->reserved_moves[i] == n) {
			word->reserved_moves[i] = 0;
			word->n_moves_available++;
			word->n_moves_reserved--;
			assert(word->n_moves_available >= 0);
			assert(word->n_moves_reserved >= 0);
			return;
		}
	}
	assert(0);
}

static maligp2_instruction *get_store_slot(maligp2_instruction_word *word, int comp) {
	switch (comp) {
	case 0:
	case 1:
		return word->u.real_slots.store_xy;
	case 2:
	case 3:
		return word->u.real_slots.store_zw;
	}
	assert(0);
	return 0;
}

/** Is there an available store slot in this word into which this variable
	can be stored without conflicting with existing allocations into virtual
	registers? */
static essl_bool can_be_stored_at(loadstore_context *ctx, node *var, maligp2_instruction_word *word, int *regp, int *compp) {
	if (_essl_maligp2_virtual_reg_allocated(ctx->vr_ctx, var)) {
		/* Already allocated into virtual register. See if it fits here. */
		maligp2_instruction *store_slot;
		_essl_maligp2_virtual_reg_get_allocation(ctx->vr_ctx, var, regp, compp);
		store_slot = get_store_slot(word, *compp);
		if (store_slot == 0) return ESSL_TRUE;
		if (store_slot->opcode == MALIGP2_STORE_WORK_REG && store_slot->args[(*compp)&1].arg == 0) {
			int mate_reg = store_slot->address_offset / MALIGP2_NATIVE_VECTOR_SIZE;
			assert(store_slot->address_offset != -1);
			return mate_reg == *regp;
		}
	} else {
		/* Not allocated. We can assign a register and component freely. */
		int i;
		for (i = 0 ; i < 4 ; i++) {
			int comp = (i + ctx->next_comp) & 3;
			maligp2_instruction *store = get_store_slot(word, comp);
			if (store == 0)
			{
				*regp = -1;
				*compp = comp;
				ctx->next_comp = comp+1;
				return ESSL_TRUE;
			}
			if (store->opcode == MALIGP2_STORE_WORK_REG && store->args[comp&1].arg == 0)
			{
				virtual_reg *mate_vreg = _essl_maligp2_virtual_reg_get(ctx->vr_ctx, store->address_offset / MALIGP2_NATIVE_VECTOR_SIZE);
				*regp = mate_vreg->index;
				*compp = comp;
				ctx->next_comp = comp+1;
				return mate_vreg->vars[comp] == 0;
			}
		}
	}

	return ESSL_FALSE;
}

static maligp2_instruction *alloc_store_slot(loadstore_context *ctx, maligp2_instruction_word *word, int comp) {
	switch (comp) {
	case 0:
	case 1:
		if (word->u.real_slots.store_xy == 0) {
			ESSL_CHECK(word->u.real_slots.store_xy = _essl_new_maligp2_instruction(ctx->pool, MALIGP2_SC_STORE_XY, MALIGP2_STORE_WORK_REG, MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 0)));
			word->used_slots |= MALIGP2_SC_STORE_XY;
		}
		return word->u.real_slots.store_xy;
	case 2:
	case 3:
		if (word->u.real_slots.store_zw == 0) {
			ESSL_CHECK(word->u.real_slots.store_zw = _essl_new_maligp2_instruction(ctx->pool, MALIGP2_SC_STORE_ZW, MALIGP2_STORE_WORK_REG, MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 0)));
			word->used_slots |= MALIGP2_SC_STORE_ZW;
		}
		return word->u.real_slots.store_zw;
	}
	assert(0);
	return 0;
}

/** Mark this variable as being stored at this word. Updates the unfinalized stores list
	and the virtual register allocations. */
static memerr mark_store_at(loadstore_context *ctx, live_delimiter *definition,
							maligp2_instruction_word *def_word, maligp2_instruction_word *result_word, maligp2_instruction_word *store_word,
							int reg, int comp) {
	node *var = *definition->var_ref;
	maligp2_instruction *store;
	loadstore_allocation *store_alloc;
	ESSL_CHECK(store = alloc_store_slot(ctx, store_word, comp));
	ESSL_CHECK(store_alloc = LIST_NEW(ctx->temp_pool, loadstore_allocation));
	store_alloc->var = var;
	store_alloc->definition = definition;
	store_alloc->def_word = def_word;
	store_alloc->result_word = result_word;
	store_alloc->store_word = store_word;
	store_alloc->block = ctx->current_block;
	store_alloc->store = store;
	store_alloc->index = comp & 1;
	LIST_INSERT_FRONT(&ctx->stores, store_alloc);

	/* Is the other component filled in? */
	if (reg == -1) {
		virtual_reg *vreg;
		ESSL_CHECK(vreg = _essl_maligp2_virtual_reg_allocate(ctx->vr_ctx));
		reg = vreg->index;
	}
	store->address_offset = reg * MALIGP2_NATIVE_VECTOR_SIZE + (comp & -2);
	store->args[comp&1].arg = var; /* Just to mark it as occupied */
	ESSL_CHECK(_essl_maligp2_virtual_reg_assign(ctx->vr_ctx, var, reg, comp));

	/* Change store to use temp variable */
	{
		node *temp;
		int def_position = definition->position;
		int store_position = START_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(store_word->cycle, 0));
		ESSL_CHECK(temp = make_temp(ctx, store_alloc->var));
		ESSL_CHECK(rewrite_var_ref_for_spill(ctx, store_alloc->definition->var_ref, temp, LIVE_DEF, def_position, store_alloc->definition->locked));
		ESSL_CHECK(rewrite_var_ref_for_spill(ctx, &store_alloc->store->args[store_alloc->index].arg, temp, LIVE_USE, store_position, 0));
		ESSL_CHECK(rewrite_move_reservations(ctx, store_alloc->def_word, store_alloc->var, temp));
		ESSL_CHECK(rewrite_move_reservations(ctx, store_alloc->result_word, store_alloc->var, temp));
		ESSL_CHECK(rewrite_move_reservations(ctx, store_alloc->store_word, store_alloc->var, temp));
	}

	return MEM_OK;
}

static essl_bool reserve_move(loadstore_context *ctx, maligp2_instruction_word *word, node *n) {
	return _essl_maligp2_reserve_move(ctx->cfg, ctx->liv_ctx->desc, word, n);
}

static memerr split_after(loadstore_context *ctx, maligp2_instruction_word *word) {
	ESSL_CHECK(_essl_maligp2_insert_word_after(ctx->pool, ctx->liv_ctx, word, ctx->current_block));
	return MEM_OK;
}

static memerr allocate_definition(loadstore_context *ctx, live_delimiter *definition, maligp2_instruction_word *word) {
	node *var = *definition->var_ref;
	maligp2_instruction_word *w2;
	int reg, comp;
	essl_bool reserved_for_mul_add = ESSL_FALSE;
	maligp2_instruction_word *result_word = word;
	if (word->u.real_slots.mul0 != 0 && word->u.real_slots.mul0->opcode == MALIGP2_MUL_ADD && definition->var_ref == &word->u.real_slots.mul0->instr_node) {
		/* This is a mul_add definition */
		result_word = word->successor->successor;
		if (definition->next != 0 &&
			definition->next->kind == LIVE_USE &&
			definition->next->position == START_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(result_word->cycle, 1)) &&
			definition->next->locked)
		{
			/* Already a move reservation there */
		} else {
			/* Reserve move */
			while (!reserve_move(ctx, result_word, var)) {
				/* No space for a move - insert extra word */
				if (_essl_maligp2_inseparable_from_predecessor(result_word)) {
					ESSL_CHECK(_essl_maligp2_insert_word_before(ctx->pool, ctx->liv_ctx, result_word->predecessor, ctx->current_block));
				} else {
					ESSL_CHECK(_essl_maligp2_insert_word_before(ctx->pool, ctx->liv_ctx, result_word, ctx->current_block));
				}
				result_word = result_word->predecessor;
			}
			reserved_for_mul_add = ESSL_TRUE;
		}
	}

	if (can_be_stored_at(ctx, var, result_word, &reg, &comp)) {
		ESSL_CHECK(mark_store_at(ctx, definition, word, result_word, result_word, reg, comp));
		return MEM_OK;
	} else if ((w2 = result_word->successor) != 0 &&
			   can_be_stored_at(ctx, var, w2, &reg, &comp) &&
			   reserve_move(ctx, w2, var)) {
		ESSL_CHECK(mark_store_at(ctx, definition, word, result_word, w2, reg, comp));
		return MEM_OK;
	} else if (result_word->successor != 0 && (w2 = result_word->successor->successor) != 0 &&
			   can_be_stored_at(ctx, var, w2, &reg, &comp) &&
			   reserve_move(ctx, w2, var)) {
		ESSL_CHECK(mark_store_at(ctx, definition, word, result_word, w2, reg, comp));
		return MEM_OK;
	}

	if (reserved_for_mul_add) {
		unreserve_move(result_word, var);
	}

	/* No store slots available. Insert a new word */
	if (_essl_maligp2_inseparable_from_successor(result_word)) {
		ESSL_CHECK(split_after(ctx, result_word->successor));
	} else {
		ESSL_CHECK(split_after(ctx, result_word));
	}
	return allocate_definition(ctx, definition, word);
}

static memerr allocate_stores(loadstore_context *ctx) {
	/* Enqueue all definitions */
	live_range *range;
	for (range = ctx->unallocated_ranges ; range != 0 ; range = range->next) {
		live_delimiter *delim;
		for (delim = range->points ; delim != 0 ; delim = delim->next) {
			if (delim->kind == LIVE_DEF) {
				ESSL_CHECK(enqueue_delimiter(ctx, delim));
			}
		}
		range->spilled = 1;
	}

	/* Allocate all definitions */
	while (more_delimiters(ctx))
	{
		live_delimiter *definition = next_delimiter(ctx);
		maligp2_instruction_word *word;
		basic_block *block;
		word = _essl_instruction_word_at_cycle(ctx->pool, ctx->cfg, POSITION_TO_CYCLE(definition->position), &block);
		ctx->current_block = block;
		ESSL_CHECK(allocate_definition(ctx, definition, word));
	}

	return MEM_OK;
}


static essl_bool can_be_loaded_at(loadstore_context *ctx, node *var, maligp2_instruction_word *word, int index,
							 maligp2_instruction_word *move_word)
{
	if (word != 0) {
		maligp2_schedule_classes sc = index == 0 ? MALIGP2_SC_LOAD0 : MALIGP2_SC_LOAD1;
		if ((word->used_slots & sc) == 0) {
			/* Load slot is free */
			return move_word == 0 || reserve_move(ctx, move_word, var);
		}
	}

	return ESSL_FALSE;
}

static essl_bool already_loaded_at(loadstore_context *ctx, node *var, maligp2_instruction_word *word, int index,
							  maligp2_instruction_word *move_word)
{
	if (word != 0) {
		maligp2_instruction **load_slots = index == 0 ? word->u.real_slots.load0 : word->u.real_slots.load1;
		int reg, comp;
		int i;
		maligp2_instruction *first_load = 0;
		_essl_maligp2_virtual_reg_get_allocation(ctx->vr_ctx, var, &reg, &comp);
		for (i = 0 ; i < 4 ; i++) {
			if (load_slots[i] != 0) {
				first_load = load_slots[i];
				break;
			}
		}

		if (first_load != 0 &&
			first_load->opcode == MALIGP2_LOAD_WORK_REG)
		{
			/* There is a work register load here */
			int load_reg = first_load->address_offset / MALIGP2_NATIVE_VECTOR_SIZE;
			if (load_reg == reg || _essl_maligp2_virtual_reg_coalesce(ctx->vr_ctx, load_reg, reg))
			{
				/* Same register */
				return move_word == 0 || reserve_move(ctx, move_word, var);
			}
		}
	}

	return ESSL_FALSE;
}

static memerr mark_load_at(loadstore_context *ctx, live_delimiter *use,
						   maligp2_instruction_word *load_word, maligp2_instruction_word *move_word, maligp2_instruction_word *use_word, 
						   int index)
{
	node *var = *use->var_ref;
	node *temp;
	maligp2_schedule_classes sc = index == 0 ? MALIGP2_SC_LOAD0 : MALIGP2_SC_LOAD1;
	maligp2_instruction **load_slots = index == 0 ? load_word->u.real_slots.load0 : load_word->u.real_slots.load1;
	int reg, comp;
	int use_position = use->position;
	_essl_maligp2_virtual_reg_get_allocation(ctx->vr_ctx, var, &reg, &comp);
	assert(load_word->cycle >= use_word->cycle);

	load_word->used_slots |= sc;
	if (load_slots[comp]) {
		/* Already loaded */
		temp = load_slots[comp]->instr_node;
		assert(temp);
		assert(load_slots[comp]->address_offset / MALIGP2_NATIVE_VECTOR_SIZE == reg);
	} else {
		/* Allocate slot */
		maligp2_instruction *load;
		int subcycle;
		essl_bool failed_alloc = ESSL_FALSE;
		ESSL_CHECK(temp = make_temp(ctx, var));
		ESSL_CHECK(load = _essl_maligp2_create_slot_instruction(ctx->pool, load_word, sc, MALIGP2_LOAD_WORK_REG, temp, &subcycle, comp, &failed_alloc));
		if(failed_alloc) return MEM_ERROR;
		ESSL_CHECK(rewrite_var_ref_for_spill(ctx, &load->instr_node, temp, LIVE_DEF, END_OF_SUBCYCLE(subcycle), 0));
		load->address_offset = reg * MALIGP2_NATIVE_VECTOR_SIZE + comp;
	}
	assert(*use->var_ref);
	ESSL_CHECK(rewrite_var_ref_for_spill(ctx, use->var_ref, temp, LIVE_USE, use_position, use->locked));
	ESSL_CHECK(rewrite_move_reservations(ctx, load_word, var, temp));
	ESSL_CHECK(rewrite_move_reservations(ctx, move_word, var, temp));
	ESSL_CHECK(rewrite_move_reservations(ctx, use_word, var, temp));
	return MEM_OK;
}

static memerr thread_from_def(loadstore_context *ctx, live_delimiter *use, maligp2_instruction_word *use_word,
							  loadstore_allocation *last_def)
{
	node *var = *use->var_ref;
	node *def_var = *last_def->definition->var_ref;
	int use_position = use->position;
	ESSL_CHECK(rewrite_var_ref_for_spill(ctx, use->var_ref, def_var, LIVE_USE, use_position, 0));
	ESSL_CHECK(rewrite_move_reservations(ctx, use_word, var, def_var));
	return MEM_OK;
}

static memerr allocate_use(loadstore_context *ctx, live_delimiter *use, maligp2_instruction_word *word,
						   live_delimiter *last_use_delim, loadstore_allocation *last_def)
{
	node *var = *use->var_ref;
	maligp2_instruction_word *pword = word->predecessor;
	maligp2_instruction_word *ppword = pword ? pword->predecessor : 0;
	maligp2_instruction_word *pppword = ppword ? ppword->predecessor : 0;

	maligp2_instruction_word *load_word;
	int load_index;
	maligp2_instruction_word *move_word;
	essl_bool new_word_needed = ESSL_TRUE;

	/* Check to see if the use can be threaded directly from last def */
	if (last_def && last_def->store_word->cycle - word->cycle <= 2) {
		return thread_from_def(ctx, use, word, last_def);
	}

	/* Earlier use in the same word? */
	if (last_use_delim) {
		node *use_var = *last_use_delim->var_ref;
		int use_position = use->position;
		ESSL_CHECK(rewrite_var_ref_for_spill(ctx, use->var_ref, use_var, LIVE_USE, use_position, 0));
		return MEM_OK;
	}

	if (
		already_loaded_at (ctx, var, load_word = word,    load_index = 1, move_word = 0)      ||
		already_loaded_at (ctx, var, load_word = word,    load_index = 0, move_word = 0)      ||
		already_loaded_at (ctx, var, load_word = pword,   load_index = 0, move_word = 0)      ||

		already_loaded_at (ctx, var, load_word = pword,   load_index = 1, move_word = pword)  ||
		already_loaded_at (ctx, var, load_word = ppword,  load_index = 0, move_word = pword)  ||
		already_loaded_at (ctx, var, load_word = ppword,  load_index = 1, move_word = ppword) ||
		already_loaded_at (ctx, var, load_word = ppword,  load_index = 0, move_word = ppword) ||

		can_be_loaded_at  (ctx, var, load_word = word,    load_index = 1, move_word = 0)      ||
		can_be_loaded_at  (ctx, var, load_word = word,    load_index = 0, move_word = 0)      ||
		can_be_loaded_at  (ctx, var, load_word = pword,   load_index = 0, move_word = 0)      ||

		can_be_loaded_at  (ctx, var, load_word = ppword,  load_index = 1, move_word = ppword) ||
		can_be_loaded_at  (ctx, var, load_word = ppword,  load_index = 0, move_word = ppword) ||
		can_be_loaded_at  (ctx, var, load_word = pword,   load_index = 1, move_word = pword)  ||
		can_be_loaded_at  (ctx, var, load_word = ppword,  load_index = 0, move_word = pword)  ||

		already_loaded_at (ctx, var, load_word = pppword, load_index = 0, move_word = ppword) ||
		can_be_loaded_at  (ctx, var, load_word = pppword, load_index = 0, move_word = ppword) ||
		ESSL_FALSE)
	{
		/* Load slot available */
		if (last_def && last_def->store_word->cycle - load_word->cycle <= 2) {
			/* Load would be too close to the store */
			if (move_word != 0) {
				unreserve_move(move_word, var);
			}
		} else {
			new_word_needed = ESSL_FALSE;
		}
	}

	if (new_word_needed) {
		/* No load slot available - insert word to hold load */
		maligp2_instruction_word *succ_word;

		if (_essl_maligp2_inseparable_from_predecessor(word)) {
			succ_word = pword;
		} else {
			succ_word = word;
		}

		/* Insert word and reserve a move in it */
		ESSL_CHECK(load_word = _essl_maligp2_insert_word_before(ctx->pool, ctx->liv_ctx, succ_word, ctx->current_block));
		DO_ASSERT(reserve_move(ctx, load_word, var));

		if (last_def && last_def->store_word->cycle - load_word->cycle <= 2) {
			/* Load would still be too close to store - just let the move handle the job */
			node *def_var = *last_def->definition->var_ref;
			ESSL_CHECK(rewrite_move_reservations(ctx, load_word, var, def_var));
			return thread_from_def(ctx, use, word, last_def);
		}

		/* Place the load in the inserted word */
		load_index = 1;
		move_word = load_word;
	}

	/* Insert load at reserved slot */
	ESSL_CHECK(mark_load_at(ctx, use, load_word, move_word, word, load_index));
	return MEM_OK;
}

static int allocation_comparator(generic_list *l1, generic_list *l2) {
	loadstore_allocation *a1 = (loadstore_allocation *)l1;
	loadstore_allocation *a2 = (loadstore_allocation *)l2;
	return a2->definition->position - a1->definition->position;
}

static memerr allocate_loads(loadstore_context *ctx) {
	/* Enqueue all uses */
	live_range *range;
	for (range = ctx->unallocated_ranges ; range != 0 ; range = range->next) {
		live_delimiter *delim;
		for (delim = range->points ; delim != 0 ; delim = delim->next) {
			if (delim->kind == LIVE_USE) {
				ESSL_CHECK(enqueue_delimiter(ctx, delim));
			}
		}
	}

	/* Allocate all uses */
	{
		live_delimiter *use = next_delimiter(ctx);
		loadstore_allocation *store_alloc;
		ptrdict last_use_delims, last_defs;
		unsigned block_i;
		ESSL_CHECK(_essl_ptrdict_init(&last_use_delims, ctx->temp_pool));
		ESSL_CHECK(_essl_ptrdict_init(&last_defs, ctx->temp_pool));
		ctx->stores = LIST_SORT(ctx->stores, allocation_comparator, loadstore_allocation);
		store_alloc = ctx->stores;
		for (block_i = 0 ; use && block_i < ctx->cfg->n_blocks ; block_i++) {
			basic_block *block = ctx->cfg->output_sequence[block_i];
			maligp2_instruction_word *word;
			ctx->current_block = block;
			for (word = block->earliest_instruction_word ; use && word != 0 ; word = word->successor) {
				ESSL_CHECK(_essl_ptrdict_clear(&last_use_delims));
				while (use &&
					   use->position >= END_OF_CYCLE(word->cycle)) {
					assert(POSITION_TO_CYCLE(use->position) == word->cycle);
					while (store_alloc && store_alloc->definition->position > use->position) {
						ESSL_CHECK(_essl_ptrdict_insert(&last_defs, store_alloc->var, store_alloc));
						store_alloc = store_alloc->next;
					}
					if (!use->locked)
					{
						/* A real use - not just a move reservation */
						node *use_var = *use->var_ref;
						live_delimiter *last_use_delim = _essl_ptrdict_lookup(&last_use_delims, use_var);
						loadstore_allocation *last_def = _essl_ptrdict_lookup(&last_defs, use_var);
						if (last_def->block != block) last_def = 0;
						ESSL_CHECK(allocate_use(ctx, use, word, last_use_delim, last_def));
						ESSL_CHECK(_essl_ptrdict_insert(&last_use_delims, use_var, use));
					}
					use = next_delimiter(ctx);
				}
			}
		}

		assert(!more_delimiters(ctx));
	}

	return MEM_OK;
}

static void correct_coalesced_reg(loadstore_context *ctx, maligp2_instruction *inst) {
	if (inst && (inst->opcode == MALIGP2_LOAD_WORK_REG || inst->opcode == MALIGP2_STORE_WORK_REG)) {
		int reg = inst->address_offset / MALIGP2_NATIVE_VECTOR_SIZE;
		int comp = inst->address_offset & (MALIGP2_NATIVE_VECTOR_SIZE-1);
		virtual_reg *vreg = _essl_maligp2_virtual_reg_get(ctx->vr_ctx, reg);
		inst->address_offset = vreg->index * MALIGP2_NATIVE_VECTOR_SIZE + comp;
	}
}

static void correct_coalesced_regs(loadstore_context *ctx) {
	unsigned block_i;
	for (block_i = 0 ; block_i < ctx->cfg->n_blocks ; block_i++) {
		basic_block *block = ctx->cfg->output_sequence[block_i];
		maligp2_instruction_word *word;
		for (word = block->earliest_instruction_word ; word != 0 ; word = word->successor) {
			int i;
			for (i = 0 ; i < MALIGP2_NATIVE_VECTOR_SIZE ; i++) {
				correct_coalesced_reg(ctx, word->u.real_slots.load0[i]);
				correct_coalesced_reg(ctx, word->u.real_slots.load1[i]);
			}
			correct_coalesced_reg(ctx, word->u.real_slots.store_xy);
			correct_coalesced_reg(ctx, word->u.real_slots.store_zw);
		}
	}
}

static void check_stores_hitting_load(maligp2_instruction_word *lword, maligp2_instruction *load,
									  basic_block *block, basic_block *prev_block, int *needed) {
	if (load != 0) {
		int lreg = load->address_offset / MALIGP2_NATIVE_VECTOR_SIZE;
		int comp = load->address_offset & 3;
		maligp2_instruction_word *sword;
		maligp2_instruction_word *latest_word = block->latest_instruction_word;
		for (sword = latest_word ;
			 sword != 0 ;
			 sword = sword == latest_word ? sword->predecessor != 0 ? sword->predecessor :
				 prev_block != 0 ? prev_block->latest_instruction_word :
				 0 : 0)
		{
			maligp2_instruction *store = (comp & 2) == 0 ? sword->u.real_slots.store_xy : sword->u.real_slots.store_zw;
			if (store != 0) {
				int sreg = store->address_offset / MALIGP2_NATIVE_VECTOR_SIZE;
				if (sreg == lreg && store->args[comp & 1].arg != 0) {
					/* Clash between store and load */
					int cycles_diff = sword->cycle - lword->cycle;
					if (cycles_diff < 3) {
						int delay = 3-cycles_diff;
						if (delay > *needed) {
							*needed = delay;
						}
					}
				}
			}
		}
	}
}


static basic_block *get_next_block(control_flow_graph *cfg, basic_block *b)
{
	int order = b->output_visit_number + 1;
	if(order >= (int)cfg->n_blocks) return NULL;
	return cfg->output_sequence[order];

}

static essl_bool block_falls_through(basic_block *block) {

	if(block->termination != TERM_KIND_JUMP) return ESSL_FALSE;
	if(block->source != NULL) 
	{
		/* conditional jump. can always fall through to the next block*/
		return ESSL_TRUE;
	} else {
		/* unconditional jump. only falls through if default_target is the next block */
		return block->successors[BLOCK_DEFAULT_TARGET]->output_visit_number == block->output_visit_number + 1;
	}
}

static memerr insert_empty_cycles_for_workreg_delay(loadstore_context *ctx) {
	unsigned block_i;
	basic_block *prev_block = 0;
	control_flow_graph *cfg = ctx->cfg;
	for (block_i = 0 ; block_i < cfg->n_blocks ; block_i++) {
		basic_block *block = cfg->output_sequence[block_i];
		if (block_falls_through(block))
		{
			/* Adjacent blocks - check that no registers are written and read too tightly */
			int needed_cycles = 0;
			maligp2_instruction_word *lword;
			basic_block *next_block = cfg->output_sequence[block_i+1];
			maligp2_instruction_word *earliest_word = next_block->earliest_instruction_word;
			for (lword = earliest_word ;
				 lword != 0 ;
				 lword = lword == earliest_word ? lword->successor != 0 ? lword->successor :
					 block_falls_through(next_block) ? cfg->output_sequence[block_i+2]->earliest_instruction_word :
					 0 : 0)
			{
				int i;
				for (i = 0 ; i < MALIGP2_NATIVE_VECTOR_SIZE ; i++) {
					check_stores_hitting_load(lword, lword->u.real_slots.load0[i], block, prev_block, &needed_cycles);
					check_stores_hitting_load(lword, lword->u.real_slots.load1[i], block, prev_block, &needed_cycles);
				}
			}

			while (needed_cycles > 0) {
				maligp2_instruction_word *latest_word = block->latest_instruction_word;
				ESSL_CHECK(_essl_maligp2_insert_word_after(ctx->pool, ctx->liv_ctx, latest_word, block));
				--needed_cycles;
			}
			prev_block = block;
		} else {
			prev_block = 0;
		}
	}
	return MEM_OK;
}

/* Set the variable reference pointed to by this spill delimiter to its original variable.
   If the delimiter has moved relative to the original delimiter, move the original delimiter
   accordingly.
*/
static essl_bool revert_delimiter_to_original_var(live_delimiter *spill_delim, live_range *orig_range) {
	live_delimiter *delim;
	for (delim = orig_range->points ; delim != 0 ; delim = delim->next)
	{
		if (delim->var_ref == spill_delim->var_ref)
		{
			delim->position = spill_delim->position;
			*delim->var_ref = orig_range->var;
			return ESSL_TRUE;
		}
	}
	return ESSL_FALSE;
}

/* Revert a spill range to its original uses and defs */
static void revert_range_to_original_var(loadstore_context *ctx, live_range *range, live_range *orig_range) {
	control_flow_graph *cfg = ctx->cfg;
	basic_block *curr_block = cfg->entry_block;
	maligp2_instruction_word *curr_word = curr_block->earliest_instruction_word;
	live_delimiter *delim;
	for (delim = range->points ; delim != 0 ; delim = delim->next)
	{
		/* Find instruction word containing this delimiter */
		while (curr_word == 0 || END_OF_CYCLE(curr_word->cycle) > delim->position) {
			if (curr_word == 0) {
				curr_block = get_next_block(cfg, curr_block);
				curr_word = curr_block->earliest_instruction_word;
			} else {
				curr_word = curr_word->successor;
			}
		}

		/* Categorize delimiter */
		switch (POSITION_TO_RELATIVE_POSITION(delim->position)) {
		case 5:
		{
			/* A load */
			maligp2_instruction **load_p = 0;
			int i;
			for (i = 0 ; i < 4 ; i++) {
				if (&curr_word->u.real_slots.load0[i]->instr_node == delim->var_ref) {
					load_p = &curr_word->u.real_slots.load0[i];
					break;
				} else if (&curr_word->u.real_slots.load1[i]->instr_node == delim->var_ref) {
					load_p = &curr_word->u.real_slots.load1[i];
					break;
				} else if (&curr_word->u.real_slots.load_const[i]->instr_node == delim->var_ref) {
					load_p = &curr_word->u.real_slots.load_const[i];
					break;
				}
			}
			assert(load_p != 0);

			if ((*load_p)->opcode == MALIGP2_LOAD_WORK_REG) {
				/* Spill load - remove it */
				*load_p = 0;
				/*printf("o");*/
			} else {
				/* Input or constant register load - revert it */
				DO_ASSERT(revert_delimiter_to_original_var(delim, orig_range));
				/*printf("l");*/
			}
			break;
		}
		case 4:
		{
			/* An ALU use */
			if (!revert_delimiter_to_original_var(delim, orig_range))
			{
				unsigned i;
				for (i = 0 ; i < MALIGP2_MAX_MOVES ; i++) {
					if (&curr_word->reserved_moves[i] == delim->var_ref) {
						/* A move reservation only present in the spill range - remove it */
						assert(curr_word->reserved_moves[i] != 0);
						curr_word->reserved_moves[i] = 0;
						curr_word->n_moves_available++;
						curr_word->n_moves_reserved--;
						assert(curr_word->n_moves_available >= 0);
						assert(curr_word->n_moves_reserved >= 0);
						/*printf("r");*/
						break;
					}
				}
			}
			break;
		}
		case 3:
			/* An ALU definition */
			DO_ASSERT(revert_delimiter_to_original_var(delim, orig_range));
			/*printf("d");*/
			break;
		case 2:
		{
			/* A store */
			maligp2_instruction **store_p = 0;
			maligp2_schedule_classes store_sc = 0;
			int i;
			for (i = 0 ; i < 2 ; i++) {
				if (&curr_word->u.real_slots.store_xy->args[i].arg == delim->var_ref) {
					store_p = &curr_word->u.real_slots.store_xy;
					store_sc = MALIGP2_SC_STORE_XY;
					break;
				} else if (&curr_word->u.real_slots.store_zw->args[i].arg == delim->var_ref) {
					store_p = &curr_word->u.real_slots.store_zw;
					store_sc = MALIGP2_SC_STORE_ZW;
					break;
				}
			}
			assert(store_p != 0);

			if ((*store_p)->opcode == MALIGP2_STORE_WORK_REG) {
				/* Spill store - remove it */
				/*printf("p");*/
				(*store_p)->args[i].arg = 0;
				if ((*store_p)->args[1-i].arg == 0) {
					/* Store instruction is empty - remove the instruction */
					*store_p = 0;
					curr_word->used_slots &= ~store_sc;
				}
			} else {
				/* Output or constant register store - revert it */
				DO_ASSERT(revert_delimiter_to_original_var(delim, orig_range));
				/*printf("s");*/
			}
			break;
		}
		default:
			/* Phi node - should not occur at this stage */
			assert(0);
		}

	}

	_essl_liveness_correct_live_range(orig_range);
}

static essl_bool revert_unallocated_spill_ranges(loadstore_context *ctx) {
	essl_bool unallocated_spill = ESSL_FALSE;
	live_range *unalloc_range;
	/* Mark the original ranges of all unallocated spill ranges as unspillable */
	for (unalloc_range = ctx->unallocated_ranges ; unalloc_range != 0 ; unalloc_range = unalloc_range->next) {
		if (unalloc_range->spill_range) {
			/* Unallocated spill range! */
			node *orig_var = GET_CHILD(unalloc_range->var, 0);
			live_range *orig_range = _essl_ptrdict_lookup(&ctx->liv_ctx->var_to_range, orig_var);
			assert(orig_range->spilled);
			orig_range->unspillable = 1;
			unallocated_spill = ESSL_TRUE;
		}
	}

	if (!unallocated_spill) return ESSL_FALSE;

	/* Remove all spill ranges whose original var is marked as unspillable
	   - note that the unallocated_ranges pointer is not valid after this */
	{
		live_range **rangep;
		for (rangep = &ctx->liv_ctx->var_ranges ; *rangep != 0 ;)
		{
			essl_bool remove = ESSL_FALSE;
			if ((*rangep)->spill_range) {
				node *orig_var = GET_CHILD((*rangep)->var, 0);
				live_range *orig_range = _essl_ptrdict_lookup(&ctx->liv_ctx->var_to_range, orig_var);
				assert((*rangep)->var->hdr.kind == EXPR_KIND_UNARY && (*rangep)->var->expr.operation == EXPR_OP_SPILL);
				if (orig_range->unspillable)
				{
					revert_range_to_original_var(ctx, (*rangep), orig_range);
					_essl_ptrdict_remove(&ctx->liv_ctx->var_to_range, (*rangep)->var);
					remove = ESSL_TRUE;
				}
			}
			/*printf("Reverted spill: %p\n", orig_var);*/
			if (remove) {
				*rangep = (*rangep)->next;
			} else {
				rangep = &(*rangep)->next;
			}

		}
	}

	/* Revive spilled ranges that were reverted */
	{
		live_range *range;
		for (range = ctx->liv_ctx->var_ranges ; range != 0 ; range = range->next)
		{
			if (range->unspillable)
			{
				range->spilled = 0;
				range->unspillable = 0;
			}
		}
	}

	return ESSL_TRUE;
}

static void correct_spill_ranges(loadstore_context *ctx) {
	live_range *range;

	for (range = ctx->liv_ctx->var_ranges ; range != 0 ; range = range->next)
	{
		if (range->spill_range)
		{
			_essl_liveness_correct_live_range(range);
		}
	}
}

static essl_bool can_fall_through(maligp2_instruction_word *word)
{
	return word->u.real_slots.branch == NULL ||
		!(word->u.real_slots.branch->opcode == MALIGP2_JMP ||
		  word->u.real_slots.branch->opcode == MALIGP2_CALL ||
		  word->u.real_slots.branch->opcode == MALIGP2_RET);
}

static memerr detect_conflict(loadstore_context *ctx,
							  interference_graph_context *cg, maligp2_instruction_word_conflict_list ***word_conflicts_p,
							  basic_block *block, maligp2_instruction_word *sword, maligp2_instruction_word *lword,
							  maligp2_instruction *store, maligp2_instruction *load)
{
	if (store && store->opcode == MALIGP2_STORE_WORK_REG &&
		load && load->opcode == MALIGP2_LOAD_WORK_REG)
	{
		int sreg = store->address_offset / MALIGP2_NATIVE_VECTOR_SIZE;
		int lreg = load->address_offset / MALIGP2_NATIVE_VECTOR_SIZE;
		assert(load->instr_node != NULL);
		if (lreg == sreg)
		{
			/* Unavoidable conflict in virtual allocation - will need instruction fixup */
			maligp2_instruction_word_conflict_list *conflict;
			ESSL_CHECK(conflict = LIST_NEW(ctx->pool, maligp2_instruction_word_conflict_list));
			conflict->next = NULL;
			conflict->sword = sword;
			conflict->lword = lword;
			conflict->lblock = block;
			**word_conflicts_p = conflict;
			*word_conflicts_p = &conflict->next;
		} else {
			/* Potential conflict - mark a conflict between the variables */
			node *svar, *lvar;
			svar = _essl_maligp2_virtual_reg_get(ctx->vr_ctx, sreg)->virtual_var;
			lvar = _essl_maligp2_virtual_reg_get(ctx->vr_ctx, lreg)->virtual_var;
			ESSL_CHECK(_essl_interference_graph_register_edge(cg, svar, lvar));
		}

	}

	return MEM_OK;
}

static interference_graph_context *create_conflict_graph(loadstore_context *ctx, maligp2_instruction_word_conflict_list **word_conflicts)
{
	interference_graph_context *cg;
	basic_block *block;
	unsigned i;
	maligp2_instruction_word *pword = NULL, *ppword = NULL;
	ESSL_CHECK(cg = _essl_mempool_alloc(ctx->pool, sizeof(*cg)));
	ESSL_CHECK(_essl_interference_graph_init(cg, ctx->pool));
	*word_conflicts = NULL;
	
	for (i = 0; i < ctx->cfg->n_blocks; ++i)
	{
		maligp2_instruction_word *word;
		block = ctx->cfg->output_sequence[i];
		for (word = block->earliest_instruction_word ; word != 0 ; word = word->successor)
		{
			if (ppword != NULL)
			{
				ESSL_CHECK(detect_conflict(ctx, cg, &word_conflicts, block, ppword, word, ppword->u.real_slots.store_xy, word->u.real_slots.load0[0]));
				ESSL_CHECK(detect_conflict(ctx, cg, &word_conflicts, block, ppword, word, ppword->u.real_slots.store_xy, word->u.real_slots.load0[1]));
				ESSL_CHECK(detect_conflict(ctx, cg, &word_conflicts, block, ppword, word, ppword->u.real_slots.store_zw, word->u.real_slots.load0[2]));
				ESSL_CHECK(detect_conflict(ctx, cg, &word_conflicts, block, ppword, word, ppword->u.real_slots.store_zw, word->u.real_slots.load0[3]));
				ESSL_CHECK(detect_conflict(ctx, cg, &word_conflicts, block, ppword, word, ppword->u.real_slots.store_xy, word->u.real_slots.load1[0]));
				ESSL_CHECK(detect_conflict(ctx, cg, &word_conflicts, block, ppword, word, ppword->u.real_slots.store_xy, word->u.real_slots.load1[1]));
				ESSL_CHECK(detect_conflict(ctx, cg, &word_conflicts, block, ppword, word, ppword->u.real_slots.store_zw, word->u.real_slots.load1[2]));
				ESSL_CHECK(detect_conflict(ctx, cg, &word_conflicts, block, ppword, word, ppword->u.real_slots.store_zw, word->u.real_slots.load1[3]));
			}
			if (can_fall_through(word))
			{
				ppword = pword;
				pword = word;
			} else {
				ppword = NULL;
				pword = NULL;
			}
		}
	}

	return cg;
}

memerr _essl_maligp2_produce_conflict_graph(loadstore_context *ctx, liveness_context *liv_ctx)
{
	if (ctx->opts->maligp2_work_reg_readwrite_workaround)
	{
		interference_graph_context *conflict_graph;
		maligp2_instruction_word_conflict_list *cl;
		ESSL_CHECK(conflict_graph = create_conflict_graph(ctx, &cl));
		while (cl != NULL)
		{
			/* Need to insert empty instructions to break conflicts */
			maligp2_instruction_word_conflict_list *conflict;
			for (conflict = cl ; conflict != NULL ; conflict = conflict->next)
			{
				if (_essl_maligp2_inseparable_from_predecessor(conflict->lword)) {
					ESSL_CHECK(_essl_maligp2_insert_word_before(ctx->pool, liv_ctx, conflict->lword->predecessor, conflict->lblock));
				} else {
					ESSL_CHECK(_essl_maligp2_insert_word_before(ctx->pool, liv_ctx, conflict->lword, conflict->lblock));
				}
			}

			/* Recalculate conflicts */
			ESSL_CHECK(conflict_graph = create_conflict_graph(ctx, &cl));
		}

		_essl_maligp2_virtual_reg_set_conflict_graph(ctx->vr_ctx, conflict_graph);
	}
	
	return MEM_OK;
}


loadstore_context *_essl_maligp2_create_loadstore_context(mempool *pool,
														  control_flow_graph *cfg,
														  virtual_reg_context *vr_ctx,
														  compiler_options *opts) {
	loadstore_context *ctx;
	ESSL_CHECK(ctx = _essl_mempool_alloc(pool, sizeof(loadstore_context)));
	ctx->pool = pool;
	ctx->cfg = cfg;
	ctx->vr_ctx = vr_ctx;
	ctx->opts = opts;

	ctx->stores = 0;

	return ctx;
}

memerr _essl_maligp2_allocate_register_loadstores(mempool *temp_pool,
												  loadstore_context *ctx,
												  liveness_context *liv_ctx,
												  live_range *unallocated_ranges) {
	int n_vr;
	ctx->temp_pool = temp_pool;
	ctx->liv_ctx = liv_ctx;
	ctx->unallocated_ranges = unallocated_ranges;
	ctx->stores = 0;
	ESSL_CHECK(_essl_priqueue_init(&ctx->var_defs, temp_pool));

#ifndef NDEBUG
	assert_all_ranges_ok(liv_ctx);
#endif

/*
	{
		live_range *range;
		for (range = ctx->unallocated_ranges ; range != 0 ; range = range->next) {
			if (range->spill_range) {
				live_delimiter *delim;
				printf("Reallocated spill range: ");
				for (delim = range->points ; delim != 0 ; delim = delim->next) {
					char p;
					switch (delim->position % 10) {
					case 2: p = 's'; break;
					case 3: p = 'd'; break;
					case 4: p = delim->locked ? 'm' : 'u'; break;
					case 5: p = 'l'; break;
					default: p = '?'; break;
					}
					printf("%d %c", delim->position / 10, p);
					if (delim->next != 0) {
						printf(" -> ");
					}
				}
				printf("\n");
			}
		}
	}
*/

	_essl_maligp2_virtual_reg_set_conflict_graph(ctx->vr_ctx, NULL);

	if (revert_unallocated_spill_ranges(ctx)) {
		/* Some spill ranges were reverted - take another round */
		return MEM_OK;
	}

	ESSL_CHECK(allocate_stores(ctx));
	
	n_vr = ctx->vr_ctx->n_virtual_regs;
	ESSL_CHECK(allocate_loads(ctx));
	if (ctx->vr_ctx->n_virtual_regs != n_vr)
	{
		assert(0);
	}

	correct_spill_ranges(ctx);
	correct_coalesced_regs(ctx);
	ESSL_CHECK(insert_empty_cycles_for_workreg_delay(ctx));

	ESSL_CHECK(_essl_maligp2_produce_conflict_graph(ctx, liv_ctx));

#ifndef NDEBUG
	assert_all_ranges_ok(liv_ctx);

	{
		/* Validate liveness */
		liveness_context *reference_liv_ctx;
		ESSL_CHECK(reference_liv_ctx = _essl_maligp2_calculate_live_ranges(ctx->pool, ctx->cfg, ctx->liv_ctx->desc, ctx->liv_ctx->error_context));
		_essl_liveness_assert_identical(ctx->liv_ctx, reference_liv_ctx);
	}
#endif

	return MEM_OK;
}

