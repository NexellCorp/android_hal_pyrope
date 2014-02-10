/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*
 *	Member functions for Suite.
 */

#include "suite.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mem.h"

#if defined(__GNUC__) && defined(MALI_GCC_TRACING)
#include <base/mali_function_tracing.h>
#endif

#ifdef _MSC_VER
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#elif defined(__SYMBIAN32__)
/*
 * defined in bin\TechView\epoc32\include\stdapis\stdio.h:
 * IMPORT_C int	 snprintf(...)
 */
IMPORT_C int _mali_sys_vsnprintf(char *str, size_t size, const char *format, va_list ap);
#define vsnprintf _mali_sys_vsnprintf
#else
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
#endif

int test_number = 1; /* test number, used in --first-test and --last-test */

#ifdef __SYMBIAN32__
#ifdef DISPLAY_TEST_TIME
u32 egl_helper_get_time(u32 *min, u32 *sec);
#endif
void SOS_DebugPrintf(const char * format,...);
#define LOGPRINTF(format) SOS_DebugPrintf format; printf format
#else
#define LOGPRINTF(format) do { printf format;    \
	fflush(stdout);                         \
	} while(0)
#endif

/*
 *	Create and Initialize a new test.
 */
test* create_test(mempool* pool, const char* name, test_function execute,
                  unsigned int test_flags, const void* user_data )
{
	test* new_test = (test*)_test_mempool_alloc(pool,sizeof(test));
	if ( NULL == new_test )
	{
		LOGPRINTF(( "mempool allocation failed at %s:%i\n", __FILE__, __LINE__ ));
		return NULL;
	}

	new_test->next_test = NULL;
	new_test->name = name;
	new_test->execute = execute;
	new_test->user_data = user_data;
	new_test->flags = test_flags;

	return new_test;
}

/*
 *	Create and Initialize a new suite.
 */

suite* create_suite(mempool* pool, const char* name, create_function create_fixture,
                    remove_function remove_fixture, result_set* results )
{
	suite* new_suite = (suite*)_test_mempool_alloc(pool,sizeof(suite));
	if ( NULL == new_suite )
	{
		LOGPRINTF(( "mempool allocation failed at %s:%i\n", __FILE__, __LINE__ ));
		return NULL;
	}

	new_suite->next_suite = NULL;
	new_suite->name = name;

	/*new_suite->test_spec = test_spec;*/

	new_suite->user_state = NULL;
	new_suite->remove_user_state = NULL;

	new_suite->create_fixture = create_fixture;
	new_suite->remove_fixture = remove_fixture;
	new_suite->fixture_variants = 1;
	new_suite->api_specific_fixture_filters = 0;

	new_suite->fixture = NULL;
	new_suite->fixture_index = 0;
	new_suite->fixture_name = NULL;

	new_suite->parent_pool = pool;
	new_suite->results = results;

	new_suite->first_test = NULL;
	new_suite->last_test = NULL;

	new_suite->this_test = NULL;
	new_suite->status = result_status_unknown;

	return new_suite;
}

/*
 *	Tear down a suite.
 */
void remove_suite(suite* test_suite)
{
	if(test_suite->remove_user_state!=NULL)
		test_suite->remove_user_state(test_suite);
}

/*
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
const char* get_arg(test_specifier* test_spec, const char* argname, const char* default_value)
{
	int i=0;
	int argname_length = strlen(argname);

	for(;i<test_spec->argc;i++)
	{
		const char* arg = test_spec->argv[i];

		if(	arg!=NULL )
		{
			int arg_len = strlen(arg);
			if(arg_len >= argname_length+3 &&
				arg[0]=='-' &&
				arg[1]=='-' &&
				arg[argname_length+2]=='=' &&
				strncmp(arg+2, argname, argname_length) == 0 )
			{
				return arg + argname_length + 3;
			}
		}
	}
	return default_value;
}

/*
 *	Set suite state, and a function which will clear it up for us.
 */
