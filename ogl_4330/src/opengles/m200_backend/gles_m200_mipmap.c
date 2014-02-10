/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2001-2002, 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_context.h"
#include "gles_m200_mipmap.h"
#include "gles_fb_texture_object.h"
#include "gles_m200_mipmap.h"
#include "shared/mali_surface.h"
#include "../gles_mipmap.h"
#include "gles_config_extension.h"
#include "shared/m200_readback_util.h"
#include "shared/m200_gp_frame_builder_inlines.h"

#if MALI_TIMELINE_PROFILING_ENABLED
#include <base/mali_profiling.h>
#endif

GLenum _gles_generate_mipmap_chain_hw(
	struct gles_texture_object* tex_obj,
	struct gles_context *ctx,
	GLenum target,
	unsigned int base_miplvl)
{
	struct mali_frame_builder* frame_builder;
	struct mali_surface *src_surface, *dst_surface;
	unsigned int lvl;
	const int mipchain = _gles_texture_object_get_mipchain_index(target);
	gles_mipmap_level *miplevel_base;
	GLenum format, type;
	mali_err_code error;
	unsigned int start_level = base_miplvl + 1;

	/* input checking */
	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_DEBUG_ASSERT_POINTER(ctx);

	frame_builder = ctx->texture_frame_builder;
	MALI_DEBUG_ASSERT_POINTER(frame_builder);

	src_surface = _gles_fb_texture_object_get_mali_surface(tex_obj->internal, _gles_texture_object_get_mipchain_index(target), base_miplvl);
	MALI_DEBUG_ASSERT_POINTER(src_surface); /* src surface must exist */
	MALI_DEBUG_ASSERT( src_surface->format.pixel_format != MALI_PIXEL_FORMAT_NONE, ("To use HW mipgen, the surface must be renderable") ); 
	
	/* retrieve format and type. These enums must be propagated down to other mipmap levels */
 	 miplevel_base = _gles_texture_object_get_mipmap_level(tex_obj, target, base_miplvl);
	MALI_DEBUG_ASSERT_POINTER(miplevel_base); /* this miplvl must already exist, as we are basing the generation on it */
	format = miplevel_base->format;
	type = miplevel_base->type;



	/* Key idea: 
	 *
	 * Each miplevel over the treshold is automatically generated through a series of HW jobs / gles texture framebuilder, using downscaling.
	 * Each miplevel below that is generated through SW calls. */ 
	for( lvl = start_level ; ;lvl ++)
	{
		unsigned int dst_width = MAX( src_surface->format.width / 2, 1 );
		unsigned int dst_height = MAX( src_surface->format.height / 2, 1 );
			

		/* the downsample scheme depends on combining 2x2 pixels down to
		 * a single pixel through the downscale unit. If there is any odd dimensions, 
		 * or if we hit a dimension that is not possible to downscale this way (say 128x1 -> 64x1),
		 * or if we crossed the treshold where mipgen is advantageous to do in SW,
		 * we will fall back to SW mipgen */
		if( (src_surface->format.width & 1) || 
		    (src_surface->format.height & 1) ||
			((src_surface->format.width * src_surface->format.height) < GLES_HW_MIPGEN_THRESHOLD) || 
			(src_surface->format.width == dst_width) ||
			(src_surface->format.height == dst_height) )
		{
			_mali_frame_builder_wait( frame_builder );
			return _gles_generate_mipmap_chain_sw(tex_obj, ctx, target, lvl - 1);
		}
		
		/* allocate dst surface, this is half the size of the src surface */
		dst_surface = _gles_texture_miplevel_allocate( ctx, tex_obj, _gles_texture_object_get_mipchain_index(target), lvl, dst_width, dst_height, format, type);
		if(dst_surface == NULL) return GL_OUT_OF_MEMORY; 
	
		/* set up framebuiler */
		_mali_frame_builder_wait(frame_builder);
		_mali_frame_builder_set_output(frame_builder, MALI_DEFAULT_COLOR_WBIDX, dst_surface, MALI_OUTPUT_COLOR | MALI_OUTPUT_FSAA_4X);
		error = _mali_frame_builder_use( frame_builder );
		if(error != MALI_ERR_NO_ERROR) 
		{
			_mali_surface_deref(dst_surface);
			return GL_OUT_OF_MEMORY;
		}

		/* Upload the texture */
		error = _mali_frame_builder_readback( frame_builder, src_surface, MALI_OUTPUT_COLOR, 0, 0, _mali_frame_builder_get_width( frame_builder ),_mali_frame_builder_get_height( frame_builder ) );
		if(error != MALI_ERR_NO_ERROR) 
		{
			_mali_surface_deref(dst_surface);
			return GL_OUT_OF_MEMORY;
		}

		/* We want the frame-builder to be reset and wait for it to complete */
		error = _mali_frame_builder_swap( frame_builder );
		if(error != MALI_ERR_NO_ERROR) 
		{
			_mali_surface_deref(dst_surface);
			return GL_OUT_OF_MEMORY;
		}

		/* finish, and cleanup */
		_mali_frame_builder_wait( frame_builder );

		/* upload surface to texobj*/
		error = _gles_texture_miplevel_assign( ctx, tex_obj, mipchain, lvl, format, type, 1, &dst_surface);
		if(error != MALI_ERR_NO_ERROR)
		{
			_mali_surface_deref(dst_surface);
			return GL_OUT_OF_MEMORY;
		}

		
		/* exit if no more miplevels to generate */
		if(dst_width == 1 && dst_height == 1) break;
	
		/* next miplevel preparations */
		src_surface = dst_surface;

	}
	
	_mali_frame_builder_wait( frame_builder );

	return MALI_ERR_NO_ERROR;
}

