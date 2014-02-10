/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2009, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "middle/dominator.h"
#include "frontend/callgraph.h"
#include "common/essl_profiling.h"
#include "frontend/ast_to_lir.h"
#include "frontend/eliminate_complex_ops.h"
#include "frontend/make_basic_blocks.h"
#include "frontend/optimise_inline_functions.h"
#include "frontend/optimise_loop_entry.h"
#include "frontend/vertex_cpu_calculations.h"
#include "frontend/ssa.h"
#ifdef DEBUGPRINT
#include "common/lir_printer.h"
#endif

memerr _essl_ast_to_lir(mempool *pool, error_context *err, typestorage_context *ts_ctx, target_descriptor *desc, compiler_options* opts, translation_unit *tu)
{
	make_basic_blocks_context make_bb_ctx;
	optimise_inline_functions_context inline_functions_ctx;
	symbol_list *sl, *prev_sl = 0;

#ifdef DEBUGPRINT
	FILE *f0 = 0;
	
	f0 = fopen("ast_irm.txt", "w");
#endif

	/* Loop entry optimisation works on the AST */
	if(opts != NULL && opts->optimise_loop_entry)
	{
		for(sl = tu->functions; sl != 0; sl = sl->next)
		{
			symbol *function = sl->sym;
			ESSL_CHECK(_essl_optimise_loop_entry(pool, function, desc));
		}
		if(tu->entry_point != NULL)
		{
			for (sl = tu->functions; sl != 0; sl = sl->next)
			{
				symbol *function = sl->sym;
				function->calls_from = NULL;
				function->calls_to = NULL;
				function->call_count = 0;
			}
			ESSL_CHECK(_essl_make_callgraph(pool, tu->root));
		}

	}

#ifdef DEBUGPRINT
	{
		char *file_name = "ast_ir0.txt";
		FILE *f = 0;
	
		f = fopen(file_name, "w");
		if(f != NULL)
		{
			ESSL_CHECK(ast_translation_unit_to_txt(f, pool, tu, desc));
			fflush(f);
			fclose(f);
		}
	}
#endif

	if(desc->enable_vscpu_calc)
	{
		TIME_PROFILE_START("vscpu");
		ESSL_CHECK(_essl_move_vertex_calculations_to_cpu(pool, tu));
		TIME_PROFILE_STOP("vscpu");
	}

#ifdef DEBUGPRINT
	{
		char *file_name = "ast_ir1.txt";
		FILE *f = 0;
	
		f = fopen(file_name, "w");
		if(f != NULL)
		{
			ESSL_CHECK(ast_translation_unit_to_txt(f, pool, tu, desc));
			fflush(f);
			fclose(f);
		}
	}
#endif

	ESSL_CHECK(_essl_make_basic_blocks_init(&make_bb_ctx, err, ts_ctx, pool, desc));

	if(opts != NULL && opts->optimise_inline_functions) {
		ESSL_CHECK(_essl_optimise_inline_functions_init(&inline_functions_ctx, err, pool));
	}

	TIME_PROFILE_START("complex_rets");
	ESSL_CHECK(_essl_eliminate_complex_returns(pool, tu));
	TIME_PROFILE_STOP("complex_rets");

	/* run the struct/matrix elimination phase */
	TIME_PROFILE_START("complex_ops");
	ESSL_CHECK(_essl_eliminate_complex_ops(pool, ts_ctx, tu));
	TIME_PROFILE_STOP("complex_ops");

	for(sl = tu->functions; sl != 0; sl = sl->next)
	{
		symbol *function = sl->sym;

		assert(function->body != 0);

		TIME_PROFILE_START("make_basic_blocks");
		ESSL_CHECK(_essl_make_basic_blocks(&make_bb_ctx, function));
		TIME_PROFILE_STOP("make_basic_blocks");

		TIME_PROFILE_START("dominance");
		ESSL_CHECK(_essl_compute_dominance_information(pool, function));
		TIME_PROFILE_STOP("dominance");
#ifdef DEBUGPRINT
		if(f0 != NULL)
		{
			ESSL_CHECK(lir_function_to_txt(f0, pool, function, desc));
		}
#endif

		TIME_PROFILE_START("ssa");
		ESSL_CHECK(_essl_ssa_transform(pool, desc, err, function));
		TIME_PROFILE_STOP("ssa");
		/* function inlining optimisation */
		if((opts && opts->optimise_inline_functions != 0)) {
			TIME_PROFILE_START("inline_functions");
			ESSL_CHECK(_essl_optimise_inline_functions(&inline_functions_ctx, function));
			ESSL_CHECK(_essl_compute_dominance_information(pool, function));
			TIME_PROFILE_STOP("inline_functions");
		}

		prev_sl = sl;
	}

#ifdef DEBUGPRINT
	{
		char *file_name = "ast_ir2.txt";
		FILE *f = 0;
	
		f = fopen(file_name, "w");
		ESSL_CHECK(lir_translation_unit_to_txt(f, pool, tu, desc));
		fflush(f);
		fclose(f);
	}
#endif

	if(opts != NULL && opts->optimise_inline_functions && desc->has_entry_point)
	{
		/* NOTE: This needs to be changed when not everything is inlined.
		   optimise_inline_functions does not know how to remove functions 
		   from the list of functions, so we nuke them all except for the entry point.
		 */
		tu->functions = prev_sl;
	}

#ifdef DEBUGPRINT
	fflush(f0);
	fclose(f0);
#endif

	return MEM_OK;
}
