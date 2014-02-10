/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include "base/instrumented/mali_pp_instrumented_internal.h"
#include "base/common/instrumented/base_common_pp_instrumented.h"

MALI_EXPORT
void _mali_instrumented_pp_context_init(mali_pp_job_handle job_handle)
{
	_mali_base_common_instrumented_pp_context_init(job_handle);
}

MALI_EXPORT
void _mali_instrumented_pp_job_setup(mali_pp_job_handle job_handle, u32 *perf_counters, u32 perf_counters_count, u32 **res)
{
	_mali_base_common_instrumented_pp_job_setup(job_handle, perf_counters, perf_counters_count, res);
}

MALI_EXPORT
void _mali_instrumented_pp_job_get_data( mali_pp_job_handle job_handle, u32 *num_cores, u32 **perf_counters, u32 *perf_counters_count, u32 **res)
{
	_mali_base_common_instrumented_pp_job_get_data(job_handle, num_cores, perf_counters, perf_counters_count, res);
}

MALI_EXPORT
mali_bool _mali_instrumented_pp_is_job_instrumented(mali_pp_job_handle job_handle)
{
	return _mali_base_common_instrumented_pp_is_job_instrumented(job_handle);
}
