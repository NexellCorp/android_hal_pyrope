/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali200/mali200_printer.h"
#include "common/essl_stringbuffer.h"
#include "common/unique_names.h"
#include "mali200/mali200_instruction.h"
#include "mali200/mali200_liveness.h"
#include "common/basic_block.h"


typedef struct 
{
	FILE *out;
	unique_name_context block_names;
	unique_name_context *node_names;

} m200_print_context;



static /*@null@*/ const char *node_name_get_or_create(m200_print_context *ctx, node *n)
{
	while(n->hdr.kind == EXPR_KIND_TRANSFER)
	{
		n = GET_CHILD(n, 0);
	}
	return _essl_unique_name_get_or_create(ctx->node_names, n);

}


static void print_comparison(m200_print_context *ctx, m200_comparison cmp)
{
#define PRINT(str) fprintf(ctx->out, "%s", str)

	switch(cmp)
	{
		case M200_CMP_NEVER:
			PRINT(".never");
			break;
		case M200_CMP_LT:
			PRINT(".lt");
			break;
		case M200_CMP_EQ:
			PRINT(".eq");
			break;
		case M200_CMP_LE:
			PRINT(".le");
			break;
		case M200_CMP_GT:
			PRINT(".gt");
			break;
		case M200_CMP_NE:
			PRINT(".ne");
			break;
		case M200_CMP_GE:
			PRINT(".ge");
			break;
		case M200_CMP_ALWAYS:
			PRINT(".always");
			break;
	}
}

