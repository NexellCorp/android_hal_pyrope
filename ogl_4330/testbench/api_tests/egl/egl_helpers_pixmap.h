/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef EGL_HELPERS_PIXMAP_H
#define EGL_HELPERS_PIXMAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <suite.h>
#include <base/mali_types.h>
#include <EGL/egl.h>

/** Enum for display formats */
typedef enum display_format
{
	DISPLAY_FORMAT_INVALID,
	DISPLAY_FORMAT_RGB565,
	DISPLAY_FORMAT_ARGB8888,
	DISPLAY_FORMAT_ARGB4444,
	DISPLAY_FORMAT_ARGB1555,
	DISPLAY_FORMAT_ARGB8885, /* Bogus format, for testing */
	DISPLAY_FORMAT_ARGB8880, /* Bogus format, for testing */
	DISPLAY_FORMAT_ARGB8808, /* Bogus format, for testing */
	DISPLAY_FORMAT_ARGB8088, /* Bogus format, for testing */
	DISPLAY_FORMAT_ARGB0888, /* Bogus format, for testing */
	DISPLAY_FORMAT_ARGB0000, /* Bogus format, for testing */
	DISPLAY_FORMAT_RGB560, /* Bogus format, for testing */
	DISPLAY_FORMAT_RGB505, /* Bogus format, for testing */
	DISPLAY_FORMAT_RGB065, /* Bogus format, for testing */
	DISPLAY_FORMAT_RGB000, /* Bogus format, for testing */
	DISPLAY_FORMAT_L8,
	DISPLAY_FORMAT_AL88,
	DISPLAY_FORMAT_A8,
	DISPLAY_FORMAT_COUNT
}
display_format;


#ifdef ANDROID
#define IMAGE_BUFFER_TYPE EGL_NATIVE_BUFFER_ANDROID
#else
#define IMAGE_BUFFER_TYPE EGL_NATIVE_PIXMAP_KHR
#endif

/**
 * @defgroup PixmapAddressing
 *
 * The coordinate system used by functions which operate upon a @ref
 * pixmap_spec has its origin at the top-left corner of a native pixmap.
 * X coordinates increase rightward and Y coordinates increase downward.
 * There is one coordinate unit per pixel.
 *
 * Where these functions take a @ref display_format argument, it is an
 * error to supply a format which does not match the pixmap being addressed
 * (i.e. no pixel format conversion is performed).
 * @{
 */

/**
 * @brief Pixmap fill callback function.
 *
 * A client-supplied function of this type will be called back by the
 * egl_helper_nativepixmap_custom_fill function for each pixel within the
 * area to be filled. It must return a pixel value to write to the pixmap.
 *
 * @param[in] context Opaque context argument from the fill specifier.
 * @param[in] x       X coordinate of pixel.
 * @param[in] y       Y coordinate of pixel.
 * @return    Pixel value to be written to the pixmap.
 * @note   The given coordinates are relative to the fill area rather
 *         than the pixmap's origin.
 */
typedef u32 pixmap_fill_callback( void *context, EGLint x, EGLint y );

/**
 * @brief Custom fill specifier.
 *
 * This structure is used to pass arguments to the
 * egl_helper_nativepixmap_custom_fill function.
 */
typedef struct pixmap_fill
{
	display_format        format;    /**< Format of source pixel data for the fill. */
	pixmap_fill_callback *get_pixel; /**< Function to get pixel values for the fill. */
	void                 *context;   /**< Opaque context argument for callback function. */
}
pixmap_fill;

/**
 * @brief Rectangular area of a native pixmap.
 */
typedef struct pixmap_rect
{
	EGLint x;      /**< X origin, in pixels from left of pixmap. */
	EGLint y;      /**< Y origin, in pixels from top of pixmap. */
	EGLint width;  /**< Width of area, in pixels. */
	EGLint height; /**< Height of area, in pixels. */
}
pixmap_rect;

