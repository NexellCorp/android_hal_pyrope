/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
/* Make the final, binary, mali200 program, 
given a series of 'm200_instruction_word'-s 
*/

#include "common/essl_system.h"
#include "mali200/mali200_emit.h"
#include "mali200/mali200_instruction.h"
#include "common/basic_block.h"
#include "common/error_reporting.h"

#ifdef DEBUGPRINT
#include "mali200/mali200_printer.h"
#endif


#define ERROR_CONTEXT  ctx->err_ctx
#define SOURCE_OFFSET  0


/** Return codes for emit m200_instruction funcs */
typedef unsigned int return_code;
#define RET_ERROR 0                     /* for use with ESSL_CHECK */
#define RET_OK 1                            /* everything normal, including: do prefetch next */
#define RET_NO_PREFETCH 2
#define RET_RENDEZ 3
#define RET_NO_RENDEZ 4
#define RET_MAX_ENUM 5                       /* must be the highest value:  # preceding alt.s */


typedef return_code (*emit_instr_func)(mali200_emit_context*, m200_instruction*);


/** normalize variables given in context:  wrd_x and bit_x, 
 * after adding a bit offset to bit_x
 */
#define NORM_WRD_BIT  while (bit_x >= 32) { ++wrd_x;  bit_x -= 32; }



/** Swizzle pattern as (8 low bits of) an int */
static int swizz_as_8(swizzle_pattern swz)
{
	int ix;
	int res = 0;
	int rep = -1;

	for (ix = 0; ix < 4; ++ix) {
		if (swz.indices[ix] != -1)   /* i.e. one that is not -1, meaning don't-care */
		{
			rep = swz.indices[ix];     /* this value will replace -1 */
		}
	}

	for (ix = 3; ix >= 0; --ix)
	{
		res = (res << 2) | ((swz.indices[ix] != -1)? swz.indices[ix] : rep);
	}

	return res;
}


/** Output mask as (4 low bits of) an int */
static int mask_as_4(swizzle_pattern swz)
{
	int ix;
	int res = 0;

	for (ix = 3; ix >= 0; --ix)
	{
		res = (res << 1) | ((swz.indices[ix] != -1) & 1);
	}

	return res;
}


/** If not a scalar, return -1, else return the single swizzle component:x|y|z|æ */
static int is_sub_reg_in(m200_input_argument* arg)
{
	int ix;
	int elm = -1;
	int cnt = 0;

	for (ix = 0; ix < 4; ++ix)
	{
		int swz = arg->swizzle.indices[ix];
		if (swz >= 0)
		{
			++cnt;
			elm = swz;
		}
	}

	return (cnt == 1)? elm : -1;
}


/** If not a scalar, return -1, else return the single swizzle component:x|y|z|æ */
static int is_sub_reg_out(m200_instruction* ins)
{
	int ix;
	int elm = -1;
	int cnt = 0;

	for (ix = 0; ix < 4; ++ix)
	{
		if (ins->output_swizzle.indices[ix] != -1)
		{
			elm = ix;
			++cnt;
		}
	}

	return (cnt == 1)? elm : -1;
}


/** Set up input sub-reg (scalar) field, 6 bits; with swizzle component: 
 *  if (scale==0) then  x|y|z|w  elif (scale==1) then xy else xyzw
 */
static int in_sub_reg(m200_input_argument* arg, int scale) /* arg, ctx are nonzero */
{
	int swz;

	switch (scale)
	{
	case 0:
		swz = is_sub_reg_in(arg);
		assert(swz != -1 && " in_sub_reg, arg is not scalar");
		return (arg->reg_index << 2) + swz;
	case 1:
		assert((arg->swizzle.indices[0] == 0 || arg->swizzle.indices[0] == -1) &&
			   (arg->swizzle.indices[1] == 1 || arg->swizzle.indices[1] == -1) &&
			   arg->swizzle.indices[2] == -1 &&
			   arg->swizzle.indices[3] == -1);
		return arg->reg_index << 2;
	case 2:
		return arg->reg_index << 2;
	default:
		assert(0);
		return -1;
	}
}


/** Set up a result/output sub-reg (scalar) field, 6 bits */
static memerr out_sub_reg(m200_instruction* ins)
{
	int ix;
	int elm = -1;

	assert(ins);

	for (ix = 0; ix < 4; ++ix) /* swizzle part */
	{
		if (ins->output_swizzle.indices[ix] != -1)
		{
			assert(elm == -1 && " out_sub_reg, result is not scalar"); /* had one already */
			elm = ix;
		}
	}

	if (elm == -1)                     /* must have one reg except when implicit/direct */
	{
		elm = 0;                       /* must have something in swizzle part */

		assert(ins->out_reg == M200_R_IMPLICIT && " out_sub_reg, inconsistent mask+register");
	}

	return (ins->out_reg << 2) + elm;
}


/** Scale the size, same encoding for varying, load and store. */
static int scale_size(int size)
{
	switch (size)
	{
	case 1:
		return 0;
	case 2:
		return 1;
	case 4:
		return 2;
	default:
		assert(0 && " scale_size, bad address multiplier\n");
		return 0;
	}
}


/** Emit a VAR instruction. */
static return_code emit_varying(mali200_emit_context* ctx, m200_instruction* ins) /* ins && ctx  are certainly OK */
{
	int op_code;
	int siz = 0;
	int b23_16 = 0;                    /* bits 23:16,  in_reg swizzle OR varying ix off */
	int b7_8 = 0;                      /* bits 7:8,  normalize's wMode OR flatshade/centroid */
	essl_bool var_eva = ESSL_FALSE;

	assert(ins);

	switch (ins->opcode)
	{
	case M200_VAR:
		op_code = 0;
		var_eva = ESSL_TRUE;
		break;
	case M200_VAR_DIV_Y:
		op_code = 1;
		var_eva = ESSL_TRUE;
		break;
	case M200_VAR_DIV_Z:
		op_code = 2;
		var_eva = ESSL_TRUE;
		break;
	case M200_VAR_DIV_W:
		op_code = 3;
		var_eva = ESSL_TRUE;
		break;
	case M200_MOV:                     /* only args[0].  Same code used for various units */
		op_code = 4;
		break;
	case M200_VAR_CUBE:
		op_code = 8;
		var_eva = ESSL_TRUE;
		break;
	case M200_MOV_DIV_Y:
		op_code = 5;
		break;
	case M200_MOV_DIV_Z:
		op_code = 6;
		break;
	case M200_MOV_DIV_W:
		op_code = 7;
		break;
	case M200_MOV_CUBE:
		op_code = 9;
		break;
	case M200_POS:
		op_code = 11;
		break;
	case M200_POINT:
		op_code = 12;
		break;
	case M200_MISC_VAL:
		op_code = 13;
		break;
	case M200_NORM3:
		op_code = 10;
		b7_8 = (ins->opcode_flags & M200_NORM_SQR)? 0 
			: (ins->opcode_flags & M200_NORM_NORM)? 1 
			: (ins->opcode_flags & M200_NORM_RCP)? 2 
			: (ins->opcode_flags & M200_NORM_RSQR)? 3 
			: 1;
		break;
	default:
		assert(0 && " emit_varying, illegal opcode");
		return RET_ERROR;
	}

	if (ins->args[0].reg_index >= 0)
	{
		b23_16 = swizz_as_8(ins->args[0].swizzle);
	}

	if (var_eva)                     /* Reads varying data values */
	{
		siz = ins->address_multiplier - 1;
		b7_8 = ((ins->opcode_flags & M200_VAR_FLATSHADING)? 1 : 0) 
			+ ((ins->opcode_flags & M200_VAR_CENTROID)? 2 : 0);
		/* use bits 23:18 for the offset, retain the X swizzle in the two lowest bits */
		b23_16 = (ins->address_offset/ins->address_multiplier << 2) | (b23_16 & 0x3);
	}

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 5, op_code));  /* 0:4 - opCode */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 2, siz));      /* 5:6 - size */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 2, b7_8));     /* 7:8 */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, 0));        /* 9; Perspective divide disable NOT IMPL in HW */

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 4, ins->args[0].reg_index)); /* input reg, varying use  .x */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, ins->args[0].negate));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, ins->args[0].absolute_value));

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 8, b23_16));   /* 16:23 */

	if (ins->out_reg == M200_R_IMPLICIT)
	{
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 8, 0xff)); /* 24:31 - outReg, wrMsk*/
	}
	else
	{
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 4, ins->out_reg));
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 4, mask_as_4(ins->output_swizzle)));
	}

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 2, ins->output_mode)); /* 32:33 - out trSat */

	return RET_OK;
}




        /** Emit a TEX instruction */
static return_code emit_texture(mali200_emit_context* ctx, m200_instruction* ins)
{
	essl_bool enabl_lod_reg = (ins->args[1].reg_index != M200_REG_UNKNOWN)? 1 : 0;
	int lod_select = (ins->opcode_flags & M200_TEX_COO_W)? 2 : (enabl_lod_reg)? 1 : 0;
	essl_bool enabl_samp_reg = (ins->args[0].reg_index != M200_REG_UNKNOWN)? 1 : 0; /* ensure essl_bool */
	assert(ins->opcode == M200_TEX);
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 6, (enabl_lod_reg)?  in_sub_reg(&ins->args[1], 0) : 0)); /* LOD */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 6, (enabl_samp_reg)? in_sub_reg(&ins->args[0], 0) : 0)); /* smpl */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 5, 0));               /* opcode, not impl in hw */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, (ins->opcode_flags & M200_TEX_LOD_REPLACE) != 0));               /* LOD mode, 0: bias, 1:replace-NB:rendezvous */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 2, lod_select));      /* LOD sub-reg select, 0:none, 1:LOD, 2:cooRegW */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 9, (int)(ins->lod_offset*16.0))); /* LOD offset (5:4 signed fixpoint) */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, enabl_samp_reg));  /* use sampler reg */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 12, ins->address_offset/ins->address_multiplier)); /* sampler index/offset */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, (ins->opcode_flags & M200_TEX_NO_RESET_SUMMATION)? 0 : 1)); /* reset texture summation */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, (ins->opcode_flags & M200_TEX_IGNORE)? 1 : 0));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 8, 0));               /* reserved */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 8, swizz_as_8(ins->output_swizzle))); /* texture sampler swizzle */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 2, ins->output_mode)); /* result saturation mode */
	return(ins->opcode_flags & M200_TEX_LOD_REPLACE) ? RET_OK : RET_RENDEZ;
}


#ifndef NDEBUG
static essl_bool check_no_input(m200_input_argument* arg) 
{
	return (arg->reg_index == M200_REG_UNKNOWN);
}
#endif


/** Emit a scalar input_arg. for mul/add: sub_reg, abs & neg  8 bits. */
static memerr emit_input1_arith(mali200_emit_context* ctx, m200_input_argument* arg) /* ctx is nonzero */
{
	assert(arg);

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 6, in_sub_reg(arg, 0)));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, arg->absolute_value));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, arg->negate));

	return MEM_OK;
}


/** Emit a vec4 input_arg. for mul/add: reg, swizz, abs & neg  14 bits. */
static memerr emit_input4_arith(mali200_emit_context* ctx, m200_input_argument* argh) /* ctx is nonzero */
{
	assert(argh);

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 4, argh->reg_index));                 /* register index */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 8, swizz_as_8(argh->swizzle)));       /* swizzle pattern */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, argh->absolute_value));            /* absolute value bit */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, argh->negate));                    /* negate bit */

	return MEM_OK;
}


/** Emit a scalar output arg. for mul/add: sub-reg, write-mask & sat-mode  9 bits. */
static memerr emit_result1_arith(mali200_emit_context* ctx, m200_instruction* ins) /* both params are nonzero */
{
	if (ins->out_reg == M200_R_IMPLICIT)
	{
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 7, 0));
	}
	else
	{
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 6, out_sub_reg(ins)));       /* register */
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, 1));                           /* write mask */
	}

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 2, ins->output_mode));                /* saturate mode */

	return MEM_OK;
}


