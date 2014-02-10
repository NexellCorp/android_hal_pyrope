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
#include "mali200/mali200_slot.h"
#include "backend/abstract_scheduler.h"
#include "mali200/mali200_instruction.h"

/** Concretize any pseudo-slots in among the wanted slots to fit in the available slots.
 *  Return 0 if the slots are not available
 */

typedef unsigned int m200_internal_schedule_classes;
#define M200_SC_SCALAR_ALU (1<<0) /* Any MUL or ADD */
#define M200_SC_SCALAR_MUL (1<<1) /* Either MUL4, MUL1 */
#define M200_SC_SCALAR_MUL_LUT (1<<2) /* Either MUL4, MUL1 or LUT */
#define M200_SC_SCALAR_ADD (1<<3) /* Either ADD4 or ADD1 */
#define M200_SC_SCALAR_MOV (1<<4) /* Any unit except LOAD, VAR, TEX and BRANCH */
#define M200_SC_VECTOR_MOV (1<<5) /* Either  MUL4, ADD4 or LUT */
#define M200_SC_SCALAR_MOV_WITH_MODIFIERS (1<<6) /* Any unit except LOAD, VAR, TEX, BRANCH and LUT*/
#define M200_SC_VECTOR_MOV_WITH_MODIFIERS (1<<7) /* Either MUL4 or ADD4 */
#define M200_SC_SCALAR_MOV_IF_NOT_SAME_WORD (1<<8) /* Transfer result in #tex or #load to register if needed */
#define M200_SC_VECTOR_MOV_IF_NOT_SAME_WORD (1<<9) /* Transfer result in #tex or #load to register if needed */

essl_bool _essl_mali200_is_coalescing_candidate(node *a)
{
	if(a->hdr.kind == EXPR_KIND_LOAD) return ESSL_TRUE;
	if(a->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL &&
	   (a->expr.operation == EXPR_OP_FUN_M200_POINT ||
		a->expr.operation == EXPR_OP_FUN_M200_POS ||
		a->expr.operation == EXPR_OP_FUN_M200_MISC_VAL))
	{
		return ESSL_TRUE;
	}
	
	return ESSL_FALSE;
}


static essl_bool can_be_replaced_by(node *a, m200_instruction *instr)
{
	node *b;
	unsigned i, n;
	node_extra *a_ex;
	node_extra *b_ex;
	if(instr == NULL) return ESSL_FALSE;
	b = instr->instr_node;
	if(instr->opcode != M200_VAR && instr->opcode != M200_POS && instr->opcode != M200_POINT && instr->opcode != M200_MISC_VAL
	   && instr->opcode != M200_LD_UNIFORM && instr->opcode != M200_LD_STACK && instr->opcode != M200_LD_REL)
	{
		return ESSL_FALSE;
	}

	if(a == NULL || b == NULL) return ESSL_FALSE;
	a_ex = EXTRA_INFO(a);
	b_ex = EXTRA_INFO(b);
	if(a_ex == NULL || b_ex == NULL) return ESSL_FALSE;

	if(a == b) return ESSL_TRUE;
	if(a->hdr.kind != b->hdr.kind) return ESSL_FALSE;
	if(a->hdr.kind == EXPR_KIND_UNARY || a->hdr.kind == EXPR_KIND_BINARY || a->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL)
	{
		if(a->expr.operation != b->expr.operation) return ESSL_FALSE;
	}
	if(GET_N_CHILDREN(a) != GET_N_CHILDREN(b)) return ESSL_FALSE;
	n = GET_N_CHILDREN(a);
	for(i = 0; i < n; ++i)
	{
		node *a_child = GET_CHILD(a, i);
		node *b_child = GET_CHILD(b, i);

		if(a_child != NULL && b_child != NULL && a_child != b_child)
		{
			return ESSL_FALSE;
		}else if(a_child != NULL && b_child == NULL)
		{
			return ESSL_FALSE;
		}else if(a_child == NULL && b_child != NULL)
		{
			return ESSL_FALSE;
		}
	}

	if(!_essl_mali200_output_modifiers_can_be_coalesced(&a_ex->u.m200_modifiers, &b_ex->u.m200_modifiers)) return ESSL_FALSE;
	
	if(!_essl_mali200_is_coalescing_candidate(a)) return ESSL_FALSE;

	if(a->hdr.kind == EXPR_KIND_LOAD)
	{
		node_extra *na, *nb;
		ESSL_CHECK(na = EXTRA_INFO(a));
		ESSL_CHECK(nb = EXTRA_INFO(b));
		if(!_essl_address_symbol_lists_equal(na->address_symbols, nb->address_symbols) || na->address_offset != nb->address_offset)
		{
			return ESSL_FALSE;
		}

	}

	return ESSL_TRUE;
}

