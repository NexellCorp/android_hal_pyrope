/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_FB_SWAPPERS_H_
#define GLES_FB_SWAPPERS_H_

#include "../gles_base.h"

/*
 * @brief Swaps components in a 16bit pixel in RGB565 format.
 */
void _color_swap_rgb565( void *src, int width, int height, int padding );

/*
 * @brief Swaps components in a 16bit pixel in AL88 format.
 */
void _color_swap_al88( void *src, int width, int height, int padding );

/*
 * @brief Swaps components in a 16bit pixel in RGB888 format.
 */
void _color_swap_rgb888( void *src, int width, int height, int padding );

/*
 * @brief Swaps components in a 32bit pixel in ARGB8888 format.
 */
void _color_swap_argb8888( void *src, int width, int height, int padding, GLboolean is_src_inverted );
void _color_invert_argb8888( void *src, int width, int height, int padding );
void _color_swap_and_invert_argb8888( void *src, int width, int height, int padding, GLboolean is_src_inverted );

/*
 * @brief Swaps components in a 16bit pixel in ARGB4444 format.
 */
void _color_swap_argb4444( void *src, int width, int height, int padding, GLboolean is_src_inverted );
void _color_invert_argb4444( void *src, int width, int height, int padding );
void _color_swap_and_invert_argb4444( void *src, int width, int height, int padding, GLboolean is_src_inverted );

/*
 * @brief Swaps components in a 16bit pixel in ARGB1555 format.
 */
void _color_swap_argb1555( void *src, int width, int height, int padding, GLboolean is_src_inverted );
void _color_invert_argb1555( void *src, int width, int height, int padding, GLboolean is_src_inverted );
void _color_swap_and_invert_argb1555( void *src, int width, int height, int padding, GLboolean is_src_inverted );

/*
 * @brief Swaps components in a 32bit pixel in AL_16_16 format.
 */
void _color_swap_al_16_16( void *src, int width, int height, int padding );


/*
 * @brief Swaps components in a 64bit pixel in ARGB_16_16_16_16 format.
 */
void _color_swap_argb_16_16_16_16( void *src, int width, int height, int padding, GLboolean is_src_inverted );
void _color_invert_argb_16_16_16_16( void *src, int width, int height, int padding );
void _color_swap_and_invert_argb_16_16_16_16( void *src, int width, int height, int padding, GLboolean is_src_inverted );



#endif /* GLES_FB_SWAPPERS_H_ */
