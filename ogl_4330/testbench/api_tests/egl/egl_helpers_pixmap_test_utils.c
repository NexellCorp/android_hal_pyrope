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
 * @file egl_helpers_pixmap_test_utils.c
 * Utility functions used by the test suite for the platform-independent
 * native pixmap helper functions.
 */

#include <limits.h>
#include <stddef.h>
#include <mali_system.h>
#include <EGL/egl.h>
#include "egl_helpers_pixmap.h"
#include "egl_helpers_pixmap_test_utils.h"

/**
 * @brief Debugging output level for this module.
 *
 * Unless overridden at compile time, only messages at level 0 or 1 are
 * printed (see mali_debug.h).
 */
#ifndef EHNPTU_DEBUG_LEVEL
#define EHNPTU_DEBUG_LEVEL 2
#endif /* EHNPTU_DEBUG_LEVEL */

/**
 * Miscellaneous numeric constants.
 */
enum
{
	MIN_CC_OFFSET = -1, /**< Minimum offset to apply to base corner cases. */
	MAX_CC_OFFSET = 1,  /**< Maximum offset to apply to base corner cases. */
	MIN_STEP = 1,       /**< Minimum distance between generated coordinates. */
	STEP_INC = 1,       /**< Linear increase in the distance between generated
	                         coordinates. */
	MAX_STEP = 5,       /**< Maximum distance between generated coordinates. */
	BORDER_OVERLAP = 16 /**< Maximum distance outside pixmap to generate
	                         coordinates (when testing clipping) */
};

/**
 * @brief Rectangle generator context.
 *
 * We use two strategies to avoid generating invalid rectangles if valid
 * is set to EGL_TRUE: Preferably, don't waste time generating them in the
 * first place (gen_rects_x1_y1 and gen_clipped_rects_x1_y1). If that
 * isn't supported (gen_cc_rects_x1_y1), then they are discarded in
 * gen_rects_x2_y2.
 */
typedef struct gen_rects_context
{
	const pixmap_rect *bbox; /**< Usually points to the bounding box used to
	                              generate the current rectangle's origin, but
	                              faked in the case of gen_border_coords. */
	EGLint         x, y;     /**< Origin of rectangle currently being generated. */
	rect_callback *callback; /**< Function to be called back with rectangles. */
	void          *context;  /**< Untyped context argument for callback function. */
	EGLBoolean     valid;    /**< Allows delivery of invalid rectangles if
	                              set to EGL_FALSE, which is typically slower. */
}
gen_rects_context;

/* Check memory for byte value. */
size_t check_bytes( const void *mem, EGLint value, size_t n )
{
	const u8 *bytes = mem;
	size_t ndiff = 0;

	MALI_DEBUG_ASSERT_POINTER( bytes );
	while ( n-- > 0 )
	{
		if ( bytes[n] != value )
		{
			ndiff ++;
		}
	}
	return ndiff;
}

/* Compare two blocks of memory. */
size_t compare_bytes( const void *mem1, const void *mem2, size_t n )
{
	const u8 *bytes1 = mem1, *bytes2 = mem2;
	size_t ndiff = 0;

	MALI_DEBUG_ASSERT_POINTER( bytes1 );
	MALI_DEBUG_ASSERT_POINTER( bytes2 );
	while ( n-- > 0 )
	{
		if ( bytes1[n] != bytes2[n] )
		{
			ndiff ++;
		}
	}
	return ndiff;
}

/* Assert that a rectangle is valid */
void assert_rect_valid( const pixmap_rect *area )
{
	MALI_DEBUG_ASSERT_POINTER( area );
	MALI_DEBUG_ASSERT( area->width >= 0,
	                   ( "Bad width %d", area->width ) );

	MALI_DEBUG_ASSERT( area->x <= INT_MAX - area->width,
	                   ( "Bad x %d", area->x ) );

	MALI_DEBUG_ASSERT( area->height >= 0,
	                   ( "Bad height %d", area->height ) );

	MALI_DEBUG_ASSERT( area->y <= INT_MAX - area->height,
	                   ( "Bad y %d", area->y ) );
}

