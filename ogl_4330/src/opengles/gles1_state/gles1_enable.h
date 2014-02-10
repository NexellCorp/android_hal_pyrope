/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles1_enable.h
 * @brief Enable/disable states in GLES
 */

#ifndef _GLES1_ENABLE_H_
#define _GLES1_ENABLE_H_

#include <GLES/mali_glext.h>
#include <mali_system.h>

struct gles_state;
struct gles_context;

/**
 * set the enable-state for a client state
 * @param ctx The pointer to the GLES context
 * @param cap Which array to be altered(GL_VERTEX_ARRAY, GL_COLOR_ARRAY, etc)
 * @param state The new state for the arrays enable
 */
GLenum MALI_CHECK_RESULT _gles1_client_state( struct gles_context *ctx, GLenum cap, GLboolean state );

/**
 * set an enable-state
 * @param ctx The pointer to the GLES context
 * @param cap Which state to be altered(GL_BLEND, GL_LIGHTING, etc)
 * @param state The new enable-state
 */
GLenum MALI_CHECK_RESULT _gles1_enable( struct gles_context *ctx, GLenum cap, GLboolean state );

#endif /* _GLES1_ENABLE_H_ */
