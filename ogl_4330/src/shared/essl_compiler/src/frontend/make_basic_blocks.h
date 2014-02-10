/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MIDDLE_MAKE_BASIC_BLOCKS_H
#define MIDDLE_MAKE_BASIC_BLOCKS_H

#include "common/essl_node.h"
#include "common/basic_block.h"
#include "common/error_reporting.h"
#include "common/essl_target.h"
#include "common/ptrdict.h"

typedef struct {
	/** For error reporting */
	error_context *err;
	/** For creating type objects */
	typestorage_context *typestor_context;
	/** For allocating memory */
	mempool *pool;
	/** For scalar value conversions */
	target_descriptor *desc;

	/** Current function being processed */
	symbol *function;
	/** Return value of the current function */
	symbol *return_value_symbol;
	/** Current block */
	basic_block *current;

	/** Cost of placing nodes in the current block */
	float current_block_cost;
	/** Pointer to the next-field of the last element in the list of local operations */
	local_operation **next_local_p;
	/** Pointer to the next-field of the last element in the list of control dependent operations */
	control_dependent_operation **next_control_dependent_p;

	/** Memoization of visited nodes. Indexed by is_address. */
	ptrdict visited[2];
} make_basic_blocks_context;

#define MBB_VISITED_VALUE 0
#define MBB_VISITED_ADDRESS 1

memerr _essl_make_basic_blocks_init(/*@out@*/ make_basic_blocks_context *ctx, error_context *err, typestorage_context *ts_ctx, mempool *pool, target_descriptor *desc);
memerr _essl_make_basic_blocks(make_basic_blocks_context *ctx, symbol *function);

/*
  flag to tell whether a variable reference should result in a load or store
 */
typedef enum {
	NO_NEED_LOAD_STORE,
	NEED_LOAD_STORE
} need_load_store;

/*
  flag to tell whether a variable reference is a control dependent load/store. If this is set, NEED_LOAD_STORE is also set.
 */
typedef enum {
	NO_VAR_IS_CONTROL_DEPENDENT,
	VAR_IS_CONTROL_DEPENDENT
} var_control_dependent;

/**
   helper function to split the lvalue node n into two, while making sure side effects are shared
*/
memerr _essl_middle_split_lvalue(node *n, /*@out@*/node **left, /*@out@*/node **right, var_control_dependent is_control_dependent, mempool *pool);

var_control_dependent _essl_is_var_ref_control_dependent(symbol *s);

essl_bool _essl_is_var_ref_load(symbol *s);

#endif
