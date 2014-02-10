/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include "egl_common.h"

#include "mali_instrumented_context.h"
#include "base/mali_profiling.h"
#include "sw_profiling.h"
#include <shared/mali_surface.h>
#include <shared/mali_image.h>
#include <shared/mali_pixel_format.h>
#include <shared/m200_readback_util.h>
#include <shared/m200_gp_frame_builder.h>
#include "shared/m200_gp_frame_builder_inlines.h"
#include <shared/m200_incremental_rendering.h>

#include "egl_platform.h"
#include "egl_mali.h"

#ifdef EGL_USE_VSYNC
#include "egl_vsync.h"
#endif

#ifdef _EGL_ARM11_PROFILING_H_
#include "egl_arm11_profiling.h"
#endif

#include "base/gp/mali_gp_job.h"
#include "regs/MALIGP2/mali_gp_plbu_config.h"
#include "egl_config.h"
#include "egl_api_trace.h"
#include "mali_instrumented_plugin.h"

#if MALI_OPROFILE_SAMPLING
#include <instrumented/mali_oprofile.h>
#endif

#if EGL_FEATURE_SURFACE_SCALING_ENABLE
#include "egl_surface_scaling.h"
#endif

MALI_STATIC egl_frame_swap_parameters *__egl_mali_create_swap_params(__egl_thread_state *tstate, egl_buffer* buffer);
MALI_STATIC void __egl_mali_destroy_swap_params(egl_frame_swap_parameters *params);

MALI_STATIC_INLINE mali_bool __egl_mali_surface_defer_release(egl_surface const *surface)
{
	mali_bool retval = MALI_FALSE;

#if !defined(MALI_ANDROID_API)
#if EGL_BACKEND_X11
	retval = MALI_FALSE;
#else
	if ( (surface->caps & SURFACE_CAP_DIRECT_RENDERING) && surface->interval>0 && surface->num_buffers>1 )
	{
		retval = MALI_TRUE;
	}
#endif
#else
	/* On Android we need deferred release whenever the surface is rendering to the framebuffer */
	retval = __egl_platform_surface_is_deferred_release( surface );
#endif

	return retval;
}

/** Called on params->ds_consumer_display activation from the __egl_swap_activation_callback function.
 *  The function must release the ds_consumer_display, or release it later, since as long as this
 *  consumer object holds the buffer, it can not be used by anybody else.
 */
MALI_STATIC void _egl_mali_frame_swap(egl_frame_swap_parameters* params)
{
	egl_buffer              *buffer = NULL;
	egl_surface             *draw_surface = NULL;
	__egl_main_context      *egl = NULL;

	MALI_DEBUG_ASSERT_POINTER( params );

#if MALI_INSTRUMENTED
	MALI_DEBUG_ASSERT_POINTER(params->instrumented_frame);
	_instrumented_set_active_frame(params->instrumented_frame);
#endif

	egl = __egl_get_main_context();
	MALI_DEBUG_ASSERT_POINTER( egl );
 
	buffer = params->buffer;
	MALI_DEBUG_ASSERT_POINTER( buffer );

	draw_surface = buffer->surface;
	MALI_DEBUG_ASSERT_POINTER( draw_surface );

#ifdef EGL_USE_VSYNC
	if( MALI_FALSE == egl->never_blit )
	{
		/* wait until (vsync_recorded - current_vsync) > interval */
		while(1)
		{
			if ( 0 == draw_surface->interval ) break;
			if ( egl->vsync_busy )
			{
				_mali_sys_yield();
			}
			else
			{
				if ( (__egl_vsync_get() - draw_surface->vsync_recorded) >= draw_surface->interval ) break;
			}
		}
	}
#endif /* EGL_USE_VSYNC */

#if MALI_INSTRUMENTED
	if (NULL != params->instrumented_frame)
	{
		/* Dump frame buffer */
		_instrumented_frame_completed( params->instrumented_frame, buffer->render_target );
	}
#endif /* MALI_INSTRUMENTED */
 
#if MALI_ANDROID_API > 12
	__egl_platform_queue_buffer( egl->base_ctx, buffer );
#else
	__egl_platform_swap_buffers( egl->base_ctx, draw_surface->dpy->native_dpy, draw_surface, buffer->render_target, draw_surface->interval );
#endif

	/* Release the previous buffer if required */
	if ( MALI_NO_HANDLE != params->previous )
	{
		mali_ds_consumer_handle to_release = params->previous->display_consumer;
		MALI_DEBUG_ASSERT( mali_ds_consumer_active( to_release ), ("Consumer %p not active", to_release ));
		mali_ds_consumer_release_all_connections( to_release );
	}

	/* Check if we need to keep current buffer until next swap */
	if ( !params->defer_release )
	{
		mali_ds_consumer_release_all_connections( params->display_consumer );
	}

	_egl_surface_job_decref( buffer->surface );

#if MALI_INSTRUMENTED
	_instrumented_set_active_frame(NULL); /* don't leave dangling pointers in TLS */
#endif /* MALI_INSTRUMENTED */

#ifdef _EGL_ARM11_PROFILING_H_
	arm11_perf_dump();
#endif
}

/**
 * @brief Acquires a context resize
 * @param tstate_api pointer to thread state api data
 * @param width width in pixels
 * @param height height in pixels
 * @return EGL_TRUE on success, EGL_FALSE on failure
 */
MALI_STATIC_INLINE EGLBoolean __egl_mali_context_resize_acquire( __egl_thread_state_api *tstate_api, u32 width, u32 height )
{
	if ( EGL_OPENVG_API == tstate_api->context->api )
	{
#ifdef EGL_MALI_VG
		return __egl_vg_context_resize_acquire( tstate_api->context, width, height );
#endif
	}
	else if ( EGL_OPENGL_ES_API == tstate_api->context->api )
	{
#ifdef EGL_MALI_GLES
		return EGL_TRUE;
#endif
	}

	return EGL_FALSE;
}

