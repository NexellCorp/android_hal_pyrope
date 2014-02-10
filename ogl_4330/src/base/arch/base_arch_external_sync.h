/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef BASE_ARCH_EXTERNAL_SYNC_H_
#define BASE_ARCH_EXTERNAL_SYNC_H_

#include <base/mali_types.h>

mali_stream_handle _mali_base_arch_stream_create(mali_base_ctx_handle ctx);

void _mali_base_arch_stream_destroy(mali_stream_handle handle);

mali_bool _mali_base_arch_fence_validate(mali_fence_handle fence);

#endif /* BASE_ARCH_EXTERNAL_SYNC_H_ */
