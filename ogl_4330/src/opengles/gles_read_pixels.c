/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */


#include "gles_context.h"

#include <mali_system.h>

#include <base/mali_debug.h>

#include "gles_state.h"
#include "gles_read_pixels.h"
#include "gles_util.h"
#include "gles_framebuffer_object.h"

#include "gles_instrumented.h"

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
#include <shared/m200_texture.h> /*Needed for the swizzle function */
#endif

#include <shared/m200_gp_frame_builder.h>
#include "gles_config_extension.h"
#include <shared/mali_surface.h>
#include "m200_backend/gles_m200_texel_format.h"

#if MALI_TIMELINE_PROFILING_ENABLED
#include <base/mali_profiling.h>
#endif

/** LUT used by code that read/write 16x16 block interleaved textures */
static const int block_interleave_lut[MALI200_TILE_SIZE * MALI200_TILE_SIZE] = 
{
       0,   1,   4,   5,  16,  17,  20,  21,  64,  65,  68,  69,  80,  81,  84,  85,
       3,   2,   7,   6,  19,  18,  23,  22,  67,  66,  71,  70,  83,  82,  87,  86,
       12,  13,   8,   9,  28,  29,  24,  25,  76,  77,  72,  73,  92,  93,  88,  89,
       15,  14,  11,  10,  31,  30,  27,  26,  79,  78,  75,  74,  95,  94,  91,  90,
       48,  49,  52,  53,  32,  33,  36,  37, 112, 113, 116, 117,  96,  97, 100, 101,
       51,  50,  55,  54,  35,  34,  39,  38, 115, 114, 119, 118,  99,  98, 103, 102,
       60,  61,  56,  57,  44,  45,  40,  41, 124, 125, 120, 121, 108, 109, 104, 105,
       63,  62,  59,  58,  47,  46,  43,  42, 127, 126, 123, 122, 111, 110, 107, 106,
       192, 193, 196, 197, 208, 209, 212, 213, 128, 129, 132, 133, 144, 145, 148, 149,
       195, 194, 199, 198, 211, 210, 215, 214, 131, 130, 135, 134, 147, 146, 151, 150,
       204, 205, 200, 201, 220, 221, 216, 217, 140, 141, 136, 137, 156, 157, 152, 153,
       207, 206, 203, 202, 223, 222, 219, 218, 143, 142, 139, 138, 159, 158, 155, 154,
       240, 241, 244, 245, 224, 225, 228, 229, 176, 177, 180, 181, 160, 161, 164, 165,
       243, 242, 247, 246, 227, 226, 231, 230, 179, 178, 183, 182, 163, 162, 167, 166,
       252, 253, 248, 249, 236, 237, 232, 233, 188, 189, 184, 185, 172, 173, 168, 169,
       255, 254, 251, 250, 239, 238, 235, 234, 191, 190, 187, 186, 175, 174, 171, 170
};


MALI_STATIC_INLINE unsigned int texture_byte_offset( int x, int y, 
                                                     int width, int height, 
                                                     int pitch, enum mali_pixel_layout layout, int bpp,
                                                     mali_bool buffer_flipped)
{

	if(buffer_flipped)
	{
		y = height - y - 1;
	}

	MALI_DEBUG_ASSERT(x >= 0 && x < width, ("x out of range"));
	MALI_DEBUG_ASSERT(y >= 0 && y < height, ("y out of range"));
	MALI_DEBUG_ASSERT(! (layout == MALI_PIXEL_LAYOUT_LINEAR && pitch == 0), ("Linear mode must have pitch > 0"));

	switch(layout)
	{
		case MALI_PIXEL_LAYOUT_LINEAR:
			return (x * bpp) + (y * pitch);
		case MALI_PIXEL_LAYOUT_INTERLEAVED_BLOCKS:
			{
				int lut_index; 
				int bytes_per_tile = bpp * MALI200_TILE_SIZE * MALI200_TILE_SIZE;
				int tile_x = x / MALI200_TILE_SIZE;
				int tile_y = y / MALI200_TILE_SIZE;
				int tiles_in_width = (width + MALI200_TILE_SIZE-1) / MALI200_TILE_SIZE;
				int target_tile_offset_in_bytes = (tile_y * tiles_in_width * bytes_per_tile) + (tile_x * bytes_per_tile);
				MALI_DEBUG_ASSERT(tile_x < tiles_in_width, ("Tile id too large"));
				x = x % MALI200_TILE_SIZE;
				y = y % MALI200_TILE_SIZE;
				lut_index = block_interleave_lut[x + y * MALI200_TILE_SIZE];
				return target_tile_offset_in_bytes + (lut_index*bpp);
		}
		default: 
			MALI_DEBUG_ASSERT(0, ("Pixel layout should be linear or blockinterleaved"));
			return 0;
	}
}


