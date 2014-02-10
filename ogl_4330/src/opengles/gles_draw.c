/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_debug.h>
#include <base/mali_types.h>

#include "gles_draw.h"
#include "gles_state.h"
#include "gles_context.h"
#include "gles_util.h"
#include <shared/m200_gp_frame_builder.h>
#include "shared/m200_gp_frame_builder_inlines.h"
#include "gles_instrumented.h"
#include "gles_buffer_object.h"
#include "gles_framebuffer_object.h"
#include "gles_gb_interface.h"
#include "mali_gp_geometry_common/gles_gb_buffer_object.h"
#include "mali_gp_geometry_common/gles_gb_vertex_rangescan.h"
#include <gles_incremental_rendering.h>

/**
 * @brief does the error checking for legal modes and the program object state
 * @param mode the GL draw mode
 * @param count A pointer to the number of indices/vertices to draw.
 * @return returns MALI_ERR_NO_ERROR if there was no error
 */
MALI_STATIC GLenum _gles_draw_error_checking( GLenum mode, GLint count )
{
	/*	mode is a GLenum which must be any of these GL_POINTS, GL_LINES, GL_LINE_LOOP, GL_LINE_STRIP, GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN.
		gl defines these as GL_POINTS 0 up to GL_TRIANGLE_FAN 6.
		This code relies on these defines never changing.
	*/
#if ( GL_POINTS != 0 ) || (GL_TRIANGLE_FAN != 6)
#error msg "This code depends on the definition of GL_POINTS and GL_TRIANGLE_FAN"
#endif

	/* verify that mode is a valid enum
	 * The test for mode >= GL_POINTS is omitted because apparently
	 * GLenum is sometimes an unsigned int?
	 */
	MALI_CHECK( mode <= GL_TRIANGLE_FAN , GL_INVALID_ENUM );

	/* verify the count is valid */
	MALI_CHECK( count >= 0, GL_INVALID_VALUE );

	GL_SUCCESS;
}

GLenum _gles_draw_elements_error_checking(GLenum mode, GLint count, GLenum type)
{
	GLenum err;
	err = _gles_draw_error_checking( mode, count );
	MALI_CHECK(GL_NO_ERROR == err, err);

	MALI_CHECK( ( GL_UNSIGNED_BYTE == type ) || ( GL_UNSIGNED_SHORT == type ), GL_INVALID_ENUM );

	GL_SUCCESS;
}

GLenum _gles_draw_arrays_error_checking(GLenum mode, GLint first, GLint count)
{
	GLenum err;
	err = _gles_draw_error_checking( mode, count );
	MALI_CHECK(GL_NO_ERROR == err, err);

	MALI_CHECK(first >= 0, GL_INVALID_VALUE);

	GL_SUCCESS;
}

/**
 * @brief  Initialization common to all drawcalls
 * @param  ctx  A pointer to the GL context
 * @param  mode The current mode (points, lines, etc)
 * @param  count   The number of indices to render
 * @return None
 */
MALI_STATIC mali_err_code _gles_init_draw_common( gles_context * ctx, GLenum mode, mali_bool is_indexed, GLint count )
{
	/* from the gl.h header:
	 *	GL_TRIANGLES                      0x0004
	 *	GL_TRIANGLE_STRIP                 0x0005
	 *	GL_TRIANGLE_FAN                   0x0006
	 * therefore the test for 3rd bit on is equivalent to testing for triangles.
	 * the range is tested further up the call stack.
	 */
	const mali_bool is_triangle = ( 0 != (mode & 0x0004) );

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( _gles_get_current_draw_frame_builder( ctx ) );

	if(is_triangle)
	{
		/* this fast path also sets the indexed flag by doing a runtime set of the masked value */
		mali_statebit_raw_set( &ctx->state.common,	(1 << MALI_STATE_MODE_TRIANGLE_TYPE) |
													(1 << MALI_STATE_MODE_LINE_TYPE) |
													(1 << MALI_STATE_MODE_POINT_TYPE) |
													(1 << MALI_STATE_MODE_INDEXED),
							(1 << MALI_STATE_MODE_TRIANGLE_TYPE) | (is_indexed << MALI_STATE_MODE_INDEXED) );

		/* double the count if strips or fans */
		if( 0 != (mode & 0x0003) ) count <<= 1;
	}
	else
	{
		const mali_bool is_line = (mode == GL_LINES) || (mode == GL_LINE_STRIP) || (mode == GL_LINE_LOOP);
		const mali_bool is_point = (mode == GL_POINTS);

		/* We use statebit_exchange as a convenient way to set/unset as needed
		 */
		(void) mali_statebit_exchange( &ctx->state.common, MALI_STATE_MODE_TRIANGLE_TYPE, is_triangle );
		(void) mali_statebit_exchange( &ctx->state.common, MALI_STATE_MODE_LINE_TYPE, is_line );
		(void) mali_statebit_exchange( &ctx->state.common, MALI_STATE_MODE_POINT_TYPE, is_point );
		(void) mali_statebit_exchange( &ctx->state.common, MALI_STATE_MODE_INDEXED, is_indexed );
	}

	if( _gles_get_current_draw_supersample_scalefactor( ctx ) > 1 )
	{
		mali_statebit_set( &ctx->state.common, MALI_STATE_MODE_SUPERSAMPLE_16X );
	}
	else
	{
		mali_statebit_unset( &ctx->state.common, MALI_STATE_MODE_SUPERSAMPLE_16X );
	}

	_gles_fbo_increase_num_verts_drawn_per_frame(ctx->state.common.framebuffer.current_object, count);

	if ( _gles_fbo_get_num_verts_drawn_per_frame(ctx->state.common.framebuffer.current_object) > GLES_TRIANGLES_DRAWN_BEFORE_INCREMENTAL_RENDERING )
	{
		_gles_fbo_reset_num_verts_drawn_per_frame(ctx->state.common.framebuffer.current_object);
		MALI_CHECK_NO_ERROR( _gles_incremental_render(ctx, ctx->state.common.framebuffer.current_object) );
	}

	return MALI_ERR_NO_ERROR;
}

