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
 * @file mali_dependency_system.h
 *
 * The Mali Dependency System is a independent dependency system that the Mali drivers use to
 * keep track on Read and Write dependencies between Surface Buffers (Resources), and the
 * entities (Framebuilders) that want to work on these surfaces (Consumers).
 * To rephrase it: Consumers (Frame builders) have read or write connections to several
 * Resources (surfaces). Other Consumers may be the Display driver that may held a
 * read dependency on a Resource (frame buffer) which is visible on screen.
 *
 * The reason why we introduce this somewhat complex Dependency System is that we want to
 * avoid to having the CPU to wait for a blocking dependency, when there are other things
 * that can be done. For example: we want to be able to start the GP job for a surface
 * even if the things we need for starting the PP job is not available. After that
 * we want to start the PP job when both the GP job is finished, and the surfaces this
 * PP job depends on. By letting the dependency system do these job starts from callbacks
 * when their list of dependencies are met, we do not need to have the functional but
 * slow locking system, that locks the CPU until the dependency is met.
 *
 * The basic entities are thus Resources which is allocated with: \a mali_ds_resource_allocate(),
 * and Consumers that are allocated through \a mali_ds_consumer_allocate().
 * You make a dependency between a Consumer and a Resource by: \a mali_ds_connect() where the
 * user also states whether the Consumer will Read or Write to the resource.
 * The consumer has an activation callback function that will be called when the Consumer should
 * start its work. The activation callback will normal start all the PP jobs for this consumer.
 *
 * When all the dependencies are added to the Consumer, it can be flushed. Flushing is to call
 * the function \a mali_ds_consumer_flush() on the consumer. This tells the Dependency system
 * that the consumer should get the Activation callback when all its dependencies are met.
 *
 * This is what we do in the activation function to the Frame today:
 * First we increase the Consumer ref count to the number of PP jobs we are about to start:
 * Calling * \a mali_ds_consumer_release_ref_count_change(frame->ds_consumer_pp_render,2)
 * the last number 2 indicates that it increases the ref count by 2.
 * Then it starts the PP jobs (2 in this case).
 *
 * The callback from the PP jobs when they are finished does this:
 * If the job fails it calls: \a mali_ds_consumer_set_error(consumer) And it always release
 * the reference count this job held:
 * \a mali_ds_consumer_release_ref_count_change(consumer,-1).
 *
 * When all (2) PP jobs are finished, the consumer is actually released, and the Dependency
 * System does the Release callback on the Consumer. The release callback will do the
 * _egl_mali_frame_swap() which shows the rendered image on screen. In this callback function
 * we also use the function \a mali_ds_connect_give_direct_ownership_and_flush() to transfer the rights
 * of the Frame Surface Resource from the Frame builder Consumer to the EGL Display consumer,
 * it will also release the previous EGL Display consumer so this can be used in the next
 * rendering.
 *
 * To see more (simple) examples of how the Dependency system works, see the test of it, found in
 * file \a dependency01.c in the testbench.
 *
 */

#ifndef _MALI_DEPENDENCY_SYSTEM_H
#define _MALI_DEPENDENCY_SYSTEM_H

#include <mali_system.h>
#include <base/mali_types.h>
#include <base/mali_macros.h>
#include <base/mali_context.h>
#include <base/common/dependency_system/base_common_ds.h>
#include "base/mali_profiling.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MALI_DS_TRACE
#define DS_TRACE_LOG_PARAM ,const char * func, const char * file, u32 line
#define DS_TRACE_PRINT(x) _mali_sys_printf x
#else
#define DS_TRACE_LOG_PARAM
#define DS_TRACE_PRINT(x) (void)0
#endif

#if MALI_TIMELINE_PROFILING_ENABLED
#define TIMELINE_PROFILING_REASON ,u32 tlp_reason
#else
#define TIMELINE_PROFILING_REASON
#endif

/**
 * Allocates a Resource - The dependency object for a Buffer that can be Read or Written to.
 * It is assumed that it has a 1-to-1 relationship with the descriptor object for the buffer,
 * and this is the \a cb_param input parameter. The optional callback function is automatically
 * called when the resource is actually released.
 * @param ctx The base context
 * @param cb_param The input to the callback function when it is released, and to the
 *        consumer_replace_resource callback function.
 * @param cb_on_release A callback function that will be called when the Resource is released.
*/
MALI_CHECK_RESULT MALI_STATIC_FORCE_INLINE mali_ds_resource_handle  mali_ds_resource_allocate(mali_base_ctx_handle ctx,
                                                               void * cb_param,
                                                               mali_ds_cb_func_resource cb_on_release DS_TRACE_LOG_PARAM )
{
        mali_ds_resource_handle resource;
        resource = mali_common_ds_resource_allocate(ctx, cb_param, cb_on_release);
        DS_TRACE_PRINT(("DS: %s %s %s %d allocated resource 0x%X with cb_param 0x%X\n", __FUNCTION__, func, file, line, resource, cb_param));
        return resource;
}

