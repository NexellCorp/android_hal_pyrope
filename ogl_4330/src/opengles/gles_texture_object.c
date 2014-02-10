/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include <base/mali_byteorder.h>

#include "gles_texture_object.h"
#include "gles_config_extension.h"
#include "gles_context.h"
#include "gles_share_lists.h"
#include "gles_util.h"
#include "gles_convert.h"
#include <base/mali_debug.h>
#include <shared/mali_named_list.h>
#include <shared/egl_image_status.h>
#include "util/mali_math.h"
#include "gles_instrumented.h"
#include "gles_state.h"
#include "gles_object.h"
#include "gles_read_pixels.h"
#include <gles_common_state/gles_framebuffer_state.h>
#include "m200_backend/gles_fb_texture_object.h"
#include "m200_backend/gles_m200_texel_format.h"
#include "gles_framebuffer_object.h"
#include <shared/m200_gp_frame_builder.h>
#include <gles_mipmap.h>

#include <gles_fb_interface.h>
#include "egl/api_interface_egl.h"

#if EXTENSION_EGL_IMAGE_OES_ENABLE
#include <shared/egl_image_status.h>
#include "egl/egl_image.h"
#endif

int _gles_unpack_alignment_to_pitch(int unpack_alignment, int width, GLenum format, GLenum type)
{
	/* unpack_alignment is taken from the GLES state, so it should have a valid value */
	MALI_DEBUG_ASSERT( ( ( 1 == unpack_alignment ) ||
				( 2 == unpack_alignment ) ||
				( 4 == unpack_alignment ) ||
				( 8 == unpack_alignment ) ),
			( "invalid unpack alignment %d", unpack_alignment ) );

	/* This equation gives the exact same result as equation 3.13 in the OpenGL 2.0 spec for all allowed values
	 * of unpack_alignment (1, 2, 4 and 8).
	 */
	return ( ( width * _gles_m200_get_input_bytes_per_texel( type, format ) + unpack_alignment - 1 ) / unpack_alignment ) * unpack_alignment;
}

mali_bool _gles_mipchain_is_complete( struct gles_texture_object* tex_obj, int chain, mali_bool uses_miplevels )
{
	int i = 0;
	int wb, hb; /* width, height of base level */
	GLenum tb, fb;  /* type and format of base level */
	struct mali_surface_specifier s0_f;

	/* get base mipmap level */
	const GLint level_base = 0;

	/* first, check that the base level exist */
	gles_mipmap_level *base = NULL;
	gles_mipchain* mipchain;
	mali_surface* surf_0;

	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	MALI_DEBUG_ASSERT_POINTER( tex_obj->internal );

	mipchain = tex_obj->mipchains[chain];
	surf_0 = _gles_fb_texture_object_get_mali_surface(tex_obj->internal, chain, 0);

	if ( NULL == surf_0 ) return MALI_FALSE;

	MALI_DEBUG_ASSERT_POINTER( mipchain );

	base = mipchain->levels[level_base];
	if ( NULL == base ) return MALI_FALSE;

	/* if we do not use miplevels, as long as the base is present, the mipchain is complete. Early out. */
	if (uses_miplevels == MALI_FALSE) return MALI_TRUE;

	wb = base->width;
	hb = base->height;

	tb = base->type;
	fb = base->format;

	s0_f = surf_0->format;

	/* we check level 0, just to make 1x1x1 base levels work without special cases */
	for ( i = 0; i < GLES_MAX_MIPMAP_LEVELS; ++i )
	{
		int w = MAX( wb >> i, 1 );
		int h = MAX( hb >> i, 1 );
		mali_surface* miplevel_surf;
		gles_mipmap_level *miplevel = mipchain->levels[i];

		if (
				NULL == miplevel       ||
				miplevel->width  != w  ||
				miplevel->height != h  ||
				miplevel->type   != tb ||
				miplevel->format != fb
		   )
		{
			MALI_DEBUG_PRINT( 3, ( "texture completeness test failed on level: %d\n", i ) );
			MALI_DEBUG_PRINT( 3, ( "miplevel                   : 0x%08x\n", ( u32 )miplevel ) );
			if ( NULL != miplevel )
			{
				MALI_DEBUG_PRINT( 3, ( "miplevel->width  != w  : %d, %d\n", miplevel->width, w )   );
				MALI_DEBUG_PRINT( 3, ( "miplevel->height != h  : %d, %d\n", miplevel->height, h )  );
				MALI_DEBUG_PRINT( 3, ( "miplevel->type   != tb : %d, %d\n", miplevel->type, tb )   );
				MALI_DEBUG_PRINT( 3, ( "miplevel->format != fb : %d, %d\n", miplevel->format, fb ) );
			}

			return MALI_FALSE;
		}

		miplevel_surf = _gles_fb_texture_object_get_mali_surface(tex_obj->internal, chain, i);

		if (
				NULL == miplevel_surf		 		|| 
				miplevel_surf->format.width != w	||
				miplevel_surf->format.height != h	||
				miplevel_surf->format.texel_format != s0_f.texel_format	||
				miplevel_surf->format.texel_layout != s0_f.texel_layout	
				|| (    tex_obj->internal->using_td_pitch_field && tex_obj->internal->linear_pitch_lvl0 != miplevel_surf->format.pitch )
				|| (    !tex_obj->internal->using_td_pitch_field 
					&& s0_f.texel_layout == M200_TEXTURE_ADDRESSING_MODE_LINEAR 
					&& (tex_obj->internal->linear_pitch_lvl0>>i) != miplevel_surf->format.pitch 
				   )
		   )
		{
			MALI_DEBUG_PRINT( 3, ( "texture completeness test failed on level: %d\n", i ) );
			MALI_DEBUG_PRINT( 3, ( "miplevel_surf                   : 0x%08x\n", ( u32 )miplevel_surf ) );
			if ( NULL != miplevel_surf )
			{
				MALI_DEBUG_PRINT( 3, ( "miplevel_surf->width  != s0_w  : %d, %d\n", miplevel_surf->format.width, w ) );
				MALI_DEBUG_PRINT( 3, ( "miplevel_surf->height != s0_h  : %d, %d\n", miplevel_surf->format.height, h ) );
				MALI_DEBUG_PRINT( 3, ( "miplevel_surf->format.pixel_format != s0_f.pixel_format : %d, %d\n", miplevel_surf->format.pixel_format, s0_f.pixel_format ) );
				MALI_DEBUG_PRINT( 3, ( "miplevel_surf->format.pixel_layout != s0_f.pixel_layout : %d, %d\n", miplevel_surf->format.pixel_layout, s0_f.pixel_layout ) );
				MALI_DEBUG_PRINT( 3, ( "miplevel_surf->format.pixel_layout == MALI_PIXEL_LAYOUT_LINEAR  : %d, %d\n", miplevel_surf->format.pixel_layout, MALI_PIXEL_LAYOUT_LINEAR ) );
				MALI_DEBUG_PRINT( 3, ( "miplevel_surf->pitch  != s0_p  : %d, %d\n", miplevel_surf->format.pitch, surf_0->format.pitch ) );
				MALI_DEBUG_PRINT( 3, ( "using the pitch field: %d\n", tex_obj->internal->using_td_pitch_field ) );
				MALI_DEBUG_PRINT( 3, ( "pitch field requitement: s0_p must be divisible by 8. %s\n", ((surf_0->format.pitch % 8) == 0)?"OK":"FAIL" ) );
			}

			return MALI_FALSE;
		}

		if ( w == 1 && h == 1)
		{
			tex_obj->complete_levels = i+1; /* counting the number of levels before we have a 1x1 texture */
			break;
		}
	}

	return MALI_TRUE;
}

void _gles_texture_object_get_internal_component_flags( gles_texture_object *tex_obj, mali_bool *red_blue_swap, mali_bool *reverse_order)
{
	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	MALI_DEBUG_ASSERT_POINTER( tex_obj->internal );
	MALI_DEBUG_ASSERT_POINTER( red_blue_swap );
	MALI_DEBUG_ASSERT_POINTER( reverse_order );
	*red_blue_swap = tex_obj->internal->red_blue_swap;
	*reverse_order = tex_obj->internal->order_invert;
}

void _gles_texture_object_init( gles_texture_object *tex_obj, enum gles_texture_target dimensionality )
{
	int i;

	MALI_DEBUG_ASSERT_POINTER( tex_obj );

	/* It should never be legal to get or create a new texture object with dimensionality GLES_TEXTURE_TARGET_INVALID. 
	 * These cases should always be caught and handled by the parent function */
	MALI_DEBUG_ASSERT( dimensionality != GLES_TEXTURE_TARGET_INVALID, ("dimensionality is in an illegal state") ); 


#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	if(dimensionality == GLES_TEXTURE_TARGET_EXTERNAL)
	{
		tex_obj->wrap_s = GL_CLAMP_TO_EDGE;
		tex_obj->wrap_t = GL_CLAMP_TO_EDGE;
	}
	else
	{
		tex_obj->wrap_s = GL_REPEAT;
		tex_obj->wrap_t = GL_REPEAT;
	}
#else
	tex_obj->wrap_s = GL_REPEAT;
	tex_obj->wrap_t = GL_REPEAT;
#endif
#if EXTENSION_TEXTURE_3D_OES_ENABLE
	tex_obj->wrap_r = GL_REPEAT;
#endif

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	tex_obj->yuv_info.colorrange = GLES_COLORRANGE_REDUCED;
	tex_obj->yuv_info.colorspace = GLES_COLORSPACE_BT_601;
	tex_obj->yuv_info.image_units_count = 1;
	tex_obj->yuv_info.is_rgb = MALI_FALSE;
#endif

#if EXTENSION_VIDEO_CONTROLS_ARM_ENABLE
	tex_obj->yuv_info.brightness = 0.0f;
	tex_obj->yuv_info.contrast   = 1.0f;
	tex_obj->yuv_info.saturation = 1.0f;
#endif

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	if(dimensionality == GLES_TEXTURE_TARGET_EXTERNAL)
	{
		tex_obj->min_filter = GL_LINEAR;
	}
	else
	{
		tex_obj->min_filter = GL_NEAREST_MIPMAP_LINEAR;
	}
#else
	tex_obj->min_filter = GL_NEAREST_MIPMAP_LINEAR;
#endif

	tex_obj->mag_filter = GL_LINEAR;
	tex_obj->generate_mipmaps = GL_FALSE;

	tex_obj->max_anisotropy    = 0;

	tex_obj->paletted = MALI_FALSE;
	tex_obj->paletted_mipmaps = MALI_FALSE;

	for ( i = 0; i < GLES_MAX_MIPCHAINS; ++i )
	{
		tex_obj->mipchains[i] = NULL;
	}

	tex_obj->dimensionality = dimensionality;

	_mali_sys_atomic_initialize(&tex_obj->ref_count, 1);
	tex_obj->internal = NULL;

	tex_obj->dirty = MALI_TRUE;
	tex_obj->completeness_check_dirty = MALI_TRUE;
	tex_obj->complete_levels = 0;
	tex_obj->mipgen_dirty = MALI_TRUE;
	tex_obj->is_complete = MALI_FALSE;
	tex_obj->is_deleted = MALI_FALSE;

#if EXTENSION_DRAW_TEX_OES_ENABLE
	tex_obj->crop_rect[ 0 ] = 0;
	tex_obj->crop_rect[ 1 ] = 0;
	tex_obj->crop_rect[ 2 ] = 0;
	tex_obj->crop_rect[ 3 ] = 0;
#endif


}

void _gles_texture_object_init_internal_object(gles_texture_object* tex_obj)
{
	/* update the initial filter settings to texture descriptor */
	_gles_m200_td_min_filter( tex_obj );
	_gles_m200_td_mag_filter( tex_obj );
}

