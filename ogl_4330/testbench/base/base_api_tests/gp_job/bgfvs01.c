/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010 ARM Limited
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

#include <base/mali_memory.h>
#include <base/gp/mali_gp_job.h>
#include <mali_gp_vs_cmd.h>
#include <mali_gp_plbu_cmd.h>

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

static void test_bgfvs01(suite* test_suite)
{
	mali_err_code err;
	mali_base_ctx_handle ctx;

	ctx = _mali_base_context_create();
	assert_fail((NULL != ctx), "ctx create failed");
	if(ctx != NULL)
	{
		err = _mali_gp_open(ctx);
		assert_fail((MALI_ERR_NO_ERROR == err), "Failed to open GP system");
		if(MALI_ERR_NO_ERROR == err)
		{
			_mali_gp_close(ctx);
		}
		_mali_base_context_destroy(ctx);
	}
}

/**
 *	Description:
 *	Remove the test fixture.
 *	Note that this is a static declaration, so that we do not have to change its name every time.
 */
static void remove_fixture(suite* test_suite) { }

suite* create_suite_bgfvs01(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "bgfvs01", create_fixture, remove_fixture, results );

	add_test(new_suite, "bgfvs01", test_bgfvs01);

	return new_suite;
}

