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
#include "mali200/mali200_word_insertion.h"
#include "mali200/mali200_instruction.h"
#include "mali200/mali200_slot.h"
#include "backend/extra_info.h"

static void insert_cycle_into_instructions(basic_block *block, int position) {
	m200_instruction_word *word;
	for (word = (m200_instruction_word *)block->earliest_instruction_word ; word != 0 ; word = word->successor) {
		int j;
		m200_instruction **insts = M200_FIRST_INSTRUCTION_IN_WORD(word);
		if (START_OF_CYCLE(word->cycle) > position) {
			word->cycle += 1;
		}
		for (j = 0 ; j < M200_N_INSTRUCTIONS_IN_WORD ; j++) {
			if (insts[j] && END_OF_SUBCYCLE(insts[j]->subcycle) >= position) {
				insts[j]->subcycle += M200_SUBCYCLES_PER_CYCLE;
			}
		}
	}
}

/** Adjust all cycle, subcycle and position indicators to accommodate insertion
 *  of an extra instruction word after the specified position in the specified block
 */
static memerr insert_cycle(liveness_context *liv_ctx, int position, basic_block *insertion_block) {
	return _essl_liveness_insert_cycle(liv_ctx, position, insertion_block, insert_cycle_into_instructions);
}


m200_instruction_word *_essl_mali200_insert_word_before(liveness_context *liv_ctx, m200_instruction_word *word, basic_block *block) {
	m200_instruction_word *new_word;
	ESSL_CHECK(new_word = _essl_new_mali200_instruction_word(liv_ctx->pool, word->cycle+1));
	ESSL_CHECK(insert_cycle(liv_ctx, START_OF_CYCLE(word->cycle), block));
	new_word->successor = word;
	new_word->predecessor = word->predecessor;
	if (word->predecessor != 0) word->predecessor->successor = new_word;
	word->predecessor = new_word;
	if (block->earliest_instruction_word == word) block->earliest_instruction_word = new_word;
	return new_word;
}

m200_instruction_word *_essl_mali200_insert_word_after(liveness_context *liv_ctx, m200_instruction_word *word, basic_block *block) {
	m200_instruction_word *new_word;
	ESSL_CHECK(new_word = _essl_new_mali200_instruction_word(liv_ctx->pool, word->cycle));
	ESSL_CHECK(insert_cycle(liv_ctx, END_OF_CYCLE(word->cycle)+1, block));
	new_word->successor = word->successor;
	new_word->predecessor = word;
	if (word->successor != 0) word->successor->predecessor = new_word;
	word->successor = new_word;
	if (block->latest_instruction_word == word) block->latest_instruction_word = new_word;

	if (word->branch) {
		switch (word->branch->opcode) {
		case M200_JMP:
		case M200_JMP_REL:
		case M200_CALL:
		case M200_CALL_REL:
		case M200_KILL:
		case M200_ALT_KILL:
		case M200_GLOB_END:
			if (block->source != 0) {
				/* Conditional jump */
				if ((word->branch->args[0].arg == 0 && word->branch->args[0].reg_index > M200_CONSTANT1) ||
					(word->branch->args[1].arg == 0 && word->branch->args[1].reg_index > M200_CONSTANT1) ||
					(word->branch->args[2].arg == 0 && word->branch->args[2].reg_index > M200_CONSTANT1)) {
					/* Branch instruction uses pseudo registers and thus cannot be moved */
					assert(0);
				}

				/* Copy any embedded constants used by the branch instruction */
				{
					int i;
					int j = 0;
					for (i = 0 ; i < 3 ; i++) {
						m200_input_argument *arg = &word->branch->args[i];
						if (arg->arg == 0 &&
							(arg->reg_index == M200_CONSTANT0 ||
							 arg->reg_index == M200_CONSTANT1))
						{
							float value = word->embedded_constants[arg->reg_index - M200_CONSTANT0][arg->swizzle.indices[0]];
							new_word->embedded_constants[0][j] = value;
							arg->reg_index = M200_CONSTANT0;
							arg->swizzle.indices[0] = j;
							j++;
						}
					}
					new_word->n_embedded_entries[0] = j;
				}

			}

			/* Move the branch instruction down */
			new_word->branch = word->branch;
			word->branch = 0;
			word->used_slots &= ~M200_SC_BRANCH;
			new_word->used_slots |= M200_SC_BRANCH;
			new_word->branch->subcycle -= M200_SUBCYCLES_PER_CYCLE;
			return new_word;
		case M200_RET:
			/* Not supported */
			assert(0);
			return NULL;
		default:
			/* Unknown branch instruction */
			assert(0);
			return NULL;
		}
	}
	return new_word;
}

