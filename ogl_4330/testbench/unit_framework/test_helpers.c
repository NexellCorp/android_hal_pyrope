/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007, 2009-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include "test_helpers.h"
#include <string.h>

void assert_string_starts_with(suite* test_suite, const char *str, const char *ref, const char *file, int line)
{

	assert_fail(
		NULL != str,
		dsprintf(test_suite, "string is NULL (%s:%d)", file, line)
	);

	if (NULL != str)
	{
		assert_fail(
			strlen(str) >= strlen(ref),
			dsprintf(test_suite, "string too short -- \"%s\" (len %d), expected \"%s\" (len %d) (%s:%d)", str, strlen(str), ref, strlen(ref), file, line)
		);
		assert_fail(
			strncmp(str, ref, strlen(ref)) == 0,
			dsprintf(test_suite, "error string \"%s\" does not start with \"%s\" (%s:%d)", str, ref, file, line)
		);
	}
}
