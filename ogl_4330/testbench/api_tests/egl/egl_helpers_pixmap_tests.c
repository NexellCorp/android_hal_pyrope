/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*** 
 * @file egl_helpers_pixmap_tests.c
 * Test suite for the platform-independent native pixmap helper functions.
 */

#include <stdlib.h>
#include <limits.h>
#include <mali_system.h>
#include <framework_main.h>
#include <suite.h>
#include <mem.h>
#include <resultset.h>
#include <EGL/egl.h>
#include "egl_helpers_pixmap.h"
#include "egl_helpers_pixmap_test_utils.h"

/**
 * @brief TEST_LIST_ENTRY
 *
 * Declares a test name string and pointer to the function to be called to
 * execute that test. Intended for use in an array initialiser.
 *
 * @param[in] name Name of the test to be declared.
 */
#define TEST_LIST_ENTRY( name ) { #name, name##_test }

/**
 * Bogus pixel value returned by egl_helper_nativepixmap_get_pixel if it
 * fails for any reason (out of enum range).
 */
#define BAD_PIXEL_VALUE 0xDEADC0DE

/**
 * Miscellaneous numeric constants.
 */
enum
{
	CHECK_ZONE = 16, /* To allow detection of writes outside a pixmap */
	MARKER = 0xA9,
	FORMAT_FILL_WIDTH = 1,
	FORMAT_FILL_HEIGHT = 1,
	PIXEL_VALUE = 0xABBABABE,
	MAX_FILL_WIDTH = 32,
	MAX_FILL_HEIGHT = 32,
	MAX_FIXTURE_NAME = 64
};

/**
 * @brief Fixture (i.e. common context) for this test suite.
 */
typedef struct fixture
{
	pixmap_spec  ps;       /**< Pixmap specifier has pointer into buffer */
	void        *buffer;   /**< Pointer to actual start of buffer (including guard zones) */
	size_t       buf_size; /**< Actual size of buffer */
	EGLint       format_index; /**< Index into the table of pixel formats */
	char         name[MAX_FIXTURE_NAME + 1]; /**< String buffer for fixture name. */
}
fixture;

/**
 * @brief Copy or custom fill test context.
 */
typedef struct fill_test_context
{
	pixmap_fill filler;      /**< Custom fill specifier. */
	pixmap_spec ps;          /**< Expected result of fill. Also used as the
	                              source data for a copy or custom fill. */
	u32         pixel_value; /**< Pixel value to write for a plain fill. */
}
fill_test_context;

/* Test only the supported subset of display formats */
static const struct
{
	display_format format;
	size_t         size;
	u32            colour_mask;
	u32            alpha_mask;
}
pixel_formats[] =
{ /* Pixel format name       Pixel size   Colour mask Alpha mask */
	{ DISPLAY_FORMAT_ARGB8888, sizeof(u32), 0x00ffffff, 0xff000000 },
	{ DISPLAY_FORMAT_RGB565,   sizeof(u16), 0x0000ffff, 0x00000000 }, /* No alpha */
	{ DISPLAY_FORMAT_ARGB4444, sizeof(u16), 0x00000fff, 0x0000f000 },
	{ DISPLAY_FORMAT_ARGB1555, sizeof(u16), 0x00007fff, 0x00008000 },
	{ DISPLAY_FORMAT_AL88,     sizeof(u16), 0x000000ff, 0x0000ff00 },
	{ DISPLAY_FORMAT_A8,       sizeof(u8),  0x00000000, 0x000000ff }, /* Alpha-only */
	{ DISPLAY_FORMAT_L8,       sizeof(u8),  0x000000ff, 0x00000000 }  /* No alpha */
};

static const struct
{
	EGLint width;
	EGLint height;
}
pixmap_dims[] =
{
	{ 0,   0 },
	{ 1,   1 },
	{ 13,  7 },
	{ 3,  17 },
	{ 25, 31 }
};

/**
 * @brief Mark memory around pixmap
 *
 * Marks the memory before and after the pixmap buffer associated
 * with the current test fixture, as well as any righthand
 * wastage on each line, to allow detection of buffer overruns.
 *
 * @param[in] test_suite Test suite context pointer
 */
MALI_STATIC void mark_all_buffer_zones( suite *test_suite )
{
	EGLint y, height, pitch;
	u8 *pixels, *buffer;
	size_t row_size, used, wastage, bottom;
	const fixture *fix;

	MALI_DEBUG_ASSERT_POINTER( test_suite );

	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	buffer = fix->buffer;
	MALI_DEBUG_ASSERT_POINTER( buffer );

	pitch = fix->ps.pitch;
	row_size = _mali_sys_abs(pitch);

	/* Mark the top check zone */
	_mali_sys_memset( buffer, MARKER, row_size * CHECK_ZONE );

	/* Calculate size of pixel data for one line */
	MALI_DEBUG_ASSERT( fix->format_index < MALI_ARRAY_SIZE(pixel_formats),
	                   ( "Bad format index %d", fix->format_index ) );

	used = fix->ps.width * pixel_formats[fix->format_index].size;
	wastage = row_size - used;

	/* Find start of righthand wastage on first line */
	pixels = fix->ps.pixels;
	MALI_DEBUG_ASSERT_POINTER( pixels );
	pixels += used;

	/* Mark the right check zone */
	height = fix->ps.height;
	for (y = 0; y < height; ++y)
	{
		_mali_sys_memset( pixels, MARKER, wastage );
		pixels += pitch; /* advance to next line (higher or lower address) */
	}

  /* Mark the bottom check zone */
  bottom = row_size * (CHECK_ZONE + height);
  _mali_sys_memset( buffer + bottom, MARKER, fix->buf_size - bottom );
}

/**
 * @brief Check for writes outside pixmap
 *
 * Checks that the memory around the pixmap buffer associated with
 * the current test fixture has not been modified since the last
 * call to mark_all_buffer_zones.
 *
 * @param[in] test_suite Test suite context pointer.
 * @return EGL_FALSE if one or more guard zones were corrupted.
 */
MALI_STATIC EGLBoolean check_all_buffer_zones( suite *test_suite )
{
	EGLint y, height, pitch;
	u8 *pixels, *buffer;
	size_t row_size, used, wastage, bottom, corrupted;
	EGLBoolean success;
	const fixture *fix;

	MALI_DEBUG_ASSERT_POINTER( test_suite );

	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	buffer = fix->buffer;
	MALI_DEBUG_ASSERT_POINTER( buffer );

	pitch = fix->ps.pitch;
	row_size = _mali_sys_abs(pitch);

	/* Validate the top check zone */
	corrupted = check_bytes( buffer, MARKER, row_size * CHECK_ZONE );
	assert_fail( !corrupted, dsprintf( test_suite,
	             "%lu bytes of memory before pixmap are corrupted",
	             MALI_STATIC_CAST(unsigned long)corrupted ) );

	success = !corrupted;

	/* Calculate size of pixel data for one line */
	MALI_DEBUG_ASSERT( fix->format_index < MALI_ARRAY_SIZE(pixel_formats),
	                   ( "Bad format index %d", fix->format_index ) );

	used = fix->ps.width * pixel_formats[fix->format_index].size;
	wastage = row_size - used;

	/* Find start of righthand wastage on first line */
	pixels = fix->ps.pixels;
	MALI_DEBUG_ASSERT_POINTER( pixels );
	pixels += used;

	/* Validate the right check zone */
	height = fix->ps.height;
	for (y = 0; y < height; ++y)
	{
		corrupted = check_bytes( pixels, MARKER, wastage );
		assert_fail( !corrupted, dsprintf( test_suite,
		             "%lu bytes of memory at end of row %d of pixmap are corrupted",
		             MALI_STATIC_CAST(unsigned long)corrupted, y ) );

		if ( corrupted )
		{
			success = EGL_FALSE;
			break;
		}
		pixels += pitch; /* advance to next line (higher or lower address) */
	}

	/* Validate the bottom check zone */
	bottom = row_size * (CHECK_ZONE + height);
	corrupted = check_bytes( buffer + bottom, MARKER, fix->buf_size - bottom );
	assert_fail( !corrupted, dsprintf( test_suite,
	             "%lu bytes of memory after pixmap are corrupted",
	             MALI_STATIC_CAST(unsigned long)corrupted ) );

	success = success && !corrupted;

	/* Return EGL_FALSE if one or more guard zones were corrupted */
	return success;
}

