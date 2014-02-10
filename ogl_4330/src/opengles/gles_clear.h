/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES_CLEAR_H_
#define _GLES_CLEAR_H_

/**
 * @file gles_clear.h
 * @brief Functions to clear the framebuffer
 */
#include "gles_base.h"

struct gles_context;

/**
 * Clear the framebuffer, adhering to color masks, channel disabling and scissoring
 * @param ctx The pointer to the GLES context
 * @param mask The bitmask stating which buffers are to be cleared(GL_COLOR_BUFFER|GL_DEPTH_BUFFER, etc. The mask could represent several buffers)
 * @note This is a wrapper around the GLES-entrypoint glClear()
 */
GLenum MALI_CHECK_RESULT _gles_clear( struct gles_context *ctx, GLbitfield mask );

#endif /* _GLES_CLEAR_H_ */
