/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "base/instrumented/mali_gp_instrumented_internal.h"
#include "base/common/instrumented/base_common_gp_instrumented.h"

MALI_EXPORT
void _mali_instrumented_gp_context_init(mali_gp_job_handle job_handle)
{
	_mali_base_common_instrumented_gp_context_init(job_handle);
}

MALI_EXPORT
void _mali_instrumented_gp_job_set_counters( mali_gp_job_handle job_handle,
		u32 *perf_counters, u32 perf_counters_count)
{
	_mali_base_common_instrumented_gp_job_set_counters(job_handle, perf_counters, perf_counters_count);
}

MALI_EXPORT
void _mali_instrumented_gp_job_get_counters( mali_gp_job_handle job_handle,
		u32 **perf_counters, u32* perf_counters_count)
{
	_mali_base_common_instrumented_gp_job_get_counters(job_handle, perf_counters, perf_counters_count);
}

MALI_EXPORT
u32* _mali_instrumented_gp_get_results_pointer(
		mali_gp_job_handle job_handle)
{
	return _mali_base_common_instrumented_gp_get_results_pointer( job_handle);
}

MALI_EXPORT
void _mali_instrumented_gp_set_results_pointer( mali_gp_job_handle job_handle,
		u32 *res)
{
	_mali_base_common_instrumented_gp_set_results_pointer( job_handle, res);
}

MALI_EXPORT
void _mali_instrumented_gp_set_pointer_array_handle(
			mali_gp_job_handle job_handle,
			mali_mem_handle pointer_array_handle)
{
	_mali_base_common_instrumented_gp_set_pointer_array_handle(job_handle, pointer_array_handle);
}

MALI_EXPORT
void _mali_instrumented_gp_set_plbu_stack_handle(
			mali_gp_job_handle job_handle,
			mali_mem_handle plbu_stack_handle)
{
	_mali_base_common_instrumented_gp_set_plbu_stack_handle(job_handle, plbu_stack_handle);
}

MALI_EXPORT
mali_bool _mali_instrumented_gp_is_job_instrumented(
		mali_gp_job_handle job_handle)
{
	return _mali_base_common_instrumented_gp_is_job_instrumented(job_handle);
}
