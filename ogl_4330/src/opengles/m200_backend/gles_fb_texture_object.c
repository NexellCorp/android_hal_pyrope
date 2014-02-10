/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include "gles_fb_texture_object.h"

#include "base/mali_debug.h"
#include "base/mali_byteorder.h"
#include "base/mali_context.h"
#include "base/mali_dependency_system.h"
#include "util/mali_math.h"
#include "util/mali_mem_ref.h"
#include "shared/mali_shared_mem_ref.h"
#include "shared/mali_surface.h"
#include "shared/m200_readback_util.h"
#include "shared/m200_gp_frame_builder_inlines.h"
#include "gles_instrumented.h"

#include "../gles_config_extension.h"
#include "../gles_texture_object.h"
#include "../gles_util.h"
#include "../gles_context.h"

#include "gles_fb_texture_memory.h"
#include "gles_m200_texel_format.h"
#include "gles_m200_td.h"
#include "gles_fb_swappers.h"

#if EXTENSION_EGL_IMAGE_OES_ENABLE
#include "egl/egl_image.h"
#endif

/**
 * @brief Swaps and inverts the color components of a texture if needed
 * @param texels The array of texels to modify
 * @param needs_rbswap Whether we need to swap the red and blue components
 * @param needs_invert Whether we need to invert the color components
 * @param src_is_reversed Whether the source components are inverted or not
 * @param width The width of the image
 * @param height The height of the image
 * @param pixel_format The pixels format
 * @param src_pitch The offset between lines in the image (it is 0 for 16X16_BLOCK_INTERLEAVED images)
 * @return void
 */
MALI_STATIC void _swap_color_components
	(   void* texels,
		GLboolean needs_rbswap,
		GLboolean needs_invert,
		GLboolean src_is_reversed,
		GLsizei width,
		GLsizei height,
		m200_texel_format texel_format,
		int src_pitch  )
{
	int  padding;
	void *tmp_src;

	MALI_DEBUG_ASSERT_POINTER( texels );
	MALI_DEBUG_ASSERT( src_pitch>=0, ("Incorrect pitch") );

	MALI_DEBUG_ASSERT( (needs_rbswap || needs_invert ),  ("There should be at least one mode to swizzle" ) );

	MALI_DEBUG_PRINT(7, ("_swap_color_components(width=%d, height=%d, texel format=%d)\n",width,height,texel_format));


	/* M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED */
	if ( 0 == src_pitch )
	{
		/* In this case, we will process the whole surface, disregarding
		 * which texels are real and which are just padding */
		width  = (width+15)&~15;
		height = (height+15)&~15;
		padding = 0;
	}
	/* M200_TEXTURE_ADDRESSING_MODE_LINEAR */
	else
	{
		padding = (src_pitch>>1) - width;
		if ( texel_format == M200_TEXEL_FORMAT_AL_88) padding = (src_pitch>>1) - width;
		if ( texel_format == M200_TEXEL_FORMAT_ARGB_8888) padding = (src_pitch>>2) - width;
#if !RGB_IS_XRGB
		if ( texel_format == M200_TEXEL_FORMAT_RGB_888) padding = (src_pitch / 3) - width;
#endif
		if ( texel_format == M200_TEXEL_FORMAT_AL_16_16) padding = (src_pitch>>2) - width;
		if ( texel_format == M200_TEXEL_FORMAT_ARGB_16_16_16_16) padding = (src_pitch>>4) - width;
	}

	tmp_src = texels;

	switch ( texel_format )
	{
		case M200_TEXEL_FORMAT_RGB_565:
			/* Note: This format cannot be inverted */
			_color_swap_rgb565( tmp_src, width, height, padding );
			break;
#if !RGB_IS_XRGB
		case M200_TEXEL_FORMAT_RGB_888:
			_color_swap_rgb888(tmp_src, width, height, padding );
			break;
#endif
		case M200_TEXEL_FORMAT_AL_88:
			_color_swap_al88(tmp_src, width, height, padding );
			break;
		case M200_TEXEL_FORMAT_ARGB_8888:
			if (!needs_invert)		_color_swap_argb8888( tmp_src, width, height, padding, src_is_reversed );
			else if (!needs_rbswap)	_color_invert_argb8888( tmp_src, width, height, padding );
			else					_color_swap_and_invert_argb8888( tmp_src, width, height, padding, src_is_reversed );
			break;
		case M200_TEXEL_FORMAT_xRGB_8888: 
			if (!needs_invert)		_color_swap_argb8888( tmp_src, width, height, padding, src_is_reversed );
			else if (!needs_rbswap)	_color_invert_argb8888( tmp_src, width, height, padding );
			else					_color_swap_and_invert_argb8888( tmp_src, width, height, padding, src_is_reversed );
			break;
		case M200_TEXEL_FORMAT_ARGB_4444:
			if (!needs_invert)		_color_swap_argb4444( tmp_src, width, height, padding, src_is_reversed );
			else if (!needs_rbswap)	_color_invert_argb4444( tmp_src, width, height, padding );
			else					_color_swap_and_invert_argb4444( tmp_src, width, height, padding, src_is_reversed );
			break;
		case M200_TEXEL_FORMAT_ARGB_1555:
			if (!needs_invert)		_color_swap_argb1555( tmp_src, width, height, padding, src_is_reversed );
			else if (!needs_rbswap)	_color_invert_argb1555( tmp_src, width, height, padding, src_is_reversed );
			else					_color_swap_and_invert_argb1555( tmp_src, width, height, padding, src_is_reversed );
			break;
		case M200_TEXEL_FORMAT_AL_16_16:
			_color_swap_al_16_16(tmp_src, width, height, padding );
			break;
		case M200_TEXEL_FORMAT_ARGB_16_16_16_16:
			if (!needs_invert)		_color_swap_argb_16_16_16_16( tmp_src, width, height, padding, src_is_reversed );
			else if (!needs_rbswap)	_color_invert_argb_16_16_16_16( tmp_src, width, height, padding );
			else					_color_swap_and_invert_argb_16_16_16_16( tmp_src, width, height, padding, src_is_reversed );
			break;
		default: 
			MALI_DEBUG_ASSERT( 0, ("Format not supported: %d\n", texel_format ) );
			return;
	}
}


/* Compressed paletted formats are extensions for OpenGL ES 2.0 */
#ifdef EXTENSION_COMPRESSED_PALETTED_TEXTURES_OES_ENABLE

MALI_STATIC_INLINE u32 _gles_get_palette4_index(const void *src_texels, unsigned int texel_index)
{
	u32 index_byte;
	u32 index_subword;

	/* The bit-layout of this is documented in the OpenGL ES 1.1 full spec, in section 3.7.4, table 3.12 */
	index_byte = ((const u8*)src_texels)[texel_index / 2];
	index_subword = 1 - (texel_index % 2); /* "big endian" nibbles */
	return (index_byte >> (index_subword * 4)) & 0xF;
}

MALI_STATIC_INLINE u32 _gles_get_palette8_index(const void *src_texels, unsigned int texel_index)
{
	return ((const GLubyte*)src_texels)[texel_index];
}

/**
 * @brief Converts paletted to non-paletted textures
 * @param texels The array of texels
 * @param source_offset The source offset
 * @param format The format
 * @param mip_width The mipmap width
 * @param mip_height The mipmap height
 * @param src_rbswap Is src red/blue swapped
 * @param src_revorder Is src rev ordered
 * @param dst_surf The destination pointer
 * @return void
 * @note none
 */
