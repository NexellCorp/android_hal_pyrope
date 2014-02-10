/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_api_trace.h
 * @brief Helper functions common for both for OpenGL ES 1.1 and 2.0 API tracing
 */

#ifndef _GLES_API_TRACE_H_
#define _GLES_API_TRACE_H_

#include "instrumented/apitrace/mali_trace.h"

#if MALI_API_TRACE

#include "gles_base.h"
#include "gles_context.h"

/**
 * Calculates the size of the input/output buffer for glTexParameter* and glGetTexParameter*
 * @param pname Type parameter passed to glTexParameter* and glGetTexParameter*
 * @return The number of input/output components
 */
u32 mali_api_trace_gl_texparameter_count(GLenum pname);

/**
 * Dump the needed user buffers required by the current glDrawArrays call
 * @param ctx Active GLES context
 * @param nvert Number of vertices given to glDrawArrays
 */
void mali_api_trace_gl_draw_arrays(struct gles_context *ctx, u32 nvert);

/**
 * Dump the needed user buffers required by the current glDrawElements call
 * @param ctx Active GLES context
 * @param count Number of indices given to glDrawElements
 * @param type The format of the indices array
 * @param indices The indices array (or offset if VBO is used)
 */
void mali_api_trace_gl_draw_elements(struct gles_context *ctx, GLenum mode, int count, GLenum type, const void *indices);

/**
 * Calculates the size requirement of the output buffer for getter functions like glGetFloatv.
 * @param pname The pname parameter to the GL getter function
 * @return The size (number of elements) that the given pname will return
 */
u32 mali_api_trace_get_getter_out_count(gles_context *ctx, GLenum pname);

/**
 * Calculates the size requirement of the output buffer for glReadPixels
 * @param ctx Active GLES context
 * @param width Width parameter passed to glReadPixels
 * @param height Height parameter passed to glReadPixels
 * @param format Format parameter passed to glReadPixels
 * @param type Type parameter passed to glReadPixels
 * @return The required buffer size
 */
u32 mali_api_trace_gl_read_pixels_size(struct gles_context *ctx, int width, int height, GLenum format, GLenum type);

/**
 * Calculates the size of the input buffer for glTexImage2D and glTexSubImage2D
 * @param ctx Active GLES context
 * @param width Width parameter passed to glReadPixels
 * @param height Height parameter passed to glReadPixels
 * @param format Format parameter passed to glReadPixels
 * @param type Type parameter passed to glReadPixels
 * @return The required buffer size
 */
u32 mali_api_trace_gl_teximage2d_size(struct gles_context *ctx, int width, int height, GLenum format, GLenum type);

#define MALI_API_TRACE_PARAM_GLUINT(value) mali_api_trace_param_unsigned_integer(value, "gluint")
#define MALI_API_TRACE_PARAM_GLINT(value) mali_api_trace_param_signed_integer(value, "glint")
#define MALI_API_TRACE_PARAM_GLBOOLEAN(value) mali_api_trace_param_unsigned_integer(value, "glboolean")
#define MALI_API_TRACE_PARAM_GLCLAMPF(value) mali_api_trace_param_float(value, "glclampf")
#define MALI_API_TRACE_PARAM_GLCLAMPX(value) mali_api_trace_param_signed_integer(value, "glclampx")
#define MALI_API_TRACE_PARAM_GLUBYTE(value) mali_api_trace_param_unsigned_integer(value, "glubyte")
#define MALI_API_TRACE_PARAM_GLBYTE(value) mali_api_trace_param_signed_integer(value, "glbyte")
#define MALI_API_TRACE_PARAM_GLSHORT(value) mali_api_trace_param_signed_integer(value, "glshort")
#define MALI_API_TRACE_PARAM_GLFIXED(value) mali_api_trace_param_signed_integer(value, "glfixed")
#define MALI_API_TRACE_PARAM_GLFLOAT(value) mali_api_trace_param_float(value, "glfloat")
#define MALI_API_TRACE_PARAM_GLENUM(value) mali_api_trace_param_unsigned_integer(value, "glenum")
#define MALI_API_TRACE_PARAM_GLSIZEI(value) mali_api_trace_param_signed_integer(value, "glsizei")
#define MALI_API_TRACE_PARAM_GLINTPTR(value) mali_api_trace_param_signed_integer(value, "glintptr")
#define MALI_API_TRACE_PARAM_GLSIZEIPTR(value) mali_api_trace_param_signed_integer(value, "glsizeiptr")
#define MALI_API_TRACE_PARAM_GLBITFIELD(value) mali_api_trace_param_unsigned_integer(value, "glbitfield")
#define MALI_API_TRACE_PARAM_INDEXPOINTER(value) mali_api_trace_param_unsigned_integer((u32)value, "indexpointer")
#define MALI_API_TRACE_PARAM_GLEGLIMAGEOES(value) mali_api_trace_param_unsigned_integer((u32)(value), "gleglimageoes")
#define MALI_API_TRACE_PARAM_VERTEXATTRIBARRAYPOINTER(value) mali_api_trace_param_unsigned_integer((u32)(value), "vertexattribarraypointer");

