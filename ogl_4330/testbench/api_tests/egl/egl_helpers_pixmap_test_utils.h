/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef EGL_HELPERS_PIXMAP_TEST_UTILS_H
#define EGL_HELPERS_PIXMAP_TEST_UTILS_H

/**
 * Utilities for unit tests of the native pixmap helper functions.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <EGL/egl.h>
#include "egl_helpers_pixmap.h"

/**
 * @brief Coordinate generator callback function.
 *
 * A function of this type will be called back by a function of type
 * coord_gen_function for every pair of coordinates it generates.
 *
 * @param[in] context    Untyped context argument passed to gen_coords.
 * @param[in] x          X coordinate.
 * @param[in] y          Y coordinate.
 * @return    EGL_FALSE to stop the enumeration, or EGL_TRUE to get
 *            the next pair of coordinates (if any).
 */
typedef EGLBoolean coord_callback( void *context, EGLint x, EGLint y );

/**
 * @brief Coordinate generator function.
 *
 * A function of this type calls a specified function with a variety of
 * coordinates related to a given bounding box.
 *
 * @param[in] bbox     Pointer to bounding box for coordinate generation.
 * @param[in] callback Pointer to function to be called back.
 * @param[in] context  Untyped context argument for callback function.
 * @return    EGL_FALSE if the generator was aborted; otherwise EGL_TRUE.
 */
typedef EGLBoolean coord_generator( const pixmap_rect *bbox, coord_callback *callback, void *context );

/**
 * @brief Rectangle generator callback function.
 *
 * A function of this type will be called back by gen_rects or
 * gen_crazy_rects for every rectangle that they generate.
 *
 * @param[in] context Untyped context argument passed to enum_valid_rects.
 * @param[in] area    Pointer to coordinates of rectangle.
 * @return    EGL_FALSE to stop the enumeration, or EGL_TRUE to get
 *            the next rectangle (if any).
 */
typedef EGLBoolean rect_callback( void *context, const pixmap_rect *area );

/**
 * @brief Test rectangle generator.
 *
 * A function of this type calls a specified function with a variety of
 * rectangles related to a given bounding box.
 *
 * @param[in] bbox      Pointer to bounding box for rectangle generation.
 * @param[in] callback  Pointer to function to be called back with rectangles.
 * @param[in] context   Untyped context argument for callback function.
 * @return    EGL_FALSE if the generator was aborted; otherwise EGL_TRUE.
 */
typedef EGLBoolean rect_generator( const pixmap_rect *bbox, rect_callback *callback, void *context );

/**
 * @brief Check memory for byte value.
 *
 * Checks that every byte of a specified block of memory has a given
 * value and returns the number of mismatches.
 *
 * @param[in] mem    Pointer to memory to check.
 * @param[in] value  Expected byte value.
 * @param[in] n      Number of bytes to compare.
 * @return    Number of bytes which didn't match the expected value.
 */
size_t check_bytes( const void *mem, EGLint value, size_t n );

/**
 * @brief Compare two blocks of memory.
 *
 * Checks that every byte of a specified block of memory matches
 * another block and returns the number of mismatches.
 *
 * @param[in] mem1 Pointer to first block of memory.
 * @param[in] mem2 Pointer to second block of memory.
 * @param[in] n    Number of bytes to compare.
 * @return    Number of bytes which differed between the two blocks.
 */
size_t compare_bytes( const void *mem1, const void *mem2, size_t n );

/**
 * @brief Assert that a rectangle is valid.
 *
 * @param[in] area Pointer to coordinates of rectangle.
 * @return    EGL_TRUE if the rectangle is valid; otherwise EGL_FALSE.
 */
void assert_rect_valid( const pixmap_rect *area );

/**
 * @brief Assert that a rectangle is entirely within a pixmap.
 *
 * @param[in] area   Pointer to coordinates of rectangle.
 * @param[in] pixmap Pointer to pixmap specifier.
 */
void assert_rect_within_pixmap( const pixmap_rect *area, const pixmap_spec *pixmap );

