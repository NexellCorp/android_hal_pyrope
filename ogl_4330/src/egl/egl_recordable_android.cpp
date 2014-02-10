/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_recordable_android.cpp
 * @brief EGL_ANDROID_recordable
 */
#include "egl_config_extension.h"
#include <math.h>

#if EGL_ANDROID_recordable_ENABLE
#include <EGL/egl.h>
#include "egl_platform.h"
#include "egl_mali.h"
#include "egl_surface.h"
#include "egl_pixmap_mapping_anative.h"
#include <mali_system.h>
#include <shared/mali_surface.h>

#if MALI_ANDROID_API >= 15
#include <system/window.h>
#else
#include <ui/egl/android_natives.h>
#include <ui/android_native_buffer.h>
#endif
#include <utils/Log.h>

#define COLOR_CONVERSION_FLOAT 0

#if MALI_ANDROID_API < 15
using namespace android;
#endif

mali_surface *__egl_platform_create_recordable_surface_from_native_buffer(void *buf, egl_surface *surface, void *ctx)
{
	android_native_buffer_t *buffer = (android_native_buffer_t *)buf;
	mali_base_ctx_handle base_ctx = ( mali_base_ctx_handle) ctx;
	mali_surface_specifier sformat;
	mali_surface *color_buffer = NULL;
	egl_android_pixel_format* format_mapping=NULL;
	u32 width = 0, height = 0;
	u32 bytes_per_pixel = 0;

	format_mapping = _egl_android_get_native_buffer_format( buffer );
	MALI_CHECK_NON_NULL(format_mapping, NULL);


	width = buffer->width;
	height = buffer->height;
	bytes_per_pixel = __m200_texel_format_get_size( format_mapping->egl_pixel_format );

	_mali_surface_specifier_ex(
			&sformat,
			width,
			height,
			buffer->stride*bytes_per_pixel,
			_mali_texel_to_pixel_format(format_mapping->egl_pixel_format),
			format_mapping->egl_pixel_format,
			MALI_PIXEL_LAYOUT_LINEAR,
			_mali_pixel_layout_to_texel_layout(MALI_PIXEL_LAYOUT_LINEAR),
			format_mapping->red_blue_swap,
			format_mapping->reverse_order,
			MALI_SURFACE_COLORSPACE_sRGB,
			MALI_SURFACE_ALPHAFORMAT_NONPRE,
			surface->config->alpha_size == 0? MALI_TRUE: MALI_FALSE );

	/*create a internal render surface*/
	color_buffer = _mali_surface_alloc( MALI_SURFACE_FLAGS_NONE, &sformat, 0, base_ctx );

	if ( NULL == color_buffer )
	{
		__android_log_print(ANDROID_LOG_ERROR, "[EGL-recordable]", "Failed to allocate empty mali surface for buffer (0x%x)", (int)buffer);
		return NULL;
	}

	return color_buffer;
}

EGLBoolean __egl_recordable_color_conversion_needed(void *win, egl_surface *surface)
{
	android_native_window_t *native_win = (android_native_window_t *)win;
	int win_format;
	EGLBoolean ret = EGL_FALSE;

	if ( surface->config->recordable_android)
	{
		native_win->query(native_win, NATIVE_WINDOW_FORMAT, &win_format );

		switch (win_format)
		{
			case HAL_PIXEL_FORMAT_YV12:
				ret = EGL_TRUE;
				break;
			default:
				break;
		}
	}
	return ret;
}

