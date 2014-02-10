/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_api_trace.c
 * @brief Helper functions common for EGL API tracing
 */

#include <base/mali_macros.h>
#include <base/mali_debug.h>
#include <shared/mali_surface.h>
#include <shared/mali_surface_specifier.h>
#include <shared/mali_image.h>
#include <EGL/egl_api_trace_format.h>

#include "egl_api_trace.h"
#include "egl_display.h"
#include "egl_surface.h"
#include "egl_thread.h"
#include "egl_handle.h"
#include "egl_platform.h"

void mali_api_trace_add_eglnativewindowtype(EGLNativeWindowType win)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		if (NULL != win)
		{
			/*
			 * Dump the internal details of the native window (which is outside our control).
			 * Use the MALI_API_TRACE_ID0, so that it is not interpreted as a parameter,
			 * but rather just as information (which will be needed in order to recreate
			 * this native struct).
			 * We insert this data into the trace output when
			 * 1) This is given as a parameter (eglCreateWindowSurface).
			 * 2) eglSwapBuffers has detected a resize.
			 */
			u32 data[EGL_WINDOW_ATTRIB_LAST];
			EGLBoolean got_size;
			
			got_size = __egl_platform_get_window_size(win, NULL, &data[EGL_WINDOW_ATTRIB_WIDTH], &data[EGL_WINDOW_ATTRIB_HEIGHT]);
			if (got_size == EGL_FALSE)
			{
				MALI_DEBUG_PRINT(1, ("EGL : Failed to get size of window 0x%x (API trace)\n", (unsigned int)win));
			}
			else
			{
				data[EGL_WINDOW_ATTRIB_HANDLE] = (u32)win;

				mali_api_trace_param_uint32_array(MALI_API_TRACE_IN|MALI_API_TRACE_ID0, data, MALI_ARRAY_SIZE(data), "eglnativewindowtype_size");
			}
		}
	}
}

/**
 * Add a EGLNativePixmapType to the trace output (excluding the pixmap data itself).
 * This is not added as a parameter, because this is not always given as input,
 * and the content of this type is not controlled by us.
 * @param win The EGLNativePixmapType to add to the trace output
 */
static void mali_api_trace_add_eglnativepixmaptype(EGLNativePixmapType pixmap)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		if (NULL != pixmap)
		{
			/*
			 * Dump the internal details of the native_pixmap struct (which is outside our control).
			 * Use the MALI_API_TRACE_ID0, so that it is not interpreted as a parameter,
			 * but rather just as information (which will be needed in order to recreate
			 * this native struct).
			 */
			u32 data[EGL_PIXMAP_ATTRIB_LAST];
			mali_surface_specifier sformat;

			data[EGL_PIXMAP_ATTRIB_HANDLE] = (u32)pixmap;
			data[EGL_PIXMAP_ATTRIB_UMP] = __egl_platform_pixmap_support_ump(pixmap) ? 1 : 0;
				
			__egl_platform_get_pixmap_size((EGLNativeDisplayType) NULL, pixmap, &data[EGL_PIXMAP_ATTRIB_WIDTH], &data[EGL_PIXMAP_ATTRIB_HEIGHT], &data[EGL_PIXMAP_ATTRIB_PITCH]);

			__egl_platform_get_pixmap_format((EGLNativeDisplayType) NULL, pixmap, &sformat);
			data[EGL_PIXMAP_ATTRIB_PIXEL_FORMAT] = sformat.pixel_format;
			data[EGL_PIXMAP_ATTRIB_TEXEL_FORMAT] = sformat.texel_format;
			data[EGL_PIXMAP_ATTRIB_PIXEL_LAYOUT] = sformat.pixel_layout;
			data[EGL_PIXMAP_ATTRIB_TEXEL_LAYOUT] = sformat.texel_layout;
			data[EGL_PIXMAP_ATTRIB_RED_BLUE_SWAP] = sformat.red_blue_swap;
			data[EGL_PIXMAP_ATTRIB_REVERSE_ORDER] = sformat.reverse_order;

			mali_api_trace_param_uint32_array(MALI_API_TRACE_IN|MALI_API_TRACE_ID0, data, MALI_ARRAY_SIZE(data), "eglnativepixmaptype_format");
		}
	}
}

