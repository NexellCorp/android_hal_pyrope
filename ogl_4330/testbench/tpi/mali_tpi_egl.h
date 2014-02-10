/*
 * $Copyright:
 * ----------------------------------------------------------------------------
 *  This confidential and proprietary software may be used only as authorized
 *  by a licensing agreement from ARM Limited.
 *          (C) COPYRIGHT 2010-2012 ARM Limited , ALL RIGHTS RESERVED
 *  The entire notice above must be reproduced on all authorized copies and
 *  copies may only be made to the extent permitted by a licensing agreement
 *  from ARM Limited.
 * ----------------------------------------------------------------------------
 * $
 */

#ifndef _MALI_TPI_EGL_H_
#define _MALI_TPI_EGL_H_

#include "tpi/mali_tpi.h"
#include <EGL/egl.h>

#ifndef MALI_TPI_EGL_API
#define MALI_TPI_EGL_API MALI_TPI_IMPORT
#endif

#if (1 == CSTD_OS_WINDOWS) || defined(TPI_OS_WINDOWS)
	#include "tpi/include/windows/mali_tpi_egl_windows.h"
#elif 1 == CSTD_OS_LINUX || defined(TPI_OS_LINUX)
	#include "tpi/include/linux/mali_tpi_egl_linux.h"
#elif 1 == CSTD_OS_ANDROID || defined(TPI_OS_ANDROID)
	#include "tpi/include/android/mali_tpi_egl_android.h"
#elif 1 == CSTD_OS_SYMBIAN || defined(TPI_OS_SYMBIAN)
	#error "There is no TPI EGL support for your operating system yet"
#elif 1 == CSTD_OS_APPLEOS || defined(TPI_OS_APPLEOS)
	#error "There is no TPI EGL support for your operating system yet"
