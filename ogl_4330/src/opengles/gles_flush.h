/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES_FLUSH_H_
#define _GLES_FLUSH_H_

/**
 * @file gles_flush.h
 * @brief Handles flush and finish, encapsulates the frame synchronisation and resource blocking
 */

#include "gles_context.h"

/** API-level function */

/**
 * Flush the renderer
 * @param ctx GLES context pointer
 * @param flush_all If MALI_TRUE will flush all bound FB, if MALI_FALSE only the current draw FB 
 * @note This is a wrapper around the GLES-entrypoint glFlush()
 */
GLenum MALI_CHECK_RESULT _gles_flush( gles_context *ctx, mali_bool flush_all );

/**
 * Flush the renderer and wait for the frame to complete
 * @param ctx GLES context pointer
 * @note This is a wrapper around the GLES-entrypoint glFinish()
 */
GLenum MALI_CHECK_RESULT _gles_finish( gles_context *ctx );


#endif /* _GLES_FLUSH_H_ */
