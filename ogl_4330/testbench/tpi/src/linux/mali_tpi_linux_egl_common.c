/*
 * Copyright:
 * ----------------------------------------------------------------------------
 * This confidential and proprietary software may be used only as authorized
 * by a licensing agreement from ARM Limited.
 *      (C) COPYRIGHT 2009-2012 ARM Limited, ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorized copies and
 * copies may only be made to the extent permitted by a licensing agreement
 * from ARM Limited.
 * ----------------------------------------------------------------------------
 */

#define MALI_TPI_EGL_API MALI_TPI_EXPORT

#include <mali_tpi.h>
#include <mali_tpi_egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#define RGB565_RED   0xF800
#define RGB565_GREEN 0x07E0
#define RGB565_BLUE  0x001F

#define RGBA8888_RED   0xFF000000
#define RGBA8888_GREEN 0x00FF0000
#define RGBA8888_BLUE  0x0000FF00
#define RGBA8888_ALPHA 0x000000FF

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_init()
{
	return _mali_tpi_egl_init_internal();
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_shutdown()
{
	return _mali_tpi_egl_shutdown_internal();
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_get_invalid_native_display(EGLNativeDisplayType *native_display)
{
	*native_display = (EGLNativeDisplayType)1;

	return MALI_TPI_TRUE;
}

#if EGL_KHR_image
MALI_TPI_IMPL EGLint mali_tpi_egl_get_pixmap_type(void)
{
	return EGL_NATIVE_PIXMAP_KHR;
}
#endif /* EGL_KHR_image */

static mali_tpi_bool config_to_color_buffer_format(EGLDisplay display, EGLConfig config, egl_color_buffer_format *out)
{
	mali_tpi_bool retval = MALI_TPI_FALSE;
	EGLint red_size, green_size, blue_size, alpha_size;
	EGLBoolean res;

	res = eglGetConfigAttrib(display, config, EGL_RED_SIZE, &red_size);
	if (EGL_FALSE == res)
	{
		goto finish;
	}
	res = eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &green_size);
	if (EGL_FALSE == res)
	{
		goto finish;
	}
	
	res = eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &blue_size);
	if (EGL_FALSE == res)
	{
		goto finish;
	}
	
	res = eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &alpha_size);
	if (EGL_FALSE == res)
	{
		goto finish;
	}

	if ((5 == red_size) && (6 == green_size) && (5 == blue_size) && (0 == alpha_size))
	{
		*out = EGL_COLOR_BUFFER_FORMAT_RGB565;
	}
	else if ((5 == red_size) && (5 == green_size) && (5 == blue_size) && (1 == alpha_size))
	{
		*out = EGL_COLOR_BUFFER_FORMAT_RGBA5551;
	}
	else if ((4 == red_size) && (4 == green_size) && (4 == blue_size) && (4 == alpha_size))
	{
		*out = EGL_COLOR_BUFFER_FORMAT_RGBA4444;
	}
	else if ((8 == red_size) && (8 == green_size) && (8 == blue_size) && (8 == alpha_size))
	{
		*out = EGL_COLOR_BUFFER_FORMAT_RGBA8888;
	}
	else if ((8 == red_size) && (8 == green_size) && (8 == blue_size) && (0 == alpha_size))
	{
		*out = EGL_COLOR_BUFFER_FORMAT_RGB888;
	}
	else
	{
		goto finish;
	}

	retval = MALI_TPI_TRUE;