gles_texture_object *_gles_texture_object_new( enum gles_texture_target dimensionality, mali_base_ctx_handle base_ctx )
{
	gles_texture_object *obj = _mali_sys_malloc( sizeof( gles_texture_object ) );
	if ( NULL == obj ) return NULL;

	/* It should never be legal to get or create a new texture object with dimensionality GLES_TEXTURE_TARGET_INVALID. 
	 * These cases should always be caught and handled by the parent function */
	MALI_DEBUG_ASSERT( dimensionality != GLES_TEXTURE_TARGET_INVALID, ("dimensionality is in an illegal state") ); 

	_gles_texture_object_init( obj, dimensionality );

	/* allocate internal object */

	obj->internal = _gles_fb_texture_object_alloc(dimensionality, base_ctx);
	if (NULL == obj->internal)
	{
		_mali_sys_free( obj );
		return  NULL;
	}

	/* initialize the internal object */
	_gles_texture_object_init_internal_object(obj);

	return obj;
}

/**
 * @brief Free a mipmap level that the texture object controls
 * @param tex_obj the texture object
 * @param mipchain_index the index of the mipchain
 * @param miplevel the mipmaplevel to free
 */
MALI_STATIC void _gles_texture_object_free_miplevel( gles_texture_object *tex_obj, int mipchain_index, int miplevel )
{
	gles_mipchain *mipchain = NULL;

	/* validate input */
	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	MALI_DEBUG_ASSERT_RANGE(mipchain_index, 0, GLES_MAX_MIPCHAINS - 1);
	MALI_DEBUG_ASSERT_RANGE(miplevel,       0, GLES_MAX_MIPMAP_LEVELS - 1);

	/* get mipchain */
	mipchain = tex_obj->mipchains[mipchain_index];
	if ( NULL == mipchain ) return;

	/* free level */
	if ( NULL != mipchain->levels[miplevel] )
	{
#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
		if ( NULL != mipchain->levels[ miplevel ]->fbo_connection )
		{
			_gles_fbo_bindings_free(mipchain->levels[ miplevel ]->fbo_connection); /* guaranteed empty; this function asserts that */
			mipchain->levels[ miplevel ]->fbo_connection = NULL;
		}
#endif /* EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE */

		_gles_mipmap_level_delete( mipchain->levels[miplevel] );
		mipchain->levels[miplevel] = NULL;
	}
}

void _gles_texture_object_delete( gles_texture_object *tex_obj )
{
	MALI_DEBUG_ASSERT_POINTER( tex_obj );

	if ( tex_obj != NULL )
	{
		int i;

		if (NULL != tex_obj->internal) _gles_fb_texture_object_deref( tex_obj->internal );
		tex_obj->internal = NULL;

		/* Free mipchains */
		for ( i=0; i<GLES_MAX_MIPCHAINS; i++ )
		{
			int j;
			gles_mipchain *mipchain = tex_obj->mipchains[i];

			if ( NULL == mipchain ) continue;

			for ( j = 0; j < GLES_MAX_MIPMAP_LEVELS; ++j )
			{
				_gles_texture_object_free_miplevel(tex_obj, i, j);
			}

			_mali_sys_free( mipchain );
			mipchain = NULL;
			tex_obj->mipchains[i] = NULL;
		}

		_mali_sys_free( tex_obj );

		tex_obj = NULL;
	}
}


void _gles_texture_object_list_entry_delete( struct gles_wrapper *wrapper )
{
	MALI_DEBUG_ASSERT_POINTER( wrapper );

	if ( NULL != wrapper )
	{
		if ( NULL != wrapper->ptr.tex )
		{
			/* dereference texture object */
			_gles_texture_object_deref( wrapper->ptr.tex );
			wrapper->ptr.tex = NULL;
		}

		/* delete wrapper */
		_gles_wrapper_free( wrapper );
		wrapper = NULL;
	}
}

void _gles_texture_object_deref( gles_texture_object *tex_obj )
{
	u32 ref_count = 0;
	MALI_DEBUG_ASSERT_POINTER( tex_obj );

	MALI_DEBUG_ASSERT( ( _mali_sys_atomic_get( &tex_obj->ref_count ) > 0 ), ( "ref_count on texture object is already 0, the object should have been deleted by now!\n" ) );

	/* dereference */
	ref_count = _mali_sys_atomic_dec_and_return( &tex_obj->ref_count );
	if ( ref_count > 0 ) return;

	_gles_texture_object_delete( tex_obj );
	tex_obj = NULL;
}

MALI_PURE int _gles_texture_object_get_mipchain_index( GLenum target )
{
	switch ( target )
	{
#if EXTENSION_TEXTURE_1D_OES_ENABLE
		case GL_TEXTURE_1D: return 0;
#endif
		case GL_TEXTURE_2D: return 0;

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
		case GL_TEXTURE_EXTERNAL_OES: return 0;
#endif

#if EXTENSION_TEXTURE_3D_OES_ENABLE
		case GL_TEXTURE_3D_OES: return 0;
#endif

					/* The enums are not supported unless OpenGL ES 2.x or the cubemap extension
					   are included in the build */
#if MALI_USE_GLES_2 || EXTENSION_TEXTURE_CUBE_MAP_ENABLE
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X: return 0;
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X: return 1;
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y: return 2;
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y: return 3;
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z: return 4;
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z: return 5;
#endif
		default:
						     MALI_DEBUG_ASSERT( 0, ( "unknown texture target" ) );
						     return 0;
	}
}

gles_mipchain *_gles_texture_object_get_mipmap_chain( gles_texture_object *tex_obj, int chain_index )
{
	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	MALI_DEBUG_ASSERT( chain_index < GLES_MAX_MIPCHAINS, ("Invalid mipchain index: %d\n", chain_index) );

	/* make sure the mip-chain exist */
	if ( NULL == tex_obj->mipchains[chain_index] )
	{
		int i;
		tex_obj->mipchains[chain_index] = _mali_sys_malloc( sizeof( gles_mipchain ) );
		if ( NULL == tex_obj->mipchains[chain_index] ) return NULL;

		/* clear struct */
		for ( i = 0; i < GLES_MAX_MIPMAP_LEVELS; ++i )
		{
			tex_obj->mipchains[chain_index]->levels[i] = NULL;
		}
	}

	return tex_obj->mipchains[chain_index];
}

gles_mipmap_level *_gles_texture_object_get_mipmap_level( gles_texture_object *tex_obj, GLenum target, int level )
{
	int chain_index;
	gles_mipchain *chain = NULL;

	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	MALI_DEBUG_ASSERT( level>=0, ("Only positive mipmap levels are supported, but got %d", level ) );

	chain_index = _gles_texture_object_get_mipchain_index( target );

	chain = _gles_texture_object_get_mipmap_chain( tex_obj, chain_index );
	if ( NULL == chain ) return NULL;

	return chain->levels[level];
}

/**
 * @brief Sets a specific mipmap level
 * @param tex_obj the texture object to set the mipmap level for
 * @param chain the texture target chain id to set the mipmap level for
 * @param level the mipmap level to set the mipmap level for
 * @param mip_level the mipmap pointer to set
 * @return GL_NO_ERROR if successful, an errorcode if not
 */
MALI_STATIC mali_err_code _gles_texture_object_set_frontend_mipmap_level( gles_texture_object *tex_obj, int chain, int level, gles_mipmap_level *mip_level )
{
	gles_mipchain *mip_chain = NULL;
	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	MALI_DEBUG_ASSERT_POINTER( mip_level );

	mip_chain = _gles_texture_object_get_mipmap_chain( tex_obj, chain );
	if ( NULL == mip_chain ) return MALI_ERR_OUT_OF_MEMORY;
	if(mip_chain->levels[level] != NULL) {
		_gles_mipmap_level_delete(mip_chain->levels[level]);
	}
	mip_chain->levels[level] = mip_level;
	return MALI_ERR_NO_ERROR;
}


/* same as above, but allocates a new miplevel if none was found */
gles_mipmap_level *_gles_texture_object_get_mipmap_level_assure( gles_texture_object *tex_obj, int chain, int level, GLint width, GLint height, GLenum format, GLenum type)
{
	gles_mipmap_level *miplevel = NULL;
	gles_mipchain* mipchain = NULL;

	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_DEBUG_ASSERT( chain >= 0, ("mipchain index out of range"));
	MALI_DEBUG_ASSERT( chain <  GLES_MAX_MIPCHAINS, ("mipchain index out of range"));

	mipchain = _gles_texture_object_get_mipmap_chain( tex_obj, chain );
	if ( NULL == mipchain ) return NULL;
	miplevel = mipchain->levels[level];
	if (NULL == miplevel)
	{
		mali_err_code err;

		miplevel = _gles_mipmap_level_new(width, height);
		if ( NULL == miplevel ) return NULL;

		err = _gles_texture_object_set_frontend_mipmap_level( tex_obj, chain, level, miplevel );
		if ( err != MALI_ERR_NO_ERROR )
		{
			MALI_DEBUG_ASSERT( err == MALI_ERR_OUT_OF_MEMORY, ( "inconsistent error. got %x, assumed %x", err, MALI_ERR_OUT_OF_MEMORY ) );
			_gles_mipmap_level_delete(miplevel);
			miplevel = NULL;
			return NULL;
		}
	}

	MALI_DEBUG_ASSERT_POINTER( tex_obj->mipchains[ chain ] );
	MALI_DEBUG_ASSERT( tex_obj->mipchains[ chain ]->levels[ level ] == miplevel, ( "something went very wrong" ) );

	miplevel->width = width;
	miplevel->height = height;
	miplevel->format = format;
	miplevel->type = type;
	miplevel->temp_consumer = MALI_NO_HANDLE;

	return miplevel;
}

MALI_CHECK_RESULT gles_texture_object *_gles_get_texobj(
		struct gles_context *ctx,
		GLint name,
		enum gles_texture_target dimensionality
		)
{
	gles_texture_object *obj = NULL;
	struct gles_wrapper *name_wrapper = NULL;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	/* It should never be legal to get or create a new texture object with dimensionality GLES_TEXTURE_TARGET_INVALID. 
	 * These cases should always be caught and handled by the parent function */
	MALI_DEBUG_ASSERT( dimensionality != GLES_TEXTURE_TARGET_INVALID, ("dimensionality is in an illegal state") ); 

	/* check for default texture object */
	if ( 0 == name )
	{
		obj = ctx->default_texture_object[dimensionality];
	}
	else
	{
		/* search the list */
		name_wrapper = __mali_named_list_get( ctx->share_lists->texture_object_list, name );
		if ( name_wrapper != NULL )
		{
			/* the name has been reserved.
			 * Note that the texture object might not have been created yet, so name_wrapper->tex_obj
			 * could still be NULL
			 */
			obj = name_wrapper->ptr.tex;
		}
		else
		{
			/* the name has not been reserved, so there is no texture object created either */
			obj = NULL;
		}
	}

	/* the texture object was not found in the list, try to create it  */
	if ( NULL == obj )
	{
		/* texture object not yet allocated, let's do that now. */
		obj = _gles_texture_object_new(dimensionality, ctx->base_ctx);
		if (NULL == obj) return NULL;

		obj->dimensionality = dimensionality;
		_gles_m200_td_dimensionality( obj );

		MALI_DEBUG_ASSERT( _mali_sys_atomic_get( &obj->ref_count ) == 1, ( "refcount not as expected" ) );

		if ( name_wrapper != NULL )
		{
			/* name has already been reserved - e.g. glBindTexture after glGenTextures */
			name_wrapper->ptr.tex = obj;
		}
		else
		{
			/* no name has been reserved - e.g. glBindTexture _without_ glGenTextures */
			name_wrapper = _gles_wrapper_alloc(WRAPPER_TEXTURE);
			if (NULL == name_wrapper)
			{
				_gles_texture_object_delete( obj );
				obj = NULL;

				return NULL;
			}

			name_wrapper->ptr.tex = obj;

			if (MALI_ERR_NO_ERROR != __mali_named_list_insert(ctx->share_lists->texture_object_list, name, name_wrapper))
			{
				_gles_texture_object_delete( obj );
				obj = NULL;
				name_wrapper->ptr.tex = NULL;

				_gles_wrapper_free( name_wrapper );
				name_wrapper = NULL;

				return NULL;
			}
		}
	}

	return obj;
}

