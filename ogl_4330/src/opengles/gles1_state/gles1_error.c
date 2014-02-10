/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005, 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>

#include "gles1_error.h"
#include "../gles_state.h"
#include "../gles_context.h"

#include <base/mali_types.h>
#include <shared/m200_gp_frame_builder.h>

void _gles1_set_error(struct gles_context *ctx, GLenum errorcode)
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	switch (errorcode)
	{
		case GL_NO_ERROR:
		case GL_INVALID_ENUM:
		case GL_INVALID_VALUE:
		case GL_INVALID_OPERATION:
		case GL_STACK_OVERFLOW:
		case GL_STACK_UNDERFLOW:
		case GL_OUT_OF_MEMORY:
#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
		case GL_INVALID_FRAMEBUFFER_OPERATION:
#endif
			break;

		default:
			MALI_DEBUG_ASSERT( MALI_FALSE, ("invalid error code set: 0x%04x", errorcode));
			break;
	}

	/* only register the first error */
	if (GL_NO_ERROR == ctx->state.common.errorcode)
	{
		/* To ensure correct ordering of errors, we need to flush drawcall
		 * merging so that if previous draw calls generate GL_OUT_OF_MEMORY it
		 * will occur ahead of the error being reported by the call to this
		 * function.
		 */
		ctx->state.common.errorcode = errorcode;
	}
}

MALI_INTER_MODULE_API GLenum _gles1_get_error(struct gles_context *ctx)
{
	u32 errorcode;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	errorcode = ctx->state.common.errorcode;

#ifdef DEBUG
	switch (errorcode)
	{
		case GL_NO_ERROR:
		case GL_INVALID_ENUM:
		case GL_INVALID_VALUE:
		case GL_INVALID_OPERATION:
		case GL_STACK_OVERFLOW:
		case GL_STACK_UNDERFLOW:
		case GL_OUT_OF_MEMORY:
#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
		case GL_INVALID_FRAMEBUFFER_OPERATION:
#endif
		break;

		default:
			MALI_DEBUG_ASSERT( MALI_FALSE, ("invalid error code set: 0x%04x", errorcode));
		break;
	}
#endif

	ctx->state.common.errorcode = GL_NO_ERROR;

	return errorcode;
}
