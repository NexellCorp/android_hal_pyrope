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
#include "egl_image_pixmap.h"
#include "egl_thread.h"
#include "egl_misc.h"
#include "egl_platform.h"
#include "egl_config.h"
#include "shared/mali_surface.h"
#include "shared/mali_shared_mem_ref.h"
#include "shared/mali_egl_image.h"

egl_image* _egl_create_image_KHR_pixmap( egl_display *display,
                                         egl_context *context,
                                         EGLClientBuffer buffer,
                                         EGLint *attr_list,
                                         void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	EGLNativePixmapType pixmap = (EGLNativePixmapType)buffer;
	EGLint *attrib = (EGLint*)attr_list;
	egl_image *image = NULL;
#if EGL_KHR_image_system_ENABLE
	EGLBoolean use_as_system_image = EGL_FALSE;
#endif

#if EGL_BACKEND_X11
	__egl_platform_flush_display( display->native_dpy );
#endif

	/* verify that we have a valid pixmap and it is valid for EGL image */
	if (( EGL_FALSE == __egl_platform_pixmap_valid( pixmap ) ) || 
		( EGL_FALSE == __egl_platform_pixmap_egl_image_compatible( pixmap, context ) ))
	{
		__egl_set_error( EGL_BAD_PARAMETER, tstate );
		return NULL;
	}

	/* Set EGL_BAD_PARAMETER if there is no EGLConfig supporting native pixmaps
	 * whose color buffer format matches the format of buffer */
	if ( EGL_FALSE == _egl_config_support_pixmap( display, pixmap ) )
	{
		__egl_set_error( EGL_BAD_PARAMETER, tstate );
		return NULL;
	}

	/* Set EGL_BAD_ACCESS if the resource dpy, ctx, target, buffer
	 * is itself an EGLImage sibling */
	if ( EGL_TRUE == _egl_image_is_sibling( display, context, buffer, EGL_NATIVE_PIXMAP_KHR, tstate ) )
	{
		__egl_set_error( EGL_BAD_ACCESS, tstate );
		return NULL;
	}

	/* Only supported attribute is EGL_IMAGE_PRESERVED_KHR, where we support preserved in any cases */
	if ( NULL != attr_list )
	{
		while ( EGL_NONE != attrib[0] )
		{
			switch ( attrib[0] )
			{
				case EGL_IMAGE_PRESERVED_KHR:
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

	image->buffer = buffer;
#if EGL_KHR_image_system_ENABLE
	if ( use_as_system_image )
	{
		image->image_buffer = (void *)buffer;
		image->use_as_system_image = use_as_system_image;
	}
#endif

	image->image_mali = __egl_platform_map_pixmap( display->native_dpy, image, (EGLNativePixmapType)pixmap );
	if ( NULL == image->image_mali )
	{
		_egl_destroy_image( image, EGL_TRUE );
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return NULL;
	}

	image->is_pixmap = EGL_TRUE;
	image->prop->colorspace = __egl_platform_get_pixmap_colorspace( pixmap );

#if MALI_EGL_IMAGE_SUPPORT_NATIVE_YUV
	_egl_image_set_default_properties( image->prop );
#endif

	return image;
}
#endif /* #if EGL_KHR_image_ENABLE */