MALI_STATIC mali_bool _gles_tex_to_nonpaletted_tex(
	const void *texels,
	int source_offset,
	GLenum format,
	int mip_width,
	int mip_height,
	mali_bool src_rbswap,
	mali_bool src_revorder,
	mali_surface *dst_surf )
{
	s32 i, j, j_stride;
	const void *src_pal = texels;
	const void *src_texels = (void *)((u32)texels + source_offset);
	void* dst_ptr;
	void* surf_ptr;
	void* temp_block = NULL;
	u16 dst_pitch;
	mali_bool is_linear;
	mali_bool need_rbswap, need_revorder;

	MALI_DEBUG_ASSERT_POINTER( texels );
	MALI_DEBUG_ASSERT_POINTER( dst_surf );

	need_revorder = ( dst_surf->format.reverse_order != src_revorder );
	need_rbswap = ( dst_surf->format.red_blue_swap != src_rbswap );

	is_linear = dst_surf->format.pixel_layout == MALI_PIXEL_LAYOUT_LINEAR ? MALI_TRUE : MALI_FALSE;


	/* grab writable pointer. */
	_mali_surface_access_lock(dst_surf);

	surf_ptr = _mali_surface_map(dst_surf, MALI_MEM_PTR_WRITABLE | MALI_MEM_PTR_READABLE);
	if ( NULL == surf_ptr )
	{
		_mali_surface_access_unlock(dst_surf);
		return MALI_FALSE;
	}

	if(is_linear) 
	{
		/* if output surface is linear, output directly to the output surface */
		dst_ptr = surf_ptr;
		dst_pitch = dst_surf->format.pitch;
	} else {
		/* otherwise, allocate a local memory block, output to it, then swizzle at the end */
		dst_pitch = 0;
		switch(format)
		{
			case GL_PALETTE4_R5_G6_B5_OES:
			case GL_PALETTE4_RGBA4_OES:
			case GL_PALETTE4_RGB5_A1_OES:
			case GL_PALETTE8_R5_G6_B5_OES:
			case GL_PALETTE8_RGBA4_OES:
			case GL_PALETTE8_RGB5_A1_OES:
				dst_pitch = 2*mip_width;
				break;
			case GL_PALETTE4_RGB8_OES:
			case GL_PALETTE8_RGB8_OES:
#if RGB_IS_XRGB
				/* pack out to 32bit format always to avoid using a 24bit format */
				dst_pitch = 4*mip_width;
#else 
				dst_pitch = 3*mip_width;
#endif
				break;
			case GL_PALETTE8_RGBA8_OES:
			case GL_PALETTE4_RGBA8_OES:
				dst_pitch = 4*mip_width;
				break;
		}
		MALI_DEBUG_ASSERT(dst_pitch != 0, ("Pitch should be set now"));

		temp_block = _mali_sys_malloc(mip_height*dst_pitch);
		if(temp_block == NULL)
		{
			_mali_surface_unmap( dst_surf );
			_mali_surface_access_unlock( dst_surf );
			return MALI_FALSE;
		}
		dst_ptr = temp_block;

	}


	/* Switch converts paletted to non-paletted textures */
	switch ( format )
	{
	case GL_PALETTE4_R5_G6_B5_OES:
	case GL_PALETTE4_RGBA4_OES:
	case GL_PALETTE4_RGB5_A1_OES:
		/* convert 4-bit paletted with 16-bit colors to 16-bit texels */
	{
		for (j=0; j<mip_height; j++)
		{
			u16 *dst;

			j_stride = j * mip_width;

			/* set dst to the start of the current column */
			dst = ((u16 *)dst_ptr) + (j * dst_pitch / 2);

			for (i=0; i<mip_width; i++)
			{
				unsigned int index = _gles_get_palette4_index(src_texels, j_stride + i);
				*dst++ = ((const u16 *)src_pal)[index];
			}
		}
	}

	break;

	case GL_PALETTE4_RGB8_OES:
	case GL_PALETTE4_RGBA8_OES:
		/* convert 4-bit paletted with 8 bit components */
	{
		u8 *dst;
		int num_components = ( GL_PALETTE4_RGB8_OES == format ? 3 : 4 );
		int dst_num_components = num_components;
#if RGB_IS_XRGB
		/* pack out to 32bit format always to avoid using a 24bit format */
		dst_num_components = 4;
#endif

		for (j=0; j<mip_height; j++)
		{
			j_stride = j * mip_width;

			/* set dst to the start of the current row */
			dst = ((u8 *)dst_ptr) + j * dst_pitch;

			for (i=0; i<mip_width; i++)
			{
				unsigned int index = _gles_get_palette4_index(src_texels, j_stride + i);
				const u8 *src_color = &((const u8*)src_pal)[index * num_components];

				*dst++ = *src_color++;
				*dst++ = *src_color++;
				*dst++ = *src_color++;
				if (4 == num_components && 4 == dst_num_components) *dst++ = *src_color++;
				if (3 == num_components && 4 == dst_num_components) *dst++ = 0xFF;
			}
		}
	}
	break;

	case GL_PALETTE8_R5_G6_B5_OES:
	case GL_PALETTE8_RGBA4_OES:
	case GL_PALETTE8_RGB5_A1_OES:
		/* convert 8-bit paletted with 16-bit colors to 16-bit texels */
	{
		for (j=0; j<mip_height; j++)
		{
			u16 *dst;
			j_stride = j * mip_width;

			/* set dst to the start of the current row */
			dst = ((u16 *)dst_ptr) + (j * dst_pitch / 2);

			for (i=0; i<mip_width; i++)
			{
				unsigned int index = _gles_get_palette8_index(src_texels, j_stride + i);
				*dst++ = ((const u16 *)src_pal)[index];
			}
		}
	}
	break;

	case GL_PALETTE8_RGB8_OES:
	case GL_PALETTE8_RGBA8_OES:
		/* convert 8-bit paletted with 8 bit components */
	{
		u8 *dst;
		int num_components = ( GL_PALETTE8_RGB8_OES == format ? 3 : 4 );
		int dst_num_components = num_components;
#if RGB_IS_XRGB
		/* pack out to 32bit format always to avoid using a 24bit format */
		dst_num_components = 4;
#endif

		for (j=0; j<mip_height; j++)
		{
			j_stride = j * mip_width;

			/* set dst to the start of the current row */
			dst = ((u8 *)dst_ptr) + j * dst_pitch;

			for (i=0; i<mip_width; i++)
			{
				unsigned int index = _gles_get_palette8_index(src_texels, j_stride + i);
				const u8 *src_color = &((const u8*)src_pal)[index * num_components];

				*dst++ = *src_color++;
				*dst++ = *src_color++;
				*dst++ = *src_color++;
				if (4 == num_components && 4 == dst_num_components) *dst++ = *src_color++;
				if (3 == num_components && 4 == dst_num_components) *dst++ = 0xFF;
			}
		}
	}
	break;

	default:
		MALI_DEBUG_ASSERT( MALI_FALSE, ("unsupported format 0x%X", format));
		break;
	}
	
	if(need_rbswap || need_revorder)
	{
		/* slow operation as it reads from mapped memory, but this should only trigger in very rare conditions */
		/* at this point, dst_surf is linear no matter what */
		_swap_color_components(dst_ptr, need_rbswap, need_revorder, src_revorder, mip_width, mip_height, dst_surf->format.texel_format, dst_pitch);
	}

	/* swizzle linear data into blockinterleaved destination, if destination was blockinterleaved */
	if(is_linear == MALI_FALSE)
	{
		mali_err_code err = _m200_texture_swizzle(
			surf_ptr, dst_surf->format.texel_layout, /* destination */
			temp_block, M200_TEXTURE_ADDRESSING_MODE_LINEAR, /* source */
			dst_surf->format.width, dst_surf->format.height, /* dimensions */
			dst_surf->format.texel_format,     /* format */
			dst_surf->format.pitch,  /* destination pitch */
			dst_pitch  /* source pitch, here called dst_pitch because it's the dst_pitch from the decompress POV */
		);
		if(err != MALI_ERR_NO_ERROR)
		{
			_mali_sys_free(temp_block);
			_mali_surface_unmap(dst_surf);
			_mali_surface_access_unlock(dst_surf);
			return MALI_FALSE;
		}
		_mali_sys_free(temp_block);
	}

	_mali_surface_unmap(dst_surf);
	_mali_surface_access_unlock(dst_surf);

	return MALI_TRUE;
}

#endif /* EXTENSION_COMPRESSED_PALETTED_TEXTURES_OES_ENABLE */



