/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005, 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>

#include "egl_surface.h"
#include "egl_thread.h"
#include "egl_platform.h"
#include "egl_misc.h"
#include "egl_handle.h"
#include "egl_mali.h"

#if MALI_USE_UNIFIED_MEMORY_PROVIDER
#include "ump/ump_ref_drv.h"
#endif

#if EGL_KHR_lock_surface_ENABLE
#include "egl_lock_surface.h"
#endif

#if EGL_FEATURE_SURFACE_SCALING_ENABLE
#include "egl_surface_scaling.h"
#endif

#ifdef EGL_MALI_VG
#include "egl_vg.h"
#endif /* EGL_MALI_VG */

#ifdef EGL_MALI_GLES
#include "egl_gles.h"
#endif

#include "egl_sync.h"

#include "egl_common_memory.h"

void _egl_surface_job_incref( egl_surface *surface )
{
	_mali_sys_mutex_lock( surface->jobs_mutex );
	if ( 0 == surface->jobs )
	{
		_mali_sys_lock_lock( surface->jobs_lock );
	}
	surface->jobs++;
	_mali_sys_mutex_unlock( surface->jobs_mutex );
}

void _egl_surface_job_decref( egl_surface *surface )
{
	_mali_sys_mutex_lock( surface->jobs_mutex );
	surface->jobs--;
	if ( 0 == surface->jobs )
	{
		_mali_sys_lock_unlock( surface->jobs_lock );
	}
	MALI_DEBUG_ASSERT( 0 <= surface->jobs,("Ref-Count below zero") );
	_mali_sys_mutex_unlock( surface->jobs_mutex );
}

void _egl_surface_wait_for_jobs( egl_surface *surface )
{
	_mali_sys_lock_lock( surface->jobs_lock );
	_mali_sys_lock_unlock( surface->jobs_lock );
}

void _egl_surface_release_all_dependencies( egl_surface *surface )
{
	MALI_DEBUG_ASSERT_POINTER( surface );

	if ( surface->frame_builder )
	{
		_egl_surface_wait_for_jobs( surface );
		/* It is sufficient to wait for all egl jobs, but in case an error occured
		   in eglSwapBuffers we might have a FB flush ongoing without having an associated
		   EGL swap callback pending in a display consumer.
		   So for error safety we need both calls. */
		_mali_frame_builder_wait_all( surface->frame_builder );
	}

	if ( NULL != surface->last_swap )
	{
		/* There was one consumer which was not released yet - do so */
		mali_ds_consumer_release_all_connections( surface->last_swap->display_consumer );
		surface->last_swap = NULL;
	}
}

/**
 * @brief releases a surface and its resources
 * @param surface the surface to release
 * @param tstate current thread state
 * @return zero if it was released, number of references if not
 */
EGLint __egl_release_surface( egl_surface *surface, void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	MALI_IGNORE( tstate );
	MALI_DEBUG_ASSERT( surface->references >= 0, ("Wrong reference count for surface 0X%x reference = %d\n", surface, surface->references) );

	if ( 0 == surface->references )
	{

#ifdef EGL_MALI_VG
		if ( NULL != tstate && (EGLClientBuffer)NULL != surface->client_buffer )
		{
			__egl_vg_destroy_pbuffer_from_client_buffer( surface, tstate );
		}
#endif

		/* Wait for any inflight rendering and release all dependencies*/
		_egl_surface_release_all_dependencies( surface );

		/* Deref the pixmap image */
		if ( surface->pixmap_image )
		{
			mali_image_deref_surfaces( surface->pixmap_image );
			mali_image_deref( surface->pixmap_image );
		}

		/* release any eglBindTexImage calls */
#ifdef EGL_MALI_GLES
		if( NULL != tstate && surface->is_bound )
		{
			__egl_context_unbind_bound_surface( surface->bound_context, surface );
			__egl_gles_unbind_tex_image( surface, tstate );
		}
#endif
		if ( _mali_sys_atomic_get(&surface->do_readback))
		{
			u32 usage;
			_mali_sys_atomic_set(&surface->do_readback, 0);
			MALI_DEBUG_ASSERT( NULL != surface->internal_target, ( "There must be an internal_target at this point" ) );
			if ( MALI_ERR_NO_ERROR == _mali_frame_builder_write_lock(surface->frame_builder, MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_ALL_CHANNELS) )
			{
				_mali_frame_builder_write_unlock(surface->frame_builder);
			}
			_mali_frame_builder_get_output(surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, &usage);
			__egl_mali_readback_surface( surface, surface->internal_target, usage, 0, 0 );
			_mali_surface_deref( surface->internal_target );
			surface->internal_target = NULL;
		}

		/* destroy the platform specific parts on the surface */
		__egl_platform_destroy_surface( surface );

#if EGL_KHR_lock_surface_ENABLE
		__egl_lock_surface_release( surface );
#endif

#if EGL_FEATURE_SURFACE_SCALING_ENABLE
        if( surface->surface_scaling_info != NULL )
        {
            __egl_surface_scaling_info_release( surface->surface_scaling_info );
            surface->surface_scaling_info = NULL;
        }
#endif

		if ( MALI_NO_HANDLE != surface->jobs_lock )
		{
			_mali_sys_lock_lock( surface->jobs_lock  );
			_mali_sys_lock_unlock( surface->jobs_lock  );
		}

		if ( MALI_NO_HANDLE != surface->jobs_mutex )
		{
			_mali_sys_mutex_lock( surface->jobs_mutex  );
			_mali_sys_mutex_unlock( surface->jobs_mutex  );
		}

		if ( MALI_NO_HANDLE != surface->jobs_mutex ) _mali_sys_mutex_destroy( surface->jobs_mutex );

		if ( MALI_NO_HANDLE != surface->jobs_lock ) _mali_sys_lock_destroy( surface->jobs_lock );

		if ( MALI_NO_HANDLE != surface->lock )
		{
			/* if we are destroying the surface mutex, unlock (if locked) */
			_mali_sys_mutex_lock( surface->lock );
			_mali_sys_mutex_unlock( surface->lock );
			_mali_sys_mutex_destroy( surface->lock );
		}

		_mali_sys_free( surface );
		surface = EGL_NO_SURFACE;

		return 0;
	}
	else
	{
		return surface->references;
	}
}


/**
 * @brief releases the framebuilder resources from a surface
 * @param surface the surface to release content from
 * @note should only be called from within the surface allocation itself
 */
void __egl_release_surface_content( egl_surface *surface )
{
	MALI_DEBUG_ASSERT( surface->references >= 0, ("Negative reference count\n") );

	/* destroy the platform specific parts on the surface */
	__egl_platform_destroy_surface( surface );

#if EGL_KHR_lock_surface_ENABLE
	__egl_lock_surface_release( surface );
#endif

}

/**
 * @brief Checks whether the surface is lock-able window surface
 * @param surface surface to check if window lockable
 * @return EGL_TRUE if given surface is window lockable, EGL_FALSE otherwise.
 */
EGLBoolean __egl_is_lockable_window_surface( const egl_surface* surface )
{
	MALI_DEBUG_ASSERT_POINTER(surface);
	MALI_DEBUG_ASSERT_POINTER(surface->config);

	if( (surface->config->surface_type & (EGL_WINDOW_BIT | EGL_LOCK_SURFACE_BIT_KHR))
			== (EGL_WINDOW_BIT | EGL_LOCK_SURFACE_BIT_KHR) )
	{
		return EGL_TRUE;
	}

	return EGL_FALSE;
}