/* Assert that a rectangle is entirely within a pixmap. */
void assert_rect_within_pixmap( const pixmap_rect *area, const pixmap_spec *pixmap )
{
	MALI_DEBUG_ASSERT_POINTER( pixmap );
	MALI_DEBUG_ASSERT( pixmap->width >= 0,
	                   ( "Bad pixmap width %d", pixmap->width ) );

	MALI_DEBUG_ASSERT( pixmap->height >= 0,
	                   ( "Bad pixmap height %d", pixmap->height ) );

	assert_rect_valid( area );
	MALI_DEBUG_ASSERT( area->x >= 0 && ( 0 == pixmap->width || area->x < pixmap->width ),
	                   ( "Bad x %d", area->x ) );

	MALI_DEBUG_ASSERT( area->x <= INT_MAX - area->width && area->x + area->width <= pixmap->width,
	                   ( "Bad width %d", area->width ) );

	MALI_DEBUG_ASSERT( area->y >= 0 && ( 0 == pixmap->height || area->y < pixmap->height ),
	                   ( "Bad y %d", area->y ) );

	MALI_DEBUG_ASSERT( area->y <= INT_MAX - area->height && area->y + area->height <= pixmap->height,
	                   ( "Bad height %d", area->height ) );
}

/* Generate a set of 'corner case' coordinates for a given bounding box. */
EGLBoolean gen_cc_coords( const pixmap_rect *bbox, coord_callback *callback, void *context )
{
	EGLint x_cc[] = { INT_MIN, 0, 0, INT_MAX};
	EGLint y_cc[] = { INT_MIN, 0, 0, INT_MAX};
	EGLint yi;

	assert_rect_valid( bbox );

	MALI_DEBUG_PRINT( EHNPTU_DEBUG_LEVEL, 
	 ( "gen_cc_coords %d,%d,%d,%d\n",
	   bbox->x, bbox->y, bbox->width, bbox->height ) );

	x_cc[1] = bbox->x;
	y_cc[1] = bbox->y;
	x_cc[2] = bbox->width - 1;
	y_cc[2] = bbox->height - 1;

	/* Iterate through the array of base y coordinates */
	for ( yi = 0; yi < MALI_ARRAY_SIZE( y_cc ); ++yi )
	{
		/* Apply a range of small offsets to the y coordinate */
		EGLint yo = MIN_CC_OFFSET, yo_limit = MAX_CC_OFFSET;
		if ( INT_MIN == y_cc[yi] )
		{
			yo = 0; /* can't go smaller than INT_MIN */
		}
		else if ( INT_MAX == y_cc[yi] )
		{
			yo_limit = 0; /* can't go bigger than INT_MAX */
		}
		for ( ; yo <= yo_limit; ++yo )
		{
			EGLint xi, y = y_cc[yi] + yo;

			/* Iterate through the array of base x coordinates */
			for ( xi = 0; xi < MALI_ARRAY_SIZE( x_cc ); ++xi )
			{
				/* Apply a range of small offsets to the x coordinate */
				EGLint xo = MIN_CC_OFFSET, xo_limit = MAX_CC_OFFSET;
				if ( INT_MIN == x_cc[xi] )
				{
					xo = 0; /* can't go smaller than INT_MIN */
				}
				else if ( INT_MAX == x_cc[xi] )
				{
					xo_limit = 0; /* can't go bigger than INT_MAX */
				}
				for ( ; xo <= xo_limit; ++xo )
				{
					EGLint x = x_cc[xi] + xo;
					/* Do a callback to deliver the coordinates */
					if ( NULL != callback && !callback( context, x, y ) )
					{
						return EGL_FALSE; /* generation aborted */
					}
				}
			}
		}
	}
	return EGL_TRUE; /* generator completed */
}

/* Generate a set of coordinates within a given bounding box. */
EGLBoolean gen_coords( const pixmap_rect *bbox, coord_callback *callback, void *context )
{
	EGLint y, y_step = 0, y_limit;

	assert_rect_valid( bbox );

	MALI_DEBUG_PRINT( EHNPTU_DEBUG_LEVEL,
	  ( "gen_coords %d,%d,%d,%d\n",
	    bbox->x, bbox->y, bbox->width, bbox->height ) );

	y_step = 0;
	y_limit = bbox->y + bbox->height;
	for ( y = bbox->y; y < y_limit; y += y_step )
	{
		EGLint x, x_step = 0, x_limit = bbox->x + bbox->width;
		for ( x = bbox->x; x < x_limit; x += x_step )
		{
			/* Do a callback to deliver the coordinates */
			if ( NULL != callback && !callback( context, x, y ) )
			{
				return EGL_FALSE; /* generator aborted */
			}
			if ( 0 == x_step )
			{
				x_step = MIN_STEP;
			}
			else
			{
				x_step += STEP_INC;
				if ( x_step > MAX_STEP )
				{
					x_step = MIN_STEP;
				}
			}
		}
		if ( 0 == y_step )
		{
			y_step = MIN_STEP;
		}
		else
		{
			y_step += STEP_INC;
			if ( y_step > MAX_STEP )
			{
				y_step = MIN_STEP;
			}
		}
	}
	return EGL_TRUE; /* generator completed */
}

