/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_m200_texel_format.h"
#include <shared/mali_surface_specifier.h>

struct enums_to_surface_format_conversion
{
	/* input values */
	GLenum type;
	GLenum format;

	/* input values. These represent the data ordering as how GLES mandates them. 
	 * These can contain "virtual" texel formats, but the driver will freak out if you try to render with them. */
	mali_pixel_format input_pformat;
	m200_texture_addressing_mode input_tformat;
	mali_bool input_rbswap;
	mali_bool input_revorder;

	GLint input_bytes_per_texel;
	GLint input_texels_per_byte; /* used by compressed formats only */

	/* storage values. These are how the format is supposed to be stored in memory.
	 * These can never contain "virtual" texel formats. */
	mali_pixel_format storage_pformat;
	m200_texture_addressing_mode storage_tformat;
	mali_bool storage_rbswap;
	mali_bool storage_revorder;

	GLint storage_bytes_per_texel;
	GLint storage_texels_per_byte; /* used by compressed formats only */


};

MALI_STATIC struct enums_to_surface_format_conversion enums_to_surface_format_conversion_table[] =
{
	/*
	 * type, format  ==>
	 * pixel format, texel format, red blue swap, reverse order, input bytes per texel, input texels per byte
	 */
	{
		GL_UNSIGNED_BYTE, GL_ALPHA, /* ==> */
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_A_8, MALI_FALSE, MALI_FALSE, 1, 0,
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_A_8, MALI_FALSE, MALI_FALSE, 1, 0
	},
	{
		GL_UNSIGNED_BYTE, GL_RGB, /* ==> */
#if RGB_IS_XRGB
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_VIRTUAL_RGB888, MALI_TRUE, MALI_FALSE, 3, 0,
		MALI_PIXEL_FORMAT_A8R8G8B8, M200_TEXEL_FORMAT_xRGB_8888, MALI_TRUE, MALI_FALSE, 4,0
#else
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_RGB_888, MALI_TRUE, MALI_FALSE, 3, 0,
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_RGB_888, MALI_TRUE, MALI_FALSE, 3, 0
#endif
	},
	{
		GL_UNSIGNED_BYTE, GL_RGBA, /* ==> */
		MALI_PIXEL_FORMAT_A8R8G8B8, M200_TEXEL_FORMAT_ARGB_8888, MALI_TRUE, MALI_FALSE, 4, 0,
		MALI_PIXEL_FORMAT_A8R8G8B8, M200_TEXEL_FORMAT_ARGB_8888, MALI_TRUE, MALI_FALSE, 4, 0
	},     /* RGBA8888 */
	{
		GL_UNSIGNED_BYTE, GL_LUMINANCE, /* ==> */
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_L_8, MALI_FALSE, MALI_FALSE, 1, 0,
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_L_8, MALI_FALSE, MALI_FALSE, 1, 0
	},
	{
		GL_UNSIGNED_BYTE, GL_LUMINANCE_ALPHA, /* ==> */
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_AL_88, MALI_FALSE, MALI_FALSE, 2, 0,
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_AL_88, MALI_FALSE, MALI_FALSE, 2, 0
	},
	{
		GL_UNSIGNED_SHORT_5_6_5, GL_RGB, /* ==> */
		MALI_PIXEL_FORMAT_R5G6B5, M200_TEXEL_FORMAT_RGB_565, MALI_FALSE, MALI_FALSE, 2, 0,
		MALI_PIXEL_FORMAT_R5G6B5, M200_TEXEL_FORMAT_RGB_565, MALI_FALSE, MALI_FALSE, 2, 0
	},    /* RGB565 */
	{
		GL_UNSIGNED_SHORT_5_5_5_1, GL_RGBA, /* ==> */
		MALI_PIXEL_FORMAT_A1R5G5B5, M200_TEXEL_FORMAT_ARGB_1555, MALI_TRUE, MALI_TRUE, 2, 0,
		MALI_PIXEL_FORMAT_A1R5G5B5, M200_TEXEL_FORMAT_ARGB_1555, MALI_TRUE, MALI_TRUE, 2, 0
	},     /* RGBA5551 */
	{
		GL_UNSIGNED_SHORT_4_4_4_4, GL_RGBA, /* ==> */
		MALI_PIXEL_FORMAT_A4R4G4B4, M200_TEXEL_FORMAT_ARGB_4444, MALI_TRUE, MALI_TRUE, 2, 0,
		MALI_PIXEL_FORMAT_A4R4G4B4, M200_TEXEL_FORMAT_ARGB_4444, MALI_TRUE, MALI_TRUE, 2, 0
	},     /* RGBA4444 */

	/* Two 16-bit formats: ARGB and LUMINANCE_ALPHA */
	{
		GL_UNSIGNED_SHORT, GL_RGBA, /* ==> */
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_ARGB_16_16_16_16, MALI_TRUE, MALI_FALSE, 8, 0,
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_ARGB_16_16_16_16, MALI_TRUE, MALI_FALSE, 8, 0
	},
	{
		GL_UNSIGNED_SHORT, GL_LUMINANCE_ALPHA, /* ==> */
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_AL_16_16, MALI_FALSE, MALI_FALSE, 4, 0,
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_AL_16_16, MALI_FALSE, MALI_FALSE, 4, 0
	},
#if EXTENSION_DEPTH_TEXTURE_ENABLE
	/* all depth textures are 16bit luminance textures under the hood */
	{
		GL_UNSIGNED_SHORT, GL_DEPTH_COMPONENT, /* ==> */
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_VIRTUAL_DEPTH16, MALI_FALSE, MALI_FALSE, 2, 0,
		MALI_PIXEL_FORMAT_Z16, M200_TEXEL_FORMAT_L_16, MALI_FALSE, MALI_FALSE, 2, 0
	},
	{
		GL_UNSIGNED_INT, GL_DEPTH_COMPONENT, /* ==> */
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_VIRTUAL_DEPTH32, MALI_FALSE, MALI_FALSE, 4, 0,
		MALI_PIXEL_FORMAT_Z16, M200_TEXEL_FORMAT_L_16, MALI_FALSE, MALI_FALSE, 2, 0
	},
#endif
#if EXTENSION_PACKED_DEPTH_STENCIL_ENABLE
	{
		GL_UNSIGNED_INT_24_8_OES, GL_DEPTH_STENCIL_OES, /* ==> */
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_VIRTUAL_STENCIL_DEPTH_8_24, MALI_FALSE, MALI_FALSE, 4, 0,
		MALI_PIXEL_FORMAT_S8Z24, M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8, MALI_FALSE, MALI_FALSE, 4, 0
	},
#endif
#if EXTENSION_BGRA8888_ENABLE
	{
		GL_UNSIGNED_BYTE, GL_BGRA_EXT, /* ==> */
		MALI_PIXEL_FORMAT_A8R8G8B8, M200_TEXEL_FORMAT_ARGB_8888, MALI_FALSE, MALI_FALSE, 4, 0,
		MALI_PIXEL_FORMAT_A8R8G8B8, M200_TEXEL_FORMAT_ARGB_8888, MALI_FALSE, MALI_FALSE, 4, 0
	},
#endif
#if EXTENSION_COMPRESSED_ETC1_RGB8_TEXTURE_OES_ENABLE
	{
		GL_NONE, GL_ETC1_RGB8_OES, /* ==> */
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_ETC, MALI_FALSE, MALI_FALSE, 0, 2,
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_ETC, MALI_FALSE, MALI_FALSE, 0, 2
		/* The number of bits that represent a 4x4 texel block is 64 bits */
		/* So the number of bytes per texel should be 0.5... */
	},
#endif
#if EXTENSION_COMPRESSED_PALETTED_TEXTURES_OES_ENABLE

	/* NOTE: All paletted textures are decompressed by the CPU before being uploaded to the driver. 
	 * The input formats describe the output of the GLES compression routine since there is really 
	 * no HW enums matching the input format directly. These are instead handled by a separate path
	 * in the driver: glCompressedTexImage2D (as opposed to glTexImage2D). 
	 *
	 * Also: All paletted texture formats are not writeable by the GPU (ie, pixel format = none).
	 * This is to fit the spec mentioning hat compressed textures are not GPU writable. */

	{
		GL_NONE, GL_PALETTE4_RGB8_OES,	/* ==> */
#if RGB_IS_XRGB
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_VIRTUAL_RGB888, MALI_TRUE, MALI_FALSE, 3, 2,
		MALI_PIXEL_FORMAT_A8R8G8B8, M200_TEXEL_FORMAT_xRGB_8888, MALI_TRUE, MALI_FALSE, 4, 0
#else
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_RGB_888, MALI_TRUE, MALI_FALSE, 3, 2,
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_RGB_888, MALI_TRUE, MALI_FALSE, 3, 0
#endif
	},
	{
		GL_NONE, GL_PALETTE4_RGBA8_OES,	/* ==> */
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_ARGB_8888, MALI_TRUE, MALI_FALSE, 4, 2,
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_ARGB_8888, MALI_TRUE, MALI_FALSE, 4, 0
	},
	{
		GL_NONE, GL_PALETTE4_R5_G6_B5_OES,	/* ==> */
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_RGB_565, MALI_FALSE, MALI_FALSE, 2, 2,
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_RGB_565, MALI_FALSE, MALI_FALSE, 2, 0
	},
	{
		GL_NONE, GL_PALETTE4_RGBA4_OES,	/* ==> */
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_ARGB_4444, MALI_TRUE, MALI_TRUE, 2, 2,
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_ARGB_4444, MALI_TRUE, MALI_TRUE, 2, 0 
	},
	{
		GL_NONE, GL_PALETTE4_RGB5_A1_OES,	/* ==> */
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_ARGB_1555, MALI_TRUE, MALI_TRUE, 2, 2,
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_ARGB_1555, MALI_TRUE, MALI_TRUE, 2, 0
	},
	{
		GL_NONE, GL_PALETTE8_RGB8_OES,	/* ==> */
#if RGB_IS_XRGB
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_VIRTUAL_RGB888, MALI_TRUE, MALI_FALSE, 3, 1,
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_xRGB_8888, MALI_TRUE, MALI_FALSE, 4, 0
#else
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_RGB_888, MALI_TRUE, MALI_FALSE, 3, 1,
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_RGB_888, MALI_TRUE, MALI_FALSE, 3, 0
#endif
	},
	{
		GL_NONE, GL_PALETTE8_RGBA8_OES,	/* ==> */
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_ARGB_8888, MALI_TRUE, MALI_FALSE, 4, 1,
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_ARGB_8888, MALI_TRUE, MALI_FALSE, 4, 0
	},
	{
		GL_NONE, GL_PALETTE8_R5_G6_B5_OES,	/* ==> */
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_RGB_565, MALI_FALSE, MALI_FALSE, 2, 1,
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_RGB_565, MALI_FALSE, MALI_FALSE, 2, 0
	},
	{
		GL_NONE, GL_PALETTE8_RGBA4_OES,	/* ==> */
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_ARGB_4444, MALI_TRUE, MALI_TRUE, 2, 1,
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_ARGB_4444, MALI_TRUE, MALI_TRUE, 2, 0
	},
	{
		GL_NONE, GL_PALETTE8_RGB5_A1_OES,	/* ==> */
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_ARGB_1555, MALI_TRUE, MALI_TRUE, 2, 1,
		MALI_PIXEL_FORMAT_NONE,	M200_TEXEL_FORMAT_ARGB_1555, MALI_TRUE, MALI_TRUE, 2, 0
	},
#endif
#if EXTENSION_TEXTURE_HALF_FLOAT_OES_ENABLE
	{
		GL_HALF_FLOAT_OES, GL_RGB, /* ==> */
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_RGB_FP16, MALI_TRUE, MALI_FALSE, 6, 0,
		MALI_PIXEL_FORMAT_NONE, M200_TEXEL_FORMAT_RGB_FP16, MALI_TRUE, MALI_FALSE, 6, 0
	},
	{
		GL_HALF_FLOAT_OES, GL_RGBA, /* ==> */
		MALI_PIXEL_FORMAT_ARGB_FP16, M200_TEXEL_FORMAT_ARGB_FP16, MALI_TRUE, MALI_FALSE, 8, 0,
		MALI_PIXEL_FORMAT_ARGB_FP16, M200_TEXEL_FORMAT_ARGB_FP16, MALI_TRUE, MALI_FALSE, 8, 0
	},     /* RGBA*fp16 */
#endif
};