/**
* @brief Assigns default values to a surface
* @param surface surface to assign default values
* @param config config surface was created with
* @param pixmap pixmap if surface is a pixmap surface
* @param type the type of surface, MALI_EGL_XXX_SURFACE
*/
MALI_STATIC void __egl_set_surface_defaults( egl_surface *surface,
                                             egl_config *config,
                                             EGLNativePixmapType *pixmap,
                                             mali_egl_surface_type type )
{
	surface->colorspace = EGL_VG_COLORSPACE_sRGB;
	surface->alpha_format = EGL_VG_ALPHA_FORMAT_NONPRE;
	surface->type = type;
	surface->config = config;
	if( MALI_EGL_PIXMAP_SURFACE == type )
	{
		surface->render_buffer = EGL_SINGLE_BUFFER;
		surface->pixmap = *pixmap;
	}
	else
	{
		surface->render_buffer = EGL_BACK_BUFFER;
	}
	surface->texture_format = EGL_NO_TEXTURE;
	surface->texture_target = EGL_NO_TEXTURE;

	/* A lockable window surface (i.e. where the EGLConfig has both EGL_LOCK_SURFACE_BIT_KHR and EGL_WINDOW_BIT
	 * set in EGL_SURFACE_TYPE), where the default is EGL_BUFFER_PRESERVED, but it may be overridden by specifying
	 * EGL_SWAP_BEHAVIOR to	eglCreateWindowSurface */
	if( __egl_is_lockable_window_surface( surface ) )
	{
		surface->swap_behavior = EGL_BUFFER_PRESERVED;
	}
	else
	{
		surface->swap_behavior = EGL_BUFFER_DESTROYED;
	}
	surface->multisample_resolve = EGL_MULTISAMPLE_RESOLVE_DEFAULT;
	surface->references = 0; /* Reference initially set to 0 so that __egl_release_surface cleans up correctly on an error */
	surface->pixel_aspect_ratio = EGL_UNKNOWN;
	surface->horizontal_resolution = EGL_UNKNOWN;
	surface->vertical_resolution = EGL_UNKNOWN;
#ifdef EGL_MALI_VG
	surface->client_buffer = (EGLClientBuffer)NULL;
#endif
	surface->is_valid = EGL_TRUE;
	surface->num_buffers = 1;
}

/**
 * @brief Parses a surface attribute list
 * @param tstate current thread state
 * @param attrib the attrib list
 * @param config config used to create surface
 * @param surface surface to be used with attribute list
 * @param width storage of width for surface
 * @param height storage of height for surface
 * @param type surface type, MALI_EGL_XXX_SURFACE
 * @return EGL_TRUE on successful parsing of list, EGL_FALSE else
 * @note updates the egl error state if needed
 */
MALI_STATIC EGLBoolean __egl_parse_surface_attribute_list( __egl_thread_state *tstate,
                                                           const EGLint *attrib,
                                                           egl_config *config,
                                                           egl_surface *surface,
                                                           u32 *width,
                                                           u32 *height,
                                                           mali_egl_surface_type type )
{
	/* default to 0,0 for pbuffer surfaces */
	if ( MALI_EGL_PBUFFER_SURFACE == type )
	{
		*width = 0;
		*height = 0;
	}

	if ( attrib )
	{
		mali_bool done = MALI_FALSE;
		while ( done != MALI_TRUE )
		{
#if EGL_FEATURE_SURFACE_SCALING_ENABLE
            /* Check whether the next attribute to be parsed is actually from an extension
             */
            if( EGL_FALSE == __egl_surface_scaling_validate_attrib( surface, attrib ) )
#endif
            {
                switch ( attrib[0] )
                {
                    case EGL_RENDER_BUFFER:
                        if ( EGL_BACK_BUFFER == attrib[1] ||
                             EGL_SINGLE_BUFFER == attrib[1] )
                        {
                            /* we only support back buffer rendering */
                            surface->render_buffer = EGL_BACK_BUFFER;
                        }
                        else
                        {
                            __egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
                            return EGL_FALSE;
                        }
                        break;

                    case EGL_VG_COLORSPACE:
                        /* Check if the parameters are legal for this attribute */
                        if ( !( EGL_VG_COLORSPACE_sRGB == attrib[1] ||
                                EGL_VG_COLORSPACE_LINEAR == attrib[1] ))
                        {
                            __egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
                            return EGL_FALSE;
                        }
                        /* Check if the config allows rendering to this colorspace */
                        if ( EGL_VG_COLORSPACE_LINEAR == attrib[1] )
                        {
                            if ( !(config->surface_type & EGL_VG_COLORSPACE_LINEAR_BIT) )
                            {
                                /* Config doesn't support linear rendering */
                                __egl_set_error( EGL_BAD_MATCH, tstate );
                                return EGL_FALSE;
                            }
                        }
                        surface->colorspace = attrib[1];
                        break;

                    case EGL_VG_ALPHA_FORMAT:
                        /* Check if parameter is legal */
                        if ( !( EGL_ALPHA_FORMAT_NONPRE == attrib[1] ||
                                EGL_ALPHA_FORMAT_PRE == attrib[1] ))
                        {
                            __egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
                            return EGL_FALSE;
                        }

                        /* Check if the config allows rendering to this alpha format */
                        if ( ( EGL_ALPHA_FORMAT_PRE == attrib[1] ) )
                        {
                            if ( !( config->surface_type & EGL_VG_ALPHA_FORMAT_PRE_BIT) )
                            {
                                /* Config doesn't support premultiplied alpha rendering */
                                __egl_set_error( EGL_BAD_MATCH, tstate );
                                return EGL_FALSE;
                            }
                        }
                        surface->alpha_format = attrib[1];
                        break;

                    case EGL_WIDTH:
                        if ( MALI_EGL_PBUFFER_SURFACE == type )
                        {
                            if ( attrib[1] < 0 || ( (u32) attrib[1] > EGL_SURFACE_MAX_WIDTH ) )
                            {
                                __egl_set_error( EGL_BAD_PARAMETER, tstate );
                                return EGL_FALSE;
                            }
                            *width = attrib[1];
                            surface->width = *width;
                        }
                        else
                        {
                            __egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
                            return EGL_FALSE;
                        }
                        break;

                    case EGL_HEIGHT:
                        if ( MALI_EGL_PBUFFER_SURFACE == type )
                        {
                            if ( attrib[1] < 0 || ((u32) attrib[1] ) > EGL_SURFACE_MAX_HEIGHT )
                            {
                                __egl_set_error( EGL_BAD_PARAMETER, tstate );
                                return EGL_FALSE;
                            }
                            *height = attrib[1];
                            surface->height = *height;
                        }
                        else
                        {
                            __egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
                            return EGL_FALSE;
                        }
                        break;

                    /* pbuffer specific attributes */
                    case EGL_TEXTURE_FORMAT:
                        /* If EGL_TEXTURE_FORMAT != EGL_NO_TEXTURE then EGL_WIDTH and EGL_HEIGHT have to specify legal texture
                         * dimensions. This differs between cores and API versions (1.1 without extensions doesn't support
                         * non-pow2 textures, while the extended 1.1 on m200 does).
                         * EGL needs to know the features of the API context that will render to the pbuffer.
                         */
                        if ( EGL_NO_TEXTURE != attrib[1] )
                        {
                            /* check if config supports OpenGL ES rendering */
                            if ( !(config->renderable_type & EGL_OPENGL_ES_BIT) && !(config->renderable_type & EGL_OPENGL_ES2_BIT) )
                            {
                                __egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
                                return EGL_FALSE;
                            }

                            /* check if the attribute is valid, and supported by config */
                            switch ( attrib[1] )
                            {
                                case EGL_TEXTURE_RGB:
                                    if ( EGL_FALSE == config->bind_to_texture_rgb )
                                    {
                                        __egl_set_error( EGL_BAD_MATCH, tstate );
										return EGL_FALSE;
                                    }
                                    break;

                                case EGL_TEXTURE_RGBA:
                                    if ( EGL_FALSE == config->bind_to_texture_rgba )
                                    {
                                        __egl_set_error( EGL_BAD_MATCH, tstate );
                                        return EGL_FALSE;
                                    }
                                    break;

                                default:
                                    __egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
                                    return EGL_FALSE;
                            }
                            /* format ok - save it to surface */
                            surface->texture_format = attrib[1];
                        }
                        break;

                    case EGL_TEXTURE_TARGET:
                        if ( EGL_NO_TEXTURE != attrib[1] )
                        {
                            /* check if config supports OpenGL ES rendering */
                            if ( !(config->renderable_type & EGL_OPENGL_ES_BIT) && !(config->renderable_type & EGL_OPENGL_ES2_BIT) )
                            {
                                __egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
                                return EGL_FALSE;
                            }

                            switch ( attrib[1] )
                            {
                                case EGL_TEXTURE_2D:
                                    surface->texture_target = EGL_TEXTURE_2D;
                                    break;

                                default:
                                    __egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
                                    return EGL_FALSE;
                            }
                        }
                        break;

                    case EGL_MIPMAP_TEXTURE:
                        /* check if config supports OpenGL ES rendering */
                        if ( !(config->renderable_type & EGL_OPENGL_ES_BIT) && !(config->renderable_type & EGL_OPENGL_ES2_BIT) )
                        {
                            __egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
                            return EGL_FALSE;
                        }

                        if ( (EGL_FALSE != attrib[1]) && (EGL_TRUE != attrib[1]) )
                        {
                            __egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
                            return EGL_FALSE;
                        }
                        surface->mipmap_texture = attrib[1];
                        break;

                    case EGL_LARGEST_PBUFFER:
                        if ( (EGL_FALSE != attrib[1]) && (EGL_TRUE != attrib[1]) )
                        {
                            __egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
                            return EGL_FALSE;
                        }
                        surface->largest_pbuffer = attrib[1];
                        break;
                    case EGL_SWAP_BEHAVIOR:
                        /* EGL_KHR_lock_surface2: setting of EGL_SWAP_BEHAVIOR at surface creation time is supported
                         * only for a lockable surface, i.e. where the EGLConfig has EGL_LOCK_SURFACE_BIT_KHR
                         * set in EGL_SURFACE_TYPE */
#if EGL_KHR_lock_surface_ENABLE
						if ( ( __egl_is_lockable_window_surface( surface ) )
								&& ((EGL_BUFFER_PRESERVED == attrib[1]) || (EGL_BUFFER_DESTROYED == attrib[1])) )
						{
							surface->swap_behavior = attrib[1];
							break;
						}
#endif
						__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
						return EGL_FALSE;
#if EGL_ANDROID_recordable_ENABLE
					case EGL_RECORDABLE_ANDROID:
						if ( (surface->config->surface_type & EGL_WINDOW_BIT)!= EGL_WINDOW_BIT)
						{
							__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
							return EGL_FALSE;
						}
						break;
#endif
                    case EGL_NONE:
                        done = MALI_TRUE;
                        break;

                    default:
                        /* faulty in-parameter, set error and exit */
                        __egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
                        return EGL_FALSE;
                }
            }
			attrib += 2;
		} /* while done != MALI_TRUE */
	} /* if attrib */

#if EGL_FEATURE_SURFACE_SCALING_ENABLE
	if( EGL_FALSE == __egl_surface_scaling_finalise_attribs( config, surface ) )
	{
        if ( EGL_FALSE == config->surface_scaling )
        {
		    __egl_set_error( EGL_BAD_MATCH, tstate );
		}
        else
        {
		    __egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
        }
		return EGL_FALSE;
	}
#endif
	return EGL_TRUE;
}

