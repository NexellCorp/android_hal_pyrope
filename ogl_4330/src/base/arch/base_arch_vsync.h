/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _BASE_ARCH_VSYNC_H_
#define _BASE_ARCH_VSYNC_H_

#include <base/mali_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Add/register a VSYNC event.
 * @param event The event type.
 */
void _mali_base_arch_vsync_event_report(mali_vsync_event event);


#ifdef __cplusplus
}
#endif

#endif /* _BASE_ARCH_PROFILING_H_ */
