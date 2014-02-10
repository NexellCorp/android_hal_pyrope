/*
 * $Copyright:
 * ----------------------------------------------------------------------------
 *  This confidential and proprietary software may be used only as authorized
 *  by a licensing agreement from ARM Limited.
 *          (C) COPYRIGHT 2005-2012 ARM Limited , ALL RIGHTS RESERVED
 *  The entire notice above must be reproduced on all authorized copies and
 *  copies may only be made to the extent permitted by a licensing agreement
 *  from ARM Limited.
 * ----------------------------------------------------------------------------
 * $
 */

#ifndef _MALI_TPI_EGL_FB_COMMON_H_
#define _MALI_TPI_EGL_FB_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/fb.h>

/**
 * @addtogroup tpi_egl
 * @{
 * @defgroup tpi_egl_internal Internals
 * @{
 */

mali_tpi_bool get_fbdev_dimensions(char *fb_path, int *width, int *height);

mali_tpi_uint32 bitfield_to_mask(struct fb_bitfield *bf);

mali_tpi_bool check_fbdev_pixel(
		char *fb_path,
		int window_x_pos,
		int window_y_pos,
		unsigned int window_width,
		unsigned int window_height,
		unsigned int pixel_x_pos,
		unsigned int pixel_y_pos,
		mali_tpi_egl_simple_color color,
		mali_tpi_bool *result);

#ifdef __cplusplus
}
#endif

/** @}
 *  @}
 */

#endif /* End (_MALI_TPI_EGL_FB_COMMON_H_) */
