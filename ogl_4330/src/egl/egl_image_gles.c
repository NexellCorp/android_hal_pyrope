/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "egl_config_extension.h"
#if EGL_KHR_image_ENABLE
#include <mali_system.h>
#include "egl_image_gles.h"
#include "egl_thread.h"
#include "egl_misc.h"
#include "egl_common.h"
#include "egl_config.h"
#include "shared/mali_surface.h"
#include "shared/mali_shared_mem_ref.h"
#include "shared/mali_egl_image.h"

#if EGL_KHR_image_system_ENABLE
#include <ump/ump.h>
#if !defined(UMP_VERSION_MAJOR) || UMP_VERSION_MAJOR == 1
#include <ump/ump_ref_drv.h>
#endif
#endif

#ifdef EGL_MALI_GLES
egl_image* _egl_create_image_KHR_gles( egl_display *display,
                                       egl_context *context,
                                       EGLenum target,
                                       EGLClientBuffer buffer,
                                       EGLint *attr_list,
                                       void *thread_state,
                                       void *buffer_data)
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	EGLint *attrib = (EGLint*)attr_list;
	egl_image *image = NULL;
	mali_bool done = MALI_FALSE;
	u32 mipmap_level = 0;
	EGLint error = EGL_SUCCESS;
#if EGL_KHR_image_system_ENABLE
	EGLBoolean use_as_system_image = EGL_FALSE;
#endif
	MALI_IGNORE( display );

	/* verify that context is an OpenGL ES context */
	if ( EGL_OPENGL_ES_API != context->api )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return NULL;
	}

	if ( NULL != attr_list )
	{
		while ( done != MALI_TRUE )
		{
			switch ( attrib[0] )
			{
				case EGL_IMAGE_PRESERVED_KHR:
					break;
				case EGL_GL_TEXTURE_LEVEL_KHR:
					mipmap_level = attrib[1];
					break;
#if EGL_KHR_image_system_ENABLE
				case EGL_IMAGE_USE_AS_SYSTEM_IMAGE_KHR:
					if ( ( EGL_TRUE == attrib[1] ) || ( EGL_FALSE == attrib[1] ) )
					{
						use_as_system_image = attrib[1];
					}
					else
					{
						__egl_set_error( EGL_BAD_PARAMETER, tstate );
						return NULL;
					}
					break;
#endif
				case EGL_NONE: done = MALI_TRUE; break;
				default:
					__egl_set_error( EGL_BAD_PARAMETER, tstate );
					return NULL;
			}
			attrib += 2;
		}
	}

	image = _egl_create_image();
	if ( NULL == image )
	{
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return NULL;
	}

	image->target = target;
	image->buffer = buffer;

#if EGL_KHR_image_system_ENABLE
	/* if buffer data is not NULL then it means that we are not creating egl image from texture but creating it from another egl_image with ump memory */
	if ( buffer_data )
	{
		egl_image_system_buffer_properties *image_buffer = MALI_REINTERPRET_CAST(egl_image_system_buffer_properties *)buffer;

		if ( NULL == image_buffer )
		{
			_egl_destroy_image( image, EGL_TRUE );
			__egl_set_error( EGL_BAD_PARAMETER, tstate );
			return NULL;
		}

		image->buffer = image_buffer->buffer;
		if ( UMP_INVALID_MEMORY_HANDLE == ( image_buffer->ump = ump_handle_create_from_secure_id( image_buffer->ump_unique_secure_id ) ) )
		{
			_egl_destroy_image( image, EGL_TRUE );
			__egl_set_error( EGL_BAD_PARAMETER, tstate );
			return NULL;
		}

		image->image_mali = mali_image_create_from_external_memory( image_buffer->width, image_buffer->height, MALI_SURFACE_FLAG_DONT_MOVE,
			                                                &image_buffer->sformat, (void *)image_buffer->ump, MALI_IMAGE_UMP_MEM,
			                                                __egl_get_main_context()->base_ctx );
		if( NULL == image->image_mali )
		{
			image->release_buffer = (void *)image_buffer;  /* this is needed so that _egl_destroy_image frees image_buffer & image_buffer->ump */
			_egl_destroy_image( image, EGL_TRUE );
			__egl_set_error( EGL_BAD_ALLOC, tstate );
			return NULL;
		}

		_mali_sys_memcpy( image->prop, &image_buffer->prop, sizeof(egl_image_properties) );
		image->release_buffer = (void *)image_buffer;
	}
	else
#else
	MALI_IGNORE(buffer_data);
#endif
	if ( EGL_SUCCESS != (error = __egl_gles_setup_egl_image( context, target, buffer, mipmap_level, image )) )
	{
		/* if EGL_GL_TEXTURE_2D_KHR target: buffer must be the name of a complete, non-default EGL_GL_TEXTURE_2D target
		 * texture object, cast into the type EGLClientBuffer. <attr_list> should specify the mipmap level which will be used
		 * as the EGLImage source. The specified mipmap level must be part of the buffer. If not specified, default level should
		 * be used.
		 * All this is checked inside gles, and __egl_gles_setup_egl_image converts the gles errors to a proper egl error.
		 */
		_egl_destroy_image( image, EGL_TRUE );
		__egl_set_error( error, tstate );
		return NULL;
	}

#if EGL_KHR_image_system_ENABLE
	if ( use_as_system_image )
	{
		egl_image_system_buffer_properties *image_buffer = NULL;
		ump_handle ump = UMP_INVALID_MEMORY_HANDLE;
		mali_surface *surface = image->image_mali->pixel_buffer[0][0];

		if ( NULL == surface )
		{
			_egl_destroy_image( image, EGL_TRUE );
			__egl_set_error( EGL_BAD_ALLOC, tstate );
			return NULL;
		}

		if ( buffer_data )
		{
			ump = ump_handle_create_from_secure_id( ((egl_image_system_buffer_properties *)buffer)->ump_unique_secure_id );
		}
		/* check if image has ump memory, else create it and copy data */
		else if ( !_egl_make_image_sharable( image ) )
		{
			if ( UMP_INVALID_MEMORY_HANDLE != ump ) ump_reference_release( ump );

			_egl_destroy_image( image, EGL_TRUE );
			__egl_set_error( EGL_BAD_ALLOC, tstate );
			return NULL;
		}

		/* now the created egl_image has ump memory with data preserved */
		image_buffer = _mali_sys_calloc( 1, sizeof( egl_image_system_buffer_properties ) );
		if ( !image_buffer )
		{
			_egl_destroy_image( image, EGL_TRUE );
			__egl_set_error( EGL_BAD_ALLOC, tstate );
			return NULL;
		}

		image_buffer->width = image->image_mali->width;
		image_buffer->height = image->image_mali->height;
		image_buffer->ump_unique_secure_id = ump_secure_id_get( ump );
		image_buffer->ump = ump;

		/* save actual buffer info */
		if( buffer_data ) { image_buffer->buffer = ((egl_image_system_buffer_properties *)buffer)->buffer; }
		else { image_buffer->buffer = buffer; }

		_mali_sys_memcpy( &image_buffer->sformat, &surface->format, sizeof(mali_surface_specifier) );
		image->image_buffer = image_buffer;
		image->release_buffer = image_buffer;

		image->use_as_system_image = use_as_system_image;
	}
#endif

	return image;
}
#endif /* EGL_MALI_GLES */

#endif /* #if EGL_KHR_image_ENABLE */