/*
	Use EGL LARGEST PBUFFER to get the largest available pbuffer when the allocation
	of the pbuffer would otherwise fail. The width and height of the allocated
	pbuffer will never exceed the values of EGL WIDTH and EGL HEIGHT,
	respectively. If the pbuffer will be used as a OpenGL ES texture (i.e.,
	the value of EGL TEXTURE TARGET is EGL TEXTURE 2D, and the value of
	EGL TEXTURE FORMAT is EGL TEXTURE RGB or EGL TEXTURE RGBA), then the
	aspect ratio will be preserved and the new width and height will be valid sizes
	for the texture target (e.g. if the underlying OpenGL ES implementation does not
	support non-power-of-two textures, both the width and height will be a power of 2)
	Use eglQuerySurface to retrieve the dimensions of the allocated pbuffer. The
	default value of EGL LARGEST PBUFFER is EGL MALI_FALSE.
*/
EGLBoolean __egl_surface_allocate( egl_surface *surface, __egl_thread_state *tstate )
{
	EGLBoolean preserve_aspect_ratio = EGL_FALSE;
	EGLBoolean alloc_succeeded = EGL_FALSE;
	u32 max_width = surface->width;    /* max failing width */
	u32 max_height = surface->height;  /* max failing height */
	u32  min_width = 0, min_height = 0; /* min successful width and height */
	const u32 accepted_difference = 8; /* accepted difference to minimal failing allocation */

	EGLBoolean largest_possible = surface->largest_pbuffer;

	if ( surface->type != MALI_EGL_PBUFFER_SURFACE ) return __egl_platform_create_surface( surface, tstate->main_ctx->base_ctx );

	if ( (EGL_TEXTURE_2D == surface->texture_target) &&
	    ((EGL_TEXTURE_RGB == surface->texture_format) || (EGL_TEXTURE_RGBA == surface->texture_format)) )
	{
		preserve_aspect_ratio = EGL_TRUE;
	}

	do
	{
		alloc_succeeded = __egl_platform_create_surface( surface, tstate->main_ctx->base_ctx );
		if ( alloc_succeeded )
		{
			/* we're close enough to call it a success if the difference between the
			 * lowest failing allocation and highest successful allocation is small enough
			 */
			if ( (max_width-surface->width) <= accepted_difference && (max_height-surface->height) <= accepted_difference )
			{
				return EGL_TRUE;
			}

			__egl_release_surface_content( surface );
		}
		else
		{
			/* don't retry if not required, just fail. Also in case we were given a pre-allocated surface */
			if ( !largest_possible )
			{
				break;
			}

			/* Mali memory could have been allocated during the search, so that our
				* guess on max / min size now is wrong. If we reach a point where the interval
				* is zero, we will fail the allocation of this surface */
			if ( (max_width == min_width) && (max_height == min_height) )
			{
				break;
			}
		}

		if ( preserve_aspect_ratio )
		{
			/* decrease the size according to its aspectratio, preserving pow2 */
			surface->width /= 2;
			surface->height/= 2;
			if ( EGL_TRUE == __egl_platform_create_surface( surface, tstate->main_ctx->base_ctx ) ) return EGL_TRUE;
		}
		else
		{
			if ( alloc_succeeded )
			{
				/* previous allocation succeeded, split into new higher interval */
				min_width = surface->width;
				min_height = surface->height;
				surface->width = surface->width + (max_width-surface->width)/2;
				surface->height = surface->height + (max_height-surface->height)/2;
			}
			else
			{
				/* previous allocation failed, split into new lower interval */
				max_width = surface->width;
				max_height= surface->height;
				surface->width = min_width + (max_width - min_width)/2;
				surface->height = min_height + (max_height - min_height)/2;
			}
		}
	}
	while ( ( (0 != surface->width) && (0 != surface->height) ) );
	
	return EGL_FALSE;
}

