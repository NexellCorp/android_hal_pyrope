/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*
 * Public header file for test suite.
 */

#ifndef SUITE_H
#define SUITE_H

#include "resultset.h"
#include "mem.h"
#ifndef WIN32_RI
	#include <mali_system.h>
#endif
#include <math.h>
#include <stdio.h>

/* Forward declarations */

struct tag_suite;

/**
 *  Flags for annotating and filtering tests
 */
#define FLAG_TEST_NONE                 0
#define FLAG_TEST_ALL                 (1 << 1)
#define FLAG_TEST_SMOKETEST           (1 << 2)
#define FLAG_TEST_PERFORMANCE         (1 << 3)
#define FLAG_TEST_DEPRECATED          (1 << 4)
#define FLAG_TEST_EXPECTED_FAILURE    (1 << 5)

/**
 *	Modes of execution
 */
typedef enum
{
	MODE_EXECUTE,
	MODE_LIST_SUITES,
	MODE_LIST_TESTS,
	MODE_LIST_FIXTURES
} test_mode;

/**
 *	Typedefs for function pointers
 */
typedef void  (*test_function  )(struct tag_suite* test_suite);
typedef void* (*create_function)(struct tag_suite* test_suite);
typedef void  (*remove_function)(struct tag_suite* test_suite);

/**
 *	Test specifier. This specifies a range of suites, tests and fixtures to use.
 */
typedef struct tag_test_specifier
{
	const char  *suite_name;		/**< Suite name to run, or NULL to run all suites */
	const char  *test_name;		/**< Test name to run, or NULL to run all tests */
	int         first_fixture;	/**< Index of first fixture to run */
	int	    last_fixture;	/**< Index of last fixture to run, or -1 for all fixtures */
	int     iterations;     /**<number of test suite iterations */

	int         first_test;		/**< Index of first test to run */
	int         last_test;		/**< Index of last test to run, or -1 for all tests */

	/*result_set* results;*/		/**< Result set to write results to. Common to all tests. */

	test_mode   mode;			/**< What to do with the tests */

	unsigned int exclude_flags;  /** Tests to exclude from execution.  Defaults to FLAG_TEST_NONE */
	unsigned int include_flags;  /** Tests to include in execution.  Defaults to FLAG_TEST_ALL */

	int         dump_images;    /**< Should we dump images - nonzero indicates yes */

	int         argc;			/**< Argument count from command line */
	char* const *argv;		/**< Arguments from command line */

	void        *api_parameters;
} test_specifier;

/**
 *	Structure representing a test in a list of tests.
 */
typedef struct tag_test
{
	struct tag_test* next_test;
	const char* name;
	test_function execute;
	const void* user_data;
	unsigned int flags;
} test;

/**
 *	Structure representing a suite of tests. A suite is a set of tests
 *	that all use the same test fixture. It is expected that a single C file
 *	will define a suite of tests and their fixture
 */
typedef struct tag_suite
{
	struct tag_suite*     next_suite;
	const char*           name;

	const test_specifier* test_spec;

	void*                 user_state;
	remove_function       remove_user_state;

	create_function       create_fixture;
	remove_function       remove_fixture;
	int                   fixture_variants;
	unsigned int		  api_specific_fixture_filters;


	mempool               fixture_pool;
	void*                 fixture;
	int                   fixture_index;
	const char*           fixture_name;

	mempool*              parent_pool;
	result_set*           results;

	test*                 first_test;
	test*                 last_test;
	test*                 this_test;

	result_status         status;
} suite;


/*
 *	Member functions for Suite.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 *	Create and Initialize a new test.
 */
test* create_test(mempool* pool, const char* name, test_function execute,
                  unsigned int test_flags, const void* user_data );

/**
 *	Create and Initialize a new suite.
 *	This starts off empty, with no tests, and a single fixture.
 */

suite* create_suite(mempool* pool, const char* name, create_function create_fixture, remove_function remove_fixture, result_set* results );

/**
 *	Tear down a suite.
 */
void remove_suite(suite* test_suite);

/**
 *	Get an argument value. The value must be passed into the program as
 *	a command line argument of the form --argname=value (no spaces). Note
 *	that an empty string is a valid argument value, and will override any
 *	defaults.
 *
 *	\param	test_spec		The test specifier which holds the arguments
 *	\param	argname			The argument name to find, case sensitive
 *	\param	default_value	The default value to return if not found.
 *	\return					The argument value, or default value if not found
 */