static const u8 u_order_y[] = {
	/* bits abcd expanded to aabbccdd */
	0x0, 0x3, 0xc, 0xf, 0x30, 0x33, 0x3c, 0x3f, 0xc0, 0xc3, 0xcc, 0xcf, 0xf0, 0xf3, 0xfc, 0xff
};

static const u8 u_order_x[] = {
	/* bits abcd expanded to 0a0b0c0d */
	0x0, 0x1, 0x4, 0x5, 0x10, 0x11, 0x14, 0x15, 0x40, 0x41, 0x44, 0x45, 0x50, 0x51, 0x54, 0x55,
};

MALI_STATIC_INLINE
u8* _interleave_get_next_texel_dst(const void* dst, const int x,
                                   const int yblock, const int y_bits,
                                   const int texel_size, const int block_size)
{
	int x_bits = u_order_x[x & 0xF];
	int new_pos = y_bits ^ x_bits;
	int dst_offset = new_pos * texel_size;
	int block_offset = (yblock + (x >> 4)) * block_size;
	u8 *dst_block = (u8*)dst + block_offset;
	return dst_block + dst_offset;
}

MALI_STATIC_INLINE
void _m200_texture_block_interleave_span(void *dst, int dst_width,
                                         const void *src, int y, int x,
                                         int len, int texel_size, int copy_elem_size)
{
	/* expand bits abc to a0b0c0  */
	u8 *src_scanline = (u8*)src;

	int xblocks = (dst_width + 15) / 16; /* round width up */

	int yblock = (y / 16) * xblocks;
	const int y_bits = u_order_y[y % 16];
	const int block_size = (16 * 16 * texel_size);

	MALI_DEBUG_ASSERT((x>=0), ("x < 0"));

	MALI_DEBUG_PRINT(6, ("_m200_texture_block_interleave_span: len==%d, texel_size==%d, elem_size==%d\n",
			len, texel_size, copy_elem_size));

	/* copy texel */

	/* Basically each switch branch here does:
	 *
	 *	_mali_sys_memcpy_cpu_to_mali(texel_dst, src_scanline, texel_size, copy_elem_size);
	 *
	 *  For performance reason switch is raised to top-level and each element size is
	 *  handled separately.
	 */

	switch(copy_elem_size)
	{
	case 1:
	{
		while ( 0 < len--)
		{
			u8* texel_dst = (u8*)_interleave_get_next_texel_dst(dst, x, yblock, y_bits, texel_size, block_size);
			int j;
			for( j = texel_size; 0 < j; j-- )
			{
				_MALI_SET_U8_ENDIAN_SAFE(texel_dst, *src_scanline);
				texel_dst++;
				src_scanline++;
			}

			x++;
		}
		break;
	}
	case 2:
	{
		while ( 0 < len--)
		{
			u16* texel_dst = (u16*)_interleave_get_next_texel_dst(dst, x, yblock, y_bits, texel_size, block_size);
			int j;
			for( j = texel_size; 0 < j; j-=2 )
			{
				_MALI_SET_U16_ENDIAN_SAFE(texel_dst, *(u16*)src_scanline);
				texel_dst++;
				src_scanline+=2;
			}

			x++;
		}
		break;
	}
	case 4:
	{
		while ( 0 < len--)
		{
			u32* texel_dst = (u32*)_interleave_get_next_texel_dst(dst, x, yblock, y_bits, texel_size, block_size);
			int j;
			for( j = texel_size; 0 < j; j-=4 )
			{
				_MALI_SET_U32_ENDIAN_SAFE(texel_dst, *(u32*)src_scanline);
				texel_dst++;
				src_scanline+=4;
			}

			x++;
		}
		break;
	}
	default:
		MALI_DEBUG_ASSERT(0, ("Unknown copy size for element"));
		break;
	}

}

/** initialize structures */
/**
 * @brief Initializes the framebuffer texture object structure
 * @param tex_obj The framebuffer texture object pointer
 * @return void
 * @note none
 */
MALI_STATIC void _gles_fb_texture_object_init(struct gles_fb_texture_object *tex_obj, enum gles_texture_target dimensionality, mali_base_ctx_handle base_ctx)
{
	int i;
	MALI_DEBUG_ASSERT_POINTER(tex_obj);

	/* It should never be legal to get or create a new texture object with dimensionality GLES_TEXTURE_TARGET_INVALID. 
	 * These cases should always be caught and handled by the parent function */
	MALI_DEBUG_ASSERT( dimensionality != GLES_TEXTURE_TARGET_INVALID, ("dimensionality is in an illeal state") ); 

	_mali_sys_memset(tex_obj, 0, sizeof(struct gles_fb_texture_object));

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	if(dimensionality == GLES_TEXTURE_TARGET_EXTERNAL)
	{
		/* external surfaces do not have miplevels. Only setting up the planes. */
		for(i = 0; i < GLES_FB_TDS_COUNT; i++) 
		{
			_gles_fb_texture_memory_init(&tex_obj->texmem[0][i], dimensionality, 1, base_ctx);
		}
	} 
	else 
#endif
	{
		/* non-external surfaces do not have planes */
		for(i = 0; i < MALI_TD_MIPLEVEL_POINTERS-1; i++) 
		{
			_gles_fb_texture_memory_init(&tex_obj->texmem[i][0], dimensionality, 1, base_ctx); /* each of the first pointers have 1 subplane each */
		}
		_gles_fb_texture_memory_init(&tex_obj->texmem[i][0], dimensionality, 3, base_ctx); /* last one holds lvl index 10, 11 and 12 */
	}

	_gles_m200_td_set_defaults( tex_obj->tds[0] );
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	_gles_m200_td_set_defaults( tex_obj->tds[1] );
	_gles_m200_td_set_defaults( tex_obj->tds[2] );
#endif
	tex_obj->num_uploaded_surfaces = 0;
	tex_obj->used_planes = 1;
	tex_obj->layout = M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED;
	tex_obj->linear_pitch_lvl0 = 0;	
	tex_obj->using_td_pitch_field = MALI_FALSE;	
	tex_obj->need_constraint_resolve = MALI_FALSE;
	tex_obj->red_blue_swap = MALI_FALSE;
	tex_obj->order_invert = MALI_FALSE;
	tex_obj->dimensionality = dimensionality;
	tex_obj->base_ctx = base_ctx;
	_mali_sys_atomic_initialize(&tex_obj->ref_count, 1);
	tex_obj->mali_tds[0] = NULL;
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	tex_obj->mali_tds[1] = NULL;
	tex_obj->mali_tds[2] = NULL;
#endif
	tex_obj->to_last_used_frame = MALI_BAD_FRAME_ID;
	tex_obj->tds_last_used_frame = MALI_BAD_FRAME_ID;
}

/** de-initialize structure */
/**
 * @brief De-initializes the framebuffer texture object structure
 * @param tex_obj The framebuffer texture object pointer
 * @return void
 * @note none
 */
MALI_STATIC void _gles_fb_texture_object_deinit(struct gles_fb_texture_object *tex_obj)
{
	int i;
	MALI_DEBUG_ASSERT_POINTER(tex_obj);

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	if(tex_obj->dimensionality == GLES_TEXTURE_TARGET_EXTERNAL)
	{
		/* external surfaces do not have miplevels. Only setting up the planes. */
		for(i = 0; i < GLES_FB_TDS_COUNT; i++) 
		{
			_gles_fb_texture_memory_reset(&tex_obj->texmem[0][i]);
		}
	} 
	else 
#endif
	{
		/* non-external surfaces do not have planes */
		for(i = 0; i < MALI_TD_MIPLEVEL_POINTERS; i++) 
		{
			_gles_fb_texture_memory_reset(&tex_obj->texmem[i][0]); 
		}
	}

	if (NULL != tex_obj->mali_tds[0]) _mali_mem_deref(tex_obj->mali_tds[0]);
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	if (NULL != tex_obj->mali_tds[1]) _mali_mem_deref(tex_obj->mali_tds[1]);
	if (NULL != tex_obj->mali_tds[2]) _mali_mem_deref(tex_obj->mali_tds[2]);
#endif
	tex_obj->mali_tds[0] = NULL;
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	tex_obj->mali_tds[1] = NULL;
	tex_obj->mali_tds[2] = NULL;
#endif
}