MALI_CHECK_RESULT egl_surface* __egl_create_surface( void*                   thread_state,
                                                     egl_display             *dpy,
                                                     mali_egl_surface_type   type,
                                                     egl_config*             config,
                                                     EGLNativeWindowType     window,
                                                     EGLNativePixmapType     pixmap,
                                                     const EGLint            *attrib,
                                                     mali_surface            *color_buffer_dst )
{
	u32 width = EGL_SURFACE_MAX_WIDTH;
	u32 height = EGL_SURFACE_MAX_HEIGHT;
	egl_surface *surface = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	MALI_DEBUG_ASSERT_POINTER( tstate );
	MALI_DEBUG_ASSERT_POINTER( dpy );
	MALI_DEBUG_ASSERT_POINTER( config );

	surface = (egl_surface*)_mali_sys_calloc(1, sizeof(egl_surface));

	if ( NULL == surface )
	{	
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return NULL;
	}

	__egl_set_surface_defaults( surface, config, &pixmap, type );

#if EGL_KHR_lock_surface_ENABLE
	if ( config->surface_type & EGL_LOCK_SURFACE_BIT_KHR )
	{
		if ( EGL_FALSE == __egl_lock_surface_initialize( surface ) )
		{
			_mali_sys_free( surface );			
			__egl_set_error( EGL_BAD_ALLOC, tstate );
			return NULL;
		}
	}
#endif
	/* If platform supports, create window automatically (e.g. full screen fb)
	   This also cures segfault due to not checking window's validity (not 0)
	   create_null_window is a stub on most platforms and just causes a bad
	   native window egl error */
	if ( MALI_EGL_WINDOW_SURFACE == type )
	{
		if( (EGLNativeWindowType)NULL == window )
		{
			window = __egl_platform_create_null_window( dpy->native_dpy );
			if( (EGLNativeWindowType)NULL == window )
			{			
				__egl_set_error( EGL_BAD_ALLOC, tstate );
				(void)__egl_release_surface( surface, tstate );
				return NULL;
			}
			surface->is_null_window = EGL_TRUE;
		}
		else
		{
			/* validation of the window is a platform dependant matter */
			if ( EGL_FALSE == __egl_platform_window_valid( dpy->native_dpy, window ) )
			{
				__egl_set_error( EGL_BAD_NATIVE_WINDOW, tstate );
				(void)__egl_release_surface( surface, tstate );
				return NULL;
			}
			/* we are not allowed to have the native window type in more than one window surface at once */
			if ( EGL_TRUE == __egl_native_window_handle_exists( window ) )
			{			
				__egl_set_error( EGL_BAD_ALLOC, tstate );
				(void)__egl_release_surface( surface, tstate );
				return NULL;
			}
		}
		surface->win = window;
	}

#if EGL_FEATURE_SURFACE_SCALING_ENABLE
     /* Allocate surface_scaling_info so we can populate it.
     * If anything sets an error in this function the allocation
     * will be freed during surface destruction.
     */
    surface->surface_scaling_info = __egl_surface_scaling_info_create();
    if( NULL == surface->surface_scaling_info )
    {    
        __egl_set_error( EGL_BAD_ALLOC, tstate );
        (void)__egl_release_surface( surface, tstate );
        return NULL;
    }
#endif

	/* Parse attribs */
	if ( EGL_FALSE == __egl_parse_surface_attribute_list( tstate, attrib, config, surface, &width, &height, type ) )
	{
		(void)__egl_release_surface( surface, tstate );
		return NULL;
	}

	surface->jobs = 0;

	surface->jobs_mutex = _mali_sys_mutex_create();
	if ( MALI_NO_HANDLE == surface->jobs_mutex )
	{	
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		(void)__egl_release_surface( surface, tstate );
		return NULL;
	}

	surface->jobs_lock = _mali_sys_lock_create();
	if ( MALI_NO_HANDLE == surface->jobs_lock )
	{	
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		(void)__egl_release_surface( surface, tstate );
		return NULL;
	}

	/* Override attribs for window surface using information from window */
	if ( MALI_EGL_WINDOW_SURFACE == type )
	{
		surface->pixel_aspect_ratio = 1;
		surface->horizontal_resolution = 1;
		surface->vertical_resolution = 1;

#if EGL_FEATURE_SURFACE_SCALING_ENABLE
        /* Small extension knowledge here. If surface_scaling_info is available then
         * do not request the size of the window
         */
        if( EGL_TRUE == surface->scaling_enabled )
        {
            __egl_surface_scaling_info_get_window_size( surface->surface_scaling_info, &width, &height );
        }
        else
#endif
        {
            if ( EGL_FALSE == __egl_platform_get_window_size(surface->win, surface->platform, &width, &height) )
            {
                /* Failed to read window size */
                width = 1;
                height = 1;
            }
            else
            {
                /* The window drawable area may be zero initially, but could become non-zero later on */
                if ( 0 == width ) width = 1;
                if ( 0 == height ) height = 1;
            }
        }
	}
	else if ( MALI_EGL_PIXMAP_SURFACE == type )
	{
		if ( EGL_FALSE == __egl_platform_pixmap_surface_compatible(dpy->native_dpy, pixmap, surface, EGL_TRUE) )
		{
			__egl_set_error( EGL_BAD_MATCH, tstate );
			(void)__egl_release_surface( surface, tstate );
			return NULL;
		}

		__egl_platform_get_pixmap_size( dpy->native_dpy, pixmap, &width, &height, NULL );
	}
	else if ( MALI_EGL_PBUFFER_SURFACE == type )
	{
		if ( ((EGL_NO_TEXTURE == surface->texture_format) && (EGL_NO_TEXTURE != surface->texture_target )) ||
		     ((EGL_NO_TEXTURE != surface->texture_format) && (EGL_NO_TEXTURE == surface->texture_target )) ||
			 ((EGL_TRUE == surface->largest_pbuffer) && (NULL != color_buffer_dst)) )
		{
			__egl_set_error( EGL_BAD_MATCH, tstate );
			(void)__egl_release_surface( surface, tstate );
			return NULL;
		}

		/* use the bound_texture_obj as temporary carrier for the destination color buffer pointer (pbuffers).
		   The platform layers will use it when creating a pbuffer instead of allocating a new surface.
		*/
		MALI_DEBUG_ASSERT( sizeof(surface->bound_texture_obj) == sizeof(color_buffer_dst), ("Pointer size mismatch\n") );
		surface->bound_texture_obj = color_buffer_dst;
	}

	surface->width = width;
	surface->height = height;

	/* place a reference to the display inside the surface struct */
	surface->dpy = dpy;

	surface->lock = _mali_sys_mutex_create();
	if ( MALI_NO_HANDLE == surface->lock )
	{	
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		(void)__egl_release_surface( surface, tstate );
		return NULL;
	}

	surface->lock_list = NULL; /* allocation of this named list is an implicit part of adding surface locks, if enabled */

#if EGL_KHR_lock_surface_ENABLE
	if ( (surface->config->surface_type & EGL_LOCK_SURFACE_BIT_KHR) != 0 )
	{
		int i;

		/* Skip buffer/dependency system initialization */
		for ( i=0; i<MAX_EGL_BUFFERS; ++i )
		{
			surface->buffer[i].id = -MALI_STATIC_CAST(int)i;
			surface->buffer[i].surface = NULL;
			surface->buffer[i].render_target = NULL;
		}
		surface->num_buffers = 0;
	}
#endif

	if ( EGL_FALSE == __egl_surface_allocate( surface, tstate ) )
	{
		(void)__egl_release_surface( surface, tstate );
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return NULL;
	}

	if ( MALI_EGL_PBUFFER_SURFACE == type )
	{
		/* reset to a clean state */
		surface->bound_texture_obj = NULL;
	}

#if EGL_KHR_lock_surface_ENABLE
	if ( (surface->config->surface_type & EGL_LOCK_SURFACE_BIT_KHR) != 0 )
	{
		surface->references = 1;
		return surface;
	}
#endif

	/* If the surface and its subcomponents are all created, then set the reference to 1 */
	surface->references = 1;

	return surface;
}

EGLSurface _egl_create_window_surface( EGLDisplay __dpy,
                                       EGLConfig __config,
                                       EGLNativeWindowType window,
                                       const EGLint *attrib_list,
                                       void *thread_state )
{
	EGLSurface handle;
	egl_display *dpy = NULL;
	egl_config  *config = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	egl_surface *surf = EGL_NO_SURFACE;

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_NO_SURFACE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate ), EGL_NO_SURFACE );
	MALI_CHECK( NULL != (config = __egl_get_check_config( __config, __dpy, tstate ) ), EGL_NO_SURFACE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_NO_SURFACE );

#if EGL_BACKEND_X11
	__egl_platform_flush_display( dpy->native_dpy );