#define MALI_API_TRACE_PARAM_GLUBYTE_ARRAY(dir, ptr, count, offset) mali_api_trace_param_uint8_array(dir, ptr, count, offset, "glubytearray")
#define MALI_API_TRACE_PARAM_GLSHORT_ARRAY(dir, ptr, count) mali_api_trace_param_int16_array(dir, ptr, count, "glshortarray")
#define MALI_API_TRACE_PARAM_GLINT_ARRAY(dir, ptr, count) mali_api_trace_param_int32_array(dir, ptr, count, "glintarray")
#define MALI_API_TRACE_PARAM_GLUINT_ARRAY(dir, ptr, count) mali_api_trace_param_uint32_array(dir, ptr, count, "gluintarray")
#define MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(dir, ptr, count) mali_api_trace_param_float_array(dir, ptr, count, "glfloatarray")
#define MALI_API_TRACE_PARAM_GLENUM_ARRAY(dir, ptr, count) mali_api_trace_param_uint32_array(dir, ptr, count, "glenumarray")
#define MALI_API_TRACE_PARAM_GLSIZEI_ARRAY(dir, ptr, count) mali_api_trace_param_int32_array(dir, ptr, count, "glsizeiarray")
#define MALI_API_TRACE_PARAM_GLBOOLEAN_ARRAY(dir, ptr, count, offset) mali_api_trace_param_uint8_array(dir, ptr, count, offset, "glbooleanarray")
#define MALI_API_TRACE_PARAM_GLFIXED_ARRAY(dir, ptr, count) mali_api_trace_param_int32_array(dir, ptr, count, "glfixedarray")
#define MALI_API_TRACE_PARAM_VERTEXATTRIBARRAYPOINTER_ARRAY(dir, ptr, count) mali_api_trace_param_uint32_array(dir, (u32*)(ptr), count, "vertexattribarraypointerarray");

#define MALI_API_TRACE_RETURN_GLINT(value) mali_api_trace_return_signed_integer(value, "glint")
#define MALI_API_TRACE_RETURN_GLENUM(value) mali_api_trace_return_unsigned_integer(value, "glenum")
#define MALI_API_TRACE_RETURN_GLBOOLEAN(value) mali_api_trace_return_unsigned_integer(value, "glboolean")
#define MALI_API_TRACE_RETURN_GLBITFIELD(value) mali_api_trace_return_unsigned_integer(value, "glbitfield")

#define MALI_API_TRACE_RETURN_GLUBYTE_ARRAY(id, ptr, count) mali_api_trace_return_uint8_array(id, ptr, count, "glubytearray")
#define MALI_API_TRACE_RETURN_GLINT_ARRAY(id, ptr, count) mali_api_trace_return_int32_array(id, ptr, count, "glintarray")
#define MALI_API_TRACE_RETURN_GLUINT_ARRAY(id, ptr, count) mali_api_trace_return_uint32_array(id, ptr, count, "gluintarray")
#define MALI_API_TRACE_RETURN_GLFLOAT_ARRAY(id, ptr, count) mali_api_trace_return_float_array(id, ptr, count, "glfloatarray")
#define MALI_API_TRACE_RETURN_GLSIZEI_ARRAY(id, ptr, count) mali_api_trace_return_int32_array(id, ptr, count, "glsizeiarray")
#define MALI_API_TRACE_RETURN_GLENUM_ARRAY(id, ptr, count) mali_api_trace_return_uint32_array(id, ptr, count, "glenumarray")
#define MALI_API_TRACE_RETURN_GLBOOLEAN_ARRAY(id, ptr, count) mali_api_trace_return_uint8_array(id, ptr, count, "glbooleanarray")
#define MALI_API_TRACE_RETURN_GLFIXED_ARRAY(id, ptr, count) mali_api_trace_return_int32_array(id, ptr, count, "glfixedarray")
#define MALI_API_TRACE_RETURN_VERTEXATTRIBARRAYPOINTER_ARRAY(id, ptr, count) mali_api_trace_return_uint32_array(id, (u32*)(ptr), count, "vertexattribarraypointerarray");