/* Generate a set of coordinates within a fixed distance of a given bounding
 * box, on the outside.
 */
EGLBoolean gen_border_coords( const pixmap_rect *bbox, coord_callback *callback, void *context )
{
	pixmap_rect border;
	EGLint bbox_bottom, bbox_right, left_border_width, right_border_width;

	assert_rect_valid( bbox );

	MALI_DEBUG_PRINT( EHNPTU_DEBUG_LEVEL,
	  ( "gen_border_coords %d,%d,%d,%d\n",
	    bbox->x, bbox->y, bbox->width, bbox->height ) );

	/* Calculate absolute position of righthand and bottom edge
	   (should be safe after assert_rect_valid) */
	bbox_right = bbox->x + bbox->width;
	bbox_bottom = bbox->y + bbox->height;

	/* Calculate horizontal extent of borders */
	left_border_width = ( bbox->x >= INT_MIN + BORDER_OVERLAP) ?
	                    BORDER_OVERLAP : bbox->x - INT_MIN;

	right_border_width = ( bbox_right <= INT_MAX - BORDER_OVERLAP ) ?
	                     BORDER_OVERLAP : INT_MAX - bbox_right;

	/* Top and bottom borders extend horizontally to join left and right
	   borders */
	border.x = bbox->x - left_border_width;
	border.width = left_border_width + bbox->width + right_border_width;

	/* Generate coordinates in top border */
	border.height = ( bbox->y >= INT_MIN + BORDER_OVERLAP) ?
	                BORDER_OVERLAP : bbox->y - INT_MIN;

	border.y = bbox->y - border.height;
	if ( NULL != callback && !gen_coords( &border, callback, context ) )
	{
		return EGL_FALSE; /* generator aborted */
	}

	/* Generate coordinates in bottom border */
	border.height = ( bbox_bottom <= INT_MAX - BORDER_OVERLAP ) ?
	                BORDER_OVERLAP : INT_MAX - bbox_bottom;

	border.y = bbox_bottom;
	if ( NULL != callback && !gen_coords( &border, callback, context ) )
	{
		return EGL_FALSE; /* generator aborted */
	}

	/* Left and right borders don't extend vertically to overlap top and
	   bottom borders */
	border.y = bbox->y;
	border.height = bbox->height;

	/* Generate coordinates in left border */
	border.x = bbox->x - left_border_width;
	border.width = left_border_width;
	if ( NULL != callback && !gen_coords( &border, callback, context ) )
	{
		return EGL_FALSE; /* generator aborted */
	}

	/* Generate coordinates in right border */
	border.x = bbox_right;
	border.width = right_border_width;
	if ( NULL != callback && !gen_coords( &border, callback, context ) )
	{
		return EGL_FALSE; /* generator aborted */
	}
	return EGL_TRUE; /* generator completed */
}

/**
 * @brief Rectangle generator bottom-right callback function.
 *
 * Completes a rectangle by setting its width and height from the given
 * coordinates, which are treated as the position of its bottom right corner
 * (but exclusive). Calls a function of type rect_callback to deliver the
 * completed rectangle.
 *
 * @param[in] context Untyped pointer to rectangle generator context.
 * @param[in] x       X coordinate.
 * @param[in] y       Y coordinate.
 * @return    EGL_FALSE to stop the coordinate generator, or EGL_TRUE to get
 *            the next pair of coordinates (if any).
 * @pre The position of the rectangle's top left corner must have already
 *      been recorded in the specified rectangle generator context.
 */
