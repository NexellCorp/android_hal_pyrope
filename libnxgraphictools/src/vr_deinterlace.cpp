#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "vr_common_def.h"
#include "vr_egl_runtime.h"
#include "vr_deinterlace_private.h"

#include "dbgmsg.h"

static bool    s_Initialized = false;
static Statics s_Statics;
Statics* vrGetStatics( void )
{
	return &s_Statics;
}

namespace 
{
	class AutoBackupCurrentEGL 
	{
	public:
		EGLContext eglCurrentContext;
		EGLSurface eglCurrentSurface[2];
		EGLDisplay eglCurrentDisplay;
		AutoBackupCurrentEGL(void)
		{
			eglCurrentContext    = eglGetCurrentContext();
			eglCurrentSurface[0] = eglGetCurrentSurface(EGL_DRAW);
			eglCurrentSurface[1] = eglGetCurrentSurface(EGL_READ);
			eglCurrentDisplay    = eglGetCurrentDisplay();	
		}
		~AutoBackupCurrentEGL()
		{
			//????
			//eglMakeCurrent(eglCurrentDisplay, eglCurrentSurface[0], eglCurrentSurface[1], eglCurrentContext);
		}
	};
	#define _AUTO_BACKUP_CURRENT_EGL_ AutoBackupCurrentEGL instanceAutoBackupCurrentEGL
};

static int vrInitializeGL( HDEINTERLACETARGET* pTarget );
static char *loadShader( const char *sFilename);
static int processShader(GLuint *pShader, const char *sFilename, GLint iShaderType);

int vrInitializeGLSurface( void )
{
	if( s_Initialized ){ ErrMsg("Error: vrInitializeGLSurface at %s:%i\n", __FILE__, __LINE__); return 0; }
	int ret = vrInitEglExtensions();
	if( ret ){ ErrMsg("Error: vrInitializeGLSurface at %s:%i\n", __FILE__, __LINE__); return ret; }
	ret = vrInitializeEGL();
	if( ret ){ ErrMsg("Error: vrInitializeGLSurface at %s:%i\n", __FILE__, __LINE__); return ret; }

	Statics* pStatics = &s_Statics;
	for(unsigned int program = 0 ; program < VR_PROGRAM_MAX ; program++)
	{
		pStatics->shader[program].iVertName = 0;
		pStatics->shader[program].iFragName = 0;
		pStatics->shader[program].iProgName = 0;
		pStatics->shader[program].iLocPosition = -1;
		pStatics->shader[program].iLocTexCoord = -1;
		pStatics->shader[program].iLocInputHeight = -1;
		pStatics->shader[program].iLocOutputHeight = -1;
		pStatics->shader[program].iLocMainTex[0] = -1;
		pStatics->shader[program].iLocRefTex = -1;
	}	
	pStatics->default_target_memory[VR_PROGRAM_DEINTERLACE] = NX_AllocateMemory(64*64, 4);		
	pStatics->default_target[VR_PROGRAM_DEINTERLACE] = vrCreateDeinterlaceTarget(64, 64, pStatics->default_target_memory[VR_PROGRAM_DEINTERLACE]);
	pStatics->default_target_memory[VR_PROGRAM_SCALE] = NX_AllocateMemory(64*64, 4);		
	pStatics->default_target[VR_PROGRAM_SCALE] = vrCreateScaleTarget(64, 64, pStatics->default_target_memory[VR_PROGRAM_SCALE]);
	pStatics->default_target_memory[VR_PROGRAM_CVT2Y] = NX_AllocateMemory(64*64*4, 4);		
	pStatics->default_target[VR_PROGRAM_CVT2Y] = vrCreateCvt2YTarget(64, 64, pStatics->default_target_memory[VR_PROGRAM_CVT2Y]);
	pStatics->default_target_memory[VR_PROGRAM_CVT2UV] = NX_AllocateMemory(64*64*4, 4);		
	pStatics->default_target[VR_PROGRAM_CVT2UV] = vrCreateCvt2UVTarget(64, 64, pStatics->default_target_memory[VR_PROGRAM_CVT2UV]);
	pStatics->default_target_memory[VR_PROGRAM_CVT2RGBA] = NX_AllocateMemory(64*64*4, 4);		
	pStatics->default_target[VR_PROGRAM_CVT2RGBA] = vrCreateCvt2RgbaTarget(64, 64, pStatics->default_target_memory[VR_PROGRAM_CVT2RGBA]);
	
 	pStatics->tex_deinterlace_ref_id = 0;
 	pStatics->tex_cvt_id = 0;

	ret=vrInitializeGL( pStatics->default_target );
	if( ret ){ ErrMsg("Error: vrInitializeGLSurface at %s:%i\n", __FILE__, __LINE__); return ret; }

	s_Initialized = true;

	//ErrMsg("vrInitializeGLSurface Done\n");	
	return 0;
}

void  vrTerminateGLSurface( void )
{
	if( ! s_Initialized ){ return; }
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return; }
	GL_CHECK(glDeleteTextures(1,&pStatics->tex_deinterlace_ref_id));
	GL_CHECK(glDeleteTextures(1,&pStatics->tex_cvt_id));
	for(unsigned int program = 0 ; program < VR_PROGRAM_MAX ; program++)
	{
		glDeleteShader(pStatics->shader[program].iVertName  );
		glDeleteShader(pStatics->shader[program].iFragName  );
		glDeleteProgram(pStatics->shader[program].iProgName );
	}
	vrDestroyDeinterlaceTarget( pStatics->default_target[VR_PROGRAM_DEINTERLACE] );
	NX_FreeMemory( pStatics->default_target_memory[VR_PROGRAM_DEINTERLACE] );
	vrDestroyScaleTarget( pStatics->default_target[VR_PROGRAM_SCALE] );
	NX_FreeMemory( pStatics->default_target_memory[VR_PROGRAM_SCALE] );
	vrDestroyCvt2YTarget( pStatics->default_target[VR_PROGRAM_CVT2Y] );
	NX_FreeMemory( pStatics->default_target_memory[VR_PROGRAM_CVT2Y] );
	vrDestroyCvt2UVTarget( pStatics->default_target[VR_PROGRAM_CVT2UV] );
	NX_FreeMemory( pStatics->default_target_memory[VR_PROGRAM_CVT2UV] );
	vrDestroyCvt2RgbaTarget( pStatics->default_target[VR_PROGRAM_CVT2RGBA] );
	NX_FreeMemory( pStatics->default_target_memory[VR_PROGRAM_CVT2RGBA] );
	vrTerminateEGL();
	s_Initialized = false;
	//ErrMsg("vrTerminateGLSurface Done\n");		
}

struct vrDoDeinterlaceTarget
{
	unsigned int        width         ;
	unsigned int        height        ;
	EGLNativePixmapType native_pixmap ;
	EGLSurface          pixmap_surface;
} ;

HDEINTERLACETARGET vrCreateDeinterlaceTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data)
{
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HDEINTERLACETARGET)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HDEINTERLACETARGET result = (HDEINTERLACETARGET)malloc(sizeof(vrDoDeinterlaceTarget));
	if( !result )
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HDEINTERLACETARGET)0;
	}
	memset( result, 0, sizeof(struct vrDoDeinterlaceTarget) );

	/* Create a EGLNativePixmapType. */
	EGLNativePixmapType pixmap_output = (fbdev_pixmap*)vrCreatePixmap(uiWidth, uiHeight, Data, VR_TRUE, 32, VR_TRUE);
	if(pixmap_output == NULL || ((fbdev_pixmap*)pixmap_output)->data == NULL)
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		free( result );
		return (HDEINTERLACETARGET)0;
	}
	
	/* Create a EGLSurface. */
	EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig32Bit, (EGLNativePixmapType)pixmap_output, NULL);
	if(surface == EGL_NO_SURFACE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
		vrDestroyPixmap(pixmap_output);
		free( result );
		return (HDEINTERLACETARGET)0;
	}

	result->width          = uiWidth ;
	result->height         = uiHeight;
	result->native_pixmap  = pixmap_output;
	result->pixmap_surface = surface;	

	/* Increase Ctx ref. */
	++pStatics->egl_info.sEGLContext32bitRef;
	// ErrMsg("Deinterlace eglCreatePixmapSurface done, ref(0x%x)\n", pStatics->egl_info.sEGLContext32bitRef);
	
	return result;
}

