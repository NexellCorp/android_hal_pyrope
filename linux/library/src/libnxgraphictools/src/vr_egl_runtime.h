#ifndef __VR_EGL_RUNTIME__
#define __VR_EGL_RUNTIME__

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "vr_common_def.h"

#if 0 /* for debugging */
#define EGL_CHECK(x) \
	x; \
	{ \
		EGLint eglError = eglGetError(); \
		if(eglError != EGL_SUCCESS) { \
			ErrMsg("eglGetError() = %i (0x%.8x) at %s:%i\n", eglError, eglError, __FILE__, __LINE__); \
			exit(1); \
		} \
	}

#define GL_CHECK(x) \
	x; \
	{ \
		GLenum glError = glGetError(); \
		if(glError != GL_NO_ERROR) { \
			ErrMsg( "glGetError() = %i (0x%.8x) at %s:%i\n", glError, glError, __FILE__, __LINE__); \
			exit(1); \
		} \
	}
#else
#define EGL_CHECK(x) 	x
#define GL_CHECK(x)		x
#endif

typedef struct
{
    EGLDisplay sEGLDisplay;
	EGLContext sEGLContext32bit;
	EGLContext sEGLContext16bit;
	EGLContext sEGLContext8bit;
	EGLConfig sEGLConfig32Bit;
	EGLConfig sEGLConfig16Bit;
	EGLConfig sEGLConfig8Bit;
	unsigned int sEGLContext32bitRef;
	unsigned int sEGLContext16bitRef;
	unsigned int sEGLContext8bitRef;
} EGLInfo;

extern PFNEGLCREATEIMAGEKHRPROC _eglCreateImageKHR;
extern PFNEGLDESTROYIMAGEKHRPROC _eglDestroyImageKHR;
extern PFNGLEGLIMAGETARGETTEXTURE2DOESPROC _glEGLImageTargetTexture2DOES;
extern PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC _glEGLImageTargetRenderbufferStorageOES;

/**
 * Initializes extensions required for EGLImage
 *
 * @return 0 on success, non-0 on failure
 */
int vrInitEglExtensions(void);


int vrInitializeEGL(void);
int vrTerminateEGL(void);
EGLNativePixmapType vrCreatePixmap(unsigned int uiWidth, unsigned int uiHeight, void* pData, int is_video_dma_buf, 
									unsigned int pixel_bits, int is_yuv_format);
void vrDestroyPixmap(EGLNativePixmapType pPixmap);


#endif /* __VR_EGL_RUNTIME__ */

