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
#include "gles1_context.h"
#include "gles_context.h"
#include "gles1_state/gles1_state.h"
#include "gles_share_lists.h"

#include <base/mali_debug.h>
#include <util/mali_mem_ref.h>
#include "gles_state.h"
#include "gles_util.h"
#include "gles_gb_interface.h"
#include "gles_fb_interface.h"
#include "gles_texture_object.h"
#include "gles_renderbuffer_object.h"
#include "gles_framebuffer_object.h"
#include <opengles/gles_sg_interface.h>

#include "gles1_vtable.h"

#ifdef USING_SHADER_GENERATOR
	#include "gles_sg_interface.h"
#endif

#include "gles_config_extension.h"
#include <shared/m200_gp_frame_builder.h>
#include "egl/api_interface_egl.h"

#ifdef NEXELL_LEAPFROG_FEATURE_FBO_SCALE_EN
#include "gles1_texture_object.h"
#include "gles_object.h"

void nx_gles_scaler_framebuffer_object_new( gles_context *ctx )
{
	GLuint framebuffer = 1, texture = 0;
	GLenum format, type, err = 0;
	GLint param = GL_LINEAR;
	if (ctx == NULL) return;

	/* texture */
	if(2 == ctx->sacle_surf_pixel_byte_size)
	{
		format = GL_RGB;
		type = GL_UNSIGNED_SHORT_5_6_5;
	}
	else 
	{
		format = GL_RGBA;
		type = GL_UNSIGNED_BYTE;	
	}		

	err = _gles_bind_texture( ctx, GL_TEXTURE_2D, texture);
	if(GL_NO_ERROR != err)
	{
		MALI_DEBUG_ERROR(("_gles_bind_texture failed(0x%x)", err));
		return;
	}
	err = _gles1_tex_image_2d( ctx, GL_TEXTURE_2D, 0, (GLint)format,
	                                    320, 240, 0, format, type, NULL, ctx->state.common.pixel.unpack_alignment );
	if(GL_NO_ERROR != err)
	{
		MALI_DEBUG_ERROR(("_gles1_tex_image_2d failed(0x%x)", err));
		return;
	}
	err = _gles1_tex_parameter( &ctx->state.common.texture_env, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &param, GLES_INT );
	if(GL_NO_ERROR != err)
	{
		MALI_DEBUG_ERROR(("_gles1_tex_parameter failed(0x%x)", err));
		return;
	}
	err = _gles1_tex_parameter( &ctx->state.common.texture_env, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &param, GLES_INT );
	if(GL_NO_ERROR != err)
	{
		MALI_DEBUG_ERROR(("_gles1_tex_parameter failed(0x%x)", err));
		return;
	}
	param = GL_CLAMP_TO_EDGE;
	err = _gles1_tex_parameter( &ctx->state.common.texture_env, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &param, GLES_INT );
	if(GL_NO_ERROR != err)
	{
		MALI_DEBUG_ERROR(("_gles1_tex_parameter failed(0x%x)", err));
		return;
	}
	err = _gles1_tex_parameter( &ctx->state.common.texture_env, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &param, GLES_INT );
	if(GL_NO_ERROR != err)
	{
		MALI_DEBUG_ERROR(("_gles1_tex_parameter failed(0x%x)", err));
		return;
	}
	
	/* framebuffer */
	err = _gles_gen_objects( ctx->share_lists->framebuffer_object_list, 1, &framebuffer, WRAPPER_FRAMEBUFFER);
	if(GL_NO_ERROR != err)
	{
		MALI_DEBUG_ERROR(("_gles_gen_objects failed(0x%x)", err));
		_gles1_delete_textures( ctx, 1, &texture );
		return;
	}
	#if 0
	err = _gles_bind_framebuffer( ctx, ctx->share_lists->framebuffer_object_list, &ctx->state.common.framebuffer, GL_FRAMEBUFFER, framebuffer );
	if(GL_NO_ERROR != err)
	{
		MALI_DEBUG_ERROR(("_gles_bind_framebuffer failed(0x%x)", err));
		_gles1_delete_textures( ctx, 1, &texture );
		_gles_delete_framebuffers( ctx, 1, &framebuffer );
		return;
	}
	err = _gles_framebuffer_texture2d(ctx,
		&ctx->state.common.framebuffer,
		ctx->share_lists->framebuffer_object_list,
		ctx->share_lists->renderbuffer_object_list,
		ctx->share_lists->texture_object_list,
		GL_FRAMEBUFFER,
		GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D,
		texture,
		0);	
	if(GL_NO_ERROR != err)
	{
		MALI_DEBUG_ERROR(("_gles_framebuffer_texture2d failed(0x%x)", err));
		_gles1_delete_textures( ctx, 1, &texture );
		_gles_bind_framebuffer( ctx, ctx->share_lists->framebuffer_object_list, &ctx->state.common.framebuffer, GL_FRAMEBUFFER, 0 );
		_gles_delete_framebuffers( ctx, 1, &framebuffer );
		return;
	}	
	#endif	
}

static void nx_gles_scaler_framebuffer_object_delete( gles_context *ctx )
{
	GLuint framebuffer = 1, texture = 0, err = 0;;
	err = _gles_delete_framebuffers( ctx, 1, &framebuffer );
	if(GL_NO_ERROR != err)
	{
		MALI_DEBUG_ERROR(("_gles_delete_framebuffers failed(0x%x)", err));
		return;
	}	
	err = _gles1_delete_textures(ctx, 1, &texture);
	if(GL_NO_ERROR != err)
	{
		MALI_DEBUG_ERROR(("_gles1_delete_textures failed(0x%x)", err));
		return;
	}	
}
#endif

