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
#include "mali200/mali200_word_insertion.h"
#include "mali200/mali200_slot.h"
#include "mali200/mali200_spilling.h"
#include "backend/extra_info.h"
#include "common/symbol_table.h"

static const int mask_n_comps[16] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};

m200_instruction_word *_essl_mali200_find_word_for_spill(regalloc_context *ctx, live_range *spill_range, basic_block **block_out) {
	int cycle = POSITION_TO_CYCLE(spill_range->start_position);
	return _essl_instruction_word_at_cycle(ctx->pool, ctx->cfg, cycle, block_out);
}

static const char spillname[] = "spill";
static symbol *make_var_symbol(mempool *pool, node *var, unsigned int size) {
	string s = { spillname, 5 };
	type_specifier *type;
	if (GET_NODE_VEC_SIZE(var) == size) {
		type = (type_specifier *)var->hdr.type;
	} else {
		ESSL_CHECK(type = _essl_clone_type(pool, var->hdr.type));
		type->u.basic.vec_size = size;
	}
	return _essl_new_variable_symbol_with_default_qualifiers(pool, s, type, SCOPE_LOCAL, ADDRESS_SPACE_THREAD_LOCAL, UNKNOWN_SOURCE_OFFSET);
}

static live_delimiter *clone_delimiter(mempool *pool, live_delimiter *delim) {
	live_delimiter *new_delim;
	ESSL_CHECK(new_delim = _essl_liveness_new_delimiter(pool, delim->var_ref, delim->kind, delim->position));
	new_delim->mask = delim->mask;
	new_delim->live_mask = delim->live_mask;
	new_delim->locked = delim->locked;
	new_delim->use_accepts_lut = delim->use_accepts_lut;
	return new_delim;
}

static memerr complete_spill_range(regalloc_context *ctx, live_range *range, live_delimiter *first_delim, live_delimiter **end_p, int cycle, int mask, essl_bool def_in_curr_spill) {
	live_range *spill_range;
	node *spill_var;
	node_extra *spill_info;
	if (def_in_curr_spill) {
		ESSL_CHECK(*end_p = _essl_liveness_new_delimiter(ctx->pool, 0, LIVE_STOP, END_OF_CYCLE(cycle)));
		(*end_p)->mask = mask;
		(*end_p)->live_mask = mask;
		if (ctx->liv_ctx->desc->options->mali200_store_workaround) {
			/* Workaround for bugzilla 3592 */
			(*end_p)->locked = 1;
		} else {
			(*end_p)->locked = GET_NODE_VEC_SIZE(range->var) > 1; /* Treat as locked if more than one component */
		}
	}

	/* Insert new range into range list */
	ESSL_CHECK(spill_var = _essl_new_unary_expression(ctx->pool, EXPR_OP_SPILL, range->var));
	_essl_ensure_compatible_node(spill_var, range->var);
	ESSL_CHECK(spill_info = CREATE_EXTRA_INFO(ctx->pool, spill_var));
	ESSL_CHECK(spill_range = _essl_liveness_new_live_range(ctx->pool, spill_var, first_delim));
	_essl_liveness_correct_live_range(spill_range);
	spill_range->spill_range = 1;
	spill_range->next = ctx->liv_ctx->var_ranges;
	ctx->liv_ctx->var_ranges = spill_range;
	spill_info->spill.sv.word = _essl_mali200_find_word_for_spill(ctx, spill_range, &spill_info->spill.sv.block);
	ESSL_CHECK(_essl_ptrdict_insert(&ctx->liv_ctx->var_to_range, spill_var, spill_range));
	return MEM_OK;
}