finish:
	return retval;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_create_pixmap(
		EGLNativeDisplayType native_display,
		int width, int height,
		EGLDisplay display,
		EGLConfig config,
		EGLNativePixmapType *pixmap)
{
	EGLBoolean res = EGL_FALSE;
	mali_tpi_bool tpi_res = MALI_TPI_FALSE;
	egl_color_buffer_format format;
	mali_tpi_bool retval = MALI_TPI_FALSE;
	egl_linux_pixmap *linux_pixmap = NULL;
	ump_result ump_open_result = UMP_ERROR;
	mali_size64 ump_size;
	EGLint buffer_size;
	const ump_alloc_flags ump_props = UMP_PROT_CPU_RD | UMP_PROT_CPU_WR;

	assert(pixmap != NULL);

	linux_pixmap = mali_tpi_alloc( sizeof(*linux_pixmap) );
	if( NULL == linux_pixmap )
	{
		goto finish;
	}

	linux_pixmap->width = width;
	linux_pixmap->height = height;

	tpi_res = config_to_color_buffer_format(display, config, &format);
	if (MALI_TPI_FALSE == tpi_res)
	{
		goto finish;
	}

	linux_pixmap->pixmap_format = (u32)format;

	res = eglGetConfigAttrib(display, config, EGL_BUFFER_SIZE, &buffer_size);
	if (EGL_FALSE == res)
	{
		goto finish;
	}

	ump_open_result = ump_open();
	if( UMP_OK != ump_open_result )
	{
		goto finish;
	}

	/* Buffer size is in bits, so make sure to divide by CHAR_BIT, rounding up, to get bytes. */
	linux_pixmap->planes[0].stride = ((mali_size64)buffer_size * width + CHAR_BIT - 1) / CHAR_BIT;
	linux_pixmap->planes[0].size = linux_pixmap->planes[0].stride * height;
	linux_pixmap->planes[0].offset = 0;
	linux_pixmap->planes[1].stride = 0;
	linux_pixmap->planes[1].size = 0;
	linux_pixmap->planes[1].offset = 0;
	linux_pixmap->planes[2].stride = 0;
	linux_pixmap->planes[2].size = 0;
	linux_pixmap->planes[2].offset = 0;

	ump_size = linux_pixmap->planes[0].size;
	linux_pixmap->handles[0] = ump_allocate_64( ump_size, ump_props );
	if( UMP_INVALID_MEMORY_HANDLE == linux_pixmap->handles[0] )
	{
		goto finish;
	}
	linux_pixmap->handles[1] = UMP_INVALID_MEMORY_HANDLE;
	linux_pixmap->handles[2] = UMP_INVALID_MEMORY_HANDLE;

	retval = MALI_TPI_TRUE;
	*pixmap = linux_pixmap;

finish:
	if ( MALI_TPI_FALSE == retval )
	{
		if ( UMP_INVALID_MEMORY_HANDLE != linux_pixmap->handles[0] )
		{
			ump_release(linux_pixmap->handles[0]);
		}

		if ( NULL != linux_pixmap )
		{
			mali_tpi_free(linux_pixmap);
		}

		if ( UMP_OK == ump_open_result )
		{
			ump_close();
		}
	}

	return retval;
}

static egl_color_buffer_format pixmap_format_to_color_buffer_format(mali_tpi_egl_pixmap_format tpi_format)
{
	static egl_color_buffer_format const table[] =
	{
		EGL_COLOR_BUFFER_FORMAT_YV12,
		EGL_COLOR_BUFFER_FORMAT_NV21,
		EGL_COLOR_BUFFER_FORMAT_YUYV
	};

	return table[tpi_format];
}

typedef struct format_info
{
	struct
	{
		size_t bytes_per_block;
		unsigned int log_pixels_per_block_x, log_pixels_per_block_y;
	}
	plane[EGL_COLOR_BUFFER_MAX_PLANES];
}
format_info;

