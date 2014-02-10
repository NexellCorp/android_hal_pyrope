/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_common.h
 * @brief Common include files for framebuilder and client APIs.
 */

#ifndef _EGL_COMMON_H_
#define _EGL_COMMON_H_

#include <EGL/egl.h>
#include <shared/m200_gp_frame_builder.h>

#ifdef EGL_MALI_VG
#include "egl_vg.h"
#endif

#ifdef EGL_MALI_GLES
#include "egl_gles.h"
#endif

#endif /* _EGL_COMMON_H_ */
