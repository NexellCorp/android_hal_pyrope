/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*** 
 * @file egl_helpers_pixmap.c
 * Implementations of platform-independent native pixmap helper functions.
 * Platform-dependent functions are defined elsewhere.
 */

#include <EGL/egl.h>

#include "suite.h"
#include "egl_framework.h"
#include "egl_helpers_pixmap.h"
#include "base/mali_byteorder.h"
#include <limits.h>

/**
 * @brief Debugging output level for this module.
 *
 * Unless overridden at compile time, only messages at level 0 or 1 are
 * printed (see mali_debug.h).
 */
#ifndef EHNP_DEBUG_LEVEL
#define EHNP_DEBUG_LEVEL 2
#endif /* EHNP_DEBUG_LEVEL */

/**
 * Bogus pixel value returned by egl_helper_nativepixmap_get_pixel if it
 * fails for any reason (out of enum range).
 */
#define BAD_PIXEL_VALUE 0xDEADC0DE

/**
 * @brief Conditions that may require a warning to be generated.
 *
 * Call do_warning with one of these values to find out whether the
 * corresponding warning message has already been generated for the
 * test currently being executed.
 */
typedef enum warn_condition
{
	CONDITION_READ_ALPHA,  /**< Alpha component non-zero in a pixel read from
	                            a pixmap that doesn't support an alpha channel
	                            properly. */
	CONDITION_WRITE_ALPHA, /**< Alpha component not maximal (fully opaque) in
	                            a pixel written to a pixmap that doesn't
	                            support an alpha channel properly. */
	CONDITION_INCOMPATIBLE_FORMAT, /**< Attempt to read or write a pixel in an
	                                    incompatible format (e.g. RGB565 vs.
	                                    ARGB8888). */
	CONDITION_RECT_BAD_SIZE,   /**< Rectangle has invalid width or height. */
	CONDITION_PIXMAP_BAD_SIZE, /**< Pixmap has invalid width or height. */
	CONDITION_LAST             /**< Must be the last member. Not to be used */
}
warn_condition;

/**
 * @brief get_alpha_mask
 *
 * Gets a bit mask for the alpha channel of a given display format (e.g.
 * bits 24-31 set for the ARGB format). This mask can be used to read and
 * manipulate the alpha component of a pixel value in isolation from other
 * components. Returns 0 if the format is unrecognised or doesn't have an
 * alpha channel.
 *
 * @param[in] format  Display format for which to get an alpha mask.
 * @return    Mask with all bits set for the alpha channel.
 */
MALI_STATIC u32 get_alpha_mask( display_format format )
{
	static const struct
	{
		display_format format;
		u32            alpha_mask;
	}
	mask_lut[] =
	{
		{ DISPLAY_FORMAT_ARGB8888, 0xFF000000 },
		{ DISPLAY_FORMAT_ARGB4444, 0xF000     },
		{ DISPLAY_FORMAT_ARGB1555, 0x8000     },
		{ DISPLAY_FORMAT_AL88,     0xFF00     },
		{ DISPLAY_FORMAT_A8,       0xFF       }
	};
	unsigned int i;
	u32 alpha_mask = 0;

	for ( i = 0; i < MALI_ARRAY_SIZE(mask_lut); ++i )
	{
		if ( mask_lut[i].format == format )
		{
			alpha_mask = mask_lut[i].alpha_mask;
			break;
		}
	}
	return alpha_mask;
}

/**
 * @brief do_warning
 *
 * Keeps track of whether each type of warning message has been generated
 * yet for the test currently being executed. Used to avoid generating the
 * same warning message repeatedly.
 *
 * @param[in] test_suite  Test suite context pointer, or NULL to suppress
 *                        warning messages.
 * @param[in] cond        Condition which may require a warning.
 * @return    EGL_TRUE if a warning is required; otherwise EGL_FALSE.
 * @warning Not thread safe because it reads and writes static objects.
 */
MALI_STATIC EGLBoolean do_warning( suite *test_suite, warn_condition cond )
{
	EGLBoolean do_warning;

	EGL_HELPER_ASSERT_MSG( cond < CONDITION_LAST, ( "do_warning: bad condition (%d)\n", cond ) );

	if ( NULL == test_suite )
	{
		do_warning = EGL_FALSE;
	}
	else
	{
		static suite *last_suite = NULL;
		static test *last_test = NULL;
		static int last_fixture = INT_MIN;
		static EGLBoolean warned[CONDITION_LAST];
		test *current_test = test_suite->this_test;
		int current_fixture = test_suite->fixture_index;

		/* Enable all warnings if a new suite, test or fixture is reached. */
		if ( test_suite != last_suite || current_test != last_test || current_fixture != last_fixture )
		{
			unsigned int i;
			for ( i = 0; i < MALI_ARRAY_SIZE(warned); ++i )
			{
				warned[i] = EGL_FALSE;
			}
			last_suite = test_suite;
			last_test = current_test;
			last_fixture = current_fixture;
		}

		/* Only do warning if we haven't warned of this condition yet. */
		do_warning = !warned[cond];
		warned[cond] = EGL_TRUE;
	}
	return do_warning;
}