/**
 * @brief Make a pixel value
 *
 * Masks the specified pixel value according to the format of the pixmap
 * associated with the current test fixture. If the pixmap doesn't support
 * alpha properly then saturate those bits.
 *
 * @param[in] test_suite  Test suite context pointer.
 * @param[in] pixel_value 32-bit pixel value.
 * @return    Pixel value masked suitable for pixmap format.
 */
MALI_STATIC u32 make_pixel_value( suite *test_suite, u32 pixel_value )
{
	const fixture *fix;
	EGLint fi;

	MALI_DEBUG_ASSERT_POINTER( test_suite );
	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	fi = fix->format_index;
	MALI_DEBUG_ASSERT( fi < MALI_ARRAY_SIZE( pixel_formats ),
	                   ( "Bad format index %d", fi ) );

	if ( fix->ps.alpha )
	{
		/* Alpha channel is supported so make all channels */
		pixel_value &= pixel_formats[fi].colour_mask |
		               pixel_formats[fi].alpha_mask;
	}
	else
	{
		/* Alpha channel isn't supported so make fully-opaque (to avoid warnings) */
		pixel_value = ( pixel_value & pixel_formats[fi].colour_mask ) |
		              pixel_formats[fi].alpha_mask;
	}
	return pixel_value;
}

/* Clear a pixmap to a given byte value */
MALI_STATIC void clear_pixmap( const fixture *fix, EGLint value )
{
	EGLint y, height, pitch;
	u8 *pixels;
	size_t used;

	MALI_DEBUG_ASSERT_POINTER( fix );

	/* Calculate size of pixel data for one line */
	MALI_DEBUG_ASSERT( fix->format_index < MALI_ARRAY_SIZE(pixel_formats),
	                   ( "Bad format index %d", fix->format_index ) );

	used = fix->ps.width * pixel_formats[fix->format_index].size;

	pixels = fix->ps.pixels;
	MALI_DEBUG_ASSERT_POINTER( pixels );
	height = fix->ps.height;
	pitch = fix->ps.pitch;

	/* Clear one row at a time */
	for (y = 0; y < height; ++y)
	{
		_mali_sys_memset( pixels, value, used );
		pixels += pitch; /* advance to next line (higher or lower address) */
	}
}

/* Check that bytes within a specified rectangle of a pixmap have a given value. */
MALI_STATIC EGLBoolean check_rect_plain( suite *test_suite, const pixmap_rect *area, EGLint value )
{
	EGLBoolean success = EGL_TRUE;
	const fixture *fix;
	EGLint y, y_limit, pitch;
	const u8 *pixels;
	size_t check_size, pix_size;

	MALI_DEBUG_ASSERT_POINTER( test_suite );
	MALI_DEBUG_ASSERT_POINTER( area );

	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );
	assert_rect_within_pixmap( area, &fix->ps );

	/* Calculate size of pixel data for one line */
	MALI_DEBUG_ASSERT( fix->format_index < MALI_ARRAY_SIZE(pixel_formats),
	                   ( "Bad format index %d", fix->format_index ) );

	pix_size = pixel_formats[fix->format_index].size;
	check_size = area->width * pix_size;

	/* Calculate address of first pixel to be checked */
	pixels = fix->ps.pixels;
	pitch = fix->ps.pitch;
	MALI_DEBUG_ASSERT_POINTER( pixels );
	pixels += ( area->y * pitch ) + ( area->x * pix_size );

	/* Validate one row at a time */
	y_limit = area->y + area->height;
	for ( y = area->y; y < y_limit; ++y )
	{
		size_t mismatch = check_bytes( pixels, value, check_size );
		assert_fail( !mismatch, dsprintf( test_suite,
		             "%lu bytes of memory in row %d are wrong",
		             MALI_STATIC_CAST(unsigned long)mismatch, y ) );
		if ( mismatch )
		{
			success = EGL_FALSE;
			break;
		}
		pixels += pitch; /* advance to next line (higher or lower address) */
	}
	return success;
}

/* Check that bytes within a specified rectangle of a pixmap match a pattern. */
MALI_STATIC EGLBoolean check_rect_pattern( suite *test_suite, const pixmap_rect *area, const void *expected, size_t exp_pitch )
{
	EGLBoolean success = EGL_TRUE;
	const fixture *fix;
	EGLint y, y_limit, pitch;
	const u8 *pixels, *exp_pixels = expected;
	size_t check_size, pix_size;

	MALI_DEBUG_ASSERT_POINTER( test_suite );
	MALI_DEBUG_ASSERT_POINTER( area );
	MALI_DEBUG_ASSERT_POINTER( expected );

	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );
	assert_rect_within_pixmap( area, &fix->ps );

	/* Calculate size of pixel data for one line */
	MALI_DEBUG_ASSERT( fix->format_index < MALI_ARRAY_SIZE(pixel_formats), ( "bad format index %d", fix->format_index ) );
	pix_size = pixel_formats[fix->format_index].size;
	check_size = area->width * pix_size;

	/* Calculate address of first pixel to be checked */
	pixels = fix->ps.pixels;
	pitch = fix->ps.pitch;
	MALI_DEBUG_ASSERT_POINTER( pixels );
	pixels += ( area->y * pitch ) + ( area->x * pix_size );

	/* Validate one row at a time */
	y_limit = area->y + area->height;
	for ( y = area->y; y < y_limit; ++y )
	{
		size_t mismatch = compare_bytes( pixels, exp_pixels, check_size );
		assert_fail( !mismatch, dsprintf( test_suite,
		             "%u bytes of memory in row %d are wrong", mismatch, y ) );
		if ( mismatch )
		{
			success = EGL_FALSE;
			break;
		}
		pixels += pitch; /* advance to next line (higher or lower address) */
		exp_pixels += exp_pitch;
	}
	return success;
}

/* Check that a specified rectangle of a pixmap has one byte value and the
   remainder has another value. */
