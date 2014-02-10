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
#include "backend/abstract_scheduler.h"
#include "mali200/mali200_instruction.h"
#include "mali200/mali200_scheduler.h"
#include "mali200/mali200_slot.h"
#include "backend/extra_info.h"
#include "common/essl_random.h"

int _essl_mali200_op_weight(node *op)
{
#ifdef RANDOMIZED_COMPILATION
	return _essl_get_random_int(0, 100);
#else
	switch (op->hdr.kind) {
	case EXPR_KIND_PHI:
	case EXPR_KIND_DONT_CARE:
	case EXPR_KIND_TRANSFER:
	case EXPR_KIND_DEPEND:
		return 0;
	case EXPR_KIND_UNARY:
		switch(op->expr.operation)
		{
		case EXPR_OP_SWIZZLE:
			return 0;
		default:
			return 1;
		}
	case EXPR_KIND_BUILTIN_FUNCTION_CALL:
		switch(op->expr.operation)
		{
		case EXPR_OP_FUN_CLAMP:
		case EXPR_OP_FUN_ABS:
		case EXPR_OP_FUN_TRUNC:
			return 0;
		default:
			return 1;
		}

	default:
		return 1;
	}
#endif
}


/*

The first instruction should have the root node as output unless DONT_SCHEDULE_MOV is set 

*/

static m200_instruction_word *add_word(mali200_scheduler_context *ctx) 
{
	m200_instruction_word *word;
	if (ctx->earliest_cycle >= MAX_COMPILER_INSTRUCTION_WORDS)
	{
		REPORT_ERROR(ctx->error_context, ERR_RESOURCES_EXHAUSTED, 0, "Maximum number of compiler supported instructions (%d) exceeded.\n", MAX_COMPILER_INSTRUCTION_WORDS);
		return 0;
	}
	ESSL_CHECK(word = _essl_new_mali200_instruction_word(ctx->pool, ctx->earliest_cycle++));
	word->original_cycle = word->cycle;
	word->successor = ctx->earliest_instruction_word;
	if(ctx->earliest_instruction_word)
		{
			ctx->earliest_instruction_word->predecessor = word;
		} else {
			ctx->latest_instruction_word = word;
		}
	ctx->earliest_instruction_word = word;
	return word;
}


static essl_bool is_vector_op(node *n)
{
	unsigned i;
	unsigned n_writes = 0;
	node_extra *ne;

	ne = EXTRA_INFO(n);
	assert(ne != 0);
	if(!ne) 
	{
		return ESSL_TRUE; /* safe fallback */
	}

	if(ne->u.m200_modifiers.trans_node == NULL && _essl_get_type_size(n->hdr.type) == 1) 
	{
		return ESSL_FALSE;
	}

	for(i = 0; i < 4; ++i)
	{
		if(ne->u.m200_modifiers.swizzle.indices[i] != -1) 
		{
			++n_writes;
		}
	}
	return n_writes > 1;
}

static m200_comparison invert_comparison(m200_comparison op)
{
	return (m200_comparison)((~op) & 0x7);
}

static unsigned get_varying_flags(node *n)
{
	node_extra *ex;
	assert(n->hdr.kind == EXPR_KIND_LOAD);
	ESSL_CHECK(ex = EXTRA_INFO(n)); /* should not fail at all */
	assert(ex->address_symbols != NULL && ex->address_symbols->next == NULL);
	assert(ex->address_symbols->sym->address_space == ADDRESS_SPACE_FRAGMENT_VARYING);
	switch(ex->address_symbols->sym->qualifier.varying)
	{
	case VARYING_QUAL_NONE: 
		return 0;
	case VARYING_QUAL_FLAT: 
		return M200_VAR_FLATSHADING;
	case VARYING_QUAL_CENTROID: 
		return M200_VAR_CENTROID;
	}
	assert(0);
	return 0;
}


static memerr schedule(mali200_scheduler_context *ctx, node *root, /*@null@*/ node *constant, node *load_op, m200_schedule_classes mask, ...) 
{
	/* Space in available words? */
	essl_bool is_vector = root ? is_vector_op(root) : ESSL_FALSE;
	
	m200_instruction_word *word = 0;
	if(root)
	{
		word = _essl_mali200_find_free_slots(ctx, &mask, root, constant, load_op, is_vector);
	}
	if (!word) 
	{
		/* Allocate new word if allowed to */
		if (ctx->can_add_cycles) 
		{
			ESSL_CHECK(word = add_word(ctx));
			mask = _essl_mali200_allocate_slots(word, mask, 0, 0, load_op, 0, is_vector);
		} 
		else 
		{
			/* Don't schedule it yet */
			ESSL_CHECK(_essl_scheduler_postpone_operation(ctx->sctx, root));
			return MEM_OK;
		}
	}

	/* Write the instructions */
	{
		va_list arglist;
		va_start(arglist, mask);
		ESSL_CHECK_VA(arglist, _essl_mali200_write_instructions(ctx, word, mask, arglist));
		va_end(arglist);
		return MEM_OK;
	}
}


static m200_comparison expr_op_to_comparison(expression_operator op, essl_bool *inverted)
{
	*inverted = ESSL_FALSE;
	switch(op)
	{
	case EXPR_OP_LT:
	case EXPR_OP_FUN_LESSTHAN:
		*inverted = ESSL_TRUE;
		return M200_CMP_GT;

	case EXPR_OP_LE:
	case EXPR_OP_FUN_LESSTHANEQUAL:
		*inverted = ESSL_TRUE;
		return M200_CMP_GE;

	case EXPR_OP_EQ:
	case EXPR_OP_FUN_EQUAL:
		return M200_CMP_EQ;


	case EXPR_OP_NE:
	case EXPR_OP_FUN_NOTEQUAL:
		return M200_CMP_NE;

	case EXPR_OP_GE:
	case EXPR_OP_FUN_GREATERTHANEQUAL:
		return M200_CMP_GE;


	case EXPR_OP_GT:
	case EXPR_OP_FUN_GREATERTHAN:
		return M200_CMP_GT;

	default:
		assert(0);
		return M200_CMP_ALWAYS;

	}
}

static memerr make_node_schedulable(mali200_scheduler_context *ctx, node *n, int priority)
{
	node_extra *ex;
	ESSL_CHECK(ex = CREATE_EXTRA_INFO(ctx->pool, n));
	ex->original_use_count = ex->unscheduled_use_count = 1;
	ex->scheduled_use_count = 0;
	ex->operation_depth = priority;
	return MEM_OK;
}

/*@null@*/ static node *create_mov_node(mali200_scheduler_context *ctx, node *src)
{
	node *n;
	node_extra *srcex;
	ESSL_CHECK(n = _essl_new_unary_expression(ctx->pool, EXPR_OP_IDENTITY, src));
	_essl_ensure_compatible_node(n, src);
	ESSL_CHECK(srcex = EXTRA_INFO(src));
	ESSL_CHECK(make_node_schedulable(ctx, n, srcex->operation_depth + 1));
	return n;
}




/*@null@*/ static node *create_zero_node(mali200_scheduler_context *ctx) {
	node *n;
	type_specifier *type;
	ESSL_CHECK(n = _essl_new_constant_expression(ctx->pool, 1));
	n->expr.u.value[0] = ctx->desc->float_to_scalar(0.0f);

	ESSL_CHECK(type = _essl_new_type(ctx->pool));
	type->basic_type = TYPE_FLOAT;
	type->u.basic.vec_size = 1;
	n->hdr.type = type;
	ESSL_CHECK(make_node_schedulable(ctx, n, 1));
	return n;
}


static essl_bool is_scalar_to_scalar_swizzle(swizzle_pattern swz)
{
	unsigned i;
	int input_idx = -1;
	unsigned n_output_components = 0;
	for(i = 0; i < N_COMPONENTS; ++i)
	{
		if(swz.indices[i] != -1)
		{
			++n_output_components;
			if(input_idx == -1)
			{
				input_idx = swz.indices[i];
			} 
			else 
			{
				if(input_idx != swz.indices[i])
				{
					return ESSL_FALSE; /* is vector input */
				}
			}
		}
	}
	return n_output_components == 1;
}

/** does the input swizzle pattern go from scalar to vector? */
static essl_bool is_scalar_to_vector_swizzle(swizzle_pattern swz)
{
	unsigned i;
	int input_idx = -1;
	unsigned n_output_components = 0;
	for(i = 0; i < N_COMPONENTS; ++i)
	{
		if(swz.indices[i] != -1)
		{
			++n_output_components;
			if(input_idx == -1)
			{
				input_idx = swz.indices[i];
			} 
			else 
			{
				if(input_idx != swz.indices[i])
				{
					return ESSL_FALSE; /* is vector input */
				}
			}
		}
	}
	return n_output_components > 1;
}

static void output_modifier_init(m200_output_modifier_set *mods, node *n)
{
	unsigned vec_size = _essl_get_type_size(n->hdr.type);
	mods->exponent_adjust = 0;
	mods->swizzle = _essl_create_identity_swizzle(vec_size);
	mods->mode = M200_OUTPUT_NORMAL;
	mods->trans_node = n;
}

static essl_bool output_modifier_prepend_exponent_adjust(m200_output_modifier_set *mods, int exponent_adjust)
{
	if(exponent_adjust == 0) 
	{
		return ESSL_TRUE; /* identity */
	}
	if(mods->exponent_adjust != 0)
	{
		return ESSL_FALSE; /* can't do two of those, it isn't invariant */
	}

	/* otherwise we should be fine */
	mods->exponent_adjust += exponent_adjust;
	return ESSL_TRUE;
}

