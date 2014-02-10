/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MIDDLE_OPTIMISE_BASIC_BLOCKS_H
#define MIDDLE_OPTIMISE_BASIC_BLOCKS_H

#include "common/lir_pass_run_manager.h"
#include "common/basic_block.h"
#include "common/essl_target.h"

memerr _essl_optimise_basic_block_sequences(pass_run_context *pr_ctx, symbol *func);
memerr _essl_optimise_basic_block_joins(pass_run_context *pr_ctx, symbol *func);
void _essl_correct_output_sequence_list(control_flow_graph *cfg);


#endif
