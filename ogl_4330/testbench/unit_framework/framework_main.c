/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*
 * Public definition of test harness executable.
 */
#ifndef WIN32_RI
	#include <mali_system.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "framework_main.h"
#include "mem.h"
#include "backtrace.h"
#include "resultset.h"

/**
 *	Show usage message.
 */
void print_framework_usage(void)
{
	/**
	 * These puts calls have been split up so that the string length stays within C89 limits
	 * NOTE: Please use only spaces, not tab stops, to format these strings.
	 */
	puts(
		"\n"
		"This will create a new set of tests and run them.\n"
		"\n"
		"Arguments:\n"
		"\n"
		"    -q --quiet     No output - just set return status.\n"
		"    -v --verbose   Show all error messages (equivalent to -sfwid)\n"
		"    -s --summary   Show summary\n"
		"    -f --fail      Show individual failures\n"
		"    -w --warn      Show individual warnings\n"
		"    -i --info      Show info messages\n"
		"    -d --debug     Show debug messages\n"
		"    -p --pass      Show pass messages\n"
		"    -h --help      Show this help message\n"
		);
	puts(
		"       --error=n   Set the minimum reported error level to n (0 to 5)\n"
		"       --suite=s   Run only the suite named 's'\n"
		"       --test=t    Run only the test named 't'\n"
		"       --first-test=n1 --last-test=n2\n"
		"                   Run all tests numbered n1 to n2 inclusive\n"
		"       --iterations=n\n"
		"                   Run test suite n times (only supported by selected test suites)\n"
		"       --fixture=n Run only fixture number n\n"
		"       --first-fixture=n1 --last-fixture=n2\n"
		"                   Run all fixtures numbered n1 to n2 inclusive\n"
		);
	puts(
		"  Filtering options: \n\n"
		"       --include=filter\n"
		"                     Include tests that match the given filter:\n"
		"                       all [default], smoketest, performance, deprecated, expectedfailure, none\n"
		"                     If this option is omitted, all tests will be included,\n"
		"                     subject to other options\n"
		"       --exclude=filter \n"
		"                     Exclude tests that match the given filter:\n"
		"                       all, smoketest, performance, deprecated, expectedfailure, none [default]\n"
		"                     If this option is omitted, no tests will be excluded,\n"
		"                     subject to other options\n"
		);
	puts(
		"       --list-suites\n"
		"       --list-tests\n"
		"       --list-fixtures\n"
		"                   Instead of running the tests, list them. Each option\n"
		"                   lists at a different level of granularity. Each line\n"
		"                   of output is a fully-qualified command line to run\n"
		"                   that suite, test, or fixture.\n"
		);
	puts(
		"	Short options may be combined (e.g. -s -f can be specified as -sf).\n"
		"	If no arguments are given, shows summary, fail, and warning messages,\n"
		"	with error level 4 (i.e. only errors and fatal errors set the return\n"
		"	status to non-0), for all tests."
		);
}

typedef struct flag_mapping
{
	unsigned int flag;
	char * name;
} flag_mapping;

flag_mapping mapping_table[] =
{
	{ FLAG_TEST_NONE,             "none" },
	{ FLAG_TEST_ALL,              "all" },
	{ FLAG_TEST_SMOKETEST,        "smoketest" },
	{ FLAG_TEST_PERFORMANCE,      "performance" },
	{ FLAG_TEST_DEPRECATED,       "deprecated" },
	{ FLAG_TEST_EXPECTED_FAILURE, "expectedfailure" },
};

static unsigned int parse_test_flags(const char* flag_arg)
{
	unsigned int flags = FLAG_TEST_NONE;
	char* string = (char*)flag_arg;
	char* token = strtok(string, ",-");

	while (NULL != token)
	{
		unsigned int i;
		for ( i=0; i<sizeof(mapping_table)/sizeof(mapping_table[1]); i++)
		{
			if (0 == strcmp(token, mapping_table[i].name))
			{
				printf("%s ", mapping_table[i].name);
				flags |= mapping_table[i].flag;
			}
		}
		token = strtok(NULL, ",-");
	}

	return flags;
}

/**
 *	Main program.
 *	This will create a new set of tests and run them.
 *
 *	\param argc	Argument count
 *	\param argv Arguments from command line - see usage message for syntax
 *	\return		0 if no errors more severe than the warning level, else
 *				the maximum warning level encountered.
 */

