/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef GLES_HELPERS_H
#define GLES_HELPERS_H

#include "suite.h"
#include "gl2_framework.h"

const char *get_glenum_name(GLenum e);

#define ASSERT_GLENUMS_EQUAL(input, expected) \
	assert_fail((input) == (expected), \
				dsprintf(test_suite, \
						 "GLenums differ -- %s is %s (0x%X), expected %s (0x%X) (%s:%d)", \
						 #input, \
						 get_glenum_name(input), \
						 input, \
						 get_glenum_name(expected), \
						 expected, \
						 __FILE__, __LINE__));

#define ASSERT_GLENUMS_EQUAL_MSG(input, expected, msg) \
	assert_fail((input) == (expected), \
				dsprintf(test_suite, \
						 "GLenums differ -- %s is %s (0x%X), expected %s (0x%X), msg: %s (%s:%d)", \
						 #input, \
						 get_glenum_name(input), \
						 input, \
						 get_glenum_name(expected), \
						 expected, \
						 msg, \
						 __FILE__, __LINE__));

#define ASSERT_GLERROR_EQUAL(expected) do {  \
		GLenum err = glGetError();           \
		ASSERT_GLENUMS_EQUAL(err, expected); \
	} while (0)

#define ASSERT_GLERROR_EQUAL_MSG(expected, msg) do {  \
		GLenum err = glGetError();           \
		ASSERT_GLENUMS_EQUAL_MSG(err, expected, msg); \
	} while (0)

#ifdef __SYMBIAN32__
#define PRINT_PERFORMANCE(microseconds, message) SOS_DebugPrintf("PERFORMANCE - %s (%s): %f s\n", test_suite->name, message, microseconds / 1000000.0)
#else
#define PRINT_PERFORMANCE(microseconds, message) printf("PERFORMANCE - %s (%s): %f s\n", test_suite->name, message, microseconds / 1000000.0)
#endif

#endif /* GLES_HELPERS_H */