/* allocate a gles-context and initialize it... */
MALI_INTER_MODULE_API struct gles_context *_gles1_create_context( mali_base_ctx_handle base_ctx,
								  struct gles_context *share_ctx,
								  egl_api_shared_function_ptrs *egl_funcptrs,
								  int robust_access )
{
	int i;
	mali_err_code merr = MALI_ERR_NO_ERROR;
	gles_context *ctx = NULL;

	MALI_DEBUG_ASSERT( base_ctx != NULL, ("No valid base ctx handle given") );

	ctx = (gles_context*)_mali_sys_calloc(1,sizeof(gles_context));
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

	/* initialize rsw push structures */
	ctx->rsw_raster = _mali_sys_malloc( sizeof( mali_rsw_raster ) );

	if( NULL == ctx->rsw_raster )
	{
		_gles1_delete_context(ctx);
		return NULL;
	}
	_gles_fb_init( ctx );

	/* allocate default-textures */
	for (i = 0; i < GLES_TEXTURE_TARGET_COUNT; ++i)
	{
		gles_texture_object *tex_obj = _gles_texture_object_new( (enum gles_texture_target)i, ctx->base_ctx );
		if (NULL == tex_obj)
		{
			_gles1_delete_context(ctx);
			return NULL;
		}

		ctx->default_texture_object[i] = tex_obj;
	}

	/* create a default FBO */
	ctx->default_framebuffer_object = _gles_framebuffer_object_new(ctx, ctx->base_ctx, MALI_FALSE);
	if( NULL == ctx->default_framebuffer_object )
	{
		_gles1_delete_context(ctx);
		return NULL;
	}

	ctx->api_version = GLES_API_VERSION_1;
	ctx->vtable      = _gles1_get_vtable();

	ctx->state.api.gles1 = (struct gles1_state *)_mali_sys_calloc(1, sizeof(struct gles1_state));
	if (NULL == ctx->state.api.gles1)
	{
		_gles1_delete_context(ctx);
		return NULL;
	}

#ifdef USING_SHADER_GENERATOR
	/* initialize shader generator */
	ctx->sg_ctx = _gles_sg_alloc( base_ctx );
	if(NULL == ctx->sg_ctx)
	{
		_gles1_delete_context(ctx);
		return NULL;
	}
#endif

	/* initialize sub-states */
	_gles1_state_init( ctx );
#ifdef USING_SHADER_GENERATOR
	_gles_sg_state_init( ctx->sg_ctx );
#endif

	if (NULL != share_ctx)
	{
		_gles_share_lists_addref(share_ctx->share_lists,  ctx->api_version);
		ctx->share_lists = share_ctx->share_lists;
	}
	else
	{
		ctx->share_lists = _gles_share_lists_alloc( ctx->api_version );
		if (NULL == ctx->share_lists)
		{
			_gles1_delete_context(ctx);
			return NULL;
		}
	}

	/* make the geometry backend do its init. */
	ctx->gb_ctx = NULL;
	merr = _gles_gb_init( ctx );
	if ( MALI_ERR_NO_ERROR != merr )
	{
		_gles1_delete_context(ctx);
		return NULL;
	}

	ctx->fb_ctx = _gles_fb_alloc(ctx);
	if( ctx->fb_ctx == NULL )
	{
		_gles1_delete_context(ctx);
		return NULL;
	}

	ctx->texture_frame_builder = _mali_frame_builder_alloc(MALI_FRAME_BUILDER_TYPE_GLES_TEXTURE_UPLOAD, ctx->base_ctx, 1, 
								MALI_FRAME_BUILDER_PROPS_NOT_ALLOCATE_HEAP | MALI_FRAME_BUILDER_PROPS_ORIGO_LOWER_LEFT, ctx->egl_funcptrs );
	if ( NULL == ctx->texture_frame_builder )
	{
		_gles1_delete_context( ctx );
		return NULL;
	}

	ctx->frame_pool = NULL;

	return ctx;
}

MALI_INTER_MODULE_API void _gles1_delete_context(gles_context *ctx)
{
	int i;

	if (NULL == ctx) return;

#ifdef NEXELL_LEAPFROG_FEATURE_FBO_SCALE_EN
	nx_gles_scaler_framebuffer_object_delete(ctx);
#endif

	if (NULL != ctx->share_lists)
	{
		/* deref all bound textures */
		_gles_texture_env_deref_textures(&ctx->state.common.texture_env);

		/* un-bind the vertex buffers */
		_gles_internal_unbind_buffer_objects(ctx);

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
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
#endif
		/* deref shared lists */
		_gles_share_lists_deref(ctx->share_lists, ctx->api_version);
		ctx->share_lists = NULL;

		_gles1_state_deinit( ctx );
	}

	if (NULL != ctx->state.api.gles1) _mali_sys_free(ctx->state.api.gles1);

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
		mali_err_code err;
		err = _gles_internal_bind_framebuffer( ctx, NULL, 0 );
		MALI_IGNORE(err);
		if (NULL != ctx->default_framebuffer_object)
		{
			_gles_framebuffer_object_deref(ctx->default_framebuffer_object);
			ctx->default_framebuffer_object = NULL;
		}
	}

	/* free backend data */
#ifdef USING_SHADER_GENERATOR
	if ( NULL != ctx->sg_ctx )
	{
		_gles_sg_free( ctx->sg_ctx );
	}
#endif


	if ( NULL != ctx->texture_frame_builder )
	{
		_mali_frame_builder_free( ctx->texture_frame_builder );
		ctx->texture_frame_builder = NULL;
	}

	_gles_gb_free( ctx );

	if (NULL != ctx->fb_ctx) _gles_fb_free( ctx->fb_ctx );
	ctx->fb_ctx = NULL;

	_mali_sys_free(ctx);
	ctx = NULL;
}

