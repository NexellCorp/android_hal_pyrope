/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

 /**
 * @file base_common_ds.h
 * Defines the interface to the internal parts of the dependency system in \a base_common_ds.c
 * The functions in this file should only be used by the frontend of the dependency system,
 * found in \a src/base/frontend/mali_dependency_system.c
 * This frontend files wraps the functions defined in this file. Because of this is the only user
 * the functions here are not documented by doxygen in this file. The the doxygen comments for
 * the functions here would be the same as the doxygen comments for the wrapped up public functions.
 * See \a base_dependency_system.h for documentation of the public functions.
 *
 * The structs used by the dependency system is also found here. These should not be used by
 * anyone else than the internal parts of the dependency system in \a base_common_ds.c and the
 * low level tests in the testbench.
 */

#ifndef _BASE_COMMON_DEPENDENCY_SYSTEM_H_
#define _BASE_COMMON_DEPENDENCY_SYSTEM_H_

#include <mali_system.h>
#include <base/mali_macros.h>
#include <base/common/tools/base_common_tools_circular_linked_list.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Can be used on the ref_count_decrease variable into function mali_ds_consumer_release_ref_count_change() */
#define MALI_DS_REF_COUNT_TRIGGER               (MALI_S32_MAX)
#define MALI_DS_REF_COUNT_GRAB                  (1)
#define MALI_DS_REF_COUNT_RELEASE               (-1)
#define MALI_DS_NUM_CONNECTIONS_PER_BLOCK       256

struct mali_ds_resource;
struct mali_ds_connection;
struct mali_ds_consumer;
struct mali_ds_manager;

/** Tells if the specified object is keeped in a reseted state,
 * or released and thus not acccessible any more */
typedef enum mali_ds_release
{
        MALI_DS_RELEASE,
        MALI_DS_KEEP
} mali_ds_release;

/**
 * Enum used as an input parameter to function \a mali_ds_resource_release_connections().
 * It tells which dependency connections it should remove.
 * \a MALI_DS_ABORT_NONE    Tells that it should not abort any connections.
 * \a MALI_DS_ABORT_WAITING Tells that it should only abort connections which has NOT been activated.
 * \a MALI_DS_ABORT_ALL     Tells that it should abort all connections.
 */
typedef enum mali_ds_abort
{
        MALI_DS_ABORT_NONE,
        MALI_DS_ABORT_WAITING,
        MALI_DS_ABORT_ALL
} mali_ds_abort;

/**
 * Used by functions creating a dependency connection between a Consumer and a Resource.
 * This tells if the action of the Consumer should read or write to the Resource.
 * Many Consumers can Read from a Resource at the same time, but only one can write.
 */
typedef enum mali_ds_connection_type
{
        MALI_DS_READ,
        MALI_DS_WRITE
} mali_ds_connection_type;

/**
 * This is a status enum used as an input to \a mali_ds_consumer_release_ref_count_change()
 * This is used to indicate that something error has occured. The error is propageted
 * to the callbackfunctions to the consumer.
 * If the callback functions to the consumer gets a callback with MALI_DS_ERROR set
 * it could mean one of two things:
 * 1) The dependency on a resource is killed by \a mali_ds_resource_release_connections
 * 2) The consumer object has been told there occured an error through the \a mali_ds_consumer_set_error()
 * function.
 * The callback functions should then know that they can not trust the result of what they are waiting for.
 */
typedef enum mali_ds_error
{
        MALI_DS_OK,
        MALI_DS_ERROR
} mali_ds_error;

/**
 * Parameter for \a mali_ds_consumer_release_set_mode() function.
 * Decides what shall happen when the consumer is told to release all its connections.
 * If MALI_DS_RELEASE_WRITE_GOTO_UNFLUSHED is set the consumer will only release write connections,
 * leaving the read connections still attached. It is then in unflushed state. Normal procedure is
 * then later to add new write dependencies, and flush it which will start a new rendering.
 * This is used for what is called incremented rendering.
 * If this mode is set, it is important to set it back to MALI_DS_RELEASE_ALL before final releas of
 * the consumer, to not keep the Read dependencies forever, which would have led to memory leak.
 * */

typedef enum mali_ds_release_mode
{
        MALI_DS_RELEASE_ALL,   /**< Normal release: 1) Release callback 2) Release connections */
        MALI_DS_RELEASE_WRITE_GOTO_UNFLUSHED  /**< 1) Release callback 2) Release write deps, 3) To UNFLUSHED State */
} mali_ds_release_mode ;


/** A Resource - Normally in a 1-to-1 relation with a Surface */
typedef struct mali_ds_resource_type * mali_ds_resource_handle;

/** A Consumer - Something that will read or write to a number of Resources.
 * Gets an activation callback when the dependencies to all the Resources it depends on are triggered.*/
typedef struct mali_ds_consumer_type * mali_ds_consumer_handle;

