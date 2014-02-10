/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "maligp2/maligp2_printer.h"
#include "common/essl_stringbuffer.h"
#include "common/unique_names.h"
#include "maligp2/maligp2_instruction.h"
#include "common/basic_block.h"
#include "backend/liveness.h"


typedef struct 
{
	FILE *out;
	unique_name_context block_names;
	unique_name_context *node_names;
	target_descriptor *desc;
	mempool *pool;
} maligp2_print_context;


static /*@null@*/ const char *node_name_get_or_create(maligp2_print_context *ctx, node *n)
{
	while(n->hdr.kind == EXPR_KIND_TRANSFER)
	{
		n = GET_CHILD(n, 0);
	}
	return _essl_unique_name_get_or_create(ctx->node_names, n);

}

static memerr print_opcode(maligp2_print_context *ctx, maligp2_instruction *instr)
{
	switch(instr->opcode)
	{
#define PRINT(str) fprintf(ctx->out, "%s", str)


	case MALIGP2_OUTPUT_0: PRINT("output_0.0"); break;
	case MALIGP2_OUTPUT_1: PRINT("output 1.0"); break;
	case MALIGP2_NEGATE: PRINT("negate"); break;

	case MALIGP2_MOV: PRINT("mov"); break;

	case MALIGP2_ADD: PRINT("add"); break;
	case MALIGP2_FLOOR: PRINT("floor"); break;
	case MALIGP2_SIGN: PRINT("sign"); break;
	case MALIGP2_SET_IF_BOTH: PRINT("set_if_both"); break;
	case MALIGP2_SGE: PRINT("sge"); break;
	case MALIGP2_SLT: PRINT("slt"); break;
	case MALIGP2_MIN: PRINT("min"); break;
	case MALIGP2_MAX: PRINT("max"); break;

	case MALIGP2_MUL: PRINT("mul"); break;
	case MALIGP2_MUL_ADD: PRINT("mul_add"); break;
	case MALIGP2_CSEL: PRINT("csel"); break;
	case MALIGP2_MUL_EXPWRAP: PRINT("mul_expwrap"); break;

	case MALIGP2_RCC: PRINT("rcc"); break;
	case MALIGP2_EX2: PRINT("ex2"); break;
	case MALIGP2_LG2: PRINT("lg2"); break;
	case MALIGP2_RSQ: PRINT("rsq"); break;
	case MALIGP2_RCP: PRINT("rcp"); break;
	case MALIGP2_LOG: PRINT("log"); break;
	case MALIGP2_EXP: PRINT("exp"); break;

	case MALIGP2_SET_ADDRESS: PRINT("set_address"); break;

	case MALIGP2_EXP_FLOOR: PRINT("exp2_floor"); break;
	case MALIGP2_FLOOR_LOG: PRINT("floor_log2_abs"); break;
	case MALIGP2_DIV_EXP_FLOOR_LOG: PRINT("reset_exp"); break;
	case MALIGP2_FLOAT_TO_FIXED: PRINT("float_to_fixed"); break;
	case MALIGP2_FIXED_TO_FLOAT: PRINT("fixed_to_float"); break;
	case MALIGP2_CLAMP: PRINT("clamp"); break;
	case MALIGP2_FRAC_FIXED_TO_FLOAT: PRINT("frac_fixed_to_float"); break;



	case MALIGP2_RET: PRINT("ret"); break;
	case MALIGP2_CALL: PRINT("call"); break;
	case MALIGP2_JMP_COND_NOT_CONSTANT: PRINT("jmp_if_not_constant"); break;
	case MALIGP2_JMP_COND: PRINT("jmp_if_misc"); break;
	case MALIGP2_JMP: PRINT("jmp"); break;
	case MALIGP2_CALL_COND: PRINT("call_if_constant"); break;
	case MALIGP2_LOOP_START: PRINT("loop_start"); break;
	case MALIGP2_REPEAT_START: PRINT("repeat_start"); break;
	case MALIGP2_LOOP_END: PRINT("loop_end"); break;
	case MALIGP2_REPEAT_END: PRINT("repeat_end"); break;
	case MALIGP2_STORE_SELECT_ADDRESS_REG: PRINT("store_select_address_reg"); break;

	case MALIGP2_LOAD_INPUT_REG: PRINT("load_input_reg"); break;
	case MALIGP2_LOAD_WORK_REG: PRINT("load_work_reg"); break;
	case MALIGP2_LOAD_CONSTANT: PRINT("load_constant"); break;
	case MALIGP2_LOAD_CONSTANT_INDEXED: PRINT("load_constant_indexed"); break;


	case MALIGP2_STORE_WORK_REG: PRINT("store_work_reg"); break;
	case MALIGP2_STORE_OUTPUT_REG: PRINT("store_output_reg"); break;
	case MALIGP2_STORE_CONSTANT: PRINT("store_constant"); break;
	case MALIGP2_CONSTANT: PRINT("constant"); break;
	case MALIGP2_RESERVE_MOV_MISC: PRINT("reserve_mov_misc"); break;
	case MALIGP2_MOV_MISC: PRINT("mov_misc"); break;
	case MALIGP2_MOV_LUT: PRINT("mov_lut"); break;


#undef PRINT

	case MALIGP2_SET_LOAD_ADDRESS:
	case MALIGP2_SET_STORE_ADDRESS:
	case MALIGP2_RESERVE_MOV:
	case MALIGP2_RESERVE_MOV_IF_NOT_SAME_WORD:
	case MALIGP2_NOP:
	case MALIGP2_NEXT_CYCLE:
	case MALIGP2_FINISH:
	case MALIGP2_SCHEDULE_EXTRA:
	case MALIGP2_UNKNOWN:
	case MALIGP2_LOCKED_CONSTANT:
		assert(0); break;

	}


	return MEM_OK;
}