/** checks for format and type mismatches */
MALI_STATIC GLenum _gles_check_texture_format_and_type_errors( GLenum format, GLenum internalformat, GLenum type )
{
	switch ( internalformat )
	{
		case GL_RGB:
#if EXTENSION_BGRA8888_ENABLE
		case GL_BGRA_EXT:
#endif
		case GL_RGBA:
		case GL_LUMINANCE:
		case GL_ALPHA:
		case GL_LUMINANCE_ALPHA:
#if EXTENSION_DEPTH_TEXTURE_ENABLE
		case GL_DEPTH_COMPONENT:
			/* GL_PACKED_DEPTH_STENCIL_OES is only valid if depth_textures extension is enabled */
#if EXTENSION_PACKED_DEPTH_STENCIL_ENABLE
		case GL_DEPTH_STENCIL_OES:
#endif
#endif
			break;

		default:
			/* legacy error code.  This used to be an integer value and the error
			 * code defined in the spec has remained */
			return GL_INVALID_VALUE;
	}

	switch ( type )
	{
		case GL_UNSIGNED_BYTE:
		case GL_UNSIGNED_SHORT:
		case GL_UNSIGNED_SHORT_5_6_5:
		case GL_UNSIGNED_SHORT_5_5_5_1:
		case GL_UNSIGNED_SHORT_4_4_4_4:
#if EXTENSION_DEPTH_TEXTURE_ENABLE
		case GL_UNSIGNED_INT: /* supported for GL_DEPTH_COMPONENT */
			/* GL_PACKED_DEPTH_STENCIL_OES is only valid if depth_textures extension is enabled */
#if EXTENSION_PACKED_DEPTH_STENCIL_ENABLE
		case GL_UNSIGNED_INT_24_8_OES: /* supported for GL_DEPTH_STENCIL_OES */
#endif
#endif
#if EXTENSION_TEXTURE_HALF_FLOAT_OES_ENABLE
		case GL_HALF_FLOAT_OES:
#endif
			break;

		default:
			return GL_INVALID_ENUM;
	}

	if (format != internalformat)
	{
		return GL_INVALID_OPERATION;
	}

	switch ( format )
	{
		case GL_RGB:
			if (!( GL_UNSIGNED_BYTE == type || GL_UNSIGNED_SHORT_5_6_5 == type
#if EXTENSION_TEXTURE_HALF_FLOAT_OES_ENABLE
						|| GL_HALF_FLOAT_OES == type
#endif
			     ))
			{
				return GL_INVALID_OPERATION;
			}
			break;

#if EXTENSION_BGRA8888_ENABLE
		case GL_BGRA_EXT:
			if ( GL_UNSIGNED_BYTE != type )
			{
				return GL_INVALID_OPERATION;
			}
			break;
#endif

		case GL_RGBA:
			if (!( GL_UNSIGNED_BYTE == type||
						GL_UNSIGNED_SHORT == type ||
						GL_UNSIGNED_SHORT_5_5_5_1 == type ||
#if EXTENSION_TEXTURE_HALF_FLOAT_OES_ENABLE
						GL_HALF_FLOAT_OES == type || 
#endif
						GL_UNSIGNED_SHORT_4_4_4_4 == type ))
			{
				return GL_INVALID_OPERATION;
			}
			break;

		case GL_LUMINANCE:
		case GL_ALPHA:
			if ( GL_UNSIGNED_BYTE != type )
			{
				return GL_INVALID_OPERATION;
			}
			break;

		case GL_LUMINANCE_ALPHA:
			if (!( GL_UNSIGNED_BYTE == type || GL_UNSIGNED_SHORT == type ))
			{
				return GL_INVALID_OPERATION;
			}
			break;

#if EXTENSION_DEPTH_TEXTURE_ENABLE
		case GL_DEPTH_COMPONENT:
			if (!( GL_UNSIGNED_SHORT == type || GL_UNSIGNED_INT == type ))
			{
				return GL_INVALID_OPERATION;
			}
			break;
			/* GL_PACKED_DEPTH_STENCIL_OES is only valid if depth_textures extension is enabled */
#if EXTENSION_PACKED_DEPTH_STENCIL_ENABLE
		case GL_DEPTH_STENCIL_OES:
			if ( GL_UNSIGNED_INT_24_8_OES != type )
			{
				return GL_INVALID_OPERATION;
			}
			break;
#endif

#endif

		default:
			return GL_INVALID_OPERATION;
	}

	return GL_NO_ERROR;
}

mali_err_code _gles_texture_miplevel_set_renderable(
		struct gles_context* ctx,
		struct gles_texture_object *tex_obj,
		GLenum target,
		GLint miplevel)
{
	mali_bool old_state;
	GLint mipchain;
	mali_surface* surf;

	mipchain = _gles_texture_object_get_mipchain_index(target);

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_DEBUG_ASSERT_RANGE(mipchain, 0, GLES_MAX_MIPCHAINS-1);
	MALI_DEBUG_ASSERT_POINTER(tex_obj->mipchains[mipchain]);
	MALI_DEBUG_ASSERT_RANGE(miplevel, 0, GLES_MAX_MIPMAP_LEVELS-1);
	MALI_DEBUG_ASSERT_POINTER(tex_obj->mipchains[mipchain]->levels[miplevel]);


	/* if the flag is already set, just early out without doing anything */
	old_state = _gles_fb_texture_object_get_renderable(tex_obj->internal, mipchain, miplevel);
	if(old_state) return MALI_ERR_NO_ERROR;

	/* at this point, the renderable flag is false. If there are outstanding reads on the texture object,
	 * we must perform a deep copy to eliminate these. This is the equivalent of pretending to write to the surface.
	 * The following function returns a pointer to a writeable surface, preparing the texobj for such a write. 
	 * We just ignore the return error. This will resolve all outstanding GLES-style dependencies. */

#ifndef NDEBUG 
	{
		/* debug assert */
		gles_mipchain* front_mipchain;
		gles_mipmap_level *front_miplevel;
		front_mipchain = _gles_texture_object_get_mipmap_chain( tex_obj, mipchain );
		MALI_DEBUG_ASSERT_POINTER( front_mipchain ); /* if the back object exist, the front object should too! */
		front_miplevel = front_mipchain->levels[miplevel];
		MALI_DEBUG_ASSERT_POINTER( front_miplevel ); /* if the back object exist, the front object should too! */
	}
#endif

	/* the cleanest way to eliminate outstanding reads is to call lock and unlock */
	surf = _gles_texture_miplevel_lock(ctx, tex_obj, mipchain, miplevel);
	if(!surf) return MALI_ERR_OUT_OF_MEMORY; 
	_gles_texture_miplevel_unlock(ctx, tex_obj, mipchain, miplevel);


	/* after resolving write dependencies, the context lock guarantees that no other
	 * dependencies have been added. So simply set the state and exit */
	_gles_fb_texture_object_set_renderable(tex_obj->internal, mipchain, miplevel);

	return MALI_ERR_NO_ERROR;
}

GLenum _gles_tex_image_2d_internal(
		gles_texture_object *tex_obj,
		struct gles_context *ctx,
		GLenum target,
		GLint level,
		GLsizei width,
		GLsizei height,
		GLenum format,
		GLenum type,
		GLboolean src_red_blue_swap,
		GLboolean src_reverse_order,
		GLvoid *pixels,
		int pitch
		)
{
	struct mali_named_list *framebuffer_object_list = NULL;
	struct gles_framebuffer_state *fb_state         = NULL;
	mali_err_code err = MALI_ERR_NO_ERROR;
	int chain_index = _gles_texture_object_get_mipchain_index( target );

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);
	MALI_DEBUG_ASSERT( GLES_MAX_MIPMAP_LEVELS > level, ( "level out of range! ( attempted to upload mipmaplevel %i, range is %i )", level, GLES_MAX_MIPMAP_LEVELS ) );

	framebuffer_object_list = ctx->share_lists->framebuffer_object_list;
	fb_state = &ctx->state.common.framebuffer;

	/* We have changed a mipmap-level, we need to check for completeness again */
	tex_obj->completeness_check_dirty = MALI_TRUE;

	if ( 0 < width && 0 < height)
	{
		mali_surface      *surf         = NULL;

		MALI_IGNORE(fb_state);
		MALI_IGNORE(framebuffer_object_list);

		/*FETCH*/
		surf = _gles_texture_miplevel_allocate(ctx, tex_obj, chain_index, level, width, height, format, type);
		if(surf == NULL) return GL_OUT_OF_MEMORY;

		/*UPLOAD*/
		_profiling_enter( INST_GL_UPLOAD_TEXTURE_TIME);
		err = _gles_fb_tex_image_2d( ctx->texture_frame_builder, tex_obj->internal, surf,
				width, height, format, type, src_red_blue_swap, src_reverse_order, pixels, pitch );
		_profiling_leave( INST_GL_UPLOAD_TEXTURE_TIME);

		/* handle upload errors */
		if( MALI_ERR_NO_ERROR != err)
		{
			_mali_surface_deref(surf); /* surface was not assigned anywhere, this deletes it */
			MALI_DEBUG_ASSERT( MALI_ERR_OUT_OF_MEMORY == err, ("unexpected error code 0x%x", err) );
			return GL_OUT_OF_MEMORY;
		}

		/* ASSIGN */
		err = _gles_texture_miplevel_assign(ctx, tex_obj, chain_index, level, format, type, 1, &surf);
		if(MALI_ERR_NO_ERROR != err)
		{
			_mali_surface_deref(surf); /* surface assignment failed, this deletes the surface */
			MALI_DEBUG_ASSERT( MALI_ERR_OUT_OF_MEMORY == err, ("unexpected error code 0x%x", err) );
			return GL_OUT_OF_MEMORY;
		}

	}
	else
	{
		/* width or height = 0 means NULL-texture. This should remove the fb miplevel, but keep the texobj miplevel intact, as it still may have FBO bindings */
		/* Must not delete the gles_mipmap_level since it (possibly) contains linkage to fbos */
		err = _gles_texture_miplevel_assign(ctx, tex_obj, chain_index, level, format, type, 0, NULL);
		if( MALI_ERR_NO_ERROR != err)
		{
			return GL_OUT_OF_MEMORY;
		}
	}

	tex_obj->paletted = MALI_FALSE;

	return _gles_convert_mali_err(err);
}