#endif

	/* check if config supports rendering to windows */
	if ( !(config->surface_type & EGL_WINDOW_BIT) )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return EGL_NO_SURFACE;
	}

	/* early out if the window is not valid */
	if ( EGL_FALSE == __egl_platform_window_valid( dpy->native_dpy, window ) )
	{
		__egl_set_error( EGL_BAD_NATIVE_WINDOW, tstate );
		return EGL_NO_SURFACE;
	}

	if ( EGL_FALSE == __egl_platform_window_compatible( dpy->native_dpy, window, config ) )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return EGL_NO_SURFACE;
	}

	surf = __egl_create_surface( tstate, dpy, MALI_EGL_WINDOW_SURFACE, config, window, (EGLNativePixmapType)NULL, attrib_list, NULL );

	MALI_CHECK( NULL != surf, EGL_NO_SURFACE );

	/* add the surface to the list of valid surface handles */
	handle = __egl_add_surface_handle( surf, __dpy );
	if ( EGL_FALSE == handle )
	{
		/* At this point surf->references will be 1, so in order to release
		   the surface, the reference count must be forced to 0 */
		surf->references = 0;
		__egl_release_surface( surf, tstate );
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_NO_SURFACE;
	}

	return handle;
}

EGLSurface _egl_create_pbuffer_surface( EGLDisplay __dpy,
                                        EGLConfig __config,
                                        const EGLint *attrib_list,
                                        void *thread_state )
{
	egl_display *dpy = NULL;
	egl_config *config = NULL;
	__egl_thread_state* tstate = (__egl_thread_state *)thread_state;
	egl_surface *surf = EGL_NO_SURFACE;
	EGLSurface handle = EGL_NO_SURFACE;

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_NO_SURFACE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate ), EGL_NO_SURFACE );
	MALI_CHECK( NULL != (config = __egl_get_check_config( __config, __dpy, tstate ) ), EGL_NO_SURFACE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_NO_SURFACE );

	/* check if config supports rendering to pbuffers */
	if ( !(config->surface_type & EGL_PBUFFER_BIT) )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return EGL_NO_SURFACE;
	}

	surf = __egl_create_surface( tstate, dpy, MALI_EGL_PBUFFER_SURFACE, config, (EGLNativeWindowType)NULL, (EGLNativePixmapType)NULL, attrib_list, NULL );

	MALI_CHECK( NULL != surf, EGL_NO_SURFACE );

	/* add the surface to the list of valid surface handles */
	handle = __egl_add_surface_handle( surf, __dpy );
	if ( EGL_FALSE == handle )
	{
		/* At this point surf->references will be 1, so in order to release
		   the surface, the reference count must be forced to 0 */
		surf->references = 0;
		__egl_release_surface( surf, tstate );
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_NO_SURFACE;
	}

	return handle;
}

/* This function only accepts a VGImage as the buffer parameter, so this does not do anything usefull for OpenGL ES
 */
EGLSurface _egl_create_pbuffer_from_client_buffer( EGLDisplay __dpy,
                                                   EGLenum buftype,
                                                   EGLClientBuffer buffer,
                                                   EGLConfig __config,
                                                   const EGLint *attrib_list,
                                                   void *thread_state )
{
	egl_display *dpy = NULL;
	egl_config *config = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	EGLSurface handle = EGL_NO_SURFACE;
	egl_surface *surf = NULL;

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_NO_SURFACE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate ), EGL_NO_SURFACE );
	MALI_CHECK( NULL != (config = __egl_get_check_config( __config, __dpy, tstate ) ), EGL_NO_SURFACE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_NO_SURFACE );
	MALI_IGNORE( config );

#if EGL_BACKEND_X11
	__egl_platform_flush_display( dpy->native_dpy );
#endif

	if ( buftype != EGL_OPENVG_IMAGE )
	{
		__egl_set_error( EGL_BAD_PARAMETER, tstate );
		return EGL_NO_SURFACE;
	}

	if ( NULL == (EGLConfig *)buffer )
	{
		__egl_set_error( EGL_BAD_PARAMETER, tstate );
		return EGL_NO_SURFACE;
	}

#ifdef EGL_MALI_VG
	surf = __egl_vg_create_pbuffer_from_client_buffer( dpy, buffer, config, attrib_list, tstate );
#endif

	MALI_CHECK( NULL != surf, EGL_NO_SURFACE );

	handle = __egl_add_surface_handle( surf, __dpy );
	if ( EGL_NO_SURFACE == handle )
	{
		surf->references = 0;
		__egl_release_surface(surf, tstate );
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_NO_SURFACE;
	}

	return handle;
}

EGLSurface _egl_create_pixmap_surface( EGLDisplay __dpy,
                                       EGLConfig __config,
                                       EGLNativePixmapType pixmap,
                                       const EGLint *attrib_list,
                                       void *thread_state )
{
	egl_display *dpy = NULL;
	EGLSurface handle;
	egl_config *config = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	egl_surface *surf = EGL_NO_SURFACE;

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_NO_SURFACE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate ), EGL_NO_SURFACE );
	MALI_CHECK( NULL != (config = __egl_get_check_config( __config, __dpy, tstate ) ), EGL_NO_SURFACE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_NO_SURFACE );

#if EGL_BACKEND_X11
	__egl_platform_flush_display( dpy->native_dpy );
#endif

	/* check if config supports rendering to pixmaps */
	if ( !(config->surface_type & EGL_PIXMAP_BIT) )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return EGL_NO_SURFACE;
	}

	if ( EGL_FALSE == __egl_platform_pixmap_valid( pixmap ) )
	{
		__egl_set_error( EGL_BAD_NATIVE_PIXMAP, tstate );
		return EGL_NO_SURFACE;
	}

	if ( EGL_TRUE == __egl_native_pixmap_handle_exists( pixmap ) )
	{
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_NO_SURFACE;
	}

	if ( EGL_FALSE == __egl_platform_pixmap_config_compatible( dpy->native_dpy, pixmap, config, EGL_TRUE ) )
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return EGL_NO_SURFACE;
	}

	surf = __egl_create_surface( tstate, dpy, MALI_EGL_PIXMAP_SURFACE, config, (EGLNativeWindowType)NULL, pixmap, attrib_list, NULL );

	MALI_CHECK( EGL_NO_SURFACE != surf, EGL_NO_SURFACE );

	/* render the pixmap content into the surface */
	if ( EGL_FALSE == __egl_mali_render_pixmap_to_surface( surf ) )
	{
		surf->references = 0;
		__egl_release_surface( surf, tstate );
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_NO_SURFACE;
	}

	/* add the surface to the list of valid surface handles */
	handle = __egl_add_surface_handle( surf, __dpy );
	if ( EGL_FALSE == handle )
	{
		/* At this point surf->references will be 1, so in order to release
		   the surface, the reference count must be forced to 0 */
		surf->references = 0;
		__egl_release_surface(surf, tstate );
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_NO_SURFACE;
	}

	return handle;
}

EGLBoolean _egl_destroy_surface_internal( EGLDisplay __dpy, egl_surface *surface, EGLBoolean tag_invalid, void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	EGLBoolean ret = EGL_FALSE;

	if ( EGL_TRUE == tag_invalid ) surface->is_valid = EGL_FALSE;

	surface->references--;

	/* clamp the references to 1 if the surface is ready to be destroyed, but is current */
	if ( EGL_TRUE == surface->is_current && 0 == surface->references) surface->references = 1;

	if ( 0 == surface->references )
	{
		_mali_sys_mutex_lock( surface->lock );
		ret = __egl_remove_surface_handle( surface, __dpy );
		_mali_sys_mutex_unlock( surface->lock );
		(void)__egl_release_surface( surface, tstate );
		return ret;
	}

	return ret;
}

EGLBoolean _egl_destroy_surface( EGLDisplay __dpy, EGLSurface __surface, void *thread_state )
{
	egl_display *dpy = NULL;
	egl_surface *surface = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;

	if ( NULL != tstate )
	{
		MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_FALSE );
		MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate ), EGL_FALSE );
		MALI_CHECK( EGL_NO_SURFACE != (surface = __egl_get_check_surface( __surface, __dpy, tstate ) ), EGL_FALSE );
		MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_FALSE);
#if EGL_KHR_lock_surface_ENABLE
		if ( EGL_FALSE != __egl_lock_surface_is_locked( surface ) )
		{
			__egl_set_error( EGL_BAD_ACCESS, tstate );
			return EGL_FALSE;
		}
