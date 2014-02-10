/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_float_type.h
 * @brief Defines the float/fixed-wrapper type, GLftype.
 */

#ifndef _GLES_FLOAT_TYPE_H_
#define _GLES_FLOAT_TYPE_H_

#include "gles_base.h"

#include <mali_system.h>

#if GLES_FTYPE!=GLES_FLOAT
#error "gles_float_type.h should not be included when FLOAT is not the FTYPE"
#endif

#include <util/mali_float.h>
#include <util/mali_float_vector3.h>
#include <util/mali_float_vector4.h>
#include <util/mali_float_matrix4x4.h>

/** a wrapper-type for GLfloat / GLfixed */
typedef GLfloat GLftype;

/** wrapper-macro for GL_FLOAT / GL_FIXED */
#define GL_FTYPE GL_FLOAT

/** convert from float to ftype */
#define FLOAT_TO_FTYPE(x) (x)

/** convert from ftype to float */
#define FTYPE_TO_FLOAT(x) (x)

/** convert from fixed to ftype */
#define FIXED_TO_FTYPE(x) fixed_to_float(x)

/** convert from ftype to fixed */
#define FTYPE_TO_FIXED(x) float_to_fixed(x)

/** convert from normalized integer to float */
#define NORMALIZED_INT_TO_FTYPE(x) normalized_int_to_float(x)

/** convert from float to normalized integer*/
#define FTYPE_TO_NORMALIZED_INT(x) float_to_normalized_int(x)

/** convert from integer to float */
#define INT_TO_FTYPE(x) ( (float) (x) )

/** convert from float to integer*/
/* BUGZILLA 3157: GLES conversion rules are that all floats should be mathematically rounded, not capped */
#define FTYPE_TO_INT(x) ( (s32) ((x) + (((x)>0)?(0.5f):(-0.5f))) )
/*#define FTYPE_TO_INT(x) ( (s32) (x) ) */

/** convert a boolean to float */
#define BOOLEAN_TO_FTYPE( x ) ( ( (x) == GL_TRUE ) ? ( GLfloat ) 1.f : ( GLfloat ) 0.f )
#define FTYPE_TO_BOOLEAN( x ) ( ( (x) != 0.f ) ? (GLboolean) GL_TRUE : (GLboolean) GL_FALSE )

/** a wrapper-type for mali_float / mali_fixed */
typedef mali_float mali_ftype;

/** a wrapper-type for mali_float_vector3 / mali_fixed_vector3 */
typedef mali_float_vector3 mali_vector3;

/** a wrapper-type for mali_float_vector4 / mali_fixed_vector4 */
typedef mali_float_vector4 mali_vector4;

/** a wrapper-type for mali_float_matrix4x4 / mali_fixed_matrix4x4 */
typedef mali_float_matrix4x4 mali_matrix4x4;

/* vector3 */

#if MALI_USE_GLES_1	/* these functions are not needed for OpenGL ES 2.0  */

/**
 * @brief normalize a 3 component vector
 * @param v the vector to normalize
 * @return a vector of unit length pointing in the same direction as the input
 */
MALI_STATIC_INLINE mali_vector3 __mali_vector3_normalize(mali_vector3 v)
{
	return __mali_float_vector3_normalize( v );
}

/**
 * @brief scale a 3 component vector
 * @param v the vector to scale
 * @return a vector with the same direction as the input and a magnitude of m * sqrt((v.x)^2 + (v.y)^2 + (v.z)^2)
 */
MALI_STATIC_INLINE mali_vector3 __mali_vector3_scale(mali_vector3 v, mali_float m)
{
       return __mali_float_vector3_scale( v, m );
}

/**
 * @brief take the dot-product of two three directional vectors
 * @param v1 The first vector in the dot-product
 * @param v2 The second vector in the dot-product
 * @return the cosine of the angle between the two vectors
 */
MALI_STATIC_INLINE mali_ftype __mali_vector3_dot3(mali_vector3 v1, mali_vector3 v2)
{
       return __mali_float_vector3_dot3( v1, v2 );
}

/* matrix4x4 */

/**
 * @brief setup an identity matrix
 * @param dst the destination matrix
 */
