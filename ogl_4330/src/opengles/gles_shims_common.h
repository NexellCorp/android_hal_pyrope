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
 * @file gles_entrypoints_shim.h
 * @brief Contains helper macros for the gles*_entrypoints_shim.c files
 *  The macros are used to produce files with thin shims for export  
 */

#ifndef _GLES_SHIMS_COMMON_H_
#define _GLES_SHIMS_COMMON_H_

/* this wraps the whole file so nothing is included without this define */
#ifdef MALI_BUILD_ANDROID_MONOLITHIC

#include "gles_base.h"

#define MALI_MANGLE_NAME(a) shim_##a

#ifndef MALI_GL_APIENTRY
#define MALI_GL_APIENTRY(a) MALI_MANGLE_NAME(a)
#endif

/* For declaring the shim functions */
#define MALI_DECLARE_SHIM_GL_APIENTRY_v0(a,t1) \
	extern void MALI_MANGLE_NAME(a)(t1);
#define MALI_DECLARE_SHIM_GL_APIENTRY_e0(a,t1) \
	extern GLenum MALI_MANGLE_NAME(a)(t1);
#define MALI_DECLARE_SHIM_GL_APIENTRY_ui0(a,t1) \
	extern GLuint MALI_MANGLE_NAME(a)(t1);
#define MALI_DECLARE_SHIM_GL_APIENTRY_v1(a,t1,p1) \
	extern void MALI_MANGLE_NAME(a)(t1 p1);
#define MALI_DECLARE_SHIM_GL_APIENTRY_e1(a,t1,p1) \
	extern GLenum MALI_MANGLE_NAME(a)(t1 p1);
#define MALI_DECLARE_SHIM_GL_APIENTRY_ui1(a,t1,p1) \
	extern GLuint MALI_MANGLE_NAME(a)(t1 p1);
#define MALI_DECLARE_SHIM_GL_APIENTRY_b1(a,t1,p1) \
	extern GLboolean MALI_MANGLE_NAME(a)(t1 p1);
#define MALI_DECLARE_SHIM_GL_APIENTRY_ubp1(a,t1,p1) \
	extern const GLubyte * MALI_MANGLE_NAME(a)(t1 p1);
#define MALI_DECLARE_SHIM_GL_APIENTRY_v2(a,t1,p1,t2,p2) \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2);
#define MALI_DECLARE_SHIM_GL_APIENTRY_bi2(a,t1,p1,t2,p2) \
	extern GLbitfield MALI_MANGLE_NAME(a)(t1 p1,t2 p2);
#define MALI_DECLARE_SHIM_GL_APIENTRY_i2(a,t1,p1,t2,p2) \
	extern GLint MALI_MANGLE_NAME(a)(t1 p1,t2 p2);
#define MALI_DECLARE_SHIM_GL_APIENTRY_v3(a,t1,p1,t2,p2,t3,p3) \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3);
#define MALI_DECLARE_SHIM_GL_APIENTRY_v4(a,t1,p1,t2,p2,t3,p3,t4,p4) \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4);
#define MALI_DECLARE_SHIM_GL_APIENTRY_v5(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5) \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5);
#define MALI_DECLARE_SHIM_GL_APIENTRY_v6(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5,t6,p6) \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5,t6 p6);
#define MALI_DECLARE_SHIM_GL_APIENTRY_v7(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5,t6,p6,t7,p7) \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5,t6 p6,t7 p7);
#define MALI_DECLARE_SHIM_GL_APIENTRY_v8(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5,t6,p6,t7,p7,t8,p8) \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5,t6 p6,t7 p7,t8 p8);
#define MALI_DECLARE_SHIM_GL_APIENTRY_v9(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5,t6,p6,t7,p7,t8,p8,t9,p9) \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5,t6 p6,t7 p7,t8 p8,t9 p9);


/* For implementing the shim functions */
#define MALI_GL_APIENTRY_v0(a,t1) \
	a(t1) \
{ \
	extern void MALI_MANGLE_NAME(a)(t1); \
	MALI_MANGLE_NAME(a)(); \
}

#define MALI_GL_APIENTRY_e0(a,t1) \
	a(t1) \
{ \
	extern GLenum MALI_MANGLE_NAME(a)(t1); \
	return MALI_MANGLE_NAME(a)(); \
}
#define MALI_GL_APIENTRY_ui0(a,t1) \
	a(t1) \
{ \
	extern GLuint MALI_MANGLE_NAME(a)(t1); \
	return MALI_MANGLE_NAME(a)(); \
}

#define MALI_GL_APIENTRY_v1(a,t1,p1) \
	a(t1 p1) \
{ \
	extern void MALI_MANGLE_NAME(a)(t1 p1); \
	MALI_MANGLE_NAME(a)(p1); \
}

#define MALI_GL_APIENTRY_e1(a,t1,p1) \
	a(t1 p1) \
{ \
	extern GLenum MALI_MANGLE_NAME(a)(t1 p1); \
	return MALI_MANGLE_NAME(a)(p1); \
}

#define MALI_GL_APIENTRY_ui1(a,t1,p1) \
	a(t1 p1) \
{ \
	extern GLuint MALI_MANGLE_NAME(a)(t1 p1); \
	return MALI_MANGLE_NAME(a)(p1); \
}