MALI_STATIC EGLBoolean gen_rects_x2_y2( void *context, EGLint x, EGLint y )
{
	gen_rects_context *grc = context;
	EGLBoolean cont = EGL_TRUE;

	MALI_DEBUG_ASSERT_POINTER( grc );

	/* Calculate the width and height of the final rectangle.
	   They may be invalid because this function is used by gen_cc_coords. */
	if ( !grc->valid ||
		   ( x >= grc->x && (unsigned)x - grc->x <= INT_MAX &&
		     y >= grc->y && (unsigned)y - grc->y <= INT_MAX ) )
	{
		pixmap_rect area;

		MALI_DEBUG_PRINT( EHNPTU_DEBUG_LEVEL, 
		  ( "gen_rects_x2_y2 accepted coords [%d,%d][%d,%d]\n",
		    grc->x, grc->y, x, y ) );

		area.x = grc->x;
		area.y = grc->y;
		area.width = x - grc->x;
		area.height = y - grc->y;

		/* Do a callback to deliver the rectangle. */
		if ( NULL != grc->callback )
		{
			cont = grc->callback( grc->context, &area );
		}
	}
	else
	{
		MALI_DEBUG_PRINT( EHNPTU_DEBUG_LEVEL, 
		  ( "gen_rects_x2_y2 rejected coords [%d,%d][%d,%d]\n",
		    grc->x, grc->y, x, y ) );
	}
	return cont;
}

/**
 * @brief Rectangle generator top-left callback.
 *
 * Uses each pair of coordinates generated by gen_coords or
 * gen_border_coords as the top-left corner of a new rectangle, and calls
 * gen_coords (possible recursion) to generate alternative bottom-right
 * corners.
 *
 * @param[in] context Untyped pointer to rectangle generator context.
 * @param[in] x       X coordinate.
 * @param[in] y       Y coordinate.
 * @return    EGL_FALSE to stop the coordinate generator, or EGL_TRUE to get
 *            the next pair of coordinates (if any).
 */
MALI_STATIC EGLBoolean gen_rects_x1_y1( void *context, EGLint x, EGLint y )
{
	gen_rects_context *grc = context;
	pixmap_rect bbox;

	MALI_DEBUG_PRINT( EHNPTU_DEBUG_LEVEL, ( "gen_rects_x1_y1 %d,%d\n", x, y ) );
	MALI_DEBUG_ASSERT_POINTER( grc );
	assert_rect_valid( grc->bbox );
	MALI_DEBUG_ASSERT( x >= grc->bbox->x && x < grc->bbox->x + grc->bbox->width, ( "Bad x %d", x ) );
	MALI_DEBUG_ASSERT( y >= grc->bbox->y && y < grc->bbox->y + grc->bbox->height, ( "Bad y %d", y ) );

	/* Record the top-left corner of a new rectangle */
	grc->x = x;
	grc->y = y;

	if ( grc->valid )
	{
		/* Calculate the remaining width and height in the bounding box for
		   rectangle generation. */
		bbox.x = x;
		bbox.y = y;
		bbox.width = grc->bbox->x + grc->bbox->width - x;
		bbox.height = grc->bbox->y + grc->bbox->height - y;
	}
	else
	{
		/* Reuse the same bounding box as for the first corner. */
		bbox = *grc->bbox;
	}

	/* Width and height of generated rectangles will be exclusive, so
	   increase the bounding box limits if possible. */
	if ( bbox.width < INT_MAX ) ++bbox.width;
	if ( bbox.height < INT_MAX ) ++bbox.height;

	/* Generate pairs of coordinates for use as the bottom-right corner
	   of the rectangle we are constructing. */
	return gen_coords( &bbox, gen_rects_x2_y2, grc );
}

/* Generate rectangles. */
EGLBoolean gen_rects( const pixmap_rect *bbox, rect_callback *callback, void *context )
{
	gen_rects_context grc;
	EGLBoolean finished;

	assert_rect_valid( bbox );
	if ( bbox->width == 0 || bbox->height == 0 )
	{
		/* Can't generate rectangles within a zero area bounding box */
		finished = EGL_TRUE;
	}
	else
	{
		/* Record the bounding box and rectangle callback function */
		grc.bbox = bbox;
		grc.callback = callback;
		grc.context = context;
		grc.valid = EGL_TRUE;

		/* Generate pairs of coordinates for use as the top-left corner
		   of rectangles. */
		finished = gen_coords( bbox, gen_rects_x1_y1, &grc );
	}
	return finished;
}

/**
 * @brief Clipped rectangle generator top-left callback function.
 *
 * Uses each pair of coordinates generated by gen_coords as the top-left
 * corner of a new rectangle and calls gen_coords or gen_border_coords to
 * generate alternative bottom-right corners.
 *
 * @param[in] context Untyped pointer to rectangle generator context.
 * @param[in] x       X coordinate.
 * @param[in] y       Y coordinate.
 * @return    EGL_FALSE to stop the coordinate generator, or EGL_TRUE to get
 *            the next pair of coordinates (if any).
 */