/**
 * @brief Platform-independent pixmap specifier.
 *
 * Call the egl_helper_nativepixmap_prepare_data function to obtain a
 * specifier for reading from or writing to a native pixmap. Clients can
 * also create pixmap specifiers themselves, provided that the pixel data
 * resides within addressable memory.
 */
typedef struct pixmap_spec
{
	suite          *test_suite; /**< Test suite context pointer, for warnings. */
	display_format  format; /**< Format of pixmap (e.g. RGB565). */
	void           *pixels; /**< Pointer to the pixel data. */
	EGLint          pitch;  /**< Offset from one row of the pixmap to
	                             the next, in bytes (may be negative). */
	EGLint          width;  /**< Width in pixels. */
	EGLint          height; /**< Height in pixels. */
	EGLBoolean      alpha;  /**< EGL_TRUE if pixmap supports non-maximal alpha
	                             (only affects formats with an alpha channel). */
	void            *attachment; /**< Device / Platform specific attachments */
}
pixmap_spec;

/** @} */

/**
 * @brief Find the correct display_format enum for a given set of colour sizes
 *
 * Takes a set of red, green, blue, alpha and luminance sizes and tries to find a suitable
 * display_format enumerator to describe the sizes which can then be used in
 * *_nativepixmap_* functions.
 *
 * @note This function will ONLY return valid formats. Bogus formats are
 * currently not supported. (A bogus format is considered to be one labelled
 * as such in the enumerator definition above.)
 *
 * @param[in] red_size       The number of bits for the red channel
 * @param[in] green_size     The number of bits for the green channel
 * @param[in] blue_size      The number of bits for the blue channel
 * @param[in] alpha_size     The number of bits for the alpha channel
 * @param[in] luminance_size The number of bits for the luminance channel
 * @return A valid display_format or DISPLAY_FORMAT_INVALID if not valid format is found
 */
display_format egl_helper_get_display_format(u32 red_size, u32 green_size, u32 blue_size, u32 alpha_size, u32 luminance_size);

/**
 * @brief Create a native pixmap
 *
 * Equivalent to calling egl_helper_create_nativepixmap_extended with bit
 * mask EGL_OPENVG_BIT|EGL_OPENGL_ES_BIT|EGL_OPENGL_ES2_BIT.
 *
 * @param[in] test_suite       Pointer to test suite context.
 * @param[in] width            Width of pixmap, in pixels.
 * @param[in] height           Height of pixmap, in pixels.
 * @param[in] format           Format of pixmap.
 * @param[out] pixmap          Pointer to an EGLNativePixmapType where the pixmap handle should be stored
 * @return    EGL_TRUE if successful. EGL_FALSE if there's an error.
 */
EGLBoolean egl_helper_create_nativepixmap( suite *test_suite, u32 width, u32 height, display_format format, EGLNativePixmapType* pixmap);

/**
 * @brief Create a native pixmap (extended)
 *
 * @param[in] test_suite       Pointer to test suite context.
 * @param[in] width            Width of pixmap, in pixels.
 * @param[in] height           Height of pixmap, in pixels.
 * @param[in] format           Format of pixmap.
 * @param[in] renderable_type  Bit mask to restrict usage of pixmap according
 *                             to EGLConfig attribute EGL_SURFACE_TYPE.
 * @param[out] pixmap          Pointer to an EGLNativePixmapType where the pixmap handle should be stored
 * @return    EGL_TRUE if successful. EGL_FALSE if there's an error.
 */
EGLBoolean egl_helper_create_nativepixmap_extended( suite *test_suite, u32 width, u32 height, display_format format, EGLint renderable_type, EGLNativePixmapType* pixmap );

/**
 * @brief Create a UMP native pixmap
 *
 * Equivalent to calling egl_helper_create_nativepixmap_ump_extended with
 * bit mask EGL_OPENVG_BIT|EGL_OPENGL_ES_BIT|EGL_OPENGL_ES2_BIT.
 *
 * @param[in] test_suite       Pointer to test suite context.
 * @param[in] width            Width of pixmap, in pixels.
 * @param[in] height           Height of pixmap, in pixels.
 * @param[in] format           Format of pixmap.
 * @param[out] pixmap          Pointer to an EGLNativePixmapType where the pixmap handle should be stored
 * @return    EGL_TRUE if successful. EGL_FALSE if there's an error.
 */