MALI_STATIC int _gles_m200_index_of(GLenum type, GLenum format)
{
	int i;
	int n;

	n = MALI_ARRAY_SIZE(enums_to_surface_format_conversion_table);

	/* enumerate the table of texture formats until we find a matching key */
	for(i = 0; i < n ; i++)
	{
		if(enums_to_surface_format_conversion_table[i].type == type && enums_to_surface_format_conversion_table[i].format == format)
		{
			return i;
		}
	}
	return -1;
}

void _gles_m200_get_input_surface_format(struct mali_surface_specifier* sformat, GLenum type, GLenum format, u16 width, u16 height )
{
	int i;
	MALI_DEBUG_ASSERT_POINTER(sformat);

	i = _gles_m200_index_of(type, format);

	if(i<0)
	{
		_mali_surface_specifier( sformat, width, height, 0, MALI_PIXEL_FORMAT_NONE, MALI_PIXEL_LAYOUT_LINEAR, MALI_FALSE, MALI_FALSE ); /* default virtual format */
	}
	else
	{
		_mali_surface_specifier_ex( sformat, 
				width,
				height,
				(width*__m200_texel_format_get_bpp( enums_to_surface_format_conversion_table[i].input_tformat ))/8,	
				enums_to_surface_format_conversion_table[i].input_pformat,
				enums_to_surface_format_conversion_table[i].input_tformat,
				MALI_PIXEL_LAYOUT_LINEAR,
				M200_TEXTURE_ADDRESSING_MODE_LINEAR,
				enums_to_surface_format_conversion_table[i].input_rbswap,
				enums_to_surface_format_conversion_table[i].input_revorder,
				MALI_SURFACE_COLORSPACE_sRGB, 
				MALI_SURFACE_ALPHAFORMAT_NONPRE,
				MALI_FALSE
				);

	}
}

