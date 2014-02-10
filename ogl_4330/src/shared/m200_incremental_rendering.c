/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <shared/m200_incremental_rendering.h>

#include <regs/MALI200/mali200_core.h>
#include <shared/m200_gp_frame_builder.h>
#include "shared/m200_gp_frame_builder_inlines.h"
#include "m200_readback_util.h"
#include <shared/mali_surface.h>

/**
 * Allocate a new surface with or without space for MRT surfaces 
 * @param format The format of the surface
 * @param base_ctx the base context of the surface
 * @param using_mrt true if we want an MRT capable surface 
 */
MALI_STATIC mali_surface* allocate_surface(mali_surface_specifier* format, mali_base_ctx_handle base_ctx, mali_bool using_mrt)
{
	mali_surface* surf;
	u32 actual_datasize;

	surf = _mali_surface_alloc_empty(MALI_SURFACE_FLAGS_NONE, format, base_ctx);
	if(surf == NULL) return NULL;

	if(using_mrt)
	{
		/* align datasize to MRT and TD requirements (MALI200_SURFACE_ALIGNMENT bytes). */
		surf->datasize = (((surf->datasize +(MALI200_SURFACE_ALIGNMENT-1)) / MALI200_SURFACE_ALIGNMENT) * MALI200_SURFACE_ALIGNMENT);

		/* then calculate the actual surface datasize based on the number of planes */
		actual_datasize = surf->datasize * 4;
	} else {
		actual_datasize = surf->datasize;
	}

	MALI_DEBUG_ASSERT(format->pixel_format != MALI_PIXEL_FORMAT_NONE, ("Surface must be writable"));
	MALI_DEBUG_ASSERT(format->pixel_layout != MALI_PIXEL_LAYOUT_LINEAR, ("Surface must not be linear"));

#ifdef HARDWARE_ISSUE_9427
	#if 0
		/* this cannot happen, so just asserting on it instead */
		/* Hardware writes outside the given memory area; increase size of memory area in cases where this can happen */
		if( offscreen_surface->format.pixel_format != MALI_PIXEL_FORMAT_NONE && /* writeable */
		    offscreen_surface->format.pixel_layout == MALI_PIXEL_LAYOUT_LINEAR &&  /* linear */
		    ((dst_width % 16) != 0) ) /* width not divisible by 16 */
		{
			actual_datasize += HARDWARE_ISSUE_9427_EXTRA_BYTES;
		}
	#endif
#endif
	
	/* and finally allocate it to the surface */
	surf->mem_ref = _mali_shared_mem_ref_alloc_mem( base_ctx, actual_datasize,
	                                MALI200_SURFACE_ALIGNMENT,  MALI_PP_WRITE | MALI_PP_READ | MALI_CPU_READ | MALI_CPU_WRITE );
	if( surf->mem_ref == NULL ) 
	{
		_mali_surface_deref(surf);
		return NULL;
	}

	return surf;

}

/**
 * Allocates a new set of attachments based on the current attachments and the capabilities
 * @param frame_builder [in] The frame builder we are doing allocating surfaces based on
 * @param offscreen_attachments [out] A pointer to where to store the offscreen attachments. Assumed to be an array of MALI200_WRITEBACK_UNIT_COUNT elements.
 * @param capabilities [in] A bitvector holding which informatino to preserve in the incremental render operation. 
 */
