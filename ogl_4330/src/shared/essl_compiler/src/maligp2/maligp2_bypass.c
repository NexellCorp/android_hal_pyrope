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
#include "maligp2/maligp2_liveness.h"
#include "maligp2/maligp2_instruction.h"
#include "maligp2/maligp2_liveness.h"
#include "maligp2/maligp2_slot.h"
#include "maligp2/maligp2_regalloc.h"

#define MALIGP2_MAX_SUCCESSORS 2

typedef struct
{
	symbol *fun;
	control_flow_graph *cfg;
	mempool *move_pool;
	mempool *temp_pool;
	mempool *permanent_pool;
	target_descriptor *desc;
} bypass_context;

typedef struct _tag_bypass_move
{
	maligp2_instruction_word *def_word;
	maligp2_schedule_classes def_schedule_class;
	int def_position;
	unsigned n_successors;
	struct _tag_bypass_move *successors[MALIGP2_MAX_SUCCESSORS];
} bypass_move;



static int lifetime_for_def(bypass_move *def, int *lifetime_end)
{
	/* returns a bitfield with the cycles this def is live within. 1<<0 -> cycle 0, 1<<1 -> cycle 1, 1<<2 -> cycle 2 */
	switch(def->def_schedule_class)
	{
	case MALIGP2_SC_LOAD1:
	case MALIGP2_SC_LOAD_CONST:
		*lifetime_end = def->def_position + 1 - START_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(0, 1));
		return 1<<0;

	case MALIGP2_SC_LUT:
		*lifetime_end = def->def_position + 1 - START_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(1, 0));
		return 1<<1;

	case MALIGP2_SC_LOAD0:
		*lifetime_end = def->def_position + 1 - START_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(1, 1));
		return (1<<0) | (1<<1);

	case MALIGP2_SC_ADD0:
	case MALIGP2_SC_ADD1:
	case MALIGP2_SC_MUL1:
	case MALIGP2_SC_MISC:
		*lifetime_end = def->def_position + 1 - START_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(2, 0));
		return (1<<1) | (1<<2);


	case MALIGP2_SC_MUL0:
		*lifetime_end = def->def_position + 1 - START_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(2, 0));
		if(def->def_word->mul_opcodes[0] == MALIGP2_MUL_ADD)
		{
			return (1<<2);
		}
		return (1<<1) | (1<<2);


	case MALIGP2_SC_BRANCH:
	case MALIGP2_SC_STORE_XY:
	case MALIGP2_SC_STORE_ZW:
	case MALIGP2_SC_A0_X:
	case MALIGP2_SC_A0_Y:
	case MALIGP2_SC_A0_Z:
	case MALIGP2_SC_A0_W:
		assert(0);
		return 0;
	}
	return 0;
}

/*@null@*/ static maligp2_instruction *get_instruction_defining_node(node *n, maligp2_instruction_word *word)
{
#define DO(unit) if(word->u.real_slots.unit != 0 && word->u.real_slots.unit->instr_node == n) return word->u.real_slots.unit
	size_t i;
	DO(add0);
	DO(add1);
	DO(mul0);
	DO(mul1);
	DO(misc);
	DO(lut);
	for(i = 0; i < MALIGP2_NATIVE_VECTOR_SIZE; ++i)
	{
		DO(load0[i]);
		DO(load1[i]);
		DO(load_const[i]);
	}
	return 0;
#undef DO
}


/*@null@*/ static maligp2_instruction **get_instruction_for_mask(maligp2_schedule_classes mask, maligp2_instruction_word *word)
{
	maligp2_instruction **ret = 0;
#define DO(unit, sc) if(mask == sc) ret = &word->u.real_slots.unit
	DO(add0, MALIGP2_SC_ADD0);
	DO(add1, MALIGP2_SC_ADD1);
	DO(mul0, MALIGP2_SC_MUL0);
	DO(mul1, MALIGP2_SC_MUL1);
	DO(misc, MALIGP2_SC_MISC);
	DO(lut, MALIGP2_SC_LUT);
	assert(ret != NULL && *ret == NULL);
	return ret;
#undef DO
}

static maligp2_instruction_word *instruction_word_for_position(bypass_context *ctx, int position, /*@out@*/ basic_block **res_block)
{
	return _essl_instruction_word_at_cycle(ctx->permanent_pool, ctx->cfg, POSITION_TO_CYCLE(position), res_block);
}


