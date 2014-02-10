/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MIDDLE_OPTIMISE_CONSTANT_FOLD
#define MIDDLE_OPTIMISE_CONSTANT_FOLD

#include "common/lir_pass_run_manager.h"

#include "common/essl_mem.h"
#include "common/essl_symbol.h"
#include "common/error_reporting.h"

memerr _essl_optimise_constant_fold_nodes_and_blocks(pass_run_context *pr_ctx, symbol *func);
memerr _essl_optimise_constant_fold_nodes(mempool *pool, symbol *function, target_descriptor *desc, error_context *e_ctx);


#endif
