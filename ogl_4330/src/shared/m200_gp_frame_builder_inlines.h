/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef M200_GP_FRAME_BUILDER_INLINES_H
#define M200_GP_FRAME_BUILDER_INLINES_H

#include "m200_gp_frame_builder_struct.h"

/**
* Set the size of the callback list for this frame
* @param frame The frame to set
* @param room The size of the callback pointer table
* @return MALI_ERR_NO_ERROR if successful, MALI_ERR_OUT_OF_MEMORY if not.
*/
MALI_IMPORT MALI_CHECK_RESULT mali_err_code _mali_frame_callback_list_set_room(mali_internal_frame *frame, int room);

/**
* Add user-defined callback functions which will be executed when it is guaranteed that the frame will not be re-used.
* I.e. when it is reset or it has been "swapped out" through _mali_frame_builder_swap().
* @param frame_builder The frame to which the callback is added
* @param cb_func A user-supplied callback which will be executed when rendering has completed. Can be NULL
* @param cb_param The parameter for cb_func
* @param can_be_deferred If the callback function can be deferred or not
* @return MALI_ERR_NO_ERROR If the initialization was successful, otherwise an error explaining what went wrong.
*/
MALI_STATIC_FORCE_INLINE mali_err_code _mali_frame_add_callback( mali_internal_frame *frame,
																mali_frame_cb_func cb_func,
																mali_frame_cb_param cb_param,
																mali_bool can_be_deferred )
{
	MALI_DEBUG_ASSERT_POINTER( frame );
	MALI_DEBUG_ASSERT( NULL != cb_func, ("Passing a NULL-pointer as cb_func\n") );
	MALI_DEBUG_ASSERT( frame->state != FRAME_UNMODIFIED , ("Adding a callback to an unmodified frame! \n"));
	MALI_DEBUG_ASSERT( frame->state != FRAME_RENDERING ||  MALI_ERR_NO_ERROR != _mali_sys_mutex_try_lock(frame->mutex),
					   ("cannot add a callback to a rendering frame without taking the mutex first!!") );

	/* make sure callback list is big enough */
	if (frame->callback_list_size == frame->callback_list_room)
	{
		const int grow_factor = 2; /* double list size if it's too small - this gives O(log n) allocations instad of O(n) */
		mali_err_code err = _mali_frame_callback_list_set_room(frame, frame->callback_list_room * grow_factor);
		if (MALI_ERR_NO_ERROR != err)
		{
			return MALI_ERR_OUT_OF_MEMORY;
		}
	}
	MALI_DEBUG_ASSERT(frame->callback_list_size < frame->callback_list_room, ("list not extended as expected"));

	/* add the callback to the list that is executed on _frame_free() and _frame_reset() */
	frame->callback_list[frame->callback_list_size].func = cb_func;
	frame->callback_list[frame->callback_list_size].param = cb_param;
	frame->callback_list[frame->callback_list_size].can_be_deferred = can_be_deferred;
	frame->callback_list_size++;

	if ( MALI_FALSE == can_be_deferred ) frame->have_non_deferred_callbacks = MALI_TRUE;

	return MALI_ERR_NO_ERROR;
}

MALI_STATIC_FORCE_INLINE mali_err_code _mali_frame_builder_add_callback_internal( mali_frame_builder *frame_builder,
                                                            mali_frame_cb_func cb_func,
                                                            mali_frame_cb_param cb_param,
                                                            mali_bool can_be_deferred )
{
	mali_internal_frame* frame;
	MALI_DEBUG_ASSERT_POINTER(frame_builder);
	frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	return _mali_frame_add_callback( frame, cb_func, cb_param, can_be_deferred );
}


/**
 * Add user-defined callback functions which will be executed when it is guaranteed that the frame will not be re-used.
 * I.e. when it is reset or it has been "swapped out" through _mali_frame_builder_swap().
 * @param frame_builder The frame builder defining the frame to which the callback is added
 * @param cb_func A user-supplied callback which will be executed when rendering has completed. Can be NULL
 * @param cb_param The parameter for cb_func
 * @return MALI_ERR_NO_ERROR If the initialization was successful, otherwise an error explaining what went wrong.
 */
MALI_STATIC_FORCE_INLINE mali_err_code _mali_frame_builder_add_callback( mali_frame_builder *frame_builder,
                                                            mali_frame_cb_func cb_func,
                                                            mali_frame_cb_param cb_param )
{
	return _mali_frame_builder_add_callback_internal( frame_builder, cb_func, cb_param, MALI_TRUE );
}


