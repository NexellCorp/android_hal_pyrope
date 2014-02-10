/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_debug.h>
#include <gles_context.h>
#include <gles_fb_interface.h>

#include "gles_rasterization.h"

GLenum _gles_polygon_offset( struct gles_context *ctx, GLftype factor, GLftype units )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	_gles_fb_polygon_offset( ctx, factor, units );
	mali_statebit_set( &ctx->state.common, MALI_STATE_VS_POLYGON_OFFSET_UPDATE_PENDING );

	GL_SUCCESS;
}
