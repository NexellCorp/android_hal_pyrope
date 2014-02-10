/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_draw.h
 * @brief Functions to draw primitives
 */

#ifndef _GLES_DRAW_H_
#define _GLES_DRAW_H_

#include <gles_base.h>

#include "gles_instrumented.h"
#include "gles_framebuffer_object.h"
#include "mali_gp_geometry_common/gles_gb_buffer_object.h"

#include "gles_sw_counters.h"

struct gles_scissor;

/**
 * @brief Performs error checking for calls to glDrawElements of both the OpenGL ES 1.x and OpenGL ES 2.x
 *        drivers
 * @param mode  What kind of primitives to render
 * @param count The number of indices to render
 * @param type  The type of the values in indices
 * @return      GL_NO_ERROR if all arguments were validated correctly, an error code otherwise
 */
GLenum _gles_draw_elements_error_checking(GLenum mode, GLint count, GLenum type);

/**
 * @brief Performs error checking for calls to glDrawArrays of both the OpenGL ES 1.x and OpenGL ES 2.x
 *        drivers
 * @param mode  What kind of primitives to render
 * @param first The starting index in the enabled arrays
 * @param count The number of vertices to render
 * @return      GL_NO_ERROR if all arguments were validated correctly, an error code otherwise
 */
GLenum _gles_draw_arrays_error_checking(GLenum mode, GLint first, GLint count);

/**
 * @brief Performs the parts of the glDrawElements initialization that is common to both
 *        OpenGL ES 1.x and OpenGL ES 2.x
 * @param ctx     The OpenGL ES context to initialize a draw call for
 * @param count   The number of indices to render
 * @param type    The type of the values in indices
 * @param mode	  The mode of the drawcall (i.e. points, lines, etc)
 * @param indices The indices of the element draw call
 * @param idx_range     A pointer returns index ranges calculated during this func
 * @param range_count   A pointer to a u32 that records the index range count
 * @param coherence    The factor to measure how randomly and disordered the indices are in the index buffer 
 * @return        An error code if the function failed, MALI_ERR_NO_ERROR otherwise
 */
mali_err_code _gles_init_draw_elements( gles_context *ctx, GLint count, GLenum type, GLenum mode, const void *indices, index_range **idx_range, u32 *range_count, u32 *coherence );

/**
 * @brief Performs the parts of the glDrawArrays initialization that is common to both
 *        OpenGL ES 1.x and OpenGL ES 2.x
 * @param ctx  The OpenGL ES context to initialize a draw call for
 * @param mode The mode of the drawcall (i.e. points, lines, etc)
 * @param count   The number of indices to render
 * @return     An error code if the function failed, MALI_ERR_NO_ERROR otherwise
 */
mali_err_code _gles_init_draw_arrays( struct gles_context *ctx, GLenum mode, GLint count );

#if MALI_SW_COUNTERS_ENABLED 
/**
 * @brief Adds counts appropriate to specific drawcalls
 * @param counters The counters structure to fill
 * @param mode the drawmode (LINES, POINTS, TRIANGLES, TRIANGLE_STRIP etc) of this drawcall
 * @param count The value to increment the counter by.
 */
MALI_STATIC_INLINE void _gles_add_drawcall_info(mali_sw_counters *counters, GLenum mode, GLint increment)
{
    MALI_DEBUG_ASSERT_POINTER( counters );

	switch (mode)
	{
		case GL_POINTS:
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, DRAW_POINTS, 1);
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, POINTS_COUNT, increment);
		break;
		case GL_LINES:
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, DRAW_LINES, 1);
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, LINES_COUNT, increment/2);
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, INDEPENDENT_LINES_COUNT, increment/2);
		break;
		case GL_LINE_LOOP:
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, DRAW_LINE_LOOP, 1);
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, LINES_COUNT, increment);
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, LOOP_LINES_COUNT, increment);
		break;
		case GL_LINE_STRIP:
			MALI_DEBUG_ASSERT(increment >= 1, ("Line count increment < 1 in GL_LINE_STRIP"));
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, DRAW_LINE_STRIP, 1);
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, LINES_COUNT, increment-1);
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, STRIP_LINES_COUNT, increment-1);
		break;
		case GL_TRIANGLES:
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, DRAW_TRIANGLES, 1);
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, TRIANGLES_COUNT, increment/3);
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, INDEPENDENT_TRIANGLES_COUNT, increment/3);
		break;
		case GL_TRIANGLE_STRIP:
			MALI_DEBUG_ASSERT(increment >= 2, ("Line count increment < 2 in GL_TRIANGLE_STRIP"));
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, DRAW_TRIANGLE_STRIP, 1);
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, TRIANGLES_COUNT, increment-2);
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, STRIP_TRIANGLES_COUNT, increment-2);
		break;
		case GL_TRIANGLE_FAN:
			MALI_DEBUG_ASSERT(increment >= 2, ("Line count increment < 2 in GL_TRIANGLE_FAN"));
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, DRAW_TRIANGLE_FAN, 1);
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, TRIANGLES_COUNT, increment-2);
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, FAN_TRIANGLES_COUNT, increment-2);
		break;
	}
}
#endif

