/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _BASE_TEST_GP_UTIL_H_
#define _BASE_TEST_GP_UTIL_H_

#include <base/mali_types.h>
#include <base/mali_runtime.h>
#include <base/mali_context.h>
#include <base/mali_debug.h>
#ifdef MALI_TEST_API
	#include <base/mali_test.h>
#endif
#include "../unit_framework/suite.h"

#include <base/mali_memory.h>
#include <base/gp/mali_gp_job.h>
#include <mali_gp_vs_cmd.h>
#include <mali_gp_vs_config.h>
#include <mali_gp_plbu_cmd.h>
#include <mali_gp_plbu_config.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Setting up a dummy job to run on Mali GP
 * @param test_suite The test suite
 * @param ctxh The Base context
 * @param num_vs_cmd Number of dummy vertex shader commands to add to the job
 * @param num_plbu_cmd Number of dummy plbu commands to add to the job
 * @return The Mali GP job that has been made
 */
mali_gp_job_handle make_dummy_job_gp(
		suite* test_suite,
		mali_base_ctx_handle ctxh,
		u32 num_vs_cmds,
		u32 num_plbu_cmds);

/**
 * Allocating the memory needed for running one or more vertex shader jobs on MaliGP
 * @param ctxh The Base context
 * @param num_jobs Number of jobs you want to allocate data for
 * @param test_suite The test suite
 * @return An error code if some allocation failed
 */
mali_err_code allocate_vs_stuff(
		mali_base_ctx_handle ctxh,
		u32 num_jobs,
		suite* test_suite);

/**
 * Setting up a real job to run on Mali GP, copying data from one buffer to another,
 * using vertex shader instructions
 * @param test_suite The test suite
 * @param ctxh The Base context
 * @param jobnum The number of the job to make.
 * @return The Mali GP job that has been made
 */
mali_gp_job_handle make_vs_job(
		suite* test_suite,
		mali_base_ctx_handle ctxh,
		u32 jobnum);

/**
 * Same as the function above except adding all commands in one go in stead
 * of adding vs commands one by one.
 * @param test_suite The test suite
 * @param ctxh The Base context
 * @param jobnum The number of the job to make.
 * @return The Mali GP job that has been made
 */
mali_gp_job_handle make_vs_job_one_add(
		suite* test_suite,
		mali_base_ctx_handle ctxh,
		u32 jobnum);
/**
 * Checking the number
 * @param test_suite The test suite
 * @param jobnum Number of vs jobs we need to check the result of
 * @return Number of errors discovered.
 */
u32 check_vs_result(
		suite* test_suite,
		u32 jobnum);

/**
 * Cleaning up after vertex shader jobs have been run. Freeing all allocated memory.
 * @num_jobs Number of jobs to clean up for.
 */
void cleanup_after_vs(
		u32 num_jobs);
/**
 * Make a number of jobs with random priority and run them.
 * @num_jobs Number of jobs to run.
 * @cmdlist_length Commandlist length. 0 is random
 * @param test_suite The test suite
 * @return Number of jobs failed
 *
 */
u32 make_and_run_vs_dummyjobs_randompriority(
		u32 num_jobs,
		u32 cmdlist_length,
		suite* test_suite);

/**
 * Get the name of a priority
 * @pri priority as integer input
 * @return Priority as a string
 *
 */
const char * get_pri_name(int pri);

/**
 * Check a buffer of logged priorities.
 * @log buffer, each entry contains priority(16 highest bits),
 * and start nr(16lowest bits). Entry number in buffer is finish nr.
 * @logsize size of buffer
 * @test_suite The test suite
 * @return Number of errors.
 *
 */
u32 check_priority_log(
		u32 log[],
		u32 logsize,
		suite* test_suite);

/**
 * Function to set a specific value to a number of values in a buffer
 * @buf_nr
 * @value Value to set
 *
 */
void set_output_buffer(
		u32 buf_nr,
		u32 value);

/**
 * Function to reset the number of global callbacks.
 * Used in bgfplbu04 and bgfplbu05
 *
 */
void reset_global_cbs(void);

/**
 * Code common for running the tests bgfplbu04 and bgfplbu05
 * Make and run jobs.
 * @run_nr Number of times to run a test
 * @test_suite The test suite
 * @one_go If you are adding commands in one go
 * @return Error code
 */
mali_err_code common_for_bgfplbu04_and_bgfplbu05(
		u32 run_nr,
		suite* test_suite,
		u32 one_go);

/**
 * Code common for running the tests bgfplbu08 and bgfplbu09
 * Make and run jobs using plbu heap.
 * @polygon_ctr number of polygons to build
 * @heap_default Default heap size
 * @heap_max Maximum heap size
 * @test_suite The test suite
 * @growable_heap If heap can grow
 * @should_fail If running is expected to fail(heap out of mem)
 */
void common_bgfplbu08_bgfplbu09(
		u32 polygon_ctr,
		u32 heap_default,
		u32 heap_max,
		u32 heap_block,
		suite* test_suite,
		u32 growable_heap,
		u32 should_fail);

/**
 * Function to check result of plbu heap
 * @tile_list Mem-handle of the tile list
 * @heap_handle Mem-handle of the heap
 * @polygon_ctr number of polygons to build
 * @expected_result Expected values of the heap entries
 * @growable_heap If heap can grow
 * @return number of errors
 */
int check_plbu_heap_result(
		mali_mem_handle tile_list,
		mali_mem_handle heap_handle,
		u32 polygon_ctr,
		u64 * expected_result,
		u32 growable_heap);

/**
 * Code common for making and running jobs using plbu heap.
 * @ctxh The Base context handle
 * @test_suite The test suite
 * @heap_handle Mem-handle for the heap
 * @polygon_ctr number of polygons to build
 * @vertex_array_handle Mem-handle for the vertex array
 * @vertex_index_handle Mem-handle for the vertex index
 * @pointer_array_handle Mem-handle for the poniter array
 * @stack Mem-handle for the stack
 * @isLast If this is the last job to run.
 */
mali_err_code run_plbu_heap_job(
		mali_base_ctx_handle ctxh,
		suite* test_suite,
		mali_mem_handle heap_handle,
		u32 polygon_ctr,
		mali_mem_handle vertex_array_handle,
		mali_mem_handle vertex_index_handle,
		mali_mem_handle pointer_array_handle,
		mali_mem_handle stack,
		int isLast);

#ifdef __cplusplus
}
#endif

#endif /* _BASE_TEST_GP_UTIL_H_ */