/**
 * Sets the pointer to the Surface Descriptor this Resource is in 1-to-1 relationship with.
 * This is the same as the \a cb_param in the \a mali_ds_resource_allocate() function.
 */
MALI_STATIC_FORCE_INLINE void mali_ds_resource_set_owner(mali_ds_resource_handle resource, void * owner DS_TRACE_LOG_PARAM)
{
        mali_common_ds_resource_set_callback_parameter(resource, owner);
        DS_TRACE_PRINT(("DS: %s %s %s %d set owner 0x%X on resource 0x%X\n", __FUNCTION__, func, file, line, owner, resource));
}


MALI_STATIC_FORCE_INLINE mali_bool mali_ds_resource_has_dependencies(mali_ds_resource_handle resource)
{
        return mali_common_ds_resource_has_dependencies(resource);
}

/**
 * Sets the owner pointer
 * This is the same as the \a cb_param in the \a mali_ds_consumer_allocate() function.
 */
MALI_STATIC_FORCE_INLINE void mali_ds_consumer_set_owner(mali_ds_consumer_handle resource, void * owner DS_TRACE_LOG_PARAM)
{
        mali_common_ds_consumer_set_callback_parameter(resource, owner);
        DS_TRACE_PRINT(("DS: %s %s %s %d set owner 0x%X on consumer 0x%X\n", __FUNCTION__, func, file, line, owner, resource));
}

/**
 * Function that can do two different things:
 * 1) Force the release of dependency connections to this resource.
 * The connections that are "killed" in this way will be signalled that they are missing a resource.
 * 2) It may release the actual Resource object itself, with the parameter \a keep_resource
 * If this parameter is set to \A MALI_DS_RELEASE the Resource, and the \a do_abort parameter
 * is set to \a MALI_DS_ABORT_ALL, the resource and its connection are removed before the
 * function returns. If the \a do_abort argument is set to something else, the deletion
 * of this resource may happen later, when all the connections to consumers are released.
 * @param resource The resource we want to kill connections from or delete
 * @param keep_resource Telling if we want to keep or delete the Resource object.
 * If it is set to be deleted, it is not allowed to add more connections to it.
 * @param connections Telling if this function should delete NONE connections,
 * WAITING connections that have not been triggered yet, or ALL which means that it
 * will also kill connections that are Trigged, and therefore currently running.
 *
 * @NOTE At the moment the integration of the drivers do NOT kill any connections from the
 * Resource itself, and have always released all connections before it deletes the Resource.
 */

MALI_STATIC_FORCE_INLINE void mali_ds_resource_release_connections(mali_ds_resource_handle resource,
                                                      mali_ds_release keep_resource,
                                                      mali_ds_abort do_abort DS_TRACE_LOG_PARAM )
{
        DS_TRACE_PRINT(("DS: %s %s %s %d resource 0x%X\n", __FUNCTION__, func, file, line, resource));
        mali_common_ds_resource_release_connections(resource, keep_resource, do_abort);
        DS_TRACE_PRINT(("END %s \n", __FUNCTION__));
}

/**
 * Allocates a Consumer in the Dependency system.
 * The consumer is an object with an associated action, a callbackfunction that will
 * be called when the consumer is activated.
 * The Consumer object can depend on a list of Resources that it can have Read or Write
 * connections to, and it will be activated when all its connections to Resources are triggered.
 * The consumer must release its connection after it has been activated.
 * When the consumer releases its connection, it will execute the \a cb_func_release before
 * it actually releases its connections. This makes it possible to use the function
 * \a mali_ds_connect_give_direct_ownership_and_flush to transfer the ownership of a Resource from
 * this consumer to another consumer when this consumer is releasing its connection to it.
 * @param ctx Mali base context
 * @param cb_param The callback parameter given to the callback functions.
 * @param cb_func_activate Callback function called when consumer is Activated.
 * @param cb_func_release  Callback function called when an Activated consumer is Releasing its connections.
 */
