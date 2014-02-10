/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef EGL_TEST_PARAMETERS_H
#define EGL_TEST_PARAMETERS_H

#include "../../unit_framework/framework_main.h"
#include <suite.h>
#include <EGL/egl.h>

typedef struct
{
	struct
	{
		unsigned int red_size;
		unsigned int green_size;
		unsigned int blue_size;
		unsigned int alpha_size;
	} window_pixel_format;
} egl_test_parameters;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief set the pixel format the test suite to use for window surfaces
 * \param test_suite The test suite struct
 * \param red Number of bits for the red pixel component
 * \param green Number of bits for the green pixel component
 * \param blue Number of bits for the blue pixel component
 * \param alpha Number of bits for the alpha pixel component
 */
void egl_test_set_window_pixel_format( suite* test_suite, EGLint red, EGLint green, EGLint blue, EGLint alpha );
/**
 * \brief Returns the pixel format the test suite is requested to use for window surfaces
 * \param test_suite The test suite struct
 * \param red Number of bits for the red pixel component
 * \param green Number of bits for the green pixel component
 * \param blue Number of bits for the blue pixel component
 * \param alpha Number of bits for the alpha pixel component
 */
void egl_test_get_requested_window_pixel_format( suite* test_suite, EGLint* red, EGLint* green, EGLint* blue, EGLint* alpha );

/**
 * \brief Checks if a given config matches the requested window pixel format
 * \param suite The test suite struct
 * \param display The EGLDisplay the config belongs to
 * \param config The config to query
 * \returns EGL_TRUE if the config can be used to create an EGLWindowSurface, EGL_FALSE if not
 */
EGLBoolean egl_test_verify_config_matches_requested_window_pixel_format( suite* test_suite, EGLDisplay display, EGLConfig config );

/**
 * \brief Returns a config suitable to be used for creating an EGL surface. The RGBA-size parameters are automatically specified to be equal to the pixel format requested at test run time.
 * \param suite The test suite struct
 * \param dpy An EGLDisplay
 * \param attributes A standard EGL attribute list giving additional attributes to eglChooseConfig. Should be terminated with EGL_NONE
 * \param out_config This will contain an EGLConfig if the function is successful finding one that matches the requested attributes
 * \returns EGL_TRUE if a config is found, EGL_FALSE if not
 */
EGLBoolean egl_test_get_window_config_with_attributes( suite* test_suite, EGLDisplay dpy, EGLint* attributes, EGLConfig* out_config );

/**
 * \brief Same as calling egl_test_get_window_config_with_attributes, with the only additional attribute being EGL_RENDERABLE_TYPE
 * \param suite The test suite struct
 * \param dpy An EGLDisplay
 * \param renderable type, The value of the EGL_RENDERABLE_TYPE attribute passed to eglChooseConfig
 * \param out_config If a config is found, it's stored here
 * \returns EGL_TRUE if successful, EGL_FALSE if not.
 */
EGLBoolean egl_test_get_window_config( suite* test_suite, EGLDisplay dpy, EGLint renderable_type, EGLConfig* out_config );

#ifdef __cplusplus
}
#endif

#endif /* !defined(EGL_TEST_PARAMETERS_H) */