GLenum _gles_tex_image_2d(
		gles_texture_object *tex_obj,
		struct gles_context *ctx,
		GLenum target,
		GLint level,
		GLint internalformat,
		GLsizei width,
		GLsizei height,
		GLint border,
		GLenum format,
		GLenum type,
		const GLvoid *pixels,
		int pitch )
{
	GLenum err;
	GLboolean red_blue_swap, reverse_order;

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	if ( GL_TEXTURE_EXTERNAL_OES == target ) return GL_INVALID_ENUM;
#endif

	err = _gles_check_texture_format_and_type_errors( format, internalformat, type );
	if ( GL_NO_ERROR != err ) return err;

	if ( ( level < 0 ) || ( level >= GLES_MAX_MIPMAP_LEVELS ) ) return GL_INVALID_VALUE;
	if ( border != 0 ) return GL_INVALID_VALUE;

	/* width & height must be 0 <= and <= max texture size */
	if ( ( width < 0 ) || ( height < 0 ) ) return GL_INVALID_VALUE;

	/* check that this level is not too large */
	if ( ( width > GLES_MAX_TEXTURE_SIZE ) || ( height > GLES_MAX_TEXTURE_SIZE ) ) return GL_INVALID_VALUE;

	/* We also need to check that level 0 won't be too large */
	if ( ( ( width << level ) > GLES_MAX_TEXTURE_SIZE ) || ( ( height << level ) > GLES_MAX_TEXTURE_SIZE ) ) return GL_INVALID_VALUE;

	MALI_CHECK_NON_NULL( tex_obj->internal, GL_OUT_OF_MEMORY );

	/* get the standard GL red_blue_swap and reverse_order flags */
	_gles_m200_get_gles_input_flags(type, format, &red_blue_swap, &reverse_order);
	/* call the internal function */
	err = _gles_tex_image_2d_internal(tex_obj, ctx, target, level, width, height, format, type, red_blue_swap, reverse_order, (GLvoid *)pixels, pitch);
	return err;
}

GLenum _gles_tex_sub_image_2d_internal(
		gles_texture_object *tex_obj,
		struct gles_context *ctx,
		GLenum target,
		GLint level,
		GLint xoffset,
		GLint yoffset,
		GLsizei width,
		GLsizei height,
		GLenum format,
		GLenum type,
		GLboolean red_blue_swap,
		GLboolean reverse_order,
		const GLvoid *pixels,
		int pitch
		)
{
	struct mali_surface* surf;
	int mipchain = _gles_texture_object_get_mipchain_index( target );
	mali_err_code upload_err;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_DEBUG_ASSERT_POINTER(tex_obj->internal);

	/* tex sub image 2d should always work on an existing mipmap level */
	surf = 	_gles_fb_texture_object_get_mali_surface(tex_obj->internal, mipchain, level );
	if(!surf) return GL_INVALID_OPERATION; /* entrypoint error code when miplevel don't exist */

	/* FETCH the existing surface, and clear outstanding dependencies */
	surf = _gles_texture_miplevel_lock(ctx, tex_obj, mipchain, level);
	if(!surf) 
	{
		return GL_OUT_OF_MEMORY; 
	}

	/* UPLOAD */
	_profiling_enter( INST_GL_UPLOAD_TEXTURE_TIME);
	upload_err = _gles_fb_tex_sub_image_2d( 
			tex_obj->internal,
			surf,
			xoffset, yoffset,
			width, height,
			format,
			type,
			red_blue_swap,
			reverse_order,
			pixels,
			pitch);
	_profiling_leave( INST_GL_UPLOAD_TEXTURE_TIME);
	_gles_texture_miplevel_unlock(ctx, tex_obj, mipchain, level);
	if(upload_err != MALI_ERR_NO_ERROR) return GL_OUT_OF_MEMORY; /* entrypoint error when a potential CoW failed */

	return GL_NO_ERROR;
}

GLenum _gles_tex_sub_image_2d(
		gles_texture_object *tex_obj,
		struct gles_context *ctx,
		GLenum target,
		GLint level,
		GLint xoffset,
		GLint yoffset,
		GLsizei width,
		GLsizei height,
		GLenum format,
		GLenum type,
		const GLvoid *pixels,
		GLint unpack_alignment
		)
{
	GLenum err;
	GLboolean red_blue_swap, reverse_order;
	int pitch;
	gles_mipmap_level *miplevel = NULL;

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	if ( GL_TEXTURE_EXTERNAL_OES == target ) return GL_INVALID_ENUM;
#endif

	if ( ( level < 0 ) || ( level >= GLES_MAX_MIPMAP_LEVELS ) ) return GL_INVALID_VALUE;
	if ( ( xoffset<0 ) || ( yoffset< 0 ) )       return GL_INVALID_VALUE;
	if ( ( width < 0 ) || ( height < 0 ) )       return GL_INVALID_VALUE;

	/* check that a level has already been successfully uploaded */
	if ( NULL == tex_obj->mipchains[0] )         return GL_INVALID_OPERATION;
	miplevel = tex_obj->mipchains[0]->levels[level];
	if ( NULL == miplevel )                      return GL_INVALID_OPERATION;

	/* check that the miplevel already uploaded corresponds with the user input and that type & format are legal */
	err = _gles_check_texture_format_and_type_errors( format, miplevel->format, type );
	if ( GL_NO_ERROR != err )                    return err;

	if ( type   != miplevel->type   )            return GL_INVALID_OPERATION;
	if ( xoffset > miplevel->width  )            return GL_INVALID_VALUE;
	if ( yoffset > miplevel->height )            return GL_INVALID_VALUE;
	if ( width   > miplevel->width  )            return GL_INVALID_VALUE;
	if ( height  > miplevel->height )            return GL_INVALID_VALUE;
	if ( (xoffset + width ) > miplevel->width  ) return GL_INVALID_VALUE;
	if ( (yoffset + height) > miplevel->height ) return GL_INVALID_VALUE;

	if ( NULL == tex_obj->internal )             return GL_OUT_OF_MEMORY;

	/* zero width or height is allowed, but since there is nothing to update we can early-out */
	if ( (0==width) || (0==height) )             return GL_NO_ERROR;

	pitch = _gles_unpack_alignment_to_pitch(unpack_alignment, width, format, type);

	/* get the standard GL red_blue_swap and reverse_order flags */
	_gles_m200_get_gles_input_flags(type, format, &red_blue_swap, &reverse_order);

	/* call the internal function */
	err = _gles_tex_sub_image_2d_internal(
			tex_obj, ctx,
			target, level,
			xoffset, yoffset,
			width, height,
			format, type,
			red_blue_swap, reverse_order,
			pixels, pitch
			);

	return err;
}

GLenum _gles_compressed_texture_image_2d(
		gles_texture_object *tex_obj,
		struct gles_context *ctx,
		GLenum target,
		GLint level,
		GLenum internalformat,
		GLsizei width,
		GLsizei height,
		GLint border,
		GLsizei imageSize,
		const GLvoid *data )
{
	int mipchain;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->share_lists );
	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	MALI_DEBUG_ASSERT_POINTER( tex_obj->internal );

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	if ( GL_TEXTURE_EXTERNAL_OES == target ) return GL_INVALID_ENUM;
#endif

	if ( border != 0 ) return GL_INVALID_VALUE;
	if ( ( width < 0 ) || ( height < 0 ) ) return GL_INVALID_VALUE;

	mipchain = _gles_texture_object_get_mipchain_index(target);

#if EXTENSION_COMPRESSED_ETC1_RGB8_TEXTURE_OES_ENABLE
	if ( GL_ETC1_RGB8_OES == internalformat )
	{
		mali_err_code merr;
		mali_surface* surface;

		if ( level < 0 || level >= GLES_MAX_MIPMAP_LEVELS ) return GL_INVALID_VALUE;
		if ( ( width > GLES_MAX_TEXTURE_SIZE ) || ( height > GLES_MAX_TEXTURE_SIZE ) ) return GL_INVALID_VALUE;
		if ( ( ( width << level ) > GLES_MAX_TEXTURE_SIZE ) || ( ( height << level ) > GLES_MAX_TEXTURE_SIZE ) ) return GL_INVALID_VALUE;

		if( width > 0 && height > 0)
		{
			/* for ETC1, imageSize must be given in 4x4 blocks, 4 bits per pixel (aka 2 pixels per byte) */
			if ( MALI_ALIGN(width, 4) * MALI_ALIGN(height, 4) / 2 != imageSize) return GL_INVALID_VALUE; 

			/* FETCH */
			surface = _gles_texture_miplevel_allocate(ctx, tex_obj, mipchain, level, width, height, internalformat, GL_NONE);
			if (NULL == surface) return GL_OUT_OF_MEMORY;

			_profiling_enter( INST_GL_UPLOAD_TEXTURE_TIME);
			/* UPLOAD */
			merr = _gles_fb_compressed_texture_image_2d_etc(tex_obj->internal, surface, width, height, imageSize, data);
			_profiling_leave( INST_GL_UPLOAD_TEXTURE_TIME);
			if( MALI_ERR_NO_ERROR != merr)
			{
				_mali_surface_deref(surface);
				return GL_OUT_OF_MEMORY;
			}
	
			/* ASSIGN */
			merr = _gles_texture_miplevel_assign(ctx, tex_obj, mipchain, level, internalformat, 0 , 1, &surface);
			if( MALI_ERR_NO_ERROR != merr)
			{
				_mali_surface_deref(surface);
				return GL_OUT_OF_MEMORY;
			}
		} else {
			merr = _gles_texture_miplevel_assign(ctx, tex_obj, mipchain, level, internalformat, 0, 0, NULL);
			if( MALI_ERR_NO_ERROR != merr)
			{
				return GL_OUT_OF_MEMORY;
			}
		}

		return GL_NO_ERROR;
	} else