static essl_bool output_modifier_prepend_output_mode(m200_output_modifier_set *mods, m200_output_mode mode)
{
	if(mods->mode == mode || mode == M200_OUTPUT_NORMAL)
	{
		return ESSL_TRUE; /* identity */
	}
	if(mods->mode != M200_OUTPUT_NORMAL) /* replacing nothing with something probably okay  */
	{
		/* okay, we're combining two different modifiers. */
		
		/* trunc only goes with itself, so reject if one of them is trunk */
		if(mods->mode == M200_OUTPUT_TRUNCATE || mode == M200_OUTPUT_TRUNCATE)
		{
			return ESSL_FALSE;
		}

		/* only thing left now is clamp_0_1 combined with clamp_0_inf, and this results in a clamp_0_1 */
		assert(
			((mods->mode == M200_OUTPUT_CLAMP_0_1) ^ (mode == M200_OUTPUT_CLAMP_0_1)) &&
			((mods->mode == M200_OUTPUT_CLAMP_0_INF) ^ (mode == M200_OUTPUT_CLAMP_0_INF)));
		mode = M200_OUTPUT_CLAMP_0_1;
	}
	/* okay. the wanted output mode is in mode. see if it's compatible with the exponent adjust */
	if(mods->exponent_adjust != 0 && mode != M200_OUTPUT_CLAMP_0_INF) 
	{
		return ESSL_FALSE;
	}

	/* all set. */
	mods->mode = mode;
	return ESSL_TRUE;

}

static void output_modifier_prepend_swizzle(m200_output_modifier_set *mods, swizzle_pattern swz)
{
	mods->swizzle = _essl_combine_swizzles(mods->swizzle, swz);
}

static void output_modifier_prepend_mask(m200_output_modifier_set *mods, combine_pattern mask, unsigned active_component)
{
	mods->swizzle = _essl_combine_swizzles(mods->swizzle, _essl_create_swizzle_from_combiner(mask, active_component));
}

typedef enum {
	NO_IGNORE_USE_COUNT,
	IGNORE_USE_COUNT
} ignore_use_count;


/*@null@*/ static node *create_float_constant(mali200_scheduler_context *ctx, float value, unsigned vec_size)
{
	node *n;
	unsigned i;
	type_specifier *type;
	ESSL_CHECK(n = _essl_new_constant_expression(ctx->pool, vec_size));
	n->expr.u.value[0] = ctx->desc->float_to_scalar(value);
	for(i = 1; i < vec_size; ++i)
	{
		n->expr.u.value[i] = n->expr.u.value[0];
	}

	ESSL_CHECK(type = _essl_new_type(ctx->pool));
	type->basic_type = TYPE_FLOAT;
	type->u.basic.vec_size = vec_size;
	n->hdr.type = type;
	return n;
}

static unsigned live_mask_from_swizzle(swizzle_pattern swz)
{
	unsigned i;
	unsigned mask = 0;
	for(i = 0; i < N_COMPONENTS; ++i)
	{
		if(swz.indices[i] != -1)
		{
			mask |= 1<<swz.indices[i];
		}
	}
	return mask;
}

static node *process_modifier_helper(mali200_scheduler_context *ctx, m200_output_modifier_set *modifiers, node *n, essl_bool *unhook_node, ignore_use_count should_ignore_use_count)
{
	essl_bool incompatible_modifiers = ESSL_FALSE;
	essl_bool found_modifier = ESSL_FALSE;
	essl_bool unhook_child = ESSL_FALSE;
	node *next_node = NULL;
	m200_output_mode m = M200_OUTPUT_NORMAL;
	node *move_src = NULL;
	node_extra *nex;
	node *new_node;

	*unhook_node = ESSL_FALSE;

	ESSL_CHECK(nex = EXTRA_INFO(n));
	if(should_ignore_use_count || (nex->unscheduled_use_count + nex->scheduled_use_count == 1 && nex->original_use_count == 1))
	{
		if(n->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL && n->expr.operation == EXPR_OP_FUN_TRUNC)
		{
			m = M200_OUTPUT_TRUNCATE;
		} 
		else if(n->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL && n->expr.operation == EXPR_OP_FUN_MAX && _essl_is_node_all_value(ctx->desc, GET_CHILD(n, 1), 0.0))
		{
			m = M200_OUTPUT_CLAMP_0_INF;
		}
		else if(n->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL && n->expr.operation == EXPR_OP_FUN_CLAMP && _essl_is_node_all_value(ctx->desc, GET_CHILD(n, 1), 0.0) && _essl_is_node_all_value(ctx->desc, GET_CHILD(n, 2), 1.0))
		{
			m = M200_OUTPUT_CLAMP_0_1;

		}
		if(m != M200_OUTPUT_NORMAL)
		{
			ESSL_CHECK(next_node = GET_CHILD(n, 0));
			found_modifier = ESSL_TRUE;
			if(GET_NODE_VEC_SIZE(next_node) != GET_NODE_VEC_SIZE(n) || !output_modifier_prepend_output_mode(modifiers, m))
			{
				incompatible_modifiers = ESSL_TRUE;
			}
		} 
		else
		{
			if(n->hdr.kind == EXPR_KIND_UNARY && n->expr.operation == EXPR_OP_SWIZZLE)
			{
				found_modifier = ESSL_TRUE;
				ESSL_CHECK(next_node = GET_CHILD(n, 0));
				if(is_scalar_to_vector_swizzle(n->expr.u.swizzle))
				{
					/* we don't want to eat scalar to vector swizzles, as they turn scalar ops into vector ops and this is bad for the scheduler */
					incompatible_modifiers = ESSL_TRUE;
				} 
				else 
				{
					output_modifier_prepend_swizzle(modifiers, n->expr.u.swizzle);
				}
			} 
			else if(n->hdr.kind == EXPR_KIND_VECTOR_COMBINE)
			{
				node *ret;
				unsigned i;
				ESSL_CHECK(ret = _essl_new_depend_expression(ctx->pool, GET_N_CHILDREN(n)));
				_essl_ensure_compatible_node(ret, n);
				ret->hdr.live_mask = n->hdr.live_mask & live_mask_from_swizzle(modifiers->swizzle);
				ESSL_CHECK(make_node_schedulable(ctx, ret, 1337));
				for(i = 0; i < GET_N_CHILDREN(n); ++i)
				{
					m200_output_modifier_set tmp_mods;
					node *child;
					node *ni;
					tmp_mods = *modifiers;
					output_modifier_prepend_mask(&tmp_mods, n->expr.u.combiner, i);
					ESSL_CHECK(ni = GET_CHILD(n, i));
					ESSL_CHECK(child = process_modifier_helper(ctx, &tmp_mods, ni, &unhook_child, NO_IGNORE_USE_COUNT));
					if(unhook_child)
					{
						SET_CHILD(n, i, NULL);
					}
					SET_CHILD(ret, i, child);
				}
				n->hdr.kind = EXPR_KIND_DEPEND;
				n->expr.operation = EXPR_OP_UNKNOWN;
				return ret;
				
			}
			else if(n->hdr.kind == EXPR_KIND_BINARY && n->expr.operation == EXPR_OP_MUL)
			{
				node *child1;
				int exponent = 0;
				essl_bool found_exponent = ESSL_TRUE;
				ESSL_CHECK(GET_CHILD(n, 0));
				ESSL_CHECK(child1 = GET_CHILD(n, 1));
				if(child1->hdr.kind == EXPR_KIND_CONSTANT)
				{
					if(_essl_is_node_all_value(ctx->desc, child1, 1.0/8.0))
					{
						exponent = -3;
					} 
					else if(_essl_is_node_all_value(ctx->desc, child1, 1.0/4.0))
					{
						exponent = -2;
					} 
					else if(_essl_is_node_all_value(ctx->desc, child1, 1.0/2.0))
					{
						exponent = -1;
					} 
					else if(_essl_is_node_all_value(ctx->desc, child1, 1.0))
					{
						exponent = 0;
					} 
					else if(_essl_is_node_all_value(ctx->desc, child1, 2.0))
					{
						exponent = 1;
					} 
					else if(_essl_is_node_all_value(ctx->desc, child1, 4.0))
					{
						exponent = 2;
					} 
					else if(_essl_is_node_all_value(ctx->desc, child1, 8.0))
					{
						exponent = 3;
					} 
					else 
					{
						found_exponent = ESSL_FALSE;
					}
					if(found_exponent)
					{
						found_modifier = ESSL_TRUE;
						ESSL_CHECK(next_node = GET_CHILD(n, 0));
						if(GET_NODE_VEC_SIZE(next_node) != GET_NODE_VEC_SIZE(n) || !output_modifier_prepend_exponent_adjust(modifiers, exponent))
						{
							incompatible_modifiers = ESSL_TRUE;
						}
					}

				}

			}

		}
	}

	if(found_modifier && !incompatible_modifiers)
	{
		node *res;
		unsigned i;
		ESSL_CHECK(res = process_modifier_helper(ctx, modifiers, next_node, &unhook_child, NO_IGNORE_USE_COUNT));
		n->hdr.kind = EXPR_KIND_DEPEND;
		n->expr.operation = EXPR_OP_UNKNOWN;
		n->hdr.live_mask = n->hdr.live_mask & live_mask_from_swizzle(modifiers->swizzle);
		/* lose the unecessary nodes */
		for(i = 0; i < GET_N_CHILDREN(n); ++i)
		{
			node *child = GET_CHILD(n, i);
			if(child != NULL)
			{
				if(i == 0 && next_node != NULL)
				{
					assert(next_node == child);
					if(unhook_child)
					{
						SET_CHILD(n, i, NULL);
					}
				} else{
					ESSL_CHECK(_essl_scheduler_forget_unscheduled_use(ctx->sctx, GET_CHILD(n, i)));
					SET_CHILD(n, i, NULL);

				}
				
			}
		}
		return res;
	} 
	else 
	{
		/* okay. final node. see if we can attach stuff to it */
		*unhook_node = ESSL_TRUE;
		if((_essl_mali200_output_modifier_is_identity(modifiers) 
			&& (modifiers->trans_node == NULL 
				|| modifiers->trans_node == n 
				|| ((nex->unscheduled_use_count + nex->scheduled_use_count == 1) 
					&& nex->original_use_count == 1 && _essl_mali200_can_handle_redirection(n)))) 
		   || (!incompatible_modifiers &&
			   (_essl_mali200_has_output_modifier_slot(n) && 
				(modifiers->mode == M200_OUTPUT_NORMAL || _essl_mali200_has_output_modifier_and_truncsat_slot(n)) && 
				(_essl_is_identity_swizzle_sized(modifiers->swizzle, M200_NATIVE_VECTOR_SIZE) 
				|| _essl_mali200_has_output_modifier_and_swizzle_slot(n, is_scalar_to_scalar_swizzle(modifiers->swizzle))) 
				&& 	(nex->unscheduled_use_count + nex->scheduled_use_count == 1 && nex->original_use_count == 1))
			   && (modifiers->exponent_adjust == 0 || (n->hdr.kind == EXPR_KIND_BINARY 
			   				&& ((n->expr.operation == EXPR_OP_ADD && ctx->desc->options->mali200_add_with_scale_overflow_workaround == ESSL_FALSE) || n->expr.operation == EXPR_OP_MUL)))
			   ))
		{
			nex->u.m200_modifiers = *modifiers;
			n->hdr.live_mask = n->hdr.live_mask & live_mask_from_swizzle(modifiers->swizzle);
			return n;
		} else 
		{
			move_src = n;
		}
	}

	

	/* need a mov or mul node here */

	{
		node_extra *ex;
		assert(move_src != NULL);
		if(modifiers->exponent_adjust != 0)
		{
			node *scale;
			node_extra *srcex;
			ESSL_CHECK(scale = create_float_constant(ctx, (1<<(modifiers->exponent_adjust+3))/8.0f, 1));
			ESSL_CHECK(make_node_schedulable(ctx, scale, 1));
			ESSL_CHECK(new_node = _essl_new_binary_expression(ctx->pool, scale, EXPR_OP_MUL, move_src)); /* constant on the left side, so we don't re-process the node */
			_essl_ensure_compatible_node(new_node, move_src);
			ESSL_CHECK(srcex = EXTRA_INFO(move_src));
			ESSL_CHECK(make_node_schedulable(ctx, new_node, srcex->operation_depth+1));
		} 
		else 
		{
			ESSL_CHECK(new_node = create_mov_node(ctx, move_src));
		}

		new_node->hdr.live_mask = n->hdr.live_mask & live_mask_from_swizzle(modifiers->swizzle);

		ESSL_CHECK(ex = EXTRA_INFO(new_node));
		ex->u.m200_modifiers = *modifiers;
		ex->u.m200_modifiers.exponent_adjust = 0;
		return new_node;
	}
}

