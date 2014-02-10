/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_macros.h>
#include <regs/MALI200/mali200_core.h>
#include "base_common_pp_instrumented.h"
#include "base/common/pp/base_common_pp_job.h"
#include <base/arch/base_arch_runtime.h>
#include "mali_instrumented_context.h"
#ifdef MALI_DUMP_ENABLE
#include "base/common/dump/base_common_dump_pp_functions_frontend.h"
#endif

#define MALI_INSTRUMENTED_CHECK_ALL_PP_JOBS_ARE_INSTRUMENTED 0

void _mali_base_common_instrumented_pp_context_init(mali_pp_job_handle job_handle)
{
	mali_pp_job *job;
	mali_base_instrumented_pp_context* ctx;
	int i;

	job = (mali_pp_job*)job_handle;
	MALI_DEBUG_ASSERT_POINTER(job);

	ctx = &job->perf_ctx;
	MALI_DEBUG_ASSERT_POINTER(ctx);

	ctx->perf_counters_count = 0;
	for (i = 0; i < _MALI_PP_MAX_SUB_JOBS; i++)
	{
		ctx->perf_values[i] = NULL;
	}
#if !defined(USING_MALI200)
	ctx->l2_perf_counters_count = 0;
#endif
	for (i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; i++)
	{
		ctx->wbx_mem[i] = NULL;
	}
}

MALI_STATIC void read_perf_values(mali_base_ctx_handle ctx, void * cb_param, mali_job_completion_status job_stat, mali_pp_job_handle job_handle)
{
#if !defined(USING_MALI200)
	mali_pp_job *originaljob = (mali_pp_job*)cb_param;
	mali_pp_job *job = (mali_pp_job*)job_handle;

	originaljob->perf_ctx.l2_perf_counters_count = job->perf_ctx.l2_perf_counters_count;
	originaljob->perf_ctx.l2_counter_id[0] = job->perf_ctx.l2_counter_id[0];
	originaljob->perf_ctx.l2_counter_id[1] = job->perf_ctx.l2_counter_id[1];
	originaljob->perf_ctx.l2_perf_values[0] = job->perf_ctx.l2_perf_values[0];
	originaljob->perf_ctx.l2_perf_values[1] = job->perf_ctx.l2_perf_values[1];
	originaljob->perf_ctx.l2_perf_values_raw[0] = job->perf_ctx.l2_perf_values_raw[0];
	originaljob->perf_ctx.l2_perf_values_raw[1] = job->perf_ctx.l2_perf_values_raw[1];
	MALI_IGNORE(job);
#else
	MALI_IGNORE(cb_param);
	MALI_IGNORE(job_handle);
#endif
	MALI_IGNORE(ctx);
	MALI_IGNORE(job_stat);
}

MALI_TEST_EXPORT
void _mali_base_common_instrumented_pp_job_setup(mali_pp_job_handle job_handle, u32 *perf_counters, u32 perf_counters_count, u32 **res)
{
	u32 i;
	mali_pp_job *job;
	job = (mali_pp_job *)job_handle;

	MALI_DEBUG_ASSERT_POINTER(job);

	/* perf_counters can be NULL if perf_counters_count is zero. */
	MALI_DEBUG_ASSERT( ! (NULL == perf_counters && perf_counters_count != 0) , ("perf_counters is a null-pointer but perf_counters_count is non-zero"));
	MALI_DEBUG_ASSERT( ! (NULL == res && perf_counters_count != 0) , ("res is a null-pointer but perf_counters_count is non-zero"));

	job->perf_ctx.perf_counters = perf_counters;
	job->perf_ctx.perf_counters_count = perf_counters_count;
	for (i = 0; i < _MALI_PP_MAX_SUB_JOBS; i++)
	{
		job->perf_ctx.perf_values[i] = res[i];
	}
}

