/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_debug.h>
#include <gles_fb_interface.h>
#include <gles_common_state/gles_pixel.h>

#include "gles2_pixel.h"

void _gles2_pixel_init( struct gles_pixel *pixel )
{
	MALI_DEBUG_ASSERT_POINTER( pixel );

	pixel->pack_alignment   = 4;
	pixel->unpack_alignment = 4;
}

GLenum _gles2_pixel_storei( struct gles_pixel *pixel, GLenum pname, GLint param )
{
	MALI_DEBUG_ASSERT_POINTER( pixel );

	if( ( param != 1 ) && ( param != 2 ) && ( param != 4 ) && ( param != 8 ) )
	{
		return GL_INVALID_VALUE;
	}

	switch( pname )
	{
		case GL_PACK_ALIGNMENT:   pixel->pack_alignment = param;   break;
		case GL_UNPACK_ALIGNMENT: pixel->unpack_alignment = param; break;
		default: return GL_INVALID_ENUM;
	}

	return GL_NO_ERROR;
}


