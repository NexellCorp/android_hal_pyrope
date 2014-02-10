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
 * @file gles_pixel.h
 * @brief Structures and functions for pixel-storage alignments used by both OpenGL ES APIs
 */
#ifndef _GLES_PIXEL_H_
#define _GLES_PIXEL_H_

/**
 * Structure that contains information on how to pack and unpack pixel data
 */
typedef struct gles_pixel
{
	GLint pack_alignment;	/**< Affects the packing of pixel data in memory */
	GLint unpack_alignment; /**< Affects the unpacking of pixel data from memory */
} gles_pixel;


#endif /* _GLES_PIXEL_H_ */
