/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_shims_common.h
 * @brief Contains helper macros for the egl_entrypoints_shim.c files
 *  The macros are used to produce files with thin shims for export
 */

#ifndef _EGL_SHIMS_COMMON_H_
#define _EGL_SHIMS_COMMON_H_


/* this wraps the whole file so nothing is included without this define */
#ifdef MALI_BUILD_ANDROID_MONOLITHIC

#define MALI_MANGLE_NAME(a) shim_##a

#ifndef MALI_EGL_APIENTRY
#define MALI_EGL_APIENTRY(a) MALI_MANGLE_NAME(a)
#endif

#ifndef MALI_EGL_IMAGE_APIENTRY
#define MALI_EGL_IMAGE_APIENTRY(a) MALI_MANGLE_NAME(a)
#endif

/* For shim declarations */
#define MALI_DECLARE_SHIM_EGL_APIENTRY_v0(a,t1) \
	extern void MALI_MANGLE_NAME(a)(t1);

#define MALI_DECLARE_SHIM_EGL_APIENTRY_e0(a,t1) \
	extern EGLenum MALI_MANGLE_NAME(a)(t1);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_i0(a,t1) \
	extern EGLint MALI_MANGLE_NAME(a)(t1);

#define MALI_DECLARE_SHIM_EGL_APIENTRY_v1(a,t1,p1) \
	extern void MALI_MANGLE_NAME(a)(t1 p1);

#define MALI_DECLARE_SHIM_EGL_APIENTRY_i0(a,t1) \
	extern EGLint MALI_MANGLE_NAME(a)(t1);

#define MALI_DECLARE_SHIM_EGL_APIENTRY_b0(a,t1) \
	extern EGLBoolean MALI_MANGLE_NAME(a)();

#define MALI_DECLARE_SHIM_EGL_APIENTRY_EC0(a,t1) \
	extern EGLContext MALI_MANGLE_NAME(a)();
#define MALI_DECLARE_SHIM_EGL_APIENTRY_b1(a,t1,p1) \
	extern EGLBoolean MALI_MANGLE_NAME(a)(t1 p1);

#define MALI_DECLARE_SHIM_EGL_APIENTRY_megli1(a,t1,p1) \
	extern mali_egl_image* MALI_MANGLE_NAME(a)(t1 p1);

#define MALI_DECLARE_SHIM_EGL_APIENTRY_ccp2(a,t1,p1,t2,p2) \
	extern const char * MALI_MANGLE_NAME(a)(t1 p1,t2 p2);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_pv2(a,t1,p1,t2,p2) \
	extern void * MALI_MANGLE_NAME(a)(t1 p1,t2 p2);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_b2(a,t1,p1,t2,p2) \
	extern EGLBoolean MALI_MANGLE_NAME(a)(t1 p1,t2 p2);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_IKR2(a,t1,p1,t2,p2) \
	extern EGLImageKHR MALI_MANGLE_NAME(a)(t1 p1,t2 p2);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_b3(a,t1,p1,t2,p2,t3,p3) \
	extern EGLBoolean MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_b4(a,t1,p1,t2,p2,t3,p3,t4,p4) \
	extern EGLBoolean MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_i4(a,t1,p1,t2,p2,t3,p3,t4,p4) \
	extern EGLint MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_IKR5(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5) \
	extern EGLImageKHR MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_b5(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5) \
	extern EGLBoolean MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_ES5(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5) \
	extern EGLSurface MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5);

#define MALI_DECLARE_SHIM_EGL_APIENTRY_INKR4(a,t1,p1,t2,p2,t3,p3,t4,p4) \
	extern EGLImageNameKHR MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_EC4(a,t1,p1,t2,p2,t3,p3,t4,p4) \
	extern EGLContext MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_ES4(a,t1,p1,t2,p2,t3,p3,t4,p4) \
	extern EGLSurface MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_ES3(a,t1,p1,t2,p2,t3,p3) \
	extern EGLSurface MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3);

