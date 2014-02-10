/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_convert.h
 * @brief Utility functionality like float-to-fixed conversions etc
 */

#ifndef _GLES_CONVERT_H_
#define _GLES_CONVERT_H_

#include "gles_ftype.h"
#include "gles_util.h"
#include <mali_system.h>
#include <base/mali_debug.h>
#include "shared/mali_convert.h"

#define BOOLEAN_TO_INT( x )   ( x )
#define BOOLEAN_TO_FLOAT( x ) ( ( (x) == GL_TRUE ) ? ( GLfloat ) 1.f : ( GLfloat ) 0.f )
#define BOOLEAN_TO_FIXED( x ) ( ( (x) == GL_TRUE ) ? ( GLfixed ) ( 1<<16 ) : ( GLfixed ) 0 )

#define INT_TO_BOOLEAN( x )   ( ( (x) != 0 )   ? (GLboolean) GL_TRUE : (GLboolean) GL_FALSE )
#define INT_TO_FIXED( x ) ( (x) << 16 )
#define FLOAT_TO_BOOLEAN( x ) ( ( (x) != 0.f ) ? (GLboolean) GL_TRUE : (GLboolean) GL_FALSE )
#define FIXED_TO_BOOLEAN( x ) ( ( (x) != 0 )   ? (GLboolean) GL_TRUE : (GLboolean) GL_FALSE )

/**
 * @brief find the mali_convert_pixel_format that corresponds to a set of
 * format and type
 * @param format The format of the pixel we want the mali pixel format for
 * @param type The type of the pixel we want the mali pixel format for
 * @return the mali pixel format
 */
MALI_STATIC_INLINE enum mali_convert_pixel_format _gles_get_convert_format(GLenum format, GLenum type)
{
	switch (type)
	{
	case GL_UNSIGNED_BYTE:
		switch (format)
		{
		case GL_LUMINANCE:       return MALI_CONVERT_PIXEL_FORMAT_L8;
		case GL_LUMINANCE_ALPHA: return MALI_CONVERT_PIXEL_FORMAT_L8A8;
		case GL_ALPHA:           return MALI_CONVERT_PIXEL_FORMAT_A8;
#if RGB_IS_XRGB
		case GL_RGB:             return MALI_CONVERT_PIXEL_FORMAT_R8G8B8A8;
#else
		case GL_RGB:             return MALI_CONVERT_PIXEL_FORMAT_R8G8B8;
#endif
		case GL_RGBA:            return MALI_CONVERT_PIXEL_FORMAT_R8G8B8A8;

			/* handle errors */
		default: MALI_DEBUG_ASSERT(0, ("Invalid format 0x%x", format));
 			return MALI_CONVERT_PIXEL_FORMAT_L8; /* just some valid dummy */
		}
	case GL_UNSIGNED_SHORT_5_6_5:   return MALI_CONVERT_PIXEL_FORMAT_R5G6B5;
	case GL_UNSIGNED_SHORT_4_4_4_4: return MALI_CONVERT_PIXEL_FORMAT_R4G4B4A4;
	case GL_UNSIGNED_SHORT_5_5_5_1: return MALI_CONVERT_PIXEL_FORMAT_R5G5B5A1;

	case GL_UNSIGNED_SHORT:
		switch (format)
		{
		case GL_RGBA:            return MALI_CONVERT_PIXEL_FORMAT_R16G16B16A16;
		case GL_LUMINANCE_ALPHA: return MALI_CONVERT_PIXEL_FORMAT_L16A16;

		default: MALI_DEBUG_ASSERT(0, ("Invalid format 0x%x", format));
		}

		/* handle errors */
	default:
		MALI_DEBUG_ASSERT(0, ("Invalid type 0x%x", type));
		return MALI_CONVERT_PIXEL_FORMAT_L8; /* just some valid dummy */
	}
}

