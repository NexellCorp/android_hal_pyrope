/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _BASE_TEST_PP_UTIL_H_
#define _BASE_TEST_PP_UTIL_H_

#include <base/mali_types.h>
#include <base/mali_runtime.h>
#include <base/mali_context.h>
#include <base/mali_debug.h>
#ifdef MALI_TEST_API
	#include <base/mali_test.h>
#endif
#include "../unit_framework/suite.h"

#include <mali_rsw.h>
#include <base/mali_memory.h>
#include <base/pp/mali_pp_job.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Type definitions needed for using MaliPP
 */
typedef u32 mali_vdb[16];
typedef u64 mali_pp_cmd;

#define ILLEGAL_PRIMITIVE 0
#define ILLEGAL_POLY_LIST 1
#define ILLEGAL_RSW 2
#define ILLEGAL_VDB 3

typedef struct pp_test_framework * pp_test_framework_handle;

typedef enum pp_test_framework_shader
{
	PPTF_SHADER_SIMPLE,
 	PPTF_SHADER_HEAVY
} pp_test_framework_shader;


/**
 * Allocates buffers needed for running jobs on MaliPP. Both cpu-mem and Mali-mem.
 * Same buffers freed by @see pp_test_framework_context_free
 * @note The context must be released with \a pp_test_framework_context_free when it will no longer be needed.
 * @param test_suite The test suite
 * @param ctxh The Base context
 * @param num_fbs Number of frame_buffers / jobs_indexes that we create data for
 * @param num_polys Number of triangles rendered per frame.
 * @return A context handle that can be used to create jobs, and verify their result. Null on error.
 */
pp_test_framework_handle pp_test_framework_context_allocate(suite* test_suite, mali_base_ctx_handle ctxh, u32 num_fbs, u32 num_polys, pp_test_framework_shader shader);

/**
 * Makes a job to run on MaliPP. @see pp_test_framework_context_allocate needs to be run before this function.
 * @param ctxh The Base context
 * @param job_array_index The job index tells which of the framebuffers in the context we render to.
 * @return The job handle.
 */
mali_pp_job_handle pp_test_framework_job_create(mali_base_ctx_handle ctxh, pp_test_framework_handle pptf_h, u32 job_array_index);
mali_pp_job_handle pp_test_framework_stream_job_create(mali_base_ctx_handle ctxh, pp_test_framework_handle pptf_h,
                                                       u32 job_array_index, mali_stream_handle stream);

/**
 * Release the memory and objects allocated for this context.
 * Frees memory allocated by \a pp_test_framework_context_allocate
 * @param num_jobs Number of jobs to cleanup for, must match the number in pp_test_framework_context_allocate.
 * @param ctxh The Base context
 */
void pp_test_framework_context_free(pp_test_framework_handle pptf);


/**
 * Checks the results of a spesific pp-job. Asserts and prints to screen if results are not as expected
 * @param test_suite The test suite, logs errors.
 * @param job_array_index The index of job to check. Checks the resulting framebuffer for this index.
 */
void pp_test_framework_check_job_result(suite* test_suite,  pp_test_framework_handle pptf_h, u32 job_array_index);

/**
 * Clears the results of a spesific index.
 * This makes it possible to afterwards create a new job for this index, run it, and verify its result.
 * @param test_suite The test suite, logs errors.
 * @param job_array_index The index of job to clear. Clears the resulting framebuffer for this index.
 */
void pp_test_framework_clear_job_result(suite* test_suite,  pp_test_framework_handle pptf_h, u32 job_array_index);

#ifdef __cplusplus
}
#endif

#endif /* _BASE_TEST_GP_UTIL_H_ */
