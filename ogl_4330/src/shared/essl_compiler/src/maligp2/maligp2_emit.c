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
#include "common/output_buffer.h"
#include "common/translation_unit.h"
#include "maligp2/maligp2_instruction.h"
#include "maligp2/maligp2_emit.h"
#include "common/compiler_options.h"

typedef struct 
{
	output_buffer *output_buf;
	unsigned start_address;
} maligp2_emit_context;


typedef struct 
{
	unsigned mul_inputs[2][2];
	unsigned mul_input0_negates[2];
	
	unsigned add_inputs[2][2];
	unsigned add_input_negates[2][2];

	int constant_register_index;

	unsigned address_register_select;

	unsigned input_indices[2];

	unsigned out_register_constant_selects[2];

	unsigned ram_rom_select;

	unsigned out_selects[4];

	unsigned add_op;
	unsigned lut_op;

	unsigned out_register_selects[2];

	unsigned mul_op;
	unsigned misc_op;

	unsigned lut_input;
	unsigned misc_input;

	unsigned branch_op;
	unsigned branch_address;

	
} maligp2_hardware_instruction_word;


static void emit_muls(maligp2_instruction_word *word, maligp2_hardware_instruction_word *hw)
{
	unsigned i;
	maligp2_opcode opcodes[2] = { MALIGP2_NOP, MALIGP2_NOP };
	int common_opcode;
	if(word->u.real_slots.mul0 != 0) opcodes[0] = word->u.real_slots.mul0->opcode;
	if(word->u.real_slots.mul1 != 0) opcodes[1] = word->u.real_slots.mul1->opcode;
	common_opcode = _essl_maligp2_get_mul_slot_opcode(opcodes[0], opcodes[1]);
	assert(common_opcode >= 0 && common_opcode < 5);
	hw->mul_op = common_opcode;
	if(word->u.real_slots.mul0 != 0) 
	{
		for(i = 0; i < 2; ++i)
		{
			if(word->u.real_slots.mul0->args[i].reg_index != -1)
			{
				hw->mul_inputs[0][i] = word->u.real_slots.mul0->args[i].reg_index;
				hw->mul_input0_negates[0] ^= word->u.real_slots.mul0->args[i].negate;
			}
		}
		if(opcodes[0] == MALIGP2_MOV)
		{
			hw->mul_inputs[0][1] = MALIGP2_LUT;
		}

	}

	if(word->u.real_slots.mul1 != 0) 
	{
		assert(!(word->u.real_slots.mul1->opcode == MALIGP2_MUL_ADD && word->u.real_slots.mul1->args[1].negate));
		for(i = 0; i < 2; ++i)
		{
			if(word->u.real_slots.mul1->args[i].reg_index != -1)
			{
				hw->mul_inputs[1][i] = word->u.real_slots.mul1->args[i].reg_index;
				hw->mul_input0_negates[1] ^= word->u.real_slots.mul1->args[i].negate;
			}
		}
		if(opcodes[1] == MALIGP2_MOV)
		{
			hw->mul_inputs[1][1] = MALIGP2_LUT;
		}

		
	}


   
}

static void emit_adds(maligp2_instruction_word *word, maligp2_hardware_instruction_word *hw)
{
	unsigned i;
	maligp2_opcode opcodes[2] = { MALIGP2_NOP, MALIGP2_NOP };
	int common_opcode;
	if(word->u.real_slots.add0 != 0) opcodes[0] = word->u.real_slots.add0->opcode;
	if(word->u.real_slots.add1 != 0) opcodes[1] = word->u.real_slots.add1->opcode;
	common_opcode = _essl_maligp2_get_add_slot_opcode(opcodes[0], opcodes[1]);
	assert(common_opcode >= 0 && common_opcode < 8);
	hw->add_op = common_opcode;
	if(word->u.real_slots.add0 != 0) 
	{
		for(i = 0; i < 2; ++i)
		{
			if(word->u.real_slots.add0->args[i].reg_index != -1)
			{
				hw->add_inputs[0][i] = word->u.real_slots.add0->args[i].reg_index;
				hw->add_input_negates[0][i] = word->u.real_slots.add0->args[i].negate;
			}
		}
		if(opcodes[0] == MALIGP2_MOV)
		{
			if(_essl_maligp2_add_slot_move_needs_two_inputs(common_opcode))
			{
				hw->add_inputs[0][1] = hw->add_inputs[0][0];
				hw->add_input_negates[0][1] = hw->add_input_negates[0][0];
			} else {
				hw->add_inputs[0][1] = MALIGP2_LUT;
				hw->add_input_negates[0][1] = 1;
			}

		}

	}

	if(word->u.real_slots.add1 != 0) 
	{
		for(i = 0; i < 2; ++i)
		{
			if(word->u.real_slots.add1->args[i].reg_index != -1)
			{
				hw->add_inputs[1][i] = word->u.real_slots.add1->args[i].reg_index;
				hw->add_input_negates[1][i] = word->u.real_slots.add1->args[i].negate;
			}
		}
		if(opcodes[1] == MALIGP2_MOV)
		{
			if(_essl_maligp2_add_slot_move_needs_two_inputs(common_opcode))
			{
				hw->add_inputs[1][1] = hw->add_inputs[1][0];
				hw->add_input_negates[1][1] = hw->add_input_negates[1][0];
			} else {
				hw->add_inputs[1][1] = MALIGP2_LUT;
				hw->add_input_negates[1][1] = 1;
			}

		}


		
	}


   
}