/**
 * @brief Rolls back a context resize
 * @param tstate_api pointer to thread state api data
 * @param width width in pixels
 * @param height height in pixels
 */
MALI_STATIC_INLINE void __egl_mali_context_resize_rollback( __egl_thread_state_api *tstate_api, u32 width, u32 height )
{
	if ( EGL_OPENVG_API == tstate_api->context->api )
	{
#ifdef EGL_MALI_VG
		__egl_vg_context_resize_rollback( tstate_api->context, width, height );
#endif
	}
	else if ( EGL_OPENGL_ES_API == tstate_api->context->api )
	{
		/* no need to rollback, since there are no pre-requisits */
	}
}

/**
 * @brief Finishes a context resize
 * @param tstate_api pointer to thread state api data
 * @param width width in pixels
 * @param height height in pixels
 */
MALI_STATIC_INLINE void __egl_mali_context_resize_finish( __egl_thread_state_api *tstate_api, u32 width, u32 height )
{
	if ( EGL_OPENVG_API == tstate_api->context->api )
	{
#ifdef EGL_MALI_VG
		__egl_vg_context_resize_finish( tstate_api->context, width, height );
#endif
	}
	else if ( EGL_OPENGL_ES_API == tstate_api->context->api )
	{
#ifdef EGL_MALI_GLES
		__egl_gles_context_resize_finish( tstate_api->context, width, height );
#endif
	}
}

EGLBoolean __egl_mali_resize_surface( egl_surface *surface, u32 width, u32 height, __egl_thread_state *tstate )
{
	__egl_thread_state_api *tstate_api = NULL;
	mali_surface *old_internal_buffer = NULL;
	mali_surface *new_internal_buffer = NULL;
	EGLBoolean status = EGL_TRUE;
	u32 usage = 0;
	u32 output_width, output_height;

	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_DEBUG_ASSERT_POINTER( tstate );

	tstate_api = __egl_get_current_thread_state_api( tstate, NULL );
	MALI_CHECK_NON_NULL( tstate_api, EGL_FALSE );

	if ( 0 == width || 0 == height ) return EGL_TRUE;

	_egl_surface_release_all_dependencies( surface );

	/* acquire context resize */
	status = __egl_mali_context_resize_acquire( tstate_api, width, height );

	/* get the old usage flag */
	_mali_frame_builder_get_output( surface->frame_builder , MALI_DEFAULT_COLOR_WBIDX, &usage);

	/* resize external / platform */
	output_width = width;
	output_height= height;
	status = __egl_platform_resize_surface( surface, &output_width, &output_height, tstate->main_ctx->base_ctx );
	if ( EGL_FALSE == status )
	{
		if ( surface->internal_target )
		{
			_mali_frame_builder_set_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, surface->internal_target, usage );
		}

		/* we were unable to resize platform specific resources, rollback with old settings */
		__egl_mali_context_resize_rollback( tstate_api, surface->width, surface->height );

		return EGL_TRUE;
	}

	/* now in case the output device (which is platform dependent) cannot support the resolution we want, we need to
	   allocate a bigger internal target to satisfy the application */
	if ( surface->internal_target || ( width > output_width || height > output_height ) )
	{
		const mali_surface_specifier* format;
		mali_surface_specifier sf;
		/* retrieve the old internal target */
		old_internal_buffer = surface->internal_target;

		/* get the format. If we do not had an internal target we grab it from the front buffer now */
		if ( old_internal_buffer ) format = &old_internal_buffer->format;
		else                       format = &surface->buffer[0].render_target->format;

		sf=*format;
		sf.width = width;
		sf.height = height;
		sf.pitch = 0;
		
		/* create new internal target */
		new_internal_buffer = _mali_surface_alloc( MALI_SURFACE_FLAGS_NONE, &sf , 0, tstate->main_ctx->base_ctx);

		if( NULL == new_internal_buffer )
		{
			/* we were unable to resize platform specific resources, rollback with old settings */
			__egl_mali_context_resize_rollback( tstate_api, surface->width, surface->height );

			/* reset old internal target */
			_mali_frame_builder_set_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, surface->internal_target, usage );

			return EGL_TRUE;
		}

		/* keep the new internal buffer */
		surface->internal_target = new_internal_buffer;
	}

	/* discard an internal target we might not need anymore */
	if ( surface->internal_target && ( width <= output_width || height <= output_height ) )
	{
		if ( (surface->caps & SURFACE_CAP_DIRECT_RENDERING) && !(surface->caps & SURFACE_CAP_WRITEBACK_CONVERSION) )
		{
			/* of course we can do this only if we can directly render to the output surface _AND_ do not need WB conversion
			   due to color format mismatches */
			_mali_surface_deref( surface->internal_target );
			surface->internal_target = NULL;
		}
	}

	surface->width  = width;
	surface->height = height;

	if ( surface->internal_target )
	{
		_mali_frame_builder_set_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, surface->internal_target, usage );
	}
	else
	{
		_mali_frame_builder_set_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, surface->buffer[surface->current_buffer].render_target, usage);
	}

	__egl_mali_context_resize_finish( tstate_api, width, height );

	/* release the old internal target */
	if ( old_internal_buffer )
	{
		_mali_surface_deref( old_internal_buffer );
	}

	if ( !(surface->caps & SURFACE_CAP_WRITEBACK_CONVERSION) && __egl_mali_surface_defer_release(surface) )
	{
	    mali_err_code err;
	    egl_frame_swap_parameters *swap_parameters = NULL;
		egl_buffer* initial_on_screen_buffer = &surface->buffer[1];

		MALI_DEBUG_ASSERT(surface->num_buffers>1,("Need at least double buffering for Direct rendering"));

		swap_parameters = __egl_mali_create_swap_params(tstate, initial_on_screen_buffer);
		if ( NULL != swap_parameters )
		{
			err = mali_ds_connect_and_activate_without_callback(swap_parameters->display_consumer, initial_on_screen_buffer->render_target->ds_resource, MALI_DS_READ);
			if ( MALI_ERR_NO_ERROR != err ) 
			{
			    /* This should never happen - if it does we continue without vsync on this special frame "soft-error" */
				__egl_mali_destroy_swap_params(swap_parameters);
				return EGL_TRUE;
			}
			surface->last_swap = swap_parameters;
		}
	}

	return EGL_TRUE;
}

