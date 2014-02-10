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
#include "mali200/mali200_instruction.h"
#include "backend/extra_info.h"

static essl_bool swizzle_has_any_enabled(swizzle_pattern swz) 
{
	unsigned i;
	for(i = 0; i < N_COMPONENTS; ++i)
	{
		if(swz.indices[i] != -1) return ESSL_TRUE;
	}
	return ESSL_FALSE;
}

static void integrate_output(m200_instruction *inst, swizzle_pattern *extra_swz /* reg to var */) {
	if (inst->instr_node) {
		/* Allocated register */
		node_extra *info = EXTRA_INFO(inst->instr_node);
		swizzle_pattern new_output_swizzle = _essl_create_undef_swizzle();
		int subreg;
		assert(info != 0);
		assert(info->reg_allocated);
		assert(info->out_reg != M200_R_SPILLED);

		*extra_swz = _essl_create_undef_swizzle();

		inst->out_reg = info->out_reg;

		for (subreg = 0 ; subreg < M200_NATIVE_VECTOR_SIZE ; subreg++) {
			if (inst->output_swizzle.indices[subreg] != -1) {
				int new_subreg = info->reg_swizzle.indices[subreg];
				if (new_subreg >= 0) {
					extra_swz->indices[new_subreg] = subreg;
					new_output_swizzle.indices[new_subreg] = new_subreg;
				}
			}
		}
		inst->output_swizzle = new_output_swizzle;

		inst->instr_node = 0;
	} else {
		/* Fixed register */
		int subreg;

		for (subreg = 0 ; subreg < N_COMPONENTS; subreg++) {
			if (inst->output_swizzle.indices[subreg] != -1) {
				extra_swz->indices[subreg] = subreg;
			} else {
				extra_swz->indices[subreg] = -1;
			}
		}
	}
}

/** Characterisation of how a register swizzle on the output
 *  of an instruction propagates to the instruction inputs.
 */
typedef enum {
	SWZ_NO_INPUTS,   /**< No inputs, should not occur */
	SWZ_IGNORE,      /**< Ignore extra swizzle */
	SWZ_PROPAGATE,   /**< Propagate extra swizzle to input swizzle */
	SWZ_OUTPUT,      /**< Put extra swizzle in output swizzle */
	SWZ_PROP_TO_MUL, /**< Propagate extra swizzle to input swizzle and to mul slot */
	SWZ_EXTENDED     /**< Set extended swizzle - ignores input swizzle */
} swizzle_prop;

/** Characterizes an instruction wrt. swizzle propagation */
static swizzle_prop get_swizzle_propagation(m200_instruction *inst) {
	switch (inst->opcode) {
		/* Instructions with no inputs */
	case M200_UNKNOWN:
	case M200_NOP:
	case M200_POS:
	case M200_POINT:
	case M200_MISC_VAL:
		return SWZ_NO_INPUTS;

		/* Instructions with component-wise input-output connectivity */
	case M200_MOV:
	case M200_ADD:
	case M200_ADD_X2:
	case M200_ADD_X4:
	case M200_ADD_X8:
	case M200_ADD_D2:
	case M200_ADD_D4:
	case M200_ADD_D8:
	case M200_MUL:
	case M200_MUL_X2:
	case M200_MUL_X4:
	case M200_MUL_X8:
	case M200_MUL_D2:
	case M200_MUL_D4:
	case M200_MUL_D8:
	case M200_NOT:
	case M200_AND:
	case M200_OR:
	case M200_XOR:
	case M200_CMP:
	case M200_MIN:
	case M200_MAX:
	case M200_DERX:
	case M200_DERY:
	case M200_FRAC:
	case M200_FLOOR:
	case M200_CEIL:
	case M200_CONS13:
	case M200_CONS22:
	case M200_CONS31:
	case M200_SEL:
		return SWZ_PROPAGATE;

		/* Instructions with no outputs */
	case M200_JMP:
	case M200_JMP_REL:
	case M200_CALL:
	case M200_CALL_REL:
	case M200_RET:
	case M200_KILL:
	case M200_ALT_KILL:
	case M200_GLOB_END:
		/* Load/store instructions */
	case M200_VAR:
	case M200_VAR_DIV_Y:
	case M200_VAR_DIV_Z:
	case M200_VAR_DIV_W:
	case M200_VAR_CUBE:
	case M200_LD_UNIFORM:
	case M200_LD_STACK:
	case M200_LD_REL:
	case M200_LD_REG:
	case M200_LD_SUBREG:
	case M200_LEA:
	case M200_ST_STACK:
	case M200_ST_REL:
	case M200_ST_REG:
	case M200_ST_SUBREG:
	case M200_LD_RGB:
	case M200_LD_ZS:
	case M200_REG_MOV:
	case M200_CONST_MOV:
	case M200_LOAD_RET:
		/* Instructions without component-wise input-output connectivity */
	case M200_HADD3:
	case M200_HADD4:
	case M200_2X2ADD:
	case M200_MUL_W1:
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
	case M200_ATAN_IT2:
	case M200_MOV_DIV_Y:
	case M200_MOV_DIV_Z:
	case M200_MOV_DIV_W:
	case M200_MOV_CUBE:
	case M200_NORM3:
		return SWZ_IGNORE;

		/* Instructions with output swizzle */
	case M200_TEX:
		return SWZ_OUTPUT;

		/* Extra propagation to comparison in mul slot */
	case M200_VSEL:
		return SWZ_PROP_TO_MUL;

		/* Extended swizzle */
	case M200_SWZ:
		return SWZ_EXTENDED;
	}

	/* Dummy return */
	return SWZ_NO_INPUTS;
}