static const char *reg_name(int reg) {
	const char *regs[] = {"input0.x",
						  "input0.y",
						  "input0.z",
						  "input0.w",
						  "input1.x",
						  "input1.y",
						  "input1.z",
						  "input1.w",
						  "ram.x",
						  "ram.y",
						  "ram.z",
						  "ram.w",
						  "constant_reg.x",
						  "constant_reg.y",
						  "constant_reg.z",
						  "constant_reg.w",
						  "add0_res_delay0",
						  "add1_res_delay0",
						  "mul0_res_delay0",
						  "mul1_res_delay0",
						  "misc_res_delay0",
						  "previous_value",
						  "lut_mode",
						  "misc_res_delay1",
						  "add0_res_delay1",
						  "add1_res_delay1",
						  "mul0_res_delay1",
						  "mul1_res_delay1",
						  "input0_delay.x",
						  "input0_delay.y",
						  "input0_delay.z",
						  "input0_delay.w"};
	if (reg == MALIGP2_REG_UNKNOWN) {
		return "#unknown";
	}
	assert(reg >= 0 && reg < 32);
	return regs[reg];
}



static const char *store_reg_name(int reg) {
	const char *regs[] = {"add0",
						  "add1",
						  "mul0",
						  "mul1",
						  "misc",
						  "mul0_delay",
						  "lut_res_0",
						  "nothing"};
	if (reg == MALIGP2_REG_UNKNOWN) {
		return "#unknown";
	}
	assert(reg >= 0 && reg < 8);
	return regs[reg];
}


static memerr print_input_argument(maligp2_print_context *ctx, maligp2_input_argument *arg)
{
	if(arg->negate) fprintf(ctx->out, "-");
	if(arg->reg_index != MALIGP2_REG_UNKNOWN)
	{
		fprintf(ctx->out, "%s", reg_name(arg->reg_index));

	} else if(arg->arg) {

		const char *name = node_name_get_or_create(ctx, arg->arg);
		ESSL_CHECK(name);
		fprintf(ctx->out, "%s", name);

	} else {
		fprintf(ctx->out, "%s", reg_name(arg->reg_index));
	}


	return MEM_OK;
}


static memerr print_store_input_argument(maligp2_print_context *ctx, maligp2_input_argument *arg)
{
	if(arg->negate) fprintf(ctx->out, "-");
	if(arg->reg_index != MALIGP2_REG_UNKNOWN)
	{
		fprintf(ctx->out, "%s", store_reg_name(arg->reg_index));

	} else if(arg->arg) {

		const char *name = node_name_get_or_create(ctx, arg->arg);
		ESSL_CHECK(name);
		fprintf(ctx->out, "%s", name);

	} else {
		fprintf(ctx->out, "%s", store_reg_name(arg->reg_index));
	}


	return MEM_OK;
}


