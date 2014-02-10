/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _MALI_BASE_DUMP_
#define _MALI_BASE_DUMP_

#ifdef MALI_DUMP_ENABLE

#include "mali_system.h"
#include "base/mali_context.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Increase the global current frame number by one.
 * The dumping system can be set up to dump spesific frames.
 * It uses the counter controlled by this variable.
 * \param ctx The base context
 */
MALI_IMPORT void _mali_base_common_dump_frame_counter_increment(mali_base_ctx_handle ctx);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MALI_DUMP_ENABLE  - Compiletime toggle*/
#endif /* _MALI_BASE_DUMP_  - Include guard */