MALI_STATIC mali_bool _gles_draw_scissor_viewpoint_check( struct gles_context * const ctx, struct mali_frame_builder *frame_builder )
{

	MALI_DEBUG_ASSERT_POINTER( ctx );

#if MALI_USE_GLES_2
	if ( GLES_API_VERSION_2 == ctx->api_version )
	{
		struct gles_viewport *viewport;
		viewport = &ctx->state.common.viewport;

		if ( ( 0 == viewport->width ) || ( 0 == viewport->height ) )
		{
			return MALI_TRUE;
		}
	}
#endif

	return _gles_scissor_zero_size_check( ctx, frame_builder );
}

mali_err_code _gles_init_draw_elements( gles_context *ctx, GLint count, GLenum type, GLenum mode, const void *indices, index_range **idx_vs_range, u32 *vs_range_count, u32 *coherence )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	/* Early-out if dealing with a zero-sized scissor box */
	if(GL_TRUE == _gles_draw_scissor_viewpoint_check( ctx, _gles_get_current_draw_frame_builder(ctx) ))
	{
		MALI_INSTRUMENTED_LOG((_instrumented_get_context(), MALI_LOG_LEVEL_WARNING,
				"glDrawElements draw call issued with GL_SCISSOR_TEST enabled and a scissor box "
				"with width or height equal to 0 - draw call ignored"));
		return MALI_ERR_EARLY_OUT;
	}

	MALI_CHECK_NO_ERROR( _gles_init_draw_common( ctx, mode, MALI_TRUE, count ) );

	/* handle index buffer objects */
	if ( ctx->state.common.vertex_array.element_vbo != NULL )
	{
		unsigned int element_size = 0;
		int offset = (int)indices; /* indices-pointer is now an offset */

		struct gles_buffer_object *vbo = ctx->state.common.vertex_array.element_vbo;
		struct gles_gb_buffer_object_data *gb_vbo = vbo->gb_data;

		/* if no index-buffer uploaded, silently terminate draw-call. */
		if (NULL == gb_vbo)
		{
			MALI_INSTRUMENTED_LOG((_instrumented_get_context(), MALI_LOG_LEVEL_WARNING,
					"glDrawElements called with no index buffer data uploaded to the buffer object "
					"bound to GL_ELEMENT_ARRAY_BUFFER- draw call ignored"));
			return MALI_ERR_EARLY_OUT;
		}

		switch (type)
		{
			case GL_UNSIGNED_BYTE:
				element_size = 1;
			break;

			case GL_UNSIGNED_SHORT:
				element_size = 2;
			break;

			default:
				MALI_DEBUG_ASSERT(0, ("unknown type"));
		}

		/* check if the referenced data is aligned to the datatype, if not silently terminate drawcall */
		if (((offset) & (element_size - 1)) != 0)
		{
			MALI_INSTRUMENTED_LOG((_instrumented_get_context(), MALI_LOG_LEVEL_WARNING, "Accessing index buffer at unaligned index"));
			return MALI_ERR_EARLY_OUT;
		}

		/* check if we attempt to read outside the buffer - ignore draw-call if so */
		if (element_size * (unsigned int)count > vbo->size)
		{
			MALI_INSTRUMENTED_LOG((_instrumented_get_context(), MALI_LOG_LEVEL_WARNING,
					"glDrawElements call issued where the VBO bound to GL_ELEMENT_ARRAY_BUFFER "
					"contains too few vertices - draw call ignored "));
			return MALI_ERR_EARLY_OUT;
		}

        if( idx_vs_range != NULL )
        {
			MALI_DEBUG_ASSERT_POINTER( vs_range_count );		
			_gles_gb_buffer_object_data_range_cache_get(gb_vbo, mode, offset, count, type, idx_vs_range, coherence, vs_range_count);
        }

		gb_vbo = NULL;
	}
	else
	{
		if ( NULL==indices )
		{
			MALI_INSTRUMENTED_LOG((_instrumented_get_context(), MALI_LOG_LEVEL_WARNING,
					"glDrawElements called with a NULL indices pointer - draw call ignored"));
			return MALI_ERR_EARLY_OUT;
		}

		if( idx_vs_range != NULL )
		{
            index_range *stack_idx_range = *idx_vs_range;
			MALI_DEBUG_ASSERT_POINTER( stack_idx_range );		

			/* Firstly find out the minimum and maximum indices in indices. */
			/* If the range from minimum to maximum is too large, */
			/* we will re-scan and get the subranges. */
			_gles_scan_indices( stack_idx_range, count, type, NULL, indices);
			
			{

		        GLint tri_count = (mode == GL_TRIANGLES) ? count / 3 : count;

		        if( (u32)((stack_idx_range[0].max - stack_idx_range[0].min + 1)  > tri_count ))
		        {
		            _gles_scan_indices_range( stack_idx_range, count, vs_range_count, type, indices );
		        }
            }
		}
	}

	return MALI_ERR_NO_ERROR;
}