/**
 * Callback function from a resource when it is deleted.
 * Makes it possible to synchronize the ref count of the surface with the Resource object,
 * to keep the surface alive until the resource is deleted.
*/
typedef void (*mali_ds_cb_func_resource)(mali_base_ctx_handle base_ctx, void * cb_param);

/**
 * A typedef of a callback function that is called when the consumer is activated, that means:
 * all the connections to the Resources it depends on are triggered.
 * Normally the callback will start the Rendering job, since this is the point when it knows that
 * all buffers it will read and write to are available for it.
 * After the Rendering is finished it MUST release its connections, so other can use these Resources.
 * The error_value variable is MALI_DS_ERROR if there were any errors, in that case it is not safe to
 * start any rendering, and all connections should be removed.
 */
typedef void (*mali_ds_cb_func_consumer_activate)(mali_base_ctx_handle base_ctx, void * owner, mali_ds_error error_value);

/**
 * A typedef of a callback function intended for the Copy On Write procedure.
 * When the user is calling \a mali_ds_connect() and tries to connect a WRITE dependency to a Resource (surface)
 * that already has a Read dependency that is not flushed, or has a mode that will not release its dependency
 * when the rendering is finished the current connection would block until this other connection is released, which
 * could be undeinitely. To avoid waiting undefenitely the consumer can set this callback function, and what it should
 * do is to perform is to allocate a new Surface and a corresponding Resource. The new Resource should be returned
 * from this callback function. Since the Resource is new, it is not blocked by any other unflushed framebuilders,
 * and the rendering could start as soon it gets all its Read connections triggered, and GP is finished.
 * */
typedef mali_ds_resource_handle (*mali_ds_cb_func_consumer_replace_resource)(void * resource_owner, void * consumer_owner);

/**
 * A typedef of a callback function that is called when the consumer is released, that means:
 * All the connections to the Resources is talled to be removed from it, and it goes to Unused state.
 * If the consumer mode was set to MALI_DS_RELEASE_WRITE_GOTO_UNFLUSHED initially this function
 * would not relase its Read dependencies, and instead go back to the unflushed state.
 * The status variable tells if the work was done with success.
 * If the release function finds out that it should not release its connections after all,
 * but set the consumer back to FLUSHED state, still holding all the connections, the function
 * should return MALI_DS_KEEP. This is used for CMU, which split each rendering into several
 * iterations of GP and PP jobs, where each continues where the last iteration stopped.
 * In all other cases it should return MALI_DS_RELEASE.
 * If status==MALI_DS_ERROR the return from this function must be MALI_DS_RELEASE.
 */
typedef mali_ds_release (*mali_ds_cb_func_consumer_release)(mali_base_ctx_handle base_ctx, void * owner, mali_ds_error status);

typedef enum mali_ds_resource_state
{
	MALI_DS_RESOURCE_STATE_UNUSED,
	MALI_DS_RESOURCE_STATE_READ,
	MALI_DS_RESOURCE_STATE_BLOCKED
} mali_ds_resource_state;

typedef enum mali_ds_consumer_state
{
	MALI_DS_CONSUMER_STATE_UNUSED,
	MALI_DS_CONSUMER_STATE_PREPARING,
	MALI_DS_CONSUMER_STATE_WAITING,
	MALI_DS_CONSUMER_STATE_ACTIVATED
} mali_ds_consumer_state;

typedef struct mali_ds_wait_element
{
	mali_base_wait_handle          wait_handle;
	struct mali_ds_wait_element *  next;
} mali_ds_wait_element;

typedef struct mali_ds_manager_connection_block
{
	mali_embedded_list_link blocks;
	void *block_ptr;
} mali_ds_manager_connection_block;

typedef struct mali_ds_resource
{
	void * owner;
	s32 ref_count_connections_out;
	mali_embedded_list_link connections_out;
	mali_ds_resource_state state;
	mali_ds_abort release_abort_type;
	mali_ds_cb_func_resource cb_on_release;
	struct mali_ds_manager * manager;
	mali_bool shutdown;
	MALI_DEBUG_CODE(u32 magic_number;)
} mali_ds_resource;

typedef struct mali_ds_connection
{
	struct mali_ds_resource * connection_resource;
	struct mali_ds_consumer * connection_consumer;
	mali_embedded_list_link  element_on_resource_list;
	mali_embedded_list_link  element_on_consumer_list;
	mali_ds_connection_type rights;
	mali_bool  is_trigged;
	MALI_DEBUG_CODE(u32 magic_number;)
} mali_ds_connection;

typedef struct mali_ds_consumer
{
	void * owner;
	s32                           ref_count_untrigged_connections;
	mali_embedded_list_link       connections_linked_list_header;
	mali_ds_cb_func_consumer_activate      cb_func_activate;
	mali_ds_cb_func_consumer_release       cb_func_release;
	mali_ds_cb_func_consumer_replace_resource   cb_func_replace_resource;
	s32                           release_ref_count;
	mali_ds_consumer_state        state;
	mali_ds_error           error_value;
	mali_ds_release_mode          release_mode;
	mali_bool                     delete_self;
	struct mali_ds_manager *      manager;
	mali_ds_wait_element *        blocked_waiter_list;
	MALI_DEBUG_CODE(u32 magic_number;)
} mali_ds_consumer;

