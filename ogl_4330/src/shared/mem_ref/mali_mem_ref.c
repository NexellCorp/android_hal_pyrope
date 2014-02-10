/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <util/mali_mem_ref.h>

MALI_STATIC void _mali_mem_ref_init(mali_mem_ref *mem)
{
	MALI_DEBUG_ASSERT_POINTER(mem);

	_mali_sys_atomic_initialize(&mem->ref_count, 1);
	mem->mali_memory = MALI_NO_HANDLE;
}

MALI_EXPORT mali_mem_ref *_mali_mem_ref_alloc(void)
{
	mali_mem_ref *ref = (mali_mem_ref*)_mali_sys_malloc(sizeof(mali_mem_ref));
	if (NULL == ref) return NULL;

	_mali_mem_ref_init(ref);
	return ref;
}

MALI_EXPORT mali_mem_ref *_mali_mem_ref_alloc_mem(mali_base_ctx_handle base_ctx, size_t size, u32 pow2_alignment, u32 mali_access)
{
	/* allocate memref itself */
	mali_mem_ref *mem_ref = _mali_mem_ref_alloc();
	if (NULL == mem_ref) return NULL;

	/* allocate memory for the memref */
	mem_ref->mali_memory = _mali_mem_alloc(base_ctx, size, pow2_alignment, mali_access );
	if (NULL == mem_ref->mali_memory)
	{
		/* release mem ref */
		_mali_mem_ref_deref( mem_ref );
		mem_ref = NULL;

		/* return error */
		return NULL;
	}

	/* ALL IS GOOD! */
	return mem_ref;
}

MALI_EXPORT void _mali_mem_ref_deref( mali_mem_ref *mem )
{
	u32 ref_count = 0;
	MALI_DEBUG_ASSERT_POINTER(mem);
	MALI_DEBUG_ASSERT(0 < _mali_sys_atomic_get( &mem->ref_count ), ("inconsistent ref count"));

	ref_count = _mali_sys_atomic_dec_and_return( &mem->ref_count );

	/* check if this was the last reference */
	if (0 == ref_count)
	{
		/* free mali mem */
		_mali_mem_free(mem->mali_memory);
		mem->mali_memory = MALI_NO_HANDLE;

		/* free the actual mem ref also */
		_mali_sys_free(mem);
		mem = NULL;
	}
}