static maligp2_schedule_classes get_free_move_slot(bypass_context *ctx, maligp2_instruction_word *word, node *n)
{
	return _essl_maligp2_allocate_move(ctx->cfg, ctx->desc, word, MALIGP2_RESERVATION_FULFILL, MALIGP2_MOV, n, n);
}


static int get_free_lut_move_slot(bypass_context *ctx, maligp2_instruction_word *word, node *n)
{
	return _essl_maligp2_allocate_move(ctx->cfg, ctx->desc, word, MALIGP2_RESERVATION_FULFILL, MALIGP2_MOV_LUT, n, n);
}


static int get_free_misc_move_slot(bypass_context *ctx, maligp2_instruction_word *word, node *n)
{
	return _essl_maligp2_allocate_move(ctx->cfg, ctx->desc, word, MALIGP2_RESERVATION_FULFILL, MALIGP2_MOV_MISC, n, n);
}



static bypass_move *create_normal_move(bypass_context *ctx, maligp2_instruction_word *word, node *n)
{
	bypass_move *mv;
	maligp2_schedule_classes sc;
	if((sc = get_free_move_slot(ctx, word, n)) == 0) return 0;
	ESSL_CHECK(mv = _essl_mempool_alloc(ctx->temp_pool, sizeof(*mv)));
	mv->def_word = word;
	mv->def_schedule_class = sc;
	mv->def_position = END_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 1));
	mv->n_successors = 0;
	return mv;
}

static bypass_move *create_lut_move(bypass_context *ctx, maligp2_instruction_word *word, node *n)
{
	bypass_move *mv;
	maligp2_schedule_classes sc;
	if((sc = get_free_lut_move_slot(ctx, word, n)) == 0) return 0;
	ESSL_CHECK(mv = _essl_mempool_alloc(ctx->temp_pool, sizeof(*mv)));
	mv->def_word = word;
	mv->def_schedule_class = sc;
	mv->def_position = END_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 1));
	mv->n_successors = 0;
	return mv;
}

static bypass_move *create_misc_move(bypass_context *ctx, maligp2_instruction_word *word, node *n)
{
	bypass_move *mv;
	maligp2_schedule_classes sc;
	if((sc = get_free_misc_move_slot(ctx, word, n)) == 0) return 0;
	ESSL_CHECK(mv = _essl_mempool_alloc(ctx->temp_pool, sizeof(*mv)));
	mv->def_word = word;
	mv->def_schedule_class = sc;
	mv->def_position = END_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 1));
	mv->n_successors = 0;
	return mv;
}

static bypass_move *create_move(bypass_context *ctx, maligp2_instruction_word *word, node *n, live_range *lr)
{
	if(_essl_maligp2_is_fixedpoint_range(lr))
	{
		return create_misc_move(ctx, word, n);
	} else {
		return create_normal_move(ctx, word, n);
	}

}

static void fulfill_move_reservations(maligp2_instruction_word *word, node *n)
{
	int i;
	assert(n != 0);

	for(i = 0; i < MALIGP2_MAX_MOVES ; ++i)
	{
		if(word->reserved_moves[i] == n && !word->move_reservation_fulfilled[i])
		{
			word->move_reservation_fulfilled[i] = ESSL_TRUE;
			++word->n_moves_available;
		}
	}

}


static void fulfill_move_reservations_nearby(maligp2_instruction_word *word, node *n)
{
	unsigned i;
	maligp2_instruction_word *up = word;
	maligp2_instruction_word *down = word->successor;
	for(i = 0; i < 2; ++i)
	{
		if(up != 0)
		{
			fulfill_move_reservations(up, n);
			up = up->predecessor;
		}
		if(down != 0)
		{
			fulfill_move_reservations(down, n);
			down = down->successor;
		}
	}
}

static memerr fill_moves(bypass_context *ctx, node *n, bypass_move *move)
{
	unsigned i;
	maligp2_instruction **instp;
	maligp2_instruction *inst;


	ESSL_CHECK(instp = get_instruction_for_mask(move->def_schedule_class, move->def_word));
	ESSL_CHECK(inst = _essl_new_maligp2_instruction(ctx->move_pool, move->def_schedule_class, MALIGP2_MOV, MALIGP2_CYCLE_TO_SUBCYCLE(move->def_word->cycle, 1)));
	inst->instr_node = n;
	inst->args[0].arg = n;

	*instp = inst;
	for(i = 0; i < move->n_successors; ++i)
	{
		ESSL_CHECK(fill_moves(ctx, n, move->successors[i]));
	}
	fulfill_move_reservations_nearby(move->def_word, n);
	return MEM_OK;
}


