/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007, 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_context.h
 * All module usage in the base driver is scoped using a context object.
 * All objects created in base are local to the context they're created in.
 */

#ifndef _MALI_CONTEXT_H_
#define _MALI_CONTEXT_H_

#include <base/mali_types.h>
#include <base/mali_macros.h>
#include <base/common/base_common_context.h>
#include <base/arch/base_arch_runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a base driver context
 * Creates a new context in the base driver and returns a handle to it
 * @return A base context handle or MALI_NO_HANDLE on error
 */
MALI_STATIC_FORCE_INLINE mali_base_ctx_handle _mali_base_context_create(void)
{
        return _mali_base_common_context_create();
}

/**
 * Destroy a base driver context
 * Destroys the given context. If any modules are still open they will be closed.
 * Any open modules when a context is destroyed will be logged in debug mode
 * @param ctx Handle to the base context to destroy
 */
MALI_STATIC_FORCE_INLINE void _mali_base_context_destroy(mali_base_ctx_handle ctx)
{
        _mali_base_common_context_destroy(ctx);
}

/**
 * Get a frame_id from context.
 * The frame id is a counter that is incremented for each call.
 * It does not need to be released.
 * @return A new frame_id, a new unique number.
 */
MALI_STATIC_FORCE_INLINE mali_base_frame_id _mali_base_frame_id_get_new(mali_base_ctx_handle ctx)
{
        return _mali_base_common_frame_id_get_new(ctx);
}

/*
 * Get next unique frame builder id
 * The id counter is incremented (atomically) for each call ensuring the uniqueness of the id. 
 *
 * @return Unique frame_builder_id
 */
MALI_STATIC_FORCE_INLINE mali_base_frame_builder_id _mali_base_frame_builder_id_get_new(mali_base_ctx_handle ctx)
{
        return _mali_base_common_frame_builder_id_get_new(ctx);
}

/**
 * @brief Get a value from the settings database
 *
 * @param setting the ID of the setting to be retrieved
 * @return the value
 */
MALI_STATIC_FORCE_INLINE u32 _mali_base_get_setting(_mali_setting_t setting)
{
	return _mali_base_arch_get_setting(setting);
}

/**
 * @brief Queue some work on the cleanup thread.
 *
 * @param ctx Handle to the base context.
 * @param task_proc The task function to be run.
 * @param task_param The argument to the task function.
 * @return MALI_ERR_NO_ERROR means the task is queued. MALI_ERR_OUT_OF_MEMORY indicates an Out of
 *         Memory condition. MALI_ERR_FUNCTION_FAILED indicates that the worker thread has been
 *         signaled to quit.
 */
MALI_STATIC_FORCE_INLINE mali_err_code _mali_base_cleanup_thread_enqueue(mali_base_ctx_handle ctx, mali_base_worker_task_proc task_proc, void *task_param)
{
	return _mali_base_common_context_cleanup_thread_enqueue(ctx, task_proc, task_param);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MALI_CONTEXT_H_ */