EGLBoolean egl_helper_create_nativepixmap_ump( suite *test_suite, u32 width, u32 height, display_format format, EGLNativePixmapType* pixmap );

/**
 * @brief Create a UMP native pixmap (extended)
 *
 * Allocates UMP memory for the native pixmap, if support for the Unified
 * Memory Provider was enabled at compile-time. That avoids the need to copy
 * the pixmap into Mali memory before the GPU can render to it. Otherwise,
 * this function falls back upon using conventional memory.
 *
 * @param[in] test_suite       Pointer to test suite context.
 * @param[in] width            Width of pixmap, in pixels.
 * @param[in] height           Height of pixmap, in pixels.
 * @param[in] format           Format of pixmap.
 * @param[in] renderable_type  Bit mask to restrict usage of pixmap according
 *                             to EGLConfig attribute EGL_SURFACE_TYPE.
 * @param[out] pixmap          Pointer to an EGLNativePixmapType where the pixmap handle should be stored
 * @return    EGL_TRUE if successful. EGL_FALSE if there's an error.
 */
EGLBoolean egl_helper_create_nativepixmap_ump_extended( suite *test_suite, u32 width, u32 height, display_format format, EGLint renderable_type, EGLNativePixmapType* pixmap );

EGLBoolean egl_helper_valid_nativepixmap( EGLNativePixmapType pixmap );

/**
 * @brief Check if EGL helper platform supports specific display format
 * @param format - pixmap display format
 * @return EGL_TRUE if supported, EGL_FALSE otherwise
 */
EGLBoolean egl_helper_nativepixmap_supported( display_format format );

/** Zero for success, non-zero for failure */
int egl_helper_free_nativepixmap( EGLNativePixmapType pixmap );
u32 egl_helper_get_nativepixmap_width( EGLNativePixmapType pixmap );
u32 egl_helper_get_nativepixmap_height( EGLNativePixmapType pixmap );
u32 egl_helper_get_nativepixmap_bits_per_pixel( EGLNativePixmapType pixmap );
u32 egl_helper_get_nativepixmap_bytes_per_pixel( EGLNativePixmapType pixmap );

/**
 * @note IMPORTANT some pixmap implementations have a pitch not equal to
 * width*bytes_per_pixel
 */
u32 egl_helper_get_nativepixmap_pitch( EGLNativePixmapType pixmap );

/**
 * @brief Returns a pointer to raw pixmap data
 * @param pixmap pixmap to retrieve data from
 * @return pointer to pixmap data
 * @note call egl_helper_nativepixmap_unmap_data when done using the data
 */
u16* egl_helper_nativepixmap_map_data( EGLNativePixmapType pixmap );

/**
 * @brief Unmaps any internal mappins performed in a previous call
 *        to egl_helper_nativepixmap_map_data
 * @param pixmap pixmap to unmap data for
 */
void egl_helper_nativepixmap_unmap_data( EGLNativePixmapType pixmap );

/**
 * @brief Check whether EGL supports directly renderable pixmaps
 *
 * @return True if supported, False if not supported
 */
EGLBoolean egl_helper_nativepixmap_directly_renderable( void );


/** @addtogroup PixmapAddressing
 *  @{
 */

/**
 * @brief Prepare a native pixmap for read/write operations
 *
 * Creates a pixmap specifier for use in subsequent read/write operations on
 * a native pixmap. It is the caller's responsibility to destroy the
 * @ref pixmap_spec by calling egl_helper_nativepixmap_release_data, when it
 * is no longer required.
 *
 * @param[in] test_suite Test suite context to make allocations/log errors
 * @param[in] pixmap     Native pixmap to be addressed.
 * @return    Pointer to pixmap specifier, or NULL on failure.
 *
 * @warn During the lifetime of the returned pixmap specifier, the result of
 *       modifying the native pixmap by other means is undefined. Also, you
 *       must not call this function twice on the same pixmap without
 *       destroying the original pixmap specifier.
 */