mali_frame_builder *__egl_mali_create_frame_builder( mali_base_ctx_handle base_ctx,
                                                     egl_config *config,
                                                     u32 num_frames,
                                                     u32 num_bufs,
                                                     struct mali_surface** buffers,
                                                     mali_bool readback_after_flush,
                                                     enum mali_frame_builder_type type )
{
	mali_frame_builder     *frame_builder = NULL;
	enum mali_frame_builder_properties fb_props = MALI_FRAME_BUILDER_PROPS_NONE;
	u32 usage = MALI_OUTPUT_COLOR;
	MALI_IGNORE( num_bufs );
	
	/* set AA samples */
	if(config->samples == 16) usage |= MALI_OUTPUT_FSAA_4X;

	/* Actually this looks weird but we never rely on a specific tie-break rule but only
	   have to ensure that they are used in a consistent manner.
	   Since long time "ORIGO_LOWER_LEFT" was (falsely) used by window surfaces, so pbuffers must not use it. */
	if( MALI_FRAME_BUILDER_TYPE_EGL_PBUFFER != type ) fb_props |= MALI_FRAME_BUILDER_PROPS_ORIGO_LOWER_LEFT;

	if( MALI_FRAME_BUILDER_TYPE_EGL_WINDOW == type || MALI_FRAME_BUILDER_TYPE_EGL_COMPOSITOR ) fb_props  |= MALI_FRAME_BUILDER_PROPS_UNDEFINED_AFTER_SWAP; 

	if( readback_after_flush )
	{
		fb_props |= (MALI_FRAME_BUILDER_PROPS_ROTATE_ON_FLUSH);
	}

	frame_builder = _mali_frame_builder_alloc(type, base_ctx, num_frames, fb_props, &egl_funcptrs );
	MALI_CHECK_NON_NULL( frame_builder, NULL );

	if ( NULL != buffers ) _mali_frame_builder_set_output(frame_builder, MALI_DEFAULT_COLOR_WBIDX, buffers[0], usage);
	else _mali_frame_builder_set_output( frame_builder, MALI_DEFAULT_COLOR_WBIDX, NULL, usage ); /* just set the usage bits if no buffer is given yet */

	if ( MALI_FRAME_BUILDER_TYPE_EGL_WINDOW == type || MALI_FRAME_BUILDER_TYPE_EGL_COMPOSITOR == type )
	{
		_mali_frame_builder_set_clearstate(frame_builder, MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_ALL_CHANNELS | MALI_FRAME_BUILDER_BUFFER_BIT_DEPTH | MALI_FRAME_BUILDER_BUFFER_BIT_STENCIL);
	}
	else
	{
		_mali_frame_builder_set_clearstate(frame_builder, MALI_FRAME_BUILDER_BUFFER_BIT_DEPTH | MALI_FRAME_BUILDER_BUFFER_BIT_STENCIL);
	}

	if( readback_after_flush )
	{
		if ( NULL != buffers ) _mali_frame_builder_set_readback(frame_builder, MALI_DEFAULT_COLOR_WBIDX, buffers[0], usage);
		else _mali_frame_builder_set_readback( frame_builder, MALI_DEFAULT_COLOR_WBIDX, NULL, usage ); /* just set the usage bits if no buffer is given yet */
	}

#ifdef _EGL_ARM11_PROFILING_H_
	arm11_perf_init();
#endif

	return frame_builder;
}

void __egl_mali_destroy_frame_builder( egl_surface *surface )
{
	mali_frame_builder *frame_builder = NULL;
	unsigned int i;

	frame_builder = surface->frame_builder;
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	_mali_frame_builder_set_output( frame_builder, MALI_DEFAULT_COLOR_WBIDX, NULL, 0 );
	_mali_frame_builder_free( frame_builder );
	surface->frame_builder = NULL;

	for ( i=0; i < surface->num_buffers; i++ )
	{
		mali_surface *target = surface->buffer[i].render_target;

		if ( NULL != target )
		{
			_mali_surface_deref( target );
			surface->buffer[i].render_target = NULL;
		}
	}

	if ( NULL != surface->internal_target )
	{
		_mali_surface_deref( surface->internal_target );
		surface->internal_target = NULL;
	}
}

EGLBoolean __egl_mali_begin_new_frame( egl_surface *surface,
                                       EGLBoolean set_framebuilder,
                                       __egl_thread_state *tstate )
{
	if ( EGL_TRUE == set_framebuilder )
	{
		if ( EGL_OPENGL_ES_API == tstate->api_current )
		{
#ifdef EGL_MALI_GLES
			MALI_CHECK( EGL_TRUE == __egl_gles_set_framebuilder( surface, tstate ), EGL_FALSE );
#endif
		}
		else if ( EGL_OPENVG_API == tstate->api_current )
		{
#ifdef EGL_MALI_VG
			MALI_CHECK( EGL_TRUE == __egl_vg_set_framebuilder( surface, tstate ), EGL_FALSE );
#endif
		}
	}

	__egl_platform_begin_new_frame( surface );

	return EGL_TRUE;
}

