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
#include "maligp2/maligp2_instruction.h"
#include "maligp2/maligp2_scheduler.h"
#include "backend/extra_info.h"

/*@null@*/ maligp2_instruction *_essl_new_maligp2_instruction(mempool *pool, 
									maligp2_schedule_classes sc, maligp2_opcode opcode, int subcycle)
{
	unsigned i;
	maligp2_instruction *inst;
	ESSL_CHECK(inst = _essl_mempool_alloc(pool, sizeof(maligp2_instruction)));
	inst->opcode = opcode;
	inst->instr_node = 0;
	inst->schedule_class = sc;
	for (i = 0; i < MALIGP2_MAX_INPUT_ARGS; ++i) {
		inst->args[i].reg_index = MALIGP2_REG_UNKNOWN;
		assert(inst->args[i].reg_index == MALIGP2_REG_UNKNOWN); /* this actually found a problem with armcc, bitfields and -1, so don't remove */
		inst->args[i].arg = 0;
	}
	inst->address_offset = -1;
	inst->address_reg = MALIGP2_ADDR_UNKNOWN;
	inst->jump_target = 0;
	inst->call_target = 0;
	inst->subcycle = subcycle;

	return inst;
}


/*@null@*/ maligp2_instruction_word *_essl_new_maligp2_instruction_word(mempool *pool, int cycle)
{
	maligp2_instruction_word *word;
	int i;
	ESSL_CHECK(word = _essl_mempool_alloc(pool, sizeof(maligp2_instruction_word)));
	word->used_slots = 0;
	word->predecessor = 0;
	word->successor = 0;
	word->cycle = cycle;
	word->n_moves_available = 5;
	word->n_moves_reserved = 0;
	word->emit_address = -1;
	for(i = 0; i < 2; ++i)
	{
		word->add_opcodes[i] = MALIGP2_NOP;
		word->mul_opcodes[i] = MALIGP2_NOP;
	}
	return word;
}


int _essl_maligp2_get_mul_slot_opcode(maligp2_opcode op0, maligp2_opcode op1)
{
	if(op0 == MALIGP2_NOP && op1 == MALIGP2_NOP) return 0; /* mul */
	if(op0 == MALIGP2_MUL_EXPWRAP && (op1 == MALIGP2_MUL || op1 == MALIGP2_NOP)) return 3;
	if(op0 == MALIGP2_NOP) return _essl_maligp2_get_mul_slot_opcode(op1, op1);
	if(op1 == MALIGP2_NOP) return _essl_maligp2_get_mul_slot_opcode(op0, op0);
	if(op0 == MALIGP2_MOV) return _essl_maligp2_get_mul_slot_opcode(MALIGP2_MUL, op1);
	if(op1 == MALIGP2_MOV) return _essl_maligp2_get_mul_slot_opcode(op0, MALIGP2_MUL);
	
	if(op0 == op1)
	{
		switch(op0)
		{
		case MALIGP2_MUL: return 0;
		case MALIGP2_MUL_ADD: return 1;
		case MALIGP2_OUTPUT_1: return 2;
		case MALIGP2_MUL_EXPWRAP: return 3;
		case MALIGP2_CSEL: return 4;
		default:
			return -1;
		}

	}
	return -1;
}

int _essl_maligp2_get_add_slot_opcode(maligp2_opcode op0, maligp2_opcode op1)
{
	
	if(op0 == MALIGP2_SET_IF_BOTH && (op1 == MALIGP2_MAX || op1 == MALIGP2_MOV || op1 == MALIGP2_NOP)) return 3;
	if(op0 == MALIGP2_NOP && op1 == MALIGP2_NOP) return 0; /* add */
	if(op0 == MALIGP2_MOV && op1 == MALIGP2_MOV) return 0; /* add */
	if(op0 == MALIGP2_MOV) return _essl_maligp2_get_add_slot_opcode(op1, op1);
	if(op0 == MALIGP2_NOP) return _essl_maligp2_get_add_slot_opcode(op1, op1);
	if(op1 == MALIGP2_MOV) return _essl_maligp2_get_add_slot_opcode(op0, op0);
	if(op1 == MALIGP2_NOP) return _essl_maligp2_get_add_slot_opcode(op0, op0);
	assert(op0 == op1);
	switch(op0)
	{
	case MALIGP2_ADD: return 0;
	case MALIGP2_FLOOR: return 1;
	case MALIGP2_SIGN: return 2;
	case MALIGP2_SET_IF_BOTH: return 3;
	case MALIGP2_SGE: return 4;
	case MALIGP2_SLT: return 5;
	case MALIGP2_MIN: return 6;
	case MALIGP2_MAX: return 7;
	default:
		return -1;

	}
}