/**
 * @brief Converts a given known data-type to enum
 * @param a Pointer to the value to convert
 * @param type The data-type of a
 * @return 0 if fails, The enum calculated from input if suceeds
 */
MALI_STATIC_INLINE GLenum _gles_convert_to_enum( const GLvoid *a, const gles_datatype type )
{
	if (NULL == a) return (GLenum) 0;

	switch( type )
	{
	case GLES_FLOAT:            return (GLenum) ((GLfloat*)(a))[0];
#if MALI_USE_GLES_1
	case GLES_FIXED:            return (GLenum) ((GLfixed*)(a))[0];
#endif

	case GLES_INT:
	case GLES_NORMALIZED_INT:   return (GLenum) ((GLint*)(a))[0];

	default:
		MALI_DEBUG_ASSERT( 0, ("illegal gles datatype\n") );
		/* dummy return to avoid warnings from armcc */
		return 0;
	}
} MALI_CHECK_RESULT

  /**
   * @brief Converts an element from array of data-values to ftype
   * @param a Pointer to the array of values to convert
   * @param i The index of the value to convert
   * @param type The data-type of the elements in a
   * @return 0 on failure, The GLftype calculated from input if successful
   */
GLftype _gles_convert_element_to_ftype( const GLvoid *a, const s32 i, const gles_datatype type );

/**
 * @brief Converts an array from a given data-type to an array of GLftype
 * @param dst Pointer to the array where the result should be stored
 * @param src Pointer to the array of elements to convert
 * @param cnt The number of elements
 * @param type The data-type of the elements
 */
MALI_STATIC_INLINE void _gles_convert_array_to_ftype(GLftype *dst,
                                                     const GLvoid *src,
                                                     s32 cnt,
                                                     const gles_datatype type)
{
	int i;

	MALI_DEBUG_ASSERT_POINTER( src );
	MALI_DEBUG_ASSERT(cnt >= 0, ("negative cnt"));

	if ( NULL == dst ) return;

	for (i = 0; i < cnt; ++i)
	{
		dst[i] = _gles_convert_element_to_ftype( src, i, type);
	}
}

/**
 * @brief Converts a boolean to a given data-type
 * @param dst Pointer to the array where the result should be stored
 * @param index The position in dst where the result should be stored
 * @param a The boolean value to be converted
 * @param type The data-type of a
 */
MALI_STATIC_INLINE void _gles_convert_from_boolean( void *dst,
                                                    int index,
                                                    const GLboolean a,
                                                    const gles_datatype type )
{
	/* this should actually crash, but since openGl should avoid crashing as far
	 * as possible, let's just save the user */
	if ( NULL == dst ) return;

	switch( type )
	{
	case GLES_FLOAT:          ((GLfloat *) dst)[ index ]   = BOOLEAN_TO_FLOAT( a ); break;
#if MALI_USE_GLES_1
	case GLES_FIXED:          ((GLfixed *) dst)[ index ]   = BOOLEAN_TO_FIXED( a ); break;
#endif
	case GLES_INT:            ((GLint *) dst)[ index ]     = BOOLEAN_TO_INT( a );   break;
	case GLES_BOOLEAN:        ((GLboolean *) dst)[ index ] = a;                     break;

	default: MALI_DEBUG_ASSERT( 0, ("illegal gles datatype\n") );
	}
}

/**
 * @brief Converts a ftype to a given data-type
 * @param dst Pointer to the array where the result should be stored
 * @param index The position in dst where the result should be stored
 * @param a The ftype value to be converted
 * @param type The data-type of a
 */
