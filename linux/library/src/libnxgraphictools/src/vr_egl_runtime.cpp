#include <EGL/egl.h>

#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>

#include "vr_common_def.h"
#include "vr_egl_runtime.h"
#include "vr_deinterlace_private.h"

#include "dbgmsg.h"
  
PFNEGLCREATEIMAGEKHRPROC _eglCreateImageKHR = NULL;
PFNEGLDESTROYIMAGEKHRPROC _eglDestroyImageKHR = NULL;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC _glEGLImageTargetTexture2DOES = NULL;
PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC _glEGLImageTargetRenderbufferStorageOES = NULL;

EGLNativePixmapType vrCreatePixmap(unsigned int uiWidth, unsigned int uiHeight, void* pData, int is_video_dma_buf, 
								unsigned int pixel_bits, int is_yuv_format)
{	
	fbdev_pixmap* pPixmap = (fbdev_pixmap *)calloc(1, sizeof(fbdev_pixmap));
	if(pPixmap == NULL)
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return NULL;
	}
	
	if(is_yuv_format && 32 == pixel_bits)
		uiWidth /= 4;
		
	pPixmap->width = uiWidth;
	pPixmap->height = uiHeight;
	if(32 == pixel_bits)
	{
		pPixmap->bytes_per_pixel = 4;
		pPixmap->buffer_size = 32;
		pPixmap->red_size = 8;
		pPixmap->green_size = 8;
		pPixmap->blue_size = 8;
		pPixmap->alpha_size = 8;
		pPixmap->luminance_size = 0;
	}
	else if(16 == pixel_bits)
	{
		pPixmap->bytes_per_pixel = 2;
		pPixmap->buffer_size = 16;
		pPixmap->red_size = 0;
		pPixmap->green_size = 0;
		pPixmap->blue_size = 0;
		pPixmap->alpha_size = 8;
		pPixmap->luminance_size = 8;
	}		
	else if(8 == pixel_bits)
	{
		pPixmap->bytes_per_pixel = 1;
		pPixmap->buffer_size = 8;
		pPixmap->red_size = 0;
		pPixmap->green_size = 0;
		pPixmap->blue_size = 0;
		pPixmap->alpha_size = 0;
		pPixmap->luminance_size = 8;
	}	
	else
	{
		ErrMsg("Error: pixel_bits(%d) is not valid. %s:%i\n", pixel_bits, __FILE__, __LINE__);
		return NULL;
	}

	int fd_handle;
	if(is_video_dma_buf)
	{
		fd_handle = (int)(((NX_MEMORY_HANDLE)pData)->privateDesc);
	}
	else
	{
		fd_handle = (int)pData;
	}
	
	pPixmap->flags = (fbdev_pixmap_flags)FBDEV_PIXMAP_COLORSPACE_sRGB;
	pPixmap->flags = (fbdev_pixmap_flags)(pPixmap->flags |FBDEV_PIXMAP_DMA_BUF);
	pPixmap->data = (unsigned short *)calloc(1, sizeof(int));
	*((int*)pPixmap->data) = (int)fd_handle;

	if(pPixmap->data == NULL)
	{
		ErrMsg("Error: NULL memory at %s:%i\n", __FILE__, __LINE__);
		return NULL;
	}
	return pPixmap;
}

void vrDestroyPixmap(EGLNativePixmapType pPixmap)
{
#ifdef VR_FEATURE_ION_ALLOC_USE	
	if(((fbdev_pixmap*)pPixmap)->data)
		free(((fbdev_pixmap*)pPixmap)->data);
#endif
	if(pPixmap)
		free(pPixmap);
}

