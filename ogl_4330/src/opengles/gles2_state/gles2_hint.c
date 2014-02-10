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

#include "gles2_hint.h"
#include "../gles_state.h"
#include "gles2_state.h"
#include "../gles_util.h"

void _gles2_hint_init( gles2_hint *hint )
{
	MALI_DEBUG_ASSERT_POINTER( hint );

	hint->generate_mipmap        = GL_DONT_CARE;
	hint->fragment_shader_derivative = GL_DONT_CARE;
}

GLenum _gles2_hint( struct gles_state *state, GLenum target, GLenum mode )
{
	static const GLenum valid_enums_mode[] = { GL_FASTEST, GL_NICEST, GL_DONT_CARE };

	MALI_DEBUG_ASSERT_POINTER( state );
	MALI_DEBUG_ASSERT_POINTER( state->api.gles2 );

	if ( _gles_verify_enum( valid_enums_mode, (u32)MALI_ARRAY_SIZE( valid_enums_mode ), mode ) == 0 ) return GL_INVALID_ENUM;

	switch( target )
	{
		case GL_GENERATE_MIPMAP_HINT: state->api.gles2->hint.generate_mipmap = mode;        break;
#if EXTENSION_FRAGMENT_SHADER_DERIVATIVE_HINT_OES_ENABLE
		case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES: state->api.gles2->hint.fragment_shader_derivative = mode;        break;
#endif
		default: return GL_INVALID_ENUM;
	}
	return GL_NO_ERROR;
}
