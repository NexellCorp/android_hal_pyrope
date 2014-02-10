/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <mali_system.h>
#include <shared/mali_egl_image.h>
#include <shared/mali_image.h>
#include <shared/mali_surface.h>
#include <shared/mali_shared_mem_ref.h>
#include "egl_thread.h"
#include "egl_image.h"
#include "egl_handle.h"
#include "egl_entrypoints.h"

/* for non-monolithic builds */
#ifndef MALI_EGL_IMAGE_APIENTRY
#define MALI_EGL_IMAGE_APIENTRY(a) a
#endif

MALI_STATIC mali_egl_image_version current_mali_egl_image_version = MALI_EGL_IMAGE_VERSION_HEAD;

/* structure used when parsing attributes */
typedef struct mali_egl_image_attributes
{
	EGLint miplevel;
	EGLint plane;
	mali_image_access_mode access_mode;
} mali_egl_image_attributes;

/**
 * @brief Sets the error state
 * @param error enum for error
 */
void mali_egl_image_set_error( EGLenum error )
{
	mali_err_code err;

	err = _mali_sys_thread_key_set_data( MALI_THREAD_KEY_MALI_EGL_IMAGE, (void*)error );

	MALI_IGNORE( err );
}

MALI_EGL_IMAGE_EXPORT EGLBoolean MALI_EGL_IMAGE_APIENTRY(mali_egl_image_init)( mali_egl_image_version version )
{
	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );

	if ( version > MALI_EGL_IMAGE_VERSION_HEAD || version < 1 )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_VERSION );
		return EGL_FALSE;
	}

	/* set the current version */
	current_mali_egl_image_version = version;

	MALI_IGNORE( current_mali_egl_image_version );

	return EGL_TRUE;
}

MALI_EGL_IMAGE_EXPORT EGLint MALI_EGL_IMAGE_APIENTRY(mali_egl_image_get_error)( void )
{
	return (EGLint)_mali_sys_thread_key_get_data( MALI_THREAD_KEY_MALI_EGL_IMAGE );
}

/**
 * @brief Verifies that an image is valid
 * @param imgptr pointer to egl_image
 * @return EGL_FALSE if not valid, EGL_TRUE if valid
 * @note sets error state
 * @note will verify that the given handle is valid and locked
 */
EGLBoolean mali_egl_image_verify_image( egl_image *imgptr )
{
	if ( EGL_NO_IMAGE_KHR == __egl_get_image_handle( imgptr ) )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_POINTER );
		return EGL_FALSE;
	}

	/* check that image has been locked by doing an attempt at locking it again
	 * if this succeeds, we know that the image was not locked to begin with
	 */
	if ( EGL_TRUE == __egl_lock_image( imgptr ) )
	{
		__egl_unlock_image( imgptr );
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_LOCK );
		return EGL_FALSE;
	}


	return EGL_TRUE;
}

/**
 * @brief Parses an attribute list given to the mali_egl_image API functions.
 * @param attrib_list pointer to input attribute list
 * @param image_attributes parsed result of given attribute list
 * @note NULL is allowed as attrib_list, where default values for miplevel and plane is 0 and MALI_EGL_IMAGE_PLANE_Y
 */