void _gles_m200_get_storage_surface_format(struct mali_surface_specifier* sformat, GLenum type, GLenum format, u16 width, u16 height )
{
	int i;
	MALI_DEBUG_ASSERT_POINTER(sformat);

	i = _gles_m200_index_of(type, format);

	if(i<0)
	{
		_mali_surface_specifier( sformat, width,height,0, MALI_PIXEL_FORMAT_NONE, MALI_PIXEL_LAYOUT_INTERLEAVED_BLOCKS, MALI_FALSE, MALI_FALSE ); /* default storage format */
	}
	else
	{

		_mali_surface_specifier_ex( sformat,
				width,
				height,
				(width*__m200_texel_format_get_bpp( enums_to_surface_format_conversion_table[i].input_tformat ))/8,
				enums_to_surface_format_conversion_table[i].storage_pformat,
				enums_to_surface_format_conversion_table[i].storage_tformat,
				MALI_PIXEL_LAYOUT_INTERLEAVED_BLOCKS,
				M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED,
				enums_to_surface_format_conversion_table[i].storage_rbswap,
				enums_to_surface_format_conversion_table[i].storage_revorder,
				MALI_SURFACE_COLORSPACE_sRGB, 
				MALI_SURFACE_ALPHAFORMAT_NONPRE,
				MALI_FALSE);
	}
}