/**
 * Creates a linear surface based on a GLES standard datasource. 
 * The surface is then filled with the data from texels
 * @param width [in] The width required for the linear surface
 * @param height [in] The height required for the linear surface
 * @param texels [in] The texels pointer, as given by glTexImage2D
 * @param src_pitch [in] The source pitch that defines the texels parameter
 * @param src_format [in] The format present in the texels data. 
 * @param src_rbswap [in] The redblue swap present in the texels data
 * @param src_revorder [in] The revorder present in the texels data
 * @param base_ctx [in] The base context used to allocate this surface. 
 */
MALI_STATIC_INLINE mali_surface* _gles_fb_alloc_linear_surface_from_gles_data(
                                     unsigned int width, unsigned int height, 
									 const void* texels, 
									 unsigned int src_pitch,
									 m200_texel_format src_format, 
									 mali_bool src_rbswap,
									 mali_bool src_revorder,
									 mali_base_ctx_handle base_ctx ) 
{
	mali_surface* retval;
	u32 bpp;
	u32 elem_size;
	unsigned int dst_pitch;
	mali_surface_specifier sformat;
	_mali_surface_specifier_ex( &sformat,
					 width, height, 0, 
	                                 MALI_PIXEL_FORMAT_NONE, src_format,
									 MALI_PIXEL_LAYOUT_LINEAR, M200_TEXTURE_ADDRESSING_MODE_LINEAR,
									 src_rbswap, src_revorder,
								     MALI_SURFACE_COLORSPACE_sRGB, MALI_SURFACE_ALPHAFORMAT_NONPRE, MALI_FALSE );


	bpp = _mali_surface_specifier_bpp(&sformat);
	elem_size = __m200_texel_format_get_bytes_per_copy_element(src_format);
	

	retval = _mali_surface_alloc(MALI_SURFACE_FLAGS_NONE, &sformat, 0, base_ctx);
	if(retval == NULL) return NULL;

	dst_pitch = retval->format.pitch; /* fetch the minimum required pitch by this surface */

	if( dst_pitch == src_pitch )
	{
		/* copy the entire data at once */
#ifndef NDEBUG
		/* for debug mode, just test that the datasize in texels equals the datasize required by mali_surface */
		u32 avail_data = ((height-1) * src_pitch) + (((width * bpp ) + 7) / 8);
		MALI_DEBUG_ASSERT(avail_data == retval->datasize, ("surface->datasize > available data. Something is amiss"));
		MALI_IGNORE(avail_data);
#endif
		_mali_mem_write_cpu_to_mali( retval->mem_ref->mali_memory, retval->mem_offset, texels, retval->datasize, elem_size);
	} else {
		/* copy line-by-line, width*bpp bytes of data */
		u32 i;
		u32 dst_offset = retval->mem_offset;
		const char* char_texels = texels;
		u32 datasize = ((width * bpp) + 7) / 8;

		MALI_DEBUG_ASSERT(dst_pitch >= datasize, ("dst picth should be >= width * bpp"));
		MALI_DEBUG_ASSERT(src_pitch >= datasize, ("src picth should be >= width * bpp"));

		for(i = 0; i < height; i++)
		{
			_mali_mem_write_cpu_to_mali( retval->mem_ref->mali_memory, dst_offset, char_texels, datasize, elem_size);
			dst_offset += dst_pitch;
			char_texels += src_pitch;
		}

	}

	return retval;
}
									 
/** Swizzles a texture in hw
 * @param frame_builder The frame builder responsible for doing the HW swizzle job
 * @param out_surface The surface where the output will be placed
 * @param src_surface The input surface
 * @return An errorcode if it fails
 *
 */
MALI_STATIC_INLINE mali_err_code _gles_fb_texture_hw_swizzle(
	mali_frame_builder *frame_builder,
    mali_surface *out_surface,
    mali_surface *src_surface)
{
	mali_err_code            error = MALI_ERR_OUT_OF_MEMORY;

	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	MALI_DEBUG_ASSERT_POINTER( out_surface );
	MALI_DEBUG_ASSERT_POINTER( src_surface );

	MALI_DEBUG_ASSERT(src_surface->format.texel_format != M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8, ("Unsupported texture format for HW sizzle"));

	/* Make sure it is avaiable */
	_mali_frame_builder_wait( frame_builder );

	_mali_frame_builder_set_output( frame_builder, MALI_DEFAULT_COLOR_WBIDX, out_surface, MALI_OUTPUT_COLOR );

	error = _mali_frame_builder_use( frame_builder );
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == error, cleanup );

	/* Upload the texture */
	error = _mali_frame_builder_readback( frame_builder, src_surface, MALI_OUTPUT_COLOR, 0, 0, _mali_frame_builder_get_width( frame_builder ), _mali_frame_builder_get_height( frame_builder ) );
	MALI_CHECK_GOTO( error == MALI_ERR_NO_ERROR, cleanup );

	/* We want the frame-builder to be reset and wait for it to complete */
	error = _mali_frame_builder_swap( frame_builder );
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == error, cleanup );

	_mali_frame_builder_wait( frame_builder );
	_mali_frame_builder_set_output( frame_builder, MALI_DEFAULT_COLOR_WBIDX, NULL, 0);

cleanup:

	MALI_ERROR( error );
}

/** allocate storage */
struct gles_fb_texture_object *_gles_fb_texture_object_alloc(enum gles_texture_target dimensionality, mali_base_ctx_handle base_ctx)
{
	struct gles_fb_texture_object *tex_obj = _mali_sys_malloc(sizeof(struct gles_fb_texture_object));
	if (NULL == tex_obj) return tex_obj;

	_gles_fb_texture_object_init(tex_obj,dimensionality, base_ctx);
	return tex_obj;
}

/** free storage */
void _gles_fb_texture_object_free(struct gles_fb_texture_object *tex_obj)
{
	MALI_DEBUG_ASSERT_POINTER(tex_obj);

	_gles_fb_texture_object_deinit(tex_obj);
	_mali_sys_free(tex_obj);
}

/** decrease ref count */
void _gles_fb_texture_object_deref(struct gles_fb_texture_object *tex_obj)
{
	u32 ref_count = 0;
	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_DEBUG_ASSERT(_mali_sys_atomic_get( &tex_obj->ref_count ) >= 1, ("inconsistent ref counting"));

	ref_count = _mali_sys_atomic_dec_and_return( &tex_obj->ref_count );

	if (0 == ref_count)
	{
		_gles_fb_texture_object_free(tex_obj);
		tex_obj = NULL;
	}
}



int _gles_m200_get_texel_pitch(int width, m200_texture_addressing_mode texel_layout)
{
	/* prefer full interleaving - it's slightly faster */
	switch (texel_layout)
	{
	case M200_TEXTURE_ADDRESSING_MODE_LINEAR:
		return width;

	case M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED:
		/* round up to next multiple of 16 */
		return (width + 0xf) & ~0xf;

	default:
		MALI_DEBUG_ASSERT(0, ("invalid texel layout %d", texel_layout));
		return 0;
	}
}

