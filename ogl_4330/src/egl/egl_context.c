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
#include "egl_common.h"
#include "egl_context.h"
#include "egl_display.h"
#include "egl_main.h"
#include "egl_mali.h"
#include "egl_thread.h"
#include "egl_misc.h"
#include "egl_platform.h"
#include "egl_handle.h"
#include "egl_api_trace.h"

#if MALI_INSTRUMENTED
#include "sw_profiling.h"
#endif

EGLBoolean _egl_bind_api( EGLenum api, void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;

	/* return at once if new API is equal to the old API */
	if ( EGL_NONE != tstate->api_current )
	{
		if ( api == tstate->api_current ) return EGL_TRUE;
	}

	switch ( api )
	{
#ifdef EGL_MALI_GLES
		case EGL_OPENGL_ES_API:
			#ifdef EGL_MALI_VG
			__egl_vg_make_not_current( tstate );
			#endif /* EGL_MALI_VG */
			tstate->api_current = api;
			if ( NULL != tstate->api_gles && NULL != tstate->api_gles->context )
			{
				if ( EGL_FALSE == __egl_gles_make_current( tstate->api_gles->context, tstate->api_gles->draw_surface, tstate->api_gles->read_surface, tstate ) )
				{
					__egl_set_error( EGL_BAD_ALLOC, tstate );
					return EGL_FALSE;
				}
			}
			break;
#endif /* EGL_MALI_GLES */

#ifdef EGL_MALI_VG
		case EGL_OPENVG_API:
			#ifdef EGL_MALI_GLES
			__egl_gles_make_not_current( tstate );
			#endif /* EGL_MALI_GLES */
			tstate->api_current = api;
			if ( NULL != tstate->api_vg && NULL != tstate->api_vg->context )
			{
				if ( EGL_FALSE == __egl_vg_make_current( tstate->api_vg->context, tstate->api_vg->draw_surface, EGL_FALSE, tstate ) )
				{
					__egl_set_error( EGL_BAD_ALLOC, tstate );
					return EGL_FALSE;
				}
			}
			break;
#endif /* EGL_MALI_VG */
		default:
			__egl_set_error( EGL_BAD_PARAMETER, tstate );
			return EGL_FALSE;
	}

	return EGL_TRUE;
}

EGLenum _egl_query_api( void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;

	return tstate->api_current;
}

egl_context* __egl_allocate_context( egl_config *config, EGLint client_version )
{
	egl_context *context = NULL;

	context = (egl_context *)_mali_sys_calloc(1, sizeof( egl_context ) );
	if ( NULL == context ) return NULL;

	context->client_version = client_version;
	context->config = config;
	context->is_valid = EGL_TRUE;

	return context;
}

/**
 * @brief Parses an attribute list given to eglCreateContext
 * @param attrib_list the attribute list
 * @param client_version storage for the client_version attribute
 * @param robustness_enabled context robustness for opengl es
 * @param reset_notification_strategy reset notification strategy for context robustness
 * @param tstate thread state
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
MALI_STATIC EGLBoolean __egl_parse_context_attribute_list( const EGLint *attrib_list,
                                                           EGLint     *client_version,
                                                           EGLBoolean *robustness_enabled,
                                                           EGLint     *reset_notification_strategy,
                                                           __egl_thread_state *tstate )
{
	mali_bool done = MALI_FALSE;
	EGLint *attrib = (EGLint*) attrib_list;

	if ( NULL == attrib_list ) return EGL_TRUE;

	*robustness_enabled = EGL_FALSE;
	*reset_notification_strategy = EGL_FALSE;

#if EGL_EXT_create_context_robustness_ENABLE
	*reset_notification_strategy = EGL_NO_RESET_NOTIFICATION_EXT;
#endif /* EGL_EXT_create_context_robustness_ENABLE */

	while ( !done )
	{
		switch ( attrib[0] )
		{
			case EGL_CONTEXT_CLIENT_VERSION:
				/* this attribute can only be specified for OpenGL ES contexts,
				 * ie. when the current rendering API is EGL_OPENGL_ES_API */
				if ( EGL_OPENGL_ES_API != tstate->api_current )
				{
					__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
					return EGL_FALSE;
				}

				/* current rendering API is OpenGL ES */
				if ( (1 != attrib[1]) && (2 != attrib[1]) ) /* only accept OpenGL ES 1.x and 2.x as valid versions */
				{
					__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
					return EGL_FALSE;
				}
				/* client version ok */
				*client_version = attrib[1];
				break;

#ifdef EGL_EXT_create_context_robustness_ENABLE
			case EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT:
				if ( (EGL_FALSE != attrib[1]) && (EGL_TRUE != attrib[1]) )
				{
					__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
					return EGL_FALSE;
				}

				if ( (EGL_TRUE == attrib[1]) && (EGL_OPENGL_ES_API != tstate->api_current) )
				{
					__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
					return EGL_FALSE;
				}

				*robustness_enabled = attrib[1];
				break;

			case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT:
				if ( (EGL_NO_RESET_NOTIFICATION_EXT != attrib[1]) && (EGL_LOSE_CONTEXT_ON_RESET_EXT != attrib[1]) )
				{
					__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
					return EGL_FALSE;
				}
				*reset_notification_strategy = attrib[1];
				break;
#endif /* EGL_EXT_create_context_robustness_ENABLE */

			case EGL_NONE:
				done = MALI_TRUE;
				break;

			default:
				/* faulty in-parameter, set error and exit */
				__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
				return EGL_FALSE;
		}
		attrib += 2;
	}

	return EGL_TRUE;
}