MALI_STATIC EGLBoolean check_rect( suite *test_suite, const pixmap_rect *area, EGLint out, const pixmap_spec *expected )
{
	EGLBoolean success = EGL_TRUE;
	const fixture *fix;

	MALI_DEBUG_ASSERT_POINTER( test_suite );
	MALI_DEBUG_ASSERT_POINTER( expected );
	MALI_DEBUG_ASSERT( expected->width >= 0,
	                   ( "Bad expected width %d", expected->width ) );
	MALI_DEBUG_ASSERT( expected->height >= 0,
	                   ( "Bad expected height %d", expected->height ) );

	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	/* Various coordinate tests will fail if a pixmap has area 0 */
	if ( 0 != fix->ps.width && 0 != fix->ps.height )
	{
		pixmap_rect clipped_area;
		EGLint expected_width, expected_height;
		const u8 *expected_pixels;

		/* We may need to look at only part of the expected result
		   if the check rectangle is partially outside the pixmap. */
		expected_width = expected->width;
		expected_height = expected->height;
		expected_pixels = expected->pixels;

		if ( NULL == area )
		{
			/* If a null rectangle was specified then the whole pixmap should have
			   been filled. */
			clipped_area.x = clipped_area.y = 0;
			clipped_area.width = fix->ps.width;
			clipped_area.height = fix->ps.height;
		}
		else
		{
			pixmap_rect outside;

			/* Get ready to clip rectangle to pixmap dimensions.
			   Expect nothing to be rendered if rectangle is invalid. */
			clipped_area.x = area->x;
			clipped_area.y = area->y;
			clipped_area.width = MAX( 0, area->width );
			clipped_area.height = MAX( 0, area->height );

			/* Find which part of the rectangle overlaps the pixmap horizontally */
			if ( clipped_area.x < 0 )
			{
				/* Rectangle to check is left of the pixmap.
				   Calculate the offset into the expected result data. */
				MALI_DEBUG_ASSERT( fix->format_index < MALI_ARRAY_SIZE(pixel_formats),
				                   ( "Bad format index %d", fix->format_index ) );
				expected_pixels -= clipped_area.x * pixel_formats[fix->format_index].size;
				expected_width = MAX( 0, expected_width + clipped_area.x );
	
				/* Clip the check rectangle's horizontal extent */
				clipped_area.width = MAX( 0, clipped_area.x + clipped_area.width );
				clipped_area.x = 0;
			}
	
			/* Guard against signed integer overflow */
			if ( clipped_area.x > INT_MAX - clipped_area.width ||
			     clipped_area.x + clipped_area.width > fix->ps.width )
			{
				/* Rectangle to check is right of the pixmap */
				if ( clipped_area.x < fix->ps.width )
				{
					clipped_area.width = fix->ps.width - clipped_area.x;
				}
				else
				{
					clipped_area.width = 0;
					clipped_area.x = fix->ps.width;
				}
			}
	
			/* Find which part of the rectangle overlaps the pixmap vertically */
			if ( clipped_area.y < 0 )
			{
				/* Rectangle to check is above the pixmap.
				   Calculate the offset into the expected result data. */
				expected_pixels -= clipped_area.y * expected->pitch;
				expected_height = MAX( 0, expected_height + clipped_area.y );
	
				/* Clip the check rectangle's vertical extent */
				clipped_area.height = MAX( 0, clipped_area.y + clipped_area.height );
				clipped_area.y = 0;
			}
			/* Guard against signed integer overflow */
			if ( clipped_area.y > INT_MAX - clipped_area.height ||
			     clipped_area.y + clipped_area.height > fix->ps.height )
			{
				/* Rectangle to check is below the pixmap */
				if ( clipped_area.y < fix->ps.height )
				{
					clipped_area.height = fix->ps.height - clipped_area.y;
				}
				else
				{
					clipped_area.height = 0;
					clipped_area.y = fix->ps.height;
				}
			}
	
			/* If rectangle is to left or right of the pixmap then its width
			   should have been reduced to 0. */
			if ( clipped_area.width > 0 )
			{
				if ( clipped_area.y > 0 )
				{
					/* Check pixmap unmodified above check rectangle */
					outside.x = 0;
					outside.width = fix->ps.width;
					outside.y = 0;
					outside.height = clipped_area.y;
					if ( !check_rect_plain( test_suite, &outside, out ) )
					{
						success = EGL_FALSE;
					}
				}
				if ( clipped_area.y + clipped_area.height < fix->ps.height )
				{
					/* Check pixmap unmodified below check rectangle */
					outside.x = 0;
					outside.width = fix->ps.width;
					outside.y = clipped_area.y + clipped_area.height;
					outside.height = fix->ps.height - outside.y;
					if ( !check_rect_plain( test_suite, &outside, out ) )
					{
						success = EGL_FALSE;
					}
				}
			}
	
			/* If rectangle is above or below the pixmap then its height
			   should have been reduced to 0. */
			if ( clipped_area.height > 0 )
			{
				if ( clipped_area.x > 0 )
				{
					/* Check pixmap unmodified to the left of check rectangle */
					outside.x = 0;
					outside.width = clipped_area.x;
					outside.y = clipped_area.y;
					outside.height = clipped_area.height;
					if ( !check_rect_plain( test_suite, &outside, out ) )
					{
						success = EGL_FALSE;
					}
				}
				if ( clipped_area.x + clipped_area.width < fix->ps.width )
				{
					/* Check pixmap unmodified to the right of check rectangle */
					outside.x = clipped_area.x + clipped_area.width;
					outside.width = fix->ps.width - outside.x;
					outside.y = clipped_area.y;
					outside.height = clipped_area.height;
					if ( !check_rect_plain( test_suite, &outside, out ) )
					{
						success = EGL_FALSE;
					}
				}
			}
		}

		/* The expected data might not be big enough to validate the whole
		   check area. */
		clipped_area.width = MIN( clipped_area.width, expected_width );
		clipped_area.height = MIN( clipped_area.height, expected_height );

		/* Check values written within rectangle. If it was completely outside
		   the pixmap then its width or height will have been clipped to 0. */
		if ( clipped_area.width > 0 && clipped_area.height > 0 &&
		     !check_rect_pattern( test_suite, &clipped_area, expected_pixels, expected->pitch ) )
		{
			success = EGL_FALSE;
		}
	}
	return success;
}

/* Write a specified number of uniform pixel values into a buffer. */
MALI_STATIC void generate_pattern( suite *test_suite, void *buffer, EGLint n, u32 pixel_seed, EGLBoolean rotate )
{
	EGLint i;
	const fixture *fix;
	EGLint fi;
	size_t pix_size;

	MALI_DEBUG_ASSERT_POINTER( test_suite );
	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	fi = fix->format_index;
	MALI_DEBUG_ASSERT( fi < MALI_ARRAY_SIZE( pixel_formats ),
	                   ( "Bad format index %d", fi ) );
	pix_size = pixel_formats[fi].size;

	MALI_DEBUG_ASSERT_POINTER( buffer );
	MALI_DEBUG_ASSERT( 0 == MALI_STATIC_CAST(unsigned long)buffer % pix_size,
	                   ( "Misaligned pixel buffer: %p\n", buffer ) );
	MALI_DEBUG_ASSERT( n >= 0, ( "Bad no. of pixels: %d\n", n ) );

	for ( i = 0; i < n; ++i )
	{
		u32 pixel_value;
		if ( fix->ps.alpha )
		{
			/* Alpha channel is supported so write all channels */
			pixel_value = pixel_seed & ( pixel_formats[fi].colour_mask | pixel_formats[fi].alpha_mask );
		}
		else
		{
			/* Alpha channel isn't supported so don't write those bits */
			pixel_value = pixel_seed & pixel_formats[fi].colour_mask;
		}
		switch ( pix_size )
		{
			case sizeof(u8):
			{
				u8 *p8 = buffer;
				p8[i] = MALI_STATIC_CAST(u8)pixel_value;
				break;
			}
			case sizeof(u16):
			{
				u16 *p16 = buffer;
				p16[i] = MALI_STATIC_CAST(u16)pixel_value;
				break;
			}
			case sizeof(u32):
			{
				u32 *p32 = buffer;
				p32[i] = pixel_value;
				break;
			}
			default:
			{
				MALI_DEBUG_ASSERT( 0, ( "Unrecognised pixel size: %u\n", pix_size ) );
				return;
			}
		}
		if ( rotate )
		{
			pixel_seed = ( pixel_seed >> 1 ) | ( ( pixel_seed & 1 ) << 31 );
		}
	}
}

/**
 * @brief Get pixel test callback function.
 *
 * Tests the egl_helper_nativepixmap_get_pixel function on a given pair
 * of coordinates. If they are within the pixmap associated with the text
 * fixture then the corresponding byte(s) will be set before invoking
 * egl_helper_nativepixmap_get_pixel to read back the pixel value.
 *
 * @param[in] context Untyped test suite context pointer.
 * @param[in] x       X coordinate.
 * @param[in] y       Y coordinate.
 * @return    EGL_FALSE to stop the enumeration, or EGL_TRUE to get
 *            the next pair of coordinates (if any).
 */
