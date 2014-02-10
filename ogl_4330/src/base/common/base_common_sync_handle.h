/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file base_common_sync_handle.h
 * The common layer contains the implementation of the sync_struct-functions.
 * Function-calls are passed through the frontend to common layer.
 */

#ifndef _BASE_COMMON_SYNC_HANDLE_H_
#define _BASE_COMMON_SYNC_HANDLE_H_
#include <base/mali_macros.h>
#include <base/common/base_common_context.h>
#include <base/common/mem/base_common_mem.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Type definition for the sync callback function
 * Called when all jobs registered with a sync handle has completed
 * @param ctx A mali base context handle
 * @param cb_param User defined callback parameter
 */
typedef void (*mali_sync_cb)(mali_base_ctx_handle ctx, void * cb_param);

/**
 * Type definition for the sync handle.
 */
typedef struct mali_sync_type * mali_sync_handle;

/**
 * Get a new core functionality synchronization object.
 * Contains all the functionality of a normal sync object except the memory cleanup support functions.
 * @param ctx The mali base contex handle obtained in a open call earlier
 * @return Non-null on success, null on failure
 */
mali_sync_handle _mali_base_common_sync_handle_core_new(mali_base_ctx_handle ctx);

/**
 * Get a new synchronization object.
 * @param ctx The mali base contex handle obtained in a open call earlier
 * @return Non-null on success, null on failure
 */
mali_sync_handle _mali_base_common_sync_handle_new(mali_base_ctx_handle ctx);

/**
 * Set the callback for a sync object. Set the callback function to be called when all jobs registered on this sync object has completed.
 * The callback function
 * @param handle Handle to the sync object to set the callback on
 * @param cbfunc The function to be called when sync is complete
 * @param cbarg  Data passed as an argument to the callback function
 */
void _mali_base_common_sync_handle_cb_function_set(mali_sync_handle handle, mali_sync_cb cbfunc, void * cbarg);

/**
 * Add a mali_mem_handle to a sync object's free-on-termination list
 * @param handle Handle to the sync object to update
 * @param mem Handle to mali memory to add to the list
 */
void _mali_base_common_sync_handle_add_mem_to_free_list(mali_sync_handle handle, mali_mem_handle mem);

/**
 * Get a wait handle which is trigged when the sync object is triggered.
 * Returns a handle to a wait object usable to wait on this sync object to trigger.
 * @note This must be called before @see _mali_sync_handle_flush if you want to be able wait on this sync object
 * @param handle Handle to a sync object
 * @return Handle which can be used with @see _mali_wait_on_handle
 */
mali_base_wait_handle _mali_base_common_sync_handle_get_wait_handle(mali_sync_handle handle);

/**
 * Start the sync operation. Tells the sync object to enable itself. The callback will never be called before this function is called.
 * If the jobs have finished before the call the callback will be called at this point, if not it will be called at a later time.
 * @param handle Handle to the sync object to start
 */
void _mali_base_common_sync_handle_flush(mali_sync_handle handle);

/**
 * Register a reference to the sync handle
 * Called by the monitored systems when they have objects which the sync handle should depend on
 * @param handle Handle to the sync object to update
 */
void _mali_base_common_sync_handle_register_reference(mali_sync_handle handle);

/**
 * Remove a reference to the sync handle
 * Called when the monitored system have objects which have completed their action
 * @param handle Handle to the sync object to update
 */
void _mali_base_common_sync_handle_release_reference(mali_sync_handle handle);

/**
 * Create a dependency between two sync handles
 * Makes one sync object depend on the completion of another sync handle
 * Limitations:
 * - A sync object can monitor multiple sync objects.
 * - A sync object can only be monitored by a single sync object.
 *
 * The completion of monitored will after the call be one of the events sync_object depends on.
 *
 * @param sync_object - Handle to the sync object which should monitor another sync handle's completion
 * @param monitored - Handle to the sync object to monitor
 */
void _mali_base_common_sync_handle_add_to_sync_handle( mali_sync_handle sync_object, mali_sync_handle monitored);

#ifdef __cplusplus
}
#endif

#endif /* _BASE_COMMON_SYNC_HANDLE_H_ */