static void emit_lut(maligp2_instruction_word *word, maligp2_hardware_instruction_word *hw)
{
	if(word->u.real_slots.lut != 0)
	{
		unsigned op = 0;
		hw->lut_input = word->u.real_slots.lut->args[0].reg_index;
		switch(word->u.real_slots.lut->opcode)
		{
		case MALIGP2_OUTPUT_1:
			hw->lut_input = MALIGP2_PREVIOUS_VALUE;
			op = 0;
			break;
		case MALIGP2_RCC:
			op = 1;
			break;
		case MALIGP2_EX2:
			op = 2;
			break;
		case MALIGP2_LG2:
			op = 3;
			break;
		case MALIGP2_RSQ:
			op = 4;
			break;
		case MALIGP2_RCP:
			op = 5;
			break;
		case MALIGP2_LOG:
			op = 6;
			break;
		case MALIGP2_EXP:
			op = 7;
			break;
		case MALIGP2_OUTPUT_0:
			hw->lut_input = MALIGP2_PREVIOUS_VALUE;
			op = 8;
			break;
		case MALIGP2_MOV:
			op = 9;
			break;
		case MALIGP2_SET_ADDRESS:
			assert(word->u.real_slots.lut->address_reg >= 0);
			op = 12 + word->u.real_slots.lut->address_reg;
			/* vector set address is handled in emit_misc */
			assert(op >= 12 && op < 16);
			break;
		default:
			assert(0);
			break;
		}
		hw->lut_op = op;

	}
}

static void emit_misc(maligp2_instruction_word *word, maligp2_hardware_instruction_word *hw)
{
	if(word->u.real_slots.misc != 0)
	{
		unsigned op = 0;
		hw->misc_input = word->u.real_slots.misc->args[0].reg_index;
		switch(word->u.real_slots.misc->opcode)
		{
		case MALIGP2_EXP_FLOOR:
			op = 0;
			break;
		case MALIGP2_FLOOR_LOG:
			op = 1;
			break;
		case MALIGP2_MOV:
			op = 2;
			break;
		case MALIGP2_DIV_EXP_FLOOR_LOG:
			op = 3;
			break;
		case MALIGP2_FLOAT_TO_FIXED:
			op = 4;
			break;
		case MALIGP2_FIXED_TO_FLOAT:
			op = 5;
			break;
		case MALIGP2_CLAMP:
			op = 6;
			break;
		case MALIGP2_FRAC_FIXED_TO_FLOAT:
			op = 7;
			break;
		case MALIGP2_SET_ADDRESS:
			assert(word->u.real_slots.lut != 0 && word->u.real_slots.lut->opcode == MALIGP2_SET_ADDRESS);
			assert((word->u.real_slots.lut->address_reg & 1) == 0);
			assert((word->u.real_slots.misc->address_reg & 1) == 1);
			assert(word->u.real_slots.misc->address_reg - word->u.real_slots.lut->address_reg  == 1);
			op = 2;
			hw->lut_op = 10 + (word->u.real_slots.lut->address_reg >> 1);
			assert(hw->lut_op >= 10 && hw->lut_op < 12);
			break;
		default:
			assert(0);
			break;
		}
		hw->misc_op = op;

	}
}