int vrInitializeEGL(void)
{
	Statics *pStatics = vrGetStatics();
	EGLBoolean bResult = EGL_FALSE;

	static const EGLint aEGLContextAttributes[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	
	EGLint aEGLAttributes_32bit[] =
	{
		EGL_SAMPLES,			0,
		EGL_RED_SIZE,			8,
		EGL_GREEN_SIZE, 		8,
		EGL_BLUE_SIZE,			8,
		EGL_ALPHA_SIZE, 		8,
		EGL_LUMINANCE_SIZE, 	0,
		EGL_BUFFER_SIZE,		32,
		EGL_DEPTH_SIZE, 		0,
		EGL_STENCIL_SIZE,		0,
		EGL_SURFACE_TYPE,		EGL_PIXMAP_BIT,
		EGL_COLOR_BUFFER_TYPE,	EGL_RGB_BUFFER,
		EGL_NONE
	};

	EGLint aEGLAttributes_8bit[] =
	{
		EGL_SAMPLES,			0,
		EGL_RED_SIZE,			0,
		EGL_GREEN_SIZE, 		0,
		EGL_BLUE_SIZE,			0,
		EGL_ALPHA_SIZE, 		0,
		EGL_LUMINANCE_SIZE, 	8,
		EGL_BUFFER_SIZE,		8,
		EGL_DEPTH_SIZE, 		0,
		EGL_STENCIL_SIZE,		0,
		EGL_SURFACE_TYPE,		EGL_PIXMAP_BIT,
		EGL_COLOR_BUFFER_TYPE,	EGL_LUMINANCE_BUFFER,
		EGL_NONE
	};
	EGLint aEGLAttributes_16bit[] =
	{
		EGL_SAMPLES,			0,
		EGL_RED_SIZE,			0,
		EGL_GREEN_SIZE, 		0,
		EGL_BLUE_SIZE,			0,
		EGL_ALPHA_SIZE, 		8,
		EGL_LUMINANCE_SIZE, 	8,
		EGL_BUFFER_SIZE,		16,
		EGL_DEPTH_SIZE, 		0,
		EGL_STENCIL_SIZE,		0,
		EGL_SURFACE_TYPE,		EGL_PIXMAP_BIT,
		EGL_COLOR_BUFFER_TYPE,	EGL_LUMINANCE_BUFFER,
		EGL_NONE
	};
	pStatics->egl_info.sEGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if(pStatics->egl_info.sEGLDisplay == EGL_NO_DISPLAY)
	{
		ErrMsg("Error: No EGL Display available at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}

	/* Initialize EGL. */
    bResult = eglInitialize(pStatics->egl_info.sEGLDisplay, NULL, NULL);
	if(bResult != EGL_TRUE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to initialize EGL at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}

	/* Choose 8bit config. */
	{
		EGLConfig *pEGLConfig = NULL;
		int iEGLConfig = 0;
		EGLint cEGLConfigs = 0;

		/* Enumerate available EGL configurations which match or exceed our required attribute list. */
		bResult = eglChooseConfig(pStatics->egl_info.sEGLDisplay, aEGLAttributes_8bit, NULL, 0, &cEGLConfigs);
		if(bResult != EGL_TRUE)
		{
			EGLint iError = eglGetError();
			ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			ErrMsg("Error: Failed to enumerate EGL configs at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		
		/* Allocate space for all EGL configs available and get them. */
		pEGLConfig = (EGLConfig *)calloc(cEGLConfigs, sizeof(EGLConfig));
		if(pEGLConfig == NULL)
		{
			ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		bResult = eglChooseConfig(pStatics->egl_info.sEGLDisplay, aEGLAttributes_8bit, pEGLConfig, cEGLConfigs, &cEGLConfigs);
		if(bResult != EGL_TRUE)
		{
			EGLint iError = eglGetError();
			ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			ErrMsg("Error: Failed to enumerate EGL configs at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		if(cEGLConfigs == 0)
		{
			ErrMsg("thers is no config at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}

		//ErrMsg("8bit config search start(total:%d)\n", cEGLConfigs);
		
		/* Loop through the EGL configs to find a color depth match.
		 * NB This is necessary, since EGL considers a higher color depth than requested to be 'better'
		 * even though this may force the driver to use a slow color conversion blitting routine. */
		for(iEGLConfig = 0; iEGLConfig < cEGLConfigs; iEGLConfig ++)
		{
			EGLint iEGLValue = 0;
			bResult = eglGetConfigAttrib(pStatics->egl_info.sEGLDisplay, pEGLConfig[iEGLConfig], EGL_BUFFER_SIZE, &iEGLValue);
			if(bResult != EGL_TRUE)
			{
				EGLint iError = eglGetError();
				ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
				ErrMsg("Error: Failed to get EGL attribute at %s:%i\n", __FILE__, __LINE__);			
				return -1;			
			}
			if(iEGLValue == 8) break;
		}

		//ErrMsg("8bit config search end(n:%d)\n", iEGLConfig);
		
		if(iEGLConfig >= cEGLConfigs)
		{
			ErrMsg("Error: Failed to find matching EGL config at %s:%i\n", __FILE__, __LINE__);
			return -1;	
		}
		pStatics->egl_info.sEGLConfig8Bit = pEGLConfig[iEGLConfig];

		/* Create context. */
		pStatics->egl_info.sEGLContext8bit = eglCreateContext(pStatics->egl_info.sEGLDisplay, pEGLConfig[iEGLConfig], EGL_NO_CONTEXT, aEGLContextAttributes);
		if(pStatics->egl_info.sEGLContext8bit == EGL_NO_CONTEXT)
		{
			EGLint iError = eglGetError();
			ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			ErrMsg("Error: Failed to create EGL context at %s:%i\n", __FILE__, __LINE__);
			return -1;	
		}

		free(pEGLConfig);
		pEGLConfig = NULL;
	}

	/* Choose 16bit config. */
	{
		EGLConfig *pEGLConfig = NULL;
		int iEGLConfig = 0;
		EGLint cEGLConfigs = 0;

		/* Enumerate available EGL configurations which match or exceed our required attribute list. */
		bResult = eglChooseConfig(pStatics->egl_info.sEGLDisplay, aEGLAttributes_16bit, NULL, 0, &cEGLConfigs);
		if(bResult != EGL_TRUE)
		{
			EGLint iError = eglGetError();
			ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			ErrMsg("Error: Failed to enumerate EGL configs at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		
		/* Allocate space for all EGL configs available and get them. */
		pEGLConfig = (EGLConfig *)calloc(cEGLConfigs, sizeof(EGLConfig));
		if(pEGLConfig == NULL)
		{
			ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		bResult = eglChooseConfig(pStatics->egl_info.sEGLDisplay, aEGLAttributes_16bit, pEGLConfig, cEGLConfigs, &cEGLConfigs);
		if(bResult != EGL_TRUE)
		{
			EGLint iError = eglGetError();
			ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			ErrMsg("Error: Failed to enumerate EGL configs at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		if(cEGLConfigs == 0)
		{
			ErrMsg("thers is no config at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		
		//ErrMsg("16bit config search start(total:%d)\n", cEGLConfigs);
		
		/* Loop through the EGL configs to find a color depth match.
		 * NB This is necessary, since EGL considers a higher color depth than requested to be 'better'
		 * even though this may force the driver to use a slow color conversion blitting routine. */
		for(iEGLConfig = 0; iEGLConfig < cEGLConfigs; iEGLConfig ++)
		{
			EGLint iEGLValue = 0;
			bResult = eglGetConfigAttrib(pStatics->egl_info.sEGLDisplay, pEGLConfig[iEGLConfig], EGL_BUFFER_SIZE, &iEGLValue);
			if(bResult != EGL_TRUE)
			{
				EGLint iError = eglGetError();
				ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
				ErrMsg("Error: Failed to get EGL attribute at %s:%i\n", __FILE__, __LINE__);			
				return -1;			
			}
			if(iEGLValue == 16)
			{
				//eglGetConfigAttrib(pStatics->egl_info.sEGLDisplay, pEGLConfig[iEGLConfig], EGL_CONFIG_ID, &iEGLValue);
				//ErrMsg("16bit config id(n:%d)\n", iEGLValue);
				break;
			}
		}
		//ErrMsg("16bit config search end(n:%d)\n", iEGLConfig);

		if(iEGLConfig >= cEGLConfigs)
		{
			ErrMsg("Error: Failed to find matching EGL config at %s:%i\n", __FILE__, __LINE__);
			return -1;	
		}
		pStatics->egl_info.sEGLConfig16Bit = pEGLConfig[iEGLConfig];

		/* Create context. */
		pStatics->egl_info.sEGLContext16bit = eglCreateContext(pStatics->egl_info.sEGLDisplay, pEGLConfig[iEGLConfig], EGL_NO_CONTEXT, aEGLContextAttributes);
		if(pStatics->egl_info.sEGLContext16bit == EGL_NO_CONTEXT)
		{
			EGLint iError = eglGetError();
			ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			ErrMsg("Error: Failed to create EGL context at %s:%i\n", __FILE__, __LINE__);
			return -1;	
		}

		free(pEGLConfig);
		pEGLConfig = NULL;
	}

	/* Choose 32bit config. */
	{
		EGLConfig *pEGLConfig = NULL;
		int iEGLConfig = 0;
		EGLint cEGLConfigs = 0;

		/* Enumerate available EGL configurations which match or exceed our required attribute list. */
		bResult = eglChooseConfig(pStatics->egl_info.sEGLDisplay, aEGLAttributes_32bit, NULL, 0, &cEGLConfigs);
		if(bResult != EGL_TRUE)
		{
			EGLint iError = eglGetError();
			ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			ErrMsg("Error: Failed to enumerate EGL configs at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		
		/* Allocate space for all EGL configs available and get them. */
		pEGLConfig = (EGLConfig *)calloc(cEGLConfigs, sizeof(EGLConfig));
		if(pEGLConfig == NULL)
		{
			ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		bResult = eglChooseConfig(pStatics->egl_info.sEGLDisplay, aEGLAttributes_32bit, pEGLConfig, cEGLConfigs, &cEGLConfigs);
		if(bResult != EGL_TRUE)
		{
			EGLint iError = eglGetError();
			ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			ErrMsg("Error: Failed to enumerate EGL configs at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		if(cEGLConfigs == 0)
		{
			ErrMsg("thers is no config at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}

		//ErrMsg("32bit config search start(total:%d)\n", cEGLConfigs);		
		/* Loop through the EGL configs to find a color depth match.
		 * NB This is necessary, since EGL considers a higher color depth than requested to be 'better'
		 * even though this may force the driver to use a slow color conversion blitting routine. */
		for(iEGLConfig = 0; iEGLConfig < cEGLConfigs; iEGLConfig ++)
		{
			EGLint iEGLValue = 0;
			bResult = eglGetConfigAttrib(pStatics->egl_info.sEGLDisplay, pEGLConfig[iEGLConfig], EGL_BUFFER_SIZE, &iEGLValue);
			if(bResult != EGL_TRUE)
			{
				EGLint iError = eglGetError();
				ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
				ErrMsg("Error: Failed to get EGL attribute at %s:%i\n", __FILE__, __LINE__);			
				return -1;			
			}
			if(iEGLValue == 32) break;
		}
		//ErrMsg("32bit config search end(n:%d)\n", iEGLConfig);

		if(iEGLConfig >= cEGLConfigs)
		{
			ErrMsg("Error: Failed to find matching EGL config at %s:%i\n", __FILE__, __LINE__);
			return -1;	
		}
		pStatics->egl_info.sEGLConfig32Bit = pEGLConfig[iEGLConfig];
		
		/* Create context. */
		pStatics->egl_info.sEGLContext32bit = eglCreateContext(pStatics->egl_info.sEGLDisplay, pEGLConfig[iEGLConfig], EGL_NO_CONTEXT, aEGLContextAttributes);
		if(pStatics->egl_info.sEGLContext32bit == EGL_NO_CONTEXT)
		{
			EGLint iError = eglGetError();
			ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			ErrMsg("Error: Failed to create EGL context at %s:%i\n", __FILE__, __LINE__);
			return -1;	
		}

		free(pEGLConfig);
		pEGLConfig = NULL;		
	}	

	//ErrMsg("[VR] EGL Init Done, disp(0x%x), 32bit_ctx(0x%x), 16bit_ctx(0x%x), 8bit_ctx(0x%x)\n", (int)pStatics->egl_info.sEGLDisplay, 
	// 							(int)pStatics->egl_info.sEGLContext32bit, (int)pStatics->egl_info.sEGLContext16bit, (int)pStatics->egl_info.sEGLContext8bit);
	return 0;
}

int vrTerminateEGL(void)
{
	Statics *pStatics = vrGetStatics();

    EGL_CHECK(eglTerminate(pStatics->egl_info.sEGLDisplay));

	/* Clear up globals. */
	//vrTerminateStatics();

	return 0;
}

int vrInitEglExtensions(void)
{
	_eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
	if ( NULL == _eglCreateImageKHR ) return -1;

	_eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress("eglDestroyImageKHR");
	if ( NULL == _eglDestroyImageKHR ) return -1;

	_glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress("glEGLImageTargetTexture2DOES");
	if ( NULL == _glEGLImageTargetTexture2DOES ) return -1;

	_glEGLImageTargetRenderbufferStorageOES = (PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC) eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES");
	if ( NULL == _glEGLImageTargetRenderbufferStorageOES ) return -1;

	return 0;
}

