/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_mipmap.h"
#include "gles_config_extension.h"

#include "gles_util.h"
#include "gles_texture_object.h"
#include "gles_state.h"
#include <shared/mali_named_list.h>

#include "shared/mali_convert.h"
#include "gles_convert.h"
#include "gles_mipmap_filter.h"

#include "m200_backend/gles_m200_td.h"
#include "m200_backend/gles_fb_texture_object.h"
#include "m200_backend/gles_m200_mipmap.h"

#if MALI_TIMELINE_PROFILING_ENABLED
#include <base/mali_profiling.h>
#endif

#include "base/mali_byteorder.h"

GLenum _gles_generate_mipmap( struct gles_context *ctx,
                               gles_texture_environment* tex_env,
                               mali_named_list* texture_object_list,
                               GLenum target )
{
	GLenum err_code = GL_NO_ERROR;
	struct gles_texture_object* tex_obj = NULL;
	int api_version;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(tex_env);
	MALI_DEBUG_ASSERT_POINTER(texture_object_list);

	api_version = (ctx->api_version == GLES_API_VERSION_1) ? 1 : 2;

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	/* external images don't allow mipmap generation */
	if ( GL_TEXTURE_EXTERNAL_OES == target )
	{
		return GL_INVALID_ENUM;
	}
#endif

	/* target must be legal */
	if (target != GL_TEXTURE_2D &&
		(target != GL_TEXTURE_CUBE_MAP || ctx->api_version != GLES_API_VERSION_2))
			return GL_INVALID_ENUM;

	/* get the texture object - abort if bound to texture 0 */
	_gles_texture_env_get_binding(tex_env, target, NULL, &tex_obj, api_version);
	MALI_DEBUG_ASSERT_POINTER(tex_obj);

	/* handle texture 2D */
	if ( GL_TEXTURE_2D == target )
	{
		if( MALI_TRUE == _gles_texture_is_mipmap_generation_necessary(tex_obj, target) )
		{
			err_code = _gles_generate_mipmap_chain(tex_obj, ctx, GL_TEXTURE_2D, 0);
		}
	}
	else if( target == GL_TEXTURE_CUBE_MAP ) /* handle texture cubemap */
	{
		int i;
		const GLenum cubefaces[6] =
		{
			GL_TEXTURE_CUBE_MAP_POSITIVE_X,
			GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
			GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
			GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
			GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
			GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
		};

#if EXTENSION_TEXTURE_CUBE_MAP_ENABLE
		/* check that we have a cube complete texture object */
		MALI_CHECK( _gles_texture_object_is_cube_complete( tex_obj ), GL_INVALID_OPERATION );
#endif
		/* get the pixel data from the internal structure */
		for(i = 0; i < 6; i++)
		{
			if( MALI_TRUE == _gles_texture_is_mipmap_generation_necessary(tex_obj, cubefaces[i]) )
			{
				err_code = _gles_generate_mipmap_chain(tex_obj, ctx, cubefaces[i], 0);
			}
			if (GL_NO_ERROR != err_code)
			{
					break;
			}
		}
	}

	/* if there were no errors generating bitmaps, we can unset the mipgen_dirty flag */
	tex_obj->mipgen_dirty = ( GL_NO_ERROR != err_code ) ? MALI_TRUE : MALI_FALSE;

	return err_code;
}


struct _block_iterator
{
	u32 y_uorder;
	u32 x_uorder;
} _block_iterator;

MALI_STATIC_INLINE void _block_iterator_step_x(struct _block_iterator* it)
{
	const u32 uorder_plus = 0xAAAAAAAB;
	const u32 uorder_mask = 0x55555555;
	it->x_uorder += uorder_plus;
	it->x_uorder &= uorder_mask;
}

MALI_STATIC_INLINE void _block_iterator_step_y(struct _block_iterator* it)
{
	const u32 uorder_plus = 0xAAAAAAAB;
	const u32 uorder_mask = 0x55555555;
	it->y_uorder += uorder_plus;
	it->y_uorder &= uorder_mask;
}

MALI_STATIC_INLINE int _block_iterator_get_index(struct _block_iterator* it)
{
	return (it->y_uorder << 1) + (it->y_uorder ^ it->x_uorder);
}

