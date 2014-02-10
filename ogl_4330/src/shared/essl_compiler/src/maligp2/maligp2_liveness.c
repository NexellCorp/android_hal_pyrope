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
#include "common/ptrdict.h"
#include "common/ptrset.h"
#include "maligp2/maligp2_instruction.h"
#include "common/basic_block.h"
#include "maligp2/maligp2_slot.h"

static /*@null@*/ node *get_node(/*@null@*/node *n)
{
	while(n && n->hdr.kind == EXPR_KIND_TRANSFER)
	{
		n = GET_CHILD(n, 0);
	}
	assert(n != 0);
	return n;
}

static memerr mark_instruction_uses(liveness_context *ctx, maligp2_instruction_word *word, maligp2_instruction *inst, int subcycle) {
	if (inst != 0) {
		int i;

		/* Uses incurred by input arguments */
		for (i = 0 ; i < MALIGP2_MAX_INPUT_ARGS ; i++) {
			node **argp;
			if(inst->args[i].arg != 0)
			{
				essl_bool lut_okay = ESSL_TRUE;
				inst->args[i].arg = get_node(inst->args[i].arg);
				argp = &inst->args[i].arg;
				if(inst->schedule_class & (MALIGP2_SC_MUL0 | MALIGP2_SC_MUL1 | MALIGP2_SC_ADD0 | MALIGP2_SC_ADD1))
				{
					/* common case: only arg 0 can take lut */
					lut_okay = (i == 0);
					/* but wait, there is more!  If this is a MOV, the
					   story gets a little more complex, as some add
					   MOVes are encoded by placing the operand in
					   both arg 0 and 1, and arg 1 does not accept
					   lut. Figure out if this is the case.
					*/
					if(inst->opcode == MALIGP2_MOV && inst->schedule_class & (MALIGP2_SC_ADD0 | MALIGP2_SC_ADD1))
					{
						if(_essl_maligp2_add_slot_move_needs_two_inputs(_essl_maligp2_get_add_slot_opcode(word->add_opcodes[0], word->add_opcodes[1])))
						{
							lut_okay = ESSL_FALSE;
						}

					}

				}
				ESSL_CHECK(_essl_liveness_mark_use(ctx, argp, START_OF_SUBCYCLE(subcycle),
											  0x1, ESSL_FALSE, lut_okay));
			}
		}
	}

	return MEM_OK;
}

static memerr mark_instruction_defs(liveness_context *ctx, maligp2_instruction *inst, int subcycle) {
	if (inst != 0 && inst->instr_node != 0) {
		inst->instr_node = get_node(inst->instr_node);
		/* Killing definition */
		ESSL_CHECK(_essl_liveness_mark_def(ctx, &inst->instr_node, END_OF_SUBCYCLE(subcycle),
									  0x1, 0));
	}

	return MEM_OK;
}

