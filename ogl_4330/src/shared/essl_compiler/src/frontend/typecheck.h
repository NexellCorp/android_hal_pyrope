/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef FRONTEND_TYPECHECK_H
#define FRONTEND_TYPECHECK_H

#include "common/essl_node.h"
#include "frontend/constant_fold.h"
#include "frontend/precision.h"
#include "common/error_reporting.h"
#include "frontend/lang.h"
#include "common/symbol_table.h"
#include "common/ptrset.h"
#include "common/essl_stringbuffer.h"

/**
   Type checking and constant folding.

   There are two interactions between the constant folding and
   the type checking.

   1. Array sizes are given as integral constant expressions, and in
      order to find the values of these expressions, we must perform
      constant folding. The size of an array is part of its type, per
      6.1 in the spec overload resolution depends on the sizes of the
      arrays, and we are also required to generate errors when arrays
      are indexed by constant expressions and these indices are out of
      bounds. Therefore, type checking of an array definition can only
      be done when the array size expression (and consequently, all
      previous declarations used by the array size expression) have
      been constant folded.

   2. Per 4.3.3 in the latest ESSL specification, a built-in function
      call whose arguments are all constant expressions, with the
      exception of the texture lookup functions, is also a constant
      expression. Therefore the constant folding pass must also be
      able to handle calls to built-in functions. However, most of the
      built-in functions are overloaded, and overload resolution
      depends on the types of the arguments. Therefore, constant
      folding of a built-in function cannot happen before the
      arguments to this function have been type checked.

   These two interactions led us to combine the two passes in the way
   described in the design document.

*/

typedef struct _tag_typecheck_context {
	constant_fold_context cfold_ctx;
	error_context *err_context;
	typestorage_context *typestor_context;
	mempool *pool;
	target_descriptor *desc;
	precision_context prec_ctx;
	language_descriptor *lang_desc;

	ptrset function_calls;
	ptrdict fun_decl_to_def;
	string_buffer *strbuf;
} typecheck_context;

/** Performs type checking on a single node.
	@param ctx The type checking context
	@param n The node to type check
	@returns The processed node if successful, null if unsuccessful
*/
/*@null@*/ node *_essl_typecheck_single_node(typecheck_context *ctx, node *n);

/**
   Type check and constant fold a semantic tree
   @param ctx The type checking context
   @param n The root node of the tree to type check and constant fold
   @returns The processed root node if successful, null if unsuccessful
*/

/*@null@*/ node *_essl_typecheck(typecheck_context *ctx, node *n, /*@out@*/ ptrdict **node_to_precision_dict);

/**  Initialize a type checking context
	 @returns zero if successful, non-zero if unsuccessful
 */
memerr _essl_typecheck_init(/*@out@*/ typecheck_context *ctx, mempool *pool, error_context *err, typestorage_context *ts_ctx, target_descriptor *desc, language_descriptor* lang_desc);

#endif /* FRONTEND_TYPECHECK_H */

