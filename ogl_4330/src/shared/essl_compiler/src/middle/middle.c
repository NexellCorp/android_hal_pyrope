/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "common/lir_pass_run_manager.h"
#include "middle/middle.h"
#include "middle/optimise_basic_blocks.h"
#include "middle/optimise_constant_fold.h"
#include "middle/control_deps.h"
#include "middle/conditional_select.h"
#include "common/find_blocks_for_operations.h"
#include "middle/expand_builtin_functions.h"
#include "middle/optimise_vector_ops.h"
#include "middle/rewrite_sampler_accesses.h"
#include "middle/proactive_calculations.h"

#ifdef DEBUGPRINT
#include "common/middle_printer.h"
#include "common/lir_printer.h"
#endif


memerr _essl_middle_transform(mempool *pool, error_context *err, typestorage_context *ts_ctx, target_descriptor *desc, compiler_options* opts, translation_unit *tu)
{
	pass_run_context pr_ctx;

	pr_ctx.pool = pool;
	pr_ctx.err_ctx = err;
	pr_ctx.ts_ctx = ts_ctx;
	pr_ctx.desc = desc;
	pr_ctx.opts = opts;
	pr_ctx.tu = tu;
	pr_ctx.pass_nr = 1;

#ifdef DEBUGPRINT
	ESSL_CHECK(_essl_run_lir_tu_pass(&pr_ctx, NULL, "init"));
#endif

	if(desc->enable_proactive_shaders)
	{
		ESSL_CHECK(_essl_run_lir_tu_pass(&pr_ctx, _essl_optimise_constant_input_calculations, "pilot_shader"));
	}

	ESSL_CHECK(_essl_run_lir_function_pass(&pr_ctx, _essl_expand_builtin_functions, "expand_builtins"));
	ESSL_CHECK(_essl_run_lir_function_pass(&pr_ctx, _essl_rewrite_sampler_accesses, "rewrite_image_sampler_accesses"));
	/* Constant propagation and folding */
	ESSL_CHECK(_essl_run_lir_function_pass(&pr_ctx, _essl_optimise_constant_fold_nodes_and_blocks, "optimise_constant_fold"));
	/* Remove surplus basic blocks */
	ESSL_CHECK(_essl_run_lir_function_pass(&pr_ctx, _essl_optimise_basic_block_sequences, "optimise_basic_blocks"));
	ESSL_CHECK(_essl_run_lir_function_pass(&pr_ctx, _essl_find_blocks_for_operations, "find_best_block"));
	
	if (opts != NULL && opts->optimise_conditional_select)
	{
		ESSL_CHECK(_essl_run_lir_function_pass(&pr_ctx, _essl_optimise_conditional_selects, "conditional_select"));
	}

	if(!tu->desc->no_store_load_forwarding_optimisation)
	{
		if((opts && opts->optimise_store_load_forwarding != 0)) 
		{
			ESSL_CHECK(_essl_run_lir_function_pass(&pr_ctx, _essl_forward_stores_to_loads_and_elide_stores, "store_load_forwarding"));
			ESSL_CHECK(_essl_run_lir_function_pass(&pr_ctx, _essl_find_blocks_for_operations, "find_best_block"));

		}
	}

	ESSL_CHECK(_essl_run_lir_function_pass(&pr_ctx, _essl_optimise_basic_block_joins, "optimise_basic_blocks"));
	ESSL_CHECK(_essl_run_lir_function_pass(&pr_ctx, _essl_find_blocks_for_operations, "find_best_block"));

	ESSL_CHECK(_essl_run_lir_function_pass(&pr_ctx, _essl_optimise_vector_ops, "optimise_vector_ops"));

	/* build control dependency graph */

	ESSL_CHECK(_essl_run_lir_function_pass(&pr_ctx, _essl_control_dependencies_calc, "control_dependence"));

	return MEM_OK;
}