static essl_bool instr_has_mov_for_pseudo_register(m200_instruction *inst, int pseudo_reg)
{
	if(inst == NULL) return ESSL_FALSE;
	if(inst->opcode != M200_MOV) return ESSL_FALSE;
	if(inst->args[0].reg_index != pseudo_reg) return ESSL_FALSE;
	return ESSL_TRUE;
}

static essl_bool word_has_mov_for_pseudo_register(m200_instruction_word *word, int pseudo_reg)
{
	if(instr_has_mov_for_pseudo_register(word->mul4, pseudo_reg)) return ESSL_TRUE;
	if(instr_has_mov_for_pseudo_register(word->mul1, pseudo_reg)) return ESSL_TRUE;
	if(instr_has_mov_for_pseudo_register(word->add4, pseudo_reg)) return ESSL_TRUE;
	if(instr_has_mov_for_pseudo_register(word->add1, pseudo_reg)) return ESSL_TRUE;
	if(instr_has_mov_for_pseudo_register(word->lut, pseudo_reg)) return ESSL_TRUE;
	if(instr_has_mov_for_pseudo_register(word->store, pseudo_reg)) return ESSL_TRUE;
	return ESSL_FALSE;
}


m200_schedule_classes _essl_mali200_allocate_slots(m200_instruction_word *word, m200_schedule_classes wanted, m200_schedule_classes used, m200_schedule_classes subcycles_unavailable_mask, node *load_op, int same_cycle, essl_bool is_vector) 
{
	m200_schedule_classes slots = wanted;
	m200_internal_schedule_classes internal_slots = 0;

	float one = 1.0f;
	swizzle_pattern dummy_swizzle;
	int dummy_reg;
	used |= subcycles_unavailable_mask;
	/* Assert that at most one pseudo class is specified */
	assert (((slots & M200_SC_PSEUDO_MASK) & ((slots & M200_SC_PSEUDO_MASK)-1)) == 0);

	/* Check that rendezvous requirement is satisfied */
	if (((slots & M200_SC_RENDEZVOUS) && (used & M200_SC_NO_RENDEZVOUS)) ||
		((slots & M200_SC_NO_RENDEZVOUS) && (used & M200_SC_RENDEZVOUS))) {
		return 0;
	}

	/* Expand vector size variable classes */
	if(slots & M200_SC_ALU)
	{
		if(is_vector)
		{
			slots |= M200_SC_VECTOR_ALU;
		} else {
			internal_slots |= M200_SC_SCALAR_ALU;
		}
		slots &= ~M200_SC_ALU;
	} else if(slots & M200_SC_MUL)
	{
		if(is_vector)
		{
			slots |= M200_SC_MUL4;
		} else {
			internal_slots |= M200_SC_SCALAR_MUL;
		}
		slots &= ~M200_SC_MUL;
	} else if(slots & M200_SC_MUL_LUT)
	{
		if(is_vector)
		{
			slots |= M200_SC_MUL4;
		} else {
			internal_slots |= M200_SC_SCALAR_MUL_LUT;
		}
		slots &= ~M200_SC_MUL_LUT;
	} else if(slots & M200_SC_ADD)
	{
		if(is_vector)
		{
			slots |= M200_SC_ADD4;
		} else {
			internal_slots |= M200_SC_SCALAR_ADD;
		}
		slots &= ~M200_SC_ADD;
	} else if(slots & M200_SC_MOV)
	{
		if(is_vector)
		{
			internal_slots |= M200_SC_VECTOR_MOV;
		} else {
			internal_slots |= M200_SC_SCALAR_MOV;
		}
		slots &= ~M200_SC_MOV;
	} else if(slots & M200_SC_MOV_WITH_MODIFIERS)
	{
		if(is_vector)
		{
			internal_slots |= M200_SC_VECTOR_MOV_WITH_MODIFIERS;
		} else {
			internal_slots |= M200_SC_SCALAR_MOV_WITH_MODIFIERS;
		}
		slots &= ~M200_SC_MOV_WITH_MODIFIERS;
	} else if(slots & M200_SC_MOV_IF_NOT_SAME_WORD)
	{
		if(is_vector)
		{
			internal_slots |= M200_SC_VECTOR_MOV_IF_NOT_SAME_WORD;
		} else {
			internal_slots |= M200_SC_SCALAR_MOV_IF_NOT_SAME_WORD;
		}
		slots &= ~M200_SC_MOV_IF_NOT_SAME_WORD;
	}

	/* try coalescing */


	if((slots & M200_SC_VAR) && word->var != NULL && (subcycles_unavailable_mask & M200_SC_VAR) == 0)
	{
		if(can_be_replaced_by(load_op, word->var))
		{
			slots &= ~M200_SC_VAR;
			slots |= M200_SC_SKIP_VAR;
		}
	}

	if((slots & M200_SC_LOAD) && word->load != NULL && (subcycles_unavailable_mask & M200_SC_LOAD) == 0)
	{
		if(can_be_replaced_by(load_op, word->load))
		{
			slots &= ~M200_SC_LOAD;
			slots |= M200_SC_SKIP_LOAD;
			if(!same_cycle && (internal_slots & (M200_SC_SCALAR_MOV_IF_NOT_SAME_WORD|M200_SC_VECTOR_MOV_IF_NOT_SAME_WORD)))
			{
				/* okay, mov needed to preserve the value. see if we have an existing mov */
				if(word_has_mov_for_pseudo_register(word, M200_HASH_LOAD))
				{
					slots |= M200_SC_SKIP_MOV;
					internal_slots &= ~(M200_SC_SCALAR_MOV_IF_NOT_SAME_WORD|M200_SC_VECTOR_MOV_IF_NOT_SAME_WORD);
				}

			}
		}
	}



	/* Check concrete classes */
	if ((slots & used & M200_SC_CONCRETE_MASK) != 0) {
		return 0;
	}



	/* Transform #var/#tex moves */
	if (internal_slots & M200_SC_SCALAR_MOV_IF_NOT_SAME_WORD) {
		if (same_cycle) {
			slots |= M200_SC_SKIP_MOV;
		} else {
			slots |= M200_SC_DONT_SCHEDULE_MOV;
			if ((used & M200_SC_ADD1) == 0) {
				slots |= M200_SC_ADD1;
			} else if ((used & M200_SC_MUL1) == 0) {
				slots |= M200_SC_MUL1;
			} else if ((used & M200_SC_ADD4) == 0) {
				slots |= M200_SC_ADD4;
			} else if ((used & M200_SC_MUL4) == 0) {
				slots |= M200_SC_MUL4;
			} else if ((used & (M200_SC_STORE | M200_SC_LSB_VECTOR_INPUT)) == 0) {
				slots |= M200_SC_STORE | M200_SC_LSB_VECTOR_INPUT; 
			} else if ((used & (M200_SC_LUT | M200_SC_LSB_VECTOR_INPUT)) == 0 &&
					   _essl_mali200_fit_float_constants(word, &one, 1, ESSL_TRUE, &dummy_swizzle, &dummy_reg)) {
				slots |= M200_SC_LUT | M200_SC_LSB_VECTOR_INPUT;
			} else {
				return 0;
			}

		}
		internal_slots &= ~M200_SC_SCALAR_MOV_IF_NOT_SAME_WORD;
	} else if (internal_slots & M200_SC_VECTOR_MOV_IF_NOT_SAME_WORD) {
		if (same_cycle) {
			slots |= M200_SC_SKIP_MOV;
		} else {
			slots |= M200_SC_DONT_SCHEDULE_MOV;
			if ((used & M200_SC_ADD4) == 0) {
				slots |= M200_SC_ADD4;
			} else if ((used & M200_SC_MUL4) == 0) {
				slots |= M200_SC_MUL4;
			} else if ((used & (M200_SC_LUT | M200_SC_LSB_VECTOR_INPUT)) == 0 &&
					   _essl_mali200_fit_float_constants(word, &one, 1, ESSL_TRUE, &dummy_swizzle, &dummy_reg)) {
				slots |= M200_SC_LUT | M200_SC_LSB_VECTOR_INPUT;

				/* vector store does not have a write mask, therefore we disable it */
/*
			} else if ((used & (M200_SC_STORE | M200_SC_LSB_VECTOR_INPUT)) == 0) {
				slots |= M200_SC_STORE | M200_SC_LSB_VECTOR_INPUT; 
*/
			} else {
				return 0;
			}


		}
		internal_slots &= ~M200_SC_VECTOR_MOV_IF_NOT_SAME_WORD;
	}

	/* Check pseudo classes */
	if (internal_slots & M200_SC_SCALAR_ALU) {
		if ((used & M200_SC_ADD1) == 0) {
			slots |= M200_SC_ADD1;
		} else if ((used & M200_SC_MUL1) == 0) {
			slots |= M200_SC_MUL1;
		} else if ((used & M200_SC_ADD4) == 0) {
			slots |= M200_SC_ADD4;
		} else if ((used & M200_SC_MUL4) == 0) {
			slots |= M200_SC_MUL4;
		} else {
			return 0;
		}
		internal_slots &= ~M200_SC_SCALAR_ALU;
	} else if (slots & M200_SC_VECTOR_ALU) {
		if ((used & M200_SC_ADD4) == 0) {
			slots |= M200_SC_ADD4;
		} else if ((used & M200_SC_MUL4) == 0) {
			slots |= M200_SC_MUL4;
		} else {
			return 0;
		}
		slots &= ~M200_SC_VECTOR_ALU;
	} else if (slots & M200_SC_SCALAR_VECTOR_MUL_LUT) {
		if ((used & (M200_SC_LUT | M200_SC_LSB_VECTOR_INPUT)) == 0) {
			slots |= M200_SC_LUT | M200_SC_LSB_VECTOR_INPUT;
		} else if ((used & M200_SC_MUL4) == 0) {
			slots |= M200_SC_MUL4;
		} else {
			return 0;
		}
		slots &= ~M200_SC_SCALAR_VECTOR_MUL_LUT;
	} else if (internal_slots & M200_SC_SCALAR_MUL) {
		if ((used & M200_SC_MUL1) == 0) {
			slots |= M200_SC_MUL1;
		} else if ((used & M200_SC_MUL4) == 0) {
			slots |= M200_SC_MUL4;
		} else {
			return 0;
		}
		internal_slots &= ~M200_SC_SCALAR_MUL;
	} else if (internal_slots & M200_SC_SCALAR_MUL_LUT) {
		if ((used & M200_SC_MUL1) == 0) {
			slots |= M200_SC_MUL1;
		} else if ((used & (M200_SC_LUT | M200_SC_LSB_VECTOR_INPUT)) == 0) {
			slots |= M200_SC_LUT | M200_SC_LSB_VECTOR_INPUT;
		} else if ((used & M200_SC_MUL4) == 0) {
			slots |= M200_SC_MUL4;
		} else {
			return 0;
		}
		internal_slots &= ~M200_SC_SCALAR_MUL_LUT;
	} else if (internal_slots & M200_SC_SCALAR_ADD) {
		if ((used & M200_SC_ADD1) == 0) {
			slots |= M200_SC_ADD1;
		} else if ((used & M200_SC_ADD4) == 0) {
			slots |= M200_SC_ADD4;
		} else {
			return 0;
		}
		internal_slots &= ~M200_SC_SCALAR_ADD;
	} else if (internal_slots & M200_SC_SCALAR_MOV) {
		if ((used & M200_SC_ADD1) == 0) {
			slots |= M200_SC_ADD1;
		} else if ((used & M200_SC_MUL1) == 0) {
			slots |= M200_SC_MUL1;
		} else if ((used & M200_SC_ADD4) == 0) {
			slots |= M200_SC_ADD4;
		} else if ((used & M200_SC_MUL4) == 0) {
			slots |= M200_SC_MUL4;
		} else if ((used & (M200_SC_LUT | M200_SC_LSB_VECTOR_INPUT)) == 0 &&
				   _essl_mali200_fit_float_constants(word, &one, 1, ESSL_TRUE, &dummy_swizzle, &dummy_reg)) {
			slots |= M200_SC_LUT | M200_SC_LSB_VECTOR_INPUT;
		} else {
			return 0;
		}
		internal_slots &= ~M200_SC_SCALAR_MOV;
	} else if (internal_slots & M200_SC_VECTOR_MOV) {
		if ((used & M200_SC_ADD4) == 0) {
			slots |= M200_SC_ADD4;
		} else if ((used & M200_SC_MUL4) == 0) {
			slots |= M200_SC_MUL4;
		} else if ((used & (M200_SC_LUT | M200_SC_LSB_VECTOR_INPUT)) == 0 &&
				   _essl_mali200_fit_float_constants(word, &one, 1, ESSL_TRUE, &dummy_swizzle, &dummy_reg)) {
			slots |= M200_SC_LUT | M200_SC_LSB_VECTOR_INPUT;
		} else {
			return 0;
		}
		internal_slots &= ~M200_SC_VECTOR_MOV;
	} else if (internal_slots & M200_SC_SCALAR_MOV_WITH_MODIFIERS) {
		/* keep in mind: scalar moves need abs, negate on input, and full set of modifiers on output */
		if ((used & M200_SC_ADD1) == 0) {
			slots |= M200_SC_ADD1;
		} else if ((used & M200_SC_MUL1) == 0) {
			slots |= M200_SC_MUL1;
		} else if ((used & M200_SC_ADD4) == 0) {
			slots |= M200_SC_ADD4;
		} else if ((used & M200_SC_MUL4) == 0) {
			slots |= M200_SC_MUL4;
		} else {
			return 0;
		}
		internal_slots &= ~M200_SC_SCALAR_MOV_WITH_MODIFIERS;
	} else if (internal_slots & M200_SC_VECTOR_MOV_WITH_MODIFIERS) {
		/* keep in mind: vector moves need abs, negate, swizzle on input, and full set of modifiers on output */
		if ((used & M200_SC_ADD4) == 0) {
			slots |= M200_SC_ADD4;
		} else if ((used & M200_SC_MUL4) == 0) {
			slots |= M200_SC_MUL4;
		} else {
			return 0;
		}
		internal_slots &= ~M200_SC_VECTOR_MOV_WITH_MODIFIERS;
	}

	assert ((slots & ~(M200_SC_CONCRETE_MASK | M200_SC_SIGNAL_MASK)) == 0);
	assert(internal_slots == 0);
	return slots;
}