static memerr print_opcode(m200_print_context *ctx, m200_instruction *instr)
{
	switch(instr->opcode)
	{
#define PRINT(str) fprintf(ctx->out, "%s", str)

	case M200_NOP: PRINT("nop"); break; 
	
	case M200_ADD: PRINT("add"); break;    
    case M200_ADD_X2: PRINT("add_x2"); break; 
    case M200_ADD_X4: PRINT("add_x4"); break;
    case M200_ADD_X8: PRINT("add_x8"); break;
    case M200_ADD_D2: PRINT("add_d2"); break;
    case M200_ADD_D4: PRINT("add_d4"); break;
    case M200_ADD_D8: PRINT("add_d8"); break;
    case M200_HADD3: PRINT("hadd3"); break;   
    case M200_HADD4: PRINT("hadd4"); break;   
	case M200_MUL: PRINT("mul"); break;    
    case M200_MUL_X2: PRINT("mul_x2"); break; 
    case M200_MUL_X4: PRINT("mul_x4"); break;
    case M200_MUL_X8: PRINT("mul_x8"); break;
    case M200_MUL_D2: PRINT("mul_d2"); break;
    case M200_MUL_D4: PRINT("mul_d4"); break;
    case M200_MUL_D8: PRINT("mul_d8"); break;
    case M200_MUL_W1: PRINT("mul_w1"); break; 

	case M200_2X2ADD: PRINT("2x2add"); break;    
	case M200_NOT: PRINT("not"); break;    
    case M200_AND: PRINT("and"); break;    
	case M200_OR: PRINT("or"); break;     
	case M200_XOR: PRINT("xor"); break;    
	case M200_CMP: PRINT("cmp"); break;    
	case M200_MIN: PRINT("min"); break;    
	case M200_MAX: PRINT("max"); break;    
	case M200_SEL: PRINT("sel"); break;   
	case M200_VSEL: PRINT("vsel"); break;   
 
	case M200_DERX: PRINT("derx"); break;   
	case M200_DERY: PRINT("dery"); break;   
	case M200_FRAC: PRINT("frac"); break;   
	case M200_FLOOR: PRINT("floor"); break;  
	case M200_CEIL: PRINT("ceil"); break;   
	case M200_SWZ: PRINT("swz"); break;    
    case M200_CONS13: PRINT("cons13"); break; 
    case M200_CONS22: PRINT("cons22"); break; 
    case M200_CONS31: PRINT("cons31"); break; 
	
	case M200_TEX: PRINT("tex"); break;

	case M200_JMP: PRINT("jmp"); break;  
    case M200_JMP_REL: PRINT("jmp_rel"); break; 
	case M200_CALL: PRINT("call"); break; 
	case M200_CALL_REL: PRINT("call_rel"); break; 
    case M200_RET: PRINT("ret"); break;  
	case M200_KILL: PRINT("kill"); break; 
    case M200_ALT_KILL: PRINT("alt_kill"); break; 
	case M200_GLOB_END: PRINT("end"); break;
	
	case M200_LD_UNIFORM: PRINT("ld_uniform"); break;   
	case M200_LD_STACK: PRINT("ld_stack"); break;   
	case M200_LD_REL: PRINT("ld_rel"); break;   
	case M200_LD_REG: PRINT("ld_reg"); break;   
	case M200_LD_SUBREG: PRINT("ld_subreg"); break; 
	case M200_LEA: PRINT("lea"); break;   
	case M200_ST_STACK: PRINT("st_stack"); break;   
	case M200_ST_REL: PRINT("st_rel"); break;   
	case M200_ST_REG: PRINT("st_reg"); break;   
	case M200_ST_SUBREG: PRINT("st_subreg"); break; 
    case M200_LD_RGB: PRINT("ld_rgb"); break;  
    case M200_LD_ZS: PRINT("ld_zs"); break;   
	case M200_MOV: PRINT("mov"); break;   
	
	case M200_RCP: PRINT("rcp"); break;       
	case M200_RCC: PRINT("rcc"); break;       
	case M200_RSQ: PRINT("rsq"); break;       
	case M200_SQRT: PRINT("sqrt"); break;      
	case M200_EXP: PRINT("exp"); break;       
	case M200_LOG: PRINT("log"); break;       
	case M200_SIN: PRINT("sin"); break;       
	case M200_COS: PRINT("cos"); break;       
	case M200_ATAN1_IT1: PRINT("atan1_it1"); break; 
	case M200_ATAN2_IT1: PRINT("atan2_it1"); break; 
	case M200_ATAN_IT2: PRINT("atan_it2"); break;  

	
	case M200_VAR: PRINT("var"); break;    
	case M200_VAR_DIV_Y: PRINT("var_div_y"); break;    
	case M200_VAR_DIV_Z: PRINT("var_div_z"); break;    
	case M200_VAR_DIV_W: PRINT("var_div_w"); break;   
	case M200_VAR_CUBE: PRINT("var_cube"); break;    

	case M200_MOV_DIV_Y: PRINT("mov_div_y"); break;    
	case M200_MOV_DIV_Z: PRINT("mov_div_z"); break;    
	case M200_MOV_DIV_W: PRINT("mov_div_w"); break;    
	case M200_MOV_CUBE: PRINT("mov_cube"); break;    
    case M200_POS: PRINT("pos"); break;    
	case M200_POINT: PRINT("point"); break; 
	case M200_MISC_VAL: PRINT("misc_val"); break; 
	case M200_NORM3: PRINT("norm3"); break; 
#undef PRINT
	case M200_REG_MOV: assert(instr->opcode != M200_REG_MOV); break;
	case M200_CONST_MOV: assert(instr->opcode != M200_CONST_MOV); break;
	case M200_LOAD_RET: assert(instr->opcode != M200_LOAD_RET); break;
	case M200_UNKNOWN: assert(instr->opcode != M200_UNKNOWN); break;
	}
	return MEM_OK;
}


static int input_swizzle_size(m200_instruction *instr, m200_input_argument *arg)
{
	int n;
	switch (instr->opcode)
	{
	case M200_RCP:
	case M200_RCC:
	case M200_RSQ:
	case M200_SQRT:
	case M200_EXP:
	case M200_LOG:
	case M200_SIN:
	case M200_COS:
	case M200_ATAN1_IT1:
	case M200_ATAN2_IT1:
		return 1;
	case M200_ATAN_IT2:
		return 3;
	case M200_2X2ADD:
		n = 1;
		if(instr->output_swizzle.indices[1] >= 0)
		{
			n = 2;
		}
		if(&instr->args[0] == arg) return n*2;
		return n;
	default:
		return (instr->instr_node != NULL) ? GET_NODE_VEC_SIZE(instr->instr_node) : M200_NATIVE_VECTOR_SIZE;
	}
}

