/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "common/essl_symbol.h"
#include "backend/liveness.h"
#include "mali200/mali200_instruction.h"
#include "mali200/mali200_word_insertion.h"


static swizzle_pattern scalar_swizzle(unsigned int size) {
	swizzle_pattern swz;
	int i;
	for (i = 0 ; i < N_COMPONENTS ; i++) {
		swz.indices[i] = i < (int)size ? 0 : -1;
	}
	return swz;
}

static void copy_embedded_constants(m200_instruction_word *dst, m200_instruction_word *src) {
	int i,j;
	for (i = 0 ; i < M200_N_CONSTANT_REGISTERS ; i++) {
		dst->n_embedded_entries[i] = src->n_embedded_entries[i];
		for (j = 0 ; j < M200_NATIVE_VECTOR_SIZE ; j++) {
			dst->embedded_constants[i][j] = src->embedded_constants[i][j];
			dst->is_embedded_constant_shareable[i][j] = src->is_embedded_constant_shareable[i][j];
		}
	}
}

static node *make_variable(regalloc_context *ctx, int size) {
	node *var;
	type_specifier *t;
	ESSL_CHECK(var = _essl_new_unary_expression(ctx->pool, EXPR_OP_IDENTITY, 0));
	ESSL_CHECK(t = _essl_new_type(ctx->pool));
	t->basic_type = TYPE_FLOAT;
	t->u.basic.vec_size = size;
	var->hdr.type = t;
	ESSL_CHECK(CREATE_EXTRA_INFO(ctx->pool, var));
	return var;
}

static void rewrite_pseudo_register_uses(m200_instruction *inst, node *tex_var, node *load_var,
										 int *tex_count, int *load_count) {
	if (inst)
	{
		int i;
		for (i = 0 ; i < M200_MAX_INPUT_ARGS ; i++) {
			if (inst->args[i].arg == 0) {
				switch (inst->args[i].reg_index) {
				case M200_HASH_TEX:
					inst->args[i].arg = tex_var;
					inst->args[i].reg_index = M200_REG_UNKNOWN;
					(*tex_count)++;
					break;
				case M200_HASH_LOAD:
					inst->args[i].arg = load_var;
					inst->args[i].reg_index = M200_REG_UNKNOWN;
					(*load_count)++;
					break;
				default:
					break;
				}
			}
		}
	}
}

static m200_instruction *make_move(regalloc_context *ctx, m200_schedule_classes sc, int subcycle, node *var, int src_reg) {
	m200_instruction *inst;
	int size = GET_NODE_VEC_SIZE(var);
	ESSL_CHECK(inst = _essl_new_mali200_instruction(ctx->pool, sc, M200_MOV, subcycle));
	inst->args[0].reg_index = src_reg;
	inst->args[0].swizzle = _essl_create_identity_swizzle(size);
	inst->instr_node = var;
	inst->output_swizzle = _essl_create_identity_swizzle(size);
	return inst;
}

static m200_instruction *make_one(regalloc_context *ctx, m200_schedule_classes sc, int subcycle, node *var) {
	m200_instruction *inst;
	int size = GET_NODE_VEC_SIZE(var);
	ESSL_CHECK(inst = _essl_new_mali200_instruction(ctx->pool, sc, M200_CMP, subcycle));
	inst->compare_function = M200_CMP_EQ;
	inst->args[0].reg_index = M200_CONSTANT0;
	inst->args[0].swizzle = _essl_create_identity_swizzle(size);
	inst->args[1].reg_index = M200_CONSTANT0;
	inst->args[1].swizzle = _essl_create_identity_swizzle(size);
	inst->instr_node = var;
	inst->output_swizzle = _essl_create_identity_swizzle(size);
	return inst;
}

static m200_instruction *make_mul(regalloc_context *ctx, m200_schedule_classes sc, int subcycle, node *var, node *scalar, int vec_reg) {
	m200_instruction *inst;
	int size = GET_NODE_VEC_SIZE(var);
	ESSL_CHECK(inst = _essl_new_mali200_instruction(ctx->pool, sc, M200_MUL, subcycle));
	inst->args[0].arg = scalar;
	inst->args[0].swizzle = scalar_swizzle(size);
	inst->args[1].reg_index = vec_reg;
	inst->args[1].swizzle = _essl_create_identity_swizzle(size);
	inst->instr_node = var;
	inst->output_swizzle = _essl_create_identity_swizzle(size);
	return inst;
}

