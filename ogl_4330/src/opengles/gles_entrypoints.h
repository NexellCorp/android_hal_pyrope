/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_entrypoints.h
 * @brief Contains helper macros for the gles*_entrypoints.c files
 */

#ifndef _GLES_ENTRYPOINTS_UTIL_H_
#define _GLES_ENTRYPOINTS_UTIL_H_

#include "gles_shims_common.h"
#include "mali_instrumented_context.h"
#include "mali_log.h"
#include "base/mali_profiling.h"

#ifndef MALI_GL_APIENTRY
#define MALI_GL_APIENTRY(a) GL_APIENTRY a
#endif

#ifdef MALI_MONOLITHIC
	#ifndef MALI_GL_APICALL
	#define MALI_GL_APICALL	MALI_VISIBLE GL_APICALL
	#endif
#endif

#ifndef MALI_GL_APICALL
#define MALI_GL_APICALL	GL_APICALL
#endif

/*
 * Determines whether gles_debug_stats.h should generate code to gather entrypoint stats or print.
 * The default is no output
 */
#ifndef GLES_DEBUG_PRINT_ENTRYPOINTS
	#define GLES_DEBUG_PRINT_ENTRYPOINTS     0
#endif
#ifndef GLES_DEBUG_STATS_FOR_ENTRYPOINTS
	#define GLES_DEBUG_STATS_FOR_ENTRYPOINTS 0
#endif

#include "gles_debug_stats.h" /* can (and often will) override GLES_ENTER_FUNC and GLES_LEAVE_FUNC */

#define GLES_GET_CONTEXT() (_gles_get_context())

/* Macros invoked on entry to and exit from every API function */
#define GLES_API_ENTER(ctx) do {\
		MALI_DEBUG_ASSERT_POINTER(ctx->vtable); \
		MALI_INSTRUMENTED_LOG((_instrumented_get_context(), MALI_LOG_LEVEL_EVERYTHING, "Enter function %s", MALI_FUNCTION)); \
		GLES_ENTER_FUNC();\
	} while (0)

#define GLES_API_LEAVE(ctx) do {\
		GLES_LEAVE_FUNC();\
		MALI_INSTRUMENTED_LOG((_instrumented_get_context(), MALI_LOG_LEVEL_EVERYTHING, "Leave function %s", MALI_FUNCTION)); \
	} while (0)


#endif /* _GLES_ENTRYPOINTS_UTIL_H_ */
