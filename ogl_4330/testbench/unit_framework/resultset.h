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
 * Public header for ResultSet class.
 * This file contains the class which manages collation and reporting of results.
 */

#ifndef RESULTSET_H_
#define RESULTSET_H_

#include "mem.h"

/**
 *	Forward declarations
 */
struct tag_test;
struct tag_suite;

/**
 *	Description:
 *	Possible status for a single error.
 */
typedef enum
{
	result_status_skip  = -2,
	result_status_unknown = -1,

	result_status_pass	= 0,
	result_status_debug	= 1,
	result_status_info	= 2,
	result_status_warn	= 3,
	result_status_fail	= 4,
	result_status_fatal	= 5,

	/* Number of possible statuses */
	result_status_count
} result_status;

/**
 *	Description:
 *	Masks which can be combined to indicate interest in several different
 *	error statuses.
 */
typedef enum
{
	result_mask_pass	= (1<<result_status_pass),
	result_mask_debug	= (1<<result_status_debug),
	result_mask_info	= (1<<result_status_info),
	result_mask_warn	= (1<<result_status_warn),
	result_mask_fail	= (1<<result_status_fail),
	result_mask_fatal	= (1<<result_status_fatal),

	result_mask_summary	= (1<<result_status_count)
} result_mask;

/**
 *	Description:
 *	Represents a single test result.
 */
typedef struct tag_result
{
	struct tag_result*	next_result;
	const char*			suite_name;
	const char*			test_name;
	int					fixture_index;
	const char*			fixture_name;
	result_status		status;
	const char*			message;
} result;

/*
 * Description:
 * Manages results reported from tests.
 */

typedef struct tag_result_set
{
	result* first_result;
	result* last_result;

	mempool	pool;

	int tests_considered;
	int tests_passed;
	int suites_considered;
	int suites_passed;

	int message_counts[result_status_count];

} result_set;

#ifdef __cplusplus
extern "C" {
#endif

/**
 *	Description:
 *	Create and initialize a new, empty result set.
 */
result_set* create_result_set(mempool* pool);

/**
 *	Description:
 *	Remove the result set.
 */
void remove_result_set(result_set* results);

/**
 *	Description:
 *	Add a result to the set.
 */
void add_result(result_set* results, struct tag_suite* current_suite, struct tag_test* test_executed, int fixture_index, result_status status, const char* message);

/**
 * Description:
 * Clear all results from the set, but keep overall statistics
 * @return 0 on success, -1 otherwise.
 */
int clear_results(result_set* results);

/**
 *	Description:
 *	Print the results that match the supplied status mask, to stdout.
 *	Returns the status value of the most severe error encountered.
 */
int print_results(result_set* results, int status_mask);

/**
 *	Description:
 *	Print the result details that match the supplied status mask, to stdout.
 *	Returns the status value of the most severe error encountered.
 */
int print_result_details(result_set* results, int status_mask);

/**
 *	Description:
 *	Print the result summary if it matches the supplied status mask, to stdout.
 */
void print_result_summary(result_set* results, int status_mask);

#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /*RESULTSET_H_*/