memerr _essl_mali200_split_word(regalloc_context *ctx, m200_instruction_word *word, basic_block *block) {
	m200_instruction_word *word1,*word2;
	node *tex_var, *load_var, *mul4_var;
	int tex_count = 0;
	int load_count = 0;
	ESSL_CHECK(word1 = _essl_mali200_insert_word_before(ctx->liv_ctx, word, block));
	ESSL_CHECK(word2 = _essl_mali200_insert_word_before(ctx->liv_ctx, word1, block));

	/* Move instructions up */
#define MOVE(inst, wnum) \
    if (word->inst != 0) \
    { \
        word->inst->subcycle += wnum*M200_SUBCYCLES_PER_CYCLE; \
        word##wnum->inst = word->inst; \
        word->inst = 0; \
    }
	MOVE(var,2);
	MOVE(tex,2);
	MOVE(load,2);
	MOVE(mul4,2);
	MOVE(mul1,1);
	MOVE(add4,1);
	MOVE(add1,1);
#undef MOVE

	/* Copy embedded constants */
	copy_embedded_constants(word1, word);
	copy_embedded_constants(word2, word);

	/* Update slot masks */
	word2->used_slots = word->used_slots & (M200_SC_VAR | M200_SC_TEX | M200_SC_LOAD | M200_SC_MUL4);
	word1->used_slots = word->used_slots & (M200_SC_MUL1 | M200_SC_ADD4 | M200_SC_ADD1);
	word->used_slots = word->used_slots & (M200_SC_LUT | M200_SC_STORE | M200_SC_BRANCH | M200_SC_LSB_VECTOR_INPUT);

	/* Create variables for word-local registers */
	ESSL_CHECK(tex_var = make_variable(ctx, 4));
	ESSL_CHECK(load_var = make_variable(ctx, 4));
	ESSL_CHECK(mul4_var = make_variable(ctx, 4));

	/* Rewrite uses of #tex and #load into these variables */
	rewrite_pseudo_register_uses(word1->mul1, tex_var, load_var, &tex_count, &load_count);
	rewrite_pseudo_register_uses(word1->add4, tex_var, load_var, &tex_count, &load_count);
	rewrite_pseudo_register_uses(word1->add1, tex_var, load_var, &tex_count, &load_count);
	rewrite_pseudo_register_uses(word->lut, tex_var, load_var, &tex_count, &load_count);
	rewrite_pseudo_register_uses(word->store, tex_var, load_var, &tex_count, &load_count);
	rewrite_pseudo_register_uses(word->branch, tex_var, load_var, &tex_count, &load_count);

	if (word2->tex && tex_count > 0)
	{
		/* Place MOV of tex result in add4 */
		ESSL_CHECK(word2->add4 = make_move(ctx, M200_SC_ADD4, CYCLE_TO_SUBCYCLE(word2->cycle,1), tex_var, M200_HASH_TEX));
		word2->used_slots |= M200_SC_ADD4;
	}

	if (word2->load && load_count > 0)
	{
		/* Place MUL of load result by one in lut */
		node *one_var;
		ESSL_CHECK(one_var = make_variable(ctx, 1));
		ESSL_CHECK(word2->add1 = make_one(ctx, M200_SC_ADD1, CYCLE_TO_SUBCYCLE(word2->cycle,1), one_var));
		ESSL_CHECK(word2->lut = make_mul(ctx, M200_SC_LUT, CYCLE_TO_SUBCYCLE(word2->cycle,0), load_var, one_var, M200_HASH_LOAD));
		word2->used_slots |= M200_SC_LUT | M200_SC_LSB_VECTOR_INPUT;
	}

	if (word2->mul4 && !word2->mul4->instr_node && word2->mul4->out_reg == M200_R_IMPLICIT &&
		word1->add4 && !word1->add4->args[0].arg && word1->add4->args[0].reg_index == M200_R_IMPLICIT)
	{
		/* Get result from mul4 to add4 */
		word2->mul4->instr_node = mul4_var;
		word2->mul4->out_reg = M200_REG_UNKNOWN;
		word1->add4->args[0].arg = mul4_var;
		word1->add4->args[0].reg_index = M200_REG_UNKNOWN;
	}

	return MEM_OK;
}
