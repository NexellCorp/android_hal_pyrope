/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_macros.h>
#include "base_common_l2_instrumented.h"
#include "base/common/pp/base_common_pp_job.h"
#include "base/common/gp/base_common_gp_job.h"


void _mali_base_common_instrumented_l2_job_set_counters_gp(mali_gp_job_handle job_handle, u32 *perf_counters, u32 perf_counters_count)
{
	mali_gp_job *job = mali_gp_handle_to_job(job_handle);

	MALI_DEBUG_ASSERT_POINTER(job_handle);

	job->perf_ctx.l2_counter_id[0] = 0;
	job->perf_ctx.l2_counter_id[1] = 0;

	if (perf_counters_count > 0)
	{
		job->perf_ctx.l2_counter_id[0] = perf_counters[0];
		job->perf_ctx.l2_perf_counters_count = 1;
		if (perf_counters_count > 1)
		{
			job->perf_ctx.l2_counter_id[1] = perf_counters[1];
			job->perf_ctx.l2_perf_counters_count = 2;
		}
	}
}

void _mali_base_common_instrumented_l2_job_set_counters_pp(mali_pp_job_handle job_handle, u32 *perf_counters, u32 perf_counters_count)
{
	mali_pp_job *job = (mali_pp_job*)job_handle;

	MALI_DEBUG_ASSERT_POINTER(job_handle);

	job->perf_ctx.l2_counter_id[0] = 0;
	job->perf_ctx.l2_counter_id[1] = 0;

	if (perf_counters_count > 0)
	{
		job->perf_ctx.l2_counter_id[0] = perf_counters[0];
		job->perf_ctx.l2_perf_counters_count = 1;
		if (perf_counters_count > 1)
		{
			job->perf_ctx.l2_counter_id[1] = perf_counters[1];
			job->perf_ctx.l2_perf_counters_count = 2;
		}
	}
}

void _mali_base_common_instrumented_l2_job_get_data_gp(mali_gp_job_handle job_handle, u32 **perf_counters, u32* perf_counters_count, u32 **res)
{
	mali_gp_job *job = mali_gp_handle_to_job(job_handle);

	MALI_DEBUG_ASSERT_POINTER(job_handle);
	MALI_DEBUG_ASSERT_POINTER(perf_counters);
	MALI_DEBUG_ASSERT_POINTER(perf_counters_count);
	MALI_DEBUG_ASSERT_POINTER(res);

	*perf_counters = job->perf_ctx.l2_counter_id;
	*perf_counters_count = job->perf_ctx.l2_perf_counters_count;
	*res = job->perf_ctx.l2_perf_values;
}

void _mali_base_common_instrumented_l2_job_get_data_pp( mali_pp_job_handle job_handle, u32 **perf_counters, u32* perf_counters_count, u32 **res, u32 **raw_res, u32 *frame_split_nr)
{
	mali_pp_job *job = (mali_pp_job*)job_handle;

	MALI_DEBUG_ASSERT_POINTER(job_handle);
	MALI_DEBUG_ASSERT_POINTER(perf_counters);
	MALI_DEBUG_ASSERT_POINTER(perf_counters_count);
	MALI_DEBUG_ASSERT_POINTER(res);
	MALI_DEBUG_ASSERT_POINTER(raw_res);
	MALI_DEBUG_ASSERT_POINTER(frame_split_nr);

	*perf_counters = job->perf_ctx.l2_counter_id;
	*perf_counters_count = job->perf_ctx.l2_perf_counters_count;
	*res = job->perf_ctx.l2_perf_values;
	*raw_res = job->perf_ctx.l2_perf_values_raw;
	*frame_split_nr = job->perf_ctx.frame_split_nr;
}