mali_surface* mali_egl_image_parse_attribute_list( mali_image *image, EGLint *attrib_list, mali_egl_image_attributes *image_attributes )
{
	mali_bool done = MALI_FALSE;
	mali_surface *surface = NULL;
	EGLint *attrib = (EGLint*)attrib_list;

	MALI_DEBUG_ASSERT_POINTER( image );
	MALI_DEBUG_ASSERT_POINTER( image_attributes );

	/* set default values */
	image_attributes->miplevel = 0;
	image_attributes->plane = MALI_EGL_IMAGE_PLANE_Y - MALI_EGL_IMAGE_PLANE_Y;
	image_attributes->access_mode = (mali_image_access_mode)MALI_EGL_IMAGE_ACCESS_READ_WRITE;

	if ( NULL != attrib_list )
	{
		while ( done != MALI_TRUE )
		{
			switch ( attrib[0] )
			{
				case MALI_EGL_IMAGE_PLANE:
					switch ( attrib[1] )
					{
						case MALI_EGL_IMAGE_PLANE_Y:
						case MALI_EGL_IMAGE_PLANE_U:
						case MALI_EGL_IMAGE_PLANE_V:
						case MALI_EGL_IMAGE_PLANE_UV:
						case MALI_EGL_IMAGE_PLANE_YUV:
							image_attributes->plane = attrib[1] - MALI_EGL_IMAGE_PLANE_Y;
							break;
						default:
							mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_ATTRIBUTE );
							return NULL;
					}
					break;

				case MALI_EGL_IMAGE_MIPLEVEL:
					/* allowed range for miplevel is 0..12 */
					if ( (attrib[1] < 0) || (attrib[1] > MALI_IMAGE_MAX_MIPLEVELS) )
					{
						mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_ATTRIBUTE );
						return NULL;
					}
					image_attributes->miplevel = attrib[1];
					break;

				case MALI_EGL_IMAGE_ACCESS_MODE:
					switch ( attrib[1] )
					{
						case MALI_EGL_IMAGE_ACCESS_READ_ONLY:  image_attributes->access_mode = MALI_IMAGE_ACCESS_READ_ONLY;  break;
						case MALI_EGL_IMAGE_ACCESS_WRITE_ONLY: image_attributes->access_mode = MALI_IMAGE_ACCESS_WRITE_ONLY; break;
						case MALI_EGL_IMAGE_ACCESS_READ_WRITE: image_attributes->access_mode = MALI_IMAGE_ACCESS_READ_WRITE; break;
						default:
							mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_ATTRIBUTE );
							return NULL;
					}
					break;

				case EGL_NONE:
				case MALI_EGL_IMAGE_NONE: done = MALI_TRUE; break;
				default:
					mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
					return NULL;
			}
			attrib += 2;
		}
	}

	/* verify that buffer exists in given egl_image pointer, set MALI_EGL_IMAGE_BAD_ACCESS if not */
	surface = mali_image_get_buffer( image, image_attributes->plane, image_attributes->miplevel, MALI_TRUE );
	if ( NULL == surface ) mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_ACCESS );

	return surface;
}

/**
 * @brief Parses an attribute list given to the mali_egl_image_create API function.
 * @param attrib_list pointer to input attribute list
 * @param image_attributes parsed result of given attribute list
 * @param width parsed width of image
 * @param height parsed height of image
 * @param alloc_data parsed result of either implicit or explicit allocation of buffer data
 * @return EGL_TRUE on success, EGL_FALSE on failure
 * @note internal error state updated on failure
 * @note NULL is allowed as attrib_list, where default values will be used
 */