static essl_bool insert_bypass_moves_rec(bypass_context *ctx, bypass_move *prev_move, live_range *lr, live_delimiter *delim, maligp2_instruction_word *start_word, int offset, basic_block *curr_block, essl_bool first_move_in_successor_block);

static essl_bool place_mov_at_cycle(bypass_context *ctx, bypass_move *prev_move, live_range *lr, live_delimiter *delim, maligp2_instruction_word *word, essl_bool lut_is_acceptable, basic_block *curr_block)
{
	live_delimiter *delim_ptr = delim;
	essl_bool lut_okay = lut_is_acceptable;
	node *n = lr->var;
	bypass_move *curr_move;

	while(delim_ptr != 0 && delim_ptr->position >= END_OF_CYCLE(word->cycle)) delim_ptr = delim_ptr->next;

	while(delim_ptr != 0 && delim_ptr->position >= END_OF_CYCLE(word->cycle-1))
	{
		if(delim_ptr->kind == LIVE_USE && !delim_ptr->use_accepts_lut)
		{
			lut_okay = ESSL_FALSE;
		}
		if(delim_ptr->kind == LIVE_USE && START_OF_SUBCYCLE(0) == POSITION_TO_RELATIVE_POSITION(delim_ptr->position))
		{
			/* store node in next cycle. flag that we're not in endgame mode by aborting early */
			break;
		}
		delim_ptr = delim_ptr->next;
	}
	if(delim_ptr)
	{
		/* normal case */
		if((curr_move = create_move(ctx, word, n, lr)) != 0)
		{
			prev_move->successors[prev_move->n_successors++] = curr_move;
			if(insert_bypass_moves_rec(ctx, curr_move, lr, delim, word, 0, curr_block, ESSL_FALSE))
			{
				return ESSL_TRUE;
			}
			--prev_move->n_successors;
		}
		if(lut_okay && (curr_move = create_lut_move(ctx, word, n)) != 0)
		{
			prev_move->successors[prev_move->n_successors++] = curr_move;
			if(insert_bypass_moves_rec(ctx, curr_move, lr, delim, word, 0, curr_block, ESSL_FALSE))
			{
				return ESSL_TRUE;
			}
			--prev_move->n_successors;
		}
		

	} else {
		/* endgame case */

		if(lut_okay && (curr_move = create_lut_move(ctx, word, n)) != 0)
		{
			prev_move->successors[prev_move->n_successors++] = curr_move;
			return ESSL_TRUE;
		}
		if((curr_move = create_move(ctx, word, n, lr)) != 0)
		{
			prev_move->successors[prev_move->n_successors++] = curr_move;
			return ESSL_TRUE;
		}
	}
	return ESSL_FALSE;
}