static memerr create_spill_ranges_for_range(regalloc_context *ctx, live_range *range) {
	/* Spill range */
	live_delimiter *curr_spill_start = 0;
	live_delimiter **curr_spill_end_p = 0;
	int curr_spill_live_mask = 0;
	int curr_spill_cycle = -1;
	essl_bool def_in_curr_spill = ESSL_FALSE;
	live_delimiter *delim;
	for (delim = range->points ; delim != 0 ; delim = delim->next) {
		int cycle = POSITION_TO_CYCLE(delim->position);
		if (cycle != curr_spill_cycle) {
			/* Complete last spill range */
			if (curr_spill_cycle != -1) {
				ESSL_CHECK(complete_spill_range(ctx, range, curr_spill_start, curr_spill_end_p, curr_spill_cycle, curr_spill_live_mask, def_in_curr_spill));
			}

			switch (delim->kind) {
			case LIVE_DEF:
			case LIVE_USE:
				/* Start new spill range */
				if (delim->live_mask != 0) {
					/* Use or partial definition - load needed */
					ESSL_CHECK(curr_spill_start = _essl_liveness_new_delimiter(ctx->pool, 0, LIVE_RESTART, START_OF_CYCLE(cycle)));
					curr_spill_start->mask = delim->live_mask;
					curr_spill_end_p = &curr_spill_start->next;
				} else {
					/* Total definition - just get ready for the DEF delimiter */
					curr_spill_end_p = &curr_spill_start;
				}
				curr_spill_cycle = cycle;
				def_in_curr_spill = ESSL_FALSE;
				break;
			default:
				curr_spill_cycle = -1;
				break;
			}
		}
		
		/* Continue current spill range */
		switch (delim->kind) {
		case LIVE_DEF:
			def_in_curr_spill = ESSL_TRUE;
			/* fall-through */
		case LIVE_USE:
			assert(curr_spill_end_p != NULL);
			ESSL_CHECK(*curr_spill_end_p = clone_delimiter(ctx->pool, delim));
			curr_spill_end_p = &(*curr_spill_end_p)->next;
			curr_spill_live_mask = delim->next != 0 ? delim->next->live_mask : 0;
			break;
		default:
			break;
		}
	}

	/* Finally, complete last spill range */
	if (curr_spill_cycle != -1) {
		ESSL_CHECK(complete_spill_range(ctx, range, curr_spill_start, curr_spill_end_p, curr_spill_cycle, curr_spill_live_mask, def_in_curr_spill));
	}

	return MEM_OK;
}

static essl_bool range_suitable_for_hash_load(live_range *range, m200_instruction_word *spill_word)
{
	live_delimiter *delim;
	if (!range->spill_range || range->points->kind != LIVE_RESTART)
	{
		return ESSL_FALSE;
	}

	for (delim = range->points->next ; delim != 0 ; delim = delim->next)
	{
		int subpos;
		if (delim->kind == LIVE_DEF) return ESSL_FALSE; /* #load cannot be written to */
		subpos = POSITION_TO_RELATIVE_POSITION(delim->position);
		if (subpos > 6) return ESSL_FALSE; /* #load is not available in first subcycle */
		if (spill_word->branch)
		{
			if (delim->var_ref == &spill_word->branch->args[0].arg ||
				delim->var_ref == &spill_word->branch->args[1].arg ||
				delim->var_ref == &spill_word->branch->args[2].arg)
			{
				/* Input to branch instruction - cannot be in pseudo register */
				return ESSL_FALSE;
			}
		}
	}

	return ESSL_TRUE;
}

static essl_bool better_for_hash_load_than(live_range *r1, live_range *r2) {
	live_delimiter *d1 = r1->points;
	live_delimiter *d2 = r2->points;
	int c1 = mask_n_comps[r1->mask], c2 = mask_n_comps[r2->mask];
	if (c1 != c2) return c1 > c2;
	while (d1->next != 0) d1 = d1->next;
	while (d2->next != 0) d2 = d2->next;
	return d1->position < d2->position;
}

static memerr allocate_hash_load_ranges(regalloc_context *ctx) {
	live_range *range;
	for (range = ctx->liv_ctx->var_ranges ; range != 0 ; range = range->next) {
		node_extra *info = EXTRA_INFO(range->var);
		m200_instruction_word *word = info->spill.sv.word;
		if (range_suitable_for_hash_load(range, word) && !range->allocated) {
			/* Spill range for load */
			if (_essl_ptrdict_has_key(&ctx->hash_load_range, word)) {
				live_range *old_range = _essl_ptrdict_lookup(&ctx->hash_load_range, word);
				if (!old_range->allocated && better_for_hash_load_than(range, old_range)) {
					/* Replace range for this word */
					ESSL_CHECK(_essl_ptrdict_insert(&ctx->hash_load_range, word, range));
				}
			} else {
				if ((word->used_slots & M200_SC_LOAD) == 0) {
					/* Load slot available */
					assert(word->load == 0);
					ESSL_CHECK(_essl_ptrdict_insert(&ctx->hash_load_range, word, range));
				}
			}
		}
	}
	
	/* Allocate all ranges found into #load */
	{
		ptrdict_iter it;
		_essl_ptrdict_iter_init(&it, &ctx->hash_load_range);
		while ((_essl_ptrdict_next(&it, (void **)(void *)&range)) != 0) {
			if (!range->allocated) {
				swizzle_pattern swz = _essl_create_identity_swizzle(GET_NODE_VEC_SIZE(range->var));
				ESSL_CHECK(_essl_mali200_allocate_reg(ctx, range, M200_HASH_LOAD, &swz));
			}
		}
	}		  

	return MEM_OK;
}

