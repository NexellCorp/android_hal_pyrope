/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef MALI_MEM_REF_H
#define MALI_MEM_REF_H

#include <mali_system.h>
#include <stddef.h>
#include <base/mali_memory.h>

/** a simple ref-counted wrapper for a mali memory block */
typedef struct mali_mem_ref
{
	mali_atomic_int ref_count;
	mali_mem_handle mali_memory;
} mali_mem_ref;

/** create a mem ref */
MALI_IMPORT mali_mem_ref *_mali_mem_ref_alloc(void);

/** create a mem ref with mali mem */
MALI_IMPORT mali_mem_ref *_mali_mem_ref_alloc_mem(mali_base_ctx_handle base_ctx, size_t size, u32 pow2_alignment, u32 mali_access);

/** add a mali memory reference */
MALI_STATIC_INLINE void _mali_mem_ref_addref(mali_mem_ref *mem)
{
	MALI_DEBUG_ASSERT_POINTER(mem);
	_mali_sys_atomic_inc( &mem->ref_count );
}

MALI_STATIC_INLINE int _mali_mem_ref_get_ref_count(mali_mem_ref *mem)
{
	MALI_DEBUG_ASSERT_POINTER(mem);
	return _mali_sys_atomic_get( &mem->ref_count );
}

/** release a mali memory reference and delete memory if unreferenced */
MALI_IMPORT void _mali_mem_ref_deref( mali_mem_ref *mem );

#endif /* MALI_MEM_REF_H */