MALI_CHECK_RESULT MALI_STATIC_FORCE_INLINE mali_ds_consumer_handle mali_ds_consumer_allocate(mali_base_ctx_handle ctx,
                                                              void * cb_param,
                                                              mali_ds_cb_func_consumer_activate cb_func_activate,
                                                              mali_ds_cb_func_consumer_release cb_func_release DS_TRACE_LOG_PARAM )
{
        mali_ds_consumer_handle consumer;

        consumer = mali_common_ds_consumer_allocate(ctx, cb_param, cb_func_activate, cb_func_release);
        DS_TRACE_PRINT(("DS: %s %s %s %d allocated consumer 0x%X, cb_param 0x%X\n", __FUNCTION__, func, file, line, consumer, cb_param));
        return consumer;
}

/** Sets the callback function the consumer calls when it is activated (rendering can start) */
MALI_STATIC_FORCE_INLINE void mali_ds_consumer_set_callback_activate(mali_ds_consumer_handle consumer, mali_ds_cb_func_consumer_activate cb_func_activate)
{
        mali_common_ds_consumer_set_callback_activate(consumer, cb_func_activate);
}

/** Sets the callback function the consumer calls when it is released, typically a callback to swap the buffer */
MALI_STATIC_FORCE_INLINE void mali_ds_consumer_set_callback_release(mali_ds_consumer_handle consumer, mali_ds_cb_func_consumer_release cb_func_release)
{
        mali_common_ds_consumer_set_callback_release(consumer, cb_func_release);
}

/** Sets the callback function not used */
MALI_STATIC_FORCE_INLINE void mali_ds_consumer_set_callback_replace_resource(mali_ds_consumer_handle consumer, mali_ds_cb_func_consumer_replace_resource cb_func)
{
        mali_common_ds_consumer_set_callback_replace_resource(consumer, cb_func);
}

/**
 * After all connections are added between the consumer and all the Resources it depends on,
 * the consumer should be flushed. If all dependency connections are met before the flush
 * is called, the activation and the activation callback function callback function will
 * be called from inside the flush function call.
 * If the consumer is still waiting for some connections when flush is called, the
 * activation of the consumer will first happen when these dependencies are met.
 * It is not allowed to call this function twice on a consumer, before it has been
 * activated and released again.
 * @param consumer Handle to the Consumer to be flushed
 */
MALI_STATIC_FORCE_INLINE void mali_ds_consumer_flush(mali_ds_consumer_handle consumer DS_TRACE_LOG_PARAM )
{
        DS_TRACE_PRINT(("DS: %s %s %s %d consumer 0x%X\n", __FUNCTION__, func, file, line, consumer));
        mali_common_ds_consumer_flush(consumer);
        DS_TRACE_PRINT(("END %s  consumer 0x%X\n", __FUNCTION__, consumer));
}

/** This function flush this consumer, and do then wait until it is actually
 * activated before returning.
 * @NOTE Can not be called from inside a DS callback function.
 */
MALI_CHECK_RESULT MALI_STATIC_FORCE_INLINE mali_err_code mali_ds_consumer_flush_and_wait(mali_ds_consumer_handle consumer TIMELINE_PROFILING_REASON DS_TRACE_LOG_PARAM )
{
        mali_err_code err;
#if MALI_TIMELINE_PROFILING_ENABLED
        _mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_SUSPEND|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|tlp_reason, 0, 0, 0, 0, 0);
        err = mali_common_ds_consumer_flush_and_wait(consumer);
        _mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_RESUME|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|tlp_reason, 0, 0, 0, 0, 0);
#else
        DS_TRACE_PRINT(("DS: %s %s %s %d consumer 0x%X\n", __FUNCTION__, func, file, line, consumer));
        err = mali_common_ds_consumer_flush_and_wait(consumer);
        DS_TRACE_PRINT(("END %s consumer 0x%X\n", __FUNCTION__, consumer));
#endif
        return err;
}

/** Changing the ref count for when the consumer should be activated.
 * This allows us to flush the consumer before the GP job starts.
 * The default value can be seen as zero. Before flushing the consumer use this function to
 * increase this value to 1. Then the consumer is not activated (and start PP job) immidiately
 * after flush. Then the GP job can be started. When the GP job returns as finished this
 * function should be called again, but now with \a ref_count_change set to -1. This will
 * make the ref_count go to zero, and if all dependencies are met, activate the consumer which
 * starts the PP job. */