/** Emit a vec4 output arg. for mul/add: reg, write-mask, sat_mode  10 bits. */
static memerr emit_result4_arith(mali200_emit_context* ctx, m200_instruction* ins) /* both params are nonzero */
{
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 4, ins->out_reg));     /* register index */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 4, (ins->out_reg == M200_R_IMPLICIT)? 0 : mask_as_4(ins->output_swizzle)));    /* to avoid writing to R0 : write mask*/
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 2, ins->output_mode));             /* saturate mode */

	return MEM_OK;
}


/** Return the opcode for a mul4 or mul1 operation, -1 for error.  
 *  Also modify the  m200_instruction  if needed to be able to emit it.
 *  Composite operations should not occur here, only their parts.
 */
static int opcode_of_mult(m200_instruction* ins) /* ins, ctx are nonzero  */
{
	switch (ins->opcode)
	{
	case M200_MUL:
		return 0;
	case M200_MUL_X2:
		return 1;
	case M200_MUL_X4:
		return 2;
	case M200_MUL_X8:
		return 3;
	case M200_MUL_D2:
		return 7;
	case M200_MUL_D4:
		return 6;
	case M200_MUL_D8:
		return 5;
	case M200_MUL_W1:
		return 4;
	case M200_NOT:                     /* only args[0] */
		assert(check_no_input(&ins->args[1]));
		return 8;
	case M200_AND:
		return 9;
	case M200_OR:
		return 10;
	case M200_XOR:
		return 11;
	case M200_CMP:
		switch (ins->compare_function)
		{
		case M200_CMP_EQ:
			return 15;
		case M200_CMP_GT:
			return 13;
		case M200_CMP_NE:
			return 12;
		case M200_CMP_GE:
			return 14;
		default:
			assert(0 && " opcode_of_mult, illegal compare code ");
			return 15;
		}
	case M200_MIN:
		return 16;
	case M200_MAX:
		return 17;
	case M200_CONS31:
		return 28;
	case M200_CONS22:
		return 29;
	case M200_CONS13:
		return 30;
	case M200_MOV:                     /* only args[0].  Same code used for various units */
		assert(check_no_input(&ins->args[1]));
		return 31;
	default:                           /* fall through */
		assert(0 && " illegal opcode_of_mult ");
		return -1;
	}
}


/** Return the opcode for a mul4 or mul1 operation, -1 for error.  
 *  Also modify the  m200_instruction  if needed to be able to emit it.
 *  Composite operations should not occur here, only their parts. 
 *  scalar is the same as not-add4.
 */
static int opcode_of_add(m200_instruction* ins, essl_bool scalar) /* ins, ctx: nonzero  */
{
	switch (ins->opcode) {
	case M200_ADD:
		return 0;
    case M200_ADD_X2:
		return 1;
    case M200_ADD_X4:
		return 2;
    case M200_ADD_X8:
		return 3;
    case M200_ADD_D2:
		return 7;
    case M200_ADD_D4:
		return 6;
    case M200_ADD_D8:
		return 5;
    case M200_HADD3:                   /* only args[0] */
		if (scalar)
		{
			goto ERRMESS;
		}
		assert(check_no_input(&ins->args[1]));
		return 16;
    case M200_HADD4:                   /* only args[0] */
		if (scalar)
		{
			goto ERRMESS;
		}
		assert(check_no_input(&ins->args[1]));
		return 17;
    case M200_2X2ADD:   
		if (scalar)
		{
			goto ERRMESS;
		}
		return 18;
	case M200_CMP:
		switch (ins->compare_function) {
		case M200_CMP_EQ:
			return 11;
		case M200_CMP_GT:
			return 9;
		case M200_CMP_NE:
			return 8;
		case M200_CMP_GE:
			return 10;
		default:
			assert(0 && " illegal opcode_of_add");
			return -1;
		}
	case M200_MIN:
		return 14;
	case M200_MAX:
		return 15;
	case M200_SEL:
		return 23;
	case M200_VSEL:
		assert(!scalar);
		return 19;
	case M200_DERX:                    /* only args[0] */
		if (ins->args[0].reg_index == M200_R_IMPLICIT)
		{
			goto ERRMESS;
		}
		ins->args[1] = ins->args[0];
		ins->args[1].negate = !ins->args[0].negate;
		return 20;
	case M200_DERY:                    /* only args[0] */
		if (ins->args[0].reg_index == M200_R_IMPLICIT)
		{
			goto ERRMESS;
		}
		ins->args[1] = ins->args[0];
		ins->args[1].negate = !ins->args[0].negate;
		return 21;
	case M200_FRAC:                    /* only args[0] */
		assert(check_no_input(&ins->args[1]));
		return 4;
	case M200_FLOOR:                   /* only args[0] */
		assert(check_no_input(&ins->args[1]));
		return 12;
	case M200_CEIL:                    /* only args[0] */
		assert(check_no_input(&ins->args[1]));
		return 13;
	case M200_SWZ:
		if (scalar)
		{
			goto ERRMESS;
		}
		return 22;
    case M200_CONS31:
		if (scalar)
		{
			goto ERRMESS;
		}
		return 28;
    case M200_CONS22:
		if (scalar)
		{
			goto ERRMESS;
		}
		return 29;
    case M200_CONS13:
		if (scalar)
		{
			goto ERRMESS;
		}
		return 30;
	case M200_MOV:                     /* same code used for various units */
		assert(check_no_input(&ins->args[1]));
		return 31;
	default:                           /* fall through */

	ERRMESS:
		assert(0 && " illegal opcode_of_add");
		return -1;
	}
}                                      /* end opcode_of_add */


/** Emit the extended swizzle special source operand, 14 + 14 bits */
static memerr emit_ext_swz(mali200_emit_context* ctx, m200_input_argument* arg)  /* params are nonzero */
{
	int ix;

	assert(arg->reg_index >= 0);

	ix = (arg->absolute_value? 4096 : 0) + (arg->negate? 8192 : 0);
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 14, arg->reg_index + ix));  /* i.e.  emit_input4_arith  without swz */

	for (ix = 0; ix < 4; ++ix)
	{
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 3, ESSL_MAX(0, arg->swizzle.indices[ix])));
	}

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 2, 0));

	return MEM_OK;
}


/** Emit an instruction for the MUL4 unit, 43 bits. */
static return_code emit_mul4(mali200_emit_context* ctx, m200_instruction* ins) /* both params are nonzero */
{
	int op_code = opcode_of_mult(ins);

	ESSL_CHECK(op_code != -1);
	ESSL_CHECK(emit_input4_arith(ctx, &ins->args[0]));

	if (ins->args[1].reg_index != M200_REG_UNKNOWN)
	{
		ESSL_CHECK(emit_input4_arith(ctx, &ins->args[1]));
	}
	else
	{
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 14, 0));
	}

	ESSL_CHECK(emit_result4_arith(ctx, ins));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 5, op_code));

	return RET_OK;
}


/** Emit an instruction for the MUL1 unit, 30 bits. */
static return_code emit_mul1(mali200_emit_context* ctx, m200_instruction* ins) /* both params are nonzero */
{
	int op_code = opcode_of_mult(ins);

	assert(op_code != -1);
	ESSL_CHECK(emit_input1_arith(ctx, &ins->args[0]));     /* scalar input 1 */

	if (ins->args[1].reg_index != M200_REG_UNKNOWN)
	{
		ESSL_CHECK(emit_input1_arith(ctx, &ins->args[1])); /* scalar input 2 */
	}
	else
	{
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 8, 0));
	}

	ESSL_CHECK(emit_result1_arith(ctx, ins));              /* scalar result */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 5, op_code));

	return RET_OK;
}


/** Emit an instruction for the ADD4 unit, 44 bits. */
static return_code emit_add4(mali200_emit_context* ctx, m200_instruction* ins) /* both params are nonzero */
{
	int op_code = opcode_of_add(ins, ESSL_FALSE);

	ESSL_CHECK(op_code != -1);

	if (ins->opcode == M200_SWZ)
	{
		ESSL_CHECK(emit_ext_swz(ctx, &ins->args[0]));      /* reg, abs, neg & extended swz, 28 bit */
	}
	else
	{
		ESSL_CHECK(emit_input4_arith(ctx, &ins->args[0]));        /* vec4 input 0, 14 bit */

		if (ins->args[1].reg_index != M200_REG_UNKNOWN)
		{
			ESSL_CHECK(emit_input4_arith(ctx, &ins->args[1]));    /* vec4 input 1, 14 bit */
		}
		else
		{
			ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 14, 0));
		}
	}

	ESSL_CHECK(emit_result4_arith(ctx, ins));                 /* vec4 result */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 5, op_code));                    /* adder operation */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, (ins->args[0].reg_index == M200_R_IMPLICIT)? 1 : 0)); /* override */

	assert(ins->args[1].reg_index != M200_R_IMPLICIT && " emit_add4, arg[1] is UNDERHAND");

	return (ins->opcode == M200_DERX || ins->opcode == M200_DERY)? RET_RENDEZ : RET_OK;
}


/** Emit an instruction for the ADD1 unit, 31 bits. */
static return_code emit_add1(mali200_emit_context* ctx, m200_instruction* ins) /* both params are nonzero */
{
	int op_code = opcode_of_add(ins, ESSL_TRUE);

	ESSL_CHECK(op_code != -1);
	ESSL_CHECK(emit_input1_arith(ctx, &ins->args[0]));

	if (ins->args[1].reg_index != M200_REG_UNKNOWN)
	{
		ESSL_CHECK(emit_input1_arith(ctx, &ins->args[1]));
	}
	else
	{
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 8, 0));
	}

	ESSL_CHECK(emit_result1_arith(ctx, ins));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 5, op_code));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, (ins->args[0].reg_index == M200_R_IMPLICIT)? 1 : 0));

	assert(ins->args[1].reg_index != M200_R_IMPLICIT && " emit_add1, arg[1] is UNDERHAND");

	return (ins->opcode == M200_DERX || ins->opcode == M200_DERY)? RET_RENDEZ : RET_OK;
}


/** Emit a scalar input_arg. for LUT: sub_reg, abs & neg  8 bits. */
static memerr emit_input1_lut(mali200_emit_context* ctx, m200_input_argument* arg) /* ctx is nonzero */
{
	assert(arg);

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, arg->absolute_value));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, arg->negate));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 6, in_sub_reg(arg, 0)));

	return MEM_OK;
}


/** Emit a vec4 input_arg. for LUT: swizz & reg  12 bits. */
static memerr emit_input4_lut(mali200_emit_context* ctx, m200_input_argument* argh) /* ctx is nonzero */
{
	assert(argh);

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 8, swizz_as_8(argh->swizzle)));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 4, argh->reg_index));

	return MEM_OK;
}


/** Emit a scalar output arg. for LUT: sat-mode, sub-reg  8 bits. */
static memerr emit_result1_lut(mali200_emit_context* ctx, m200_instruction* ins) /* both params are nonzero */
{
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 2, ins->output_mode));           /* saturate mode */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 6, out_sub_reg(ins)));      /* register */

	return MEM_OK;
}


/** Emit a vec4 output arg. for LUT: write-mask, reg  8 bits. */
static memerr emit_result4_lut(mali200_emit_context* ctx, m200_instruction* ins) /* both params are nonzero */
{
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 4, mask_as_4(ins->output_swizzle)));/* write mask */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 4, ins->out_reg));               /* register */

	return MEM_OK;
}


/** If this arg can be handled as a scalar in the LUT multiply vector-scalar op, 
 *  return the input_scalar_reg_specifier (LUT form) for that, else -1
 */
static int scalar_lut_mul(m200_input_argument* arg)
{
	int elm = -1;

	if (arg->swizzle.indices[0] != -1)
	{
		elm = arg->swizzle.indices[0];
	}

	if (arg->swizzle.indices[1] != -1)
	{
		if (elm == -1)
		{
			elm = arg->swizzle.indices[1];
		}
		else if (arg->swizzle.indices[1] != elm)
		{
			return -1;
		}
	}

	if (arg->swizzle.indices[2] != -1)
	{
		if (elm == -1)
		{
			elm = arg->swizzle.indices[2];
		}
		else if (arg->swizzle.indices[2] != elm)
		{
			return -1;
		}
	}

	if (arg->swizzle.indices[3] != -1)
	{
		if (elm == -1)
		{
			elm = arg->swizzle.indices[3];
		}
		else if (arg->swizzle.indices[3] != elm)
		{
			return -1;
		}
	}

	assert(elm != -1 && " in scalar_lut_mul: nothing in swizzle ");

	return (((arg->reg_index << 2) + elm) << 2) + arg->negate*2 + arg->absolute_value;
}