static memerr mark_instruction_sequence_defs_and_uses(liveness_context *ctx, basic_block *block, void *dummy) {
	maligp2_instruction_word *word;
	IGNORE_PARAM(dummy);
	for (word = (maligp2_instruction_word *)block->latest_instruction_word ; word != 0 ; word = word->predecessor) {
		/* Init range point to end of subcycle 0 */
		int subcycle = MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 0);
		unsigned i;

		ESSL_CHECK(mark_instruction_defs(ctx, word->u.real_slots.branch, subcycle));
		ESSL_CHECK(mark_instruction_defs(ctx, word->u.real_slots.store_xy, subcycle));
		ESSL_CHECK(mark_instruction_defs(ctx, word->u.real_slots.store_zw, subcycle));

		ESSL_CHECK(mark_instruction_uses(ctx, word, word->u.real_slots.branch, subcycle));
		ESSL_CHECK(mark_instruction_uses(ctx, word, word->u.real_slots.store_xy, subcycle));
		ESSL_CHECK(mark_instruction_uses(ctx, word, word->u.real_slots.store_zw, subcycle));

		subcycle++;

		ESSL_CHECK(mark_instruction_defs(ctx, word->u.real_slots.add0, subcycle));
		ESSL_CHECK(mark_instruction_defs(ctx, word->u.real_slots.add1, subcycle));
		ESSL_CHECK(mark_instruction_defs(ctx, word->u.real_slots.mul0, subcycle));
		ESSL_CHECK(mark_instruction_defs(ctx, word->u.real_slots.mul1, subcycle));
		ESSL_CHECK(mark_instruction_defs(ctx, word->u.real_slots.lut, subcycle));
		ESSL_CHECK(mark_instruction_defs(ctx, word->u.real_slots.misc, subcycle));

		ESSL_CHECK(mark_instruction_uses(ctx, word, word->u.real_slots.add0, subcycle));
		ESSL_CHECK(mark_instruction_uses(ctx, word, word->u.real_slots.add1, subcycle));
		ESSL_CHECK(mark_instruction_uses(ctx, word, word->u.real_slots.mul0, subcycle));
		ESSL_CHECK(mark_instruction_uses(ctx, word, word->u.real_slots.mul1, subcycle));
		ESSL_CHECK(mark_instruction_uses(ctx, word, word->u.real_slots.lut, subcycle));
		ESSL_CHECK(mark_instruction_uses(ctx, word, word->u.real_slots.misc, subcycle));

		for (i = 0 ; i < MALIGP2_MAX_MOVES ; i++) {
			if (word->reserved_moves[i] != 0) {
				word->reserved_moves[i] = get_node(word->reserved_moves[i]);
				assert(word->reserved_moves[i] != 0);
				ESSL_CHECK(_essl_liveness_mark_use(ctx, &word->reserved_moves[i], START_OF_SUBCYCLE(subcycle),
											  0x1, ESSL_TRUE, 1));
			}
		}

		subcycle++;

		for(i = 0; i < MALIGP2_NATIVE_VECTOR_SIZE; ++i)
		{
			ESSL_CHECK(mark_instruction_defs(ctx, word->u.real_slots.load0[i], subcycle));
			ESSL_CHECK(mark_instruction_defs(ctx, word->u.real_slots.load1[i], subcycle));
			ESSL_CHECK(mark_instruction_defs(ctx, word->u.real_slots.load_const[i], subcycle));
		}

		for(i = 0; i < MALIGP2_NATIVE_VECTOR_SIZE; ++i)
		{
			ESSL_CHECK(mark_instruction_uses(ctx, word, word->u.real_slots.load0[i], subcycle));
			ESSL_CHECK(mark_instruction_uses(ctx, word, word->u.real_slots.load1[i], subcycle));
			ESSL_CHECK(mark_instruction_uses(ctx, word, word->u.real_slots.load_const[i], subcycle));
		}


	}

	return MEM_OK;
}

/** Component mask for variable size */
static unsigned int mask_from_node(node *n) {
	return (1U << GET_NODE_VEC_SIZE(n))-1;
}

liveness_context *_essl_maligp2_calculate_live_ranges(mempool *pool, control_flow_graph *cfg, target_descriptor *desc,
													  error_context *err) {
	liveness_context *ctx;
	int block_i;

	ESSL_CHECK(ctx = _essl_liveness_create_context(pool, cfg, desc, mark_instruction_sequence_defs_and_uses, 0, mask_from_node, err));
	ESSL_CHECK(_essl_liveness_calculate_live_ranges(ctx));

	/* Check that all variables have been assigned ranges */
	for (block_i = cfg->n_blocks-1 ; block_i >= 0 ; block_i--) {
		maligp2_instruction_word *word;
		for (word = cfg->output_sequence[block_i]->earliest_instruction_word ; word != 0 ; word = word->successor) {
			maligp2_instruction **instp = MALIGP2_FIRST_INSTRUCTION_IN_WORD(word);
			int j;
			for (j = 0 ; j < MALIGP2_N_INSTRUCTIONS_IN_WORD ; j++) {
				maligp2_instruction *inst = instp[j];
				if (inst) {
					int k;
					assert(inst->instr_node == 0 || _essl_ptrdict_lookup(&ctx->var_to_range, inst->instr_node));
					for (k = 0 ; k < MALIGP2_MAX_INPUT_ARGS ; k++) {
						assert(inst->args[k].arg == 0 || _essl_ptrdict_lookup(&ctx->var_to_range, inst->args[k].arg));
					}
				}
			}
		}
	}

	return ctx;
}

