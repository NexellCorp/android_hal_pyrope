/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/* Public definition of test harness executable. */

#include "main_suite.h"
#include "mem.h"

#include <base/mali_test.h>
#include <base/mali_runtime.h>

/*
 *	External declarations of suite factory functions. This is more compact than
 *	a separate .h file for each suite.
 */
extern suite* create_suite_mf01(mempool* pool, result_set* results);
extern suite* create_suite_mf11(mempool* pool, result_set* results);
extern suite* create_suite_bgfvs01(mempool* pool, result_set* results);
extern suite* create_suite_bgfvs04(mempool* pool, result_set* results);
extern suite* create_suite_bgfplbu04(mempool* pool, result_set* results);
extern suite* create_suite_pf01(mempool* pool, result_set* results);
extern suite* create_suite_pf02(mempool* pool, result_set* results);

/**
 *	Function which will create, run, and remove a single test suite.
 *
 *	\param results	A result set in which to store the results of the test.
 *	\param factory	A pointer to a factory function that creates the suite to run.
 *
 *	\return			Zero if the suite executed successfully (even if it had test failures),
 *					or non-zero if the suite terminated doe to a fatal error.
 */
static result_status run_suite(result_set* results, test_specifier* test_spec, suite* (*factory)(mempool* pool, result_set* results) )
{
	mempool pool;
	suite* new_suite;
	result_status status = result_status_unknown;

	if(MEM_OK != _test_mempool_init(&pool, 1024)) {
		return result_status_fatal;
	}

	new_suite = factory(&pool,results);
	if(NULL == new_suite) {
		_test_mempool_destroy(&pool);
		return result_status_fatal;
	}

	switch(test_spec->mode)
	{
		case MODE_EXECUTE:
			status = execute(new_suite, test_spec);
			break;

		case MODE_LIST_SUITES:
		case MODE_LIST_TESTS:
		case MODE_LIST_FIXTURES:
			list(new_suite,test_spec);
			break;
	}
	_test_mempool_destroy(&pool);

	return status;
}

/*
 *	Main test runner function.
 *	Full documentation in header.
 */

int run_main_suite(int mask, test_specifier* test_spec)
{
	int worst_error = 0;
	mempool pool;
	result_set* results = NULL;

	if(MEM_OK != _test_mempool_init(&pool, 1024)) {
		return result_status_fatal;
	}

	results = create_result_set(&pool);
	if(NULL == results) {
		_test_mempool_destroy(&pool);
		return result_status_fatal;
	}


	#if MALI_TEST_API
		_mali_test_mem_tracking_start();
	#endif

	run_suite(results, test_spec, create_suite_mf01);
	run_suite(results, test_spec, create_suite_mf11);
 	run_suite(results, test_spec, create_suite_bgfvs01);
	run_suite(results, test_spec, create_suite_bgfvs04);
	run_suite(results, test_spec, create_suite_bgfplbu04);
 	run_suite(results, test_spec, create_suite_pf01);
	run_suite(results, test_spec, create_suite_pf02);

	/* Print results, clean up, and exit */
	worst_error = print_results(results, mask);

	_test_mempool_destroy(&pool);

	#if MALI_TEST_API
		_mali_test_auto_term();
		_mali_test_mem_tracking_stop();
		_mali_test_mem_tracking_print_status();
	#endif

	return worst_error;
}

#include "unit_framework/framework_main.h"
int main(int argc, char **argv)
{
	return test_main(run_main_suite, NULL, argc, argv);
}
