/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALI200_WORD_INSERTION_H
#define MALI200_WORD_INSERTION_H

#include "mali200/mali200_instruction.h"
#include "mali200/mali200_regalloc.h"
#include "common/basic_block.h"

m200_instruction_word *_essl_mali200_insert_word_before(liveness_context *liv_ctx, m200_instruction_word *word, basic_block *block);
m200_instruction_word *_essl_mali200_insert_word_after(liveness_context *liv_ctx, m200_instruction_word *word, basic_block *block);

memerr _essl_mali200_phielim_insert_move(void *vctx, node *src, node *dst, int earliest, int latest, basic_block *block, essl_bool backward,
										 int *src_position_p, int *dst_position_p, node ***src_ref_p, node ***dst_ref_p);

#endif