static memerr print_swizzle(m200_print_context *ctx, m200_instruction *instr, m200_input_argument *arg)
{
	size_t i;
	swizzle_pattern swz = arg->swizzle;
	size_t size = input_swizzle_size(instr, arg);
	
	fprintf(ctx->out, ".");
	for(i = 0; i < size; ++i)
	{
		if(swz.indices[i] >= 0 && swz.indices[i] < M200_NATIVE_VECTOR_SIZE)
		{
			fprintf(ctx->out, "%c", "xyzw"[swz.indices[i]]);
		} else {
			fprintf(ctx->out, "?");
		}
	}
	return MEM_OK;
}

static memerr print_output_mask(m200_print_context *ctx, swizzle_pattern swz, node *n)
{
	size_t i;
	size_t size = (n != NULL) ? GET_NODE_VEC_SIZE(n) : M200_NATIVE_VECTOR_SIZE;
	
	fprintf(ctx->out, ".");
	for(i = 0; i < size; ++i)
	{
		if(swz.indices[i] >= 0 && swz.indices[i] < M200_NATIVE_VECTOR_SIZE)
		{
			fprintf(ctx->out, "1");
		} else {
			fprintf(ctx->out, "0");
		}
	}
	return MEM_OK;
}



const char *
_essl_reg_name(int reg) {
	const char *regs[] = {"r0", "r1", "r2", "r3", "r4", "r5", 
						  "r6", "r7", "r8", "r9", "r10", "r11", 
						  "constant0", "constant1", "#tex", "#load"};
	if (reg == M200_REG_UNKNOWN) {
		return "#unknown";
	} else if (reg == M200_R_IMPLICIT) {
		return "#implicit";
	}
	assert(reg >= 0 && reg < 16);
	return regs[reg];
}


static memerr print_input_argument(m200_print_context *ctx, m200_instruction *instr, m200_input_argument *arg)
{
	if(arg->negate) fprintf(ctx->out, "-");
	if(arg->absolute_value) fprintf(ctx->out, "|");
	if(arg->reg_index != M200_REG_UNKNOWN)
	{
		fprintf(ctx->out, "%s", _essl_reg_name(arg->reg_index));

	} else if(arg->arg) {

		const char *name = node_name_get_or_create(ctx, arg->arg);
		ESSL_CHECK(name);
		fprintf(ctx->out, "%s", name);

	} else {
		fprintf(ctx->out, "%s", _essl_reg_name(arg->reg_index));
	}

	ESSL_CHECK(print_swizzle(ctx, instr, arg));
	if(arg->absolute_value) fprintf(ctx->out, "|");

	return MEM_OK;
}


static memerr print_address_argument(m200_print_context *ctx, m200_instruction *instr, m200_input_argument *arg)
{
	fprintf(ctx->out, "%d * (", instr->address_multiplier);
	if(arg->arg || arg->reg_index != M200_REG_UNKNOWN)
	{
		ESSL_CHECK(print_input_argument(ctx, instr, arg));
		if(instr->address_offset >= 0)
		{
			fprintf(ctx->out, "+");
		}
	}
	fprintf(ctx->out, "%d)", instr->address_offset/instr->address_multiplier);
	return MEM_OK;
}