/** Emit an instruction for the LUT unit. */
static return_code emit_lut(mali200_emit_context* ctx, m200_instruction* ins)
{
	int op_code, mode = 0;

	switch (ins->opcode)
	{
	case M200_RCP:                     /* 1/x */
		op_code= 0;
		break;
	case M200_RCC:                     /* 1/x, but not 0 or inf */
		op_code= 1;
		break;
	case M200_RSQ:                     /* 1/sqrt(x) */
		op_code= 3;
		break;
	case M200_SQRT:                    /* sqrt(x) */
		op_code= 2;
		break;
	case M200_EXP:                     /* 2^x */
		op_code= 4;
		break;
	case M200_LOG:                     /* log2(x) */
		op_code= 5;
		break;
	case M200_SIN:                     /* sin 2*pi*x */
		op_code= 6;
		break;
	case M200_COS:                     /* cos 2*pi*x */
		op_code = 7;
		break;
	case M200_ATAN1_IT1:               /* 1st iteration of 1-argument atan */
		mode = 1;
		op_code = 8;
		break;
	case M200_ATAN2_IT1:               /* 1st iteration of 2-argument atan */ 
		mode = 1;
		op_code = 9;
		break;
	case M200_ATAN_IT2:                /* 2nd iteration of atan, mode 2 */
		assert(!ins->args[0].negate && !ins->args[0].absolute_value);

		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 2, 2));
		ESSL_CHECK(emit_input4_lut(ctx, &ins->args[0]));/* 12 bits */
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 8, 0));
		ESSL_CHECK(emit_result1_lut(ctx, ins));         /* 8 bits */

		return RET_OK;
	case M200_MUL:
		{                   /* M200_SC_SCALAR_VECTOR_MUL, mode 3 */
			m200_input_argument* scalar = &ins->args[0];/* prefer scalar */
			m200_input_argument* vector = &ins->args[1]; /* prefer vec4 */
			int scalar_spec = scalar_lut_mul(scalar);

			assert(!vector->negate && !vector->absolute_value);
			assert(ins->output_mode == M200_OUTPUT_NORMAL);
			assert(scalar_spec >= 0);                   /* to make absolutely sure */

			ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 2, 3));    /* mode 3: vec4*scalar */
			ESSL_CHECK(emit_input4_lut(ctx, vector));         /* 12 bits */
			ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 8, scalar_spec));
			ESSL_CHECK(emit_result4_lut(ctx, ins));         /* 8 bits */

			return RET_OK;
		}
	case M200_MOV:
		{
			/* Find embedded one constant */
			int emb;
			int const_subreg = -1;
			m200_input_argument* left = &ins->args[0]; /* vec4 to move*/

			for (emb = 0 ; emb < M200_N_CONSTANT_REGISTERS ; emb++)
			{
				unsigned int comp;
				for (comp = 0 ; comp < ctx->word->n_embedded_entries[emb] ; comp++)
				{
					if (ctx->word->embedded_constants[emb][comp] == 1.0f)
					{
						const_subreg = ((12+emb) << 2) | comp;
					}
				}
			}

			assert(const_subreg != -1);
			assert(!left->absolute_value && !left->negate);

			/* Write as mul */

			ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 2, 3));    /* mode 3: vec4*scalar */
			ESSL_CHECK(emit_input4_lut(ctx, left));         /* 12 bits */
			ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 8, const_subreg << 2)); /* constant */
			ESSL_CHECK(emit_result4_lut(ctx, ins));         /* 8 bits */

			return RET_OK;
		}
	default:
		assert(0 && " emit_lut, illegal opcode");
		return RET_ERROR;                      /* illegal, return error */
	} /* end switch */

	if (mode == 1)
	{
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 2, 1));    /* mode 1: 1.part of atan */
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 4, op_code));

		if (op_code == 8)
		{
			ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 8, 0));
		}
		else
		{
			ESSL_CHECK(emit_input1_lut(ctx, &ins->args[1]));/* 8 bits */
		}

		ESSL_CHECK(emit_input1_lut(ctx, &ins->args[0]));/* 8 bits */
		ESSL_CHECK(emit_result4_lut(ctx, ins));         /* 8 bits */

		return RET_OK;
	}

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 2, 0));        /* mode 0: most of the functions */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 4, op_code));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 8, 0));
	ESSL_CHECK(emit_input1_lut(ctx, &ins->args[0]));    /* 8 bits */
	ESSL_CHECK(emit_result1_lut(ctx, ins));             /* 8 bits */

	return  RET_OK;
}


static essl_bool out_is_all4(m200_instruction* ins)
{
	return ins->output_swizzle.indices[0] == 0 && ins->output_swizzle.indices[1] == 1 &&
	       ins->output_swizzle.indices[2] == 2 && ins->output_swizzle.indices[3] == 3;
}


/** Return whether input mask says 'is vec4 without reshuffling' */
static essl_bool in_is_unchanged4(m200_input_argument* arg)
{
	return arg->swizzle.indices[0] == 0 && arg->swizzle.indices[1] == 1 &&
	       arg->swizzle.indices[2] == 2 && arg->swizzle.indices[3] == 3;
}


/** Emit a LOAD instruction. */
static return_code emit_load(mali200_emit_context* ctx, m200_instruction* ins)
{
	int op_code;
	int xtra_sub = 0;                  /* bits 17:12 */
	int scale = scale_size(ins->address_multiplier);

	switch (ins->opcode)
	{
	case M200_LD_UNIFORM:              /* load from uniforms */
		op_code = 0;
		break;
	case M200_LD_STACK:                /* load from stack */
		op_code = 3;
		break;
	case M200_LD_REL:                  /* load from array (relative address) */
		op_code = 2;
		xtra_sub = in_sub_reg(&ins->args[2], 0); /* Base reg, scalar */
		break;
	case M200_MOV:                     /* from: (part of) reg given by args[0] to: #load */
		assert(in_is_unchanged4(&ins->args[0]) && ins->out_reg != M200_HASH_LOAD && out_is_all4(ins));
		/* op_code = 4, i.e. M200_LD_REG */
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 25, 4)); /* opcode, data, scale, 17:12, index & enable */
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 16, ins->args[0].reg_index)); /* displacement */

		return MEM_OK;
	case M200_LD_REG:                  /* load from a register - which given by index */
		op_code = 4;
		scale = 2;                     /* is actually ignored: always a vec4 */
		break;
	default:
		assert(0 && " emit_load, illegal opcode");
		return RET_ERROR;              /* illegal: return error */
	} /* end switch */

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 4, op_code));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 6, 0));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 2, scale));                       /* Scale */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 6, xtra_sub));

	if (ins->args[0].reg_index == -1)
	{
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 7, 0));                        /* no index reg, don't enable */
	}
	else
	{
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 6, in_sub_reg(&ins->args[0], 0)));/* Index reg */
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, 1));                        /* enable use Index reg */
	}

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 16, ins->address_offset/ins->address_multiplier)); /* Displacement */

	return RET_OK;
}



/** Emit a STORE unit instruction */
static return_code emit_store(mali200_emit_context* ctx, m200_instruction* ins)
{
	int op_code = -1;
	int data_sub = 0;                  /* instr's bits 9:4 */
	int xtra_sub = 0;                  /* instr's bits 17:12 */
	int scale = 0;
	int disp = ins->address_multiplier == 0 ? 0 : ins->address_offset/ins->address_multiplier;    /* displacement/ int constant */
	int index_sub = 0;
	int index_en = 0;

	if (ins->opcode != M200_MOV && ins->args[0].reg_index != -1)
	{
		index_sub = in_sub_reg(&ins->args[0], 0); /* index reg, scalar */
		index_en = 1;                                    /* do use index reg */
	}

	switch (ins->opcode)
	{
	case M200_ST_STACK:                /* store to stack, src2=: [stackP+scale*(stc1+displ)] */
		op_code = 3;
		scale = scale_size(ins->address_multiplier);
		assert(_essl_is_identity_swizzle(ins->args[1].swizzle) || is_sub_reg_in(&ins->args[1]) != -1);
		data_sub = in_sub_reg(&ins->args[1], scale);   /* src */
		ctx->store_emitted = ESSL_TRUE;
		break;
	case M200_ST_REL:                  /* store to array */
		op_code = 2;
		scale = scale_size(ins->address_multiplier);
		assert(_essl_is_identity_swizzle(ins->args[1].swizzle) || is_sub_reg_in(&ins->args[1]) != -1);
		data_sub = in_sub_reg(&ins->args[1], scale); /* src */
		xtra_sub = in_sub_reg(&ins->args[2], 0);       /* base reg, scalar */
		ctx->store_emitted = ESSL_TRUE;
		break;
	case M200_ST_REG:                  /* store to indexed register, only 8 bytes */
		op_code = 0;
		scale = 2;                     /* is actually ignored: always a vec4 */
		assert(_essl_is_identity_swizzle(ins->args[1].swizzle));
		data_sub = ins->args[1].reg_index << 2;        /* src, vec4 */
		break;
	case M200_MOV:                     /* from: arg[0] to: out_reg etc. of ins */
		{
			/*               data_sub,swizz  xtra_sub,swizz   displacement
			 * ST_REG    vec4   in_reg,-        -               ut_reg
			 * LD_SUBREG scal   in_reg,-       ut_reg,ut_msk    in_swz  <- not needed
			 * ST_SUBREG scal   ut_reg,-       in_reg,in_msk    ut_swz */
			if (in_is_unchanged4(&ins->args[0]) && out_is_all4(ins))
			{
				op_code = 0;                               /* M200_ST_REG */
				scale = 2;                                 /* is actually ignored: always a vec4 */
				data_sub = ins->args[0].reg_index << 2;
				disp = ins->out_reg;
			}
			else
			{
				disp = is_sub_reg_out(ins);
				assert(is_sub_reg_in(&ins->args[0]) != -1 && disp != -1);
				ESSL_CHECK(is_sub_reg_in(&ins->args[0]) != -1 && disp != -1);
				
				op_code = 1;                           /* M200_ST_SUBREG */
				scale = 0;                             /* is actually ignored: always scalar */
				data_sub = ins->out_reg << 2;
				xtra_sub = in_sub_reg(&ins->args[0], 0);
			}

			index_en = 0;
		}
		break;
	case M200_ST_SUBREG:               /* store indexed sub-register */
		op_code = 1;
		scale = 0;                                     /* is actually ignored: always scalar */
		data_sub = ins->out_reg << 2;                  /* dst, 4 potential sub_regs */
		index_sub = in_sub_reg(&ins->args[0], 0);
		index_en = 1;
		assert(ins->out_reg == ins->args[1].reg_index);
		assert(_essl_is_identity_swizzle(ins->args[1].swizzle));
		xtra_sub = in_sub_reg(&ins->args[2], 0);  /* src, may have swizz */
		break;
	case M200_LD_SUBREG:               /* load indexed sub-register */
		op_code = 5;
		scale = 0;                                     /* is actually ignored: always scalar */
		xtra_sub = out_sub_reg(ins);              /* dst, may have swizz */
		assert(_essl_is_identity_swizzle(ins->args[1].swizzle));
		data_sub = ins->args[1].reg_index << 2;        /* src, 4 potential sub_regs */
		break;
	case M200_LEA:                     /* load effective address which refers into the stack */
		op_code = 6;
		scale = scale_size(ins->address_multiplier);
		data_sub = out_sub_reg(ins);              /* dst, may have swizz */
		break;
    case M200_LD_RGB:                  /* framectxfer RGB load */
		op_code = 15;
		scale = 2;
		data_sub = ins->out_reg << 2;                  /* dst, vec4 */
		break;
    case M200_LD_ZS:                   /* framebuffer Z/stencil load */
		op_code = 14;
		scale = 2;
		data_sub = ins->out_reg << 2;                  /* dst, vec2-3-4(?) */
		break;
	default:
		assert(0 && " emit_store, illegal opcode");
		return RET_ERROR;               /* illegal, return error */
	}                                  /* end switch */

	/* No swizzles allowed on vector stores */
	assert(!(ins->opcode == M200_ST_STACK || ins->opcode == M200_ST_REL) ||
	       scale == 0 ||
	       _essl_is_identity_swizzle(ins->args[1].swizzle));

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 4, op_code));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 6, data_sub));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 2, scale));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 6, xtra_sub));

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 6, index_sub)); /* index reg, scalar */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, index_en));  /* do use index reg */

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 16, disp));    /* displacement */

	return RET_OK;
}