static void emit_branch(maligp2_instruction_word *word, maligp2_hardware_instruction_word *hw)
{
	if(word->u.real_slots.branch != 0)
	{
		unsigned op = 0;
		switch(word->u.real_slots.branch->opcode)
		{
		case MALIGP2_NOP:
			op = 0;
			break;
		case MALIGP2_RET:
			op = 1;
			break;
		case MALIGP2_CALL:
			op = 2;
			break;
		case MALIGP2_JMP_COND_NOT_CONSTANT:
			op = 3;
			break;
		case MALIGP2_JMP:
			op = 4;
			break;
		case MALIGP2_CALL_COND:
			op = 5;
			break;
		case MALIGP2_LOOP_START:
			op = 6;
			break;
		case MALIGP2_REPEAT_START:
			op = 7;
			break;
		case MALIGP2_LOOP_END:
			op = 8;
			break;
		case MALIGP2_REPEAT_END:
			op = 9;
			break;
/*		case MALIGP2_SWITCH_INPUT_BANK:
			op = 10;
			break; */
/*		case MALIGP2_CALL_A0_X:
			op = 11;
			break; */
		case MALIGP2_STORE_SELECT_ADDRESS_REG:
			op = 12;
			hw->branch_address = word->u.real_slots.branch->address_reg;
			break;
		case MALIGP2_JMP_COND:
			op = 13;
			break;
		default:
			assert(0);
			break;
		}
		hw->branch_op = op;
	}
}

static void emit_single_load(maligp2_instruction *instr, maligp2_hardware_instruction_word *hw, unsigned load_index)
{
	switch(instr->opcode)
	{
	case MALIGP2_LOAD_INPUT_REG:
		assert(load_index == 0);
		hw->input_indices[load_index] = instr->address_offset/MALIGP2_NATIVE_VECTOR_SIZE + 16;
		break;
	case MALIGP2_LOAD_WORK_REG:
		hw->input_indices[load_index] = instr->address_offset/MALIGP2_NATIVE_VECTOR_SIZE;
		break;
	default:
		assert(0);
		break;

	}

}

static void emit_constant_load(maligp2_instruction *instr, maligp2_hardware_instruction_word *hw)
{
	switch(instr->opcode)
	{
	case MALIGP2_LOAD_CONSTANT_INDEXED:
		hw->address_register_select = instr->address_reg;
		/*@fall-through@*/
	case MALIGP2_LOAD_CONSTANT:
		hw->constant_register_index = instr->address_offset/MALIGP2_NATIVE_VECTOR_SIZE;
		break;
	default:
		assert(0);
		break;

	}
	

}

static maligp2_instruction *get_instr_from_array(maligp2_instruction **arr)
{
	unsigned i;
	maligp2_instruction *ret = 0;
	for(i = 0; i < MALIGP2_NATIVE_VECTOR_SIZE; ++i)
	{
		if(arr[i] != 0)
		{
			assert(arr[i]->opcode != MALIGP2_CONSTANT);
			if(!ret)
			{
				ret = arr[i];
			} else {

				assert(ret->address_offset/MALIGP2_NATIVE_VECTOR_SIZE == arr[i]->address_offset/MALIGP2_NATIVE_VECTOR_SIZE);
				assert(ret->address_reg == arr[i]->address_reg);
				assert(ret->opcode == arr[i]->opcode);
			}
		}
	}
	return ret;
}

static void emit_loads(maligp2_instruction_word *word, maligp2_hardware_instruction_word *hw)
{
	maligp2_instruction *instr;
	if(0 != (instr = get_instr_from_array(word->u.real_slots.load0)))
	{
		emit_single_load(instr, hw, 0);
	}

	if(0 != (instr = get_instr_from_array(word->u.real_slots.load1)))
	{
		emit_single_load(instr, hw, 1);
	}

	if(0 != (instr = get_instr_from_array(word->u.real_slots.load_const)))
	{
		emit_constant_load(instr, hw);
	}

	
}


static void emit_single_store(maligp2_instruction *instr, maligp2_hardware_instruction_word *hw, unsigned store_index)
{
	unsigned i;
	essl_bool is_actual_store = ESSL_FALSE;
	for(i = 0; i < 2; ++i)
	{
		if(instr->args[i].reg_index != -1)
		{
			hw->out_selects[store_index*2 + i] =  instr->args[i].reg_index;
			if(instr->args[i].reg_index != MALIGP2_OUTPUT_DISABLE)
			{
				is_actual_store = ESSL_TRUE;
			}

		}
	}
	switch(instr->opcode)
	{
	case MALIGP2_STORE_WORK_REG:
		hw->out_register_selects[store_index] = instr->address_offset/MALIGP2_NATIVE_VECTOR_SIZE;
		break;

	case MALIGP2_STORE_OUTPUT_REG:
		hw->out_register_selects[store_index] = instr->address_offset/MALIGP2_NATIVE_VECTOR_SIZE + 16;
		break;
	case MALIGP2_STORE_CONSTANT:
		if(is_actual_store)
		{
			hw->out_register_constant_selects[store_index] = 1;
		}
		break;
	default:
		assert(0);
		break;
	}

}

