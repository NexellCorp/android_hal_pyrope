/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef SHADERGEN_MALIGP2_INSTRUCTION_H
#define SHADERGEN_MALIGP2_INSTRUCTION_H

#include "shadergen_maligp2/shader_pieces.h"

typedef enum {
	INSTRUCTION_MASK_NONE = 0,
	INSTRUCTION_MASK_INPUT_REG = 1,
	INSTRUCTION_MASK_CONSTANT_REG = 2
} instruction_mask_flag;

void _shadergen_maligp2_merge_instructions(instruction *first, const instruction *second, instruction_mask_flag fields);
void _shadergen_maligp2_correct_flow_address(instruction *inst, int inst_address);


#endif