MALI_CHECK_RESULT mali_err_code _gles_fb_tex_image_2d(
	mali_frame_builder *frame_builder,
	struct gles_fb_texture_object *tex_obj,
	struct mali_surface* surface,
	GLsizei width,
	GLsizei height,
	GLenum format,
	GLenum type,
	mali_bool src_red_blue_swap,
	mali_bool src_reverse_order,
	const void *texels,
	GLuint src_pitch
	)
{
	mali_err_code err;
	mali_surface_specifier sformat;
	mali_bool do_hw_swizzle = MALI_FALSE;
	mali_bool support_rbswap, support_revorder; 

	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	MALI_DEBUG_ASSERT_POINTER( tex_obj );

	sformat = surface->format;

	__m200_texel_format_flag_support( sformat.texel_format, &support_rbswap, &support_revorder);

	/* check required conditions */
	MALI_DEBUG_ASSERT(sformat.texel_format != M200_TEXEL_FORMAT_NO_TEXTURE, ("No texel format found!"));
	MALI_DEBUG_ASSERT_RANGE(width,  0, GLES_MAX_TEXTURE_SIZE);
	MALI_DEBUG_ASSERT_RANGE(height, 0, GLES_MAX_TEXTURE_SIZE);

#if ! MALI_PLATFORM_ARM_NEON 
	/* Is it all possible to swizzle this texture in hw (iow. can Mali render to this format)?
	 * And if it is, is the texture large enough that it would benefit us to do it in HW?
	 * Note: For NEON enabled platforms it's always better to do swizzling on CPU. */
	if ( ( sformat.pixel_format != MALI_PIXEL_FORMAT_NONE ) && 
	     ( sformat.pixel_format != MALI_PIXEL_FORMAT_S8Z24) && 
	     ( sformat.pixel_format != MALI_PIXEL_FORMAT_Z16 ) && 
	     ( sformat.pixel_format != MALI_PIXEL_FORMAT_S8 ) && 
	     ( sformat.texel_format != M200_TEXEL_FORMAT_xRGB_8888 ) && 
		 ( GLES_HW_SWIZZLE_THRESHOLD <= ( width * height ) ) )
	{
		do_hw_swizzle = MALI_TRUE;
	}
#endif /* MALI_PLATFORM_ARM_NEON */

	/* Upload the texel data to the surface, if any */
	if ( (NULL != texels) && ( width*height >0 ) )
	{
		/* swizzle from LINEAR to whatever layout is required by the surface (typically BLOCKINTERLEAVED) */
		if ( MALI_TRUE == do_hw_swizzle )  
		{
			mali_surface* linear_surf;
			/* Create a new LINEAR mali surface with a pitch matching the input data, fill it with data from texels. 
			 * The last two parameters denotes a possible memory area that the surface /may/ use, if it is large enough 
			 * This step is the first point where GLES is touching the texels content */ 
			
			linear_surf = _gles_fb_alloc_linear_surface_from_gles_data( width, height, texels, 
			                                                            src_pitch, sformat.texel_format, 
			                                                            src_red_blue_swap, src_reverse_order, tex_obj->base_ctx);
			if(linear_surf == NULL) 
			{
				return MALI_ERR_OUT_OF_MEMORY;
			}

			/* A new HW job will then render to the BLOCKINTERLEAVED surface using the LINEAR surface as a source.
			 * This takes care of the swizzling requirements and the flag shuffling all in one go. */
			err = _gles_fb_texture_hw_swizzle(
				frame_builder,
				surface,
				linear_surf );
			if(err != MALI_ERR_NO_ERROR)
			{
				_mali_surface_deref(linear_surf);
				return err;
			}

			/* clean up the linear surface */
			_mali_surface_deref(linear_surf);
		}
		else
		{
			void* dst_buf = NULL;
			mali_convert_request conv_request;
			mali_surface_specifier src_format;
			mali_convert_rectangle conv_rect;

			_mali_surface_access_lock( surface );
			dst_buf = _mali_surface_map( surface, MALI_MEM_PTR_WRITABLE );
			if (NULL == dst_buf) 
			{
				_mali_surface_access_unlock( surface );
				return MALI_ERR_OUT_OF_MEMORY;
			}
			
			/* Setup initial conversion parameters */
			conv_rect.sx = 0;
			conv_rect.sy = 0;
			conv_rect.dx = 0;
			conv_rect.dy = 0;
			conv_rect.width = width;
			conv_rect.height = height;
			_gles_m200_get_input_surface_format( &src_format, type, format, width, height );
			_mali_convert_request_initialize( &conv_request, dst_buf, surface->format.pitch, &surface->format, texels, src_pitch, &src_format, NULL, 0, &conv_rect, MALI_FALSE, MALI_TRUE, MALI_FALSE, MALI_CONVERT_SOURCE_OPENGLES );

			if ( MALI_FALSE == _mali_convert_texture( &conv_request ) )
			{
				/* TODO: expand the texture conversion library to cover all formats. 
				 * Bugzilla entry 10969 tracks this. Until it is in place, the old upload code 
				 * will be used. This branch represent that old code path, and it is a bit slow.  
				 */

				GLboolean needs_rbswap = (( conv_request.src_format.red_blue_swap != conv_request.dst_format.red_blue_swap ) & support_rbswap);
				GLboolean needs_invert = (( conv_request.src_format.reverse_order != conv_request.dst_format.reverse_order ) & support_revorder);
				

				err = _m200_texture_swizzle(
					dst_buf, sformat.texel_layout, /* destination */
					texels, M200_TEXTURE_ADDRESSING_MODE_LINEAR, /* source */
					width, height, /* dimensions */
					sformat.texel_format,     /* format */
					surface->format.pitch,  /* destination pitch */
					src_pitch  /* source pitch */
					);
				if (MALI_ERR_NO_ERROR != err)
				{
					_mali_surface_unmap( surface );
					_mali_surface_access_unlock( surface );
					return err;
				}
				if( needs_rbswap || needs_invert )
				{
					_swap_color_components( dst_buf, needs_rbswap, needs_invert, src_reverse_order,
					                        width, height, sformat.texel_format, surface->format.pitch);
				}
			}

			_mali_surface_unmap( surface );
			_mali_surface_access_unlock( surface );
		} /* else do swizzle in SW */

	} /* if( null != texels && width*height>0 ) */
	
	return MALI_ERR_NO_ERROR;
}

struct gles_fb_texture_object* _gles_fb_texture_object_copy( struct gles_fb_texture_object *src )
{
	struct gles_fb_texture_object *dst = NULL;
	unsigned int i,j;

	MALI_DEBUG_ASSERT_POINTER(src);

	dst = _gles_fb_texture_object_alloc(src->dimensionality, src->base_ctx);
	if (NULL == dst) return NULL;

	dst->dimensionality = src->dimensionality;
	dst->base_ctx = src->base_ctx; 
	/* texmem blocks handled below */
	dst->num_uploaded_surfaces = src->num_uploaded_surfaces; /* we keep all the surfaces, so this number is the same in the copy */
	dst->used_planes = src->used_planes;
	/* tds handled below in the td_deep_copy function */
	/* mali_tds should be left as NULL */
	dst->red_blue_swap = src->red_blue_swap;
	dst->order_invert = src->order_invert;
	dst->layout = src->layout;
	dst->linear_pitch_lvl0 = src->linear_pitch_lvl0;
	dst->using_td_pitch_field = src->using_td_pitch_field;
	dst->need_constraint_resolve = src->need_constraint_resolve;
	dst->using_renderable_miplevels = src->using_renderable_miplevels;
	
	/* make a full copy of all planes */
	for(j = 0; j < src->used_planes; j++)
	{
		for(i = 0; i < MALI_TD_MIPLEVEL_POINTERS; i++)
		{
			_gles_fb_texture_memory_copy(&dst->texmem[i][j], &src->texmem[i][j]); 
		}
	}

	/* update the TD for the deep copy change of addresses and to propagate texture env */
	_gles_m200_td_deep_copy( dst, src );

	/* success */
	return dst;
}