static return_code emit_branch(mali200_emit_context* ctx, m200_instruction* ins) /* ins and ctx are non-zero */
{
	int  op_co;
	int  b34_19 = 0;                   /* stack frame size */
	int  b40_35 = 0;
	int  b72_68 = 0;                   /* RET_OK implies do prefetch next */
	return_code ret_cod = (ins->compare_function == M200_CMP_ALWAYS)? RET_NO_PREFETCH : RET_OK;

	switch (ins->opcode)
	{
	case M200_JMP:                     /* conditional or unconditional jump */
		op_co = 0;                     /* fill at fixup: target_dist, target_siz */
		/* OPT: set no_prefetch for conditional backwards jumps? */
		break;
    case M200_JMP_REL:                 /* register-relative jump */
		op_co = 6;
		b40_35 = in_sub_reg(&ins->args[2], 0); /* target addr. register */
		break;
	case M200_CALL:                    /* function call. */
		op_co = 4;                     /* fill at fixup: frame_size, target_dist, target_siz */
		break;                         /* - to be ready for recursive funcs                  */
	case M200_CALL_REL:                /* register-relative function call */
		op_co = 7;
		b34_19 = ins->address_offset/ins->address_multiplier;
		assert(b34_19 != -1);
		b40_35 = in_sub_reg(&ins->args[2], 0); /* target addr. register */
		break;
    case M200_RET:                     /* function call return; refers #load */
		op_co = 5;
		b34_19 = ins->address_offset/ins->address_multiplier;  /* frame size of returning func */
		assert(b34_19 != -1);
		break;
	case M200_KILL:                    /* pixel kill: takes no destination argument */
		op_co = 3;
		b34_19 = 0xf;                  /* sub-pixel bitmap */
		break;
    case M200_ALT_KILL:                /* alternative pixel kill */
		{
			int ix;

			op_co = 8;
			b40_35 = ins->args[2].reg_index << 2; /* kill_register */

			for (ix = 3; ix >= 0; --ix)
			{
				assert(ins->args[2].swizzle.indices[ix] == -1 || ins->args[2].swizzle.indices[ix] == ix);
				b72_68 = (b72_68 << 1) | (ins->args[2].swizzle.indices[ix] >= 0);
			}

			ret_cod = RET_OK;
		}
		break;
    case M200_GLOB_END:                /* global_return */
		op_co = 1;
		ret_cod = RET_NO_PREFETCH;
		break;
	default:                           /* fall through */
		assert(0 && " emit_branch, illegal opcode");
		return RET_ERROR;              /* internal error */
	}

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 4, op_co));

	if (ins->compare_function > M200_CMP_NEVER && ins->compare_function < M200_CMP_ALWAYS)
	{
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 6, in_sub_reg(&ins->args[0], 0)));
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 6, in_sub_reg(&ins->args[1], 0)));
	}
	else
	{
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 12, 0));
	}

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 3, ins->compare_function));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 16, b34_19));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 6, b40_35));
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 27, 0));       /* JMP | CALL: fix up w. dist to target       */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 5, b72_68));   /* JMP | CALL: fix up w. instr size at target */

	return ret_cod;
}


/**  the mali200 floating point format is:
 *      0:9    man
 *      10:14  exp
 *      15     neg 
 * val = (exp==0)? 0.0 : (1.0+man/1024.0)*2^(exp-15);
 * return  (neg)? -val : val;
 * And then there may be plus_inf, minus_inf, not_a_number 
 * and un_normalized small values.
 */
static int to_float16(float f)
{
	union {
		float f;
		unsigned int i;
	} u;
	unsigned int f_bits;
	unsigned int fp16_sig;
	signed int xpo;
	unsigned int man;
	signed int fp16_xpo;
	unsigned int fp16_man;

	u.f = f;
	f_bits = u.i;
	fp16_sig = (f_bits >> (31-15)) & 0x8000;
	xpo = (f_bits >> 23) & 0xff;
	man = f_bits & 0x007fffff;

	fp16_xpo = xpo + (15-127);
	/* round to nearest, away from zero in case of a tie */
	fp16_man = (man + 0x1000) >> 13;
	/* mantissa carry */
	fp16_xpo += fp16_man >> 10;
	fp16_man &= 0x3ff;
	if (fp16_xpo > 30) {
		/* overflow or NaN*/
		fp16_xpo = 31;
		fp16_man = (xpo == 255 && man != 0) ? 1 : 0;
	} else if (fp16_xpo <= 0) {
		/* underflow or zero */
		fp16_xpo = 0;
		fp16_man = 0;
	}

	return fp16_sig | (fp16_xpo << 10) | fp16_man;
}


static memerr emit_emb_const(mali200_emit_context* ctx, int conCnt, float* con)
{
	int ix;

	assert(con!=0 && conCnt<=4);

	for (ix = 0; ix<4; ++ix)
	{
		int i16 = 0;
		if (ix < conCnt)  i16 = to_float16(con[ix]);
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 16, i16));
	}

	return MEM_OK;
}


static memerr emit_check(mali200_emit_context* ctx, m200_instruction* instr, emit_instr_func fun, int bit, int hdr_wrd, int expected_bits_added, essl_bool* property)
{
	if (instr && instr->opcode != M200_NOP)
	{
		int old_offset = _essl_output_buffer_get_word_position(ctx->output_buf)*32 + _essl_output_buffer_get_bit_position(ctx->output_buf);
		int new_offset;
		return_code ret;

		ret = fun(ctx, instr);
		if (ret == RET_ERROR)
		{
			return MEM_ERROR;
		}

		_essl_output_buffer_replace_bits(ctx->output_buf, hdr_wrd, bit, 1, 1);
		property[ret] = ESSL_TRUE;

		new_offset = _essl_output_buffer_get_word_position(ctx->output_buf)*32 + _essl_output_buffer_get_bit_position(ctx->output_buf);
		if (old_offset + expected_bits_added != new_offset)
		{
			assert(0);
			return MEM_ERROR;
		}
	}

	return MEM_OK;
}


/**  Emit one 'wide' instruction.  
 * First, emit header into the binary buffer.
 * For now, fill 0 into: length-of-this, end-of-prog, rendevouz, 
 *                       length-of-next, video, prefetch-hint.
 * Then emit the 'short' instructions that are present, in order, and
 * then any embedded constants. 
 *  Also note position and size of emitted code for use in fixups etc.
 */
static memerr emit_wide_instruction(mali200_emit_context* ctx, m200_instruction_word* wid)
{
	int  hdr_wrd = _essl_output_buffer_get_word_position(ctx->output_buf);
	essl_bool property [RET_MAX_ENUM] = {0}; /* set of return_code-s */

	ctx->word = wid;

	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 5, 0));        /* 0:4 (length-of-this) */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 2, (wid->end_of_program_marker)? 1 : 0));/* 5:6 (EoProg), (Rnd) */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 10, 0));       /* placeholders for instr. enabled - fill later */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, (wid->n_embedded_entries[0])? 1 : 0)); /* 17 */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 1, (wid->n_embedded_entries[1])? 1 : 0)); /* 18 */
	ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 13, 0));       /* 19:31 (length-of-next, video, prefetch-hint) */

	if (emit_check(ctx, wid->var,    emit_varying, 7, hdr_wrd, 34, property) != MEM_OK ||
	    emit_check(ctx, wid->tex,    emit_texture, 8, hdr_wrd, 62, property) != MEM_OK ||
	    emit_check(ctx, wid->load,   emit_load,    9, hdr_wrd, 41, property) != MEM_OK ||
	    emit_check(ctx, wid->mul4,   emit_mul4,   10, hdr_wrd, 43, property) != MEM_OK ||
	    emit_check(ctx, wid->mul1,   emit_mul1,   11, hdr_wrd, 30, property) != MEM_OK ||
	    emit_check(ctx, wid->add4,   emit_add4,   12, hdr_wrd, 44, property) != MEM_OK ||
	    emit_check(ctx, wid->add1,   emit_add1,   13, hdr_wrd, 31, property) != MEM_OK ||
	    emit_check(ctx, wid->lut,    emit_lut,    14, hdr_wrd, 30, property) != MEM_OK ||
	    emit_check(ctx, wid->store,  emit_store,  15, hdr_wrd, 41, property) != MEM_OK)
	{
		return MEM_ERROR;
	}
	
	wid->bran_wrd = _essl_output_buffer_get_word_position(ctx->output_buf); /* note start of conditional branch sub-instr */
	wid->bran_bit = _essl_output_buffer_get_bit_position(ctx->output_buf);
	
	if (emit_check(ctx, wid->branch, emit_branch, 16, hdr_wrd, 73, property) != MEM_OK)
	{
		return MEM_ERROR;
	}

	if (wid->n_embedded_entries[0])
	{
		ESSL_CHECK(emit_emb_const(ctx, wid->n_embedded_entries[0], wid->embedded_constants[0]));
		_essl_output_buffer_replace_bits(ctx->output_buf, hdr_wrd, 17, 1, 1);
	}

	if (wid->n_embedded_entries[1])
	{
		ESSL_CHECK(emit_emb_const(ctx, wid->n_embedded_entries[1], wid->embedded_constants[1]));
		_essl_output_buffer_replace_bits(ctx->output_buf, hdr_wrd, 18, 1, 1);
	}

	assert(!(property[RET_RENDEZ] && property[RET_NO_RENDEZ]));

	if (!property[RET_NO_PREFETCH])
	{
		_essl_output_buffer_replace_bits(ctx->output_buf, hdr_wrd, 25, 1, 1); /* do prefetch next instruction hint */
	}

	if (property[RET_RENDEZ])
	{
		_essl_output_buffer_replace_bits(ctx->output_buf, hdr_wrd, 6, 1, 1);  /* rendezvous flag */
	}

	if (_essl_output_buffer_get_bit_position(ctx->output_buf) != 0)                    /* align to 32 bit word boundary */
	{
		ESSL_CHECK(_essl_output_buffer_append_bits(ctx->output_buf, 32 - _essl_output_buffer_get_bit_position(ctx->output_buf), 0));
	}

	wid->emit_adr = hdr_wrd;                  /* note position and size of this instr */
	wid->emit_siz = _essl_output_buffer_get_word_position(ctx->output_buf) - hdr_wrd;

	return MEM_OK;
}


static memerr  emit_function(mali200_emit_context *ctx, symbol *fun)
{
	control_flow_graph * cfg = fun->control_flow_graph;
	size_t blk_nr;
	int old_instr_start = -1;

	int instr_start = _essl_output_buffer_get_word_position(ctx->output_buf); /* word_ix of start of current wide instr */

	assert(cfg != 0);
	cfg->start_address = instr_start;

	for (blk_nr = 0; blk_nr < cfg->n_blocks; ++blk_nr)
	{
		m200_instruction_word* wid;
		basic_block*  b = cfg->output_sequence[blk_nr];

		assert(b != 0);

		for (wid = (m200_instruction_word*)b->earliest_instruction_word;  wid != 0; wid = wid->successor)
		{
			int instr_length;

			assert(_essl_output_buffer_get_bit_position(ctx->output_buf) == 0);   /* is aligned on 32_bit boundary */

			ESSL_CHECK(emit_wide_instruction(ctx, wid));
			
			/* fix up instruction length */

			instr_length = _essl_output_buffer_get_word_position(ctx->output_buf) - instr_start;
			_essl_output_buffer_replace_bits(ctx->output_buf, instr_start, 0, 5, instr_length); 

			if (old_instr_start >= 0)
			{
				_essl_output_buffer_replace_bits(ctx->output_buf, old_instr_start, 19, 5, instr_length);
			}

			old_instr_start = instr_start;
			instr_start = _essl_output_buffer_get_word_position(ctx->output_buf);
		}
	}

	if (old_instr_start >= 0)
	{
		_essl_output_buffer_replace_bits(ctx->output_buf, old_instr_start, 25, 1, 0);  /* no prefetch_next in last instr */
	}

	cfg->end_address = instr_start;

	return MEM_OK;
}