const char* get_arg(test_specifier* test_spec, const char* argname, const char* default_value);

/**
 *	Set suite state, and a function which will clear it up for us.
 */
void set_user_state(suite* test_suite, void* state_object, remove_function remove_state_function);

/**
 *	Set the number of fixture variants.
 */
void set_fixture_variants(suite* test_suite, int num_variants);

/**
 *	Set fixture name. This can be used to give additional cues to the user.
 */
void set_fixture_name(suite* test_suite, const char* name);

/**
 *	Add a test function to the suite (including user data)
 */
void add_test_with_user_data(suite* test_suite, const char* name,
                             test_function execute, const void* user_data);

/**
 *	Add a test function to the suite (including user data and filtering flags)
 */
void add_test_with_user_data_and_flags(suite* test_suite, const char* name,
                                       test_function execute, const void* user_data,
                                       unsigned int test_flags);

/**
 *	Add a test function to the suite
 */
void add_test(suite* test_suite, const char* name, test_function execute);

/**
 * Add a test with non-default filter flags
 */
void add_test_with_flags(suite* test_suite, const char* name, test_function execute,
                         unsigned int test_flags);

/**
 *	Execute all tests in the test suite.
 *	Any test that does not fail will implicitly pass.
 *
 *	\param	test_suite	The test suite to execute
 *	\param	test_spec	A specifier to restrict the range of tests run,
 *						or NULL to execute all tests.
 *	\return				The most severe error encountered.
 */
result_status execute(suite* test_suite, const test_specifier* test_spec);

/**
 *	List the test suite, optionally listing all the tests too.
 *	Output is sent to stdout, as a series of lines suitable for invoking
 *	the test program.
 *
 *	\param	test_suite	The test suite to list.
 *	\param	test_spec	A specifier to restrict the range of tests listed,
 *						or NULL to list all tests.
 */
void list(suite* test_suite, const test_specifier* test_spec);

/**
 *	Set the status of the current test with a message.
 *	Note: Pass is assumed; use pass only if you want to send a message too.
 */
void skip(suite* test_suite);
void pass(suite* test_suite, const char* message);
void debug(suite* test_suite, const char* message);
void info(suite* test_suite, const char* message);
void warn(suite* test_suite, const char* message);
void fail(suite* test_suite, const char* message);
void fatal(suite* test_suite, const char* message);
void assert_little_endian(suite* test_suite);

/**
 *	Assert macro for use inside test functions. We assume
 *	that there is a pointer to the test suite available called
 *	test_suite. We also assume that we want to return immediately
 *	if the test fails.
 */

#define assert_fail(condition, message) { if(!(condition)){fail(test_suite,(message)); }  }
#define assert_fatal(condition, message) { if(!(condition)){fatal(test_suite,(message)); return;} }

/**
 *	Assert macro for use inside test functions. We assume
 *	that there is a pointer to the test suite available called
 *	test_suite. We assume that we DO NOT want to return immediately
 *	if the test fails.
 */
#define assert_warn(condition, message) { if(!(condition)){warn(test_suite,(message));} }

/**
 *	Complex asserts.
 *	These will all set a status of "fail" if they fail
 */
