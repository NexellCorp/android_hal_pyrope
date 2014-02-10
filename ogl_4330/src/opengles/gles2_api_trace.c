/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles2_api_trace.c
 * @brief Helper functions for OpenGL ES 2.0 API tracing
 */

#include "gles2_api_trace.h"
#include "gles2_state/gles2_program_object.h"
#include "shared/binary_shader/bs_symbol.h"

u32 mali_api_trace_get_uniform_size(mali_named_list* program_object_list, GLuint program, GLint location)
{
	gles2_program_object *po;
	GLenum object_type;
	struct bs_uniform_location* bs_loc;
	u32 height, width;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	po = _gles2_program_internal_get_type(program_object_list, program, &object_type);

	/* assert that name is valid */
	MALI_CHECK( object_type != GL_INVALID_VALUE, 0 );

	/* assert that object is a program object */
	MALI_CHECK( object_type == GLES_PROGRAM, 0 );

	/* assert program has been linked */
	MALI_DEBUG_ASSERT_POINTER( po->render_state );
	MALI_CHECK( MALI_TRUE == po->render_state->binary.linked, 0 );

	/* location must be valid */
	MALI_CHECK( location >= 0, 0 );
	MALI_CHECK( (u32)location < po->render_state->gl_uniform_location_size, 0 );

	bs_loc = &(po->render_state->gl_uniform_locations[location]);

	/* check if it's a sampler */
	if( bs_symbol_is_sampler(bs_loc->symbol) )
	{
		return 1;
	}

	width = bs_loc->symbol->type.primary.vector_size;
	height = 1;
	if(bs_loc->symbol->datatype == DATATYPE_MATRIX) height *= width;

	return (height * width);
}