/**
 * @brief Generate a set of corner case coordinates.
 *
 * Calls a specified function with a variety of cartesian coordinates deemed
 * to be 'corner cases' for a given bounding box.
 *
 * @param[in] bbox     Pointer to bounding box for coordinate generation.
 * @param[in] callback Pointer to function to be called back.
 * @param[in] context  Untyped context argument for callback function.
 * @return    EGL_FALSE if the generator was aborted; otherwise EGL_TRUE.
 */
EGLBoolean gen_cc_coords( const pixmap_rect *bbox, coord_callback *callback, void *context );

/**
 * @brief Generate a set of coordinates.
 *
 * Calls a specified function with a variety of cartesian coordinates
 * within a given bounding box. Width and height are exclusive.
 *
 * @param[in] bbox     Pointer to bounding box for coordinate generation.
 * @param[in] callback Pointer to function to be called back.
 * @param[in] context  Untyped context argument for callback function.
 * @return    EGL_FALSE if the generator was aborted; otherwise EGL_TRUE.
 */
EGLBoolean gen_coords( const pixmap_rect *bbox, coord_callback *callback, void *context );

/**
 * @brief Generate a set of coordinates bordering a rectangle.
 *
 * Calls a specified function with a variety of cartesian coordinates
 * within a fixed distance of a given bounding box, on the outside.
 *
 * @param[in] bbox     Pointer to bounding box for coordinate generation.
 * @param[in] callback Pointer to function to be called back.
 * @param[in] context  Untyped context argument for callback function.
 * @return    EGL_FALSE if the generator was aborted; otherwise EGL_TRUE.
 */
EGLBoolean gen_border_coords( const pixmap_rect *bbox, coord_callback *callback, void *context );

/**
 * @brief Generate rectangles.
 *
 * Calls a specified function with a variety of rectangles which are
 * completely enclosed by a given bounding box. All rectangles will be
 * valid (non-negative width and height).
 *
 * @param[in] bbox     Pointer to bounding box for rectangle generation.
 * @param[in] callback Pointer to function to be called back with rectangles.
 * @param[in] context  Untyped context argument for callback function.
 * @return    EGL_FALSE if the generator was aborted; otherwise EGL_TRUE.
 */
EGLBoolean gen_rects( const pixmap_rect *bbox, rect_callback *callback, void *context );

/**
 * @brief Generate clipped rectangles.
 *
 * Calls a specified function with a variety of rectangles which overlap
 * a given bounding box but are not enclosed by it. All rectangles will be
 * valid (non-negative width and height).
 *
 * @param[in] bbox     Pointer to bounding box for rectangle generation.
 * @param[in] callback Pointer to function to be called back with rectangles.
 * @param[in] context  Untyped context argument for callback function.
 * @return    EGL_FALSE if the generator was aborted; otherwise EGL_TRUE.
 */
EGLBoolean gen_clipped_rects( const pixmap_rect *bbox, rect_callback *callback, void *context );

/**
 * @brief Generate corner case rectangles.
 *
 * Calls a specified function with a variety of rectangles deemed to be
 * 'corner cases' for a given bounding box. All rectangles will be
 * valid (non-negative width and height).
 *
 * @param[in] bbox     Pointer to bounding box for rectangle generation.
 * @param[in] callback Pointer to function to be called back with rectangles.
 * @param[in] context  Untyped context argument for callback function.
 * @return    EGL_FALSE if the generator was aborted; otherwise EGL_TRUE.
 */
EGLBoolean gen_cc_rects( const pixmap_rect *bbox, rect_callback *callback, void *context );

/**
 * @brief Generate crazy rectangles.
 *
 * Calls a specified function once with every possible rectangle where the
 * left/top coordinates and width/height are some combination of 0 and the
 * signed integer limits.
 *
 * @param[in] bbox     Pointer to bounding box for rectangle generation
 *                     (ignored).
 * @param[in] callback Pointer to function to be called back with rectangles.
 * @param[in] context  Untyped context argument for callback function.
 * @return    EGL_FALSE if the enumeration was aborted; otherwise EGL_TRUE.
 */
EGLBoolean gen_crazy_rects( const pixmap_rect *bbox, rect_callback *callback, void *context );

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* EGL_HELPERS_PIXMAP_TEST_UTILS_H */