EGLBoolean mali_egl_image_create_parse_attribute_list( EGLint *attrib_list, egl_image_properties *image_attributes, EGLint *width, EGLint *height, EGLBoolean *alloc_data )
{
	mali_bool done = MALI_FALSE;
	EGLint *attrib = (EGLint*)attrib_list;

	MALI_DEBUG_ASSERT_POINTER( image_attributes );

	/* set default values */
	_egl_image_set_default_properties( image_attributes );

	*width = 0;
	*height = 0;
	*alloc_data = EGL_TRUE;

	if ( NULL != attrib_list )
	{
		while ( done != MALI_TRUE )
		{
			switch ( attrib[0] )
			{
				case MALI_EGL_IMAGE_FORMAT:
					if (
						attrib[1] != MALI_EGL_IMAGE_PLANE_RGB &&
						attrib[1] != MALI_EGL_IMAGE_FORMAT_YUV420_PLANAR &&
						attrib[1] != EGL_YUV420P_KHR &&
						attrib[1] != EGL_YVU420SP_KHR &&
						attrib[1] != EGL_YUV420SP_KHR &&
						attrib[1] != EGL_YV12_KHR
						#ifdef NEXELL_ANDROID_FEATURE_EGL_YV12_KHR_444_FORMAT
						&& attrib[1] != EGL_YV12_KHR_444
						#endif
					   )
					{
						mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
						return EGL_FALSE;
					}
					/* TODO: translate format value to be compatible with mali_image */
					image_attributes->image_format = attrib[1];
					break;
				case MALI_EGL_IMAGE_WIDTH:
					if ( (attrib[1] < 0) || (attrib[1] > EGL_SURFACE_MAX_WIDTH) )
					{
						mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
						return EGL_FALSE;
					}
					*width = attrib[1];
					break;

				case MALI_EGL_IMAGE_HEIGHT:
					if ( (attrib[1] < 0) || (attrib[1] > EGL_SURFACE_MAX_HEIGHT) )
					{
						mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
						return EGL_FALSE;
					}
					*height = attrib[1];
					break;

				case MALI_EGL_IMAGE_ALLOCATE_IMAGE_DATA:
					if ( attrib[1] != EGL_TRUE && attrib[1] != EGL_FALSE )
					{
						mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
						return EGL_FALSE;
					}
					*alloc_data = attrib[1];
					break;

				case MALI_EGL_IMAGE_LAYOUT:
					if ( attrib[1] != MALI_EGL_IMAGE_LAYOUT_LINEAR && attrib[1] != MALI_EGL_IMAGE_LAYOUT_BLOCKINTERLEAVED )
					{
						mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
						return EGL_FALSE;
					}
					image_attributes->layout = attrib[1];
					break;

				case MALI_EGL_IMAGE_RANGE:
					if ( MALI_EGL_IMAGE_RANGE_FULL != attrib[1] && MALI_EGL_IMAGE_RANGE_REDUCED != attrib[1] )
					{
						mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
						return EGL_FALSE;
					}

					if ( MALI_EGL_IMAGE_RANGE_FULL == attrib[1] ) image_attributes->colorrange = EGL_FULL_RANGE_KHR;
					else image_attributes->colorrange = EGL_REDUCED_RANGE_KHR;
					break;

				case MALI_EGL_IMAGE_COLORSPACE:
					switch ( attrib[1] )
					{
						case MALI_EGL_IMAGE_COLORSPACE_LINEAR:
						case MALI_EGL_IMAGE_COLORSPACE_SRGB:
						case MALI_EGL_IMAGE_COLORSPACE_BT_601:
						case MALI_EGL_IMAGE_COLORSPACE_BT_709:
							image_attributes->colorspace = attrib[1];
							break;

						default:
							mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
							return EGL_FALSE;

					}
					break;

				case MALI_EGL_IMAGE_ALPHA_FORMAT:
					if ( MALI_EGL_IMAGE_ALPHA_PRE != attrib[1] && MALI_EGL_IMAGE_ALPHA_NONPRE != attrib[1] )
					{
						mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
						return EGL_FALSE;
					}
					if ( MALI_EGL_IMAGE_ALPHA_PRE == attrib[1] ) image_attributes->alpha_format = EGL_ALPHA_FORMAT_PRE;
					else image_attributes->alpha_format = EGL_ALPHA_FORMAT_NONPRE;
					break;

				case MALI_EGL_IMAGE_BITS_R:
					if ( attrib[1] >8 || attrib[1] < 0 )
					{
						mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
						return EGL_FALSE;
					}
					image_attributes->min_bits[0] = attrib[1];
					break;

				case MALI_EGL_IMAGE_BITS_G:
					if ( attrib[1] >8 || attrib[1] < 0 )
					{
						mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
						return EGL_FALSE;
					}
					image_attributes->min_bits[1] = attrib[1];
					break;

				case MALI_EGL_IMAGE_BITS_B:
					if ( attrib[1] >8 || attrib[1] < 0 )
					{
						mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
						return EGL_FALSE;
					}
					image_attributes->min_bits[2]= attrib[1];
					break;

				case MALI_EGL_IMAGE_BITS_A:
					if ( attrib[1] >8 || attrib[1] < 0 )
					{
						mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
						return EGL_FALSE;
					}
					image_attributes->min_bits[3] = attrib[1];
					break;


				case EGL_NONE:
				case MALI_EGL_IMAGE_NONE: done = MALI_TRUE; break;
				default:
					mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_ATTRIBUTE );
					return EGL_FALSE;
			}
			attrib += 2;
		}
	}
	return EGL_TRUE;
}