static essl_bool insert_bypass_moves_rec(bypass_context *ctx, bypass_move *prev_move, live_range *lr, live_delimiter *delim, maligp2_instruction_word *start_word, int offset, basic_block *curr_block, essl_bool first_move_in_successor_block)
{
	int i;
	int lifetime_mask;
	int lifetime_end = 0;



	/* advance delimiters past the def */
	while(delim != 0 && delim->position >= prev_move->def_position) 
	{
		delim = delim->next;
	}

	while(delim != 0 && delim->position >= START_OF_CYCLE(start_word->cycle)) 
	{
		delim = delim->next;
	}
	if(delim == 0)
	{
		return ESSL_TRUE; /* done */
	}
	if(first_move_in_successor_block)
	{
		assert(start_word->predecessor == 0);
		if(curr_block->predecessors != 0 && curr_block->predecessors->next != 0)
		{
			/* more than one predecessor -> leave this for the work register allocator */
			return ESSL_FALSE;
		}
	}

	lifetime_mask = lifetime_for_def(prev_move, &lifetime_end);
	/*  look for store usages, these need a def in a functional unit at that exact cycle */
	{
		int min_cycle = -1;
		live_delimiter *delim_ptr = delim;
		maligp2_instruction_word *curr_word = start_word;
		for(i = offset; i < 3 && curr_word != 0; ++i, curr_word = curr_word->successor)
		{
			min_cycle = curr_word->cycle;
			if(lifetime_mask & (1 << i))
			{
				while(delim_ptr != 0 && delim_ptr->position >= START_OF_CYCLE(curr_word->cycle))
				{
					delim_ptr = delim_ptr->next;
				}

				while(delim_ptr != 0 && delim_ptr->position > END_OF_CYCLE(curr_word->cycle))
				{
					if(delim_ptr->kind == LIVE_USE && START_OF_SUBCYCLE(0) == POSITION_TO_RELATIVE_POSITION(delim_ptr->position))
					{
						/* use in a store, we need to insert a cycle right here */
						
						return place_mov_at_cycle(ctx, prev_move, lr, delim, curr_word, ESSL_TRUE, curr_block);
							
					}
					delim_ptr = delim_ptr->next;
				}

			}

		}
		if(min_cycle != -1)
		{
			lifetime_end = ESSL_MAX(lifetime_end, END_OF_CYCLE(min_cycle));
		}

	}


	/* now see if all remaining delimiters are within the lifetime of the current def */
	{
		live_delimiter *delim_ptr = delim;
		while(delim_ptr != 0 && delim_ptr->position > lifetime_end)
		{
			delim_ptr = delim_ptr->next;
		}
		if(delim_ptr == 0)
		{
			return ESSL_TRUE; /* yay, done with this live range */
		}


	}

	{
		essl_bool lut_is_acceptable = ESSL_TRUE;
		for(i = 2; i >= offset; --i)
		{
			maligp2_instruction_word *curr_word = start_word;
			int j;
			for(j = offset; j < i && curr_word != 0; ++j)
			{
				curr_word = curr_word->successor;
			}
			if(j != i && (lifetime_mask & (1 << i)))
			{
				if(curr_block->n_successors > 0)
				{
					unsigned i;
					essl_bool succeeded = ESSL_TRUE;
					/* reached the end of this basic block before we got to i. will try placing moves in the successor blocks instead */
					for(i = 0; i < curr_block->n_successors; ++i)
					{
						basic_block *target = curr_block->successors[i];
						succeeded = ESSL_TRUE;
						if(!insert_bypass_moves_rec(ctx, prev_move, lr, delim, (maligp2_instruction_word*)target->earliest_instruction_word, j, target, ESSL_TRUE))
						{
							succeeded = ESSL_FALSE;
							break;
						}				
					}
					if(succeeded) 
					{
						/* got through the entire list */
						return ESSL_TRUE;
					} else {
						prev_move->n_successors = 0; /* reset possible half-done moves */
					}
				}				
				

			} else if(curr_word && (lifetime_mask & (1 << i)))
			{
				if(place_mov_at_cycle(ctx, prev_move, lr, delim, curr_word, lut_is_acceptable, curr_block))
				{
					return ESSL_TRUE;
				}
				
				lut_is_acceptable = ESSL_FALSE;
			}
		}
	}
	return ESSL_FALSE;
}



static essl_bool insert_bypass_moves(bypass_context *ctx, live_range *lr)
{
	essl_bool res = ESSL_FALSE;
	bypass_move first_move = {NULL, 0,0,0, {NULL, NULL}};
	live_delimiter *delim;
	maligp2_instruction_word *start_word;
	maligp2_instruction *instr;
	basic_block *start_block = NULL;
	unsigned n_defs = 0;
	unsigned n_uses = 0;

	/* Ignore spilled ranges - they are already taken care of */
	if (lr->spilled) return ESSL_TRUE;

	for(delim = lr->points; delim != 0; delim = delim->next)
	{
		if(delim->kind == LIVE_DEF) ++n_defs;
		if(delim->kind == LIVE_USE) ++n_uses;
	}
	assert(n_defs != 0);
	if(n_defs > 1) return ESSL_FALSE; /* merged phi node, we can't handle this */
	assert(lr->points != 0);
	if(lr->points->kind == LIVE_RESTART)
	{
		/* range that is defined in a later basic block, we can't handle this */
		return ESSL_FALSE;
	}
	assert(lr->points->kind == LIVE_DEF);

	start_word = instruction_word_for_position(ctx, lr->points->position, &start_block);
	first_move.def_position = lr->points->position;
	first_move.def_word = start_word;
	instr = get_instruction_defining_node(lr->var, start_word);
	assert(instr != 0);
	
	ESSL_CHECK(instr);

	first_move.def_schedule_class = instr->schedule_class;
	first_move.n_successors = 0;
	
	res = insert_bypass_moves_rec(ctx, &first_move, lr, lr->points, start_word, 0, start_block, ESSL_FALSE);
	if(res)
	{
		unsigned i;
		for(i = 0; i < first_move.n_successors; ++i)
		{
			ESSL_CHECK(fill_moves(ctx, lr->var, first_move.successors[i]));
		}
		fulfill_move_reservations_nearby(first_move.def_word, lr->var);

	}


	return res;
}


