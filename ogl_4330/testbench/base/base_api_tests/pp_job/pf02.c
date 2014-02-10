/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */ 

#include <base/mali_types.h>
#include <base/mali_runtime.h>
#include <base/mali_context.h>
#include <base/mali_debug.h>
#ifdef MALI_TEST_API
	#include <base/mali_test.h>
#endif
#include "../unit_framework/suite.h"
#include "base_test_pp_util.h"

#include <base/mali_memory.h>
#include <base/pp/mali_pp_job.h>

/**
 *	The test fixture.
 *	Note that this is not visible outside this file, so we need not change its name every time.
 */
typedef struct
{
	int dummy;	
} fixture;
 
/**
 *	Description:
 *	Create the test fixture.
 *	Note that this is a static declaration, so that we do not have to change its name every time.
 */
static void* create_fixture(suite* test_suite)
{
	mempool* pool = &test_suite->fixture_pool;
	fixture* fix = (fixture*)_test_mempool_alloc(pool,sizeof(fixture));
	return fix;
}

static u32 finished_pp_jobs;

static void callback_pp(mali_base_ctx_handle ctx, void * cb_param, mali_job_completion_status status, mali_pp_job_handle job_handle)
{
	if (MALI_JOB_STATUS_END_SUCCESS == status) finished_pp_jobs++;
}

void test_pf02(suite* test_suite)
{
	mali_base_ctx_handle ctxh;
	mali_base_wait_handle wait_handle;
    mali_err_code err;
	mali_pp_job_handle ppjob;
	pp_test_framework_handle pptf;

	ctxh = _mali_base_context_create();
	assert_fail((NULL != ctxh), "ctx create failed");
	if(ctxh != NULL)
	{
		err = _mali_pp_open(ctxh);
		assert_fail((MALI_ERR_NO_ERROR == err), "Failed to open pp system");
		if(MALI_ERR_NO_ERROR == err)
			{	
			err = _mali_mem_open(ctxh);
			assert_fail((MALI_ERR_NO_ERROR == err), "Failed to open mem system");
			if(MALI_ERR_NO_ERROR == err)
			{
				
				pptf = pp_test_framework_context_allocate(test_suite, ctxh, 1, 1, PPTF_SHADER_SIMPLE);
				assert_fail((NULL != pptf), "Failed to allocate for pp job");
				if(NULL != pptf)
				{				
					ppjob = pp_test_framework_job_create(ctxh, pptf, 0);
					assert_fail((MALI_NO_HANDLE != ppjob), "failed to make job");
					if(MALI_NO_HANDLE != ppjob)
					{				
						wait_handle = _mali_pp_job_get_wait_handle(ppjob);
						assert_fail((NULL != wait_handle), "failed to get wait_handle");
						if(NULL !=wait_handle)
						{						
						    _mali_pp_job_set_callback(ppjob, callback_pp, NULL);
						  	finished_pp_jobs = 0;
						    _mali_pp_job_start(ppjob, MALI_JOB_PRI_HIGH, NULL);
						
							_mali_wait_on_handle(wait_handle);
						
							assert_ints_equal(finished_pp_jobs, 1, "Wrong number of pp_jobs finished");
						
							pp_test_framework_check_job_result(test_suite, pptf, 0);
						}
						else
						{
							_mali_pp_job_free(ppjob);
						}
					}
					pp_test_framework_context_free(pptf);
				}
				_mali_mem_close(ctxh);
			}
			_mali_pp_close(ctxh);
		}
		_mali_base_context_destroy(ctxh);
	}
}

/**
 *	Description:
 *	Remove the test fixture.
 *	Note that this is a static declaration, so that we do not have to change its name every time.
 */
static void remove_fixture(suite* test_suite) { }

suite* create_suite_pf02(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "pf02", create_fixture, remove_fixture, results );

	add_test(new_suite, "pf02", test_pf02);
	
	return new_suite;	
}
