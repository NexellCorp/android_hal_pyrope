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
#include "backend/liveness.h"
#include "common/ptrdict.h"
#include "common/ptrset.h"
#include "mali200/mali200_instruction.h"
#include "common/basic_block.h"
#include "backend/extra_info.h"

static /*@null@*/ node *get_node(/*@null@*/node *n)
{
	while(n && n->hdr.kind == EXPR_KIND_TRANSFER)
	{
		n = GET_CHILD(n, 0);
	}
	assert(n != 0);
	return n;
}


static essl_bool def_output_swizzle_covers_use_input_swizzle(swizzle_pattern *def_out, swizzle_pattern *use_in) {
	unsigned i;
	for (i = 0 ; i < N_COMPONENTS ; i++) {
		if (use_in->indices[i] >= 0 && def_out->indices[use_in->indices[i]] == -1) {
			return ESSL_FALSE;
		}
	}
	return ESSL_TRUE;
}

static essl_bool is_instruction_input_locked(m200_instruction *inst) {
	return (essl_bool)((inst->schedule_class & M200_SC_STORE) != 0 ||
		inst->opcode == M200_ALT_KILL);
}

static essl_bool is_instruction_output_locked(m200_instruction *inst) {
	return
		(essl_bool)((inst->schedule_class & M200_SC_STORE) != 0 ||
		((inst->schedule_class & M200_SC_VAR) != 0 && inst->opcode != M200_MOV) ||
		inst->opcode == M200_ATAN1_IT1 ||
		inst->opcode == M200_ATAN2_IT1 ||
		inst->opcode == M200_2X2ADD ||
		inst->opcode == M200_CONS13 ||
		inst->opcode == M200_CONS22 ||
		inst->opcode == M200_CONS31 ||
		inst->opcode == M200_MUL_W1);
}

static memerr mark_instruction_uses(liveness_context *ctx, m200_instruction *inst, int position) {
	if (inst != 0) {
		int i;

		/* Uses incurred by input arguments */
		for (i = 0 ; i < M200_MAX_INPUT_ARGS ; i++) {
			node **argp;
			int mask;
			if (inst->args[i].arg != 0) {

				inst->args[i].arg = get_node(inst->args[i].arg);				

				argp = &inst->args[i].arg;

				mask = _essl_mask_from_swizzle_input(&inst->args[i].swizzle);
				mask &= 0xF;
				if (mask != 0) {
					ESSL_CHECK(_essl_liveness_mark_use(ctx, argp, position, mask,
												  is_instruction_input_locked(inst), 0));
				}
			}
		}
	}

	return MEM_OK;
}

static memerr mark_instruction_defs(liveness_context *ctx, m200_instruction *inst, int position) {
	unsigned write_mask;
	if (inst != 0 && inst->instr_node != 0) {

		inst->instr_node = get_node(inst->instr_node);

		/* Definition incurred by result node */
		write_mask = _essl_mask_from_swizzle_input(&inst->output_swizzle);
		write_mask &= 0xF;
		if(write_mask != 0)
		{
			ESSL_CHECK(_essl_liveness_mark_def(ctx, &inst->instr_node, position,
										  write_mask,
										  is_instruction_output_locked(inst)));
		}
	}

	return MEM_OK;
}

static void rewrite_pseudo_register_uses(m200_instruction *inst, node *var, int reg) {
	int i;
	if (inst != 0) {
		for (i = 0 ; i < M200_MAX_INPUT_ARGS ; i++) {
			if (inst->args[i].arg != NULL)
			{
				inst->args[i].arg = get_node(inst->args[i].arg);
				if(inst->args[i].arg == var) {
					inst->args[i].arg = NULL;
					inst->args[i].reg_index = reg;
				}
			}
		}
	}
}

static void rewrite_pseudo_register_uses_in_word(m200_instruction_word *word, node **varp, int reg) {
	if (*varp != 0) {
		rewrite_pseudo_register_uses(word->mul4, *varp, reg);
		rewrite_pseudo_register_uses(word->mul1, *varp, reg);
		rewrite_pseudo_register_uses(word->add4, *varp, reg);
		rewrite_pseudo_register_uses(word->add1, *varp, reg);
		rewrite_pseudo_register_uses(word->lut, *varp, reg);
		rewrite_pseudo_register_uses(word->store, *varp, reg);
		/* Do not rewrite uses in the branch unit, since the register allocator
		   cannot handle pseudo register uses in the branch unit.
		   The scheduler will have made sure such uses always go via a move.
		*/
		*varp = 0;
	}
}

