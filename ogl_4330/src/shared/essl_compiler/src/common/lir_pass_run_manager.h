/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef _COMMON_LIR_PASS_RUN_MANAGER
#define _COMMON_LIR_PASS_RUN_MANAGER

#include "common/essl_system.h"
#include "common/essl_mem.h"
#include "common/essl_target.h"
#include "common/essl_symbol.h"
#include "common/error_reporting.h"
#include "common/compiler_options.h"
#include "common/translation_unit.h"

typedef struct pass_run_context
{
	mempool *pool;
	mempool *tmp_pool;
	error_context *err_ctx;
	typestorage_context *ts_ctx;
	target_descriptor *desc;
	compiler_options *opts;
	translation_unit *tu;
	unsigned pass_nr;
}pass_run_context;

typedef memerr(*lir_func_pass)(pass_run_context *ctx, symbol *function);
typedef memerr(*lir_tu_pass)(pass_run_context *ctx, translation_unit *tu);
memerr _essl_run_lir_function_pass(pass_run_context *ctx, lir_func_pass pass, const char * const_pass_name);
memerr _essl_run_lir_tu_pass(pass_run_context *ctx, lir_tu_pass pass, const char * const_pass_name);

#endif /* _COMMON_LIR_PASS_RUN_MANAGER */