memerr _essl_mali200_phielim_insert_move(void *vctx, node *src, node *dst, int earliest, int latest, basic_block *block, essl_bool backward,
										 int *src_position_p, int *dst_position_p, node ***src_ref_p, node ***dst_ref_p)
{
	/* OPT: look at backward parameter */
	liveness_context *liv_ctx = vctx;
	m200_instruction_word *word, *first_word;
	int earliest_rel_pos, latest_rel_pos;
	m200_schedule_classes sc = 0;
	int size = GET_NODE_VEC_SIZE(dst);
	m200_instruction *inst;
	IGNORE_PARAM(backward);
	for (first_word = block->earliest_instruction_word ; END_OF_CYCLE(first_word->cycle) >= earliest ; first_word = first_word->successor);
	earliest_rel_pos = earliest-END_OF_CYCLE(first_word->cycle);
	latest_rel_pos = 0;
	for (word = first_word ; word != 0 && START_OF_CYCLE(word->cycle) > latest ; word = word->successor) {
		const m200_schedule_classes SCM2 = M200_SC_SUBCYCLE_0_MASK | M200_SC_SUBCYCLE_1_MASK | M200_SC_SUBCYCLE_3_MASK;
		const m200_schedule_classes SCM1 = M200_SC_SUBCYCLE_0_MASK | M200_SC_SUBCYCLE_2_MASK | M200_SC_SUBCYCLE_3_MASK;
		const m200_schedule_classes SCM0 = M200_SC_SUBCYCLE_1_MASK | M200_SC_SUBCYCLE_2_MASK | M200_SC_SUBCYCLE_3_MASK;
		if (END_OF_CYCLE(word->cycle) < latest) {
			latest_rel_pos = latest-END_OF_CYCLE(word->cycle);
		}
		/* Find available slot - first subcycle cannot be used because it might be repeated */
		if ((earliest_rel_pos >= 6 && latest_rel_pos <= 5 &&
			 (sc = _essl_mali200_allocate_slots(word, M200_SC_MOV, word->used_slots, SCM2, NULL, 0, size)) != 0)
			||
			(earliest_rel_pos >= 4 && latest_rel_pos <= 3 &&
			 (sc = _essl_mali200_allocate_slots(word, M200_SC_MOV, word->used_slots, SCM1, NULL, 0, size)) != 0)
			||
			(earliest_rel_pos >= 2 && latest_rel_pos <= 1 &&
			 (sc = _essl_mali200_allocate_slots(word, M200_SC_MOV, word->used_slots, SCM0, NULL, 0, size)) != 0))
		{
			/* Move slot available */
			break;
		}

		earliest_rel_pos = POSITIONS_PER_CYCLE-1;
	}

	if (sc == 0) {
		/* No space for move - insert new word to hold it */
		if (END_OF_CYCLE(first_word->cycle) < latest) {
			/* Must insert before first word */
			assert(earliest >= START_OF_CYCLE(first_word->cycle));
			ESSL_CHECK(word = _essl_mali200_insert_word_before(liv_ctx, first_word, block));
		} else {
			ESSL_CHECK(word = _essl_mali200_insert_word_after(liv_ctx, first_word, block));
		}
		sc = _essl_mali200_allocate_slots(word, M200_SC_MOV, word->used_slots, 0, NULL, 0, size);
		assert(sc != 0);
	}

	/* Insert move */
	word->used_slots |= sc;
	ESSL_CHECK(inst = _essl_mali200_create_slot_instruction(liv_ctx->pool, word, &sc, M200_MOV));
	inst->args[0].arg = src;
	inst->instr_node = dst;
	inst->args[0].swizzle = inst->output_swizzle =_essl_create_identity_swizzle(size);
	*src_position_p = START_OF_SUBCYCLE(inst->subcycle);
	*dst_position_p = END_OF_SUBCYCLE(inst->subcycle);
	*src_ref_p = &inst->args[0].arg;
	*dst_ref_p = &inst->instr_node;
	return MEM_OK;
}