#define MALI_DECLARE_SHIM_EGL_APIENTRY_SKR3(a,t1,p1,t2,p2,t3,p3) \
	extern EGLSyncKHR MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3);

#define MALI_DECLARE_SHIM_EGL_APIENTRY_ES0(a,t1) \
	extern EGLSurface MALI_MANGLE_NAME(a)();
#define MALI_DECLARE_SHIM_EGL_APIENTRY_CNKR1(a,t1,p1) \
	extern EGLClientNameKHR MALI_MANGLE_NAME(a)(t1 p1);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_ES1(a,t1,p1) \
	extern EGLSurface MALI_MANGLE_NAME(a)(t1 p1);

#define MALI_DECLARE_SHIM_EGL_APIENTRY_ED0(a,t1) \
	extern EGLDisplay MALI_MANGLE_NAME(a)();
#define MALI_DECLARE_SHIM_EGL_APIENTRY_ED1(a,t1,p1) \
	extern EGLDisplay MALI_MANGLE_NAME(a)(t1 p1);

#define MALI_DECLARE_SHIM_EGL_APIENTRY_v2(a,t1,p1,t2,p2) \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_bi2(a,t1,p1,t2,p2) \
	extern GLbitfield MALI_MANGLE_NAME(a)(t1 p1,t2 p2);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_i2(a,t1,p1,t2,p2) \
	extern GLint MALI_MANGLE_NAME(a)(t1 p1,t2 p2);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_v3(a,t1,p1,t2,p2,t3,p3) \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_v4(a,t1,p1,t2,p2,t3,p3,t4,p4) \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_v5(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5) \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5);
#define MALI_DECLARE_SHIM_EGL_APIENTRY_EPT1(a,t1,p1) \
	extern __eglMustCastToProperFunctionPointerType MALI_MANGLE_NAME(a)(t1 p1);


/* For shim implementations */
#define MALI_EGL_APIENTRY_v0(a,t1) \
	a(t1) \
{ \
	extern void MALI_MANGLE_NAME(a)(t1); \
	MALI_MANGLE_NAME(a)(); \
}

#define MALI_EGL_APIENTRY_e0(a,t1) \
	a(t1) \
{ \
	extern EGLenum MALI_MANGLE_NAME(a)(t1); \
	return MALI_MANGLE_NAME(a)(); \
}
#define MALI_EGL_APIENTRY_i0(a,t1) \
	a(t1) \
{ \
	extern EGLint MALI_MANGLE_NAME(a)(t1); \
	return MALI_MANGLE_NAME(a)(); \
}

#define MALI_EGL_APIENTRY_v1(a,t1,p1) \
	a(t1 p1) \
{ \
	extern void MALI_MANGLE_NAME(a)(t1 p1); \
	MALI_MANGLE_NAME(a)(p1); \
}

#define MALI_EGL_APIENTRY_i0(a,t1) \
	a(t1) \
{ \
	extern EGLint MALI_MANGLE_NAME(a)(t1); \
	return MALI_MANGLE_NAME(a)(); \
}

#define MALI_EGL_APIENTRY_b0(a,t1) \
	a(t1) \
{ \
	extern EGLBoolean MALI_MANGLE_NAME(a)(); \
	return MALI_MANGLE_NAME(a)(); \
}

#define MALI_EGL_APIENTRY_EC0(a,t1) \
	a(t1) \
{ \
	extern EGLContext MALI_MANGLE_NAME(a)(); \
	return MALI_MANGLE_NAME(a)(); \
}
#define MALI_EGL_APIENTRY_b1(a,t1,p1) \
	a(t1 p1) \
{ \
	extern EGLBoolean MALI_MANGLE_NAME(a)(t1 p1); \
	return MALI_MANGLE_NAME(a)(p1); \
}

#define MALI_EGL_APIENTRY_megli1(a,t1,p1) \
	a(t1 p1) \
{ \
	extern mali_egl_image* MALI_MANGLE_NAME(a)(t1 p1); \
	return MALI_MANGLE_NAME(a)(p1); \
}

