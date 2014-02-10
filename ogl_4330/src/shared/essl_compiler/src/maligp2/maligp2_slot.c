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
#include "backend/abstract_scheduler.h"
#include "backend/extra_info.h"
#include "maligp2/maligp2_instruction.h"
#include "maligp2/maligp2_scheduler.h"
#include "maligp2/maligp2_slot.h"


/* 
   MaliGP2 shares the opcode field between the two adders and the two multipliers. These functions determine whether 
   two abstract operations can be encoded using the MaliGP2 opcode field for respectively the adders and the multipliers

*/

static essl_bool add_slot_match(maligp2_opcode op0, maligp2_opcode op1)
{
	
	if(op1 == MALIGP2_SET_IF_BOTH) return ESSL_FALSE;
	if(op0 == MALIGP2_NOP || op1 == MALIGP2_NOP) return ESSL_TRUE;
/*	if(op0 == MALIGP2_SET_IF_BOTH && op1 != MALIGP2_MAX && op1 != MALIGP2_MOV) return ESSL_FALSE; */
	if(op0 == MALIGP2_SET_IF_BOTH && op1 != MALIGP2_MAX) return ESSL_FALSE;
	if(op0 == op1) return ESSL_TRUE;
/* disabling moves with max/min, as the bypass allocator does not know how to make sure that input to moves don't come from the lut unit in these cases 
	if(op0 == MALIGP2_MOV && (op1 == MALIGP2_ADD || op1 == MALIGP2_MAX || op1 == MALIGP2_MIN)) return ESSL_TRUE;
	if(op1 == MALIGP2_MOV && (op0 == MALIGP2_ADD || op0 == MALIGP2_MAX || op0 == MALIGP2_MIN)) return ESSL_TRUE;
*/
	if(op0 == MALIGP2_MOV && (op1 == MALIGP2_ADD)) return ESSL_TRUE;
	if(op1 == MALIGP2_MOV && (op0 == MALIGP2_ADD)) return ESSL_TRUE;
	return ESSL_FALSE;
}

static int n_add_moves_available(maligp2_opcode op0, maligp2_opcode op1)
{
	int n = 0;
	if(op0 == MALIGP2_NOP && add_slot_match(MALIGP2_MOV, op1)) ++n;
	if(op1 == MALIGP2_NOP && add_slot_match(op0, MALIGP2_MOV)) ++n;
	return n;
}

static essl_bool mul_slot_match(maligp2_opcode op0, maligp2_opcode op1)
{
	if(op1 == MALIGP2_MUL_EXPWRAP) return ESSL_FALSE;
	if(op0 == MALIGP2_NOP || op1 == MALIGP2_NOP) return ESSL_TRUE;
	if(op0 == MALIGP2_MOV) return mul_slot_match(MALIGP2_MUL, op1);
	if(op1 == MALIGP2_MOV) return mul_slot_match(op0, MALIGP2_MUL);
	if(op0 == MALIGP2_MUL_EXPWRAP && op1 == MALIGP2_MUL) return ESSL_TRUE;
	
	return (op0 == op1);
}

static int n_mul_moves_available(maligp2_opcode op0, maligp2_opcode op1)
{
	int n = 0;
	if(op0 == MALIGP2_NOP && mul_slot_match(MALIGP2_MOV, op1)) ++n;
	if(op1 == MALIGP2_NOP && mul_slot_match(op0, MALIGP2_MOV)) ++n;
	return n;
}

static maligp2_schedule_classes insert_add_op(maligp2_instruction_word *word, maligp2_opcode op, int *moves_lost)
{
	int prev_moves_available = n_add_moves_available(word->add_opcodes[0], word->add_opcodes[1]);
	if(word->add_opcodes[1] == MALIGP2_NOP && add_slot_match(word->add_opcodes[0], op))
	{
		*moves_lost = prev_moves_available - n_add_moves_available(word->add_opcodes[0], op);
		if(*moves_lost <= word->n_moves_available)
		{
			word->add_opcodes[1] = op;
			word->n_moves_available -= *moves_lost;
			word->used_slots |= MALIGP2_SC_ADD1;
			return MALIGP2_SC_ADD1;
		}
	}

	if(word->add_opcodes[0] == MALIGP2_NOP && add_slot_match(op, word->add_opcodes[1]))
	{
		*moves_lost = prev_moves_available - n_add_moves_available(op, word->add_opcodes[1]);
		if(*moves_lost <= word->n_moves_available)
		{
			word->add_opcodes[0] = op;
			word->n_moves_available -= *moves_lost;
			word->used_slots |= MALIGP2_SC_ADD0;
			return MALIGP2_SC_ADD0;
		}
	}

	return 0;
}

static void rollback_add_op(maligp2_instruction_word *word, int selected_slot, int moves_lost)
{
	word->used_slots &= ~selected_slot;
	word->add_opcodes[selected_slot == MALIGP2_SC_ADD1] = MALIGP2_NOP;
	word->n_moves_available += moves_lost;
	
}

