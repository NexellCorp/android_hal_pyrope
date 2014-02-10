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

#include <EGL/eglplatform.h>
#include "egl_gles.h"
#include "egl_handle.h"
#include "egl_misc.h"
#include "mali_instrumented_context.h"
#include "egl_config_extension.h"
#include <shared/egl_image_status.h>
#include "api_interface_gles.h"


mali_err_code __egl_gles_initialize( __egl_main_context *egl )
{
	MALI_DEBUG_ASSERT_POINTER( egl );

	if ( egl->linker->caps & _EGL_LINKER_OPENGL_ES_BIT )
	{
		mali_err_code err = egl->linker->gles_func[0].initialize( &egl->gles_global_data );
		MALI_CHECK_NO_ERROR( err );
	}

	if ( egl->linker->caps & _EGL_LINKER_OPENGL_ES2_BIT )
	{
		mali_err_code err = egl->linker->gles_func[1].initialize( &egl->gles_global_data );
		MALI_CHECK_NO_ERROR( err );
	}

	MALI_SUCCESS;
}

void __egl_gles_shutdown( __egl_main_context *egl )
{
	if ( egl->linker->caps & _EGL_LINKER_OPENGL_ES_BIT )
	{
		egl->linker->gles_func[0].shutdown( &egl->gles_global_data );
	}

	if ( egl->linker->caps & _EGL_LINKER_OPENGL_ES2_BIT )
	{
		egl->linker->gles_func[1].shutdown( &egl->gles_global_data );
	}
}

void __egl_gles_context_resize_finish( egl_context *context, u32 width, u32 height )
{
	__egl_main_context *egl = __egl_get_main_context();
	u32 ver = context->client_version - 1;
	/*if update viewport is needed*/
	if(EGL_FALSE == context->gles_viewport_set)
	{
		egl->linker->gles_func[ver].viewport( context->api_context, 0, 0, width, height );
		egl->linker->gles_func[ver].scissor( context->api_context, 0, 0, width, height );
		context->gles_viewport_set = EGL_TRUE;
	}
	else
	{
#if defined(HAVE_ANDROID_OS) && (MALI_ANDROID_API <= 10)
		/* support case 527125: set MALI_STATE_VS_VIEWPORT_UPDATE_PENDING bit to force a viewport transform matrix recalculation*/
		egl->linker->gles_func[ver].update_viewport_state( context->api_context);
#endif
	}
}

EGLBoolean __egl_gles_resize_surface( egl_context *context, u32 width, u32 height )
{
	__egl_main_context *egl = __egl_get_main_context();
	u32 ver = context->client_version - 1;

	egl->linker->gles_func[ver].viewport( context->api_context, 0, 0, width, height );
	egl->linker->gles_func[ver].scissor( context->api_context, 0, 0, width, height );

	return EGL_TRUE;
}

#if EGL_KHR_image_ENABLE
void __egl_gles_set_egl_image_caps( egl_context *context, EGLint client_version )
{
	context->egl_image_caps = EGL_IMAGE_NONE;
	if ( 1 == client_version)
	{
		if ( EGL_KHR_gl_texture_2D_image_GLES1_ENABLE ) context->egl_image_caps |= EGL_IMAGE_GL_TEXTURE_2D_IMAGE;
		if ( EGL_KHR_gl_texture_cubemap_image_GLES1_ENABLE ) context->egl_image_caps |= EGL_IMAGE_GL_TEXTURE_CUBEMAP_IMAGE;
		if ( EGL_KHR_gl_texture_3D_image_GLES1_ENABLE ) context->egl_image_caps |= EGL_IMAGE_GL_TEXTURE_3D_IMAGE;
		if ( EGL_KHR_gl_renderbuffer_image_GLES1_ENABLE ) context->egl_image_caps |= EGL_IMAGE_GL_RENDERBUFFER_IMAGE;
	}
	else if ( 2 == client_version )
	{
		if ( EGL_KHR_gl_texture_2D_image_GLES2_ENABLE ) context->egl_image_caps |= EGL_IMAGE_GL_TEXTURE_2D_IMAGE;
		if ( EGL_KHR_gl_texture_cubemap_image_GLES2_ENABLE ) context->egl_image_caps |= EGL_IMAGE_GL_TEXTURE_CUBEMAP_IMAGE;
		if ( EGL_KHR_gl_texture_3D_image_GLES2_ENABLE ) context->egl_image_caps |= EGL_IMAGE_GL_TEXTURE_3D_IMAGE;
		if ( EGL_KHR_gl_renderbuffer_image_GLES2_ENABLE ) context->egl_image_caps |= EGL_IMAGE_GL_RENDERBUFFER_IMAGE;
	}
}
#endif /* EGL_KHR_image_ENABLE */