#endif
#if EXTENSION_COMPRESSED_PALETTED_TEXTURES_OES_ENABLE
	{
		mali_err_code merr;
		mali_surface* surface;
		int i;
		u32 user_palette_size;
		u32 texels_per_byte;
		u32 mip_width;
		u32 mip_height;
		switch ( internalformat )
		{
			/* The format / type combinations used to get the texel format are taken from Table 3.11 of Section 3.7.4 of the OpenGL ES 1.1 spec */
			case GL_PALETTE4_RGB8_OES:
				user_palette_size = 16*3; 
				texels_per_byte = 2;
				break;
			case GL_PALETTE4_RGBA8_OES:
				user_palette_size = 16*4; 
				texels_per_byte = 2;
				break;
			case GL_PALETTE4_R5_G6_B5_OES:
				user_palette_size = 16*2; 
				texels_per_byte = 2;
				break;
			case GL_PALETTE4_RGBA4_OES:
				user_palette_size = 16*2; 
				texels_per_byte = 2;
				break;
			case GL_PALETTE4_RGB5_A1_OES:
				user_palette_size = 16*2; 
				texels_per_byte = 2;
				break;
			case GL_PALETTE8_RGB8_OES:
				user_palette_size = 256*3; 
				texels_per_byte = 1;
				break;
			case GL_PALETTE8_RGBA8_OES:
				user_palette_size = 256*4; 
				texels_per_byte = 1;
				break;
			case GL_PALETTE8_R5_G6_B5_OES:
				user_palette_size = 256*2; 
				texels_per_byte = 1;
				break;
			case GL_PALETTE8_RGBA4_OES:
				user_palette_size = 256*2; 
				texels_per_byte = 1;
				break;
			case GL_PALETTE8_RGB5_A1_OES:
				user_palette_size = 256*2; 
				texels_per_byte = 1;
				break;
			default:
				return GL_INVALID_ENUM;
		}

		/* calculate the expected data size and check against the user-provided argument */
		{
			u32 num_bytes = user_palette_size;
			u32 mip_width = width;
			u32 mip_height = height;
			int current_miplevel;

			if( (mip_width*mip_height) >0 ) 
			{
				for ( current_miplevel = 0; current_miplevel <= -level; ++current_miplevel )
				{
					/* implements the level padding */
					num_bytes += ( (mip_width*mip_height) + texels_per_byte - 1) / texels_per_byte;

					/* calculate size of next mipmap level. Clamp to 1 to support rectangular textures */
					mip_width = MAX(1, mip_width/2);
					mip_height = MAX(1, mip_height/2);
				}
			}

			if( (u32)imageSize != num_bytes ) 
			{
				return GL_INVALID_VALUE;
			}

		}


		/* paletted textures are uploaded all in one go, and always from level 0, so no need to left-shift by level here */
		if ( level > 0 ) return GL_INVALID_VALUE;
		if ( ( -level ) >= GLES_MAX_MIPMAP_LEVELS ) return GL_INVALID_VALUE;
		if ( ( width > GLES_MAX_TEXTURE_SIZE ) || ( height > GLES_MAX_TEXTURE_SIZE ) ) return GL_INVALID_VALUE;

		/* All supported compressed formats, except ETC, are paletted. For paletted textures, all
		   mipmap levels are uploaded simultaniously, with level being _negative_ */

		if( width > 0 && height > 0)
		{
			tex_obj->paletted = MALI_TRUE;
			tex_obj->paletted_mipmaps = ( level != 0 ) ? MALI_TRUE : MALI_FALSE;

			mip_width = width;
			mip_height = height;
			for(i = 0; i <= (-level); i++)
			{
				/* FETCH */
				surface = _gles_texture_miplevel_allocate(ctx, tex_obj, mipchain, i, mip_width, mip_height, internalformat, GL_NONE);
				if (NULL == surface) return GL_OUT_OF_MEMORY;

				_profiling_enter( INST_GL_UPLOAD_TEXTURE_TIME);
				/* UPLOAD */
				merr = _gles_fb_compressed_texture_image_2d_paletted(tex_obj->internal, surface, internalformat, width, height, i, imageSize, data);
				_profiling_leave( INST_GL_UPLOAD_TEXTURE_TIME);
				if( MALI_ERR_NO_ERROR != merr)
				{
					_mali_surface_deref(surface);
					return GL_OUT_OF_MEMORY;
				}

				/* ASSIGN */
				merr = _gles_texture_miplevel_assign(ctx, tex_obj, mipchain, i, internalformat, 0 , 1, &surface);
				if( MALI_ERR_NO_ERROR != merr)
				{
					_mali_surface_deref(surface);
					return GL_OUT_OF_MEMORY;
				}

				if(mip_width>0) mip_width  = MAX(1, mip_width/2);
				if(mip_height>0) mip_height = MAX(1, mip_height/2);
			} /* for */
		} else {
			/* disable all the levels */
			for(i = 0; i <= (-level); i++)
			{
				merr = _gles_texture_miplevel_assign(ctx, tex_obj, mipchain, i, internalformat, 0, 0, NULL);
				if( MALI_ERR_NO_ERROR != merr)
				{
					return GL_OUT_OF_MEMORY;
				}
			}
		} 

		return GL_NO_ERROR;
	} 
#else  /* EXTENSION_COMPRESSED_PALETTED_TEXTURES_OES_ENABLE */
	{
		/* NOP */
		return GL_NO_ERROR;
	}
#endif /* EXTENSION_COMPRESSED_PALETTED_TEXTURES_OES_ENABLE */
}

/* internal implementation of glCompressedTexSubImage2D */
GLenum _gles_compressed_texture_sub_image_2d(
		gles_texture_object *tex_obj,
		struct gles_context *ctx,
		GLenum target,
		GLint level,
		GLint xoffset,
		GLint yoffset,
		GLsizei width,
		GLsizei height,
		GLenum format,
		GLsizei imageSize,
		const GLvoid *data )
{
	gles_mipmap_level *miplevel = NULL;

	MALI_DEBUG_ASSERT_POINTER( ctx );

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	if ( GL_TEXTURE_EXTERNAL_OES == target ) return GL_INVALID_ENUM;
#endif

	miplevel = _gles_texture_object_get_mipmap_level( tex_obj, target, 0 );
	if ( NULL == miplevel ) return GL_INVALID_OPERATION;
	if ( level < 0 || level > GLES_MAX_TEXTURE_SIZE_LOG2 ) return GL_INVALID_VALUE;

	return _gles_fb_compressed_texture_sub_image_2d( tex_obj->internal, level, xoffset, yoffset, width, height, format, imageSize, data );
}

GLenum _gles_copy_texture_image_2d(
		gles_texture_object *tex_obj,
		struct gles_context *ctx,
		GLenum target,
		GLint level,
		GLenum internalformat,
		GLint x,
		GLint y,
		GLsizei width,
		GLsizei height,
		GLint border )
{
	GLint readpixel_type; 
	GLint readpixel_format; 
	mali_err_code mali_err;
	GLenum gl_err;
	struct mali_surface* new_surface;
	void* pixels;
	int mipchain; 

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	if ( GL_TEXTURE_EXTERNAL_OES == target ) return GL_INVALID_ENUM;
#endif

	if ( ( level < 0 ) || ( level >= GLES_MAX_MIPMAP_LEVELS ) ) return GL_INVALID_VALUE;
	if ( ( x<0 ) || ( y<0 ) ) return GL_INVALID_VALUE;
	if ( ( width < 0 ) || ( height < 0 ) ) return GL_INVALID_VALUE;
	if ( ( width > GLES_MAX_TEXTURE_SIZE ) || ( height > GLES_MAX_TEXTURE_SIZE ) ) return GL_INVALID_VALUE;
	if ( ( ( width<<level ) > GLES_MAX_TEXTURE_SIZE ) || ( ( height<<level ) > GLES_MAX_TEXTURE_SIZE ) ) return GL_INVALID_VALUE;
	if ( border != 0 ) return GL_INVALID_VALUE;

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	/* Return a GL_INVALID_FRAMEBUFFER_OPERATION if the current framebuffer object is incomplete */
	if ( GL_FRAMEBUFFER_COMPLETE != _gles_framebuffer_internal_complete( ctx->state.common.framebuffer.current_object ) )
	{
		return GL_INVALID_FRAMEBUFFER_OPERATION;
	}
#endif

	_mali_frame_builder_acquire_output( ctx->state.common.framebuffer.current_object->draw_frame_builder );
	
	mipchain = _gles_texture_object_get_mipchain_index(target);

	/* can not create COLOR textures from FBOs with no color channel
	 * can not create ALPHA empowered textures from FBOs with no alpha channel
	 * These rules correspond to table 3.9 in the GLES spec. We don't actually store 
	 * the GLenum of the color buffer since not all buffers are created through GL.
	 * This is the closest we will get to adhering this rule. */
	switch ( internalformat )
	{
		case GL_RGBA: 
			/* GL_RGBA internalformat require a framebuffer with both a color and an alpha channel */
			if ( _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_ALPHA_BITS ) == 0 ) return GL_INVALID_OPERATION;
			if ( _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_RED_BITS ) == 0 ) return GL_INVALID_OPERATION;
			/* internalformat = GL_RGBA may be stored as 32bit RGBA8888, 16bit RGBA4444 or 16bit RGBA5551. 
			 * We're storing it as RGBA8888 no matter what. */
			readpixel_format = GL_RGBA;
			readpixel_type = GL_UNSIGNED_BYTE;
			break;
		case GL_LUMINANCE_ALPHA: 
			/* GL_RGBA internalformat require a framebuffer with both a color and an alpha channel */
			if ( _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_RED_BITS ) == 0 ) return GL_INVALID_OPERATION;
			if ( _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_ALPHA_BITS ) == 0 ) return GL_INVALID_OPERATION;
			/* internalformat = GL_LUMINANCE_ALPHA is always stored as 16bit LA88, regardless of framebuffer. */
			readpixel_format = GL_LUMINANCE_ALPHA;
			readpixel_type = GL_UNSIGNED_BYTE;
			break;
		case GL_ALPHA:
			/* GL_ALPHA internalformat require a framebuffer with an alpha channel */
			if ( _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_ALPHA_BITS ) == 0 ) return GL_INVALID_OPERATION;
			/* internalformat = GL_ALPHA is always stored as 8bit A8, regardless of framebuffer.*/
			readpixel_format = GL_ALPHA;
			readpixel_type = GL_UNSIGNED_BYTE;
			break;
		case GL_LUMINANCE:
			/* GL_LUMINANCE require a framebuffer with a color channel */
			if ( _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_RED_BITS ) == 0 ) return GL_INVALID_OPERATION;
			/* internalformat = GL_LUMINANCE is always stored as 8bit L8, regardless of framebuffer.*/
			readpixel_format = GL_LUMINANCE;
			readpixel_type = GL_UNSIGNED_BYTE;
			break;
		case GL_RGB:
			/* GL_RGB require a framebuffer with a color channel */
			if ( _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_RED_BITS ) == 0 ) return GL_INVALID_OPERATION;
			/* internalformat = GL_RGB may be stored as RGBx8888, RGB888 or RGB565 depending on framebuffer format.
			 * We're storing it as whatever is given by GL_RGB, GL_UNSIGNED_BYTE offers (as of writing, RGB888)*/
			readpixel_format = GL_RGB;
			readpixel_type = GL_UNSIGNED_BYTE;
			break;
		default:
			/* other internal formats are not accepted */
			return GL_INVALID_ENUM;
	}

	/* 16bit textures are not legal reading targets. Early out with an illegal operation if the framebuffer is a 16bit format
	 * This check covers both RGBA_16_16_16_16, and LA_16_16 formats */
	if ( _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_RED_BITS ) == 16 ) return GL_INVALID_OPERATION;

	new_surface = _gles_texture_miplevel_allocate(ctx, tex_obj, mipchain, level, width, height, readpixel_format, readpixel_type);
	if(new_surface == NULL) return GL_OUT_OF_MEMORY;

	/* empty surface - we can't copy to it */
	if(new_surface->mem_ref == NULL) 
	{
		_mali_surface_deref(new_surface);
		return GL_INVALID_OPERATION;
	}
	
	/* readpixel the data into the surface */
	_mali_surface_access_lock(new_surface);
	pixels = _mali_surface_map(new_surface, MALI_MEM_PTR_WRITABLE);
	if(pixels == NULL) 
	{
		_mali_surface_access_unlock(new_surface);
		_mali_surface_deref(new_surface);
		return GL_OUT_OF_MEMORY;
	}

	gl_err = _gles_read_pixels_internal(ctx, x,y, 0, 0, width, height, &  new_surface->format, pixels);

	_mali_surface_unmap(new_surface);
	_mali_surface_access_unlock(new_surface);
	if(gl_err != GL_NO_ERROR) 
	{
		_mali_surface_deref(new_surface);
		return gl_err;
	}

	/* assign mali_surface to texture */
	mali_err = _gles_texture_miplevel_assign( ctx, tex_obj, mipchain, level, readpixel_format, readpixel_type, 1, &new_surface);
	if(mali_err != MALI_ERR_NO_ERROR) 
	{
		_mali_surface_deref(new_surface);
		return GL_OUT_OF_MEMORY;
	}

	return GL_NO_ERROR;
}