static void integrate_inputs(m200_instruction *inst, swizzle_pattern *extra_swz /* reg to var */) {
	int i;
	swizzle_prop swzp = get_swizzle_propagation(inst);
	for (i = 0 ; i < M200_MAX_INPUT_ARGS ; i++) {
		if (inst->args[i].arg) {
			/* Allocated register */
			node_extra *info = EXTRA_INFO(inst->args[i].arg);
			assert(info != 0);
			assert(info->reg_allocated);
			assert(info->out_reg != M200_R_SPILLED);

			inst->args[i].reg_index = info->out_reg;
			switch (swzp) {
			case SWZ_NO_INPUTS:
				assert(0); /* Should not happen, as we are currently looking at an input */
				break;
			case SWZ_IGNORE:
			case SWZ_OUTPUT:
				inst->args[i].swizzle = _essl_combine_swizzles(inst->args[i].swizzle, info->reg_swizzle);
				break;
			case SWZ_PROPAGATE:
			case SWZ_PROP_TO_MUL:
				inst->args[i].swizzle = _essl_combine_swizzles(_essl_combine_swizzles(*extra_swz, inst->args[i].swizzle), info->reg_swizzle);
				break;
			case SWZ_EXTENDED:
				assert(0 && "Extended swizzle");
				break;
			}

			inst->args[i].arg = 0;
		} else if (inst->args[i].reg_index != M200_REG_UNKNOWN) {
			/* Fixed register */
			switch (swzp) {
			case SWZ_NO_INPUTS:
				assert(0); /* Should not happen, as we are currently looking at an input */
				break;
			case SWZ_IGNORE:
			case SWZ_OUTPUT:
				/* No swizzle to propagate */
				break;
			case SWZ_PROPAGATE:
			case SWZ_PROP_TO_MUL:
				inst->args[i].swizzle = _essl_combine_swizzles(*extra_swz, inst->args[i].swizzle);
				break;
			case SWZ_EXTENDED:
				assert(0 && "Extended swizzle");
				break;
			}
		}
	}

	/* Handle additional extra swizzle propagation */
	switch (swzp) {
	case SWZ_OUTPUT:
		inst->output_swizzle = _essl_combine_swizzles(*extra_swz, inst->output_swizzle);
		break;
	case SWZ_PROP_TO_MUL:
		assert(0 && "Extended swizzle");
		break;
	default:
		break;
	}

}

static essl_bool instruction_is_dead(m200_instruction *inst) {
	assert(!_essl_mali200_opcode_has_side_effects(inst->opcode) || swizzle_has_any_enabled(inst->output_swizzle) || inst->instr_node == 0);
	return !swizzle_has_any_enabled(inst->output_swizzle) && !_essl_mali200_opcode_has_side_effects(inst->opcode);
}

static void integrate_instruction(m200_instruction_word *word, m200_instruction **instp) {
	if (*instp) {
		essl_bool is_varying_used_for_tex = (*instp)->schedule_class == M200_SC_VAR && word->tex != NULL;

		if (instruction_is_dead(*instp) && !is_varying_used_for_tex) {
			*instp = 0;
		} else {
			swizzle_pattern extra_swz;
			integrate_output(*instp, &extra_swz);
			integrate_inputs(*instp, &extra_swz);

			if (instruction_is_dead(*instp))
			{
				if(is_varying_used_for_tex) 
				{
					(*instp)->out_reg = M200_R_IMPLICIT;
				} else {
					*instp = 0;
				}
			}
		}
	}
}

void _essl_mali200_integrate_allocations(control_flow_graph *cfg) {
	unsigned i;
	for (i = 0 ; i < cfg->n_blocks ; i++) {
		basic_block *block = cfg->output_sequence[i];
		m200_instruction_word *word;
		for (word = (m200_instruction_word *)block->earliest_instruction_word ; word != 0 ; word = word->successor) {
			integrate_instruction(word, &word->var);
			integrate_instruction(word, &word->tex);
			integrate_instruction(word, &word->load);
			integrate_instruction(word, &word->mul4);
			integrate_instruction(word, &word->mul1);
			integrate_instruction(word, &word->add4);
			integrate_instruction(word, &word->add1);
			integrate_instruction(word, &word->lut);
			integrate_instruction(word, &word->store);
			integrate_instruction(word, &word->branch);
		}
	}
}