/**
 * @brief The swap worker thread calls this function to finally return the surface to the compositor
 * @param data Pointer to an egl_frame_swap_parameters struct , passed on to _egl_mali_frame_swap
 * @note As _egl_mali_frame_swap is potentially blocking, this callback is used from a
 *       thread running besides to the application thread and to the regular callback thread
 */
MALI_STATIC void __egl_worker_callback( void* data )
{
	egl_frame_swap_parameters* params = MALI_REINTERPRET_CAST(egl_frame_swap_parameters*)data;
	MALI_DEBUG_ASSERT_POINTER( params );

	_egl_mali_frame_swap( params );
}

/**
 * @brief The activation callback of a display consumer
 * @note Called by the dependency system when the PP job is done and the surface is ready for getting displayed
 */
MALI_STATIC void __egl_swap_activation_callback(mali_base_ctx_handle base_ctx, void *buffer, mali_ds_error status)
{
	egl_frame_swap_parameters* params = MALI_REINTERPRET_CAST(egl_frame_swap_parameters*)buffer;
	MALI_DEBUG_ASSERT_POINTER( params );

	MALI_IGNORE( base_ctx );

	params->error_in_buffer  = status;

#ifdef EGL_ASYNC_SWAPBUFFERS
	{
		__egl_main_context* main_ctx = __egl_get_main_context();

#if MALI_INSTRUMENTED
		MALI_DEBUG_ASSERT_POINTER(params->instrumented_frame);
		_instrumented_set_active_frame(params->instrumented_frame);
#endif

	/* Try to add _egl_mali_frame_swap as an asynchronous job */
#if MALI_ANDROID_API > 13
		if ( MALI_ERR_NO_ERROR == _mali_base_worker_task_add( params->buffer->swap_worker, __egl_worker_callback, params) )
#else
		if ( MALI_ERR_NO_ERROR == _mali_base_worker_task_add( main_ctx->worker, __egl_worker_callback, params) )
#endif
		{
#if MALI_INSTRUMENTED
			_instrumented_set_active_frame(NULL); /* don't leave dangling pointers in TLS */
#endif
			return;
		}
#if MALI_INSTRUMENTED
		_instrumented_set_active_frame(NULL); /* don't leave dangling pointers in TLS */
#endif
	}
#endif
	/* Call directly in case no helper threads were used or the job handover failed */
	__egl_worker_callback ( buffer );
}

/**
 * @brief The release callback of a display consumer
 * @note Called by the dependency system when the surface is taken off-screen
 */
MALI_STATIC mali_ds_release __egl_swap_release_callback(mali_base_ctx_handle base_ctx, void *buffer, mali_ds_error status)
{
	egl_frame_swap_parameters* params = MALI_REINTERPRET_CAST(egl_frame_swap_parameters*)buffer;
	MALI_DEBUG_ASSERT_POINTER( params );

	MALI_IGNORE( base_ctx );
	MALI_IGNORE( status );

	__egl_mali_destroy_swap_params( params );

	return MALI_DS_RELEASE;
}

/**
 * @brief Destructor of egl_frame_swap_parameters
 */
MALI_STATIC void __egl_mali_destroy_swap_params(egl_frame_swap_parameters *params)
{
	if ( params )
	{
		mali_ds_consumer_free( params->display_consumer );
#if MALI_INSTRUMENTED
		if ( params->instrumented_frame )
		{
			_instrumented_release_frame( params->instrumented_frame );
		}
#endif
		_mali_sys_free( params );
	}
}

/**
 * @brief Constructor of egl_frame_swap_parameters
 */
MALI_STATIC egl_frame_swap_parameters *__egl_mali_create_swap_params(__egl_thread_state *tstate, egl_buffer* buffer)
{
	egl_frame_swap_parameters *swap_parameters = (egl_frame_swap_parameters *)_mali_sys_malloc(sizeof(*swap_parameters));

	if ( swap_parameters )
	{
		swap_parameters->display_consumer = mali_ds_consumer_allocate(
		                                         tstate->main_ctx->base_ctx,
				                                 swap_parameters,
		                                         __egl_swap_activation_callback,
		                                         __egl_swap_release_callback	);
		if ( MALI_NO_HANDLE != swap_parameters->display_consumer )
		{
			swap_parameters->buffer = buffer;
			swap_parameters->error_in_buffer = MALI_DS_OK;
			swap_parameters->previous = NULL;
			swap_parameters->defer_release = MALI_FALSE;
#if MALI_INSTRUMENTED
			swap_parameters->instrumented_frame = _instrumented_acquire_current_frame();
#endif
#ifdef NEXELL_LEAPFROG_FEATURE_FBO_SCALE_EN
			swap_parameters->tstate = tstate;
#endif

			return swap_parameters;
		}
		_mali_sys_free( swap_parameters );
	}
	return NULL;
}

/**
 * @brief posts a color buffer
 * This will retrieve the color attachment for the given draw surface, mark
 * the attachment as busy, then flush the mali frame builder.
 * @param surface a surface where the frame builder can be found
 * @param pass_to_display will finish the frame and ensures that the display consumer is activated next on it
 * @param tstate the thread state to use
 * @param tstate_api the thread state api to use
 * @return EGL_TRUE on success, EGL_FALSE else
 * @note this will mark the surface as busy
 */
