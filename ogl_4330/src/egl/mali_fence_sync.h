/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALI_FENCE_SYNC_H
#define MALI_FENCE_SYNC_H

/* This file only offers functionality useful is MALI_EXTERNAL_SYNC is defined. 
 * The functions below should not be called if that is not set */
#if MALI_EXTERNAL_SYNC == 1

#include <mali_system.h>
#include <sync/mali_external_sync.h>


/**
 * A mali fence sync object is a per-thread accumulator, that tracks all flushes happening on a thread up to the current point in time.
 * It also provides an accumulated mali_fence_handle for all those flushes, which can be used for EGL fence objects. 
 *
 * It does so by accumulating a mali_fence_handles produced by all pp_job_start calls within a given thread.
 * This file holds and controls all the callbacks to the framebuilder required to make that happen. 
 *
 * Unfortunately, since native sync handles are only given on job start, there may be jobs pending in the dependency system. 
 * We still want to track those, and we do so through a simple counter. Only when the counter is 0 will the accumulated_sync be 
 * good to wait for. 
 *
 */

/* prototype declaration */
struct mali_fence_sync;

/**
 * @brief Create a new mali_fence_sync object
 * @return A new object
 */
struct mali_fence_sync* _mali_fence_sync_alloc( void );

/**
 * @brief Frees a mali_fence_sync object
 * @param sync The object to delete
 */
void _mali_fence_sync_free( struct mali_fence_sync* sync );

/**
 * @brief A function that should be called whenever a flush occurs (a pp job is created)
 * @param sync - The object to notify
 */
void _mali_fence_sync_notify_job_create( struct mali_fence_sync* sync );

/**
 * @brief A function that should be called whenever a job is actually started
 * @param sync - The object to notify
 * @param success - Whether the job succeeded or not (TRUE = started successfully, FALSE = did not start for various reasons)
 * @param fence - The fence handle of the job that was just started. Equal to MALI_FENCE_INVALID_HANDLE if success = FALSE.
 */
void _mali_fence_sync_notify_job_start( struct mali_fence_sync* sync, mali_bool success, const mali_fence_handle fence);

/**
 * @brief Extracts a fully accumulated  mali_fence_handle from the mali_fence_sync. 
 *        It is typically only called in eglCreateFence. 
 * @param sync - The sync object to extract from 
 * @return A fully accumulated sync handle. 
 *
 * @note  This function will block until all pending jobs are gone. Use sparingly. 
 *
 */
mali_fence_handle _mali_fence_sync_get_fence_handle( struct mali_fence_sync* sync );

#endif /* MALI_EXTERNAL_SYNC == 1 */

#endif /* MALI_FENCE_SYNC_H */