egl_context* __egl_gles_create_context( egl_config *config,
                                        egl_context *share_list,
                                        EGLint client_version,
                                        EGLBoolean robustness_enabled,
                                        EGLint reset_notification_strategy,
                                        __egl_thread_state *tstate )
{
	egl_context *ctx = EGL_NO_CONTEXT;
	__egl_main_context *egl = __egl_get_main_context();
	u32 ver = client_version - 1;
	int robust_access = 0;

	/* make sure config supports the wanted api and client version */
	switch ( client_version )
	{
		case 1:
			if ( !(EGL_OPENGL_ES_BIT & config->renderable_type) || !(egl->linker->caps & _EGL_LINKER_OPENGL_ES_BIT) )
			{
				__egl_set_error( EGL_BAD_CONFIG, tstate );
				return EGL_NO_CONTEXT;
			}
			break;

		case 2:
			if ( !(EGL_OPENGL_ES2_BIT & config->renderable_type) || !(egl->linker->caps & _EGL_LINKER_OPENGL_ES2_BIT) )
			{
				__egl_set_error( EGL_BAD_CONFIG, tstate );
				return EGL_NO_CONTEXT;
			}
			break;

		default:
			__egl_set_error( EGL_BAD_CONFIG, tstate );
			return EGL_NO_CONTEXT;
	}

	if ( EGL_TRUE == robustness_enabled )
	{
		/* the shared context has to have matching robustness and reset notification strategy settings */
		if ( NULL != share_list )
		{
			if ( (EGL_FALSE == share_list->robustness_enabled) || (reset_notification_strategy != share_list->reset_notification_strategy) )
			{
				__egl_set_error( EGL_BAD_MATCH, tstate );
				return EGL_NO_CONTEXT;
			}
		}
	}

	ctx = __egl_allocate_context( config, client_version );
	if ( NULL == ctx )
	{
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return EGL_NO_CONTEXT;
	}

#if EGL_EXT_create_context_robustness_ENABLE
	if ( EGL_TRUE == robustness_enabled )
	{
		if ( EGL_NO_RESET_NOTIFICATION_EXT == reset_notification_strategy ) robust_access = 1;
		else if ( EGL_LOSE_CONTEXT_ON_RESET_EXT == reset_notification_strategy ) robust_access = 2;
		else
		{
			robust_access = 1;
		}
	}
#endif /* EGL_EXT_create_context_robustness_ENABLE */

	if ( EGL_NO_CONTEXT == share_list )
	{
		ctx->api_context = egl->linker->gles_func[ver].create_context( tstate->main_ctx->base_ctx, NULL, &egl_funcptrs, robust_access );
	}
	else
	{
		ctx->api_context = egl->linker->gles_func[ver].create_context( tstate->main_ctx->base_ctx, share_list->api_context, &egl_funcptrs, robust_access );
	}

	if ( NULL == ctx->api_context )
	{
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		__egl_release_context( ctx, ( void * ) tstate );
		return EGL_NO_CONTEXT;
	}
#if EGL_KHR_image_ENABLE
	__egl_gles_set_egl_image_caps( ctx, client_version );
#endif
	ctx->api = EGL_OPENGL_ES_API;

	ctx->robustness_enabled = robustness_enabled;
	ctx->reset_notification_strategy = reset_notification_strategy;

	return ctx;
}

EGLBoolean __egl_gles_release_context( egl_context *ctx, void *thread_state )
{
	__egl_main_context *egl = __egl_get_main_context();
	mali_linked_list_entry *entry;
	__egl_thread_state *tstate = ( __egl_thread_state * ) thread_state;
	u32 ver = 0;

	MALI_CHECK_NON_NULL( ctx->api_context, EGL_TRUE );

	/* Release all bound images from the context */
	entry = __mali_linked_list_get_first_entry( &ctx->bound_images );

	while ( NULL != entry )
	{
		egl_surface *surface;

		surface = entry->data;

		__egl_gles_unbind_tex_image( surface, tstate );

		entry = __mali_linked_list_get_next_entry( entry );
	}

	/* Free the list, no callback as the data in the entries are just pointers and will be free'd elsewhere */
	__mali_linked_list_empty( &ctx->bound_images, NULL );

 	ver = ctx->client_version - 1;
	egl->linker->gles_func[ver].delete_context( ctx->api_context );
	ctx->api_context = NULL;

	return EGL_TRUE;
}

