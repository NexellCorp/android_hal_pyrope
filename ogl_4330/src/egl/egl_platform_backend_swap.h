/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005, 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_platform_backend_swap.h
 * @brief EGL platform abstraction layer for handling fbdev pan / flip / vsync.
 */

#ifndef _EGL_PLATFORM_BACKEND_SWAP_
#define _EGL_PLATFORM_BACKEND_SWAP_

int egl_platform_backend_swap(int fb_dev, void *var_info);

#endif
