/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2007, 2009-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef _MALI_PP_INSTRUMENTED_INTERNAL_H_
#define _MALI_PP_INSTRUMENTED_INTERNAL_H_

#include "mali_system.h"
#include "base/pp/mali_pp_handle.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Init/reset the instrumented context for the specified PP job
 *
 * @param job_handle PP job to init/reset instrumented context for
 */
MALI_IMPORT
void _mali_instrumented_pp_context_init(mali_pp_job_handle job_handle);

/**
 * Setting the array that tells which counters to read for a job and
 * where to put the result.
 *
 * @param job_handle            PP job handle
 * @param perf_counters         an array with the counter indices to read
 * @param perf_counters_count   the number of counters in the perf_counters array
 * @param res                   an array where the extracted counters should be put
 */
MALI_IMPORT
void _mali_instrumented_pp_job_setup(mali_pp_job_handle job_handle, u32 *perf_counters, u32 perf_counters_count, u32 **res);

/**
 * Get the array that tells which counters to read / has been read for a job,
 * and the resulting values
 *
 * The perf_counters array tells which counters on mali we will readout after
 * the job has run.
 *
 * @param job_handle            PP job handle
 * @param num_cores             number of PP cores used in the job i.e. number of PP subjobs
 * @param perf_counters         returns the array with the counter indices to read
 * @param perf_counters_count   returns the number of counters in the perf_counters array
 * @param res                   pointer to location where counter results/values is stored
 */
MALI_IMPORT
void _mali_instrumented_pp_job_get_data( mali_pp_job_handle job_handle, u32 *num_cores, u32 **perf_counters, u32 *perf_counters_count, u32 **res);
/**
 * Find out whether a PP job is instrumented.
 *
 * @param job_handle PP job to check for instrumentation
 * @return MALI_TRUE if job is instrumented, MALI_FALSE if not.
 */
MALI_IMPORT
mali_bool _mali_instrumented_pp_is_job_instrumented(mali_pp_job_handle job_handle);

#ifdef __cplusplus
}
#endif

#endif /* _MALI_PP_INSTRUMENTED_INTERNAL_H_  */