EGLContext _egl_create_context( EGLDisplay __dpy,
                                EGLConfig __config,
                                EGLContext __share_list,
                                const EGLint *attrib_list,
                                void *thread_state )
{
	EGLContext handle;
	EGLint client_version;
	EGLBoolean robustness_enabled = EGL_FALSE;
	EGLint reset_notification_strategy = EGL_FALSE;
	egl_display *dpy = NULL;
	egl_config  *config = NULL;
	egl_context *share_list = EGL_NO_CONTEXT;
	egl_context *context = EGL_NO_CONTEXT;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	mali_err_code error;

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_NO_CONTEXT );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate ), EGL_NO_CONTEXT );
	MALI_CHECK( NULL != (config = __egl_get_check_config( __config, __dpy, tstate ) ), EGL_NO_CONTEXT );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_FALSE );

	/* verify that the shared context is of the same client API type as the current one */
	if ( EGL_NO_CONTEXT != __share_list )
	{
		share_list = __egl_get_context_ptr( __share_list, __dpy );
		if ( NULL == share_list )
		{
			__egl_set_error( EGL_BAD_CONTEXT, tstate );
			return EGL_NO_CONTEXT;
		}

		if ( tstate->api_current != share_list->api )
		{		
			__egl_set_error( EGL_BAD_CONTEXT, tstate );
			return EGL_NO_CONTEXT;
		}
	}

	client_version = 1;
	MALI_CHECK( EGL_TRUE == __egl_parse_context_attribute_list( attrib_list, &client_version, &robustness_enabled, &reset_notification_strategy, tstate ), EGL_NO_CONTEXT );

	switch ( tstate->api_current )
	{
		case EGL_OPENGL_ES_API:
#ifdef EGL_MALI_GLES
			context = __egl_gles_create_context( config, share_list, client_version, robustness_enabled, reset_notification_strategy, tstate );
#endif
			break;

		case EGL_OPENVG_API:
#ifdef EGL_MALI_VG
			context = __egl_vg_create_context( config, share_list, client_version, tstate );
#endif
			break;

		case EGL_NONE:
		default:
			MALI_DEBUG_ASSERT( EGL_FALSE, ("Invalid API specifier: %d", tstate->api_current) );
			__egl_set_error( EGL_BAD_MATCH, tstate );
			return EGL_NO_CONTEXT;
	}

	MALI_CHECK( EGL_NO_CONTEXT != context, EGL_NO_CONTEXT );

	context->config = config;
	context->surface = EGL_NO_SURFACE;
	context->is_lost = EGL_FALSE;

	handle = __egl_add_context_handle( context, __dpy );
	if ( EGL_NO_CONTEXT == handle )
	{
		__egl_release_context( context, thread_state );
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_NO_CONTEXT;
	}

	/* If the context and its subcomponents are all created, then set the reference to 1 */
	context->references = 1;

	error = __mali_linked_list_init( &context->bound_images );

	if ( MALI_ERR_NO_ERROR != error )
	{
		__egl_release_context( context, thread_state );
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_NO_CONTEXT;
	}

	return handle;
}

void __egl_context_unbind_bound_surface( egl_context *ctx, egl_surface *surface )
{
	mali_linked_list_entry *entry;
	mali_bool found = MALI_FALSE;

	entry = __mali_linked_list_get_first_entry( &ctx->bound_images );

	while ( NULL != entry )
	{
		if ( surface == entry->data )
		{
			__mali_linked_list_remove_entry( &ctx->bound_images, entry );
			found = MALI_TRUE;

			break;
		}

		entry = __mali_linked_list_get_next_entry( entry );
	}

	MALI_DEBUG_ASSERT( found, ( "Did not find the image we are releasing" ) );

	/* For release builds */
	MALI_IGNORE( found );
}