/**
 * Add pixmap data for a EGLNativePixmapType to the trace output.
 * Pixmaps are updated outside the control of the APIs, so this
 * data is grabbed when a native pixmap surface is created and eglWaitNative() is called.
 * @param win The EGLNativePixmapType pixmap data to add to the trace output
 */
void mali_api_trace_add_pixmap_data(EGLNativePixmapType pixmap)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		if (NULL != pixmap)
		{
			/*
			 * The internal details of the native_pixmap struct should already be dumped by
			 * mali_api_trace_add_eglnativepixmaptype. All we do here is dump the content of the pixmap.
			 * Use the MALI_API_TRACE_ID0, so that it is not interpreted as a parameter,
			 * but rather just as information (which will be needed in order to recreate
			 * this native struct).
			 */
			mali_image* image;

			/* Create a Mali image from the native pixmap, with one surface at
			 * plane 0 and mipmap level 0. The pixmap data is copied into internal
			 * mali memory if it cannot be shared via UMP. Some native pixmaps
			 * cannot be read directly.
			 */
			image = __egl_platform_map_pixmap((EGLNativeDisplayType) NULL,NULL,pixmap);
			if (NULL == image)
			{
				MALI_DEBUG_PRINT(1, ("EGL : Failed to create Mali image from pixmap 0x%x (API trace)\n", (unsigned int)pixmap));
			}
			else
			{
				MALI_DEBUG_CODE(mali_bool can_release;)
				mali_surface* surface;

				/* Get a pointer to the surface at plane 0 and mipmap level 0. */
				surface = mali_image_get_buffer(image, 0, 0, MALI_FALSE);
				if (NULL == surface)
				{
					MALI_DEBUG_PRINT(1, ("EGL : Failed to find surface in Mali image %p (API trace)\n", image));
				}
				else
				{
					u8* data_ptr;
	    	
					/* Lock the surface to prevent concurrent writes while dumping it.
					 * (Probably not strictly necessary because the surface was created
					 * specifically for us, but _mali_surface_map requires it.)
					 */
					_mali_surface_access_lock(surface);
	    	
					data_ptr = (u8*)_mali_surface_map(surface, MALI_MEM_PTR_READABLE);
					if (NULL == data_ptr)
					{
						MALI_DEBUG_PRINT(1, ("EGL : Failed to map surface %p into readable memory (API trace)\n", surface));
					}
					else
					{
						/* Dump the surface contents as an octet array parameter, with the
						 * native pixmap handle masquerading as the offset of the argument
						 * within the array. Use the MALI_API_TRACE_ID0 so that it is not
						 * interpreted as a real parameter. Yuck!
						 */
						mali_api_trace_param_uint8_array(MALI_API_TRACE_IN|MALI_API_TRACE_ID0, data_ptr, surface->datasize, (u32)pixmap, "eglnativepixmaptype_data");
						_mali_surface_unmap(surface);
					}
					_mali_surface_access_unlock(surface);
				}

				/* Dereference mali image resources and query whether any remain */
				MALI_DEBUG_CODE(can_release = mali_image_deref(image);)
				MALI_DEBUG_ASSERT(can_release, ("Unexpected reference(s) to mali image"));
			}
		}
	}
}

void mali_api_trace_param_eglnativewindowtype(EGLNativeWindowType win)
{
	mali_api_trace_add_eglnativewindowtype(win); /* This is needed in order to be able to recreate the native EGLNativeWindowType type */
	mali_api_trace_param_unsigned_integer((u32)win, "eglnativewindowtype");
}

void mali_api_trace_param_eglnativepixmaptype(EGLNativePixmapType pixmap)
{
	mali_api_trace_add_eglnativepixmaptype(pixmap); /* This is needed in order to be able to recreate the native EGLNativePixmapType type */
	mali_api_trace_param_unsigned_integer((u32)pixmap, "eglnativepixmaptype");
}

void mali_api_trace_param_attrib_array(const EGLint* ptr, EGLint size)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		/* If we find EGL_MATCH_NATIVE_PIXMAP, then we need to dump that pixmap first */
		if (NULL != ptr)
		{
			EGLint i;
			for (i = 0; EGL_NONE != ptr[i]; i += 2)
			{
				if (EGL_MATCH_NATIVE_PIXMAP == ptr[i])
				{
					mali_api_trace_add_eglnativepixmaptype((EGLNativePixmapType)(ptr[i + 1]));
				}
			}
		}

		mali_api_trace_param_int32_array(MALI_API_TRACE_IN, ptr, size, "eglattribarray");
	}
}

