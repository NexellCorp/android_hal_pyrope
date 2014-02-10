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

#ifndef _MALI_TPI_EGL_INTERNAL_H_
#define _MALI_TPI_EGL_INTERNAL_H_

/** @file mali_tpi_egl_internal.h
 * @brief Prototypes for the EGL-specific parts of the interface.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup tpi_egl
 * @{
 * @defgroup tpi_egl_internal Internals
 * @{
 */

/**
 * @brief Internal initialization function
 *
 * @return @c MALI_TPI_TRUE on success, @c MALI_TPI_FALSE on failure
 * @note This function need not be reentrant.
 */
mali_tpi_bool _mali_tpi_egl_init_internal( void );

/* ------------------------------------------------------------------------- */
/**
 * @brief Internal shutdown function
 *
 * @return @c MALI_TPI_TRUE on success, @c MALI_TPI_FALSE on failure
 * @note This function need not be reentrant.
 */
mali_tpi_bool _mali_tpi_egl_shutdown_internal( void );

mali_tpi_bool check_pixel_within_window(
		int pixel_x_pos,
		int pixel_y_pos,
		int window_width,
		int window_height);

mali_tpi_bool check_pixel(
		void *visible_framebuffer_start,
		unsigned int bytes_per_pixel,
		int window_x_pos,
		int window_y_pos,
		unsigned int pixel_x_pos,
		unsigned int pixel_y_pos,
		unsigned int framebuffer_width,
		unsigned int framebuffer_height,
		mali_tpi_uint32 red_mask,
		mali_tpi_uint32 green_mask,
		mali_tpi_uint32 blue_mask,
		mali_tpi_uint32 alpha_mask,
		mali_tpi_egl_simple_color color);

#ifdef __cplusplus
}
#endif

/** @}
 *  @}
 */

#endif /* End (_MALI_TPI_EGL_INTERNAL_H_) */