static maligp2_schedule_classes insert_mul_op(maligp2_instruction_word *word, maligp2_opcode op, int *moves_lost)
{
	int prev_moves_available = n_mul_moves_available(word->mul_opcodes[0], word->mul_opcodes[1]);
	if(word->mul_opcodes[1] == MALIGP2_NOP && mul_slot_match(word->mul_opcodes[0], op))
	{
		*moves_lost = prev_moves_available - n_mul_moves_available(word->mul_opcodes[0], op);
		if(*moves_lost <= word->n_moves_available)
		{
			word->mul_opcodes[1] = op;
			word->n_moves_available -= *moves_lost;
			word->used_slots |= MALIGP2_SC_MUL1;
			return MALIGP2_SC_MUL1;
		}
	}

	if(word->mul_opcodes[0] == MALIGP2_NOP && mul_slot_match(op, word->mul_opcodes[1]))
	{
		*moves_lost = prev_moves_available - n_mul_moves_available(op, word->mul_opcodes[1]);
		if(*moves_lost <= word->n_moves_available)
		{
			word->mul_opcodes[0] = op;
			word->n_moves_available -= *moves_lost;
			word->used_slots |= MALIGP2_SC_MUL0;
			return MALIGP2_SC_MUL0;
		}
	}

	return 0;
}

static void rollback_mul_op(maligp2_instruction_word *word, int selected_slot, int moves_lost)
{
	word->used_slots &= ~selected_slot;
	word->mul_opcodes[selected_slot == MALIGP2_SC_MUL1] = MALIGP2_NOP;
	word->n_moves_available += moves_lost;
	
}


typedef struct {
	target_descriptor *desc;
	control_flow_graph *cfg;
	reservation_style res_style;
	int *slots;
	node **transfer_nodes;
	int *address;
	essl_bool no_address_needed_override;
	cse_enable cse_enabled;

} maligp2_slot_context;

static essl_bool _essl_maligp2_allocate_slots_rec(maligp2_slot_context *ctx, maligp2_instruction_word *word, int earliest_use_subcycle, essl_bool same_cycle, int instrs_so_far, va_list arglist);




essl_bool _essl_maligp2_allocate_slots(control_flow_graph *cfg, target_descriptor *desc, maligp2_instruction_word *word, int earliest_use_subcycle, essl_bool same_cycle, int *slots, node **transfer_nodes, reservation_style res_style, cse_enable cse_enabled, int *address, va_list arglist)
{
	maligp2_slot_context context, *ctx = &context;
	ctx->desc = desc;
	ctx->cfg = cfg;
	ctx->res_style = res_style;
	ctx->cse_enabled = cse_enabled;
	ctx->slots = slots;
	ctx->transfer_nodes = transfer_nodes;
	ctx->address = address;
	ctx->no_address_needed_override = ESSL_FALSE;
	return _essl_maligp2_allocate_slots_rec(ctx, word, earliest_use_subcycle, same_cycle, 0, arglist);


}


static essl_bool try_add_slot(maligp2_opcode op, maligp2_slot_context *ctx, maligp2_instruction_word *word, int earliest_use_subcycle, essl_bool same_cycle, int instrs_so_far, va_list arglist)
{
	essl_bool result = ESSL_FALSE;
	int moves_lost = 0;
	maligp2_schedule_classes selected_slot = insert_add_op(word, op, &moves_lost);
	if(selected_slot) 
	{
		ctx->slots[instrs_so_far] = selected_slot;
		result = _essl_maligp2_allocate_slots_rec(ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far+1, arglist);
		assert(word->add_opcodes[selected_slot == MALIGP2_SC_ADD1] == op);
		assert(word->used_slots & selected_slot);
		if(!result) rollback_add_op(word, selected_slot, moves_lost);
		
	}
	return result;


}


static essl_bool try_mul_slot(maligp2_opcode op, maligp2_slot_context *ctx, maligp2_instruction_word *word, int earliest_use_subcycle, essl_bool same_cycle, int instrs_so_far, va_list arglist)
{
	essl_bool result = ESSL_FALSE;
	int moves_lost = 0;
	maligp2_schedule_classes selected_slot = insert_mul_op(word, op, &moves_lost);
	if(selected_slot) 
	{
		ctx->slots[instrs_so_far] = selected_slot;
		result = _essl_maligp2_allocate_slots_rec(ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far+1, arglist);
		assert(word->mul_opcodes[selected_slot == MALIGP2_SC_MUL1] == op);
		assert(word->used_slots & selected_slot);
		if(!result) rollback_mul_op(word, selected_slot, moves_lost);
		
	}
	return result;


}