essl_bool _essl_maligp2_add_slot_move_needs_two_inputs(int common_opcode)
{

	switch(common_opcode)
	{
	case 0: /* add */
		return ESSL_FALSE;
	case 3: /* set_if_both - max */
	case 6: /* min */
	case 7: /* max */
		return ESSL_TRUE;
	default:
		assert(0);
		return ESSL_TRUE;
	}

}



maligp2_instruction *_essl_maligp2_create_slot_instruction(mempool *pool, maligp2_instruction_word *word,
														   maligp2_schedule_classes mask, maligp2_opcode opcode, node *out, int *subcycle, unsigned vec_idx, essl_bool *failed_alloc) {
	maligp2_instruction *inst;
	maligp2_instruction **instp = 0;
	maligp2_schedule_classes sc;
	int rel_subcycle;
	*failed_alloc = ESSL_TRUE;
	if(opcode == MALIGP2_CONSTANT || opcode == MALIGP2_LOCKED_CONSTANT)
	{
		assert(mask < MALIGP2_NATIVE_VECTOR_SIZE);
		rel_subcycle = 2;
		if(word->embedded_constants[mask] == out)
		{
			instp = &word->u.real_slots.load_const[mask];
			sc = MALIGP2_SC_LOAD_CONST;
		} else {
			*subcycle = MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, rel_subcycle);
			assert(0);
			return 0;
		}
	} else if((opcode == MALIGP2_SET_LOAD_ADDRESS || opcode == MALIGP2_SET_STORE_ADDRESS) && mask == 0)
	{
		*failed_alloc = ESSL_FALSE;
		return 0;
	} else {

		if(mask & MALIGP2_SC_STORE_XY)
		{
			sc = MALIGP2_SC_STORE_XY;
			instp = &word->u.real_slots.store_xy;
			rel_subcycle = 0;
		} else 	if(mask & MALIGP2_SC_STORE_ZW)
		{
			sc = MALIGP2_SC_STORE_ZW;
			instp = &word->u.real_slots.store_zw;
			rel_subcycle = 0;
		} else if (mask & MALIGP2_SC_BRANCH) {
			sc = MALIGP2_SC_BRANCH;
			instp = &word->u.real_slots.branch;
			rel_subcycle = 0;
		} else if (mask & MALIGP2_SC_MISC) {
			sc = MALIGP2_SC_MISC;
			instp = &word->u.real_slots.misc;
			rel_subcycle = 1;
		} else if (mask & MALIGP2_SC_LUT) {
			sc = MALIGP2_SC_LUT;
			instp = &word->u.real_slots.lut;
			rel_subcycle = 1;
		} else if (mask & MALIGP2_SC_ADD0) {
			sc = MALIGP2_SC_ADD0;
			instp = &word->u.real_slots.add0;
			rel_subcycle = 1;
		} else if (mask & MALIGP2_SC_ADD1) {
			sc = MALIGP2_SC_ADD1;
			instp = &word->u.real_slots.add1;
			rel_subcycle = 1;
		} else if (mask & MALIGP2_SC_MUL0) {
			sc = MALIGP2_SC_MUL0;
			instp = &word->u.real_slots.mul0;
			rel_subcycle = 1;
		} else if (mask & MALIGP2_SC_MUL1) {
			sc = MALIGP2_SC_MUL1;
			instp = &word->u.real_slots.mul1;
			rel_subcycle = 1;
		} else if (mask & MALIGP2_SC_LOAD_CONST) {
			sc = MALIGP2_SC_LOAD_CONST;
			instp = &word->u.real_slots.load_const[vec_idx];
			rel_subcycle = 2;
		} else if (mask & MALIGP2_SC_LOAD0) {
			sc = MALIGP2_SC_LOAD0;
			instp = &word->u.real_slots.load0[vec_idx];
			rel_subcycle = 2;
		} else if (mask & MALIGP2_SC_LOAD1) {
			sc = MALIGP2_SC_LOAD1;
			instp = &word->u.real_slots.load1[vec_idx];
			rel_subcycle = 2;
		} else {
			*failed_alloc = ESSL_FALSE;
			return 0; /* Done */
		}
	}

	assert(*instp == 0);
	*subcycle = MALIGP2_CYCLE_TO_SUBCYCLE(word->cycle, rel_subcycle);
	ESSL_CHECK(inst = _essl_new_maligp2_instruction(pool, sc, opcode, *subcycle));
	*instp = inst;
	*failed_alloc = ESSL_FALSE;
	return inst;
}