void mali_api_trace_param_eglclientbuffer_eglcreatepbufferfromclientbuffer(EGLClientBuffer value, EGLenum type)
{
	switch(type)
	{
	case EGL_OPENVG_IMAGE:
		mali_api_trace_param_unsigned_integer((u32)value, "eglclientbuffer_vgimage");
		break;
	default:
		mali_api_trace_param_unsigned_integer((u32)value, "eglclientbuffer_unknown");
		break;
	}
}

void mali_api_trace_param_eglclientbuffer_eglcreateimagekhr(EGLClientBuffer value, EGLenum type)
{
	switch(type)
	{
	case EGL_NATIVE_PIXMAP_KHR:
		mali_api_trace_param_eglnativepixmaptype((EGLNativePixmapType)value);
		mali_api_trace_add_pixmap_data((EGLNativePixmapType)value);
		break;
	case EGL_VG_PARENT_IMAGE_KHR:
		mali_api_trace_param_unsigned_integer((u32)value, "eglclientbuffer_vgimage");
		break;
	case EGL_GL_TEXTURE_2D_KHR:
		mali_api_trace_param_unsigned_integer((u32)value, "eglclientbuffer_texture2d");
		break;
	case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR:
		mali_api_trace_param_unsigned_integer((u32)value, "eglclientbuffer_texturecubepx");
		break;
	case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR:
		mali_api_trace_param_unsigned_integer((u32)value, "eglclientbuffer_texturecubenx");
		break;
	case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR:
		mali_api_trace_param_unsigned_integer((u32)value, "eglclientbuffer_texturecubepy");
		break;
	case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR:
		mali_api_trace_param_unsigned_integer((u32)value, "eglclientbuffer_texturecubeny");
		break;
	case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR:
		mali_api_trace_param_unsigned_integer((u32)value, "eglclientbuffer_texturecubepz");
		break;
	case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR:
		mali_api_trace_param_unsigned_integer((u32)value, "eglclientbuffer_texturecubenz");
		break;
	case EGL_GL_TEXTURE_3D_KHR:
		mali_api_trace_param_unsigned_integer((u32)value, "eglclientbuffer_texture3d");
		break;
	case EGL_GL_RENDERBUFFER_KHR:
		mali_api_trace_param_unsigned_integer((u32)value, "eglclientbuffer_renderbuffer");
		break;
	default:
		mali_api_trace_param_unsigned_integer((u32)value, "eglclientbuffer_unknown");
		break;
	}
}

u32 mali_api_trace_attrib_size(const EGLint *attrib_list)
{
	u32 retval = 0;

	if (NULL != attrib_list)
	{
		while (attrib_list[retval] != EGL_NONE)
		{
			retval += 2;
		}
		retval += 1; /* also include the EGL_NONE which we found */
	}

	return retval;
}

mali_bool mali_api_trace_query_surface_has_return_data(EGLDisplay __dpy, EGLSurface __surface, EGLint attribute, void *thread_state)
{
	egl_display *dpy = NULL;
	egl_surface *surface = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), MALI_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate ), MALI_FALSE );
	MALI_CHECK( EGL_NO_SURFACE != (surface = __egl_get_check_surface( __surface, __dpy, tstate ) ), MALI_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), MALI_FALSE );

	/* This is basically a copy of _egl_query_surface */
	switch ( attribute )
	{
		case EGL_VG_ALPHA_FORMAT:
		case EGL_VG_COLORSPACE:
		case EGL_CONFIG_ID:
		case EGL_HEIGHT:
		case EGL_HORIZONTAL_RESOLUTION:
		case EGL_PIXEL_ASPECT_RATIO:
		case EGL_RENDER_BUFFER:
		case EGL_SWAP_BEHAVIOR:
		case EGL_MULTISAMPLE_RESOLVE:
		case EGL_VERTICAL_RESOLUTION:
		case EGL_WIDTH:
			return MALI_TRUE;
		case EGL_LARGEST_PBUFFER:
		case EGL_MIPMAP_TEXTURE:
		case EGL_MIPMAP_LEVEL:
		case EGL_TEXTURE_FORMAT:
		case EGL_TEXTURE_TARGET:
			  if ( MALI_EGL_PBUFFER_SURFACE == surface->type )
			  {
				  return MALI_FALSE;
			  }
			  break;
	}

	return MALI_FALSE;
}