void set_user_state(suite* test_suite, void* state_object,
                    remove_function remove_state_function)
{
	test_suite->user_state = state_object;
	test_suite->remove_user_state = remove_state_function;
}

/*
 *	Set the number of fixture variants.
 */
void set_fixture_variants(suite* test_suite, int num_variants)
{
	test_suite->fixture_variants = num_variants;
}

/*
 *	Set the name of the current fixture.
 */
void set_fixture_name(suite* test_suite, const char* name)
{
	test_suite->fixture_name = name;
}

/*
 *	Add a test function to the suite
 */
static void setup_new_test(suite* test_suite, const char* name,
                           test_function execute, unsigned int test_flags,
                           const void* user_data)
{
	/* Create the new test */
	test* new_test = create_test(test_suite->parent_pool, name, execute, test_flags, user_data);
	if ( NULL == new_test )
	{
		LOGPRINTF(( "Failed to create test at %s:%i!\n", __FILE__, __LINE__ ));
		return;
	}

	/* Add it to the test suite */
	if(test_suite->first_test == NULL)
	{
		test_suite->first_test = new_test;
	}
	else
	{
		test_suite->last_test->next_test = new_test;
	}
	test_suite->last_test = new_test;
}

void add_test_with_user_data(suite* test_suite, const char* name,
                             test_function execute, const void* user_data)
{
	setup_new_test(test_suite, name, execute, FLAG_TEST_ALL, user_data);
}

void add_test_with_user_data_and_flags(suite* test_suite, const char* name,
                                       test_function execute, const void* user_data,
                                       unsigned int test_flags)
{
	setup_new_test(test_suite, name, execute, test_flags, user_data);
}

/*
 *	Add a test function to the suite
 */
void add_test(suite* test_suite, const char* name, test_function execute)
{
	setup_new_test(test_suite, name, execute, FLAG_TEST_ALL, NULL );
}

void add_test_with_flags(suite* test_suite, const char* name, test_function execute,
                         unsigned int test_flags)
{
	setup_new_test(test_suite, name, execute, test_flags, NULL );
}

static void log_test_info(suite* test_suite)
{
#if defined(__SYMBIAN32__) && defined(DISPLAY_TEST_TIME)
	u32 min, sec;
	u32 hour = egl_helper_get_time(&min, &sec);
#endif
	if(test_suite->fixture_name == NULL)
	{
#if defined(__SYMBIAN32__) && defined(DISPLAY_TEST_TIME)
		LOGPRINTF(( "%02d:%02d:%02d Running test %s.%s\n", hour, min, sec, test_suite->name, test_suite->this_test->name ));
	} else {
		LOGPRINTF(( "%02d:%02d:%02d Running test %s.%s on fixture %s\n", hour, min, sec, test_suite->name, test_suite->this_test->name, test_suite->fixture_name ));
#else
		LOGPRINTF(( "Running test %s.%s\n", test_suite->name, test_suite->this_test->name ));
	} else {
		LOGPRINTF(( "Running test %s.%s on fixture %s\n", test_suite->name, test_suite->this_test->name, test_suite->fixture_name ));
#endif
	}
}

/*
 *	Execute all tests in the test suite, for each variant of the test fixture.
 *	Returns status of last suite executed - this is important in case
 *	it was a fatal error.
 */
