/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_ESSL_TEST_SYSTEM_H
#define COMMON_ESSL_TEST_SYSTEM_H

#define NEEDS_STDIO
#define NEEDS_CTYPE
#define NEEDS_TIME
#define NEEDS_MALLOC

#include "common/essl_system.h"

/* These wrapper functions are only for use in the offline compiler and in tests. */
void *_essl_malloc_wrapper(unsigned int size);
void _essl_free_wrapper(void *ptr);


#endif