static memerr fixup_jumps_calls(mali200_emit_context *ctx, symbol *fun)
{
	control_flow_graph * cfg = fun->control_flow_graph;
	size_t blk_nr;

	for (blk_nr = 0; blk_nr < cfg->n_blocks; ++blk_nr) /* fix_up jumps and functions */
	{
		m200_instruction_word * wide;
		basic_block* b = cfg->output_sequence[blk_nr];/* each block */

		for (wide = (m200_instruction_word *)b->earliest_instruction_word; wide != 0; wide = wide->successor)
		{
			m200_instruction* bra_ins = wide->branch;

			if (bra_ins != 0)                          /* has branch (usually last in block) */
			{
				basic_block* tar_block;
				m200_instruction_word* tar_wide;       /* target of jump or call */
				int wrd_x = wide->bran_wrd;
				int bit_x = wide->bran_bit + 19;        /* pos of frame_siz in branch ins */

				NORM_WRD_BIT;                          /* works on wrd_x and bit_x */

				assert(wide->emit_adr != -1);

				if (bra_ins->opcode == M200_CALL)      /* fix calls, here to handle recursion */
				{
					symbol* call_symb = bra_ins->call_target;
					control_flow_graph* call_cfg;
					assert(call_symb);
					call_cfg = call_symb->control_flow_graph;
					assert(call_cfg && call_cfg->n_blocks > 0);
					tar_block = call_cfg->output_sequence[0];
				}
				else if (bra_ins->opcode == M200_JMP)  /* fix up jump to target */
				{
					tar_block = bra_ins->jump_target;
				}
				else
				{
					continue;                          /* to next in loop */
				}

				assert(tar_block);

				while (tar_block->earliest_instruction_word == 0)
				{
					tar_block = tar_block->successors[BLOCK_DEFAULT_TARGET];
					assert(tar_block);
				}

				tar_wide = (m200_instruction_word *)tar_block->earliest_instruction_word;
				assert(tar_wide->emit_adr != -1);

				_essl_output_buffer_replace_bits(ctx->output_buf, wrd_x, bit_x, 16, bra_ins->address_offset/bra_ins->address_multiplier);
				bit_x += 22;                           /* position of jump target dist */
				NORM_WRD_BIT;                          /* works on wrd_x and bit_x */

				_essl_output_buffer_replace_bits(ctx->output_buf, wrd_x, bit_x, 27, tar_wide->emit_adr - wide->emit_adr);
				bit_x += 27;                           /* position of target instr length */
				NORM_WRD_BIT;                          /* works on wrd_x and bit_x */

				_essl_output_buffer_replace_bits(ctx->output_buf, wrd_x, bit_x, 5, tar_wide->emit_siz);
			} /* end  if has branch instruction */
		} /* end  for all instructions */
	} /* end  for all blocks */

	return MEM_OK;
}

static memerr print_store_warning(mali200_emit_context *ctx)
{
	if(ctx->store_emitted && ctx->tu->desc->options != NULL && ctx->tu->desc->options->mali200_unsafe_store_report && ctx->err_ctx != NULL)
	{
		const char *error_string = 
			"Emitted unsafe store instructions.\n"
			"            Due to Mali200 hardware issue 3558, store instructions may hang\n"
			"            the core in Mali200 hardware revisions r0p1 and r0p2. See the GX525\n"
			"            errata for more details. The compiler emits store instructions\n"
			"            whenever arrays are used or when compiling complex shaders.\n";

		if(ctx->tu->desc->options->mali200_unsafe_store_error)
		{
			REPORT_ERROR(ctx->err_ctx, ERR_UNSAFE_CODE, 0, "%s"
						 "            To turn this error into a warning and risk a hardware hang, use \n"
						 "            \"#pragma ARM_issue_3558_error(off)\"\n", error_string);
			return MEM_ERROR;
		} else {
			REPORT_WARNING(ctx->err_ctx, ERR_WARNING, 0, "%s", error_string);

		}
	}

	return MEM_OK;
}

memerr _essl_mali200_emit_function(error_context *err_ctx, output_buffer *buf, translation_unit *tu, symbol *function)
{
	mali200_emit_context context, *ctx = &context;
	
	ctx->store_emitted = ESSL_FALSE;
	ctx->output_buf = buf;
	ctx->err_ctx = err_ctx;
	ctx->tu = tu;

	ESSL_CHECK(emit_function(ctx, function));
	ESSL_CHECK(fixup_jumps_calls(ctx, function));
	ESSL_CHECK(print_store_warning(ctx));
	return MEM_OK;
}

memerr _essl_mali200_emit_translation_unit(error_context *err_ctx, output_buffer *buf, translation_unit *tu)
{
	mali200_emit_context context, *ctx = &context;
	symbol_list *sl;
	
	ctx->store_emitted = ESSL_FALSE;
	ctx->output_buf = buf;
	ctx->err_ctx = err_ctx;
	ctx->tu = tu;

	if (tu->entry_point)
	{
		ESSL_CHECK(emit_function(ctx, tu->entry_point));
	}

	for (sl = tu->functions; sl != 0; sl = sl->next) 
	{
		symbol *sym = sl->sym;
		ESSL_CHECK(sym);
		if (sym != tu->entry_point && !sym->opt.pilot.is_proactive_func) 
		{
			ESSL_CHECK(emit_function(ctx, sym));
		}

	}

	for (sl = tu->functions; sl != 0; sl = sl->next) 
	{
		symbol *sym = sl->sym;
		ESSL_CHECK(sym);
		if(!sym->opt.pilot.is_proactive_func)
		{
			ESSL_CHECK(fixup_jumps_calls(ctx, sym));
		}
	}
	ESSL_CHECK(print_store_warning(ctx));

	return MEM_OK;
}







#ifdef UNIT_TEST

static void print_buf(const char* txt, int cnt, u32* buf)
{
	int ix;

	if (txt)
	{
		printf("%s", txt);
	}

	for (ix = 0; ix < cnt; ++ix) 
	{
		if (ix % 4 == 0)
		{
			printf("\n");
		}

		printf(" %08x", buf[ix]);
	}

	printf(" ;\n");
}


static void  print_ctx(const char* txt, mali200_emit_context* ctx)
{
	if (txt)
	{
		printf("%s", txt);
	}

	if (ctx==0)
	{
		printf(" print_ctx no ctx\n");
		return;
	}

	printf("   ctx-> wrd_x:%u, bit_x:%u", 
	       (unsigned int)_essl_output_buffer_get_word_position(ctx->output_buf),
	       (unsigned int)_essl_output_buffer_get_bit_position(ctx->output_buf));
	print_buf(" ", _essl_output_buffer_get_word_position(ctx->output_buf) + (_essl_output_buffer_get_bit_position(ctx->output_buf) > 0 ? 1 : 0), _essl_output_buffer_get_raw_pointer(ctx->output_buf));
}


#ifdef DETAILED_DIFF
static u32 get_bits(int bit_x, int bit_c, u32* buf)
{
	u32 res;
	int wrd_x = bit_x/32;

	bit_x = bit_x%32;
	res = buf[wrd_x] >> bit_x;

	if (bit_x + bit_c > 32)
	{
		res = (res & ((1 << (32 - bit_x)) - 1)) + (buf[wrd_x + 1] << (32 - bit_x));
	}

	return res & ((1 << bit_c) - 1);
}


static void print_swz(const char* txt, int ma)
{
	printf("%s<%x %x %x %x>", txt, (ma & 3), ((ma >> 2) & 3), ((ma >> 4) & 3), ((ma >> 6) & 3));
}


static void print_reg_na(int ma)
{
	printf("%x", (ma & 15));

	if (ma & 16)
	{
		printf(" _neg");
	}

	if (ma & 32)
	{
		printf(" _abs");
	}
}


static void print_reg_swz_an(const char* txt, u32 ma)
{
	printf("%s%x", txt, (ma & 15));   /* register nr. */
	print_swz(" _swz", (ma >> 4) & 255);
	
	if (ma & 4096)
	{
		printf(" _abs");
	}

	if (ma & 8192)
	{
		printf(" _neg");
	}
}


static void print_subreg(const char* txt, int ma)
{
	printf("%s%x.%x", txt, (ma >> 2), (ma & 3));
}


static void print_subreg_an(const char* txt, int ma)
{
	print_subreg(txt, ma & 63);

	if (ma & 64)
	{
		printf(" _abs");
	}

	if (ma & 128)
	{
		printf(" _neg");
	}
}


static void print_an_subreg(const char* txt, int ma)
{
	if (ma & 1)
	{
		printf(" abs_");
	}

	if (ma & 2)
	{
		printf(" neg_");
	}

	print_subreg(txt, ma >> 2);
}


static void print_trSat(const char* txt, int ma)
{
	switch (ma)
	{
	case 0:  
		printf("%s_no", txt); 
	    return;
	case 1:  
		printf("%s_sat", txt); 
	    return;
	case 2:  
		printf("%s_pos ", txt); 
	    return;
	case 3:  
		printf("%s_trnc", txt); 
	    return;
	}

	printf("%sBAD trSat", txt);
}


static void print_cmp(int ma)
{
	printf(" cmp:");
	
	if (ma & 1)
	{
		printf("<");
	}

	if (ma & 2)
	{
		printf("=");
	}

	if (ma & 4)
	{
		printf(">");
	}
}


static void print_1const(u32 ma)
{
	printf(" (%x *1. %x)", ma >> 10, ma & 1023);
}


/* return bit_x for the following */
static int print_varying(int bit_x, u32* buf)
{
	int op_co = get_bits(bit_x, 5, buf);
	int b7_8 = get_bits(bit_x + 7, 2, buf);

	printf("\nVAR  opC:%d  sz:%d ", op_co, get_bits(bit_x + 5, 2, buf));

	if (op_co > 3 && op_co != 8)
	{
		printf(" norm_w:%x", b7_8);
	}
	else
	{
		if (b7_8 & 1)
		{
			printf(" flat");
		}

		if (b7_8 & 2)
		{
			printf(" cntr");
		}
	}

	printf("  (0:%x)  in-R:", get_bits(bit_x + 9, 1, buf));
	print_reg_na(get_bits(bit_x + 10, 6, buf));

	if (op_co > 3 && op_co != 8)
	{
		print_swz(" .swz", get_bits(bit_x + 16, 8, buf));
	}
	else
	{
		printf("  off:%d", get_bits(bit_x + 18, 6, buf));
	}

	printf("  out R:%x .wrM:%x", get_bits(bit_x + 24, 4, buf), get_bits(bit_x + 28, 4, buf));
	print_trSat(" _tS:", get_bits(bit_x + 32, 2, buf));

	return bit_x + 34;
}


static int print_tex(int bit_x, u32* buf)
{
	print_subreg("\nTEX  lod-R:", get_bits(bit_x, 6, buf));
	print_subreg("  sam-R:", get_bits(bit_x + 6, 6, buf));
	printf("  (0:%x)  lodMo:%x lodEn:%x lodOf:%x  samEn:%x samOf:%x", 
	       get_bits(bit_x + 12, 5, buf), get_bits(bit_x + 17, 1, buf), 
	       get_bits(bit_x + 18, 2, buf), get_bits(bit_x + 20, 9, buf), 
	       get_bits(bit_x + 29, 1, buf), get_bits(bit_x + 30, 12, buf));
	printf("  reset:%x ign:%x  (0:%x)  sam-", 
	       get_bits(bit_x + 42, 1, buf), get_bits(bit_x + 43, 1, buf), 
	       get_bits(bit_x + 44, 8, buf));
	print_swz("  samp-swz", get_bits(bit_x + 52, 8, buf));
	print_trSat(" _tS:", get_bits(bit_x + 60, 2, buf));

	return bit_x + 62;
}


