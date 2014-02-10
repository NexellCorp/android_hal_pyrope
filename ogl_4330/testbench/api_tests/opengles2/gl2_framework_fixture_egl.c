/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include "gl2_framework.h"
#include "gl2_framework_parameters.h"
#include <string.h>
#include "../unit_framework/suite.h"

#include "egl_helpers.h"

#ifdef UNDER_CE
#include <rcdef.h> /* for WS_EX_APPWINDOW and WS_EX_WINDOWEDGE definitions */
#endif

typedef struct fixture {
	int width, height;
	int multisample;
	int render_to_fbo;
	int rgba[4];
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	EGLConfig  config;
	EGLConfig  *configs;
	EGLNativeWindowType fb_win;
	egl_helper_windowing_state *windowing_state;

} fixture;

int gles2_window_width(suite* test_suite)
{
	fixture* f = test_suite->fixture;
	return f->width;
}

int gles2_window_height(suite* test_suite)
{
	fixture* f = test_suite->fixture;
	return f->height;
}

int gles2_configuration_alpha_bits(suite* test_suite)
{
	fixture* f = test_suite->fixture;
	return f->rgba[3];
}

int gles2_window_multisample(suite* test_suite)
{
	fixture* f = test_suite->fixture;
	return f->multisample;
}

EGLDisplay gles2_window_get_display(suite* test_suite)
{
	fixture* f = test_suite->fixture;
	return f->display;
}

EGLSurface gles2_window_get_surface(suite* test_suite)
{
	fixture* f = test_suite->fixture;
	return f->surface;
}

EGLContext gles2_window_get_context(suite* test_suite)
{
	fixture* f = test_suite->fixture;
	return f->context;
}

EGLContext gles2_window_get_config(suite* test_suite)
{
	fixture* f = test_suite->fixture;
	return f->config;
}

egl_helper_windowing_state *gles2_framework_get_windowing_state( suite *test_suite )
{
	fixture* f = test_suite->fixture;
	return f->windowing_state;
}

void gles_deinit_egl( fixture *fix, suite* test_suite )
{
	if ( EGL_NO_DISPLAY != fix->display )
	{
		/* Only switch to no surface if there is a current display - some tests close the display */
		if( EGL_NO_DISPLAY != eglGetCurrentDisplay() )
		{
			if( EGL_FALSE == eglMakeCurrent( fix->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT ) )
			{
				return;
			}
		}
		if ( EGL_NO_CONTEXT != fix->context )
		{
			eglDestroyContext( fix->display, fix->context );
		}
		if ( EGL_NO_SURFACE != fix->surface )
		{
			eglDestroySurface( fix->display, fix->surface );
		}
		if ( 0 != fix->fb_win )
		{
			EGLBoolean status =
				egl_helper_destroy_window( fix->windowing_state, fix->fb_win );
			ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_destroy_window failed" );
		}
		eglTerminate( fix->display );
	}
	fix->display = EGL_NO_DISPLAY;
	fix->surface = EGL_NO_SURFACE;
	fix->context = EGL_NO_CONTEXT;
	fix->config = NULL;
	fix->fb_win = 0;
	if ( NULL != fix->configs ) free( fix->configs );
	fix->configs = NULL;
}

