/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include "common/lir_pass_run_manager.h"
#include "common/essl_profiling.h"

#ifdef DEBUGPRINT
#include "common/lir_printer.h"
#endif

#ifdef DEBUGPRINT
static memerr dump_ir(pass_run_context *ctx, const char *const_pass_name)
{
	char file_name[40];
	FILE *f = 0;

	/* Dump LIR */
	snprintf(file_name, sizeof(file_name), "t%02d_%s.txt",
			 ctx->pass_nr, const_pass_name);
	f = fopen(file_name, "w");
	if(f == NULL)
	{
		return MEM_OK;
	}
	if(lir_translation_unit_to_txt(f, ctx->tmp_pool, ctx->tu, ctx->desc) == MEM_ERROR)
	{
		fclose(f);
		return MEM_ERROR;
	}
	fflush(f);
	fclose(f);
	return MEM_OK;
}
#endif

memerr _essl_run_lir_function_pass(pass_run_context *ctx, lir_func_pass pass, const char *const_pass_name)
{
	mempool tmp_pool;
	symbol_list *sl;

#if defined(TIME_PROFILING)
	char pass_name[40];
	snprintf(pass_name, sizeof(pass_name), "t%02d_%s",
			 ctx->pass_nr, const_pass_name);
#endif

	IGNORE_PARAM(const_pass_name);
	
	ESSL_CHECK(_essl_mempool_init(&tmp_pool, 0, _essl_mempool_get_tracker(ctx->pool)));
	ctx->tmp_pool = &tmp_pool;

	if(pass != NULL)
	{
		for(sl = ctx->tu->functions; sl != 0; sl = sl->next)
		{
			symbol *function = sl->sym;

			TIME_PROFILE_START(pass_name);
			if(pass(ctx, function) == MEM_ERROR)
			{
				_essl_mempool_destroy(&tmp_pool);
				return MEM_ERROR;
			}
			TIME_PROFILE_STOP(pass_name);
		}
	}
#ifdef DEBUGPRINT
	if(dump_ir(ctx, const_pass_name) == MEM_ERROR)
	{
		_essl_mempool_destroy(&tmp_pool);
		return MEM_ERROR;
	}
#endif
	
	ctx->tmp_pool = NULL;
	_essl_mempool_destroy(&tmp_pool);

	ctx->pass_nr++;
	
	return MEM_OK;
}

memerr _essl_run_lir_tu_pass(pass_run_context *ctx, lir_tu_pass pass, const char *const_pass_name)
{
	mempool tmp_pool;

#if defined(TIME_PROFILING)
	char pass_name[40];
	snprintf(pass_name, sizeof(pass_name), "t%02d_%s",
			 ctx->pass_nr, const_pass_name);
#endif

	IGNORE_PARAM(const_pass_name);

	ESSL_CHECK(_essl_mempool_init(&tmp_pool, 0, _essl_mempool_get_tracker(ctx->pool)));
	ctx->tmp_pool = &tmp_pool;

	if(pass != NULL)
	{
		TIME_PROFILE_START(pass_name);
		if(pass(ctx, ctx->tu) == MEM_ERROR)
		{
			_essl_mempool_destroy(&tmp_pool);
			return MEM_ERROR;
		}
		TIME_PROFILE_STOP(pass_name);
	}

#ifdef DEBUGPRINT
	if(dump_ir(ctx, const_pass_name) == MEM_ERROR)
	{
		_essl_mempool_destroy(&tmp_pool);
		return MEM_ERROR;
	}
#endif
	ctx->tmp_pool = NULL;
	_essl_mempool_destroy(&tmp_pool);

	ctx->pass_nr++;

	return MEM_OK;
}