MALI_STATIC mali_err_code allocate_offscreen_attachments( struct mali_frame_builder* fbuilder, 
                                                          struct mali_frame_builder_output_chain* offscreen_attachments,
                                                          mali_incremental_render_capabilities capabilities )
{
	int wbx_id;
	mali_surface_specifier rgbaformat, zsformat;
	mali_err_code err = MALI_ERR_NO_ERROR;

	int w = _mali_frame_builder_get_width(fbuilder);
	int h = _mali_frame_builder_get_height(fbuilder);
	
	_mali_surface_specifier_ex( &rgbaformat,
			w,
			h,
			0,
			MALI_PIXEL_FORMAT_A8R8G8B8,
			M200_TEXEL_FORMAT_ARGB_8888,
			MALI_PIXEL_LAYOUT_INTERLEAVED_BLOCKS,
			M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED,
			MALI_FALSE,
			MALI_FALSE,
			MALI_SURFACE_COLORSPACE_sRGB,
			MALI_SURFACE_ALPHAFORMAT_NONPRE,
			MALI_FALSE
		);	

	_mali_surface_specifier_ex( &zsformat,
			w,
			h,
			0,
			MALI_PIXEL_FORMAT_S8Z24,
			M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8,
			MALI_PIXEL_LAYOUT_INTERLEAVED_BLOCKS,
			M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED,
			MALI_FALSE,
			MALI_FALSE,
			MALI_SURFACE_COLORSPACE_sRGB,
			MALI_SURFACE_ALPHAFORMAT_NONPRE,
			MALI_FALSE
		);	

	MALI_DEBUG_ASSERT_POINTER(fbuilder);
	MALI_DEBUG_ASSERT_POINTER(offscreen_attachments);


	/* clear outputs */
	for(wbx_id = 0; wbx_id < MALI200_WRITEBACK_UNIT_COUNT; wbx_id++)
	{
		offscreen_attachments[wbx_id].usage = 0;
		offscreen_attachments[wbx_id].buffer = NULL;
	}

	if(capabilities & MALI_INCREMENTAL_RENDER_PRESERVE_COLOR_BUFFER)
	{
		/* TODO: moderate these requirements to fit the actual output surfaces. 
		 * No point in doing a full 8888 output if we only use 5650. 
		 * */

		offscreen_attachments[MALI_DEFAULT_COLOR_WBIDX].usage = MALI_OUTPUT_COLOR;
		offscreen_attachments[MALI_DEFAULT_COLOR_WBIDX].buffer = allocate_surface(&rgbaformat, fbuilder->base_ctx, capabilities & MALI_INCREMENTAL_RENDER_PRESERVE_MULTISAMPLING );
		if(offscreen_attachments[MALI_DEFAULT_COLOR_WBIDX].buffer == NULL) err = MALI_ERR_OUT_OF_MEMORY;
		wbx_id++;

	}

	if((capabilities & MALI_INCREMENTAL_RENDER_PRESERVE_DEPTH_BUFFER) 
	    || 
	   (capabilities & MALI_INCREMENTAL_RENDER_PRESERVE_STENCIL_BUFFER))
	{
		/* create a depth / stencil output buffer */
	
		/* setup whether to use it (and thus read it back) as depth and/or stencil */
		if(capabilities & MALI_INCREMENTAL_RENDER_PRESERVE_DEPTH_BUFFER) offscreen_attachments[MALI_DEFAULT_DEPTH_WBIDX].usage |= MALI_OUTPUT_DEPTH;
		if(capabilities & MALI_INCREMENTAL_RENDER_PRESERVE_STENCIL_BUFFER) offscreen_attachments[MALI_DEFAULT_DEPTH_WBIDX].usage |= MALI_OUTPUT_STENCIL;

		offscreen_attachments[MALI_DEFAULT_DEPTH_WBIDX].buffer = allocate_surface(&zsformat, fbuilder->base_ctx, capabilities & MALI_INCREMENTAL_RENDER_PRESERVE_MULTISAMPLING );
		if(offscreen_attachments[MALI_DEFAULT_DEPTH_WBIDX].buffer == NULL) err = MALI_ERR_OUT_OF_MEMORY;
		wbx_id++;

	} /* endif depth / stencil */

	/* deal with error handling, free up allocated surfaces */
	if(err != MALI_ERR_NO_ERROR )
	{
		for(wbx_id = 0; wbx_id < MALI200_WRITEBACK_UNIT_COUNT; wbx_id++)
		{
			if(offscreen_attachments[wbx_id].buffer) _mali_surface_deref(offscreen_attachments[wbx_id].buffer);
			offscreen_attachments[wbx_id].usage = 0;
			offscreen_attachments[wbx_id].buffer = NULL;
		}
	}

	return err;
}


/***********************************
 * External functions (non-static) *
 ***********************************/

