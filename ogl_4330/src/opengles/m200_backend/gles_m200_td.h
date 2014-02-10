/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_M200_TD_H
#define GLES_M200_TD_H

#include "../gles_base.h"
#include "shared/binary_shader/bs_object.h"
#include "shared/m200_td.h"
#include "../gles_state.h"
#include "gles_context.h"
#include "gles_fb_texture_object.h"
#include "gles_fb_texture_memory.h"

/**
 * Updates the texture descriptor.
 * 	- Allocates mali memory for the texture descriptor(s) and remap tabel.
 *  - Sets the remap table.
 * 	- Generates new TD's if the current texture object's dirty flag is set and resets the flag.
 * 	- Copies the TD to mali memory.
 * 	- Allocated mali memory is added to the frames misc list, to be freed later by the frame.
 * @param the current base context handle
 * @param fb_ctx fragment backend context
 * @param frame_builder the current frame_builder
 * @param state the gl state
 * @param bs_program program binary state
 * @return MALI_ERR_NO_ERROR if no sampler are used or MALI_ERR_OUT_OF_MEMORY if out of memory.
 *         Otherwise MALI_ERR_NO_ERROR
 */
MALI_CHECK_RESULT mali_err_code _gles_m200_update_texture_descriptors( struct gles_context *ctx, struct gles_fb_context *fb_ctx, mali_frame_builder *frame_builder, gles_state *state, bs_program *prog_binary_state );

/**
 * Assigns a set of TDs and assign them to the given TD remap table at slot 0-num_descriptors. 
 * The TDs are taken from the input texture object. 
 * @param ctx The GLES context
 * @param frame_builder The framebuilder to add read dependencies on (used by renderable textures)
 * @param td_remap The TD remap table to add TDs to.
 * @param texture_object The texture object to get TDs from. 
 *        This function may update the mali_td field of the internal texture object as needed. 
 * @param num_descriptors The number of descriptors to add 
 **/
MALI_CHECK_RESULT mali_err_code _gles_add_texobj_to_remap_table(struct gles_context *ctx, mali_frame_builder* frame_builder, u32* td_remap, gles_texture_object *texture_object, int num_descriptors);

/**
 * Creates a texture descriptor meeting GLES spec defaults, but with NO_TEXTURE
 * format, so no texel data read attempt will be made.
 */
MALI_STATIC_INLINE void _gles_m200_td_set_defaults( m200_td dest )
{
	_mali_sys_memset( dest, 0x0, sizeof( m200_td ) );

	/* Default no-texture, in this form no texture will be read
	 */
	MALI_TD_SET_TEXEL_FORMAT( dest, M200_TEXEL_FORMAT_NO_TEXTURE );
	MALI_TD_SET_TEXTURE_DIMENSIONALITY( dest, M200_TEXTURE_DIMENSIONALITY_2D );
}