MALI_STATIC EGLBoolean gen_clipped_rects_x1_y1( void *context, EGLint x, EGLint y )
{
	gen_rects_context *grc = context;
	EGLBoolean cont;

	MALI_DEBUG_PRINT( EHNPTU_DEBUG_LEVEL, ( "gen_clipped_rects_x1_y1 %d,%d\n", x, y ) );
	MALI_DEBUG_ASSERT_POINTER( grc );
	assert_rect_valid( grc->bbox );
	MALI_DEBUG_ASSERT( x >= grc->bbox->x && x < grc->bbox->x + grc->bbox->width, ( "Bad x %d", x ) );
	MALI_DEBUG_ASSERT( y >= grc->bbox->y && y < grc->bbox->y + grc->bbox->height, ( "Bad y %d", y ) );

	/* Record the top-left corner of a new rectangle */
	grc->x = x;
	grc->y = y;
	
	if ( grc->valid )
	{
		/* Ensure that the other corner is outside the original bounding box. */
		pixmap_rect border;
		EGLint right_border_width, bbox_right, bbox_bottom;

		/* Calculate absolute position of righthand and bottom edge
		   (should be safe after assert_rect_valid) */
		bbox_right = grc->bbox->x + grc->bbox->width;
		bbox_bottom = grc->bbox->y + grc->bbox->height;

		/* Calculate new bounding boxes to ensure that the bottom-right corner is
		   outside the original bounding box (rectangle is clipped) but also below
		   and to the right of the top-left corner (rectangle is valid) */
		right_border_width = ( bbox_right <= INT_MAX - BORDER_OVERLAP ) ?
		                     BORDER_OVERLAP : INT_MAX - bbox_right;

		/* Generate coordinates in bottom border, which extends horizontally to
		   join right border. */
		border.x = x; /* must be to the right of first coordinate */
		border.width = bbox_right + right_border_width - x;
		border.y = bbox_bottom;
		border.height = ( bbox_bottom <= INT_MAX - BORDER_OVERLAP ) ?
		                BORDER_OVERLAP : INT_MAX - bbox_bottom;

		/* Width and height of generated rectangles will be exclusive, so
		   increase the bounding box limits if possible. */
		if ( border.width < INT_MAX ) ++border.width;
		if ( border.height < INT_MAX ) ++border.height;

		cont = gen_coords( &border, gen_rects_x2_y2, grc );
		if ( cont )
		{
			/* Generate coordinates in right border, which doesn't extend vertically
			   to overlap the bottom border. */
			border.x = bbox_right;
			border.width = right_border_width;
			border.y = y; /* must be below the first coordinate */
			border.height = bbox_bottom - y;

			/* Width and height of generated rectangles will be exclusive, so
			   increase the bounding box limits if possible. */
			if ( border.width < INT_MAX ) ++border.width;
			if ( border.height < INT_MAX ) ++border.height;

			cont = gen_coords( &border, gen_rects_x2_y2, grc );
		}
	}
	else
	{
		/* Any coordinates in a border will do. */
		cont = gen_border_coords( grc->bbox, gen_rects_x2_y2, grc );
	}
	return cont;
}

/* Generate rectangles which overlap a given bounding box but are not
 * enclosed by it.
 */
EGLBoolean gen_clipped_rects( const pixmap_rect *bbox, rect_callback *callback, void *context )
{
	EGLBoolean cont;
	gen_rects_context grc;

	assert_rect_valid( bbox );

	/* Record the bounding box and rectangle callback function */
	grc.bbox = bbox; /* for gen_clipped_rects_x1_y1 only */
	grc.callback = callback;
	grc.context = context;
	grc.valid = EGL_TRUE;

	/* Generate pairs of coordinates for use as the top-left corner
	   of rectangles with their origin inside the bounding box. Use a
	   special top-left callback function to ensure that bottom right
	   corners are outside the bounding box. */
	cont = gen_coords( bbox, gen_clipped_rects_x1_y1, &grc );
	if ( cont )
	{
		pixmap_rect expanded;
		EGLint bbox_right, bbox_bottom, bsize;

		/* Calculate absolute position of righthand and bottom edges
		   (should be safe after assert_rect_valid) */
		bbox_right = bbox->x + bbox->width;
		bbox_bottom = bbox->y + bbox->height;

		/* Expand the bounding box to allow gen_rects_x1_y1 to generate
		   coordinates outside it. */
		bsize = ( bbox->x >= INT_MIN + BORDER_OVERLAP) ?
		        BORDER_OVERLAP : bbox->x - INT_MIN;
		expanded.x = bbox->x - bsize;
		expanded.width = bbox->width + bsize;

		bsize = ( bbox_right <= INT_MAX - BORDER_OVERLAP ) ?
		        BORDER_OVERLAP : INT_MAX - bbox_right;
		expanded.width += bsize;

		bsize = ( bbox->y >= INT_MIN + BORDER_OVERLAP) ?
		        BORDER_OVERLAP : bbox->y - INT_MIN;
		expanded.y = bbox->y - bsize;
		expanded.height = bbox->height + bsize;

		bsize = ( bbox_bottom <= INT_MAX - BORDER_OVERLAP ) ?
		        BORDER_OVERLAP : INT_MAX - bbox_bottom;
		expanded.height += bsize;

		grc.bbox = &expanded;

		/* Generate pairs of coordinates for use as the top-left corner of
		   rectangles with their origin outside the bounding box. Reuse the
		   generic top-left callback function because a rectangle with one corner
		   outside will be clipped wherever the other corner may be. */
		cont = gen_border_coords( bbox, gen_rects_x1_y1, &grc );
	}
	return cont;
}

