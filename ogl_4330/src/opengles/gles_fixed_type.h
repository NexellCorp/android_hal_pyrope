/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_fixed_type.h
 * @brief Defines the fixed/fixed-wrapper type, GLftype.
 */

#ifndef _GLES_FIXED_TYPE_H_
#define _GLES_FIXED_TYPE_H_

#include "gles_base.h"

#include <mali_system.h>

#if GLES_FTYPE!=GLES_FIXED
#error "gles_fixed_type.h should not be included when FIXED is not the FTYPE"
#endif

#include <util/mali_fixed.h>
#include <util/mali_fixed_vector3.h>
#include <util/mali_fixed_vector4.h>
#include <util/mali_fixed_matrix4x4.h>

/** a wrapper-type for GLfixed / GLfixed */
typedef GLfixed GLftype;

/** wrapper-macro for GL_FIXED / GL_FIXED */
#define GL_FTYPE GL_FIXED

/** convert from fixed to ftype */
#define FIXED_TO_FTYPE(x) (x)

/** convert from ftype to fixed */
#define FTYPE_TO_FIXED(x) (x)

/** convert from fixed to ftype */
#define FIXED_TO_FTYPE(x) fixed_to_fixed(x)

/** convert from ftype to fixed */
#define FTYPE_TO_FIXED(x) fixed_to_fixed(x)

/** convert from normalized integer to fixed */
#define NORMALIZED_INT_TO_FTYPE(x) normalized_int_to_fixed(x)

/** convert from fixed to normalized integer*/
#define FTYPE_TO_NORMALIZED_INT(x) fixed_to_normalized_int(x)

/** convert from integer to fixed */
#define INT_TO_FTYPE(x) ( (fixed) (x) )

/** convert from fixed to integer*/
#define FTYPE_TO_INT(x) ( (s32) (x) )

/** convert a boolean to fixed */
#define BOOLEAN_TO_FTYPE( x ) ( ( (x) == GL_TRUE ) ? ( GLfixed ) 1.f : ( GLfixed ) 0.f )
#define FTYPE_TO_BOOLEAN( x ) ( ( (x) != 0.f ) ? (GLboolean) GL_TRUE : (GLboolean) GL_FALSE )

/** a wrapper-type for mali_fixed / mali_fixed */
typedef mali_fixed mali_ftype;

/** a wrapper-type for mali_fixed_vector3 / mali_fixed_vector3 */
typedef mali_fixed_vector3 mali_vector3;

/** a wrapper-type for mali_fixed_vector4 / mali_fixed_vector4 */
typedef mali_fixed_vector4 mali_vector4;

/** a wrapper-type for mali_fixed_matrix4x4 / mali_fixed_matrix4x4 */
typedef mali_fixed_matrix4x4 mali_matrix4x4;

/* vector3 */

/**
 * @brief scale a 3 component vector
 * @param v the vector to scale
 * @return a vector with the same direction as the input and a magnitude of m * sqrt((v.x)^2 + (v.y)^2 + (v.z)^2)
 */
MALI_STATIC_INLINE mali_vector3 __mali_vector3_scale(mali_vector3 v, mali_fixed m)
{
	return __mali_fixed_vector3_scale( v, m );
}

/**
 * @brief add one 3 component vector from another 3 component vector
 * @param v1 the first vector
 * @param v2 the vector to add
 * @return a vector containing the component-wise added result
 */
MALI_STATIC_INLINE mali_vector3 __mali_vector3_add(mali_vector3 v1, mali_vector3 v2)
{
	return __mali_fixed_vector3_add( v1, v2 );
}

/**
 * @brief subtract one 3 component vector from another 3 component vector
 * @param v1 the first vector
 * @param v2 the vector to subtract
 * @return a vector containing the component-wise subtracted result
 */
MALI_STATIC_INLINE mali_vector3 __mali_vector3_sub(mali_vector3 v1, mali_vector3 v2)
{
	return __mali_fixed_vector3_sub( v1, v2 );
}

/**
 * @brief calculate magnitude of a 3 component vector
 * @param v the vector in question
 * @return the magnitude of v
 */
MALI_STATIC_INLINE mali_fixed __mali_vector3_magnitude(mali_vector3 v)
{
	return __mali_fixed_vector3_magnitude( v );
}

/**
 * @brief normalize a 3 component vector
 * @param v the vector to normalize
 * @return a vector of unit length pointing in the same direction as the input
 */
MALI_STATIC_INLINE mali_vector3 __mali_vector3_normalize(mali_vector3 v)
{
	return __mali_fixed_vector3_normalize( v );
}

/**
 * @brief take the dot-product of two three directional vectors
 * @param v1 The first vector in the dot-product
 * @param v2 The second vector in the dot-product
 * @return the cosine of the angle between the two vectors
 */