MALI_TEST_EXPORT
void _mali_base_common_instrumented_pp_job_get_data( mali_pp_job_handle job_handle, u32 *num_cores, u32 **perf_counters, u32 *perf_counters_count, u32 **res)
{
	u32 i;
	mali_pp_job* job = (mali_pp_job*)job_handle;

	MALI_DEBUG_ASSERT_POINTER(job);
	MALI_DEBUG_ASSERT_POINTER(num_cores);
	MALI_DEBUG_ASSERT_POINTER(perf_counters);
	MALI_DEBUG_ASSERT_POINTER(perf_counters_count);
	MALI_DEBUG_ASSERT_POINTER(res);

	*num_cores = job->num_cores;
	*perf_counters = job->perf_ctx.perf_counters;
	*perf_counters_count = job->perf_ctx.perf_counters_count;

	for (i = 0; i < _MALI_PP_MAX_SUB_JOBS; i++)
	{
	   	   res[i] = job->perf_ctx.perf_values[i];
	}
}

void _mali_base_common_instrumented_pp_job_start(mali_pp_job_handle job_handle, mali_job_priority priority, mali_fence_handle *fence)
{
	mali_pp_job * job;
	u32 i;
	u32 j;
	mali_pp_registers *regs;
	void* wbx_mem[MALI200_WRITEBACK_UNIT_COUNT] = { NULL, };
	u32   wbx_size[MALI200_WRITEBACK_UNIT_COUNT] = { 0, };
	mali_bool must_run = MALI_TRUE;

	if (NULL == _instrumented_get_context())
	{
		/* Instrumentation is not active, so we enforce normal job start behavior */
		_mali_base_common_pp_job_start(job_handle, priority, fence);
		return;
	}

	/* Sync fences not supported with instrumented. */
	if (NULL != fence) *fence = MALI_FENCE_INVALID_HANDLE;

	MALI_DEBUG_ASSERT_POINTER(job_handle);

#if MALI_INSTRUMENTED_CHECK_ALL_PP_JOBS_ARE_INSTRUMENTED
	MALI_DEBUG_ASSERT(MALI_TRUE == _mali_base_common_instrumented_pp_is_job_instrumented(job_handle), ("Job is not setup for instrumentation"));
#endif

	job = (mali_pp_job *)job_handle;

	/*
	 * Do not try to be smart and enforce normal behavior if we have two or less counters.
	 * This will cause problems if the frame builder callback tries to start new GP jobs in
	 * its callback (due to CMU). It will deadlock because the GP way of starting jobs will
	 * then wait for the job to complete in the callback thread.
	 */

	if (2 <= job->perf_ctx.perf_counters_count)
	{
		/*
		 * Take a copy of the write back buffer, incase this is also used as read back.
		 * If we read and write to the same buffer, running the job many times will
		 * only make a mess of the result.
		 * Incremental rendering could trigger this situation.
		 */

		for (i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; i++)
		{
			if (MALI_NO_HANDLE != job->perf_ctx.wbx_mem[i])
			{
				wbx_size[i] = _mali_mem_size_get(job->perf_ctx.wbx_mem[i]);
				wbx_mem[i] = _mali_sys_malloc(wbx_size[i]);
				if (NULL == wbx_mem[i])
				{
					u32 j;
					for (j = 0; j < i; j++)
					{
						_mali_sys_free(wbx_mem[j]);
					}

					_mali_base_common_pp_job_run_postprocessing(job, MALI_JOB_STATUS_END_UNKNOWN_ERR);
					return;
				}
				_mali_mem_read(wbx_mem[i], job->perf_ctx.wbx_mem[i], 0, wbx_size[i]);
			}
		}
	}

	for (i = 0; must_run || (i < job->perf_ctx.perf_counters_count); i += 2)
	{
		mali_pp_job *newjob;
		mali_base_wait_handle wait_handle;
		mali_pp_job_handle newjob_handle;

		must_run = MALI_FALSE;

		/* make a new job based on the original job */
		newjob_handle = _mali_base_common_pp_job_new(job->ctx, job->num_cores, job->stream);
		if (MALI_NO_HANDLE == newjob_handle)
		{
			for (i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; i++)
			{
				if (NULL != wbx_mem[i])
				{
					_mali_sys_free(wbx_mem[i]);
				}
			}

			_mali_base_common_pp_job_run_postprocessing(job, MALI_JOB_STATUS_END_UNKNOWN_ERR);
			return;
		}

		regs = &(((mali_pp_job *)newjob_handle)->registers);
		_mali_sys_memcpy_32(regs, &job->registers, sizeof(mali_pp_registers));

#ifdef MALI_DUMP_ENABLE
		if (i > 0)
		{
			/* Make sure we don't dump duplicate jobs */
			_mali_common_dump_pp_enable_dump(newjob_handle, MALI_FALSE);
		}
#endif

		newjob = (mali_pp_job*)newjob_handle;

		wait_handle = _mali_base_common_pp_job_get_wait_handle(newjob_handle);
		if (MALI_NO_HANDLE == wait_handle)
		{
			_mali_base_common_pp_job_free(newjob_handle);
			for (i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; i++)
			{
				if (NULL != wbx_mem[i])
				{
					_mali_sys_free(wbx_mem[i]);
				}
			}

			_mali_base_common_pp_job_run_postprocessing(job, MALI_JOB_STATUS_END_UNKNOWN_ERR);
			return;
		}

		/* set up the callback that will read back performance values */
		_mali_base_common_pp_job_set_callback(newjob_handle, read_perf_values, job);

		/* set the hardware registers that the results should be read from */
		if (i < job->perf_ctx.perf_counters_count)
		{
			newjob->perf_ctx.counter_id[0] = job->perf_ctx.perf_counters[i];

			for(j = 0; j < job->num_cores; j++)
			{
				newjob->perf_ctx.perf_values[j] = &(job->perf_ctx.perf_values[j][i]);
			}

			newjob->perf_ctx.perf_counters_count = 1;
			/* if there's one more job to read */
			if ( i + 1 < job->perf_ctx.perf_counters_count )
			{
				newjob->perf_ctx.counter_id[1] = job->perf_ctx.perf_counters[i+1];
				newjob->perf_ctx.perf_counters_count = 2;
			}
		}

#if !defined(USING_MALI200)
		newjob->perf_ctx.l2_perf_counters_count = job->perf_ctx.l2_perf_counters_count;
		newjob->perf_ctx.l2_counter_id[0] = job->perf_ctx.l2_counter_id[0];
		newjob->perf_ctx.l2_counter_id[1] = job->perf_ctx.l2_counter_id[1];
#endif

		if (0 != i)
		{
			u32 ii;
			/* restore render target buffers in case there is some readback+writeback to same buffer */
			for (ii = 0; ii < MALI200_WRITEBACK_UNIT_COUNT; ii++)
			{
				if (MALI_NO_HANDLE != job->perf_ctx.wbx_mem[ii])
				{
					_mali_mem_write(job->perf_ctx.wbx_mem[ii], 0, wbx_mem[ii], wbx_size[ii]);
				}
			}
		}

#if defined(USING_MALI450)
		_mali_sys_memcpy_32(&newjob->info, &job->info, sizeof(m450_pp_job_frame_info));
#endif /* defined(USING_MALI450) */

		/* then, start it */
		_mali_base_common_pp_job_start(newjob_handle, priority, NULL);

		/* make sure we don't have overlapping jobs, or otherwise they will share same stack on a multicore system */
		_mali_base_arch_sys_wait_handle_wait(wait_handle);
	}

	for (i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; i++)
	{
		if (NULL != wbx_mem[i])
		{
			_mali_sys_free(wbx_mem[i]);
		}
	}

	_mali_base_common_pp_job_run_postprocessing(job, MALI_JOB_STATUS_END_SUCCESS);
}

MALI_EXPORT
void _mali_base_common_instrumented_pp_job_set_wbx_mem(mali_pp_job_handle job_handle, u32 wb_unit, mali_mem_handle mem)
{
	MALI_DEBUG_ASSERT_POINTER(job_handle);
	MALI_DEBUG_ASSERT(wb_unit < MALI200_WRITEBACK_UNIT_COUNT, ("WB unit out of range"));

	((mali_pp_job*)job_handle)->perf_ctx.wbx_mem[wb_unit] = mem;
}

mali_bool _mali_base_common_instrumented_pp_is_job_instrumented(mali_pp_job_handle job_handle)
{
	mali_pp_job *job = (mali_pp_job*)job_handle;
	MALI_DEBUG_ASSERT_POINTER(job);

	if (job->perf_ctx.perf_counters_count > 0)
	{
		return MALI_TRUE;
	}

	return MALI_FALSE;
}
