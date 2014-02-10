/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef FRONTEND_CONSTANT_FOLD_H
#define FRONTEND_CONSTANT_FOLD_H

#include "common/essl_mem.h"
#include "common/essl_node.h"
#include "common/essl_target.h"
#include "common/error_reporting.h"

/**
   Constant folding pass.

   The constant folding pass performed in the frontend is done in cooperation with the type checker,
   and the type checker is calling the shots. Therefore, _essl_constant_fold_single_node is called
   by the _essl_typecheck.

*/


typedef struct {
	mempool *pool;
	target_descriptor *desc;

	error_context *err_context;
} constant_fold_context;


/** Performs constant folding on a single node.
	@param ctx The constant folding context
	@param n The node to constant fold
	@returns The processed node if successful, null if unsuccessful

*/
/*@null@*/ node *_essl_constant_fold_single_node(constant_fold_context *ctx, node *n);

/**  Initialize a constant folding context
	 @returns zero if successful, non-zero if unsuccessful
 */
memerr _essl_constant_fold_init(/*@out@*/ constant_fold_context *ctx, mempool *pool, target_descriptor *desc, error_context *e_ctx);



#endif /* FRONTEND_CONSTANT_FOLD_H */