result_status execute(suite* test_suite, const test_specifier* test_spec)
{
	int first_fixture = 0;
	int last_fixture = test_suite->fixture_variants - 1;
	result_status suite_status = result_status_pass;
	int suite_passed = 1; /* boolean, 0 = false, 1 = true */
	int suite_executed = 0; /* 1 if a test should be executed withing suite */

	assert(test_suite!=NULL);
	assert(test_spec!=NULL);
	test_suite->test_spec = test_spec;

	/* Do we have a restricted suite name? */
	if(test_spec->suite_name!=NULL)
	{
		if(strcmp(test_suite->name, test_spec->suite_name)!=0)
			return result_status_pass;
	}

	/* Do we have restricted fixture variants? */
	first_fixture = test_spec->first_fixture;

	if(test_spec->last_fixture >= 0)
		last_fixture = test_spec->last_fixture;

	/* Iterate over all the tests */
	test_suite->status = result_status_unknown;

	for(test_suite->this_test = test_suite->first_test;
		test_suite->this_test != NULL && test_suite->status != result_status_fatal;
		test_suite->this_test = test_suite->this_test->next_test, ++test_number)
	{
		/* Do we have a restricted suite name? */
		if(test_spec->test_name!=NULL)
		{
			if(strcmp(test_suite->this_test->name, test_spec->test_name)!=0)
				continue;
		}

		if ( test_spec->first_test > 0 && test_number < test_spec->first_test ) continue;
		if ( test_spec->last_test > 0 && test_number > test_spec->last_test ) continue;

		/* Check to see if we are supposed to include this test */
		if ( ( 0 == (test_spec->include_flags & test_suite->this_test->flags) ) ||
		     (0 != (test_spec->exclude_flags & test_suite->this_test->flags) ) )
		{
			continue;
		}

		if (!suite_executed)
		{
			/* Count the suite in resultset as "considered" */
			test_suite->results->suites_considered++;
			suite_executed = 1;
		}

		/* For each test, iterate over all the fixture variants */
		for(test_suite->fixture_index = first_fixture;
			test_suite->fixture_index <= last_fixture;
			test_suite->fixture_index++)
		{
			test_suite->status = result_status_unknown;

			/* Create the fixture memory pool and the fixture */
			if ( MEM_OK != _test_mempool_init(&test_suite->fixture_pool, 0) )
				return result_status_fatal;

			if( test_suite->create_fixture != NULL)
			{
				test_suite->fixture = test_suite->create_fixture(test_suite);
				if( test_suite->fixture == NULL )
				{
					/* Fixture creation failed. Exit the fixture loop and go on to next test. */
					_test_mempool_destroy(&test_suite->fixture_pool);
					continue;
				}
			}

			/* Ready to run, so update the number of tests considered and log which test/fixture will be run. */
			test_suite->results->tests_considered++;
			log_test_info(test_suite);

			/* Execute the tests */
			test_suite->this_test->execute(test_suite);

#if defined(__GNUC__) && defined(MALI_GCC_TRACING)
			{
				char dump_file[512];
				_mali_sys_snprintf(dump_file, 512, "gcc_tracing_%s_%s.trace", test_suite->name, test_suite->this_test->name);
				_mali_sys_printf("Dumping to %s\n", dump_file);
				_mali_sys_trace_dump_last_64k(dump_file);
			}
#endif

			/* After executing, remove the fixture and kill the fixture memory pool */
			if(test_suite->remove_fixture != NULL)
				test_suite->remove_fixture(test_suite);
			_test_mempool_destroy(&test_suite->fixture_pool);
			test_suite->fixture = NULL;

			if(test_suite->status == result_status_unknown)
			{
				/* No explicit status set, assume it's a pass */
				add_result(test_suite->results, test_suite, test_suite->this_test, test_suite->fixture_index, result_status_pass, "no message");
				test_suite->results->tests_passed++;
			}
			else if (test_suite->status == result_status_pass ||
					 test_suite->status == result_status_debug ||
					 test_suite->status == result_status_info ||
					 test_suite->status == result_status_warn)
			{
				test_suite->results->tests_passed++;
			}
			else if(test_suite->status == result_status_fatal)
			{
				/* Fatal error - halt everything! */
				return result_status_fatal;
			}
			else if (test_suite->status == result_status_fail)
			{
				suite_status = result_status_fail;
				suite_passed = 0;
			} /* else if (test_suite->status == result_status_skip) {} */
		}
	}

	/* Count the suite as "passed" if all tests passed (and we actually did run some tests within this suite) */
	if (suite_executed && suite_passed)
	{
		test_suite->results->suites_passed++;
	}

	/* No fatal errors - return a "pass" status */
	return suite_status;
}

