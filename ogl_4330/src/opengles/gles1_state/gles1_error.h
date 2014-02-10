/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005, 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles1_error.h
 * @brief Functions for setting and getting errors.
 */

#ifndef _GLES1_ERROR_H_
#define _GLES1_ERROR_H_

#include "../gles_base.h"

struct gles_context;

/**
 * Sets an error code in the GL-state
 * @param ctx The OpenGL ES context
 * @param errorcode the errorcode to set
 */
void _gles1_set_error(struct gles_context *ctx, GLenum errorcode);

/**
 * Gets the current viewport and reset to GL_NO_ERROR
 * @param ctx The OpenGL ES context
 * @return the current error
 * @note This implements the GLES-entrypoint glGetError()
 */
GLenum _gles1_get_error(struct gles_context *ctx) MALI_CHECK_RESULT;

#endif /* _GLES1_ERROR_H_ */