/**
 * @brief clear_alpha
 *
 * Zeroes the alpha component of a pixel value and generates a warning
 * if it was not maximal. Designed to be used when writing pixels to a
 * pixmap which doesn't support an alpha channel properly. Returns the
 * input pixel value unmodified if 0 is specified as the alpha channel
 * bit mask.
 *
 * @param[in] test_suite  Test suite context pointer, or NULL to suppress
 *                        warning messages.
 * @param[in] pixel_value Pixel value to modify, in any format.
 * @param[in] alpha_mask  Bit mask for alpha channel in pixel format.
 * @return    Pixel value with zeroed alpha component.
 * @warning Not thread safe because it uses do_warning.
 */
MALI_STATIC u32 clear_alpha( suite *test_suite, u32 pixel_value, u32 alpha_mask )
{
	if ( alpha_mask != (alpha_mask & pixel_value) &&
		   do_warning( test_suite, CONDITION_WRITE_ALPHA ) )
	{
		/* A warning here means we need to either fix the OS code for handling pixmaps,
		 * or disable the testcase only for EGL_PIXMAP_NOALPHA and warn that we've had
		 * to do so - must warn, otherwise we get false positive/negative */
		warn( test_suite, dsprintf( test_suite,
		                            "Attempted to write alpha value (0x%x) to pixmap. Test may fail due to incorrect OS alpha support",
		                            pixel_value & alpha_mask ) );
	}

	/* Zero alpha component because the pixmap doesn't support it.*/
	pixel_value &= ~alpha_mask;

	return pixel_value;
}

/**
 * @brief saturate_alpha
 *
 * Saturates the alpha component of a pixel value and generates a warning
 * if it was non-zero. Designed to be used when reading pixels from a pixmap
 * which doesn't support an alpha channel properly. Returns the input pixel
 * value unmodified if 0 is specified as the alpha channel bit mask.
 *
 * @param[in] test_suite  Test suite context pointer, or NULL to suppress
 *                        warning messages.
 * @param[in] pixel_value Pixel value to modify, in any format.
 * @param[in] alpha_mask  Bit mask for alpha channel in pixel format.
 * @return    Pixel value with saturated alpha component.
 * @warning Not thread safe because it uses do_warning.
 */
MALI_STATIC u32 saturate_alpha( suite *test_suite, u32 pixel_value, u32 alpha_mask )
{
	if ( 0 != (alpha_mask & pixel_value) &&
		   do_warning( test_suite, CONDITION_READ_ALPHA ) )
	{
		/* A warning here means we need to either fix the OS code for handling pixmaps,
		 * or disable the testcase only for EGL_PIXMAP_NOALPHA and warn that we've had
		 * to do so - must warn, otherwise we get false positive/negative */
		warn( test_suite, dsprintf( test_suite,
		                            "Read unexpected alpha value (0x%x) from pixmap. Test may fail due to incorrect OS alpha support",
		                            pixel_value & alpha_mask ) );
	}

	/* Saturate alpha component because the pixmap doesn't support it.*/
	pixel_value = pixel_value | alpha_mask;

	return pixel_value;
}

/**
 * @brief check_pixmap
 *
 * Validates the format and dimensions of a pixmap to be used with an
 * egl_helper_nativepixmap_... function. Generates warnings if the pixel
 * format doesn't match the required format, or the pixmap has bad width or
 * height. No warnings will be generated if the 'test_suite' member of the
 * pixmap specifier is NULL.
 *
 * @param[in] ps     Pixmap specifier to check.
 * @param[in] format Required pixmap format.
 * @param[in] fn     Name of the calling function.
 * @return    EGL_TRUE if the formats match; otherwise EGL_FALSE.
 * @warning Not thread safe because it uses do_warning.
 */
MALI_STATIC EGLBoolean check_pixmap( const pixmap_spec *ps, display_format format, const char *fn )
{
	suite *test_suite;
	EGLBoolean okay = EGL_TRUE;

	EGL_HELPER_ASSERT_POINTER( ps );
	EGL_HELPER_ASSERT_POINTER( fn );

	test_suite = ps->test_suite;

	if ( format != ps->format )
	{
		/* We don't currently support pixel format conversion. */
		if ( do_warning( test_suite, CONDITION_INCOMPATIBLE_FORMAT ) )
		{
			warn( test_suite, dsprintf( test_suite,
			                            "%s: incompatible pixmap format (%d != %d)",
			                            fn, format, ps->format ) );
		}
		okay = EGL_FALSE;
	}
	if ( ps->width <= 0 || ps->height <= 0 )
	{
		/* Invalid pixmap dimensions */
		if ( do_warning( test_suite, CONDITION_PIXMAP_BAD_SIZE ) )
		{
			warn( test_suite, dsprintf( test_suite,
			                            "%s: bad pixmap dimensions %d,%d",
			                            fn, ps->width, ps->height ) );
		}
		okay = EGL_FALSE;
	}
	return okay;
}