static void get_format_info(mali_tpi_egl_pixmap_format format, format_info *info)
{
	static format_info const table[] =
	{
		/* YV12  */
		{{ {1,0,0}, {1,1,1}, {1,1,1} }},
		/* NV21  */
		{{ {1,0,0}, {2,1,1}, {0,0,0} }},
		/* YUYV  */
		{{ {4,1,0}, {0,0,0}, {0,0,0} }}
	};
	*info = table[format];
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_create_external_pixmap(
	EGLNativeDisplayType native_display,
	int width,
	int height,
	EGLDisplay display,
	mali_tpi_egl_pixmap_format format,
	EGLNativePixmapType *pixmap)
{
	mali_tpi_bool result = MALI_TPI_FALSE;
	egl_linux_pixmap *linux_pixmap = NULL;
	ump_result ump_open_result = UMP_ERROR;

	assert(pixmap != NULL);

	ump_open_result = ump_open();
	if (ump_open_result == UMP_OK)
	{
		linux_pixmap = mali_tpi_alloc(sizeof(*linux_pixmap));
		if (linux_pixmap != NULL)
		{
			egl_color_buffer_format const egl_format = pixmap_format_to_color_buffer_format(format);
			format_info info;
			unsigned int plane_index = 0;
			unsigned int width_quantum = 0, height_quantum = 0;
			size_t cumulative_size = 0;
			ump_alloc_flags const ump_props = UMP_PROT_CPU_RD | UMP_PROT_CPU_WR | \
					UMP_PROT_W_RD | UMP_PROT_W_WR |	\
					UMP_PROT_X_RD | UMP_PROT_X_WR |	\
					UMP_PROT_Y_RD | UMP_PROT_Y_WR |	\
					UMP_PROT_Z_RD | UMP_PROT_Z_WR;

			get_format_info(format, &info);
			for (plane_index = 0; plane_index < EGL_COLOR_BUFFER_MAX_PLANES; ++plane_index)
			{
				if (width_quantum < info.plane[plane_index].log_pixels_per_block_x)
				{
					width_quantum = info.plane[plane_index].log_pixels_per_block_x;
				}
				if (height_quantum < info.plane[plane_index].log_pixels_per_block_y)
				{
					height_quantum = info.plane[plane_index].log_pixels_per_block_y;
				}
			}
			assert(width_quantum < CHAR_BIT * sizeof(unsigned int));
			assert(height_quantum < CHAR_BIT * sizeof(unsigned int));
			/* Transform from logarithmic format, and then use them to round up width and height to the next multiple. */
			width_quantum = (1 << width_quantum) - 1;
			height_quantum = (1 << height_quantum) - 1;
			width = (width + width_quantum) & ~width_quantum;
			height = (height + height_quantum) & ~height_quantum;

			linux_pixmap->pixmap_format = (u32) egl_format;
			linux_pixmap->width = width;
			linux_pixmap->height = height;

			for (plane_index = 0; plane_index < NELEMS(info.plane); ++plane_index)
			{
				if (info.plane[plane_index].bytes_per_block == 0)
				{
					linux_pixmap->planes[plane_index].stride = 0;
					linux_pixmap->planes[plane_index].size = 0;
					linux_pixmap->planes[plane_index].offset = 0;
				}
				else
				{
					size_t stride;
					unsigned int const plane_width = width >> info.plane[plane_index].log_pixels_per_block_x;
					unsigned int const plane_height = height >> info.plane[plane_index].log_pixels_per_block_y;

					assert(plane_width != 0);
					assert(plane_height != 0);

					linux_pixmap->planes[plane_index].offset = cumulative_size;
					assert(info.plane[plane_index].bytes_per_block < SIZE_MAX / plane_width);
					stride = plane_width * info.plane[plane_index].bytes_per_block;
					linux_pixmap->planes[plane_index].stride = stride;

					assert(plane_height < SIZE_MAX / stride);
					linux_pixmap->planes[plane_index].size = plane_height * stride;
					assert(linux_pixmap->planes[plane_index].size < SIZE_MAX - cumulative_size);
					cumulative_size += linux_pixmap->planes[plane_index].size;
				}
			}

			/* Just use a single handle for now, as that's all the driver can cope with. */
			linux_pixmap->handles[0] = ump_allocate_64(cumulative_size, ump_props);
			linux_pixmap->handles[1] = UMP_INVALID_MEMORY_HANDLE;
			linux_pixmap->handles[2] = UMP_INVALID_MEMORY_HANDLE;

			if (linux_pixmap->handles[0] != UMP_INVALID_MEMORY_HANDLE)
			{
				/* Success! Everything is set up. */
				*pixmap = linux_pixmap;
				result = MALI_TPI_TRUE;
				/* Everything below here is cleanup code. */
			}
			else
			{
				mali_tpi_free(linux_pixmap);
			}
		}

		if (!result)
		{
			ump_close();
		}
	}

	return result;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_load_pixmap(
	EGLNativePixmapType pixmap,
	unsigned int num_planes,
	void const * const data[])
{
	mali_tpi_bool result = MALI_TPI_FALSE;
	egl_linux_pixmap *linux_pixmap = pixmap;
	unsigned int plane_index = 0;

	assert(linux_pixmap != NULL);
	if (num_planes <= NELEMS(linux_pixmap->planes))
	{
		void *dest = NULL;
		mali_size64 size64 = ump_size_get_64(linux_pixmap->handles[0]);
		size_t size = size64;
		assert(size64 == (mali_size64)size);

		dest = ump_map(linux_pixmap->handles[0], 0, size);

		if (dest)
		{
			mali_tpi_bool right_number_of_planes = MALI_TPI_TRUE;
			for (plane_index = 0; plane_index < NELEMS(linux_pixmap->planes); ++plane_index)
			{
				if (linux_pixmap->planes[plane_index].size != 0)
				{
					if (plane_index < num_planes)
					{
						assert(data[plane_index] != NULL);
						/* Our fake pixmap format has no gaps between lines, so we can do one copy instead of copying each
						 * line separately.
						 */
						memcpy(
							(char *)dest + linux_pixmap->planes[plane_index].offset,
							data[plane_index],
							linux_pixmap->planes[plane_index].size);
					}
					else
					{
						right_number_of_planes = MALI_TPI_FALSE;
					}
				}
				else if (plane_index < num_planes)
				{
					right_number_of_planes = MALI_TPI_FALSE;
				}
			}
			ump_cpu_msync_now(linux_pixmap->handles[0], UMP_MSYNC_CLEAN, dest, size);
			ump_unmap(linux_pixmap->handles[0], 0, size);
			if (right_number_of_planes)
			{
				result = MALI_TPI_TRUE;
			}
		}
	}
	return result;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_load_pixmap_from_file(
	EGLNativePixmapType pixmap,
	unsigned int num_planes,
	mali_tpi_file * const files[])
{
	mali_tpi_bool result = MALI_TPI_FALSE;
	egl_linux_pixmap *linux_pixmap = pixmap;
	unsigned int plane_index = 0;

	assert(linux_pixmap != NULL);
	if (num_planes <= NELEMS(linux_pixmap->planes))
	{
		void *dest = NULL;
		mali_size64 size64 = ump_size_get_64(linux_pixmap->handles[0]);
		size_t size = size64;
		assert(size64 == (mali_size64)size);

		dest = ump_map(linux_pixmap->handles[0], 0, size);

		if (dest)
		{
			mali_tpi_bool read_was_ok = MALI_TPI_TRUE;
			for (plane_index = 0; plane_index < NELEMS(linux_pixmap->planes); ++plane_index)
			{
				if (linux_pixmap->planes[plane_index].size != 0)
				{
					if (plane_index < num_planes)
					{
						size_t bytes_read;
						assert(files[plane_index] != NULL);

						/* Our fake pixmap format has no gaps between lines, so we can do one copy instead of copying each
						 * line separately.
						 */
						bytes_read = mali_tpi_fread(
							(char*)dest + linux_pixmap->planes[plane_index].offset,
							1,
							linux_pixmap->planes[plane_index].size,
							files[plane_index]);

						if (bytes_read < linux_pixmap->planes[plane_index].size)
						{
							read_was_ok = MALI_TPI_FALSE;
						}
					}
					else
					{
						read_was_ok = MALI_TPI_FALSE;
					}
				}
				else if (plane_index < num_planes)
				{
					read_was_ok = MALI_TPI_FALSE;
				}
			}
			ump_cpu_msync_now(linux_pixmap->handles[0], UMP_MSYNC_CLEAN, dest, size);
			ump_unmap(linux_pixmap->handles[0], 0, size);
			if (read_was_ok)
			{
				result = MALI_TPI_TRUE;
			}
		}
	}

	return result;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_destroy_pixmap(
		EGLNativeDisplayType native_display,
		EGLNativePixmapType pixmap)
{
	int i;
	egl_linux_pixmap *linux_pixmap = pixmap;

	assert(pixmap != NULL);
	for (i = 0; i < NELEMS(linux_pixmap->handles); ++i)
	{
		ump_release(linux_pixmap->handles[i]);
	}
	mali_tpi_free(pixmap);
	/* ump_open() and ump_close() is reference counted so calling ump_close() here won't destroy other pixmaps */
	ump_close();

	return MALI_TPI_TRUE;
}

static void fill_pixel_rgb565(unsigned char *current_pixel, mali_tpi_egl_simple_color color)
{
	mali_tpi_uint16 pixel = 0;

	/* red */
	if (0x8 & color)
	{
		pixel |= RGB565_RED;
	}
	/* green */
	if (0x4 & color)
	{
		pixel |= RGB565_GREEN;
	}
	/* blue */
	if (0x2 & color)
	{
		pixel |= RGB565_BLUE;
	}

	memcpy(current_pixel, &pixel, 2);
}

static void fill_pixel_rgba8888(unsigned char *current_pixel, mali_tpi_egl_simple_color color)
{
	mali_tpi_uint32 pixel = 0;

	/* red */
	if (0x8 & color)
	{
		pixel |= RGBA8888_RED;
	}
	/* green */
	if (0x4 & color)
	{
		pixel |= RGBA8888_GREEN;
	}
	/* blue */
	if (0x2 & color)
	{
		pixel |= RGBA8888_BLUE;
	}
	/* aplha */
	if (0x1 & color)
	{
		pixel |= RGBA8888_ALPHA;
	}

	memcpy(current_pixel, &pixel, 4);
}

static void fill_area(void *buffer, EGLint top, EGLint left, EGLint bottom, EGLint right,
		EGLint stride, egl_color_buffer_format format, mali_tpi_egl_simple_color color)
{
	unsigned char *line_start = buffer;
	unsigned char *current_pixel;
	int i, j, bytes_per_pixel = 0;

	assert((EGL_COLOR_BUFFER_FORMAT_RGB565 == format) ||
	       (EGL_COLOR_BUFFER_FORMAT_RGBA8888 == format));

	if (EGL_COLOR_BUFFER_FORMAT_RGB565 == format)
	{
		bytes_per_pixel = 2;
	}
	else if (EGL_COLOR_BUFFER_FORMAT_RGBA8888 == format)
	{
		bytes_per_pixel = 4;
	}

	for (i = 0; i < (bottom - top); i++)
	{
		current_pixel = line_start;
		for (j = 0; j < (right - left); j++)
		{
			if (EGL_COLOR_BUFFER_FORMAT_RGB565 == format)
			{
				fill_pixel_rgb565(current_pixel, color);
				current_pixel += bytes_per_pixel;
			}
			else if (EGL_COLOR_BUFFER_FORMAT_RGBA8888 == format)
			{
				fill_pixel_rgba8888(current_pixel, color);
				current_pixel += bytes_per_pixel;
			}
		}

		line_start += stride;
	}
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_fill_pixmap(
		EGLNativeDisplayType native_display,
		EGLNativePixmapType pixmap,
		mali_tpi_egl_simple_color color,
		int to_fill)
{
	mali_tpi_bool retval = MALI_TPI_FALSE;
	EGLint width, height, stride;
	void *mapping = NULL;
	egl_linux_pixmap *linux_pixmap = pixmap;
	u64 ump_size = 0;
	egl_color_buffer_format format;

	assert(pixmap != NULL);

	width = linux_pixmap->width;
	height = linux_pixmap->height;
	stride = linux_pixmap->planes[0].stride;
	format = (egl_color_buffer_format)linux_pixmap->pixmap_format;

	assert((EGL_COLOR_BUFFER_FORMAT_RGB565 == format) ||
		(EGL_COLOR_BUFFER_FORMAT_RGBA8888 == format));

	ump_size = ump_size_get_64(linux_pixmap->handles[0]);
	if ( 0 == ump_size)
	{
		goto finish;
	}

	mapping = ump_map(linux_pixmap->handles[0], 0, (size_t)ump_size);
	if ( NULL == mapping )
	{
		goto finish;
	}

	if (MALI_TPI_FALSE != (MALI_TPI_EGL_TOP_LEFT & to_fill))
	{
		fill_area(mapping, 0, 0, height/2, width/2, stride, format, color);
	}
	if (MALI_TPI_FALSE != (MALI_TPI_EGL_TOP_RIGHT & to_fill))
	{
		fill_area(mapping, 0, width/2, height/2, width, stride, format, color);
	}
	if (MALI_TPI_FALSE != (MALI_TPI_EGL_BOTTOM_LEFT & to_fill))
	{
		fill_area(mapping, height/2, 0, height, width/2, stride, format, color);
	}
	if (MALI_TPI_FALSE != (MALI_TPI_EGL_BOTTOM_RIGHT & to_fill))
	{
		fill_area(mapping, height/2, width/2, height, width, stride, format, color);
	}

	ump_cpu_msync_now(linux_pixmap->handles[0], UMP_MSYNC_CLEAN, mapping, (size_t)ump_size);
	ump_unmap(linux_pixmap->handles[0], mapping, (size_t)ump_size);

	retval = MALI_TPI_TRUE;
finish:
	if ( MALI_TPI_FALSE == retval )
	{
		if ( NULL != mapping )
		{
			ump_unmap(linux_pixmap->handles[0], mapping, (size_t)ump_size);
		}
	}
	return retval;
}

static mali_tpi_bool check_pixel_rgb565(unsigned char *buffer, int x, int y, EGLint stride,
		mali_tpi_egl_simple_color color)
{
	unsigned char *address;
	mali_tpi_uint16 pixel;
	mali_tpi_bool retval = MALI_TPI_TRUE;

	address = buffer + (y * stride) + (x * 2 /* bytes per pixel */);

	memcpy(&pixel, address, 2);

	/* red */
	if (0x8 & color)
	{
		retval = retval && ((RGB565_RED & pixel) == (RGB565_RED));
	}
	else
	{
		retval = retval && (!(RGB565_RED & pixel));
	}

	/* green */
	if (0x4 & color)
	{
		retval = retval && ((RGB565_GREEN & pixel) == (RGB565_GREEN));
	}
	else
	{
		retval = retval && (!(RGB565_GREEN & pixel));
	}

	/* blue */
	if (0x2 & color)
	{
		retval = retval && ((RGB565_BLUE & pixel) == (RGB565_BLUE));
	}
	else
	{
		retval = retval && (!(RGB565_BLUE & pixel));
	}

	return retval;
}

static mali_tpi_bool check_pixel_rgba8888(unsigned char *buffer, int x, int y, EGLint stride,
		mali_tpi_egl_simple_color color)
{
	unsigned char *address;
	mali_tpi_uint32 pixel;
	mali_tpi_bool retval = MALI_TPI_TRUE;

	address = buffer + (y * stride) + (x * 4 /* bytes per pixel */);

	memcpy(&pixel, address, 4);

	/* red */
	if (0x8 & color)
	{
		retval = retval && ((RGBA8888_RED & pixel) == (RGBA8888_RED));
	}
	else
	{
		retval = retval && (!(RGBA8888_RED & pixel));
	}

	/* green */
	if (0x4 & color)
	{
		retval = retval && ((RGBA8888_GREEN & pixel) == (RGBA8888_GREEN));
	}
	else
	{
		retval = retval && (!(RGBA8888_GREEN & pixel));
	}

	/* blue */
	if (0x2 & color)
	{
		retval = retval && ((RGBA8888_BLUE & pixel) == (RGBA8888_BLUE));
	}
	else
	{
		retval = retval && (!(RGBA8888_BLUE & pixel));
	}

	/* alpha */
	if (0x1 & color)
	{
		retval = retval && ((RGBA8888_ALPHA & pixel) == (RGBA8888_ALPHA));
	}
	else
	{
		retval = retval && (!(RGBA8888_ALPHA & pixel));
	}

	return retval;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_check_pixmap_pixel(
		EGLNativeDisplayType native_display,
		EGLNativePixmapType pixmap,
		int x, int y,
		mali_tpi_egl_simple_color color,
		mali_tpi_bool *result)
{
	mali_tpi_bool retval = MALI_TPI_FALSE;
	EGLint width, height, stride;
	void *mapping = NULL;
	egl_linux_pixmap *linux_pixmap = pixmap;
	u64 ump_size = 0;
	egl_color_buffer_format format;

	assert(pixmap != NULL);

	width = linux_pixmap->width;
	height = linux_pixmap->height;
	stride = linux_pixmap->planes[0].stride;
	format = (egl_color_buffer_format)linux_pixmap->pixmap_format;

	assert((EGL_COLOR_BUFFER_FORMAT_RGB565 == format) ||
		(EGL_COLOR_BUFFER_FORMAT_RGBA8888 == format));

	if ((x >= width) || (x < 0) || (y >= height) || (y < 0))
	{
		goto finish;
	}

	ump_size = ump_size_get_64(linux_pixmap->handles[0]);
	if ( 0 == ump_size)
	{
		goto finish;
	}

	mapping = ump_map(linux_pixmap->handles[0], 0, (size_t)ump_size);
	if ( NULL == mapping )
	{
		goto finish;
	}

	if (EGL_COLOR_BUFFER_FORMAT_RGB565 == format)
	{
		*result = check_pixel_rgb565(mapping, x, y, stride, color);
	}
	else if (EGL_COLOR_BUFFER_FORMAT_RGBA8888 == format)
	{
		*result = check_pixel_rgba8888(mapping, x, y, stride, color);
	}

	ump_unmap(linux_pixmap->handles[0], mapping, (size_t)ump_size);

	retval = MALI_TPI_TRUE;

finish:
	if ( MALI_TPI_FALSE == retval )
	{
		if ( NULL != mapping )
		{
			ump_unmap(linux_pixmap->handles[0], mapping, (size_t)ump_size);
		}
	}
	return retval;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_create_window(
	EGLNativeDisplayType native_display,
	int width, int height,
	EGLDisplay display,
	EGLConfig config,
	EGLNativeWindowType *window )
{
	mali_tpi_bool retval = MALI_TPI_FALSE;
	mali_tpi_bool tpi_res;
	fbdev_window *win;
	int display_width, display_height;
	EGLBoolean egl_res;
	int config_format;
	int setenv_ret;
	char config_string[255] = "";

	assert( window != NULL );
	assert( display != EGL_NO_DISPLAY );

	tpi_res = _mali_tpi_egl_get_display_dimensions(native_display, &display_width, &display_height);
	if (MALI_TPI_FALSE == tpi_res)
	{
		goto finish;
	}

	/* Check the requested size isn't too large, zero or negative */
	if ((width  > display_width)  || (width  <= 0) ||
	    (height > display_height) || (height <= 0))
	{
		goto finish;
	}

	win = mali_tpi_alloc( sizeof(*win) );
	if( NULL == win )
	{
		goto finish;
	}

	retval = MALI_TPI_TRUE;

	win->width = width;
	win->height = height;

	/* Set the requested pixel format into the process environment */
	egl_res = eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &config_format);
	assert(EGL_TRUE == egl_res);

	sprintf(config_string, "0x%x", config_format);
	setenv_ret = setenv("MALI_EGL_TPI_FBDEV_WINDOW_FORMAT", config_string, 1);
	if( 0 != setenv_ret )
	{
		mali_tpi_free( win );
		retval = MALI_TPI_FALSE;
		goto finish;
	}

	*window = win;

finish:
	return retval;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_resize_window(
		EGLNativeDisplayType native_display,
		EGLNativeWindowType *window,
		int width, int height,
		EGLDisplay egl_display,
		EGLConfig config)
{
	mali_tpi_bool retval = MALI_TPI_FALSE;
	mali_tpi_bool tpi_res;
	fbdev_window *win = *window;
	int display_width, display_height;

	tpi_res = _mali_tpi_egl_get_display_dimensions(native_display, &display_width, &display_height);
	if (MALI_TPI_FALSE == tpi_res)
	{
		goto finish;
	}
	
	/* Check the requested size isn't too large, zero or negative */
	if ((width  > display_width)  || (width  <= 0) ||
	    (height > display_height) || (height <= 0))
	{
		goto finish;
	}

	if (NULL == win)
	{
		/* Resizing from fullscreen to non-fullscreen, need to alloc struct */
		win = mali_tpi_alloc(sizeof(*win));
		if (NULL == win)
		{
			goto finish;
		}
		*window = win;
	}

	retval = MALI_TPI_TRUE;
	win->width = width;
	win->height = height;

finish:
	return retval;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_create_fullscreen_window(
	EGLNativeDisplayType native_display,
	EGLDisplay display,
	EGLConfig config,
	EGLNativeWindowType *window )
{
	mali_tpi_bool tpi_res;
	mali_tpi_bool retval = MALI_TPI_FALSE;
	int display_width, display_height;
	
	assert(window != NULL);
	assert(display != EGL_NO_DISPLAY);

	tpi_res = _mali_tpi_egl_get_display_dimensions(native_display, &display_width, &display_height);
	if (MALI_TPI_FALSE == tpi_res)
	{
		goto finish;
	}

	retval = mali_tpi_egl_create_window(native_display, display_width, display_height, display, config, window);

finish:
	return retval;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_destroy_window(
		EGLNativeDisplayType native_display,
		EGLNativeWindowType window)
{
	if (NULL != window)
	{
		mali_tpi_free(window);
	}

	return MALI_TPI_TRUE;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_get_window_dimensions(
		EGLNativeDisplayType native_display,
		EGLNativeWindowType window,
		int *width, int *height)
{
	mali_tpi_bool retval = MALI_TPI_FALSE;
	mali_tpi_bool tpi_res;
	fbdev_window *win = window;

	if (NULL == win)
	{
		int display_width, display_height;
		tpi_res = _mali_tpi_egl_get_display_dimensions(native_display, &display_width, &display_height);
		if (MALI_TPI_FALSE == tpi_res)
		{
			goto finish;
		}
		*width = display_width;
		*height = display_height;
	}
	else
	{
		*width = win->width;
		*height = win->height;
	}

	retval = MALI_TPI_TRUE;

finish:
	return retval;
}