void _gles_calculate_conversion_rect(
		s32 sx, s32 sy,
		s32 dx, s32 dy,
		s32 render_width, s32 render_height,
		s32 src_width, s32 src_height,
		s32 dst_width, s32 dst_height,
		mali_convert_rectangle* convert_rect)
{
	MALI_DEBUG_ASSERT_POINTER(convert_rect);
	{
		s32 convert_rect_height = 0;
		s32 convert_rect_width	= 0;

		/*
 			Adjust negative offsets or offsets outside the surfaces
		*/
		
		if( sx < 0 )
		{
			dx -= sx;
			sx = 0;
		}
		else
		if( sx > src_width )
		{
			sx = src_width;
		}

		if( sy < 0 )
		{
			dy -= sy;
			sy = 0;
		}
		else
		if( sy > src_height)
		{
			sy = src_height;
		}

		if( dy < 0 )
		{
			dy = 0;
		}
		else
		if( dy > dst_height )
		{
			dy = dst_height;
		}

		if( dx < 0 )
		{
			dx = 0;
		}
		else
		if( dx > dst_width )
		{
			dx = dst_width;
		}

		MALI_DEBUG_ASSERT( dx>-1 && sx > -1  , (" dx>-1 && sx > -1 "));

		/*
		 	Set up the convert rect's width and height making sure it's completely inside the surfaces		
		*/
		
		{
			const s32 x_src_diff =  (src_width  - sx );
			const s32 x_dst_diff =  (dst_width  - dx );
			const s32 y_src_diff =  (src_height - sy );
			const s32 y_dst_diff =  (dst_height - dy );
			MALI_DEBUG_ASSERT(x_src_diff >= -1 , ("x_src_diff >= -1 "));
			MALI_DEBUG_ASSERT(x_dst_diff >= -1 , ("x_dst_diff >= -1 "));
			MALI_DEBUG_ASSERT(y_src_diff >= -1 , ("y_src_diff >= -1 "));
			MALI_DEBUG_ASSERT(y_dst_diff >= -1 , ("y_dst_diff >= -1"));
			convert_rect_width  = MIN( x_src_diff, x_dst_diff );
			convert_rect_height = MIN( y_src_diff, y_dst_diff );
			convert_rect_width  = MIN( convert_rect_width, render_width);
 			convert_rect_height  = MIN( convert_rect_height, render_height);
		}

		convert_rect->sx 		=  sx;
		convert_rect->sy 		=  sy;
		convert_rect->dx 		=  dx;
		convert_rect->dy 		=  dy;
		convert_rect->height		=  convert_rect_height;
		convert_rect->width		=  convert_rect_width;
	}

	MALI_DEBUG_ASSERT( convert_rect->sx + convert_rect->width  <= (u32) src_width , ("invalid rect: convert_rect->sx + convert_rect->width"  ));
	MALI_DEBUG_ASSERT( convert_rect->sy + convert_rect->height <= (u32) src_height , ("invalid rect: convert_rect->sy + convert_rect->height <= (u32) src_height"));
	MALI_DEBUG_ASSERT( convert_rect->dx + convert_rect->width  <= (u32) dst_width , ("invalid rect: convert_rect->dx + convert_rect->width  <= (u32) dst_width"));
	MALI_DEBUG_ASSERT( convert_rect->dy + convert_rect->height <= (u32) dst_height , ("invalid rect: convert_rect->dy + convert_rect->height <= (u32) dst_height"));
}