/**
 * @brief clip_fill
 *
 * Clips an interval [ start .. start + length - 1 ] to keep within the
 * range [ 0 .. limit ]. The interval is typically one side of a pixmap fill
 * rectangle. If the lower bound (@a start) is negative then this function
 * will return 0 and reduce @a length to keep the upper bound the same;
 * otherwise it returns the unmodified value of @a start. The value of
 * @a length will also be reduced if necessary to ensure that @a start +
 * @a length <= @a limit.
 * Unless @a offset is a null pointer, the value it points to will be set
 * to the amount by which the lower bound increased, modulo the original
 * value pointed to by @a offset. This is to support source pixmap
 * coordinate wrap-around.
 *
 * @param[in]     start  Lower bound of interval to be clipped.
 * @param[in,out] length Size of interval to be clipped (must not be
 *                       negative).
 * @param[in]     limit  Upper limit of range to clip to (must not be
 *                       negative). The lower limit is fixed as 0.
 * @param[in,out] offset On entry: Divisor to use when calculating the
 *                                 output value (must be greater than 0).
 *                       On exit: Amount by which the lower bound increased,
 *                                modulo the input value.
 * @return        Start of clipped interval.
 * @note Typically @a start is the lefthand side and @a length the width of
 *       a rectangular area to be filled, @a limit is the width of a pixmap
 *       to be filled, and @a offset points to the width of a source pixmap
 *       (if applicable). On exit, @a offset will be the offset into the
 *       source pixmap.
 */
MALI_STATIC EGLint clip_fill( EGLint start, EGLint *length, EGLint limit, EGLint *offset )
{
	EGLint new_length;

	EGL_HELPER_ASSERT_POINTER( length );
	EGL_HELPER_ASSERT_MSG( *length >= 0,
	                   ( "Bad length %d in %s", *length, MALI_FUNCTION ) );
	EGL_HELPER_ASSERT_MSG( limit >= 0,
	                   ( "Bad limit %d in %s", limit, MALI_FUNCTION ) );

	if ( NULL != offset )
	{
		/* Calculate any offset into the source pixel required to preserve
		 * image alignment despite having clipped the fill rectangle. */
		EGLint new_offset, offset_limit = *offset;

		EGL_HELPER_ASSERT_MSG( offset_limit > 0,
		                   ( "Bad offset_limit %d in %s",
		                   offset_limit, MALI_FUNCTION ) );

		/* Check whether we need to clip the lower bound of the input interval,
		   whilst avoiding integer overflow (note that -INT_MIN > INT_MAX). */
		if ( start < 0 && start >= -INT_MAX && offset_limit > 0 )
		{
			new_offset = -start; /* Amount clipped from left of line */

			/* Allow for source pixel data coordinate wraparound but don't bother
			 * dividing by a huge number if no wrap-around is required. */
			if ( INT_MAX != offset_limit ) new_offset %= offset_limit;
			else if ( INT_MAX == new_offset ) new_offset = 0;
		}
		else
		{
			/* Upper bound is < 0 (interval fully out of range), lower bound is
			   >= 0 (interval fully in range), or bad source pixel data width. */
			new_offset = 0;
		}
		EGL_HELPER_ASSERT_MSG( new_offset >= 0 && new_offset < offset_limit,
		                   ( "Bad offset %d in %s", new_offset, MALI_FUNCTION ) );
		*offset = new_offset;
	}

	new_length = *length;

	if ( start >= limit )
	{
		/* Clip the lower bound to the upper limit specified by the caller and
		   zero the length. */
		start = limit;
		new_length = 0;
	}
	else
	{
		if ( start < 0 )
		{
			/* Clip the lower bound to zero and adjust the length accordingly. */
			new_length += start;
			start = 0;

			/* If the adjusted length is negative then the upper bound of the input
			   interval was less than 0. */
			new_length = MAX( 0, new_length );
		}

		/* Guard against upper bound greater than INT_MAX, which could otherwise
		   cause integer overflow. */
		if ( new_length > INT_MAX - start || start + new_length > limit )
		{
			/* Clip the upper bound to the limit specified by the caller, by
			   recalculating the length. */
			new_length = limit - start;
		}
	}

	EGL_HELPER_ASSERT_MSG( new_length <= *length,
	                   ( "Length grew from %d to %d in %s", *length, new_length, MALI_FUNCTION ) );
	EGL_HELPER_ASSERT_MSG( new_length >= 0,
	                   ( "Negative length in %s", new_length, MALI_FUNCTION ) );
	*length = new_length;

	EGL_HELPER_ASSERT_MSG( start >= 0 && start + new_length <= limit,
	                   ( "Bad start %d in %s", start, MALI_FUNCTION ) );
	return start;
}

/**
 * @brief clip_rect
 *
 * Validates a rectangle passed to an an egl_helper_nativepixmap_...
 * function and clips it if necessary to keep within the boundaries of the
 * pixmap to be addressed. If the width and height of source pixel data are
 * supplied as input then the X and Y offset into that data will be output
 * (where 0,0 corresponds to the top left corner of the original rectangle).
 * No warnings will be generated if the 'test_suite' member of the pixmap
 * specifier is NULL.
 *
 * @param[out]    out    Clipped rectangle.
 * @param[in]     ps     Pixmap specifier.
 * @param[in]     in     Rectangular area to be clipped to pixmap, or NULL
 *                       to output the pixmap's dimensions.
 * @param[in,out] src_x  On entry: Pointer to width of source pixel data,
 *                                 or NULL to skip calculation of X offset.
 *                       On exit:  X offset into the source pixel data.
 * @param[in,out] src_y  On entry: Pointer to height of source pixel data,
 *                                 or NULL to skip calculation of Y offset.
 *                       On exit:  Y offset into the source pixel data.
 * @param[in]     fn     Name of the calling function.
 * @return EGL_TRUE if the input rectangle was valid; otherwise EGL_FALSE.
 * @pre The pixmap dimensions must already have been validated.
 */