GLenum _gles_copy_texture_sub_image_2d(
		gles_texture_object *tex_obj,
		struct gles_context *ctx,
		GLenum target,
		GLint level,
		GLint xoffset,
		GLint yoffset,
		GLint x,
		GLint y,
		GLsizei width,
		GLsizei height )
{
	GLenum gl_err = GL_NO_ERROR;
	struct mali_surface* surface;
	void* pixels;
	int mipchain;
	gles_mipmap_level *miplevel = NULL;

	MALI_DEBUG_ASSERT_POINTER( ctx );

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	if ( GL_TEXTURE_EXTERNAL_OES == target ) return GL_INVALID_ENUM;
#endif

	if ( ( level < 0 ) || ( level >= GLES_MAX_MIPMAP_LEVELS ) ) return GL_INVALID_VALUE;
	if ( ( xoffset<0 ) || ( yoffset<0 ) ) return GL_INVALID_VALUE;
	if ( ( x<0 ) || ( y<0 ) ) return GL_INVALID_VALUE;
	if ( ( width < 0 ) || ( height < 0 ) ) return GL_INVALID_VALUE;
	if ( ( width > GLES_MAX_TEXTURE_SIZE ) || ( height > GLES_MAX_TEXTURE_SIZE ) ) return GL_INVALID_VALUE;

	mipchain = _gles_texture_object_get_mipchain_index(target);
	surface = _gles_fb_texture_object_get_mali_surface(tex_obj->internal, mipchain, level);
	if ( NULL == surface) return GL_INVALID_OPERATION;
	if(surface->mem_ref == NULL) return GL_INVALID_OPERATION;


	miplevel = _gles_texture_object_get_mipmap_level( tex_obj, target, level );
	MALI_DEBUG_ASSERT_POINTER(miplevel);

	if ( ( xoffset + width )  > miplevel->width )  return GL_INVALID_VALUE;
	if ( ( yoffset + height ) > miplevel->height ) return GL_INVALID_VALUE;

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	/* Return a GL_INVALID_FRAMEBUFFER_OPERATION if the current framebuffer object is incomplete */
	if ( GL_FRAMEBUFFER_COMPLETE != _gles_framebuffer_internal_complete( ctx->state.common.framebuffer.current_object ) )
	{
		return GL_INVALID_FRAMEBUFFER_OPERATION;
	}
#endif
	
	_mali_frame_builder_acquire_output( ctx->state.common.framebuffer.current_object->draw_frame_builder );

	/* 16bit textures are not legal reading targets. Early out with an illegal operation if the framebuffer is a 16bit format
	 * This check covers both RGBA_16_16_16_16, and LA_16_16 formats */
	if ( _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_RED_BITS ) == 16 ) return GL_INVALID_OPERATION;

	/* GLES rules state: 
	 * can not create COLOR textures from FBOs with no color channel
	 * can not create ALPHA empowered textures from FBOs with no alpha channel
	 * These rules correspond to table 3.9 in the GLES spec. We don't actually store 
	 * the GLenum of the color buffer since not all buffers are created through GL.
	 * This is the closest we will get to adhering this rule. */
	switch ( miplevel->format )
	{
		case GL_RGBA: 
			/* GL_RGBA internalformat require a framebuffer with both a color and an alpha channel */
			if ( _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_ALPHA_BITS ) == 0 ) return GL_INVALID_OPERATION;
			if ( _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_RED_BITS ) == 0 ) return GL_INVALID_OPERATION;
			break;
		case GL_LUMINANCE_ALPHA: 
			/* GL_RGBA internalformat require a framebuffer with both a color and an alpha channel */
			if ( _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_RED_BITS ) == 0 ) return GL_INVALID_OPERATION;
			if ( _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_ALPHA_BITS ) == 0 ) return GL_INVALID_OPERATION;
			break;
		case GL_ALPHA:
			/* GL_ALPHA internalformat require a framebuffer with an alpha channel */
			if ( _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_ALPHA_BITS ) == 0 ) return GL_INVALID_OPERATION;
			break;
		case GL_LUMINANCE:
			/* GL_LUMINANCE require a framebuffer with a color channel */
			if ( _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_RED_BITS ) == 0 ) return GL_INVALID_OPERATION;
			break;
		case GL_RGB:
			/* GL_RGB require a framebuffer with a color channel */
			if ( _gles_fbo_get_bits( ctx->state.common.framebuffer.current_object, GL_RED_BITS ) == 0 ) return GL_INVALID_OPERATION;
			break;
	}


	/* 16bit textures are also not legal writing targets. Early out with an illegal operation if this happens */
	    switch ( surface->format.texel_format )
	    {
	        case  M200_TEXEL_FORMAT_AL_16_16:
	        case M200_TEXEL_FORMAT_ARGB_FP16:
	        case M200_TEXEL_FORMAT_ARGB_16_16_16_16:
	            return GL_INVALID_OPERATION;
	        default:
	            break;
	    }


	/* fetch the existing surface. This eliminates any outstanding operations on the surface and 
	 * makes it ready for writing */
	surface = _gles_texture_miplevel_lock(ctx, tex_obj, mipchain, level );
	if(surface == NULL) return GL_OUT_OF_MEMORY;


	/* readpixel the data into the surface */
	_mali_surface_access_lock(surface);
	pixels = _mali_surface_map(surface, MALI_MEM_PTR_WRITABLE);
	if(pixels == NULL) 
	{
		_mali_surface_access_unlock(surface);
		_gles_texture_miplevel_unlock(ctx, tex_obj, mipchain, level);
		return GL_OUT_OF_MEMORY;
	}

	gl_err = _gles_read_pixels_internal(ctx, x, y, xoffset, yoffset, width, height, & surface->format, pixels);
	
	_mali_surface_unmap(surface);
	_mali_surface_access_unlock(surface);
	_gles_texture_miplevel_unlock(ctx, tex_obj, mipchain, level);
	if(gl_err != GL_NO_ERROR) 
	{
		return gl_err;
	}

	return GL_NO_ERROR;
}

#if MALI_USE_GLES_2 || EXTENSION_TEXTURE_CUBE_MAP_ENABLE
mali_bool _gles_texture_object_is_cube_complete( gles_texture_object *tex_obj )
{
	int i, w, h;
	int level_base;
	GLenum format, type;

	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	MALI_DEBUG_ASSERT( GLES_TEXTURE_TARGET_CUBE == tex_obj->dimensionality, ( "invalid dimensionality %X - expected %X\n", tex_obj->dimensionality, GL_TEXTURE_CUBE_MAP ) );

	level_base = 0;

	/* check that first level is defined */
	if ( NULL == tex_obj->mipchains[0] )            return MALI_FALSE;
	if ( NULL == tex_obj->mipchains[0]->levels[level_base] ) return MALI_FALSE;

	w = tex_obj->mipchains[0]->levels[0]->width;
	h = tex_obj->mipchains[0]->levels[0]->height;
	format = tex_obj->mipchains[0]->levels[0]->format;
	type = tex_obj->mipchains[0]->levels[0]->type;

	if ( w != h ) return MALI_FALSE; /* base level must be square */
	if ( w  < 0 ) return MALI_FALSE; /* base level must be positive */

	for ( i = 1; i < GLES_MAX_MIPCHAINS; ++i )
	{
		gles_mipchain *mipchain = tex_obj->mipchains[i];
		if ( NULL == mipchain )                     return MALI_FALSE;
		if ( NULL == mipchain->levels[level_base] ) return MALI_FALSE;
		if ( NULL == _gles_fb_texture_object_get_mali_surface(tex_obj->internal, i, 0)) return MALI_FALSE;
		if ( w != mipchain->levels[level_base]->width )  return MALI_FALSE;
		if ( h != mipchain->levels[level_base]->height ) return MALI_FALSE;
		if ( format != mipchain->levels[level_base]->format )  return MALI_FALSE;
		if ( type != mipchain->levels[level_base]->type )  return MALI_FALSE;
	}

	return MALI_TRUE;
}
#endif

void _gles_texture_object_check_completeness( gles_texture_object *tex_obj )
{
	int num_chains_to_test = 1;

	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_DEBUG_ASSERT( MALI_TRUE == tex_obj->completeness_check_dirty, ("Texture object completeness check is not needed") );

	/* We will now run the function, next time we will use the result if the dirty-flag hasn't changed
	 *  which is done if min_filter changes or we upload a new miplevel to the texture */
	tex_obj->completeness_check_dirty = MALI_FALSE;

	/* if the current min filter does not use mipmaps we only require the base level */
	if ( ( tex_obj->min_filter == GL_NEAREST ) || ( tex_obj->min_filter == GL_LINEAR ) )
	{
		tex_obj->complete_levels = 1;
		if ( GLES_TEXTURE_TARGET_2D == tex_obj->dimensionality )
		{
			if ( NULL == tex_obj->mipchains[ 0 ] || NULL == tex_obj->mipchains[ 0 ]->levels[ 0 ] ||
					NULL == _gles_fb_texture_object_get_mali_surface(tex_obj->internal, 0, 0))
			{
				/* We have verified that the texture is now incomplete */
				tex_obj->is_complete = MALI_FALSE;

				return;
			}

			/* We have verified that the texture is now complete */
			tex_obj->is_complete = MALI_TRUE;

			return;
		}

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
		if ( GLES_TEXTURE_TARGET_EXTERNAL == tex_obj->dimensionality )
		{
			u8 used_planes = 0;
			u8 p_i = 0;
			MALI_DEBUG_ASSERT_POINTER(tex_obj->internal);
			
			if ( NULL == tex_obj->mipchains[ 0 ] 
					|| NULL == tex_obj->mipchains[ 0 ]->levels[ 0 ]  ) 
			{
				/* We have verified that the texture is now incomplete */
				tex_obj->is_complete = MALI_FALSE;
				return;
			}
			used_planes = tex_obj->internal->used_planes;
			MALI_DEBUG_ASSERT_RANGE(used_planes, 1, 3);


			for(p_i = 0; p_i<used_planes; p_i++)
			{
				if( NULL == _gles_fb_texture_object_get_mali_surface_at_plane(tex_obj->internal, 0, 0, p_i) )
				{
					/* We have verified that the texture is now incomplete */
					tex_obj->is_complete = MALI_FALSE;
					return;
				}
			}

			if( NULL == tex_obj->mipchains[0]->levels[0] )
			{
				tex_obj->is_complete = MALI_FALSE;
				return;
			}

			/* external images only support clamp to edge */
			if ( (tex_obj->wrap_s != GL_CLAMP_TO_EDGE || tex_obj->wrap_t != GL_CLAMP_TO_EDGE)) {
				tex_obj->is_complete = MALI_FALSE;
				return;
			}

			tex_obj->is_complete = MALI_TRUE;
			return;
		}
#endif

#if MALI_USE_GLES_2 || EXTENSION_TEXTURE_CUBE_MAP_ENABLE
		if ( GLES_TEXTURE_TARGET_CUBE == tex_obj->dimensionality )
		{
			/* We have verified that the texture is now either complete or incomplete */
			tex_obj->is_complete = _gles_texture_object_is_cube_complete(tex_obj);

			return;
		}
#endif
		MALI_DEBUG_ASSERT( MALI_FALSE, ( "invalid texture dimensionality 0x%x", tex_obj->dimensionality ) );
	}

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	/* external images do not support mipmapping */
	if ( GLES_TEXTURE_TARGET_EXTERNAL == tex_obj->dimensionality )
	{
		tex_obj->is_complete = MALI_FALSE;
		return;
	}
#endif

	/* if the texture is in a paletted format, we know that all mipmap levels were uploaded simultaniously -
	 * so either we have mipmaps or we have not.
	 */
	if ( MALI_TRUE == tex_obj->paletted )
	{
		/* We have verified that the texture is now either complete or incomplete */
		tex_obj->is_complete = tex_obj->paletted_mipmaps;

		return;
	}

	/* min filter is set to use mipmaps, so we have to do a full mipmap-completeness check */
	switch ( tex_obj->dimensionality )
	{
#if MALI_USE_GLES_2 || EXTENSION_TEXTURE_CUBE_MAP_ENABLE
		case GLES_TEXTURE_TARGET_CUBE:
			num_chains_to_test = GLES_MAX_MIPCHAINS; /* we have 6 mipchains to test with cubemaps instead of 1 for 2d-textures*/
			if ( MALI_FALSE == _gles_texture_object_is_cube_complete(tex_obj) )
			{
				/* We have verified that the texture is now incomplete */
				tex_obj->is_complete = MALI_FALSE;

				return;
			}
			/* fallthrough to next case; TARGET_2D */
#endif
		case GLES_TEXTURE_TARGET_2D:
			{
				int i;
				for (i = 0; i < num_chains_to_test; ++i)
				{
					/* check if there's a base level */
					if (NULL == tex_obj->mipchains[i] || NULL == tex_obj->mipchains[i]->levels[0] ||
							NULL == _gles_fb_texture_object_get_mali_surface(tex_obj->internal, i, 0))
					{
						/* We have verified that the texture is now incomplete */
						tex_obj->is_complete = MALI_FALSE;

						return;
					}

					if ( MALI_FALSE == _gles_mipchain_is_complete(tex_obj, i, MALI_TRUE) )
					{
						/* We have verified that the texture is now incomplete */
						tex_obj->is_complete = MALI_FALSE;

						return;
					}
				}
			}
			break;

		default:
			MALI_DEBUG_ASSERT( MALI_FALSE, ( "invalid texture dimensionality 0x%x", tex_obj->dimensionality ) );
			/* We have verified that the texture is now incomplete */
			tex_obj->is_complete = MALI_FALSE;

			return;
	}

	/* We have verified that the texture is now complete */
	tex_obj->is_complete = MALI_TRUE;

}

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
MALI_STATIC enum gles_colorrange _gles_convert_egl_colorrange( egl_image *img )
{
	MALI_DEBUG_ASSERT_POINTER( img );

	if ( NULL != img->prop && img->image_mali && img->image_mali->yuv_info )
	{
		if ( EGL_REDUCED_RANGE_KHR == img->prop->colorrange ) return GLES_COLORRANGE_REDUCED;
		if ( EGL_FULL_RANGE_KHR == img->prop->colorrange ) return GLES_COLORRANGE_FULL;

		MALI_DEBUG_ASSERT( MALI_FALSE, ( "invalid colorrange %d", img->prop->colorrange ) );
		return GLES_COLORRANGE_INVALID;
	}

	return GLES_COLORRANGE_FULL;
}

