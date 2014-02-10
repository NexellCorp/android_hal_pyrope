/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_util.h"

/**
 * Function to verify enums
 */
mali_bool _gles_verify_enum( const GLenum *enums, u32 count, u32 param )
{
	u32 i;

	MALI_DEBUG_ASSERT_POINTER( enums );

	for ( i = 0 ; i < count; ++i )
	{
		if ( param == enums[i] ) return MALI_TRUE;
	}
	return MALI_FALSE;
}

/**
 * Function to verify bools 
 */
mali_bool _gles_verify_bool( const GLboolean *enums, u32 count, u32 param )
{
	u32 i;

	MALI_DEBUG_ASSERT_POINTER( enums );

	for ( i = 0 ; i < count; ++i )
	{
		if ( param == enums[i] ) return MALI_TRUE;
	}
	return MALI_FALSE;
}

/**
 * Function to read size of primitive data types
 */
int _gles_get_type_size(GLenum type)
{
	int size = 0;
	switch (type)
	{
	case GL_BYTE:
	case GL_UNSIGNED_BYTE:
		size = 1;
		break;

	case GL_SHORT:
	case GL_UNSIGNED_SHORT:
		size = 2;
		break;

	case GL_FIXED:
	case GL_FLOAT:
		size = 4;
		break;

	default:
		MALI_DEBUG_ASSERT(0, ("unkown type"));
	}
	return size;
}