static memerr print_instruction(maligp2_print_context *ctx, maligp2_instruction *instr)
{
	const char *address_regs[] = {"A0.x", "A0.y", "A0.z", "A0.w"};
	size_t i;

	ESSL_CHECK(print_opcode(ctx, instr));

	for(i = 0; i < MALIGP2_MAX_INPUT_ARGS; ++i)
	{
		if(instr->args[i].arg)
		{
			ESSL_CHECK(node_name_get_or_create(ctx, instr->args[i].arg));
		}
	}
	if(instr->instr_node)
	{
		const char *name = node_name_get_or_create(ctx, instr->instr_node);
		ESSL_CHECK(name);
		fprintf(ctx->out, " %s", name);
	} else if (instr->schedule_class == MALIGP2_SC_MISC)
	{
		fprintf(ctx->out, " misc");
	}

	switch (instr->opcode)
	{
	case MALIGP2_LOAD_CONSTANT_INDEXED:
	{
		const char *address_regs[] = {"A0.x", "A0.y", "A0.z", "A0.w", "undef", "undef", "aL", "undef"};
		assert(instr->address_reg >= 0 && instr->address_reg < 7);

		fprintf(ctx->out, ", [%s + %d]", address_regs[instr->address_reg], instr->address_offset/MALIGP2_NATIVE_VECTOR_SIZE);
		break;
	}
	case MALIGP2_LOAD_INPUT_REG:
	case MALIGP2_LOAD_WORK_REG:
	case MALIGP2_LOAD_CONSTANT:
		fprintf(ctx->out, ", [%d]", instr->address_offset/MALIGP2_NATIVE_VECTOR_SIZE);
		break;
	case MALIGP2_STORE_WORK_REG:
	case MALIGP2_STORE_OUTPUT_REG:
		fprintf(ctx->out, ", [%d], ", instr->address_offset/MALIGP2_NATIVE_VECTOR_SIZE);
		ESSL_CHECK(print_store_input_argument(ctx, &instr->args[0]));
		fprintf(ctx->out, ", ");
		ESSL_CHECK(print_store_input_argument(ctx, &instr->args[1]));
		break;
	case MALIGP2_STORE_CONSTANT:
		assert(instr->address_reg >= 0 && instr->address_reg < 4);
		
		fprintf(ctx->out, " [%s], ", address_regs[instr->address_reg]);
		ESSL_CHECK(print_store_input_argument(ctx, &instr->args[0]));
		fprintf(ctx->out, ", ");
		ESSL_CHECK(print_store_input_argument(ctx, &instr->args[1]));
		break;
	case MALIGP2_SET_ADDRESS:
		assert(instr->address_reg >= 0 && instr->address_reg < 4);
		fprintf(ctx->out, " %s, ", address_regs[instr->address_reg]);
		ESSL_CHECK(print_input_argument(ctx, &instr->args[0]));
		break;
	case MALIGP2_STORE_SELECT_ADDRESS_REG:
		fprintf(ctx->out, " %s", address_regs[instr->address_reg]);
		break;
	case MALIGP2_CONSTANT:
		assert(instr->instr_node != NULL);
		if(instr->instr_node->hdr.kind == EXPR_KIND_CONSTANT)
		{
			fprintf(ctx->out, ", %f", ctx->desc->scalar_to_float(instr->instr_node->expr.u.value[0]));
		} else if(instr->instr_node->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE)
		{
			const char *name;
			ESSL_CHECK(name = _essl_string_to_cstring(ctx->pool, instr->instr_node->expr.u.sym->name));
			fprintf(ctx->out, ", %s + %d", name, instr->address_offset/MALIGP2_NATIVE_VECTOR_SIZE);
		}
		break;
	case MALIGP2_JMP_COND_NOT_CONSTANT:
	case MALIGP2_JMP_COND:
	case MALIGP2_JMP:
	case MALIGP2_LOOP_END:
	case MALIGP2_REPEAT_END:
	{
		const char *name = _essl_unique_name_get_or_create(&ctx->block_names, instr->jump_target);
		ESSL_CHECK(name);
		fprintf(ctx->out, " %s", name);
		break;
	}
	default:
		for(i = 0; i < MALIGP2_MAX_INPUT_ARGS; ++i)
		{
			if(instr->args[i].arg || instr->args[i].reg_index != MALIGP2_REG_UNKNOWN)
			{
				fprintf(ctx->out, ", ");
				ESSL_CHECK(print_input_argument(ctx, &instr->args[i]));
			}
			
		}
	}

	fprintf(ctx->out, " \n");

	return MEM_OK;
}


