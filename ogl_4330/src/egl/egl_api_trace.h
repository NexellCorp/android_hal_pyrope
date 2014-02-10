/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_api_trace.h
 * @brief Helper functions for EGL API tracing
 */

#ifndef _EGL_API_TRACE_H_
#define _EGL_API_TRACE_H_

#include "instrumented/apitrace/mali_trace.h"

#if MALI_API_TRACE

#include <EGL/egl.h>
#include <EGL/eglext.h>

/**
 * Add a EGLNativeWindowType to the trace output.
 * This is not added as a parameter, because this is not always given as input,
 * and the content of this type is not controlled by us.
 * E.g. When a Window surface is resized, we will detect this in eglSwapBuffers,
 * and this function will add the new values to the trace output.
 * @param win The EGLNativeWindowType to add to the trace output
 */
void mali_api_trace_add_eglnativewindowtype(EGLNativeWindowType win);

/**
 * Add a parameter with type EGLNativeWindowType to the trace output
 * @param value The EGLNativeWindowType value
 */
void mali_api_trace_param_eglnativewindowtype(EGLNativeWindowType win);

/**
 * Add a parameter with type EGLNativePixmapType to the trace output
 * @param value The EGLNativePixmapType value
 */
void mali_api_trace_param_eglnativepixmaptype(EGLNativePixmapType pixmap);

/**
 * Add pixmap data for a EGLNativePixmapType to the trace output.
 * Pixmaps are updated outside the control of the APIs, so this
 * data is grabbed when a native pixmap surface is created and eglWaitNative()
 * and eglCreateImageKHR() is called.
 * @param win The EGLNativePixmapType pixmap data to add to the trace output
 */
void mali_api_trace_add_pixmap_data(EGLNativePixmapType pixmap);

/**
 * Add a parameter with type EGLClientBuffer to the trace output, as given to eglCreatePbufferFromClientBuffer
 * @param value The EGLClientBuffer value
 * @param type The actual type of EGLClientBuffer (VGimage, pixmap, gles texture etc)
 */
void mali_api_trace_param_eglclientbuffer_eglcreatepbufferfromclientbuffer(EGLClientBuffer value, EGLenum type);

/**
 * Add a parameter with type EGLClientBuffer to the trace output, as given to eglCreateImageKHR
 * @param value The EGLClientBuffer value
 * @param type The actual type of EGLClientBuffer (VGimage, pixmap, gles texture etc)
 */
void mali_api_trace_param_eglclientbuffer_eglcreateimagekhr(EGLClientBuffer value, EGLenum type);

/**
 * Add a parameter with type EGLint array to the trace output,
 * where we might have some EGLNativePixmapType in the array (EGL_MATCH_NATIVE_PIXMAP).
 * This must therefore be handled more specially then just a normal EGLint array.
 * @param ptr The EGLNativeWindowType value
 * @param size The number of elements pointed to by ptr
 */
void mali_api_trace_param_attrib_array(const EGLint* ptr, EGLint size);

/**
 * Find out the number of elements in a attribute list (EGL_NONE terminates list)
 * @param attrib_list Pointer to the attribute list to find the size of
 * @return The number of elements int attrib_list
 */
u32 mali_api_trace_attrib_size(const EGLint *attrib_list);

/**
 * Check if eglQuerySurface string actually did return a value or not
 * This is not as simple as checking the return value, cos even though
 * this function returns EGL_TRUE, it might still in some cases not touch
 * the output value parameter!
 * @param dpy EGLDisplay, as passed to eglQueryString
 * @param surface EGLSurface, as passed to eglQueryString
 * @param attribute Attribute parameter, as passed to eglQueryString
 * @param thread_state Thread state, as obtaied in eglQueryString
 * @return MALI_TRUE if a value is returned, otherwise MALI_FALSE
 */
mali_bool mali_api_trace_query_surface_has_return_data(EGLDisplay dpy, EGLSurface surface, EGLint attribute, void *thread_state);

#define MALI_API_TRACE_PARAM_EGLINT(value) mali_api_trace_param_signed_integer(value, "eglint")
#define MALI_API_TRACE_PARAM_EGLENUM(value) mali_api_trace_param_signed_integer(value, "eglenum")
#define MALI_API_TRACE_PARAM_EGLDISPLAY(value) mali_api_trace_param_unsigned_integer((u32)value, "egldisplay")
#define MALI_API_TRACE_PARAM_EGLCONTEXT(value) mali_api_trace_param_unsigned_integer((u32)value, "eglcontext")
#define MALI_API_TRACE_PARAM_EGLSURFACE(value) mali_api_trace_param_unsigned_integer((u32)value, "eglsurface")
#define MALI_API_TRACE_PARAM_EGLCONFIG(value) mali_api_trace_param_unsigned_integer((u32)value, "eglconfig")
#define MALI_API_TRACE_PARAM_EGLCLIENTBUFFER_EGLCREATEPBUFFERFROMCLIENTBUFFER(value, type) mali_api_trace_param_eglclientbuffer_eglcreatepbufferfromclientbuffer(value, type)
#define MALI_API_TRACE_PARAM_EGLCLIENTBUFFER_EGLCREATEIMAGEKHR(value, type) mali_api_trace_param_eglclientbuffer_eglcreateimagekhr(value, type)
#define MALI_API_TRACE_PARAM_EGLIMAGEKHR(value) mali_api_trace_param_unsigned_integer((u32)value, "eglimagekhr")
#define MALI_API_TRACE_PARAM_EGLNATIVEWINDOWTYPE(value) mali_api_trace_param_eglnativewindowtype(value)
#define MALI_API_TRACE_PARAM_EGLNATIVEPIXMAPTYPE(value) mali_api_trace_param_eglnativepixmaptype(value)
#define MALI_API_TRACE_PARAM_EGLNATIVEDISPLAYTYPE(value) mali_api_trace_param_unsigned_integer((u32)value, "eglnativedisplaytype")

