/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_M200_RSW_UTIL_H
#define GLES_M200_RSW_UTIL_H

#include "../gles_base.h"

/**
 * @brief convert gles conditional to mali conditional
 */
u8 _gles_m200_gles_to_mali_conditional( GLenum gles_conditional );
/**
 * @brief convert mali conditional to gles conditional
 */
GLenum _gles_m200_mali_to_gles_conditional( u8 mali_conditional );

/**
 * @brief convert gles logicop to mali logicop
 */
u8 _gles_m200_gles_to_mali_logicop( GLenum gles_logicop );
/**
 * @brief convert mali logicop to gles logicop
 */
GLenum _gles_m200_mali_to_gles_logicop( u8 mali_logicop );

/**
 * @brief convert gles blend equation to mali blend_equation
 */
u8 _gles_m200_gles_to_mali_blend_equation( GLenum gles_blend_equation );
/**
 * @brief convert mali blend equation to gles blend_equation
 */
GLenum _gles_m200_mali_to_gles_blend_equation( u8 mali_blend_equation );

/**
 * @brief convert gles stencil operation enum to mali stencil operation
 */
u8 _gles_m200_gles_to_mali_stencil_operation( GLenum gles_operation );
/**
 * @brief convert mali stencil operation enum to gles stencil operation
 */
GLenum _gles_m200_mali_to_gles_stencil_operation( u8 mali_operation );

/**
 * @brief convert gles blend factor enum to mali blend factor
 */
u8 _gles_m200_gles_to_mali_blend_func( GLenum gles_blend_func );

/**
 * @brief convert mali blend factor enum to gles blend factor
 */
GLenum _gles_m200_mali_to_gles_blend_func( u8 mali_blend_func );


#endif /* GLES_M200_RSW_UTIL_H */