static memerr handle_output(maligp2_scheduler_context *ctx, maligp2_instruction *slot, node *out) 
{

	node_extra *info = EXTRA_INFO(out);
	const symbol_list *syms;
	ESSL_CHECK(info);
	slot->address_offset = 0;
	/* relocation for variable references is handled later on when we fix up constants, so don't process them here */
	if(out->hdr.kind != EXPR_KIND_VARIABLE_REFERENCE)
	{
		slot->address_offset = info->address_offset;
		for(syms = info->address_symbols; syms != NULL; syms = syms->next)
		{
			ESSL_CHECK(_essl_maligp2_add_address_offset_relocation(ctx->relocation_context, syms->sym, slot));
		}
	}

	slot->instr_node = out;

	return MEM_OK;
}

static memerr _essl_maligp2_handle_no_modifier_input(maligp2_input_argument *arg, /*@null@*/ node *in)
{
	arg->arg = in;
	arg->reg_index = MALIGP2_REG_UNKNOWN;
	arg->negate = 0;

	if(arg->arg == 0) return MEM_OK; /* if there isn't a real operand, don't try to recurse on it */
	return MEM_OK;
}


static memerr _essl_maligp2_handle_input(maligp2_scheduler_context *ctx, 
								  maligp2_input_argument *arg,  /*@null@*/ node *in,
								  int subcycle, /*@null@*/ node *no_schedule_node)
{
	arg->arg = in;
	arg->reg_index = MALIGP2_REG_UNKNOWN;
	arg->negate = 0;
	

	if(arg->arg == 0) return MEM_OK; /* if no real operand, don't try to recurse on it */
	/* all properly initialized, we can now commence eating input operations */
	for(;;)
	{
		if(arg->arg->hdr.kind == EXPR_KIND_UNARY 
		   && arg->arg->expr.operation == EXPR_OP_NEGATE)
		{
			arg->negate = !arg->negate;
		} else {
			break;
		}
		/* eat the node we just processed */
		if(arg->arg != no_schedule_node)
		{
			ESSL_CHECK(_essl_scheduler_schedule_extra_operation(ctx->sctx, &arg->arg, subcycle));
		}
		ESSL_CHECK(arg->arg = GET_CHILD(arg->arg, 0));
	}
	return MEM_OK;
}


