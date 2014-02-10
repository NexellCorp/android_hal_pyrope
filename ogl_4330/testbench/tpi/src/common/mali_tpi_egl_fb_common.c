/*
 * $Copyright:
 * ----------------------------------------------------------------------------
 *  This confidential and proprietary software may be used only as authorized
 *  by a licensing agreement from ARM Limited.
 *          (C) COPYRIGHT 2009-2012 ARM Limited , ALL RIGHTS RESERVED
 *  The entire notice above must be reproduced on all authorized copies and
 *  copies may only be made to the extent permitted by a licensing agreement
 *  from ARM Limited.
 * ----------------------------------------------------------------------------
 * $
 */

#include <tpi/mali_tpi.h>
#include <tpi/mali_tpi_egl.h>

#include <tpi/include/common/mali_tpi_egl_fb_common.h>
#include <linux/fb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>

static mali_tpi_bool get_fbinfo(int fd, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo)
{
	int res;
	mali_tpi_bool retval = MALI_TPI_TRUE;

	if (NULL != vinfo)
	{
		res = ioctl(fd, FBIOGET_VSCREENINFO, vinfo);
		if (-1 == res)
		{
			retval = MALI_TPI_FALSE;
			goto finish;
		}
	}

	if (NULL != finfo)
	{
		res = ioctl(fd, FBIOGET_FSCREENINFO, finfo);
		if (-1 == res)
		{
			retval = MALI_TPI_FALSE;
			goto finish;
		}
	}

finish:
	return retval;
}

mali_tpi_bool get_fbdev_dimensions(char *fb_path, int *width, int *height)
{
	mali_tpi_bool retval = MALI_TPI_TRUE;
	mali_tpi_bool tpi_res;
	int fd;
	struct fb_var_screeninfo vinfo;

	fd = open(fb_path, O_RDWR);
	if (-1 == fd)
	{
		retval = MALI_TPI_FALSE;
		goto finish;
	}

	tpi_res = get_fbinfo(fd, &vinfo, NULL);
	if (MALI_TPI_FALSE == tpi_res)
	{
		close(fd);
		retval = MALI_TPI_FALSE;
		goto finish;
	}

	*width = vinfo.xres;
	*height = vinfo.yres;

	close(fd);

finish:
	return retval;
}

mali_tpi_uint32 bitfield_to_mask(struct fb_bitfield *bf)
{
	mali_tpi_uint32 result;

	result = 1 << bf->length;
	result -= 1;
	result = result << bf->offset;

	return result;
}

mali_tpi_bool check_fbdev_pixel(
		char *fb_path,
		int window_x_pos,
		int window_y_pos,
		unsigned int window_width,
		unsigned int window_height,
		unsigned int pixel_x_pos,
		unsigned int pixel_y_pos,
		mali_tpi_egl_simple_color color,
		mali_tpi_bool *result)
{
	mali_tpi_bool retval = MALI_TPI_TRUE;
	int fd;
	int bytes_per_pixel;
	struct fb_var_screeninfo vinfo;
	unsigned char *buffer;
	unsigned char *visible_start;

	fd = open(fb_path, O_RDONLY);
	if (-1 == fd)
	{
		retval = MALI_TPI_FALSE;
		goto finish;
	}

	retval = get_fbinfo(fd, &vinfo, NULL);
	if (MALI_TPI_FALSE == retval)
	{
		goto finish;
	}

	if (window_width == 0 && window_height == 0)
	{
		window_width = vinfo.xres;
		window_height = vinfo.yres;
	}

	retval = check_pixel_within_window(pixel_x_pos, pixel_y_pos, window_width, window_height);
	if (retval == MALI_TPI_FALSE)
	{
		goto finish;
	}

	bytes_per_pixel = vinfo.bits_per_pixel >> 3;

	buffer = mmap(NULL, vinfo.xres * vinfo.yres_virtual * bytes_per_pixel, PROT_READ, MAP_SHARED, fd, 0);
	if (MAP_FAILED == buffer)
	{
		retval = MALI_TPI_FALSE;
		goto finish;
	}

	/* Get the top left of the area of the framebuffer currently being displayed */
	visible_start = buffer + (vinfo.yoffset * vinfo.xres * bytes_per_pixel);

	*result = check_pixel(
			visible_start,
			bytes_per_pixel,
			window_x_pos, window_y_pos,
			pixel_x_pos,
			pixel_y_pos,
			vinfo.xres, vinfo.yres,
			bitfield_to_mask(&vinfo.red),
			bitfield_to_mask(&vinfo.green),
			bitfield_to_mask(&vinfo.blue),
			bitfield_to_mask(&vinfo.transp),
			color);

	munmap(buffer, vinfo.xres * vinfo.yres_virtual * bytes_per_pixel);

finish:
	if (fd != -1)
	{
		close(fd);
	}

	return retval;
}
