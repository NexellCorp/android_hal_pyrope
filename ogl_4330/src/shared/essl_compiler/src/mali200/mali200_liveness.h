/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALI200_LIVENESS_H
#define MALI200_LIVENESS_H

#include "common/essl_node.h"
#include "common/basic_block.h"
#include "common/essl_mem.h"
#include "backend/liveness.h"

liveness_context *_essl_mali200_calculate_live_ranges(mempool *pool, control_flow_graph *cfg, target_descriptor *desc,
													  error_context *err);

#endif