static m200_schedule_classes subcycle_use_mask(int rel_subcycle) {
	switch (rel_subcycle) {
	case 0:
		return M200_SC_SUBCYCLE_0_MASK;
	case 1:
		return M200_SC_SUBCYCLE_0_MASK | M200_SC_SUBCYCLE_1_MASK;
	case 2:
		return M200_SC_SUBCYCLE_0_MASK | M200_SC_SUBCYCLE_1_MASK | M200_SC_SUBCYCLE_2_MASK;
	case 3:
		return M200_SC_SUBCYCLE_0_MASK | M200_SC_SUBCYCLE_1_MASK | M200_SC_SUBCYCLE_2_MASK | M200_SC_SUBCYCLE_3_MASK;
	default:
		assert(0); /* Unexpected relative subcycle */
		return 0;
	}
}

m200_instruction_word *_essl_mali200_find_free_slots(mali200_scheduler_context *ctx, m200_schedule_classes *maskp, node *n, /*@null@*/ node *constant, node *load_op, essl_bool is_vector)
{
	int use = _essl_scheduler_get_earliest_use(n);
	int use_cycle = SUBCYCLE_TO_CYCLE(use);
	int latest_use = _essl_scheduler_get_latest_use(n);
	int latest_use_cycle = SUBCYCLE_TO_CYCLE(latest_use);
	m200_schedule_classes subcycle_mask = subcycle_use_mask(SUBCYCLE_TO_RELATIVE_SUBCYCLE(use));
	m200_instruction_word *word;

	/* if we have a load or a tex followed by a mov if not same word,
	 * we only need to care about the part of the subcycle_mask that's
	 * in subcycle 3, as this is the only place where loads and
	 * texture lookups can go. The mov if not same word can be placed
	 * later than the first use - the first use will just refer to the
	 * pseudo-register then.
	*/
	if((*maskp & ~(M200_SC_RENDEZVOUS | M200_SC_NO_RENDEZVOUS)) == (M200_SC_MOV_IF_NOT_SAME_WORD | M200_SC_DONT_SCHEDULE_MOV | M200_SC_LOAD) ||
		(*maskp & ~(M200_SC_RENDEZVOUS | M200_SC_NO_RENDEZVOUS)) == (M200_SC_MOV_IF_NOT_SAME_WORD | M200_SC_DONT_SCHEDULE_MOV | M200_SC_TEX | M200_SC_VAR))
	{
		subcycle_mask &= M200_SC_SUBCYCLE_3_MASK;
	}


	for (word = ctx->latest_instruction_word ; word != 0 ; word = word->predecessor) {
		m200_schedule_classes allocated;
		if (word->cycle >= use_cycle) {
			int same_cycle = word->cycle == latest_use_cycle;
			/* If the value produced is input to a branch instruction in the same cycle,
			   we need a move in any case, since the register spiller cannot handle
			   pseudo registers as inputs to a branch instruction. In this case,
			   we treat the latest use as not being in the same cycle, forcing a move if one
			   is needed for a latest use in a later cycle.
			*/
			if (word->branch &&
				(word->branch->args[0].arg == n ||
				 word->branch->args[1].arg == n ||
				 word->branch->args[2].arg == n))
			{
				/* Must place move, but not in subcycle 0 */
				same_cycle = ESSL_FALSE;
				subcycle_mask |= M200_SC_SUBCYCLE_0_MASK;
			}

			if(word->cycle > use_cycle)
			{
				subcycle_mask = 0;
			}
			if(constant)
			{
				/* check for constant space */
				if(!_essl_mali200_fit_constants(word, ctx->desc, constant, 0, 0)) continue;
			}
			if (word->cycle == use_cycle) {
				allocated = _essl_mali200_allocate_slots(word, *maskp, word->used_slots, subcycle_mask, load_op, same_cycle, is_vector);
			} else {
				allocated = _essl_mali200_allocate_slots(word, *maskp, word->used_slots, 0, load_op, same_cycle, is_vector);
			}

			if (allocated != 0) {
				/* Slots are free */
				*maskp = allocated;
				return word;
			}
		}
	}
	return 0;
}