MALI_STATIC enum gles_colorspace _gles_convert_egl_colorspace( egl_image *img )
{
	MALI_DEBUG_ASSERT_POINTER( img );

	if ( NULL != img->prop && img->image_mali && img->image_mali->yuv_info )
	{
		if ( EGL_COLORSPACE_BT_709 == img->prop->colorspace ) return GLES_COLORSPACE_BT_709;
		if ( EGL_COLORSPACE_BT_601 == img->prop->colorspace ) return GLES_COLORSPACE_BT_601;

		MALI_DEBUG_ASSERT( MALI_FALSE, ( "invalid colorspace %d", img->prop->colorspace ) );
		return GLES_COLORSPACE_INVALID;
	}

	return GLES_COLORSPACE_BT_709;
}

MALI_STATIC u32 _gles_get_image_units_count( egl_image *img )
{
	MALI_DEBUG_ASSERT_POINTER( img );
	MALI_DEBUG_ASSERT_POINTER( img->image_mali );

	if(NULL!=img->image_mali->yuv_info)
	{
		return img->image_mali->yuv_info->planes_count;
	}

	return 1;
}
#endif

GLenum _gles_bind_tex_image(
		gles_texture_object *tex_obj,
		struct gles_context *ctx,
		GLenum target,
		GLenum internalformat,
		mali_bool egl_mipmap_texture,
		mali_surface *surface )
{

	/* Always works with level 0, no matter what the EGLSurface says */
	u32 level_to_update                          = 0;
	mali_err_code mali_err;
	GLenum err                                   = GL_NO_ERROR;
	GLenum type = 0;
	GLenum format = 0;

	MALI_IGNORE(internalformat);

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_CHECK_NON_NULL( surface, GL_INVALID_OPERATION );

	MALI_CHECK( MALI_TRUE==_gles_fb_egl_image_texel_format_valid( surface->format.texel_format ), GL_INVALID_OPERATION );

	_gles_m200_get_gles_type_and_format( surface->format.texel_format, &type, &format );

	/* replace all miplevels with empty state, unbind all mali_surfaces */
	mali_err = _gles_texture_reset(ctx, tex_obj);
	if(mali_err != MALI_ERR_NO_ERROR) return GL_OUT_OF_MEMORY;

	/* assign mali_surface to texture, set to renderable */
	surface->flags |= MALI_SURFACE_FLAG_EGL_IMAGE_SIBLING; /* flag as EGL image sibling to avoid EGL images being created from it */ 
	mali_err = _gles_texture_miplevel_assign( ctx, tex_obj, _gles_texture_object_get_mipchain_index(target), level_to_update, format, type, 1, &surface);
	if(mali_err != MALI_ERR_NO_ERROR) return GL_OUT_OF_MEMORY;
	_mali_surface_addref(surface); /* surface successfully attached, addref it */

	mali_err = _gles_texture_miplevel_set_renderable(ctx, tex_obj, target, level_to_update );
	MALI_DEBUG_ASSERT(mali_err == MALI_ERR_NO_ERROR, ("Miplevel just added to a reset texture, impossible to have usage count at this point")); 

	tex_obj->paletted = MALI_FALSE;

	/* the following conditions has to be satisfied if mipmap levels are to be automatically generated
	 * 1. The EGL_MIPMAP_TEXTURE must be EGL_TRUE
	 * 2. The OpenGL ES texture parameter GL_GENERATE_MIPMAP is GL_TRUE
	 * 3. The value of the EGL_MIPMAP_LEVEL must match the texture parameter GL_TEXTURE_BASE_LEVEL (this is always TRUE for us)
	 */
	if ( MALI_TRUE == egl_mipmap_texture && tex_obj->generate_mipmaps )
	{
		err = _gles_generate_mipmap_chain(tex_obj, ctx, target, 0);
		if ( err != GL_NO_ERROR ) return err;
	}

	return err;
}

#if EXTENSION_EGL_IMAGE_OES_ENABLE
GLenum _gles_egl_image_target_texture_2d(
		gles_texture_object *tex_obj,
		struct gles_context *ctx,
		GLenum target,
		GLeglImageOES image )
{
	egl_image *img                               = NULL;
	mali_image *image_mali                       = NULL;
	mali_surface *surface                        = NULL;
	u32 level_to_update                          = 0;   /* Only level 0 can be a EGLImage target */
	GLenum type = 0;
	GLenum format = 0;
	mali_err_code mali_err;
	egl_api_shared_function_ptrs *egl_funcptrs;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(ctx->share_lists);
	MALI_DEBUG_ASSERT_POINTER(tex_obj);

	egl_funcptrs = (egl_api_shared_function_ptrs*)ctx->egl_funcptrs;

	MALI_DEBUG_ASSERT_POINTER(egl_funcptrs);
	MALI_DEBUG_ASSERT_POINTER(egl_funcptrs->get_eglimage_ptr);

#if defined(__SYMBIAN32__)
#if defined(MALI_USE_GLES_1)
	img = egl_funcptrs->get_eglimage_ptr( (EGLImageKHR)image, EGL_OPENGL_ES_BIT );
#else
	img = egl_funcptrs->get_eglimage_ptr( (EGLImageKHR)image, EGL_OPENGL_ES2_BIT );
#endif
#else
	img = egl_funcptrs->get_eglimage_ptr( (EGLImageKHR)image, 0 );
#endif  /* __SYMBIAN32__ */
	MALI_CHECK_NON_NULL( img, GL_INVALID_VALUE );

	image_mali = img->image_mali;
	MALI_CHECK_NON_NULL( image_mali, GL_INVALID_OPERATION );

	surface = image_mali->pixel_buffer[0][0];
	MALI_CHECK_NON_NULL( surface, GL_INVALID_OPERATION );

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	/* external images YUV textures are not compatible with GL_TEXTURE_2D */
	if ( GL_TEXTURE_2D == target && NULL != image_mali->yuv_info )
	{
		MALI_DEBUG_PRINT(0, ("GLES: Warning: GL_TEXTURE_2D used with a external image. Use GL_TEXTURE_EXTERNAL_OES\n") );
		return GL_INVALID_OPERATION;
	}
#endif

	MALI_CHECK( MALI_TRUE==_gles_fb_egl_image_texel_format_valid( surface->format.texel_format ), GL_INVALID_OPERATION );
	_gles_m200_get_gles_type_and_format( surface->format.texel_format, &type, &format );

	/* replace all miplevels with empty state, unbind all mali_surfaces */
	mali_err = _gles_texture_reset(ctx, tex_obj);
	if(mali_err != MALI_ERR_NO_ERROR) return GL_OUT_OF_MEMORY;

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	/* external images have different default settings */
	if ( GL_TEXTURE_EXTERNAL_OES == target )
	{
		tex_obj->wrap_s = GL_CLAMP_TO_EDGE;
		tex_obj->wrap_t = GL_CLAMP_TO_EDGE;
		tex_obj->min_filter = GL_LINEAR;
		tex_obj->mag_filter = GL_LINEAR;
		tex_obj->yuv_info.colorrange = _gles_convert_egl_colorrange( img );
		tex_obj->yuv_info.colorspace = _gles_convert_egl_colorspace( img );
		tex_obj->yuv_info.image_units_count = _gles_get_image_units_count( img );
		tex_obj->yuv_info.is_rgb = MALI_FALSE;
	}
	/* the external image extension, says that all the textures should return a GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES between 1 an 3 inclusive */
	MALI_DEBUG_ASSERT_RANGE(tex_obj->yuv_info.image_units_count, 1, 3);
#endif

	/* assign mali_surface to texture, set to renderable */
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	if ( GL_TEXTURE_EXTERNAL_OES == target )
	{
#if defined(__ARMCC__)
		mali_surface* surfaces[3];
		surfaces[0] = image_mali->pixel_buffer[0][0];
		surfaces[1] = image_mali->pixel_buffer[1][0];
		surfaces[2] = image_mali->pixel_buffer[2][0];
#else
		mali_surface* surfaces[3] = { image_mali->pixel_buffer[0][0], image_mali->pixel_buffer[1][0], image_mali->pixel_buffer[2][0] };
#endif
		/* RGB special case */
		if(NULL!=image_mali->pixel_buffer[0][0] && NULL==image_mali->pixel_buffer[1][0] && image_mali->pixel_buffer[1][0] == image_mali->pixel_buffer[2][0])
		{
			surfaces[1] = surfaces[0];
			surfaces[2] = surfaces[0];
			tex_obj->yuv_info.image_units_count = 3;
			tex_obj->yuv_info.is_rgb = MALI_TRUE;
		}
		MALI_DEBUG_ASSERT_POINTER(surfaces[0]);
		MALI_DEBUG_ASSERT_POINTER(surfaces[1]);
		MALI_DEBUG_ASSERT_POINTER(surfaces[2]);
		surfaces[0]->flags |= MALI_SURFACE_FLAG_EGL_IMAGE_SIBLING; /* flag as EGL image sibling to avoid EGL images being created from it */ 
		surfaces[1]->flags |= MALI_SURFACE_FLAG_EGL_IMAGE_SIBLING; /* flag as EGL image sibling to avoid EGL images being created from it */ 
		surfaces[2]->flags |= MALI_SURFACE_FLAG_EGL_IMAGE_SIBLING; /* flag as EGL image sibling to avoid EGL images being created from it */ 
		mali_err = _gles_texture_miplevel_assign( ctx, tex_obj, _gles_texture_object_get_mipchain_index(target), level_to_update, format, type, 3, surfaces);
		if(mali_err != MALI_ERR_NO_ERROR) return GL_OUT_OF_MEMORY;
		/* surfaces successfully attached, addref them */
		_mali_surface_addref(surfaces[0]);
		_mali_surface_addref(surfaces[1]);
		_mali_surface_addref(surfaces[2]);
	}
	else
#endif
	{
		surface->flags |= MALI_SURFACE_FLAG_EGL_IMAGE_SIBLING; /* flag as EGL image sibling to avoid EGL images being created from it */ 
		mali_err = _gles_texture_miplevel_assign( ctx, tex_obj, _gles_texture_object_get_mipchain_index(target), level_to_update, format, type, 1, &surface);
		if(mali_err != MALI_ERR_NO_ERROR) return GL_OUT_OF_MEMORY;
		_mali_surface_addref(surface); /* surface successfully attached, addref it */
	}

	mali_err = _gles_texture_miplevel_set_renderable(ctx, tex_obj, target, level_to_update );
	MALI_DEBUG_ASSERT(mali_err == MALI_ERR_NO_ERROR, ("Miplevel just added to a reset texture, impossible to have usage count at this point")); 

	tex_obj->paletted = MALI_FALSE;

	return GL_NO_ERROR;
}