static int print_mul4(const char* txt, int bit_x, u32* buf) /* "\nMUL4 in1" */
{
	int opC = get_bits(bit_x + 38, 5, buf);

	if (opC == 22)  /* ext SWZ: add4 only - not used for mult. */
	{
		printf("%s%x (0:%x)", txt, get_bits(bit_x, 4, buf), get_bits(bit_x + 4, 8, buf));

		if (get_bits(bit_x + 12, 1, buf) & 16)
		{
			printf(" _neg");
		}

		if (get_bits(bit_x + 13, 1, buf) & 32)
		{
			printf(" _abs");
		}

		printf(" _swz[%x %x %x %x]  (0:%x)", 
		       get_bits(bit_x + 14, 3, buf), get_bits(bit_x + 17, 3, buf), 
		       get_bits(bit_x + 20, 3, buf), get_bits(bit_x + 23, 3, buf), 
		       get_bits(bit_x + 26, 2, buf));
	}
	else
	{
		print_reg_swz_an(txt, get_bits(bit_x, 14, buf));
		print_reg_swz_an("  in2-R:", get_bits(bit_x + 14, 14, buf));
	}

	printf("  out-R:%x _wrM:%x", 
	       get_bits(bit_x + 28, 4, buf), get_bits(bit_x + 32, 4, buf));
	print_trSat(" _tS:", get_bits(bit_x + 36, 2, buf));
	printf(" opC:%d", opC);

	return bit_x + 43;
}


static int print_add4(int bit_x, u32* buf)
{
	print_mul4("\nADD4  in1-R:", bit_x, buf);
	printf("  overR:%x", get_bits(bit_x + 43, 1, buf));

	return bit_x + 44;
}


static int print_mul1(const char* txt, int bit_x, u32* buf)  /* "\nMUL1 in1-R:" */
{
	print_subreg_an(txt, get_bits(bit_x, 8, buf));
	print_subreg_an("  in2-R:", get_bits(bit_x + 8, 8, buf));
	print_subreg("  out-R:", get_bits(bit_x + 16, 6, buf));
	printf(" _wrtNbl:%x", get_bits(bit_x + 22, 1, buf));
	print_trSat(" _tS:", get_bits(bit_x + 23, 2, buf));
	printf("  opC:%d", get_bits(bit_x + 25, 5, buf));

	return bit_x + 30;
}


static int print_add1(int bit_x, u32* buf)
{
	print_mul1("\nADD1  in1-R:", bit_x, buf);
	printf("  overR:%x", get_bits(bit_x + 30, 1, buf));

	return bit_x + 31;
}


static int print_lut(int bit_x, u32* buf)
{
	int mode = get_bits(bit_x, 2, buf);

	printf("\nLUT  mode:%x", mode);

	if (mode == 0)
	{
		printf("  opC:%d  (0:%x) ", get_bits(bit_x + 2, 4, buf), get_bits(bit_x + 6, 8, buf));
		print_an_subreg(" in-R:", get_bits(bit_x + 14, 8, buf));
		print_trSat("  out _tS:", get_bits(bit_x + 22, 2, buf));
		print_subreg("  R:", get_bits(bit_x + 24, 6, buf));
	}
	else if (mode == 1)
	{
		printf("  opC:%d ", get_bits(bit_x + 2, 4, buf));
		print_an_subreg(" in2-R:", get_bits(bit_x + 6, 8, buf));
		print_an_subreg("  in1-R:", get_bits(bit_x + 14, 8, buf));
		printf("  out wM:%x R:%x", get_bits(bit_x + 22, 4, buf), get_bits(bit_x + 26, 4, buf));
	}
	else if (mode == 2)
	{
		print_swz("  in-swz", get_bits(bit_x + 2, 8, buf));
		printf("  R:%x  (0:%x)", get_bits(bit_x + 10, 4, buf), get_bits(bit_x + 14, 8, buf));
		print_trSat("  out _tS:", get_bits(bit_x + 22, 2, buf));
		print_subreg("  R:", get_bits(bit_x + 24, 6, buf));
	}
	else /* mode == 3 */
	{
		print_swz("  in1- .swz", get_bits(bit_x + 2, 8, buf));
		printf(" R:%x ", get_bits(bit_x + 10, 4, buf));
		print_an_subreg(" in2-R:", get_bits(bit_x + 14, 8, buf));
		printf("  out wM:%x R:%x", get_bits(bit_x + 22, 4, buf), get_bits(bit_x + 26, 4, buf));
	}

	return bit_x +30;
}


static int print_lo_st(essl_bool store, int bit_x, u32* buf)
{
	int opC = get_bits(bit_x, 4, buf);

	if (store)
	{
		printf("\nSTORE  opC:%d", opC);
		print_subreg("  data R:", get_bits(bit_x + 4, 6, buf));
	}
	else
	{
		printf("\nLOAD  opC:%x  (0:%x)", opC, get_bits(bit_x + 4, 6, buf));
	}

	printf("  scale:%x", get_bits(bit_x + 10, 2, buf));

	if (opC == 2)
	{
		print_subreg("  base-R:", get_bits(bit_x + 12, 6, buf));
	}
	else if (opC == 1 && store)
	{
		print_subreg("  src-R:", get_bits(bit_x + 12, 6, buf));
	}
	else if (opC == 5 && store)
	{
		print_subreg("  dst-R:", get_bits(bit_x + 12, 6, buf));
	}
	else
	{
		printf(" (0:%x)", get_bits(bit_x + 12, 6, buf));
	}

	print_subreg("  index-R:", get_bits(bit_x + 18, 6, buf));
	printf("  _nabl:%x  displ:%d", get_bits(bit_x + 24, 1, buf), get_bits(bit_x + 25, 16, buf));

	return bit_x + 41;
}


static int print_c_br(int bit_x, u32* buf)
{
	int opC = get_bits(bit_x, 4, buf);

	printf("\nC-BR  opC:%d", opC);
	print_subreg("  left-R:", get_bits(bit_x + 4, 6, buf));
	print_subreg("  right-R:", get_bits(bit_x + 10, 6, buf));
	print_cmp(get_bits(bit_x + 16, 3, buf));
	printf((opC == 3 || opC == 8)? "  pixMap:%x" 
	       : (opC == 4 || opC == 5 || opC == 7)? "  frame:%x" 
	       : "  (0:%x)", get_bits(bit_x + 19, 16, buf));
	print_subreg((opC == 6 || opC == 7)? " jmp to R:" 
	             : (opC == 8)? " kill R:" 
	             : " (0:", get_bits(bit_x + 35, 6, buf));

	if (opC == 8)
	{
		printf(")");
	}

	printf("  jmp dst:%d", get_bits(bit_x + 41, 27, buf));
	printf((opC == 8)? "  kill bitM:%x" : "  dstI:%d", get_bits(bit_x + 68, 5, buf));

	return bit_x + 73;
}


static int print_4const(const char* txt, int bit_x, u32* buf)
{
	printf(txt);
	print_1const(get_bits(bit_x, 16, buf));
	print_1const(get_bits(bit_x + 16, 16, buf));
	print_1const(get_bits(bit_x + 32, 16, buf));
	print_1const(get_bits(bit_x + 48, 16, buf));

	return bit_x + 64;
}


        /* Print a wide instruction.  First call: starting at buf[0], bit_x should be 0.
		 */
static int print_instr_word(int bit_x, u32* buf)
{
	u32 enable = get_bits(bit_x + 7, 12, buf);

	printf("\n     leng:%d  end:%x  rndz:%x  enab:%x  Nxt -lng:%d  (0:%x) -fetch:%x (0:%x)", 
	       get_bits(bit_x, 5, buf), get_bits(bit_x + 5, 1, buf), 
	       get_bits(bit_x + 6, 1, buf), enable, 
	       get_bits(bit_x + 19, 5, buf), get_bits(bit_x + 24, 1, buf), 
	       get_bits(bit_x + 25, 1, buf), get_bits(bit_x + 26, 6, buf));

	bit_x += 32;

	if (enable & 1)     bit_x = print_varying(bit_x, buf);
	if (enable & 2)     bit_x = print_tex(bit_x, buf);
	if (enable & 4)     bit_x = print_lo_st(ESSL_FALSE, bit_x, buf);
	if (enable & 8)     bit_x = print_mul4("\nMUL4  in1-R:", bit_x, buf);
	if (enable & 16)    bit_x = print_mul1("\nMUL1  in1-R:", bit_x, buf);
	if (enable & 32)    bit_x = print_add4(bit_x, buf);
	if (enable & 64)    bit_x = print_add1(bit_x, buf);
	if (enable & 128)   bit_x = print_lut(bit_x, buf);
	if (enable & 256)   bit_x = print_lo_st(ESSL_TRUE, bit_x, buf);
	if (enable & 512)   bit_x = print_c_br(bit_x, buf);
	if (enable & 1024)  bit_x = print_4const("\nCONST-0  ", bit_x, buf);
	if (enable & 2048)  bit_x = print_4const("\nCONST-1  ", bit_x, buf);

	printf("\n\n");

	return bit_x;
}

#endif /* DETAILED_DIFF */


#define CPY_CHR(chr)  { if (dst >= dst_end) return 0;  *(dst++) = chr; }



/** Initializes a mali200_emit_context instance */
static memerr emit_context_init(mali200_emit_context* ctx, output_buffer* output_buf, error_context* err_ctx, mempool *pool)
{
	ESSL_CHECK(_essl_output_buffer_init(output_buf, pool));

	ctx->output_buf = output_buf;
	ctx->err_ctx = err_ctx;
	ctx->store_emitted = ESSL_FALSE;

	return MEM_OK;
}


/** Reset an old emit200_context instance, for a new round of code generation. */
static memerr emit_context_reset(mali200_emit_context* ctx, mempool *pool)
{
	ESSL_CHECK(_essl_output_buffer_init(ctx->output_buf, pool));

	return MEM_OK;
}


static void fill_input_arg(m200_input_argument* arg, int reg, swizzle_pattern swz, essl_bool abs, essl_bool neg)
{
	arg->reg_index = reg;
	arg->swizzle = swz;
	arg->absolute_value = abs;
	arg->negate = neg;
}


static m200_instruction* make_instr(mempool* pool, m200_opcode op_code, unsigned op_msk,
                                    int in0reg, swizzle_pattern in0swz, essl_bool in0abs, essl_bool in0neg,  
                                    int in1reg, swizzle_pattern in1swz, essl_bool in1abs, essl_bool in1neg, 
                                    int ad_off, int ad_mul, int ut_reg, 
                                    m200_output_mode ut_mod, float lod_off, swizzle_pattern ut_swz, 
                                    basic_block* jmp_tar, m200_comparison cmp)
{
	m200_instruction* ins = _essl_mempool_alloc(pool, sizeof(m200_instruction));

	ins->opcode = op_code;
	ins->opcode_flags = op_msk;
	fill_input_arg(&ins->args[0], in0reg, in0swz, in0abs, in0neg);
	fill_input_arg(&ins->args[1], in1reg, in1swz, in1abs, in1neg);
	ins->address_offset = ad_off*ad_mul;
	ins->address_multiplier = ad_mul;
	ins->out_reg = ut_reg;
	ins->output_mode = ut_mod;
	ins->lod_offset = lod_off;
	ins->output_swizzle = ut_swz;
	ins->jump_target = jmp_tar;
	ins->compare_function = cmp;

	return ins;
}


static m200_instruction_word* make_long_instr(mempool* pool, m200_instruction_word* next)
{
	m200_instruction_word* ins = _essl_mempool_alloc(pool, sizeof(m200_instruction_word));
	
	ins->successor = next;

	return ins;
}


static signed char single_swizz(char symb)
{
	switch (symb)
	{
	case 'x': case 'X':
		return 0;
	case 'y': case 'Y':
		return 1;
	case 'z': case 'Z':
		return 2;
	case 'w': case 'W':
		return 3;
	case '0': 
		return 4;
	case '1': 
		return 5;
	case '-': 
		return 6;
	case 'f': case 'F':
		return 7;
	case '?':
		return -1;
	}

	return -1;
}


