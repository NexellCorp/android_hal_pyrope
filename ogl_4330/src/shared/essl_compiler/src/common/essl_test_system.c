/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_test_system.h"


/* These wrapper functions are only for use in the offline compiler and in tests. */

void *_essl_malloc_wrapper(unsigned int size)
{
	return malloc((size_t)size);
}

void _essl_free_wrapper(void *ptr)
{
	free(ptr);
}