static void _do_convert_rgba8888_to_yv12( android_native_buffer_t *native_buf, mali_surface *render_target, void *yuv_base, void *rgb_base)
{
	uint8_t *yuvbuf = (uint8_t *)yuv_base;
	const int yuv_tex_offset_y = 0;
	int yuv_tex_stride_y = native_buf->stride;
	int yuv_tex_offset_v = yuv_tex_stride_y * native_buf->height;
	int yuv_tex_stride_v = (yuv_tex_stride_y/2 + 0xf) & ~0xf;
	int yuv_tex_offset_u = yuv_tex_offset_v + yuv_tex_stride_v * native_buf->height/2;
	int yuv_tex_stride_u = yuv_tex_stride_v;
	int x, y;
	float r_fract, b_fract, g_fract, y_fract, u_fract, v_fract;
	float Y,u,v;

	u32 pixel;
	u8 r,g,b;

   	for( x=0; x<native_buf->width; x++)
	{
		for( y=0; y<native_buf->height; y++)
		{
		    pixel = *(u32 *)((u32) rgb_base+4*x+(render_target->format.height-y-1)*render_target->format.pitch);
			r = (u8)((pixel & 0x00ff0000)>>16);
			g = (u8)((pixel & 0x0000ff00)>>8);
			b = (u8)( pixel & 0x000000ff);
#if COLOR_CONVERSION_FLOAT
			float wr=0.299, wb=0.114, wg=0.587; /*BT.601 recommended constants*/
			r_fract = r*1.0f/255;
			g_fract = g*1.0f/255;
			b_fract = b*1.0f/255;
			y_fract = wr * r_fract + wg * g_fract + wb * b_fract;
			u_fract = 0.5 * ((b_fract - y_fract) / (1.0 - wb)) + 0.5;
			v_fract = 0.5 * ((r_fract - y_fract) / (1.0 - wr)) + 0.5;
			if ((y_fract < 0.0) || (y_fract > 1.0)
					|| (u_fract < 0.0) || (u_fract > 1.0)
					|| (v_fract < 0.0) || (v_fract > 1.0)) {
				y_fract = 0.0;
				u_fract = v_fract = 0.5;
			}
			/*convert to fraction of full range*/
			Y = y_fract * (235 - 16) + 16;
			u = u_fract * (240 - 16) + 16;
			v = v_fract * (240 - 16) + 16;
			yuvbuf[yuv_tex_offset_y + (y * yuv_tex_stride_y) + x] = (uint8_t)rint(Y);
			yuvbuf[yuv_tex_offset_u + (y/2 * yuv_tex_stride_u) + (x/2)] = (uint8_t)rint(u);
			yuvbuf[yuv_tex_offset_v + (y/2 * yuv_tex_stride_v) + (x/2)] = (uint8_t)rint(v);
#else
			/*computer RGB input and 8-bit BT.601 YUV output.
			*/
			Y = ( (  66 * r + 129 * g +  25 * b + 128) >> 8) +  16;
			u = ( ( -38 * r -  74 * g + 112 * b + 128) >> 8) + 128;
			v = ( ( 112 * r -  94 * g -  18 * b + 128) >> 8) + 128;
			yuvbuf[yuv_tex_offset_y + (y * yuv_tex_stride_y) + x] = (uint8_t)(Y);
			yuvbuf[yuv_tex_offset_u + (y/2 * yuv_tex_stride_u) + (x/2)] = (uint8_t)(u);
			yuvbuf[yuv_tex_offset_v + (y/2 * yuv_tex_stride_v) + (x/2)] = (uint8_t)(v);
#endif

		}
	}
}

void __egl_platform_convert_color_space_to_native(void *buf, void *surface, void *yuv_base)
{
	void *rgb_addr;
	android_native_buffer_t *native_buf = (android_native_buffer_t *)buf;
	mali_surface *render_target = (mali_surface *)surface;

	_mali_surface_access_lock(render_target);

	rgb_addr = _mali_surface_map(render_target, MALI_MEM_PTR_READABLE);

	if(rgb_addr == NULL)
	{
		 __android_log_print(ANDROID_LOG_ERROR, "[EGL-recordable]", "Failed to map render target into readable memory %s", __func__ );
		_mali_surface_access_unlock( render_target);
		return ;
	}

	switch (native_buf->format)
	{
		case HAL_PIXEL_FORMAT_YV12:
			if( render_target->format.texel_format == M200_TEXEL_FORMAT_ARGB_8888 )
			{
				_do_convert_rgba8888_to_yv12(native_buf, render_target, yuv_base, rgb_addr);
			}
			break;
		default:
			break;
	}
	_mali_surface_unmap(render_target);
	_mali_surface_access_unlock( render_target);
}
#endif