/**
 * @brief Corner case rectangle generator top-left callback.
 *
 * Uses each pair of coordinates generated by gen_cc_coords as the top-left
 * corner of a new rectangle, and re-enters it to generate bottom-right
 * corners. Reuses the coordinate bounding box passed to gen_cc_rects.
 *
 * @param[in] context Untyped pointer to rectangle generator context.
 * @param[in] x       X coordinate.
 * @param[in] y       Y coordinate.
 * @return    EGL_FALSE to stop the coordinate generator, or EGL_TRUE to get
 *            the next pair of coordinates (if any).
 */
MALI_STATIC EGLBoolean gen_cc_rects_x1_y1( void *context, EGLint x, EGLint y )
{
	gen_rects_context *grc = context;
	pixmap_rect bbox;

	MALI_DEBUG_PRINT( EHNPTU_DEBUG_LEVEL, ( "gen_cc_rects_x1_y1 %d,%d\n", x, y ) );
	MALI_DEBUG_ASSERT_POINTER( grc );

	/* Record the top-left corner of a new rectangle */
	grc->x = x;
	grc->y = y;

	/* Width and height of generated rectangles will be exclusive, so
	   increase the bounding box limits if possible. */
	bbox = *grc->bbox;
	if ( bbox.width < INT_MAX ) ++bbox.width;
	if ( bbox.height < INT_MAX ) ++bbox.height;

	/* Generate pairs of coordinates for use as the bottom-right corner
	   of the rectangle we are constructing. */
	return gen_cc_coords( &bbox, gen_rects_x2_y2, grc );
}

/* Generate corner case rectangles for a given bounding box. */
EGLBoolean gen_cc_rects( const pixmap_rect *bbox, rect_callback *callback, void *context )
{
	gen_rects_context grc;

	assert_rect_valid( bbox );

	/* Record the bounding box and rectangle callback function */
	grc.bbox = bbox;
	grc.callback = callback;
	grc.context = context;
	grc.valid = EGL_TRUE;

	/* Generate pairs of coordinates for use as the top-left corner
	   of rectangles. */
	return gen_cc_coords( bbox, gen_cc_rects_x1_y1, &grc );
}

/* Generate crazy rectangles (including invalid ones). */
EGLBoolean gen_crazy_rects( const pixmap_rect *bbox, rect_callback *callback, void *context )
{
	EGLint yi;
	pixmap_rect area;
	static const EGLint int_limits[] = { INT_MIN, 0, INT_MAX };

	MALI_IGNORE(bbox);

  /* Test each pair of pre-set coordinates in turn */
	for ( yi = 0; yi < MALI_ARRAY_SIZE( int_limits ); ++yi )
	{
		EGLint xi;

		area.y = int_limits[yi];
		for ( xi = 0; xi < MALI_ARRAY_SIZE( int_limits ); ++xi )
		{
			EGLint hi;

			area.x = int_limits[xi];
			for ( hi = 0; hi < MALI_ARRAY_SIZE( int_limits ); ++hi )
			{
				EGLint wi;

				area.height = int_limits[hi];
				for ( wi = 0; wi < MALI_ARRAY_SIZE( int_limits ); ++wi )
				{
					area.width = int_limits[wi];
					if (NULL != callback && !callback( context, &area ) )
					{
						return EGL_FALSE; /* generation aborted */
					}
				}
			}
		}
	}
	return EGL_TRUE; /* generation complete */
}