EGLBoolean gles_init_egl( fixture *fix, int version)
{
	int attr = 0;
	int major, minor;
	int max_configs;
	int i = 0;
	EGLint attribs[256];
	EGLint attribs_context[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	int allowed_FBO_formats[3][5] =
		{
			{5, 6, 5, 0, GL_RGB565},
			{5, 5, 5, 1, GL_RGB5_A1},
			{4, 4, 4, 4, GL_RGBA4}
		};
	EGLint num_configs = 0;
	EGLBoolean status;
	EGLBoolean assigned_config = EGL_FALSE;

	fix->display = EGL_NO_DISPLAY;
	fix->surface = EGL_NO_SURFACE;
	fix->context = EGL_NO_CONTEXT;
	fix->config  = NULL;
	fix->configs  = NULL;

	fix->display = eglGetDisplay( EGL_DEFAULT_DISPLAY );
	if ( EGL_NO_DISPLAY == fix->display ) return EGL_FALSE;
	if ( EGL_FALSE == eglInitialize( fix->display, &major, &minor ) ) return EGL_FALSE;

	status = eglGetConfigs(fix->display, NULL, 0, &max_configs);
	if(max_configs <= 0) return EGL_FALSE;
	if( status == EGL_FALSE ) return status;
	fix->configs = (EGLConfig *)malloc( sizeof( EGLConfig) * max_configs );
	if ( NULL == fix->configs ) return EGL_FALSE;

	attr = 0;
	attribs[attr++] = EGL_RENDERABLE_TYPE;
	if(version == 2)
	{
		attribs[attr++] = EGL_OPENGL_ES2_BIT;
	} else
	{
		attribs[attr++] = EGL_OPENGL_ES_BIT;
		attribs_context[1] = 1;
	}


	if(fix->multisample > 1)
	{
		attribs[attr++] = EGL_SAMPLES;
		attribs[attr++] = fix->multisample;
	}

	attribs[attr++] = EGL_RED_SIZE;
	attribs[attr++] = fix->rgba[0];
	attribs[attr++] = EGL_GREEN_SIZE;
	attribs[attr++] = fix->rgba[1];
	attribs[attr++] = EGL_BLUE_SIZE;
	attribs[attr++] = fix->rgba[2];
	attribs[attr++] = EGL_ALPHA_SIZE;
	attribs[attr++] = fix->rgba[3];

	attribs[attr++] = EGL_DEPTH_SIZE;
	attribs[attr++] = 24;
	attribs[attr++] = EGL_STENCIL_SIZE;
	attribs[attr++] = 8;

	attribs[attr++] = EGL_NONE;

	if ( EGL_FALSE == eglChooseConfig( fix->display, attribs, fix->configs, max_configs, &num_configs ) ) return EGL_FALSE;
	if ( 0 == num_configs ) return EGL_FALSE;

	for ( i=0; i<num_configs; i++ )
	{
		EGLint value;
		EGLConfig config;
		config = fix->configs[i];

		/*Use this to explicitly check that the EGL config has the expected color depths */
		eglGetConfigAttrib( fix->display, config, EGL_RED_SIZE, &value );
		if (  fix->rgba[0] != value ) continue;
		eglGetConfigAttrib( fix->display, config, EGL_GREEN_SIZE, &value );
		if (  fix->rgba[1] != value ) continue;
		eglGetConfigAttrib( fix->display, config, EGL_BLUE_SIZE, &value );
		if (  fix->rgba[2] != value ) continue;
		eglGetConfigAttrib( fix->display, config, EGL_ALPHA_SIZE, &value );
		if (  fix->rgba[3] != value ) continue;
		eglGetConfigAttrib( fix->display, config, EGL_SAMPLES, &value );
		if ( fix->multisample != value ) continue;
	
		assigned_config = EGL_TRUE;
		fix->config = config;
		break;
	}
	if ( EGL_FALSE == assigned_config )
	{
		fprintf(stderr, "WARNING: unable to find suitable EGL config\n");
		return EGL_FALSE;
	}

	fix->surface = eglCreateWindowSurface( fix->display, fix->config, (EGLNativeWindowType)fix->fb_win, NULL );
	if ( EGL_NO_SURFACE == fix->surface )
	{
		fprintf(stderr, "WARNING: unable to create a window surface\n");
		return EGL_FALSE;
	}

	fix->context = eglCreateContext( fix->display, fix->config, EGL_NO_CONTEXT, attribs_context );
	if ( EGL_NO_CONTEXT == fix->context )
	{
		fprintf(stderr, "WARNING: unable to create a context\n");
		return EGL_FALSE;
	}

	if ( EGL_FALSE == eglMakeCurrent( fix->display, fix->surface, fix->surface, fix->context ) )
	{
		fprintf(stderr, "WARNING: unable to make the created context current\n");
		return EGL_FALSE;
	}

	/* if we want to render to an FBO, create and bind an FBO of the proper size */
	if(fix->render_to_fbo)
	{
		GLuint myfbo, renderbuffer;
		GLenum framebufferstatus;
		int i;
		GLenum format = GL_INVALID_ENUM;

		glGenFramebuffers(1, &myfbo);
		if(myfbo == 0) return EGL_FALSE;
		glGenRenderbuffers(1, &renderbuffer);
		if(renderbuffer == 0) return EGL_FALSE;
		glBindFramebuffer(GL_FRAMEBUFFER, myfbo);

		glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
		for(i = 0; i < 3; ++i)
		{
			if (fix->rgba[0] == allowed_FBO_formats[i][0] &&
			    fix->rgba[1] == allowed_FBO_formats[i][1] &&
			    fix->rgba[2] == allowed_FBO_formats[i][2] &&
			    fix->rgba[3] == allowed_FBO_formats[i][3])
			{
				format = (GLenum)allowed_FBO_formats[i][4];
			}
		}

		if (format != GL_INVALID_ENUM)
		{
			glRenderbufferStorage(GL_RENDERBUFFER, format, fix->width, fix->height);
		}
		else
		{
#ifdef GL_RGB8_OES
			if(strstr((char*) glGetString(GL_EXTENSIONS), "GL_OES_rgb8_rgba8") != NULL
				&& fix->rgba[0] == 8 && fix->rgba[1] == 8 && fix->rgba[2] == 8 && fix->rgba[3] == 0)
			{
				glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB, fix->width, fix->height);
			} else
#endif

#ifdef GL_RGBA8_OES
			if(strstr((char*) glGetString(GL_EXTENSIONS), "GL_OES_rgb8_rgba8") != NULL
				&& fix->rgba[0] == 8 && fix->rgba[1] == 8 && fix->rgba[2] == 8 && fix->rgba[3] == 8)
			{
				glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, fix->width, fix->height);
			} else
#endif
			{
				fprintf(stderr, "Unable to create a FBO colorbuffer of the specified colordepth\n");
				return EGL_FALSE;
			}
		}

		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);

		glGenRenderbuffers(1, &renderbuffer);
		if(renderbuffer == 0) return EGL_FALSE;
		glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, fix->width, fix->height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);

		glGenRenderbuffers(1, &renderbuffer);
		if(renderbuffer == 0) return EGL_FALSE;
		glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, fix->width, fix->height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);


		framebufferstatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(framebufferstatus != GL_FRAMEBUFFER_COMPLETE)
		{
			fprintf(stderr, "Created framebuffer is not framebuffer complete\n");
			return EGL_FALSE;
		}
		if(glGetError() != GL_NO_ERROR)
		{
			fprintf(stderr, "Errors occurred while creating framebuffer\n");
			return EGL_FALSE;
		}
		/* initialize buffers to match default state when we don't use FBO rendering */
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

	return EGL_TRUE;
}