MALI_STATIC_INLINE void _gles_convert_from_ftype( void *dst, int index,  const GLftype a, const gles_datatype type )
{
	/* this should actually crash, but since openGl should avoid crashing as far
	 * as possible, let's just save the user */
	if ( NULL == dst ) return;

	switch( type )
	{
	case GLES_FLOAT:          ((GLfloat *) dst)[ index ]   = FTYPE_TO_FLOAT( a );          break;
#if MALI_USE_GLES_1
	case GLES_FIXED:          ((GLfixed *) dst)[ index ]   = FTYPE_TO_FIXED( a );          break;
#endif
	case GLES_INT:            ((GLint *) dst)[ index ]     = FTYPE_TO_INT( a );            break;
	case GLES_NORMALIZED_INT: ((GLint *) dst)[ index ]     = FTYPE_TO_NORMALIZED_INT( a ); break;
	case GLES_BOOLEAN:        ((GLboolean *) dst)[ index ] = FTYPE_TO_BOOLEAN( a );        break;

	default: MALI_DEBUG_ASSERT( 0, ("illegal gles datatype\n") );
	}
}

/**
 * @brief Converts a int to a given data-type
 * @param dst Pointer to the array where the result should be stored
 * @param index The position in dst where the result should be stored
 * @param a The int value to be converted
 * @param type The data-type of a
 */
MALI_STATIC_INLINE void _gles_convert_from_int( void *dst, int index, const GLint a, const gles_datatype type )
{
	/* this should actually crash, but since openGl should avoid crashing as far
	 * as possible, let's just save the user */
	if ( NULL == dst ) return;

	switch( type )
	{
	case GLES_FLOAT:          ((GLfloat *) dst)[ index ]   = INT_TO_FTYPE( a );   break;
#if MALI_USE_GLES_1
	case GLES_FIXED:          ((GLfixed *) dst)[ index ]   = INT_TO_FIXED( a );   break;
#endif
	case GLES_INT:            ((GLint *) dst)[ index ]     = a;                   break;
	case GLES_BOOLEAN:        ((GLboolean *) dst)[ index ] = INT_TO_BOOLEAN( a ); break;

	default: MALI_DEBUG_ASSERT( 0, ("illegal gles datatype\n") );
	}
}

/**
 * @brief Converts an enum to a given data-type
 * @param dst Pointer to the array where the result should be stored
 * @param index The position in dst where the result should be stored
 * @param a The int value to be converted
 * @param type The data-type of a
 */
MALI_STATIC_INLINE void _gles_convert_from_enum( void *dst, int index, const GLenum a, const gles_datatype type )
{
	/* this should actually crash, but since openGl should avoid crashing as far as possible, let's just save the user */
	if ( NULL == dst ) return;

	switch( type )
	{
	case GLES_FLOAT:          ((GLfloat *) dst)[ index ]   = (GLfloat)a; break;
#if MALI_USE_GLES_1
	case GLES_FIXED:          ((GLfixed *) dst)[ index ]   = a; break;
#endif
	case GLES_INT:            ((GLint *) dst)[ index ]     = a; break;
	case GLES_BOOLEAN:        ((GLboolean *) dst)[ index ] = INT_TO_BOOLEAN(a); break;

	default: MALI_DEBUG_ASSERT( 0, ("illegal gles datatype\n") );
	}
}

/**
 * @brief Converts a mali_err_code into the only GLenum we can return (GL_OUT_OF_MEMORY)
 * @param err Internal mali error code
 * @return GLenum error code (only GL_OUT_OF_MEMORY or GL_NO_ERROR are returned)
 */
MALI_CHECK_RESULT GLenum _gles_convert_mali_err_do( mali_err_code err );

/**
 * @brief Fast wrapper around the above function.
 *        Converts a mali_err_code into the only GLenum we can return (GL_OUT_OF_MEMORY)
 * @param err Internal mali error code
 * @return GLenum error code (only GL_OUT_OF_MEMORY or GL_NO_ERROR are returned)
 */
MALI_STATIC_INLINE MALI_CHECK_RESULT GLenum _gles_convert_mali_err( mali_err_code err )
{
	if(err == MALI_ERR_NO_ERROR) return GL_NO_ERROR;
	return _gles_convert_mali_err_do( err );
}

#endif /* _GLES_UTIL_H_ */