void vrDestroyDeinterlaceTarget ( HDEINTERLACETARGET target )
{
	if( !target ){ return; }
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return ; }
	_AUTO_BACKUP_CURRENT_EGL_;
	
	
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, //EGL_NO_SURFACE, EGL_NO_SURFACE,
				pStatics->default_target[VR_PROGRAM_DEINTERLACE]->pixmap_surface, pStatics->default_target[VR_PROGRAM_DEINTERLACE]->pixmap_surface, 
				pStatics->egl_info.sEGLContext32bit);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return;	
	}

	if( target->pixmap_surface ) { EGL_CHECK(eglDestroySurface(pStatics->egl_info.sEGLDisplay,target->pixmap_surface)); }
	if( target->native_pixmap  ) { vrDestroyPixmap(target->native_pixmap); }

	/* Decrease Ctx ref. */
	--pStatics->egl_info.sEGLContext32bitRef;
	if(!pStatics->egl_info.sEGLContext32bitRef)	
	{
		EGL_CHECK(eglDestroyContext(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLContext32bit));		
		//ErrMsg("Deinterlace eglDestroyContext done(0x%x, 0x%x)\n", (int)pStatics->egl_info.sEGLDisplay, (int)pStatics->egl_info.sEGLContext32bit);
	}
	// else
	// 	ErrMsg("Deinterlace eglDestroySurface done, ref(0x%x)\n", pStatics->egl_info.sEGLContext32bitRef);
	
}

HDEINTERLACETARGET vrCreateScaleTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data)
{
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HDEINTERLACETARGET)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HDEINTERLACETARGET result = (HDEINTERLACETARGET)malloc(sizeof(vrDoDeinterlaceTarget));
	if( !result )
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HDEINTERLACETARGET)0;
	}
	memset( result, 0, sizeof(struct vrDoDeinterlaceTarget) );

	/* Create a EGLNativePixmapType. */
	EGLNativePixmapType pixmap_output = (fbdev_pixmap*)vrCreatePixmap(uiWidth, uiHeight, Data, VR_TRUE, 8, VR_TRUE);
	if(pixmap_output == NULL || ((fbdev_pixmap*)pixmap_output)->data == NULL)
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		free( result );
		return (HDEINTERLACETARGET)0;
	}
	
	/* Create a EGLSurface. */
	EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig8Bit, (EGLNativePixmapType)pixmap_output, NULL);
	if(surface == EGL_NO_SURFACE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
		vrDestroyPixmap(pixmap_output);
		free( result );
		return (HDEINTERLACETARGET)0;
	}

	result->width          = uiWidth ;
	result->height         = uiHeight;
	result->native_pixmap  = pixmap_output;
	result->pixmap_surface = surface;	

	/* Increase Ctx ref. */
	++pStatics->egl_info.sEGLContext8bitRef;
	// ErrMsg("Scale eglCreatePixmapSurface done, ref(0x%x)\n", pStatics->egl_info.sEGLContext8bitRef);
	
	return result;
}

void vrDestroyScaleTarget ( HDEINTERLACETARGET target )
{
	if( !target ){ return; }
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return ; }
	_AUTO_BACKUP_CURRENT_EGL_;

	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, //EGL_NO_SURFACE, EGL_NO_SURFACE,
				pStatics->default_target[VR_PROGRAM_SCALE]->pixmap_surface, pStatics->default_target[VR_PROGRAM_SCALE]->pixmap_surface, 
				pStatics->egl_info.sEGLContext8bit);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return;	
	}
			
	if( target->pixmap_surface ) { EGL_CHECK(eglDestroySurface(pStatics->egl_info.sEGLDisplay,target->pixmap_surface)); }
	if( target->native_pixmap  ) { vrDestroyPixmap(target->native_pixmap); }

	/* Decrease Ctx ref. */
	--pStatics->egl_info.sEGLContext8bitRef;
	if(!pStatics->egl_info.sEGLContext8bitRef)
	{
		EGL_CHECK(eglDestroyContext(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLContext8bit));	
		//ErrMsg("Scale eglDestroyContext done(0x%x, 0x%x)\n", (int)pStatics->egl_info.sEGLDisplay, (int)pStatics->egl_info.sEGLContext8bit);
	}
	// else
	// 	ErrMsg("Scale eglDestroySurface done, ref(0x%x)\n", pStatics->egl_info.sEGLContext8bitRef);
}

HDEINTERLACETARGET vrCreateCvt2YTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data)
{
	//ErrMsg("vrCreateCvt2YTarget start(%dx%d)\n", uiWidth, uiHeight);

	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HDEINTERLACETARGET)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HDEINTERLACETARGET result = (HDEINTERLACETARGET)malloc(sizeof(vrDoDeinterlaceTarget));
	if( !result )
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HDEINTERLACETARGET)0;
	}
	memset( result, 0, sizeof(struct vrDoDeinterlaceTarget) );

	/* Create a EGLNativePixmapType. */
	EGLNativePixmapType pixmap_output = (fbdev_pixmap*)vrCreatePixmap(uiWidth, uiHeight, Data, VR_TRUE, 8, VR_TRUE);
	if(pixmap_output == NULL || ((fbdev_pixmap*)pixmap_output)->data == NULL)
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		free( result );
		return (HDEINTERLACETARGET)0;
	}
	
	/* Create a EGLSurface. */
	EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig8Bit, (EGLNativePixmapType)pixmap_output, NULL);
	if(surface == EGL_NO_SURFACE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
		vrDestroyPixmap(pixmap_output);
		free( result );
		return (HDEINTERLACETARGET)0;
	}

	result->width          = uiWidth ;
	result->height         = uiHeight;
	result->native_pixmap  = pixmap_output;
	result->pixmap_surface = surface;	

	/* Increase Ctx ref. */
	++pStatics->egl_info.sEGLContext8bitRef;
	//ErrMsg("vrCreateCvt2YTarget done, ref(0x%x)\n", pStatics->egl_info.sEGLContext8bitRef);
	
	return result;
}

void vrDestroyCvt2YTarget ( HDEINTERLACETARGET target )
{
	if( !target ){ return; }
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return ; }
	_AUTO_BACKUP_CURRENT_EGL_;
	
	
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, //EGL_NO_SURFACE, EGL_NO_SURFACE,
				pStatics->default_target[VR_PROGRAM_CVT2Y]->pixmap_surface, pStatics->default_target[VR_PROGRAM_CVT2Y]->pixmap_surface, 
				pStatics->egl_info.sEGLContext8bit);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return;	
	}

	if( target->pixmap_surface ) { EGL_CHECK(eglDestroySurface(pStatics->egl_info.sEGLDisplay,target->pixmap_surface)); }
	if( target->native_pixmap  ) { vrDestroyPixmap(target->native_pixmap); }

	/* Decrease Ctx ref. */
	--pStatics->egl_info.sEGLContext8bitRef;
	if(!pStatics->egl_info.sEGLContext8bitRef)	
	{
		EGL_CHECK(eglDestroyContext(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLContext8bit));		
		//ErrMsg("Cvt2Y eglDestroyContext done(0x%x, 0x%x)\n", (int)pStatics->egl_info.sEGLDisplay, (int)pStatics->egl_info.sEGLContext8bit);
	}
	//else
	// 	ErrMsg("Cvt2Y eglDestroySurface done, ref(0x%x)\n", pStatics->egl_info.sEGLContext8bitRef);
	
}

