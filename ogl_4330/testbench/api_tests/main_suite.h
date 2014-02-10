/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2007, 2009-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MAIN_SUITE_H
#define MAIN_SUITE_H

#include "resultset.h"
#include "suite.h"

/**
 *	Main test runner function.
 *	This will create each suite of tests, run them, and print the results.
 *
 *	\param mask A bitwise OR of the result types to list (values from result_mask).
 *	\return The status code for the worst error encountered.
 *
 *	\sa result_mask
 */

int run_main_suite(int mask, test_specifier* test_spec);

#endif /* MAIN_SUITE_H */