static memerr print_instruction(m200_print_context *ctx, m200_instruction *instr)
{
	size_t i;
	m200_opcode opcode = instr->opcode;

	ESSL_CHECK(print_opcode(ctx, instr));

	for (i = 0; i < M200_MAX_INPUT_ARGS; ++i) {
		if (instr->args[i].arg) {
			(void)node_name_get_or_create(ctx, instr->args[i].arg);
		}
	}
	if (instr->instr_node) {
		if (instr->out_reg != M200_REG_UNKNOWN)	{
			const char *name = node_name_get_or_create(ctx, 
															   instr->instr_node);
			ESSL_CHECK(name);
			fprintf(ctx->out, " %s(%s)", _essl_reg_name(instr->out_reg), name);
		} else {
			const char *name = node_name_get_or_create(ctx, 
															   instr->instr_node);
			ESSL_CHECK(name);
			fprintf(ctx->out, " %s", name);
		}
	} else if (instr->out_reg != M200_REG_UNKNOWN) {
			fprintf(ctx->out, " %s", _essl_reg_name(instr->out_reg));
	}
	switch (instr->output_mode) {
	case M200_OUTPUT_NORMAL:
		break;
	case M200_OUTPUT_CLAMP_0_1:
		fprintf(ctx->out, "_sat");
		break;
	case M200_OUTPUT_CLAMP_0_INF:
		fprintf(ctx->out, "_pos");
		break;
	case M200_OUTPUT_TRUNCATE:
		fprintf(ctx->out, "_trunc");
		break;
	}
	ESSL_CHECK(print_output_mask(ctx, instr->output_swizzle, instr->instr_node));

	if (opcode == M200_LD_UNIFORM || opcode == M200_LD_STACK || opcode == M200_LD_REL) {
		if (opcode == M200_LD_REL) {
			fprintf(ctx->out, ", ");
			ESSL_CHECK(print_input_argument(ctx, instr, &instr->args[2]));
			fprintf(ctx->out, " + [");

		} else {

			fprintf(ctx->out, ", [");
		}
		ESSL_CHECK(print_address_argument(ctx, instr, &instr->args[0]));
		fprintf(ctx->out, "]");
	} else if (opcode == M200_TEX) {
		if(instr->opcode_flags & M200_TEX_NO_RESET_SUMMATION) fprintf(ctx->out, "_noreset");
		if(instr->opcode_flags & M200_TEX_IGNORE) fprintf(ctx->out, "_ignore");
		if(instr->opcode_flags & M200_TEX_LOD_REPLACE) fprintf(ctx->out, "_lod");
		fprintf(ctx->out, " sampler[");
		ESSL_CHECK(print_address_argument(ctx, instr, &instr->args[0]));
		fprintf(ctx->out, "], lod(");
		if (instr->args[1].arg || instr->args[1].reg_index != -1)	{
			ESSL_CHECK(print_input_argument(ctx, instr, &instr->args[1]));
			fprintf(ctx->out, " + ");
		}
		fprintf(ctx->out, "%f)", instr->lod_offset);
	} else if (opcode == M200_ST_STACK || opcode == M200_ST_REL 
			   || opcode == M200_LD_SUBREG || opcode == M200_ST_SUBREG)	{
		if (opcode == M200_ST_REL) {
			fprintf(ctx->out, " [");
			ESSL_CHECK(print_input_argument(ctx, instr, &instr->args[2]));
			fprintf(ctx->out, " + ");
		} else {
			fprintf(ctx->out, " [");
		}
		ESSL_CHECK(print_address_argument(ctx, instr, &instr->args[0]));
		fprintf(ctx->out, "], ");
		if (opcode == M200_ST_SUBREG) {
			ESSL_CHECK(print_input_argument(ctx, instr, &instr->args[2]));
		} else {
			ESSL_CHECK(print_input_argument(ctx, instr, &instr->args[1]));
		}
	} else if (opcode == M200_VAR || opcode == M200_VAR_DIV_Y || opcode == M200_VAR_DIV_Z || opcode == M200_VAR_DIV_W || opcode == M200_VAR_CUBE) {
		/* NOTE: varying flags are ignored */
		fprintf(ctx->out, ", var[");
		ESSL_CHECK(print_address_argument(ctx, instr, &instr->args[0]));
		fprintf(ctx->out, "]");
	} else if (opcode == M200_JMP || opcode == M200_CMP) {
		if (instr->compare_function == M200_CMP_NEVER) {
			fprintf(ctx->out, " never");
		}
		else if (instr->compare_function != M200_CMP_ALWAYS) {
			const char *cmp_funs[] = {"<=>", "<", "==", "<=", ">", "!=", ">=", "yes"};
			fprintf(ctx->out, " (");
			ESSL_CHECK(print_input_argument(ctx, instr, &instr->args[0]));
			fprintf(ctx->out, " %s ", cmp_funs[instr->compare_function]);

			ESSL_CHECK(print_input_argument(ctx, instr, &instr->args[1]));
			fprintf(ctx->out, ")");
		}
		if (instr->jump_target) {
			fprintf(ctx->out, " %s", _essl_unique_name_get_or_create(&ctx->block_names, 
																	 instr->jump_target));
		}
	} else if (opcode == M200_RET) {
		fprintf(ctx->out, " %d", instr->address_offset/instr->address_multiplier);
	} else if (opcode == M200_CALL) {
		fprintf(ctx->out, ", %d", instr->address_offset/instr->address_multiplier);
	} else {
		if(opcode == M200_KILL || opcode == M200_GLOB_END)
		{
			print_comparison(ctx, instr->compare_function);
		}
		for (i = 0; i < M200_MAX_INPUT_ARGS; ++i) {
			if (instr->args[i].arg || instr->args[i].reg_index != M200_REG_UNKNOWN)	{
				fprintf(ctx->out, ", ");
				ESSL_CHECK(print_input_argument(ctx, instr, &instr->args[i]));
			}
		}
	}
	fprintf(ctx->out, "\n");
	return MEM_OK;
}