MALI_STATIC EGLBoolean get_pixel_cb( void *context, EGLint x, EGLint y )
{
	suite *test_suite = context;
	const fixture *fix;
	u32 got_pixel, expected;

	MALI_DEBUG_ASSERT_POINTER( test_suite );
	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	/* Clear the destination pixmap */
	clear_pixmap( fix, 0 );

	/* Check for valid coordinates (required when testing 'corner cases') */
	if ( x >= 0 && x < fix->ps.width && y >= 0 && y < fix->ps.height )
	{
		/* Calculate the address of the pixel to be read */
		u8 *pixels = fix->ps.pixels;

		MALI_DEBUG_ASSERT( fix->format_index < MALI_ARRAY_SIZE( pixel_formats ),
		                   ( "Bad format index %d", fix->format_index ) );

		pixels += ( y * fix->ps.pitch ) + ( x * pixel_formats[fix->format_index].size );

		/* Set the pixel value to be read back. Note that generate_pattern
		 * clears the alpha bits for us if not supported. */
		expected = make_pixel_value( test_suite, PIXEL_VALUE );
		generate_pattern( test_suite, pixels, 1, expected, EGL_FALSE );
	}
	else
	{
		expected = BAD_PIXEL_VALUE;
	}

	/* Read from the same coordinates and check the pixel value */
	got_pixel = egl_helper_nativepixmap_get_pixel( &fix->ps, x, y, fix->ps.format );

	assert_fail( got_pixel == expected, dsprintf( test_suite,
	             "Read pixel value 0x%x at [%d,%d], expected 0x%x",
	             got_pixel, x, y, expected ) );

	return (got_pixel == expected);
}

/* Test that the get pixel function reads the correct memory location */
MALI_STATIC void get_pixel_test( suite *test_suite, coord_generator *make_coords )
{
	MALI_DEBUG_ASSERT_POINTER( test_suite );

	/* Initialise guard zones around all pixmaps */
	mark_all_buffer_zones( test_suite );

	/* Try reading a pixel from each pair of coordinates in turn. */
	if ( NULL != make_coords )
	{
		const fixture *fix = test_suite->fixture;
		pixmap_rect bbox;

		MALI_DEBUG_ASSERT_POINTER( fix );
		bbox.x = 0;
		bbox.y = 0;
		bbox.width = fix->ps.width;
		bbox.height = fix->ps.height;
		make_coords( &bbox, get_pixel_cb, test_suite );
	}

	/* Check that no buffer overruns occurred */
	check_all_buffer_zones( test_suite );
}

/**
 * @brief Write pixel test callback function.
 *
 * Tests the egl_helper_nativepixmap_write_pixel function on a given pair
 * of coordinates by verifying that it updates the corresponding memory
 * location(s) with the expected values.
 *
 * @param[in] context Untyped pointer to pixmap specifier giving
 *                    expected output.
 * @param[in] x       X coordinate.
 * @param[in] y       Y coordinate.
 * @return    EGL_FALSE to stop the enumeration, or EGL_TRUE to get
 *            the next pair of coordinates (if any).
 */
MALI_STATIC EGLBoolean write_pixel_cb( void *context, EGLint x, EGLint y )
{
	const pixmap_spec *expected = context;
	suite *test_suite;
	const fixture *fix;
	pixmap_rect area;
	u32 pixel_value;

	MALI_DEBUG_ASSERT_POINTER( expected );
	test_suite = expected->test_suite;
	MALI_DEBUG_ASSERT_POINTER( test_suite );
	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	/* Clear the destination pixmap */
	clear_pixmap( fix, 0 );

	/* Write to the pixmap at the test coordinates */
	pixel_value = make_pixel_value( test_suite, PIXEL_VALUE );
	egl_helper_nativepixmap_write_pixel( &fix->ps, x, y, pixel_value, fix->ps.format );

	/* Check that the correct area was updated, and none other */
	area.x = x;
	area.y = y;
	area.width = area.height = 1;
	return check_rect( test_suite, &area, 0, expected );
}

/* Test that the write pixel function writes the correct memory location */
MALI_STATIC void write_pixel_test( suite *test_suite, coord_generator *make_coords )
{
	const fixture *fix;
	u32 exp_pixels[1];
	pixmap_spec expected;

	MALI_DEBUG_ASSERT_POINTER( test_suite );
	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	/* Initialise guard zones around pixmap */
	mark_all_buffer_zones( test_suite );

	/* Initialise a buffer with the expected output for this format.
	   Don't use 'pixel_value', which is the wrong type (endianness etc). */
	generate_pattern( test_suite, exp_pixels, MALI_ARRAY_SIZE( exp_pixels ), PIXEL_VALUE, EGL_FALSE );

	/* Use a pixmap specifier to describe the expected output */
	expected.test_suite = test_suite;
	expected.format = fix->ps.format;
	expected.pixels = exp_pixels;
	expected.pitch = 0; /* repeat same row */
	expected.width = MALI_ARRAY_SIZE( exp_pixels );
	expected.height = INT_MAX; /* unlimited */
	expected.alpha = fix->ps.alpha;

	/* Try writing a pixel to each pair of coordinates in turn. */
	if ( NULL != make_coords )
	{
		pixmap_rect bbox;
		bbox.x = 0;
		bbox.y = 0;
		bbox.width = fix->ps.width;
		bbox.height = fix->ps.height;
		make_coords( &bbox, write_pixel_cb, &expected );
	}

	/* Check that no buffer overruns occurred */
	check_all_buffer_zones( test_suite );
}

/* Test that the plain fill function writes the correct memory locations */
EGLBoolean plain_fill_cb( void *context, const pixmap_rect *area )
{
	const fill_test_context *ftc = context;
	suite *test_suite;
	const fixture *fix;

	MALI_DEBUG_ASSERT_POINTER( ftc );

	test_suite = ftc->ps.test_suite;
	MALI_DEBUG_ASSERT_POINTER( test_suite );
	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	/* Clear the destination pixmap */
	clear_pixmap( fix, 0 );

	/* Fill a rectangular area of the pixmap */
	egl_helper_nativepixmap_plain_fill( &fix->ps, area, ftc->pixel_value, fix->ps.format );

	/* Check that the correct area was updated, and none other. */
	return check_rect( test_suite, area, 0, &ftc->ps );
}

MALI_STATIC void plain_fill_test( suite *test_suite, rect_generator *make_rects )
{
	u32 exp_pixels[MAX_FILL_WIDTH];
	fill_test_context ftc;
	const fixture *fix;

	MALI_DEBUG_ASSERT_POINTER( test_suite );

	/* Initialise guard zones around pixmap */
	mark_all_buffer_zones( test_suite );

	/* Initialise a buffer with the expected output for this format
	   (only one row is required because we pass 0 as pitch) */
	generate_pattern( test_suite, exp_pixels, MALI_ARRAY_SIZE( exp_pixels ), PIXEL_VALUE, EGL_FALSE );

	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	/* Use a pixmap specifier to describe the expected output */
	ftc.ps.test_suite = test_suite;
	ftc.ps.format = fix->ps.format;
	ftc.ps.pixels = exp_pixels;
	ftc.ps.pitch = 0; /* repeat same row */
	ftc.ps.width = MALI_ARRAY_SIZE( exp_pixels );
	ftc.ps.height = INT_MAX; /* unlimited */
	ftc.ps.alpha = fix->ps.alpha;

	/* Make pixel value to fill with suitable for pixmap format */
	ftc.pixel_value = make_pixel_value( test_suite, PIXEL_VALUE );

	if ( NULL == make_rects )
	{
		/* Fill a null rectangle (the whole pixmap) */
		plain_fill_cb( &ftc, NULL );
	}
	else
	{
		/* Fill various rectangular areas of the pixmap */
		pixmap_rect bbox;
		bbox.x = 0;
		bbox.y = 0;
		bbox.width = fix->ps.width;
		bbox.height = fix->ps.height;
		make_rects( &bbox, plain_fill_cb, &ftc );
	}

	/* Check that no buffer overruns occurred */
	check_all_buffer_zones( test_suite );
}