MALI_EGL_IMAGE_EXPORT mali_egl_image* MALI_EGL_IMAGE_APIENTRY(mali_egl_image_lock_ptr)( EGLImageKHR image )
{
	__egl_thread_state *tstate = NULL;
	egl_image *imgptr = NULL;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );

	/* lock the EGL main context so that no concurrent changes can be
	 * made to the EGLImage
	 */
	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if ( NULL == tstate )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_IMAGE );
		return NULL;
	}

	imgptr = __egl_get_image_ptr( image );
	if ( NULL == imgptr )
	{
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_IMAGE );
		return NULL;
	}

	if ( EGL_FALSE == __egl_lock_image( imgptr ) )
	{
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_LOCK );
		return NULL;
	}

	__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );

	return MALI_REINTERPRET_CAST(mali_egl_image *)imgptr;
}

MALI_EGL_IMAGE_EXPORT EGLBoolean MALI_EGL_IMAGE_APIENTRY(mali_egl_image_unlock_ptr)( EGLImageKHR image )
{
	egl_image *imgptr = NULL;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );

	/* verify the image handle */
	imgptr = __egl_get_image_ptr( image );
	if ( NULL == imgptr )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_IMAGE );
		return EGL_FALSE;
	}

	/* unmap the surface if it is mapped */
	mali_image_unlock_all_sessions( imgptr->image_mali );
	imgptr->current_session_id = -1;

	if ( EGL_FALSE == __egl_unlock_image( imgptr ) )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_LOCK );
		return EGL_FALSE;
	}

	/* destroy the image if EGLImageDestroyKHR has been called */
	if ( MALI_FALSE == imgptr->is_valid )
	{
		_egl_destroy_image ( imgptr, EGL_FALSE );
	}

	return EGL_TRUE;
}

MALI_EGL_IMAGE_EXPORT EGLBoolean MALI_EGL_IMAGE_APIENTRY(mali_egl_image_set_data)( mali_egl_image *image, EGLint *attribs, void *data )
{
	mali_image_err_code retval = MALI_IMAGE_ERR_NO_ERROR;
	mali_egl_image_attributes image_attributes;
	mali_surface *surface = NULL;
	egl_image *imgptr = MALI_REINTERPRET_CAST(egl_image*)image;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );

	/* verify valid image, and that it's not in use */
	if ( EGL_FALSE == mali_egl_image_verify_image( imgptr ) ) return EGL_FALSE;

	/* parse the attribute list, validate buffer selection */
	surface = mali_egl_image_parse_attribute_list( imgptr->image_mali, attribs, &image_attributes );
	if ( NULL == surface ) return EGL_FALSE;

	retval = mali_image_set_data( imgptr->image_mali, image_attributes.plane, image_attributes.miplevel, 0, data );

	if ( MALI_IMAGE_ERR_NO_ERROR == retval ) return EGL_TRUE;

	/* translate mali_image_err_code into eglimage error code */
	switch ( retval )
	{
		case MALI_IMAGE_ERR_BAD_ACCESS_MODE:  mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_ACCESS );    break;
		case MALI_IMAGE_ERR_BAD_BUFFER:       mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_ACCESS );    break;
		case MALI_IMAGE_ERR_IN_USE:           mali_egl_image_set_error( MALI_EGL_IMAGE_IN_USE );        break;
		case MALI_IMAGE_ERR_BAD_PARAMETER:    mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER ); break;
		default: MALI_DEBUG_PRINT( 0, ("MALI_EGL_IMAGE: Unhandled retval (0x%X) in %s\n", retval, MALI_FUNCTION) ); break;
	}

	return EGL_FALSE;
}

MALI_EGL_IMAGE_EXPORT EGLBoolean MALI_EGL_IMAGE_APIENTRY(mali_egl_image_get_width)( mali_egl_image *image, EGLint *width )
{
	egl_image *imgptr = MALI_REINTERPRET_CAST(egl_image *)image;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );

	if ( EGL_FALSE == mali_egl_image_verify_image( imgptr ) ) return EGL_FALSE;

	if ( NULL == width )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
		return EGL_FALSE;
	}

	*width = imgptr->image_mali->width;

	return EGL_TRUE;
}