static memerr print_block(m200_print_context *ctx, basic_block *b)
{
	m200_instruction_word *word;
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


	for(word = (m200_instruction_word *)b->earliest_instruction_word; word != 0; word = word->successor)
	{
		unsigned i; 
		if (word->original_cycle != word->cycle) {
			if (word->original_cycle == 0) {
				fprintf(ctx->out, "cycle %d (inserted)  -  positions %d-%d\n", word->cycle, START_OF_CYCLE(word->cycle), END_OF_CYCLE(word->cycle));
			} else {
				fprintf(ctx->out, "cycle %d (%d)  -  positions %d-%d\n", word->cycle, word->original_cycle, START_OF_CYCLE(word->cycle), END_OF_CYCLE(word->cycle));
			}
		} else {
			fprintf(ctx->out, "cycle %d  -  positions %d-%d\n", word->cycle, START_OF_CYCLE(word->cycle), END_OF_CYCLE(word->cycle));
		}

		for(i = 0; i < 2; ++i)
		{
			if(word->n_embedded_entries[i])
			{
				unsigned j;
				fprintf(ctx->out, "constant%u =", i);
				for(j = 0; j < word->n_embedded_entries[i]; ++j)
				{
					fprintf(ctx->out, " %.2f", word->embedded_constants[i][j]);
				}
				fprintf(ctx->out, "\n");

			}
		}

#define DO(unit, v) if(v) { fprintf(ctx->out, "<%s> ", unit);ESSL_CHECK(print_instruction(ctx, v)); }
		DO(" var  ", word->var);
		DO(" tex  ", word->tex);
		DO(" load ", word->load);

		DO(" mul4 ", word->mul4);
		DO(" mul1 ", word->mul1);

		DO(" add4 ", word->add4);
		DO(" add1 ", word->add1);

		DO("  lut ", word->lut);
		DO("store ", word->store);
		DO("branch", word->branch);

#undef DO
		if(word->end_of_program_marker) fprintf(ctx->out, "end_of_program_marker\n");
		fprintf(ctx->out, "\n");
	}


	fprintf(ctx->out, "\n");
	return MEM_OK;
}













memerr _essl_m200_print_function(FILE *out, mempool *pool, symbol *fun, unique_name_context **node_name_out)
{
	size_t i;
	m200_print_context context, *ctx = &context;
	control_flow_graph *g = fun->control_flow_graph;
	const char *funname = _essl_string_to_cstring(pool, fun->name);
	ESSL_CHECK(funname);
	if (node_name_out != 0 && *node_name_out != 0)
	{
		ctx->node_names = *node_name_out;
	}
	else
	{
		ESSL_CHECK(ctx->node_names = _essl_mempool_alloc(pool, sizeof(unique_name_context)));
		ESSL_CHECK(_essl_unique_name_init(ctx->node_names, pool, "temp"));
		if (node_name_out != 0)
		{
			*node_name_out = ctx->node_names;
		}
	}
	ESSL_CHECK(_essl_unique_name_init(&ctx->block_names, pool, funname));
	ESSL_CHECK(_essl_unique_name_set(&ctx->block_names, g->entry_block, funname));
	ctx->out = out;


	for(i = 0; i < g->n_blocks; ++i)
	{
		ESSL_CHECK(print_block(ctx, g->output_sequence[i]));

	}

	fprintf(ctx->out, "\n");

	return MEM_OK;
}
