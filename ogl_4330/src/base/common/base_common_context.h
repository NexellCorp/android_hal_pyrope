/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _BASE_COMMON_CONTEXT_H_
#define _BASE_COMMON_CONTEXT_H_

#include <base/mali_worker.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Defintion of the base context type
 */
typedef struct mali_base_ctx_type * mali_base_ctx_handle;


/**
 * Create a base driver context
 * Creates a new context in the base driver and returns a handle to it
 * @return A base context handle or NULL on error
 */
MALI_TEST_IMPORT
mali_base_ctx_handle _mali_base_common_context_create(void);

/**
 * Destroy a base driver context
 * Destroys the given context. If any modules are still open they will be closed.
 * Any open modules when a context is destroyed will be logged in debug mode
 * @param ctx Handle to the base context to destroy
 */
MALI_TEST_IMPORT
void _mali_base_common_context_destroy(mali_base_ctx_handle ctx);

/**
 * Add a reference to a base driver context
 * Adds a reference to the base context
 * @param The base context to add a reference to
 */
void _mali_base_common_context_reference_add(mali_base_ctx_handle ctx);

/**
 * Remove a reference to a base driver context
 * Removes a reference to the base context
 * @param The base context to remove a reference from
 */
void _mali_base_common_context_reference_remove(mali_base_ctx_handle ctx);


/**
 * Create a unique mali_gp_frame_id in this context.
 * Gets a unique mali_gp_frame_id. This tells the jobsubsystem which jobs
 * that are built from the same frame builder, and for those it will skip
 * the userspecified nr of cmds from the start of the jobs cmd list.
 * @param ctx The base context to create the frame id in
 * @return  Handle to an unique frame
 */
mali_base_frame_id _mali_base_common_frame_id_get_new(mali_base_ctx_handle ctx);

/**
 * Create an unique frame builder ID in this context. The ID is used for tagging
 * GP and PP jobs with the ID of the frame builder that issued them. 
 */
mali_base_frame_id _mali_base_common_frame_builder_id_get_new(mali_base_ctx_handle ctx);

/* Opaque type for mali_ds_manager */
struct mali_ds_manager;

/**
 * Set the dependency system context in the given base context
 * @param ctx_handle Handle to the Base context
 * @param ds_context Pointer to the Dependency System context
 */
void _mali_base_common_context_set_dependency_system_context(mali_base_ctx_handle ctx_handle, struct mali_ds_manager *ds_context);

/**
 * Retrieve the dependency system context from a given base context
 * @param ctx_handle Handle to the Base context
 * @return Pointer to the Dependency System context
 */
struct mali_ds_manager * _mali_base_common_context_get_dependency_system_context(mali_base_ctx_handle ctx_handle);

/**
 * Destroy cleanup thread, blocking for all remaining tasks to complete.
 *
 * @param ctx_handle Handle to the base context.
 */
void _mali_base_common_context_cleanup_thread_end(mali_base_ctx_handle ctx_handle);

/**
 * Queue some work on the cleanup thread.
 * @param ctx Handle to the base context.
 * @param task_proc The task function to be run.
 * @param task_param The argument to the task function.
 * @return MALI_ERR_NO_ERROR means the task is queued. MALI_ERR_OUT_OF_MEMORY indicates an Out of
 *         Memory condition. MALI_ERR_FUNCTION_FAILED indicates that the worker thread has been
 *         signaled to quit.
 */
mali_err_code _mali_base_common_context_cleanup_thread_enqueue(mali_base_ctx_handle ctx, mali_base_worker_task_proc task_proc, void *task_param);

#ifdef MALI_DUMP_ENABLE

/* Opaque type for mali_dump_context */
struct mali_dump_context;

/**
 * Set the dump ontext in the given base context
 * @param ctx_handle Handle to the Base context
 * @param ds_context Pointer to the dump context
 */
void _mali_base_common_context_set_dump_context(mali_base_ctx_handle ctx_handle, struct mali_dump_context *dump_context);

/**
 * Retrieve dump context from a given base context
 * @param ctx_handle Handle to the Base context
 * @return Pointer to the dump context
 */
struct mali_dump_context * _mali_base_common_context_get_dump_context(mali_base_ctx_handle ctx_handle);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _BASE_COMMON_CONTEXT_H_ */