static void prepare_for_rollback(bypass_context *ctx)
{
	size_t i, j;
	for (i = 0 ; i < ctx->cfg->n_blocks ; i++) {
		basic_block *block = ctx->cfg->output_sequence[i];
		maligp2_instruction_word *word;
		for (word = block->earliest_instruction_word ; word != 0 ; word = word->successor) 
		{
			word->n_moves_available_before_reg_alloc = word->n_moves_available;
			word->used_slots_before_reg_alloc = word->used_slots;
			for(j = 0; j < MALIGP2_MAX_MOVES; ++j)
			{
				assert(word->move_reservation_fulfilled[j] == ESSL_FALSE);

			}


		}
	}
}


void _essl_maligp2_rollback_bypass_network(symbol *fun)
{
	control_flow_graph *cfg = fun->control_flow_graph;
	size_t i, j;
	for (i = 0 ; i < cfg->n_blocks ; i++) {
		basic_block *block = cfg->output_sequence[i];
		maligp2_instruction_word *word;
		for (word = block->earliest_instruction_word ; word != 0 ; word = word->successor) 
		{
			word->n_moves_available = word->n_moves_available_before_reg_alloc;
			word->used_slots = word->used_slots_before_reg_alloc;
#define DO(mask, slot) if((word->used_slots & mask) == 0) slot = 0;
			DO(MALIGP2_SC_LOAD0, word->u.real_slots.load0[0]);
			DO(MALIGP2_SC_LOAD0, word->u.real_slots.load0[1]);
			DO(MALIGP2_SC_LOAD0, word->u.real_slots.load0[2]);
			DO(MALIGP2_SC_LOAD0, word->u.real_slots.load0[3]);
			DO(MALIGP2_SC_LOAD1, word->u.real_slots.load1[0]);
			DO(MALIGP2_SC_LOAD1, word->u.real_slots.load1[1]);
			DO(MALIGP2_SC_LOAD1, word->u.real_slots.load1[2]);
			DO(MALIGP2_SC_LOAD1, word->u.real_slots.load1[3]);
			DO(MALIGP2_SC_LOAD_CONST, word->u.real_slots.load_const[0]);
			DO(MALIGP2_SC_LOAD_CONST, word->u.real_slots.load_const[1]);
			DO(MALIGP2_SC_LOAD_CONST, word->u.real_slots.load_const[2]);
			DO(MALIGP2_SC_LOAD_CONST, word->u.real_slots.load_const[3]);

			DO(MALIGP2_SC_ADD0, word->u.real_slots.add0);
			if(word->u.real_slots.add0 == 0) word->add_opcodes[0] = MALIGP2_NOP;
			DO(MALIGP2_SC_ADD1, word->u.real_slots.add1);
			if(word->u.real_slots.add1 == 0) word->add_opcodes[1] = MALIGP2_NOP;
			DO(MALIGP2_SC_MUL0, word->u.real_slots.mul0);
			if(word->u.real_slots.mul0 == 0) word->mul_opcodes[0] = MALIGP2_NOP;
			DO(MALIGP2_SC_MUL1, word->u.real_slots.mul1);
			if(word->u.real_slots.mul1 == 0) word->mul_opcodes[1] = MALIGP2_NOP;
			DO(MALIGP2_SC_MISC, word->u.real_slots.misc);
			DO(MALIGP2_SC_LUT, word->u.real_slots.lut);
			DO(MALIGP2_SC_BRANCH, word->u.real_slots.branch);
			DO(MALIGP2_SC_STORE_XY, word->u.real_slots.store_xy);
			DO(MALIGP2_SC_STORE_ZW, word->u.real_slots.store_zw);


#undef DO
			for(j = 0; j < MALIGP2_MAX_MOVES; ++j)
			{
				if(word->move_reservation_fulfilled[j])
				{
					word->move_reservation_fulfilled[j] = ESSL_FALSE;
				}

			}


		}
	}

}


memerr _essl_maligp2_allocate_bypass_network(mempool *pool, symbol *fun, target_descriptor *desc, live_range *ranges, live_range **out_allocated_ranges, live_range **out_unallocated_ranges)