/* internal function to release a context-pointer and delete if refcount == 0 */
EGLint __egl_release_context( egl_context* ctx, void *thread_state )
{
	MALI_DEBUG_ASSERT( ctx->references >= 0, ("negative ref count for context") );

	if ( 0 != ctx->references ) return ctx->references;

	if ( EGL_OPENGL_ES_API == ctx->api )
	{
#ifdef EGL_MALI_GLES
		__egl_gles_release_context( ctx, thread_state );
#endif
	}
	else if ( EGL_OPENVG_API == ctx->api )
	{
#ifdef EGL_MALI_VG
		__egl_vg_release_context( ctx );
#endif
	}
	
	__mali_linked_list_deinit( &ctx->bound_images );
	_mali_sys_free( ctx );

	return 0;
}

EGLBoolean _egl_destroy_context_internal( EGLDisplay __dpy, egl_context *ctx, EGLBoolean tag_invalid, void *thread_state )
{
	void *handle_reference = NULL;
	EGLBoolean ret = EGL_FALSE;
	if ( EGL_TRUE == tag_invalid ) ctx->is_valid = EGL_FALSE;

	ctx->references--;

	/* clamp the references to 1 if the context is ready to be destroyed, but is current */
	if ( EGL_TRUE == ctx->is_current && 0 == ctx->references ) ctx->references = 1;

	handle_reference = (void *)ctx;
	if ( 0 == ctx->references )
	{
		ret = __egl_remove_context_handle( (egl_context *)handle_reference, __dpy );
		(void)__egl_release_context( ctx, thread_state );
		return ret;
	}

	return ret;
}

EGLBoolean _egl_destroy_context( EGLDisplay __dpy, EGLContext __ctx, void *thread_state )
{
	egl_display *dpy = NULL;
	egl_context *ctx = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;

	if ( NULL != tstate )
	{
		MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_FALSE );
		MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate ), EGL_FALSE );
		MALI_CHECK( EGL_NO_CONTEXT != (ctx = __egl_get_check_context( __ctx, __dpy, tstate ) ), EGL_FALSE );
		MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_FALSE );
	}
	else
	{
		/* This is needed in case the call is made from the destructor.
		 * Error handling for missing thread state is performed in entrypoint
		 */
		ctx = __egl_get_context_ptr( __ctx, __dpy );
	}

	_egl_destroy_context_internal( __dpy, ctx, EGL_TRUE, ( __egl_thread_state * ) thread_state );

	return EGL_TRUE;
}