typedef struct mali_ds_manager
{
	mali_base_ctx_handle ctx;
	mali_mutex_handle global_list_manipulation_mutex;
	u32 global_list_lock_thread_id;
	u32 global_list_lock_counter;
	mali_embedded_list_link blocks_alloced;
	mali_embedded_list_link free_connections_list;
	MALI_DEBUG_CODE(u32 magic_number;)
} mali_ds_manager;

/** Debug defines used in assert macros */
#ifndef MALI_DEBUG_SKIP_CODE
#define MALI_DS_CONNECTION_MAGIC_NR   0x01234567
#define MALI_DS_RESOURCE_MAGIC_NR     0x12345678
#define MALI_DS_CONSUMER_MAGIC_NR     0x23456789
#define MALI_DS_MANAGER_MAGIC_NR      0x3456789A
#endif


MALI_IMPORT mali_err_code mali_common_ds_open(mali_base_ctx_handle ctx);

MALI_IMPORT void mali_common_ds_close(mali_base_ctx_handle ctx);


MALI_IMPORT mali_ds_resource_handle  mali_common_ds_resource_allocate(mali_base_ctx_handle ctx,
                                                                      void * owner,
                                                                      mali_ds_cb_func_resource cb_on_release);

MALI_IMPORT void mali_common_ds_resource_set_callback_parameter(mali_ds_resource_handle resource, void * owner);


MALI_IMPORT mali_bool mali_common_ds_resource_has_dependencies(mali_ds_resource_handle resource);


MALI_IMPORT void mali_common_ds_resource_release_connections(mali_ds_resource_handle resource,
                                                             mali_ds_release keep_resource,
                                                             mali_ds_abort connections);

MALI_IMPORT mali_ds_consumer_handle mali_common_ds_consumer_allocate(mali_base_ctx_handle ctx,
                                                              void * owner,
                                                              mali_ds_cb_func_consumer_activate cb_func_activate,
                                                              mali_ds_cb_func_consumer_release cb_func_release);

MALI_IMPORT void mali_common_ds_consumer_set_callback_parameter(mali_ds_consumer_handle resource, void * owner);

MALI_IMPORT void mali_common_ds_consumer_set_callback_activate(mali_ds_consumer_handle consumer, mali_ds_cb_func_consumer_activate cb_func_activate);


MALI_IMPORT void mali_common_ds_consumer_set_callback_replace_resource(mali_ds_consumer_handle consumer, mali_ds_cb_func_consumer_replace_resource cb_func);

MALI_IMPORT void mali_common_ds_consumer_set_callback_release(mali_ds_consumer_handle consumer, mali_ds_cb_func_consumer_release cb_func_release);

MALI_IMPORT void mali_common_ds_consumer_activation_ref_count_change(mali_ds_consumer_handle consumer_in, signed int ref_count_change );

MALI_IMPORT void mali_common_ds_consumer_flush(mali_ds_consumer_handle consumer);

MALI_IMPORT mali_err_code mali_common_ds_consumer_flush_and_wait(mali_ds_consumer_handle consumer_in);

MALI_IMPORT void mali_common_ds_consumer_release_ref_count_change(mali_ds_consumer_handle consumer_h, signed int ref_count_change);

MALI_IMPORT int mali_common_ds_consumer_active(mali_ds_consumer_handle consumer_h);

MALI_IMPORT void mali_common_ds_consumer_release_set_mode(mali_ds_consumer_handle consumer_h, mali_ds_release_mode mode);

MALI_IMPORT void mali_common_ds_consumer_free(mali_ds_consumer_handle consumer_h);

MALI_IMPORT void mali_common_ds_consumer_set_error(mali_ds_consumer_handle consumer_h);

MALI_IMPORT mali_err_code mali_common_ds_connect(mali_ds_consumer_handle arrow_to,
                                                 mali_ds_resource_handle arrow_from,
                                                 mali_ds_connection_type rights) MALI_CHECK_RESULT;

MALI_IMPORT mali_err_code mali_common_ds_connect_and_activate_without_callback(mali_ds_consumer_handle arrow_to,
                                                                       mali_ds_resource_handle arrow_from,
                                                                       mali_ds_connection_type rights) MALI_CHECK_RESULT;

#ifndef MALI_DEBUG_SKIP_CODE
void mali_common_ds_system_debug_print_consumer(mali_ds_consumer_handle consumer);
void mali_common_ds_system_debug_print_resource(mali_ds_resource_handle resource_in);
#endif /* #ifndef MALI_DEBUG_SKIP_CODE */


#ifdef __cplusplus
}
#endif

#endif /* _BASE_COMMON_DEPENDENCY_SYSTEM_H_ */
