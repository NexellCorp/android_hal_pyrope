/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef BACKEND_INSTRUCTION_H
#define BACKEND_INSTRUCTION_H

#include "common/basic_block.h"

/* Max number of instruction words supported by the compiler */
#define MAX_COMPILER_INSTRUCTION_WORDS 10000

typedef struct _tag_instruction_word
{
	/* instruction words are chained together as a double-linked list */
	struct _tag_instruction_word *predecessor;
	struct _tag_instruction_word *successor;

	short cycle; /**< the cycle of this instruction word, counted _backwards_ */
	short original_cycle;
} instruction_word;

void *_essl_instruction_word_at_cycle(mempool *pool, control_flow_graph *cfg, int cycle, basic_block **block_out);

#endif
