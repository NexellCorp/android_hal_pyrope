/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef PIXMAP_HELPERS_H
#define PIXMAP_HELPERS_H

#include "suite.h"
#include "gl2_framework.h"
#include <EGL/mali_eglext.h>
#include <EGL/fbdev_window.h>

/* forward declaration */
struct fbdev_pixmap;

/**
 * Create a new EGL Pixmap with the requested width, height and bitdepths
 * @return a valid pixmap pointer, or NULL on failure
 */
struct fbdev_pixmap* create_pixmap(int width, int height, int red, int green, int blue, int alpha, int luminance);

/**
 * Create a new EGL Pixmap with the requested width, height and bitdepths using a UMP to allocate the pixmap data
 * @return a valid pixmap pointer, or NULL on failure
 */
struct fbdev_pixmap* create_pixmap_ump(int width, int height, int red, int green, int blue, int alpha, int luminance );

/**
 * Maps pixmap data and returns a pointer to the raw pixmap data
 * @param pixmap pointer to pixmap
 * @return pointer to raw pixmap data
 * @note call unmap_pixmap_data when done using the pointer
 */
u16* map_pixmap_data( fbdev_pixmap *pixmap );

/**
 * Unmaps pixmap data
 * @param pixmap pointer to pixmap
 */
void unmap_pixmap_data( fbdev_pixmap *pixmap );

/**
 * Frees a pixmap.
 * @note Any EGL Images created from this pixmap must be destroyed prior to this call
 */
void free_pixmap(fbdev_pixmap* pixmap);


#endif /* PIXMAP_HELPERS_H */