{
	mempool tmp_pool;
	bypass_context context, *ctx = &context;
	*out_unallocated_ranges = 0;
	*out_allocated_ranges = 0;
	ESSL_CHECK(_essl_mempool_init(&tmp_pool, 0, _essl_mempool_get_tracker(pool)));
	ctx->fun = fun;
	ctx->cfg = fun->control_flow_graph;
	ctx->permanent_pool = pool;
	ctx->move_pool = pool;
	ctx->temp_pool = &tmp_pool;
	ctx->desc = desc;
	prepare_for_rollback(ctx);

	while(ranges != 0)
	{
		live_range *curr_range = ranges;
		ranges = ranges->next;

		if(insert_bypass_moves(ctx, curr_range))
		{
			LIST_INSERT_FRONT(out_allocated_ranges, curr_range);

		} else {
			if(_essl_mempool_get_tracker(pool)->out_of_memory_encountered)
			{
				_essl_mempool_destroy(&tmp_pool);
				return MEM_ERROR;
			}
			LIST_INSERT_FRONT(out_unallocated_ranges, curr_range);

		}
		ESSL_CHECK(_essl_mempool_clear(&tmp_pool));

	}
	_essl_mempool_destroy(&tmp_pool);
	
	return MEM_OK;

}


typedef struct
{
	maligp2_instruction_word *prev2_word;
	maligp2_instruction_word *prev_word;
	maligp2_instruction_word *curr_word;
	essl_bool prev_value_enabled;

} maligp2_prev_instructions;

static memerr integrate_inputs(maligp2_instruction *inst, maligp2_prev_instructions *prevs) {
	unsigned i, j;
	for (i = 0 ; i < MALIGP2_MAX_INPUT_ARGS ; i++) {
		if (inst->args[i].arg) {
#define DO(unit, index) if((unit) != 0 && (unit)->instr_node == inst->args[i].arg) inst->args[i].reg_index = index;
			if(prevs->prev2_word != 0)
			{
				DO(prevs->prev2_word->u.real_slots.add0, MALIGP2_ADD0_DELAY1);
				DO(prevs->prev2_word->u.real_slots.add1, MALIGP2_ADD1_DELAY1);
				DO(prevs->prev2_word->u.real_slots.mul0, MALIGP2_MUL0_DELAY1);
				DO(prevs->prev2_word->u.real_slots.mul1, MALIGP2_MUL1_DELAY1);
				DO(prevs->prev2_word->u.real_slots.misc, MALIGP2_MISC_DELAY1);
			}
			if(prevs->prev_word != 0)
			{
				for(j = 0; j < MALIGP2_NATIVE_VECTOR_SIZE; ++j)
				{
					DO(prevs->prev_word->u.real_slots.load0[j], MALIGP2_INPUT_REGISTER_0_X_DELAY0 + j);
				}
				DO(prevs->prev_word->u.real_slots.add0, MALIGP2_ADD0_DELAY0);
				DO(prevs->prev_word->u.real_slots.add1, MALIGP2_ADD1_DELAY0);
				DO(prevs->prev_word->u.real_slots.mul0, MALIGP2_MUL0_DELAY0);
				DO(prevs->prev_word->u.real_slots.mul1, MALIGP2_MUL1_DELAY0);
				DO(prevs->prev_word->u.real_slots.misc, MALIGP2_MISC_DELAY0);
				DO(prevs->prev_word->u.real_slots.lut, MALIGP2_LUT);
			}
			assert(prevs->curr_word != 0);
			for(j = 0; j < MALIGP2_NATIVE_VECTOR_SIZE; ++j)
			{
				DO(prevs->curr_word->u.real_slots.load0[j], MALIGP2_INPUT_REGISTER_0_X + j);
				DO(prevs->curr_word->u.real_slots.load1[j], MALIGP2_INPUT_REGISTER_1_X + j);
				DO(prevs->curr_word->u.real_slots.load_const[j], MALIGP2_CONSTANT_REGISTER_X + j);
			}
#undef DO
			assert(inst->args[i].arg == 0 || inst->args[i].reg_index != -1);
			ESSL_CHECK(inst->args[i].arg == 0 || inst->args[i].reg_index != -1);
			if(inst->args[i].reg_index != -1)
			{
				assert(inst->args[i].reg_index >= 0 && inst->args[i].reg_index < 32);
				inst->args[i].arg = 0;
			}


		}
	}

	return MEM_OK;
}