/* Test that the bitmap copy function writes the correct memory locations */
EGLBoolean copy_cb( void *context, const pixmap_rect *area )
{
	const fill_test_context *ftc = context;
	suite *test_suite;
	const fixture *fix;

	MALI_DEBUG_ASSERT_POINTER( ftc );

	test_suite = ftc->ps.test_suite;
	MALI_DEBUG_ASSERT_POINTER( test_suite );
	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	/* Clear the destination pixmap */
	clear_pixmap( fix, 0 );

	/* Fill a rectangular area of the pixmap */
	egl_helper_nativepixmap_copy( &fix->ps, area, &ftc->ps );

	/* Check that the correct area was updated, and none other */
	return check_rect( test_suite, area, 0, &ftc->ps );
}

/* Test that the bitmap copy function writes the correct memory locations */
MALI_STATIC void copy_test( suite *test_suite, rect_generator *make_rects )
{
	fixture *fix;
	u32 expected[MAX_FILL_HEIGHT * MAX_FILL_WIDTH];
	fill_test_context ftc;

	MALI_DEBUG_ASSERT_POINTER( test_suite );

	/* Initialise guard zones around pixmap */
	mark_all_buffer_zones( test_suite );

	/* Initialise a buffer with the source data for the copy operation
	   (which will also be used to validate the result) */
	generate_pattern( test_suite, expected, MALI_ARRAY_SIZE( expected ), PIXEL_VALUE, EGL_TRUE );

	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	/* Create a second pixmap to act as the source for the copy */
	ftc.ps.test_suite = test_suite;
	ftc.ps.pixels = expected;
	ftc.ps.width = MAX_FILL_WIDTH;
	ftc.ps.height = MAX_FILL_HEIGHT;

	MALI_DEBUG_ASSERT( fix->format_index < MALI_ARRAY_SIZE( pixel_formats ),
	                   ( "Bad format index %d", fix->format_index ) );

	ftc.ps.pitch = ftc.ps.width * pixel_formats[fix->format_index].size;
	ftc.ps.format = fix->ps.format;

	/* generate_pattern always writes pixel values suitable for the destination
	 * pixmap. If it doesn't support alpha then those bits will be 0 in the
	 * source pixmap and we should tell egl_helper_nativepixmap_copy to ignore
	 * them or it will generate a warning: */
	ftc.ps.alpha = fix->ps.alpha;

	if ( NULL == make_rects )
	{
		/* Fill a null rectangle (the whole pixmap) */
		copy_cb( &ftc, NULL );
	}
	else
	{
		/* Fill various rectangular areas of the pixmap */
		pixmap_rect bbox;
		bbox.x = 0;
		bbox.y = 0;
		bbox.width = fix->ps.width;
		bbox.height = fix->ps.height;
		make_rects( &bbox, copy_cb, &ftc );
	}

	/* Check that no buffer overruns occurred */
	check_all_buffer_zones( test_suite );
}

/**
 * @brief Pixmap fill-by-copy callback.
 *
 * Called back by the egl_helper_nativepixmap_custom_fill function for
 * each pixel within the area to be filled. Looks up the output pixel
 * value in a pixmap accessed through the context pointer.
 *
 * @param[in] context Untyped pointer to source pixmap specifier.
 * @param[in] x       X coordinate of pixel.
 * @param[in] y       Y coordinate of pixel.
 * @return    Pixel value to be written to the pixmap.
 */
MALI_STATIC u32 copy_filler(void *context, EGLint x, EGLint y)
{
	const pixmap_spec *src = context;
	suite *test_suite;
	u32 pixel_value;
	const fixture *fix;
	EGLint fi;

	MALI_DEBUG_ASSERT_POINTER( src );
	test_suite = src->test_suite;
	MALI_DEBUG_ASSERT_POINTER( test_suite );

	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );
	fi = fix->format_index;
	MALI_DEBUG_ASSERT( fi < MALI_ARRAY_SIZE( pixel_formats ),
	                   ( "Bad format index %d", fi ) );

	assert_fail( x >= 0, dsprintf( test_suite, "Bad x coordinate %d", x ) );
	assert_fail( y >= 0, dsprintf( test_suite, "Bad y coordinate %d", y ) );
	if ( x >= 0 && y >= 0 && x < src->width && y < src->height )
	{
		/* Calculate address of start of row within source pixmap */
		u8 *row_start = src->pixels;
		size_t pix_size = pixel_formats[fi].size;

		MALI_DEBUG_ASSERT_POINTER( row_start );
		MALI_DEBUG_ASSERT( 0 == MALI_STATIC_CAST(unsigned long)row_start % pix_size,
		                   ( "Misaligned pixel buffer: %p\n", row_start ) );
		row_start += y * src->pitch;

		switch ( pix_size )
		{
			case sizeof(u8):
			{
				pixel_value = row_start[x];
				break;
			}
			case sizeof(u16):
			{
				u16 *p16 = MALI_REINTERPRET_CAST(u16 *)row_start;
				pixel_value = p16[x];
				break;
			}
			case sizeof(u32):
			{
				u32 *p32 = MALI_REINTERPRET_CAST(u32 *)row_start;
				pixel_value = p32[x];
				break;
			}
			default:
			{
				MALI_DEBUG_ASSERT( 0, ( "Unrecognised pixel size: %u\n", pix_size ) );
				pixel_value = 0;
				break;
			}
		}
	}
	else
	{
		/* Outside source pixmap so just use a plain colour */
		pixel_value = PIXEL_VALUE & ( pixel_formats[fi].colour_mask | pixel_formats[fi].alpha_mask );
	}
	/* If the destination pixmap doesn't support an alpha channel properly then
	 * pretend it is fully opaque or egl_helper_nativepixmap_custom_fill will
	 * generate a warning: */
	if ( !src->alpha )
	{
		pixel_value |= pixel_formats[fi].alpha_mask;
	}
	return pixel_value;
}

/* Test that the custom fill function writes the correct memory locations */
EGLBoolean custom_fill_cb( void *context, const pixmap_rect *area )
{
	const fill_test_context *ftc = context;
	suite *test_suite;
	const fixture *fix;

	MALI_DEBUG_ASSERT_POINTER( ftc );

	test_suite = ftc->ps.test_suite;
	MALI_DEBUG_ASSERT_POINTER( test_suite );
	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	/* Clear the destination pixmap */
	clear_pixmap( fix, 0 );

	/* Fill a rectangular area of the pixmap */
	egl_helper_nativepixmap_custom_fill( &fix->ps, area, &ftc->filler );

	/* Check that the correct area was updated, and none other */
	return check_rect( test_suite, area, 0, &ftc->ps );
}

