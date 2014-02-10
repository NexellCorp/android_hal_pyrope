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
 * Implementation for ResultSet class.
 * This file contains the class which manages collation and reporting of results.
 */

#include "resultset.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "suite.h"

/**
 *	Description:
 *	Names of the different statuses.
 */
static const char * status_names[] =
{
	"Pass",
	"Debug",
	"Info",
	"Warn",
	"Fail",
	"Fatal",
};

#ifdef __SYMBIAN32__
void SOS_DebugPrintf(const char * format,...);
#define LOGPRINTF(format) SOS_DebugPrintf format; printf format
#else
#define LOGPRINTF(format) do { printf format;   \
	fflush(stdout);                         \
	} while(0)
#endif

/**
 *	Description:
 *	Create and initialize a new result.
 */
static result* create_result(mempool* pool, suite* current_suite, test* test_executed, int fixture_index, result_status status, const char* message)
{
	result* new_result = (result*)_test_mempool_alloc(pool,sizeof(result));
	if(new_result == NULL) return NULL;

	new_result->next_result = NULL;
	new_result->suite_name = current_suite->name;
	new_result->test_name = test_executed->name;
	new_result->fixture_index = fixture_index;
	new_result->fixture_name = NULL;
	new_result->status = status;
	new_result->message = message;

	/* Need to copy the name, as it is likely to be allocated on the fixture memory pool */
	if(current_suite->fixture_name!=NULL)
	{
		char* name = (char*)_test_mempool_alloc(pool,strlen(current_suite->fixture_name) + 1);
		strcpy(name, current_suite->fixture_name);
		new_result->fixture_name = name;
	}

	return new_result;
}

/**
 *	Description:
 *	Create and initialize a new, empty result set.
 */
result_set* create_result_set(mempool* pool)
{
	result_set* results = (result_set*)_test_mempool_alloc(pool,sizeof(result_set));
	if ( NULL == results )
	{
		LOGPRINTF(( "Failed to allocate memory for result set at %s:%i\n", __FILE__, __LINE__ ));
		return NULL;
	}

	results->first_result = NULL;
	results->last_result = NULL;

	if ( MEM_OK != _test_mempool_init(&results->pool, 1024) )
	{
		LOGPRINTF(( "Failed to allocate memory for result set at %s:%i\n", __FILE__, __LINE__ ));
		return NULL;
	}

	results->suites_considered = 0;
	results->suites_passed = 0;

	results->tests_considered = 0;
	results->tests_passed = 0;

	MEMSET(results->message_counts, 0, sizeof(results->message_counts));

	return results;
}

/**
 *	Description:
 *	Remove the result set.
 */
void remove_result_set(result_set* results)
{
	_test_mempool_destroy(&results->pool);
}

/**
 *	Description:
 *	Add a result to the set.
 */
void add_result(result_set* results, suite* current_suite, test* test_executed, int fixture_index, result_status status, const char* message)
{
	/* Create the new result */
	result* new_result = create_result(&results->pool, current_suite, test_executed, fixture_index, status, message);

	/* Add it to the result set */
	if(results->first_result == NULL)
	{
		results->first_result = new_result;
	}
	else
	{
		results->last_result->next_result = new_result;
	}
	results->last_result = new_result;

	if (status >= 0 && status < result_status_count)
	{
		results->message_counts[status]++;
	}
}

/**
 * Description:
 * Clear all results from the set, but keep overall statistics
 */
int clear_results(result_set* results)
{
	assert( results );
	if( _test_mempool_destroy(&results->pool) != MEM_OK ) return -1;
	if( _test_mempool_init(&results->pool, 1024) != MEM_OK ) return -1;
	results->first_result = NULL;
	results->last_result = NULL;
	return 0;
}

int print_result_details(result_set* results, int status_mask)
{
	int worst_error = result_status_pass;
	result* presult = NULL;

	if ( NULL == results ) return result_status_fatal;

	presult = results->first_result;

	while(presult != NULL)
	{
		if( ( (1<<presult->status) & status_mask) != 0)
		{
			if(presult->fixture_name == NULL)
			{
				LOGPRINTF(( "%s: %s.%s.%d - %s\n",
						status_names[presult->status],
						presult->suite_name,
						presult->test_name,
						presult->fixture_index,
						presult->message ));
			}
			else
			{
				LOGPRINTF(( "%s: %s.%s.%d (%s) - %s\n",
						status_names[presult->status],
						presult->suite_name,
						presult->test_name,
						presult->fixture_index,
						presult->fixture_name,
						presult->message ));
			}
		}

		if(presult->status > worst_error)
		{
			worst_error = presult->status;
		}

		presult = presult->next_result;
	}

	return worst_error;
}

#ifdef __SYMBIAN32__
#define PRINTFLUSH(format) SOS_DebugPrintf format; printf format; fflush(stdout)
#else
#define PRINTFLUSH(format) printf format; fflush(stdout)
#endif

void print_result_summary(result_set* results, int status_mask)
{
	if( (status_mask & result_mask_summary) != 0)
	{
		PRINTFLUSH(("-------------------------------------------------\n"));

		if(results->message_counts[result_status_warn] == 0 &&
		   results->message_counts[result_status_fail] == 0 &&
		   results->message_counts[result_status_fatal] == 0)
		{
			PRINTFLUSH(( "All assertions passed\n" ));
		} else {
			int i = 0;

			for(i = result_status_warn; i < result_status_count; i++)
			{
				if(results->message_counts[i]!=0)
				{
					PRINTFLUSH(("%d assertion%s %s\n", results->message_counts[i], (results->message_counts[i]==1?"":"s"), status_names[i] ));
				}
			}
		}

		PRINTFLUSH(("\n"));
		if(results->tests_passed == results->tests_considered)
		{
			PRINTFLUSH(("All %d tests passed\n", results->tests_passed));
		} else {
			PRINTFLUSH(("%d tests considered\n", results->tests_considered));
			PRINTFLUSH(("%d tests did not pass completely\n", results->tests_considered - results->tests_passed));
		}

		PRINTFLUSH(("\n"));
		if(results->suites_passed == results->suites_considered)
		{
			PRINTFLUSH(("All %d suites passed\n", results->suites_passed));
		} else {
			PRINTFLUSH(("%d suites considered\n", results->suites_considered));
			PRINTFLUSH(("%d suites did not pass completely\n", results->suites_considered - results->suites_passed));
		}
		PRINTFLUSH(("-------------------------------------------------\n"));
	}
}
#undef PRINTFLUSH

int print_results(result_set* results, int status_mask)
{
	int ret = print_result_details(results, status_mask);
	print_result_summary(results, status_mask);
	return ret;
}