MALI_EGL_IMAGE_EXPORT EGLBoolean MALI_EGL_IMAGE_APIENTRY(mali_egl_image_get_height)( mali_egl_image *image, EGLint *height )
{
	egl_image *imgptr = MALI_REINTERPRET_CAST(egl_image *)image;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );

	if ( EGL_FALSE == mali_egl_image_verify_image( imgptr ) ) return EGL_FALSE;

	if ( NULL == height )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
		return EGL_FALSE;
	}

	*height = imgptr->image_mali->height;

	return EGL_TRUE;
}

MALI_EGL_IMAGE_EXPORT EGLBoolean MALI_EGL_IMAGE_APIENTRY(mali_egl_image_get_format)( mali_egl_image *image, EGLint *format )
{
	egl_image *imgptr = MALI_REINTERPRET_CAST(egl_image *)image;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );

	if ( EGL_FALSE == mali_egl_image_verify_image( imgptr ) ) return EGL_FALSE;

	if ( NULL == format )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
		return EGL_FALSE;
	}

	if ( NULL != imgptr->image_mali->yuv_info )
	{
		*format = imgptr->image_mali->yuv_info->format;
	}
	else *format = EGL_NONE;

	return EGL_TRUE;
}

MALI_EGL_IMAGE_EXPORT EGLBoolean MALI_EGL_IMAGE_APIENTRY(mali_egl_image_get_miplevels)( mali_egl_image *image, EGLint *miplevels )
{
	egl_image *imgptr = MALI_REINTERPRET_CAST(egl_image *)image;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );

	if ( EGL_FALSE == mali_egl_image_verify_image( imgptr ) ) return EGL_FALSE;

	if ( NULL == miplevels )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
		return EGL_FALSE;
	}

	*miplevels = imgptr->prop->miplevels;

	return EGL_TRUE;
}


MALI_EGL_IMAGE_EXPORT void* MALI_EGL_IMAGE_APIENTRY(mali_egl_image_map_buffer)( mali_egl_image *image, EGLint *attribs )
{
	mali_egl_image_attributes image_attributes;
	mali_image_err_code retval;
	mali_surface *surface = NULL;
	s32 session_id = 0;
	void *data = NULL;
	egl_image *imgptr = MALI_REINTERPRET_CAST( egl_image *)image;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );

	if ( EGL_FALSE == mali_egl_image_verify_image( imgptr ) ) return NULL;

	/* parse the attribute list, validate buffer selection */
	surface = mali_egl_image_parse_attribute_list( imgptr->image_mali, attribs, &image_attributes );
	if ( NULL == surface ) return NULL;

	retval = mali_image_lock( imgptr->image_mali, image_attributes.access_mode,
	                          image_attributes.plane, image_attributes.miplevel,
                             0, 0, surface->format.width, surface->format.height,
                             MALI_FALSE, MALI_FALSE,
                             &session_id, &data );

	/* translate MALI_IMAGE errors into MALI_EGL_IMAGE errors */
	switch ( retval )
	{
		case MALI_IMAGE_ERR_BAD_PARAMETER:    mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER ); break;
		case MALI_IMAGE_ERR_BAD_BUFFER:       mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_ACCESS );    break;
		case MALI_IMAGE_ERR_IN_USE:           mali_egl_image_set_error( MALI_EGL_IMAGE_IN_USE );        break;
		case MALI_IMAGE_ERR_BAD_ALLOC:
		case MALI_IMAGE_ERR_BAD_LOCK:         mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_MAP );       break;
		case MALI_IMAGE_ERR_NO_ERROR:
			break;
		default: MALI_DEBUG_PRINT( 0, ("MALI_EGL_IMAGE: Unhandled retval (0x%X) in %s\n", retval, MALI_FUNCTION) ); break;
	}

	if ( 0 != session_id ) imgptr->current_session_id = session_id;

	return data;
}