MALI_STATIC EGLBoolean __egl_mali_post_color_buffer( egl_surface *surface,
                                                     EGLBoolean pass_to_display,
                                                     __egl_thread_state *tstate,
                                                     __egl_thread_state_api *tstate_api )
{
	mali_err_code err = MALI_ERR_NO_ERROR;
	unsigned int old_buffer = surface->current_buffer;
	egl_buffer *buffer = &surface->buffer[surface->current_buffer];
	egl_frame_swap_parameters *swap_parameters = NULL;

	MALI_IGNORE( tstate_api );

	if ( !buffer) return EGL_FALSE;

	_mali_frame_set_inc_render_on_flush(surface->frame_builder, MALI_FALSE );

	if ( EGL_TRUE == pass_to_display )
	{
		swap_parameters = __egl_mali_create_swap_params(tstate, buffer);

		if ( !swap_parameters )
		{
			goto cleanup;
		}

		err = _mali_frame_builder_swap( surface->frame_builder );
		if ( MALI_ERR_NO_ERROR != err )
		{
			goto cleanup;
		}

		/* Ensure the display consumer is activated on it afterwards */
		err =  mali_ds_connect(swap_parameters->display_consumer, buffer->render_target->ds_resource, MALI_DS_READ);
		if( MALI_ERR_NO_ERROR != err )
		{
			/* special case cleanup - we are mostly done and "only" miss the display part.
			   So we need to get a new surface */
			__egl_mali_destroy_swap_params(swap_parameters);

#ifndef EGL_BACKEND_X11
			surface->current_buffer = __egl_platform_get_buffer( surface );
#endif

			return EGL_FALSE;
		}

		/* This one must release the previous one (which might be a NULL ptr though) */
		swap_parameters->previous = surface->last_swap;
		/* And decide whether to chain and track the current one or not */
		swap_parameters->defer_release = __egl_mali_surface_defer_release(surface);
		surface->last_swap = swap_parameters->defer_release ? swap_parameters : NULL;
 
		/* Increment the job count */
		_egl_surface_job_incref( surface );
		/* And finally flush it */
		mali_ds_consumer_flush(swap_parameters->display_consumer);

		/* grab the next output buffer */
#ifndef EGL_BACKEND_X11
		surface->current_buffer = __egl_platform_get_buffer( surface );
#endif
	}
	else
	{
		err = _mali_frame_builder_flush( surface->frame_builder );
	}

#if MALI_ANDROID_API < 13
	{
		u32 usage = 0;
		_mali_frame_builder_get_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, &usage );
#ifndef EGL_BACKEND_X11
		_mali_frame_builder_set_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, surface->buffer[surface->current_buffer].render_target, usage);
#endif
	}
#endif

	return EGL_TRUE;

cleanup:
	/* reset the frame builder since we have callbacks waiting to get done */
	_mali_frame_builder_reset( surface->frame_builder );

	/* set current buffer back to old value, since buffers were not rotated */
	if ( EGL_TRUE == pass_to_display )
	{
		__egl_mali_destroy_swap_params( swap_parameters );
		surface->current_buffer = old_buffer;
	}

	return EGL_FALSE;
}

EGLBoolean __egl_mali_readback_surface( egl_surface *surface, mali_surface *m_surf_preserve, u32 usage, u16 old_width, u16 old_height )
{
	EGLBoolean retval = EGL_TRUE;

	if ( MALI_ERR_NO_ERROR == _mali_frame_builder_use( surface->frame_builder ) )
	{
		/* do a standard readback - setup reference counting on the mali_memory.
			We do not setup a read DS here: It can cause a CoW (we cannot handle on a fixed memory location surface)
			or block following write accesses in case we never swap the buffer again. */
		_mali_shared_mem_ref_usage_addref( m_surf_preserve->mem_ref );
		if ( MALI_ERR_NO_ERROR == _mali_frame_builder_add_callback( surface->frame_builder,
																	(mali_frame_cb_func)_mali_shared_mem_ref_usage_deref,
																	(mali_frame_cb_param)m_surf_preserve->mem_ref ) )
		{
			u16 offset_x = 0; 
			/* setup regular readback, i.e. scaled on resize */
			u16 offset_y = 0;
			u16 mwidth = _mali_frame_builder_get_width( surface->frame_builder );
			u16 mheight = _mali_frame_builder_get_height( surface->frame_builder );
			MALI_IGNORE( old_height );
			MALI_IGNORE( old_width );

			if ( MALI_ERR_NO_ERROR != _mali_frame_builder_readback( surface->frame_builder, m_surf_preserve, usage, offset_x, offset_y, mwidth, mheight ) )
			{
				retval = EGL_FALSE;
			}
		}
		else
		{
			_mali_shared_mem_ref_usage_deref( m_surf_preserve->mem_ref );
			retval = EGL_FALSE;
		}
	}
	else
	{
		retval = EGL_FALSE;
	}

	/* if resize occured then this should delete this surface */
	_mali_surface_deref( m_surf_preserve );
	_mali_sys_atomic_set(&surface->do_readback, 0);
	return retval;
}


MALI_STATIC u32 find_spare_wb_unit( mali_frame_builder *fb )
{
	/* Try to find a spare WB unit if possible */
	u32 dummy_usage;
	u32 wbc_wbu = MALI200_WRITEBACK_UNIT_COUNT;

	MALI_DEBUG_ASSERT ( 3 == MALI200_WRITEBACK_UNIT_COUNT, ("Who changed Mali WB unit count?") );

	do
	{
		wbc_wbu--;
		if ( NULL == _mali_frame_builder_get_output( fb, wbc_wbu, &dummy_usage ) ) return wbc_wbu;
	}
	while (wbc_wbu);

	return MALI_DEFAULT_COLOR_WBIDX;
}