GLenum _gles_read_pixels_internal( gles_context *ctx,
					int src_x_offset, int src_y_offset,
					int dst_x_offset, int dst_y_offset,
					int render_width, int render_height,
					const mali_surface_specifier* dst_format,
					void *dst_pixels )
{
	
	mali_frame_builder     *frame_builder;
	
	mali_surface           *src;
	void                   *src_pixels;
	mali_ds_consumer_handle temp_consumer;
	mali_err_code err = MALI_ERR_NO_ERROR;
	GLenum gerr = GL_NO_ERROR;
	mali_convert_request convert_request; 
	mali_convert_rectangle convert_rect;
	mali_bool convert_result;
	u32 src_bpp, dst_bpp;
	u32 i = 0;
	u32 src_usage;

	/* There are two mali surfaces in this function 
	 *
	 *  - src - is the screen output, which readpixel fetches the data from. The flush operation render to this one. 
	 *  - dst - is the function output, which readpixel writes to. The surface is accepted as a set of input parameters 
	 *          making out most fields in the surface. This way, this function don't need to worry about dst locking,
	 *          or dst being a real surface at all. 
	 *
	 */
	
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_INSTRUMENTED_LOG((_instrumented_get_context(), MALI_LOG_LEVEL_WARNING, "glReadPixels was called"));

	MALI_DEBUG_ASSERT(render_width <= dst_format->width, ("renderwidth must be <= dst_width, otherwise we won't fit the outputs"));
	MALI_DEBUG_ASSERT(render_height <= dst_format->height, ("renderheight must be <= dst_width, otherwise we won't fit the outputs"));

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	/* prepare current FBO for rendering */
	gerr = _gles_fbo_internal_draw_setup( ctx );
	if(gerr != GL_NO_ERROR) return gerr;	
#endif

	/* fetch the framebuilder to read from */
	frame_builder = _gles_get_current_read_frame_builder( ctx );
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	/* flush it, to initiate the hardware. This will add one write dependency */
	
 	err = _mali_frame_builder_flush( frame_builder );
	if( err != MALI_ERR_NO_ERROR) return GL_OUT_OF_MEMORY;
	
	/* fetch primary color output surface from the framebuilder 
	 * Keep looking until we have a set WB unit with a MALI_OUTPUT_COLOR */
	do {
		src_usage = 0 ;
		src = _mali_frame_builder_get_output( frame_builder, i, &src_usage );
		if(src_usage & MALI_OUTPUT_COLOR) break; /* break on the first color output */
	} while (++i < 3);
	
	if ( (NULL==src) || !(MALI_OUTPUT_COLOR & src_usage) )
	{
		/* we don't have a color target at all. This can happen if we read from an FBO with no color attachment */
		return GL_INVALID_OPERATION;
	}

	/* early out of readpixels if no pixels were written */
	if ( 0 == src->format.width || 0 == src->format.height )	return GL_NO_ERROR;

	/* 
	 * To read from the framebuilder safely, we need exclusive access to it. 
	 * The best way is to set up a READ dependency on the surface and lock it through the
	 * dependency system. 
	 */
	temp_consumer = mali_ds_consumer_allocate(ctx->base_ctx, ctx, NULL, NULL );
	if(MALI_NO_HANDLE == temp_consumer) return GL_OUT_OF_MEMORY;

	/* set up a dependency. */ 
	err = mali_ds_connect(temp_consumer, src->ds_resource, MALI_DS_READ); 
	if(MALI_ERR_NO_ERROR != err)
	{
		/* kill all references, free temp consumer. */
		mali_ds_consumer_release_all_connections( temp_consumer );
		mali_ds_consumer_free(temp_consumer);
		return GL_OUT_OF_MEMORY;
	}

	/* flush the temp consumer. This will also flush all other dependencies pending.
	 * This function will return when all pending operations are done, and thus acts as a wait as well. */
	err = mali_ds_consumer_flush_and_wait( temp_consumer , VR_PROFILING_EVENT_REASON_SUSPEND_RESUME_SW_GLES_WAIT_READPIXELS );
	if(MALI_ERR_NO_ERROR != err)
	{
		/* kill all references, free temp consumer. */
		mali_ds_consumer_release_all_connections( temp_consumer );
		mali_ds_consumer_free(temp_consumer);
		return GL_OUT_OF_MEMORY;
	}

	_gles_check_for_rendering_errors( ctx );

	/* all that remains as this point is to copy / convert the data from the rendertarget onto the output surface */
	/* locking surface to be able to map it */ 
	_mali_surface_access_lock(src); /* take the access lock; we're rendering to this surface */
	src_pixels = _mali_surface_map(src, MALI_MEM_PTR_READABLE);
	if( NULL == src_pixels )
	{
		_mali_surface_access_unlock(src);
		mali_ds_consumer_release_all_connections(temp_consumer);
		mali_ds_consumer_free(temp_consumer);
		return GL_OUT_OF_MEMORY;
	}



	/* Copy all pixels in the subwindow of the SRC surface into the DST surface.
	 * 
	 * The function will read dst_<width,height> pixels from src_offset_<x,y> and write them to 
	 * offset 0 in dst. 
	 *
	 * Examples: 
	 *    Assuming src=128x128
	 *    x=0,y=0,w=64,h=64 -> reading first 64x64 pixels from src, Result is a 64x64 image. 
	 *    x=10,y=10,w=64,h=64 -> reading x=10-72,y=10-72 from src. Result is a 64x64 image. 
	 *    x=-5,y=-5,w=64,h=64 -> reading x=0-59,y=0-59 from src. Result stored as a 64x64 image, with data in bottomright, rest undefined
	 *
	 *    Assuming src=32x42, 
	 *    x=0,y=0,w=64,h=64 -> reading first 32x42 pixels from src. Stored as a 64x64 image with data in topleft, rest is undefined
	 *    x=10,y=10,w=64,h=64 -> reading x=10-32,y=10-42 from src. Result is a 64x64 image with data in topleft, rest is undefined. 
	 *    x=-5,y=-5,w=64,h=64 -> reading x=0-32,y=0-42 from src. Result stored as a 64x64 image, with first 5 columns/rows undefined, then 32x42 data, rest undefined
	 *
	 **/

	{
		_gles_calculate_conversion_rect( src_x_offset , src_y_offset, dst_x_offset, dst_y_offset, render_width, render_height, 
						 src->format.width, src->format.height, dst_format->width, dst_format->height, & convert_rect  );
	}

	convert_result = MALI_FALSE;

	src_bpp = __mali_pixel_format_get_bpp(src->format.pixel_format) / 8;
	dst_bpp = __m200_texel_format_get_bpp(dst_format->texel_format) / 8;

	
	/* setup convert request */
	if( MALI_TRUE == ctx->state.common.framebuffer.current_object->read_flip_y ) 
	{
		const u32 src_byteoffset = texture_byte_offset(0, 0, src->format.width, src->format.height, src->format.pitch, src->format.pixel_layout, src_bpp, MALI_TRUE);
		const u32 dst_byteoffset = texture_byte_offset(0, 0, dst_format->width, dst_format->height, dst_format->pitch, dst_format->pixel_layout, dst_bpp, MALI_FALSE);
			
		u8* adjusted_src_pixels = (((u8*)src_pixels)+src_byteoffset);
		u8* adjusted_dst_pixels = (((u8*)dst_pixels)+dst_byteoffset);

		_mali_convert_request_initialize( 	&convert_request, 
							adjusted_dst_pixels, dst_format->pitch, dst_format, 
							adjusted_src_pixels, - src->format.pitch, & src->format,
							NULL, 0, &convert_rect, MALI_FALSE, MALI_TRUE, MALI_FALSE,
							MALI_CONVERT_SOURCE_OPENGLES );
	}
	else
	{
		_mali_convert_request_initialize( 	&convert_request, 
							dst_pixels , dst_format->pitch, dst_format, 
							src_pixels , src->format.pitch, & src->format,
							NULL, 0, &convert_rect, MALI_FALSE, MALI_TRUE, MALI_FALSE,
							MALI_CONVERT_SOURCE_OPENGLES );
	}
	convert_result = _mali_convert_texture(&convert_request); /* copy all pixels */

	if( MALI_FALSE ==  convert_result )
	{
		MALI_DEBUG_ASSERT( 0, ("unsupported framebuffer format encountered in glReadPixels: %d\n", src->format.pixel_format ) );
	} /* end clip and copy data scope */


	_mali_surface_unmap( src );
	_mali_surface_access_unlock( src );
	mali_ds_consumer_release_all_connections(temp_consumer);
	mali_ds_consumer_free(temp_consumer);

	_mali_frame_builder_wait_all( frame_builder);

	return GL_NO_ERROR;
}

