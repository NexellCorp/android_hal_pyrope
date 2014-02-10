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
#include "base_test_gp_util.h"

#include <base/mali_memory.h>
#include <base/gp/mali_gp_job.h>
#include <mali_gp_vs_cmd.h>
#include <mali_gp_vs_config.h>
#include <mali_gp_plbu_cmd.h>
#include <mali_gp_plbu_config.h>

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

static int num_callbacks;
static int jobs_failed_gp;

static mali_bool callback_gp(mali_base_ctx_handle ctx, void * cb_param, mali_job_completion_status completion_status, mali_gp_job_handle job_handle)
{
	num_callbacks++;
	if(MALI_JOB_STATUS_END_SUCCESS != completion_status) jobs_failed_gp++;
	MALI_DEBUG_PRINT(5, ("Callback from job nr %d\n", num_callbacks));
	return MALI_TRUE;
}

static void test_bgfvs04(suite* test_suite)
{
    mali_gp_job_handle jobh;
    mali_err_code err;
	mali_base_ctx_handle ctxh;
	mali_base_wait_handle wait_handle;

	jobs_failed_gp = 0;

	ctxh = _mali_base_context_create();
	assert_fail((NULL != ctxh), "ctx create failed");
	if(ctxh != NULL)
	{
		err = _mali_gp_open(ctxh);
		assert_fail((MALI_ERR_NO_ERROR == err), "Errorcode returned GP open");
		if(MALI_ERR_NO_ERROR == err)
		{
		    err = _mali_mem_open(ctxh);
			assert_fail((MALI_ERR_NO_ERROR == err), "Errorcode returned mem open");
			if(MALI_ERR_NO_ERROR == err)
			{

				err = allocate_vs_stuff(ctxh,1, test_suite);
				assert_fail((MALI_ERR_NO_ERROR == err), "Errorcode returned alloc vs stuff");
			    if(MALI_ERR_NO_ERROR == err)
				{
				    jobh = make_vs_job(test_suite, ctxh,0);
				    if(MALI_NO_HANDLE != jobh)
				    {

						wait_handle = _mali_gp_job_get_wait_handle(jobh);
						assert_fail((MALI_NO_HANDLE != wait_handle), "nullpointer");
						if(MALI_NO_HANDLE != wait_handle)
						{
						    _mali_gp_job_set_callback(jobh, callback_gp, NULL);

							num_callbacks = 0;
							jobs_failed_gp = 0;
							err = _mali_gp_job_start(jobh, MALI_JOB_PRI_HIGH);
							assert_fail((err == MALI_ERR_NO_ERROR), "job_start_failed");
							if(err == MALI_ERR_NO_ERROR)
							{
								_mali_wait_on_handle(wait_handle);

							}
							else
							{
								_mali_gp_job_free(jobh);
								_mali_wait_on_handle(wait_handle);
							}
							assert_ints_equal(num_callbacks, 1, "Did not recieve interrupt from started vertex shader job.");
							assert_ints_equal(check_vs_result(test_suite,0),0, "Output data from vertex shader wrong");
							assert_ints_equal(jobs_failed_gp, 0, "At least one job failed");
						}
						else
						{
							_mali_gp_job_free(jobh);
						}
				    }

					/* second run, using cmds_add */

					jobh = make_vs_job_one_add(test_suite, ctxh, 0);
				    if(MALI_NO_HANDLE != jobh)
				    {
						wait_handle = _mali_gp_job_get_wait_handle(jobh);
						assert_fail((MALI_NO_HANDLE != wait_handle), "nullpointer");
						if(MALI_NO_HANDLE != wait_handle)
						{
						    _mali_gp_job_set_callback(jobh, callback_gp, NULL);

							num_callbacks = 0;
							jobs_failed_gp = 0;
							err = _mali_gp_job_start(jobh, MALI_JOB_PRI_HIGH);

							assert_fail((err == MALI_ERR_NO_ERROR), "job_start_failed");
							if(err == MALI_ERR_NO_ERROR)
							{
								_mali_wait_on_handle(wait_handle);
							}
							else
							{
								_mali_gp_job_free(jobh);
								_mali_wait_on_handle(wait_handle);
							}
						    assert_ints_equal(num_callbacks, 1, "Did not recieve interrupt from started vertex shader job.");
							assert_ints_equal(check_vs_result(test_suite,0),0, "Output data from vertex shader wrong");
							assert_ints_equal(jobs_failed_gp, 0, "At least one job failed");
						}
						else
						{
							_mali_gp_job_free(jobh);
						}
				    }
					cleanup_after_vs(1);
				}
			    _mali_mem_close(ctxh);
			}
		    _mali_gp_close(ctxh);
		}
		_mali_base_context_destroy(ctxh);
	}
	assert_ints_equal(jobs_failed_gp, 0, "jobs did not finish OK");

}

/**
 *	Description:
 *	Remove the test fixture.
 *	Note that this is a static declaration, so that we do not have to change its name every time.
 */
static void remove_fixture(suite* test_suite) { }

suite* create_suite_bgfvs04(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "bgfvs04", create_fixture, remove_fixture, results );

	add_test(new_suite, "bgfvs04", test_bgfvs04);

	return new_suite;
}