static node *process_modifier(mali200_scheduler_context *ctx, node *n)
{
	node *ret;
	essl_bool unhook_node = ESSL_FALSE;
	m200_output_modifier_set mod;
	node_extra *nex;
	ESSL_CHECK(nex = EXTRA_INFO(n));
	
	output_modifier_init(&mod, nex->u.m200_modifiers.trans_node ? nex->u.m200_modifiers.trans_node : n);
	ESSL_CHECK(ret = process_modifier_helper(ctx, &mod, n, &unhook_node, IGNORE_USE_COUNT));
	if(unhook_node)
	{
		/* nothing could be done about this. mark as tried and return the old node */
		nex->u.m200_modifiers.trans_node = n;
		return n;
	}

	ESSL_CHECK(APPEND_CHILD(n, ret, ctx->pool));
	return ret;
}


static m200_opcode get_exponent_adjusted_opcode(node *n)
{
	m200_opcode add_opcode = M200_ADD;
	m200_opcode mul_opcode = M200_MUL;
	node_extra *nex;
	nex = EXTRA_INFO(n);
	assert(nex != 0);
	switch(nex->u.m200_modifiers.exponent_adjust)
	{
	case -3:
		add_opcode = M200_ADD_D8;
		mul_opcode = M200_MUL_D8;
		break;
	case -2:
		add_opcode = M200_ADD_D4;
		mul_opcode = M200_MUL_D4;
		break;
	case -1:
		add_opcode = M200_ADD_D2;
		mul_opcode = M200_MUL_D2;
		break;
	case 0:
		add_opcode = M200_ADD;
		mul_opcode = M200_MUL;
		break;
	case 1:
		add_opcode = M200_ADD_X2;
		mul_opcode = M200_MUL_X2;
		break;
	case 2:
		add_opcode = M200_ADD_X4;
		mul_opcode = M200_MUL_X4;
		break;
	case 3:
		add_opcode = M200_ADD_X8;
		mul_opcode = M200_MUL_X8;
		break;
	default:
		assert(0);
	}
	assert(n->hdr.kind == EXPR_KIND_BINARY);
	if(n->expr.operation == EXPR_OP_ADD) 
	{
		return add_opcode;
	}
	assert(n->expr.operation == EXPR_OP_MUL);
	return mul_opcode;

}


static m200_schedule_classes get_mov_schedule_class(node *n)
{
	node_extra *nex;
	nex = EXTRA_INFO(n);
	assert(nex != 0);
	if(_essl_mali200_output_modifier_is_identity(&nex->u.m200_modifiers))
	{
		return M200_SC_MOV;
	}
	else 
	{
		return M200_SC_MOV_WITH_MODIFIERS;
	}

}


static void mark_read_precision(node *n, unsigned bit_precision)
{
	symbol *s;
	node_extra *ex;
	ex = EXTRA_INFO(n);
	assert(ex != NULL);
	assert(ex->address_symbols != NULL);
	s = ex->address_symbols->sym;
	if(s->max_read_bit_precision < bit_precision)
	{
		s->max_read_bit_precision = bit_precision;
	}
}

static memerr mali200_schedule_single_operation(mali200_scheduler_context * ctx, node *n);

static memerr handle_add(mali200_scheduler_context * ctx, node *res)
{
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));

	return schedule(ctx, res, NULL, NULL, M200_SC_ADD, get_exponent_adjusted_opcode(res), &res, a, b);
}

static memerr handle_mul(mali200_scheduler_context * ctx, node *res)
{
	node *a;
	node *b;
	node_extra *ex;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	ESSL_CHECK(ex = EXTRA_INFO(res));
	if(ex->u.m200_modifiers.trans_node == NULL &&
		(_essl_is_node_all_value(ctx->desc, b, 1.0/8.0) 
		|| _essl_is_node_all_value(ctx->desc, b, 1.0/4.0) 
		|| _essl_is_node_all_value(ctx->desc, b, 1.0/2.0) 
		|| _essl_is_node_all_value(ctx->desc, b, 1.0) 
		|| _essl_is_node_all_value(ctx->desc, b, 2.0) 
		|| _essl_is_node_all_value(ctx->desc, b, 4.0) 
		|| _essl_is_node_all_value(ctx->desc, b, 8.0)
		))
	{
		ESSL_CHECK(process_modifier(ctx, res));
		return mali200_schedule_single_operation(ctx, res);
	}
	else if(ex->u.m200_modifiers.exponent_adjust == 0 
				&& ex->u.m200_modifiers.mode == M200_OUTPUT_NORMAL 
				&& !is_vector_op(a) 
				&& is_vector_op(b) 
				&& is_vector_op(res))
	{
		return schedule(ctx, res, NULL, NULL, M200_SC_SCALAR_VECTOR_MUL_LUT, M200_MUL, &res, a, b);
	}
	else if(ex->u.m200_modifiers.exponent_adjust == 0 
				&& ex->u.m200_modifiers.mode == M200_OUTPUT_NORMAL 
				&& !is_vector_op(b) 
				&& is_vector_op(a) 
				&& is_vector_op(res))
	{
		return schedule(ctx, res, NULL, NULL, M200_SC_SCALAR_VECTOR_MUL_LUT, M200_MUL, &res, b, a);
	}
	else
	{
		m200_opcode op = get_exponent_adjusted_opcode(res);
		m200_schedule_classes cls = M200_SC_MUL;
		if(op == M200_MUL && ex->u.m200_modifiers.mode == M200_OUTPUT_NORMAL)
		{
			cls = M200_SC_MUL_LUT;
		}
		return schedule(ctx, res, NULL, NULL, cls, op, &res, a, b);
	}
}

static memerr handle_rcp(mali200_scheduler_context * ctx, node *res)
{
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	return schedule(ctx, res, NULL, NULL, M200_SC_LUT, M200_RCP, &res, a);
}

static memerr handle_swizzle(mali200_scheduler_context * ctx, node *res)
{
	node_extra *ex;
	ESSL_CHECK(ex = EXTRA_INFO(res));
	if(ex->u.m200_modifiers.trans_node == NULL
		&& !is_scalar_to_vector_swizzle(res->expr.u.swizzle))
	{
		ESSL_CHECK(process_modifier(ctx, res));
		return mali200_schedule_single_operation(ctx, res);
	}
	else
	{
		return schedule(ctx, res, NULL, NULL, M200_SC_MOV_WITH_MODIFIERS, M200_MOV, &res, res);
	}
}

static memerr handle_simple_instruction(mali200_scheduler_context * ctx, node *res)
{
	return schedule(ctx, res, NULL, NULL, M200_SC_MOV_WITH_MODIFIERS, M200_MOV, &res, res);
}

static memerr handle_vector_combine(mali200_scheduler_context * ctx, node *res)
{
	ESSL_CHECK(process_modifier(ctx, res));
	return mali200_schedule_single_operation(ctx, res);
}

static memerr handle_subvector_access(mali200_scheduler_context * ctx, node *res)
{
	node *data;
	node *index;
	ESSL_CHECK(data = GET_CHILD(res, 0));
	ESSL_CHECK(index = GET_CHILD(res, 1));
	return schedule(ctx, res, NULL, NULL, M200_SC_STORE | M200_SC_LSB_VECTOR_INPUT, M200_LD_SUBREG, &res, index, data);
}