essl_bool _essl_maligp2_is_fixedpoint_range(live_range *r)
{
	return
		r->var->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL &&
		(r->var->expr.operation == EXPR_OP_FUN_MALIGP2_FLOAT_TO_FIXED ||
		 r->var->expr.operation == EXPR_OP_FUN_MALIGP2_LOG2_FIXED);
}

static void insert_cycle_into_instructions(basic_block *block, int position) {
	maligp2_instruction_word *word;
	for (word = (maligp2_instruction_word *)block->earliest_instruction_word ; word != 0 ; word = word->successor) {
		int j;
		maligp2_instruction **insts = MALIGP2_FIRST_INSTRUCTION_IN_WORD(word);
		if (START_OF_CYCLE(word->cycle) > position) {
			word->cycle += 1;
		}
		for (j = 0 ; j < MALIGP2_N_INSTRUCTIONS_IN_WORD ; j++) {
			if (insts[j] && END_OF_SUBCYCLE(insts[j]->subcycle) >= position) {
				insts[j]->subcycle += MALIGP2_SUBCYCLES_PER_CYCLE;
			}
		}
	}
}

static memerr reserve_move_for_fixed_point_range(liveness_context *ctx, maligp2_instruction_word *word)
{
	live_range *range;
	for (range = ctx->var_ranges ; range != 0 ; range = range->next)
	{
		if (_essl_maligp2_is_fixedpoint_range(range))
		{
			live_delimiter *delim;
			for (delim = range->points ; delim != 0 && delim->next != 0 ; delim = delim->next)
			{
				if (delim->position > START_OF_CYCLE(word->cycle) && delim->next->position < END_OF_CYCLE(word->cycle))
				{
					/* Fixed point range crossing newly inserted word */
					int i;
					node **move_ref = 0;
					int move_position = START_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 1));
					live_delimiter *move_delim;

					/* Reserve move */
					DO_ASSERT(_essl_maligp2_reserve_move(ctx->cfg, ctx->desc, word, range->var));
					for (i = 0 ; i < MALIGP2_MAX_MOVES ; i++)
					{
						if (word->reserved_moves[i] == range->var)
						{
							move_ref = &word->reserved_moves[i];
							break;
						}
					}
					assert(move_ref);

					/* Make move reservation delimiter */
					ESSL_CHECK(move_delim = _essl_liveness_new_delimiter(ctx->pool, move_ref, LIVE_USE, move_position));
					move_delim->mask = 0x1;
					move_delim->live_mask = 0x1;
					move_delim->locked = 1;
				
					/* Insert delimiter */
					move_delim->next = delim->next;
					delim->next = move_delim;

					return MEM_OK;
				}
			}
		}
	}
	return MEM_OK;
}

maligp2_instruction_word *_essl_maligp2_insert_word_before(mempool *pool, liveness_context *ctx, maligp2_instruction_word *word, basic_block *block) {
	maligp2_instruction_word *new_word;
	assert(!_essl_maligp2_inseparable_from_predecessor(word));
	ESSL_CHECK(new_word = _essl_new_maligp2_instruction_word(pool, word->cycle+1));
	ESSL_CHECK(_essl_liveness_insert_cycle(ctx, START_OF_CYCLE(word->cycle), block, insert_cycle_into_instructions));
	/* Insert word */
	new_word->predecessor = word->predecessor;
	new_word->successor = word;
	if (word->predecessor != 0) {
		word->predecessor->successor = new_word;
	} else {
		block->earliest_instruction_word = new_word;
	}
	word->predecessor = new_word;

	ESSL_CHECK(reserve_move_for_fixed_point_range(ctx, new_word));

	return new_word;
}

