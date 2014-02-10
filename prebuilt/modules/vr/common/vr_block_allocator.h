/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __VR_BLOCK_ALLOCATOR_H__
#define __VR_BLOCK_ALLOCATOR_H__

#include "vr_kernel_memory_engine.h"

vr_physical_memory_allocator * vr_block_allocator_create(u32 base_address, u32 cpu_usage_adjust, u32 size, const char *name);

#endif /* __VR_BLOCK_ALLOCATOR_H__ */