void __egl_gles_make_not_current( __egl_thread_state *tstate )
{
	__egl_main_context *egl = __egl_get_main_context();

	if ( (NULL != tstate->context_gles) && (NULL != tstate->api_gles) )
	{
		u32 ver = tstate->api_gles->context->client_version - 1;
		egl->linker->gles_func[ver].flush( tstate->context_gles, MALI_FALSE );
		egl->linker->gles_func[ver].make_current( NULL );
	}
}

EGLBoolean __egl_gles_make_current( egl_context *ctx, egl_surface *draw, egl_surface *read, __egl_thread_state *tstate )
{
	u32 ver = 0;
	__egl_main_context *egl = __egl_get_main_context();
	mali_err_code err = MALI_ERR_NO_ERROR;
	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( draw );
	MALI_DEBUG_ASSERT_POINTER( read );
	MALI_DEBUG_ASSERT_POINTER( draw->config );
	MALI_DEBUG_ASSERT_POINTER( read->config );

	ver = ctx->client_version - 1;

	if ( EGL_OPENGL_ES_API == tstate->api_current )
	{
		/* gather the surface capabilities on the stack. This will be stack-passed to the set_draw_frame_builder function */
		struct egl_gles_surface_capabilities surface_data;
		surface_data.red_size = draw->config->red_size;
		surface_data.green_size = draw->config->green_size;
		surface_data.blue_size = draw->config->blue_size;
		surface_data.alpha_size = draw->config->alpha_size;
		surface_data.depth_size = draw->config->depth_size;
		surface_data.stencil_size = draw->config->stencil_size;
		surface_data.samples = draw->config->samples;
		surface_data.sample_buffers = draw->config->sample_buffers;
		surface_data.surface_type = draw->type;


		egl->linker->gles_func[ver].make_current( ctx->api_context );
		err = egl->linker->gles_func[ver].set_draw_frame_builder( ctx->api_context, draw->frame_builder, surface_data);
		if ( MALI_ERR_NO_ERROR != err )
		{
			egl->linker->gles_func[ver].make_current( NULL );
			return EGL_FALSE;
		}
		err = egl->linker->gles_func[ver].set_read_frame_builder( ctx->api_context, read->frame_builder, read->type );
		if ( err != MALI_ERR_NO_ERROR )
		{
			egl->linker->gles_func[ver].make_current( NULL );
			return EGL_FALSE;
		}
		if ( EGL_FALSE == ctx->gles_viewport_set )
		{
			if ( EGL_FALSE == __egl_gles_resize_surface( ctx, draw->width, draw->height ) )
			{
				egl->linker->gles_func[ver].make_current( NULL );
				return EGL_FALSE;
			}
			ctx->gles_viewport_set = EGL_TRUE;
		}
	}

	return EGL_TRUE;
}

void __egl_gles_remove_framebuilder_from_client_ctx( __egl_thread_state *tstate )
{
	u32 ver = 0;
	__egl_main_context *egl = __egl_get_main_context();
	MALI_DEBUG_ASSERT_POINTER( tstate );

	if ( NULL == tstate->context_gles ) return;

	ver = tstate->api_gles->context->client_version - 1;
#if 0
	/* Do not flush window surfaces, they will be updated in response to eglSwapBuffers */
	if ( EGL_NO_SURFACE != tstate->api_gles->draw_surface && MALI_EGL_WINDOW_SURFACE != tstate->api_gles->draw_surface->type )
	{
		egl->linker->gles_func[ver].flush( tstate->context_gles, MALI_FALSE );
	}
#endif
	egl->linker->gles_func[ver].make_current( NULL );
}

EGLBoolean __egl_gles_set_framebuilder( egl_surface *surface, __egl_thread_state *tstate )
{
	u32 ver = 0;
	__egl_main_context *egl = __egl_get_main_context();
	mali_err_code err = MALI_ERR_NO_ERROR;

	/* gather the surface capabilities on the stack. This will be stack-passed to the set_draw_frame_builder function */
	struct egl_gles_surface_capabilities surface_data;
	surface_data.red_size = surface->config->red_size;
	surface_data.green_size = surface->config->green_size;
	surface_data.blue_size = surface->config->blue_size;
	surface_data.alpha_size = surface->config->alpha_size;
	surface_data.depth_size = surface->config->depth_size;
	surface_data.stencil_size = surface->config->stencil_size;
	surface_data.samples = surface->config->samples;
	surface_data.sample_buffers = surface->config->sample_buffers;
	surface_data.surface_type = surface->type;

	ver = tstate->api_gles->context->client_version - 1;

	err = egl->linker->gles_func[ver].set_draw_frame_builder( tstate->context_gles, surface->frame_builder, surface_data);
	MALI_CHECK( MALI_ERR_NO_ERROR == err, EGL_FALSE );

	return EGL_TRUE;
}