MALI_STATIC_FORCE_INLINE void mali_ds_consumer_activation_ref_count_change(mali_ds_consumer_handle consumer, signed int ref_count_change )
{
        mali_common_ds_consumer_activation_ref_count_change(consumer, ref_count_change );
}

/** Release all connections a consumer has. Afterwards the consumer will be in Unused state.
 * This function can not be used if \a mali_ds_consumer_release_ref_count_set_initial()
 * has been called on the consumer. */
MALI_STATIC_FORCE_INLINE void mali_ds_consumer_release_all_connections(mali_ds_consumer_handle consumer_h DS_TRACE_LOG_PARAM )
{
        DS_TRACE_PRINT(("DS: %s %s %s %d consumer 0x%X\n", __FUNCTION__, func, file, line, consumer_h));
        mali_common_ds_consumer_release_ref_count_change(consumer_h, MALI_DS_REF_COUNT_TRIGGER);
        DS_TRACE_PRINT(("END %s %s %s %d consumer 0x%X\n", __FUNCTION__, func, file, line, consumer_h));
}

/** Returning if a consumer is active or not. Useful for assertions */
MALI_STATIC_FORCE_INLINE int mali_ds_consumer_active(mali_ds_consumer_handle consumer_h DS_TRACE_LOG_PARAM )
{
        int retval;
        DS_TRACE_PRINT(("DS: %s %s %s %d consumer 0x%X\n", __FUNCTION__, func, file, line, consumer_h));
        retval = mali_common_ds_consumer_active(consumer_h);
        DS_TRACE_PRINT(("DS: %s %s %s %d consumer 0x%X\n", __FUNCTION__, func, file, line, consumer_h));
        return retval;
}

/** Set a ref count on how many times the \a mali_ds_consumer_release_ref_count_dec() must be called
 * before the resources added to this consumer is released.*/
MALI_STATIC_FORCE_INLINE void mali_ds_consumer_release_ref_count_set_initial(mali_ds_consumer_handle consumer_h, u32 ref_count_initial DS_TRACE_LOG_PARAM )
{
        MALI_DEBUG_ASSERT(ref_count_initial>0, ("Wrong usage."));
        DS_TRACE_PRINT(("DS: %s %s %s %d consumer 0x%X, init: %d\n", __FUNCTION__, func, file, line, consumer_h, ref_count_initial));
        mali_common_ds_consumer_release_ref_count_change(consumer_h, ref_count_initial);
}

/** Decrement the release reference count. Its initial value is set by the function
 * \a mali_ds_consumer_release_ref_count_set_initial(). Current function must afterwards decrement the
 * consumers release reference count the same number of times before the connections will be released.*/

MALI_STATIC_FORCE_INLINE void mali_ds_consumer_release_ref_count_dec(mali_ds_consumer_handle consumer_h DS_TRACE_LOG_PARAM )
{
        DS_TRACE_PRINT(("DS: %s %s %s %d consumer 0x%X\n", __FUNCTION__, func, file, line, consumer_h));
        mali_common_ds_consumer_release_ref_count_change(consumer_h, -1);
        DS_TRACE_PRINT(("END %s consumer 0x%X\n", __FUNCTION__, consumer_h));
}

/**
 * Set what happends when the consumer will release its connections after a call to
 * \a mali_ds_consumer_release_ref_count_change().
 * Normally all connections are released and the consumer is set back to the unused state,
 * but if the mode is set to: MALI_DS_RELEASE_WRITE_GOTO_UNFLUSHED, the read dependencies are not set
 * and the consumer goes back to the unflushed preparing state */
MALI_STATIC_FORCE_INLINE void mali_ds_consumer_release_set_mode(mali_ds_consumer_handle consumer_h, mali_ds_release_mode mode DS_TRACE_LOG_PARAM )
{
        DS_TRACE_PRINT(("DS: %s all=%d %s %s %d consumer 0x%X\n", __FUNCTION__, mode==MALI_DS_RELEASE_ALL, func, file, line, consumer_h));
        mali_common_ds_consumer_release_set_mode(consumer_h, mode);
        DS_TRACE_PRINT(("END %s \n", __FUNCTION__));
}