/*
 *	List the test suite, optionally listing all the tests too.
 *	Output is sent to stdout, as a series of lines suitable for invoking
 *	the test program.
 *
 *	\param	test_suite	The test suite to list.
 *	\param	test_spec	A specifier to restrict the range of tests listed.
 */
void list(suite* test_suite, const test_specifier* test_spec)
{
	int first_fixture = 0;
	int last_fixture = test_suite->fixture_variants - 1;

	const char* cmd = test_spec->argv[0];
	char *opt_images;

	assert(test_spec!=NULL);

	/* Do we have a restricted suite name? */
	if(test_spec->suite_name!=NULL)
	{
		if(strcmp(test_suite->name, test_spec->suite_name)!=0)
			return;
	}

	/* Pass on image dumping choice */
	opt_images = (test_spec->dump_images) ? " --dump-images" : "";


	if(test_spec->mode == MODE_LIST_SUITES)
	{
		/*
		 * Just output the suite name
		 * NOTE: suite name may contain spaces
		 */
		printf("%s --suite=\"%s\"%s\n", cmd, test_suite->name, opt_images);
		return;
	}

	/* Do we have restricted fixture variants? */
	first_fixture = test_spec->first_fixture;

	if(test_spec->last_fixture >= 0)
		last_fixture = test_spec->last_fixture;

	/* Iterate over all the tests */

	test_suite->status = result_status_unknown;

	for(test_suite->this_test = test_suite->first_test;
		test_suite->this_test != NULL && test_suite->status != result_status_fatal;
		test_suite->this_test = test_suite->this_test->next_test)
	{
		/* Do we have a restricted test name? */
		if(test_spec->test_name!=NULL)
		{
			if(strcmp(test_suite->this_test->name, test_spec->test_name)!=0)
				continue;
		}

		if(test_spec->mode == MODE_LIST_TESTS)
		{
			/*
			 * Just output the suite name
			 * NOTE: suite/test name may contain spaces
			 */
			printf("%s --suite=\"%s\" --test=\"%s\"%s\n", cmd, test_suite->name, test_suite->this_test->name, opt_images);
			continue;
		}

		/* For each test, iterate over all the fixture variants */

		for(test_suite->fixture_index = first_fixture;
			test_suite->fixture_index <= last_fixture;
			test_suite->fixture_index++)
		{

			/*
			 * Output the fully-qualified suite and test name
			 * NOTE: suite/test name may contain spaces
			 */
			printf("%s --suite=\"%s\" --test=\"%s\" --fixture=%d%s\n", cmd, test_suite->name, test_suite->this_test->name, test_suite->fixture_index, opt_images);
		}
	}
}

/*
 *	Skip the current test.
 */
void skip(suite* test_suite)
{
	test_suite->status = result_status_skip;
}

/*
 *	Fail the current test with a message.
 */
void debug(suite* test_suite, const char* message)
{
	if (test_suite->status < result_status_debug)
	{
		test_suite->status = result_status_debug;
	}
	add_result(test_suite->results, test_suite, test_suite->this_test, test_suite->fixture_index, result_status_debug, message);
}

/*
 *	Pass the current test with a message.
 */
void pass(suite* test_suite, const char* message)
{
	if (test_suite->status < result_status_pass)
	{
		test_suite->status = result_status_pass;
	}
	add_result(test_suite->results, test_suite, test_suite->this_test, test_suite->fixture_index, test_suite->status, message);
}

/*
 *	Fail the current test with a message.
 */
void info(suite* test_suite, const char* message)
{
	if (test_suite->status < result_status_info)
	{
		test_suite->status = result_status_info;
	}
	add_result(test_suite->results, test_suite, test_suite->this_test, test_suite->fixture_index, result_status_info, message);
}

/*
 *	Fail the current test with a message.
 */