mali_err_code _gles_init_draw_arrays( gles_context *ctx, GLenum mode, GLint count )
{
	/* Early-out if dealing with a zero-sized scissor box */
	if( GL_TRUE == _gles_draw_scissor_viewpoint_check( ctx, _gles_get_current_draw_frame_builder(ctx)) )
	{
		MALI_INSTRUMENTED_LOG((_instrumented_get_context(), MALI_LOG_LEVEL_WARNING,
				"glDrawArrays draw call issued with GL_SCISSOR_TEST enabled and a scissor box "
				"with width or height equal to 0 - draw call ignored"));
		return MALI_ERR_EARLY_OUT;
	}

	MALI_CHECK_NO_ERROR( _gles_init_draw_common( ctx, mode, MALI_FALSE, count ) );

	return MALI_ERR_NO_ERROR;
}

GLsizei _gles_round_down_vertex_count(GLenum mode, GLsizei count)
{
	MALI_DEBUG_ASSERT(count >= 0, ("negative count (%d) not supported", count));
	/* handle non-error cases on a per mode basis */
	switch(mode)
	{
		/* triangles must have a multiple of three, excess is rounded down */
		case GL_TRIANGLES:
			count -= count%0x3;
			break;

			/* strips must be greater than 3, above three the previous 2 vertices are used with each new vertex */
		case GL_TRIANGLE_FAN:
		case GL_TRIANGLE_STRIP:
			if ( count < 3 ) { count = 0; }
			break;

			/* the count must be a multiple of two */
		case GL_LINES:
			count = count&~0x1;
			break;

			/* need more than two, then the previous vertex is used */
		case GL_LINE_STRIP:
		case GL_LINE_LOOP:
			if ( count < 2 ) { count = 0; }
			break;

			/* any value >= 0 is acceptable here */
		case GL_POINTS:
			break;

		default:
			MALI_DEBUG_ASSERT( MALI_FALSE, ("unsupported mode 0x%x", mode));
			break;
	}

	return count;
}


mali_bool _gles_scissor_zero_size_check( struct gles_context * const ctx, struct mali_frame_builder *frame_builder )
{
	struct gles_scissor *scissor;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	scissor = &ctx->state.common.scissor;

	MALI_DEBUG_ASSERT_POINTER( scissor );

	/* Don't waste any time if scissor is disabled
	 */
	if( ! mali_statebit_get( &ctx->state.common, GLES_STATE_SCISSOR_ENABLED ) )
	{
		return MALI_FALSE;
	}

	/* Zero width/height is a trivial case
	 */
	if( ( 0 == scissor->width ) || ( 0 == scissor->height ) )
	{
		return MALI_TRUE;
	}

	/* Clamp scissor dimensions to frame dimensions, if the top/bottom and left/right pairs have the same values
	 * then the scissor box does not intersect the frame, hence it is zero sized.
	 */
	{
		int supersample_scalef = _gles_get_current_draw_supersample_scalefactor(ctx);
		s32 vertical_clamp	= _mali_frame_builder_get_height( frame_builder );
		s32 horizontal_clamp	= _mali_frame_builder_get_width( frame_builder );
		s32 scissor_bottom, scissor_top;
		s32 scissor_left, scissor_right;

		if(supersample_scalef != 1)
		{
 			vertical_clamp /= supersample_scalef;
			horizontal_clamp /= supersample_scalef;
		}

		scissor_top		= scissor->y + scissor->height;
		scissor_bottom	= scissor->y;
		scissor_left	= scissor->x;
		scissor_right	= scissor->x + scissor->width;

		if( scissor_top < 0 )					scissor_top		= 0;
		if( scissor_top > vertical_clamp )		scissor_top		= vertical_clamp;
		if( scissor_bottom < 0 )				scissor_bottom	= 0;
		if( scissor_bottom > vertical_clamp )	scissor_bottom	= vertical_clamp;

		if( scissor_left < 0 )					scissor_left	= 0;
		if( scissor_left > horizontal_clamp )	scissor_left	= horizontal_clamp;
		if( scissor_right < 0 )					scissor_right	= 0;
		if( scissor_right > horizontal_clamp )	scissor_right	= horizontal_clamp;

		return ((scissor_top == scissor_bottom) || (scissor_left == scissor_right));
	}
}