MALI_STATIC  GLenum  realloc_miplevel(struct gles_context *ctx,  
                                          struct gles_texture_object *tex_obj,
                                          const int mipchain,
                                          GLenum format,
                                          GLenum type,
                                          u16 miplvl)
{
   struct mali_surface* dst_surface;
   void* src_buf, *dst_buf;
   mali_err_code mali_err = MALI_ERR_NO_ERROR;
   mali_surface* src_surface;
   int width, height;

   src_surface = _gles_fb_texture_object_get_mali_surface(tex_obj->internal, mipchain , miplvl);
   MALI_DEBUG_ASSERT_POINTER(src_surface); 
   width = src_surface->format.width;
   height = src_surface->format.height;

     /* map src_surface so that we can read from it */
    _mali_surface_access_lock( src_surface );
     src_buf = _mali_surface_map( src_surface, MALI_MEM_PTR_READABLE );
     if( src_buf == NULL) 
     {
         _mali_surface_access_unlock(src_surface);
         return GL_OUT_OF_MEMORY;
     }

    /* allocate new mali surface */
    dst_surface = _gles_texture_miplevel_allocate(ctx, tex_obj, mipchain, miplvl, width , height, format, type);
    if( NULL == dst_surface)
    {
        _mali_surface_unmap(src_surface);
        _mali_surface_access_unlock(src_surface);
        return GL_OUT_OF_MEMORY;
    }
   
    MALI_DEBUG_ASSERT( dst_surface != src_surface, (" dst surface is same as src in realloc_miplevel  \n") );

    _mali_surface_access_lock( dst_surface );
    dst_buf = _mali_surface_map( dst_surface, MALI_MEM_PTR_WRITABLE );
    if (NULL == dst_buf)
    {
        _mali_surface_access_unlock( dst_surface );
        _mali_surface_deref( dst_surface );
        _mali_surface_unmap(src_surface);
        _mali_surface_access_unlock(src_surface);
        return GL_OUT_OF_MEMORY; 
    }

    _mali_mem_copy( dst_surface->mem_ref->mali_memory, 
                                   dst_surface->mem_offset, 
                                   src_surface->mem_ref->mali_memory, 
                                   src_surface->mem_offset,  
                                   dst_surface->datasize);
     
     
    _mali_surface_unmap(dst_surface);
    _mali_surface_access_unlock(dst_surface);

    _mali_surface_unmap(src_surface);
    _mali_surface_access_unlock(src_surface);
        

     /* assign mali_surface to texture */
     mali_err = _gles_texture_miplevel_assign( ctx, tex_obj, mipchain, miplvl, format, type, 1, &dst_surface);
     if(mali_err != MALI_ERR_NO_ERROR) 
     {
           _mali_surface_deref(dst_surface);
           return GL_OUT_OF_MEMORY;
     }

 
     return GL_NO_ERROR;
}


typedef void (*downsample_func)(const void* src, void* dst, int num_components, u32 sample_mask, u32 sample_shift);

/* function for downsampling 16-bit texture formats */
void _downsample_2x2_rgba16161616(const void* src, void* dst, int num_components, u32 sample_mask, u32 sample_shift)
{
	int avg[4] = { 0, 0, 0, 0 };
	int i, b;

	const u16* src_u16 = (const u16*)src;
	u16* dst_u16 = (u16*)dst;

	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_DEBUG_ASSERT_POINTER(dst);

	/* compute average of 4 neighboring pixel */
	for (i = 0; i < 4; ++i)
	{
		if ((sample_mask & (1 << i)) == 0) continue;
		for (b = 0; b < num_components; ++b)
		{
			u16* srcx = (u16*)src_u16 + (i * num_components) + b;
			avg[b] += _MALI_GET_U16_ENDIAN_SAFE(srcx);
		}
	}

	/* store average in destination */
	for (b = 0; b < num_components; ++b)
	{
		u16* dstx = dst_u16+b;
		_MALI_SET_U16_ENDIAN_SAFE(dstx, avg[b] >> sample_shift);
	}
}

