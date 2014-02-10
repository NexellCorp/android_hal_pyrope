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
#include "../unit_framework/suite.h"

#include <base/mali_memory.h>

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

static void test_mf01(suite* test_suite)
{
	mali_mem_handle mem;
	mali_base_ctx_handle ctxh;
	mali_err_code err;

	ctxh = _mali_base_context_create();
	assert_fail((NULL != ctxh), "ctx create failed");
	if(ctxh != NULL)
	{

		err = _mali_mem_open(ctxh);
		assert_fail((MALI_ERR_NO_ERROR == err), "Memory system could not be opened");
		if(MALI_ERR_NO_ERROR == err)
		{

			mem = _mali_mem_alloc(ctxh, 1, 0, MALI_PP_READ);
			assert_fail((MALI_NO_HANDLE != mem), "nullpointer from mem alloc");
			if(mem != MALI_NO_HANDLE)
			{
				_mali_mem_free(mem);

				mem = _mali_mem_alloc(ctxh, 256, 0, MALI_PP_READ);
				assert_fail((MALI_NO_HANDLE != mem), "nullpointer from mem alloc");
				if(mem != MALI_NO_HANDLE){
					_mali_mem_free(mem);
				}
			}
			_mali_mem_close(ctxh);
		}
		_mali_base_context_destroy(ctxh);
	}


	/* once again with a memleak */
	#if defined DEBUG
	printf("Should log a memleak : \n");
	#endif
	ctxh = _mali_base_context_create();
	assert_fail((NULL != ctxh), "ctx create failed");
	if(ctxh != NULL)
	{

		err = _mali_mem_open(ctxh);
		assert_fail((MALI_ERR_NO_ERROR == err), "Memory system could not be opened");
		if(MALI_ERR_NO_ERROR == err)
		{

			mem = _mali_mem_alloc(ctxh, 1, 0, MALI_PP_READ);
			assert_fail((MALI_NO_HANDLE != mem), "nullpointer from mem alloc");

			_mali_mem_close(ctxh);
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

suite* create_suite_mf01(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(pool, "mf01", create_fixture, remove_fixture, results );

	add_test(new_suite, "mf01", test_mf01);

	return new_suite;
}