EGLBoolean __egl_gles_bind_tex_image( egl_surface *surface, __egl_thread_state *tstate )
{
	GLint gl_target = GL_TEXTURE_2D;
	GLint gl_internalformat = GL_RGB;
	GLint gl_error;
	void* gl_tex_obj = NULL;
	__egl_main_context *egl = __egl_get_main_context();
	u32 ver = 0;
	mali_surface *surf = NULL;

	MALI_DEBUG_ASSERT_POINTER( tstate );
	MALI_DEBUG_ASSERT_POINTER( surface );

	ver = tstate->api_gles->context->client_version - 1;

	if ( EGL_TEXTURE_2D == surface->texture_target )
	{
		gl_target = GL_TEXTURE_2D;
	}
	if ( EGL_TEXTURE_RGB == surface->texture_format )
	{
		gl_internalformat = GL_RGB;
	}
	else if ( EGL_TEXTURE_RGBA == surface->texture_format )
	{
		gl_internalformat = GL_RGBA;
	}
	/* Hardware revisions <= r0p4 does not support linear textures with pitch less than 64
	 * See bugzilla #8759 */
#if HARDWARE_ISSUE_4699
	if ( (surface->width * __mali_pixel_format_get_bpp( surface->config->pixel_format )/8) < 64)
	{
		__egl_set_error( EGL_BAD_MATCH, tstate );
		return EGL_FALSE;
	}
#endif

	egl->linker->gles_func[ver].finish( tstate->api_gles->context->api_context );
	gl_error = egl->linker->gles_func[ver].get_error( tstate->api_gles->context->api_context ); /* reset the error state */

	/* get the current backbuffer of the given surface */
	surf = _mali_frame_builder_get_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, NULL );
	if ( NULL == surf ) return EGL_FALSE;

	gl_error = egl->linker->gles_func[ver].bind_tex_image( tstate->api_gles->context->api_context, gl_target, gl_internalformat, surface->mipmap_texture, surf, &gl_tex_obj );

	if ( GL_NO_ERROR != gl_error )
	{
		switch ( gl_error )
		{
			case GL_OUT_OF_MEMORY:
				__egl_set_error( EGL_BAD_ALLOC, tstate );
				break;

			default:
				__egl_set_error( EGL_BAD_PARAMETER, tstate );
				break;
		}
		return EGL_FALSE;
	}
	surface->is_bound = EGL_TRUE;
	surface->bound_context = tstate->api_gles->context;
	surface->bound_texture_obj = gl_tex_obj;
	surface->bound_api_version = ver;

	return EGL_TRUE;
}

void __egl_gles_unbind_tex_image( egl_surface *surface, __egl_thread_state *tstate )
{
	__egl_main_context *egl = tstate->main_ctx;
	u32 ver = surface->bound_api_version;

	if(surface->is_bound != MALI_TRUE) return;

	egl->linker->gles_func[ver].unbind_tex_image( surface->bound_context->api_context, surface->bound_texture_obj );

	surface->is_bound = EGL_FALSE;
	surface->bound_context = NULL;
	surface->bound_texture_obj = NULL;
	surface->bound_api_version = 0;


}

void __egl_gles_flush( __egl_thread_state *tstate, mali_bool flush_all )
{
	u32 ver = 0;
	__egl_main_context *egl = __egl_get_main_context();

	MALI_DEBUG_ASSERT_POINTER( tstate );
	MALI_DEBUG_ASSERT_POINTER( tstate->context_gles );

	ver = tstate->api_gles->context->client_version - 1;

	egl->linker->gles_func[ver].flush( tstate->context_gles, flush_all );
}

void __egl_gles_finish( __egl_thread_state *tstate )
{
	u32 ver = 0;
	__egl_main_context *egl = __egl_get_main_context();

	MALI_DEBUG_ASSERT_POINTER( tstate );
	MALI_DEBUG_ASSERT_POINTER( tstate->context_gles );

	ver = tstate->api_gles->context->client_version - 1;

	egl->linker->gles_func[ver].finish( tstate->context_gles );
}