maligp2_instruction_word *_essl_maligp2_insert_word_after(mempool *pool, liveness_context *ctx, maligp2_instruction_word *word, basic_block *block) {
	essl_bool branch = ESSL_FALSE;
	maligp2_instruction_word *new_word;
	assert(!_essl_maligp2_inseparable_from_successor(word));
	ESSL_CHECK(new_word = _essl_new_maligp2_instruction_word(pool, word->cycle));
	/* STOP delimiters at the end of the word should not be moved up, so we add one position */
	ESSL_CHECK(_essl_liveness_insert_cycle(ctx, END_OF_CYCLE(word->cycle)+1, block, insert_cycle_into_instructions));
	/* Insert word */
	new_word->successor = word->successor;
	new_word->predecessor = word;
	if (word->successor != 0) {
		word->successor->predecessor = new_word;
	} else {
		block->latest_instruction_word = new_word;
	}
	word->successor = new_word;

	/* Move any jump instruction down */
	if (word->u.real_slots.branch) {
		branch = ESSL_TRUE;
		switch (word->u.real_slots.branch->opcode) {
		case MALIGP2_JMP_COND:
			/* Move misc instruction */
			new_word->u.real_slots.misc = word->u.real_slots.misc;
			word->u.real_slots.misc = 0;
			new_word->used_slots |= MALIGP2_SC_MISC;
			word->used_slots &= ~MALIGP2_SC_MISC;
			new_word->u.real_slots.misc->subcycle -= MALIGP2_SUBCYCLES_PER_CYCLE;
			word->n_moves_available += 1;
			new_word->n_moves_available -= 1;
			assert(new_word->u.real_slots.misc->subcycle == MALIGP2_CYCLE_TO_SUBCYCLE(new_word->cycle, 1));

			/* Correct liveness delimiter for misc input */
			{
				node **misc_input_ref = &new_word->u.real_slots.misc->args[0].arg;
				live_range *range = _essl_ptrdict_lookup(&ctx->var_to_range, *misc_input_ref);
				live_delimiter *delim;
				for (delim = range->points ; delim != 0 && delim->next != 0 ; delim = delim->next)
				{
					if (delim->next->var_ref == misc_input_ref)
					{
						delim->next->position -= POSITIONS_PER_CYCLE;
						assert(delim->next->position == START_OF_SUBCYCLE(new_word->u.real_slots.misc->subcycle));

						if (delim->position == END_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 2)))
						{
							/* Source of the condition is a load - insert move reservation */
							unsigned i;
							live_delimiter *move_delim;
							node **move_var_ref = 0;
							int move_pos = START_OF_SUBCYCLE(MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 1));
							DO_ASSERT(_essl_maligp2_reserve_move(ctx->cfg, ctx->desc, word, *misc_input_ref));
							for (i = 0 ; i < MALIGP2_MAX_MOVES ; i++)
							{
								if (word->reserved_moves[i] == *misc_input_ref)
								{
									move_var_ref = &word->reserved_moves[i];
									break;
								}
							}
							assert(move_var_ref);

							ESSL_CHECK(move_delim = _essl_liveness_new_delimiter(ctx->pool, move_var_ref, LIVE_USE, move_pos));
							move_delim->mask = 0x1;
							move_delim->locked = 1;
							move_delim->next = delim->next;
							delim->next = move_delim;
						}
						break;
					}
				}
				assert(delim != 0 && delim->next != 0);
				_essl_liveness_correct_live_range(range);
			}

			/* Fall-through to branch */
		case MALIGP2_RET:
		case MALIGP2_JMP:
		case MALIGP2_LOOP_END:
		case MALIGP2_REPEAT_END:
			/* Move branch instruction */
			new_word->u.real_slots.branch = word->u.real_slots.branch;
			word->u.real_slots.branch = 0;
			new_word->used_slots |= MALIGP2_SC_BRANCH;
			word->used_slots &= ~MALIGP2_SC_BRANCH;
			new_word->u.real_slots.branch->subcycle -= MALIGP2_SUBCYCLES_PER_CYCLE;
			assert(new_word->u.real_slots.branch->subcycle == MALIGP2_CYCLE_TO_SUBCYCLE(new_word->cycle, 0));
			break;
		case MALIGP2_JMP_COND_NOT_CONSTANT:
			/* Not supported */
			assert(word->u.real_slots.branch->opcode != MALIGP2_JMP_COND_NOT_CONSTANT);
			break;
		default:
			/* Not stop of control flow */
			branch = ESSL_FALSE;
			break;
		}
	}

	if (!branch)
	{
		/* Fixed point range crossing word is only possible if there is no branch instruction */
		ESSL_CHECK(reserve_move_for_fixed_point_range(ctx, new_word));
	}

	return new_word;
}