static swizzle_pattern make_swz(char* inp)
{
	int ix;
	swizzle_pattern sp = _essl_create_undef_swizzle();

	for (ix = 0; ix < 4; ++ix)
	{
		if (inp[ix] == 0)
		{
			break;
		}

		sp.indices[ix] = single_swizz(inp[ix]);
	}


	return sp;
}



static essl_bool binaries_differ(mali200_emit_context* ctx, u32* correct, size_t correctZ, const char* id)
{
	if (memcmp(_essl_output_buffer_get_raw_pointer(ctx->output_buf),
	           correct,
	           ESSL_MIN(correctZ, _essl_output_buffer_get_size(ctx->output_buf) * 4)))
	{
		size_t nr;

		printf("\n****** diff for %s ******", id);
		print_ctx("\ndebug_buf:", ctx);
#ifdef DETAILED_DIFF
		print_instr_word(0, _essl_output_buffer_get_raw_pointer(ctx->output_buf));
#endif
		printf("correct:");

		for (nr = 0; nr < correctZ/4; ++nr)
		{
			printf("%s%08x", (nr % 4 == 0)? "\n " : " ", correct[nr]);
		}

		printf("\n");
#ifdef DETAILED_DIFF
		print_instr_word(0, correct); 
#endif
		return ESSL_TRUE;
	}

	return ESSL_FALSE;
}