#define assert_string_startswith(output,expected,value_name) assert_fail(strncmp((output), (expected), strlen((expected)))==0, dsprintf(test_suite, "%s returned: '%s', expected '%s'", (value_name), (output), (expected)))
#define assert_strings_equal(output,expected,value_name) assert_fail(strcmp((output), (expected))==0, dsprintf(test_suite, "%s returned: '%s', expected '%s'", (value_name), (output), (expected)))
#define assert_string_startswith(output,expected,value_name) assert_fail(strncmp((output), (expected), strlen((expected)))==0, dsprintf(test_suite, "%s returned: '%s', expected '%s'", (value_name), (output), (expected)))
#define assert_strings_equal_ws(output,expected,value_name) assert_fail(whitespace_strcmp((output), (expected))==0, dsprintf(test_suite, "%s returned: '%s', expected '%s'", (value_name), (output), (expected)))
#define assert_ints_equal(output,expected,value_name) assert_fail((output)==(expected), dsprintf(test_suite, "%s returned: '%d', expected '%d'", (value_name), (output), (expected)))
#define assert_long_longs_equal(output,expected,value_name) assert_fail((output)==(expected), dsprintf(test_suite, "%s returned: '%lld', expected '%lld'", (value_name), (output), (expected)))
#define assert_floats_equal(output,expected,epsilon,value_name) assert_fail(fabs((output)-(expected))<(epsilon), dsprintf(test_suite, "%s returned: '%g', expected '%g'", (value_name), (output), (expected)))
#define assert_mem_equal(output,expected,size,value_name) assert_fail(memcmp((output), (expected), (size))==0, dsprintf(test_suite, "%s returned: '%s', expected '%s'", (value_name), (output), (expected)))
#define assert_ints_less(output,expected,value_name) assert_fail((output)<(expected), dsprintf(test_suite, "%s returned: '%d', expected < '%d'", (value_name), (output), (expected)))
#define assert_ints_less_equal(output,expected,value_name) assert_fail((output)<=(expected), dsprintf(test_suite, "%s returned: '%d', expected <= '%d'", (value_name), (output), (expected)))
#define assert_ints_greater_equal(output,expected,value_name) assert_fail((output)>=(expected), dsprintf(test_suite, "%s returned: '%d', expected >= '%d'", (value_name), (output), (expected)))

/**
 *	Dynamically allocated sprintf. This returns a pointer to a dynamically allocated string
 *	in the test suite's results pool. This ensures that the string will not disappear
 *	before the output is presented.
 */
const char* dsprintf(suite* test_suite, const char* fmt, ...);

/**
 *	Compare strings, ignoring quantity of whitespace.
 *	Conceptually equivalent to taking the two strings, and for each
 *	replacing all contiguous strings of whitespace characters with a single
 *	space, then calling strcmp. Ignores trailing and leading whitespace.
 *
 *	\param s1	Left hand string to compare
 *	\param s2	Right hand string to compare
 *	\return		Negative value if s1<s2, zero if equal, positive if s2>s1
 */
int whitespace_strcmp(const char* s1, const char* s2);

/**
 *  Print framework usage
 */
void print_framework_usage( void );


#ifdef WIN32_RI
  #define USE_MALI_SYS_FUNCS_TO_ALLOW_LEAK_TESTING	0
  #define _mali_sys_snprintf _snprintf
  #define MALI_FUNCTION __FUNCTION__
#else
  #ifdef __SYMBIAN32__
    /**
     * Symbian OS Base driver uses its own heap for memory allocation,
     * so we need to use _mali_sys_xxx functions to prevent USER 42 panic
     */
    #define USE_MALI_SYS_FUNCS_TO_ALLOW_LEAK_TESTING	1
  #else
    #define USE_MALI_SYS_FUNCS_TO_ALLOW_LEAK_TESTING	0
  #endif
#endif

#if USE_MALI_SYS_FUNCS_TO_ALLOW_LEAK_TESTING /*Enable this to check for memory leaks in the tests and framework in addition to the driver */
    #define MALLOC_FUNC _mali_sys_malloc
    #define CALLOC_FUNC _mali_sys_calloc
    #define REALLOC_FUNC _mali_sys_realloc
    #define FREE_FUNC   _mali_sys_free
    #define MEMSET_FUNC _mali_sys_memset
    #define MEMCPY_FUNC _mali_sys_memcpy
#else
#include <stdlib.h>
#include <string.h>
    #define MALLOC_FUNC malloc
    #define CALLOC_FUNC calloc
    #define REALLOC_FUNC realloc
    #define FREE_FUNC   free
    #define MEMSET_FUNC memset
    #define MEMCPY_FUNC memcpy
#endif

#define MALLOC(a) MALLOC_FUNC((a))
#define CALLOC(a,b) CALLOC_FUNC((a), (b))
#define REALLOC(a,b) REALLOC_FUNC((a), (b))
#define FREE(a) FREE_FUNC((a))
#define MEMSET(dst,val,size) MEMSET_FUNC((dst),(val),(size))
#define MEMCPY(dst,src,size) MEMCPY_FUNC((dst),(src),(size))

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SUITE_H */
