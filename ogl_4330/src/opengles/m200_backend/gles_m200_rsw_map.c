/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_m200_rsw_map.h"
#include <mali_rsw.h>

#ifdef MALI_USE_GLES_1

u8 _gles_m200_gles_to_mali_logicop( GLenum gles_logicop )
{
	switch ( gles_logicop )
	{
		case GL_CLEAR:         return M200_LOGIC_OP_ALWAYS_0;
		case GL_AND:           return M200_LOGIC_OP_AND;
		case GL_AND_REVERSE:   return M200_LOGIC_OP_S_NOT_D;
		case GL_COPY:          return M200_LOGIC_OP_S;
		case GL_AND_INVERTED:  return M200_LOGIC_OP_D_NOT_S;
		case GL_NOOP:          return M200_LOGIC_OP_D;
		case GL_XOR:           return M200_LOGIC_OP_XOR;
		case GL_OR:            return M200_LOGIC_OP_OR;
		case GL_NOR:           return M200_LOGIC_OP_NOR;
		case GL_EQUIV:         return M200_LOGIC_OP_XNOR;
		case GL_INVERT:        return M200_LOGIC_OP_NOT_D;
		case GL_OR_REVERSE:    return M200_LOGIC_OP_NOT_NOT_S_D;
		case GL_COPY_INVERTED: return M200_LOGIC_OP_NOT_S;
		case GL_OR_INVERTED:   return M200_LOGIC_OP_NOT_NOT_D_S;
		case GL_NAND:          return M200_LOGIC_OP_NAND;
		case GL_SET:           return M200_LOGIC_OP_ALWAYS_1;
	}
	MALI_DEBUG_ASSERT( 0, ("switch parameter not a valid gles logical operation\n") );
	return 0;
}

GLenum _gles_m200_mali_to_gles_logicop( u8 mali_logicop )
{
	switch ( mali_logicop )
	{
		case M200_LOGIC_OP_ALWAYS_0:    return GL_CLEAR;
		case M200_LOGIC_OP_AND:         return GL_AND;
		case M200_LOGIC_OP_S_NOT_D:     return GL_AND_REVERSE;
		case M200_LOGIC_OP_S:           return GL_COPY;
		case M200_LOGIC_OP_D_NOT_S:     return GL_AND_INVERTED;
		case M200_LOGIC_OP_D:           return GL_NOOP;
		case M200_LOGIC_OP_XOR:         return GL_XOR;
		case M200_LOGIC_OP_OR:          return GL_OR;
		case M200_LOGIC_OP_NOR:         return GL_NOR;
		case M200_LOGIC_OP_XNOR:        return GL_EQUIV;
		case M200_LOGIC_OP_NOT_D:       return GL_INVERT;
		case M200_LOGIC_OP_NOT_NOT_S_D: return GL_OR_REVERSE;
		case M200_LOGIC_OP_NOT_S:       return GL_COPY_INVERTED;
		case M200_LOGIC_OP_NOT_NOT_D_S: return GL_OR_INVERTED;
		case M200_LOGIC_OP_NAND:        return GL_NAND;
		case M200_LOGIC_OP_ALWAYS_1:    return GL_SET;
	}
	MALI_DEBUG_ASSERT( 0, ("switch parameter not a valid gles logical operation\n") );
	return 0;
}

#endif /* MALI_USE_GLES_1 */

u8 _gles_m200_gles_to_mali_conditional( GLenum gles_conditional )
{
	switch( gles_conditional )
	{
		case GL_NEVER:    return M200_TEST_ALWAYS_FAIL;
		case GL_LESS:     return M200_TEST_LESS_THAN;
		case GL_GEQUAL:   return M200_TEST_GREATER_THAN_OR_EQUAL;
		case GL_LEQUAL:   return M200_TEST_LESS_THAN_OR_EQUAL;
		case GL_GREATER:  return M200_TEST_GREATER_THAN;
		case GL_NOTEQUAL: return M200_TEST_NOT_EQUAL;
		case GL_EQUAL:    return M200_TEST_EQUAL;
		case GL_ALWAYS:   return M200_TEST_ALWAYS_SUCCEED;
	}
	MALI_DEBUG_ASSERT( 0, ("switch parameter not a valid GLES conditional\n") );
	return 0;
}