MALI_STATIC_INLINE mali_ftype __mali_vector3_dot3(mali_vector3 v1, mali_vector3 v2)
{
	return __mali_fixed_vector3_dot3( v1, v2 );
}


/* vector4 */

/**
 * @brief add two 4 component vectors
 * @param v1 the first vector
 * @param v2 the second vector
 * @return a vector containing the component-wise added result
 */
MALI_STATIC_INLINE mali_vector4 __mali_vector4_add(mali_vector4 v1, mali_vector4 v2)
{
	return __mali_fixed_vector4_add( v1, v2 );
}

/**
 * @brief subtract one 4 component vector from another 4 component vector
 * @param v1 the first vector
 * @param v2 the vector to subtract
 * @return a vector containing the component-wise subtracted result
 */
MALI_STATIC_INLINE mali_vector4 __mali_vector4_sub(mali_vector4 v1, mali_vector4 v2)
{
	return __mali_fixed_vector4_sub( v1, v2 );
}

/* matrix4x4 */

/**
 * @brief setup an identity matrix
 * @param dst the destination matrix
 */
MALI_STATIC_INLINE void __mali_matrix4x4_make_identity(mali_matrix4x4 dst)
{
	__mali_fixed_matrix4x4_make_identity( dst );
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
	__mali_fixed_matrix4x4_make_frustum( dst, l, r, b, t, n, f );
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
	__mali_fixed_matrix4x4_make_ortho( dst, l, r, b, t, n, f );
}

/**
 * @brief copy a 4x4 matrix
 * @param dst the destination matrix
 * @param src the source matrix
 */
MALI_STATIC_INLINE void __mali_matrix4x4_copy(mali_matrix4x4 dst, mali_matrix4x4 src)
{
	__mali_fixed_matrix4x4_copy( dst, src );
}

/**
 * @brief multiply two 4x4 matrices
 * @param dst the destination matrix
 * @param src1 the first source matrix. this may be the same as the destination matrix
 * @param src2 the second source matrix. this may not be the same as the destination matrix
 */
MALI_STATIC_INLINE void __mali_matrix4x4_multiply(mali_matrix4x4 dst, mali_matrix4x4 src1, const mali_matrix4x4 src2)
{
	__mali_fixed_matrix4x4_multiply( dst, src1, src2 );
}

/**
 * @brief inverse a 4x4 matrix
 * @param dst the destination matrix
 * @param src the source matrix
 */
MALI_STATIC_INLINE mali_err_code __mali_matrix4x4_invert(mali_matrix4x4 dst, mali_matrix4x4 src)
{
	return __mali_fixed_matrix4x4_invert( dst, src );
}

/**
 * @brief transpose a 4x4 matrix
 * @param dst the destination matrix
 * @param src the source matrix
 */
MALI_STATIC_INLINE void __mali_matrix4x4_transpose(mali_matrix4x4 dst, mali_matrix4x4 src)
{
	__mali_fixed_matrix4x4_transpose( dst, src );
}

/**
 * @brief transform a 3 component vector with a 4x4 matrix, but ignore all w-operations.
 * @param m the matrix
 * @param v the vector
 * @return the transformed vector
 */
MALI_STATIC_INLINE mali_vector3 __mali_matrix4x4_rotate_vector3(mali_matrix4x4 m, mali_vector3 v)
{
	return __mali_fixed_matrix4x4_rotate_vector3( m, v );
}

/**
 * @brief transform a 3 component vector with a 4x4 matrix, but ignore all w-operations.
 * @param dst the destination-vector
 * @param m the matrix
 * @param v the vector
 */
MALI_STATIC_INLINE void __mali_matrix4x4_rotate_array3(mali_ftype dst[3], mali_matrix4x4 m, mali_ftype v[3])
{
	__mali_fixed_matrix4x4_rotate_array3( dst, m, v );
}

/**
 * @brief transform a 4 component vector with a 4x4 matrix
 * @param m the matrix
 * @param v the vector
 * @return the transformed vector
 */
MALI_STATIC_INLINE mali_vector4 __mali_matrix4x4_transform_vector4(mali_matrix4x4 m, mali_vector4 v)
{
	return __mali_fixed_matrix4x4_transform_vector4( m, v );
}

/**
 * @brief transform a 4 component vector with a 4x4 matrix
 * @param dst The destination-vector
 * @param m the matrix
 * @param v the vector
 */
MALI_STATIC_INLINE void __mali_matrix4x4_transform_array4(mali_ftype dst[4], mali_matrix4x4 m, mali_ftype v[3])
{
	__mali_fixed_matrix4x4_transform_array4( dst, m, v );
}

#undef MAKE_FTYPE_IDENTIFIER

#endif /* _GLES_FIXED_TYPE_H_ */