#define MALI_API_TRACE_GL_DRAW_ARRAYS(ctx, nvert) mali_api_trace_gl_draw_arrays(ctx, nvert)
#define MALI_API_TRACE_GL_DRAW_ELEMENTS(ctx, mode, count, type, indices) mali_api_trace_gl_draw_elements(ctx, mode, count, type, indices)

#else

/* No API tracing enabled, so use empty macroes */

#define MALI_API_TRACE_PARAM_GLUINT(value)
#define MALI_API_TRACE_PARAM_GLINT(value)
#define MALI_API_TRACE_PARAM_GLBOOLEAN(value)
#define MALI_API_TRACE_PARAM_GLCLAMPF(value)
#define MALI_API_TRACE_PARAM_GLCLAMPX(value)
#define MALI_API_TRACE_PARAM_GLUBYTE(value)
#define MALI_API_TRACE_PARAM_GLBYTE(value)
#define MALI_API_TRACE_PARAM_GLSHORT(value)
#define MALI_API_TRACE_PARAM_GLFIXED(value)
#define MALI_API_TRACE_PARAM_GLFLOAT(value)
#define MALI_API_TRACE_PARAM_GLENUM(value)
#define MALI_API_TRACE_PARAM_GLSIZEI(value)
#define MALI_API_TRACE_PARAM_GLINTPTR(value)
#define MALI_API_TRACE_PARAM_GLSIZEIPTR(value)
#define MALI_API_TRACE_PARAM_GLBITFIELD(value)
#define MALI_API_TRACE_PARAM_INDEXPOINTER(value)
#define MALI_API_TRACE_PARAM_GLEGLIMAGEOES(value)
#define MALI_API_TRACE_PARAM_VERTEXATTRIBARRAYPOINTER(value)

#define MALI_API_TRACE_PARAM_GLUBYTE_ARRAY(dir, ptr, count, offset)
#define MALI_API_TRACE_PARAM_GLSHORT_ARRAY(dir, ptr, count)
#define MALI_API_TRACE_PARAM_GLINT_ARRAY(dir, ptr, count)
#define MALI_API_TRACE_PARAM_GLUINT_ARRAY(dir, ptr, count)
#define MALI_API_TRACE_PARAM_GLFLOAT_ARRAY(dir, ptr, count)
#define MALI_API_TRACE_PARAM_GLENUM_ARRAY(dir, ptr, count)
#define MALI_API_TRACE_PARAM_GLSIZEI_ARRAY(dir, ptr, count)
#define MALI_API_TRACE_PARAM_GLBOOLEAN_ARRAY(dir, ptr, count, offset)
#define MALI_API_TRACE_PARAM_GLFIXED_ARRAY(dir, ptr, count)
#define MALI_API_TRACE_PARAM_VERTEXATTRIBARRAYPOINTER_ARRAY(dir, ptr, count)

#define MALI_API_TRACE_RETURN_GLINT(value)
#define MALI_API_TRACE_RETURN_GLENUM(value)
#define MALI_API_TRACE_RETURN_GLBOOLEAN(value)
#define MALI_API_TRACE_RETURN_GLBITFIELD(value)

#define MALI_API_TRACE_RETURN_GLUBYTE_ARRAY(id, ptr, count)
#define MALI_API_TRACE_RETURN_GLINT_ARRAY(id, ptr, count)
#define MALI_API_TRACE_RETURN_GLUINT_ARRAY(id, ptr, count)
#define MALI_API_TRACE_RETURN_GLFLOAT_ARRAY(id, ptr, count)
#define MALI_API_TRACE_RETURN_GLSIZEI_ARRAY(id, ptr, count)
#define MALI_API_TRACE_RETURN_GLENUM_ARRAY(id, ptr, count)
#define MALI_API_TRACE_RETURN_GLBOOLEAN_ARRAY(id, ptr, count)
#define MALI_API_TRACE_RETURN_GLFIXED_ARRAY(id, ptr, count)
#define MALI_API_TRACE_RETURN_VERTEXATTRIBARRAYPOINTER_ARRAY(id, ptr, count)

#define MALI_API_TRACE_GL_DRAW_ARRAYS(ctx, nvert)
#define MALI_API_TRACE_GL_DRAW_ELEMENTS(ctx, mode, count, type, indices)

#endif /* MALI_API_TRACE */

#endif /* _GLES_API_TRACE_H_ */