HDEINTERLACETARGET vrCreateCvt2UVTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data)
{
	//ErrMsg("vrCreateCvt2UVTarget start(%dx%d)\n", uiWidth, uiHeight);

	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HDEINTERLACETARGET)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HDEINTERLACETARGET result = (HDEINTERLACETARGET)malloc(sizeof(vrDoDeinterlaceTarget));
	if( !result )
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HDEINTERLACETARGET)0;
	}
	memset( result, 0, sizeof(struct vrDoDeinterlaceTarget) );

	//4pixel마다 UV존재
	uiWidth /= 2;
	uiHeight /= 2;
	
	/* Create a EGLNativePixmapType. */
	EGLNativePixmapType pixmap_output = (fbdev_pixmap*)vrCreatePixmap(uiWidth, uiHeight, Data, VR_TRUE, 16, VR_TRUE);
	if(pixmap_output == NULL || ((fbdev_pixmap*)pixmap_output)->data == NULL)
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		free( result );
		return (HDEINTERLACETARGET)0;
	}
	
	/* Create a EGLSurface. */
	EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig16Bit, (EGLNativePixmapType)pixmap_output, NULL);
	if(surface == EGL_NO_SURFACE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
		vrDestroyPixmap(pixmap_output);
		free( result );
		return (HDEINTERLACETARGET)0;
	}

	result->width          = uiWidth ;
	result->height         = uiHeight;
	result->native_pixmap  = pixmap_output;
	result->pixmap_surface = surface;	

	/* Increase Ctx ref. */
	++pStatics->egl_info.sEGLContext16bitRef;
	//ErrMsg("vrCreateCvt2UVTarget done, ref(0x%x)\n", pStatics->egl_info.sEGLContext16bitRef);
	
	return result;
}

void vrDestroyCvt2UVTarget ( HDEINTERLACETARGET target )
{
	if( !target ){ return; }
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return ; }
	_AUTO_BACKUP_CURRENT_EGL_;
	
	
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, //EGL_NO_SURFACE, EGL_NO_SURFACE,
				pStatics->default_target[VR_PROGRAM_CVT2UV]->pixmap_surface, pStatics->default_target[VR_PROGRAM_CVT2UV]->pixmap_surface, 
				pStatics->egl_info.sEGLContext16bit);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return;	
	}

	if( target->pixmap_surface ) { EGL_CHECK(eglDestroySurface(pStatics->egl_info.sEGLDisplay,target->pixmap_surface)); }
	if( target->native_pixmap  ) { vrDestroyPixmap(target->native_pixmap); }

	/* Decrease Ctx ref. */
	--pStatics->egl_info.sEGLContext16bitRef;
	if(!pStatics->egl_info.sEGLContext16bitRef)	
	{
		EGL_CHECK(eglDestroyContext(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLContext16bit));		
		//ErrMsg("Cvt2UV eglDestroyContext done(0x%x, 0x%x)\n", (int)pStatics->egl_info.sEGLDisplay, (int)pStatics->egl_info.sEGLContext16bit);
	}
	//else
	// 	ErrMsg("Cvt2UV eglDestroySurface done, ref(0x%x)\n", pStatics->egl_info.sEGLContext16bitRef);
	
}

HDEINTERLACETARGET vrCreateCvt2RgbaTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data)
{
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HDEINTERLACETARGET)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HDEINTERLACETARGET result = (HDEINTERLACETARGET)malloc(sizeof(vrDoDeinterlaceTarget));
	if( !result )
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HDEINTERLACETARGET)0;
	}
	memset( result, 0, sizeof(struct vrDoDeinterlaceTarget) );

	/* Create a EGLNativePixmapType. */
	EGLNativePixmapType pixmap_output = (fbdev_pixmap*)vrCreatePixmap(uiWidth, uiHeight, Data, VR_TRUE, 32, VR_FALSE);
	if(pixmap_output == NULL || ((fbdev_pixmap*)pixmap_output)->data == NULL)
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		free( result );
		return (HDEINTERLACETARGET)0;
	}
	
	/* Create a EGLSurface. */
	EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig32Bit, (EGLNativePixmapType)pixmap_output, NULL);
	if(surface == EGL_NO_SURFACE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
		vrDestroyPixmap(pixmap_output);
		free( result );
		return (HDEINTERLACETARGET)0;
	}

	result->width          = uiWidth ;
	result->height         = uiHeight;
	result->native_pixmap  = pixmap_output;
	result->pixmap_surface = surface;	

	/* Increase Ctx ref. */
	++pStatics->egl_info.sEGLContext32bitRef;
	// ErrMsg("Deinterlace eglCreatePixmapSurface done, ref(0x%x)\n", pStatics->egl_info.sEGLContext32bitRef);
	
	return result;
}

void vrDestroyCvt2RgbaTarget ( HDEINTERLACETARGET target )
{
	if( !target ){ return; }
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return ; }
	_AUTO_BACKUP_CURRENT_EGL_;
	
	
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, //EGL_NO_SURFACE, EGL_NO_SURFACE,
				pStatics->default_target[VR_PROGRAM_CVT2RGBA]->pixmap_surface, pStatics->default_target[VR_PROGRAM_CVT2RGBA]->pixmap_surface, 
				pStatics->egl_info.sEGLContext32bit);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return;	
	}

	if( target->pixmap_surface ) { EGL_CHECK(eglDestroySurface(pStatics->egl_info.sEGLDisplay,target->pixmap_surface)); }
	if( target->native_pixmap  ) { vrDestroyPixmap(target->native_pixmap); }

	/* Decrease Ctx ref. */
	--pStatics->egl_info.sEGLContext32bitRef;
	if(!pStatics->egl_info.sEGLContext32bitRef)	
	{
		EGL_CHECK(eglDestroyContext(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLContext32bit));		
		//ErrMsg("Deinterlace eglDestroyContext done(0x%x, 0x%x)\n", (int)pStatics->egl_info.sEGLDisplay, (int)pStatics->egl_info.sEGLContext32bit);
	}
	// else
	// 	ErrMsg("Deinterlace eglDestroySurface done, ref(0x%x)\n", pStatics->egl_info.sEGLContext32bitRef);
	
}


struct vrDoDeinterlaceSource
{
	unsigned int        width        ;
	unsigned int        height       ;
	GLuint              texture_name[VR_INPUT_MODE_YUV_MAX] ;
	EGLNativePixmapType native_pixmap[VR_INPUT_MODE_YUV_MAX];
	EGLImageKHR         pixmap_image[VR_INPUT_MODE_YUV_MAX];
};

HDEINTERLACESOURCE vrCreateDeinterlaceSource  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data)
{
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HDEINTERLACESOURCE)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HDEINTERLACESOURCE result = (HDEINTERLACESOURCE)malloc(sizeof(struct vrDoDeinterlaceSource));
	if( !result )
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HDEINTERLACESOURCE)0;
	}
	memset( result, 0, sizeof(struct vrDoDeinterlaceSource) );
	
	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_DEINTERLACE]->pixmap_surface, 
								pStatics->default_target[VR_PROGRAM_DEINTERLACE]->pixmap_surface, pStatics->egl_info.sEGLContext32bit);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return 0;	
	}

	EGLNativePixmapType pixmapInput = (fbdev_pixmap*)vrCreatePixmap(uiWidth, uiHeight, Data, VR_TRUE, 32, VR_TRUE);
	if(pixmapInput == NULL || ((fbdev_pixmap*)pixmapInput)->data == NULL)
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
		free(result);
		return (HDEINTERLACESOURCE)0;
	}

	//RGB is not supported	
	EGLint imageAttributes[] = {
		EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, 
		EGL_NONE
	};	
	EGLImageKHR eglImage = EGL_CHECK(_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
							           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput, imageAttributes));	

	GLuint textureName;
	GL_CHECK(glGenTextures(1, &textureName));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_DEINTERLACE));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GL_CHECK(_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage));		

	result->width        = uiWidth ;
	result->height       = uiHeight;
	result->texture_name[0] = textureName;
	result->native_pixmap[0]= pixmapInput;
	result->pixmap_image[0] = eglImage   ;				 
	return result;
}

