/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALIGP2_LIVENESS_H
#define MALIGP2_LIVENESS_H

#include "common/essl_node.h"
#include "common/basic_block.h"
#include "common/essl_mem.h"
#include "backend/liveness.h"
#include "maligp2/maligp2_instruction.h"

liveness_context *_essl_maligp2_calculate_live_ranges(mempool *pool, control_flow_graph *cfg, target_descriptor *desc,
													  error_context *err);
maligp2_instruction_word *_essl_maligp2_insert_word_before(mempool *pool, liveness_context *ctx, maligp2_instruction_word *word, basic_block *block);
maligp2_instruction_word *_essl_maligp2_insert_word_after(mempool *pool, liveness_context *ctx, maligp2_instruction_word *word, basic_block *block);
essl_bool _essl_maligp2_is_fixedpoint_range(live_range *r);

#endif