GLenum _gles_m200_mali_to_gles_conditional( u8 mali_conditional )
{
	switch( mali_conditional )
	{
		case M200_TEST_ALWAYS_FAIL:           return GL_NEVER;
		case M200_TEST_LESS_THAN:             return GL_LESS;
		case M200_TEST_GREATER_THAN_OR_EQUAL: return GL_GEQUAL;
		case M200_TEST_LESS_THAN_OR_EQUAL:    return GL_LEQUAL;
		case M200_TEST_GREATER_THAN:          return GL_GREATER;
		case M200_TEST_NOT_EQUAL:             return GL_NOTEQUAL;
		case M200_TEST_EQUAL:                 return GL_EQUAL;
		case M200_TEST_ALWAYS_SUCCEED:        return GL_ALWAYS;
	}
	MALI_DEBUG_ASSERT( 0, ("switch parameter not a valid GLES conditional\n") );
	return 0;
}

u8 _gles_m200_gles_to_mali_blend_equation( GLenum gles_blend_equation )
{
	switch( gles_blend_equation )
	{
		case GL_FUNC_ADD:              return M200_BLEND_CsS_pCdD;
		case GL_FUNC_SUBTRACT:         return M200_BLEND_CsS_mCdD;
		case GL_FUNC_REVERSE_SUBTRACT: return M200_BLEND_CdD_mCsS;
#if EXTENSION_BLEND_MINMAX_EXT_ENABLE
		case GL_MIN_EXT:               return M200_BLEND_MIN_CsS_pCdD_Cd;
		case GL_MAX_EXT:               return M200_BLEND_MAX_CsS_pCdD_Cd;
#endif
	}
	MALI_DEBUG_ASSERT( 0, ("switch parameter not a valid GLES blend equation\n") );
	return 0;
}

GLenum _gles_m200_mali_to_gles_blend_equation( u8 mali_blend_equation )
{
	switch( mali_blend_equation )
	{
		case M200_BLEND_CsS_pCdD:        return GL_FUNC_ADD;
		case M200_BLEND_CsS_mCdD:        return GL_FUNC_SUBTRACT;
		case M200_BLEND_CdD_mCsS:        return GL_FUNC_REVERSE_SUBTRACT;
#if EXTENSION_BLEND_MINMAX_EXT_ENABLE
		case M200_BLEND_MIN_CsS_pCdD_Cd: return GL_MIN_EXT;
		case M200_BLEND_MAX_CsS_pCdD_Cd: return GL_MAX_EXT;
#endif
	}
	MALI_DEBUG_ASSERT( 0, ("switch parameter not a valid GLES blend equation\n") );
	return 0;
}

u8 _gles_m200_gles_to_mali_stencil_operation( GLenum gles_operation )
{
	switch( gles_operation )
	{
		case GL_KEEP:      return M200_STENCIL_OP_KEEP_CURRENT;
		case GL_ZERO:      return M200_STENCIL_OP_SET_TO_ZERO;
		case GL_REPLACE:   return M200_STENCIL_OP_SET_TO_REFERENCE;
		case GL_INCR:      return M200_STENCIL_OP_SATURATING_INCREMENT;
		case GL_DECR:      return M200_STENCIL_OP_SATURATING_DECREMENT;
		case GL_INVERT:    return M200_STENCIL_OP_BITWISE_INVERT;
		case GL_INCR_WRAP: return M200_STENCIL_OP_INCREMENT;
		case GL_DECR_WRAP: return M200_STENCIL_OP_DECREMENT;
	}
	MALI_DEBUG_ASSERT( 0, ("switch parameter not a valid GLES stencil action\n") );
	return 0;
}

GLenum _gles_m200_mali_to_gles_stencil_operation( u8 mali_operation )
{
	switch( mali_operation )
	{
		case M200_STENCIL_OP_KEEP_CURRENT:         return GL_KEEP;
		case M200_STENCIL_OP_SET_TO_ZERO:          return GL_ZERO;
		case M200_STENCIL_OP_SET_TO_REFERENCE:     return GL_REPLACE;
		case M200_STENCIL_OP_SATURATING_INCREMENT: return GL_INCR;
		case M200_STENCIL_OP_SATURATING_DECREMENT: return GL_DECR;
		case M200_STENCIL_OP_BITWISE_INVERT:       return GL_INVERT;
		case M200_STENCIL_OP_INCREMENT:            return GL_INCR_WRAP;
		case M200_STENCIL_OP_DECREMENT:            return GL_DECR_WRAP;
	}
	MALI_DEBUG_ASSERT( 0, ("switch parameter not a valid GLES stencil action\n") );
	return 0;
}