void vrDestroyDeinterlaceSource ( HDEINTERLACESOURCE source )
{
	if( !source ){ return; }
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return ; }
	_AUTO_BACKUP_CURRENT_EGL_;
	GL_CHECK(glDeleteTextures(1,&source->texture_name[0]));
	EGL_CHECK(_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, source->pixmap_image[0]));
	vrDestroyPixmap(source->native_pixmap[0]);
}

HDEINTERLACESOURCE vrCreateScaleSource  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data)
{
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HDEINTERLACESOURCE)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HDEINTERLACESOURCE result = (HDEINTERLACESOURCE)malloc(sizeof(struct vrDoDeinterlaceSource));
	if( !result )
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HDEINTERLACESOURCE)0;
	}
	memset( result, 0, sizeof(struct vrDoDeinterlaceSource) );

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_SCALE]->pixmap_surface, 
									pStatics->default_target[VR_PROGRAM_SCALE]->pixmap_surface, pStatics->egl_info.sEGLContext8bit);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return 0;	
	}

	EGLNativePixmapType pixmapInput = (fbdev_pixmap*)vrCreatePixmap(uiWidth, uiHeight, Data, VR_TRUE, 8, VR_TRUE);
	if(pixmapInput == NULL || ((fbdev_pixmap*)pixmapInput)->data == NULL)
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
		free(result);
		return (HDEINTERLACESOURCE)0;
	}

	//RGB is not supported	
	EGLint imageAttributes[] = {
		EGL_IMAGE_PRESERVED_KHR, /*EGL_TRUE*/EGL_FALSE, 
		EGL_NONE
	};	
	EGLImageKHR eglImage = EGL_CHECK(_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
							           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput, imageAttributes));	

	GLuint textureName;
	GL_CHECK(glGenTextures(1, &textureName));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_NEAREST));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GL_CHECK(_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage));		

	result->width        = uiWidth ;
	result->height       = uiHeight;
	result->texture_name[0] = textureName;
	result->native_pixmap[0]= pixmapInput;
	result->pixmap_image[0] = eglImage   ;				 
	return result;
}

void vrDestroyScaleSource ( HDEINTERLACESOURCE source )
{
	vrDestroyDeinterlaceSource(source);
}

HDEINTERLACESOURCE vrCreateCvt2YSource  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data)
{
	//ErrMsg("vrCreateCvt2YSource start(%dx%d)\n", uiWidth, uiHeight);

	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HDEINTERLACESOURCE)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HDEINTERLACESOURCE result = (HDEINTERLACESOURCE)malloc(sizeof(struct vrDoDeinterlaceSource));
	if( !result )
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HDEINTERLACESOURCE)0;
	}
	memset( result, 0, sizeof(struct vrDoDeinterlaceSource) );
	
	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_CVT2Y]->pixmap_surface, 
								pStatics->default_target[VR_PROGRAM_CVT2Y]->pixmap_surface, pStatics->egl_info.sEGLContext8bit);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return 0;	
	}

	EGLNativePixmapType pixmapInput = (fbdev_pixmap*)vrCreatePixmap(uiWidth, uiHeight, Data, VR_FALSE, 32, VR_FALSE);
	if(pixmapInput == NULL || ((fbdev_pixmap*)pixmapInput)->data == NULL)
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
		free(result);
		return (HDEINTERLACESOURCE)0;
	}

	//RGB is not supported	
	EGLint imageAttributes[] = {
		EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, 
		EGL_NONE
	};	
	EGLImageKHR eglImage = EGL_CHECK(_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
							           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput, imageAttributes));	

	GLuint textureName;
	GL_CHECK(glGenTextures(1, &textureName));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_NEAREST));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GL_CHECK(_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage));		

	result->width        = uiWidth ;
	result->height       = uiHeight;
	result->texture_name[0] = textureName;
	result->native_pixmap[0]= pixmapInput;
	result->pixmap_image[0] = eglImage   ;	

	//ErrMsg("vrCreateCvt2YSource done\n");
	
	return result;
}

void vrDestroyCvt2YSource ( HDEINTERLACESOURCE source )
{
	if( !source ){ return; }
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return ; }
	_AUTO_BACKUP_CURRENT_EGL_;
	GL_CHECK(glDeleteTextures(1,&source->texture_name[0]));
	EGL_CHECK(_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, source->pixmap_image[0]));
	vrDestroyPixmap(source->native_pixmap[0]);
}

HDEINTERLACESOURCE vrCreateCvt2UVSource  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data)
{
	//ErrMsg("vrCreateCvt2UVSource start(%dx%d)\n", uiWidth, uiHeight);

	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HDEINTERLACESOURCE)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HDEINTERLACESOURCE result = (HDEINTERLACESOURCE)malloc(sizeof(struct vrDoDeinterlaceSource));
	if( !result )
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HDEINTERLACESOURCE)0;
	}
	memset( result, 0, sizeof(struct vrDoDeinterlaceSource) );
	
	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_CVT2UV]->pixmap_surface, 
								pStatics->default_target[VR_PROGRAM_CVT2UV]->pixmap_surface, pStatics->egl_info.sEGLContext16bit);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return 0;	
	}

	EGLNativePixmapType pixmapInput = (fbdev_pixmap*)vrCreatePixmap(uiWidth, uiHeight, Data, VR_FALSE, 32, VR_FALSE);
	if(pixmapInput == NULL || ((fbdev_pixmap*)pixmapInput)->data == NULL)
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
		free(result);
		return (HDEINTERLACESOURCE)0;
	}

	//RGB is not supported	
	EGLint imageAttributes[] = {
		EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, 
		EGL_NONE
	};	
	EGLImageKHR eglImage = EGL_CHECK(_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
							           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput, imageAttributes));	

	GLuint textureName;
	GL_CHECK(glGenTextures(1, &textureName));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_LINEAR));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GL_CHECK(_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage));		

	result->width        = uiWidth ;
	result->height       = uiHeight;
	result->texture_name[0] = textureName;
	result->native_pixmap[0]= pixmapInput;
	result->pixmap_image[0] = eglImage   ;	

	//ErrMsg("vrCreateCvt2UVSource done\n");
	
	return result;
}

void vrDestroyCvt2UVSource ( HDEINTERLACESOURCE source )
{
	if( !source ){ return; }
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return ; }
	_AUTO_BACKUP_CURRENT_EGL_;
	GL_CHECK(glDeleteTextures(1,&source->texture_name[0]));
	EGL_CHECK(_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, source->pixmap_image[0]));
	vrDestroyPixmap(source->native_pixmap[0]);
}

