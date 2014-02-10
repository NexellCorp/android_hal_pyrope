/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_incremental_rendering.h"

#include <shared/mali_mem_pool.h>
#include <shared/m200_gp_frame_builder_inlines.h>

mali_err_code _gles_incremental_render( gles_context       *ctx,
                                        gles_framebuffer_object *fbo )
{
	mali_err_code retval, begin_err;

	_mali_frame_builder_acquire_output( ctx->state.common.framebuffer.current_object->draw_frame_builder );

	retval = _mali_incremental_render(fbo->draw_frame_builder);

	/* incremental rendering implies a _mali_frame_builder_reset - whether it succeeds or not */
	begin_err = _gles_begin_frame(ctx);
	if( MALI_ERR_NO_ERROR == retval )
	{
		/* return the first error code if incr.rendering was unsuccessful */
		retval = begin_err;
	}

	/* make sure the frame_builder is in the state it should be as in a drawcall.
	 * we still hold the write lock on the frame_builder so this is the minimum we need to do.
	 */ 
	if ( MALI_ERR_NO_ERROR == begin_err )
	{
		mali_err_code err;

		/* make sure the frame builder is ready for use */
		err = _mali_frame_builder_use( _gles_get_current_draw_frame_builder( ctx ) ); 
		if ( MALI_ERR_NO_ERROR == retval )
		{
			retval = err;
		}
		
		/* connect the pool */
		ctx->frame_pool = _mali_frame_builder_frame_pool_get( _gles_get_current_draw_frame_builder( ctx ) );
	}
	
	return retval;
}