/**
 * @brief Internal function that starts a glDrawElements draw call. Draw call initialization and
 *        initialization of the fb backends has to be completed before calling this function
 * @param ctx     The OpenGL ES context to perform the draw call with
 * @param mode    What kind of primitives to render
 * @param count   The number of indices to render
 * @param type    The type of the values in indices
 * @param indices The indices of the element draw call
 * @param idx_range  The arrays stores the smallest and largest indices for each range
 * @param range_count The count of the idx_range
 * @param coherence    The factor to measure how randomly and disordered the indices are in the index buffer 
 * @return        An error code if the function failed, MALI_ERR_NO_ERROR otherwise
 */
MALI_STATIC_INLINE mali_err_code _gles_draw_elements_internal( struct gles_context *ctx, GLenum mode, GLint count, GLenum type, const void *indices, const index_range *idx_range, const index_range *idx_range_buffer, u32 range_count, u32 coherence )
{
	u32 transformed_count = 0;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->frame_pool );

	MALI_CHECK_NO_ERROR( _gles_gb_draw_indexed_range( ctx, mode, idx_range, idx_range_buffer, range_count, count, &transformed_count, type, coherence, indices ) );

#if MALI_SW_COUNTERS_ENABLED
	{
		mali_sw_counters *counters;
		counters = _gles_get_sw_counters(ctx);

		if(counters != NULL)
		{
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, DRAW_ELEMENTS_CALLS, 1);
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, DRAW_ELEMENTS_NUM_INDICES, count);
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, DRAW_ELEMENTS_NUM_TRANSFORMED, transformed_count);
		}

		_gles_add_drawcall_info(counters, mode, count);
	}
#endif /* MALI_SW_COUNTERS_ENABLED */

	_profiling_count( INST_GL_DRAW_ELEMENTS_CALLS, 1);
	_profiling_count( INST_GL_DRAW_ELEMENTS_NUM_INDICES, count);
	_profiling_count( INST_GL_DRAW_ELEMENTS_NUM_TRANSFORMED, transformed_count);

	_gles_add_instrumented_drawcall_info(_instrumented_get_context(), mode, count);

	return MALI_ERR_NO_ERROR;
}

/**
 * @brief Internal function that starts a glDrawArrays draw call. Draw call initialization and
 *        initialization of the fb backends has to be completed before calling this function
 * @param ctx   The OpenGL ES context to perform the draw call with
 * @param mode  What kind of primitives to render
 * @param first The starting index in the enabled arrays
 * @param count The number of vertices to render
 * @return      An error code if the function failed, MALI_ERR_NO_ERROR otherwise
*/
MALI_STATIC_INLINE mali_err_code _gles_draw_arrays_internal( struct gles_context *ctx, GLenum mode, GLint first, GLint count )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->frame_pool );

	MALI_CHECK_NO_ERROR( _gles_gb_draw_nonindexed( ctx, mode, first, count ) );

#if MALI_SW_COUNTERS_ENABLED
	{
	    mali_sw_counters *counters = _gles_get_sw_counters(ctx);

		if(counters != NULL)
		{
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, DRAW_ARRAYS_CALLS, 1);
			MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, DRAW_ARRAYS_NUM_TRANSFORMED, count);
		}

		_gles_add_drawcall_info(counters, mode, count);
	}
#endif /* MALI_SW_COUNTERS_ENABLED */

	_profiling_count( INST_GL_DRAW_ARRAYS_CALLS, 1);
	_profiling_count( INST_GL_DRAW_ARRAYS_NUM_TRANSFORMED, count);

	_gles_add_instrumented_drawcall_info(_instrumented_get_context(), mode, count);

	return MALI_ERR_NO_ERROR;
}

/**
 * @brief Round down the vertex count to the biggest value smaller than count that gives an
 *        integer amount of primitives
 * @param mode  The kind of primitives
 * @param count The vertex count to be rounded down
 * @return      The vertex count after it has been rounded down to an integer amount
 */
GLsizei _gles_round_down_vertex_count(GLenum mode, GLsizei count);

/**
 * @brief Checks for a zero-sized scissor box
 * @param ctx   		The OpenGL ES context
 * @param frame_builder The frame builder to use for clamping the scissor dimensions
 * @return MALI_TRUE if the scissor box is zero-sized.
 * @note This check is required as Mali200 cannot encode zero-sized scissor box that is:
 * @note scissorbox with width or height equals to zero and
 * @note scissor box that does not intersect the frame.
 */
mali_bool _gles_scissor_zero_size_check( struct gles_context * const ctx, struct mali_frame_builder *frame_builder );

#endif /* _GLES_DRAW_H_ */