static memerr integrate_store_inputs(maligp2_instruction *inst, maligp2_prev_instructions *prevs) {
	unsigned i;

	for (i = 0 ; i < MALIGP2_MAX_INPUT_ARGS ; i++) 
	{
		if (inst->args[i].arg) 
		{
#define DO(unit, index) if((unit) != 0 && (unit)->instr_node == inst->args[i].arg) inst->args[i].reg_index = index;
			if (prevs->prev_word != 0) {
				DO(prevs->prev_word->u.real_slots.mul0, MALIGP2_OUTPUT_MUL0_DELAY1);
			}
			assert(prevs->curr_word != 0);
			DO(prevs->curr_word->u.real_slots.add0, MALIGP2_OUTPUT_ADD0);
			DO(prevs->curr_word->u.real_slots.add1, MALIGP2_OUTPUT_ADD1);
			DO(prevs->curr_word->u.real_slots.mul0, MALIGP2_OUTPUT_MUL0);
			DO(prevs->curr_word->u.real_slots.mul1, MALIGP2_OUTPUT_MUL1);
			DO(prevs->curr_word->u.real_slots.misc, MALIGP2_OUTPUT_MISC);
			DO(prevs->curr_word->u.real_slots.lut, MALIGP2_OUTPUT_LUT_RES0);
#undef DO
			assert(inst->args[i].arg == 0 || inst->args[i].reg_index != -1);
			ESSL_CHECK(inst->args[i].arg == 0 || inst->args[i].reg_index != -1);
			if(inst->args[i].reg_index != -1)
			{
				assert(inst->args[i].reg_index >= 0 && inst->args[i].reg_index < 8);
				inst->args[i].arg = 0;
			}


		} else {
			inst->args[i].reg_index = MALIGP2_OUTPUT_DISABLE;
		}
	}

	return MEM_OK;
}

static void rotate_instructions(maligp2_instruction **arr, int n_elems, int rotation)
{
	int i;
	if(rotation > 0)
	{
		for(i = n_elems - 1; i >= rotation; --i)
		{
			arr[i] = arr[i-rotation];
		}
		for(i = 0; i < rotation; ++i)
		{
			arr[i] = NULL;
		}
	} else if(rotation < 0)
	{
		for(i = 0; i < n_elems-rotation; ++i)
		{
			arr[i] = arr[i+rotation];
		}
		for(i = n_elems - rotation; i < n_elems; ++i)
		{
			arr[i] = NULL;
		}


	}


}
#define INVALID_ROTATION 42
static memerr fixup_load_instruction(maligp2_instruction **arr)
{
	int i;
	int rotation = INVALID_ROTATION;
	int max_idx = -1;
	int min_idx = MALIGP2_NATIVE_VECTOR_SIZE;
	int n_loads = 0;
	for(i = 0; i < MALIGP2_NATIVE_VECTOR_SIZE; ++i)
	{
		if(arr[i] != NULL)
		{
			int this_rotation = (arr[i]->address_offset & (MALIGP2_NATIVE_VECTOR_SIZE-1)) - i;
			assert(arr[i]->opcode != MALIGP2_CONSTANT);
			max_idx = ESSL_MAX(max_idx, i);
			min_idx = ESSL_MIN(min_idx, i);
			n_loads++;
			if(rotation == INVALID_ROTATION)
			{
				rotation = this_rotation;
			} else {
				assert(rotation == this_rotation);
				ESSL_CHECK(rotation == this_rotation);
			}
		}
	}

	if(rotation != INVALID_ROTATION)
	{
		assert(max_idx + rotation < MALIGP2_NATIVE_VECTOR_SIZE);
		ESSL_CHECK(max_idx + rotation < MALIGP2_NATIVE_VECTOR_SIZE);
		assert(min_idx + rotation >= 0);
		ESSL_CHECK(min_idx + rotation >= 0);
		rotate_instructions(arr, MALIGP2_NATIVE_VECTOR_SIZE, rotation);
	}
	return MEM_OK;
}

static void rotate_input_arguments(maligp2_input_argument **arr, unsigned n_elems, int rotation)
{
	int i;
	for(i = n_elems - 1; i >= rotation; --i)
	{
		*arr[i] = *arr[i-rotation];
	}
	for(i = 0; i < rotation; ++i)
	{
		arr[i]->reg_index = -1;
		arr[i]->arg = NULL;
		arr[i]->negate = 0;
	}


}