/**
 * Add user-defined callback functions which will be executed when it is guaranteed that the frame will not be re-used.
 * The callback will be marked as non deferred, meaning that the callback have to be executed in place,
 * for example when the frame is reset.
 * @param frame_builder The frame builder defining the frame to which the callback is added
 * @param cb_func A user-supplied callback which will be executed when rendering has completed. Can be NULL
 * @param cb_param The parameter for cb_func
 * @return MALI_ERR_NO_ERROR If the initialization was successful, otherwise an error explaining what went wrong.
 */
MALI_STATIC_FORCE_INLINE mali_err_code _mali_frame_builder_add_callback_non_deferred( mali_frame_builder *frame_builder,
                                                            mali_frame_cb_func cb_func,
                                                            mali_frame_cb_param cb_param )
{
	return _mali_frame_builder_add_callback_internal( frame_builder, cb_func, cb_param, MALI_FALSE );
}

/**
 * Add user-defined callback functions which will be executed when it is guaranteed that the frame will not be re-used.
 * This version of the function is threadsafe. use it when not inside a fbuilder_write_lock. 
 * @param frame_builder The frame builder defining the frame to which the callback is added
 * @param cb_func A user-supplied callback which will be executed when rendering has completed. Can be NULL
 * @param cb_param The parameter for cb_func
 *
 * @return MALI_ERR_NO_ERROR If the setup was successful.
 *         MALI_ERR_OUT_OF_MEMORY if the setup failed due to memory concerns
 *         MALI_ERR_FUNCTION_FAILED if the setup failed due to the frame being in UNMODIFIED state. 
 *           No callbacks should be added on this framestate, as it represents a frame without any data on it. 
 *           Do a writelock/clean and create a frame worthy of flushing, or just deal with the error by executing 
 *           the callback immediately.  
 */
MALI_STATIC_FORCE_INLINE mali_err_code _mali_frame_builder_add_callback_threadsafe( mali_frame_builder *frame_builder,
                                                            mali_frame_cb_func cb_func,
                                                            mali_frame_cb_param cb_param )
{
	mali_internal_frame* frame;
	mali_err_code err;
	MALI_DEBUG_ASSERT_POINTER(frame_builder);
	frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	MALI_DEBUG_ASSERT_POINTER(frame);
	
	/* lock the frame to check the frame state. */
	_mali_sys_mutex_lock(frame->mutex);

	/* Two conditions to deal with:
	 *
	 *  - UNMODIFIED frames should never have callbacks on them. 
	 *    These represent frames that have no work, and waiting for that work to finish make no sense. 
	 *
	 *  - If you have a FRAME_RENDERING frame, make sure it is not swapped. 
	 *    If it has been swapped, it is a yet-finished frame from a previous swapbuffers call. 
	 *    Adding a callback on this is incorrect; the frame is actually UNMODIFIED. 
	 *
	 *    Example with a swap count of 3:  
	 *      eglSwapBuffers -> frame 1 starts out CLEAN, now enters RENDERING. Callback added to frame 1. 
	 *      eglSwapBuffers -> frame 2 stats out UNMODIFIED. Callback not added, earlying out. 
	 *      eglSwapBuffers -> frame 3 starts out as UNMODIFIED. Callback not added. earlying out. 
	 *      eglSwapBuffers -> frame 4 overlaps with frame 1. If frame 1 is not yet done, it will still be RENDERING.
	 *                        Adding a callback here is effectively adding a callback on frame 1, not frame 4.
	 *                        Strictly speaking, frame 4 has is still UNMODIFIED. Taking same path as conditino 1.
	 */

	if(frame->state == FRAME_UNMODIFIED || 
	   (frame->state == FRAME_RENDERING && frame->reset_on_finish) )
	{
		_mali_sys_mutex_unlock(frame->mutex);
		return MALI_ERR_FUNCTION_FAILED; 
	}

	err = _mali_frame_add_callback( frame, cb_func, cb_param, MALI_TRUE );
	_mali_sys_mutex_unlock(frame->mutex);

	return err;
}


/**
 * Get the gp_job for the frame currently being built
 * @param frame_builder The frame builder defining the frame
 * @return An actual gp_job_handle. Will never return MALI_NO_HANDLE
 */
MALI_STATIC_FORCE_INLINE mali_gp_job_handle _mali_frame_builder_get_gp_job( mali_frame_builder *frame_builder )
{
	mali_internal_frame* frame;
	MALI_DEBUG_ASSERT_POINTER(frame_builder);
	frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );

	MALI_DEBUG_ASSERT_POINTER( frame );
	/* return the gp job belonging to the current frame */
	return frame->gp_job;
}

/**
 * Get the render width of the framebuilder based on the current attachments
 * @param frame_builder The frame builder defining the frame
 * @return Width in pixels
 */
MALI_STATIC_INLINE u32 _mali_frame_builder_get_width( mali_frame_builder *frame_builder )
{
	MALI_DEBUG_ASSERT_POINTER(frame_builder);
	return frame_builder->output_width;
}
/**
 * Get the render height of the frame based on the current attachments
 * @param frame_builder The frame builder defining the frame
 * @return Height in pixels
 */