static essl_bool try_load_slot(maligp2_schedule_classes wanted_slots, node *res, maligp2_slot_context *ctx, maligp2_instruction_word *word, int earliest_use_subcycle, essl_bool same_cycle, int instrs_so_far, va_list arglist)
{
	essl_bool result = ESSL_FALSE;
	maligp2_schedule_classes orig_used_slots = word->used_slots;
	essl_bool orig_no_address_needed_override = ctx->no_address_needed_override;
	result = (word->used_slots & wanted_slots) == 0;
	if(!result)
	{
		/* the slot is already partially used, see if the swizzle we want is available */
		unsigned i;
		node_extra *ne;
		maligp2_instruction **arr = 0;
		const symbol_list *address_symbols = 0;
		node *address_arg = 0;
		int address_offset_divided = 0;
		int address_reg = -1;
		switch(wanted_slots)
		{
		case MALIGP2_SC_LOAD_CONST:
			arr = word->u.real_slots.load_const;
			break;
		case MALIGP2_SC_LOAD0:
			arr = word->u.real_slots.load0;
			break;
		case MALIGP2_SC_LOAD1:
			arr = word->u.real_slots.load1;
			break;
		default:
			assert(0);
			break;
			
		}

		ESSL_CHECK(ne = EXTRA_INFO(res));
		assert(ne->address_symbols != 0);

		for(i = 0; i < MALIGP2_NATIVE_VECTOR_SIZE; ++i)
		{
			node_extra *le;
			if(arr[i] != 0 && arr[i]->instr_node != 0)
			{
				if(arr[i]->opcode == MALIGP2_CONSTANT || arr[i]->opcode == MALIGP2_LOCKED_CONSTANT)
				{
					/* a constant cannot work with a load, returning false */
					return ESSL_FALSE;
				}
				ESSL_CHECK(le = EXTRA_INFO(arr[i]->instr_node));
				if(address_symbols == 0)
				{
					address_symbols = le->address_symbols;
					address_offset_divided = le->address_offset/4;
					address_arg = GET_CHILD(arr[i]->instr_node, 0);
					address_reg = arr[i]->address_reg;

				}
				assert(_essl_address_symbol_lists_equal(address_symbols, le->address_symbols));
				assert(address_offset_divided == le->address_offset/4);
				assert(address_arg == GET_CHILD(arr[i]->instr_node, 0));
				assert(address_reg == arr[i]->address_reg);

			}
			
		}
		assert(address_symbols != 0);
		if(_essl_address_symbol_lists_equal(address_symbols, ne->address_symbols) && address_offset_divided == ne->address_offset/4 && address_arg == GET_CHILD(res, 0))
		{
			ctx->no_address_needed_override = ESSL_TRUE;
			*ctx->address = address_reg;
			if(arr[ne->address_offset&3] == 0)
			{
				/* free sub-slot */
				result = ESSL_TRUE;
			} else {
				/* can do transfer node stuff */

			}


		}
		
		
	}
	if(result)
	{
		word->used_slots |= wanted_slots;
		ctx->slots[instrs_so_far] = wanted_slots;
		result = _essl_maligp2_allocate_slots_rec(ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far+1, arglist);
		assert(word->used_slots & wanted_slots);
		if(!result)
		{
			word->used_slots = orig_used_slots;
			ctx->slots[instrs_so_far] = 0;
		}
		ctx->no_address_needed_override = orig_no_address_needed_override;

		
	}
	return result;

}


static essl_bool try_slot(maligp2_schedule_classes wanted_slots, maligp2_slot_context *ctx, maligp2_instruction_word *word, int earliest_use_subcycle, essl_bool same_cycle, int instrs_so_far, va_list arglist)
{
	essl_bool result = ESSL_FALSE;
	if((word->used_slots & wanted_slots) == 0 && ((wanted_slots & MALIGP2_SC_MISC) == 0 || word->n_moves_available > 0))
	{
		word->used_slots |= wanted_slots;
		ctx->slots[instrs_so_far] = wanted_slots;
		if(wanted_slots & MALIGP2_SC_MISC) --word->n_moves_available;
		result = _essl_maligp2_allocate_slots_rec(ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far+1, arglist);
		assert(word->used_slots & wanted_slots);
		if(!result)
		{
			if(wanted_slots & MALIGP2_SC_MISC) ++word->n_moves_available;
			word->used_slots &= ~wanted_slots;
			ctx->slots[instrs_so_far] = 0;
		}
		
	}
	return result;

}

static maligp2_schedule_classes slot_for_address(int addr)
{
	switch(addr)
	{
	case MALIGP2_ADDR_A0_X:
		return MALIGP2_SC_A0_X;
	case MALIGP2_ADDR_A0_Y:
		return MALIGP2_SC_A0_Y;
	case MALIGP2_ADDR_A0_Z:
		return MALIGP2_SC_A0_Z;
	case MALIGP2_ADDR_A0_W:
		return MALIGP2_SC_A0_W;
	default:
		assert(0);
		return 0;

	}
}