MALI_STATIC_INLINE void _gles_m200_td_minmag_or_mip0_change( gles_texture_object *tex_obj, u32 plane )
{
	MALI_DEBUG_ASSERT_POINTER( tex_obj );

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	MALI_DEBUG_ASSERT_LEQ(plane, 2);
#else
	MALI_DEBUG_ASSERT_LEQ(plane, 0);
#endif

	/*
	 *
	 * The Mali200 hardware does not have any hardware-level support for
	 * non-mipmapped minfilters (GL_LINEAR, GL_NEAREST);
	 * these are emulated by doing the equivalent of
	 * (minfilter=GL_*_MIPMAP_NEAREST) combined with lambda-clamps.
	 *
	 * In the case of (minfilter=GL_NEAREST_MIPMAP_*, magfilter=GL_LINEAR),
	 * OpenGL/ES requires a min/mag lod threshold of 0.5;
	 * the hardware handles this correctly.
	 * Now, if you instead use (minfilter=GL_NEAREST, magfilter=GL_LINEAR),
	 * the correct min/mag-threshold lod is, per the GL spec, 0;
	 * in this case, since we don't really support minfilter=GL_NEAREST
	 * directly in hardware, the driver must effectively encode
	 * minfilter=GL_NEAREST_MIPMAP_NEAREST with lambda-clamps.
	 *
	 * This produces a situation where the hardware will apply a threshold
	 * of 0.5 when it actually should have applied a threshold of 0;
	 * the following driver code applies a lod-bias of 0.5 in order to
	 * cancel out this effect.
	 *
	 */

	if( ( tex_obj->min_filter == GL_NEAREST ) && ( tex_obj->mag_filter == GL_LINEAR ) )
	{
		MALI_TD_SET_LAMBDA_BIAS( tex_obj->internal->tds[plane], MALI_STATIC_CAST(u32)(1 << 3) );
	}
	/*
	Enabling this will break glTexParameteri API tests, maybe also conformance. This is, mandatory for
	EXT_shader_texture_lod to function correctly. Needs more investigation.


	Having this disabled well make texture2DLodEXT and and texture2DProjLodEXT functions
	to sample incorrect mip-level at n + 0.5, where n >= 0 if nearest mip-level selection
	is used. Enabling this will break glTexParameter tests and for the time being, maybe
	conformance as well -- something that hasn''t yet been investigated. Thus wee keep
	this disabled and let the LOD tests fail for now.
	*/
	
	else if (tex_obj->mag_filter == GL_NEAREST &&
            (tex_obj->min_filter == GL_LINEAR_MIPMAP_NEAREST || tex_obj->min_filter == GL_NEAREST_MIPMAP_NEAREST))
	{
		/* The Mali texturing unit uses rounding scheme in which 0.5 is rounded up to next integer,
		 * meaning that passing in LOD value of x+0.5 will result in miplevel(x+1) being selected. The GLES
		 * spec, however, says x+0.5 should select miplevel(x).
		 *
		 * Therefore we need to add a bias of -1/16 to comply with the spec. */
		MALI_TD_SET_LAMBDA_BIAS( tex_obj->internal->tds[plane], MALI_STATIC_CAST(u32)(0x1FF));
	}
	else
	{
		MALI_TD_SET_LAMBDA_BIAS( tex_obj->internal->tds[plane], 0 );
	}

	if( GL_NEAREST == tex_obj->min_filter || GL_LINEAR == tex_obj->min_filter )
	{
		MALI_TD_SET_LAMBDA_HIGH_CLAMP( tex_obj->internal->tds[plane], 0 );
	}
	else
	{
		/* If lvl0 is null, this function will be called again before
		 * the texture can be used (when level 0 is uploaded) */

		if( NULL != tex_obj->mipchains[0] && NULL != tex_obj->mipchains[0]->levels[0] )
		{
			u32 largest_dimension = MAX( tex_obj->mipchains[0]->levels[0]->width, tex_obj->mipchains[0]->levels[0]->height );
			u32 levels = _mali_log_base2( _mali_floor_pow2( largest_dimension ) );
			MALI_TD_SET_LAMBDA_HIGH_CLAMP( tex_obj->internal->tds[plane], ( levels << 4 ) );
		}
	}
}