MALI_EXPORT mali_err_code _mali_incremental_render( mali_frame_builder *frame_builder )
{
	mali_err_code            error;
	int i;
	
	struct mali_frame_builder_output_chain original_attachments[MALI200_WRITEBACK_UNIT_COUNT]; /* attachments that came with the framebuilder */
	struct mali_frame_builder_output_chain offscreen_attachments[MALI200_WRITEBACK_UNIT_COUNT]; /* attachments that this function allocates */

	u32 buffers_unchanged_mask = (MALI_COLOR_PLANE_UNCHANGED | MALI_DEPTH_PLANE_UNCHANGED | MALI_STENCIL_PLANE_UNCHANGED );
	mali_bool all_buffers_unchanged = (frame_builder->buffer_state_per_plane & buffers_unchanged_mask) == buffers_unchanged_mask;
	u32 buffers_cleared_mask = (MALI_COLOR_PLANE_CLEAR_TO_COLOR | MALI_DEPTH_PLANE_CLEAR_TO_COLOR | MALI_STENCIL_PLANE_CLEAR_TO_COLOR );
	mali_bool all_buffers_cleared = (frame_builder->buffer_state_per_plane & buffers_cleared_mask) == buffers_cleared_mask;

	mali_incremental_render_capabilities capabilities;

	MALI_DEBUG_ASSERT_POINTER(frame_builder);

	capabilities = _mali_frame_builder_get_targets_to_preserve(frame_builder);

	/* If all the buffers are set to "defined but unchanged", or "cleared to color" then we don't even need to flush on first place. Reset and early out */
	if ( all_buffers_unchanged )	
	{
		_mali_frame_builder_reset( frame_builder );

		frame_builder->buffer_state_per_plane |= MALI_COLOR_PLANE_UNCHANGED | MALI_DEPTH_PLANE_UNCHANGED | MALI_STENCIL_PLANE_UNCHANGED;	

		return MALI_ERR_NO_ERROR;
	}
	else if ( all_buffers_cleared )
	{
		_mali_frame_builder_reset( frame_builder );

		frame_builder->buffer_state_per_plane |= MALI_COLOR_PLANE_CLEAR_TO_COLOR | MALI_DEPTH_PLANE_CLEAR_TO_COLOR | MALI_STENCIL_PLANE_CLEAR_TO_COLOR;

		return MALI_ERR_NO_ERROR;
	}

	_mali_frame_set_inc_render_on_flush( frame_builder, MALI_FALSE );
	MALI_DEBUG_ASSERT(frame_builder->output_valid, ("The framebuilder outputs are not valid. No drawing should occur!"));

	/* allocate new attachments based on the old attachments and the capabilities */
	error = allocate_offscreen_attachments( frame_builder, &offscreen_attachments[0], capabilities );
	if( MALI_ERR_NO_ERROR != error) return error;
	
	_mali_frame_builder_acquire_output(frame_builder);
	/* store off original attachments, replace with offscreen attachments */
	for(i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; i++)
	{
		original_attachments[i] = frame_builder->output_buffers[i];
		
		frame_builder->output_buffers[i] = offscreen_attachments[i]; 		
	}

	/* Do the framebuilder incremental render operation. 
	 * This will trigger a flush (to the offscreen buffers). To ensure the flush triggers, we do a writelock. 
	 * Then a reset (clearing the PLBU command lists) 
	 * and then setting up a set of readbacks based on the offscreen buffers */
	error = _mali_frame_builder_write_lock( frame_builder, MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_ALL_CHANNELS | MALI_FRAME_BUILDER_BUFFER_BIT_DEPTH | MALI_FRAME_BUILDER_BUFFER_BIT_STENCIL );
	_mali_frame_builder_write_unlock( frame_builder );
	if(error == MALI_ERR_NO_ERROR) error = _mali_frame_builder_swap( frame_builder );
	_mali_frame_builder_reset( frame_builder ); /* set frame state to CLEAN, making the next drawcall flush */
	
	/* ... and set the original attachments back */
	for(i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; i++)
	{
		frame_builder->output_buffers[i] = original_attachments[i]; 		
	}

	if(error == MALI_ERR_NO_ERROR)
	{

		error = _mali_frame_builder_write_lock( frame_builder, MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_ALL_CHANNELS | MALI_FRAME_BUILDER_BUFFER_BIT_DEPTH | MALI_FRAME_BUILDER_BUFFER_BIT_STENCIL );
		_mali_frame_builder_write_unlock( frame_builder );
	}

	/* if nothing went wrong so far, set up the readbacks */
	if( error == MALI_ERR_NO_ERROR) 
	{
		for(i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; i++)
		{
			struct mali_surface* surf = offscreen_attachments[i].buffer;
			if(surf == NULL) continue;

			error = _mali_frame_builder_add_callback( frame_builder,
				(mali_frame_cb_func)_mali_shared_mem_ref_owner_deref,
				(mali_frame_cb_param) surf->mem_ref) ;
			if(MALI_ERR_NO_ERROR != error) break;
		
			_mali_shared_mem_ref_owner_addref( surf->mem_ref );

			if( (MALI_DEFAULT_COLOR_WBIDX == i) && (frame_builder->buffer_state_per_plane & MALI_COLOR_PLANE_DIRTY))
			{
				error = _mali_frame_builder_readback( frame_builder, surf, offscreen_attachments[i].usage, 0, 0, _mali_frame_builder_get_width( frame_builder ), _mali_frame_builder_get_height( frame_builder ) );
				if(error != MALI_ERR_NO_ERROR) break;
			}
			else if( (MALI_DEFAULT_DEPTH_WBIDX == i) && (frame_builder->buffer_state_per_plane & MALI_DEPTH_PLANE_DIRTY))
			{
				error = _mali_frame_builder_readback( frame_builder, surf, offscreen_attachments[i].usage, 0, 0, _mali_frame_builder_get_width( frame_builder ), _mali_frame_builder_get_height( frame_builder ) );
				if(error != MALI_ERR_NO_ERROR) break;
			}
			else if( (MALI_DEFAULT_STENCIL_WBIDX == i) && (frame_builder->buffer_state_per_plane & MALI_STENCIL_PLANE_DIRTY))
			{
				error = _mali_frame_builder_readback( frame_builder, surf, offscreen_attachments[i].usage, 0, 0, _mali_frame_builder_get_width( frame_builder ), _mali_frame_builder_get_height( frame_builder ) );
				if(error != MALI_ERR_NO_ERROR) break;
			}

			error = _mali_frame_builder_add_surface_read_dependency(frame_builder, offscreen_attachments[i].buffer);
			if(error != MALI_ERR_NO_ERROR) break;
		}
	}

	/* function cleanup starts here. Whether things failed or not, this is where everything is cleaned up */
	for(i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; i++)
	{
		if(offscreen_attachments[i].buffer == NULL) continue;
		/* we deref the surface, but the memory in the surface has been addref'ed to the frame */
		_mali_surface_deref(offscreen_attachments[i].buffer);
	}

	return error; 
}

