/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef _EGL_FRAMEWORK_
#define _EGL_FRAMEWORK_

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

/*
 * NOTE: the tests should actually include egl_helpers.h.
 * This will then in turn include egl_framework_actual.h.
 * This was done to fix a recursive inclusion issue. If this file must be removed, the test source files must
 * be changed to include egl_helpers.h (and not egl_framework_actual.h, as this will cause an include cycle).
 */
#include "egl_helpers.h"

#endif /* _EGL_FRAMEWORK_ */