MALI_EGL_IMAGE_EXPORT EGLBoolean MALI_EGL_IMAGE_APIENTRY(mali_egl_image_unmap_buffer)( mali_egl_image *image, EGLint *attribs )
{
	mali_surface *surface = NULL;
	mali_egl_image_attributes image_attributes;
	mali_image_err_code retval;
	egl_image *imgptr = MALI_REINTERPRET_CAST(egl_image *)image;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );
	if ( EGL_FALSE == mali_egl_image_verify_image( imgptr ) ) return EGL_FALSE;

	/* parse the attribute list, validate buffer selection */
	surface = mali_egl_image_parse_attribute_list( imgptr->image_mali, attribs, &image_attributes );
	if ( NULL == surface ) return EGL_FALSE;

	retval = mali_image_unlock( imgptr->image_mali,
	                            image_attributes.plane, image_attributes.miplevel,
	                            0, 0, surface->format.width, surface->format.height, imgptr->current_session_id );

	if ( MALI_IMAGE_ERR_NO_ERROR != retval )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_MAP );
		return EGL_FALSE;
	}

	imgptr->current_session_id = -1;

	return EGL_TRUE;
}

MALI_EGL_IMAGE_EXPORT EGLBoolean MALI_EGL_IMAGE_APIENTRY(mali_egl_image_get_buffer_width)( mali_egl_image *image, EGLint *attribs, EGLint *width )
{
	mali_surface *surface = NULL;
	mali_egl_image_attributes image_attributes;
	egl_image *imgptr = MALI_REINTERPRET_CAST(egl_image *)image;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );

	if ( EGL_FALSE == mali_egl_image_verify_image( imgptr ) ) return EGL_FALSE;

	/* parse the attribute list, validate buffer selection */
	surface = mali_egl_image_parse_attribute_list( imgptr->image_mali, attribs, &image_attributes );
	if ( NULL == surface ) return EGL_FALSE;

	if ( NULL == width )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
		return EGL_FALSE;
	}

	*width = surface->format.width;

	return EGL_TRUE;
}

MALI_EGL_IMAGE_EXPORT EGLBoolean MALI_EGL_IMAGE_APIENTRY(mali_egl_image_get_buffer_height)( mali_egl_image *image, EGLint *attribs, EGLint *height )
{
	mali_surface *surface = NULL;
	mali_egl_image_attributes image_attributes;
	egl_image *imgptr = MALI_REINTERPRET_CAST(egl_image *)image;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );

	if ( EGL_FALSE == mali_egl_image_verify_image( imgptr ) ) return EGL_FALSE;

	/* parse the attribute list, validate buffer selection */
	surface = mali_egl_image_parse_attribute_list( imgptr->image_mali, attribs, &image_attributes );
	if ( NULL == surface ) return EGL_FALSE;

	if ( NULL == height )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
		return EGL_FALSE;
	}

	*height = surface->format.height;

	return EGL_TRUE;
}

MALI_EGL_IMAGE_EXPORT EGLBoolean MALI_EGL_IMAGE_APIENTRY(mali_egl_image_get_buffer_secure_id)( mali_egl_image *image, EGLint *attribs, EGLint *secure_id )
{
#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	ump_handle ump_mem_handle;
	mali_surface *surface = NULL;
	mali_egl_image_attributes image_attributes;
	egl_image *imgptr = MALI_REINTERPRET_CAST(egl_image *)image;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );

	if ( EGL_FALSE == mali_egl_image_verify_image( imgptr ) ) return EGL_FALSE;

	/* parse the attribute list, validate buffer selection */
	surface = mali_egl_image_parse_attribute_list( imgptr->image_mali, attribs, &image_attributes );
	if ( NULL == surface ) return EGL_FALSE;

	if ( NULL == secure_id )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
		return EGL_FALSE;
	}

	ump_mem_handle = _mali_mem_get_ump_memory( surface->mem_ref->mali_memory );
	if ( UMP_INVALID_MEMORY_HANDLE == ump_mem_handle )
	{
        *secure_id = (EGLint)UMP_INVALID_SECURE_ID;
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
		return EGL_FALSE;
	}

	*secure_id = (EGLint)ump_secure_id_get( ump_mem_handle );
	if ( UMP_INVALID_SECURE_ID == (u32)*secure_id )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
		return EGL_FALSE;
	}

	return EGL_TRUE;