MALI_STATIC_INLINE u32 _mali_frame_builder_get_height( mali_frame_builder *frame_builder )
{
	MALI_DEBUG_ASSERT_POINTER(frame_builder);
	return frame_builder->output_height;
}

/**
 * @brief Returns a the frame-pool for the current frame, if one is not present
 * @param frame_builder The frame_builder to construct the frame-pool for
 * @return A valid pointer to an initialized mali_mem_pool, or NULL if none existed (call use/writelock first)
 */
MALI_STATIC_FORCE_INLINE mali_mem_pool *_mali_frame_builder_frame_pool_get( mali_frame_builder *frame_builder )
{
	mali_internal_frame* frame;
	MALI_DEBUG_ASSERT_POINTER(frame_builder);
	frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );
	return &frame->frame_pool;
}


/**
 * @brief Returns a mali_bool flag if fp16 format is enabled
 * @param frame_builder The frame_builder active at the moment
 * @return A mali_bool flag
 */
MALI_STATIC_FORCE_INLINE mali_bool _mali_frame_builder_get_fp16_flag( mali_frame_builder *frame_builder )
{
	MALI_DEBUG_ASSERT_POINTER(frame_builder);
	return frame_builder->fp16_format_used;
}

/**
* Update the fragment stack requirements for the current frame
* @param frame The current frame
* @param stack_start The initial stack position required for a specific shader
* @param stack_size The stack size required for a specific shader
*/
MALI_STATIC_FORCE_INLINE void _mali_frame_update_fragment_stack( mali_internal_frame *frame, u32 stack_start, u32 stack_size )
{
	u32 stack_grow;
	MALI_DEBUG_ASSERT_POINTER( frame );

	/* store the maximum stack requirements for the fragment shader */
	stack_grow = stack_size - stack_start;

	frame->fs_stack.start = MAX( frame->fs_stack.start, stack_start );
	frame->fs_stack.grow = MAX( frame->fs_stack.grow, stack_grow );
}

/**
* Get the gp_job for the frame currently being built
* @param frame The current frame
* @return An actual gp_job_handle. Will never return MALI_NO_HANDLE
*/
MALI_STATIC_FORCE_INLINE mali_gp_job_handle _mali_frame_get_gp_job( mali_internal_frame *frame )
{
	MALI_DEBUG_ASSERT_POINTER( frame );
	MALI_DEBUG_ASSERT( ( frame->state != FRAME_CLEAN),
	("must call _mali_frame_builder_use() before this function\n") );

	/* return the gp job belonging to the current frame */
	return frame->gp_job;
}

/**
* Returns whether incremental rendering is requested or not. Will only return true
* if all conditions for correct incremental rendering are true.
* @param frame_builder The frame builder defining the frame
* @param frame The current frame
*/
MALI_STATIC_FORCE_INLINE mali_bool _mali_frame_incremental_rendering_requested( mali_frame_builder *frame_builder, mali_internal_frame *frame )
{
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	MALI_DEBUG_ASSERT_POINTER( frame );
	return (frame->num_flushes_since_reset > frame_builder->incremental_render_flush_threshold);
}

/**
 * @brief Return the unique ID of the current frame in the framebuilder. 
 * @note The ID is only valid after calling _mali_frame_builder_writelock / _mali_frame_builder_use.
 *       Clearing the frame will give it a new ID. 
 * @return an integer unique to the current frame.
 */
MALI_STATIC_FORCE_INLINE mali_base_frame_id _mali_frame_builder_get_current_frame_id( mali_frame_builder* frame_builder )
{
	mali_internal_frame *frame;
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );
	MALI_DEBUG_ASSERT_POINTER( frame );
	MALI_DEBUG_ASSERT( frame->state == FRAME_DIRTY, ("Need to call write_lock/use before calling this") );

	return frame->frame_id;
}

#if MALI_SW_COUNTERS_ENABLED
/**
 * @brief Get the software counter table.
 * @param frame_builder The framebuild holding the counter table
 * @return A pointer to the table of counters.
 */
MALI_STATIC_FORCE_INLINE mali_sw_counters * _mali_frame_builder_get_counters(mali_frame_builder* frame_builder)
{
	mali_internal_frame *frame;
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	frame = GET_CURRENT_INTERNAL_FRAME( frame_builder );
	MALI_DEBUG_ASSERT_POINTER( frame );
	MALI_DEBUG_ASSERT( frame->state == FRAME_DIRTY, ("Need to call write_lock/use before calling this") );

	return frame->sw_counters;
}
#endif


#endif /* M200_GP_FRAME_BUILDER_INLINES_H */