/* Test that the custom fill function writes the correct memory locations */
MALI_STATIC void custom_fill_test( suite *test_suite, rect_generator *make_rects )
{
	fixture *fix;
	u32 expected[MAX_FILL_HEIGHT * MAX_FILL_WIDTH];
	fill_test_context ftc;

	MALI_DEBUG_ASSERT_POINTER( test_suite );

	/* Initialise guard zones around pixmap */
	mark_all_buffer_zones( test_suite );

	/* Initialise a buffer with the source data for the copy operation
	   (which will also be used to validate the result) */
	generate_pattern( test_suite, expected, MALI_ARRAY_SIZE( expected ), PIXEL_VALUE, EGL_TRUE );

	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	/* Create a second pixmap to act as the source for the copy */
	ftc.ps.test_suite = test_suite;
	ftc.ps.pixels = expected;
	ftc.ps.width = MAX_FILL_WIDTH;
	ftc.ps.height = MAX_FILL_HEIGHT;
	MALI_DEBUG_ASSERT( fix->format_index < MALI_ARRAY_SIZE( pixel_formats ),
	                   ( "Bad format index %d", fix->format_index ) );
	ftc.ps.pitch = ftc.ps.width * pixel_formats[fix->format_index].size;
	ftc.ps.format = fix->ps.format; /* unused */
	ftc.ps.alpha = fix->ps.alpha;

	/* Set up a custom filler with the source pixmap as its context */
	ftc.filler.get_pixel = copy_filler;
	ftc.filler.context = &ftc.ps;
	ftc.filler.format = fix->ps.format;

	if ( NULL == make_rects )
	{
		/* Fill a null rectangle (the whole pixmap) */
		custom_fill_cb( &ftc, NULL );
	}
	else
	{
		/* Fill various rectangular areas of the pixmap */
		pixmap_rect bbox;
		bbox.x = 0;
		bbox.y = 0;
		bbox.width = fix->ps.width;
		bbox.height = fix->ps.height;
		make_rects( &bbox, custom_fill_cb, &ftc );
	}

	/* Check that no buffer overruns occurred */
	check_all_buffer_zones( test_suite );
}

/* Test getting pixels in different formats */
MALI_STATIC void format_get_pixel_test( suite *test_suite )
{
	const fixture *fix;
	EGLint f;

	MALI_DEBUG_ASSERT_POINTER( test_suite );
	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	/* Initialise guard zones around all pixmaps */
	mark_all_buffer_zones( test_suite );

	/* Try each pixel format in turn */
	for ( f = 0; f < MALI_ARRAY_SIZE( pixel_formats ); ++f )
	{
		u32 pixel_value;

		if ( f == fix->format_index )
		{
			continue; /* skip identical pixel format */
		}

		/* Try reading a pixel from the pixmap */
		pixel_value = egl_helper_nativepixmap_get_pixel( &fix->ps, 0, 0, pixel_formats[f].format );
		assert_fatal( pixel_value == BAD_PIXEL_VALUE, dsprintf( test_suite,
		              "Read pixel value 0x%x for format %d, expected 0x%x",
		              pixel_value, f, BAD_PIXEL_VALUE ) );
	}

	/* Check that no buffer overruns occurred */
	check_all_buffer_zones( test_suite );
}

/* Test writing pixels in different formats */
MALI_STATIC void format_write_pixel_test( suite *test_suite )
{
	const fixture *fix;
	EGLint f;
	pixmap_rect area;
	u32 expected[1];

	MALI_DEBUG_ASSERT_POINTER( test_suite );
	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	/* Initialise guard zones around all pixmaps */
	mark_all_buffer_zones( test_suite );

	/* Area to check for modification is whole pixmap */
	area.x = 0;
	area.y = 0;
	area.width = fix->ps.width;
	area.height = fix->ps.height;

	/* Try each pixel format in turn */
	for ( f = 0; f < MALI_ARRAY_SIZE( pixel_formats ); ++f )
	{
		u32 pixel_value;

		if ( f == fix->format_index )
		{
			continue; /* skip identical pixel format */
		}

		/* Clear the destination pixmap */
		clear_pixmap( fix, 0 );

		/* Initialise a buffer with the expected output for this format */
		generate_pattern( test_suite, &expected, MALI_ARRAY_SIZE(expected), PIXEL_VALUE, EGL_FALSE );

		/* Try writing a pixel to the target pixmap */
		egl_helper_nativepixmap_write_pixel( &fix->ps, 0, 0, pixel_value, pixel_formats[f].format );

		/* Writing a pixel to an incompatible pixmap should do nothing */
		if ( !check_rect_plain( test_suite, &area, 0 ) )
		{
			break;
		}
	}

	/* Check that no buffer overruns occurred */
	check_all_buffer_zones( test_suite );
}

/* Test plain fills with colours specified in different formats */
MALI_STATIC void format_plain_fill_test( suite *test_suite )
{
	const fixture *fix;
	EGLint f;
	pixmap_rect area;
	u32 expected[FORMAT_FILL_WIDTH];

	MALI_DEBUG_ASSERT_POINTER( test_suite );
	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	/* Initialise guard zones around all pixmaps */
	mark_all_buffer_zones( test_suite );

	/* Area to check for modification is whole pixmap */
	area.x = 0;
	area.y = 0;
	area.width = fix->ps.width;
	area.height = fix->ps.height;

	/* Try each pixel format in turn */
	for ( f = 0; f < MALI_ARRAY_SIZE( pixel_formats ); ++f )
	{
		u32 pixel_value;

		if ( f == fix->format_index )
		{
			continue; /* skip identical pixel format */
		}

		/* Clear the destination pixmap */
		clear_pixmap( fix, 0 );

		/* Mask the pixel value appropriately for the pixel format */
		pixel_value = pixel_formats[f].alpha_mask | (PIXEL_VALUE & pixel_formats[f].colour_mask);

		/* Initialise a buffer with the expected output for this format */
		generate_pattern( test_suite, &expected, MALI_ARRAY_SIZE(expected), PIXEL_VALUE, EGL_FALSE );

		/* Try filling the pixmap */
		egl_helper_nativepixmap_plain_fill( &fix->ps, &area, pixel_value, pixel_formats[f].format );

		/* Filling an incompatible pixmap should do nothing */
		if ( !check_rect_plain( test_suite, &area, 0 ) )
		{
			break;
		}
	}

	/* Check that no buffer overruns occurred */
	check_all_buffer_zones( test_suite );
}

/* Test copying from pixmaps of different formats */
MALI_STATIC void format_copy_test( suite *test_suite )
{
	const fixture *fix;
	EGLint f;
	pixmap_rect area;
	u32 expected[FORMAT_FILL_WIDTH];
	pixmap_spec src;

	MALI_DEBUG_ASSERT_POINTER( test_suite );
	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	/* Initialise guard zones around all pixmaps */
	mark_all_buffer_zones( test_suite );

	/* Create a second pixmap to act as the source for the copy */
	src.test_suite = test_suite;
	src.pixels = expected;
	src.width = FORMAT_FILL_WIDTH;
	src.height = FORMAT_FILL_HEIGHT;
	src.pitch = 0; /* repeat the first row */
	src.alpha = EGL_TRUE;

	/* Area to check for modification is whole pixmap */
	area.x = 0;
	area.y = 0;
	area.width = fix->ps.width;
	area.height = fix->ps.height;

	/* Try each pixel format in turn */
	for ( f = 0; f < MALI_ARRAY_SIZE( pixel_formats ); ++f )
	{
		u32 pixel_value;

		if ( f == fix->format_index )
		{
			continue; /* skip identical pixel format */
		}

		/* Set the source pixmap format */
		src.format = pixel_formats[f].format;

		/* Clear the destination pixmap */
		clear_pixmap( fix, 0 );

		/* Mask the pixel value appropriately for the pixel format */
		pixel_value = pixel_formats[f].alpha_mask | (PIXEL_VALUE & pixel_formats[f].colour_mask);

		/* Initialise a buffer with the expected output for this format
		   (only one row is required because we pass 0 to check_rect as exp_pitch) */
		generate_pattern( test_suite, expected, MALI_ARRAY_SIZE(expected), PIXEL_VALUE, EGL_FALSE );

		/* Try copying pixels from the second to the first pixmap */
		egl_helper_nativepixmap_copy( &fix->ps, &area, &src );

		/* Copying to an incompatible pixmap should do nothing */
		if ( !check_rect_plain( test_suite, &area, 0 ) )
		{
			break;
		}
	}

	/* Check that no buffer overruns occurred */
	check_all_buffer_zones( test_suite );
}