u8 _gles_m200_gles_to_mali_blend_func( GLenum gles_blend_func )
{
	switch( gles_blend_func )
	{
		case GL_ZERO:                return M200_RSW_BLEND_ZERO;
		case GL_ONE:                 return M200_RSW_BLEND_ONE;
		case GL_SRC_COLOR:           return M200_RSW_BLEND_SRC_COLOR;
		case GL_ONE_MINUS_SRC_COLOR: return M200_RSW_BLEND_ONE_MINUS_SRC_COLOR;
		case GL_DST_COLOR:           return M200_RSW_BLEND_DST_COLOR;
		case GL_ONE_MINUS_DST_COLOR: return M200_RSW_BLEND_ONE_MINUS_DST_COLOR;
		case GL_SRC_ALPHA:           return M200_RSW_BLEND_SRC_ALPHA;
		case GL_ONE_MINUS_SRC_ALPHA: return M200_RSW_BLEND_ONE_MINUS_SRC_ALPHA;
		case GL_DST_ALPHA:           return M200_RSW_BLEND_DST_ALPHA;
		case GL_ONE_MINUS_DST_ALPHA: return M200_RSW_BLEND_ONE_MINUS_DST_ALPHA;
		case GL_SRC_ALPHA_SATURATE:  return M200_RSW_BLEND_SRC_ALPHA_SATURATE;
		/* constant color only available in GLES 2.x
		 */
#ifdef MALI_USE_GLES_2
		case GL_CONSTANT_COLOR:           return M200_RSW_BLEND_CONSTANT_COLOR;
		case GL_ONE_MINUS_CONSTANT_COLOR: return M200_RSW_BLEND_ONE_MINUS_CONSTANT_COLOR;
		case GL_CONSTANT_ALPHA:           return M200_RSW_BLEND_CONSTANT_ALPHA;
		case GL_ONE_MINUS_CONSTANT_ALPHA: return M200_RSW_BLEND_ONE_MINUS_CONSTANT_ALPHA;
#endif
	}
	MALI_DEBUG_ASSERT( 0, ("switch value not a valid GLES blend factor\n") );
	return 0;
}

GLenum _gles_m200_mali_to_gles_blend_func( u8 mali_blend_func )
{
	switch( mali_blend_func )
	{
		case M200_RSW_BLEND_ZERO:                return GL_ZERO;
		case M200_RSW_BLEND_ONE:                 return GL_ONE;
		case M200_RSW_BLEND_SRC_COLOR:           return GL_SRC_COLOR;
		case M200_RSW_BLEND_ONE_MINUS_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR;
		case M200_RSW_BLEND_DST_COLOR:           return GL_DST_COLOR;
		case M200_RSW_BLEND_ONE_MINUS_DST_COLOR: return GL_ONE_MINUS_DST_COLOR;
		case M200_RSW_BLEND_SRC_ALPHA:           return GL_SRC_ALPHA;
		case M200_RSW_BLEND_ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
		case M200_RSW_BLEND_DST_ALPHA:           return GL_DST_ALPHA;
		case M200_RSW_BLEND_ONE_MINUS_DST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
		case M200_RSW_BLEND_SRC_ALPHA_SATURATE:  return GL_SRC_ALPHA_SATURATE;
		/* constant color only available in GLES 2.x
		 */
#ifdef MALI_USE_GLES_2
		case M200_RSW_BLEND_CONSTANT_COLOR:           return GL_CONSTANT_COLOR;
		case M200_RSW_BLEND_ONE_MINUS_CONSTANT_COLOR: return GL_ONE_MINUS_CONSTANT_COLOR;
		case M200_RSW_BLEND_CONSTANT_ALPHA:           return GL_CONSTANT_ALPHA;
		case M200_RSW_BLEND_ONE_MINUS_CONSTANT_ALPHA: return GL_ONE_MINUS_CONSTANT_ALPHA;
#endif
	}
	MALI_DEBUG_ASSERT( 0, ("switch value not a valid GLES blend factor\n") );
	return 0;
}
