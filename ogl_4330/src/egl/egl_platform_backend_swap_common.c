/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005, 2007-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <sys/ioctl.h>
#include <linux/fb.h>

int egl_platform_backend_swap(int fb_dev, void *var_info)
{
	if (ioctl(fb_dev, FBIOPAN_DISPLAY, (struct fb_var_screeninfo *)var_info) == -1)
	{
		return 0;
	}
	
	return 1;
}