MALI_CHECK_RESULT mali_err_code _gles_fb_tex_sub_image_2d(
	struct gles_fb_texture_object *tex_obj,
	struct mali_surface* surface,
	/* the remaining parameters specify input data */
	GLint xoffset,
	GLint yoffset,
	GLsizei width,
	GLsizei height,
	GLenum format,
	GLenum type,
	GLboolean src_red_blue_swap,
	GLboolean src_reverse_order,
	const void *texels,
	GLuint pitch )
{

	/* check required conditions */
	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_DEBUG_ASSERT_RANGE( width, 0, GLES_MAX_TEXTURE_SIZE);
	MALI_DEBUG_ASSERT_RANGE( height, 0, GLES_MAX_TEXTURE_SIZE);

	if ( (NULL != texels) && (width*height > 0) )
	{
		void *dst_ptr;
		s32 update_width, update_height;

		mali_convert_request conv_request;
		mali_surface_specifier src_format;
		mali_convert_rectangle conv_rect;

		/* Get the write & access lock at this point. Nothing is allowed to modify surfaces before this lock is done */
		_mali_surface_access_lock( surface );

		/* get a pointer to the destination surface */
		dst_ptr = _mali_surface_map( surface, MALI_MEM_PTR_WRITABLE | MALI_MEM_PTR_READABLE );
		if ( NULL == dst_ptr )
		{
			/* kill all references, free temp consumer. */
			_mali_surface_access_unlock( surface );
			return MALI_ERR_OUT_OF_MEMORY;
		}

		/* Update the size */
		update_width  = (xoffset<0) ? (width + xoffset) : (width);
		xoffset = (xoffset<0) ? (0) : xoffset;
		update_height = (yoffset<0) ? (height + yoffset) : (height);
		yoffset = (yoffset<0) ? (0) : yoffset;

		/* Setup initial conversion parameters */
		conv_rect.sx = 0;
		conv_rect.sy = 0;
		conv_rect.dx = xoffset;
		conv_rect.dy = yoffset;
		conv_rect.width = update_width;
		conv_rect.height = update_height;
		_gles_m200_get_input_surface_format( &src_format, type, format, width, height );
		_mali_convert_request_initialize( &conv_request, dst_ptr, surface->format.pitch, &surface->format, texels, pitch, &src_format, NULL, 0, &conv_rect, MALI_FALSE, MALI_TRUE, MALI_FALSE, MALI_CONVERT_SOURCE_OPENGLES );

		if ( MALI_FALSE == _mali_convert_texture( &conv_request  ) )
		{
			/* TODO: expand the texture conversion library to cover all formats. 
			 * Bugzilla entry 10969 tracks this. Until it is in place, the old upload code 
			 * will be used. This branch represent that old code path, and it is a bit slow.  
			 */

			int texel_size, elem_size;
			mali_bool support_rbswap, support_revorder;
			void *cctexels = NULL; /* Color Corrected Texels */
			const void *texels_to_process;
			struct mali_surface_specifier* sformat;
			GLboolean needs_rbswap;
			GLboolean needs_invert; 

			MALI_DEBUG_PRINT( 4, ("\tTEXTURE CONVERSION FAILURE ON SUBTEXIMAGE2D!!!\n"));	

			sformat = &surface->format;
			MALI_DEBUG_ASSERT( sformat != NULL, ("No surface format found!") );
			MALI_DEBUG_ASSERT( sformat->texel_format != M200_TEXEL_FORMAT_NO_TEXTURE, ("No texel format found!") );
			MALI_DEBUG_ASSERT( __m200_texel_format_get_bpp(sformat->texel_format) % 8 == 0, ("unsupported bits per texel: %d\n", __m200_texel_format_get_bpp(sformat->texel_format)));

			/* Prepare the conversion */
			texel_size = __m200_texel_format_get_size( sformat->texel_format );
			elem_size = __m200_texel_format_get_bytes_per_copy_element( sformat->texel_format );

			_gles_m200_get_storage_surface_format(&src_format, type, format, width, height);
			__m200_texel_format_flag_support( src_format.texel_format, &support_rbswap, &support_revorder);	

			needs_rbswap = (( tex_obj->red_blue_swap != src_red_blue_swap ) & support_rbswap);
			needs_invert = (( tex_obj->order_invert  != src_reverse_order ) & support_revorder);

			/* Pre-process texels.
			 * Check that the source red_blue_swap and reverse_order flags match those in the
			 * texture object. If they don't, create a new array and reorder the components. */
			if (needs_rbswap || needs_invert)
			{
				/* Copy the texels to a mutable array */
				cctexels = _mali_sys_malloc( width * height * texel_size );
				if(NULL == cctexels)
				{
					_mali_surface_unmap(surface);
					_mali_surface_access_unlock(surface);
					return MALI_ERR_OUT_OF_MEMORY;
				}
				_mali_sys_memcpy( cctexels, texels, width * height * texel_size );

				_swap_color_components( cctexels, needs_rbswap, needs_invert, src_reverse_order,
						width, height, src_format.texel_format, pitch );

				/* Create a pointer to the array to process */
				texels_to_process = (const void *)cctexels;
			}
			else
			{
				/* Create a pointer to the array to process */
				texels_to_process = texels;
			}

			switch (sformat->texel_layout)
			{ 

				case M200_TEXTURE_ADDRESSING_MODE_LINEAR:
					{
						s32 j;
						xoffset *= texel_size;

						for (j=0; j<update_height; j++)
						{
							int dst_pitch = surface->format.width * texel_size;
							int src_pitch = pitch;

							u8       *dst = (u8 *)dst_ptr + (dst_pitch * (j + yoffset)) + xoffset;
							const u8 *src = (const u8 *)texels_to_process  + (src_pitch * j);

							_mali_sys_memcpy(dst, src, update_width * texel_size);
						}
					}
					break;

				case M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED:
					{
						int y;
						for (y = 0; y < update_height; ++y)
						{
							const u8 *src = (const u8 *)texels_to_process  + (pitch * y);
							_m200_texture_block_interleave_span(dst_ptr, surface->format.width, src,
									y + yoffset, xoffset, update_width, texel_size, elem_size);
						}
					}
					break;
				default:
					MALI_DEBUG_ASSERT( 0, ("invalid texel layout %d\n", sformat->texel_layout) );
			}
			if (NULL != cctexels) _mali_sys_free(cctexels);
		}

		_mali_surface_unmap(surface);
		_mali_surface_access_unlock(surface);

	} /* if NULL != texels && width*height > 0 */

	return MALI_ERR_NO_ERROR;
}

#if EXTENSION_COMPRESSED_ETC1_RGB8_TEXTURE_OES_ENABLE
MALI_CHECK_RESULT mali_err_code _gles_fb_compressed_texture_image_2d_etc( 
	struct gles_fb_texture_object *tex_obj,
	struct mali_surface* surface, 
	int width,
	int height,
	int image_size,
	const void *texels )
{
	struct mali_surface_specifier sformat;

	MALI_DEBUG_ASSERT_POINTER( tex_obj );

	/* ETC mipmap levels are not compatible with any other surface type, and can as such use any layout it please 
	 * as long as it is consistent. This means we use the layout matching the input data - no swizzle required.  */
	_mali_surface_specifier_ex(&sformat, width, height, 0, MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_ETC, MALI_PIXEL_LAYOUT_INTERLEAVED_BLOCKS, M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED, MALI_FALSE, MALI_FALSE, MALI_SURFACE_COLORSPACE_sRGB, MALI_SURFACE_ALPHAFORMAT_NONPRE, MALI_FALSE );

	/* calculate the expected data size and check against the user-provided argument */
 
    MALI_DEBUG_CODE(
		{
			/* each dimension must be rounded up to the nearest full block size */
		 	int block_size = ((MALI_ALIGN(width,4)*MALI_ALIGN(height,4)))/2;
			MALI_DEBUG_ASSERT( image_size == block_size, ("illegal image size"));
		}
    ) /* MALI_DEBUG_CODE */

	/* upload data, if any */
	if ( (NULL != texels) && (width*height > 0) )
	{
			mali_err_code err = MALI_ERR_NO_ERROR;
			void *dst_ptr = NULL;
			int block_width = MALI_ALIGN(width,4);
			int block_height = MALI_ALIGN(height,4);
			int src_pitch = (block_width * __m200_texel_format_get_bpp(sformat.texel_format) + 7) / 8;
			int dst_pitch = (_gles_m200_get_texel_pitch(block_width, sformat.texel_layout) * __m200_texel_format_get_bpp( sformat.texel_format) + 7) / 8;
			mali_convert_request conv_request;
			struct mali_surface_specifier src_format;
			mali_convert_rectangle conv_rect;

			/* grab writable pointer. */
			_mali_surface_access_lock(surface);

			MALI_DEBUG_ASSERT(surface->datasize >= (u32) image_size, ("Image size should not have more data than the surface can hold"));

			dst_ptr = _mali_surface_map( surface, MALI_MEM_PTR_WRITABLE );
			if ( NULL == dst_ptr )
			{
				_mali_surface_access_unlock(surface);
				return  MALI_ERR_OUT_OF_MEMORY;
			}

			conv_rect.sx = 0;
			conv_rect.sy = 0;
			/* dx != 0 or dy != 0 not supported for ETC */
			conv_rect.dx = 0;
			conv_rect.dy = 0;
			conv_rect.width = width;
			conv_rect.height = height;
			_mali_surface_specifier_ex(&src_format, width, height, src_pitch, MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_ETC, MALI_PIXEL_LAYOUT_LINEAR, M200_TEXTURE_ADDRESSING_MODE_LINEAR, MALI_FALSE, MALI_FALSE, MALI_SURFACE_COLORSPACE_sRGB, MALI_SURFACE_ALPHAFORMAT_NONPRE, MALI_FALSE );
			_mali_convert_request_initialize( &conv_request, dst_ptr, dst_pitch, &surface->format, texels, src_pitch, &src_format, NULL, 0, &conv_rect, MALI_FALSE, MALI_TRUE, MALI_FALSE, MALI_CONVERT_SOURCE_OPENGLES );

			if ( MALI_FALSE == _mali_convert_texture( &conv_request  ) )
			{
				err = _m200_texture_swizzle( dst_ptr, sformat.texel_layout, 
					texels, M200_TEXTURE_ADDRESSING_MODE_LINEAR, 
					block_width, block_height,
					sformat.texel_format, 
					dst_pitch,
					src_pitch
				);
			}

			_mali_surface_unmap( surface );
			_mali_surface_access_unlock(surface);

			if(err != MALI_ERR_NO_ERROR) 
			{
				return err;
			}

	}
	return MALI_ERR_NO_ERROR;
}
#endif

