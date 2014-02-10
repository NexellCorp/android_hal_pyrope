#ifndef __VR_DEINTERACE_PRIVATE_
#define __VR_DEINTERACE_PRIVATE_

#include "vr_deinterlace.h"
#include <nx_alloc_mem.h>
#include "vr_common_def.h"
#include "vr_egl_runtime.h"

//program
enum{
	VR_PROGRAM_DEINTERLACE,
	VR_PROGRAM_SCALE,
	VR_PROGRAM_CVT2Y,
	VR_PROGRAM_CVT2UV,
	VR_PROGRAM_CVT2RGBA,
	VR_PROGRAM_MAX
};

//texture unit
typedef enum{
	VR_INPUT_MODE_DEINTERLACE,
	VR_INPUT_MODE_DEINTERLACE_REF,
	VR_INPUT_MODE_NEAREST,
	VR_INPUT_MODE_LINEAR,
	VR_INPUT_MODE_MAX
}VRInputMode;

//YUV texture unit 
typedef enum{
	VR_INPUT_MODE_Y,
	VR_INPUT_MODE_U,
	VR_INPUT_MODE_V,
	VR_INPUT_MODE_YUV_MAX
}VRInputYUVMode;


typedef struct
{
	/* Shader variables. */
	GLuint iVertName;
	GLuint iFragName;
	GLuint iProgName;
	GLint iLocPosition;
	GLint iLocTexCoord;
	GLint iLocInputHeight;
	GLint iLocOutputHeight;
	GLint iLocMainTex[VR_INPUT_MODE_YUV_MAX];
	GLint iLocRefTex;
}Shader;

typedef struct
{
	EGLInfo    egl_info;
	Shader     shader[VR_PROGRAM_MAX];
	
	NX_MEMORY_HANDLE   default_target_memory[VR_PROGRAM_MAX];
	HDEINTERLACETARGET default_target[VR_PROGRAM_MAX];

	GLuint tex_deinterlace_ref_id;
	GLuint tex_cvt_id;
}Statics;


//interanl API
void vrTerminateStatics();
Statics *vrGetStatics();

//external
#ifdef VR_FEATURE_ION_ALLOC_USE
int vrCreateSurface(EGLSurface* pSurface, EGLNativePixmapType* pPixmpaOutput, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE outData);
#else
int vrCreateSurface(EGLSurface* pSurface, EGLNativePixmapType* pPixmpaOutput, unsigned int uiWidth, unsigned int uiHeight, void* outData);
#endif
void vrDestroySurface(EGLSurface surface, EGLNativePixmapType pixmapOnput);
int vrInitGL(unsigned int input_width, unsigned int input_height, unsigned int output_width, unsigned int output_height, EGLSurface surface);
void vrDeinitGL(void);
#ifdef VR_FEATURE_INPUT_EGLIMAGE_USE
#ifdef VR_FEATURE_ION_ALLOC_USE
int vrCreateEglImageInput(EGLImageKHR* pEglImage, EGLNativePixmapType *pPixmpaInput, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE inData);
#else
int vrCreateEglImageInput(EGLImageKHR* pEglImage, EGLNativePixmapType *pPixmpaInput, unsigned int uiWidth, unsigned int uiHeight, void* inData);
#endif
void vrDestroyEglImageInput(EGLImageKHR eglImage, EGLNativePixmapType pixmapInput);
int vrDrawSurface(EGLSurface surface_output);
#else
int vrDrawSurface(EGLSurface surface_output, unsigned int input_width, unsigned int input_height, void *imgBuf);
#endif
void vrWaitSurfaceDone(EGLSurface surface_output);

#endif  /* __VR_DEINTERACE_PRIVATE_ */