/* function for downsampling fp16 texture formats */
void _downsample_2x2_rgba_fp16(const void* src, void* dst, int num_components, u32 sample_mask, u32 sample_shift)
{
	float avg[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	int i, b;

	const gles_fp16* src_fp16 = (const gles_fp16*)src;
	gles_fp16* dst_u16 = (gles_fp16*)dst;

	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_DEBUG_ASSERT_POINTER(dst);

	/* compute average of 4 neighboring pixel */
	for (i = 0; i < 4; ++i)
	{
		if ((sample_mask & (1 << i)) == 0) continue;
		for (b = 0; b < num_components; ++b)
		{
			avg[b] += _gles_fp16_to_fp32( src_fp16[(i * num_components) + b] );
		}
	}

	/* store average in destination */
	for (b = 0; b < num_components; ++b)
	{
		dst_u16[b] = _gles_fp32_to_fp16(avg[b]/ ((float)(0x1 << sample_shift)));
	}
}

/* functions for downsampling 8-bit texture formats */
#if (MALI_DOWNSAMPLE_VIANEON_DEFINED)
 void _downsample_2x2_rgba8888(const void* src, void* dst, int num_components, u32 sample_mask, u32 sample_shift)
{

       MALI_DEBUG_ASSERT_POINTER(src);
       MALI_DEBUG_ASSERT_POINTER(dst);
       
       _mali_osu_downsample8888(src, (u32*)dst,sample_mask);       
}

void _downsample_2x2_rgba888(const void* src, void* dst, int num_components, u32 sample_mask, u32 sample_shift)
{

       MALI_DEBUG_ASSERT_POINTER(src);
       MALI_DEBUG_ASSERT_POINTER(dst);

       _mali_osu_downsample888(src, dst, sample_mask);
}
void _downsample_2x2_rgba88(const void* src, void* dst, int num_components, u32 sample_mask, u32 sample_shift)
{

       MALI_DEBUG_ASSERT_POINTER(src);
       MALI_DEBUG_ASSERT_POINTER(dst);

       /* compute average of 4 neighboring pixel */
       _mali_osu_downsample88(src, dst, sample_mask);      
}

void _downsample_2x2_rgba8(const void* src, void* dst, int num_components, u32 sample_mask, u32 sample_shift)
{      
       MALI_DEBUG_ASSERT_POINTER(src);
       MALI_DEBUG_ASSERT_POINTER(dst);
       
      _mali_osu_downsample8(src, dst ,sample_mask);
}
#else
 void _downsample_2x2_rgba8888(const void* src, void* dst, int num_components, u32 sample_mask, u32 sample_shift)
{
 
       u32 avg[4] = { 0, 0, 0, 0 };
       int i;

       const u8* src_u8 = (const u8*)src;
       u32* dst_u32 = (u32*)dst;

       MALI_DEBUG_ASSERT_POINTER(src);
       MALI_DEBUG_ASSERT_POINTER(dst);

       for (i = 0; i < 4; ++i)
       {
            if ((sample_mask & (1 << i)) != 0) 
           {
               u8* srcx = (u8*)src_u8 + ((i * num_components) );

              avg[0] += _MALI_GET_U8_ENDIAN_SAFE(srcx);
              avg[1] += _MALI_GET_U8_ENDIAN_SAFE(srcx + 1);
              avg[2] += _MALI_GET_U8_ENDIAN_SAFE(srcx + 2);
              avg[3] += _MALI_GET_U8_ENDIAN_SAFE(srcx + 3);
           }   
           
      }

       avg[0] >>= sample_shift;
       avg[1] >>= sample_shift;
       avg[2] >>= sample_shift;
       avg[3] >>= sample_shift;

      _MALI_SET_U32_ENDIAN_SAFE(dst_u32, (avg[0] ) | (avg[1] << 8 ) | (avg[2] << 16 ) | (avg[3] << 24 ) );

}

void _downsample_2x2_rgba888(const void* src, void* dst, int num_components, u32 sample_mask, u32 sample_shift)
{
       int avg[3] = { 0, 0, 0  };
       int i;

       const u8* src_u8 = (const u8*)src;
       u8* dst_u8 = (u8*)dst;

       MALI_DEBUG_ASSERT_POINTER(src);
       MALI_DEBUG_ASSERT_POINTER(dst);

       /* compute average of 4 neighboring pixel */
       for (i = 0; i < 4; ++i)
       {
           if ((sample_mask & (1 << i)) != 0)
           {
                u8* srcx = (u8*)src_u8 + ((i * num_components) );

                avg[0] += _MALI_GET_U8_ENDIAN_SAFE(srcx);
                avg[1] += _MALI_GET_U8_ENDIAN_SAFE(srcx + 1);
                avg[2] += _MALI_GET_U8_ENDIAN_SAFE(srcx + 2);
           }
     }

    avg[0] >>= sample_shift;
    avg[1] >>= sample_shift;
    avg[2] >>= sample_shift;
              
    _MALI_SET_U8_ENDIAN_SAFE(dst_u8, avg[0]   );
    _MALI_SET_U8_ENDIAN_SAFE(dst_u8+1, avg[1]   );
    _MALI_SET_U8_ENDIAN_SAFE(dst_u8+2, avg[2]   );


}

void _downsample_2x2_rgba88(const void* src, void* dst, int num_components, u32 sample_mask, u32 sample_shift)
{
       u16 avg[2] = { 0, 0 };
       int i;

       const u8* src_u8 = (const u8*)src;
       u16* dst_u16 = (u16*)dst;

       MALI_DEBUG_ASSERT_POINTER(src);
       MALI_DEBUG_ASSERT_POINTER(dst);

       /* compute average of 4 neighboring pixel */
       for (i = 0; i < 4; ++i)
       {
             if ((sample_mask & (1 << i)) != 0)
             {
                 u8* srcx = (u8*)src_u8 + ((i * num_components) );
                 avg[0] += _MALI_GET_U8_ENDIAN_SAFE(srcx);
                 avg[1] += _MALI_GET_U8_ENDIAN_SAFE(srcx + 1);
             }
     }

     avg[0] >>= sample_shift;
     avg[1] >>= sample_shift;
              
     _MALI_SET_U16_ENDIAN_SAFE(dst_u16, (avg[0] ) | (avg[1] << 8 )  );

}

void _downsample_2x2_rgba8(const void* src, void* dst, int num_components, u32 sample_mask, u32 sample_shift)
{
       int avg =  0;
       int i;

       const u8* src_u8 = (const u8*)src;
       u8* dst_u8 = (u8*)dst;

       MALI_DEBUG_ASSERT_POINTER(src);
       MALI_DEBUG_ASSERT_POINTER(dst);

       /* compute average of 4 neighboring pixel */
       for (i = 0; i < 4; ++i)
       {
             if ((sample_mask & (1 << i)) != 0)
             {
                u8* srcx = (u8*)src_u8 + ((i * num_components) );
                avg += _MALI_GET_U8_ENDIAN_SAFE(srcx);
             }
      }

      avg >>= sample_shift;
      _MALI_SET_U8_ENDIAN_SAFE(dst_u8, avg );

}

#endif

/* function for downsampling packed 565 format textures */
void _downsample_2x2_rgb565(const void* src, void* dst, int num_components, u32 sample_mask, u32 sample_shift)
{
	u32 avg[3] = { 0, 0, 0 };
	int i;

	const u16* src_u16 = (const u16*)src;
	u16* dst_u16 = (u16*)dst;

	MALI_IGNORE(num_components);
	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_DEBUG_ASSERT_POINTER(dst);

	/* compute average of 4 neighboring pixel */
	for (i = 0; i < 4; ++i)
	{
		u16 srcval;

		if ((sample_mask & (1 << i)) == 0) continue;

		srcval = _MALI_GET_U16_ENDIAN_SAFE(src_u16 + i);
		avg[2] += srcval & 0x1f;
		avg[1] += (srcval >> 5) & 0x3f;
		avg[0] += (srcval >> 11) & 0x1f;

	}

	avg[2] >>= sample_shift;
	avg[1] >>= sample_shift;
	avg[0] >>= sample_shift;

	/* store average in destination */
	_MALI_SET_U16_ENDIAN_SAFE(dst_u16, (avg[0] << 11) | (avg[1] << 5) | avg[2]);
}

/* function for downsampling packed 444 format textures */
void _downsample_2x2_rgba4444(const void* src, void* dst, int num_components, u32 sample_mask, u32 sample_shift)
{
	u32 avg[4] = { 0, 0, 0, 0 };
	int i;

	const u16* src_u16 = (const u16*)src;
	u16* dst_u16 = (u16*)dst;

	MALI_IGNORE(num_components);
	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_DEBUG_ASSERT_POINTER(dst);

	/* compute average of 4 neighboring pixel */
	for (i = 0; i < 4; ++i)
	{
		u16 srcval;

		if ((sample_mask & (1 << i)) == 0) continue;

		srcval = _MALI_GET_U16_ENDIAN_SAFE(src_u16 + i);

		avg[3] += srcval & 0xf;
		avg[2] += (srcval >> 4) & 0xf;
		avg[1] += (srcval >> 8) & 0xf;
		avg[0] += (srcval >> 12) & 0xf;	}

	avg[3] >>= sample_shift;
	avg[2] >>= sample_shift;
	avg[1] >>= sample_shift;
	avg[0] >>= sample_shift;

	/* store average in destination */
	_MALI_SET_U16_ENDIAN_SAFE(dst_u16, (avg[0] << 12) | (avg[1] << 8) | (avg[2] << 4) | avg[3]);
}
/* function for downsampling packed 5551 format textures */
void _downsample_2x2_rgba1555(const void* src, void* dst, int num_components, u32 sample_mask, u32 sample_shift)
{
	u32 avg[4] = { 0, 0, 0, 0 };
	int i;

	const u16* src_u16 = (const u16*)src;
	u16* dst_u16 = (u16*)dst;

	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_IGNORE(num_components);
	MALI_DEBUG_ASSERT_POINTER(dst);

	/* compute average of 4 neighboring pixel */
	for (i = 0; i < 4; ++i)
	{
		u16 srcval;

		if ((sample_mask & (1 << i)) == 0) continue;

		srcval = _MALI_GET_U16_ENDIAN_SAFE(src_u16 + i);

		avg[3] += srcval & 0x1f;
		avg[2] += (srcval >> 5) & 0x1f;
		avg[1] += (srcval >> 10) & 0x1f;
		avg[0] += (srcval >> 15) & 0x1;
	}

	avg[3] >>= sample_shift;
	avg[2] >>= sample_shift;
	avg[1] >>= sample_shift;
	avg[0] >>= sample_shift;

	/* store average in destination */
	_MALI_SET_U16_ENDIAN_SAFE(dst_u16, (avg[0] << 15) | (avg[1] << 10) | (avg[2] << 5) | avg[3]);
}


/* function for downsampling packed 5551 format textures */
void _downsample_2x2_rgba5551(const void* src, void* dst, int num_components, u32 sample_mask, u32 sample_shift)
{
	u32 avg[4] = { 0, 0, 0, 0 };
	int i;

	const u16* src_u16 = (const u16*)src;
	u16* dst_u16 = (u16*)dst;

	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_IGNORE(num_components);
	MALI_DEBUG_ASSERT_POINTER(dst);

	/* compute average of 4 neighboring pixel */
	for (i = 0; i < 4; ++i)
	{
		u16 srcval;

		if ((sample_mask & (1 << i)) == 0) continue;

		srcval = _MALI_GET_U16_ENDIAN_SAFE(src_u16 + i);

		avg[3] += srcval & 0x1;
		avg[0] += (srcval >> 1) & 0x1f;
		avg[1] += (srcval >> 6) & 0x1f;
		avg[2] += (srcval >> 11) & 0x1f;
	}

	avg[3] >>= sample_shift;
	avg[2] >>= sample_shift;
	avg[1] >>= sample_shift;
	avg[0] >>= sample_shift;

	/* store average in destination */
	_MALI_SET_U16_ENDIAN_SAFE(dst_u16, (avg[2] << 11) | (avg[1] << 6) | (avg[0] << 1) | avg[3]);
}

GLenum _gles_generate_mipmap_chain_sw_16x16blocked(
	struct gles_texture_object *tex_obj,
	struct gles_context *ctx,
	GLenum target,
	unsigned int base_miplvl)
{
	GLenum format, type;
	mali_err_code merr;
	unsigned int bytes_per_texel;
	const void *src_buf = NULL;
	int lev;
	int src_num_blocks_in_x_axis, odd_fixup, num_components = 0;
	downsample_func downsample = NULL;
	const int mipchain = _gles_texture_object_get_mipchain_index(target);
	gles_mipmap_level *miplevel_base;
	mali_surface* src_surface;
	unsigned int start_level = base_miplvl + 1;
	mali_bool has_neon_impl = MALI_FALSE; 
		
	/* retrieve format and type. These enums must be propagated down to other mipmap levels */
	miplevel_base = _gles_texture_object_get_mipmap_level(tex_obj, target, base_miplvl);
	MALI_DEBUG_ASSERT_POINTER(miplevel_base); /* this miplvl must already exist, as we are basing the generation on it */
	format = miplevel_base->format;
	type = miplevel_base->type;

	/* this surface MUST exist at this point, otherwise we can't generate miplevels. Callers need to ensure this! */
	src_surface = _gles_fb_texture_object_get_mali_surface(tex_obj->internal, _gles_texture_object_get_mipchain_index(target), base_miplvl);
	MALI_DEBUG_ASSERT_POINTER(src_surface); 

	bytes_per_texel = __m200_texel_format_get_size( src_surface->format.texel_format );

	/* Setting downsampling/conversion callback function
	 * Also set number of components to use in the conversion (not used for packed formats) */
	switch (src_surface->format.texel_format)
	{
		case M200_TEXEL_FORMAT_L_8:
			/* fallthrough */
		case M200_TEXEL_FORMAT_A_8:
			downsample = _downsample_2x2_rgba8;
			has_neon_impl = MALI_TRUE;
			num_components = bytes_per_texel;
			break; 
		case M200_TEXEL_FORMAT_AL_88:
			downsample = _downsample_2x2_rgba88;
			has_neon_impl = MALI_TRUE;
			num_components = bytes_per_texel;
			break; 
#if !RGB_IS_XRGB
		case M200_TEXEL_FORMAT_RGB_888:
			downsample = _downsample_2x2_rgba888;
			has_neon_impl = MALI_TRUE;
			num_components =bytes_per_texel;
			break;
#endif
		case M200_TEXEL_FORMAT_xRGB_8888:
			/* fallthrough */
		case M200_TEXEL_FORMAT_ARGB_8888:
			downsample = _downsample_2x2_rgba8888;
			has_neon_impl = MALI_TRUE;
			num_components = bytes_per_texel;
			break;
		case M200_TEXEL_FORMAT_RGB_565:
			downsample = _downsample_2x2_rgb565;
			break;
		case M200_TEXEL_FORMAT_ARGB_1555:
			if(src_surface->format.reverse_order)
			{
				downsample = _downsample_2x2_rgba5551;
			} else {
				downsample = _downsample_2x2_rgba1555;
			}
			break;
		case M200_TEXEL_FORMAT_ARGB_4444:
			downsample = _downsample_2x2_rgba4444;
			break;
		case M200_TEXEL_FORMAT_ARGB_16_16_16_16:
			downsample = _downsample_2x2_rgba16161616;
			num_components = 4;
			break;
#if EXTENSION_TEXTURE_HALF_FLOAT_LINEAR_OES_ENABLE
		case M200_TEXEL_FORMAT_ARGB_FP16:
			downsample = _downsample_2x2_rgba_fp16;
			num_components = 4;
			break;
#endif
		case M200_TEXEL_FORMAT_AL_16_16:
			downsample = _downsample_2x2_rgba16161616;
			num_components = 2;
			break;
		case M200_TEXEL_FORMAT_L_16:
			downsample = _downsample_2x2_rgba16161616;
			num_components = 1;
			break;
		case M200_TEXEL_FORMAT_ETC: 
			/* generating mipmaps for ETC is not supported, fallthrough - should be handled earlier */
		default:
			/* If this assert hits, someone is attempting to generate a mipmap off a texel format
			 * not officially supported by GLES. Either some external texture has snuck by the 
			 * format legality check, or a new format has been added, without adding 
			 * mipmap generation support for it. Either way, this function does not currently handle 
			 * it and support for the missing format must be added - or the format must be unsupported. 
			 */
			MALI_DEBUG_ASSERT(0, ("unsupported texture format detected when generating mipmaps"));
			return GL_INVALID_OPERATION;
	}
	

	/* setup initial source data */
	src_num_blocks_in_x_axis = (src_surface->format.width + 15)/ 16; /* align up to the nearest block, each block is 16 pixels wide */

	/* map src_surface so that we can read from it */
	_mali_surface_access_lock( src_surface );
	src_buf = _mali_surface_map( src_surface, MALI_MEM_PTR_READABLE );
	if( src_buf == NULL) 
	{
		_mali_surface_access_unlock(src_surface);
		return GL_OUT_OF_MEMORY;
	}


	/* generate all mipmap levels, based on src_surface */
	for ( lev = start_level ; ; lev++ )
	{
		struct mali_surface* dst_surface;
		void* dst_buf;
		int x, y;
		int dst_block_index = 0;
		int src_block_index = 0;	
		int dst_width = MAX(src_surface->format.width >> 1, 1);
		int dst_height = MAX(src_surface->format.height>> 1, 1);
		int dst_num_blocks_in_x_axis = (dst_width+15) / 16;
		int src_last_block_width = src_surface->format.width % 16;

		/* if there's an odd number of blocks in x-direction, apply and odd-fixup number to the block index */
		odd_fixup = (src_num_blocks_in_x_axis & 1);

		/* allocate new mali surface */
		dst_surface = _gles_texture_miplevel_allocate(ctx, tex_obj, _gles_texture_object_get_mipchain_index(target), lev, dst_width, dst_height, format, type);
		if( NULL == dst_surface)
		{
			_mali_surface_unmap( src_surface );
			_mali_surface_access_unlock( src_surface );
			return GL_OUT_OF_MEMORY;
		}

		/* map new surface, in order to be able to write to it. Mapping this surface is safe - no lock order starvation may 
		 * occur as no others may access this surface just yet. */
		_mali_surface_access_lock( dst_surface );
		dst_buf = _mali_surface_map( dst_surface, MALI_MEM_PTR_WRITABLE );
		if (NULL == dst_buf)
		{
			_mali_surface_access_unlock( dst_surface );
			_mali_surface_deref( dst_surface );
			_mali_surface_unmap( src_surface );
			_mali_surface_access_unlock( src_surface );
			return GL_OUT_OF_MEMORY; 
		}

		/* generate content */
		for (y = 0; y < dst_height; y += 16)
		{
			const int block_height_odd = MIN(src_surface->format.height - y, 16) & 1;
			for (x = 0; x < dst_width; x += 16)
			{
				const int block_width_odd = MIN(src_surface->format.width - x, 16) & 1;
				const int block_width = MIN(dst_width - x, 16);
				const int block_height = MIN(dst_height - y, 16);
#if (MALI_DOWNSAMPLE_VIANEON_DEFINED)
				u32 odd_row = 1;
#endif
				int u, v;

				u8* dst_u8 = (u8*)dst_buf + dst_block_index * 16 * 16 * bytes_per_texel;

				struct _block_iterator it;

				/* Define neighboring blocks to sample from */
				int src_block_offsets[4];
				src_block_offsets[0] = src_block_index * 16 * 16 * bytes_per_texel;
				src_block_offsets[1] = (src_block_index + 1) * 16 * 16 * bytes_per_texel;
				src_block_offsets[2] = (src_block_index + src_num_blocks_in_x_axis + 1) * 16 * 16 * bytes_per_texel;
				src_block_offsets[3] = (src_block_index + src_num_blocks_in_x_axis) * 16 * 16 * bytes_per_texel;

				it.y_uorder = 0;
				for (v = 0; v < block_height; v++)
				{
					u32 sample_mask = 0xF;
					u32 sample_shift = 2;
#if (MALI_DOWNSAMPLE_VIANEON_DEFINED)
					odd_row = odd_row ^ 1; /* every second row is obviously odd */
#endif

					if ((v == block_height - 1) && (block_height_odd != 0))
					{
						sample_mask &= 0x3; /* first two pixels 1100 */
						sample_shift--;
					}

					it.x_uorder = 0;
					for (u = 0; u < block_width; u++)
					{
						int dst_index = _block_iterator_get_index(&it);
						int src_index;
						

						/* Get offset to block where we want to sample from */
						const u8* src_u8 = (const u8*)src_buf + src_block_offsets[dst_index >> 6]; /* div by 64 */

						if ((u == block_width - 1) && (block_width_odd != 0))
						{
							sample_mask &= 0x9; /* first and last pixel 1001 */
							sample_shift--;
						}
						
/*  produce 2 dst pixels at a time if we can*/
#if (MALI_DOWNSAMPLE_VIANEON_DEFINED)
						if ( has_neon_impl && sample_mask == 0xF )   /* is this neon based downsample function and downsample musk is 0xF
						for the 2 next pixels guarantee*/
						{
							dst_index-=odd_row; /* for the odd rows interleaved indexes are 3,2....,7,6 e.t.c. */
							if (block_height_odd == 0 || u < block_width - 2 )
							{
								u++; 
								_block_iterator_step_x(&it);
							}
						}
#else
						MALI_IGNORE(has_neon_impl);
#endif
						/* Compute source index inside block
						 * Only want to sample maximum 2x2 pixels, so multiply by 4
						 * Also make sure src_index repeats [0..FF]
						 */
						src_index = (dst_index << 2) & 0xFF;
						/*
						 * Call callback function which takes care of the actual downsampling based on input texelformat
						 */
						downsample(src_u8 + src_index * bytes_per_texel, dst_u8 + dst_index * bytes_per_texel, num_components, sample_mask, sample_shift);

						_block_iterator_step_x(&it);
					}
					_block_iterator_step_y(&it);
				}

				dst_block_index++;
				src_block_index += 2;
			}
			/* Will only sample every 2nd block line in source buffer.
			 * Also have to accomodate for odd number of blocks.
			 */
			if (src_last_block_width == 1 && src_surface->format.width  > 1)
			{
			 	src_block_index += src_num_blocks_in_x_axis + odd_fixup;
			}
			else
			{
			 	src_block_index += src_num_blocks_in_x_axis - odd_fixup;
			}
		}

		src_num_blocks_in_x_axis = dst_num_blocks_in_x_axis;
		src_buf = dst_buf;

		/*
		 * In order to generate the next miplevel, the old src should be unmapped, and the
		 * dst should be rebranded as src. We do this by changing some pointers around. 
		 */
		_mali_surface_unmap( src_surface );
		_mali_surface_access_unlock( src_surface );

		/* must unmap the dst surface before we can assign it */
		_mali_surface_unmap( dst_surface );
		_mali_surface_access_unlock( dst_surface );

		/* free previous data (if present), set new miplevel to dst_surface*/
		merr = _gles_texture_miplevel_assign( ctx, tex_obj, mipchain, lev, format, type, 1, &dst_surface);
		if(merr != MALI_ERR_NO_ERROR)
		{
			MALI_DEBUG_ASSERT(merr == MALI_ERR_OUT_OF_MEMORY, ("Only possible error code is OOM"));
			_mali_surface_deref( dst_surface );
			return GL_OUT_OF_MEMORY; 
		}

		/* remap dst surface, in order to be able to write to it. Mapping this surface is safe - no lock order starvation may 
		 * occur as no others may access this surface just yet. */
		_mali_surface_access_lock( dst_surface );
		dst_buf = _mali_surface_map( dst_surface, MALI_MEM_PTR_WRITABLE );
		if (NULL == dst_buf)
		{
			_mali_surface_access_unlock( dst_surface );
			return GL_OUT_OF_MEMORY; 
		}


		src_surface = dst_surface;

		/* did we generate all the mipmap levels? */
		if( ( 1 == src_surface->format.width ) && ( 1 == src_surface->format.height ) ) break;

	}

	/* If it's the last level, also unmap the src surface, since we don't need it anymore */
	_mali_surface_unmap( src_surface );
	_mali_surface_access_unlock( src_surface );

	tex_obj->paletted = MALI_FALSE;

	return GL_NO_ERROR;
}


/* Generates all required mipmaps based on the texel data for the level 0 mipmap. */
GLenum _gles_generate_mipmap_chain_sw(
	struct gles_texture_object *tex_obj,
	struct gles_context *ctx,
	GLenum target,
	unsigned int base_miplvl )
	
{
	const GLvoid *pixels;
	
	gles_mipmap_level *miplevel_base;
	GLenum format;
	GLenum type;
	mali_surface* src_surface;
	unsigned int start_level = base_miplvl + 1;

	enum mali_convert_pixel_format convert_format;
	u32 bytes_per_texel;
	const void *src_buf = NULL;
	int lev;
	int src_width, src_height;
	int mipchain = _gles_texture_object_get_mipchain_index( target );

	/* retrieve format and type. These enums must be propagated down to other mipmap levels */
	miplevel_base = _gles_texture_object_get_mipmap_level(tex_obj, target, base_miplvl);
	MALI_DEBUG_ASSERT_POINTER(miplevel_base); /* this miplvl must already exist, as we are basing the generation on it */
	format = miplevel_base->format;
	type = miplevel_base->type;

	/* this surface MUST exist at this point, otherwise we can't generate miplevels. Callers need to ensure this! */
	src_surface = _gles_fb_texture_object_get_mali_surface(tex_obj->internal, mipchain, base_miplvl);
	MALI_DEBUG_ASSERT_POINTER(src_surface); 
	
	/* block interleaved textures are done faster on the specialcased function for just that */
	if (src_surface->format.texel_layout == M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED)
	{
		return _gles_generate_mipmap_chain_sw_16x16blocked(tex_obj, ctx, target, base_miplvl);
	}

	bytes_per_texel = __m200_texel_format_get_size( src_surface->format.texel_format );
	MALI_DEBUG_ASSERT( bytes_per_texel > 0, ("Invalid format leading to incorrect number of bytes\n") );
	MALI_CHECK( bytes_per_texel > 0, GL_INVALID_OPERATION );

	convert_format = _gles_get_convert_format(format, type);

	/* map the pixels */
	_mali_surface_access_lock(src_surface);
	pixels = _mali_surface_map( src_surface, MALI_MEM_PTR_READABLE );
	if( NULL == pixels) 
	{
		_mali_surface_access_unlock(src_surface);
	}


	src_width = src_surface->format.width;
	src_height = src_surface->format.height;
       

	src_buf = pixels;

	for ( lev = start_level; ; lev++ )
	{
		/* floor operation needed to respect OpenGL mipmap level definition */
		/* floor doesn't affect power of two textures*/
		int dst_width  = MAX( src_width / 2, 1 );
		int dst_height = MAX( src_height / 2, 1 );
		struct mali_surface* dst_surface;
		void* dst_buf;
		mali_err_code merr;



		/* allocate new mali surface */
		dst_surface = _gles_texture_miplevel_allocate(ctx, tex_obj, mipchain, lev, dst_width, dst_height, format, type);
		if( NULL == dst_surface)
		{
			_mali_surface_unmap( src_surface );
			_mali_surface_access_unlock( src_surface );
			return GL_OUT_OF_MEMORY;
		}
	
		/* map new surface, in order to be able to write to it. Mapping this surface is safe - no lock order starvation may 
		 * occur as no others may access this surface just yet. */
		_mali_surface_access_lock( dst_surface );
		dst_buf = _mali_surface_map( dst_surface, MALI_MEM_PTR_WRITABLE );
		if (NULL == dst_buf)
		{
			_mali_surface_access_unlock( dst_surface );
			_mali_surface_deref( dst_surface );
			_mali_surface_unmap( src_surface );
			_mali_surface_access_unlock( src_surface );
			return GL_OUT_OF_MEMORY; 
		}

		_gles_downsample_rgba8888(
			src_buf, src_width, src_height, src_surface->format.pitch, /* src specification */
			dst_buf, dst_width, dst_height, dst_surface->format.pitch, /* dst specification */
			convert_format
			);
		/*
		 * In order to generate the next miplevel, the old src should be unmapped, and the
		 * dst should be rebranded as src. We do this by changing some pointers around. 
		 */
		_mali_surface_unmap( src_surface );
		_mali_surface_access_unlock( src_surface );
		
		/* must unmap the dst surface before we can assign it */
		_mali_surface_unmap( dst_surface );
		_mali_surface_access_unlock( dst_surface );

		/* free previous data (if present), set new miplevel to dst_surface*/
		merr = _gles_texture_miplevel_assign( ctx, tex_obj, mipchain, lev, format, type, 1, &dst_surface);
		if(merr != MALI_ERR_NO_ERROR)
		{
			MALI_DEBUG_ASSERT(merr == MALI_ERR_OUT_OF_MEMORY, ("Only possible error code is OOM"));
			_mali_surface_deref( dst_surface );
			return GL_OUT_OF_MEMORY; 
		}

		/* remap dst surface, in order to be able to write to it. Mapping this surface is safe - no lock order starvation may 
		 * occur as no others may access this surface just yet. */
		_mali_surface_access_lock( dst_surface );
		dst_buf = _mali_surface_map( dst_surface, MALI_MEM_PTR_WRITABLE );
		if (NULL == dst_buf)
		{
			_mali_surface_access_unlock( dst_surface );
			return GL_OUT_OF_MEMORY; 
		}

		src_surface = dst_surface;
		src_buf = dst_buf;
		src_width  = dst_width;
		src_height = dst_height;

		/* did we generate all the mipmap levels? */
		if( ( 1 == src_width ) && ( 1 == src_height ) ) break;
	}

	_mali_surface_unmap(src_surface);
	_mali_surface_access_unlock(src_surface);

	return GL_NO_ERROR;
}

GLenum _gles_generate_mipmap_chain(
	struct gles_texture_object *tex_obj,
	struct gles_context *ctx,
	GLenum target,
	unsigned int base_miplvl)
{
	struct mali_surface* base_surface;
	int chain_index = _gles_texture_object_get_mipchain_index(target);
	gles_mipmap_level *miplevel_base = _gles_texture_object_get_mipmap_level(tex_obj, target, base_miplvl);

	/* input conditions */
	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_LEQ(base_miplvl, GLES_MAX_MIPMAP_LEVELS - 1); /* 0..12 is legal */

	/* early out if there is no miplevel 0 */
	base_surface = _gles_fb_texture_object_get_mali_surface( tex_obj->internal, chain_index, base_miplvl );	
	if(base_surface == NULL) return GL_INVALID_OPERATION; /* early out, nothing to generate as this texture not uploaded */

	/* get rid of all dependencies by lock/unlocking the surface point, as in glTexSubImage */
	base_surface = _gles_texture_miplevel_lock(ctx, tex_obj, chain_index, base_miplvl);
	if(base_surface == NULL) return GL_OUT_OF_MEMORY; /* we verified that it must exist earlier, but if there was a dependency, we may run OOM here */
	_gles_texture_miplevel_unlock(ctx, tex_obj, chain_index, base_miplvl); /* just release it immediately */

	if(base_surface->format.width <= 1 && base_surface->format.height <= 1) return GL_NO_ERROR; /* nothing to do */
	
	/* mipgen not supported for ETC - but no error code specified in the Khronos spec. Return silently */
	if(base_surface->format.texel_format == M200_TEXEL_FORMAT_ETC) return GL_NO_ERROR; 
	
	/* mipgen not supported for depth textures */
	if(base_surface->format.pixel_format == MALI_PIXEL_FORMAT_S8Z24) return GL_INVALID_OPERATION;
	if(base_surface->format.pixel_format == MALI_PIXEL_FORMAT_Z16) return GL_INVALID_OPERATION; 

    /* Drop EGL connection by entirely copying the miplevel 0 into the new non-EGL mali surface*/
	if (base_miplvl == 0 && (base_surface->flags & MALI_SURFACE_FLAG_EGL_IMAGE_SIBLING)) 
	{
		GLenum err;
		MALI_DEBUG_ASSERT_POINTER(miplevel_base);
		err =  realloc_miplevel(ctx, tex_obj, chain_index, miplevel_base->format, miplevel_base->type, 0);
		if (err != GL_NO_ERROR)	return err;

		/* renew base_surface*/
		base_surface = _gles_fb_texture_object_get_mali_surface( tex_obj->internal, chain_index, base_miplvl );
	}

	MALI_DEBUG_ASSERT( !(base_miplvl == 0 && (base_surface->flags & MALI_SURFACE_FLAG_EGL_IMAGE_SIBLING)), (" miplevel 0 is still EGL Sibling\n") );

	/* Decide whether to do mipgen in SW or HW */
	if( (base_surface->format.width * base_surface->format.height) > GLES_HW_MIPGEN_THRESHOLD /* large enough for the HW setup to be worth it */
	    && base_surface->format.pixel_format != MALI_PIXEL_FORMAT_NONE		    /* a format mali can write to */
	    && base_surface->format.pixel_format != MALI_PIXEL_FORMAT_ARGB_FP16 )   /* mali can't multisample to FP16 */
	{
		/* perform mipgen in HW */	
		return _gles_generate_mipmap_chain_hw( tex_obj, ctx, target, base_miplvl );

	} else {
		/* perform mipgen in SW */
		if( base_surface->format.texel_layout == M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED )
		{
			/* block interleaved express mode */
			return _gles_generate_mipmap_chain_sw_16x16blocked( tex_obj, ctx, target, base_miplvl );
		} else {
			/* linear mode */
			return _gles_generate_mipmap_chain_sw( tex_obj, ctx, target, base_miplvl );
		}

	}


}