/* function to compare to floats even nans (bit-equal) */

static essl_bool float_equals(float a, float b) 
{
	union{
		float f;
		int i; } if1, if2;
	if1.f = a;
	if2.f = b;
	return (essl_bool)(if1.i == if2.i);
} 



essl_bool _essl_mali200_fit_float_constants(m200_instruction_word *word, float *vals, unsigned n_vals, essl_bool is_shareable, /*@null@*/ swizzle_pattern *res_swizzle, /*@null@*/ int *res_reg)
{
	unsigned i, j, k;
	swizzle_pattern swz = _essl_create_undef_swizzle();
	for(k = 0; k < M200_N_CONSTANT_REGISTERS; ++k)
	{
		unsigned n_entries = word->n_embedded_entries[k];
		for(j = 0; j < n_vals; ++j)
		{

			if(is_shareable)
			{
				for(i = 0; i < n_entries; ++i)
				{
					if(word->is_embedded_constant_shareable[k][i] && float_equals(vals[j], word->embedded_constants[k][i]))
					{
						swz.indices[j] = (signed char)i;
						break;
					}
					
				}
			} else {
				i = n_entries;
			}
			if(i >= M200_NATIVE_VECTOR_SIZE) goto nextreg;

			if(i == n_entries)
			{
				swz.indices[j] = (signed char)i;
				word->is_embedded_constant_shareable[k][i] = is_shareable;
				word->embedded_constants[k][i] = vals[j];
				++n_entries;
			}
		}
		for(i = n_vals; i < N_COMPONENTS; ++i)
		{
			swz.indices[i] = -1;
		}

		if(res_swizzle && res_reg)
		{
			/* only update the constants when they're actually wanted */
			word->n_embedded_entries[k] = n_entries;
			*res_reg = M200_CONSTANT0 + k;
			*res_swizzle = swz; 
		}
		return ESSL_TRUE;

	nextreg:
		continue;
	}
	
	return ESSL_FALSE;
}

essl_bool _essl_mali200_fit_constants(m200_instruction_word *word, target_descriptor *desc, node *constant, /*@null@*/ swizzle_pattern *res_swizzle, /*@null@*/ int *res_reg)
{
	/* placeholder values for non-shareable values */
	unsigned i;
	essl_bool is_shareable = ESSL_FALSE;
	float vals[M200_NATIVE_VECTOR_SIZE] = {0.0, 0.0, 0.0, 0.0};
	unsigned n_vals = 1;
	if(constant->hdr.kind == EXPR_KIND_CONSTANT)
	{
		is_shareable = ESSL_TRUE;
		assert(constant->hdr.type->basic_type != TYPE_MATRIX_OF);
		n_vals = GET_NODE_VEC_SIZE(constant);
		for(i = 0; i < n_vals; ++i)
		{
			vals[i] = desc->scalar_to_float(constant->expr.u.value[i]);
		}
		
	}
	return _essl_mali200_fit_float_constants(word, vals, n_vals, is_shareable, res_swizzle, res_reg);}