static essl_bool alloc_address_slot(int wanted_address, maligp2_schedule_classes acceptable_slots, unsigned n_words_wanted, maligp2_instruction_word *word, int *address)
{
	unsigned i;
	maligp2_instruction_word *w;
	maligp2_schedule_classes wanted_slot = slot_for_address(wanted_address);
	if((wanted_slot & acceptable_slots) == 0) return ESSL_FALSE;
	for(w = word, i = 0; w != 0 && i < n_words_wanted; w = w->successor, ++i)
	{
		if(w->used_slots & wanted_slot)	return ESSL_FALSE;
	}
	if(i != n_words_wanted) return ESSL_FALSE;


	for(w = word, i = 0; w != 0 && i < n_words_wanted; w = w->successor, ++i)
	{
		w->used_slots |= wanted_slot;
	}
	*address = wanted_address;
	return ESSL_TRUE;


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

static essl_bool nodes_equal(maligp2_slot_context *ctx, node *a, node *b)
{
	unsigned i, n;
	if(a == 0 || b == 0) return ESSL_FALSE;
	if(a == b) return ESSL_TRUE;
	if(a->hdr.kind != b->hdr.kind) return ESSL_FALSE;
	if(GET_N_CHILDREN(a) != GET_N_CHILDREN(b)) return ESSL_FALSE;
	n = GET_N_CHILDREN(a);
	for(i = 0; i < n; ++i)
	{
		if(GET_CHILD(a, i) != NULL || GET_CHILD(b, i) != NULL) return ESSL_FALSE;
	}
	if(a->hdr.kind == EXPR_KIND_BINARY || a->hdr.kind == EXPR_KIND_TERNARY || a->hdr.kind == EXPR_KIND_BUILTIN_FUNCTION_CALL)
	{
		return a->expr.operation == b->expr.operation;
	}

	if(a->hdr.kind == EXPR_KIND_CONSTANT)
	{
		assert(_essl_get_type_size(a->hdr.type) == 1);
		return float_equals(ctx->desc->scalar_to_float(a->expr.u.value[0]),
							ctx->desc->scalar_to_float(b->expr.u.value[0]));
	}

	if(a->hdr.kind == EXPR_KIND_LOAD)
	{
		node_extra *na, *nb;
		ESSL_CHECK(na = EXTRA_INFO(a));
		ESSL_CHECK(nb = EXTRA_INFO(b));
		if(_essl_address_symbol_lists_equal(na->address_symbols, nb->address_symbols) && na->address_offset == nb->address_offset)
		{
			return ESSL_TRUE;
		}
	}
	return ESSL_FALSE;

}

static essl_bool try_replace_with_transfer(maligp2_schedule_classes wanted_slots, node *res, maligp2_slot_context *ctx, maligp2_instruction_word *word, int earliest_use_subcycle, essl_bool same_cycle, int instrs_so_far, va_list arglist)
{
	unsigned i;
	node *replace_node = 0;
	if(ctx->cse_enabled == MALIGP2_DONT_PERFORM_CSE) return ESSL_FALSE;
	if(!replace_node && (wanted_slots & MALIGP2_SC_LOAD0))
	{
		for(i = 0; i < MALIGP2_NATIVE_VECTOR_SIZE; ++i)
		{
			if(word->u.real_slots.load0[i] != 0 && nodes_equal(ctx, res, word->u.real_slots.load0[i]->instr_node))
			{
				replace_node = word->u.real_slots.load0[i]->instr_node;
				break;
			}
		}
	}

	if(!replace_node && (wanted_slots & MALIGP2_SC_LOAD1))
	{
		for(i = 0; i < MALIGP2_NATIVE_VECTOR_SIZE; ++i)
		{
			if(word->u.real_slots.load1[i] != 0 && nodes_equal(ctx, res, word->u.real_slots.load1[i]->instr_node))
			{
				replace_node = word->u.real_slots.load1[i]->instr_node;
				break;
			}
		}
	}

	if(!replace_node && (wanted_slots & MALIGP2_SC_LOAD_CONST))
	{
		for(i = 0; i < MALIGP2_NATIVE_VECTOR_SIZE; ++i)
		{
			if(word->u.real_slots.load_const[i] != 0 && nodes_equal(ctx, res, word->u.real_slots.load_const[i]->instr_node))
			{
				replace_node = word->u.real_slots.load_const[i]->instr_node;
				break;
			}
		}
	}

	if(!replace_node && (wanted_slots & MALIGP2_SC_ADD0))
	{
		if(word->u.real_slots.add0 != 0 && nodes_equal(ctx, res, word->u.real_slots.add0->instr_node))
		{
			replace_node = word->u.real_slots.add0->instr_node;
		}
	}
	if(!replace_node && (wanted_slots & MALIGP2_SC_ADD1))
	{
		if(word->u.real_slots.add1 != 0 && nodes_equal(ctx, res, word->u.real_slots.add1->instr_node))
		{
			replace_node = word->u.real_slots.add1->instr_node;
		}

	}


	if(!replace_node && (wanted_slots & MALIGP2_SC_MUL0))
	{
		if(word->u.real_slots.mul0 != 0 && nodes_equal(ctx, res, word->u.real_slots.mul0->instr_node))
		{
			replace_node = word->u.real_slots.mul0->instr_node;
		}
	}
	if(!replace_node && (wanted_slots & MALIGP2_SC_MUL1))
	{
		if(word->u.real_slots.mul1 != 0 && nodes_equal(ctx, res, word->u.real_slots.mul1->instr_node))
		{
			replace_node = word->u.real_slots.mul1->instr_node;
		}

	}


	if(!replace_node && (wanted_slots & MALIGP2_SC_MISC))
	{
		if(word->u.real_slots.misc != 0 && nodes_equal(ctx, res, word->u.real_slots.misc->instr_node))
		{
			replace_node = word->u.real_slots.misc->instr_node;
		}

	}

	if(!replace_node && (wanted_slots & MALIGP2_SC_LUT))
	{
		if(word->u.real_slots.lut != 0 && nodes_equal(ctx, res, word->u.real_slots.lut->instr_node))
		{
			replace_node = word->u.real_slots.lut->instr_node;
		}

	}

	if(replace_node != 0)
	{
		essl_bool result = _essl_maligp2_allocate_slots_rec(ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far+1, arglist);
		if(result)
		{
			ctx->transfer_nodes[instrs_so_far] = replace_node;
		}
		
		return result;
	} else {
		return ESSL_FALSE;
	}

}

static int find_free_move_slot(maligp2_instruction_word *word, node *n) {
	int i;
	assert(n != 0);
	for (i = 0 ; i < MALIGP2_MAX_MOVES ; i++) {
		if (word->reserved_moves[i] == 0) {
			word->reserved_moves[i] = n;
			++word->n_moves_reserved;
			break;
		}
	}
	assert(i < MALIGP2_MAX_MOVES);
	return i;
}

static void rollback_address_slot(unsigned n_words_wanted, maligp2_instruction_word *word, int *address)
{
	unsigned i;
	maligp2_instruction_word *w;
	maligp2_schedule_classes wanted_slot = slot_for_address(*address);
	for(w = word, i = 0; w != 0 && i < n_words_wanted; w = w->successor, ++i)
	{
		w->used_slots &= ~wanted_slot;
	}
	*address = MALIGP2_ADDR_UNKNOWN;
}


static essl_bool _essl_maligp2_allocate_slots_rec(maligp2_slot_context *ctx, maligp2_instruction_word *word, int earliest_use_subcycle, essl_bool same_cycle, int instrs_so_far, va_list arglist)
{
	int load_subcycle = MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 2);
	int func_subcycle = MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 1);
	int store_subcycle = MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, 0);
	int i;
	unsigned n_slots;
	node **res_ptr = NULL;
	node *res = NULL;
	int wanted_index = -1;
	int reservation_index = -1;

	essl_bool result = ESSL_FALSE;
	maligp2_opcode op = va_arg(arglist, maligp2_opcode);

	assert(instrs_so_far < MALIGP2_MAX_INSTRUCTIONS_PER_BATCH);
	ctx->slots[instrs_so_far] = 0;
	ctx->transfer_nodes[instrs_so_far] = 0;
	switch(op)
	{
		/* ops without result nodes */
	case MALIGP2_FINISH:
		return ESSL_TRUE;

	case MALIGP2_NEXT_CYCLE:
		if(!word->predecessor) return ESSL_FALSE;
		return _essl_maligp2_allocate_slots_rec(ctx, word->predecessor, earliest_use_subcycle, ESSL_FALSE, instrs_so_far, arglist);

	case MALIGP2_SCHEDULE_EXTRA:
		(void)va_arg(arglist, node **);
		return _essl_maligp2_allocate_slots_rec(ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);		


	case MALIGP2_UNKNOWN:
	case MALIGP2_NOP:
		assert(0);
		break;


	default:
		break;

	}


	res_ptr = va_arg(arglist, node **);
	res = res_ptr != NULL ? *res_ptr : NULL;
	/* go through mov reservations and free one if necessary */
	if(res != 0)
	{
		/* don't unreserve for stuff that happens in the load subcycle */
		if(op != MALIGP2_LOCKED_CONSTANT && op != MALIGP2_CONSTANT && op != MALIGP2_LOAD_INPUT_REG && op != MALIGP2_LOAD_WORK_REG && op != MALIGP2_LOAD_CONSTANT && op != MALIGP2_LOAD_CONSTANT_INDEXED && !(op == MALIGP2_RESERVE_MOV_IF_NOT_SAME_WORD && same_cycle))
		{
			for(i = 0; i < MALIGP2_MAX_MOVES; ++i)
			{
				assert (res != 0);
				if(word->reserved_moves[i] == res && !word->move_reservation_fulfilled[i])
				{
					reservation_index = i;
					switch(ctx->res_style)
					{
					case MALIGP2_RESERVATION_DELETE:
						word->reserved_moves[i] = 0;
						--word->n_moves_reserved;
						break;
					case MALIGP2_RESERVATION_FULFILL:
						word->move_reservation_fulfilled[reservation_index] = ESSL_TRUE;
						break;
					case MALIGP2_RESERVATION_KEEP:
						return ESSL_TRUE;
					}
					++word->n_moves_available;
				}
				
			}
		}
	}
	switch(op)
	{
	case MALIGP2_RESERVE_MOV:
	case MALIGP2_RESERVE_MOV_IF_NOT_SAME_WORD:
		ctx->slots[instrs_so_far] = -1;
		if(op == MALIGP2_RESERVE_MOV_IF_NOT_SAME_WORD && same_cycle)
		{
			result = _essl_maligp2_allocate_slots_rec(ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far+1, arglist);
		} else
		{
			if(res != 0)
			{
				if(word->n_moves_available > 0)
				{
					int index = find_free_move_slot(word, res);
					essl_bool redundant_move = ESSL_FALSE;
					ctx->slots[instrs_so_far] = index;
					--word->n_moves_available;
					
					result = _essl_maligp2_allocate_slots_rec(ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far+1, arglist);
					assert(ctx->slots[instrs_so_far] == index);
					assert(word->reserved_moves[ctx->slots[instrs_so_far]] == res);

					if (result && ctx->transfer_nodes[instrs_so_far+1] != NULL)
					{
						/* The node was subsumed by some other node that already has a reservation in this word.
						   If there was already a move reservationn for it, this move is redundant.
						*/
						/* This code assumes that the node that is transferred to was allocated in the
						   command immediately following the MALIGP2_RESERVE_MOV*.
						*/
						node *rep = ctx->transfer_nodes[instrs_so_far+1];
						int i;
						assert(rep != 0);
						for (i = 0 ; i < MALIGP2_MAX_MOVES ; i++)
						{
							if (word->reserved_moves[i] == rep)
							{
								redundant_move = ESSL_TRUE;
								break;
							}
						}
					}

					if (redundant_move || !result)
					{
						/* Either the allocation failed, or the move was redundant.
						   In any case cancel the reservation.
						 */
						++word->n_moves_available;
						--word->n_moves_reserved;
						assert(word->n_moves_available <= MALIGP2_MAX_MOVES);
						assert(word->reserved_moves[index] != 0);
						word->reserved_moves[index] = 0;
					}
				}
			} else {
					result = _essl_maligp2_allocate_slots_rec(ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far+1, arglist);
				

			}
		}
		break;

	case MALIGP2_FLOOR:
	case MALIGP2_SIGN:
		/* add unary */
		(void)va_arg(arglist, node *);
		if(earliest_use_subcycle < func_subcycle)
		{
			/* result = try_replace_with_transfer(MALIGP2_SC_ADD0|MALIGP2_SC_ADD1, res, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist); 
			   if(result) break; */
			result = try_add_slot(op, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
		}

		break;
	case MALIGP2_ADD:
	case MALIGP2_SET_IF_BOTH:
	case MALIGP2_SGE:
	case MALIGP2_SLT:
	case MALIGP2_MIN:
	case MALIGP2_MAX:
		/* add binary */
		(void)va_arg(arglist, node *);
		(void)va_arg(arglist, node *);
		if(earliest_use_subcycle < func_subcycle)
		{
			/*
			result = try_replace_with_transfer(MALIGP2_SC_ADD0|MALIGP2_SC_ADD1, res, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
			if(result) break; */
			result = try_add_slot(op, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
		}
		break;

	case MALIGP2_MUL:
	case MALIGP2_MUL_EXPWRAP:
		/* mul binary */
		(void)va_arg(arglist, node *);
		(void)va_arg(arglist, node *);
		if(earliest_use_subcycle < func_subcycle)
		{
			/*
			result = try_replace_with_transfer(MALIGP2_SC_MUL0|MALIGP2_SC_MUL1, res, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
			if(result) break; */
			result = try_mul_slot(op, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
		}
		break;


	case MALIGP2_MUL_ADD:
	case MALIGP2_CSEL:
		/* mul_add takes one argument (the rest are implicit), csel takes three arguments */
		if(op == MALIGP2_CSEL)
		{
			unsigned i;
			for(i = 0; i < 3; ++i)
			{
				(void)va_arg(arglist, node *);
			}
		} else {
			/* special case for mul_add. make sure that two words exists after the mul_add in the current basic block. otherwise the register allocator will be in trouble */
			if(word->successor == NULL || word->successor->successor == NULL) 
			{
				result = ESSL_FALSE;
				break;
			}
			(void)va_arg(arglist, node *);
		}
		if(earliest_use_subcycle < func_subcycle)
		{
			int moves_lost0 = 0;
			int selected_slot0 = insert_mul_op(word, op, &moves_lost0);
			if(selected_slot0) 
			{
				int moves_lost1 = 0;
				int selected_slot1 = insert_mul_op(word, op, &moves_lost1);
				if(selected_slot1) 
				{
				
					ctx->slots[instrs_so_far] = selected_slot0 | selected_slot1;
					result = _essl_maligp2_allocate_slots_rec(ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far+1, arglist);
					if(!result) rollback_mul_op(word, selected_slot1, moves_lost1);
				}
				if(!result) rollback_mul_op(word, selected_slot0, moves_lost0);

			}
			
		}
		break;

	case MALIGP2_NEGATE:
		(void)va_arg(arglist, node *);
		op = MALIGP2_MOV;
		if(earliest_use_subcycle < func_subcycle)
		{
			result = try_mul_slot(op, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
			if(!result)
			{
				result = try_add_slot(op, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
			}
		}
		break;
	case MALIGP2_MOV:
		(void)va_arg(arglist, node *);
		if(earliest_use_subcycle < func_subcycle)
		{
			result = try_add_slot(op, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
			if(!result)
			{
				result = try_mul_slot(op, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
			}
			if(!result)
			{
				/* note. trying misc as the final slot ensures that fixed point ranges are allocatable no matter where they are in the live range sequence */
				result = try_slot(MALIGP2_SC_MISC, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
			}
		}
		break;

	case MALIGP2_OUTPUT_0:
		if(earliest_use_subcycle < func_subcycle)
		{
			result = try_replace_with_transfer(MALIGP2_SC_LUT, res, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
			if(result) break;
			result = try_slot(MALIGP2_SC_LUT, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
		}
		break;
		

	case MALIGP2_OUTPUT_1:
		if(earliest_use_subcycle < func_subcycle)
		{
			result = try_replace_with_transfer(MALIGP2_SC_LUT, res, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
			if(result) break;
			result = try_slot(MALIGP2_SC_LUT, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
		}
		break;
		
		


	case MALIGP2_RCC:
	case MALIGP2_EX2:
	case MALIGP2_LG2:
	case MALIGP2_RSQ:
	case MALIGP2_RCP:
	case MALIGP2_LOG:
	case MALIGP2_EXP:
	case MALIGP2_MOV_LUT:
		(void)va_arg(arglist, node *);
		if(earliest_use_subcycle < func_subcycle)
		{
			/*
			result = try_replace_with_transfer(MALIGP2_SC_LUT, res, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
			if(result) break;
			*/
			result = try_slot(MALIGP2_SC_LUT, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
		}

		break;

	case MALIGP2_EXP_FLOOR:
	case MALIGP2_FLOOR_LOG:
	case MALIGP2_DIV_EXP_FLOOR_LOG:
	case MALIGP2_FLOAT_TO_FIXED:
	case MALIGP2_FIXED_TO_FLOAT:
	case MALIGP2_CLAMP:
	case MALIGP2_FRAC_FIXED_TO_FLOAT:
	case MALIGP2_MOV_MISC:
	case MALIGP2_RESERVE_MOV_MISC:
		(void)va_arg(arglist, node *);
		if(earliest_use_subcycle < func_subcycle)
		{
			/*
			result = try_replace_with_transfer(MALIGP2_SC_MISC, res, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
			if(result) break;
			*/
			result = try_slot(MALIGP2_SC_MISC, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
		}

		break;



	case MALIGP2_SET_LOAD_ADDRESS:
	case MALIGP2_SET_STORE_ADDRESS:
		(void)va_arg(arglist, node *);
		n_slots = va_arg(arglist, unsigned int);
		if(ctx->no_address_needed_override)
		{
			ctx->slots[instrs_so_far] = 0;
			result = _essl_maligp2_allocate_slots_rec(ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far+1, arglist);

		} else if(earliest_use_subcycle < func_subcycle)
		{
			maligp2_schedule_classes acceptable_slots = MALIGP2_SC_A0_Y | MALIGP2_SC_A0_Z | MALIGP2_SC_A0_W;
			if(op == MALIGP2_SET_STORE_ADDRESS) acceptable_slots = MALIGP2_SC_A0_X;
			if((word->used_slots & MALIGP2_SC_LUT) == 0)
			{
				if(alloc_address_slot(MALIGP2_ADDR_A0_X, acceptable_slots, n_slots, word, ctx->address) ||
				   alloc_address_slot(MALIGP2_ADDR_A0_Y, acceptable_slots, n_slots, word, ctx->address) ||
				   alloc_address_slot(MALIGP2_ADDR_A0_Z, acceptable_slots, n_slots, word, ctx->address) ||
				   alloc_address_slot(MALIGP2_ADDR_A0_W, acceptable_slots, n_slots, word, ctx->address))
				{
					if((result = try_slot(MALIGP2_SC_LUT, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist)) == 0)
					{
						rollback_address_slot(n_slots, word, ctx->address);
					}
				}
					
			} else if((word->used_slots & MALIGP2_SC_MISC) == 0 && word->u.real_slots.lut && word->u.real_slots.lut->opcode == MALIGP2_SET_ADDRESS)
			{
				int possible_address = (1 - (word->u.real_slots.lut->address_reg & 1)) + (word->u.real_slots.lut->address_reg & 2);
				if(alloc_address_slot(possible_address, acceptable_slots, n_slots, word, ctx->address))
				{
					if((result = try_slot(MALIGP2_SC_MISC, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist)) == 0)
					{
						rollback_address_slot(n_slots, word, ctx->address);
					}
				}

			}
		}

		break;



	case MALIGP2_CALL:
	case MALIGP2_CALL_COND:
		(void)va_arg(arglist, symbol *);
		if(earliest_use_subcycle < store_subcycle)
		{
			result = try_slot(MALIGP2_SC_BRANCH, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
		}

		break;

	case MALIGP2_JMP_COND_NOT_CONSTANT:
	case MALIGP2_JMP_COND:
	case MALIGP2_JMP:
	case MALIGP2_LOOP_END:
	case MALIGP2_REPEAT_END:
		(void)va_arg(arglist, basic_block *);
		if(earliest_use_subcycle < store_subcycle)
		{
			result = try_slot(MALIGP2_SC_BRANCH, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
		}

		break;

    case MALIGP2_RET:
	case MALIGP2_LOOP_START:
	case MALIGP2_REPEAT_START:
	case MALIGP2_STORE_SELECT_ADDRESS_REG:
		if(earliest_use_subcycle < store_subcycle)
		{
			result = try_slot(MALIGP2_SC_BRANCH, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
		}
		break;



	case MALIGP2_LOAD_WORK_REG:
		if(earliest_use_subcycle < load_subcycle)
		{
			assert(res != NULL);
			result = try_replace_with_transfer(MALIGP2_SC_LOAD0|MALIGP2_SC_LOAD1, res, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
			if(result) break;
			result = try_load_slot(MALIGP2_SC_LOAD1, res, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
			if(result) break;
			result = try_load_slot(MALIGP2_SC_LOAD0, res, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
		}
		break;


	case MALIGP2_LOAD_INPUT_REG:
		if(earliest_use_subcycle < load_subcycle)
		{
			assert(res != NULL);
			result = try_replace_with_transfer(MALIGP2_SC_LOAD0, res, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
			if(result) break;
			result = try_load_slot(MALIGP2_SC_LOAD0, res, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
		}
		break;

	case MALIGP2_LOAD_CONSTANT_INDEXED:
	case MALIGP2_LOAD_CONSTANT:
		if(earliest_use_subcycle < load_subcycle)
		{
			assert(res != NULL);
			result = try_replace_with_transfer(MALIGP2_SC_LOAD_CONST, res, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
			if(result) break;
			result = try_load_slot(MALIGP2_SC_LOAD_CONST, res, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
		}
		break;

	case MALIGP2_CONSTANT:
		if(earliest_use_subcycle < load_subcycle)
		{
			result = try_replace_with_transfer(MALIGP2_SC_LOAD_CONST, res, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);
			if(result) break;
			if((word->used_slots & MALIGP2_SC_LOAD_CONST) == 0 || word->n_embedded_constants > 0)
			{
				/* it is possible to place constants in this cycle */
				maligp2_schedule_classes old_used_slots = word->used_slots;
				word->used_slots |= MALIGP2_SC_LOAD_CONST;
				if(word->n_embedded_constants < MALIGP2_NATIVE_VECTOR_SIZE)
				{

					ctx->slots[instrs_so_far] = word->n_embedded_constants;
					word->embedded_constants[word->n_embedded_constants++] = res;

					result = _essl_maligp2_allocate_slots_rec(ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far+1, arglist);
					if(!result)
					{
						/* rollback */
						word->embedded_constants[--word->n_embedded_constants] = 0;
						ctx->slots[instrs_so_far] = 0;
						word->used_slots = old_used_slots;

					}
				}
			}
		}
		break;

	case MALIGP2_LOCKED_CONSTANT:
		wanted_index = va_arg(arglist, int);
		assert(wanted_index >= 0 && wanted_index < 4);
		if(earliest_use_subcycle < load_subcycle)
		{
			if((word->used_slots & MALIGP2_SC_LOAD_CONST) == 0 || word->n_embedded_constants > 0)
			{
				/* it is possible to place constants in this cycle */
				maligp2_schedule_classes old_used_slots = word->used_slots;
				int old_was_locked = word->embedded_constants_locked;
				int old_n_embedded_constants = word->n_embedded_constants;
				word->used_slots |= MALIGP2_SC_LOAD_CONST;
				word->embedded_constants_locked = 1;
				if(word->n_embedded_constants < MALIGP2_NATIVE_VECTOR_SIZE && word->n_embedded_constants <= wanted_index)
				{

					ctx->slots[instrs_so_far] = wanted_index;
					while(word->n_embedded_constants < wanted_index)
					{
						word->embedded_constants[word->n_embedded_constants++] = NULL;
					}
					word->embedded_constants[word->n_embedded_constants++] = res;

					result = _essl_maligp2_allocate_slots_rec(ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far+1, arglist);
					if(!result)
					{
						/* rollback */
						word->embedded_constants[word->n_embedded_constants] = NULL;
						word->n_embedded_constants = old_n_embedded_constants;
						word->embedded_constants_locked = old_was_locked;
						ctx->slots[instrs_so_far] = 0;
						word->used_slots = old_used_slots;

					}
				}
			}
		}
		break;


	case MALIGP2_STORE_WORK_REG:
	case MALIGP2_STORE_OUTPUT_REG:
	case MALIGP2_STORE_CONSTANT:
		if(earliest_use_subcycle < store_subcycle)
	    {
			int wanted_slots = MALIGP2_SC_STORE_XY | MALIGP2_SC_STORE_ZW;
			(void)va_arg(arglist, node*);
			(void)va_arg(arglist, node*);
			(void)va_arg(arglist, node*);
			(void)va_arg(arglist, node*);
			result = try_slot(wanted_slots, ctx, word, earliest_use_subcycle, same_cycle, instrs_so_far, arglist);

		}
		break;

	case MALIGP2_SET_ADDRESS:
	case MALIGP2_FINISH:
	case MALIGP2_NEXT_CYCLE:
	case MALIGP2_SCHEDULE_EXTRA:
	case MALIGP2_UNKNOWN:
	case MALIGP2_NOP:
		assert(0);
		break;
	}
	if(!result)
	{
		/* rollback */

		if(reservation_index >= 0)
		{
			assert(res != 0);
			switch(ctx->res_style)
			{
			case MALIGP2_RESERVATION_DELETE:
				find_free_move_slot(word, res);
				break;
			case MALIGP2_RESERVATION_FULFILL:
				word->move_reservation_fulfilled[reservation_index] = ESSL_FALSE;
				break;
			case MALIGP2_RESERVATION_KEEP:
				assert(0);
				break;
			}
			assert(word->n_moves_available <= MALIGP2_MAX_MOVES);
			--word->n_moves_available;

		}
		ctx->slots[instrs_so_far] = 0;



	}

	return result;
}

static maligp2_schedule_classes allocate_move_helper(control_flow_graph *cfg, target_descriptor *desc, maligp2_instruction_word *word,
													 int *slots, reservation_style res_style, ...)
{
	essl_bool ret;
	int address_reg;
	node *transfer_nodes[MALIGP2_MAX_INSTRUCTIONS_PER_BATCH] = {0};
	va_list arglist;
	va_start(arglist, res_style);

	ret = _essl_maligp2_allocate_slots(cfg, desc, word, 0, 0, slots, transfer_nodes, res_style, MALIGP2_DONT_PERFORM_CSE, &address_reg, arglist);
	va_end(arglist);
	assert(transfer_nodes[0] == 0);
	return ret;
}

maligp2_schedule_classes _essl_maligp2_allocate_move(control_flow_graph *cfg, target_descriptor *desc, maligp2_instruction_word *word,
													 reservation_style res_style, maligp2_opcode opcode, node *src, node *dst)
{
	int slots[MALIGP2_MAX_INSTRUCTIONS_PER_BATCH];
	int ret = allocate_move_helper(cfg, desc, word, slots, res_style, opcode, &dst, src, MALIGP2_FINISH);
	if(!ret) return 0;
	return (maligp2_schedule_classes)slots[0];
}

essl_bool _essl_maligp2_reserve_move(control_flow_graph *cfg, target_descriptor *desc, maligp2_instruction_word *word, node *n) {
	int slots[MALIGP2_MAX_INSTRUCTIONS_PER_BATCH];
	return allocate_move_helper(cfg, desc, word, slots, MALIGP2_RESERVATION_KEEP, MALIGP2_RESERVE_MOV, &n, MALIGP2_FINISH);
}


void _essl_maligp2_demote_misc_move_reservation(control_flow_graph *cfg, target_descriptor *desc, maligp2_instruction_word *word)
{
	int slots[MALIGP2_MAX_INSTRUCTIONS_PER_BATCH];
	node *n;
	assert(word->u.real_slots.misc != NULL && word->u.real_slots.misc->opcode == MALIGP2_RESERVE_MOV_MISC);
	n = word->u.real_slots.misc->args[0].arg;
	assert(n != NULL);
	word->u.real_slots.misc = NULL;
	word->used_slots &= ~MALIGP2_SC_MISC;
	++word->n_moves_available;
	
	allocate_move_helper(cfg, desc, word, slots, MALIGP2_RESERVATION_KEEP, MALIGP2_RESERVE_MOV, &n, MALIGP2_FINISH);
}
