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
 * @file gles1_api_trace.c
 * @brief Helper functions for OpenGL ES 1.1 API tracing
 */

#include "gles1_api_trace.h"

u32 mali_api_trace_gl_light_count(GLenum pname)
{
	u32 count = 0;

	switch(pname)
	{
	case GL_AMBIENT:
	case GL_DIFFUSE:
	case GL_SPECULAR:
	case GL_EMISSION:
	case GL_POSITION:
		count = 4;
		break;
	case GL_SPOT_DIRECTION:
		count = 3;
		break;
	case GL_SPOT_EXPONENT:
	case GL_SPOT_CUTOFF:
	case GL_CONSTANT_ATTENUATION:
	case GL_LINEAR_ATTENUATION:
	case GL_QUADRATIC_ATTENUATION:
		count = 1;
		break;
	default:
		count = 0;
		break;
	}

	return count;
}

u32 mali_api_trace_gl_lightmodel_count(GLenum pname)
{
	u32 count = 0;

	switch(pname)
	{
	case GL_LIGHT_MODEL_TWO_SIDE:
		count = 1;
		break;
	case GL_LIGHT_MODEL_AMBIENT:
		count = 4;
		break;
	default:
		count = 0;
		break;
	}

	return count;
}


u32 mali_api_trace_gl_material_count(GLenum pname)
{
	u32 count = 0;

	switch(pname)
	{
	case GL_AMBIENT:
	case GL_DIFFUSE:
	case GL_SPECULAR:
	case GL_EMISSION:
		count = 4;
		break;
	case GL_SHININESS:
	case GL_AMBIENT_AND_DIFFUSE:
		count = 1;
		break;
	default:
		count = 0;
		break;
	}

	return count;
}

u32 mali_api_trace_gl_texenv_count(GLenum pname)
{
	u32 count = 0;

	switch(pname)
	{
	case GL_TEXTURE_ENV_MODE:
	case GL_COMBINE_RGB:
	case GL_COMBINE_ALPHA:
	case GL_SRC0_RGB:
	case GL_SRC1_RGB:
	case GL_SRC2_RGB:
	case GL_SRC0_ALPHA:
	case GL_SRC1_ALPHA:
	case GL_SRC2_ALPHA:
	case GL_OPERAND0_RGB:
	case GL_OPERAND1_RGB:
	case GL_OPERAND2_RGB:
	case GL_OPERAND0_ALPHA:
	case GL_OPERAND1_ALPHA:
	case GL_OPERAND2_ALPHA:
	case GL_RGB_SCALE:
	case GL_ALPHA_SCALE:
	case GL_COORD_REPLACE_OES:
		count = 1;
		break;
	case GL_TEXTURE_ENV_COLOR:
		count = 4;
		break;
	default:
		count = 0;
		break;
	}

	return count;
}

u32 mali_api_trace_gl_pointparameter_count(GLenum pname)
{
	u32 count = 0;

	switch(pname)
	{
	case GL_POINT_SIZE_MIN:
	case GL_POINT_SIZE_MAX:
	case GL_POINT_FADE_THRESHOLD_SIZE:
		count = 1;
		break;
	case GL_POINT_DISTANCE_ATTENUATION:
		count = 3;
		break;
	default:
		count = 0;
		break;
	}

	return count;
}