static memerr fixup_store_instructions(maligp2_instruction_word *word)
{
	maligp2_instruction *store_xy = word->u.real_slots.store_xy;
	maligp2_instruction *store_zw = word->u.real_slots.store_zw;
	maligp2_opcode op = MALIGP2_NOP;
	maligp2_input_argument *args[MALIGP2_NATIVE_VECTOR_SIZE] = {0};
	if(store_xy == NULL && store_zw == NULL) return MEM_OK;
	if(store_xy != NULL)
	{
		if(op == MALIGP2_NOP)
		{
			op = store_xy->opcode;
		} else {
			assert(op == store_xy->opcode);

		}
	}
	if(store_zw != NULL)
	{
		if(op == MALIGP2_NOP)
		{
			op = store_zw->opcode;
		} else {
			assert(op == store_zw->opcode);
		}
	}
	if(op == MALIGP2_STORE_WORK_REG || op == MALIGP2_NOP) return MEM_OK;
	assert(store_xy != NULL);
	ESSL_CHECK(store_xy != NULL); 
	
	if(store_xy != NULL)
	{
		args[0] = &store_xy->args[0];
		args[1] = &store_xy->args[1];
	}
	if(store_zw != NULL)
	{
		args[2] = &store_zw->args[0];
		args[3] = &store_zw->args[1];
	}
	{
		int rotation = -1;
		rotation = store_xy->address_offset % MALIGP2_NATIVE_VECTOR_SIZE;
		assert(store_zw == NULL || store_zw->address_offset % MALIGP2_NATIVE_VECTOR_SIZE == rotation);

		assert(rotation >= 0);
		if(rotation > 0)
		{
			rotate_input_arguments(args, store_zw != NULL ? MALIGP2_NATIVE_VECTOR_SIZE : 2, rotation);
		}
	}

	

	return MEM_OK;
}

static memerr integrate_bypass_allocations_for_function(mempool *tmp_pool, control_flow_graph *cfg) 
{
	unsigned i;
	int j;
	unsigned k;
	maligp2_prev_instructions *prev_array;

	ESSL_CHECK(prev_array = _essl_mempool_alloc(tmp_pool, sizeof(*prev_array)*cfg->n_blocks));

	for (i = 0 ; i < cfg->n_blocks ; i++) 
	{
		basic_block *block = cfg->output_sequence[i];
		maligp2_instruction_word *word;
		maligp2_prev_instructions *prevs = &prev_array[i];
		prevs->prev_value_enabled = (block->predecessors != 0 && block->predecessors->next == 0) && (block->predecessors->block->output_visit_number == block->output_visit_number - 1);
		for (word = (maligp2_instruction_word *)block->earliest_instruction_word ; word != 0 ; word = word->successor) {
			prevs->prev2_word = prevs->prev_word;
			prevs->prev_word = prevs->curr_word;
			prevs->curr_word = word;

			ESSL_CHECK(fixup_load_instruction(word->u.real_slots.load_const));
			ESSL_CHECK(fixup_load_instruction(word->u.real_slots.load0));
			ESSL_CHECK(fixup_load_instruction(word->u.real_slots.load1));
			ESSL_CHECK(fixup_store_instructions(word));

#define DO(unit) if(word->u.real_slots.unit != 0) ESSL_CHECK(integrate_inputs(word->u.real_slots.unit, prevs))
#define DO_STORE(unit) if(word->u.real_slots.unit != 0) ESSL_CHECK(integrate_store_inputs(word->u.real_slots.unit, prevs))
			DO(add0);
			DO(add1);
			DO(mul0);
			DO(mul1);
			DO(misc);
			DO(lut);
			DO_STORE(store_xy);
			DO_STORE(store_zw);
#undef DO
#undef DO_STORE

			for(j = 0; j < MALIGP2_MAX_MOVES; ++j)
			{
				assert(word->reserved_moves[j] == 0 || word->move_reservation_fulfilled[j]);
			}

			prevs->prev_value_enabled = ESSL_TRUE;

		}
		prevs->prev_value_enabled = ESSL_FALSE;

		for(k = 0; k < block->n_successors; ++k)
		{
			basic_block *succ = block->successors[k];
			prev_array[succ->output_visit_number] = *prevs;
		}
	}
	return MEM_OK;
}

memerr _essl_maligp2_integrate_bypass_allocations(mempool *tmp_pool, translation_unit *tu)
{
	symbol_list *sl;
	for(sl = tu->functions; sl != 0; sl = sl->next)
	{
		control_flow_graph *cfg = sl->sym->control_flow_graph;
		ESSL_CHECK(integrate_bypass_allocations_for_function(tmp_pool, cfg));
	}
	return MEM_OK;

}
