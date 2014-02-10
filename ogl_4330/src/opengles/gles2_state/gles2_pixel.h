/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles2_pixel.h
 * @brief Setting of pixel-storage alignments
 */
#ifndef _GLES2_PIXEL_H_
#define _GLES2_PIXEL_H_

#include <GLES2/mali_gl2.h>

struct gles_pixel;

/**
 * @brief Initializes the pixel-state
 * @param pixel The pointer to the pixel-state
 */
void _gles2_pixel_init( struct gles_pixel *pixel );

/**
 * @brief Set the value for pixel-storing
 * @param pixel The pointer to the pixel-state
 * @param pname The identifier for which parameter to alter
 * @param param The new value
 * @return An errorcode.
 * @note This is a wrapper function for the GLES entrypoint glPixelStore().
 */
GLenum _gles2_pixel_storei( struct gles_pixel *pixel, GLenum pname, GLint param ) MALI_CHECK_RESULT;

#endif	/* _GLES2_PIXEL_H_ */