#define MALI_GL_APIENTRY_b1(a,t1,p1) \
	a(t1 p1) \
{ \
	extern GLboolean MALI_MANGLE_NAME(a)(t1 p1); \
	return MALI_MANGLE_NAME(a)(p1); \
}

#define MALI_GL_APIENTRY_ubp1(a,t1,p1) \
	a(t1 p1) \
{ \
	extern const GLubyte * MALI_MANGLE_NAME(a)(t1 p1); \
	return MALI_MANGLE_NAME(a)(p1); \
}

#define MALI_GL_APIENTRY_v2(a,t1,p1,t2,p2) \
	a(t1 p1, t2 p2) \
{ \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2); \
	MALI_MANGLE_NAME(a)(p1,p2); \
}
#define MALI_GL_APIENTRY_bi2(a,t1,p1,t2,p2) \
	a(t1 p1, t2 p2) \
{ \
	extern GLbitfield MALI_MANGLE_NAME(a)(t1 p1,t2 p2); \
	return MALI_MANGLE_NAME(a)(p1,p2); \
}
#define MALI_GL_APIENTRY_i2(a,t1,p1,t2,p2) \
	a(t1 p1, t2 p2) \
{ \
	extern GLint MALI_MANGLE_NAME(a)(t1 p1,t2 p2); \
	return MALI_MANGLE_NAME(a)(p1,p2); \
}
#define MALI_GL_APIENTRY_v3(a,t1,p1,t2,p2,t3,p3) \
	a(t1 p1,t2 p2,t3 p3) \
{ \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3); \
	MALI_MANGLE_NAME(a)(p1,p2,p3); \
}
#define MALI_GL_APIENTRY_v4(a,t1,p1,t2,p2,t3,p3,t4,p4) \
	a(t1 p1,t2 p2,t3 p3,t4 p4) \
{ \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4); \
	MALI_MANGLE_NAME(a)(p1,p2,p3,p4); \
}
#define MALI_GL_APIENTRY_v5(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5) \
	a(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5) \
{ \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5); \
	MALI_MANGLE_NAME(a)(p1,p2,p3,p4,p5); \
}
#define MALI_GL_APIENTRY_v6(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5,t6,p6) \
	a(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5,t6 p6) \
{ \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5,t6 p6); \
	MALI_MANGLE_NAME(a)(p1,p2,p3,p4,p5,p6); \
}
#define MALI_GL_APIENTRY_v7(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5,t6,p6,t7,p7) \
	a(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5,t6 p6,t7 p7) \
{ \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5,t6 p6,t7 p7); \
	MALI_MANGLE_NAME(a)(p1,p2,p3,p4,p5,p6,p7); \
}
#define MALI_GL_APIENTRY_v8(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5,t6,p6,t7,p7,t8,p8) \
	a(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5,t6 p6,t7 p7,t8 p8) \
{ \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5,t6 p6,t7 p7,t8 p8); \
	MALI_MANGLE_NAME(a)(p1,p2,p3,p4,p5,p6,p7,p8); \
}

#define MALI_GL_APIENTRY_v9(a,t1,p1,t2,p2,t3,p3,t4,p4,t5,p5,t6,p6,t7,p7,t8,p8,t9,p9) \
	a(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5,t6 p6,t7 p7,t8 p8,t9 p9) \
{ \
	extern void MALI_MANGLE_NAME(a)(t1 p1,t2 p2,t3 p3,t4 p4,t5 p5,t6 p6,t7 p7,t8 p8,t9 p9); \
	MALI_MANGLE_NAME(a)(p1,p2,p3,p4,p5,p6,p7,p8,p9); \
}

typedef  const	void		* maliConstVoidPtr;
typedef			void		** maliVoidPtrPtr;
typedef			void		* maliVoidPtr;
typedef  const	GLvoid		* maliGLConstVoidPtr;
typedef			GLvoid		* maliGLVoidPtr;
typedef			GLvoid		** maliGLVoidPtrPtr;
typedef  const	GLuint		* maliGLConstUintPtr;
typedef			GLuint		* maliGLUintPtr;
typedef  const	GLint		* maliGLConstIntPtr;
typedef			GLint		* maliGLIntPtr;
typedef  const	GLfloat		* maliGLConstFloatPtr;
typedef			GLfloat		* maliGLFloatPtr;
typedef			GLboolean	* maliGLBooleanPtr;
typedef const	GLubyte		* maliGLConstUbytePtr;
typedef const	char		* maliConstCharPtr;
typedef const	char		** maliConstCharPtrPtr;
typedef			char		* maliCharPtr;
typedef const	GLfixed		* maliGLConstFixedPtr;
typedef			GLfixed		* maliGLFixedPtr;
typedef const	GLshort		* maliGLConstShortPtr;
typedef			GLshort		* maliGLShortPtr;
typedef const	GLsizei		* maliGLConstSizeiPtr;
typedef			GLsizei		* maliGLSizeiPtr;
typedef const	GLenum		* maliGLConstEnumPtr;
typedef			GLenum		* maliGLEnumPtr;

#endif /* MALI_BUILD_ANDROID_MONOLITHIC */

#endif /* _GLES_SHIMS_COMMON_H_ */
