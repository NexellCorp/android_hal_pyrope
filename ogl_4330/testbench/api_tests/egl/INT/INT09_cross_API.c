/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*
 * Various tests that render into the same surface
 */

#include "../egl_framework.h"
#include "../egl_test_parameters.h"
#include <suite.h>

typedef struct
{
	EGLDisplay display;
	EGLSurface surface;
	EGLSurface surface_pbuffer;
	EGLSurface surface_client_pbuffer;
	EGLSurface surface_pixmap;
	EGLContext context_vg;
	EGLContext context_gles;
	EGLConfig config;
	EGLNativeWindowType win_native;
	EGLNativePixmapType pixmap;
	u32 width, height;
	egl_helper_windowing_state *windowing_state;
	VGImageFormat image_format;
	display_format pixmap_display_format;
	EGLNativeDisplayType native_dpy;
} fixture;

static void* create_fixture( suite *test_suite )
{
	static EGLint config_attributes[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT|EGL_OPENGL_ES2_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT | EGL_PIXMAP_BIT,
		EGL_NONE
	};
	EGLint context_attributes[] = { EGL_CONTEXT_CLIENT_VERSION, MALI_EGL_GLES_MAJOR_VERSION, EGL_NONE };
	EGLint pbuffer_attributes[] = { EGL_TEXTURE_TARGET, EGL_TEXTURE_2D, EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGB, EGL_WIDTH, TEST_DEFAULT_EGL_WIDTH_LARGE*2, EGL_HEIGHT, TEST_DEFAULT_EGL_HEIGHT_LARGE*2, EGL_NONE };
	EGLBoolean status;
	mempool *pool = &test_suite->fixture_pool;
	fixture *fix = (fixture *)_test_mempool_alloc( pool, sizeof( fixture ) );

	fix->config = (EGLConfig)NULL;
	fix->display = EGL_NO_DISPLAY;
	fix->surface = EGL_NO_SURFACE;
	fix->surface_pbuffer = EGL_NO_SURFACE;
	fix->surface_client_pbuffer = EGL_NO_SURFACE;
	fix->surface_pixmap = EGL_NO_SURFACE;
	fix->context_vg = EGL_NO_CONTEXT;
	fix->context_gles = EGL_NO_CONTEXT;
	fix->config = (EGLConfig)NULL;
	fix->windowing_state = NULL;
	fix->pixmap_display_format = DISPLAY_FORMAT_INVALID;

	{
		EGLint r, g, b, a;
		egl_test_get_requested_window_pixel_format( test_suite, &r, &g, &b, &a );
		if ( 8 == r && 8 == g && 8 == b && 8 == a )
		{
			fix->image_format = VG_sRGBA_8888;
			fix->pixmap_display_format = DISPLAY_FORMAT_ARGB8888;
		}
		else if ( 5 == r && 6 == g && 5 == b )
		{
			fix->image_format = VG_sRGB_565;
			fix->pixmap_display_format = DISPLAY_FORMAT_RGB565;
		}
		else if ( 4 == r && 4 == g && 4 == b && 4 == a )
		{
			fix->image_format = VG_sRGBA_4444;
			fix->pixmap_display_format = DISPLAY_FORMAT_ARGB4444;
		}
		else if ( 5 == r && 5 == g && 5 == b && 1 == a )
		{
			fix->image_format = VG_sRGBA_5551;
			fix->pixmap_display_format = DISPLAY_FORMAT_ARGB1555;
		}
	}
	/* Initialize the windowing system */
	status = egl_helper_windowing_init( &fix->windowing_state, test_suite );
	if ( EGL_TRUE != status )
	{
		fail( test_suite, "windowing abstraction failed to initialize" );
		/* fixture freed as part of the fixture pool */
		return NULL;
	}

	status = egl_helper_create_window( fix->windowing_state, TEST_DEFAULT_EGL_WIDTH_LARGE*2, TEST_DEFAULT_EGL_HEIGHT_LARGE*2, &fix->win_native ) ;
	ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_create_window failed" );
	fix->width = egl_helper_get_window_width( fix->windowing_state, fix->win_native );
	ASSERT_EGL_EQUAL( fix->width, TEST_DEFAULT_EGL_WIDTH_LARGE*2, "egl_helper_get_window_width failed" );
	fix->height = egl_helper_get_window_height( fix->windowing_state, fix->win_native );
	ASSERT_EGL_EQUAL( fix->height, TEST_DEFAULT_EGL_HEIGHT_LARGE*2, "egl_helper_get_window_height failed" );

	status = egl_helper_create_nativepixmap_extended( test_suite, TEST_DEFAULT_EGL_WIDTH_LARGE*2, TEST_DEFAULT_EGL_HEIGHT_LARGE*2, fix->pixmap_display_format, EGL_OPENGL_ES2_BIT, &fix->pixmap );
	ASSERT_EGL_EQUAL( status, EGL_TRUE, "Pixmap creation failed" );

	status = egl_helper_prepare_display( test_suite, &fix->display, EGL_TRUE, &fix->native_dpy );

	status = egl_test_get_window_config_with_attributes( test_suite, fix->display, config_attributes, &fix->config );
	ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_test_get_window_config failed" );

	if ( (EGLConfig)NULL == fix->config )
	{
		return NULL;
	}


	eglBindAPI( EGL_OPENVG_API );
	fix->context_vg = eglCreateContext( fix->display, fix->config, (EGLContext)NULL, NULL );
	ASSERT_EGL_NOT_EQUAL( EGL_NO_CONTEXT, fix->context_vg, "eglCreateContext failed creating OpenVG context" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateContext sat an error" );

	eglBindAPI( EGL_OPENGL_ES_API );
	fix->context_gles = eglCreateContext( fix->display, fix->config, (EGLContext)NULL, context_attributes );
	ASSERT_EGL_NOT_EQUAL( EGL_NO_CONTEXT, fix->context_gles, "eglCreateContext failed creating OpenGL ES context" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateContext sat an error" );

	eglBindAPI( EGL_OPENVG_API );
	fix->surface = eglCreateWindowSurface( fix->display, fix->config, fix->win_native, NULL );
	ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, fix->surface, "eglCreateWindowSurface failed" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreateWindowSurface sat an error" );

	eglBindAPI( EGL_OPENGL_ES_API );
	fix->surface_pbuffer = eglCreatePbufferSurface( fix->display, fix->config, pbuffer_attributes );
	ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, fix->surface, "eglCreatePbufferSurface failed" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreatePbufferSurface sat an error" );

	fix->surface_pixmap = eglCreatePixmapSurface( fix->display, fix->config, fix->pixmap, NULL );
	ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, fix->surface_pixmap, "eglCreatePixmapSurface failed" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreatePixmapSurface sat an error" );

	return fix;
}

static void remove_fixture( suite *test_suite )
{
	fixture *fix = (fixture *)test_suite->fixture;
	EGLBoolean status;

	eglBindAPI( EGL_OPENGL_ES_API );

	status = eglMakeCurrent( fix->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

	if ( EGL_NO_SURFACE != fix->surface ) eglDestroySurface( fix->display, fix->surface );
	if ( EGL_NO_SURFACE != fix->surface_pbuffer ) eglDestroySurface( fix->display, fix->surface_pbuffer );
	if ( EGL_NO_SURFACE != fix->surface_pixmap ) eglDestroySurface( fix->display, fix->surface_pixmap );
	if ( EGL_NO_CONTEXT != fix->context_vg ) eglDestroyContext( fix->display, fix->context_vg );
	if ( EGL_NO_CONTEXT != fix->context_gles ) eglDestroyContext( fix->display, fix->context_gles );
	if ( EGL_NO_DISPLAY != fix->display ) egl_helper_terminate_display( test_suite, fix->display, &fix->native_dpy );

	status = egl_helper_destroy_window( fix->windowing_state, fix->win_native );
	ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_destroy_window failed" );

	if ( NULL != fix->windowing_state )
	{
		EGLBoolean status = egl_helper_windowing_deinit( &fix->windowing_state );
		ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_windowing_deinit failed" );
	}

	ASSERT_FAIL( 0 == egl_helper_free_nativepixmap( fix->pixmap ) );
}

static void test_cross_API( suite *test_suite )
{
	VGPath  vg_path;
	VGPaint vg_paint1;
	VGPaint vg_paint2;
	VGImage image;
	VGImage image_pixmap;
	VGfloat red[] = {1.0f, 0.0f, 0.0f, 1.0f };
	VGfloat green[] = {0.0f, 1.0f, 0.0f, 1.0f };
	VGfloat blue[] = {0.0f, 0.0f, 1.0f, 1.0f };
	VGfloat white[] = {1.0f, 1.0f, 1.0f, 1.0f };
	VGfloat black[] = {0.0f, 0.0f, 0.0f, 0.0f };
	VGfloat yellow[] = {1.0f, 1.0f, 0.0f, 1.0f };
	GLuint gles_tex_id;
	GLuint prog, vs, fs;
	static GLfloat center_quad[12] =
	{
		-0.5, -0.5, 0.0f,
		 0.5, -0.5, 0.0f,
		 0.5,  0.5, 0.0f,
		-0.5,  0.5, 0.0f
	};
	static GLfloat vertices_triangle1[] =
	{
		 0.0, 1.0, 0.0,
		 1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0
	};
	static GLfloat vertices_triangle2[] =
	{
		 0.0, -1.0, 0.0,
		-1.0,  0.0, 0.0,
		 1.0,  0.0, 0.0
	};
	static GLfloat gles_color1[] = { 0.5f, 0.0f, 0.0f, 1.0f };
	static GLfloat gles_color2[] = { 1.0f, 0.0f, 0.0f, 1.0f };
	EGLSurface pbuffer;
	EGLint surface_attribute_pbuffer[] = { EGL_COLORSPACE, EGL_COLORSPACE_sRGB, EGL_ALPHA_FORMAT, EGL_ALPHA_FORMAT_PRE, EGL_RENDER_BUFFER, EGL_SINGLE_BUFFER, EGL_NONE };

	fixture *fix = (fixture *)test_suite->fixture;
	EGLBoolean status;
	if ( (EGLConfig)NULL == fix->config ) return;

	/* Case 1: Render into pbuffer using OpenVG. Use this surface as texture in OpenGL ES */
	eglBindAPI( EGL_OPENVG_API );
	status = eglMakeCurrent( fix->display, fix->surface_pbuffer, fix->surface_pbuffer, fix->context_vg );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed setting OpenVG context current" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

	vgSetfv( VG_CLEAR_COLOR, 4, white );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgClear( 0, 0, TEST_DEFAULT_EGL_WIDTH_LARGE*2, TEST_DEFAULT_EGL_HEIGHT_LARGE*2);
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vg_path = vgCreatePath( VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0, 0.0, 0, 0, VG_PATH_CAPABILITY_ALL );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vguEllipse( vg_path, 2*TEST_DEFAULT_EGL_WIDTH_LARGE/2.0f, 2*TEST_DEFAULT_EGL_HEIGHT_LARGE/2.0f, 2*TEST_DEFAULT_EGL_WIDTH_LARGE/1.2f, 2*TEST_DEFAULT_EGL_HEIGHT_LARGE/1.2f );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vg_paint1 = vgCreatePaint();
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgSetColor( vg_paint1, 0xFF0000FF );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vg_paint2 = vgCreatePaint();
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgSetColor( vg_paint2, 0x000000FF );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgSetPaint( vg_paint1, VG_FILL_PATH );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgSetPaint( vg_paint2, VG_STROKE_PATH );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgLoadIdentity();
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgSeti( VG_STROKE_LINE_WIDTH, 5 );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgSeti( VG_STROKE_JOIN_STYLE, VG_JOIN_BEVEL );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgDrawPath( vg_path, VG_FILL_PATH );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgDrawPath( vg_path, VG_STROKE_PATH );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgDestroyPath( vg_path );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgDestroyPaint( vg_paint1 );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgDestroyPaint( vg_paint2 );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vg_path = vgCreatePath( VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0, 0.0, 0, 0, VG_PATH_CAPABILITY_ALL );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vguEllipse( vg_path, 2*TEST_DEFAULT_EGL_WIDTH_LARGE/2.0f, 2*TEST_DEFAULT_EGL_HEIGHT_LARGE/2.0f, 2*TEST_DEFAULT_EGL_WIDTH_LARGE/4.0f, 2*TEST_DEFAULT_EGL_HEIGHT_LARGE/4.0f );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vg_paint1 = vgCreatePaint();
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgSetColor( vg_paint1, 0xFFFF007F );  /* 50% alpha */
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vg_paint2 = vgCreatePaint();
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgSetColor( vg_paint2, 0x000000FF );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgSetPaint( vg_paint1, VG_FILL_PATH );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgSetPaint( vg_paint2, VG_STROKE_PATH );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgLoadIdentity();
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgSeti( VG_STROKE_LINE_WIDTH, 2 );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgSeti( VG_STROKE_JOIN_STYLE, VG_JOIN_BEVEL );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgDrawPath( vg_path, VG_FILL_PATH );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgDrawPath( vg_path, VG_STROKE_PATH );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgDestroyPath( vg_path );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgDestroyPaint( vg_paint1 );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgDestroyPaint( vg_paint2 );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	status = eglMakeCurrent( fix->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed to unset context" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

	/* bind the pbuffer as a texture with OpenGL ES */
	eglBindAPI( EGL_OPENGL_ES_API );
	status = eglMakeCurrent( fix->display, fix->surface_pbuffer, fix->surface_pbuffer, fix->context_gles );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed setting OpenGL ES context current" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	#if 0
	glEnable(GL_TEXTURE_2D);
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );
	#endif

	glGenTextures(1, &gles_tex_id );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	glActiveTexture( GL_TEXTURE0 );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	glBindTexture( GL_TEXTURE_2D, gles_tex_id );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	status = eglBindTexImage( fix->display, fix->surface_pbuffer, EGL_BACK_BUFFER );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglBindTexImage failed" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglBindTexImage sat an error" );

	/* go back to regular surface */
	status = eglMakeCurrent( fix->display, fix->surface, fix->surface, fix->context_gles );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed setting OpenGL ES context current" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	/* render using the bound pbuffer as texture with OpenGL ES */
	prog = egl_load_shader2("egl-pass_two_attribs.vert", "egl-texture.frag", &vs, &fs);
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	egl_draw_custom_square_textured_offset(center_quad, 0.0f, 0.0f, gles_tex_id);
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	egl_unload_shader2( prog, vs, fs);
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	glDeleteTextures( 1, &gles_tex_id );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	gl_save_backbuffer( "cross_API1.ppm", 0, 0, TEST_DEFAULT_EGL_WIDTH_LARGE*2, TEST_DEFAULT_EGL_HEIGHT_LARGE*2 );
	eglSwapBuffers( fix->display, fix->surface );

	status = eglReleaseTexImage( fix->display, fix->surface_pbuffer, EGL_BACK_BUFFER );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglReleaseTexImage failed" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglReleaseTexImage sat an error" );

	status = eglMakeCurrent( fix->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed to unset context" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

	eglBindAPI( EGL_OPENVG_API );
	status = eglMakeCurrent( fix->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed to unset context" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );


	/* Case 2: Render OpenVG content into client buffer, draw into a pbuffer, bind texture to gles */
	eglBindAPI( EGL_OPENVG_API );
	status = eglMakeCurrent( fix->display, fix->surface_pbuffer, fix->surface_pbuffer, fix->context_vg );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

	image = vgCreateImage( fix->image_format, fix->width, fix->height, VG_IMAGE_QUALITY_NONANTIALIASED );

	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );
	pbuffer = eglCreatePbufferFromClientBuffer( fix->display, EGL_OPENVG_IMAGE, (EGLClientBuffer)image, fix->config, surface_attribute_pbuffer );
	ASSERT_EGL_NOT_EQUAL( EGL_NO_SURFACE, pbuffer, "eglCreatePbufferFromClientBuffer failed" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglCreatePbufferFromClientBuffer sat an error" );

	status = eglMakeCurrent( fix->display, pbuffer, pbuffer, fix->context_vg );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed " );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

	/* render into client buffer (VGImage image) */
	/* should give an image with:
	 * - yellow
	 * - red
	 * - green
	 * - blue
	 */
	vgSetfv( VG_CLEAR_COLOR, 4, yellow);
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );
	vgClear( 0, 0, fix->width, fix->height );

	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgSetfv( VG_CLEAR_COLOR, 4, red );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );
	vgClear( 0, 3*fix->height/4, fix->width, fix->height/4 );

	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgSetfv( VG_CLEAR_COLOR, 4, green );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );
	vgClear( 0, 2*fix->height/4, fix->width, fix->height/4 );

	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgSetfv( VG_CLEAR_COLOR, 4, blue );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );
	vgClear( 0, fix->height/4, fix->width, fix->height/4 );

	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	status = eglMakeCurrent( fix->display, fix->surface_pbuffer, fix->surface_pbuffer, fix->context_vg );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed setting OpenVG context current" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

	/* render image */
	vgSetfv( VG_CLEAR_COLOR, 4, white );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );
	vgClear( 0, 0, fix->width, fix->height );

	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );
	vgDrawImage( image );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	status = eglMakeCurrent( fix->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed to unset context" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

	/* bind the pbuffer as a texture with OpenGL ES */
	eglBindAPI( EGL_OPENGL_ES_API );
	status = eglMakeCurrent( fix->display, fix->surface_pbuffer, fix->surface_pbuffer, fix->context_gles );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed setting OpenGL ES context current" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	#if 0
	glEnable(GL_TEXTURE_2D);
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );
	#endif

	glGenTextures(1, &gles_tex_id );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	glActiveTexture( GL_TEXTURE0 );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	glBindTexture( GL_TEXTURE_2D, gles_tex_id );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );


	status = eglBindTexImage( fix->display, fix->surface_pbuffer, EGL_BACK_BUFFER );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglBindTexImage failed" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglBindTexImage sat an error" );

	/* go back to regular surface */
	status = eglMakeCurrent( fix->display, fix->surface, fix->surface, fix->context_gles );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed setting OpenGL ES context current" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

	glClearColor( 1.0f, 1.0f, 1.0f, 1.0f );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );


	/* render using the bound pbuffer as texture with OpenGL ES */
	prog = egl_load_shader2("egl-pass_two_attribs.vert", "egl-texture.frag", &vs, &fs);
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	egl_draw_custom_square_textured_offset(center_quad, 0.0f, 0.0f, gles_tex_id);
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	egl_unload_shader2( prog, vs, fs);
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	glDeleteTextures( 1, &gles_tex_id );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	gl_save_backbuffer( "cross_API2.ppm", 0, 0, TEST_DEFAULT_EGL_WIDTH_LARGE*2, TEST_DEFAULT_EGL_HEIGHT_LARGE*2 );
	eglSwapBuffers( fix->display, fix->surface );

	status = eglMakeCurrent( fix->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed to unset context" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );


	/* Case 3: Render OpenGL ES content into a pixmap surface, render content with OpenVG using vg image */
	status = eglMakeCurrent( fix->display, fix->surface_pixmap, fix->surface_pixmap, fix->context_gles );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed setting OpenGL ES context current" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

	glClearColor( 1.0f, 1.0f, 1.0f, 1.0f );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	glClear( GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	prog = egl_load_shader2("egl-pass_two_attribs.vert", "egl-color.frag", &vs, &fs);
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	glDisable(GL_DEPTH_TEST);
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	glDisable(GL_CULL_FACE);
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	egl_draw_custom_triangle( vertices_triangle1, gles_color1 );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	egl_draw_custom_triangle( vertices_triangle2, gles_color2);
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	egl_unload_shader2( prog, vs, fs );
	ASSERT_GLES_ERROR( glGetError(), GL_NO_ERROR, "GLES" );

	eglWaitClient();

	eglBindAPI( EGL_OPENVG_API );
	status = eglMakeCurrent( fix->display, fix->surface, fix->surface, fix->context_vg );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed setting OpenVG context current" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );

	/* convert the pixmap into a VGImage */
	image_pixmap = vgCreateImage( VG_sRGB_565, egl_helper_get_nativepixmap_width(fix->pixmap), egl_helper_get_nativepixmap_height(fix->pixmap), VG_IMAGE_QUALITY_NONANTIALIASED );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	{
		u16 *data = egl_helper_nativepixmap_map_data( fix->pixmap );
		vgImageSubData( image_pixmap, data, egl_helper_get_nativepixmap_pitch(fix->pixmap), VG_sRGB_565, 0, 0, egl_helper_get_nativepixmap_width(fix->pixmap), egl_helper_get_nativepixmap_height(fix->pixmap) );
		ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );
		egl_helper_nativepixmap_unmap_data( fix->pixmap );
	}

	/* clear the surface */
	vgSetfv( VG_CLEAR_COLOR, 4, black );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgClear( 0, 0, TEST_DEFAULT_EGL_WIDTH_LARGE*2, TEST_DEFAULT_EGL_HEIGHT_LARGE*2 );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	/* draw the gles content as an image, scaled */
	vgSeti( VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgLoadIdentity();
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgScale( 0.5f, 0.5f );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgTranslate( TEST_DEFAULT_EGL_WIDTH_LARGE, TEST_DEFAULT_EGL_HEIGHT_LARGE );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	vgDrawImage( image_pixmap );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );


	vg_save_backbuffer( "cross_API3.ppm", 0, 0, TEST_DEFAULT_EGL_WIDTH_LARGE*2, TEST_DEFAULT_EGL_HEIGHT_LARGE*2 );
	ASSERT_VG_ERROR( vgGetError(), VG_NO_ERROR, "VG" );

	eglSwapBuffers( fix->display, fix->surface );

	status = eglMakeCurrent( fix->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
	ASSERT_EGL_EQUAL( EGL_TRUE, status, "eglMakeCurrent failed to unset context" );
	ASSERT_EGL_ERROR( eglGetError(), EGL_SUCCESS, "eglMakeCurrent sat an error" );
}

/**
 *	Description:
 *	The only public function in the test suite is the suite factory function.
 *	This creates the suite and adds all the tests to it.
 *
 */
suite* create_suite_cross_API(mempool* pool, result_set* results)
{
	suite* new_suite = create_suite(  pool, "cross_API", create_fixture,
	                                 remove_fixture, results );

	add_test_with_flags( new_suite, "INT09_cross_API", test_cross_API,
	                     FLAG_TEST_ALL | FLAG_TEST_SMOKETEST );

	return new_suite;
}