MALI_STATIC EGLBoolean clip_rect( pixmap_rect       *out,
                                  const pixmap_spec *ps,
                                  const pixmap_rect *in,
                                  EGLint            *src_x,
                                  EGLint            *src_y,
                                  const char        *fn )
{
	suite *test_suite;
	EGLBoolean okay = EGL_TRUE;

	EGL_HELPER_ASSERT_POINTER( out );
	EGL_HELPER_ASSERT_POINTER( ps );
	EGL_HELPER_ASSERT_POINTER( fn );

	test_suite = ps->test_suite;

	if ( NULL == in )
	{
		/* Null rectangle means fill entire pixmap */
		if ( NULL != src_x ) *src_x = 0;
		if ( NULL != src_y ) *src_y = 0;
		out->x = out->y = 0;
		out->width = ps->width;
		out->height = ps->height;
	}
	else
	{
		if ( in->width < 0 || in->height < 0 )
		{
			/* Report bad fill extent if we haven't done so before. */
			if ( do_warning( test_suite, CONDITION_RECT_BAD_SIZE ) )
			{
				warn( ps->test_suite, dsprintf( test_suite,
				                                "%s: bad rectangle dimensions %d,%d",
				                                fn, in->width, in->height ) );
			}
			okay = EGL_FALSE;
		}
		else
		{
			/* Clip fill extent to pixmap and calculate source pixmap coordinates.
			 * Source coordinates wrap around at bitmap edges.
			 */
			out->width = in->width;
			out->x = clip_fill( in->x, &out->width, ps->width, src_x );
			out->height = in->height;
			out->y = clip_fill( in->y, &out->height, ps->height, src_y );
		}
	}
	return okay;
}

EGLBoolean egl_helper_create_nativepixmap( suite *test_suite, u32 width, u32 height, display_format format, EGLNativePixmapType* pixmap )
{
	/* Set default config attribute mask bits */
	return egl_helper_create_nativepixmap_extended( test_suite, width, height, format,
	                                                EGL_OPENVG_BIT|EGL_OPENGL_ES_BIT|EGL_OPENGL_ES2_BIT, pixmap );
}

EGLBoolean egl_helper_create_nativepixmap_ump( suite *test_suite, u32 width, u32 height, display_format format, EGLNativePixmapType* pixmap )
{
	/* Set default config attribute mask bits */
	return egl_helper_create_nativepixmap_ump_extended( test_suite, width, height, format,
	                                                    EGL_OPENVG_BIT|EGL_OPENGL_ES_BIT|EGL_OPENGL_ES2_BIT, pixmap );
}

u32 egl_helper_nativepixmap_get_pixel( const pixmap_spec *src, EGLint x, EGLint y, display_format format )
{
	/* Cause a runtime error for non-debug builds if it fails: */
	u32 pixel_value = BAD_PIXEL_VALUE;

	EGL_HELPER_ASSERT_POINTER( src );
	EGL_HELPER_DEBUG_PRINT( EHNP_DEBUG_LEVEL,
	  ( "egl_helper_nativepixmap_get_pixel in format %d from %p at [%d,%d]\n",
	    format, src->pixels, x, y ) );

	/* Check that the source pixel format matches the pixmap */
	if ( check_pixmap( src, format, MALI_FUNCTION ) &&
	     x >= 0 && x < src->width && y >= 0 && y < src->height )
	{
		/* Calculate address of the relevant row of pixels */
		const void *data = MALI_REINTERPRET_CAST(const u8 *)src->pixels + y * src->pitch;
		u32 alpha_mask;

		/* If this pixmap doesn't support alpha then find a
		   suitable bit mask for the pixel format. */
		alpha_mask = ( src->alpha ? 0 : get_alpha_mask( src->format ) );

		switch( format )
		{
			case DISPLAY_FORMAT_ARGB8888:
			{
				const u32 *pixels = data;
				pixel_value = pixels[x];
				break;
			}
			case DISPLAY_FORMAT_RGB565:
			case DISPLAY_FORMAT_ARGB4444:
			case DISPLAY_FORMAT_ARGB1555:
			case DISPLAY_FORMAT_AL88:
			{
				const u16 *pixels = data;
				pixel_value = _MALI_GET_U16_ENDIAN_SAFE(&pixels[x]);
				break;
			}
			case DISPLAY_FORMAT_A8:
			case DISPLAY_FORMAT_L8:
			{
				const u8 *pixels = data;
				pixel_value = pixels[x];
				break;
			}
			default:
			{
				EGL_HELPER_ASSERT_MSG( 0, ( "egl_helper_nativepixmap_get_pixel: unsupported display format: %d", format ) );
				alpha_mask = 0;
				break;
			}
		}

		/* Saturate alpha component if the source pixmap doesn't support it. */
		pixel_value = saturate_alpha( src->test_suite, pixel_value, alpha_mask );
	}
	return pixel_value;
}