/* windows display handling */
#if defined(TESTBENCH_DESKTOP_GL) || defined(UNDER_CE)
LRESULT CALLBACK processWindow(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam) {
	switch(uiMsg) {
		case WM_CLOSE:
				PostQuitMessage(0);
				return 0;

		case WM_ACTIVATE:
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SIZE:
				return 0;
	}

	return DefWindowProc(hWnd, uiMsg, wParam, lParam);
}

HWND createWindow(int x, int y, int uiWidth, int uiHeight) {
	WNDCLASS wc;
	RECT wRect;
	HWND sWindow;
	HINSTANCE hInstance;

	wRect.left = 0L;
	wRect.right = (long)uiWidth;
	wRect.top = 0L;
	wRect.bottom = (long)uiHeight;

	hInstance = GetModuleHandle(NULL);

#ifdef UNDER_CE
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.hIcon = NULL;
	wc.hCursor = NULL;
#else
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
#endif
	wc.lpfnWndProc = (WNDPROC)processWindow;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"OGLES";

	RegisterClass(&wc);

#ifdef UNDER_CE
	AdjustWindowRectEx(&wRect, WS_CLIPSIBLINGS | WS_CLIPCHILDREN, FALSE, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
	sWindow = CreateWindowEx(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, L"OGLES", L"main", WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, wRect.right - wRect.left, wRect.bottom - wRect.top, NULL, NULL, hInstance, NULL);
#else
	AdjustWindowRectEx(&wRect, WS_POPUPWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, FALSE, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
	sWindow = CreateWindowEx(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, L"OGLES", L"main", WS_POPUPWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, wRect.right - wRect.left, wRect.bottom - wRect.top, NULL, NULL, hInstance, NULL);
#endif


	ShowWindow(sWindow, SW_SHOW);

	return sWindow;
}
#endif /* TESTBENCH_DESKTOP_GL */

void* gles_create_fixture(suite* test_suite, int version)
{
	mempool* pool = &test_suite->fixture_pool;
	fixture* fix = (fixture*)_test_mempool_alloc(pool,sizeof(fixture));
	const char* config_name;
	unsigned int config_flags;
	int retval;

	retval = gl2_get_config_information(test_suite->fixture_index, &config_name, &fix->width, &fix->height, fix->rgba, &fix->multisample, &config_flags);
	if(retval == 1)
	{
		printf("warning: fixture %i does not exist\n", test_suite->fixture_index);
		exit(1);
	}

	if( (test_suite->api_specific_fixture_filters & config_flags) != 0)
	{
		printf("Skipping test %s.%s on config %s due to filtering\n", test_suite->name, test_suite->this_test->name, config_name);
		return NULL;
	}

	test_suite->fixture_name = config_name;
	fix->render_to_fbo = config_flags & CONFIG_FLAG_RENDER_TO_FBO;

	/* Initialize the windowing system */
	if ( EGL_FALSE == egl_helper_windowing_init( &fix->windowing_state, test_suite ))
	{
		fprintf(stderr,"Window init failed on test %s\n", test_suite->name);
		return NULL;
	}
	if ( EGL_FALSE == egl_helper_create_window(fix->windowing_state,
                    fix->width, fix->height, &fix->fb_win ))
	{
		fprintf(stderr,"egl_helper_create_window failed on test %s\n", test_suite->name);
		return NULL;
	}

	if ( EGL_FALSE == gles_init_egl( fix, version ) )
	{
		fprintf(stderr,"EGL init failed on test %s\n", test_suite->name);
		gles_deinit_egl( fix, test_suite );
		return NULL;
	}

	/* egl_helper_create_window allocates the entire frambuffer device, and so sets viewport to the native resolution (1360x768 on Odroid), so the default viewport is wrong
	   This is a hacky "fix" */
#ifdef ANDROID
	glEnable(GL_SCISSOR_TEST);
	glScissor(0, 0, fix->width, fix->height);
	glViewport(0, 0, fix->width, fix->height);
#endif
	glDisable(GL_DITHER);

	return fix;
}

void* gles1_create_fixture(suite* test_suite)
{
	return gles_create_fixture(test_suite, 1);
}

void* gles2_create_fixture(suite* test_suite)
{
	return  gles_create_fixture(test_suite, 2);
}

/* this is used by the multisample test suite */
void* gles2_create_fixture_with_special_dimensions(suite* test_suite, int width, int height, char rgba[4], int multisample)
{
	mempool* pool = &test_suite->fixture_pool;
	fixture* fix = (fixture*)_test_mempool_alloc(pool,sizeof(fixture));

	fix->width = width;
	fix->height = height;
	fix->rgba[0] = rgba[0];
	fix->rgba[1] = rgba[1];
	fix->rgba[2] = rgba[2];
	fix->rgba[3] = rgba[3];
	fix->multisample = multisample;
	fix->render_to_fbo = 0;

	/* Initialize the windowing system */
	if ( EGL_FALSE == egl_helper_windowing_init( &fix->windowing_state, test_suite ))
	{
		return NULL;
	}
#if defined(ANDROID) && !defined(USE_TPI)
	/* create window outside framebuffer */
	if ( EGL_FALSE == egl_helper_create_window_outside_framebuffer(fix->windowing_state,
                    fix->width, fix->height, &fix->fb_win ))
#else
	if ( EGL_FALSE == egl_helper_create_window(fix->windowing_state,
                    fix->width, fix->height, &fix->fb_win ))
#endif
	{
		return NULL;
	}

	if ( EGL_FALSE == gles_init_egl( fix, 2 ) )
	{
		gles_deinit_egl( fix, test_suite );
		return NULL;
	}
	
	/* egl_helper_create_window allocates the entire frambuffer device, and so sets viewport to the native resolution (1360x768 on Odroid), so the default viewport is wrong
	   This is a hacky "fix" */
#ifdef ANDROID
	glEnable(GL_SCISSOR_TEST);
	glScissor(0, 0, fix->width, fix->height);
	glViewport(0, 0, fix->width, fix->height);
#endif
	glDisable(GL_DITHER);

	return fix;
}

void gles_share_context_remove(suite* test_suite, EGLContext context)
{
	fixture* fix = (fixture*)test_suite->fixture;

	if (EGL_NO_DISPLAY != fix->display)
	{
		if (EGL_NO_CONTEXT != context)
		{
			eglDestroyContext(fix->display, context);
			context = EGL_NO_CONTEXT;
		}
	}
}

EGLContext gles_share_context_create(suite* test_suite, int version, EGLContext share_list)
{
	EGLint num_configs;
	EGLint config_attribs[256];
	EGLint context_attributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	EGLContext context;
	int attr;
	EGLBoolean status;
	int max_configs;

	fixture* fix = (fixture*)test_suite->fixture;

	attr = 0;

	status = eglGetConfigs(fix->display, NULL, 0, &max_configs);
		if(max_configs <= 0) return EGL_FALSE;
		if( status == EGL_FALSE ) return EGL_NO_CONTEXT;
		fix->configs = (EGLConfig *)MALLOC( sizeof( EGLConfig) * max_configs );
		if ( NULL == fix->configs ) return EGL_FALSE;

	/* Make sure it's OpenGL 2.x compatible */
	config_attribs[attr++] = EGL_RENDERABLE_TYPE;
	if(version == 1)
	{
		config_attribs[attr++] = EGL_OPENGL_ES_BIT;
		context_attributes[1] = 1;
	}
	else
	{
		config_attribs[attr++] = EGL_OPENGL_ES2_BIT;
	}

	/* Terminate list */
	config_attribs[attr++] = EGL_NONE;

	/* check if there's any suitable configs... */
	if (EGL_FALSE == eglChooseConfig(fix->display, config_attribs, fix->configs, max_configs, &num_configs)) return EGL_NO_CONTEXT;
	if ( 0 == num_configs ) return EGL_FALSE;
	/* Choose the first one */
	fix->config = fix->configs[0];

	if ( NULL == fix->config )
	{
		fprintf(stderr, "WARNING: unable to find suitable EGL config\n");
		return EGL_FALSE;
	}

	/* try to create a context */
	context = eglCreateContext(fix->display, fix->config, share_list, context_attributes );
	if (EGL_NO_CONTEXT == context)	return context;

	/* bind it all  */
	if (!eglMakeCurrent(fix->display, fix->surface, fix->surface, context))
	{
		gles_share_context_remove(test_suite, context);
		return EGL_NO_CONTEXT;
	}

	return context;
}

void context_make_current( suite* test_suite, EGLContext context )
{
	fixture* fix = (fixture*)test_suite->fixture;

	if (!eglMakeCurrent(fix->display, fix->surface, fix->surface, context))
	return;
}


void gles2_remove_fixture(suite* test_suite)
{
	fixture* fix = (fixture*)test_suite->fixture;
	test_suite->fixture_name = NULL;

	gles_deinit_egl( fix, test_suite );

	if( fix->windowing_state != NULL ) {
		EGLBoolean status = egl_helper_windowing_deinit( &fix->windowing_state );
		/* Failure to cleanup can cause all subsequent tests to fail on WinCE */
		ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_windowing_deinit failed" );
	}
}

void gles1_remove_fixture(suite* test_suite)
{
	fixture* fix = (fixture*)test_suite->fixture;
	test_suite->fixture_name = NULL;

	gles_deinit_egl( fix, test_suite );

	if( fix->windowing_state != NULL ) {
		EGLBoolean status = egl_helper_windowing_deinit( &fix->windowing_state );
		/* Failure to cleanup can cause all subsequent tests to fail on WinCE */
		ASSERT_EGL_EQUAL( status, EGL_TRUE, "egl_helper_windowing_deinit failed" );
	}
}

void swap_buffers( suite *test_suite )
{
	fixture *fix = (fixture *)test_suite->fixture;
	if ( NULL != fix ) eglSwapBuffers( fix->display, fix->surface );
}