#define MALI_EGL_APIENTRY_ccp2(a,t1,p1,t2,p2) \
	a(t1 p1,t2 p2) \
{ \
	extern const char * MALI_MANGLE_NAME(a)(t1 p1,t2 p2); \
	return MALI_MANGLE_NAME(a)( p1, p2); \
}
#define MALI_EGL_APIENTRY_pv2(a,t1,p1,t2,p2) \
	a(t1 p1,t2 p2) \
{ \
	extern void * MALI_MANGLE_NAME(a)(t1 p1,t2 p2); \
	return MALI_MANGLE_NAME(a)( p1, p2); \
}
#define MALI_EGL_APIENTRY_b2(a,t1,p1,t2,p2) \
	a(t1 p1,t2 p2) \
{ \
	extern EGLBoolean MALI_MANGLE_NAME(a)(t1 p1,t2 p2); \
	return MALI_MANGLE_NAME(a)( p1, p2); \
}
#define MALI_EGL_APIENTRY_IKR2(a,t1,p1,t2,p2) \
	a(t1 p1,t2 p2) \
{ \
	extern EGLImageKHR MALI_MANGLE_NAME(a)(t1 p1,t2 p2); \
	return MALI_MANGLE_NAME(a)( p1, p2); \
}
#define MALI_EGL_APIENTRY_b3(a,t1,p1,t2,p2,t3,p3) \
	a(t1 p1,t2 p2,t3 p3) \
{ \
	extern EGLBoolean MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3); \
	return MALI_MANGLE_NAME(a)( p1, p2, p3); \
}
#define MALI_EGL_APIENTRY_b4(a,t1,p1,t2,p2,t3,p3,t4,p4) \
	a(t1 p1,t2 p2,t3 p3,t4 p4) \
{ \
	extern EGLBoolean MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4); \
	return MALI_MANGLE_NAME(a)( p1, p2, p3, p4); \
}
#define MALI_EGL_APIENTRY_i4(a,t1,p1,t2,p2,t3,p3,t4,p4) \
	a(t1 p1,t2 p2,t3 p3,t4 p4) \
{ \
	extern EGLint MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4); \
	return MALI_MANGLE_NAME(a)( p1, p2, p3, p4); \
}
#define MALI_EGL_APIENTRY_IKR5(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5) \
	a(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5) \
{ \
	extern EGLImageKHR MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5); \
	return MALI_MANGLE_NAME(a)( p1, p2, p3, p4, p5); \
}
#define MALI_EGL_APIENTRY_b5(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5) \
	a(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5) \
{ \
	extern EGLBoolean MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5); \
	return MALI_MANGLE_NAME(a)( p1, p2, p3, p4, p5); \
}
#define MALI_EGL_APIENTRY_ES5(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5) \
	a(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5) \
{ \
	extern EGLSurface MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5); \
	return MALI_MANGLE_NAME(a)( p1, p2, p3, p4, p5); \
}

#define MALI_EGL_APIENTRY_INKR4(a,t1,p1,t2,p2,t3,p3,t4,p4) \
	a(t1 p1,t2 p2,t3 p3,t4 p4) \
{ \
	extern EGLImageNameKHR MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4); \
	return MALI_MANGLE_NAME(a)( p1, p2, p3, p4); \
}
#define MALI_EGL_APIENTRY_EC4(a,t1,p1,t2,p2,t3,p3,t4,p4) \
	a(t1 p1,t2 p2,t3 p3,t4 p4) \
{ \
	extern EGLContext MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4); \
	return MALI_MANGLE_NAME(a)( p1, p2, p3, p4); \
}
#define MALI_EGL_APIENTRY_ES4(a,t1,p1,t2,p2,t3,p3,t4,p4) \
	a(t1 p1,t2 p2,t3 p3,t4 p4) \
{ \
	extern EGLSurface MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4); \
	return MALI_MANGLE_NAME(a)( p1, p2, p3, p4); \
}
#define MALI_EGL_APIENTRY_ES3(a,t1,p1,t2,p2,t3,p3) \
	a(t1 p1,t2 p2,t3 p3) \
{ \
	extern EGLSurface MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3); \
	return MALI_MANGLE_NAME(a)( p1, p2, p3); \
}