static void emit_stores(maligp2_instruction_word *word, maligp2_hardware_instruction_word *hw)
{
	if(word->u.real_slots.store_xy != 0)
	{
		emit_single_store(word->u.real_slots.store_xy, hw, 0);
	}
	if(word->u.real_slots.store_zw != 0)
	{
		emit_single_store(word->u.real_slots.store_zw, hw, 1);
	}
	
}





static memerr write_word(maligp2_emit_context *ctx, maligp2_hardware_instruction_word *word)
{
#ifndef NDEBUG
	unsigned start_word = _essl_output_buffer_get_size(ctx->output_buf);
#endif

#define WRITE(n_bits, value) ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, (n_bits), word->value))
	unsigned i, j;
	assert(_essl_output_buffer_get_bit_position(ctx->output_buf) == 0);
	for(j = 0; j < 2; ++j)
	{
		for(i = 0; i < 2; ++i)
		{
			WRITE(5, mul_inputs[j][i]);
		}
	}


	for(j = 0; j < 2; ++j)
	{
		WRITE(1, mul_input0_negates[j]);
	}

	for(j = 0; j < 2; ++j)
	{
		for(i = 0; i < 2; ++i)
		{
			WRITE(5, add_inputs[j][i]);
		}
	}


	for(j = 0; j < 2; ++j)
	{
		for(i = 0; i < 2; ++i)
		{
			WRITE(1, add_input_negates[j][i]);
		}
	}

	WRITE(9, constant_register_index&0x1FF);
	WRITE(3, address_register_select);

	WRITE(5, input_indices[0]);
	WRITE(4, input_indices[1]); /* 4 is not a typo, they have different spans */
	for(i = 0; i < 2; ++i)
	{
		WRITE(1, out_register_constant_selects[i]);
	}

	WRITE(2, ram_rom_select);

	for(i = 0; i < MALIGP2_NATIVE_VECTOR_SIZE; ++i)
	{
		WRITE(3, out_selects[i]);
	}
	
	WRITE(3, add_op);
	
	WRITE(4, lut_op);

	for(i = 0; i < 2; ++i)
	{
		WRITE(5, out_register_selects[i]);
	}
	
	WRITE(3, mul_op);

	WRITE(3, misc_op);

	WRITE(5, lut_input);
	WRITE(5, misc_input);

	WRITE(4, branch_op);
	WRITE(8, branch_address);



#undef WRITE
	assert(_essl_output_buffer_get_bit_position(ctx->output_buf) == 0);
	assert(_essl_output_buffer_get_size(ctx->output_buf) - start_word == 4);
	return MEM_OK;
}







static memerr emit_word(maligp2_emit_context *ctx, maligp2_instruction_word *word)
{
	unsigned i, j;
	const unsigned default_input = MALIGP2_PREVIOUS_VALUE; /* saves dynamic power */
	maligp2_hardware_instruction_word hardware_word;
	memset(&hardware_word, 0, sizeof(maligp2_hardware_instruction_word));
	for(j = 0; j < 2; ++j)
	{
		for(i = 0; i < 2; ++i)
		{
			hardware_word.mul_inputs[j][i] = default_input;
			hardware_word.add_inputs[j][i] = default_input;
		}
	}
	hardware_word.address_register_select = MALIGP2_ADDR_NOT_INDEXED;
	hardware_word.ram_rom_select = 0;

	for(i = 0; i < MALIGP2_NATIVE_VECTOR_SIZE; ++i)
	{
		hardware_word.out_selects[i] = MALIGP2_OUTPUT_DISABLE;
	}
	hardware_word.misc_op = 2;

	hardware_word.lut_input = default_input;
	hardware_word.misc_input = default_input;

	emit_loads(word, &hardware_word);

	emit_muls(word, &hardware_word);
	emit_adds(word, &hardware_word);

	emit_lut(word, &hardware_word);
	emit_misc(word, &hardware_word); /* must be run after emit_lut due to vector address set */

	emit_branch(word, &hardware_word);

	emit_stores(word, &hardware_word);

	word->emit_address = _essl_output_buffer_get_size(ctx->output_buf);
	ESSL_CHECK(write_word(ctx, &hardware_word));
	return MEM_OK;
}