EGLenum __egl_gles_fence_flush( __egl_thread_state *tstate )
{
	mali_frame_builder     *frame_builder;
	u32                     ver;
	__egl_main_context     *egl = __egl_get_main_context();
	EGLenum                 result = EGL_SUCCESS;

	MALI_DEBUG_ASSERT_POINTER( tstate );
	MALI_DEBUG_ASSERT_POINTER( tstate->context_gles );

	ver = tstate->api_gles->context->client_version - 1;

	if ( GL_NO_ERROR != egl->linker->gles_func[ver].fence_flush( tstate->context_gles ) )
	{
		result = EGL_BAD_ALLOC;
	}
	return result;
}

#if EGL_KHR_image_ENABLE
EGLint __egl_gles_setup_egl_image( egl_context *context, EGLenum target, EGLClientBuffer buffer, u32 level, struct egl_image *image )
{
	u32 ver = 0;
	EGLint err = GLES_SURFACE_TO_EGL_IMAGE_STATUS_NO_ERROR;
	enum egl_image_gles_target gles_target = GLES_TARGET_UNKNOWN;
	u32 gles_buffer;
	__egl_main_context *egl = __egl_get_main_context();

	MALI_DEBUG_ASSERT_POINTER( context );
	MALI_DEBUG_ASSERT_POINTER( image );

	ver = context->client_version - 1;

	switch ( target )
	{
		case EGL_GL_TEXTURE_2D_KHR:
			gles_target = GLES_TARGET_TEXTURE_2D;
			break;

		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR:
			gles_target = GLES_TARGET_TEXTURE_CUBE_MAP_POSITIVE_X;
			break;
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR:
			gles_target = GLES_TARGET_TEXTURE_CUBE_MAP_NEGATIVE_X;
			break;
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR:
			gles_target = GLES_TARGET_TEXTURE_CUBE_MAP_POSITIVE_Y;
			break;
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR:
			gles_target = GLES_TARGET_TEXTURE_CUBE_MAP_NEGATIVE_Y;
			break;
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR:
			gles_target = GLES_TARGET_TEXTURE_CUBE_MAP_POSITIVE_Z;
			break;
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR:
			gles_target = GLES_TARGET_TEXTURE_CUBE_MAP_NEGATIVE_Z;
			break;

		case EGL_GL_RENDERBUFFER_KHR:
			gles_target = GLES_TARGET_RENDERBUFFER;
			break;

		default:
			MALI_DEBUG_ASSERT( 0, ("illegal target format\n") );
	}

	gles_buffer = (GLuint)buffer;

	if ( EGL_GL_RENDERBUFFER_KHR == target )
	{
		if ( 1 == context->client_version )
		{
			MALI_DEBUG_ASSERT( 0, ("EGLImage renderbuffer extension not supported by GLES1\n") );
		}
		else if ( 2 == context->client_version )
		{
			err = egl->linker->gles_func[ver].setup_egl_image_from_renderbuffer( context->api_context, gles_buffer, image );
		}
	}
	else
	{
		/* 2D textures and cubemaps */
		err = egl->linker->gles_func[ver].setup_egl_image_from_texture( context->api_context, gles_target, gles_buffer, level, image );
	}

	/* OpenGL ES has limited concept of colorspaces and alpha format. Defaulting to premultiplied and sRGB for use in VG sharing */
	image->prop->alpha_format = EGL_VG_ALPHA_FORMAT_PRE;
	image->prop->colorspace = EGL_VG_COLORSPACE_sRGB;

	/* convert error code to egl error */
	switch( err )
	{
		case GLES_SURFACE_TO_EGL_IMAGE_STATUS_NO_ERROR:
			break;
		case GLES_SURFACE_TO_EGL_IMAGE_STATUS_INVALID_MIPLVL:
			return EGL_BAD_MATCH;
		case GLES_SURFACE_TO_EGL_IMAGE_STATUS_OBJECT_INCOMPLETE:
		case GLES_SURFACE_TO_EGL_IMAGE_STATUS_OBJECT_UNAVAILABLE:
		case GLES_SURFACE_TO_EGL_IMAGE_STATUS_OBJECT_RESERVED:
			return EGL_BAD_PARAMETER;
		case GLES_SURFACE_TO_EGL_IMAGE_STATUS_ALREADY_SIBLING:
			return EGL_BAD_ACCESS;
		case GLES_SURFACE_TO_EGL_IMAGE_STATUS_OOM:
			return EGL_BAD_ALLOC;
		default:
			MALI_DEBUG_ASSERT( 0, ("unknown error returned by gles\n") );
	}

	return EGL_SUCCESS;
}