HDEINTERLACESOURCE vrCreateCvt2RgbaSource  (unsigned int uiWidth, unsigned int uiHeight, 
								NX_MEMORY_HANDLE DataY, NX_MEMORY_HANDLE DataU, NX_MEMORY_HANDLE DataV)
{
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HDEINTERLACESOURCE)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HDEINTERLACESOURCE result = (HDEINTERLACESOURCE)malloc(sizeof(struct vrDoDeinterlaceSource));
	if( !result )
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HDEINTERLACESOURCE)0;
	}
	memset( result, 0, sizeof(struct vrDoDeinterlaceSource) );
	
	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_CVT2RGBA]->pixmap_surface, 
								pStatics->default_target[VR_PROGRAM_CVT2RGBA]->pixmap_surface, pStatics->egl_info.sEGLContext32bit);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return 0;	
	}

	EGLNativePixmapType pixmapInput[VR_INPUT_MODE_YUV_MAX] = {NULL,};
	pixmapInput[VR_INPUT_MODE_Y] = (fbdev_pixmap*)vrCreatePixmap(uiWidth, uiHeight, DataY, VR_TRUE, 8, VR_FALSE);
	if(pixmapInput[VR_INPUT_MODE_Y] == NULL || ((fbdev_pixmap*)pixmapInput[VR_INPUT_MODE_Y])->data == NULL)
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
		free(result);
		return (HDEINTERLACESOURCE)0;
	}
	pixmapInput[VR_INPUT_MODE_U] = (fbdev_pixmap*)vrCreatePixmap(uiWidth/2, uiHeight/2, DataU, VR_TRUE, 8, VR_FALSE);
	if(pixmapInput[VR_INPUT_MODE_U] == NULL || ((fbdev_pixmap*)pixmapInput[VR_INPUT_MODE_U])->data == NULL)
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
		free(result);
		return (HDEINTERLACESOURCE)0;
	}
	pixmapInput[VR_INPUT_MODE_V] = (fbdev_pixmap*)vrCreatePixmap(uiWidth/2, uiHeight/2, DataV, VR_TRUE, 8, VR_FALSE);
	if(pixmapInput[VR_INPUT_MODE_V] == NULL || ((fbdev_pixmap*)pixmapInput[VR_INPUT_MODE_V])->data == NULL)
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
		free(result);
		return (HDEINTERLACESOURCE)0;
	}

	//RGB is not supported	
	EGLint imageAttributes[] = {
		EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, 
		EGL_NONE
	};	
	EGLImageKHR eglImage[VR_INPUT_MODE_YUV_MAX] = {NULL,};
	eglImage[VR_INPUT_MODE_Y] = EGL_CHECK(_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
							           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput[VR_INPUT_MODE_Y], imageAttributes));	
	eglImage[VR_INPUT_MODE_U] = EGL_CHECK(_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
							           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput[VR_INPUT_MODE_U], imageAttributes));	
	eglImage[VR_INPUT_MODE_V] = EGL_CHECK(_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
							           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput[VR_INPUT_MODE_V], imageAttributes));	

	GLuint textureName[VR_INPUT_MODE_YUV_MAX];
	GL_CHECK(glGenTextures(VR_INPUT_MODE_YUV_MAX, textureName));
	
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_Y));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName[VR_INPUT_MODE_Y]));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GL_CHECK(_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage[VR_INPUT_MODE_Y]));		
	result->texture_name[VR_INPUT_MODE_Y] = textureName[VR_INPUT_MODE_Y];
	result->native_pixmap[VR_INPUT_MODE_Y]= pixmapInput[VR_INPUT_MODE_Y];
	result->pixmap_image[VR_INPUT_MODE_Y] = eglImage[VR_INPUT_MODE_Y]   ;				 

	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_U));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName[VR_INPUT_MODE_U]));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GL_CHECK(_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage[VR_INPUT_MODE_U]));		
	result->texture_name[VR_INPUT_MODE_U] = textureName[VR_INPUT_MODE_U];
	result->native_pixmap[VR_INPUT_MODE_U]= pixmapInput[VR_INPUT_MODE_U];
	result->pixmap_image[VR_INPUT_MODE_U] = eglImage[VR_INPUT_MODE_U]   ;				 

	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_V));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName[VR_INPUT_MODE_V]));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GL_CHECK(_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage[VR_INPUT_MODE_V]));		
	result->texture_name[VR_INPUT_MODE_V] = textureName[VR_INPUT_MODE_V];
	result->native_pixmap[VR_INPUT_MODE_V]= pixmapInput[VR_INPUT_MODE_V];
	result->pixmap_image[VR_INPUT_MODE_V] = eglImage[VR_INPUT_MODE_V]   ;				 
	
	result->width        = uiWidth ;
	result->height       = uiHeight;
	return result;
}

void vrDestroyCvt2RgbaSource ( HDEINTERLACESOURCE source )
{
	if( !source ){ return; }
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return ; }
	_AUTO_BACKUP_CURRENT_EGL_;
	GL_CHECK(glDeleteTextures(VR_INPUT_MODE_YUV_MAX, source->texture_name));
	EGL_CHECK(_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, source->pixmap_image[VR_INPUT_MODE_Y]));
	EGL_CHECK(_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, source->pixmap_image[VR_INPUT_MODE_U]));
	EGL_CHECK(_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, source->pixmap_image[VR_INPUT_MODE_V]));
	vrDestroyPixmap(source->native_pixmap[VR_INPUT_MODE_Y]);
	vrDestroyPixmap(source->native_pixmap[VR_INPUT_MODE_U]);
	vrDestroyPixmap(source->native_pixmap[VR_INPUT_MODE_V]);
}


void  vrRunDeinterlace( HDEINTERLACETARGET target, HDEINTERLACESOURCE source)
{
	Statics* pStatics = vrGetStatics();
	Shader* pshader = &(vrGetStatics()->shader[VR_PROGRAM_DEINTERLACE]);	
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	
	const float aSquareTexCoord[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};
	if( NULL == pStatics || NULL == pshader || NULL == target || NULL == source )
	{
		ErrMsg("Error: NULL output surface at %s:%i\n", __FILE__, __LINE__);
		return;
	}
	_AUTO_BACKUP_CURRENT_EGL_;

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, target->pixmap_surface, target->pixmap_surface, pStatics->egl_info.sEGLContext32bit);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return;	
	}
	GL_CHECK(glUseProgram(pshader->iProgName));
	GL_CHECK(glViewport(0,0,((fbdev_pixmap *)target->native_pixmap)->width,((fbdev_pixmap *)target->native_pixmap)->height));
	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
	GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
	GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord, 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord));

	GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord));

    GL_CHECK(glUniform1f(pshader->iLocInputHeight, source->height));
    //GL_CHECK(glUniform1f(pStatics->shader[program].iLocOutputHeight, output_height));
	GL_CHECK(glUniform1i(pshader->iLocMainTex[0], VR_INPUT_MODE_DEINTERLACE));
	GL_CHECK(glUniform1i(pshader->iLocRefTex, VR_INPUT_MODE_DEINTERLACE_REF));

	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_DEINTERLACE));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, source->texture_name[0]));

	//DbgMsg( "draw\n" ); 
	GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
	EGL_CHECK(eglSwapBuffers(pStatics->egl_info.sEGLDisplay, target->pixmap_surface));
}

void vrRunScale( HDEINTERLACETARGET target, HDEINTERLACESOURCE source)
{
	Statics* pStatics = vrGetStatics();
	Shader* pshader = &(vrGetStatics()->shader[VR_PROGRAM_SCALE]);	
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	
	const float aSquareTexCoord[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};
	if( NULL == pStatics || NULL == pshader || NULL == target || NULL == source )
	{
		ErrMsg("Error: NULL output surface at %s:%i\n", __FILE__, __LINE__);
		return;
	}
	_AUTO_BACKUP_CURRENT_EGL_;

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, target->pixmap_surface, target->pixmap_surface, pStatics->egl_info.sEGLContext8bit);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return; 
	}

	GL_CHECK(glUseProgram(pshader->iProgName));
	GL_CHECK(glViewport(0,0,((fbdev_pixmap *)target->native_pixmap)->width,((fbdev_pixmap *)target->native_pixmap)->height));
	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
	GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
	GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord, 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord));

	GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord));

	GL_CHECK(glUniform1i(pshader->iLocMainTex[0], VR_INPUT_MODE_NEAREST));

	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_NEAREST));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, source->texture_name[0]));

	//DbgMsg( "draw\n" );
	GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
	EGL_CHECK(eglSwapBuffers(pStatics->egl_info.sEGLDisplay, target->pixmap_surface));
}