void egl_helper_nativepixmap_write_pixel( const pixmap_spec *dst, EGLint x, EGLint y, u32 pixel_value, display_format format )
{
	EGL_HELPER_ASSERT_POINTER( dst );
	EGL_HELPER_DEBUG_PRINT( EHNP_DEBUG_LEVEL,
	                  ( "egl_helper_nativepixmap_write_pixel 0x%X in format %d to %p at [%d,%d]\n",
	                    pixel_value, format, dst->pixels, x, y ) );

	/* Check that the source pixel format matches the pixmap */
	if ( check_pixmap( dst, format, MALI_FUNCTION ) &&
	     x >= 0 && x < dst->width && y >= 0 && y < dst->height )
	{
		/* Calculate address of the relevant row of pixels */
		void *data = MALI_REINTERPRET_CAST(u8 *)dst->pixels + y * dst->pitch;

		/* Zero alpha component if not supported by destination pixmap. */
		if ( !dst->alpha )
		{
			pixel_value = clear_alpha( dst->test_suite, pixel_value, get_alpha_mask( dst->format ) );
		}

		switch( format )
		{
			case DISPLAY_FORMAT_ARGB8888:
			case DISPLAY_FORMAT_ARGB0888:
			{
				u32 *pixels = data;
				pixels[x] = pixel_value;
				break;
			}
			case DISPLAY_FORMAT_RGB565:
			case DISPLAY_FORMAT_ARGB4444:
			case DISPLAY_FORMAT_ARGB1555:
			case DISPLAY_FORMAT_AL88:
			{
				u16 *pixels = data;
				pixels[x] = MALI_STATIC_CAST(u16)pixel_value; /* narrowing conversion */
				break;
			}
			case DISPLAY_FORMAT_A8:
			case DISPLAY_FORMAT_L8:
			{
				u8 *pixels = data;
				_MALI_SET_U16_ENDIAN_SAFE(&pixels[x], MALI_STATIC_CAST(u8)pixel_value); /* narrowing conversion */
				break;
			}
			default:
			{
				EGL_HELPER_ASSERT_MSG( 0, ( "egl_helper_nativepixmap_write_pixel: unsupported display format: %d", format ) );
				break;
			}
		}
	}
}

/**
 * @brief GENERIC_PLAIN_FILL
 *
 * Fills a span of pixels in a pixmap with a plain color. Assumes that the
 * alpha channel has already been stripped, if required.
 *
 * @param[in] pixel_t     Type of pixel value (e.g. u32 for 32bpp).
 * @param[in] dst_start   Pointer to destination pixel data.
 * @param[in] dst_x       Offset into destination pixel data.
 * @param[in] dst_width   No. of pixels to write to destination.
 * @param[in] pixel_value Pixel value to write.
 */
#define GENERIC_PLAIN_FILL( pixel_t, dst_start, dst_x, dst_width, pixel_value ) \
	do \
	{ \
		pixel_t *pixels = MALI_REINTERPRET_CAST(pixel_t *)(dst_start) + (dst_x); \
		EGLint fx; \
		for (fx = 0; fx < (dst_width); ++fx ) \
		{ \
			pixels[fx] = (pixel_value); \
		} \
	} \
	while (0)


#define GENERIC_PLAIN_FILL_16( pixel_t, dst_start, dst_x, dst_width, pixel_value ) \
	do \
	{ \
		u16 *pixels = MALI_REINTERPRET_CAST(pixel_t *)(dst_start) + (dst_x); \
		EGLint fx; \
		for (fx = 0; fx < (dst_width); ++fx ) \
		{ \
			_MALI_SET_U16_ENDIAN_SAFE(&pixels[fx], pixel_value); \
		} \
	} \
	while (0)

/* Fill an area of a native pixmap with plain color.
 */