memerr _essl_mali200_create_spill_ranges(regalloc_context *ctx) {
	live_range *range;
	ptrset_iter it;
	_essl_ptrset_iter_init(&it, &ctx->unallocated_ranges);
	while ((range = _essl_ptrset_next(&it)) != 0) {
		node_extra *info = EXTRA_INFO(range->var);
		ESSL_CHECK(info->spill.spill_symbol = make_var_symbol(ctx->pool, range->var, GET_NODE_VEC_SIZE(range->var)));
		ESSL_CHECK(create_spill_ranges_for_range(ctx, range));
		range->spilled = 1;
	}

	ESSL_CHECK(allocate_hash_load_ranges(ctx));

	return MEM_OK;
}


static essl_bool store_available(m200_instruction_word *store_word) {
	int store_sc = M200_SC_STORE | M200_SC_LSB_VECTOR_INPUT | M200_SC_RENDEZVOUS;
	return ((store_word->used_slots & store_sc) == 0);
}


static int def_cost(m200_instruction_word *word, int def_pos) {
	if (def_pos < 3 || !store_available(word)) {
		return 30;
	}
	return 10;
}

int _essl_mali200_spill_cost(graph_coloring_context *gc_ctx, live_range *range)
{
	if (range->unspillable) {
		return 1000000;
	} else {
		int cost = 0;
		int last_cycle = 1000000;
		essl_bool def_in_word = ESSL_FALSE;
		int def_pos = 0;
		m200_instruction_word *word = 0, *last_word = 0;
		basic_block *block;
		int block_cost = 0;
		live_delimiter *delim;
		for (delim = range->points ; delim != 0 ; delim = delim->next)
		{
			int cycle = POSITION_TO_CYCLE(delim->position);
			int vicinity = 0;
			if (cycle != last_cycle) {
				if (def_in_word) {
					assert(word != NULL);
					cost += def_cost(word, def_pos) * block_cost;
					def_in_word = ESSL_FALSE;
				}

				last_word = word;
				word = _essl_instruction_word_at_cycle(gc_ctx->pool, gc_ctx->liv_ctx->cfg, cycle, &block);
				block_cost = (int)(block->cost * 4.0f) + 1;
				if (cycle == last_cycle-1 && last_word->successor == word) {
					vicinity = 2;
				} else {
					vicinity = 2;
				}
			}
			switch (delim->kind) {
			case LIVE_USE:
				/* Load slot available? */
				assert(word != NULL);
				if ((word->used_slots & M200_SC_LOAD) == 0) {
					cost += 2 * vicinity * block_cost;
				} else {
					cost += 5 * vicinity * block_cost;
				}
				break;
			case LIVE_DEF:
				def_in_word = ESSL_TRUE;
				def_pos = POSITION_TO_RELATIVE_POSITION(delim->position);
				break;
			case LIVE_STOP:
			case LIVE_RESTART:
				break;
			case LIVE_UNKNOWN:
				assert(0);
				break;
			}
			last_cycle = cycle;
		}

		if (def_in_word) {
			assert(word != NULL);
			cost += def_cost(word, def_pos) * block_cost;
		}

		/* Discourage spilling very short ranges */
		if (POSITION_TO_CYCLE(range->start_position) - last_cycle <= 1) {
			cost *= 1000;
		}

		return cost;
	}
}

static m200_instruction *put_store(regalloc_context *ctx, m200_instruction_word *store_word, symbol *sym, node *var, unsigned int size) {
	int store_sc = M200_SC_STORE | M200_SC_LSB_VECTOR_INPUT | M200_SC_NO_RENDEZVOUS;
	m200_instruction *store;
	ESSL_CHECK(store = _essl_new_mali200_instruction(ctx->pool, M200_SC_STORE, M200_ST_STACK, CYCLE_TO_SUBCYCLE(store_word->cycle, 0)));
	store->args[1].arg = var;
	store->args[1].swizzle = _essl_create_identity_swizzle(size);
	store->address_multiplier = size == 3 ? 4 : size;
	ESSL_CHECK(_essl_mali200_add_address_offset_relocation(ctx->rel_ctx, M200_RELOC_VARIABLE, sym, ctx->function, 1, store->address_multiplier, store));
	assert(store_word->store == 0);
	store_word->store = store;
	store_word->used_slots |= store_sc;

	return store;
}