#else
	#error "There is no TPI EGL support for your operating system"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup tpi_egl TPI EGL library
 * @{
 */

/** @defgroup tpi_egl_pixels Simple colors
 * @{
 */

/** @brief Type for simple 4-bit color.
 *
 * Each color component is either on or off as this is the only format
 * unambiguously translatable to any real pixel format without needing to emulate
 * rounding and floating-point conversions.
 *
 * The format is represented as:
 * 7......0
 * XXXXRGBA
 */
typedef mali_tpi_uint8 mali_tpi_egl_simple_color;

/** Opaque red color */
#define MALI_TPI_EGL_SIMPLE_COLOR_RED   0x9
/** Opaque green color */
#define MALI_TPI_EGL_SIMPLE_COLOR_GREEN 0x5
/** Opaque blue color */
#define MALI_TPI_EGL_SIMPLE_COLOR_BLUE  0x3
/** Opaque black color */
#define MALI_TPI_EGL_SIMPLE_COLOR_BLACK 0x1
/** Opaque white color */
#define MALI_TPI_EGL_SIMPLE_COLOR_WHITE 0xF

/**
 * @}
 * @defgroup tpi_egl_init Initialization and shutdown
 * @{
 */

/** @brief Initialize the tpi_egl module.
 *
 * It is legal to call this multiple times, and to re-initialize after
 * shutdown.
 */
MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_init( void );

/** @brief Shut down the EGL module.
 *
 * It is legal to call this multiple times.
 */
MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_shutdown( void );

/** @}
 * @defgroup tpi_egl_display Display querying
 * @{
 */

/** @brief Get the platform-specific default display handle
 *
 * @return The platform-specific default display handle
 */
MALI_TPI_EGL_API EGLNativeDisplayType mali_tpi_egl_get_default_native_display( void );

/** @brief Get a validatable invalid platform-specific display handle
 *
 * On some platforms it is not possible to validate a display handle (passing an invalid display will typically just
 * cause a seg fault). On such platforms this function will fail and not update dpy. Otherwise, it will return an
 * invalid native display handle.
 *
 * @param[out] native_display An invalid native display handle if @c MALI_TPI_TRUE was returned, not updated 
 * if not
 *
 * @return @c MALI_TPI_TRUE if dpy was updated, @c MALI_TPI_FALSE if the platform can not validate invalid displays
 */
MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_get_invalid_native_display(EGLNativeDisplayType *native_display);

/**
 * @}
 * @defgroup tpi_egl_config Config selection
 * @{
 */

/** @brief Choose a config with option to get exact matches.
 *
 * Refer to the specification for the precise semantics.
 *
 * @param display         EGL display
 * @param[in] min_attribs @c NULL, or list of attrib/value pairs terminated by
 *                        @c EGL_NONE to match as in @c eglChooseConfig.
 * @param[in] exact_attribs   @c NULL, or list of attrib/value pairs terminated by
 *                        @c EGL_NONE that have to match exactly.
 * @param[out] configs    @c NULL, or array to hold list of returned configs
 * @param config_size     Size of the array in @c configs
 * @param[out] num_config The number of configs written to @c configs (must not
 *                        be NULL).
 */
MALI_TPI_EGL_API EGLBoolean mali_tpi_egl_choose_config(
	EGLDisplay    display,
	EGLint const *min_attribs,
	EGLint const *exact_attribs,
	EGLConfig *   configs,
	EGLint        config_size,
	EGLint *      num_config );

/**
 * @}
 * @defgroup tpi_egl_pixmap Pixmap creation and querying
 * @{
 */

/** @brief The #eglCreateImageKHR type tag for native pixmaps.
 *
 * The #EGLNativePixmapType returned by #mali_tpi_egl_create_pixmap() corresponds to a particular type tag. When the
 * type tag and the #EGLNativePixmapType (cast to type EGLClientBuffer) are passed to #eglCreateImageKHR(), that
 * function can create an #EGLImageKHR.
 *
 * The type tag has a platform-specific value.
 *
 * @warning You can only call this function in a build with #EGL_KHR_image defined true.
 *
 * @return A type tag suitable for passing to #eglCreateImageKHR().
 */
MALI_TPI_EGL_API EGLint mali_tpi_egl_get_pixmap_type(void);

/** @brief A format for a pixmap.
 *
 * Not all the possible formats for pixmaps correspond to values of EGLConfig. This enumeration allows you to specify
 * those formats when creating a pixmap with #mali_tpi_egl_create_external_pixmap().
 */
typedef enum mali_tpi_egl_pixmap_format
{
	MALI_TPI_EGL_PIXMAP_FORMAT_YV12,
	MALI_TPI_EGL_PIXMAP_FORMAT_NV21,
	MALI_TPI_EGL_PIXMAP_FORMAT_YUYV
}
mali_tpi_egl_pixmap_format;

/** @brief Create a pixmap.
 *
 * The pixmap will be compatible with the config supplied.
 *
 * @param native_display The native display to create the pixmap on
 * @param width          The width of the new pixmap in pixels
 * @param height         The height of the new pixmap in pixels
 * @param display        The display to create the pixmap on
 * @param config         The config to make the pixmap compatible with
 * @param [out] pixmap   The created pixmap
 *
 * @return @c MALI_TPI_TRUE if successful, @c MALI_TPI_FALSE if the display was invalid or the pixmap could 
 * not be created.
 */
MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_create_pixmap(
	EGLNativeDisplayType native_display,
	int width,
	int height,
	EGLDisplay display,
	EGLConfig config,
	EGLNativePixmapType *pixmap);

/** @brief Create a YUV pixmap.
 *
 * Unlike #mali_tpi_egl_create_pixmap(), this function lets you create pixmaps with a format that doesn't correspond to
 * an EGLConfig, for example YUV formats used with an "external texture" client extension.
 *
 * Because of chroma subsampling and block ordering, if the width and height you specify are not multiples of the
 * (format-specific) block size, they will be rounded up.
 *
 * @param      native_display The native display to create the pixmap on.
 * @param      width          The width of the new pixmap in pixels.
 * @param      height         The height of the new pixmap in pixels.
 * @param      display        The display to create the pixmap on.
 * @param      format         The format to use for the pixmap.
 * @param[out] pixmap         The created pixmap. Must not be NULL.
 *
 * @retval MALI_TPI_TRUE  if the pixmap was successfully created.
 * @retval MALI_TPI_FALSE if the pixmap could not be created. @p pixmap will not be written in this case.
 */
MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_create_external_pixmap(
	EGLNativeDisplayType native_display,
	int width,
	int height,
	EGLDisplay display,
	mali_tpi_egl_pixmap_format format,
	EGLNativePixmapType *pixmap);

/** @brief Set a pixmap's contents from memory.
 *
 * Replaces the contents of the pixmap with the contents of the given memory. The source data must be arranged with the
 * same ordering as the pixmap and the natural row stride. If the pixmap has a planar YUV format, the source data must
 * have as well, and you must supply the correct number of pointers.
 *
 * @param[in] pixmap     The pixmap whose contents should be altered. Must not be null.
 * @param[in] num_planes The size of the array @p data; that is, the number of planes to read from. Must be equal to the
 *                       number of planes in the format, and in the range [1,3].
 * @param[in] data       An array of one to three pointers, each of which points to a plane of source data.
 *
 * @retval MALI_TPI_TRUE  if the file was successfully loaded and the pixmap's contents set.
 * @retval MALI_TPI_FALSE if the pixmap's memory could not be synchronized. In this case the contents of the pixmap are
 *                        undefined but it is still a valid pixmap.
 */
MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_load_pixmap(
	EGLNativePixmapType pixmap,
	unsigned int num_planes,
	void const * const data[]);

/** @brief Set a pixmap's contents from a raw binary file.
 *
 * Interprets a file in raw binary format, replacing the contents of a pixmap with the contents of the file. Since the
 * file is a raw dump of memory and doesn't contain metadata, you must be very careful to ensure the file has the same
 * dimensions, strides and format as the pixmap it's being loaded into. The source data from the file must have the same
 * ordering as the pixmap and the natural row stride.
 *
 * @param[in] pixmap     The pixmap whose contents should be altered. Must not be null.
 * @param[in] num_planes The size of the array @p data; that is, the number of planes to read from. Must be equal to the
 *                       number of planes in the format, and in the range [1,3].
 * @param[in] files      An array of one to three file-handles, each of which is opened in read mode and contains one
 *                       plane of source data.
 *
 * @retval MALI_TPI_TRUE  if the file was successfully loaded and the pixmap's contents set.
 * @retval MALI_TPI_FALSE if reading the file failed or the pixmap's memory could not be synchronized. In this case the
 *                        contents of the pixmap are undefined but it is still a valid pixmap.
 */
MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_load_pixmap_from_file(
	EGLNativePixmapType pixmap,
	unsigned int num_planes,
	mali_tpi_file * const files[]);

/** @brief Destroy a pixmap.
 *
 * @param native_display The native display containing the pixmap to delete
 * @param pixmap         The pixmap to delete
 *
 * @return MALI_TPI_TRUE if successful, MALI_TPI_FALSE if the display or pixmap
 * were invalid.
 */
MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_destroy_pixmap(
	EGLNativeDisplayType native_display,
	EGLNativePixmapType pixmap);

/** Represents the top left quadrant of a pixmap */
#define MALI_TPI_EGL_TOP_LEFT     0x1
/** Represents the top right quadrant of a pixmap */
#define MALI_TPI_EGL_TOP_RIGHT    0x2
/** Represents the bottom left quadrant of a pixmap */
#define MALI_TPI_EGL_BOTTOM_LEFT  0x4
/** Represents the bottom right quadrant of a pixmap */
#define MALI_TPI_EGL_BOTTOM_RIGHT 0x8

/** @brief Fill portions of a pixmap with a solid color
 *
 * Fills the specified quadrants of a pixmap with the specified color using native APIs. The remaining quadrants are not
 * touched.
 *
 * @sa MALI_TPI_EGL_TOP_LEFT
 * @sa MALI_TPI_EGL_TOP_RIGHT
 * @sa MALI_TPI_EGL_BOTTOM_LEFT
 * @sa MALI_TPI_EGL_BOTTOM_RIGHT
 *
 * @param native_display The native display containing the pixmap to fill
 * @param pixmap         The pixmap to fill
 * @param color          The color to fill the specified quadrants with
 * @param to_fill        Bit-wise or of the quadrants to fill
 *
 * @return MALI_TPI_TRUE if successful, MALI_TPI_FALSE if the pixmap or display
 * was invalid.
 */
MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_fill_pixmap(
	EGLNativeDisplayType native_display,
	EGLNativePixmapType pixmap,
	mali_tpi_egl_simple_color color,
	int to_fill);

/** @brief Check a pixel from a pixmap.
 *
 * Checks if the specified pixel in the pixmap is the specified color.
 *
 * @param native_display The native display containing the pixmap to check the pixel from
 * @param pixmap         The pixmap containing the pixel to check
 * @param x              The x co-ordinate of the pixel to check, relative to the upper-left
 *                       corner of the pixmap
 * @param y              The y co-ordinate of the pixel to check, relative to the upper-left
 *                       corner of the pixmap
 * @param color          The color to check against
 *
 * @param[out] result  MALI_TPI_TRUE if the specified pixel is @a color color,
 * MALI_TPI_FALSE if it was the wrong color, not updated if MALI_TPI_FALSE was
 * returned
 *
 * @return MALI_TPI_TRUE if the pixel was successfully read and *result was updated
 * @return MALI_TPI_FALSE if the display or pixmap was invalid
 */
MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_check_pixmap_pixel(
	EGLNativeDisplayType native_display,
	EGLNativePixmapType pixmap,
	int x,
	int y,
	mali_tpi_egl_simple_color color,
	mali_tpi_bool *result);

/**
 * @}
 * @defgroup tpi_egl_window Window creation and querying
 * @{
 */

/** @brief Creates a native window of the specified size.
 *
 * The window will be compatible with the config supplied.
 *
 * @param native_display The native display that the window will be used with
 * @param width          The window client width
 * @param height         The window client height
 * @param display        The display that the window will be used with
 * @param config         The config to make the window compatible with
 * @param [out] window   Output pointer for the generated window (non-NULL)
 *
 * @return @c MALI_TPI_TRUE if successful, @c MALI_TPI_FALSE if the display was invalid or the window could not be
 * created
 */
MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_create_window(
	EGLNativeDisplayType native_display,
	int width,
	int height,
	EGLDisplay display,
	EGLConfig config,
	EGLNativeWindowType *window );

/** @brief Resizes a native window to the specified size.
 *
 * @param native_display   The native display containing the window
 * @param [in,out] window  The window to resize
 * @param width            The requested new window client width
 * @param height           The requested new window client height
 * @param egl_display      The EGLDisplay the window will be used for
 * @param config           The EGLConfig the window should be compatible with
 *
 * @return @c MALI_TPI_TRUE on success, @c MALI_TPI_FALSE on failure
 */
MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_resize_window(
	EGLNativeDisplayType native_display,
	EGLNativeWindowType *window,
	int width,
	int height,
	EGLDisplay egl_display,
	EGLConfig config);

/** @brief Creates a full-screen/optimized window
 *
 * The window will be compatible with the config supplied.
 *
 * @param native_display  The native display that the window will be used with
 * @param display         The display that the window will be used with (hint only)
 * @param config          The config to make the window compatible with
 * @param [out] window    Output pointer for the generated window (non-NULL)
 *
 * @return @c MALI_TPI_TRUE if successful, @c MALI_TPI_FALSE if the display was invalid or the window could not be
 * created
 */
MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_create_fullscreen_window(
	EGLNativeDisplayType native_display,
	EGLDisplay display,
	EGLConfig config,
	EGLNativeWindowType *window );

/** @brief Destroy a window created with @c mali_tpi_egl_create_window or
 * @c mali_tpi_egl_create_fullscreen_window
 *
 * @param native_display   The native display containing the window to destroy
 * @param window           The window to destroy.
 *
 * @return @c MALI_TPI_TRUE on success, @c MALI_TPI_FALSE on failure
 */
MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_destroy_window(
	EGLNativeDisplayType native_display,
	EGLNativeWindowType window );

/** @brief Get the dimensions of a native window
 *
 * @param native_display  The native display containing the native window
 * @param window          The window to query
 * @param [out] width     The width of the window in pixels
 * @param [out] height    The height of the window in pixels
 *
 * @return MALI_TPI_TRUE if width and height were updated, MALI_TPI_FALSE on error
 */
MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_get_window_dimensions(
	EGLNativeDisplayType native_display,
	EGLNativeWindowType window,
	int *width,
	int *height);

/** @brief Check a pixel from a window's front buffer.
 *
 * Checks if the specified pixel in the window's front buffer is the specified color.
 *
 * @param native_display The native display containing the window to check the pixel from
 * @param window         The window containing the pixel to check
 * @param x              The x co-ordinate of the pixel to check, relative to the upper-left
 *                       corner of the window
 * @param y              The y co-ordinate of the pixel to check, relative to the upper-left
 *                       corner of the window
 * @param color          The color to check against
 * @param[out] result    MALI_TPI_TRUE if the specified pixel is @a color color,
 *                       MALI_TPI_FALSE if it was the wrong color, not updated if MALI_TPI_FALSE was returned
 *
 * @return MALI_TPI_TRUE if the pixel was successfully read and *result was updated
 * @return MALI_TPI_FALSE if the display or window was invalid
 */
MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_check_window_pixel(
	EGLNativeDisplayType native_display,
	EGLNativeWindowType window,
	int x,
	int y,
	mali_tpi_egl_simple_color color,
	mali_tpi_bool *result);

/** @}
 *  @}
 */

#ifdef __cplusplus
}
#endif

#include "tpi/include/common/mali_tpi_egl_internal.h"

#endif /* End (_MALI_TPI_EGL_H_) */