#else
	MALI_IGNORE(image);
	MALI_IGNORE(attribs);
	MALI_IGNORE(secure_id);
	mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_MAP );
	return EGL_FALSE;
#endif
}

MALI_EGL_IMAGE_EXPORT EGLBoolean MALI_EGL_IMAGE_APIENTRY(mali_egl_image_get_buffer_layout)( mali_egl_image *image, EGLint *attribs, EGLint *layout )
{
	egl_image *imgptr = MALI_REINTERPRET_CAST(egl_image *)image;
	mali_surface *surface = NULL;
	mali_pixel_layout internal_layout;
	mali_egl_image_attributes image_attributes;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );

	if ( EGL_FALSE == mali_egl_image_verify_image( imgptr ) ) return EGL_FALSE;

	/* parse the attribute list, validate buffer selection */
	surface = mali_egl_image_parse_attribute_list( imgptr->image_mali, attribs, &image_attributes );
	if ( NULL == surface ) return EGL_FALSE;

	if ( NULL == layout )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_PARAMETER );
		return EGL_FALSE;
	}

	internal_layout = surface->format.pixel_layout;
	switch ( internal_layout )
	{
		case MALI_PIXEL_LAYOUT_LINEAR:              *layout = MALI_EGL_IMAGE_LAYOUT_LINEAR; break;
		case MALI_PIXEL_LAYOUT_INTERLEAVED_BLOCKS:  *layout = MALI_EGL_IMAGE_LAYOUT_BLOCKINTERLEAVED; break;
		default:
			MALI_DEBUG_ASSERT(0, ("Unknown internal layout"));
			return EGL_FALSE;
	}

	return EGL_TRUE;
}

MALI_EGL_IMAGE_EXPORT EGLBoolean MALI_EGL_IMAGE_APIENTRY(mali_egl_image_create_sync)( mali_egl_image *image )
{
	egl_image *imgptr = MALI_REINTERPRET_CAST(egl_image *)image;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );
	if ( EGL_FALSE == mali_egl_image_verify_image( imgptr ) ) return EGL_FALSE;

	if ( MALI_FALSE == _mali_image_create_sync_handle( imgptr->image_mali ) )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_ACCESS );
		return EGL_FALSE;
	}

	return EGL_TRUE;
}

MALI_EGL_IMAGE_EXPORT EGLBoolean MALI_EGL_IMAGE_APIENTRY(mali_egl_image_set_sync)( mali_egl_image *image )
{
	egl_image *imgptr = MALI_REINTERPRET_CAST(egl_image *)image;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );
	if ( EGL_FALSE == mali_egl_image_verify_image( imgptr ) ) return EGL_FALSE;

	if ( MALI_FALSE == _mali_image_lock_sync_handle( imgptr->image_mali ) )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_LOCK );
		return EGL_FALSE;
	}

	return EGL_TRUE;
}

MALI_EGL_IMAGE_EXPORT EGLBoolean MALI_EGL_IMAGE_APIENTRY(mali_egl_image_unset_sync)( mali_egl_image *image )
{
	egl_image *imgptr = MALI_REINTERPRET_CAST(egl_image *)image;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );
	if ( EGL_FALSE == mali_egl_image_verify_image( imgptr ) ) return EGL_FALSE;

	if ( MALI_FALSE == _mali_image_unlock_sync_handle( imgptr->image_mali ) )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_LOCK );
		return EGL_FALSE;
	}

	return EGL_TRUE;
}

MALI_EGL_IMAGE_EXPORT EGLBoolean MALI_EGL_IMAGE_APIENTRY(mali_egl_image_wait_sync)( mali_egl_image *image, EGLint timeout )
{
	egl_image *imgptr = MALI_REINTERPRET_CAST(egl_image *)image;
	mali_image *surface = NULL;
	mali_ds_consumer_handle temp_consumer;
	mali_err_code err;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );
	if ( EGL_FALSE == mali_egl_image_verify_image( imgptr ) ) return EGL_FALSE;