pixmap_spec *egl_helper_nativepixmap_prepare_data( suite *test_suite, EGLNativePixmapType pixmap );

/**
 * @brief Destroy a pixmap specifier.
 *
 * Destroys a pixmap specifier returned by a preceding call to
 * egl_helper_nativepixmap_prepare_data.
 *
 * @param[in] ps Pixmap specifier to release.
 */
void egl_helper_nativepixmap_release_data( pixmap_spec *ps );

/**
 * @brief Get a pixel value from a native pixmap
 *
 * @param[in] src         Source pixmap specifier.
 * @param[in] x           X coordinate of pixel to read.
 * @param[in] y           Y coordinate of pixel to read.
 * @param[in] format      Required pixel format (must match the pixmap).
 * @return    Pixel value read from pixmap.
 * @warning Not thread safe.
 */
u32 egl_helper_nativepixmap_get_pixel( const pixmap_spec *src, EGLint x, EGLint y, display_format format );

/**
 * @brief Write a pixel value to a native pixmap
 *
 * @param[in] dst         Destination pixmap specifier.
 * @param[in] x           X coordinate of pixel to write.
 * @param[in] y           Y coordinate of pixel to write.
 * @param[in] pixel_value Pixel value to write, in the specified format.
 * @param[in] format      Format of the @a pixel_value (must match the pixmap).
 * @warning Not thread safe.
 */
void egl_helper_nativepixmap_write_pixel( const pixmap_spec *dst, EGLint x, EGLint y, u32 pixel_value, display_format format );

/**
 * @brief Fill an area of a native pixmap with plain color.
 *
 * Sets all pixels within a rectangular area of a native pixmap to a
 * specified color and opacity.
 *
 * @param[in] dst         Destination pixmap specifier.
 * @param[in] area        Rectangular area of pixmap to be filled, or NULL
 *                        to fill the whole pixmap.
 * @param[in] pixel_value Pixel value to write, in the specified format.
 * @param[in] format      Format of the @a pixel_value (must match the pixmap).
 * @warning Not thread safe.
 */
void egl_helper_nativepixmap_plain_fill( const pixmap_spec *dst, const pixmap_rect *area, u32 pixel_value, display_format format );

/**
 * @brief Copy pixels from one pixmap to another.
 *
 * Sets the color and opacity of all pixels within a rectangular area
 * of a native pixmap by copying pixels from another. If the area to
 * be filled exceeds the dimensions of the source pixmap then it will
 * be tiled horizontally and vertically as necessary.
 *
 * @param[in] dst  Destination pixmap specifier.
 * @param[in] area Rectangular area of pixmap to be filled, or NULL
 *                 to fill the whole pixmap.
 * @param[in] src  Source pixmap specifier.
 * @warning Not thread safe.
 */
void egl_helper_nativepixmap_copy( const pixmap_spec *dst, const pixmap_rect *area, const pixmap_spec *src );

/**
 * @brief Fill an area of a native pixmap using a custom pattern.
 *
 * Sets the color and opacity of all pixels within a rectangular area
 * of a native pixmap by calling a client-supplied function to obtain the
 * new value for each pixel.
 *
 * @param[in] dst    Destination pixmap specifier.
 * @param[in] area   Rectangular area of pixmap to be filled, or NULL
 *                   to fill the whole pixmap.
 * @param[in] filler Specifier of custom fill callback.
 * @warning Not thread safe.
 */
void egl_helper_nativepixmap_custom_fill( const pixmap_spec *dst, const pixmap_rect *area, const pixmap_fill *filler );

/** @} */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* EGL_HELPERS_PIXMAP_H */
