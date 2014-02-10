/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _BASE_TEST_UTIL_H_
#define _BASE_TEST_UTIL_H_

#include <base/mali_types.h>
#include <base/mali_runtime.h>
#include <base/mali_context.h>
#include <base/mali_debug.h>
#ifdef MALI_TEST_API
	#include <base/mali_test.h>
#endif
#include "../unit_framework/suite.h"

#include <base/mali_memory.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PRINT_MAGIC(alloc) do{\
	MALI_DEBUG_PRINT(5, (#alloc "  0x%08x @ %d\n", \
	(u32)_mali_mem_mali_addr_get((alloc), 0), \
	 (u32)_mali_mem_size_get((alloc))));\
	}while(0)

#define MEM_SET_MAGIC(alloc, value) do{\
	MALI_DEBUG_ASSERT((NULL != alloc), (("nullpointer")));\
	PRINT_MAGIC(alloc);\
	intern_memset_word( alloc , value );\
	} while(0)

#define CHECK_NO_ERR(expr) \
	do{\
		mali_err_code err;\
		char txt[300];\
		err = expr;\
		if (MALI_ERR_NO_ERROR != err)\
		{\
			_mali_sys_snprintf(txt, 299, "Error in: %s on line %d\n",MALI_FUNCTION, __LINE__ );\
			assert_fail(0, txt);\
		}\
		else assert_fail(1, "");\
	} while(0)

void intern_memset_word(mali_mem_handle mem, u32 word);

int rand_range(int from, int to_including);

#ifdef __cplusplus
}
#endif


#endif /* _BASE_TEST_UTIL_H_ */
