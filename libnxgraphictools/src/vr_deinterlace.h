#ifndef __VR_DEINTERACE__
#define __VR_DEINTERACE__

#include <nx_alloc_mem.h>

extern const char deinterace_vertex_shader[];
extern const char deinterace_frag_shader[];
extern const char scaler_vertex_shader[];
extern const char scaler_frag_shader[];
extern const char cvt2y_vertex_shader[];
extern const char cvt2y_frag_shader[];
extern const char cvt2uv_vertex_shader[];
extern const char cvt2uv_frag_shader[];
extern const char cvt2rgba_vertex_shader[];
extern const char cvt2rgba_frag_shader[];

typedef struct vrDoDeinterlaceTarget* HDEINTERLACETARGET;
typedef struct vrDoDeinterlaceSource* HDEINTERLACESOURCE;

int                vrInitializeGLSurface    ( void );

HDEINTERLACETARGET vrCreateDeinterlaceTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data);
HDEINTERLACESOURCE vrCreateDeinterlaceSource  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data);
void               vrRunDeinterlace           ( HDEINTERLACETARGET target, HDEINTERLACESOURCE source);
void               vrWaitDeinterlaceDone      ( void );
void               vrDestroyDeinterlaceTarget ( HDEINTERLACETARGET target );
void               vrDestroyDeinterlaceSource ( HDEINTERLACESOURCE source );

HDEINTERLACETARGET vrCreateScaleTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data);
HDEINTERLACESOURCE vrCreateScaleSource  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data);
void               vrRunScale           	      ( HDEINTERLACETARGET target, HDEINTERLACESOURCE source);
void               vrWaitScaleDone      ( void );
void               vrDestroyScaleTarget ( HDEINTERLACETARGET target );
void               vrDestroyScaleSource ( HDEINTERLACESOURCE source );

HDEINTERLACETARGET vrCreateCvt2YTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data);
HDEINTERLACESOURCE vrCreateCvt2YSource  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data);
void               vrRunCvt2Y           	      ( HDEINTERLACETARGET target, HDEINTERLACESOURCE source);
void               vrWaitCvt2YDone      ( void );
void               vrDestroyCvt2YTarget ( HDEINTERLACETARGET target );
void               vrDestroyCvt2YSource ( HDEINTERLACESOURCE source );

HDEINTERLACETARGET vrCreateCvt2UVTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data);
HDEINTERLACESOURCE vrCreateCvt2UVSource  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data);
void               vrRunCvt2UV           	      ( HDEINTERLACETARGET target, HDEINTERLACESOURCE source);
void               vrWaitCvt2UVDone      ( void );
void               vrDestroyCvt2UVTarget ( HDEINTERLACETARGET target );
void               vrDestroyCvt2UVSource ( HDEINTERLACESOURCE source );

HDEINTERLACETARGET vrCreateCvt2RgbaTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE Data);
HDEINTERLACESOURCE vrCreateCvt2RgbaSource  (unsigned int uiWidth, unsigned int uiHeight, 
												NX_MEMORY_HANDLE DataY, NX_MEMORY_HANDLE DataU, NX_MEMORY_HANDLE DataV);
void               vrRunCvt2Rgba           	( HDEINTERLACETARGET target, HDEINTERLACESOURCE source);
void               vrWaitCvt2RgbaDone      ( void );
void               vrDestroyCvt2RgbaTarget ( HDEINTERLACETARGET target );
void               vrDestroyCvt2RgbaSource ( HDEINTERLACESOURCE source );

void               vrTerminateGLSurface     ( void );

#endif  /* __VR_DEINTERACE__ */