MALI_STATIC_INLINE void _gles_m200_td_min_filter( gles_texture_object *tex_obj )
{
	u32 minify  = 0;
	u32 mipmode = M200_MIPMAP_MODE_NEAREST;
	u32 filter  = tex_obj->min_filter;

	switch( filter )
	{
	case GL_NEAREST:
	case GL_NEAREST_MIPMAP_LINEAR:
	case GL_NEAREST_MIPMAP_NEAREST:
		minify = 1;
		break;
	case GL_LINEAR:
	case GL_LINEAR_MIPMAP_NEAREST:
	case GL_LINEAR_MIPMAP_LINEAR:
		break;
	default:
		MALI_DEBUG_ERROR(("invalid enum"));
	}

	MALI_TD_SET_POINT_SAMPLE_MINIFY( tex_obj->internal->tds[0], minify );
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	MALI_TD_SET_POINT_SAMPLE_MINIFY( tex_obj->internal->tds[1], minify );
	MALI_TD_SET_POINT_SAMPLE_MINIFY( tex_obj->internal->tds[2], minify );
#endif

	switch( filter )
	{
	case GL_NEAREST_MIPMAP_LINEAR:
	case GL_LINEAR_MIPMAP_LINEAR:
		mipmode =  M200_MIPMAP_MODE_QUALITY_TRILINEAR;
		break;
	case GL_NEAREST:
	case GL_LINEAR:
	case GL_LINEAR_MIPMAP_NEAREST:
	case GL_NEAREST_MIPMAP_NEAREST:
		break;
	default:
		MALI_DEBUG_ERROR(("invalid enum"));
	}
	
	MALI_TD_SET_MIPMAPPING_MODE( tex_obj->internal->tds[0], mipmode );
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	MALI_TD_SET_MIPMAPPING_MODE( tex_obj->internal->tds[1], mipmode );
	MALI_TD_SET_MIPMAPPING_MODE( tex_obj->internal->tds[2], mipmode );
#endif

	_gles_m200_td_minmag_or_mip0_change( tex_obj, 0 );
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	_gles_m200_td_minmag_or_mip0_change( tex_obj, 1 );
	_gles_m200_td_minmag_or_mip0_change( tex_obj, 2 );
#endif
}

MALI_STATIC_INLINE void _gles_m200_td_mag_filter( gles_texture_object *tex_obj )
{
	u32 filter  = tex_obj->mag_filter;
	u32 magnify = filter == GL_NEAREST ? 1 : 0;

	MALI_TD_SET_POINT_SAMPLE_MAGNIFY( tex_obj->internal->tds[0], magnify );
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	MALI_TD_SET_POINT_SAMPLE_MAGNIFY( tex_obj->internal->tds[1], magnify );
	MALI_TD_SET_POINT_SAMPLE_MAGNIFY( tex_obj->internal->tds[2], magnify );
#endif

	_gles_m200_td_minmag_or_mip0_change( tex_obj, 0 );
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	_gles_m200_td_minmag_or_mip0_change( tex_obj, 1 );
	_gles_m200_td_minmag_or_mip0_change( tex_obj, 2 );
#endif
}

MALI_STATIC_INLINE void _gles_m200_td_dimensionality( gles_texture_object *tex_obj )
{
	switch( tex_obj->dimensionality )
	{
	case GLES_TEXTURE_TARGET_2D:
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	case GLES_TEXTURE_TARGET_EXTERNAL:
#endif
		MALI_TD_SET_TEXTURE_FORMAT( tex_obj->internal->tds[0], M200_TEXTURE_FORMAT_NORMALIZED );
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
		MALI_TD_SET_TEXTURE_FORMAT( tex_obj->internal->tds[1], M200_TEXTURE_FORMAT_NORMALIZED );
		MALI_TD_SET_TEXTURE_FORMAT( tex_obj->internal->tds[2], M200_TEXTURE_FORMAT_NORMALIZED );
#endif
		MALI_TD_SET_TEXTURE_DIMENSIONALITY( tex_obj->internal->tds[0], M200_TEXTURE_DIMENSIONALITY_2D );
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
		MALI_TD_SET_TEXTURE_DIMENSIONALITY( tex_obj->internal->tds[1], M200_TEXTURE_DIMENSIONALITY_2D );
		MALI_TD_SET_TEXTURE_DIMENSIONALITY( tex_obj->internal->tds[2], M200_TEXTURE_DIMENSIONALITY_2D );
#endif
		break;

	case GLES_TEXTURE_TARGET_CUBE:
		MALI_TD_SET_TEXTURE_FORMAT( tex_obj->internal->tds[0], M200_TEXTURE_FORMAT_CUBE_MAP );
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
		MALI_TD_SET_TEXTURE_FORMAT( tex_obj->internal->tds[1], M200_TEXTURE_FORMAT_CUBE_MAP );
		MALI_TD_SET_TEXTURE_FORMAT( tex_obj->internal->tds[2], M200_TEXTURE_FORMAT_CUBE_MAP );
#endif
		MALI_TD_SET_TEXTURE_DIMENSIONALITY( tex_obj->internal->tds[0], M200_TEXTURE_DIMENSIONALITY_3D );
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
		MALI_TD_SET_TEXTURE_DIMENSIONALITY( tex_obj->internal->tds[1], M200_TEXTURE_DIMENSIONALITY_3D );
		MALI_TD_SET_TEXTURE_DIMENSIONALITY( tex_obj->internal->tds[2], M200_TEXTURE_DIMENSIONALITY_3D );
#endif
		break;

	default:
		MALI_DEBUG_ASSERT( MALI_FALSE, ("invalid texture target"));
	}
}

