/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_types.h>
#include <base/mali_runtime.h>
#include <base/mali_context.h>
#include <base/mali_debug.h>
#ifdef MALI_TEST_API
	#include <base/mali_test.h>
#endif
#include "../unit_framework/suite.h"
#include "base_test_util.h"

#include <base/mali_memory.h>

void intern_memset_word(mali_mem_handle mem, u32 word)
{
	u32 * memp;
	u32 size_word;
	size_word = _mali_mem_list_size_get(mem) / sizeof(word);
	memp = (u32*)_mali_mem_ptr_map_area(mem, 0, size_word*sizeof(word), 0, MALI_MEM_PTR_WRITABLE|MALI_MEM_PTR_NO_PRE_UPDATE);
	if (NULL != memp)
	{
		while(size_word--) *memp++ = word;
		_mali_mem_ptr_unmap_area(mem);
	}
}

int rand_range(int from, int to_including)
{
	return from + (_mali_sys_rand() % (to_including - from + 1));
}