int test_main( suite_func suitefunc, parse_func parsefunc, int argc, char **argv )
{
	int mask = result_mask_fatal;
	int use_default_mask = 1;
	int error = 0;
	int i=0;
	int min_error_level = 4;
	mempool parse_pool;
	test_specifier test_spec;

	if ( MEM_OK != _test_mempool_init( &parse_pool, 256 ) )
	{
		printf( "Failed to allocate mempool for parsing at %s:%i\n", __FILE__, __LINE__ );
		return result_status_fatal;
	}

	test_spec.api_parameters = NULL;
	test_spec.suite_name = NULL;
	test_spec.test_name = NULL;
	test_spec.first_fixture = 0;
	test_spec.last_fixture = -1;
	test_spec.first_test = 0;
	test_spec.last_test = -1;
	test_spec.iterations = 1;
	test_spec.argc = argc;
	test_spec.argv = argv;
	test_spec.mode = MODE_EXECUTE;
	test_spec.dump_images = 0;
	test_spec.include_flags = FLAG_TEST_ALL;
	test_spec.exclude_flags = FLAG_TEST_NONE;

#ifdef SUITE_ENABLE_BACKTRACE
	suite_enable_backtrace();
#endif

	/* init the api parameters (argument string == NULL) */
	if ( NULL != parsefunc && !parsefunc( &test_spec, &parse_pool, NULL ) )
	{
		return 6; /* was unable to create and/or init default api parameters */
	}

	for( i = 1; i < argc; i++ )
	{
		const char* string = argv[i];
		if(string[0]=='-')
		{
			if(string[1]=='-')
			{
				if(strcmp(string,"--quiet"	)==0)
				{
					mask=result_mask_fatal;
					use_default_mask = 0;
				}
				else
				if(strcmp(string,"--verbose")==0)
				{
					mask=-1;
					use_default_mask = 0;
				}
				else
				if(strcmp(string,"--summary")==0)
				{
					mask|=result_mask_summary;
					use_default_mask = 0;
				}
				else
				if(strcmp(string,"--fail"	)==0)
				{
					mask|=result_mask_fail;
					use_default_mask = 0;
				}
				else
				if(strcmp(string,"--warn"	)==0)
				{
					mask|=result_mask_warn;
					use_default_mask = 0;
				}
				else
				if(strcmp(string,"--info"	)==0)
				{
					mask|=result_mask_info;
					use_default_mask = 0;
				}
				else
				if(strcmp(string,"--debug"	)==0)
				{
					mask|=result_mask_debug;
					use_default_mask = 0;
				}
				else
				if(strcmp(string,"--pass"	)==0)
				{
					mask|=result_mask_pass;
					use_default_mask = 0;
				}
				else
				if(strncmp(string,"--iterations=",13 )==0)
				{
					test_spec.iterations = atoi(string+13);
				}
				else
				if(strcmp(string,"--list-suites")==0)
				{
					test_spec.mode = MODE_LIST_SUITES;
				}
				else
				if(strcmp(string,"--list-tests")==0)
				{
					test_spec.mode = MODE_LIST_TESTS;
				}
				else
				if(strcmp(string,"--list-fixtures")==0)
				{
					test_spec.mode = MODE_LIST_FIXTURES;
				}
				else
				if(strcmp(string,"--dump-images")==0)
				{
					test_spec.dump_images = 1;
				}
				else
				if(strncmp(string,"--error=",8)==0)
				{
					min_error_level = atoi(string+8);
				}
				else
				if(strncmp(string,"--fixture=",10)==0)
				{
					test_spec.first_fixture =
					test_spec.last_fixture = atoi(string+10);
				}
				else
				if(strncmp(string,"--first-fixture=",16)==0)
				{
					test_spec.first_fixture = atoi(string+16);
				}
				else
				if(strncmp(string,"--last-fixture=",15)==0)
				{
					test_spec.last_fixture = atoi(string+15);
				}
				else
				if(strncmp(string,"--first-test=",13)==0)
				{
					test_spec.first_test = atoi(string+13);
				}
				else
				if(strncmp(string,"--last-test=",12)==0)
				{
					test_spec.last_test = atoi(string+12);
				}
				else
				if(strncmp(string,"--suite=",8)==0)
				{
					test_spec.suite_name = string+8;
				}
				else
				if(strncmp(string,"--test=",7)==0)
				{
					test_spec.test_name = string+7;
				}
				else
				if(strncmp(string,"--include=",10)==0)
				{
					printf("Setting include flags: ");
					test_spec.include_flags = parse_test_flags(string+10);
					printf("\n");
				}
				else
				if(strncmp(string,"--exclude=",10)==0)
				{
					printf("Setting exclude flags: ");
					test_spec.exclude_flags = parse_test_flags(string+10);
					printf("\n");
				}
				else
				{
					if ( NULL == parsefunc )
					{
						/* Suite has no custom parse function, return unrecognized command error */
						print_framework_usage();
						return 5;
					}
					else
					{
						/* Check if current suite recognizes this parameter */
						if ( !parsefunc( &test_spec, &parse_pool, string ) )
						{
							/* Suite didn't recognize parameter either, return unrecognized command error */
							return 5;
						}
						/* Parameter was handled by suite parsefunc */
					}
				}
			}
			else
			{
				size_t j = strlen(string);
				while(j>1)
				{
					--j;

					use_default_mask = 0;

					switch(string[j])
					{
						case 'q':	mask=result_mask_fatal;		break;
						case 'v':	mask=-1;					break;
						case 's':	mask|=result_mask_summary;	break;
						case 'f':	mask|=result_mask_fail;		break;
						case 'w':	mask|=result_mask_warn;		break;
						case 'i':	mask|=result_mask_info;		break;
						case 'd':	mask|=result_mask_debug;	break;
						case 'p':	mask|=result_mask_pass;		break;

						default:
						{
							if ( NULL == parsefunc )
							{
								/* Suite has no custom parse function, return unrecognized command error */
								print_framework_usage();
								return 5;
							}
							else
							{
								/* Check if current suite recognizes this parameter */
								if ( !parsefunc( &test_spec, &parse_pool, string ) )
								{
									/* Suite didn't recognize parameter either, return unrecognized command error */
									return 5;
								}
								/* Parameter was handled by suite parsefunc */
							}
						}
					}
				}
			}
		}
		else
		{
			if ( NULL == parsefunc )
			{
				/* Suite has no custom parse function, return unrecognized command error */
				print_framework_usage();
				return 5;
			}
			else
			{
				/* Check if current suite recognizes this parameter */
				if ( !parsefunc( &test_spec, &parse_pool, string ) )
				{
					/* Suite didn't recognize parameter either, return unrecognized command error */
					return 5;
				}
				/* Parameter was handled by suite parsefunc */
			}
		}
	}

	if(use_default_mask)
		mask = ~(result_mask_pass|result_mask_debug|result_mask_info);

	error = suitefunc(mask, &test_spec);

	_test_mempool_destroy( &parse_pool );

	if(error < min_error_level)
		return 0;
	else
		return error;
}