void _gles_m200_td_dimensions( struct gles_texture_object *tex_obj, u32 width, u32 height, u32 stride, u32 bpp, mali_bool is_linear, u32 plane);

#define FROM_ADDR( x ) ( x >> 6 )

#ifndef NDEBUG
#define ASSERT_FROM_ADDR_COMPATIBLE( x ) MALI_DEBUG_ASSERT( ((x) & ((1<<6)-1)) == 0, ("Alignment requirement: %x must be aligned to 6 bits", x))
#else 
#define ASSERT_FROM_ADDR_COMPATIBLE( x )
#endif

MALI_STATIC_INLINE void _gles_m200_td_deep_copy( struct gles_fb_texture_object *dst, struct gles_fb_texture_object *src )
{
	u32 mipaddress;
	int i;
	int p;

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	_mali_sys_memcpy_32( dst->tds, src->tds, 3 * sizeof( m200_td ) );
#else
	_mali_sys_memcpy_32( dst->tds, src->tds, sizeof( m200_td ) );
#endif

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	for(p=0; p<3; p++)
#else
	for(p=0; p<1; p++)
#endif
	{
	
		/* The addresses for each mipmap will be changed in the copy */
		for( i=0; i < MALI_TD_MIPLEVEL_POINTERS; i++ )
		{
			struct gles_fb_texture_memory* memblock = _gles_fb_texture_object_get_memblock(dst, i, p, NULL);
			MALI_DEBUG_ASSERT_POINTER(memblock);

			if( _gles_fb_texture_memory_valid(memblock) )
			{
				mali_addr addr = _gles_fb_texture_memory_get_addr(memblock);
				ASSERT_FROM_ADDR_COMPATIBLE(addr);
				mipaddress = FROM_ADDR( addr );

				switch( i )
				{
					case 0: MALI_TD_SET_MIPMAP_ADDRESS_0( dst->tds[p], mipaddress ); break;
					case 1: MALI_TD_SET_MIPMAP_ADDRESS_1( dst->tds[p], mipaddress ); break;
					case 2: MALI_TD_SET_MIPMAP_ADDRESS_2( dst->tds[p], mipaddress ); break;
					case 3: MALI_TD_SET_MIPMAP_ADDRESS_3( dst->tds[p], mipaddress ); break;
					case 4: MALI_TD_SET_MIPMAP_ADDRESS_4( dst->tds[p], mipaddress ); break;
					case 5: MALI_TD_SET_MIPMAP_ADDRESS_5( dst->tds[p], mipaddress ); break;
					case 6: MALI_TD_SET_MIPMAP_ADDRESS_6( dst->tds[p], mipaddress ); break;
					case 7: MALI_TD_SET_MIPMAP_ADDRESS_7( dst->tds[p], mipaddress ); break;
					case 8: MALI_TD_SET_MIPMAP_ADDRESS_8( dst->tds[p], mipaddress ); break;
					case 9: MALI_TD_SET_MIPMAP_ADDRESS_9( dst->tds[p], mipaddress ); break;
					case 10: MALI_TD_SET_MIPMAP_ADDRESS_10( dst->tds[p], mipaddress ); break;
				}
			} /* if valid */
		} /* for level */
	} /* for plane */
}

