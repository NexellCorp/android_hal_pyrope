/*
 * This confidential and proprietary software may be used only as
 * authorized by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef M200_FBFRAME_H
#define M200_FBFRAME_H

#include <shared/m200_gp_frame_builder.h>
#include "m200_gp_frame_builder_struct.h"

/**
 * Allocate one single frame.
 *
 * @param base_ctx The base context to use for mali resource allocations.
 * @param frame_builder The framebuilder to set up the frame.
 * @return An allocated mali_internal_frame that indicates if the frame have been allocated succesfully, NULL in other case.
 */
mali_internal_frame* _mali_frame_alloc( mali_base_ctx_handle base_ctx, mali_frame_builder *frame_builder);

/**
 * Releases all resources held by a mali_internal_frame.
 * Used during frame builder release and during cleanups after failed initialization of new frames.
 * @param frame The frame to free
 */
void _mali_frame_free( mali_internal_frame *frame );

/**
 * Resets a mali_internal_frame. This includes executing callbacks, freeing memory added
 * to the garbage collector, resetting the heap, resets the gp job and projobs, 
 * and clean up a pending CoW if the PP activation callback was missed due to an OOM during flush.
 */
void _mali_frame_reset( mali_internal_frame *frame );

/**
 * Wait for all mali hw processing of the frame to finish and the callbacks to complete,
 * once it is done waiting, unlock the mutex and return.
 */
void _mali_frame_wait( mali_internal_frame *frame );

/**
 * Internal helper function that returns with the calling thread holding the frames mutex
 * after any rendering has finished. Necessary to avoid race conditions when waiting for
 * HW processing to complete.
 * @param frame The frame to wait for and take the mutex from
 */
void _mali_frame_wait_and_take_mutex( mali_internal_frame *frame );

#endif /* M200_FBFRAME_H */
