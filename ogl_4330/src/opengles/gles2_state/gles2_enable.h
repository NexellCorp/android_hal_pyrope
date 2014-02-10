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
* @file gles2_enablee.h
* @brief Handle GL enable state
*/

#ifndef _GLES2_ENABLE_H_
#define _GLES2_ENABLE_H_

#include "../gles_base.h"
struct gles_context;

/**
 * set an enable-state
 * @param ctx The pointer to the GLES context
 * @param cap Which state to be altered(GL_BLEND, GL_LIGHTING, etc)
 * @param state The new enable-state
 */
GLenum MALI_CHECK_RESULT _gles2_enable( struct gles_context *ctx, GLenum cap, GLboolean state );

#endif /* _GLES2_ENABLE_H_ */
