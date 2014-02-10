/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles1_hint.h"

#include <gles_state.h>
#include "gles1_state.h"
#include <gles_util.h>

void _gles1_hint_init( gles1_hint *hint )
{
	MALI_DEBUG_ASSERT_POINTER( hint );

	hint->fog                    = GL_DONT_CARE;
	hint->generate_mipmap        = GL_DONT_CARE;
	hint->line_smooth            = GL_DONT_CARE;
	hint->perspective_correction = GL_DONT_CARE;
	hint->point_smooth           = GL_DONT_CARE;
}

GLenum _gles1_hint( struct gles_state *state, GLenum target, GLenum mode )
{
	gles1_hint *hint;
	GLenum valid_enums_mode[] = { GL_FASTEST, GL_NICEST, GL_DONT_CARE };

	MALI_DEBUG_ASSERT_POINTER( state );
	MALI_DEBUG_ASSERT_POINTER( state->api.gles1 );
	hint = &(state->api.gles1->hint);

	if (0 == _gles_verify_enum( valid_enums_mode, (u32)MALI_ARRAY_SIZE( valid_enums_mode ), mode )) MALI_ERROR( GL_INVALID_ENUM );

	switch( target )
	{
		case GL_FOG_HINT:                    hint->fog = mode;                    break;
		case GL_GENERATE_MIPMAP_HINT:        hint->generate_mipmap = mode;        break;
		case GL_LINE_SMOOTH_HINT:            hint->line_smooth = mode;            break;
		case GL_PERSPECTIVE_CORRECTION_HINT: hint->perspective_correction = mode; break;
		case GL_POINT_SMOOTH_HINT:           hint->point_smooth = mode;           break;
		default: MALI_ERROR( GL_INVALID_ENUM );
	}
	GL_SUCCESS;
}
