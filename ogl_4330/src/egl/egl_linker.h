/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef _EGL_LINKER_H_
#define _EGL_LINKER_H_

/**
 * @file egl_linker.h
 * @brief Inter client API linker functionality.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef EGL_MALI_GLES
#include "api_interface_gles.h"
#endif

#ifdef EGL_MALI_VG
#include "api_interface_vg.h"
#endif

/* EGL Linker Capability Bits */
#define _EGL_LINKER_NONE        		0x0000
#define _EGL_LINKER_OPENGL_ES_BIT		0x0001
#define _EGL_LINKER_OPENVG_BIT			0x0002
#define _EGL_LINKER_OPENGL_ES2_BIT		0x0004
#define _EGL_LINKER_PLATFORM_BIT		0x0008


/**
 * Internal structure for the linker
 */
typedef struct egl_linker
{
	void *handle_vg;
	void *handle_gles1;
	void *handle_gles2;
	void *handle_shared;
    void *handle_platform;

#ifdef EGL_MALI_GLES
	egl_gles_api_functions   gles_func[2]; /** list of pointers to gles api functions */
#endif

#ifdef EGL_MALI_VG
	egl_vg_api_functions     vg_func;      /** list of pointers to vg api functions */
#endif

    void (* (*platform_get_proc_address)( void *handle_platform, const char *procname ))( void ); /** The platform specific get_proc_address **/

	EGLint caps;
} egl_linker;

/**
 * @brief Initializes the linker
 * @param linker pointer to a valid egl_linker struct
 * @return EGL_TRUE on success, EGL_FALSE else
 * @note memory pointed by linker should be zeroed (or allocate using calloc)  before passing to this function
 */
EGLBoolean egl_linker_init( egl_linker *linker );

/**
 * @brief Deinitializes the linker
 * @param linker pointer to a valid egl_linker struct
 */
void egl_linker_deinit( egl_linker *linker );

#ifdef __cplusplus
}
#endif

#endif /* _EGL_LINKER_H_ */