static void allocate_mul_add_channel(liveness_context *ctx, m200_instruction *mul, m200_instruction *add)
{
	if (mul && add && mul->instr_node)
	{
		live_range *mul_range = _essl_ptrdict_lookup(&ctx->var_to_range, mul->instr_node);
		if (mul_range->points->next != 0 && mul_range->points->next->next == 0)
		{
			/* mul result has only one use - check if this is an input to the add */
			if (mul->instr_node == add->args[0].arg &&
				def_output_swizzle_covers_use_input_swizzle(&mul->output_swizzle, &add->args[0].swizzle))
			{
				/* ADD input 0 is only use of MUL output */
				mul->out_reg = M200_R_IMPLICIT;
				mul->instr_node = 0;
				add->args[0].reg_index = M200_R_IMPLICIT;
				add->args[0].arg = 0;

				_essl_liveness_remove_range(ctx, mul_range);
			}
		}
	}
}

static void allocate_all_mul_add_channels(liveness_context *ctx)
{
	int block_i;
	for (block_i = (int)ctx->cfg->n_blocks-1 ; block_i >= 0 ; block_i--) {
		m200_instruction_word *word;
		for (word = ctx->cfg->output_sequence[block_i]->earliest_instruction_word ; word != 0 ; word = word->successor) {
			allocate_mul_add_channel(ctx, word->mul4, word->add4);
			allocate_mul_add_channel(ctx, word->mul1, word->add1);
		}
	}
}

/* Swap the inputs of the instruction in the add unit if it is symmetric and the second input
 * is the output from the instruction in the mul unit.
 */
static void swap_mul_add_inputs(m200_instruction *mul, m200_instruction *add)
{
	if (mul && add && mul->instr_node &&
		add->args[0].arg != mul->instr_node &&
		add->args[1].arg == mul->instr_node &&
		_essl_mali200_opcode_is_symmetric(add->opcode))
	{
		m200_input_argument temp = add->args[1];
		add->args[1] = add->args[0];
		add->args[0] = temp;
	}
}

static memerr mark_instruction_sequence_defs_and_uses(liveness_context *ctx, basic_block *block, void *dummy) {
	m200_instruction_word *word;
	IGNORE_PARAM(dummy);
	for (word = (m200_instruction_word *)block->latest_instruction_word ; word != 0 ; word = word->predecessor) {
		/* Init range point to end of the cycle */
		int position = END_OF_CYCLE(word->cycle);

		swap_mul_add_inputs(word->mul4, word->add4);
		swap_mul_add_inputs(word->mul1, word->add1);

		if (word->load) {
			rewrite_pseudo_register_uses_in_word(word, &word->load->instr_node, M200_HASH_LOAD);
		}
		if (word->tex) {
			rewrite_pseudo_register_uses_in_word(word, &word->tex->instr_node, M200_HASH_TEX);
		}

		/* Mark branch use positions at the very end of the word, since these uses are
		   moved when an instruction word is inserted after this word and should therefore
		   overlap everything else in the word. */
		ESSL_CHECK(mark_instruction_uses(ctx, word->branch, position));

		position++;

		/* End of subcycle 0 */
		ESSL_CHECK(mark_instruction_defs(ctx, word->branch, position));
		ESSL_CHECK(mark_instruction_defs(ctx, word->store, position));
		ESSL_CHECK(mark_instruction_defs(ctx, word->lut, position));

		position++;

		/* Start of subcycle 0 */
		ESSL_CHECK(mark_instruction_uses(ctx, word->store, position));
		ESSL_CHECK(mark_instruction_uses(ctx, word->lut, position));

		position++;

		/* End of subcycle 1 */
		ESSL_CHECK(mark_instruction_defs(ctx, word->add1, position));
		ESSL_CHECK(mark_instruction_defs(ctx, word->add4, position));

		position++;

		/* Start of subcycle 1 */
		ESSL_CHECK(mark_instruction_uses(ctx, word->add1, position));
		ESSL_CHECK(mark_instruction_uses(ctx, word->add4, position));

		position++;

		/* End of subcycle 2 */
		ESSL_CHECK(mark_instruction_defs(ctx, word->mul1, position));
		ESSL_CHECK(mark_instruction_defs(ctx, word->mul4, position));

		position++;

		/* Start of subcycle 2 */
		ESSL_CHECK(mark_instruction_uses(ctx, word->mul1, position));
		ESSL_CHECK(mark_instruction_uses(ctx, word->mul4, position));

		position++;

		/* Mark var/tex/load inputs one position later than the outputs to
		   avoid allocating any inputs and outputs into the same register,
		   so that the var/tex/load subcycle can be safely re-executed. */
		ESSL_CHECK(mark_instruction_uses(ctx, word->load, position));
		ESSL_CHECK(mark_instruction_uses(ctx, word->tex, position));
		ESSL_CHECK(mark_instruction_uses(ctx, word->var, position));

		position++;

		/* Start of subcycle 3 */
		ESSL_CHECK(mark_instruction_defs(ctx, word->load, position));
		ESSL_CHECK(mark_instruction_defs(ctx, word->tex, position));
		ESSL_CHECK(mark_instruction_defs(ctx, word->var, position));

		/* Assert that the variable that receives the var output is not an input
		   to var, tex or load */
		if (word->var && word->var->instr_node) {
			assert(word->var->args[0].arg != word->var->instr_node);
			assert(!word->tex || word->tex->args[0].arg != word->var->instr_node);
			assert(!word->tex || word->tex->args[1].arg != word->var->instr_node);
			assert(!word->load || word->load->args[0].arg != word->var->instr_node);
			assert(!word->load || word->load->args[1].arg != word->var->instr_node);
			assert(!word->load || word->load->args[2].arg != word->var->instr_node);
		}
	}

	return MEM_OK;
}

