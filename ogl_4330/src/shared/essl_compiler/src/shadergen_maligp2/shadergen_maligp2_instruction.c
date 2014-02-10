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
#include "shadergen_maligp2/shadergen_maligp2_instruction.h"

static const instruction instruction_masks[] = {
	{ { 0x00000000, 0x00000000, 0x00000000, 0x00000000 } }, /* Empty mask */
	{ { 0x00000000, 0xfc000000, 0x00000007, 0x00000000 } }, /* Input register select */
	{ { 0x00000000, 0x007fc000, 0x00000000, 0x00000000 } }  /* Constant regsiter select */

/* unused masks: input, artihmetic, output, controlflow
	{ 0x00000000, 0xfc7fc000, 0x00000007, 0x00000000 },
	{ 0xffffffff, 0x00003fff, 0x03f80000, 0x000ffff0 },
	{ 0x00000000, 0x03800000, 0xfc07ff98, 0x0000000f },
	{ 0x00000000, 0x00000000, 0x00000060, 0xfff00000 }
*/
};

void _shadergen_maligp2_merge_instructions(instruction *first, const instruction *second, instruction_mask_flag fields) {
	int i;
	for (i = 0 ; i < 4 ; i++) {
		unsigned int mask = instruction_masks[fields].data[i];
		first->data[i] = (first->data[i] & ~mask) | (second->data[i] & mask);
	}
}

void _shadergen_maligp2_correct_flow_address(instruction *inst, int inst_address) {
	int flow_op = (inst->data[3] & 0x00f00000) >> 20;
	if (flow_op != 12 && flow_op != 0) { /* Don't correct write to constant registers */
		unsigned flow_address, branch_address, bank;
		int offset;
		branch_address = (inst->data[3] >> 24) & 0xFF;
		bank = (inst->data[2] >> 5) & 0x3;
		offset = branch_address | (bank << 8);
		if(offset >= 512) offset -= 1024;

		flow_address = inst_address+offset;
		assert(flow_address < 1024);

		branch_address = flow_address& 0xFF;
		bank = (flow_address>>8)&0xFF;
		if(bank == 0) bank = 3;

		inst->data[3] = (inst->data[3] & 0x00ffffff) | (branch_address << 24);
		inst->data[2] = (inst->data[2] & 0xffffff9f) | (bank << 5);
	}
}

