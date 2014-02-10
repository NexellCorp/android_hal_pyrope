/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALI200_WORD_SPLITTING_H
#define MALI200_WORD_SPLITTING_H

#include "common/basic_block.h"
#include "backend/liveness.h"
#include "mali200/mali200_regalloc.h"

memerr _essl_mali200_split_word(regalloc_context *ctx, m200_instruction_word *word, basic_block *block);

#endif