void  vrRunCvt2Y( HDEINTERLACETARGET target, HDEINTERLACESOURCE source)
{
	Statics* pStatics = vrGetStatics();
	Shader* pshader = &(vrGetStatics()->shader[VR_PROGRAM_CVT2Y]);	
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	
	const float aSquareTexCoord[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};
	if( NULL == pStatics || NULL == pshader || NULL == target || NULL == source )
	{
		ErrMsg("Error: NULL output surface at %s:%i\n", __FILE__, __LINE__);
		return;
	}
	_AUTO_BACKUP_CURRENT_EGL_;

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, target->pixmap_surface, target->pixmap_surface, pStatics->egl_info.sEGLContext8bit);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return;	
	}
	GL_CHECK(glUseProgram(pshader->iProgName));
	GL_CHECK(glViewport(0,0,((fbdev_pixmap *)target->native_pixmap)->width,((fbdev_pixmap *)target->native_pixmap)->height));
	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
	GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
	GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord, 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord));

	GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord));

	GL_CHECK(glUniform1i(pshader->iLocMainTex[0], VR_INPUT_MODE_NEAREST));

	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_NEAREST));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, source->texture_name[0]));

	//DbgMsg( "draw\n" ); 
	GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
	EGL_CHECK(eglSwapBuffers(pStatics->egl_info.sEGLDisplay, target->pixmap_surface));
}

void  vrRunCvt2UV( HDEINTERLACETARGET target, HDEINTERLACESOURCE source)
{
	//DbgMsg( "vrRunCvt2UV start\n" ); 

	Statics* pStatics = vrGetStatics();
	Shader* pshader = &(vrGetStatics()->shader[VR_PROGRAM_CVT2UV]);	
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	
	const float aSquareTexCoord[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};
	if( NULL == pStatics || NULL == pshader || NULL == target || NULL == source )
	{
		ErrMsg("Error: NULL output surface at %s:%i\n", __FILE__, __LINE__);
		return;
	}
	_AUTO_BACKUP_CURRENT_EGL_;

	/* Make context current. */
	GL_CHECK();
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, target->pixmap_surface, target->pixmap_surface, pStatics->egl_info.sEGLContext16bit);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return;	
	}
	EGL_CHECK();
	GL_CHECK();
	GL_CHECK(glUseProgram(pshader->iProgName));
	GL_CHECK(glViewport(0,0,((fbdev_pixmap *)target->native_pixmap)->width,((fbdev_pixmap *)target->native_pixmap)->height));
	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
	GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
	GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord, 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord));

	GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord));

	GL_CHECK(glUniform1i(pshader->iLocMainTex[0], VR_INPUT_MODE_LINEAR));

	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_LINEAR));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, source->texture_name[0]));

	GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
	EGL_CHECK(eglSwapBuffers(pStatics->egl_info.sEGLDisplay, target->pixmap_surface));
}

void  vrRunCvt2Rgba( HDEINTERLACETARGET target, HDEINTERLACESOURCE source)
{
	Statics* pStatics = vrGetStatics();
	Shader* pshader = &(vrGetStatics()->shader[VR_PROGRAM_CVT2RGBA]);	
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	
	const float aSquareTexCoord[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};
	if( NULL == pStatics || NULL == pshader || NULL == target || NULL == source )
	{
		ErrMsg("Error: NULL output surface at %s:%i\n", __FILE__, __LINE__);
		return;
	}
	_AUTO_BACKUP_CURRENT_EGL_;

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, target->pixmap_surface, target->pixmap_surface, pStatics->egl_info.sEGLContext32bit);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return;	
	}
	GL_CHECK(glUseProgram(pshader->iProgName));
	GL_CHECK(glViewport(0,0,((fbdev_pixmap *)target->native_pixmap)->width,((fbdev_pixmap *)target->native_pixmap)->height));
	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
	GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
	GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord, 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord));

	GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord));

    //GL_CHECK(glUniform1f(pStatics->shader[program].iLocOutputHeight, output_height));
	GL_CHECK(glUniform1i(pshader->iLocMainTex[VR_INPUT_MODE_Y], VR_INPUT_MODE_Y));
	GL_CHECK(glUniform1i(pshader->iLocMainTex[VR_INPUT_MODE_U], VR_INPUT_MODE_U));
	GL_CHECK(glUniform1i(pshader->iLocMainTex[VR_INPUT_MODE_V], VR_INPUT_MODE_V));

	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_Y));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, source->texture_name[VR_INPUT_MODE_Y]));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_U));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, source->texture_name[VR_INPUT_MODE_U]));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_V));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, source->texture_name[VR_INPUT_MODE_V]));

	//DbgMsg( "draw\n" ); 
	GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
	EGL_CHECK(eglSwapBuffers(pStatics->egl_info.sEGLDisplay, target->pixmap_surface));
}

void  vrWaitDeinterlaceDone ( void )
{
	_AUTO_BACKUP_CURRENT_EGL_;
	//GL_CHECK(glFinish());// ?????
	EGL_CHECK(eglWaitGL());
}

void  vrWaitScaleDone ( void )
{
	vrWaitDeinterlaceDone();
}

void  vrWaitCvt2YDone ( void )
{
	vrWaitDeinterlaceDone();
}

void  vrWaitCvt2UVDone ( void )
{
	vrWaitDeinterlaceDone();
}

void  vrWaitCvt2RgbaDone ( void )
{
	vrWaitDeinterlaceDone();
}

#ifdef VR_FEATURE_SHADER_FILE_USE
/* loadShader():	Load the shader source into memory.
 *
 * sFilename: String holding filename to load.
 */
static char *loadShader(const char *sFilename)
{
	char *pResult = NULL;
	FILE *pFile = NULL;
	long iLen = 0;

	pFile = fopen(sFilename, "r");
	if(pFile == NULL) {
		ErrMsg("Error: Cannot read file '%s'\n", sFilename);
		return NULL;
	}
	fseek(pFile, 0, SEEK_END); /* Seek end of file. */
	iLen = ftell(pFile);
	fseek(pFile, 0, SEEK_SET); /* Seek start of file again. */
	pResult = (char*)calloc(iLen+1, sizeof(char));
	if(pResult == NULL)
	{
		ErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return NULL;
	}
	fread(pResult, sizeof(char), iLen, pFile);
	pResult[iLen] = '\0';
	fclose(pFile);

	return pResult;
}

/* processShader(): Create shader, load in source, compile, dump debug as necessary.
 *
 * pShader: Pointer to return created shader ID.
 * sFilename: Passed-in filename from which to load shader source.
 * iShaderType: Passed to GL, e.g. GL_VERTEX_SHADER.
 */