/* local declarations of the array holding all legal formats */
struct legal_formats
{
	GLenum format; 
	GLenum type;
};
	
GLenum _gles_read_pixels( gles_context *ctx, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels )
{
	struct mali_surface_specifier sformat;
	int pitch;
	int pack_alignment;
	u32 i;
	mali_bool matching_format = MALI_FALSE;
	mali_bool matching_type = MALI_FALSE;
	mali_bool matching_format_and_type = MALI_FALSE;

	const struct legal_formats legal_output_formats[] = 
	{
		{ GL_RGBA, GL_UNSIGNED_BYTE },                                                 /* glReadPixels always support this format */
		{ GLES_IMPLEMENTATION_COLOR_READ_FORMAT, GLES_IMPLEMENTATION_COLOR_READ_TYPE } /* implementation specific format/type, required by the spec */
	};

	MALI_DEBUG_ASSERT_POINTER(ctx);

	/* check for input errors */
	for(i = 0; i < MALI_ARRAY_SIZE(legal_output_formats); i++)
	{
		if(legal_output_formats[i].format == format) matching_format = MALI_TRUE;
		if(legal_output_formats[i].type == type) matching_type = MALI_TRUE;
		if(legal_output_formats[i].format == format && legal_output_formats[i].type == type) matching_format_and_type = MALI_TRUE;
	}
	if( MALI_FALSE == matching_format ) return GL_INVALID_ENUM;
	if( MALI_FALSE == matching_type ) return GL_INVALID_ENUM;
	if( MALI_FALSE == matching_format_and_type ) return GL_INVALID_OPERATION;

	if( ( width < 0 ) || ( height < 0 ) )                                     return GL_INVALID_VALUE;
	
	/* special checking */
	if( pixels == NULL )                                                      return GL_INVALID_OPERATION;
	if( width == 0 || height == 0 )                                           return GL_NO_ERROR; /* silent return, no operation done */

	/* find the surface specifier matching the input format/enum, using the same rules as with textures. */
	_gles_m200_get_input_surface_format(&sformat, type, format, width, height);

	/* pitch is given by the pack alignment */
	pack_alignment = ctx->state.common.pixel.pack_alignment;
	pitch = width * _gles_m200_get_input_bytes_per_texel(type, format);
	pitch = ((pitch + pack_alignment - 1) / pack_alignment) * pack_alignment;

	_mali_frame_builder_acquire_output( ctx->state.common.framebuffer.current_object->draw_frame_builder );

	sformat.pitch=pitch;

	/* ensure pixels is aligned, if not do a memory copy pass.
	 * We do this copy pass because the internal functions require pixel alignment for 32bpp and 16bpp pixel outputs.
	 * glReadPixels can however support any pointer, which is not necessarily aligned. */
	if (
	   (__m200_texel_format_get_size(sformat.texel_format) == 4 && ((u32)pixels % 4 != 0)) || 
	   (__m200_texel_format_get_size(sformat.texel_format) == 2 && ((u32)pixels % 2 != 0))  
	   )
	{
		GLenum glerr;
		void* tempmem = _mali_sys_malloc(sformat.pitch * height); /* this memory is aligned */
		if(tempmem == NULL) return GL_OUT_OF_MEMORY;
		glerr = _gles_read_pixels_internal(ctx, x, y, 0, 0, width, height, &sformat, tempmem);
		if(glerr == GL_NO_ERROR) _mali_sys_memcpy(pixels, tempmem, sformat.pitch * height);
		_mali_sys_free(tempmem);
		return glerr;
	}

	/* no realignment required, output directly to pixels */
	return _gles_read_pixels_internal(ctx, x, y, 0, 0, width, height, &sformat, pixels);
}

#if EXTENSION_ROBUSTNESS_EXT_ENABLE
GLenum _gles_read_n_pixels_ext( gles_context *ctx, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, GLvoid *pixels )
{
	if( ctx->robust_strategy )
	{
		struct mali_surface_specifier sformat;

		_gles_m200_get_input_surface_format(&sformat, type, format, width, height);
		if( width * height * __m200_texel_format_get_size(sformat.texel_format) > bufSize )
		{
			return GL_INVALID_OPERATION;
		}
		return _gles_read_pixels( ctx, x, y, width, height, format, type, pixels);
	}

	return GL_INVALID_OPERATION;
}
#endif