MALI_STATIC_INLINE void _gles_m200_td_level_change( struct gles_texture_object *tex_obj, u32 level )
{
	int p = 0;
	int planes_to_change = 1;

	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	MALI_DEBUG_ASSERT( level < GLES_MAX_MIPMAP_LEVELS, ("Invalid level passed to _gles_m200_td_level_change\n") );

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	MALI_DEBUG_ASSERT_RANGE(tex_obj->internal->used_planes, 1, 3);
	if(0==level) planes_to_change = tex_obj->internal->used_planes; /* External surfaces cannot have mipmap levels */
#endif

	for(p=0; p<planes_to_change; p++)
	{
		struct gles_fb_texture_memory* memblock = _gles_fb_texture_object_get_memblock(tex_obj->internal, level, p, NULL);
		MALI_DEBUG_ASSERT_POINTER(memblock); /* cannot fail, but the buffer in it may not be allocated */

		if( _gles_fb_texture_memory_valid(memblock) )
		{
			u32 mipaddress;
			mali_addr addr = _gles_fb_texture_memory_get_addr(memblock);
			const mali_surface_specifier* surfspec = _gles_fb_texture_memory_get_specifier(memblock);
			ASSERT_FROM_ADDR_COMPATIBLE(addr);
			mipaddress = FROM_ADDR( addr );

			switch( level )
			{
			case 0:
				
				/* fields that only need to be set by level 0 */
				if ( M200_TEXTURE_ADDRESSING_MODE_LINEAR == surfspec->texel_layout )
				{
					_gles_m200_td_dimensions( tex_obj,
											surfspec->width,
											surfspec->height,
											surfspec->pitch,
											__m200_texel_format_get_size( surfspec->texel_format ),
											MALI_TRUE, p );
				}
				else
				{
					/* on the next function we didn't pass the stride and bpp since they are not used for non-linear textures  */
					_gles_m200_td_dimensions( tex_obj,
												surfspec->width,
												surfspec->height,
												0, 0, MALI_FALSE, p );
				}
				_gles_m200_td_minmag_or_mip0_change( tex_obj, p );
				MALI_TD_SET_TEXEL_FORMAT( tex_obj->internal->tds[p], surfspec->texel_format );
				MALI_TD_SET_TEXTURE_ADDRESSING_MODE( tex_obj->internal->tds[p], surfspec->texel_layout );

				/* Assert that the next two setters are legal for the given format.
				 * To be complete, every surface must have the same format/flags, 
				 * and a surface cannot have illegal flags set, so we just pick whatever is set on lvl 0. */
				#ifndef NDEBUG		
				{
					mali_bool support_redblue, support_revorder;
					__m200_texel_format_flag_support( surfspec->texel_format, &support_redblue, &support_revorder);
					if(surfspec->red_blue_swap)
					{
						MALI_DEBUG_ASSERT(support_redblue, ("Illegal flag state was set on surface. Should not be possible."));
					}
					if(surfspec->reverse_order)
					{
						MALI_DEBUG_ASSERT(support_revorder, ("Illegal flag state was set on surface. Should not be possible."));
					}
				}
				#endif
				MALI_TD_SET_TEXEL_RED_BLUE_SWAP( tex_obj->internal->tds[p], surfspec->red_blue_swap);
				MALI_TD_SET_TEXEL_ORDER_INVERT( tex_obj->internal->tds[p], surfspec->reverse_order);
				MALI_TD_SET_MIPMAP_ADDRESS_0( tex_obj->internal->tds[p], mipaddress );
				break;
			case 1:
				MALI_TD_SET_MIPMAP_ADDRESS_1( tex_obj->internal->tds[p], mipaddress );
				break;
			case 2:
				MALI_TD_SET_MIPMAP_ADDRESS_2( tex_obj->internal->tds[p], mipaddress );
				break;
			case 3:
				MALI_TD_SET_MIPMAP_ADDRESS_3( tex_obj->internal->tds[p], mipaddress );
				break;
			case 4:
				MALI_TD_SET_MIPMAP_ADDRESS_4( tex_obj->internal->tds[p], mipaddress );
				break;
			case 5:
				MALI_TD_SET_MIPMAP_ADDRESS_5( tex_obj->internal->tds[p], mipaddress );
				break;
			case 6:
				MALI_TD_SET_MIPMAP_ADDRESS_6( tex_obj->internal->tds[p], mipaddress );
				break;
			case 7:
				MALI_TD_SET_MIPMAP_ADDRESS_7( tex_obj->internal->tds[p], mipaddress );
				break;
			case 8:
				MALI_TD_SET_MIPMAP_ADDRESS_8( tex_obj->internal->tds[p], mipaddress );
				break;
			case 9:
				MALI_TD_SET_MIPMAP_ADDRESS_9( tex_obj->internal->tds[p], mipaddress );
				break;
			case 10:
				MALI_TD_SET_MIPMAP_ADDRESS_10( tex_obj->internal->tds[p], mipaddress );
				break;
			default:
				/* mipmaps > 10 are in a contiguous block*/
				break;
			}
		}
	}
}