static int processShader(GLuint *pShader, const char *sFilename, GLint iShaderType)
{
	GLint iStatus;
	const char *aStrings[1] = { NULL };

	/* Create shader and load into GL. */
	*pShader = GL_CHECK(glCreateShader(iShaderType));
	aStrings[0] = loadShader(sFilename);
	if(aStrings[0] == NULL)
	{
		ErrMsg("Error: wrong shader code %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	GL_CHECK(glShaderSource(*pShader, 1, aStrings, NULL));

	/* Clean up shader source. */
	free((void *)(aStrings[0]));
	aStrings[0] = NULL;

	/* Try compiling the shader. */
	GL_CHECK(glCompileShader(*pShader));
	GL_CHECK(glGetShaderiv(*pShader, GL_COMPILE_STATUS, &iStatus));

	/* Dump debug info (source and log) if compilation failed. */
	if(iStatus != GL_TRUE) {
		GLint iLen;
		char *sDebugSource = NULL;
		char *sErrorLog = NULL;

		/* Get shader source. */
		GL_CHECK(glGetShaderiv(*pShader, GL_SHADER_SOURCE_LENGTH, &iLen));
		sDebugSource = (char*)malloc(iLen);
		GL_CHECK(glGetShaderSource(*pShader, iLen, NULL, sDebugSource));
		DbgMsg("Debug source START:\n%s\nDebug source END\n\n", sDebugSource);
		free(sDebugSource);

		/* Now get the info log. */
		GL_CHECK(glGetShaderiv(*pShader, GL_INFO_LOG_LENGTH, &iLen));
		sErrorLog = (char*)malloc(iLen);
		GL_CHECK(glGetShaderInfoLog(*pShader, iLen, NULL, sErrorLog));
		DbgMsg("Log START:\n%s\nLog END\n\n", sErrorLog);
		free(sErrorLog);

		DbgMsg("Compilation FAILED!\n\n");
		return -1;
	}
	return 0;
}
#else
/* processShader(): Create shader, load in source, compile, dump debug as necessary.
 *
 * pShader: Pointer to return created shader ID.
 * sFilename: Passed-in filename from which to load shader source.
 * iShaderType: Passed to GL, e.g. GL_VERTEX_SHADER.
 */
static int processShader(GLuint *pShader, const char *pString, GLint iShaderType)
{
	GLint iStatus;
	const char *aStrings[1] = { NULL };

	if(pString == NULL)
	{
		ErrMsg("Error: wrong shader code %s:%i\n", __FILE__, __LINE__);
		return -1;
	}

	/* Create shader and load into GL. */
	*pShader = GL_CHECK(glCreateShader(iShaderType));
	if(pShader == NULL)
	{
		ErrMsg("Error: wrong shader code %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	aStrings[0] = pString;
	GL_CHECK(glShaderSource(*pShader, 1, aStrings, NULL));

	/* Clean up shader source. */
	aStrings[0] = NULL;

	/* Try compiling the shader. */
	GL_CHECK(glCompileShader(*pShader));
	GL_CHECK(glGetShaderiv(*pShader, GL_COMPILE_STATUS, &iStatus));

	/* Dump debug info (source and log) if compilation failed. */
	if(iStatus != GL_TRUE) {
		GLint iLen;
		char *sDebugSource = NULL;
		char *sErrorLog = NULL;

		/* Get shader source. */
		GL_CHECK(glGetShaderiv(*pShader, GL_SHADER_SOURCE_LENGTH, &iLen));
		sDebugSource = (char*)malloc(iLen);
		GL_CHECK(glGetShaderSource(*pShader, iLen, NULL, sDebugSource));
		DbgMsg("Debug source START:\n%s\nDebug source END\n\n", sDebugSource);
		free(sDebugSource);

		/* Now get the info log. */
		GL_CHECK(glGetShaderiv(*pShader, GL_INFO_LOG_LENGTH, &iLen));
		sErrorLog = (char*)malloc(iLen);
		GL_CHECK(glGetShaderInfoLog(*pShader, iLen, NULL, sErrorLog));
		DbgMsg("Log START:\n%s\nLog END\n\n", sErrorLog);
		free(sErrorLog);

		DbgMsg("Compilation FAILED!\n\n");
		return -1;
	}
	return 0;
}
#endif

static int vrInitializeGL( HDEINTERLACETARGET* pTarget)
{
	Statics* pStatics = vrGetStatics();
	unsigned int program;
	//For 32bit context
	{
		/* For Deinterlace. */
		{
			program = VR_PROGRAM_DEINTERLACE;
			/* Make context current. */
			EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pTarget[program]->pixmap_surface, pTarget[program]->pixmap_surface, pStatics->egl_info.sEGLContext32bit);
			if(bResult == EGL_FALSE)
			{
				EGLint iError = eglGetError();
				ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
				ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
				return -1;	
			}
			
			/* Load shaders. */
			if(processShader(&pStatics->shader[program].iVertName, VERTEX_SHADER_SOURCE_DEINTERLACE, GL_VERTEX_SHADER) < 0)
			{
				ErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			if(processShader(&pStatics->shader[program].iFragName, FRAGMENT_SHADER_SOURCE_DEINTERLACE, GL_FRAGMENT_SHADER) < 0)
			{
				ErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
				return -1;
			}

			/* Set up shaders. */
			pStatics->shader[program].iProgName = GL_CHECK(glCreateProgram());
			//DbgMsg("Deinterlace iProgName(%d)\n", pStatics->shader[program].iProgName);		
			GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iVertName));
			GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iFragName));
			GL_CHECK(glLinkProgram(pStatics->shader[program].iProgName));
			GL_CHECK(glUseProgram(pStatics->shader[program].iProgName));

			/* Vertex positions. */
			pStatics->shader[program].iLocPosition = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v4Position"));
			if(pStatics->shader[program].iLocPosition == -1)
			{
				ErrMsg("Error: Attribute not found at %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocPosition));

			/* Fill texture. */
			pStatics->shader[program].iLocTexCoord = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v2TexCoord"));
			if(pStatics->shader[program].iLocTexCoord == -1)
			{
				ErrMsg("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocTexCoord));

		    /* Texture Height. */
		    pStatics->shader[program].iLocInputHeight = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "u_fTexHeight"));
		    if(pStatics->shader[program].iLocInputHeight == -1)
		    {
				ErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
				//return -1;
		    }

		    /* diffuse texture. */
		    pStatics->shader[program].iLocMainTex[0] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuse"));
		    if(pStatics->shader[program].iLocMainTex[0] == -1)
		    {
				ErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
				//return -1;
		    }
		    else 
		    {
		        //GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_DEINTERLACE));
		    }

			/* ref texture. */
		    pStatics->shader[program].iLocRefTex = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "ref_tex"));
		    if(pStatics->shader[program].iLocRefTex == -1)
		    {
				ErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
				//return -1;
		    }
		    else 
		    {
		        //GL_CHECK(glUniform1i(pStatics->shader[program].iLocRefTex, VR_INPUT_MODE_DEINTERLACE_REF));
		    }
			
			//set texture
			GL_CHECK(glGenTextures(1, &pStatics->tex_deinterlace_ref_id));
			GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_DEINTERLACE_REF));
			GL_CHECK(glBindTexture(GL_TEXTURE_2D, pStatics->tex_deinterlace_ref_id));
			GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
			GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
			GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
			GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT)); 
			{
				unsigned int temp_imgbuf[2] = {0x00000000, 0xFFFFFFFF};
				GL_CHECK(glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,
						 1,2,0,
						 GL_RGBA,GL_UNSIGNED_BYTE,temp_imgbuf));	
			}
		}		

		/* For Cvt2Rgba. */
		{
			program = VR_PROGRAM_CVT2RGBA;
			/* Make context current. */
			EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pTarget[program]->pixmap_surface, pTarget[program]->pixmap_surface, pStatics->egl_info.sEGLContext32bit);
			if(bResult == EGL_FALSE)
			{
				EGLint iError = eglGetError();
				ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
				ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
				return -1;	
			}
			
			/* Load shaders. */
			if(processShader(&pStatics->shader[program].iVertName, VERTEX_SHADER_SOURCE_CVT2RGBA, GL_VERTEX_SHADER) < 0)
			{
				ErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			if(processShader(&pStatics->shader[program].iFragName, FRAGMENT_SHADER_SOURCE_CVT2RGBA, GL_FRAGMENT_SHADER) < 0)
			{
				ErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
		
			/* Set up shaders. */
			pStatics->shader[program].iProgName = GL_CHECK(glCreateProgram());
			//DbgMsg("Deinterlace iProgName(%d)\n", pStatics->shader[program].iProgName);		
			GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iVertName));
			GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iFragName));
			GL_CHECK(glLinkProgram(pStatics->shader[program].iProgName));
			GL_CHECK(glUseProgram(pStatics->shader[program].iProgName));
		
			/* Vertex positions. */
			pStatics->shader[program].iLocPosition = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v4Position"));
			if(pStatics->shader[program].iLocPosition == -1)
			{
				ErrMsg("Error: Attribute not found at %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocPosition));
		
			/* Fill texture. */
			pStatics->shader[program].iLocTexCoord = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v2TexCoord"));
			if(pStatics->shader[program].iLocTexCoord == -1)
			{
				ErrMsg("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocTexCoord));
				
			/* Y texture. */
			pStatics->shader[program].iLocMainTex[VR_INPUT_MODE_Y] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuseY"));
			if(pStatics->shader[program].iLocMainTex[VR_INPUT_MODE_Y] == -1)
			{
				ErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
				//return -1;
			}
			else 
			{
				//GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_DEINTERLACE));
			}
				
			/* U texture. */
			pStatics->shader[program].iLocMainTex[VR_INPUT_MODE_U] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuseU"));
			if(pStatics->shader[program].iLocMainTex[VR_INPUT_MODE_U] == -1)
			{
				ErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
				//return -1;
			}
			else 
			{
				//GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_DEINTERLACE));
			}
				
			/* V texture. */
			pStatics->shader[program].iLocMainTex[VR_INPUT_MODE_V] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuseV"));
			if(pStatics->shader[program].iLocMainTex[VR_INPUT_MODE_V] == -1)
			{
				ErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
				//return -1;
			}
			else 
			{
				//GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_DEINTERLACE));
			}		
		}
	}
	
	//For 8bit context
	#if 1
	{	
		/* For Scaler. */
		{
			program = VR_PROGRAM_SCALE;
			#if 1
			/* Make context current. */
			EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pTarget[program]->pixmap_surface, pTarget[program]->pixmap_surface, pStatics->egl_info.sEGLContext8bit);
			if(bResult == EGL_FALSE)
			{
				EGLint iError = eglGetError();
				ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
				ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
				return -1;	
			}
			#endif
			
			/* Load shaders. */
			if(processShader(&pStatics->shader[program].iVertName, VERTEX_SHADER_SOURCE_SCALE, GL_VERTEX_SHADER) < 0)
			{
				ErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			if(processShader(&pStatics->shader[program].iFragName, FRAGMENT_SHADER_SOURCE_SCALE, GL_FRAGMENT_SHADER) < 0)
			{
				ErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			
			/* Set up shaders. */
			pStatics->shader[program].iProgName = GL_CHECK(glCreateProgram());
			//DbgMsg("Scaler iProgName(%d)\n", pStatics->shader[program].iProgName);
			GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iVertName));
			GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iFragName));
			GL_CHECK(glLinkProgram(pStatics->shader[program].iProgName));
			GL_CHECK(glUseProgram(pStatics->shader[program].iProgName));

			/* Vertex positions. */
			pStatics->shader[program].iLocPosition = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v4Position"));
			if(pStatics->shader[program].iLocPosition == -1)
			{
				ErrMsg("Error: Attribute not found at %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocPosition));

			/* Fill texture. */
			pStatics->shader[program].iLocTexCoord = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v2TexCoord"));
			if(pStatics->shader[program].iLocTexCoord == -1)
			{
				ErrMsg("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocTexCoord));

			/* diffuse texture. */
			pStatics->shader[program].iLocMainTex[0] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuse"));
			if(pStatics->shader[program].iLocMainTex[0] == -1)
			{
				ErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
				//return -1;
			}
			else 
			{
				//GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_NEAREST));
			}
		}

		/* For Cvt2Y. */
		{
			program = VR_PROGRAM_CVT2Y;
			#if 1
			/* Make context current. */
			EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pTarget[program]->pixmap_surface, pTarget[program]->pixmap_surface, pStatics->egl_info.sEGLContext8bit);
			if(bResult == EGL_FALSE)
			{
				EGLint iError = eglGetError();
				ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
				ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
				return -1;	
			}
			#endif
			
			/* Load shaders. */
			if(processShader(&pStatics->shader[program].iVertName, VERTEX_SHADER_SOURCE_CVT2Y, GL_VERTEX_SHADER) < 0)
			{
				ErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			if(processShader(&pStatics->shader[program].iFragName, FRAGMENT_SHADER_SOURCE_CVT2Y, GL_FRAGMENT_SHADER) < 0)
			{
				ErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			
			/* Set up shaders. */
			pStatics->shader[program].iProgName = GL_CHECK(glCreateProgram());
			//DbgMsg("Scaler iProgName(%d)\n", pStatics->shader[program].iProgName);
			GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iVertName));
			GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iFragName));
			GL_CHECK(glLinkProgram(pStatics->shader[program].iProgName));
			GL_CHECK(glUseProgram(pStatics->shader[program].iProgName));

			/* Vertex positions. */
			pStatics->shader[program].iLocPosition = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v4Position"));
			if(pStatics->shader[program].iLocPosition == -1)
			{
				ErrMsg("Error: Attribute not found at %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocPosition));

			/* Fill texture. */
			pStatics->shader[program].iLocTexCoord = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v2TexCoord"));
			if(pStatics->shader[program].iLocTexCoord == -1)
			{
				ErrMsg("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocTexCoord));

			/* diffuse texture. */
			pStatics->shader[program].iLocMainTex[0] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuse"));
			if(pStatics->shader[program].iLocMainTex[0] == -1)
			{
				ErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
				//return -1;
			}
			else 
			{
				//GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_NEAREST));
			}
		}		
	}
	#endif	


	//For 16bit context
	#if 1
	{	
		/* For Cvt2UV. */
		{
			program = VR_PROGRAM_CVT2UV;
			#if 1
			/* Make context current. */
			EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pTarget[program]->pixmap_surface, pTarget[program]->pixmap_surface, pStatics->egl_info.sEGLContext16bit);
			if(bResult == EGL_FALSE)
			{
				EGLint iError = eglGetError();
				ErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
				ErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
				return -1;	
			}
			#endif
			
			/* Load shaders. */
			if(processShader(&pStatics->shader[program].iVertName, VERTEX_SHADER_SOURCE_CVT2UV, GL_VERTEX_SHADER) < 0)
			{
				ErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			if(processShader(&pStatics->shader[program].iFragName, FRAGMENT_SHADER_SOURCE_CVT2UV, GL_FRAGMENT_SHADER) < 0)
			{
				ErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			
			/* Set up shaders. */
			pStatics->shader[program].iProgName = GL_CHECK(glCreateProgram());
			//DbgMsg("Scaler iProgName(%d)\n", pStatics->shader[program].iProgName);
			GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iVertName));
			GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iFragName));
			GL_CHECK(glLinkProgram(pStatics->shader[program].iProgName));
			GL_CHECK(glUseProgram(pStatics->shader[program].iProgName));

			/* Vertex positions. */
			pStatics->shader[program].iLocPosition = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v4Position"));
			if(pStatics->shader[program].iLocPosition == -1)
			{
				ErrMsg("Error: Attribute not found at %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocPosition));

			/* Fill texture. */
			pStatics->shader[program].iLocTexCoord = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v2TexCoord"));
			if(pStatics->shader[program].iLocTexCoord == -1)
			{
				ErrMsg("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
				return -1;
			}
			//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocTexCoord));

			/* diffuse texture. */
			pStatics->shader[program].iLocMainTex[0] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuse"));
			if(pStatics->shader[program].iLocMainTex[0] == -1)
			{
				ErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
				//return -1;
			}
			else 
			{
				//GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_NEAREST));
			}
		}
	}
	#endif
	return 0;
}