void egl_helper_nativepixmap_plain_fill( const pixmap_spec *dst, const pixmap_rect *area, u32 pixel_value, display_format format )
{
	pixmap_rect clipped;

	EGL_HELPER_ASSERT_POINTER( dst );

	EGL_HELPER_DEBUG_PRINT( EHNP_DEBUG_LEVEL,
	  ( "egl_helper_nativepixmap_plain_fill 0x%X in format %d to %p\n",
	    pixel_value, format, dst->pixels ) );

	if ( NULL != area )
	{
		EGL_HELPER_DEBUG_PRINT( EHNP_DEBUG_LEVEL,
		  ( "area to fill is x %d, y %d, width %d, height %d\n",
		    area->x, area->y, area->width, area->height ) );
	}

	/* Check that the source pixel format matches the pixmap */
	if ( check_pixmap( dst, format, MALI_FUNCTION ) &&
	     clip_rect( &clipped, dst, area, NULL, NULL, MALI_FUNCTION ) )
	{
		u8 *row_start;
		EGLint x = clipped.x, width = clipped.width, fy;

		/* Zero alpha component if not supported by destination pixmap. */
		if ( !dst->alpha )
		{
			pixel_value = clear_alpha( dst->test_suite, pixel_value, get_alpha_mask( dst->format ) );
		}

		row_start = MALI_REINTERPRET_CAST(u8 *)dst->pixels + clipped.y * dst->pitch;
		for ( fy = 0; fy < clipped.height; ++fy )
		{
			switch ( format )
			{
				case DISPLAY_FORMAT_ARGB8888:
				case DISPLAY_FORMAT_ARGB0888:
				{
					GENERIC_PLAIN_FILL(u32, row_start, x, width, pixel_value);
					break;
				}
				case DISPLAY_FORMAT_RGB565:
				case DISPLAY_FORMAT_ARGB4444:
				case DISPLAY_FORMAT_ARGB1555:
				case DISPLAY_FORMAT_AL88:
				{
					GENERIC_PLAIN_FILL_16(u16, row_start, x, width, pixel_value);
					break;
				}
				case DISPLAY_FORMAT_A8:
				case DISPLAY_FORMAT_L8:
				{
					/* It is faster to use MEMSET for 8 bit-per-pixel formats. */
					u8 *pixels = MALI_REINTERPRET_CAST(u8 *)row_start + x;
					MEMSET(pixels, MALI_STATIC_CAST(u8)pixel_value, width * sizeof(*pixels)); /* narrowing conversion */
					break;
				}
				default:
				{
					EGL_HELPER_ASSERT_MSG( 0, ( "egl_helper_nativepixmap_plain_fill: unsupported display format: %d", format ) );
					return;
				}
			}
			row_start += dst->pitch;
		}
	}
}

/**
 * @brief GENERIC_COPIER
 *
 * Copies a span of pixels from one pixmap to another, stripping the alpha
 * channel if required. Restarts from the beginning of the source data if
 * it is narrower than the destination width.
 *
 * @param[in] test_suite     Test suite context pointer, for warnings.
 * @param[in] pixel_t        Type of pixel value (e.g. u32 for 32bpp).
 * @param[in] dst_start      Pointer to destination pixel data.
 * @param[in] dst_x          Offset into destination pixel data.
 * @param[in] dst_width      No. of pixels to write to destination.
 * @param[in] dst_alpha_mask Bit mask for destination alpha channel, or 0
 *                           to write unmodified alpha values.
 * @param[in] src_start      Pointer to source pixel data.
 * @param[in] src_x          Offset into source pixel data (ignored after
 *                           right-to-left wraparound has occurred).
 * @param[in] src_width      Maximum no. of pixels to read from source
 *                           (fewer pixels will be read until right-to-left
 *                           wraparound has occurred, if src_x > 0).
 * @param[in] src_alpha_mask Bit mask for source alpha channel, or 0 to
 *                           read unmodified alpha values.
 */
#define GENERIC_COPIER( test_suite, pixel_t, dst_start, dst_x, dst_width, dst_alpha_mask, src_start, src_x, src_width, src_alpha_mask ) \
	do \
	{ \
		pixel_t *dst_pixels = MALI_REINTERPRET_CAST(pixel_t *)(dst_start) + (dst_x); \
		pixel_t *src_pixels = MALI_REINTERPRET_CAST(pixel_t *)(src_start); \
		EGLint fx, sx = (src_x); \
		if ( 0 == (dst_alpha_mask) && 0 == (src_alpha_mask) ) \
		{ \
			/* Copy many pixels at a time from the source pixmap. */ \
			EGLint src_span; \
			for (fx = 0; fx < (dst_width); fx += src_span ) \
			{ \
				src_span = MIN((src_width) - sx, (dst_width) - fx); \
				MEMCPY(dst_pixels + fx, src_pixels + sx, src_span * sizeof(pixel_t)); \
				sx = 0; /* Source offset only applies first time */ \
			} \
		} \
		else \
		{ \
			/* We must copy one pixel at a time to modify the alpha channel. */ \
			for (fx = 0; fx < (dst_width); ++fx ) \
			{ \
				u32 pixel_value = src_pixels[sx]; \
				pixel_value = saturate_alpha( (test_suite), (pixel_value), (src_alpha_mask) ); \
				pixel_value = clear_alpha( (test_suite), (pixel_value), (dst_alpha_mask) ); \
				dst_pixels[fx] = MALI_STATIC_CAST(pixel_t)pixel_value; \
				if (++sx >= (src_width)) \
				{ \
					sx = 0; \
				} \
			} \
		} \
	} \
	while (0)

