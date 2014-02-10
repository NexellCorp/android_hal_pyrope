/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef FIND_BLOCKS_FOR_OPERATIONS_H
#define FIND_BLOCKS_FOR_OPERATIONS_H

#include "common/lir_pass_run_manager.h"
#include "common/basic_block.h"
#include "common/essl_target.h"

memerr _essl_find_blocks_for_operations(pass_run_context *pr_ctx, symbol *func);
memerr _essl_find_blocks_for_operations_func(mempool *pool, symbol *function);
int _essl_calc_op_weight(node *n, basic_block* best_block, target_descriptor *desc, ptrset *visited_nodes, int *weight);

#endif