/* NOTE:
 * This is an API entrypoint function retrieved through eglGetProcAddress.
 * The reason for doing it this way is because GLES1 and GLES2 both supports
 * the glEGLImageTargetTexture2DOES extension.
 * eglGetProcAddress might not be able to determine at runtime which
 * library to return the proc address for. Placing the actual entrypoint
 * in EGL resolves this issue, since EGL knows which gles context that is
 * current at calltime.
 */
void __egl_gles_image_target_texture_2d( GLenum target, GLeglImageOES image )
{
	__egl_thread_state *tstate = NULL;

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_IMAGE_LOCK );
	if ( NULL != tstate )
	{
		EGLenum api;
		__egl_thread_state_api *tstate_api = __egl_get_current_thread_state_api( tstate, &api );
		if ( EGL_OPENGL_ES_API == api )
		{
			u32 ver = 0;

			ver = tstate_api->context->client_version - 1;
			tstate->main_ctx->linker->gles_func[ver].glEGLImageTargetTexture2DOES( target, image );
		}
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_IMAGE_UNLOCK );
	}
}
#endif /* EGL_KHR_image_ENABLE */

#ifdef EXTENSION_EGL_IMAGE_OES_ENABLE
#ifdef EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
#ifdef EGL_MALI_GLES
/* NOTE:
 * This is an API entrypoint function retrieved through eglGetProcAddress.
 * The reason for doing it this way is because GLES1 and GLES2 both support
 * the glEGLImageTargetRenderbufferStorageOES extension.
 * eglGetProcAddress might not be able to determine at runtime which
 * library to return the proc address for. Placing the actual entrypoint
 * in EGL resolves this issue, since EGL knows which gles context that is
 * current at calltime.
 */
void __egl_gles_image_target_renderbuffer_storage( GLenum target, GLeglImageOES image )
{
	__egl_thread_state *tstate = NULL;

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_IMAGE_LOCK );
	if ( NULL != tstate )
	{
		EGLenum api;
		__egl_thread_state_api *tstate_api = __egl_get_current_thread_state_api( tstate, &api );
		if ( EGL_OPENGL_ES_API == api )
		{
			u32 ver = 0;

			ver = tstate_api->context->client_version - 1;
			tstate->main_ctx->linker->gles_func[ver].glEGLImageTargetRenderbufferStorageOES( target, image );
		}
		__egl_release_current_thread_state( EGL_MAIN_MUTEX_IMAGE_UNLOCK );
	}
}
#endif
#endif
#endif

/* NOTE:
 * This function can return a pointer to a v1.1 or a v2.0 extensions. In the case that both apis are
 * present, it will try to check which is in use via the thread state. If this is not set then it
 * will default to returning the v2.0 extension. There is a possibility that this will be wrong
 * so in general extensions present in both apis should be implemented in egl and checked at runtime
 * as done above for glEGLImageTargetTexture2DOES and glEGLImageTargetRenderbufferStorageOES.
 */
void (* __egl_gles_get_proc_address( __egl_thread_state *tstate, const char *procname ))(void)
{
	__egl_main_context *egl = __egl_get_main_context();
	__eglMustCastToProperFunctionPointerType funcptr = NULL;
	EGLint client_version = 0;
	if ( NULL != tstate )
	{
		EGLenum api;
		__egl_thread_state_api *tstate_api = __egl_get_current_thread_state_api( tstate, &api );
		if ( NULL != tstate_api && NULL != tstate_api->context )
		{
			client_version = tstate_api->context->client_version;
		}
	}

	if ( ( 2 != client_version ) && ( egl->linker->caps & _EGL_LINKER_OPENGL_ES_BIT ) )
	{
		MALI_DEBUG_ASSERT_POINTER( egl->linker->gles_func[0].get_proc_address );
		funcptr = egl->linker->gles_func[0].get_proc_address(procname);
	}
	if ( ( 1 != client_version ) && ( egl->linker->caps & _EGL_LINKER_OPENGL_ES2_BIT ) )
	{
		MALI_DEBUG_ASSERT_POINTER( egl->linker->gles_func[1].get_proc_address );
		funcptr = egl->linker->gles_func[1].get_proc_address(procname);
	}

	return funcptr;
}