EGLBoolean _egl_query_context( EGLDisplay __dpy,
                               EGLContext __ctx,
                               EGLint attribute,
                               EGLint *value,
                               void *thread_state )
{
	egl_display *dpy = NULL;
	egl_context *ctx = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate ), EGL_FALSE );
	MALI_CHECK( EGL_NO_CONTEXT != (ctx = __egl_get_check_context( __ctx, __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_null_value( value, EGL_BAD_PARAMETER, tstate ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_FALSE );

	switch ( attribute )
	{
		case EGL_CONTEXT_CLIENT_TYPE:
			*value = ctx->api;
			break;

		case EGL_CONTEXT_CLIENT_VERSION:
			*value = ctx->client_version;
			break;

		case EGL_RENDER_BUFFER:
			if ( EGL_NO_SURFACE != ctx->surface )
			{
				switch ( ctx->surface->type )
				{
					case MALI_EGL_WINDOW_SURFACE:
						*value = EGL_BACK_BUFFER; /* we use back buffer for windows */
						break;
					case MALI_EGL_PBUFFER_SURFACE:
						*value = EGL_BACK_BUFFER;
						break;
					case MALI_EGL_PIXMAP_SURFACE:
						*value = EGL_SINGLE_BUFFER;
						break;
					case MALI_EGL_INVALID_SURFACE:
						/* Fall-through */
					default:
						MALI_DEBUG_ASSERT( 0, ("EGL surface is tagged with invalid surface type") );
						return EGL_FALSE;
				}
			}
			else
			{
				*value = EGL_NONE;
			}
			break;

		case EGL_CONFIG_ID:
			*value = ctx->config->config_id;
			break;

		default:
			__egl_set_error( EGL_BAD_ATTRIBUTE, tstate );
			return EGL_FALSE;
	}

	return EGL_TRUE;
}

/**
 * @brief Checks if a surface and a context is compatible with eachother
 * @param ctx the context
 * @param surface the surface
 * @return EGL_TRUE if compatible, EGL_FALSE else
 */
MALI_STATIC MALI_CHECK_RESULT EGLBoolean __egl_context_and_surface_compatible( egl_context *ctx, egl_surface *surface )
{
	MALI_DEBUG_ASSERT( NULL != ctx, ("ctx is a NULL pointer") );
	MALI_DEBUG_ASSERT( NULL != surface, ("surface is a NULL pointer") );

	/* verify various aspects of similarity between context config and surface config */
	if ( ctx->config == surface->config ) return EGL_TRUE;	

	/* they must support the same type of color buffer ( RGB or luminance ) */
	if ( ctx->config->color_buffer_type != surface->config->color_buffer_type ) return EGL_FALSE;
	
	if ( ctx->config->alpha_size != surface->config->alpha_size ) return EGL_FALSE;	

	/* OpenGL ES utilizes depth and stencil buffer */
	if ( EGL_OPENGL_ES_API == ctx->api )
	{
		if ( ctx->config->depth_size != surface->config->depth_size ) return EGL_FALSE;				
		if ( ctx->config->stencil_size != surface->config->stencil_size ) return EGL_FALSE;
	}	

	/* the surface was created with respect to an EGLConfig supporting client API rendering of the same type as
	 * the API type of the context
	 */
	if ( EGL_OPENGL_ES_API == ctx->api )
	{
		if ( !(surface->config->renderable_type & EGL_OPENGL_ES_BIT) && !(surface->config->renderable_type & EGL_OPENGL_ES2_BIT) ) return EGL_FALSE;
	}
	else if ( EGL_OPENVG_API == ctx->api )
	{
		if ( !(surface->config->renderable_type & EGL_OPENVG_BIT) ) return EGL_FALSE;
	}	

	/* they must have color buffers and ancillary buffers of the same depth ( per component )*/
	switch ( surface->config->color_buffer_type )
	{
		case EGL_RGB_BUFFER:
			if ( ctx->config->red_size != surface->config->red_size ||
			     ctx->config->green_size != surface->config->green_size ||
			     ctx->config->blue_size != surface->config->blue_size )
			{								
				return EGL_FALSE;
			}
			break;

		case EGL_LUMINANCE_BUFFER:
			if ( ctx->config->luminance_size != surface->config->luminance_size ) return EGL_FALSE;						
			break;

		default:
			MALI_DEBUG_ASSERT(0, ("Unknown color_buffer_type in __egl_context_and_surface_compatible\n"));
			break;
	}

	return EGL_TRUE;
}

/**
 * @brief Checks if a surface is bound to a given thread state api
 * @param surface pointer to egl_surface
 * @param tstate_api pointer to thread state api
 * @return EGL_TRUE if bound, EGL_FALSE else
 */
MALI_STATIC MALI_CHECK_RESULT EGLBoolean __egl_surface_bound_to_api( egl_surface *surface, __egl_thread_state_api *tstate_api )
{
	if ( NULL == tstate_api ) return EGL_FALSE;

	if ( surface == tstate_api->read_surface || surface == tstate_api->draw_surface ) return EGL_TRUE;

	return EGL_FALSE;
}

/**
 * @brief Verifies that a context is not current to another thread than the current
 * @param ctx pointer to egl_context
 * @param pointer to the current thread state
 * @param tstate_api the current thread state api
 * @return EGL_TRUE if context is not bound to any other thread than current, EGL_FALSE else
 */
MALI_STATIC MALI_CHECK_RESULT EGLBoolean __egl_make_current_verify_context( egl_context *ctx,
                                                                            __egl_thread_state *tstate,
                                                                            __egl_thread_state_api *tstate_api )
{
	MALI_DEBUG_ASSERT_POINTER( tstate );
	 /* If ctx is current to some other thread an EGL_BAD_ACCESS error is generated. */
	if ( EGL_NO_CONTEXT != ctx )
	{
		if ( EGL_TRUE == ctx->is_current )
		{
			/* we know that the context is current to another thread if it's not
			 * current to its own thread state api */
			if ( (NULL == tstate_api) || (tstate_api->context != ctx) )
			{
				__egl_set_error( EGL_BAD_ACCESS, tstate );
				return EGL_FALSE;
			}
		}
	}

	return EGL_TRUE;
}

/**
 * @brief Verifies that a surface and context is compatible and may be set current
 * @param surface surface to verify
 * @param ctx context to verify
 * @param tstate current thread state
 * @param tstate_api thread state api to make resources current on
 * @return EGL_TRUE if resources can be made current, EGL_FALSE else
 * @note verifies the following:
 *       - surface is not bound to any context in another thread
 *       - surface is compatible with context
 *       - native window is valid (for window surfaces)
 */
MALI_STATIC MALI_CHECK_RESULT EGLBoolean __egl_make_current_verify_surface( egl_surface *surface,
                                                                            egl_context *ctx,
                                                                            __egl_thread_state *tstate )
{
	MALI_DEBUG_ASSERT_POINTER( tstate );
	/* surface is not allowed to be bound to contexts in another thread */
	if ( EGL_NO_SURFACE != surface )
	{
		if ( EGL_TRUE == surface->is_current )
		{
			/*
			 * We have both an OpenVG and an OpenGL ES thread state api, so we'll have to check if the surface is current to any of them.
			 * A surface is allowed to be current, as long as it's current to its own thread (eg. it may be current to both vg and gles)
			 */
			if ( ( EGL_FALSE == __egl_surface_bound_to_api( surface, tstate->api_vg ) ) &&
			     ( EGL_FALSE == __egl_surface_bound_to_api( surface, tstate->api_gles ) ) )
			{
				__egl_set_error( EGL_BAD_ACCESS, tstate );
				return EGL_FALSE;
			}
		}

		/* surface has to be compatible with context */
		if ( EGL_NO_CONTEXT != ctx )
		{
			if ( EGL_FALSE == __egl_context_and_surface_compatible( ctx, surface ) )
			{
				__egl_set_error( EGL_BAD_MATCH, tstate );
				return EGL_FALSE;
			}
		}

		/* check if native window underlying the surface is still valid */
		if ( MALI_EGL_WINDOW_SURFACE == surface->type )
		{
			if ( EGL_FALSE == __egl_platform_window_valid( surface->dpy->native_dpy, surface->win ) )
			{
				MALI_API_TRACE_EGLNATIVEWINDOWTYPE_RESIZED( surface->win );
				__egl_set_error( EGL_BAD_NATIVE_WINDOW, tstate );
				return EGL_FALSE;
			}
		}
	}
	return EGL_TRUE;
}

/**
 * @brief Make a context not current given a thread state api
 * @param tstate current thread state
 * @param tstate_api thread state api to make not current
 * @return EGL_TRUE on success, EGL_FALSE on error
 */
MALI_STATIC MALI_CHECK_RESULT EGLBoolean __egl_make_current_release_context( __egl_thread_state *tstate,
                                                                   __egl_thread_state_api *tstate_api )
{
	EGLBoolean retval;
	EGLDisplay display_handle;

	if ( EGL_OPENGL_ES_API == tstate->api_current )
	{
#ifdef EGL_MALI_GLES
		__egl_gles_remove_framebuilder_from_client_ctx( tstate );
		tstate->context_gles = NULL;
#endif
	}

	if ( EGL_OPENVG_API == tstate->api_current )
	{
#ifdef EGL_MALI_VG
		__egl_vg_remove_framebuilder_from_client_ctx( tstate );
		tstate->context_vg = NULL;
#endif
	}

	display_handle = __egl_get_display_handle( tstate_api->display );
	tstate_api->context->is_current = EGL_FALSE;
	tstate_api->context->surface = EGL_NO_SURFACE;
	retval = _egl_destroy_context_internal( display_handle, tstate_api->context, EGL_FALSE, ( void * ) tstate );
	tstate_api->context = EGL_NO_CONTEXT;
	MALI_IGNORE( retval );

	return EGL_TRUE;
}

/**
 * @brief Make a surface not current given a thread state api
 * @param draw_read either EGL_DRAW or EGL_READ to indicate which surface to make not current
 * @param tstate pointer to current thread state
 * @param tstate_api pointer to thread state api
 * @return EGL_TRUE on success, EGL_FALSE on error
 */
MALI_STATIC MALI_CHECK_RESULT EGLBoolean __egl_make_current_release_surface( EGLint draw_read,
                                                                   __egl_thread_state *tstate,
                                                                   __egl_thread_state_api *tstate_api )
{
	EGLDisplay display_handle;
	EGLBoolean retval;
	__egl_thread_state_api *other_api;

	display_handle = __egl_get_display_handle( tstate_api->display );
	if ( EGL_DRAW == draw_read )
	{
		if ( tstate_api == tstate->api_gles ) other_api = tstate->api_vg;
		else other_api = tstate->api_gles;

		if( EGL_NO_SURFACE != tstate_api->draw_surface )
		{
			tstate_api->draw_surface->is_current = __egl_surface_bound_to_api( tstate_api->draw_surface, other_api );

#ifdef HAVE_ANDROID_OS
			__egl_platform_disconnect_surface( tstate_api->draw_surface );
#endif

			retval = _egl_destroy_surface_internal( display_handle, tstate_api->draw_surface, EGL_FALSE, tstate );
			if ( EGL_TRUE == retval )
			{
				/* surface was destroyed - check if this is the same surface as the read surface */
				if ( tstate_api->draw_surface == tstate_api->read_surface )
				{
					tstate_api->read_surface = EGL_NO_SURFACE;
				}
			}

			tstate_api->draw_surface = EGL_NO_SURFACE;
		}
	}
	else if ( EGL_READ == draw_read )
	{
		if ( tstate_api == tstate->api_gles ) other_api = tstate->api_vg;
		else other_api = tstate->api_gles;

		if ( EGL_NO_SURFACE != tstate_api->read_surface )
		{
			tstate_api->read_surface->is_current = __egl_surface_bound_to_api( tstate_api->read_surface, other_api );

			retval = _egl_destroy_surface_internal( display_handle, tstate_api->read_surface, EGL_FALSE, tstate );

			tstate_api->read_surface = EGL_NO_SURFACE;
		}
	}

	MALI_IGNORE( retval );

	return EGL_TRUE;
}

#if defined( NEXELL_LEAPFROG_FEATURE_VIEWPORT_SCALE_EN ) || defined( NEXELL_LEAPFROG_FEATURE_FBO_SCALE_EN ) /* nexell for leapfrog */
#include <gles1_context.h>
#endif
#ifdef NEXELL_LEAPFROG_FEATURE_VIEWPORT_SCALE_EN
#include <gles1_state/gles1_viewport.h>
#endif

/* function for eglMakeCurrent to be able to call this outside the current thread state */
EGLBoolean _egl_make_current( EGLDisplay __dpy,
                               EGLSurface __draw,
                               EGLSurface __read,
                               EGLContext __ctx,
                               void *thread_state )
{
	EGLenum api = EGL_NONE;
	EGLBoolean retval = EGL_TRUE;
	egl_display *dpy = NULL;
	egl_context *ctx = EGL_NO_CONTEXT;
	egl_surface *draw = EGL_NO_SURFACE;
	egl_surface *read = EGL_NO_SURFACE;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	__egl_thread_state_api *tstate_api = NULL;

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate ), EGL_FALSE );

	MALI_IGNORE( retval );

#if MALI_INSTRUMENTED
	{
		mali_instrumented_frame *instr_frame = _instrumented_acquire_current_frame();
		if (NULL != instr_frame)
		{
			_mali_profiling_analyze(instr_frame); /* move SW counters from TLS to this frame */
			_instrumented_release_frame(instr_frame);
		}
	}
#endif

	/* retrieve the surfaces and context */
	if ( EGL_NO_SURFACE != __draw ) MALI_CHECK( EGL_NO_SURFACE != (draw = __egl_get_check_surface( __draw, __dpy, tstate ) ), EGL_FALSE );
	if ( EGL_NO_SURFACE != __read ) MALI_CHECK( EGL_NO_SURFACE != (read = __egl_get_check_surface( __read, __dpy, tstate ) ), EGL_FALSE );
	if ( EGL_NO_CONTEXT != __ctx )  MALI_CHECK( EGL_NO_CONTEXT != (ctx  = __egl_get_check_context( __ctx,  __dpy, tstate ) ), EGL_FALSE );

	/* either none or all of ctx,draw,read can be assigned no context / surface */
	if ( (( EGL_NO_CONTEXT == ctx ) && ( EGL_NO_SURFACE!=draw || EGL_NO_SURFACE!=read )) ||
		  (( EGL_NO_CONTEXT != ctx ) && ( EGL_NO_SURFACE==draw || EGL_NO_SURFACE==read )) )
	{	
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return EGL_FALSE;
	}

	/* A terminating display is only allowed to be used when setting a context not current */
	if ( dpy->flags & EGL_DISPLAY_TERMINATING )
	{
		if ( EGL_NO_CONTEXT != __ctx || EGL_NO_SURFACE != __draw || EGL_NO_SURFACE != __read )
		{
			__egl_set_error( EGL_NOT_INITIALIZED, tstate );
			return EGL_FALSE;
		}
	}


	if ( EGL_NO_CONTEXT != ctx )
	{

		/* the client API to use is given by the ctx if it is not EGL_NO_CONTEXT */
		if ( EGL_OPENGL_ES_API == ctx->api )
		{
			tstate_api = tstate->api_gles;
			api = EGL_OPENGL_ES_API;
		}
		else if ( EGL_OPENVG_API == ctx->api )
		{
			tstate_api = tstate->api_vg;
			api = EGL_OPENVG_API;
			/* an OpenVG context is not allowed to have different read and write surfaces */
			if ( draw !=read )
			{
				__egl_set_error( EGL_BAD_MATCH, tstate );
				return EGL_FALSE;
			}
		}

		/* return EGL_CONTEXT_LOST in case of power management event */
		if ( EGL_TRUE == ctx->is_lost )
		{
			__egl_set_error( EGL_CONTEXT_LOST, tstate );
			return EGL_FALSE;
		}
	}
	else
	{
		tstate_api = __egl_get_current_thread_state_api( tstate, &api );
	}

	MALI_CHECK( EGL_TRUE == __egl_make_current_verify_context( ctx, tstate, tstate_api ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_make_current_verify_surface( read, ctx, tstate ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_make_current_verify_surface( draw, ctx, tstate ), EGL_FALSE );

	/* check if the same surfaces and context are being set again */
	if ( NULL != tstate_api )
	{
		if ( (ctx == tstate_api->context) && (draw == tstate_api->draw_surface) && (read == tstate_api->read_surface) )
		{
			return EGL_TRUE;
		}
	}

	if ( (NULL != tstate_api) && (EGL_NO_CONTEXT != tstate_api->context) )
	{
		/* flush any pbuffer operations before we possibly delete the context */
		if ( (draw != tstate_api->draw_surface) && (MALI_EGL_PBUFFER_SURFACE == tstate_api->draw_surface->type) && EGL_FALSE == tstate_api->draw_surface->is_bound )
		{
			retval = __egl_mali_post_to_pbuffer_surface( tstate_api->draw_surface, tstate, tstate_api );
		}
		/* release the context */
		if ( ctx != tstate_api->context )
		{
			MALI_CHECK( EGL_TRUE == __egl_make_current_release_context( tstate, tstate_api ), EGL_FALSE );
		}

		/* release the draw surface */
		if ( draw != tstate_api->draw_surface )
		{
			MALI_CHECK( EGL_TRUE == __egl_make_current_release_surface( EGL_DRAW, tstate, tstate_api ), EGL_FALSE );
		}

		/* release the read surface */
		if ( read != tstate_api->read_surface )
		{
			MALI_CHECK( EGL_TRUE == __egl_make_current_release_surface( EGL_READ, tstate, tstate_api ), EGL_FALSE );
		}

		/* If we are terminating, see if there are any contexts/surfaces
		 * registered to this display, and if not, properly terminate
		 * the display */
		if ( dpy->flags & EGL_DISPLAY_TERMINATING
			 && 0 == __mali_named_list_size( dpy->surface )
			 && 0 == __mali_named_list_size( dpy->context )
		    && 0 == __mali_named_list_size( tstate->main_ctx->egl_images ) )
		{
			__egl_release_display( dpy, EGL_FALSE );
		}
	}

	/* bind the resources if they are given */
	if ( EGL_NO_SURFACE!=draw &&  EGL_NO_CONTEXT!=ctx )
	{
		/* We do not allow resources to be created on a display once
		 * termination begins. This means that a terminating/now terminated
		 * display should not be able to bind new resources. We assert that
		 * here: */
		MALI_DEBUG_ASSERT(
			(dpy->flags & EGL_DISPLAY_INITIALIZED) &&
			( (!(dpy->flags & EGL_DISPLAY_TERMINATING)) ||
			( EGL_TRUE == draw->is_current &&
			  EGL_TRUE == read->is_current &&
			  EGL_TRUE == ctx->is_current ) ), ("Tried to bind contexts/surfaces on terminated/terminating display") );

		/* create a new thread state api if selected api is not existing */
		if ( NULL == tstate_api )
		{
			tstate_api = (__egl_thread_state_api *)_mali_sys_calloc(1, sizeof( __egl_thread_state_api ) );
			if ( NULL == tstate_api )
			{
				__egl_set_error( EGL_BAD_ALLOC, tstate );
				return EGL_FALSE;
			}
		}

		if ( EGL_OPENGL_ES_API == api ) tstate->api_gles = tstate_api;
		else if ( EGL_OPENVG_API == api ) tstate->api_vg = tstate_api;

#if HAVE_ANDROID_OS
		if ( draw != tstate_api->draw_surface ) 
		{
			if ( EGL_FALSE == __egl_platform_connect_surface( draw ) )
			{
				/* no VG on Android, so don't touch tstate->api_vg */
				_mali_sys_free( tstate->api_gles );
				tstate->context_gles = NULL;
				tstate->api_gles = NULL;
				__egl_set_error( EGL_BAD_ALLOC, tstate );
				return EGL_FALSE;
			}
		}
#endif

		switch ( ctx->api )
		{
			case EGL_OPENGL_ES_API:
#ifdef EGL_MALI_GLES
				tstate->context_gles = ctx->api_context;
				if ( EGL_FALSE == __egl_gles_make_current( ctx, draw, read, tstate ) )
				{
#if HAVE_ANDROID_OS
					__egl_platform_disconnect_surface( draw );
#endif
					/* release context */
					if ( ctx == tstate_api->context )
					{
						__egl_gles_remove_framebuilder_from_client_ctx( tstate );
						tstate_api->context->is_current = EGL_FALSE;
						retval = _egl_destroy_context_internal( __dpy, ctx, EGL_FALSE, thread_state );
						tstate_api->context->surface = EGL_NO_SURFACE;
						tstate_api->context = EGL_NO_CONTEXT;
					}
					/* release draw surface */
					if ( draw == tstate_api->draw_surface )
					{
						retval = __egl_make_current_release_surface( EGL_DRAW, tstate, tstate_api );
					}
					/* release the read surface */
					if ( read == tstate_api->read_surface )
					{
						retval = __egl_make_current_release_surface( EGL_READ, tstate, tstate_api );
					}
					_mali_sys_free( tstate->api_gles );
					tstate->context_gles = NULL;
					tstate->api_gles = NULL;
					__egl_set_error( EGL_BAD_ALLOC, tstate );
					return EGL_FALSE;
				}
#endif /* EGL_MALI_GLES */
				break;

			case EGL_OPENVG_API:
#ifdef EGL_MALI_VG
				tstate->context_vg = ctx->api_context;
				if ( EGL_FALSE == __egl_vg_make_current( ctx, draw, EGL_TRUE, tstate ) )
				{
					/* release context */
					if ( ctx == tstate_api->context )
					{
						__egl_vg_remove_framebuilder_from_client_ctx( tstate );
						tstate_api->context->is_current = EGL_FALSE;
						retval = _egl_destroy_context_internal( __dpy, ctx, EGL_FALSE, ( void * ) tstate );
						tstate_api->context->surface = EGL_NO_SURFACE;
						tstate_api->context = EGL_NO_CONTEXT;
					}
					/* release draw surface */
					if ( draw == tstate_api->draw_surface )
					{
						retval = __egl_make_current_release_surface( EGL_DRAW, tstate, tstate_api );
					}
					/* release the read surface */
					if ( read == tstate_api->read_surface )
					{
						retval = __egl_make_current_release_surface( EGL_READ, tstate, tstate_api );
					}
					_mali_sys_free( tstate->api_vg );
					tstate->context_vg = NULL;
					tstate->api_vg = NULL;
					__egl_set_error( EGL_BAD_ALLOC, tstate );
					return EGL_FALSE;
				}
#endif /* EGL_MALI_VG */
				break;

			default :
				MALI_DEBUG_ASSERT( EGL_FALSE, (" ") );
		}		

		/* update currents */
		tstate_api->display = dpy;

		if ( ctx != tstate_api->context ) ctx->references++;
		tstate_api->context = ctx;
		ctx->is_current = EGL_TRUE;
		ctx->surface = draw; /* place a reference to the current surface when the context is current */

		if ( draw != tstate_api->draw_surface ) 
		{
			draw->references++;
		}
		tstate_api->draw_surface = draw;
		draw->is_current = EGL_TRUE;

		if ( read != tstate_api->read_surface )
		{
			read->references++;
		}
		tstate_api->read_surface = read;
		read->is_current = EGL_TRUE;

		/* platform has to provide vsync support */
		draw->interval = __egl_platform_supports_vsync() ? 1 : 0;

		/* default value is 1, clamped to min/max swap interval range */
		if (draw->interval < tstate_api->draw_surface->config->min_swap_interval)
			draw->interval = tstate_api->draw_surface->config->min_swap_interval;
		else if (draw->interval > tstate_api->draw_surface->config->max_swap_interval)
			draw->interval = tstate_api->draw_surface->config->max_swap_interval;
	}

#if defined( NEXELL_LEAPFROG_FEATURE_VIEWPORT_SCALE_EN ) || defined( NEXELL_LEAPFROG_FEATURE_FBO_SCALE_EN ) /* nexell for leapfrog */
	if(ctx)
	{		
		gles_context *pgles_ctx = ctx->api_context;
		/* if surface size exist */
		if(pgles_ctx && ctx->surface && ctx->surface->width && ctx->surface->height)
		{
			#if 0
			__egl_platform_get_window_size( ctx->surface->win, ctx->surface->platform, &pgles_ctx->scale_surf_width, &pgles_ctx->scale_surf_height );
			#else
			pgles_ctx->scale_surf_width = ctx->surface->width;
			pgles_ctx->scale_surf_height = ctx->surface->height;
			#endif
			pgles_ctx->sacle_surf_pixel_byte_size = ctx->surface->config->buffer_size / 8;
			#ifdef NEXELL_LEAPFROG_FEATURE_VIEWPORT_SCALE_EN
			_gles1_viewport(&pgles_ctx->state, 0, 0, pgles_ctx->scale_surf_width, pgles_ctx->scale_surf_height);
			#endif
		}
		#ifdef NEXELL_LEAPFROG_FEATURE_FBO_SCALE_EN
		if(pgles_ctx)
		{
			nx_gles_scaler_framebuffer_object_new(pgles_ctx);
		}
		#endif		
	}
#endif

	return EGL_TRUE;
}

EGLContext _egl_get_current_context( void *thread_state )
{
	EGLContext context = EGL_NO_CONTEXT;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;

	switch ( tstate->api_current )
	{
		case EGL_OPENGL_ES_API:
			if ( NULL != tstate->api_gles )
			{
				MALI_DEBUG_ASSERT_POINTER( tstate->api_gles->display );
				context = __egl_get_context_handle( tstate->api_gles->context, __egl_get_display_handle( tstate->api_gles->display ) );
			}
			break;

		case EGL_OPENVG_API:
			if ( NULL != tstate->api_vg )
			{
				MALI_DEBUG_ASSERT_POINTER( tstate->api_vg->display );
				context = __egl_get_context_handle( tstate->api_vg->context, __egl_get_display_handle( tstate->api_vg->display ) );
			}
			break;

		default:
			break;
	}

	return context;
}