EGLBoolean __egl_mali_post_to_window_surface( egl_surface *surface,
                                              __egl_thread_state *tstate,
                                              __egl_thread_state_api *tstate_api )
{
	u32 wbc_wbu = MALI_DEFAULT_COLOR_WBIDX;
	EGLBoolean retval   = EGL_TRUE;
	u32 width, height;
	u32 usage;
	mali_surface* m_surf_preserve = NULL;
	mali_err_code err;
	u16	old_width;
	u16	old_height;
	EGLBoolean surface_resized = EGL_FALSE;

	MALI_DEBUG_CODE( u32 debug_usage = 0; mali_surface *surface_before; )

	MALI_DEBUG_ASSERT_POINTER( tstate );
	MALI_DEBUG_ASSERT_POINTER( tstate_api );
	MALI_DEBUG_ASSERT_POINTER( surface );

	/* For buffer preserved now it's time to allocate an internal target for flushing into 
	 * as we must guarantee safe access to previous buffer. */
	if ( EGL_BUFFER_PRESERVED == surface->swap_behavior )
	{
		if ( NULL == surface->internal_target )
		{
			surface->internal_target = _mali_surface_alloc( MALI_SURFACE_FLAGS_NONE, &surface->buffer[surface->current_buffer].render_target->format, 0, tstate->main_ctx->base_ctx );
			if ( NULL == surface->internal_target ) 
			{
				return EGL_FALSE;
			}
		}

		/* Always use the internal target as readback source and keep a reference until a read back job
		 * has been added */
		m_surf_preserve = surface->internal_target;
		_mali_surface_addref( m_surf_preserve );
		_mali_sys_atomic_set(&surface->do_readback, 1);	
	}

	/* If we have an internal target, make sure that we have the correct color format and the correct pixel data ready for readback in it */
	if ( surface->internal_target )
	{

		/* grab the usage flags of the default output (internal_surface) */
		_mali_frame_builder_get_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, &usage );

		if ( EGL_BUFFER_PRESERVED == surface->swap_behavior )
		{
			/* Now check whether we can just use a 2nd WB unit to do the job in order to avoid the flush call but
			   still emit data into the internal surface */
			wbc_wbu = find_spare_wb_unit( surface->frame_builder );
			if ( surface->internal_target->format.width  == surface->buffer[surface->current_buffer].render_target->format.width &&
				 surface->internal_target->format.height == surface->buffer[surface->current_buffer].render_target->format.height &&
				 MALI_DEFAULT_COLOR_WBIDX != wbc_wbu )
			{
				_mali_frame_builder_set_output( surface->frame_builder, wbc_wbu, surface->internal_target, usage);
			}
			else
			{
				/* Unavoidable isolated flush into the internal target is needed */
				err = _mali_frame_builder_flush( surface->frame_builder );
				if ( MALI_ERR_NO_ERROR != err )
				{
					if ( NULL != m_surf_preserve ) _mali_surface_deref( m_surf_preserve );
					return EGL_FALSE;
				}
			}
		}

		/* then attach the proper output surface */
		_mali_frame_builder_set_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX,
										surface->buffer[surface->current_buffer].render_target, usage);

		/* mark framebuilder dirty, should be implicitly done inside framebuilder so that the
		   next flush will write-out again */
		err = _mali_frame_builder_write_lock( surface->frame_builder, MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_ALL_CHANNELS );
		MALI_IGNORE( err );
		_mali_frame_builder_write_unlock( surface->frame_builder );
	}

	MALI_DEBUG_CODE( surface_before = _mali_frame_builder_get_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, &debug_usage ); )

	/* this callback might overwrite the output of WB unit MALI_DEFAULT_COLOR_WBIDX - be aware */
	_mali_frame_builder_acquire_output( surface->frame_builder );

#if HAVE_ANDROID_OS
	if ( __egl_platform_surface_buffer_invalid( surface ) )
	{
		MALI_DEBUG_PRINT( 2, ( "%s: posting to window surface failed; couldn't acquire output buffer (surface=%p)", 
		                       MALI_FUNCTION, surface ) );
		return EGL_FALSE;
	}
#endif

	/* grab the old surface size */  
	old_width = surface->width;
	old_height = surface->height;

	retval = __egl_mali_post_color_buffer( surface, EGL_TRUE, tstate, tstate_api);
	/* now the display consumer is the next one operating on that surface */

#if EGL_FEATURE_SURFACE_SCALING_ENABLE
	if( EGL_TRUE == surface->scaling_enabled )
	{   // ensure surface extent & border color are updated for new frame
		__egl_surface_scaling_activate( surface, surface->surface_scaling_info );
	}
#endif

	if ( EGL_TRUE == surface->immediate_mode ) _egl_surface_wait_for_jobs( surface );

	/* Set the internal target current again */
	if ( surface->internal_target )
	{
		_mali_frame_builder_set_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, surface->internal_target, usage);
		/* and remove potential leftovers from the writeback conversion WBU in case we used it */
		if ( MALI_DEFAULT_COLOR_WBIDX != wbc_wbu )
		{
			_mali_frame_builder_set_output( surface->frame_builder, wbc_wbu, NULL, 0);
		}
	}

#if EGL_FEATURE_SURFACE_SCALING_ENABLE
	/* Only check for resize when not using surface scaling */
	if( EGL_FALSE == surface->scaling_enabled )
#endif
	/* 
	 * surface->win is the size of the window requested by the window manager,
	 * while surface->width/height is the size used for the surface within EGL
	 */

	if ( EGL_TRUE == __egl_platform_get_window_size( surface->win, surface->platform, &width, &height ) )
	{
		/* set surface resized flag */
		if ( (width != surface->width) || (height != surface->height) || EGL_TRUE == surface->force_resize ) surface_resized = EGL_TRUE;
	}

	if ( EGL_TRUE == surface_resized )
	{
		/* no need to resize if surface width and height are equal to given width and height */
		MALI_API_TRACE_EGLNATIVEWINDOWTYPE_RESIZED(surface->win);
#if (MALI_ANDROID_API < 13) && !defined(EGL_BACKEND_X11)
		if ( EGL_FALSE == __egl_mali_resize_surface( surface, width, height, tstate ) )
		{
			retval = EGL_FALSE;
		}
		else
		{
			surface->force_resize = EGL_FALSE;
		}
#endif
	}

	MALI_DEBUG_CODE( if ( surface->num_buffers > 1 && surface_before == _mali_frame_builder_get_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, &debug_usage ) )
					 {
						 MALI_DEBUG_PRINT( 3, ("__egl_mali_post_color_buffer did not setup a different drawing surface as expected\n") );
					 } );

	if ( EGL_FALSE == __egl_mali_begin_new_frame( surface, EGL_TRUE, tstate ) ) retval = EGL_FALSE;