static memerr emit_function(maligp2_emit_context *ctx, symbol *fun) 
{
	control_flow_graph * cfg = fun->control_flow_graph;
	size_t i;

	assert(cfg != 0);

	cfg->start_address = _essl_output_buffer_get_size(ctx->output_buf);

	for (i = 0; i < cfg->n_blocks; ++i) 
	{
		maligp2_instruction_word *word;
		basic_block *b = cfg->output_sequence[i];
		assert(b != 0);

		for (word = (maligp2_instruction_word *)b->earliest_instruction_word; word != 0; word = word->successor)
		{
			ESSL_CHECK(emit_word(ctx, word));
		}
	}

	cfg->end_address = _essl_output_buffer_get_size(ctx->output_buf);

	return MEM_OK;
}


static int address_for_jump_target(maligp2_emit_context *ctx, basic_block *b)
{
	maligp2_instruction_word *word = 0;
	while(b->earliest_instruction_word == 0)
	{
		b = b->successors[BLOCK_DEFAULT_TARGET];
		assert(b != 0);
	}
	word = (maligp2_instruction_word *)b->earliest_instruction_word;
	assert(word->emit_address >= 0);
	return word->emit_address - ctx->start_address;
}

static memerr fixup_jumps_calls(maligp2_emit_context *ctx, symbol *fun, essl_bool relative) 
{
	control_flow_graph * cfg = fun->control_flow_graph;
	size_t i;

	assert(cfg != 0);

	for (i = 0; i < cfg->n_blocks; ++i) 
	{
		maligp2_instruction_word *word;
		basic_block *b = cfg->output_sequence[i];
		assert(b != 0);

		for (word = (maligp2_instruction_word *)b->earliest_instruction_word; word != 0; word = word->successor)
		{
			if(word->u.real_slots.branch != 0)
			{
				basic_block *target = 0;
				if(word->u.real_slots.branch->call_target != 0)
				{
					control_flow_graph *tcfg = word->u.real_slots.branch->call_target->control_flow_graph;
					assert(tcfg != 0 && tcfg->n_blocks > 0);
					target = tcfg->output_sequence[0];
				} else if(word->u.real_slots.branch->jump_target != 0)
				{
					target = word->u.real_slots.branch->jump_target;
				}


				if(target != 0)
				{
					int address = address_for_jump_target(ctx, target);
					unsigned branch_address = 0;
					unsigned bank = 0;
					assert(address >= 0);
					assert(address % 4 == 0);

					if (relative) {
						address = address - (word->emit_address - ctx->start_address);
						
					}
					address /= 4;
					branch_address = (unsigned)address & 0xFF;
					bank = ((unsigned)address & 0x300)>>8;

					if(!relative)
					{
						if(bank == 0) bank = 3;
					}
					

					_essl_output_buffer_replace_bits(ctx->output_buf, word->emit_address + 3, 24, 8, branch_address);
					_essl_output_buffer_replace_bits(ctx->output_buf, word->emit_address + 2, 5, 2, bank);
				}

			}
		}
	}
	return MEM_OK;
}

memerr _essl_maligp2_emit_function(error_context *err_ctx, output_buffer *buf, translation_unit *tu, symbol *function)
{
	maligp2_emit_context context, *ctx = &context;

	IGNORE_PARAM(err_ctx);
	IGNORE_PARAM(tu);
	
	ctx->output_buf = buf;

	ESSL_CHECK(emit_function(ctx, function));
	ESSL_CHECK(fixup_jumps_calls(ctx, function, ESSL_FALSE));
	return MEM_OK;
}



memerr _essl_maligp2_emit_translation_unit(error_context *err_ctx, output_buffer *buf, translation_unit *tu, essl_bool relative_jumps) 
{
	maligp2_emit_context context, *ctx = &context;
	symbol_list *sl;
	IGNORE_PARAM(err_ctx);
	ctx->output_buf = buf;
	ctx->start_address = _essl_output_buffer_get_size(ctx->output_buf);
	for (sl = tu->functions; sl != 0; sl = sl->next) 
	{
		symbol *sym = sl->sym;
		ESSL_CHECK(sym);
		if (sym != tu->entry_point && !sym->opt.pilot.is_proactive_func) 
		{
			ESSL_CHECK(emit_function(ctx, sym));
		}

	}
	if (tu->entry_point) {
		ESSL_CHECK(emit_function(ctx, tu->entry_point));
	}

	for (sl = tu->functions; sl != 0; sl = sl->next) 
	{
		symbol *sym = sl->sym;
		ESSL_CHECK(sym);
		if(!sym->opt.pilot.is_proactive_func)
		{
			ESSL_CHECK(fixup_jumps_calls(ctx, sym, relative_jumps));
		}
	}

	return MEM_OK;
}


