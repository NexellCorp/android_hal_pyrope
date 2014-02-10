/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <gles_fb_interface.h>

#include "gles_multisampling.h"

void _gles_sample_coverage( struct gles_context *ctx, GLftype value, GLboolean invert )
{
	const mali_float mali_value = CLAMP( FTYPE_TO_FLOAT( value ), 0.0f, 1.0f );
	const mali_bool mali_invert = ( GL_FALSE == invert ) ? MALI_FALSE : MALI_TRUE;

	_gles_fb_sample_coverage( ctx, mali_value, mali_invert );
}