#define GENERIC_COPIER_16( test_suite, pixel_t, dst_start, dst_x, dst_width, dst_alpha_mask, src_start, src_x, src_width, src_alpha_mask ) \
	do \
	{ \
		pixel_t *dst_pixels = MALI_REINTERPRET_CAST(pixel_t *)(dst_start) + (dst_x); \
		pixel_t *src_pixels = MALI_REINTERPRET_CAST(pixel_t *)(src_start); \
		EGLint fx, sx = (src_x); \
		if ( 0 == (dst_alpha_mask) && 0 == (src_alpha_mask) ) \
		{ \
			/* Copy many pixels at a time from the source pixmap. */ \
			EGLint src_span; \
			for (fx = 0; fx < (dst_width); fx += src_span ) \
			{ \
				src_span = MIN((src_width) - sx, (dst_width) - fx); \
				MEMCPY(dst_pixels + fx, src_pixels + sx, src_span * sizeof(pixel_t)); \
				sx = 0; /* Source offset only applies first time */ \
			} \
		} \
		else \
		{ \
			/* We must copy one pixel at a time to modify the alpha channel. */ \
			for (fx = 0; fx < (dst_width); ++fx ) \
			{ \
				u32 pixel_value = _MALI_GET_U16_ENDIAN_SAFE(&src_pixels[sx]); \
				pixel_value = saturate_alpha( (test_suite), (pixel_value), (src_alpha_mask) ); \
				pixel_value = clear_alpha( (test_suite), (pixel_value), (dst_alpha_mask) ); \
				_MALI_SET_U16_ENDIAN_SAFE(&dst_pixels[fx], MALI_STATIC_CAST(pixel_t)pixel_value); \
				if (++sx >= (src_width)) \
				{ \
					sx = 0; \
				} \
			} \
		} \
	} \
	while (0)


/* Copy pixels from one pixmap to another.
 */
void egl_helper_nativepixmap_copy( const pixmap_spec *dst, const pixmap_rect *area, const pixmap_spec *src )
{
	pixmap_rect clipped;
	EGLint src_x, src_y, src_width;

	EGL_HELPER_ASSERT_POINTER( dst );
	EGL_HELPER_ASSERT_POINTER( src );

	EGL_HELPER_DEBUG_PRINT( EHNP_DEBUG_LEVEL,
	  ( "egl_helper_nativepixmap_copy from %p in format %d to %p\n",
	    src->pixels, src->format, dst->pixels ) );

	if ( NULL != area )
	{
		EGL_HELPER_DEBUG_PRINT( EHNP_DEBUG_LEVEL,
		  ( "area to fill is x %d, y %d, width %d, height %d\n",
		    area->x, area->y, area->width, area->height ) );
	}

	src_width = src->width; /* used in innermost loop */

	/* Source coordinates wrap around at the pixmap edges. */
	src_x = src_width;
	src_y = src->height; 

	/* Check that the source pixel format matches the pixmap */
	if ( check_pixmap( dst, src->format, MALI_FUNCTION ) &&
	     check_pixmap( src, dst->format, MALI_FUNCTION ) &&
	     clip_rect( &clipped, dst, area, &src_x, &src_y, MALI_FUNCTION ) )
	{
		u8 *row_start;
		const u8 *src_row_start = src->pixels;
		EGLint x = clipped.x, width = clipped.width, fy;
		u32 src_alpha_mask, dst_alpha_mask;
		suite *test_suite = dst->test_suite;

		/* Set up a suitable alpha bit masks for the pixel format. */
		src_alpha_mask = ( src->alpha ? 0 : get_alpha_mask( src->format ) );
		dst_alpha_mask = ( dst->alpha ? 0 : get_alpha_mask( dst->format ) );

		row_start = MALI_REINTERPRET_CAST(u8 *)dst->pixels + clipped.y * dst->pitch;
		for ( fy = 0; fy < clipped.height; ++fy )
		{
			switch ( dst->format )
			{
				case DISPLAY_FORMAT_ARGB8888:
				{
					GENERIC_COPIER( test_suite, u32, row_start, x, width, dst_alpha_mask, src_row_start, src_x, src_width, src_alpha_mask );
					break;
				}
				case DISPLAY_FORMAT_RGB565:
				case DISPLAY_FORMAT_ARGB4444:
				case DISPLAY_FORMAT_ARGB1555:
				case DISPLAY_FORMAT_AL88:
				{
					GENERIC_COPIER_16( test_suite, u16, row_start, x, width, dst_alpha_mask, src_row_start, src_x, src_width, src_alpha_mask );
					break;
				}
				case DISPLAY_FORMAT_A8:
				case DISPLAY_FORMAT_L8:
				{
					GENERIC_COPIER( test_suite, u8, row_start, x, width, dst_alpha_mask, src_row_start, src_x, src_width, src_alpha_mask );
					break;
				}
				default:
				{
					EGL_HELPER_ASSERT_MSG( 0, ( "egl_helper_nativepixmap_copy: unsupported display format: %d", dst->format ) );
					return;
				}
			}
			if ( ++src_y >= src->height )
			{
				/* Wrap around from the top to the bottom of the source pixmap */
				src_y = 0;
				src_row_start = src->pixels;
			}
			else
			{
				/* Advance to the next line of the source pixmap */
				src_row_start += src->pitch;
			}
			row_start += dst->pitch;
		}
	}
}

/**
 * @brief GENERIC_CUSTOM_FILL
 *
 * Fills a span of pixels in a pixmap, calling a client-supplied function
 * to get pixel values and stripping the alpha channel if required.
 *
 * @param[in] test_suite     Test suite context pointer, for warnings.
 * @param[in] pixel_t        Type of pixel value (e.g. u32 for 32bpp).
 * @param[in] dst_start      Pointer to destination pixel data.
 * @param[in] dst_x          Offset into destination pixel data.
 * @param[in] dst_width      No. of pixels to write to destination.
 * @param[in] dst_alpha_mask Bit mask for destination alpha channel, or 0 to
 *                           read unmodified alpha values.
 * @param[in] filler         Specifier of custom fill callback.
 * @param[in] fill_x         X coordinate relative to fill origin.
 * @param[in] fill_y         Y coordinate relative to fill origin.
 */
