/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles2_context.h
 * @brief Defines the functions that creates and deletes the gles_context
 *        for OpenGL ES 2.x
 */

#include "gles_context.h"

/**
 * @brief Constructor that creates an OpenGL ES 2.x gles_context. This context will have
 *        a vtable directing OpenGL ES API calls to the OpenGL ES 2.x second layer functions.
 *
 * @param base_ctx The base context to use with the gles context
 * @param share_ctx Optional gles context that this gles context will share its lists with
 * @param instrumented_ctx The instrumented context to use with the gles context, if
 *                         instrumentation is enabled
 * @param egl_get_image_ptr A pointer to a function to retrieve an EGL image from
 * @param egl_funcptrs A pointer to the shared egl function pointers struct. Used by eglsync and eglimage extension.
 * @return a gles_context for OpenGL ES 2.x
 */
struct gles_context *_gles2_create_context( mali_base_ctx_handle base_ctx, struct gles_context *share_ctx, egl_api_shared_function_ptrs *egl_funcptrs, int robust_access );

/**
 * @brief Destructor for deleting an OpenGL ES 2.x gles_context.
 *
 * @param ctx The OpenGL ES 2.x context to delete
 */
void  _gles2_delete_context( struct gles_context *ctx );