#if MALI_ANDROID_API < 13
	if ( EGL_BUFFER_PRESERVED == surface->swap_behavior )
	{
		MALI_DEBUG_ASSERT( m_surf_preserve, ( "There must always be a buffer to preserve with EGL_BUFFER_PRESERVED" ) );

		if ( retval == EGL_TRUE ) 
		{
			retval = __egl_mali_readback_surface( surface, m_surf_preserve, usage, old_width, old_height );
		}
		else
		{
			_mali_surface_deref( m_surf_preserve );
		}
	}
#endif

#if MALI_INSTRUMENTED
	{
		/* Create a new frame, and return the old. We use and own the old, which will be released in _egl_mali_frame_complete_callback */
		mali_instrumented_frame *frame;
		MALI_CHECK(MALI_ERR_NO_ERROR == _instrumented_begin_new_frame(&frame), EGL_FALSE);
		if (NULL != frame)
		{
			_mali_profiling_analyze(frame); /* move SW counters from TLS to this frame */

#if MALI_INSTRUMENTED_PLUGIN_ENABLED
			if (MALI_INSTRUMENTED_FEATURE_IS_ENABLED(CINSTR_CLIENTAPI_COMMON, CINSTR_FEATURE_FRAME_COMPLETE))
			{
				_mali_instrumented_plugin_send_event_render_pass_complete(CINSTR_EVENT_RENDER_PASS_COMPLETE_CPU, 0, frame->frame_no);
				_mali_instrumented_plugin_send_event_frame_complete(CINSTR_EVENT_FRAME_COMPLETE_CPU, frame->frame_no);
			}
#endif

			_instrumented_release_frame(frame);
		}
	}
#endif

#if MALI_TIMELINE_PROFILING_ENABLED
	_mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_SINGLE|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_SINGLE_SW_EGL_NEW_FRAME, 0, 0, 0, 0, 0);
#endif

#if MALI_OPROFILE_SAMPLING
	mali_oprofile_sample();
#endif

	return retval;
}

EGLBoolean __egl_mali_render_surface_to_pixmap( egl_surface *surface, EGLNativePixmapType target, EGLBoolean remap_pixmap, __egl_thread_state *tstate, __egl_thread_state_api *tstate_api )
{
	mali_image   *pixmap_image   = NULL;
	mali_surface *pixmap_surface = NULL;
	mali_surface *output = NULL;
	mali_err_code retval = MALI_ERR_NO_ERROR;
	u32 available_wbx_id = MALI_DEFAULT_COLOR_WBIDX;
	mali_bool support_ump;
	u32 i, dummy_usage = 0, usage = 0;

	output = _mali_frame_builder_get_output( surface->frame_builder , MALI_DEFAULT_COLOR_WBIDX, &usage );
	support_ump = __egl_platform_pixmap_support_ump( target );

	if ( support_ump && EGL_TRUE == remap_pixmap )
	{
		/* use the platform implementation of mapping the pixmap into a mali_image */
		pixmap_image =  __egl_platform_map_pixmap( surface->dpy->native_dpy, NULL, target );
		MALI_CHECK_GOTO( pixmap_image != NULL, failure );

		pixmap_surface = pixmap_image->pixel_buffer[0][0];
		MALI_DEBUG_ASSERT_POINTER( pixmap_surface );

		/* try to find if we have an available output buffer */
		for ( i = 0; i < MALI200_WRITEBACK_UNIT_COUNT; i++ )
		{
			if ( NULL == _mali_frame_builder_get_output( surface->frame_builder, i, &dummy_usage ) )
			{
				available_wbx_id = i;
				break;
			}
		}

		/* we found an available buffer, so set the pixmap as concurrent rendering target */
		if ( available_wbx_id != MALI_DEFAULT_COLOR_WBIDX )
		{
			_mali_frame_builder_set_output( surface->frame_builder, available_wbx_id, pixmap_surface, usage);
		}

#if EGL_SURFACE_LOCKING_ENABLED
		pixmap_surface->flags |= MALI_SURFACE_FLAG_TRACK_SURFACE;
		_mali_surface_set_event_callback( pixmap_surface, 
		                                  MALI_SURFACE_EVENT_GPU_WRITE, 
		                                  MALI_REINTERPRET_CAST(mali_surface_eventfunc)_egl_surface_gpu_write_callback, 
		                                  NULL );
		_mali_surface_set_event_callback( pixmap_surface, 
		                                  MALI_SURFACE_EVENT_GPU_WRITE_DONE, 
		                                  MALI_REINTERPRET_CAST(mali_surface_eventfunc)_egl_surface_gpu_write_done_callback, 
		                                  NULL );

		if ( MALI_ERR_NO_ERROR != _mali_frame_builder_write_lock( surface->frame_builder, MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_ALL_CHANNELS ) ) goto failure;
		
		_mali_frame_builder_write_unlock( surface->frame_builder );
#endif
	}

	/* TODO: take special care when flushing a window surface with writeback conversion into a pixmap */

	/* flush it and wait */
	retval = _mali_frame_builder_flush( surface->frame_builder );
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == retval, failure );

	_mali_frame_builder_wait( surface->frame_builder );

	/* 
	 * Do a CPU copy into the pixmap if
	 * 1. we do not have direct access to the pixmap memory or 
	 * 2. even we support UMP, but we failed to find an available output buffer
	 *
	 * If we picked up one output buffer before, we should unattach it now
	 */
	if ( EGL_TRUE == support_ump )
	{
		/* for the case when we are not remapping the pixmap, we have already flushed into the default writeback unit */
		if ( EGL_TRUE == remap_pixmap )
		{
			if ( available_wbx_id == MALI_DEFAULT_COLOR_WBIDX )
			{
				__egl_platform_copy_buffers( tstate->main_ctx->base_ctx, tstate_api->display->native_dpy, surface, output, target );
			}
			else
			{
				_mali_frame_builder_set_output( surface->frame_builder, available_wbx_id, NULL, usage);
			}
		}
	}
	else
	{
		__egl_platform_copy_buffers( tstate->main_ctx->base_ctx, tstate_api->display->native_dpy, surface, output, target );
	}

	if ( NULL != pixmap_image ) 
	{
		mali_image_deref_surfaces( pixmap_image );
		mali_image_deref( pixmap_image );
	}

	return EGL_TRUE;

