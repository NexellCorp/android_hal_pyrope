/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles1_api_trace.h
 * @brief Helper functions for OpenGL ES 1.1 API tracing
 */

#ifndef _GLES1_API_TRACE_H_
#define _GLES1_API_TRACE_H_

#include "gles_api_trace.h"

#if MALI_API_TRACE

/**
 * Calculates the size of the input/output buffer for glLight* and glGetLight*
 * @param pname Type parameter passed to glLight* and glGetLight*
 * @return The number of output components
 */
u32 mali_api_trace_gl_light_count(GLenum pname);

/**
 * Calculates the size of the input buffer for glLightModel*
 * @param pname Type parameter passed to glLightModel*
 * @return The number of output components
 */
u32 mali_api_trace_gl_lightmodel_count(GLenum pname);

/**
 * Calculates the size of the input/output buffer for glMaterial* and glGetMaterial*
 * @param pname Type parameter passed to glMaterial* and glGetMaterial*
 * @return The number of output components
 */
u32 mali_api_trace_gl_material_count(GLenum pname);

/**
 * Calculates the size of the input/output buffer for glTexEnv* and glGetTexEnv*
 * @param pname Type parameter passed to glTexEnv* and glGetMaterial*
 * @return The number of output components
 */
u32 mali_api_trace_gl_texenv_count(GLenum pname);

/**
 * Calculates the size of the input buffer for glPointParameter*
 * @param pname Type parameter passed to glGetMaterial*
 * @return The number of output components
 */
u32 mali_api_trace_gl_pointparameter_count(GLenum pname);

#endif /* MALI_API_TRACE */

#endif /* _GLES1_API_TRACE_H_ */