/** Calling this function will free the object allocated to the consumer handle.
 * The consumer_handle can not be used after calls to this funciton.
 * This is not true if the consumer is currently activated. If so it is freed after the
 * consumer release callback is called.
 * @NOTE To force free, call this first: mali_ds_consumer_release_ref_count_change(consumer, MALI_DS_REF_COUNT_TRIGGER);
 *
*/
MALI_STATIC_FORCE_INLINE void mali_ds_consumer_free(mali_ds_consumer_handle consumer_h DS_TRACE_LOG_PARAM )
{
        DS_TRACE_PRINT(("DS: %s %s %s %d consumer 0x%X\n", __FUNCTION__, func, file, line, consumer_h));
        mali_common_ds_consumer_free(consumer_h);
        DS_TRACE_PRINT(("END %s \n", __FUNCTION__));
}

/** Calling this function will set the consumer into error state.
 * The error state is forwarded as a parameter into to activation and release
 * callback functions for this consumer.
 * After all connections are released from the consumer, the error state will be
 * removed.*/
MALI_STATIC_FORCE_INLINE void mali_ds_consumer_set_error(mali_ds_consumer_handle consumer_h DS_TRACE_LOG_PARAM)
{
        DS_TRACE_PRINT(("DS: %s %s %s %d consumer 0x%X\n", __FUNCTION__, func, file, line, consumer_h));
        mali_common_ds_consumer_set_error(consumer_h);
        DS_TRACE_PRINT(("END %s \n", __FUNCTION__));
}

/**
 * Make a dependency connection to a Consumer from a Resource.
 * You can add many connections to a Consumer. This can be visualized as there are
 * pointing arrows from a lot of Resources to this Consumer. When all the arrows
 * are connected, the user calls flush on the Consumer. If all the arrows are already
 * triggered, the Consumer will be activated and doing its callback during the flush.
 * If some of the Resources are busy, the Consumer will get into a wait state during
 * flush, and it will first be activated with the callback when all the Resources
 * it depends on are available, so that all the connections are triggered.
 * @param consumer_handle Connection to the specified Consumer
 * @param resource_handle Connection from the specified Resource
 * @param rights Sets whether the connection will Read or Write from the Resource
 */
MALI_CHECK_RESULT MALI_STATIC_FORCE_INLINE mali_err_code mali_ds_connect(mali_ds_consumer_handle consumer_handle,
                                          mali_ds_resource_handle resource_handle,
                                          mali_ds_connection_type rights DS_TRACE_LOG_PARAM )
{

        DS_TRACE_PRINT(("DS: %s %s %s %d consumer 0x%X resource: 0x%X\n", __FUNCTION__, func, file, line, consumer_handle, resource_handle));
        return mali_common_ds_connect(consumer_handle, resource_handle, rights);
}

MALI_CHECK_RESULT MALI_STATIC_FORCE_INLINE mali_err_code mali_ds_connect_and_activate_without_callback(mali_ds_consumer_handle arrow_to,
                                                                mali_ds_resource_handle arrow_from,
                                                                mali_ds_connection_type rights DS_TRACE_LOG_PARAM )
{
        DS_TRACE_PRINT(("DS: %s %s %s %d consumer 0x%X resource 0x%X\n", __FUNCTION__, func, file, line, arrow_to));
        return mali_common_ds_connect_and_activate_without_callback(arrow_to, arrow_from, rights);
}

