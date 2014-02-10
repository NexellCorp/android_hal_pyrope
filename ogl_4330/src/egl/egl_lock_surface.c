/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include "egl_config_extension_m200.h"

#if EGL_KHR_lock_surface_ENABLE

#include "egl_lock_surface.h"
#include "egl_handle.h"
#include "egl_misc.h"
#include "egl_platform.h"

EGLBoolean __egl_lock_surface_initialize( egl_surface* surface )
{
	surface->lock_surface = _mali_sys_calloc(1, sizeof(egl_lock_surface_attributes));
	if ( NULL == surface->lock_surface ) return EGL_FALSE;

	return EGL_TRUE;
}

void __egl_lock_surface_release( egl_surface* surface )
{
	if ( NULL != surface->lock_surface) _mali_sys_free( surface->lock_surface );
	surface->lock_surface = NULL;
}

EGLBoolean _egl_lock_surface_KHR( EGLDisplay display_handle, EGLSurface surface_handle, const EGLint* attrib_list, __egl_thread_state* tstate )
{
	egl_display* display = NULL;
	egl_surface* surface = NULL;

	/* Verify handles and get internal structures for display and surface */
	MALI_CHECK( EGL_NO_DISPLAY != (display = __egl_get_check_display( display_handle, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( display, tstate ), EGL_FALSE );
	MALI_CHECK( EGL_NO_SURFACE != (surface = __egl_get_check_surface( surface_handle, display_handle, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( display, tstate), EGL_FALSE );

	/* Check that EGL_SURFACE_TYPE contains EGL_LOCK_SURFACE_BIT_KHR */
	if ( (surface->config->surface_type & EGL_LOCK_SURFACE_BIT_KHR) == 0 )
	{
		__egl_set_error( EGL_BAD_ACCESS, tstate );
		return EGL_FALSE;
	}
	/* Check that surface is not already locked */
	if ( EGL_FALSE != surface->lock_surface->is_locked )
	{
		__egl_set_error( EGL_BAD_ACCESS, tstate );
		return EGL_FALSE;
	}

	/* Check that surface is not current to a client API */
	if ( EGL_FALSE != surface->is_current )
	{
		__egl_set_error( EGL_BAD_ACCESS, tstate );
		return EGL_FALSE;
	}
	/* Check that the surface isn't bound to a texture */
	if ( EGL_FALSE != surface->is_bound )
	{
		__egl_set_error( EGL_BAD_ACCESS, tstate );
		return EGL_FALSE;
	}
	/* Set default attribute values */
	surface->lock_surface->map_preserve_pixels = EGL_FALSE;
	surface->lock_surface->lock_usage_hint = EGL_READ_SURFACE_BIT_KHR | EGL_WRITE_SURFACE_BIT_KHR;

	/* Verify attribute list and update default values */
	while ( NULL != attrib_list && EGL_NONE != *attrib_list )
	{
		EGLint attribute = *attrib_list++;
		EGLint value = *attrib_list++;
		switch (attribute)
		{
			case EGL_MAP_PRESERVE_PIXELS_KHR:
				if ( EGL_TRUE != value && EGL_FALSE != value )
				{
					__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
					return EGL_FALSE;
				}
				surface->lock_surface->map_preserve_pixels = value;
				break;

			case EGL_LOCK_USAGE_HINT_KHR:
				if ( (~(EGL_READ_SURFACE_BIT_KHR | EGL_WRITE_SURFACE_BIT_KHR) & value) != 0 )
				{
					__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
					return EGL_FALSE;
				}
				surface->lock_surface->lock_usage_hint = value;
				break;

			default:
				__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
				return EGL_FALSE;

		}
	}

	/* Lock the surface */
	surface->lock_surface->is_locked = EGL_TRUE;

	/* The color buffer isn't actually mapped before eglQuerySurface is called with
	 * EGL_BITMAP_POINTER_KHR, so we're done for now.
	 */

	/* Success */
	return EGL_TRUE;
}

EGLBoolean _egl_unlock_surface_KHR( EGLDisplay display_handle, EGLSurface surface_handle, __egl_thread_state* tstate )
{
	egl_display* display = NULL;
	egl_surface* surface = NULL;

	/* Verify handles and get internal structures for display and surface */
	MALI_CHECK( EGL_NO_DISPLAY != (display = __egl_get_check_display( display_handle, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( display, tstate ), EGL_FALSE );
	MALI_CHECK( EGL_NO_SURFACE != (surface = __egl_get_check_surface( surface_handle, display_handle, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( display, tstate), EGL_FALSE );

	/* Check if surface is lockable or is already unlocked */
	if ( !surface->lock_surface || !surface->lock_surface->is_locked )
	{
		__egl_set_error( EGL_BAD_ACCESS, tstate );
		return EGL_FALSE;
	}

	/* Unmap the surface contents */
	(void)__egl_lock_surface_unmap_buffer( surface, tstate );

	/* Mark the surface as unlocked */
	surface->lock_surface->is_locked = EGL_FALSE;

	/* Success */
	return EGL_TRUE;
}

EGLBoolean __egl_lock_surface_map_buffer( egl_surface* surface, __egl_thread_state* tstate )
{
	EGLBoolean result;
#if EGL_SURFACE_LOCKING_ENABLED
	mali_surface *target = NULL;
#endif

	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_DEBUG_ASSERT_POINTER( tstate );

	if ( EGL_FALSE == surface->lock_surface->is_locked )
	{
		__egl_set_error( EGL_BAD_ACCESS, tstate );
		return EGL_FALSE;
	}

	/* If the surface is already mapped, just return the previous pointer */
	if ( NULL != surface->lock_surface->mapped_pointer )
	{
		return EGL_TRUE;
	}

#if EGL_BACKEND_X11
	_mali_frame_builder_acquire_output( surface->frame_builder );
#endif

	result = __egl_platform_lock_surface_map_buffer( surface->dpy->native_dpy, surface, surface->lock_surface->map_preserve_pixels );

#if EGL_SURFACE_LOCKING_ENABLED
	if ( EGL_TRUE == result && NULL != surface->frame_builder )
	{
		/* trigger the MALI_SURFACE_EVENT_CPU_WRITE event */
		target = _mali_frame_builder_get_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, NULL );
		target->flags |= MALI_SURFACE_FLAG_TRACK_SURFACE;

		_mali_surface_set_event_callback( target, MALI_SURFACE_EVENT_CPU_ACCESS,
		                                  MALI_REINTERPRET_CAST(mali_surface_eventfunc)_egl_surface_cpu_access_callback,
		                                  NULL );
		_mali_surface_access_lock( target );
		_mali_surface_trigger_event( target, target->mem_ref, MALI_SURFACE_EVENT_CPU_ACCESS );
		_mali_surface_access_unlock( target );
	}
#endif

	return result;
}

EGLBoolean __egl_lock_surface_unmap_buffer( egl_surface* surface, __egl_thread_state* tstate )
{
#if EGL_SURFACE_LOCKING_ENABLED
	mali_surface *target = NULL;
#endif
	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_DEBUG_ASSERT_POINTER( tstate );

	/* If the surface was never mapped, early out */
	if ( NULL == surface->lock_surface->mapped_pointer ) return EGL_TRUE;

#if EGL_SURFACE_LOCKING_ENABLED
	/* trigger the MALI_SURFACE_EVENT_CPU_WRITE_DONE event */
	if ( NULL != surface->frame_builder )
	{
		target = _mali_frame_builder_get_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, NULL );
		_mali_surface_set_event_callback( target, MALI_SURFACE_EVENT_CPU_ACCESS_DONE,
		                                  MALI_REINTERPRET_CAST(mali_surface_eventfunc)_egl_surface_cpu_access_done_callback,
		                                  NULL);
		_mali_surface_access_lock( target );
		_mali_surface_trigger_event( target, target->mem_ref, MALI_SURFACE_EVENT_CPU_ACCESS_DONE );
		_mali_surface_access_unlock( target );
	}
#endif

	(void)__egl_platform_lock_surface_unmap_buffer( surface->dpy->native_dpy, surface );

	/* Mark surface as not mapped */
	surface->lock_surface->mapped_pointer = NULL;

	/* Success */
	return EGL_TRUE;
}

MALI_STATIC_INLINE int __egl_config_equals( const egl_config* config, int red, int green, int blue, int alpha )
{
	return config->red_size == red &&
	       config->green_size == green &&
	       config->blue_size == blue &&
	       config->alpha_size == alpha;
}

void __egl_lock_surface_initialize_configs( egl_display* display )
{
	int i;
	for ( i=0; i < display->num_configs; ++i )
	{
		egl_config* config = &display->configs[i];
		EGLint rgb_depth = 0;
		const egl_display_native_format* format = &display->native_format;

		if ( (config->surface_type & EGL_LOCK_SURFACE_BIT_KHR) == 0 ) continue;

		rgb_depth = format->red_size + format->green_size + format->blue_size;
		if ( EGL_TRUE == __egl_config_equals( config, 5, 6, 5, 0 ) && ( rgb_depth <= 16 ) )
		{
			if ( format->red_offset   == 11 &&
			     format->green_offset == 5 &&
			     format->blue_offset  == 0 )
			{
				config->surface_type |= EGL_OPTIMAL_FORMAT_BIT_KHR;
			}

		}
		else if ( EGL_TRUE == __egl_config_equals( config, 8, 8, 8, 8 ) && ( rgb_depth <= 24 ) )
		{
			if ( format->red_offset   == 8 &&
			     format->green_offset == 16 &&
			     format->blue_offset  == 24 )
			{
				config->surface_type |= EGL_OPTIMAL_FORMAT_BIT_KHR;
			}
		}
	}
}

EGLBoolean __egl_lock_surface_is_locked( const egl_surface* surface )
{
	if ( NULL != surface->lock_surface )
	{
		return surface->lock_surface->is_locked;
	}

	return EGL_FALSE;
}

EGLBoolean __egl_lock_surface_attrib( egl_surface *surface, EGLint attribute, EGLint value, EGLBoolean *result, __egl_thread_state* tstate )
{
    MALI_IGNORE( attribute );
    MALI_IGNORE( value );
    
	if ( EGL_FALSE != __egl_lock_surface_is_locked( surface ) )
	{
		__egl_set_error( EGL_BAD_ACCESS, tstate );
		*result = EGL_FALSE;
        return EGL_TRUE;
	}
    
    return EGL_FALSE;
}

#endif /* EGL_KHR_lock_surface_ENABLE */