static int simple_test(mempool* pool, error_context* err_ctx)
{
	mali200_emit_context ctx_object;
	mali200_emit_context* ctx = &ctx_object;
	output_buffer output_buf;
	int tests_failed = 0;

	u32 correct4 [] = {0x02037e80, 0xf2e4558a, 0x1d65500b, 0x00722800, 
	                   0x05517100, 0x9356800c, 0x1a732ab0, 0xafd13113, 
	                   0x69a3df27, 0x000fb9aa, 0x00000000, 0x80104000, 
	                   0x0012a00f, 0x00000010};
	u32 correct5 [] = {0x020093c0, 0xffe40804, 0x86001500, 0x39001000, 
	                   0x011c0804, 0xf8801e00, 0x204b1e83, 0x00007c2b};
	u32 correct6 [] = {0x0203ffc0, 0xff1d14e0, 0x82101655, 0x95000007, 
	                   0x035c0000, 0x265cea00, 0x1550b627, 0x315444a3, 
	                   0x9f1c4000, 0x06534015, 0xb0c7c803, 0x005aa028, 
	                   0x004c3a00, 0x00000000, 0x07000000, 0x082007c0, 
	                   0x00000890};
	u32 correct7 [] = {0x0200cdc0, 0xff030c60, 0x00000000, 0x39001004, 
	                   0x5390855e, 0x38006f8f, 0x9e8043f6, 0x000447a7, 
	                   0x00000000};
	u32 correct7b[] = {0x02036000, 0x0a4a058b, 0x8ba009c9, 0x00007814, 
	                   0x00000000, 0x00000000, 0x00000000, 0x00000000};
	u32 correct8 [] = {0x0200bcc0, 0xf2003c0d, 0xc1b324d0, 0xe0112df8,
	                   0x00721a08, 0xc4a21200, 0x40d42304, 0x00064900};
	u32 correct8b[] = {0x02011e00, 0x19440403, 0x0004e800, 0xf3cf9ea0, 
	                   0x24c9c4e0, 0x469c8e4f, 0x00188f45, 0x00000000, 
	                   0x00000000};

	swizzle_pattern no_care_swz = make_swz("");
	swizzle_pattern all_swz = make_swz("xyzw");
	m200_instruction_word* in8b = make_long_instr(pool, NULL);
	m200_instruction_word* in8  = make_long_instr(pool, in8b);
	m200_instruction_word* in7b = make_long_instr(pool, in8);
	m200_instruction_word* in7  = make_long_instr(pool, in7b);
	m200_instruction_word* in6  = make_long_instr(pool, in7);
	m200_instruction_word* in5  = make_long_instr(pool, in6);
	m200_instruction_word* in4  = make_long_instr(pool, in5);

    /**********/

	in4->var =    make_instr(pool, M200_NORM3, (M200_NORM_RSQR),
							 M200_R5, all_swz, ESSL_FALSE, ESSL_TRUE, 
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 -1, -1, M200_R2,
							 M200_OUTPUT_TRUNCATE, 0.0, all_swz,
							 0, M200_CMP_NEVER);

	in4->load =   make_instr(pool, M200_LD_REL, (0),
							 M200_R5, make_swz("z"), ESSL_FALSE, ESSL_FALSE, 
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 3, 2, M200_HASH_LOAD, 
							 M200_OUTPUT_NORMAL, 0.0, all_swz,
							 0, M200_CMP_NEVER);
	fill_input_arg(&in4->load->args[2], M200_R5, make_swz("y"), ESSL_FALSE, ESSL_FALSE);

	in4->mul4 =   make_instr(pool, M200_NOT, (0),
							 M200_R5, all_swz, ESSL_FALSE, ESSL_FALSE, 
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 -1, -1, M200_R2, 
							 M200_OUTPUT_CLAMP_0_INF, 0.0, make_swz("?yzw"),
							 0, M200_CMP_NEVER);

	in4->mul1 =   make_instr(pool, M200_CMP, (0),
							 M200_R5, make_swz("y"), ESSL_FALSE, ESSL_FALSE, 
							 M200_CONSTANT0, make_swz("x"), ESSL_FALSE, ESSL_FALSE, /* 2.5 */
							 -1, -1, M200_R_IMPLICIT, 
							 M200_OUTPUT_NORMAL, 0.0, all_swz,
							 0, M200_CMP_GT);

	in4->add4 =   make_instr(pool, M200_ADD_D4, (0),
							 M200_R5, make_swz("wxyz"), ESSL_FALSE, ESSL_FALSE, 
							 M200_CONSTANT0, make_swz("zzzz"), ESSL_FALSE, ESSL_FALSE, /* 13.0, 13.0 ..*/
							 -1, -1, M200_R3, 
							 M200_OUTPUT_CLAMP_0_INF, 0.0, make_swz("xyz?"),
							 0, M200_CMP_NEVER);

	in4->add1 =   make_instr(pool, M200_SEL, (0),
							 M200_R4, make_swz("w"), ESSL_FALSE, ESSL_FALSE, 
							 M200_CONSTANT0, make_swz("y"), ESSL_FALSE, ESSL_FALSE, /* 1.5 */
							 -1, -1, M200_R4, 
							 M200_OUTPUT_TRUNCATE, 0.0, make_swz("?y??"),
							 0, M200_CMP_NEVER);

	in4->lut =    make_instr(pool, M200_MUL, (0),
							 M200_R1, make_swz("w"), ESSL_FALSE, ESSL_TRUE, 
							 M200_HASH_LOAD, make_swz("wxyz"), ESSL_FALSE, ESSL_FALSE, 
							 -1, -1, M200_R4, 
							 M200_OUTPUT_NORMAL, 0.0, make_swz("x?zw"),
							 0, M200_CMP_NEVER);

	in4->branch = make_instr(pool, M200_KILL, (0),
							 M200_R5, make_swz("y"), ESSL_FALSE, ESSL_FALSE, 
							 M200_CONSTANT0, make_swz("w"), ESSL_FALSE, ESSL_FALSE, /* 2.0 */
							 0, 0, M200_REG_UNKNOWN, 
							 M200_OUTPUT_NORMAL, 0.0, all_swz,
							 0, M200_CMP_NE);

	in4->n_embedded_entries[0] = 4;
	in4->embedded_constants[0][0] = 2.5;
	in4->embedded_constants[0][1] = 1.5;
	in4->embedded_constants[0][2] = 13.0;
	in4->embedded_constants[0][3] = 2.0;

    /**********/

	in5->var =    make_instr(pool, M200_MOV, (0),
							 M200_R2, all_swz, ESSL_FALSE, ESSL_FALSE, 
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 0, 0, M200_HASH_LOAD, 
							 M200_OUTPUT_NORMAL, 0.0, all_swz,
							 0, M200_CMP_NEVER);

	in5->tex =    make_instr(pool, M200_TEX, (0),
							 M200_R5, make_swz("y"), ESSL_FALSE, ESSL_FALSE, 
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 0, -1, M200_HASH_TEX, 
							 M200_OUTPUT_NORMAL, 1.5, all_swz,
							 0, M200_CMP_NEVER);

	in5->load =   make_instr(pool, M200_LD_REG, (0),
							 M200_R1, make_swz("w"), ESSL_FALSE, ESSL_FALSE, 
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 0, 2, M200_HASH_LOAD, 
							 M200_OUTPUT_NORMAL, 0.0, all_swz,
							 0, M200_CMP_NEVER);

	in5->add4 =   make_instr(pool, M200_SWZ, (0),
							 M200_HASH_LOAD, make_swz("y-fw"), ESSL_FALSE, ESSL_FALSE, 
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 -1, -1, M200_R4, 
							 M200_OUTPUT_NORMAL, 0.0, all_swz,
							 0, M200_CMP_NEVER);

	in5->store =  make_instr(pool, M200_ST_REL, (0),
							 M200_HASH_TEX, make_swz("x"), ESSL_FALSE, ESSL_FALSE, 
							 M200_R4, all_swz, ESSL_FALSE, ESSL_FALSE, 
							 1, 4, M200_REG_UNKNOWN, 
							 M200_OUTPUT_NORMAL, 0.0, all_swz,
							 0, M200_CMP_NEVER);
	fill_input_arg(&in5->store->args[2], M200_R5, make_swz("y"), ESSL_FALSE, ESSL_FALSE);

    /**********/

	in6->var =    make_instr(pool, M200_VAR, (M200_VAR_FLATSHADING),
							 M200_R5, make_swz("y"), ESSL_FALSE, ESSL_FALSE, 
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 7, 4, M200_R_IMPLICIT, 
							 M200_OUTPUT_CLAMP_0_1, 0.0, all_swz,
							 0, M200_CMP_NEVER);

	in6->tex =    make_instr(pool, M200_TEX, (M200_TEX_NO_RESET_SUMMATION),
							 M200_R5, make_swz("z"), ESSL_FALSE, ESSL_FALSE, 
							 M200_R5, make_swz("y"), ESSL_FALSE, ESSL_FALSE, 
							 7, -1, M200_R_IMPLICIT, 
							 M200_OUTPUT_CLAMP_0_INF, 0.5, make_swz("xyyy"),
							 0, M200_CMP_NEVER);

	in6->load =   make_instr(pool, M200_LD_UNIFORM, (0),
							 M200_R5, make_swz("w"), ESSL_FALSE, ESSL_FALSE, 
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 1, 1, M200_HASH_LOAD, 
							 M200_OUTPUT_NORMAL, 0.0, all_swz,
							 0, M200_CMP_NEVER);

	in6->mul4 =   make_instr(pool, M200_MUL_X2, (0),
							 M200_R5, make_swz("wyzw"), ESSL_FALSE, ESSL_TRUE, 
							 M200_CONSTANT0, all_swz, ESSL_FALSE, ESSL_FALSE, /* 0.5  1.5  2.5  4.5 */
							 -1, -1, M200_R1, 
							 M200_OUTPUT_CLAMP_0_1, 0.0, make_swz("xy?w"),
							 0, M200_CMP_NEVER);

	in6->mul1 =   make_instr(pool, M200_MUL_X4, (0),
							 M200_R5, make_swz("y"), ESSL_TRUE, ESSL_FALSE, 
							 M200_CONSTANT0, make_swz("y"), ESSL_FALSE, ESSL_FALSE, /* 1.5 */
							 -1, -1, M200_R2, 
							 M200_OUTPUT_NORMAL, 0.0, make_swz("??z?"),
							 0, M200_CMP_GT);

	in6->add4 =   make_instr(pool, M200_MOV, (0),
							 M200_R5, make_swz("yyxw"), ESSL_FALSE, ESSL_FALSE,
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 -1, -1, M200_R1, 
							 M200_OUTPUT_NORMAL, 0.0, make_swz("xyz?"),
							 0, M200_CMP_NEVER);

	in6->add1 =   make_instr(pool, M200_FLOOR, (0),
							 M200_R5, make_swz("z"), ESSL_TRUE, ESSL_FALSE, 
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 -1, -1, M200_R3, 
							 M200_OUTPUT_CLAMP_0_INF, 0.0, make_swz("?y??"),
							 0, M200_CMP_NEVER);

	in6->lut =    make_instr(pool, M200_SIN, (0),
							 M200_HASH_LOAD, make_swz("z"), ESSL_TRUE, ESSL_FALSE, 
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 -1, -1, M200_R1, 
							 M200_OUTPUT_NORMAL, 0.0, make_swz("??z?"),
							 0, M200_CMP_NEVER);

	in6->store =  make_instr(pool, M200_LEA, (0),
							 M200_R5, make_swz("y"), ESSL_FALSE, ESSL_FALSE, 
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 5, 2, M200_R4, 
							 M200_OUTPUT_NORMAL, 0.0, make_swz("?y??"),
							 0, M200_CMP_NEVER);

	in6->branch = make_instr(pool, M200_JMP, (0),
							 M200_HASH_TEX, make_swz("z"), ESSL_FALSE, ESSL_FALSE, 
							 M200_CONSTANT0, make_swz("x"), ESSL_FALSE, ESSL_FALSE, /* 0.5 */
							 5, 4, M200_REG_UNKNOWN, 
							 M200_OUTPUT_NORMAL, 0.0, no_care_swz,
							 0, M200_CMP_GT);

	in6->n_embedded_entries[0] = 4;
	in6->embedded_constants[0][0] = 0.5;
	in6->embedded_constants[0][1] = 1.5;
	in6->embedded_constants[0][2] = 2.5;
	in6->embedded_constants[0][3] = 4.5;

    /**********/

	in7->var =    make_instr(pool, M200_VAR, (0),
							 M200_R3, make_swz("w"), ESSL_FALSE, ESSL_FALSE, 
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 0, 4, M200_R_IMPLICIT, 
							 M200_OUTPUT_NORMAL, 0.0, all_swz,
							 0, M200_CMP_NEVER);
	
	in7->tex =    make_instr(pool, M200_TEX, (0),
							 M200_REG_UNKNOWN, no_care_swz, ESSL_TRUE, ESSL_FALSE, 
							 M200_REG_UNKNOWN, no_care_swz, ESSL_TRUE, ESSL_FALSE, 
							 4, -1, M200_HASH_TEX, 
							 M200_OUTPUT_NORMAL, 0.0, all_swz, 
							 0, M200_CMP_NEVER);
	
	in7->mul1 =   make_instr(pool, M200_MOV, (0),
							 M200_R3, make_swz("y"), ESSL_FALSE, ESSL_FALSE, 
							 M200_REG_UNKNOWN, all_swz, ESSL_TRUE, ESSL_FALSE, 
							 -1, -1, M200_R1, 
							 M200_OUTPUT_CLAMP_0_1, 0.0, make_swz("???w"),
							 0, M200_CMP_NEVER);
	
	in7->mul4 =   make_instr(pool, M200_CONS13, (0),
							 M200_HASH_TEX, make_swz("y"), ESSL_FALSE, ESSL_FALSE, 
							 M200_R2, all_swz, ESSL_FALSE, ESSL_FALSE,
							 -1, -1, M200_R5, 
							 M200_OUTPUT_NORMAL, 0.0, all_swz,
							 0, M200_CMP_GT);

	in7->lut =    make_instr(pool, M200_ATAN1_IT1, (0),
							 M200_R3,  make_swz("w"), ESSL_TRUE, ESSL_FALSE,
							 M200_REG_UNKNOWN, no_care_swz,  ESSL_FALSE, ESSL_FALSE,
							 -1,  -1, M200_R4, 
							 M200_OUTPUT_NORMAL, 0.0, all_swz, 
							 0, M200_CMP_NEVER );

	in7->store =  make_instr(pool, M200_LD_RGB, (0),
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE,
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE,
							 0, -1, M200_R2, 
							 M200_OUTPUT_NORMAL, 0.0, no_care_swz,
							 0, M200_CMP_NEVER);

    /**********/

	in7b->add1 =  make_instr(pool, M200_ADD_D8, (0),
							 M200_R2, make_swz("w"), ESSL_FALSE, ESSL_TRUE, 
							 M200_R1, make_swz("y"), ESSL_FALSE, ESSL_FALSE, 
							 -1, -1, M200_R2, 
							 M200_OUTPUT_NORMAL, 0.0, make_swz("??z?"),
							 0, M200_CMP_NEVER);

	in7b->lut =   make_instr(pool, M200_ATAN_IT2, (0),
							 M200_R4, all_swz, ESSL_FALSE, ESSL_FALSE, 
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 -1, -1, M200_R5, 
							 M200_OUTPUT_CLAMP_0_1, 0.0, make_swz("???w"),
							 0, M200_CMP_NEVER);

	in7b->branch= make_instr(pool, M200_CALL, (0),
							 M200_R2, make_swz("z"), ESSL_FALSE, ESSL_FALSE, 
							 M200_CONSTANT0, make_swz("x"), ESSL_FALSE, ESSL_FALSE, 
							 12, -1, M200_REG_UNKNOWN, 
							 M200_OUTPUT_NORMAL, 0.0, no_care_swz,
							 0, M200_CMP_LE);

	in7b->n_embedded_entries[0] = 1;
	in7b->embedded_constants[0][0] = 0.0;

    /**********/

	in8->var =    make_instr(pool, M200_MISC_VAL, (0),
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 -1, -1, M200_R2, 
							 M200_OUTPUT_NORMAL, 0.0, all_swz, 
							 0, M200_CMP_NEVER);
	
	in8->mul4 =   make_instr(pool, M200_CMP, (0),
							 M200_R4, make_swz("wxyz"), ESSL_FALSE, ESSL_FALSE,
							 M200_R3, make_swz("WZYX"), ESSL_FALSE, ESSL_FALSE,
							 -1, -1, M200_R3, 
							 M200_OUTPUT_TRUNCATE, 0.0, make_swz("?yzw"),
							 0, M200_CMP_GT);

	in8->mul1 =   make_instr(pool, M200_NOT, (0),
							 M200_R2, make_swz("y"), ESSL_FALSE, ESSL_TRUE, 
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 -1, -1, M200_R1, 
							 M200_OUTPUT_NORMAL, 0.0, make_swz("???w"),
							 0, M200_CMP_NEVER);

	in8->add4 =   make_instr(pool, M200_HADD4, (0),
							 M200_R3, all_swz, ESSL_FALSE, ESSL_FALSE,
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 -1, -1, M200_R4,
							 M200_OUTPUT_NORMAL, 0.0, make_swz("?y??"),
							 0, M200_CMP_NEVER);

	in8->add1 =   make_instr(pool, M200_DERX, (0),
							 M200_R2, make_swz("y"), ESSL_FALSE, ESSL_TRUE,
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE, 
							 -1, -1, M200_R1,
							 M200_OUTPUT_NORMAL, 0.0, make_swz("??z?"),
							 0, M200_CMP_NEVER);

	in8->store =  make_instr(pool, M200_ST_STACK, (0),
							 M200_R2, make_swz("y"), ESSL_FALSE, ESSL_FALSE,
							 M200_R4, make_swz("x"), ESSL_FALSE, ESSL_FALSE,
							 12, 1, M200_REG_UNKNOWN,
							 M200_OUTPUT_NORMAL, 0.0, no_care_swz,
							 0, M200_CMP_NEVER);

    /**********/

	in8b->load =  make_instr(pool, M200_LD_STACK, (0),
							 M200_R4, make_swz("y"), ESSL_FALSE, ESSL_FALSE,     /* index reg */
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE,
							 12, 2, M200_HASH_LOAD,
							 M200_OUTPUT_NORMAL, 0.0, all_swz,
							 0, M200_CMP_NEVER);

	in8b->mul4 =  make_instr(pool, M200_MOV, (0),
							 M200_R4, make_swz("wyzx"), ESSL_FALSE, ESSL_FALSE,
							 M200_REG_UNKNOWN, no_care_swz, ESSL_FALSE, ESSL_FALSE,
							 -1, -1, M200_R5,
							 M200_OUTPUT_NORMAL, 0.0, all_swz, 
							 0, M200_CMP_NEVER);

	in8b->mul1 =  make_instr(pool, M200_CMP, (0),
							 M200_HASH_LOAD, make_swz("X"), ESSL_FALSE, ESSL_FALSE, 
							 M200_R3, make_swz("w"), ESSL_FALSE, ESSL_FALSE, 
							 -1, -1, M200_R3, 
							 M200_OUTPUT_NORMAL, 0.0, make_swz("??z?"),
							 0, M200_CMP_GE);

	in8b->add4 =  make_instr(pool, M200_ADD_D4, (0),
							 M200_R2, make_swz("wxyz"), ESSL_FALSE, ESSL_FALSE,
							 M200_HASH_LOAD, all_swz, ESSL_FALSE, ESSL_FALSE,
							 -1, -1, M200_R2, 
							 M200_OUTPUT_CLAMP_0_INF, 0.0, make_swz("xyz?"),
							 0, M200_CMP_NEVER);

	in8b->branch= make_instr(pool, M200_RET, (0),
							 M200_R4, make_swz("y"), ESSL_FALSE, ESSL_FALSE, 
							 M200_R3, make_swz("w"), ESSL_FALSE, ESSL_FALSE, 
							 12, -1, M200_REG_UNKNOWN, 
							 M200_OUTPUT_NORMAL, 0.0, no_care_swz,
							 0, M200_CMP_EQ);

    /**********/

#ifdef PDEBUG 
	printf("(PDEBUG is defined)  ");
#endif /* PDEBUG */ 

	assert(emit_context_init(ctx, &output_buf, err_ctx, pool));

	assert(emit_wide_instruction(ctx, in4));
	if (binaries_differ(ctx, correct4, sizeof(correct4), "in4"))
	{
		++tests_failed;
	}

	assert(emit_context_reset(ctx, pool));
	assert(emit_wide_instruction(ctx, in5));
	if (binaries_differ(ctx, correct5, sizeof(correct5), "in5"))
	{
		++tests_failed;
	}

	assert(emit_context_reset(ctx, pool));
	assert(emit_wide_instruction(ctx, in6));
	if (binaries_differ(ctx, correct6, sizeof(correct6), "in6"))
	{
		++tests_failed;
	}

	assert(emit_context_reset(ctx, pool));
	assert(emit_wide_instruction(ctx, in7));
	if (binaries_differ(ctx, correct7, sizeof(correct7), "in7"))
	{
		++tests_failed;
	}

	assert(emit_context_reset(ctx, pool));
	assert(emit_wide_instruction(ctx, in7b));
	if (binaries_differ(ctx, correct7b, sizeof(correct7b), "in7b"))
	{
		++tests_failed;
	}

	assert(emit_context_reset(ctx, pool));
	assert(emit_wide_instruction(ctx, in8));
	if (binaries_differ(ctx, correct8, sizeof(correct8), "in8"))
	{
		++tests_failed;
	}

	assert(emit_context_reset(ctx, pool));
	assert(emit_wide_instruction(ctx, in8b));
	if (binaries_differ(ctx, correct8b, sizeof(correct8b), "in8b"))
	{
		++tests_failed;
	}

	return tests_failed;
}


int main(int argc, char* argv[])
{
	mempool_tracker tracker;
	mempool pool;
	error_context err_room;
	int ss [2] = {0, 0};
	int failed;

	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	_essl_mempool_init(&pool, 0, &tracker);
	if (!_essl_error_init(&err_room, &pool, "Testing emit. ", ss, 2))
	{
		printf("\nNo error_context ");
	}

	failed = simple_test(&pool, &err_room);

	if (err_room.buf_idx > 0)
	{
		printf("\nerr_room.buf:%p buf_idx:%u buf_size:%u ", err_room.buf, (unsigned int)err_room.buf_idx, (unsigned int)err_room.buf_size);
		printf("\n%s", err_room.buf);
		++failed;
	}

	_essl_mempool_destroy(&pool);

	if (failed)
	{
		printf("\nErrors in emit (%d)\n", failed);
		return 1;
	}

	printf("All tests OK!\n");

	return 0;
}


#endif /* UNIT_TEST */