#if EXTENSION_COMPRESSED_PALETTED_TEXTURES_OES_ENABLE
MALI_CHECK_RESULT mali_err_code _gles_fb_compressed_texture_image_2d_paletted(
	struct gles_fb_texture_object *tex_obj,
	struct mali_surface* surface,
	GLenum format,
	int width,
	int height,
	int miplevel_to_extract_from_texels,
	int image_size,
	const void *texels )
{
	u32                           user_palette_size;
	int                           block_dim;
	struct mali_surface_specifier sformat;
	int                           first_miplevel;
	int                           last_miplevel;
	int                           current_miplevel;
	int                           mip_width;
	int                           mip_height;
	int                           source_offset;
	int                           texels_per_byte;
	int                           bytes_per_texel;
	int                           palette_color_count = 0;
	mali_bool                     support_rbswap, support_revorder; 
	mali_bool                     src_rbswap, src_revorder; 

	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	block_dim = 1;
	user_palette_size = 0;

	/*  paletted textures use negative mipmap values */
	first_miplevel = 0;
	last_miplevel = MAX(0, miplevel_to_extract_from_texels);

	/* The format / type combinations used to get the texel format are taken from Table 3.11 of Section 3.7.4 of the OpenGL ES 1.1 spec */
	texels_per_byte = _gles_m200_get_input_texels_per_byte(GL_NONE, format);
	MALI_DEBUG_ASSERT(((2==texels_per_byte) || (1==texels_per_byte)), ("Texels per byte is not correct"));
	if(1==texels_per_byte) palette_color_count = 256;
	if(2==texels_per_byte) palette_color_count = 16;

	_gles_m200_get_storage_surface_format(&sformat, GL_NONE, format, width, height );

	bytes_per_texel = _gles_m200_get_input_bytes_per_texel(GL_NONE, format);
	user_palette_size = bytes_per_texel * palette_color_count;

	/* figure out if rbswap / revorder is needed 
	 * by default, the rbswap/revorder will be given by the 
	 * src defaults, which coincide with what is already
	 * present in the src format struct. */
	src_rbswap = sformat.red_blue_swap;
	src_revorder = sformat.reverse_order;
	__m200_texel_format_flag_support( sformat.texel_format, &support_rbswap, &support_revorder);	

	MALI_DEBUG_ASSERT( block_dim >= 1, ("Minimum block size must be at least 1") );

	/* calculate the expected data size and check against the user-provided argument */
	{
		u32 num_bytes = user_palette_size;
		mip_width = width;
		mip_height = height;

		if(width*height)
		{
			for ( current_miplevel = first_miplevel; current_miplevel <= last_miplevel; ++current_miplevel )
			{
				/* each dimension must be rounded up to the nearest full block size */
				int block_width = ((mip_width+block_dim-1)/block_dim)*block_dim;
				int block_height = ((mip_height+block_dim-1)/block_dim)*block_dim;

				/* implements the level padding */
				num_bytes += ( (block_width*block_height) + texels_per_byte - 1) / texels_per_byte;

				/* calculate size of next mipmap level. Clamp to 1 to support rectangular textures */
				mip_width = MAX(1, mip_width/2);
				mip_height = MAX(1, mip_height/2);
			}
		}

		MALI_DEBUG_ASSERT ( (u32)image_size >= num_bytes, ("Not enough data") );
	}

	mip_width = width;
	mip_height = height;
	source_offset = user_palette_size;

	for ( current_miplevel = first_miplevel; current_miplevel <= last_miplevel; ++current_miplevel )
	{
		/* we only want to return the i'th miplevel, skip all the others */
		if(current_miplevel != miplevel_to_extract_from_texels) 
		{
			/* calculate size of next mipmap level. Clamp to 1 to support rectangular textures */
			source_offset += (mip_width * mip_height) / texels_per_byte;
			mip_width = MAX(1, mip_width/2);
			mip_height = MAX(1, mip_height/2);
			continue; 
		}

		/* convert texture data supplied by user to non-paletted texture */
		if ( (NULL != texels) && (width*height > 0) )
		{
			/* upload data to the surface */
			if(MALI_FALSE == _gles_tex_to_nonpaletted_tex( texels, source_offset, format, mip_width, mip_height, src_rbswap, src_revorder, surface ))
			{
				return MALI_ERR_OUT_OF_MEMORY;
			}
		}

		return MALI_ERR_NO_ERROR;
	}

	MALI_DEBUG_ASSERT(0, ("This point should never be reached"));
	return MALI_ERR_OUT_OF_MEMORY;
}
#endif


MALI_CHECK_RESULT GLenum _gles_fb_compressed_texture_sub_image_2d(
	struct gles_fb_texture_object *tex_obj,
	GLint miplevel,
	GLint xoffset,
	GLint yoffset,
	GLsizei width,
	GLsizei height,
	GLenum format,
	GLsizei image_size,
	const GLvoid *texels)
{
	MALI_IGNORE( tex_obj );
	MALI_IGNORE( miplevel );
	MALI_IGNORE( xoffset );
	MALI_IGNORE( yoffset );
	MALI_IGNORE( width );
	MALI_IGNORE( height );
	MALI_IGNORE( image_size );
	MALI_IGNORE( texels );

#if EXTENSION_COMPRESSED_ETC1_RGB8_TEXTURE_OES_ENABLE
	/* ETC does not allow regions to be updated */
	if ( GL_ETC1_RGB8_OES == format ) return GL_INVALID_OPERATION;
#else
	MALI_IGNORE(format);
#endif

	/* no other compressed formats are supported */
	return GL_INVALID_ENUM;
}

#if EXTENSION_EGL_IMAGE_OES_ENABLE

MALI_CHECK_RESULT mali_bool _gles_fb_egl_image_texel_format_valid( m200_texel_format texel_format )
{
	u32 i;
	const m200_texel_format accepted_formats[] =
		{
			M200_TEXEL_FORMAT_L_8,
			M200_TEXEL_FORMAT_A_8,
			M200_TEXEL_FORMAT_I_8,
			M200_TEXEL_FORMAT_RGB_565,
			M200_TEXEL_FORMAT_ARGB_1555,
			M200_TEXEL_FORMAT_ARGB_4444,
			M200_TEXEL_FORMAT_AL_88,
#if !RGB_IS_XRGB
			M200_TEXEL_FORMAT_RGB_888,
#endif
			M200_TEXEL_FORMAT_ARGB_8888,
			M200_TEXEL_FORMAT_xRGB_8888,
		};

	for ( i = 0; i < MALI_ARRAY_SIZE(accepted_formats); i++ )
	{
		if ( texel_format == accepted_formats[i] )
		{
			return MALI_TRUE;
		}
	}

	return MALI_FALSE;
}

