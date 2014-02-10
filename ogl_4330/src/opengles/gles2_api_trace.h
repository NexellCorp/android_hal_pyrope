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
 * @file gles2_api_trace.h
 * @brief Helper functions for OpenGL ES 2.0 API tracing
 */

#ifndef _GLES2_API_TRACE_H_
#define _GLES2_API_TRACE_H_

#include "gles_api_trace.h"

#if MALI_API_TRACE

#include "gles_base.h"
#include "shared/mali_named_list.h"

/**
 * Returns the size of the specified uniform (vector)
 * Used by glGetUniformfv/glGetUniformiv.
 * @param program_object_list Project object list, found in context
 * @param program Shader program ID
 * @param location Uniform location
 * @return The size of the specified uniform
 */
u32 mali_api_trace_get_uniform_size(mali_named_list* program_object_list, GLuint program, GLint location);

#define MALI_API_TRACE_PARAM_PROGRAMID(value) mali_api_trace_param_unsigned_integer(value, "programid")
#define MALI_API_TRACE_PARAM_SHADERID(value) mali_api_trace_param_unsigned_integer(value, "shaderid")

#define MALI_API_TRACE_PARAM_SHADERID_ARRAY(dir, ptr, count) mali_api_trace_param_uint32_array(dir, ptr, count, "shaderidarray")

#define MALI_API_TRACE_RETURN_PROGRAMID(value) mali_api_trace_return_unsigned_integer(value, "programid")
#define MALI_API_TRACE_RETURN_SHADERID(value) mali_api_trace_return_unsigned_integer(value, "shaderid")

#define MALI_API_TRACE_RETURN_SHADERID_ARRAY(id, ptr, count) mali_api_trace_return_uint32_array(id, ptr, count, "shaderidarray")

#else

/* No API tracing enabled, so use empty macroes */

#define MALI_API_TRACE_PARAM_PROGRAMID(value)
#define MALI_API_TRACE_PARAM_SHADERID(value)

#define MALI_API_TRACE_PARAM_SHADERID_ARRAY(dir, ptr, count)

#define MALI_API_TRACE_RETURN_PROGRAMID(value)
#define MALI_API_TRACE_RETURN_SHADERID(value)

#define MALI_API_TRACE_RETURN_SHADERID_ARRAY(id, ptr, count)

#endif /* MALI2_API_TRACE */

#endif /* _GLES_API_TRACE_H_ */