MALI_STATIC_INLINE void __mali_matrix4x4_make_identity(mali_matrix4x4 dst)
{
	__mali_float_matrix4x4_make_identity( dst );
}

/**
 * @brief setup a frusum matrix
 * @param dst the destination matrix
 * @param l the distance to the left plane
 * @param r the distance to the right plane
 * @param b the distance to the bottom plane
 * @param t the distance to the top plane
 * @param n the distance to the near plane
 * @param f the distance to the far plane
 */
MALI_STATIC_INLINE void __mali_matrix4x4_make_frustum(mali_matrix4x4 dst, mali_ftype l, mali_ftype r, mali_ftype b, mali_ftype t, mali_ftype n, mali_ftype f)
{
	__mali_float_matrix4x4_make_frustum( dst, l, r, b, t, n, f );
}

/**
 * @brief setup an ortho matrix
 * @param dst the destination matrix
 * @param l the distance to the left plane
 * @param r the distance to the right plane
 * @param b the distance to the bottom plane
 * @param t the distance to the top plane
 * @param n the distance to the near plane
 * @param f the distance to the far plane
 */
MALI_STATIC_INLINE void __mali_matrix4x4_make_ortho(mali_matrix4x4 dst, mali_ftype l, mali_ftype r, mali_ftype b, mali_ftype t, mali_ftype n, mali_ftype f)
{
	__mali_float_matrix4x4_make_ortho( dst, l, r, b, t, n, f );
}

/**
 * @brief copy a 4x4 matrix
 * @param dst the destination matrix
 * @param src the source matrix
 */
MALI_STATIC_INLINE void __mali_matrix4x4_copy(mali_matrix4x4 dst, mali_matrix4x4 src)
{
	__mali_float_matrix4x4_copy( dst, src );
}

/**
 * @brief multiply two 4x4 matrices
 * @param dst the destination matrix
 * @param src1 the first source matrix. this may be the same as the destination matrix
 * @param src2 the second source matrix. this may not be the same as the destination matrix
 */
MALI_STATIC_INLINE void __mali_matrix4x4_multiply(mali_matrix4x4 dst, mali_matrix4x4 src1, const mali_matrix4x4 src2)
{
#if (!MALI_OSU_MATH)
       __mali_float_matrix4x4_multiply( dst, src1, src2 );
#else
        _mali_osu_matrix4x4_multiply( (float*)dst, (float*)src1, (const float*)src2 );
#endif
}

/**
 * @brief inverse a 4x4 matrix
 * @param dst the destination matrix
 * @param src the source matrix
 */
MALI_STATIC_INLINE mali_err_code __mali_matrix4x4_invert(mali_matrix4x4 dst, mali_matrix4x4 src)
{
	return __mali_float_matrix4x4_invert( dst, src );
}

/**
 * @brief transpose a 4x4 matrix
 * @param dst the destination matrix
 * @param src the source matrix
 */
MALI_STATIC_INLINE void __mali_matrix4x4_transpose(mali_matrix4x4 dst, mali_matrix4x4 src)
{
#if (!MALI_OSU_MATH)
	__mali_float_matrix4x4_transpose( dst, src );
#else
	_mali_osu_matrix4x4_transpose( (float*)dst, (float*)src );
#endif
}

/**
 * @brief transform a 4 component vector with a 4x4 matrix
 * @param m the matrix
 * @param v the vector
 * @return the transformed vector
 */
MALI_STATIC_INLINE mali_vector4 __mali_matrix4x4_transform_vector4(mali_matrix4x4 m, mali_vector4 v)
{
	return __mali_float_matrix4x4_transform_vector4( m, v );
}

/**
 * @brief transform a 4 component vector float array with a 4x4 matrix
 * @param dst The destination-vector
 * @param m the matrix
 * @param v the vector
 */
MALI_STATIC_INLINE void __mali_matrix4x4_transform_array4(mali_ftype dst[4], mali_matrix4x4 m, mali_ftype v[3])
{
	__mali_float_matrix4x4_transform_array4( dst, m, v );
}

#undef MAKE_FTYPE_IDENTIFIER
#endif /* MALI_USE_GLES_1 */
#endif /* _GLES_FTYPE_H_ */
