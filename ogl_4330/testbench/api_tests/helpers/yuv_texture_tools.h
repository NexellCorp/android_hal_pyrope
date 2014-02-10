/*
 * $Copyright:
 * ----------------------------------------------------------------
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 * ----------------------------------------------------------------
 * $
 */

#ifndef YUV_TEXTURE_TOOLS_H
#define YUV_TEXTURE_TOOLS_H

#ifdef __cplusplus
extern "C" {
#endif

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#if EGL_MALI_GLES && MALI_EGL_GLES_MAJOR_VERSION == 2
#include "../opengles2/gl2_framework.h"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

#if EGL_MALI_VG
#include <VG/openvg.h>
#include <VG/vgext.h>
#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "suite.h"
#include "base/mali_types.h"
#ifndef ANDROID
#include <shared/mali_egl_image.h>
#endif


typedef u32 (yuv_helper_threadproc)(void *);

#define ARRAY_SIZE(x) ( sizeof(x) / sizeof(x[0]) )

typedef struct yuv_image
{
	u8 *plane[3];
} yuv_image;

typedef struct helper_yuv_format
{
	EGLint format;
	EGLint colorspace;
	EGLint range;
} helper_yuv_format;

/**
 * Stores info about max/min/etc. values for a YUV format/colorspace/range combo
 */
typedef struct yuv_values
{
	u8 l_max;
	u8 l_mid;
	u8 l_quarter;
	u8 l_min;
	u8 c_max;
	u8 c_mid;
	u8 c_min;
} yuv_values;

/**
 * Initializes extensions required for YUV support (i.e. EGLImage-related extensions)
 *
 * @return 0 on success, non-0 on failure
 */
int yuv_texture_tools_init_extensions( void );

/**
 * Retrieves a set of yuv_values (@see yuv_values) for the given format (@see helper_yuv_format)
 */
void get_yuv_values( yuv_values *values, helper_yuv_format *info );

/**
 * Scales a pattern to a given target size.
 * Assumes the yuv_image is in YUV444
 */
void scale_pattern_to_target(
    yuv_image *image,
    int width, int height,
    u8 *pattern_l, u8 *pattern_cr, u8 *pattern_cb,
    int pattern_width, int pattern_height );

/**
 * Downsamples a YUV444 image to the requested format
 */
void downsample_yuv444( yuv_image *dst, yuv_image *src, EGLint width, EGLint height, helper_yuv_format *info );

/**
 * Converts subset a YUV444 image to a subset of RGB-float buffer, useful to convert multiple smaller yuv patters to rgb
 * @param width is Width to convert
 * @param height is Height to convert
 * @param pitch is the real width of image.
 */
void convert_yuv444_to_rgb_subset( float *rgb, yuv_image *yuv, int width, int height, int pitch, int xoffset, int yoffset, helper_yuv_format *info );

/**
 * Converts a YUV444 image to a RGB-float buffer
 */
void convert_yuv444_to_rgb( float *rgb, yuv_image *yuv, int width, int height, helper_yuv_format *info );

#ifdef ANDROID
EGLImageKHR create_egl_image_yuv( suite *test_suite, yuv_image *yuv_source_image, EGLDisplay dpy, EGLint width, EGLint height, helper_yuv_format *info );
#else 
/**
 * Creates an EGLImage to hold YUV data for the requested format
 * @return an image handle, EGL_NO_IMAGE_KHR on failure
 */
EGLImageKHR create_egl_image_yuv( suite *test_suite, EGLDisplay dpy, EGLint width, EGLint height, helper_yuv_format *info );

/**
 * Destroys the EGLImage created by create_egl_image_yuv function
 * @return EGL_TRUE on success, EGL_FALSE otherwise
 */
EGLBoolean destroy_egl_image_yuv( EGLDisplay dpy, EGLImageKHR image, EGLNativePixmapType pixmap );

/**
 * Writes YUV data to an EGLImage
 */
EGLBoolean write_yuv_data_to_image( EGLImageKHR egl_image, yuv_image *yuv_source_image, EGLint width, EGLint height, helper_yuv_format *info );

#endif /* non-android */

#if EGL_MALI_GLES && MALI_EGL_GLES_MAJOR_VERSION == 2

/**
 * Creates a GLES texture from the given EGLImage
 *
 * @note it is the callee's responsibility to check for errors!
 *
 * @return the texture handle
 */
unsigned int create_texture_from_image( EGLImageKHR egl_image );

unsigned int create_2d_texture_from_image( EGLImageKHR egl_image );


/**
 * Renders a textured quad using the given texture(s)
 * @note the texture is assumed to be an external image
 */
void render_yuv_texture( GLuint *tex, int num_textures, int width, int height );

/**
 * @brief Renders a textured quad using the given texture(s)
 * @note textures are assumed to be external images
 * @note same number of samples as textures will be set up
 */
void render_yuv_texture_multiple_samplers( GLuint *tex, int num_textures, int width, int height );

void render_yuv_texture_multiple_samplers_mixed( GLuint *tex, int num_textures_yuv, int num_textures_rgb, int width, int height );

void render_yuv_texture_custom_conversion( int width, int height, int xofs, int yofs );

/**
 * Compares the backbuffer to a RGB-float reference buffer, using the provided epsilon
 * @return the number of pixels that differ
 */
int compare_to_reference( BackbufferImage *backbuffer, float *rgb, int width, int height, float epsilon );

/*
 * Adds pixels in source to pixels in destination
 */
void additive_blend_rgb_images( float *dst, float *src, int width, int height );
#endif

/**
 * Generates a black YUV444 image
 */
void generate_yuv444_image_black( yuv_image *image, int width, int height );

u32 yuv_helper_create_thread( suite *test_suite, yuv_helper_threadproc *threadproc, void *start_param );

u32 yuv_helper_wait_for_thread( suite *test_suite, u32 thread_handle );

#ifdef __cplusplus
}
#endif /* extern "C" */

#endif /* YUV_TEXTURE_TOOLS_H */