#endif
	}
	else
	{
		/* This is needed in case the call is made from the destructor.
		 * Error handling for missing thread state is performed in entrypoint
		 */
		surface = __egl_get_surface_ptr( __surface, __dpy );
		MALI_DEBUG_ASSERT_POINTER( surface );
	}

	_egl_destroy_surface_internal( __dpy, surface, EGL_TRUE, thread_state );

	return EGL_TRUE;
}

EGLBoolean _egl_query_surface( EGLDisplay __dpy,
                               EGLSurface __surface,
                               EGLint attribute,
                               EGLint *value,
                               void *thread_state )
{
	egl_display *dpy = NULL;
	egl_surface *surface = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate ), EGL_FALSE );
	MALI_CHECK( EGL_NO_SURFACE != (surface = __egl_get_check_surface( __surface, __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_null_value( value, EGL_BAD_PARAMETER, tstate ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_FALSE );

#if EGL_FEATURE_SURFACE_SCALING_ENABLE
    /* This extension adds attributes that the standard EGL module
     * doesn't know about so we query the extension implementation directly
     * with the requested attribute before testing for standard attributes
     */
     if( EGL_TRUE == __egl_surface_scaling_query_attrib( surface, attribute, value ) )
     {
        return EGL_TRUE;
     }
#endif

	/* Note: "Querying EGL TEXTURE FORMAT, EGL TEXTURE TARGET,
	 * EGL MIPMAP TEXTURE, or EGL MIPMAP LEVEL for a non-pbuffer surface is not
	 * an error, but value is not modified.
	 */
#if EGL_KHR_reusable_sync_ENABLE
	 /*
	  * in JellyBean, we run into a racing situation for sync_lock:
	  *  we take the sync_lock while waiting for the compositor to provide a native buf to query from.
	  * and compositor is waiting for the sync_lock to process at this point.
	  *
	  * The sync lock must be locked after return of acquire output.
	  */
	 __egl_sync_mutex_unlock();
#endif

	if ( EGL_WIDTH == attribute || EGL_HEIGHT == attribute )
	{
		if ( surface->type == MALI_EGL_WINDOW_SURFACE && surface->is_current )
		{
			_mali_frame_builder_acquire_output( surface->frame_builder );
#if HAVE_ANDROID_OS
			if ( __egl_platform_surface_buffer_invalid( surface ) )
			{
				MALI_DEBUG_PRINT( 2, ( "%s: surface query failed; couldn't acquire output buffer (surface=%p, attribute=%x)",
				                       MALI_FUNCTION, surface, attribute ) );

#if EGL_KHR_reusable_sync_ENABLE
				__egl_sync_mutex_lock();
#endif
				return EGL_FALSE;
			}
#endif
		}
	}
#if EGL_KHR_reusable_sync_ENABLE
	__egl_sync_mutex_lock();
#endif

	switch ( attribute )
	{
		case EGL_VG_ALPHA_FORMAT:       *value = surface->alpha_format; break;
		case EGL_VG_COLORSPACE:         *value = surface->colorspace; break;
		case EGL_CONFIG_ID:             *value = surface->config->config_id; break;
		case EGL_HEIGHT:
			*value = (EGLint)surface->height;
			break;
		case EGL_HORIZONTAL_RESOLUTION: *value = surface->horizontal_resolution; break;
		case EGL_PIXEL_ASPECT_RATIO:    *value = surface->pixel_aspect_ratio; break;
		case EGL_RENDER_BUFFER:         *value = surface->render_buffer; break;
		case EGL_SWAP_BEHAVIOR:         *value = surface->swap_behavior; break;
		case EGL_MULTISAMPLE_RESOLVE:   *value = surface->multisample_resolve; break;
		case EGL_VERTICAL_RESOLUTION:   *value = surface->vertical_resolution; break;
		case EGL_WIDTH:
			*value = (EGLint)surface->width;
			break;
		case EGL_LARGEST_PBUFFER: if ( MALI_EGL_PBUFFER_SURFACE == surface->type ) *value = surface->largest_pbuffer; break;
		case EGL_MIPMAP_TEXTURE:  if ( MALI_EGL_PBUFFER_SURFACE == surface->type ) *value = surface->mipmap_texture; break;
		case EGL_MIPMAP_LEVEL:    if ( MALI_EGL_PBUFFER_SURFACE == surface->type ) *value = surface->mipmap_level; break;
		case EGL_TEXTURE_FORMAT:  if ( MALI_EGL_PBUFFER_SURFACE == surface->type ) *value = surface->texture_format; break;
		case EGL_TEXTURE_TARGET:  if ( MALI_EGL_PBUFFER_SURFACE == surface->type ) *value = surface->texture_target; break;
#if EGL_KHR_lock_surface_ENABLE
		case EGL_BITMAP_POINTER_KHR:
			{
				if ( NULL == surface->lock_surface->mapped_pointer )
				{
					if ( EGL_FALSE == __egl_lock_surface_map_buffer( surface, tstate ) )
						return EGL_FALSE;
				}
				*value = MALI_REINTERPRET_CAST(EGLint)surface->lock_surface->mapped_pointer;
				break;
			}
		case EGL_BITMAP_PITCH_KHR:
			{
				/* This should also automatically map the backbuffer */
				if ( NULL == surface->lock_surface->mapped_pointer )
				{
					if ( EGL_FALSE == __egl_lock_surface_map_buffer( surface, tstate ) )
						return EGL_FALSE;
				}
				*value = surface->lock_surface->bitmap_pitch;
				break;
			}
		case EGL_BITMAP_ORIGIN_KHR:
			if ( (surface->config->surface_type & EGL_LOCK_SURFACE_BIT_KHR) != 0 )
			{
				*value = surface->lock_surface->bitmap_origin;
			}
			else
			{
				__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
				return EGL_FALSE;
			}
			break;
		case EGL_BITMAP_PIXEL_RED_OFFSET_KHR:
			if ( (surface->config->surface_type & EGL_LOCK_SURFACE_BIT_KHR) != 0 )
			{
				*value = surface->lock_surface->bitmap_pixel_red_offset;
			}
			else
			{
				__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
				return EGL_FALSE;
			}
			break;
		case EGL_BITMAP_PIXEL_GREEN_OFFSET_KHR:
			if ( (surface->config->surface_type & EGL_LOCK_SURFACE_BIT_KHR) != 0 )
			{
				*value = surface->lock_surface->bitmap_pixel_green_offset;
			}
			else
			{
				__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
				return EGL_FALSE;
			}
			break;
		case EGL_BITMAP_PIXEL_BLUE_OFFSET_KHR:
			if ( (surface->config->surface_type & EGL_LOCK_SURFACE_BIT_KHR) != 0 )
			{
				*value = surface->lock_surface->bitmap_pixel_blue_offset;
			}
			else
			{
				__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
				return EGL_FALSE;
			}
			break;
		case EGL_BITMAP_PIXEL_ALPHA_OFFSET_KHR:
			if ( (surface->config->surface_type & EGL_LOCK_SURFACE_BIT_KHR) != 0 )
			{
				*value = surface->lock_surface->bitmap_pixel_alpha_offset;
			}
			else
			{
				__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
				return EGL_FALSE;
			}
			break;
		case EGL_BITMAP_PIXEL_LUMINANCE_OFFSET_KHR:
			if ( (surface->config->surface_type & EGL_LOCK_SURFACE_BIT_KHR) != 0 )
			{
				*value = surface->lock_surface->bitmap_pixel_luminance_offset;
			}
			else
			{
				__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
				return EGL_FALSE;
			}
			break;
		case EGL_BITMAP_PIXEL_SIZE_KHR:
			if ( (surface->config->surface_type & EGL_LOCK_SURFACE_BIT_KHR) != 0 )
			{
				*value = surface->lock_surface->bitmap_pixel_size;
			}
			else
			{
				__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
				return EGL_FALSE;
			}
			break;
#endif /* EGL_KHR_lock_surface_ENABLE */
		default:
			__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
			return EGL_FALSE;
	}

	return EGL_TRUE;
}

EGLBoolean _egl_surface_attrib( EGLDisplay __dpy,
                                EGLSurface __surface,
                                EGLint attribute,
                                EGLint value,
                                void *thread_state )
{
	egl_display *dpy = NULL;
	egl_surface *surface = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
    EGLBoolean return_value = EGL_TRUE;

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate ), EGL_FALSE );
	MALI_CHECK( EGL_NO_SURFACE != (surface = __egl_get_check_surface( __surface, __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_FALSE );

#if EGL_KHR_lock_surface_ENABLE
    if ( EGL_TRUE == __egl_lock_surface_attrib( surface, attribute, value, &return_value, tstate ) )
    {
        return return_value;
    }
#endif

#if EGL_FEATURE_SURFACE_SCALING_ENABLE
    if ( EGL_TRUE == __egl_surface_scaling_attrib( surface, attribute, value, &return_value, tstate ) )
    {
        return return_value;
    }
#endif

	switch ( attribute )
	{
		case EGL_MIPMAP_LEVEL:
			/* should return EGL_BAD_PARAMETER if OpenGL ES rendering is not supported by surface */
			if ( !(surface->config->renderable_type & EGL_OPENGL_ES_BIT) && !(surface->config->renderable_type & EGL_OPENGL_ES2_BIT) )
			{
				__egl_set_error( EGL_BAD_PARAMETER, tstate );
				return_value = EGL_FALSE;
                break;
			}

            /* If the value of pbuffer attribute EGL TEXTURE FORMAT is EGL NO TEXTURE, if
                the value of attribute EGL TEXTURE TARGET is EGL NO TEXTURE, or if surface is
                not a pbuffer, then attribute EGL MIPMAP LEVEL may be set, but has no effect
            */

            if ( (EGL_NO_TEXTURE != surface->texture_format) &&
                 (EGL_NO_TEXTURE != surface->texture_target) && (MALI_EGL_PBUFFER_SURFACE == surface->type) )
            {
                surface->mipmap_level = value;
            }
            break;

		case EGL_SWAP_BEHAVIOR:
			/* Verify that "value" is supported, set EGL_BAD_PARAMETER if not */
			switch (value)
			{
				case EGL_BUFFER_PRESERVED:
					/* Check that the associated config supports EGL_BUFFER_PRESERVED */
					if ( !(surface->config->surface_type & EGL_SWAP_BEHAVIOR_PRESERVED_BIT) )
					{
						__egl_set_error( EGL_BAD_MATCH, tstate );
						return_value = EGL_FALSE;
					}
					break;

				case EGL_BUFFER_DESTROYED:
					/* Default action. All configs supports this */
					break;

				default:
					/* Unsupported value */
					__egl_set_error ( EGL_BAD_PARAMETER, tstate );
					return_value = EGL_FALSE;

			}

			if( EGL_TRUE == return_value )
			{
				/* If we go from buffer preserved to buffer destroy and we have no writeback conversion we can
				 * safely remove the internal_target. *But* we need to keep it if we have an in-flight readback. */
				if ( !_mali_sys_atomic_get(&surface->do_readback)
					 && EGL_BUFFER_PRESERVED == surface->swap_behavior
					 && EGL_BUFFER_DESTROYED == value
					 && 0 == ( surface->caps & SURFACE_CAP_WRITEBACK_CONVERSION )
					 && NULL != surface->internal_target )
				{
					_mali_surface_deref( surface->internal_target );
					surface->internal_target = NULL;
				}
				surface->swap_behavior = value;
			}
			break;

		case EGL_MULTISAMPLE_RESOLVE:
			switch (value)
			{
				case EGL_MULTISAMPLE_RESOLVE_DEFAULT:
					break;

				case EGL_MULTISAMPLE_RESOLVE_BOX:
					if ( !(surface->config->surface_type & EGL_MULTISAMPLE_RESOLVE_BOX_BIT) )
					{
						__egl_set_error( EGL_BAD_MATCH, tstate );
						return_value = EGL_FALSE;
					}
					break;

				default:
					/* Unsupported value */
					__egl_set_error ( EGL_BAD_PARAMETER, tstate );
					return_value = EGL_FALSE;
			}

            if( EGL_TRUE == return_value )
			{
                surface->multisample_resolve = value;
            }
			break;

			default:
			__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
			return_value = EGL_FALSE;
	}

	return return_value;
}

EGLSurface _egl_get_current_surface( EGLint readdraw, void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	__egl_thread_state_api *tstate_api = NULL;

	/* If there is no current context for the current rendering API, then
	 * EGL NO SURFACE is returned (this is not an error).*/
	tstate_api = __egl_get_current_thread_state_api( tstate, NULL );
	MALI_CHECK_NON_NULL( tstate_api, EGL_NO_SURFACE );

	MALI_CHECK( EGL_NO_CONTEXT != tstate_api->context, EGL_NO_SURFACE );

	switch ( readdraw )
	{
		case EGL_DRAW: return __egl_get_surface_handle( tstate_api->draw_surface, __egl_get_display_handle( tstate_api->display ) );
		case EGL_READ: return __egl_get_surface_handle( tstate_api->read_surface, __egl_get_display_handle( tstate_api->display ) );
		default: __egl_set_error( EGL_BAD_PARAMETER, tstate );
	}

	return EGL_NO_SURFACE;
}

void _egl_surface_cpu_access_callback( mali_surface *surface, enum mali_surface_event event, void* mem_ref, void *data )
{
	egl_memory_handle egl_mem_handle = EGL_INVALID_MEMORY_HANDLE;
	egl_buffer_name egl_buf_name = EGL_INVALID_BUFFER_NAME;
	egl_surface_lock_item *lock_item = NULL;

	MALI_IGNORE(event);
	MALI_IGNORE(data);
	MALI_DEBUG_ASSERT_POINTER( mem_ref );

	egl_mem_handle = _egl_memory_get_handle_from_mali_memory( ((struct mali_shared_mem_ref *)mem_ref)->mali_memory );
	if ( EGL_INVALID_MEMORY_HANDLE == egl_mem_handle ) return;
	egl_buf_name = _egl_memory_get_name_from_handle( -1, egl_mem_handle );
	if ( EGL_INVALID_BUFFER_NAME == egl_buf_name ) return;

#if EGL_SURFACE_LOCKING_ENABLED
	lock_item = (egl_surface_lock_item *)_mali_sys_calloc( 1, sizeof(egl_surface_lock_item) );
	if ( NULL == lock_item )
	{
		MALI_DEBUG_PRINT( 0, ("WARNING: failed to create lock_item in %s:%i\n", __func__, __LINE__) );
		return;
	}

	lock_item->usage = SURFACE_LOCK_USAGE_CPU_WRITE;
	lock_item->buf_name = egl_buf_name;

	__egl_platform_register_lock_item( lock_item );
	__egl_platform_process_lock_item( lock_item );

	_mali_sys_free( lock_item );
#endif

#if MALI_USE_UNIFIED_MEMORY_PROVIDER && EGL_DEFERRED_UMP_CACHE_FLUSH
	ump_cache_operations_control( UMP_CACHE_OP_START );
	ump_switch_hw_usage_secure_id( egl_buf_name, UMP_USED_BY_CPU );
	ump_cache_operations_control( UMP_CACHE_OP_FINISH );
#endif
}

void _egl_surface_cpu_access_done_callback( mali_surface *surface, enum mali_surface_event event, void* mem_ref, void *data )
{
#if EGL_SURFACE_LOCKING_ENABLED
	egl_memory_handle egl_mem_handle = EGL_INVALID_MEMORY_HANDLE;
	egl_buffer_name egl_buf_name = EGL_INVALID_BUFFER_NAME;
	egl_surface_lock_item *lock_item = NULL;

	MALI_IGNORE(event);
	MALI_IGNORE(data);
	MALI_DEBUG_ASSERT_POINTER( mem_ref );

	egl_mem_handle = _egl_memory_get_handle_from_mali_memory( ((struct mali_shared_mem_ref *)mem_ref)->mali_memory );
	if ( EGL_INVALID_MEMORY_HANDLE == egl_mem_handle ) return;
	egl_buf_name = _egl_memory_get_name_from_handle( -1, egl_mem_handle );
	if ( EGL_INVALID_BUFFER_NAME == egl_buf_name ) return;

	lock_item = (egl_surface_lock_item *)_mali_sys_calloc( 1, sizeof(egl_surface_lock_item) );
	if ( NULL == lock_item )
	{
		MALI_DEBUG_PRINT( 0, ("WARNING: failed to create lock_item in %s:%i\n", __func__, __LINE__) );
		return;
	}

	lock_item->usage = SURFACE_LOCK_USAGE_CPU_WRITE;
	lock_item->buf_name = egl_buf_name;

	__egl_platform_release_lock_item( lock_item );

	_mali_sys_free( lock_item );
#endif
}

void _egl_surface_gpu_write_callback( mali_surface *surface, enum mali_surface_event event, void* mem_ref, void *data )
{
	egl_memory_handle egl_mem_handle = EGL_INVALID_MEMORY_HANDLE;
	egl_buffer_name egl_buf_name = EGL_INVALID_BUFFER_NAME;
	egl_surface_lock_item *lock_item = NULL;

	MALI_IGNORE(event);
	MALI_IGNORE(data);
	MALI_DEBUG_ASSERT_POINTER( mem_ref );

	egl_mem_handle = _egl_memory_get_handle_from_mali_memory( ((struct mali_shared_mem_ref *)mem_ref)->mali_memory );
	if ( EGL_INVALID_MEMORY_HANDLE == egl_mem_handle ) return;
	egl_buf_name = _egl_memory_get_name_from_handle( -1, egl_mem_handle );
	if ( EGL_INVALID_BUFFER_NAME == egl_buf_name ) return;

#if EGL_SURFACE_LOCKING_ENABLED
	lock_item = (egl_surface_lock_item *)_mali_sys_calloc( 1, sizeof(egl_surface_lock_item) );
	if ( NULL == lock_item )
	{
		MALI_DEBUG_PRINT( 0, ("WARNING: failed to create lock_item in %s:%i\n", __func__, __LINE__) );
		return;
	}

	lock_item->usage = SURFACE_LOCK_USAGE_RENDERABLE;
	lock_item->buf_name = egl_buf_name;

	__egl_platform_register_lock_item( lock_item );
	__egl_platform_process_lock_item( lock_item );

	_mali_sys_free( lock_item );
#endif

#if MALI_USE_UNIFIED_MEMORY_PROVIDER && EGL_DEFERRED_UMP_CACHE_FLUSH
	ump_cache_operations_control( UMP_CACHE_OP_START );
	ump_switch_hw_usage_secure_id( egl_buf_name, UMP_USED_BY_MALI );
	ump_cache_operations_control( UMP_CACHE_OP_FINISH );
#endif
}

void _egl_surface_gpu_write_done_callback( mali_surface *surface, enum mali_surface_event event, void* mem_ref, void *data )
{
#if EGL_SURFACE_LOCKING_ENABLED
	egl_memory_handle egl_mem_handle = EGL_INVALID_MEMORY_HANDLE;
	egl_buffer_name egl_buf_name = EGL_INVALID_BUFFER_NAME;
	egl_surface_lock_item *lock_item = NULL;

	MALI_IGNORE(event);
	MALI_IGNORE(data);
	MALI_DEBUG_ASSERT_POINTER( mem_ref );

	egl_mem_handle = _egl_memory_get_handle_from_mali_memory( ((struct mali_shared_mem_ref *)mem_ref)->mali_memory );
	if ( EGL_INVALID_MEMORY_HANDLE == egl_mem_handle ) return;
	egl_buf_name = _egl_memory_get_name_from_handle( -1, egl_mem_handle );
	if ( EGL_INVALID_BUFFER_NAME == egl_buf_name ) return;

	lock_item = (egl_surface_lock_item *)_mali_sys_calloc( 1, sizeof(egl_surface_lock_item) );
	if ( NULL == lock_item )
	{
		MALI_DEBUG_PRINT( 0, ("WARNING: failed to create lock_item in %s:%i\n", __func__, __LINE__) );
		return;
	}

	lock_item->usage = SURFACE_LOCK_USAGE_RENDERABLE;
	lock_item->buf_name = egl_buf_name;

	__egl_platform_release_lock_item( lock_item );

	_mali_sys_free( lock_item );
#endif
}

void _egl_surface_gpu_read_callback( mali_surface *surface, enum mali_surface_event event, void* mem_ref, void *data )
{
	egl_memory_handle egl_mem_handle = EGL_INVALID_MEMORY_HANDLE;
	egl_buffer_name egl_buf_name = EGL_INVALID_BUFFER_NAME;
	egl_surface_lock_item *lock_item = NULL;

	MALI_IGNORE(event);
	MALI_IGNORE(data);
	MALI_DEBUG_ASSERT_POINTER( mem_ref );

	egl_mem_handle = _egl_memory_get_handle_from_mali_memory( ((struct mali_shared_mem_ref *)mem_ref)->mali_memory );;
	if ( EGL_INVALID_MEMORY_HANDLE == egl_mem_handle ) return;
	egl_buf_name = _egl_memory_get_name_from_handle( -1, egl_mem_handle );
	if ( EGL_INVALID_BUFFER_NAME == egl_buf_name ) return;
	
#if EGL_SURFACE_LOCKING_ENABLED
	lock_item = (egl_surface_lock_item *)_mali_sys_calloc( 1, sizeof(egl_surface_lock_item) );
	if ( NULL == lock_item )
	{
		MALI_DEBUG_PRINT( 0, ("WARNING: failed to create lock_item in %s:%i\n", __func__, __LINE__) );
		return;
	}

	lock_item->usage = SURFACE_LOCK_USAGE_TEXTURE;
	lock_item->buf_name = egl_buf_name;

	__egl_platform_register_lock_item( lock_item );
	__egl_platform_process_lock_item( lock_item );

	_mali_sys_free( lock_item );
#endif

#if MALI_USE_UNIFIED_MEMORY_PROVIDER && EGL_DEFERRED_UMP_CACHE_FLUSH
	ump_cache_operations_control( UMP_CACHE_OP_START );
	ump_switch_hw_usage_secure_id( egl_buf_name, UMP_USED_BY_MALI );
	ump_cache_operations_control( UMP_CACHE_OP_FINISH );
#endif
}

void _egl_surface_gpu_read_done_callback( mali_surface *surface, enum mali_surface_event event, void* mem_ref, void *data )
{
#if EGL_SURFACE_LOCKING_ENABLED
	egl_memory_handle egl_mem_handle = EGL_INVALID_MEMORY_HANDLE;
	egl_buffer_name egl_buf_name = EGL_INVALID_BUFFER_NAME;
	egl_surface_lock_item *lock_item = NULL;

	MALI_IGNORE(event);
	MALI_IGNORE(data);
	MALI_DEBUG_ASSERT_POINTER( mem_ref );

	egl_mem_handle = _egl_memory_get_handle_from_mali_memory( ((struct mali_shared_mem_ref *)mem_ref)->mali_memory );
	if ( EGL_INVALID_MEMORY_HANDLE == egl_mem_handle ) return;
	egl_buf_name = _egl_memory_get_name_from_handle( -1, egl_mem_handle );
	if ( EGL_INVALID_BUFFER_NAME == egl_buf_name ) return;

	lock_item = (egl_surface_lock_item *)_mali_sys_calloc( 1, sizeof(egl_surface_lock_item) );
	if ( NULL == lock_item )
	{
		MALI_DEBUG_PRINT( 0, ("WARNING: failed to create lock_item in %s:%i\n", __func__, __LINE__) );
		return;
	}

	lock_item->usage = SURFACE_LOCK_USAGE_TEXTURE;
	lock_item->buf_name = egl_buf_name;

	__egl_platform_release_lock_item( lock_item );

	_mali_sys_free( lock_item );
#endif
}

void _egl_surface_update_texture_callback( mali_surface *surface, enum mali_surface_event event, void* mem_ref, void *data )
{
	MALI_IGNORE(event);
	MALI_IGNORE(mem_ref);

	__egl_platform_update_image(surface, data);
}