#endif 

void _gles_fb_texture_object_assign(
                                  struct gles_fb_texture_object *tex_obj,
                                  int mipchain, int miplevel, unsigned int planes_count,
                                  struct mali_surface **surfaces)
{
	unsigned int plane;

	/* DESIGN INTENT: Caller of this function triggers deep copies as needed.
	 * This function simply forwards the assigns to the right texmem block,
	 * and updates the "need constraint resolve" flag as needed. */
	

	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	if(surfaces && miplevel == 0) 
	{
		tex_obj->used_planes = planes_count;
	}
	MALI_DEBUG_ASSERT_LEQ(mipchain, GLES_MAX_MIPCHAINS - 1);
	MALI_DEBUG_ASSERT_LEQ(miplevel, GLES_MAX_MIPMAP_LEVELS - 1);

	for(plane = 0; plane < tex_obj->used_planes; plane++)
	{
		unsigned int sublevel = 0;
		struct gles_fb_texture_memory* texmem;
		mali_bool old_surface_existed;

		texmem = _gles_fb_texture_object_get_memblock(tex_obj, miplevel, plane, &sublevel);
		MALI_DEBUG_ASSERT_POINTER(texmem);
		old_surface_existed = (NULL != _gles_fb_texture_memory_get_surface(texmem, mipchain, sublevel));

		if(surfaces && surfaces[plane])
		{
			_gles_fb_texture_memory_assign(texmem, mipchain, sublevel, surfaces[plane]);

			/* after assign, check whether we need a constraint resolver */
			if(!_gles_fb_texture_memory_valid(texmem)) tex_obj->need_constraint_resolve = MALI_TRUE;
		
			if(!old_surface_existed) tex_obj->num_uploaded_surfaces++;

		} 
		else 
		{
			_gles_fb_texture_memory_unassign(texmem, mipchain, sublevel);
			if(old_surface_existed) tex_obj->num_uploaded_surfaces--;
		}
	}
}

MALI_CHECK_RESULT struct mali_surface* _gles_fb_surface_alloc(
                                 struct gles_fb_texture_object *tex_obj,
                                 u32 mipchain,
                                 u32 miplevel,
                                 struct mali_surface_specifier* sformat)
{
	unsigned int sublevel = 0;
	struct gles_fb_texture_memory* texmem;
	
	/* design intent: caller deals with when it is safe to allocate surface off a texmem block.
	 * This function simply forwards the allocate to the right texmem block */

	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_DEBUG_ASSERT_POINTER(sformat);
	MALI_DEBUG_ASSERT_LEQ(mipchain, GLES_MAX_MIPCHAINS - 1);
	MALI_DEBUG_ASSERT_LEQ(miplevel, GLES_MAX_MIPMAP_LEVELS - 1);

	/* always forward to plane 0 */
	texmem = _gles_fb_texture_object_get_memblock(tex_obj, miplevel, 0, &sublevel);
	return  _gles_fb_texture_memory_allocate(texmem, mipchain, sublevel, sformat);

}

mali_err_code _gles_texture_object_resolve_constraints(struct gles_texture_object *tex_obj)
{
	unsigned int level, plane;
	struct gles_fb_texture_object *fb_tex_obj;
	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	fb_tex_obj = tex_obj->internal;
	MALI_DEBUG_ASSERT_POINTER(fb_tex_obj);

	for(plane = 0; plane < fb_tex_obj->used_planes; plane++)
	{
		for(level = 0; level < MALI_TD_MIPLEVEL_POINTERS; level++)
		{
			mali_err_code err = _gles_fb_texture_memory_resolve(&fb_tex_obj->texmem[level][plane]);
			if(err != MALI_ERR_NO_ERROR) return err;
			_gles_m200_td_level_change( tex_obj, level );
		}
	}
	fb_tex_obj->need_constraint_resolve = MALI_FALSE;
	return MALI_ERR_NO_ERROR;
}


struct mali_surface* _gles_fb_texture_object_get_mali_surface(struct gles_fb_texture_object *tex_obj, u16 chain_index, u16 mipmap_level)
{
	unsigned int sublevel = 0;
	struct gles_fb_texture_memory* texmem;
	
	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_DEBUG_ASSERT_LEQ(chain_index, GLES_MAX_MIPCHAINS - 1);
	MALI_DEBUG_ASSERT_LEQ(mipmap_level, GLES_MAX_MIPMAP_LEVELS - 1);
	
	/* fetch from plane 0 */
	texmem = _gles_fb_texture_object_get_memblock(tex_obj, mipmap_level, 0, &sublevel);
	return _gles_fb_texture_memory_get_surface(texmem, chain_index, sublevel);
}


struct mali_surface* _gles_fb_texture_object_get_mali_surface_at_plane(struct gles_fb_texture_object *tex_obj, u16 chain_index, u16 mipmap_level, u16 mipmap_plane)
{
	unsigned int sublevel = 0;
	struct gles_fb_texture_memory* texmem;
	
	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_DEBUG_ASSERT_LEQ(chain_index, GLES_MAX_MIPCHAINS - 1);
	MALI_DEBUG_ASSERT_LEQ(mipmap_level, GLES_MAX_MIPMAP_LEVELS - 1);

	texmem = _gles_fb_texture_object_get_memblock(tex_obj, mipmap_level, mipmap_plane, &sublevel);
	return _gles_fb_texture_memory_get_surface(texmem, chain_index, sublevel);
}


mali_bool _gles_fb_texture_object_get_renderable( struct gles_fb_texture_object *tex_obj, u32 mipchain, u32 miplevel )
{
	unsigned int sublevel = 0;
	struct gles_fb_texture_memory* texmem;
	
	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_DEBUG_ASSERT_LEQ(mipchain, GLES_MAX_MIPCHAINS - 1);
	MALI_DEBUG_ASSERT_LEQ(miplevel, GLES_MAX_MIPMAP_LEVELS - 1);

	texmem = _gles_fb_texture_object_get_memblock(tex_obj, miplevel, 0, &sublevel);
	return _gles_fb_texture_memory_get_renderable(texmem);
}

void _gles_fb_texture_object_set_renderable( struct gles_fb_texture_object *tex_obj, u32 mipchain, u32 miplevel )
{
	unsigned int sublevel = 0;
	struct gles_fb_texture_memory* texmem;
	
	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_DEBUG_ASSERT_LEQ(mipchain, GLES_MAX_MIPCHAINS - 1);
	MALI_DEBUG_ASSERT_LEQ(miplevel, GLES_MAX_MIPMAP_LEVELS - 1);

	texmem = _gles_fb_texture_object_get_memblock(tex_obj, miplevel, 0, &sublevel);
	_gles_fb_texture_memory_set_renderable(texmem);

	tex_obj->using_renderable_miplevels |= MALI_TRUE;
}


mali_bool _gles_fb_texture_setup_egl_image( struct gles_fb_texture_object *tex_obj, GLint mipchain, GLint miplevel,
		struct egl_image *image)
{
	mali_surface* surf; 
	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	MALI_DEBUG_ASSERT_POINTER( image );
	MALI_DEBUG_ASSERT_RANGE(mipchain, 0, GLES_MAX_MIPCHAINS - 1);
	MALI_DEBUG_ASSERT_RANGE(miplevel, 0, GLES_MAX_MIPMAP_LEVELS - 1);
	
	surf = _gles_fb_texture_object_get_mali_surface(tex_obj,mipchain,miplevel);
	if(surf == NULL) return MALI_FALSE;

	/* fill in data, and update reference counts */
	{
		/* increase refcount since the texture is now an EGLImage sibling */
		_mali_surface_addref( surf );

		image->image_mali = mali_image_create_from_surface( surf, tex_obj->base_ctx );
		if ( NULL == image->image_mali )
		{
			_mali_surface_deref( surf );
			return MALI_FALSE;
		}

	}

	return MALI_TRUE;
}