void warn(suite* test_suite, const char* message)
{
	if (test_suite->status < result_status_warn)
	{
		test_suite->status = result_status_warn;
	}
	add_result(test_suite->results, test_suite, test_suite->this_test, test_suite->fixture_index, result_status_warn, message);
}

/*
 *	Fail the current test with a message.
 */
void fail(suite* test_suite, const char* message)
{
	if (test_suite->status < result_status_fail)
	{
		test_suite->status = result_status_fail;
	}
	add_result(test_suite->results, test_suite, test_suite->this_test, test_suite->fixture_index, result_status_fail, message);
}
/*
 *	Fail the current test with a message.
 */
void fatal(suite* test_suite, const char* message)
{
	test_suite->status = result_status_fatal;
	add_result(test_suite->results, test_suite, test_suite->this_test, test_suite->fixture_index, result_status_fatal, message);
}

void assert_little_endian(suite* test_suite)
{
	unsigned int a = 1;
	if (((unsigned char*)&a)[0] != 1)
	{
		fatal(test_suite, "this test suite requires a little endian platform");
	}
}

/*
 *	Create a dynamically allocated string from formatted input
 */

const char* dsprintf(suite* test_suite, const char* fmt, ...)
{
	#define MAX_DSPRINTF_STRING_LENGTH 892

	char pad[MAX_DSPRINTF_STRING_LENGTH];
	va_list arglist;
	char* string = NULL;
	int len = 0;

	va_start(arglist, fmt);
	len = vsnprintf(pad, MAX_DSPRINTF_STRING_LENGTH, fmt, arglist);
	va_end(arglist);

	if(len < 0)
		len = MAX_DSPRINTF_STRING_LENGTH-1;

	string = (char*) _test_mempool_alloc(&test_suite->results->pool, len + 1 );
	if ( NULL == string )
	{
		LOGPRINTF(( "mempool allocation failed at %s:%i\n", __FILE__, __LINE__ ));
		return NULL;
	}

	if(string!=NULL)
	{
		va_start(arglist, fmt);
		if ( vsnprintf(string, len + 1, fmt, arglist) < 0 ) printf( "string truncated at %s:%i\n", __FILE__, __LINE__ );
		va_end(arglist);

		/* Ensure null termination */
		string[len] = 0;
	}

	return string;
}

/*
 *	Compare strings, ignoring quantity of whitespace.
 *	Conceptually equivalent to taking the two strings, and for each
 *	replacing all contiguous strings of whitespace characters with a single
 *	space, then calling strcmp.
 */
int whitespace_strcmp(const char* s1, const char* s2)
{
	const char* p1 = s1;
	const char* p2 = s2;
	char c1;
	char c2;

	if(s1==NULL && s2==NULL) return 0;
	if(s1==NULL) return -1;
	if(s2==NULL) return 1;

	c1 = *p1++;
	c2 = *p2++;

	/* Skip initial whitespace */
	while(isspace(c1)) c1 = *p1++;
	while(isspace(c2)) c2 = *p2++;

	while(c1!=0 && c2!=0)
	{
		if( c1!=c2 && !isspace(c1) && !isspace(c2) )
		{
			return(c2-c1);
		}

		/* Still equal up to this point. */

		if(isspace(c1))
		{
			/* This was a space - search forward to next nonspace character */
			while(isspace(c1)) { c1 = *p1++; } ;
		}
		else
		{
			/* Next character MAY be a space. */
			c1 = *p1++;
		}

		if(isspace(c2))
		{
			/* This was a space - search forward to next nonspace character */
			while(isspace(c2)) { c2 = *p2++; } ;
		}
		else
		{
			/* Next character MAY be a space. */
			c2 = *p2++;
		}
	}

	/* Skip trailing whitespace */
	if(c2==0) while(isspace(c1)) c1 = *p1++;
	if(c1==0) while(isspace(c2)) c2 = *p2++;

	return (c2-c1);
}