/* Wrap most important functions with logging capabilities using the preprocessor */
#ifndef MALI_DS_IMPLEMENTOR
#if MALI_DS_TRACE
#define mali_ds_resource_allocate(x,y,z) mali_ds_resource_allocate(x,y,z, __FUNCTION__, __FILE__, __LINE__)
#define mali_ds_resource_set_owner(x,y) mali_ds_resource_set_owner(x,y, __FUNCTION__, __FILE__, __LINE__)
#define mali_ds_consumer_set_owner(x,y) mali_ds_consumer_set_owner(x,y, __FUNCTION__, __FILE__, __LINE__)
#define mali_ds_resource_release_connections(x,y,z) mali_ds_resource_release_connections(x,y,z, __FUNCTION__, __FILE__, __LINE__)
#define mali_ds_connect_and_activate_without_callback(x,y,z) mali_ds_connect_and_activate_without_callback(x,y,z, __FUNCTION__, __FILE__, __LINE__)
#define mali_ds_connect(x,y,z) mali_ds_connect(x,y,z, __FUNCTION__, __FILE__, __LINE__)
#define mali_ds_consumer_release_ref_count_set_initial(o,p) mali_ds_consumer_release_ref_count_set_initial(o, p , __FUNCTION__, __FILE__, __LINE__)
#define mali_ds_consumer_release_ref_count_dec(x) mali_ds_consumer_release_ref_count_dec(x, __FUNCTION__, __FILE__, __LINE__)
#define mali_ds_consumer_release_set_mode(x,y) mali_ds_consumer_release_set_mode(x,y,  __FUNCTION__, __FILE__, __LINE__)
#define mali_ds_consumer_free(x) mali_ds_consumer_free(x, __FUNCTION__, __FILE__, __LINE__)
#define mali_ds_consumer_set_error(x) mali_ds_consumer_set_error(x, __FUNCTION__, __FILE__, __LINE__)
#define mali_ds_consumer_release_all_connections(x) mali_ds_consumer_release_all_connections(x, __FUNCTION__, __FILE__, __LINE__)
#if !MALI_TIMELINE_PROFILING_ENABLED
#define mali_ds_consumer_flush_and_wait(x,r) mali_ds_consumer_flush_and_wait(x, __FUNCTION__, __FILE__, __LINE__)
#else
#define mali_ds_consumer_flush_and_wait(x,r) mali_ds_consumer_flush_and_wait(x, r, __FUNCTION__, __FILE__, __LINE__)
#endif
#define mali_ds_consumer_allocate(x,y,z,w) mali_ds_consumer_allocate(x,y,z,w, __FUNCTION__, __FILE__, __LINE__)
#define mali_ds_consumer_flush(x) mali_ds_consumer_flush(x, __FUNCTION__, __FILE__, __LINE__)
#define mali_ds_consumer_active(x) mali_ds_consumer_active(x, __FUNCTION__, __FILE__, __LINE__)
#else
#if !MALI_TIMELINE_PROFILING_ENABLED
#define mali_ds_consumer_flush_and_wait(x,r) mali_ds_consumer_flush_and_wait(x)
#endif
#endif
#endif

#ifndef MALI_DEBUG_SKIP_CODE
/**
 * The function prints a list of all resources this consumer is connected to.
 * For each of these resources it prints all consumers they are connected to again.
 * Don't call this function directly from GDB since it takes the mutex, and if the
 * program is hanging or stopped with a breakpoint in a thread that holds that mutex
 * GDB will block on this mutex as well.
 * Function intended for Debug purposes only.
 * @note From GDB use the equivalent function \a debug_ds_system_print_consumer() instead
 * @param consumer Handle to the Consumer
 */
MALI_STATIC_FORCE_INLINE void mali_dependency_system_debug_print_consumer(mali_ds_consumer_handle consumer)
{
        mali_common_ds_system_debug_print_consumer(consumer);
}

/**
 * The function prints a list of all consumers that is connected to this resource.
 * For each of these consumers it prints all resources they are connected to again.
 * Don't call this function directly from GDB since it takes the mutex, and if the
 * program is hanging or stopped with a breakpoint in a thread that holds that mutex
 * GDB will block on this mutex as well.
 * Function intended for Debug purposes only.
 * @note From GDB use the equivalent function \a debug_ds_system_print_resource() instead
 * @param consumer Handle to the Consumer
 */
MALI_STATIC_FORCE_INLINE void mali_dependency_system_debug_print_resource(mali_ds_resource_handle resource)
{
        mali_common_ds_system_debug_print_resource(resource);
}

#elif __SYMBIAN32__
MALI_STATIC_FORCE_INLINE void mali_dependency_system_debug_print_consumer(mali_ds_consumer_handle consumer)
{ }

MALI_STATIC_FORCE_INLINE void mali_dependency_system_debug_print_resource(mali_ds_resource_handle resource)
{ }

/** GDB Debugging
 * If the program goes into a deadlock state, it is possible to connect GDB into it and find
 * what it blocks on. To connect to a program with gdb: "attatch PID"
 * Then it is possible to call the internals of the print functions:
 * \a mali_dependency_system_debug_print_consumer() \a mali_dependency_system_debug_print_consumer()
 * which should not be called from GDB.
 * The internals that does not take the mutexes and Can be called from GDB are called:
 * \a debug_ds_system_print_consumer(consumer) and \a debug_ds_system_print_resource(resource) .
 * To run this function: "call debug_ds_system_print_consumer(0xADDR_CONSUMER)" */

#endif /* #ifndef MALI_DEBUG_SKIP_CODE */

#ifdef __cplusplus
}
#endif

#endif /*_MALI_DEPENDENCY_SYSTEM_H */