void _gles_m200_get_gles_type_and_format(m200_texel_format internal_format, GLenum *type, GLenum *format)
{
	MALI_DEBUG_ASSERT_POINTER( type );
	MALI_DEBUG_ASSERT_POINTER( format );

	switch ( internal_format )
	{
	case M200_TEXEL_FORMAT_L_8:
		*type = GL_UNSIGNED_BYTE;
		*format = GL_LUMINANCE;
		break;
	case M200_TEXEL_FORMAT_A_8:
		*type = GL_UNSIGNED_BYTE;
		*format = GL_ALPHA;
		break;
	case M200_TEXEL_FORMAT_I_8:
		*type = GL_UNSIGNED_BYTE;
		*format = GL_ALPHA;
		break;
	case M200_TEXEL_FORMAT_RGB_565:
		*type = GL_UNSIGNED_SHORT_5_6_5;
		*format = GL_RGB;
		break;
	case M200_TEXEL_FORMAT_ARGB_1555:
		*type = GL_UNSIGNED_SHORT_5_5_5_1;
		*format = GL_RGBA;
		break;
	case M200_TEXEL_FORMAT_ARGB_4444:
		*type = GL_UNSIGNED_SHORT_4_4_4_4;
		*format = GL_RGBA;
		break;
	case M200_TEXEL_FORMAT_AL_88:
		*type = GL_UNSIGNED_BYTE;
		*format = GL_LUMINANCE_ALPHA;
		break;
#if !RGB_IS_XRGB
	case M200_TEXEL_FORMAT_RGB_888:
		*type = GL_UNSIGNED_BYTE;
		*format = GL_RGB;
		break;
#endif
	case M200_TEXEL_FORMAT_ARGB_8888:
		*type = GL_UNSIGNED_BYTE;
		*format = GL_RGBA;
		break;
	case M200_TEXEL_FORMAT_xRGB_8888:
		*type = GL_UNSIGNED_BYTE;
		*format = GL_RGB;
		break;
#if EXTENSION_TEXTURE_HALF_FLOAT_OES_ENABLE
	case M200_TEXEL_FORMAT_RGB_FP16:
		*type = GL_HALF_FLOAT_OES;
		*format = GL_RGB;
		break;
	case M200_TEXEL_FORMAT_ARGB_FP16:
		*type = GL_HALF_FLOAT_OES;
		*format = GL_RGBA;
		break;
#endif
	default:
		MALI_DEBUG_ASSERT( 0, ("Invalid internal format: %d", internal_format) );
	}
}

void _gles_m200_get_gles_input_flags(GLenum type, GLenum format, GLboolean *red_blue_swap, GLboolean *reverse_order)
{
	int i;
	MALI_DEBUG_ASSERT_POINTER( red_blue_swap );
	MALI_DEBUG_ASSERT_POINTER( reverse_order );

	i = _gles_m200_index_of(type, format);
	MALI_DEBUG_ASSERT( i>=0, ("Unsupported type (%d) and format (%d) combination", type, format) );

	*red_blue_swap = (GLboolean)enums_to_surface_format_conversion_table[i].input_rbswap;
	*reverse_order = (GLboolean)enums_to_surface_format_conversion_table[i].input_revorder;
}

GLint _gles_m200_get_input_bytes_per_texel(GLenum type, GLenum format)
{
	int i;

	i = _gles_m200_index_of(type, format);

	if(i>=0)
	{
		return enums_to_surface_format_conversion_table[i].input_bytes_per_texel;
	}

	return 0;
}

GLint _gles_m200_get_input_texels_per_byte(GLenum type, GLenum format)
{
	int i;

	i = _gles_m200_index_of(type, format);

	if(i>=0)
	{
		return enums_to_surface_format_conversion_table[i].input_texels_per_byte;
	}

	return 0;
}