failure:
	if ( NULL != pixmap_image ) 
	{
		mali_image_deref_surfaces( pixmap_image );
		mali_image_deref( pixmap_image );
	}

	if ( EGL_TRUE == support_ump && EGL_TRUE == remap_pixmap )
	{
		_mali_frame_builder_set_output( surface->frame_builder, available_wbx_id, NULL, usage);
		_mali_frame_builder_set_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, output, usage);
	}
	return EGL_FALSE;
}

EGLBoolean __egl_mali_render_pixmap_to_surface( egl_surface *surface )
{
	mali_image   *pixmap_image   = NULL;
	mali_surface *pixmap_surface = NULL;
	mali_err_code retval = MALI_ERR_NO_ERROR;

	/* Wrap the native pixmap into a mali_surface, while taking care of
	 * using the fast-path in case the pixmap is allocated through UMP.
	 * This should all be handled in the platform specific parts of EGL.
	 */
	pixmap_image =  __egl_platform_map_pixmap( surface->dpy->native_dpy, NULL, surface->pixmap );
	MALI_CHECK_GOTO( pixmap_image != NULL, failure );

	/* This will only work with RGB pixmaps, so take the RGB plane as the readback surface */
	pixmap_surface = pixmap_image->pixel_buffer[0][0];
	MALI_DEBUG_ASSERT_POINTER( pixmap_surface );

	surface->pixmap_image = pixmap_image;

	retval = _mali_frame_builder_use( surface->frame_builder );
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == retval, failure );

	retval = _mali_frame_builder_readback( surface->frame_builder, pixmap_surface, MALI_OUTPUT_COLOR, 0, 0, _mali_frame_builder_get_width( surface->frame_builder ), _mali_frame_builder_get_height( surface->frame_builder ) );
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == retval, failure );

	retval = _mali_frame_builder_flush( surface->frame_builder );
	MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == retval, failure );

	_mali_frame_builder_wait( surface->frame_builder );

	return EGL_TRUE;

failure:
	return EGL_FALSE;
}

#ifdef EGL_MALI_VG
EGLBoolean __egl_mali_vg_post_to_pbuffer_surface( egl_surface *surface,
                                                  EGLBoolean client_buffer,
                                                  __egl_thread_state *tstate,
                                                  __egl_thread_state_api *tstate_api )
{
	EGLBoolean             retval = EGL_TRUE;
	mali_surface *color_attachment = NULL;
	MALI_IGNORE( tstate );
	MALI_IGNORE( tstate_api );

	color_attachment = _mali_frame_builder_get_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, NULL);

	retval = __egl_vg_post_to_pbuffer_surface( surface, client_buffer, color_attachment, tstate_api );

	return retval;
}
#endif /* EGL_MALI_VG */

#ifdef EGL_MALI_GLES
EGLBoolean __egl_mali_gles_post_to_pbuffer_surface( egl_surface *surface,
                                                    __egl_thread_state *tstate,
                                                    __egl_thread_state_api *tstate_api )
{
	mali_err_code retval;
	MALI_IGNORE( tstate );
	MALI_IGNORE( tstate_api );
	MALI_DEBUG_ASSERT_POINTER( surface );
	
	/*Do increamental rendering on PBuffer*/
	retval = _mali_incremental_render( surface->frame_builder );
	
	return EGL_TRUE;
}
#endif /* EGL_MALI_GLES */

EGLBoolean __egl_mali_post_to_pbuffer_surface( egl_surface *surface,
                                               __egl_thread_state *tstate,
                                               __egl_thread_state_api *tstate_api )
{
	MALI_DEBUG_ASSERT_POINTER( tstate );
	MALI_DEBUG_ASSERT_POINTER( tstate_api );
	MALI_DEBUG_ASSERT_POINTER( surface );

	if ( EGL_OPENVG_API == tstate_api->context->api )
	{
#ifdef EGL_MALI_VG
		if ( (EGLClientBuffer)NULL == surface->client_buffer ) return EGL_TRUE;
		MALI_CHECK( EGL_TRUE == __egl_mali_post_color_buffer( surface, EGL_FALSE, tstate, tstate_api ), EGL_FALSE );
		MALI_CHECK( EGL_TRUE == __egl_mali_vg_post_to_pbuffer_surface( surface, EGL_TRUE, tstate, tstate_api ), EGL_FALSE );
#endif /* EGL_MALI_VG */
	}
	else if( EGL_OPENGL_ES_API == tstate_api->context->api )
	{
#ifdef EGL_MALI_GLES
		if ( (EGLClientBuffer)NULL == surface->client_buffer ) return EGL_TRUE;
		MALI_CHECK( EGL_TRUE == __egl_mali_gles_post_to_pbuffer_surface( surface, tstate, tstate_api ), EGL_FALSE );
#endif
	}
	return EGL_TRUE;
}