static memerr print_block(maligp2_print_context *ctx, basic_block *b)
{
	maligp2_instruction_word *word;
	phi_list *phi;
	const char *name = _essl_unique_name_get_or_create(&ctx->block_names, b);
	ESSL_CHECK(name);
	fprintf(ctx->out, "%s:\n", name);

	for(phi = b->phi_nodes; phi != 0; phi = phi->next)
	{
		phi_source *src;
		const char *phiname = node_name_get_or_create(ctx, phi->phi_node);
		ESSL_CHECK(phiname);
		fprintf(ctx->out, "%s = phi(", phiname);
		for(src = phi->phi_node->expr.u.phi.sources; src != 0; src = src->next)
		{
			const char *phiname = node_name_get_or_create(ctx, src->source);
			if(src != phi->phi_node->expr.u.phi.sources) fprintf(ctx->out, ", ");
			ESSL_CHECK(phiname);
			fprintf(ctx->out, "%s", phiname);

		}
		fprintf(ctx->out, ")\n");

	}


	for(word = (maligp2_instruction_word *)b->earliest_instruction_word; word != 0; word = word->successor)
	{
		int i;
		if (word->original_cycle != word->cycle) {
			if (word->original_cycle == 0) {
				fprintf(ctx->out, "cycle %d (inserted)  -  positions %d-%d\n", word->cycle, START_OF_CYCLE(word->cycle), END_OF_CYCLE(word->cycle));
			} else {
				fprintf(ctx->out, "cycle %d (%d)  -  positions %d-%d\n", word->cycle, word->original_cycle, START_OF_CYCLE(word->cycle), END_OF_CYCLE(word->cycle));
			}
		} else {
			fprintf(ctx->out, "cycle %d  -  positions %d-%d\n", word->cycle, START_OF_CYCLE(word->cycle), END_OF_CYCLE(word->cycle));
		}


#define DO(unit, v) if(v) { fprintf(ctx->out, "<%s> ", unit);ESSL_CHECK(print_instruction(ctx, v)); }
		DO("  load0_x   ", word->u.real_slots.load0[0]);
		DO("  load0_y   ", word->u.real_slots.load0[1]);
		DO("  load0_z   ", word->u.real_slots.load0[2]);
		DO("  load0_w   ", word->u.real_slots.load0[3]);
		DO("  load1_x   ", word->u.real_slots.load1[0]);
		DO("  load1_y   ", word->u.real_slots.load1[1]);
		DO("  load1_z   ", word->u.real_slots.load1[2]);
		DO("  load1_w   ", word->u.real_slots.load1[3]);
		DO("load_const_x", word->u.real_slots.load_const[0]);
		DO("load_const_y", word->u.real_slots.load_const[1]);
		DO("load_const_z", word->u.real_slots.load_const[2]);
		DO("load_const_w", word->u.real_slots.load_const[3]);


		DO("    add0    ", word->u.real_slots.add0);
		DO("    add1    ", word->u.real_slots.add1);

		DO("    mul0    ", word->u.real_slots.mul0);
		DO("    mul1    ", word->u.real_slots.mul1);

		DO("    lut     ", word->u.real_slots.lut);
		DO("    misc    ", word->u.real_slots.misc);
		for(i = 0; i < MALIGP2_MAX_MOVES ; ++i)
		{
			if(word->reserved_moves[i] != 0 && !word->move_reservation_fulfilled[i])
			{
				const char *name = node_name_get_or_create(ctx, word->reserved_moves[i]);
				ESSL_CHECK(name);
				fprintf(ctx->out, "< res mov %d  > mov %s, %s\n", i, name, name);
			}

		}



		DO("  store_xy  ", word->u.real_slots.store_xy);
		DO("  store_zw  ", word->u.real_slots.store_zw);
		DO("   branch   ", word->u.real_slots.branch);

#undef DO
		fprintf(ctx->out, "Moves available: %d\n", word->n_moves_available);
		if (_essl_maligp2_inseparable_from_successor(word))
		{
			fprintf(ctx->out, "||\n");
		} else {
			fprintf(ctx->out, "\n");
		}
	}


	fprintf(ctx->out, "\n");
	return MEM_OK;
}













memerr _essl_maligp2_print_function(FILE *out, mempool *pool, symbol *fun, target_descriptor *desc, unique_name_context *node_names)
{
	size_t i;
	maligp2_print_context context, *ctx = &context;
	control_flow_graph *g = fun->control_flow_graph;
	const char *funname = _essl_string_to_cstring(pool, fun->name);
	ESSL_CHECK(funname);
	ESSL_CHECK(ctx->node_names = node_names);
	ESSL_CHECK(_essl_unique_name_init(&ctx->block_names, pool, funname));
	ESSL_CHECK(_essl_unique_name_set(&ctx->block_names, g->entry_block, funname));
	ctx->desc = desc;
	ctx->out = out;
	ctx->pool = pool;


	for(i = 0; i < g->n_blocks; ++i)
	{
		ESSL_CHECK(print_block(ctx, g->output_sequence[i]));

	}

	fprintf(ctx->out, "\n");

	return MEM_OK;
}