#undef FROM_ADDR
#undef ASSERT_FROM_ADDR_COMPATIBLE 

/**
 * @brief Converts a GL wrap mode to a M200 wrap mode
 * @param mode the GL wrap mode to convert
 * @returns a M200 wrap mode.
 * @note The function assertcrashes on unknown input. This is because all inputs should already be tested as legal
 */
MALI_STATIC_INLINE m200_texture_wrap_mode _gles_m200_get_wrap_mode(GLenum mode)
{
	switch (mode)
	{
	case GL_REPEAT:		 return M200_TEXTURE_WRAP_MODE_REPEAT;
	case GL_CLAMP_TO_EDGE:	 return M200_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE;
	case GL_MIRRORED_REPEAT: return M200_TEXTURE_WRAP_MODE_MIRRORED_REPEAT;
	default:
		MALI_DEBUG_ASSERT( MALI_FALSE, ("unsupported clamp mode"));
		return M200_TEXTURE_WRAP_MODE_REPEAT;
	}
}

MALI_STATIC_INLINE void _gles_m200_td_set_wrap_mode_s( gles_texture_object *tex_obj )
{
	m200_texture_wrap_mode wrap_mode = _gles_m200_get_wrap_mode( tex_obj->wrap_s );
	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_TD_SET_WRAP_MODE_S( tex_obj->internal->tds[0], wrap_mode );
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	MALI_TD_SET_WRAP_MODE_S( tex_obj->internal->tds[1], wrap_mode );
	MALI_TD_SET_WRAP_MODE_S( tex_obj->internal->tds[2], wrap_mode );
#endif
}

MALI_STATIC_INLINE void _gles_m200_td_set_wrap_mode_t( gles_texture_object *tex_obj )
{
	m200_texture_wrap_mode wrap_mode = _gles_m200_get_wrap_mode( tex_obj->wrap_t );
	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_TD_SET_WRAP_MODE_T( tex_obj->internal->tds[0], wrap_mode );
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	MALI_TD_SET_WRAP_MODE_T( tex_obj->internal->tds[1], wrap_mode );
	MALI_TD_SET_WRAP_MODE_T( tex_obj->internal->tds[2], wrap_mode );
#endif
}

#if EXTENSION_TEXTURE_3D_OES_ENABLE
MALI_STATIC_INLINE void _gles_m200_td_set_wrap_mode_r( gles_texture_object *tex_obj )
{
	m200_texture_wrap_mode wrap_mode = _gles_m200_get_wrap_mode( tex_obj->wrap_r );
	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_TD_SET_WRAP_MODE_R( tex_obj->internal->tds[0], wrap_mode );
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	MALI_TD_SET_WRAP_MODE_R( tex_obj->internal->tds[1], wrap_mode );
	MALI_TD_SET_WRAP_MODE_R( tex_obj->internal->tds[2], wrap_mode );
#endif
}
#endif

#endif /* GLES_M200_TD_H */