/*	surface = imgptr->image_mali->pixel_buffer[0][0]; */
	surface = imgptr->image_mali;

	if ( MALI_FALSE == _mali_image_lock_sync_handle( imgptr->image_mali ) )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_LOCK );
		return EGL_FALSE;
	}

	/* NOTE: No longer timing out! */
	temp_consumer = mali_ds_consumer_allocate(surface->base_ctx, image, NULL, NULL );
	if(MALI_NO_HANDLE == temp_consumer) return EGL_FALSE;

	/* set up a dependency. */
	err = mali_ds_connect(temp_consumer, surface->pixel_buffer[0][0]->ds_resource, MALI_DS_READ);
	if(MALI_ERR_NO_ERROR != err)
	{
		/* kill all references, free temp consumer. */
		mali_ds_consumer_release_all_connections( temp_consumer );
		mali_ds_consumer_free(temp_consumer);
		mali_egl_image_set_error( MALI_IMAGE_ERR_BAD_ALLOC );
		return EGL_FALSE;
	}

	/* flush the temp consumer. This will wait until noone is using the mali_surface anymore.
	 * Note: There is no timeout on this. That is a deliberate breaking of the API */
	MALI_IGNORE(timeout);

	err = mali_ds_consumer_flush_and_wait( temp_consumer , VR_PROFILING_EVENT_REASON_SUSPEND_RESUME_SW_VR_EGL_IMAGE_SYNC_WAIT );
	if(MALI_ERR_NO_ERROR != err)
	{
		/* kill all references, free temp consumer. */
		mali_ds_consumer_release_all_connections( temp_consumer );
		mali_ds_consumer_free(temp_consumer);
		mali_egl_image_set_error( MALI_IMAGE_ERR_BAD_ALLOC );
		return EGL_FALSE;
	}

	_mali_image_unlock_sync_handle( surface );

	return EGL_TRUE;
}


MALI_EGL_IMAGE_EXPORT EGLImageKHR MALI_EGL_IMAGE_APIENTRY(mali_egl_image_create)( EGLDisplay display, EGLint *attribs )
{
	__egl_thread_state *tstate = NULL;
	egl_image_properties image_attributes_egl;
	EGLint width, height = 0;
	EGLBoolean alloc_data;
	EGLImageKHR egl_image = EGL_NO_IMAGE_KHR;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if ( NULL == tstate )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_OUT_OF_MEMORY );
		return EGL_FALSE;
	}

	_mali_sys_memset( &image_attributes_egl, 0, sizeof(egl_image_properties) );
	if ( EGL_FALSE == mali_egl_image_create_parse_attribute_list( attribs, &image_attributes_egl, &width, &height, &alloc_data ) )
	{
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
		/* error should already be set */
		return EGL_NO_IMAGE_KHR;
	}

	egl_image = _egl_create_image_internal( display, &image_attributes_egl, width, height, tstate );
	if ( EGL_NO_IMAGE_KHR == egl_image )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_OUT_OF_MEMORY );
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
		return EGL_NO_IMAGE_KHR;
	}

	__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );

	return egl_image;
}

MALI_EGL_IMAGE_EXPORT EGLBoolean MALI_EGL_IMAGE_APIENTRY(mali_egl_image_destroy)( EGLImageKHR image )
{
	__egl_thread_state *tstate = NULL;
	egl_image *imgptr = NULL;

	mali_egl_image_set_error( MALI_EGL_IMAGE_SUCCESS );

	/* lock the EGL main context so that no concurrent changes can be
	 * made to the EGLImage
	 */
	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_ALL_LOCK );
	if ( NULL == tstate )
	{
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_IMAGE );
		return EGL_FALSE;
	}

	imgptr = __egl_get_image_ptr( image );
	if ( NULL == imgptr )
	{
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );
		mali_egl_image_set_error( MALI_EGL_IMAGE_BAD_IMAGE );
		return EGL_FALSE;
	}

	_egl_destroy_image( imgptr, EGL_TRUE );

	__egl_release_current_thread_state( EGL_MAIN_MUTEX_ALL_UNLOCK );

	return EGL_TRUE;
}