memerr _essl_maligp2_write_instructions(maligp2_scheduler_context *ctx, 
				  maligp2_instruction_word *word, int *slots, node **transfer_nodes, int address_reg, va_list arglist) 
{
	maligp2_instruction *slot;
	node **out_ptr;
	node *out;
	essl_bool have_scheduled_operation = ESSL_FALSE;
	node *tmp;
	unsigned idx;
	int subcycle = 0;
	for(idx = 0; ; ++idx) {
		essl_bool handled = ESSL_TRUE;
		maligp2_opcode opcode = va_arg(arglist, maligp2_opcode);
		/* first check for special opcodes */
		
		switch(opcode)
		{
		case MALIGP2_FINISH:
			return MEM_OK;

		case MALIGP2_NEXT_CYCLE:
			--idx;
			assert(word->predecessor);
			word = word->predecessor;
			break;

		case MALIGP2_SCHEDULE_EXTRA:
			--idx;
			out_ptr = va_arg(arglist, node **);
			out = out_ptr != NULL ? *out_ptr : NULL;
			if(out != NULL)
			{
				ESSL_CHECK(_essl_scheduler_schedule_extra_operation(ctx->sctx, out_ptr, subcycle));
			}
			break;

		case MALIGP2_RESERVE_MOV:
		case MALIGP2_RESERVE_MOV_IF_NOT_SAME_WORD:
			out_ptr = va_arg(arglist, node **);
			break;

		default:
			handled = ESSL_FALSE;
			break;
		}
		if(handled)
		{
			continue;
		}

		out_ptr = va_arg(arglist, node **);
		out = out_ptr != NULL ? *out_ptr : NULL;
		{
			node_extra *ne;
			unsigned offset = 0;
			if(out != 0)
			{
				ne = EXTRA_INFO(out);
				if(ne != 0) offset = ne->address_offset & 3;

			}
			if(transfer_nodes[idx] != 0)
			{
				slot = 0;
			} else {
				essl_bool failed_alloc = ESSL_FALSE;
				slot = _essl_maligp2_create_slot_instruction(ctx->pool, word, slots[idx], opcode, out, &subcycle, offset, &failed_alloc);
				if(failed_alloc) return MEM_ERROR;
			}
		}
		assert(transfer_nodes[idx] != 0 || opcode == MALIGP2_CONSTANT || opcode == MALIGP2_SET_LOAD_ADDRESS || opcode == MALIGP2_SET_STORE_ADDRESS || slot != 0);
		if (out)
		{
			if(slot)
			{
				ESSL_CHECK(handle_output(ctx, slot, out));
			}
			if (opcode == MALIGP2_RESERVE_MOV || opcode == MALIGP2_RESERVE_MOV_IF_NOT_SAME_WORD)
			{

			} else {
				if(!have_scheduled_operation)
				{
					ESSL_CHECK(_essl_scheduler_schedule_operation(ctx->sctx, out, subcycle));
					have_scheduled_operation = ESSL_TRUE;
				} else {
					ESSL_CHECK(_essl_scheduler_schedule_extra_operation(ctx->sctx, out_ptr, subcycle));
					out = *out_ptr;
					if(slot)
					{
						slot->instr_node = out;
					}
				}
			}
		}
		if(transfer_nodes[idx] != 0)
		{
			/* rewrite node as transfer */
			if(out != transfer_nodes[idx]) /* there could be a transfer due to an object identity. in this case, it would be bad to link the node to itself */
			{
				assert(out != NULL);
				_essl_rewrite_node_to_transfer(out, transfer_nodes[idx]);
				out->hdr.is_control_dependent = ESSL_FALSE;
			}
			continue;
		}
		switch (opcode) {

		case MALIGP2_ADD:
		case MALIGP2_SET_IF_BOTH:
		case MALIGP2_SGE:
		case MALIGP2_SLT:
		case MALIGP2_MIN:
		case MALIGP2_MAX:
		case MALIGP2_MUL:
		case MALIGP2_MUL_EXPWRAP:
			/* Ordinary binary operation. Takes two inputs */
			assert(slot != NULL);
			tmp = va_arg(arglist, node *);
			ESSL_CHECK(_essl_maligp2_handle_input(ctx, &slot->args[0], tmp, subcycle, out));
			tmp = va_arg(arglist, node *);
			ESSL_CHECK(_essl_maligp2_handle_input(ctx, &slot->args[1], tmp, subcycle, out));
			break;


		case MALIGP2_FLOOR:
		case MALIGP2_SIGN:
			/* Ordinary unary operation. Takes one input */
			assert(slot != NULL);
			tmp = va_arg(arglist, node *);
			ESSL_CHECK(_essl_maligp2_handle_input(ctx, &slot->args[0], tmp, subcycle, out));
			break;

		case MALIGP2_NEGATE:
			/* Ordinary unary operation. Takes one input */
			assert(slot != NULL);
			tmp = va_arg(arglist, node *);
			slot->opcode = MALIGP2_MOV;
			ESSL_CHECK(_essl_maligp2_handle_input(ctx, &slot->args[0], tmp, subcycle, out));
			slot->args[0].negate = !slot->args[0].negate;
			break;

		case MALIGP2_OUTPUT_0:
		case MALIGP2_OUTPUT_1:
			/* no inputs at all */
			break;

		case MALIGP2_MUL_ADD:
			assert(slot != NULL);
			slot->args[0].reg_index = MALIGP2_LUT;
			slot->args[1].reg_index = MALIGP2_MUL0_DELAY0;
			{
				maligp2_instruction *slot2;
				ESSL_CHECK(slot2 = _essl_new_maligp2_instruction(ctx->pool, slot->schedule_class, opcode, subcycle));
				*slot2 = *slot;
				slot2->schedule_class = MALIGP2_SC_MUL1;
				slot2->instr_node = 0;
				assert(word->u.real_slots.mul1 == 0);
				word->u.real_slots.mul1 = slot2;
				slot2->args[0].reg_index = MALIGP2_LUT;
				tmp = va_arg(arglist, node *);

				ESSL_CHECK(_essl_maligp2_handle_no_modifier_input(&slot2->args[1], tmp));
			}
			break;

		case MALIGP2_CSEL:
			/* Ternary */
			assert(slot != NULL);
			{
				maligp2_instruction *slot2;
				ESSL_CHECK(slot2 = _essl_new_maligp2_instruction(ctx->pool, slot->schedule_class, opcode, subcycle));
				*slot2 = *slot;
				slot2->schedule_class = MALIGP2_SC_MUL1;
				slot2->instr_node = 0;
				assert(word->u.real_slots.mul1 == 0);
				word->u.real_slots.mul1 = slot2;

				slot2->args[0].reg_index = MALIGP2_LUT;

				/* condition */
				tmp = va_arg(arglist, node *);
				ESSL_CHECK(_essl_maligp2_handle_no_modifier_input(&slot->args[1], tmp));

				/* first choice */
				tmp = va_arg(arglist, node *);
				ESSL_CHECK(_essl_maligp2_handle_input(ctx, &slot2->args[0], tmp, subcycle, out));

				/* second choice */
				tmp = va_arg(arglist, node *);
				ESSL_CHECK(_essl_maligp2_handle_input(ctx, &slot->args[0], tmp, subcycle, out));

			}
			break;

		case MALIGP2_MOV:

		case MALIGP2_RCC:
		case MALIGP2_EX2:
		case MALIGP2_LG2:
		case MALIGP2_RSQ:
		case MALIGP2_RCP:
		case MALIGP2_LOG:
		case MALIGP2_EXP:

		case MALIGP2_EXP_FLOOR:
		case MALIGP2_FLOOR_LOG:
		case MALIGP2_DIV_EXP_FLOOR_LOG:
		case MALIGP2_FLOAT_TO_FIXED:
		case MALIGP2_FIXED_TO_FLOAT:
		case MALIGP2_CLAMP:
		case MALIGP2_FRAC_FIXED_TO_FLOAT:

			assert(slot != NULL);
			tmp = va_arg(arglist, node *);
			ESSL_CHECK(_essl_maligp2_handle_no_modifier_input(&slot->args[0], tmp));
			break;

		case MALIGP2_MOV_MISC:

			assert(slot != NULL);
			tmp = va_arg(arglist, node *);
			ESSL_CHECK(_essl_maligp2_handle_no_modifier_input(&slot->args[0], tmp));
			slot->opcode = MALIGP2_MOV;
			break;

		case MALIGP2_RESERVE_MOV_MISC:

			assert(slot != NULL);
			tmp = va_arg(arglist, node *);
			ESSL_CHECK(_essl_maligp2_handle_no_modifier_input(&slot->args[0], tmp));
			break;

			

		case MALIGP2_SET_LOAD_ADDRESS:
		case MALIGP2_SET_STORE_ADDRESS:
			tmp = va_arg(arglist, node *);
			(void)va_arg(arglist, unsigned);
			if(slot != 0)
			{

				ESSL_CHECK(_essl_maligp2_handle_no_modifier_input(&slot->args[0], tmp));
				slot->address_reg = address_reg;
				slot->opcode = MALIGP2_SET_ADDRESS;
				if(slot->schedule_class == MALIGP2_SC_MISC)
				{
					if((address_reg & 1) != 1)
					{
						/* the even part should be in lut, the odd part in misc. swap the two to compensate */
						word->u.real_slots.misc = word->u.real_slots.lut;
						word->u.real_slots.lut = slot;
						word->u.real_slots.misc->schedule_class = MALIGP2_SC_MISC;
						word->u.real_slots.lut->schedule_class = MALIGP2_SC_LUT;

					}
					
				}
			}

			break;



		case MALIGP2_LOAD_CONSTANT_INDEXED:
			assert(slot != NULL);
			slot->address_reg = address_reg;
			break;

		case MALIGP2_LOAD_CONSTANT:
		case MALIGP2_LOAD_WORK_REG:
		case MALIGP2_LOAD_INPUT_REG:
			break;


		case MALIGP2_CONSTANT:
			assert(slot);
			word->embedded_constants[slots[idx]] = out;
			break;

		case MALIGP2_LOCKED_CONSTANT:
			(void)va_arg(arglist, int); /* skip wanted index */
			assert(slot);
			slot->opcode = MALIGP2_CONSTANT;
			word->embedded_constants[slots[idx]] = out;
			break;

		case MALIGP2_STORE_WORK_REG:
		case MALIGP2_STORE_OUTPUT_REG:
		case MALIGP2_STORE_CONSTANT:
			assert(slot != NULL);
		    {
				unsigned i;
				node *inputs[MALIGP2_NATIVE_VECTOR_SIZE] = {0};
				inputs[0] = va_arg(arglist, node *);
				inputs[1] = va_arg(arglist, node *);
				inputs[2] = va_arg(arglist, node *);
				inputs[3] = va_arg(arglist, node *);
				for(i = 0; i < 2; ++i)
				{
					ESSL_CHECK(_essl_maligp2_handle_no_modifier_input(&slot->args[i], inputs[i]));
				}
				slot->instr_node = 0; /* no result from a store */
				slot->address_reg = address_reg;

				ESSL_CHECK(word->u.real_slots.store_zw = _essl_new_maligp2_instruction(ctx->pool, slot->schedule_class, opcode, subcycle));
				*word->u.real_slots.store_zw = *slot;
				ESSL_CHECK(handle_output(ctx, word->u.real_slots.store_zw, out));
				slot = word->u.real_slots.store_zw;
				slot->schedule_class = MALIGP2_SC_STORE_ZW;
				slot->instr_node = 0; /* no result from a store */
				
				for(i = 0; i < 2; ++i)
				{
					ESSL_CHECK(_essl_maligp2_handle_no_modifier_input(&slot->args[i], inputs[2+i]));
				}
				
				
			}
			break;

		case MALIGP2_NOP:
			break;

#if 0 /* No function call support */
		case MALIGP2_CALL:
		case MALIGP2_CALL_COND:
			assert(slot != NULL);
			sym = va_arg(arglist, symbol *);

			if(sym->type->basic_type == TYPE_VOID)
			{
				slot->instr_node = 0;
			}

			slot->call_target = sym;
			/* flag relocations of the parameter symbols */
			{
				parameter *arg;
				single_declarator *parm;
				for(arg = out->expr.u.fun.arguments, parm = sym->parameters; 
					arg != 0 && parm != 0;  arg = arg->next, parm = parm->next)
				{
					parm->sym->address = 0;

					ESSL_CHECK(_essl_maligp2_add_symbol_address_relocation(ctx->relocation_context, 
																	  parm->sym, arg->sym));

				}
			}
			break;
#endif

		case MALIGP2_JMP:
		case MALIGP2_JMP_COND:
		case MALIGP2_JMP_COND_NOT_CONSTANT:
		case MALIGP2_LOOP_END:
		case MALIGP2_REPEAT_END:
			assert(slot != NULL);
			slot->jump_target = va_arg(arglist, basic_block *);
			break;

		case MALIGP2_RET:
		case MALIGP2_LOOP_START:
		case MALIGP2_REPEAT_START:
			break;

		case MALIGP2_STORE_SELECT_ADDRESS_REG:
			assert(slot != NULL);
			slot->address_reg = address_reg;
			break;


		default:
			assert(0); /* Not supported yet */
		}
	}

}

essl_bool _essl_maligp2_inseparable(maligp2_instruction_word *w2) {
	return w2->u.real_slots.mul0 && w2->u.real_slots.mul1 && w2->u.real_slots.mul0->opcode == MALIGP2_MUL_ADD;
}

essl_bool _essl_maligp2_inseparable_from_successor(maligp2_instruction_word *w) {
	return w->successor && _essl_maligp2_inseparable(w->successor);
}

essl_bool _essl_maligp2_inseparable_from_predecessor(maligp2_instruction_word *w) {
	return w->predecessor && _essl_maligp2_inseparable(w);
}