static memerr handle_subvector_update(mali200_scheduler_context * ctx, node *res)
{
	node *idx;
	node *b;
	node *a;
	node_extra *exa, *exres;
	node *trans_node;

	ESSL_CHECK(idx = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	ESSL_CHECK(a = GET_CHILD(res, 2));

	if(!_essl_scheduler_is_only_use_of_source(res, a) 
		|| !_essl_mali200_has_output_modifier_slot(a) 
		|| _essl_mali200_is_coalescing_candidate(a))
	{
		ESSL_CHECK(a = create_mov_node(ctx, a));
		SET_CHILD(res, 2, a);
		
	}
	ESSL_CHECK(exa = EXTRA_INFO(a));
	ESSL_CHECK(exres = EXTRA_INFO(res));
	trans_node = (exres->u.m200_modifiers.trans_node != NULL) ? exres->u.m200_modifiers.trans_node : res;
	assert(exa->u.m200_modifiers.trans_node == NULL || exa->u.m200_modifiers.trans_node == trans_node); /* the latter can happen if a subvector_update is postponed */
	exa->u.m200_modifiers.trans_node = trans_node;

	return schedule(ctx, res, NULL, NULL, M200_SC_STORE | M200_SC_LSB_VECTOR_INPUT, M200_ST_SUBREG, &res, idx, b);
}

static memerr handle_clamp(mali200_scheduler_context * ctx, node *res)
{
	node *zero;
	node *one;

	ESSL_CHECK(zero = GET_CHILD(res, 1));
	ESSL_CHECK(one = GET_CHILD(res, 2));
	if(_essl_is_node_all_value(ctx->desc, zero, 0.0) 
		&& _essl_is_node_all_value(ctx->desc, one, 1.0))
	{
		node_extra *ex;
		essl_bool no_output_trans_node;
		ESSL_CHECK(ex = EXTRA_INFO(res));
		no_output_trans_node = ex->u.m200_modifiers.trans_node == NULL || ex->u.m200_modifiers.trans_node == res;
		assert(no_output_trans_node && "clamp() cannot handle output modifier");
		if(no_output_trans_node)
		{
			ESSL_CHECK(process_modifier(ctx, res));
			return mali200_schedule_single_operation(ctx, res);
		}
		else
		{
			assert(0 && "not implemented operation");
			return MEM_ERROR;
		}
	}
	else
	{
		assert(0 && "not implemented operation");
		return MEM_ERROR;
	}
}

static memerr handle_max(mali200_scheduler_context * ctx, node *res)
{
	node *zero;
	node_extra *ex;
	ESSL_CHECK(ex = EXTRA_INFO(res));
	ESSL_CHECK(zero = GET_CHILD(res, 1));
	if(ex->u.m200_modifiers.trans_node == NULL 
		&& _essl_is_node_all_value(ctx->desc, zero, 0.0))
	{
		ESSL_CHECK(process_modifier(ctx, res));
		return mali200_schedule_single_operation(ctx, res);
	}
	else
	{
		node *a;
		node *b;
		ESSL_CHECK(a = GET_CHILD(res, 0));
		ESSL_CHECK(b = GET_CHILD(res, 1));
		return schedule(ctx, res, NULL, NULL, M200_SC_ALU, M200_MAX, &res, a, b);
	}
}

static memerr handle_min(mali200_scheduler_context * ctx, node *res)
{
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	return schedule(ctx, res, NULL, NULL, M200_SC_ALU, M200_MIN, &res, a, b);
}

static memerr handle_identity(mali200_scheduler_context * ctx, node *res)
{
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	return schedule(ctx, res, NULL, NULL, get_mov_schedule_class(res), M200_MOV, &res, a);
}

static memerr handle_comparison(mali200_scheduler_context * ctx, node *res)
{
	node *a;
	node *b;
	essl_bool inverted = ESSL_FALSE;
	m200_comparison cmp;

	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	cmp = expr_op_to_comparison(res->expr.operation, &inverted);
	return schedule(ctx, res, NULL, NULL, M200_SC_ALU, M200_CMP, &res, cmp, 0, inverted ? b : a, inverted ? a : b);

}

static memerr handle_not(mali200_scheduler_context * ctx, node *res)
{
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	return schedule(ctx, res, NULL, NULL, M200_SC_MUL, M200_NOT, &res, a);
}

static memerr handle_conditional_select(mali200_scheduler_context * ctx, node *res)
{
	node *cond;
	node *c;
	node *d;
	ESSL_CHECK(cond = GET_CHILD(res, 0));
	if(is_vector_op(cond))
	{
		assert(0 && "not implemented operation");
		return MEM_ERROR;
	}
	ESSL_CHECK(c = GET_CHILD(res, 1));
	ESSL_CHECK(d = GET_CHILD(res, 2));
	if(_essl_is_node_comparison(cond))
	{
		node *a;
		node *b;
		essl_bool inverted = ESSL_FALSE;
		m200_comparison cmp;
		ESSL_CHECK(a = GET_CHILD(cond, 0));
		ESSL_CHECK(b = GET_CHILD(cond, 1));
		cmp = expr_op_to_comparison(cond->expr.operation, &inverted);
		return schedule(ctx, res, NULL, NULL, 
				   M200_SC_ADD|M200_SC_MUL1, 
				   M200_SEL, &res, c, d, 
				   M200_CMP, &cond, cmp, M200_ALU_DONT_WRITE_REG_FILE, inverted ? b : a, inverted ? a : b);

	}
	else
	{
		return schedule(ctx, res, NULL, NULL, 
				   M200_SC_ADD|M200_SC_MUL1, 
				   M200_SEL, &res, c, d, 
				   M200_MOV, NULL, cond);

	}
}

static memerr handle_constant(mali200_scheduler_context * ctx, node *res)
{
	return schedule(ctx, res, res, NULL, M200_SC_MOV_WITH_MODIFIERS, M200_CONST_MOV, &res);	
}

static memerr handle_load(mali200_scheduler_context * ctx, node *res)
{
	if(res->expr.u.load_store.address_space == ADDRESS_SPACE_UNIFORM)
	{
		node *child;
		m200_schedule_classes sc = M200_SC_MOV_IF_NOT_SAME_WORD | M200_SC_DONT_SCHEDULE_MOV | M200_SC_LOAD;
		child = GET_CHILD(res, 0);
		if (child != NULL) 
		{
			/* Indexed load */
			sc |= M200_SC_NO_RENDEZVOUS;
		}
		mark_read_precision(res, 16);
		return schedule(ctx, res, NULL, res, sc, 
				   M200_REG_MOV, &res, M200_HASH_LOAD, M200_LD_UNIFORM, &res, child);
	}
	else if(res->expr.u.load_store.address_space == ADDRESS_SPACE_FRAGMENT_VARYING)
	{
		node *child;
		m200_schedule_classes sc = M200_SC_VAR;
		child = GET_CHILD(res, 0);
		if (child != NULL) 
		{
			/* Indexed load */
			sc |= M200_SC_NO_RENDEZVOUS;
		} 
		mark_read_precision(res, 16);
		return schedule(ctx, res, NULL, res, sc, M200_VAR, &res, get_varying_flags(res), child);
	}
	else if(res->expr.u.load_store.address_space == ADDRESS_SPACE_FRAGMENT_OUT 
				|| res->expr.u.load_store.address_space == ADDRESS_SPACE_GLOBAL)
	{
		node *zero;
		node *child;
		m200_schedule_classes sc = M200_SC_MOV_IF_NOT_SAME_WORD | M200_SC_DONT_SCHEDULE_MOV | M200_SC_LOAD;
		child = GET_CHILD(res, 0);
		if (child != NULL) 
		{
			/* Indexed load */
			sc |= M200_SC_NO_RENDEZVOUS;
		} 
		mark_read_precision(res, 16);
		ESSL_CHECK(zero = create_zero_node(ctx));
		ESSL_CHECK(APPEND_CHILD(res, zero, ctx->pool));
		return schedule(ctx, res, NULL, res, sc,
				   M200_REG_MOV, &res, M200_HASH_LOAD, M200_LD_REL, &res, child, zero);

	}
	else if(res->expr.u.load_store.address_space == ADDRESS_SPACE_REGISTER)
	{
		preallocated_var *prealloc;
		ESSL_CHECK(prealloc = LIST_NEW(ctx->pool, preallocated_var));
		prealloc->var = res;
		prealloc->allocation = *res->expr.u.load_store.alloc;
		LIST_INSERT_FRONT(&ctx->sctx->current_block->preallocated_defs, prealloc);
		return _essl_scheduler_schedule_operation(ctx->sctx, res, 999999);
	}
	else
	{
		mark_read_precision(res, 16);
		return schedule(ctx, res, NULL, res, 
				   M200_SC_MOV_IF_NOT_SAME_WORD | M200_SC_DONT_SCHEDULE_MOV | M200_SC_LOAD, 
				   M200_REG_MOV, &res, M200_HASH_LOAD, M200_LD_STACK, &res, GET_CHILD(res, 0));
	}
}

static memerr handle_m200_pos(mali200_scheduler_context * ctx, node *res)
{
	return schedule(ctx, res, NULL, res, M200_SC_VAR, M200_POS, &res);
}

static memerr handle_m200_point(mali200_scheduler_context * ctx, node *res)
{
	return schedule(ctx, res, NULL, res, M200_SC_VAR, M200_POINT, &res);
}

static memerr handle_m200_miscval(mali200_scheduler_context * ctx, node *res)
{
	return schedule(ctx, res, NULL, res, M200_SC_VAR, M200_MISC_VAL, &res);
}

static memerr handle_m200_ldrgb(mali200_scheduler_context * ctx, node *res)
{
	ctx->tu->buffer_usage.color_read = ESSL_TRUE;
	return schedule(ctx, res, NULL, NULL, M200_SC_STORE, M200_LD_RGB, &res);
}

static memerr handle_m200_ldzs(mali200_scheduler_context * ctx, node *res)
{
	ctx->tu->buffer_usage.depth_read = ESSL_TRUE;
	ctx->tu->buffer_usage.stencil_read = ESSL_TRUE;
	return schedule(ctx, res, NULL, NULL, M200_SC_STORE, M200_LD_ZS, &res);
}

static memerr handle_store(mali200_scheduler_context * ctx, node *res)
{
	node *value;
	ESSL_CHECK(value = GET_CHILD(res, 1));
	if(res->expr.u.load_store.address_space == ADDRESS_SPACE_FRAGMENT_OUT 
		|| res->expr.u.load_store.address_space == ADDRESS_SPACE_GLOBAL)
	{
		node *zero;
		ESSL_CHECK(zero = create_zero_node(ctx));
		ESSL_CHECK(APPEND_CHILD(res, zero, ctx->pool));
		return schedule(ctx, res, zero, NULL, M200_SC_STORE | M200_SC_LSB_VECTOR_INPUT | M200_SC_NO_RENDEZVOUS, 
						M200_ST_REL, &res, 
						GET_CHILD(res, 0), value, zero);

	}
	else if(res->expr.u.load_store.address_space == ADDRESS_SPACE_REGISTER)
	{
		preallocated_var *prealloc;
		ESSL_CHECK(prealloc = LIST_NEW(ctx->pool, preallocated_var));
		prealloc->var = value;
		prealloc->allocation = *res->expr.u.load_store.alloc;
		LIST_INSERT_FRONT(&ctx->sctx->current_block->preallocated_uses, prealloc);
		return _essl_scheduler_schedule_operation(ctx->sctx, res, -999999);
	}
	else
	{
		return schedule(ctx, res, NULL, NULL, 
						M200_SC_STORE | M200_SC_LSB_VECTOR_INPUT | M200_SC_NO_RENDEZVOUS, 
						M200_ST_STACK, &res, 
						GET_CHILD(res, 0), value);
	}
}

static memerr handle_op_lut(mali200_scheduler_context * ctx, node *res)
{
	node *a;
	m200_opcode opc = M200_UNKNOWN;
	expression_operator op;
	op = res->expr.operation;
	switch(op)
	{
		case EXPR_OP_FUN_SIN_0_1:
			opc = M200_SIN;
			break;
		case EXPR_OP_FUN_COS_0_1:
			opc = M200_COS;
			break;
		case EXPR_OP_FUN_EXP2:
			opc = M200_EXP;
			break;
		case EXPR_OP_FUN_LOG2:
			opc = M200_LOG;
			break;
		case EXPR_OP_FUN_SQRT:
			opc = M200_SQRT;
			break;
		case EXPR_OP_FUN_INVERSESQRT:
			opc = M200_RSQ;
			break;
		default:
			assert(0 && "not implemented operation");
			return MEM_ERROR;
	}
	ESSL_CHECK(a = GET_CHILD(res, 0));
	return schedule(ctx, res, NULL, NULL, M200_SC_LUT, opc, &res, a);
}

static memerr handle_op_add(mali200_scheduler_context * ctx, node *res)
{
	node *a;
	m200_opcode opc = M200_UNKNOWN;
	expression_operator op;
	op = res->expr.operation;
	switch(op)
	{
		case EXPR_OP_FUN_FLOOR:
			opc = M200_FLOOR;
			break;
		case EXPR_OP_FUN_CEIL:
			opc = M200_CEIL;
			break;
		case EXPR_OP_FUN_FRACT:
			opc = M200_FRAC;
			break;
		default:
			assert(0 && "not implemented operation");
			return MEM_ERROR;	
	}
	ESSL_CHECK(a = GET_CHILD(res, 0));
	return schedule(ctx, res, NULL, NULL, M200_SC_ADD, opc, &res, a);
}

static memerr handle_trunc(mali200_scheduler_context * ctx, node *res)
{
	ESSL_CHECK(process_modifier(ctx, res));
	return mali200_schedule_single_operation(ctx, res);
}

static memerr handle_cubemap(mali200_scheduler_context * ctx, node *res)
{
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	return schedule(ctx, res, NULL, NULL, M200_SC_VAR, M200_MOV_CUBE, &res, a);
}

static memerr handle_texture(mali200_scheduler_context * ctx, node *res)
{
	node *coord;
	node *sampler;
	node *coord_address = NULL;
	expression_operator op;
	m200_opcode varying_opcode;
	node *lod = NULL;
	double lod_offset = 0.0;
	unsigned int sched_class = 0;
	unsigned int lod_replace_bit = 0;
	unsigned int reg_sched_class = M200_SC_MOV_IF_NOT_SAME_WORD | M200_SC_DONT_SCHEDULE_MOV | M200_SC_TEX | M200_SC_VAR | M200_SC_RENDEZVOUS;
	unsigned int lod_sched_class = M200_SC_MOV_IF_NOT_SAME_WORD | M200_SC_DONT_SCHEDULE_MOV | M200_SC_TEX | M200_SC_VAR;
	op = res->expr.operation;
	sampler = GET_CHILD(res, 0);
	ESSL_CHECK(coord = GET_CHILD(res, 1));
	varying_opcode = M200_UNKNOWN;

	if(GET_N_CHILDREN(res) == 3)
	{
		lod = GET_CHILD(res, 2);
	}

	if(GET_N_CHILDREN(coord) > 0)
	{
		coord_address = GET_CHILD(coord, 0);
	}

	if(coord->hdr.kind == EXPR_KIND_LOAD 
		&& coord->expr.u.load_store.address_space == ADDRESS_SPACE_FRAGMENT_VARYING
		&& coord_address == NULL)
	{
		node *coalescing_candidate = coord;
		int var_flags = 0;

		var_flags = get_varying_flags(coord);
		mark_read_precision(coord, 24);

		switch(op)
		{
			case EXPR_OP_FUN_TEXTURE1D:
			case EXPR_OP_FUN_TEXTURE2D:
			case EXPR_OP_FUN_TEXTURE3D:
				sched_class = reg_sched_class;
				varying_opcode = M200_VAR;
				break;
		  	case EXPR_OP_FUN_TEXTURE1DLOD:
			case EXPR_OP_FUN_TEXTURE2DLOD:
			case EXPR_OP_FUN_TEXTURE3DLOD:
				sched_class = lod_sched_class;
				lod_replace_bit = M200_TEX_LOD_REPLACE;
				varying_opcode = M200_VAR;
				break;

			case EXPR_OP_FUN_TEXTURE1DPROJ:
				sched_class = reg_sched_class;
				varying_opcode = M200_VAR_DIV_Y;
				break;

			case EXPR_OP_FUN_TEXTURE1DPROJLOD:
				lod_replace_bit = M200_TEX_LOD_REPLACE;
				sched_class = lod_sched_class;
				varying_opcode = M200_VAR_DIV_Y;
				break;

			case EXPR_OP_FUN_TEXTURE2DPROJ:
				sched_class = reg_sched_class;
				varying_opcode = M200_VAR_DIV_Z;
				break;

		  	case EXPR_OP_FUN_TEXTURE2DPROJLOD:
				lod_replace_bit = M200_TEX_LOD_REPLACE;
				sched_class = lod_sched_class;
				varying_opcode = M200_VAR_DIV_Z;
				break;

			case EXPR_OP_FUN_TEXTURE2DPROJ_W:
			case EXPR_OP_FUN_TEXTURE3DPROJ:
				sched_class = reg_sched_class;
				varying_opcode = M200_VAR_DIV_W;
				break;

		  	case EXPR_OP_FUN_TEXTURE2DPROJLOD_W:
			case EXPR_OP_FUN_TEXTURE3DPROJLOD:
				lod_replace_bit = M200_TEX_LOD_REPLACE;
				sched_class = lod_sched_class;
				varying_opcode = M200_VAR_DIV_W;
				break;

			case EXPR_OP_FUN_TEXTURECUBE:
				sched_class = reg_sched_class;
				varying_opcode = M200_VAR_CUBE;
				lod_offset = -1.0;
				break;

			case EXPR_OP_FUN_TEXTURECUBELOD:
				lod_replace_bit = M200_TEX_LOD_REPLACE;
				sched_class = lod_sched_class;
				varying_opcode = M200_VAR_CUBE;
				lod_offset = 0.0;
				break;
			default:
				assert(0 && "not implemented operation");
				return MEM_ERROR;

		}
		if(varying_opcode != M200_VAR)
		{
			coalescing_candidate = NULL; /* don't coalesce funny loads */
		}

		return schedule(ctx, res, NULL, coalescing_candidate, 
							sched_class,
							M200_REG_MOV, &res, M200_HASH_TEX, 
							M200_TEX, &res, lod_replace_bit, sampler, lod, lod_offset,
							varying_opcode, &coord, var_flags, coord_address);
	}
	else
	{
		switch(op)
		{
			case EXPR_OP_FUN_TEXTURE1D:
			case EXPR_OP_FUN_TEXTURE2D:
			case EXPR_OP_FUN_TEXTURE3D:
				sched_class = reg_sched_class;
				varying_opcode = M200_MOV;
				break;

			case EXPR_OP_FUN_TEXTURE1DLOD:
			case EXPR_OP_FUN_TEXTURE2DLOD:
			case EXPR_OP_FUN_TEXTURE3DLOD:
				lod_replace_bit = M200_TEX_LOD_REPLACE;
				sched_class = lod_sched_class;
				varying_opcode = M200_MOV;
				break;

			case EXPR_OP_FUN_TEXTURE1DPROJ:
				sched_class = reg_sched_class;
				varying_opcode = M200_MOV_DIV_Y;
				break;

			case EXPR_OP_FUN_TEXTURE1DPROJLOD:
				lod_replace_bit = M200_TEX_LOD_REPLACE;
				sched_class = lod_sched_class;
				varying_opcode = M200_MOV_DIV_Y;
				break;

			case EXPR_OP_FUN_TEXTURE2DPROJ:
				sched_class = reg_sched_class;
				varying_opcode = M200_MOV_DIV_Z;
				break;

			case EXPR_OP_FUN_TEXTURE2DPROJLOD:
				lod_replace_bit = M200_TEX_LOD_REPLACE;
				sched_class = lod_sched_class;
				varying_opcode = M200_MOV_DIV_Z;
				break;

			case EXPR_OP_FUN_TEXTURE2DPROJ_W:
			case EXPR_OP_FUN_TEXTURE3DPROJ:
				sched_class = reg_sched_class;
				varying_opcode = M200_MOV_DIV_W;
				break;

			case EXPR_OP_FUN_TEXTURE2DPROJLOD_W:
			case EXPR_OP_FUN_TEXTURE3DPROJLOD:
				lod_replace_bit = M200_TEX_LOD_REPLACE;
				sched_class = lod_sched_class;
				varying_opcode = M200_MOV_DIV_W;
				break;

			case EXPR_OP_FUN_TEXTURECUBELOD:
				lod_replace_bit = M200_TEX_LOD_REPLACE;
				sched_class = lod_sched_class;
				varying_opcode = M200_MOV_CUBE;
				lod_offset = 0.0;
				break;

			case EXPR_OP_FUN_TEXTURECUBE:
				sched_class = reg_sched_class;
				varying_opcode = M200_MOV_CUBE;
				lod_offset = -1.0;
			break;
			
			default:
				assert(0 && "not implemented operation");
				return MEM_ERROR;
		}
		return schedule(ctx, res, NULL, NULL,
							sched_class,
							M200_REG_MOV, &res, M200_HASH_TEX, 
							M200_TEX, &res, lod_replace_bit, sampler, lod, lod_offset,
							varying_opcode, NULL, coord);
	}
}

static memerr handle_hadd(mali200_scheduler_context * ctx, node *res)
{
	node *a;
	m200_opcode op;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	op = GET_NODE_VEC_SIZE(a) == 4 ? M200_HADD4 : M200_HADD3;
	assert(GET_NODE_VEC_SIZE(a) == 4 || GET_NODE_VEC_SIZE(a) == 3);
	return schedule(ctx, res, NULL, NULL, M200_SC_ADD4, op, &res, a);
}

static memerr handle_haddpair(mali200_scheduler_context * ctx, node *res)
{
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	return schedule(ctx, res, NULL, NULL, M200_SC_ADD4, M200_2X2ADD, &res, a, b);
}

static memerr handle_normalize(mali200_scheduler_context * ctx, node *res)
{
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	return schedule(ctx, res, NULL, NULL, M200_SC_VAR, M200_NORM3, &res, a);
}

static memerr handle_atan(mali200_scheduler_context * ctx, node *res)
{
	expression_operator op;
	
	op = res->expr.operation;
	if(op == EXPR_OP_FUN_M200_ATAN_IT1)
	{
		if(GET_N_CHILDREN(res) == 1)
		{
			node *a;
			ESSL_CHECK(a = GET_CHILD(res, 0));
			return schedule(ctx, res, NULL, NULL, M200_SC_LUT|M200_SC_LSB_VECTOR_INPUT, M200_ATAN1_IT1, &res, a);
		}
		else
		{
			node *y;
			node *x;
			assert(GET_N_CHILDREN(res) == 2);
			ESSL_CHECK(y = GET_CHILD(res, 0));
			ESSL_CHECK(x = GET_CHILD(res, 1));
			return schedule(ctx, res, NULL, NULL, M200_SC_LUT|M200_SC_LSB_VECTOR_INPUT, M200_ATAN2_IT1, &res, y, x);
		}
	}
	else
	{
		node *a;
		assert(op == EXPR_OP_FUN_M200_ATAN_IT2);
		ESSL_CHECK(a = GET_CHILD(res, 0));
		return schedule(ctx, res, NULL, NULL, M200_SC_LUT|M200_SC_LSB_VECTOR_INPUT, M200_ATAN_IT2, &res, a);
	}
}

static memerr handle_logicalxor(mali200_scheduler_context * ctx, node *res)
{
	node *a;
	node *b;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	ESSL_CHECK(b = GET_CHILD(res, 1));
	return schedule(ctx, res, NULL, NULL, M200_SC_MUL, M200_XOR, &res, a, b);
}

static memerr handle_derivative(mali200_scheduler_context * ctx, node *res)
{
	m200_opcode opc;
	node *a;
	ESSL_CHECK(a = GET_CHILD(res, 0));
	if(res->expr.operation == EXPR_OP_FUN_M200_DERX)
	{
		opc = M200_DERX;
	}
	else
	{
		assert(res->expr.operation == EXPR_OP_FUN_M200_DERY);
		opc = M200_DERY;
	}
	return schedule(ctx, res, NULL, NULL, M200_SC_ADD | M200_SC_RENDEZVOUS, opc, &res, a);
}

static memerr handle_depend(mali200_scheduler_context * ctx, node *res)
{
	node_extra *resinfo;
	ESSL_CHECK(resinfo = EXTRA_INFO(res));
	return _essl_scheduler_schedule_operation(ctx->sctx, res, resinfo->earliest_use);
}

static memerr mali200_schedule_single_operation(mali200_scheduler_context * ctx, node *n) 
{
	node_kind kind;
	expression_operator op;
	ESSL_CHECK(n);
	kind = n->hdr.kind;
	op = n->expr.operation;
	if(kind == EXPR_KIND_UNARY)
	{
		if(op == EXPR_OP_SWIZZLE)
		{
			return handle_swizzle(ctx, n);
		}
		else if(op == EXPR_OP_NEGATE)
		{
			return handle_simple_instruction(ctx, n);
		}
		else if(op == EXPR_OP_IDENTITY)
		{
			return handle_identity(ctx, n);
		}
		else if(op == EXPR_OP_NOT)
		{
			return handle_not(ctx, n);
		}
		else
		{
			assert(0 && "not implemented operation");
			return MEM_ERROR;
		}
	}
	else if(kind == EXPR_KIND_BINARY)
	{
		if(op == EXPR_OP_ADD)
		{
			return handle_add(ctx, n);
		}
		else if(op == EXPR_OP_MUL)
		{
			return handle_mul(ctx, n);
		}
		else if(op == EXPR_OP_SUBVECTOR_ACCESS)
		{
			return handle_subvector_access(ctx, n);
		}
		else if(op == EXPR_OP_LT 
				|| op == EXPR_OP_LE
				|| op == EXPR_OP_EQ
				|| op == EXPR_OP_NE
				|| op == EXPR_OP_GE
				|| op == EXPR_OP_GT)
		{
			return handle_comparison(ctx, n);
		}
		else if(op == EXPR_OP_LOGICAL_XOR)
		{
			return handle_logicalxor(ctx, n);
		}
		else
		{
			assert(0 && "not implemented operation");
			return MEM_ERROR;
		}
	}
	else if(kind == EXPR_KIND_TERNARY)
	{
		if(op == EXPR_OP_SUBVECTOR_UPDATE)
		{
			return handle_subvector_update(ctx, n);
		}
		else if(op == EXPR_OP_CONDITIONAL_SELECT)
		{
			return handle_conditional_select(ctx, n);
		}
		else
		{
			assert(0 && "not implemented operation");
			return MEM_ERROR;
		}
	}
	else if(kind == EXPR_KIND_BUILTIN_FUNCTION_CALL)
	{
		if(op == EXPR_OP_FUN_RCP)
		{
			return handle_rcp(ctx, n);
		}
		else if(op == EXPR_OP_FUN_ABS)
		{
			return handle_simple_instruction(ctx, n);
		}
		else if(op == EXPR_OP_FUN_CLAMP)
		{
			return handle_clamp(ctx, n);
		}
		else if(op == EXPR_OP_FUN_MAX)
		{
			return handle_max(ctx, n);
		}
		else if(op == EXPR_OP_FUN_MIN)
		{
			return handle_min(ctx, n);
		}
		else if(op == EXPR_OP_FUN_LESSTHAN
				|| op == EXPR_OP_FUN_LESSTHANEQUAL
				|| op == EXPR_OP_FUN_EQUAL
				|| op == EXPR_OP_FUN_NOTEQUAL
				|| op == EXPR_OP_FUN_GREATERTHANEQUAL
				|| op == EXPR_OP_FUN_GREATERTHAN)
		{
			return handle_comparison(ctx, n);
		}
		else if(op == EXPR_OP_FUN_NOT)
		{
			return handle_not(ctx, n);
		}
		else if(op == EXPR_OP_FUN_M200_POS)
		{
			return handle_m200_pos(ctx, n);
		}
		else if(op == EXPR_OP_FUN_M200_POINT)
		{
			return handle_m200_point(ctx, n);
		}
		else if(op == EXPR_OP_FUN_M200_MISC_VAL)
		{
			return handle_m200_miscval(ctx, n);
		}
		else if(op == EXPR_OP_FUN_M200_LD_RGB)
		{
			return handle_m200_ldrgb(ctx, n);
		}
		else if(op == EXPR_OP_FUN_M200_LD_ZS)
		{
			return handle_m200_ldzs(ctx, n);
		}
		else if(op == EXPR_OP_FUN_SIN_0_1
				|| op == EXPR_OP_FUN_COS_0_1
				|| op == EXPR_OP_FUN_EXP2
				|| op == EXPR_OP_FUN_LOG2
				|| op == EXPR_OP_FUN_SQRT
				|| op == EXPR_OP_FUN_INVERSESQRT)
		{
			return handle_op_lut(ctx, n);
		}
		else if(op == EXPR_OP_FUN_FLOOR 
				|| op == EXPR_OP_FUN_CEIL
				|| op == EXPR_OP_FUN_FRACT)
		{
			return handle_op_add(ctx, n);
		}
		else if(op == EXPR_OP_FUN_TRUNC)
		{
			return handle_trunc(ctx, n);
		}
		else if(op == EXPR_OP_FUN_M200_MOV_CUBEMAP)
		{
			return handle_cubemap(ctx, n);
		}
		else if(op == EXPR_OP_FUN_M200_HADD)
		{
			return handle_hadd(ctx, n);
		}
		else if(op == EXPR_OP_FUN_M200_HADD_PAIR)
		{
			return handle_haddpair(ctx, n);
		}
		else if(op == EXPR_OP_FUN_NORMALIZE)
		{
			return handle_normalize(ctx, n);
		}
		else if(op == EXPR_OP_FUN_M200_ATAN_IT1
				|| op == EXPR_OP_FUN_M200_ATAN_IT2)
		{
			return handle_atan(ctx, n);
		}
		else if(op == EXPR_OP_FUN_M200_DERX 
				|| op == EXPR_OP_FUN_M200_DERY)
		{
			return handle_derivative(ctx, n);
		}
		else if(_essl_node_is_texture_operation(n))
		{
			return handle_texture(ctx, n);
		}
		else
		{
			assert(0 && "not implemented operation");
			return MEM_ERROR;
		}
	}
	else if(kind == EXPR_KIND_VECTOR_COMBINE)
	{
		return handle_vector_combine(ctx, n);
	}
	else if(kind == EXPR_KIND_CONSTANT)
	{
		return handle_constant(ctx, n);
	}
	else if(kind == EXPR_KIND_LOAD)
	{
		return handle_load(ctx, n);
	}
	else if(kind == EXPR_KIND_STORE)
	{
		return handle_store(ctx, n);
	}
	else if(kind == EXPR_KIND_DEPEND)
	{
		return handle_depend(ctx, n);
	}
	else
	{
		assert(0 && "not implemented operation");
		return MEM_ERROR;
	}
}

static int pressure_for_op_def(node *op)
{
	unsigned n_live = 0;
	if(op->hdr.kind == EXPR_KIND_STORE)
	{
		return 0;
	}
	if(op->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE 
		|| (op->hdr.kind == EXPR_KIND_BINARY && op->expr.operation == EXPR_OP_INDEX) 
		|| (op->hdr.kind == EXPR_KIND_UNARY && op->expr.operation == EXPR_OP_MEMBER))
	{
		return 1;
	}
	if(op->hdr.live_mask == 0)
	{
		n_live = GET_NODE_VEC_SIZE(op);
	} 
	else 
	{
		/* count number of bits in live mask */
		unsigned live = op->hdr.live_mask;
		while(live != 0)
		{
			++n_live;
			live = live & (live - 1); /* clear MSB */
		}
	}
	return n_live;
}

static int count_unscheduled_child(node *n)
{
	node_extra *ex;

	if(n == NULL) 
	{
		return 0;
	}
	ex = EXTRA_INFO(n);
	assert(ex != NULL);
	if(ex->unscheduled_use_count == ex->original_use_count)
	{
		return pressure_for_op_def(n);
	} 
	else 
	{
		return 0;
	}
}


static int get_register_pressure_effect_for_node(node *n)
{
	unsigned i;
	node *child;
	int effect = 0;
	essl_bool handled = ESSL_FALSE;
	if(n == NULL) 
	{
		return 0;
	}
	if(n->hdr.kind == EXPR_KIND_DEPEND)
	{
		return 0;
	}
	if(n->hdr.kind == EXPR_KIND_STORE)
	{
		effect = 0;
		effect += count_unscheduled_child(GET_CHILD(n, 0));
		effect += count_unscheduled_child(GET_CHILD(n, 1));
		return effect;
	}
	else if(n->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL && n->expr.operation == EXPR_OP_FUN_CLAMP)
	{
		/* the constant nodes of a clamp don't count */
		effect += count_unscheduled_child(GET_CHILD(n, 0));
		handled = ESSL_TRUE;
	} 
	else if(_essl_node_is_texture_operation(n))
	{
		node *coord = GET_CHILD(n, 1);
		if(coord->hdr.kind == EXPR_KIND_LOAD && coord->expr.u.load_store.address_space == ADDRESS_SPACE_FRAGMENT_VARYING && GET_CHILD(coord, 0) == NULL)
		{
			for(i = 0; i < GET_N_CHILDREN(n); ++i)
			{
				child = GET_CHILD(n, i);
				if(i != 1)
				{
					effect += count_unscheduled_child(child);
				}
			}
			handled = ESSL_TRUE;
		}
	} 
	else if(n->hdr.kind == EXPR_KIND_TERNARY && n->expr.operation == EXPR_OP_CONDITIONAL_SELECT)
	{
		node *cmp = GET_CHILD(n, 0);
		for(i = 0; i < GET_N_CHILDREN(cmp); ++i)
		{
			child = GET_CHILD(cmp, i);
			effect += count_unscheduled_child(child);
		}

		for(i = 1; i < GET_N_CHILDREN(n); ++i)
		{
			child = GET_CHILD(n, i);
			effect += count_unscheduled_child(child);
		}
		handled = ESSL_TRUE;
	}
	/* regular case */
	if(!handled)
	{
		for(i = 0; i < GET_N_CHILDREN(n); ++i)
		{
			essl_bool already_seen = ESSL_FALSE;
			unsigned j;
			child = GET_CHILD(n, i);
			for(j = 0; j < i; ++j)
			{
				if(GET_CHILD(n, j) == child)
				{
					already_seen = ESSL_TRUE;
					break;
				}
			}
			if(!already_seen)
			{
				effect += count_unscheduled_child(child);
			}
		}
	}
	effect -= pressure_for_op_def(n);

	return effect;
}


static int _essl_mali200_operation_priority(node *n, int current_register_pressure)
{
#ifdef RANDOMIZED_COMPILATION
	return _essl_get_random_int(0, 1000);
#else

	node_extra *ex;
	int base_metric;
	int register_pressure_effect = get_register_pressure_effect_for_node(n);
	int register_pressure_scale = 0;
	ex = EXTRA_INFO(n);
	base_metric = 1*ex->operation_depth;

	if(is_vector_op(n))
	{
		if(n->hdr.kind == EXPR_KIND_BINARY && n->expr.operation == EXPR_OP_MUL &&
		   is_vector_op(GET_CHILD(n, 0)) + is_vector_op(GET_CHILD(n, 1)) == 1)
		{
			/* scalar - vector mul */
			base_metric += 1;
		} 
		else if(n->hdr.kind == EXPR_KIND_UNARY && n->expr.operation == EXPR_OP_IDENTITY)
		{
			/* moves are plentiful, don't prioritize them */
		} 
		else 
		{
			base_metric += 3; /* vector ops are harder to schedule than scalar ops, prioritize */
		}
		
	}

	if(current_register_pressure < 8)
	{
		register_pressure_scale = 8;
	} 
	else if(current_register_pressure < 16)
	{
		register_pressure_scale = 0;
	} 
	else if(current_register_pressure < 24)
	{
		register_pressure_scale = -10;
	} 
	else 
	{
		register_pressure_scale = -30;
	}

	if(n->hdr.kind == EXPR_KIND_STORE)
	{
		base_metric -= 10000;
	}

	return base_metric - 0*ex->earliest_use + register_pressure_scale*register_pressure_effect;
#endif
}


static int data_dependency_delay(/*@null@*/ node *n, node *dependency)
{
	int delay = 0;
	IGNORE_PARAM(dependency);
	if(n != NULL)
	{
		switch(n->hdr.kind)
		{
		case EXPR_KIND_BUILTIN_FUNCTION_CALL:
			switch(n->expr.operation)
			{
			case EXPR_OP_FUN_DFDX: /*@ fall-through @*/
			case EXPR_OP_FUN_DFDY: /*@ fall-through @*/
			case EXPR_OP_FUN_M200_DERX: /*@ fall-through @*/
			case EXPR_OP_FUN_M200_DERY:
				delay += 1; /* gotta wait get them one subcycle early, so we don't mess up by sending the input through the multipliers */
			default:
				break;
			}
			break;
		default:
			break;
		}
	}
	return delay;
}


static memerr make_comparison(mali200_scheduler_context *ctx, node *cond, m200_comparison *op_out, node **constant_out, node **left_out, node **right_out) 
{
	m200_comparison op = M200_CMP_ALWAYS;
	if(cond->hdr.kind == EXPR_KIND_BINARY &&
	   (cond->expr.operation == EXPR_OP_LT ||
		cond->expr.operation == EXPR_OP_LE ||
		cond->expr.operation == EXPR_OP_EQ ||
		cond->expr.operation == EXPR_OP_NE ||
		cond->expr.operation == EXPR_OP_GE ||
		cond->expr.operation == EXPR_OP_GT
		   ))
	{
		ESSL_CHECK(_essl_scheduler_schedule_extra_operation(ctx->sctx, &cond, CYCLE_TO_SUBCYCLE(ctx->earliest_cycle, 0)));
		ESSL_CHECK(*left_out = GET_CHILD(cond, 0));
		ESSL_CHECK(*right_out = GET_CHILD(cond, 1));
		switch(cond->expr.operation)
		{
		case EXPR_OP_LT:
			op = M200_CMP_LT;
			break;
		case EXPR_OP_LE:
			op = M200_CMP_LE;
			break;
		case EXPR_OP_EQ:
			op = M200_CMP_EQ;
			break;
		case EXPR_OP_NE:
			op = M200_CMP_NE;
			break;
		case EXPR_OP_GE:
			op = M200_CMP_GE;
			break;
		case EXPR_OP_GT:
			op = M200_CMP_GT;
			break;

		default:
			assert(0);
			break;
		}

		*constant_out = NULL;
	} else 
	{
		op = M200_CMP_NE;

		*left_out = cond;
		ESSL_CHECK(*right_out = create_zero_node(ctx));			
		--EXTRA_INFO(*right_out)->unscheduled_use_count;
		++EXTRA_INFO(*right_out)->scheduled_use_count;
		*constant_out = *right_out;
	}

	*op_out = op;

	return MEM_OK;
}

static essl_bool is_zero_constant(node *constant) 
{
	unsigned i;
	if (constant->hdr.kind != EXPR_KIND_CONSTANT)
	{
		return ESSL_FALSE;
	}
	for (i = 0 ; i < GET_NODE_VEC_SIZE(constant); i++) 
	{
		if (constant->expr.u.value[i].mali200_float != 0.0f) 
		{
			return ESSL_FALSE;
		}
	}
	return ESSL_TRUE;
}

static essl_bool is_eliminatable_exit_block(mali200_scheduler_context *ctx, basic_block *b)
{
	if(!(b->termination == TERM_KIND_EXIT && b->control_dependent_ops == NULL && (b->source == NULL || b->source->hdr.kind == EXPR_KIND_PHI))) 
	{
		return ESSL_FALSE;
	}
	if(ctx->function == ctx->tu->entry_point || ctx->function->opt.pilot.is_proactive_func)
	{
		return ESSL_TRUE;
	}
	if(b->output_visit_number > 0)
	{
		basic_block *prev_block = ctx->cfg->output_sequence[b->output_visit_number - 1];
		/* if we have a conditional jump with fallthrough into the exit block, we can handle this with embedded end of program marker when we're in the entry point, but not otherwise */
		if(prev_block->termination == TERM_KIND_JUMP && prev_block->source != NULL)
		{
			return ESSL_FALSE;
		}
	}

	return ESSL_TRUE;
}

static essl_bool does_program_perform_conditional_discard(control_flow_graph *cfg)
{
	essl_bool conditional_discard_encountered = ESSL_FALSE;
	unsigned int i;
	for(i = 0; i < cfg->n_blocks; ++i)
	{
		basic_block *b = cfg->output_sequence[i];
		if(b->termination == TERM_KIND_DISCARD && b->source != NULL)
		{
			conditional_discard_encountered = ESSL_TRUE;
		}
	}
	return conditional_discard_encountered;
}


static memerr schedule_block_exit(mali200_scheduler_context *ctx, basic_block *b)
{
	m200_schedule_classes exit_classes = 0;
	if(ctx->program_does_conditional_discard) 
	{
		exit_classes = M200_SC_NO_RENDEZVOUS;
	}
	/* first check to see if we need termination at all */
	/* exit block with no out variables -> ret is done in the return blocks instead */
	if(is_eliminatable_exit_block(ctx, b))
	{
 		ESSL_CHECK(add_word(ctx));
		return MEM_OK;
	}

	switch(b->termination)
	{
	case TERM_KIND_UNKNOWN:
		assert(0);
		break;
	case TERM_KIND_DISCARD:
		ctx->tu->discard_used = ESSL_TRUE;
		if (b->source != 0) 
		{
			/* Conditional discard */
			if (b->source->hdr.kind == EXPR_KIND_BINARY &&
				b->source->expr.operation == EXPR_OP_LT &&
				is_zero_constant(GET_CHILD(b->source, 1)))
			{
				/* Alternative pixel kill */
				ESSL_CHECK(_essl_scheduler_schedule_extra_operation(ctx->sctx, &b->source, CYCLE_TO_SUBCYCLE(ctx->earliest_cycle, 0)));
				ESSL_CHECK(_essl_scheduler_schedule_extra_operation(ctx->sctx, GET_CHILD_ADDRESS(b->source, 1), CYCLE_TO_SUBCYCLE(ctx->earliest_cycle, 0)));
				ESSL_CHECK(schedule(ctx, NULL, NULL, NULL, M200_SC_BRANCH, M200_ALT_KILL, NULL, GET_CHILD(b->source, 0)));
			} 
			else 
			{
				/* Normal pixel kill */
				m200_comparison op;
				node *constant;
				node *left;
				node *right;
				assert(GET_NODE_VEC_SIZE(b->source) == 1);
				ESSL_CHECK(make_comparison(ctx, b->source, &op, &constant, &left, &right));
				ESSL_CHECK(schedule(ctx, NULL, constant, NULL, M200_SC_BRANCH, M200_KILL, NULL, op, left, right));
			}
		} 
		else 
		{
			/* Unconditional discard */
			m200_instruction_word *last_word;
			ESSL_CHECK(schedule(ctx, NULL, NULL, NULL, M200_SC_BRANCH|exit_classes, M200_KILL, NULL, M200_CMP_ALWAYS, NULL, NULL));
			last_word = ctx->latest_instruction_word;
			assert(last_word != NULL);
			last_word->end_of_program_marker = ESSL_TRUE; /* KILL does not terminate execution, therefore we add an end flag */
		}
		break;
	case TERM_KIND_JUMP:
		if (b->source != 0) 
		{
			/* Conditional jump */
			essl_bool invert = ESSL_FALSE;
			if (b->successors[BLOCK_TRUE_TARGET]->output_visit_number == b->output_visit_number + 1)
			{
				basic_block *tmp = b->successors[BLOCK_TRUE_TARGET];
				b->successors[BLOCK_TRUE_TARGET] = b->successors[BLOCK_DEFAULT_TARGET];
				b->successors[BLOCK_DEFAULT_TARGET] = tmp;
				invert = ESSL_TRUE;
			
			}
			assert(b->successors[BLOCK_DEFAULT_TARGET]->output_visit_number == b->output_visit_number + 1);

			{
				m200_comparison op;
				node *constant;
				node *left;
				node *right;
				essl_bool will_exit = (ctx->function == ctx->tu->entry_point || ctx->function->opt.pilot.is_proactive_func) && is_eliminatable_exit_block(ctx, b->successors[BLOCK_DEFAULT_TARGET]);
				ESSL_CHECK(make_comparison(ctx, b->source, &op, &constant, &left, &right));
				if(invert) 
				{
					op = invert_comparison(op);
				}
				if(is_eliminatable_exit_block(ctx, b->successors[BLOCK_TRUE_TARGET]))
				{
					if(ctx->desc->has_entry_point && ctx->function != ctx->tu->entry_point && !ctx->function->opt.pilot.is_proactive_func)
					{
						ESSL_CHECK(schedule(ctx, NULL, NULL, NULL, M200_SC_BRANCH|M200_SC_LOAD|M200_SC_LSB_VECTOR_INPUT, M200_RET, NULL, op, left, right, M200_LOAD_RET, NULL, ctx->function));
					} 
					else 
					{
						ESSL_CHECK(schedule(ctx, NULL, constant, NULL, M200_SC_BRANCH|exit_classes, M200_GLOB_END, NULL, op, left, right));
					}
				} 
				else 
				{
					m200_schedule_classes classes = M200_SC_BRANCH;
					if(will_exit)
					{
						classes |= exit_classes;
					}
					ESSL_CHECK(schedule(ctx, NULL, constant, NULL, classes, M200_JMP, NULL, b->successors[BLOCK_TRUE_TARGET], op, left, right));
				}
				if(will_exit)
				{
					/* attach an end flag to the end of this block */
					m200_instruction_word *word = ctx->latest_instruction_word;
					assert(word != NULL);
					assert(ctx->function == ctx->tu->entry_point || ctx->function->opt.pilot.is_proactive_func);
					word->end_of_program_marker = ESSL_TRUE;
						
				}
			}
			break;
		} 
		else if (!is_eliminatable_exit_block(ctx, b->successors[BLOCK_DEFAULT_TARGET]))
		{

			/* This is either a normal jump or a jump to an exit block containing control-dependent operations. */
			if(b->successors[BLOCK_DEFAULT_TARGET]->output_visit_number == b->output_visit_number + 1)
			{
				/* jump to the next block -> no-op */
				ESSL_CHECK(add_word(ctx));
				break;
			}
			ESSL_CHECK(schedule(ctx, NULL, NULL, NULL, M200_SC_BRANCH, M200_JMP, NULL, b->successors[BLOCK_DEFAULT_TARGET], M200_CMP_ALWAYS, NULL, NULL));
			break;
		}
		/* This is a jump to an exit block with no control-dependent operations. We can just return directly. */

		/*@ fallthrough @*/
	case TERM_KIND_EXIT:
		if(ctx->desc->has_entry_point && ctx->function != ctx->tu->entry_point && !ctx->function->opt.pilot.is_proactive_func)
		{
			ESSL_CHECK(schedule(ctx, NULL, NULL, NULL, M200_SC_BRANCH|M200_SC_LOAD|M200_SC_LSB_VECTOR_INPUT, M200_RET, NULL, M200_CMP_ALWAYS, NULL, NULL, M200_LOAD_RET, NULL, ctx->function));
		} 
		else 
		{
			/* don't do a RET, just insert a word with the end of program marker flag set */
			m200_instruction_word *word;
			ESSL_CHECK(word = add_word(ctx));
			word->used_slots |= exit_classes;
			word->end_of_program_marker = ESSL_TRUE;
		}
		break;
	}
	return MEM_OK;
}



memerr _essl_mali200_schedule_function(mempool *pool, translation_unit *tu, symbol *function, mali200_relocation_context *relocation_context, error_context *error_context)
{
	scheduler_context sched_ctx, *sctx = &sched_ctx;
	mali200_scheduler_context m200_ctx, *ctx = &m200_ctx;
	ctx->sctx = sctx;
	ctx->pool = pool;
	ctx->tu = tu;
	ctx->desc = tu->desc;
	ctx->can_add_cycles = ESSL_TRUE;
	ctx->earliest_instruction_word = 0;
	ctx->latest_instruction_word = 0;
	ctx->earliest_cycle = 1;
	ctx->function = function;
	ctx->cfg = function->control_flow_graph;
	ctx->relocation_context = relocation_context;
	ctx->error_context = error_context;
	ctx->program_does_conditional_discard = does_program_perform_conditional_discard(ctx->cfg);
	ESSL_CHECK(_essl_scheduler_init(sctx, ctx->pool, ctx->cfg, _essl_mali200_operation_priority, ctx));
	_essl_scheduler_set_data_dependency_delay_callback(sctx, data_dependency_delay);


	while(_essl_scheduler_more_blocks(sctx))
	{
		basic_block *b;
		ESSL_CHECK(b = _essl_scheduler_begin_block(sctx, CYCLE_TO_SUBCYCLE(ctx->earliest_cycle, 0)));
		b->bottom_cycle = ctx->earliest_cycle;

		ctx->earliest_instruction_word = 0;
		ctx->latest_instruction_word = 0;
		ctx->can_add_cycles = ESSL_TRUE;
		ESSL_CHECK(schedule_block_exit(ctx, b));
		while(!_essl_scheduler_block_complete(sctx))
		{
			node *op = _essl_scheduler_next_operation(sctx);
			ESSL_CHECK(mali200_schedule_single_operation(ctx, op));
			
		}

		
		
		while(_essl_scheduler_more_operations(sctx))
		{
			node *op = _essl_scheduler_next_operation(sctx);

			ctx->can_add_cycles = ESSL_FALSE;
			if(op->expr.earliest_block == b || op->expr.best_block == b)
			{
				ctx->can_add_cycles = ESSL_TRUE;
			}
			if(op->expr.earliest_block == NULL) 
			{
				ctx->can_add_cycles = ESSL_TRUE;
			}

			ESSL_CHECK(mali200_schedule_single_operation(ctx, op));
			
		}

		assert(_essl_scheduler_block_complete(sctx));
		b->earliest_instruction_word = ctx->earliest_instruction_word;
		b->latest_instruction_word = ctx->latest_instruction_word;
		ESSL_CHECK(_essl_scheduler_finish_block(sctx));

		b->top_cycle = ctx->earliest_cycle-1;
	}

	return MEM_OK;
}