#define MALI_API_TRACE_PARAM_ATTRIB_ARRAY(ptr, size) mali_api_trace_param_attrib_array(ptr, size)
#define MALI_API_TRACE_PARAM_EGLINT_ARRAY(dir, ptr, size) mali_api_trace_param_int32_array(dir, ptr, size, "eglintarray")
#define MALI_API_TRACE_PARAM_EGLCONFIG_ARRAY(dir, ptr, size) mali_api_trace_param_uint32_array(dir, (u32*)ptr, size, "eglconfigarray");

#define MALI_API_TRACE_RETURN_EGLBOOLEAN(value) mali_api_trace_return_unsigned_integer(value, "eglboolean")
#define MALI_API_TRACE_RETURN_EGLINT(value) mali_api_trace_return_signed_integer(value, "eglint")
#define MALI_API_TRACE_RETURN_EGLENUM(value) mali_api_trace_return_signed_integer(value, "eglenum")
#define MALI_API_TRACE_RETURN_EGLDISPLAY(value) mali_api_trace_return_unsigned_integer((u32)value, "egldisplay")
#define MALI_API_TRACE_RETURN_EGLCONTEXT(value) mali_api_trace_return_unsigned_integer((u32)value, "eglcontext")
#define MALI_API_TRACE_RETURN_EGLSURFACE(value) mali_api_trace_return_unsigned_integer((u32)value, "eglsurface")
#define MALI_API_TRACE_RETURN_EGLIMAGEKHR(value) mali_api_trace_return_unsigned_integer((u32)value, "eglimagekhr")

#define MALI_API_TRACE_RETURN_EGLINT_ARRAY(id, ptr, size) mali_api_trace_return_int32_array(id, ptr, size, "eglintarray")
#define MALI_API_TRACE_RETURN_EGLCONFIG_ARRAY(id, ptr, size) mali_api_trace_return_handle_array(id, (u32*)ptr, size, "eglconfigarray")

#define MALI_API_TRACE_EGLNATIVEWINDOWTYPE_RESIZED(win) mali_api_trace_add_eglnativewindowtype(win)
#define MALI_API_TRACE_ADD_PIXMAP_DATA(pixmap) mali_api_trace_add_pixmap_data(pixmap)

#else

/* No API tracing enabled, so use empty macros */

#define MALI_API_TRACE_PARAM_EGLINT(value)
#define MALI_API_TRACE_PARAM_EGLENUM(value)
#define MALI_API_TRACE_PARAM_EGLDISPLAY(value)
#define MALI_API_TRACE_PARAM_EGLCONTEXT(value)
#define MALI_API_TRACE_PARAM_EGLSURFACE(value)
#define MALI_API_TRACE_PARAM_EGLCONFIG(value)
#define MALI_API_TRACE_PARAM_EGLCLIENTBUFFER_EGLCREATEPBUFFERFROMCLIENTBUFFER(value, type)
#define MALI_API_TRACE_PARAM_EGLCLIENTBUFFER_EGLCREATEIMAGEKHR(value, type)
#define MALI_API_TRACE_PARAM_EGLIMAGEKHR(value)
#define MALI_API_TRACE_PARAM_EGLNATIVEDISPLAYTYPE(value)
#define MALI_API_TRACE_PARAM_EGLNATIVEPIXMAPTYPE(value)
#define MALI_API_TRACE_PARAM_EGLNATIVEWINDOWTYPE(value)

#define MALI_API_TRACE_PARAM_ATTRIB_ARRAY(ptr, size)
#define MALI_API_TRACE_PARAM_EGLINT_ARRAY(dir, ptr, size)
#define MALI_API_TRACE_PARAM_EGLCONFIG_ARRAY(dir, ptr, size)

#define MALI_API_TRACE_RETURN_EGLBOOLEAN(value)
#define MALI_API_TRACE_RETURN_EGLINT(value)
#define MALI_API_TRACE_RETURN_EGLENUM(value)
#define MALI_API_TRACE_RETURN_EGLDISPLAY(value)
#define MALI_API_TRACE_RETURN_EGLCONTEXT(value)
#define MALI_API_TRACE_RETURN_EGLSURFACE(value)
#define MALI_API_TRACE_RETURN_EGLIMAGEKHR(value)

#define MALI_API_TRACE_RETURN_EGLINT_ARRAY(id, ptr, size)
#define MALI_API_TRACE_RETURN_EGLCONFIG_ARRAY(id, ptr, size)

#define MALI_API_TRACE_EGLNATIVEWINDOWTYPE_RESIZED(win)
#define MALI_API_TRACE_ADD_PIXMAP_DATA(pixmap)

#endif /* MALI_API_TRACE */

#endif /* _EGL_API_TRACE_H_ */