MALI_INTER_MODULE_API enum egl_image_from_gles_surface_status _gles_setup_egl_image_from_texture( struct gles_context *ctx, enum egl_image_gles_target egl_target, GLuint name, u32 miplevel, struct egl_image *image )
{
	mali_named_list *texture_object_list;
	struct gles_wrapper *name_wrapper;
	gles_texture_object *tex_obj;
	mali_bool is_complete = MALI_FALSE;
	mali_bool level_exists = MALI_FALSE;
	mali_bool retval = MALI_FALSE;
	GLint mipchain = 0;
	GLenum target = 0;
	mali_err_code mali_err;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(image);

	/* convert from egl_image_gles_target to gles target */
	switch ( egl_target )
	{
		case GLES_TARGET_TEXTURE_2D:
			target = GL_TEXTURE_2D;
			break;

#if MALI_USE_GLES_2 || EXTENSION_TEXTURE_CUBE_MAP_ENABLE
		case GLES_TARGET_TEXTURE_CUBE_MAP_POSITIVE_X:
			target = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
			break;
		case GLES_TARGET_TEXTURE_CUBE_MAP_NEGATIVE_X:
			target = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
			break;
		case GLES_TARGET_TEXTURE_CUBE_MAP_POSITIVE_Y:
			target = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
			break;
		case GLES_TARGET_TEXTURE_CUBE_MAP_NEGATIVE_Y:
			target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
			break;
		case GLES_TARGET_TEXTURE_CUBE_MAP_POSITIVE_Z:
			target = GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
			break;
		case GLES_TARGET_TEXTURE_CUBE_MAP_NEGATIVE_Z:
			target = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
			break;
#endif

		default:
			/* EGL should already have checked that the target value is
			   valid with the given context. See _egl_create_image_KHR */
			MALI_DEBUG_ASSERT( 0, ( "unknown texture target" ) );
			break;
	}

	mipchain = _gles_texture_object_get_mipchain_index( target );

	/* should result in a EGL_BAD_MATCH since the specified level does not exist */
	if ( miplevel >= GLES_MAX_MIPMAP_LEVELS ) return GLES_SURFACE_TO_EGL_IMAGE_STATUS_INVALID_MIPLVL;

	/* should result in a EGL_BAD_PARAMETER since it is disallowed to create EGLimages from the default texture objects */
	if ( 0 == name ) return GLES_SURFACE_TO_EGL_IMAGE_STATUS_OBJECT_RESERVED;

	texture_object_list = ctx->share_lists->texture_object_list;

	MALI_DEBUG_ASSERT_POINTER(texture_object_list);

	name_wrapper = __mali_named_list_get( texture_object_list, name );
	MALI_CHECK_NON_NULL( name_wrapper, GLES_SURFACE_TO_EGL_IMAGE_STATUS_OBJECT_UNAVAILABLE );
	MALI_CHECK_NON_NULL( name_wrapper->ptr.tex, GLES_SURFACE_TO_EGL_IMAGE_STATUS_OBJECT_UNAVAILABLE );

	/* tex_obj is know to be non-NULL now */
	tex_obj = name_wrapper->ptr.tex;

	/* should result in EGL_BAD_ACCESS since EGLImages cannot be made from resources that are already siblings */
	{
		mali_surface* surf = _gles_fb_texture_object_get_mali_surface(tex_obj->internal, mipchain, miplevel);
		if(surf)
		{
			MALI_CHECK( !(surf->flags & MALI_SURFACE_FLAG_EGL_IMAGE_SIBLING) , GLES_SURFACE_TO_EGL_IMAGE_STATUS_ALREADY_SIBLING );
		}
	}

	switch ( target )
	{
		case GL_TEXTURE_2D:
			if ( tex_obj->mipchains[mipchain] != NULL 
			     && tex_obj->mipchains[mipchain]->levels[miplevel] != NULL 
				 && _gles_fb_texture_object_get_mali_surface(tex_obj->internal, mipchain, miplevel) != NULL )
			{
				level_exists = MALI_TRUE;
			}

			if ( GLES_TEXTURE_TARGET_2D == tex_obj->dimensionality )
			{
				is_complete = _gles_texture_object_is_complete( tex_obj );
			}
			break;

#if MALI_USE_GLES_2 || EXTENSION_TEXTURE_CUBE_MAP_ENABLE
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
			mipchain = _gles_texture_object_get_mipchain_index( target );
			if ( tex_obj->mipchains[mipchain] != NULL 
			     && tex_obj->mipchains[mipchain]->levels[miplevel] != NULL 
				 && _gles_fb_texture_object_get_mali_surface(tex_obj->internal, mipchain, miplevel) != NULL )
			{
				level_exists = MALI_TRUE;
			}

			if ( GLES_TEXTURE_TARGET_CUBE == tex_obj->dimensionality )
			{
				is_complete = _gles_texture_object_is_complete( tex_obj );
			}
			break;
#endif
		default:
			MALI_DEBUG_ASSERT( 0, ( "unknown texture target" ) );
	}

	/* the GL error codes are not defined in the spec since this is an internal function. We only
	 * need to give EGL enough information to set the correct EGL error code
	 */
	MALI_CHECK( MALI_TRUE == is_complete, GLES_SURFACE_TO_EGL_IMAGE_STATUS_OBJECT_INCOMPLETE );

	/* NOTE the level requirement here is non-standard */
	MALI_CHECK( MALI_TRUE == level_exists, GLES_SURFACE_TO_EGL_IMAGE_STATUS_INVALID_MIPLVL );
	MALI_CHECK( miplevel < MALI_TD_MIPLEVEL_POINTERS, GLES_SURFACE_TO_EGL_IMAGE_STATUS_INVALID_MIPLVL );

	/* flag miplevel as renderable */
	mali_err = _gles_texture_miplevel_set_renderable(ctx, tex_obj, target, miplevel );
	if(mali_err != MALI_ERR_NO_ERROR)
	{
		MALI_DEBUG_ASSERT(mali_err == MALI_ERR_OUT_OF_MEMORY, ("only legal error code is oom"));
		return GLES_SURFACE_TO_EGL_IMAGE_STATUS_OOM;
	}

	/* call the fragment to fill in egl image data */
	retval = _gles_fb_texture_setup_egl_image( tex_obj->internal, mipchain, miplevel, image);
	MALI_CHECK( MALI_TRUE == retval, GLES_SURFACE_TO_EGL_IMAGE_STATUS_OOM );

	/* and finally, if everything passed, set the mali_surface to "egl sibling" */
	{
		mali_surface* surf = _gles_fb_texture_object_get_mali_surface(tex_obj->internal, mipchain, miplevel);
		MALI_DEBUG_ASSERT_POINTER(surf);
		MALI_IGNORE(surf);
		surf->flags |= MALI_SURFACE_FLAG_EGL_IMAGE_SIBLING;
	}


	return GLES_SURFACE_TO_EGL_IMAGE_STATUS_NO_ERROR;
}
#endif /* EXTENSION_EGL_IMAGE_OES_ENABLE */

mali_bool _gles_texture_is_mipmap_generation_necessary(gles_texture_object* tex_obj, GLenum target)
{
	gles_mipmap_level* mip_level_zero = NULL;
	mali_bool level_zero_is_renderable;
	u32 mipchain;

	MALI_DEBUG_ASSERT_POINTER(tex_obj);

	mipchain = _gles_texture_object_get_mipchain_index( target );
	mip_level_zero = _gles_texture_object_get_mipmap_level(tex_obj, target, 0);
	level_zero_is_renderable = _gles_fb_texture_object_get_renderable(tex_obj->internal, mipchain, 0);

	/* Mipmaps always need to be generated for renderable objects like FBO */
	if ( NULL != mip_level_zero && level_zero_is_renderable)
	{
		return MALI_TRUE;
	}
	else
	{
		return tex_obj->mipgen_dirty;
	}
}

GLenum _gles_bind_texture(
		struct gles_context *ctx,
		GLenum target,
		GLuint name )
{
	gles_texture_object  *new_obj = NULL;
	gles_texture_object  *old_obj = NULL;
	GLuint                old_name;

	enum gles_texture_target dimensionality;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->share_lists );

	dimensionality = _gles_get_dimensionality( target, ctx->api_version );

	/* use dimensionality to determine if the input target is a valid enum  */
	if ( GLES_TEXTURE_TARGET_INVALID == dimensionality ) return GL_INVALID_ENUM;

	/* get the texture object to modify */
	_gles_texture_env_get_binding( &ctx->state.common.texture_env, target, &old_name, &old_obj, ctx->api_version );

	/* if the old binding is the same as the new binding, there's no need to do anything */
	/* We have to do a check for multicontext due to bugzilla entry 9063 */
	if ( old_name == name && old_obj->is_deleted == MALI_FALSE && _mali_sys_atomic_get(&ctx->share_lists->ref_count) < 2) return GL_NO_ERROR;

	/* find the texture object to bind */
	new_obj = _gles_get_texobj( ctx, name, dimensionality );
	MALI_CHECK_NON_NULL( new_obj, GL_OUT_OF_MEMORY );

	/* typecheck */
	MALI_CHECK( new_obj->dimensionality == dimensionality, GL_INVALID_OPERATION );

	/* set a new reference to the new texture object */
	_gles_texture_env_set_binding( &ctx->state.common.texture_env, target, name, new_obj, ctx->api_version );

	/* update reference counts */
	_gles_texture_object_addref( new_obj );
	_gles_texture_object_deref( old_obj );

	return GL_NO_ERROR;
}

enum gles_texture_target _gles_get_dimensionality_cubemap_detail(GLenum target)
{
#if EXTENSION_TEXTURE_CUBE_MAP_ENABLE || MALI_USE_GLES_2
        switch(target)
        {
        case GL_TEXTURE_CUBE_MAP:
                return GLES_TEXTURE_TARGET_INVALID; /* un-handle this enum */

        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
                return GLES_TEXTURE_TARGET_CUBE;
        }
#endif
        return _gles_get_dimensionality(target, GLES_API_VERSION_1);
}

GLenum _gles_get_active_bound_texture_object( GLenum target, struct gles_texture_environment *texture_env, struct gles_texture_object **output_texture_obj )
{
        enum gles_texture_target dimensionality;

        MALI_DEBUG_ASSERT_POINTER( texture_env );

        dimensionality = _gles_get_dimensionality_cubemap_detail(target);
        if (dimensionality == GLES_TEXTURE_TARGET_INVALID)
        {
                return GL_INVALID_ENUM;
        }

        *output_texture_obj = _gles_get_texobj_from_unit_id(texture_env, dimensionality, texture_env->active_texture);
        /* _gles_get_texobj_from_unit_id should not be able to return NULL */
        MALI_DEBUG_ASSERT_POINTER( output_texture_obj );
        return GL_NO_ERROR;
}
