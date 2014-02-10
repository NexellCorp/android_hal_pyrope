/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include "gles2_context.h"
#include "gles_context.h"
#include "gles_share_lists.h"
#include <base/mali_debug.h>
#include <util/mali_mem_ref.h>
#include "gles_state.h"
#include "gles2_state/gles2_state.h"
#include "gles_common_state/gles_tex_state.h"
#include "gles_util.h"
#include "gles_gb_interface.h"
#include "gles_fb_interface.h"
#include "gles_texture_object.h"
#include "gles_renderbuffer_object.h"
#include "gles_framebuffer_object.h"
#include "gles2_vtable.h"
#include "gles_config_extension.h"
#include "gles_convert.h"
#include <shared/m200_gp_frame_builder.h>
#include "egl/api_interface_egl.h"

/* allocate a gles-context and initialize it... */
MALI_INTER_MODULE_API struct gles_context *_gles2_create_context( mali_base_ctx_handle base_ctx, 
								  struct gles_context *share_ctx, 
								  egl_api_shared_function_ptrs *egl_funcptrs,
								  int robust_access )
{
	int i;
	mali_err_code merr = MALI_ERR_NO_ERROR;
	gles_context *ctx = NULL;

	MALI_DEBUG_ASSERT( base_ctx != NULL, ("No valid base ctx handle given") );

	ctx = (gles_context *)_mali_sys_calloc(1,sizeof(gles_context));
	if (NULL == ctx) return NULL;

#if EXTENSION_ROBUSTNESS_EXT_ENABLE
	switch( robust_access )
	{
		case 0:
			ctx->robust_strategy = 0;
			break;
		case 1:
			ctx->robust_strategy = GL_NO_RESET_NOTIFICATION_EXT;
			break;
		case 2:
			ctx->robust_strategy = GL_LOSE_CONTEXT_ON_RESET_EXT;
			break;
		default:
			ctx->robust_strategy = 0;
			break;
	}
#endif
	/* attach the EGLImage function pointer */
	ctx->egl_funcptrs = egl_funcptrs;

	/* create GL_RENDERER string */
	_gles_create_renderer_string( (char *)ctx->renderer, sizeof(ctx->renderer) );

	/* attach to the base context */
	ctx->base_ctx = base_ctx;

	ctx->api_version = GLES_API_VERSION_2;
	ctx->vtable      = _gles2_get_vtable();
	
	/* allocate working buffer for dirty bits in a list of u16 */
	ctx->bit_buffer = (u32 *)_mali_sys_calloc(2048,sizeof(u32));
	if (NULL == ctx->bit_buffer)
	{
		_gles2_delete_context(ctx);
		return NULL;
	}

	/* allocate default-textures */
	for (i = 0; i < GLES_TEXTURE_TARGET_COUNT; ++i)
	{
		gles_texture_object *tex_obj = _gles_texture_object_new( (enum gles_texture_target)i, ctx->base_ctx );
		if (NULL == tex_obj)
		{
			_gles2_delete_context(ctx);
			return NULL;
		}

		ctx->default_texture_object[i] = tex_obj;
	}

	/* initialize rsw push structures */
	ctx->rsw_raster = _mali_sys_malloc( sizeof( mali_rsw_raster ) );

	if( NULL == ctx->rsw_raster )
	{
		_gles2_delete_context(ctx);
		return NULL;
	}

	_gles_fb_init( ctx );

	/* create a default FBO */
	ctx->default_framebuffer_object = _gles_framebuffer_object_new(ctx, ctx->base_ctx, MALI_FALSE);
	if( NULL == ctx->default_framebuffer_object )
	{
		_gles2_delete_context(ctx);
		return NULL;
	}

	/* create the GLES state */
	ctx->state.api.gles2 = (struct gles2_state *)_mali_sys_calloc(1, sizeof(struct gles2_state));
	if (NULL == ctx->state.api.gles2)
	{
		_gles2_delete_context(ctx);
		return NULL;
	}

	/* initialize sub-states */
	_gles2_state_init( ctx );
	if (NULL != share_ctx)
	{
		_gles_share_lists_addref(share_ctx->share_lists, ctx->api_version);
		ctx->share_lists = share_ctx->share_lists;
	}
	else
	{
		ctx->share_lists = _gles_share_lists_alloc(ctx->api_version);
		if (NULL == ctx->share_lists)
		{
			_gles2_delete_context(ctx);
			return NULL;
		}
	}

	/* make the geometry backend do its init. */
	ctx->gb_ctx = NULL;
	merr = _gles_gb_init( ctx );
	if ( MALI_ERR_NO_ERROR != merr )
	{
		_gles2_delete_context(ctx);
		return NULL;
	}

	ctx->fb_ctx = _gles_fb_alloc(ctx);
	if( ctx->fb_ctx == NULL )
	{
		_gles2_delete_context(ctx);
		return NULL;
	}

	ctx->texture_frame_builder = _mali_frame_builder_alloc(MALI_FRAME_BUILDER_TYPE_GLES_TEXTURE_UPLOAD, ctx->base_ctx, 1, 
								MALI_FRAME_BUILDER_PROPS_ORIGO_LOWER_LEFT | MALI_FRAME_BUILDER_PROPS_NOT_ALLOCATE_HEAP, ctx->egl_funcptrs );
	if ( NULL == ctx->texture_frame_builder )
	{
		_gles2_delete_context( ctx );
		return NULL;
	}

	ctx->frame_pool = NULL;

	return ctx;
}