/**
 * @brief Pixmap simple-filler callback.
 *
 * Called back by the egl_helper_nativepixmap_custom_fill function for
 * each pixel within the area to be filled. Always returns the same
 * pixel value.
 *
 * @param[in] context Pointer to the pixel value to return.
 * @param[in] x       X coordinate of pixel.
 * @param[in] y       Y coordinate of pixel.
 * @return    Pixel value to be written to the pixmap.
 */
MALI_STATIC u32 trivial_filler(void *context, EGLint x, EGLint y)
{
	u32 *pixel_value = context;

	MALI_DEBUG_ASSERT_POINTER( pixel_value );
	return *pixel_value;
}

/* Test custom filling with colours specified in different formats */
MALI_STATIC void format_custom_fill_test( suite *test_suite )
{
	const fixture *fix;
	EGLint f;
	pixmap_rect area;
	u32 expected[FORMAT_FILL_WIDTH], pixel_value;
	pixmap_fill filler;

	MALI_DEBUG_ASSERT_POINTER( test_suite );
	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	/* Initialise guard zones around all pixmaps */
	mark_all_buffer_zones( test_suite );

	/* Area to check for modification is whole pixmap */
	area.x = 0;
	area.y = 0;
	area.width = fix->ps.width;
	area.height = fix->ps.height;

	filler.get_pixel = trivial_filler;
	filler.context = &pixel_value;

	/* Try each pixel format in turn */
	for ( f = 0; f < MALI_ARRAY_SIZE( pixel_formats ); ++f )
	{
		if ( f == fix->format_index )
		{
			continue; /* skip identical pixel format */
		}

		/* Set the source pixel format */
		filler.format = pixel_formats[f].format;

		/* Clear the destination pixmap */
		clear_pixmap( fix, 0 );

		/* Mask the pixel value appropriately for the pixel format */
		pixel_value = PIXEL_VALUE & pixel_formats[f].colour_mask;

		/* Initialise a buffer with the expected output for this format
		   (only one row is required because we pass 0 to check_rect as exp_pitch) */
		generate_pattern( test_suite, expected, MALI_ARRAY_SIZE(expected), PIXEL_VALUE, EGL_FALSE );

		/* Try filling the pixmap via a callback */
		egl_helper_nativepixmap_custom_fill( &fix->ps, &area, &filler );

		/* Custom-filling an incompatible pixmap should do nothing */
		if ( !check_rect_plain( test_suite, &area, 0 ) )
		{
			break;
		}
	}

	/* Check that no buffer overruns occurred */
	check_all_buffer_zones( test_suite );
}

/* Test that we get back a value written to each pixel of the pixmap */
MALI_STATIC void write_get_pixel_test( suite *test_suite )
{
	const fixture *fix;
	EGLint x, y, width, height;

	MALI_DEBUG_ASSERT_POINTER( test_suite );
	fix = test_suite->fixture;
	MALI_DEBUG_ASSERT_POINTER( fix );

	/* Initialise guard zones around all pixmaps */
	mark_all_buffer_zones( test_suite );

	width = fix->ps.width;
	height = fix->ps.height;

	/* Clear the destination pixmap */
	clear_pixmap( fix, 0 );

	/* Test every possible location within the pixmap in turn */
	for ( y = 0; y < height; ++y )
	{
		for ( x = 0; x < width; ++x )
		{
			u32 pixel_value, got_pixel;

			/* Synthesise a fairly unique pixel value from the coordinates */
			pixel_value = make_pixel_value( test_suite,  x + y );

			/* Write the pixel value then immediately read it back */
			egl_helper_nativepixmap_write_pixel( &fix->ps, x, y, pixel_value, fix->ps.format );
			got_pixel = egl_helper_nativepixmap_get_pixel( &fix->ps, x, y, fix->ps.format );

			assert_fatal( got_pixel == pixel_value, dsprintf( test_suite,
			              "Read back pixel value 0x%x at [%d,%d], expected 0x%x",
			              got_pixel, x, y, pixel_value ) );
		}
	}

	/* Check that no buffer overruns occurred */
	check_all_buffer_zones( test_suite );
}

/* Try getting a pixel from each pair of non-clipped coordinates in turn. */
MALI_STATIC void simple_get_pixel_test( suite *test_suite )
{
	get_pixel_test( test_suite, gen_coords);
}

/* Try writing a pixel to each pair of non-clipped coordinates in turn. */
MALI_STATIC void simple_write_pixel_test( suite *test_suite )
{
	write_pixel_test( test_suite, gen_coords );
}

/* Try plain-filling every valid non-clipped rectangle in turn. */
MALI_STATIC void simple_plain_fill_test( suite *test_suite )
{
	plain_fill_test( test_suite, gen_rects );
}

/* Try pattern-filling every valid non-clipped rectangle in turn. */
MALI_STATIC void simple_copy_test( suite *test_suite )
{
	copy_test( test_suite, gen_rects );
}

/* Try custom-filling every valid non-clipped rectangle in turn. */
MALI_STATIC void simple_custom_fill_test( suite *test_suite )
{
	custom_fill_test( test_suite, gen_rects );
}

/* Try getting a pixel from each pair of clipped coordinates in turn. */
MALI_STATIC void clipped_get_pixel_test( suite *test_suite )
{
	get_pixel_test( test_suite, gen_border_coords );
}

/* Try writing a pixel to each pair of clipped coordinates in turn. */
MALI_STATIC void clipped_write_pixel_test( suite *test_suite )
{
	write_pixel_test( test_suite, gen_border_coords );
}

/* Try plain-filling every valid clipped rectangle in turn. */
MALI_STATIC void clipped_plain_fill_test( suite *test_suite )
{
	plain_fill_test( test_suite, gen_clipped_rects );
}

/* Try pattern-filling every valid clipped rectangle in turn. */
MALI_STATIC void clipped_copy_test( suite *test_suite )
{
	copy_test( test_suite, gen_clipped_rects );
}

/* Try custom-filling every valid clipped rectangle in turn. */
MALI_STATIC void clipped_custom_fill_test( suite *test_suite )
{
	custom_fill_test( test_suite, gen_clipped_rects );
}

/* Try getting a pixel from each pair of 'corner case' coordinates in turn. */
MALI_STATIC void corner_get_pixel_test( suite *test_suite )
{
	get_pixel_test( test_suite, gen_cc_coords );
}

/* Try writing a pixel to each pair of 'corner case' coordinates in turn. */
MALI_STATIC void corner_write_pixel_test( suite *test_suite )
{
	write_pixel_test( test_suite, gen_cc_coords );
}

/* Test that the plain fill function doesn't misbehave in corner cases */
MALI_STATIC void corner_plain_fill_test( suite *test_suite )
{
	plain_fill_test( test_suite, gen_cc_rects );
}

/* Test that the pattern fill function doesn't misbehave in corner cases */
MALI_STATIC void corner_copy_test( suite *test_suite )
{
	copy_test( test_suite, gen_cc_rects );
}

/* Test that the custom fill function doesn't misbehave in corner cases */
MALI_STATIC void corner_custom_fill_test( suite *test_suite )
{
	custom_fill_test( test_suite, gen_cc_rects );
}

/* Test that the plain fill function doesn't misbehave with crazy rectangles */
MALI_STATIC void limits_plain_fill_test( suite *test_suite )
{
	plain_fill_test( test_suite, gen_crazy_rects );
}

/* Test that the pixmap copy function doesn't misbehave with crazy rectangles */
MALI_STATIC void limits_copy_test( suite *test_suite )
{
	copy_test( test_suite, gen_crazy_rects );
}