#define GENERIC_CUSTOM_FILL( test_suite, pixel_t, dst_start, dst_x, dst_width, dst_alpha_mask, filler, fill_x, fill_y ) \
	do \
	{ \
		pixel_t *pixels = MALI_REINTERPRET_CAST(pixel_t *)(dst_start) + (dst_x); \
		EGLint fx; \
		for (fx = 0; fx < (dst_width); ++fx ) \
		{ \
			u32 pixel_value = filler->get_pixel( (filler)->context, (fill_x) + fx, (fill_y) ); \
			if ( 0 != (dst_alpha_mask) ) \
			{ \
				pixel_value = clear_alpha( (test_suite), pixel_value, (dst_alpha_mask) ); \
			} \
			pixels[fx] = MALI_STATIC_CAST(pixel_t)pixel_value; \
		} \
	} \
	while (0)

#define GENERIC_CUSTOM_FILL_16( test_suite, pixel_t, dst_start, dst_x, dst_width, dst_alpha_mask, filler, fill_x, fill_y ) \
	do \
	{ \
		pixel_t *pixels = MALI_REINTERPRET_CAST(pixel_t *)(dst_start) + (dst_x); \
		EGLint fx; \
		for (fx = 0; fx < (dst_width); ++fx ) \
		{ \
			u32 pixel_value = filler->get_pixel( (filler)->context, (fill_x) + fx, (fill_y) ); \
			if ( 0 != (dst_alpha_mask) ) \
			{ \
				pixel_value = clear_alpha( (test_suite), pixel_value, (dst_alpha_mask) ); \
			} \
			_MALI_SET_U16_ENDIAN_SAFE(&pixels[fx], MALI_STATIC_CAST(pixel_t)pixel_value); \
		} \
	} \
	while (0)


/* Fill an area of a native pixmap using a custom pattern.
 */
void egl_helper_nativepixmap_custom_fill( const pixmap_spec *dst, const pixmap_rect *area, const pixmap_fill *filler )
{
	pixmap_rect clipped;
	EGLint src_x, src_y; 

	EGL_HELPER_ASSERT_POINTER( dst );
	EGL_HELPER_ASSERT_POINTER( filler );

	EGL_HELPER_DEBUG_PRINT( EHNP_DEBUG_LEVEL,
	  ( "egl_helper_nativepixmap_custom_fill %p(%p) in format %d to %p\n",
	    filler->get_pixel, filler->context, filler->format, dst->pixels ) );

	if ( NULL != area )
	{
		EGL_HELPER_DEBUG_PRINT( EHNP_DEBUG_LEVEL,
		  ( "area to fill is x %d, y %d, width %d, height %d\n",
		    area->x, area->y, area->width, area->height ) );
	}

	/* The coordinates passed to a custom fill function don't wrap around. */
	src_x = src_y = INT_MAX; 

	/* Check that the source pixel format matches the pixmap */
	if ( check_pixmap( dst, filler->format, MALI_FUNCTION ) &&
	     clip_rect( &clipped, dst, area, &src_x, &src_y, MALI_FUNCTION ) )
	{
		u8 *row_start;
		EGLint x = clipped.x, width = clipped.width, fy;
		u32 alpha_mask;
		suite *test_suite = dst->test_suite;

		/* Set up a suitable alpha bit mask for the pixel format. */
		alpha_mask = ( dst->alpha ? 0 : get_alpha_mask( dst->format ) );

		row_start = MALI_REINTERPRET_CAST(u8 *)dst->pixels + clipped.y * dst->pitch;
		for ( fy = 0; fy < clipped.height; ++fy )
		{
			switch ( filler->format )
			{
				case DISPLAY_FORMAT_ARGB8888:
				{
					GENERIC_CUSTOM_FILL( test_suite, u32, row_start, x, width, alpha_mask, filler, src_x, src_y + fy );
					break;
				}
				case DISPLAY_FORMAT_RGB565:
				case DISPLAY_FORMAT_ARGB4444:
				case DISPLAY_FORMAT_ARGB1555:
				case DISPLAY_FORMAT_AL88:
				{
					GENERIC_CUSTOM_FILL_16( test_suite, u16, row_start, x, width, alpha_mask, filler, src_x, src_y + fy );
					break;
				}
				case DISPLAY_FORMAT_A8:
				case DISPLAY_FORMAT_L8:
				{
					GENERIC_CUSTOM_FILL( test_suite, u8, row_start, x, width, alpha_mask, filler, src_x, src_y + fy );
					break;
				}
				default:
				{
					EGL_HELPER_ASSERT_MSG( 0, ( "egl_helper_nativepixmap_custom_fill: unsupported display format: %d", filler->format ) );
					return;
				}
			}
			row_start += dst->pitch;
		}
	}
}