#define MALI_EGL_APIENTRY_SKR3(a,t1,p1,t2,p2,t3,p3) \
	a(t1 p1,t2 p2,t3 p3) \
{ \
	extern EGLSyncKHR MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3); \
	return MALI_MANGLE_NAME(a)( p1, p2, p3); \
}

#define MALI_EGL_APIENTRY_ES0(a,t1) \
	a(t1) \
{ \
	extern EGLSurface MALI_MANGLE_NAME(a)(); \
	return MALI_MANGLE_NAME(a)(); \
}
#define MALI_EGL_APIENTRY_CNKR1(a,t1,p1) \
	a(t1 p1) \
{ \
	extern EGLClientNameKHR MALI_MANGLE_NAME(a)(t1 p1); \
	return MALI_MANGLE_NAME(a)(p1); \
}
#define MALI_EGL_APIENTRY_ES1(a,t1,p1) \
	a(t1 p1) \
{ \
	extern EGLSurface MALI_MANGLE_NAME(a)(t1 p1); \
	return MALI_MANGLE_NAME(a)(p1); \
}

#define MALI_EGL_APIENTRY_ED0(a,t1) \
	a(t1) \
{ \
	extern EGLDisplay MALI_MANGLE_NAME(a)(); \
	return MALI_MANGLE_NAME(a)(); \
}
#define MALI_EGL_APIENTRY_ED1(a,t1,p1) \
	a(t1 p1) \
{ \
	extern EGLDisplay MALI_MANGLE_NAME(a)(t1 p1); \
	return MALI_MANGLE_NAME(a)(p1); \
}

#define MALI_EGL_APIENTRY_v2(a,t1,p1,t2,p2) \
	a(t1 p1, t2 p2) \
{ \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2); \
	MALI_MANGLE_NAME(a)(p1,p2); \
}
#define MALI_EGL_APIENTRY_bi2(a,t1,p1,t2,p2) \
	a(t1 p1, t2 p2) \
{ \
	extern GLbitfield MALI_MANGLE_NAME(a)(t1 p1,t2 p2); \
	return MALI_MANGLE_NAME(a)(p1,p2); \
}
#define MALI_EGL_APIENTRY_i2(a,t1,p1,t2,p2) \
	a(t1 p1, t2 p2) \
{ \
	extern GLint MALI_MANGLE_NAME(a)(t1 p1,t2 p2); \
	return MALI_MANGLE_NAME(a)(p1,p2); \
}
#define MALI_EGL_APIENTRY_v3(a,t1,p1,t2,p2,t3,p3) \
	a(t1 p1,t2 p2,t3 p3) \
{ \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3); \
	MALI_MANGLE_NAME(a)(p1,p2,p3); \
}
#define MALI_EGL_APIENTRY_v4(a,t1,p1,t2,p2,t3,p3,t4,p4) \
	a(t1 p1,t2 p2,t3 p3,t4 p4) \
{ \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4); \
	MALI_MANGLE_NAME(a)(p1,p2,p3,p4); \
}
#define MALI_EGL_APIENTRY_v5(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5) \
	a(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5) \
{ \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5); \
	MALI_MANGLE_NAME(a)(p1,p2,p3,p4,p5); \
}
#define MALI_EGL_APIENTRY_EPT1(a,t1,p1) \
	a(t1 p1) \
{ \
	extern __eglMustCastToProperFunctionPointerType MALI_MANGLE_NAME(a)(t1 p1); \
	return MALI_MANGLE_NAME(a)(p1); \
}

typedef void         * maliVoidPtr;
typedef const EGLint * maliEGLConstIntPtr;
typedef EGLint       * maliEGLIntPtr;
typedef EGLConfig    * maliEGLConfigPtr;

#endif /* MALI_BUILD_ANDROID_MONOLITHIC */

#endif /* _EGL_SHIMS_COMMON_H */
