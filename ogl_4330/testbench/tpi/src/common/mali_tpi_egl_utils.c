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

#define MALI_TPI_EGL_API MALI_TPI_EXPORT
#include "tpi/mali_tpi.h"
#include "tpi/mali_tpi_egl.h"

#include <string.h>
#include <stddef.h>
#include <assert.h>

mali_tpi_bool _mali_tpi_egl_init_internal( void )
{
	/* No initialization currently necessary */
	return MALI_TPI_TRUE;
}

mali_tpi_bool _mali_tpi_egl_shutdown_internal( void )
{
	/* No shutdown currently necessary */
	return MALI_TPI_TRUE;
}

MALI_TPI_IMPL EGLBoolean mali_tpi_egl_choose_config(
	EGLDisplay    display,
	EGLint const *min_attribs,
	EGLint const *exact_attribs,
	EGLConfig *   configs,
	EGLint        config_size,
	EGLint *      num_config)
{
	EGLint i;
	EGLint matched;
	EGLint num_min_config;  /* Number of configs matching min_attribs */
	EGLConfig *min_configs; /* Configs matching min_attribs */
	static const EGLint dummy[] = { EGL_NONE };

	if( NULL == num_config )
	{
		/* For eglChooseConfig, this is an EGL_BAD_PARAMETER error.
		 * Create one ourselves.
		 */
		return eglChooseConfig(
			display, min_attribs, configs, config_size, num_config );
	}

	if( !eglChooseConfig( display, min_attribs, NULL, 0, &num_min_config ) )
	{
		return EGL_FALSE;
	}

	min_configs = mali_tpi_alloc( num_min_config * sizeof(EGLConfig) );
	if( NULL == min_configs )
	{
		/* Out of memory */
		return EGL_FALSE;
	}

	/* Obtain everything that matches min_attribs */
	if ( !eglChooseConfig(
		display, min_attribs, min_configs, num_min_config, &num_min_config) )
	{
		mali_tpi_free( min_configs );
		return EGL_FALSE;
	}

	/* Filter by exact_attribs, in-place so as not to
	 * alter the outputs if an error occurs later.
	 */
	if( NULL == exact_attribs )
	{
		exact_attribs = dummy;
	}

	matched = 0;
	for( i = 0; i < num_min_config; i++ )
	{
		EGLint j;
		EGLBoolean match = EGL_TRUE;
		for( j = 0; match && (exact_attribs[j] != EGL_NONE); j += 2 )
		{
			EGLint attrib_value;

			if( (exact_attribs[j + 1] == EGL_DONT_CARE) &&
				(exact_attribs[j]     != EGL_LEVEL) )
			{
				/* Skip don't-cares; but it's not valid value for EGL_LEVEL */
				continue;
			}

			if( !eglGetConfigAttrib(
				display, min_configs[i], exact_attribs[j], &attrib_value ) )
			{
				mali_tpi_free( min_configs );
				return EGL_FALSE;
			}

			if( attrib_value != exact_attribs[j + 1] )
			{
				match = EGL_FALSE;
			}
		}
		if( match )
		{
			min_configs[matched++] = min_configs[i];
		}
	}

	/* min_configs now holds the filtered results. Output them if necessary */
	if( NULL != configs )
	{
		if( config_size < matched )
		{
			/* This also applies the required clamping for the return value */
			matched = config_size;
		}
		memcpy( configs, min_configs, matched * sizeof(EGLConfig) );
	}

	*num_config = matched;
	mali_tpi_free( min_configs );
	return EGL_TRUE;
}

mali_tpi_bool check_pixel_within_window(
		int pixel_x_pos,
		int pixel_y_pos,
		int window_width,
		int window_height)
{
	return (!((pixel_x_pos >= window_width)  || (pixel_x_pos < 0) ||
	          (pixel_y_pos >= window_height) || (pixel_y_pos < 0)));
}

static mali_tpi_bool check_color_component(
		void *pixel_address,
		int bytes_per_pixel,
		mali_tpi_uint32 mask,
		mali_tpi_bool expected)
{
	mali_tpi_bool retval;
	mali_tpi_uint32 pixel, component;

	assert(bytes_per_pixel <= (int) sizeof(pixel));

	/* Make sure we don't get an unaligned access (MIDEGL-724?)*/
	memcpy(&pixel, pixel_address, bytes_per_pixel);

	component = pixel & mask;

	if (MALI_TPI_FALSE == expected)
	{
		if (0 == component)
		{
			retval = MALI_TPI_TRUE;
		}
		else
		{
			retval = MALI_TPI_FALSE;
		}
	}
	else
	{
		if (component == mask)
		{
			retval = MALI_TPI_TRUE;
		}
		else
		{
			retval = MALI_TPI_FALSE;
		}
	}

	return retval;
}

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
		mali_tpi_egl_simple_color color)
{
	mali_tpi_bool retval;
	unsigned int abs_x_pos, abs_y_pos;
	unsigned char *pixel_address;

	abs_x_pos = window_x_pos + pixel_x_pos;
	abs_y_pos = window_y_pos + pixel_y_pos;

	pixel_address = visible_framebuffer_start;

	pixel_address += abs_y_pos * framebuffer_width * bytes_per_pixel;
	pixel_address += abs_x_pos * bytes_per_pixel;

	retval = check_color_component(pixel_address, bytes_per_pixel, red_mask,   color & 0x8) &&
	         check_color_component(pixel_address, bytes_per_pixel, green_mask, color & 0x4) &&
	         check_color_component(pixel_address, bytes_per_pixel, blue_mask,  color & 0x2) &&
	         check_color_component(pixel_address, bytes_per_pixel, alpha_mask, color & 0x1);

	return retval;
}