/** Component mask for variable size */
static unsigned int mask_from_node(node *n) {
	return (1U << GET_NODE_VEC_SIZE(n))-1;
}

liveness_context *_essl_mali200_calculate_live_ranges(mempool *pool, control_flow_graph *cfg, target_descriptor *desc,
													  error_context *err) {
	liveness_context *ctx;
	int block_i;

	ESSL_CHECK(ctx = _essl_liveness_create_context(pool, cfg, desc, mark_instruction_sequence_defs_and_uses, 0, mask_from_node, err));
	ESSL_CHECK(_essl_liveness_calculate_live_ranges(ctx));
	allocate_all_mul_add_channels(ctx);

#ifndef NDEBUG
	/* DEBUG: Check that all variables have been assigned ranges */
	for (block_i = (int)cfg->n_blocks-1 ; block_i >= 0 ; block_i--) {
		m200_instruction_word *word;
		for (word = cfg->output_sequence[block_i]->earliest_instruction_word ; word != 0 ; word = word->successor) {
			m200_instruction **instp = M200_FIRST_INSTRUCTION_IN_WORD(word);
			int j;
			for (j = 0 ; j < M200_N_INSTRUCTIONS_IN_WORD ; j++) {
				m200_instruction *inst = instp[j];
				if (inst) {
					int k;
					assert(inst->instr_node == 0 || _essl_ptrdict_lookup(&ctx->var_to_range, inst->instr_node));
					for (k = 0 ; k < M200_MAX_INPUT_ARGS ; k++) {
						assert(inst->args[k].arg == 0 || _essl_ptrdict_lookup(&ctx->var_to_range, inst->args[k].arg));
					}
				}
			}
		}
	}

	/* DEBUG: Check that all var refs point to non-null var fields */
	{
		live_range *range;
		for (range = ctx->var_ranges ; range != 0 ; range = range->next)
		{
			live_delimiter *delim;
			for (delim = range->points ; delim != 0 ; delim = delim->next)
			{
				assert(delim->var_ref == 0 || *delim->var_ref != 0);
			}
		}
	}
#endif

	/* Fix live ranges of instructions without write mask */
	{
		ptrset var_refs;
		ESSL_CHECK(_essl_ptrset_init(&var_refs, pool));

		for (block_i = (int)cfg->n_blocks-1 ; block_i >= 0 ; block_i--)
		{
			m200_instruction_word *word;
			for (word = cfg->output_sequence[block_i]->earliest_instruction_word ; word != 0 ; word = word->successor)
			{
				if (word->store && (word->store->opcode == M200_LD_RGB || word->store->opcode == M200_LD_ZS || word->store->opcode == M200_ST_SUBREG))
				{
					ESSL_CHECK(_essl_ptrset_insert(&var_refs, &word->store->instr_node));
				}
			}
		}

		ESSL_CHECK(_essl_liveness_fix_dead_definitions(pool, ctx->var_ranges, &var_refs));
	}

	return ctx;
}
