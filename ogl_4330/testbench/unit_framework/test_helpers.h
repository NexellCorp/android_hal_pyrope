/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2007, 2009-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include "suite.h"

#include <float.h>
#include <string.h>

/* functions (don't use directly, use macro wrappers instead) */
void assert_string_starts_with(suite* test_suite, const char *str, const char *ref, const char *file, int line);

#define ASSERT_FAIL(expr)  assert_fail(expr, dsprintf(test_suite, "assertation \"%s\" failed (%s:%d)", #expr, __FILE__, __LINE__))
#define ASSERT_FATAL(expr) assert_fatal(expr, dsprintf(test_suite, "assertation \"%s\" failed (%s:%d)", #expr, __FILE__, __LINE__))

/* simplified wrappers that autogenerate the name string */
#define ASSERT_INTS_EQUAL(value, expected)              assert_fail((int)(value) == (int)(expected),                          dsprintf(test_suite, "%s is %d, expected %d (%s:%d)", #value, value, expected, __FILE__, __LINE__))
#define ASSERT_INTS_EQUAL_EPSILON(value, expected, eps) assert_fail((int)(value) - (int)(expected) <= fabs(eps),              dsprintf(test_suite, "%s is %d, expected %d (%s:%d)", #value, value, expected, __FILE__, __LINE__))
#define ASSERT_INTS_DIFFER(value, expected)             assert_fail((int)(value) != (int)(expected),                          dsprintf(test_suite, "%s is %d (%s:%d)", #value, value, __FILE__, __LINE__))
#define ASSERT_FLOATS_EQUAL(value, expected, eps)       assert_fail(fabs((float)(value) - (float)(expected)) <= fabs(eps),    dsprintf(test_suite, "%s is %f, expected %f (%s:%d)", #value, (float)(value), (float)(expected), __FILE__, __LINE__))
#define ASSERT_FLOAT_GREATER_THAN(value, expected)      assert_fail((float)(value) > (float)(expected),                       dsprintf(test_suite, "%s is %f, should be greater than %f (%s:%d)", #value, (float)(value), (float)(expected), __FILE__, __LINE__))
#define ASSERT_FLOAT_LESS_THAN(value, expected)         assert_fail((float)(value) < (float)(expected),                       dsprintf(test_suite, "%s is %f, should be less than %f (%s:%d)", #value, (float)(value), (float)(expected), __FILE__, __LINE__))
#ifdef __SYMBIAN32__
#define ASSERT_POINTERS_EQUAL(ptr, expected)            assert_fail((ptr)==(expected),                                        dsprintf(test_suite, "pointer %s is %X, expected %X (%s:%d)", #ptr, (ptr), (expected), __FILE__, __LINE__))
#else
#define ASSERT_POINTERS_EQUAL(ptr, expected)            assert_fail((ptr)==(expected),                                        dsprintf(test_suite, "pointer %s is %p, expected %p (%s:%d)", #ptr, (ptr), (expected), __FILE__, __LINE__))
#endif

#define ASSERT_POINTER_IS_NULL(ptr)                     assert_fail(NULL == (ptr),     dsprintf(test_suite, "pointer %s is not NULL (%s:%d)", #ptr, __FILE__, __LINE__))
#define ASSERT_POINTER_NOT_NULL(ptr)                    assert_fail(NULL != (ptr),     dsprintf(test_suite, "pointer %s is NULL (%s:%d)", #ptr, __FILE__, __LINE__))
#define ASSERT_STRING_STARTS_WITH(str, ref)             assert_string_starts_with(test_suite, str, ref, __FILE__, __LINE__)
#define ASSERT_STRINGS_EQUAL(str, ref)                  assert_fail(strcmp(str, ref) == 0, dsprintf(test_suite, "%s is \"%s\", expected \"%s\" (%s:%d)", #str, str, ref, __FILE__, __LINE__))

/* add a test to a suite with the name equal to the test-function name */
#define ADD_TEST(suite, test) add_test(suite, #test, test)

#endif /* TEST_HELPERS_H */
