/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MIDDLE_OPTIMISE_INLINE_FUNCTIONS_H
#define MIDDLE_OPTIMISE_INLINE_FUNCTIONS_H

#include "common/essl_node.h"
#include "common/basic_block.h"
#include "common/error_reporting.h"

typedef struct {
	error_context *err;
	mempool *pool;
	mempool temp_pool;

	symbol *function;
	ptrdict *control_dependence;

	/* used during block cloning */
	ptrdict cloned_nodes;
	ptrdict cloned_blocks;
	basic_block* clone_block;
} optimise_inline_functions_context;

memerr _essl_optimise_inline_functions_init(/*@out@*/ optimise_inline_functions_context *ctx, error_context *err, mempool *pool);
memerr _essl_optimise_inline_functions(optimise_inline_functions_context *ctx, symbol *function);

#endif /* MIDDLE_OPTIMISE_INLINE_FUNCTIONS_H */
