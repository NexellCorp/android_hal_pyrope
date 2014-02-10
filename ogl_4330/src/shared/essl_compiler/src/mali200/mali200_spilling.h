/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALI200_SPILLING_H
#define MALI200_SPILLING_H

#include "mali200/mali200_regalloc.h"
#include "mali200/mali200_instruction.h"
#include "common/essl_symbol.h"
#include "backend/graph_coloring.h"

memerr _essl_mali200_create_spill_ranges(regalloc_context *ctx);
memerr _essl_mali200_insert_spills(regalloc_context *ctx);
m200_instruction_word *_essl_mali200_find_word_for_spill(regalloc_context *ctx, live_range *spill_range, basic_block **block_out);
int _essl_mali200_spill_cost(graph_coloring_context *gc_ctx, live_range *range);

#endif