/* Test that the custom fill function doesn't misbehave with crazy rectangles */
MALI_STATIC void limits_custom_fill_test( suite *test_suite )
{
	custom_fill_test( test_suite, gen_crazy_rects );
}

/* Test that the plain fill function doesn't misbehave with null rectangles */
MALI_STATIC void null_plain_fill_test( suite *test_suite )
{
	plain_fill_test( test_suite, NULL );
}

/* Test that the pixmap copy function doesn't misbehave with null rectangles */
MALI_STATIC void null_copy_test( suite *test_suite )
{
	copy_test( test_suite, NULL );
}

/* Test that the custom fill function doesn't misbehave with null rectangles */
MALI_STATIC void null_custom_fill_test( suite *test_suite )
{
	custom_fill_test( test_suite, NULL );
}

MALI_STATIC void *create_fixture( suite *test_suite )
{
	fixture *fix;
	mempool *pool;

	MALI_DEBUG_ASSERT_POINTER( test_suite );
	pool = &test_suite->fixture_pool;

	fix = _test_mempool_alloc( pool, sizeof( *fix ) );

	if ( NULL != fix )
	{
		EGLint di, fi;
		size_t row_size;

		/* Test each combination of pixmap size, pixel format and alpha channel support. */
		di = test_suite->fixture_index / ( MALI_ARRAY_SIZE( pixel_formats ) * 2 );
		fi = test_suite->fixture_index % ( MALI_ARRAY_SIZE( pixel_formats ) * 2 );
		if ( fi < MALI_ARRAY_SIZE( pixel_formats ) )
		{
			fix->ps.alpha = EGL_TRUE;
		}
		else
		{
			fi -= MALI_ARRAY_SIZE( pixel_formats );
			fix->ps.alpha = EGL_FALSE;
		}

		fix->format_index = fi;
		fix->ps.test_suite = test_suite;

		MALI_DEBUG_ASSERT( di < MALI_ARRAY_SIZE( pixmap_dims ), ( "Bad pixmap dimensions index %d", di ) );
		fix->ps.width = pixmap_dims[di].width;
		fix->ps.height = pixmap_dims[di].height;

		MALI_DEBUG_ASSERT( fi < MALI_ARRAY_SIZE( pixel_formats ), ( "Bad format index %d", fi ) );
		fix->ps.format = pixel_formats[fi].format;

		/* Excessive pitch and top/bottom check zones to allow detection
		 * of illegal writes outside the pixmap.
		 */
		row_size = (fix->ps.width + CHECK_ZONE) * pixel_formats[fi].size;
		fix->ps.pitch = MALI_STATIC_CAST(EGLint)row_size;

		/* Allocate memory for the pixmap and surrounding check zones */
		fix->buf_size = row_size * (CHECK_ZONE * 2 + fix->ps.height);
		fix->buffer = _test_mempool_alloc( pool, fix->buf_size );
		if ( NULL == fix->buffer )
		{
			fix = NULL;
		}
		else
		{
			/* Actual pixmap starts after the top check zone. */
			fix->ps.pixels = MALI_REINTERPRET_CAST(u8 *)fix->buffer + fix->ps.pitch * CHECK_ZONE;

			/* Generate user-visible name of this fixture */
			_mali_sys_snprintf( fix->name, sizeof(fix->name),
			                    "%d format:%d width:%d height:%d pitch:%d alpha:%s",
			                    test_suite->fixture_index, fix->ps.format,
			                    fix->ps.width, fix->ps.height, fix->ps.pitch,
			                    fix->ps.alpha ? "yes" : "no" );

			set_fixture_name( test_suite, fix->name );
		}
	}
	return fix;
}

int run_ehp_suite( int mask, test_specifier *test_spec )
{
	int worst_error = 0;
	mempool pool;
	result_set *results = NULL;
	suite *new_suite;
	const static struct
	{
		const char    *name;
		test_function  execute;
	}
	tests[] =
	{
		TEST_LIST_ENTRY( format_get_pixel ),
		TEST_LIST_ENTRY( format_write_pixel ),
		TEST_LIST_ENTRY( format_plain_fill ),
		TEST_LIST_ENTRY( format_copy ),
		TEST_LIST_ENTRY( format_custom_fill ),
		TEST_LIST_ENTRY( write_get_pixel ),
		TEST_LIST_ENTRY( simple_get_pixel ),
		TEST_LIST_ENTRY( simple_write_pixel ),
		TEST_LIST_ENTRY( simple_plain_fill ),
		TEST_LIST_ENTRY( simple_copy ),
		TEST_LIST_ENTRY( simple_custom_fill ),
		TEST_LIST_ENTRY( clipped_get_pixel ),
		TEST_LIST_ENTRY( clipped_write_pixel ),
		TEST_LIST_ENTRY( clipped_plain_fill ),
		TEST_LIST_ENTRY( clipped_copy ),
		TEST_LIST_ENTRY( clipped_custom_fill ),
		TEST_LIST_ENTRY( corner_get_pixel ),
		TEST_LIST_ENTRY( corner_write_pixel ),
		TEST_LIST_ENTRY( corner_plain_fill ),
		TEST_LIST_ENTRY( corner_copy ),
		TEST_LIST_ENTRY( corner_custom_fill ),
		TEST_LIST_ENTRY( limits_plain_fill ),
		TEST_LIST_ENTRY( limits_copy ),
		TEST_LIST_ENTRY( limits_custom_fill ),
		TEST_LIST_ENTRY( null_plain_fill ),
		TEST_LIST_ENTRY( null_copy ),
		TEST_LIST_ENTRY( null_custom_fill )
	};
	EGLint i;

	/* Initialize a memory pool to be used for the result set and
	   test suite. */
	if ( MEM_OK != _test_mempool_init( &pool, 0 ) )
	{
		MALI_DEBUG_PRINT( 0, ( "Fatal error initializing memory pool\n" ) );
		return result_status_fatal;
	}

	/* Create and initialize a new, empty result set.
	   This is an absolute requirement of the execute function. */
	results = create_result_set( &pool );
	if ( NULL == results )
	{
		MALI_DEBUG_PRINT( 0, ( "Fatal error creating result set\n" ) );
		return result_status_fatal;
	}

	/* Create a new test suite.
	   We don't need to specify fixture creator/destructor functions. */
	new_suite = create_suite( &pool, "egl_helper_nativepixmap", create_fixture, NULL, results );
	if ( NULL == new_suite )
	{
		MALI_DEBUG_PRINT( 0, ( "Fatal error creating test suite\n" ) );
		return result_status_fatal;
	}

	/* We will use one fixture per combination of pixel format, pixmap
	 * dimensions, and alpha channel supported/unsupported.
	 */
	set_fixture_variants( new_suite, MALI_ARRAY_SIZE( pixel_formats ) * MALI_ARRAY_SIZE( pixmap_dims ) * 2 );

	/* Add tests to the suite */
	for ( i = 0; i < MALI_ARRAY_SIZE( tests ); ++i )
	{
		add_test( new_suite, tests[i].name, tests[i].execute );
	}

	switch ( test_spec->mode )
	{
		case MODE_EXECUTE:
			execute( new_suite, test_spec );
			break;

		case MODE_LIST_SUITES:
		case MODE_LIST_TESTS:
		case MODE_LIST_FIXTURES:
			list( new_suite, test_spec );
			break;

		default:
			MALI_DEBUG_PRINT( 0, ( "Unrecognised test specifier mode %d\n", test_spec->mode ) );
			break;
	}
	remove_suite( new_suite );

	/* Print results and find the most serious error */
	worst_error = print_results( results, mask );

	remove_result_set( results );
	_test_mempool_destroy( &pool );

	return worst_error;
}

int main( int argc, char *argv[] )
{
	return test_main( run_ehp_suite, NULL, argc, argv );
}
