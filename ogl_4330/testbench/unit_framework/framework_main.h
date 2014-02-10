/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef _FRAMEWORK_MAIN_H_
#define _FRAMEWORK_MAIN_H_

#include "suite.h"
typedef int (*suite_func)( int, test_specifier* );
typedef int (*parse_func)( test_specifier*, mempool*, const char* );

int  test_main( suite_func suitefunc, parse_func parsefunc, int argc, char **argv );

#endif /* _FRAMEWORK_MAIN_H_ */