static m200_instruction *put_load(regalloc_context *ctx, m200_instruction_word *load_word, m200_schedule_classes sc, symbol *sym, node *var, unsigned int size, int live_mask) {
	m200_instruction *load;
	sc = _essl_mali200_allocate_slots(load_word, sc, load_word->used_slots, 0, var, 0, size) & M200_SC_CONCRETE_MASK;
	load_word->used_slots |= sc;
	assert(sc != 0);
	if (sc != M200_SC_LOAD) {
		/* Put move instruction */
		m200_instruction *mov;
		ESSL_CHECK(mov = _essl_mali200_create_slot_instruction(ctx->pool, load_word, &sc, M200_MOV));
		mov->instr_node = var;
		mov->output_swizzle = _essl_create_identity_swizzle_from_mask(live_mask);
		mov->output_mode = M200_OUTPUT_NORMAL;

		mov->args[0].reg_index = M200_HASH_LOAD;
		mov->args[0].swizzle = _essl_create_identity_swizzle(size);
	}
	assert(load_word->load == 0);
	ESSL_CHECK(load = _essl_mali200_create_slot_instruction(ctx->pool, load_word, &sc, M200_LD_STACK));
	load->out_reg = M200_HASH_LOAD;
	load->output_swizzle = _essl_create_identity_swizzle_from_mask(live_mask);
	load->output_mode = M200_OUTPUT_NORMAL;
	load->address_multiplier = size == 3 ? 4 : size;
	assert(GET_VEC_SIZE(sym->type) >= size);
	ESSL_CHECK(_essl_mali200_add_address_offset_relocation(ctx->rel_ctx, M200_RELOC_VARIABLE, sym, ctx->function, 1, load->address_multiplier, load));
	return load;
}

memerr _essl_mali200_insert_spills(regalloc_context *ctx) {
	live_range *range;
	for (range = ctx->liv_ctx->var_ranges ; range != 0 ; range = range->next) {
		if (range->spill_range) {
			live_delimiter *delim;
			node *spill_var = range->var;
			node *spilled_var = GET_CHILD(range->var, 0);
			int size = GET_NODE_VEC_SIZE(spill_var);
			node_extra *spill_info = EXTRA_INFO(spill_var);
			node_extra *spilled_info = EXTRA_INFO(spilled_var);
			m200_instruction_word *spill_word = spill_info->spill.sv.word;
			basic_block *block = spill_info->spill.sv.block;
			int word_store_input_pos = START_OF_SUBCYCLE(CYCLE_TO_SUBCYCLE(spill_word->cycle, 0));
			int latest_def_pos = range->start_position;
			assert(spill_info->reg_allocated);

			/* Go through spill range delimiters and insert load and/or store */
			for (delim = range->points ; delim != 0 ; delim = delim->next) {
				switch (delim->kind) {
				case LIVE_RESTART:
					/* Indicates a load */
					if (spill_info->out_reg == M200_HASH_LOAD) {
						/* Load in same word */
						ESSL_CHECK(put_load(ctx, spill_word, M200_SC_LOAD, spilled_info->spill.spill_symbol, spill_var, size, delim->next->live_mask));
					} else {
						/* Insert word for load */
						m200_instruction_word *load_word;
						ESSL_CHECK(load_word = _essl_mali200_insert_word_before(ctx->liv_ctx, spill_word, block));
						ESSL_CHECK(put_load(ctx, load_word, M200_SC_LOAD | M200_SC_MOV, spilled_info->spill.spill_symbol, spill_var, size, delim->next->live_mask));
					}
					break;
				case LIVE_STOP:
					/* Indicates a store */
					if (latest_def_pos > word_store_input_pos && store_available(spill_word)) {
						ESSL_CHECK(put_store(ctx, spill_word, spilled_info->spill.spill_symbol, spill_var, size));
					} else {
						m200_instruction_word *store_word;
						ESSL_CHECK(store_word = _essl_mali200_insert_word_after(ctx->liv_ctx, spill_word, block));
						ESSL_CHECK(put_store(ctx, store_word, spilled_info->spill.spill_symbol, spill_var, size));
					}
					break;
				case LIVE_DEF:
					latest_def_pos = delim->position;
					/* fall-through */
				case LIVE_USE:
					/* Rewrite defs and uses to the spill var */
					*delim->var_ref = spill_var;
					break;
				case LIVE_UNKNOWN:
					assert(0);
					break;
				}
			}
		}
	}
	return MEM_OK;
}