MALI_INTER_MODULE_API void _gles2_delete_context(gles_context *ctx)
{
	int i;

	if (NULL == ctx) return;

	if (NULL != ctx->share_lists)
	{
		/* deref all bound textures */
		_gles_texture_env_deref_textures(&ctx->state.common.texture_env);

		/* un-bind the vertex buffers */
		_gles_internal_unbind_buffer_objects(ctx);

		if (NULL != ctx->share_lists->framebuffer_object_list)
		{
			mali_err_code mali_err;
			/* bind the default FBO. Or rather, just bind it to NULL. No point in binding
			 * anything real, as even the default FBO will be deleted further below */
			mali_err = _gles_internal_bind_framebuffer(ctx, NULL, 0);
			/* No point in setting error-code, no way of getting it after context is deleted */
			MALI_IGNORE(mali_err);
		}

		if (NULL != ctx->share_lists->renderbuffer_object_list)
		{
			/* Dereferencing by setting back the default-objects */
			_gles_internal_bind_renderbuffer(&ctx->state.common.renderbuffer, NULL, 0);
		}

		if (NULL != ctx->share_lists->program_object_list)
		{
			GLenum err;
			/* free the program object list by un-using it */
			err = _gles2_use_program( &ctx->state, ctx->share_lists->program_object_list, 0 );
			MALI_DEBUG_ASSERT(err == GL_NO_ERROR, ("_gles_bind_buffer failed unexpectedly") );
			MALI_IGNORE(err);
		}

		/* deref shared lists */
		_gles_share_lists_deref(ctx->share_lists, ctx->api_version);
		ctx->share_lists = NULL;

		_gles2_state_deinit( ctx );
	}

	if (NULL != ctx->state.api.gles2) _mali_sys_free(ctx->state.api.gles2);

	if( NULL != ctx->rsw_raster ) _mali_sys_free( ctx->rsw_raster );

	/* delete default textures */
	for (i = 0; i < GLES_TEXTURE_TARGET_COUNT; ++i)
	{
		/* if we got as far as allocating the default_texture_object */
		if ( NULL != ctx->default_texture_object[i] )
		{
			_gles_texture_object_delete( ctx->default_texture_object[i]);
			ctx->default_texture_object[i] = NULL;
		}
	}

	/* This will dereference the default FBO once, disconnecting it from the context.
	 * Its framebuilder will still exist, and may very well be unflushed */
	{
		mali_err_code err = _gles_internal_bind_framebuffer( ctx, NULL, 0 );
		MALI_IGNORE(err);
	}
	/* delete the default FBO */
	if (NULL != ctx->default_framebuffer_object)
	{
		_gles_framebuffer_object_deref(ctx->default_framebuffer_object);
		ctx->default_framebuffer_object = NULL;
	}

	if ( NULL != ctx->texture_frame_builder )
	{
		_mali_frame_builder_free( ctx->texture_frame_builder );
		ctx->texture_frame_builder = NULL;
	}
	/* free backend data */
	_gles_gb_free( ctx );
	
	/* free working buffer */
	if (NULL != ctx->bit_buffer) _mali_sys_free(ctx->bit_buffer);
	ctx->bit_buffer = NULL;

	if (NULL != ctx->fb_ctx) _gles_fb_free( ctx->fb_ctx );
	ctx->fb_ctx = NULL;

	_mali_sys_free(ctx);
}

