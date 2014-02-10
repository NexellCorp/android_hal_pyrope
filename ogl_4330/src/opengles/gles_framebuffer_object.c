/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_macros.h>
#include <util/mali_math.h>
#include <gles_fb_interface.h>
#include <gles_common_state/gles_framebuffer_state.h>
#include <m200_backend/gles_fb_texture_object.h>

#include "gles_framebuffer_object.h"
#include "gles_base.h"
#include "gles_config.h"
#include "gles_config_extension.h"
#include "gles_util.h"
#include "gles_context.h"
#include "gles_share_lists.h"
#include "gles_renderbuffer_object.h"
#include "gles_texture_object.h"
#include "gles_object.h"
#include "gles_instrumented.h"
#include "gles_convert.h"
#include "gles_fbo_bindings.h"
#include "gles_flush.h"

#include <shared/m200_gp_frame_builder_inlines.h>
#include <shared/m200_incremental_rendering.h>


#if EXTENSION_DISCARD_FRAMEBUFFER
MALI_STATIC_INLINE void _gles_framebuffer_update_discard_flags(gles_framebuffer_object* fb_obj, gles_framebuffer_attachment* ap_obj);
#endif

/**
 * @brief detaches whatever is attached to an attachment point
 * @note Keep in mind that the render attachment is still kept around until the next constraint resolve happens.
 * @param attachment_point The point to detach from. The attachment point will still have a valid render attachment hanging; this will not be modified until we do a completeness resolve.
 * @param fbo the framebuffer object this attachment point is connected to. The FBO will not be flushed, just detached.
 * @return any error which might occur when flushing the surface owner
 */
MALI_CHECK_RESULT MALI_STATIC mali_err_code _gles_framebuffer_internal_detach( gles_framebuffer_attachment* attachment_point, gles_framebuffer_object* fbo )
{
	mali_err_code err = MALI_ERR_NO_ERROR;
	MALI_DEBUG_ASSERT_POINTER( attachment_point );
	MALI_DEBUG_ASSERT_POINTER( fbo );

	/* start detaching */
	switch(attachment_point->attach_type)
	{
		case GLES_ATTACHMENT_TYPE_RENDERBUFFER:
		{
			#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
			gles_renderbuffer_object *rb_obj = attachment_point->ptr.rb_obj;

			MALI_DEBUG_ASSERT_POINTER( rb_obj );

			/* make the attached renderbuffer forget about this FBO */
			_gles_fbo_bindings_remove_binding( rb_obj->fbo_connection, fbo, attachment_point );

			/* dereference the attached renderbuffer once. No longer attached */
			_gles_renderbuffer_object_deref(rb_obj);
			attachment_point->ptr.rb_obj = NULL;
			#else
			MALI_DEBUG_ASSERT(0, ("Can never attach a renderbuffer from FBOs unless we support FBOs"));
			#endif /* EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE */
		} break;
		case GLES_ATTACHMENT_TYPE_TEXTURE:
		{
			#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
			struct gles_mipmap_level *mip = NULL;
			gles_texture_object *tex_obj = attachment_point->ptr.tex_obj;

			MALI_DEBUG_ASSERT_POINTER( tex_obj );

			/* retrieve the bound mipmap level */
			mip = _gles_texture_object_get_mipmap_level(tex_obj, attachment_point->cubeface, attachment_point->miplevel);

			MALI_DEBUG_ASSERT_POINTER( mip ); /* the texture mip level must be present for the attach to succeed */
			MALI_DEBUG_ASSERT_POINTER( mip->fbo_connection ); /* the FBO connection must be present for any attach to succeed */

			/* make the attached texture object forget about this fbo */
			_gles_fbo_bindings_remove_binding( mip->fbo_connection, fbo, attachment_point );

			_gles_texture_object_deref(tex_obj);
			attachment_point->ptr.tex_obj = NULL;
			#else
			MALI_DEBUG_ASSERT(0, ("Can never attach a texture from FBOs unless we support FBOs"));
			#endif /* EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE */
		}
		break;
		case GLES_ATTACHMENT_TYPE_NONE:
			MALI_DEBUG_ASSERT(MALI_TRUE == fbo->implicit_framebuilder, ("Should never detach from FBO #0"));
		break;
		default:
			MALI_DEBUG_ASSERT(0, ("unexpected type"));
	}

	attachment_point->attach_type = GLES_ATTACHMENT_TYPE_NONE;
	attachment_point->name = 0;
	attachment_point->miplevel = 0;
	attachment_point->completeness_dirty = MALI_TRUE;
#if EXTENSION_DISCARD_FRAMEBUFFER
	attachment_point->discarded = MALI_FALSE;
	_gles_framebuffer_update_discard_flags( fbo, attachment_point );
#endif
	fbo->completeness_dirty = MALI_TRUE;

	return err;
}

mali_surface* _gles_get_attachment_surface(gles_framebuffer_attachment *attachment_point)
{
	MALI_DEBUG_ASSERT_POINTER( attachment_point );
	switch( attachment_point->attach_type )
	{
		case GLES_ATTACHMENT_TYPE_TEXTURE:
		{
			struct gles_fb_texture_object *fb_tex_obj = attachment_point->ptr.tex_obj->internal;
			return _gles_fb_texture_object_get_mali_surface( fb_tex_obj, _gles_texture_object_get_mipchain_index(attachment_point->cubeface), attachment_point->miplevel );
		}
		case GLES_ATTACHMENT_TYPE_RENDERBUFFER:
		{
			struct gles_renderbuffer_object *rb_obj = attachment_point->ptr.rb_obj;
			return rb_obj->render_target;
		}
		case GLES_ATTACHMENT_TYPE_NONE:
		{
			return NULL;
		}
		default:
		{
			MALI_DEBUG_ASSERT( 0, ("Not a valid attachment type") );
			return NULL;
		}
	}
}

GLint _gles_fbo_get_bits( gles_framebuffer_object* fb_obj, GLenum pname )
{
	struct mali_surface *surface = NULL;
	struct gles_framebuffer_attachment *attachment_point;
	u32 r_bits, g_bits, b_bits, a_bits, d_bits, s_bits, l_bits, i_bits;

	MALI_DEBUG_ASSERT_POINTER( fb_obj );

	/* FBOs without implicit_framebuilders have the bits stored in the color attachment */
	if(fb_obj->implicit_framebuilder == MALI_FALSE)
	{
		const struct egl_gles_surface_capabilities* surface_capabilities;
		attachment_point = &fb_obj->color_attachment;
		MALI_DEBUG_ASSERT_POINTER( attachment_point );

		/* retrieve the bits from the egl config */
		surface_capabilities = &attachment_point->ptr.surface_capabilities;
		/* If this fails, the default FBO was queried prior to eglMakeCurrent.
		 * There is no surface to query at this stage */

		switch( pname )
		{
			case GL_RED_BITS:       return surface_capabilities->red_size;
			case GL_GREEN_BITS:     return surface_capabilities->green_size;
			case GL_BLUE_BITS:      return surface_capabilities->blue_size;
			case GL_ALPHA_BITS:     return surface_capabilities->alpha_size;
			case GL_DEPTH_BITS:     return surface_capabilities->depth_size;
			case GL_STENCIL_BITS:   return surface_capabilities->stencil_size;
			case GL_SAMPLES:        return surface_capabilities->samples;
			case GL_SAMPLE_BUFFERS: return surface_capabilities->sample_buffers;
			default:
				MALI_DEBUG_ASSERT( 0, ("Incorrect pname, this should be caught earlier") );
				return 0;
		}
	}

	/* all other bits are found by querying the surface formats of the attachments */
	switch( pname )
	{
		case GL_RED_BITS:
		case GL_GREEN_BITS:
		case GL_BLUE_BITS:
		case GL_ALPHA_BITS:
			attachment_point = &fb_obj->color_attachment;
			MALI_DEBUG_ASSERT_POINTER(attachment_point);
			break;
		case GL_DEPTH_BITS:
			attachment_point = &fb_obj->depth_attachment;
			MALI_DEBUG_ASSERT_POINTER(attachment_point);
			break;
		case GL_STENCIL_BITS:
			attachment_point = &fb_obj->stencil_attachment;
			MALI_DEBUG_ASSERT_POINTER(attachment_point);
			break;
		case GL_SAMPLES:
			attachment_point = &fb_obj->color_attachment;
			return attachment_point->fsaa_samples;
		case GL_SAMPLE_BUFFERS:
			attachment_point = &fb_obj->color_attachment;
			return attachment_point->fsaa_samples ? 1 : 0;
		default:
			return 0; /* all other values, just return 0 */
	}

	MALI_DEBUG_ASSERT_POINTER( attachment_point );

	/* get bits from the chosen attachment's current surface */
	surface = _gles_get_attachment_surface(attachment_point);
	if( surface == NULL ) return 0;

	/* retrieve bits from surface pixelformat */
	__m200_texel_format_get_bpc( surface->format.texel_format, &r_bits, &g_bits, &b_bits, &a_bits, &d_bits, &s_bits, &l_bits, &i_bits );

	/* return the proper bits */
	switch( pname )
	{
		case GL_RED_BITS:       return r_bits;
		case GL_GREEN_BITS:     return g_bits;
		case GL_BLUE_BITS:      return b_bits;
		case GL_ALPHA_BITS:     return a_bits;
		case GL_DEPTH_BITS:     return d_bits+l_bits;	/* See bugzilla entry 10968 for details. */
		case GL_STENCIL_BITS:   return s_bits;
		default:
			MALI_DEBUG_ASSERT( 0, ("What happened, switch in switch with different pnames") );
			return 0;
	}
}

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
/**
 * @brief retrieves an attachment point based on an identificator.
 * @param fb_obj a framebuffer object to get an attachment point from
 * @param point_identificator a GL enum identifying the attachment point.
 * @return A pointer to the attachment point struct, or NULL if the required attachment point was not legal.
 * @note The attachment point pointer only is valid as long as the object is valid.
 *       Releasing the object lock, deleting the object or such will make the pointer invalid. Do not free the return pointer.
 * @note Of course, the framebuffer listlock must be set prior to calling this function.
 */
MALI_STATIC gles_framebuffer_attachment* _gles_get_attachment_point(gles_framebuffer_object* fb_obj, GLenum attachment)
{
	MALI_DEBUG_ASSERT_POINTER( fb_obj );

	/* get stencil/depth attachment point */
	switch (attachment)
	{
		case GL_DEPTH_ATTACHMENT: return &fb_obj->depth_attachment;
		case GL_STENCIL_ATTACHMENT: return &fb_obj->stencil_attachment;
	}

	/* Only possible attachment now is color attachment 0, assert it is in legal range */
	MALI_CHECK( attachment == GL_COLOR_ATTACHMENT0 , NULL );

	/* return the color attachment */
	return &fb_obj->color_attachment;
}
#endif /* EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE */

mali_err_code _gles_internal_purge_renderbuffer_from_framebuffer(struct gles_framebuffer_object* fb_object,
                                                        struct gles_renderbuffer_object* rb_object )
{
	mali_err_code err_return = MALI_ERR_NO_ERROR;

	/* silently handle fboID=0 (ie, fb_object=NULL); nothing can ever be detached from FBO 0 . */
	if( NULL == fb_object ) return err_return;

	MALI_DEBUG_ASSERT_POINTER( rb_object );

	/* detach from color attachment points */
	if( fb_object->color_attachment.attach_type == GLES_ATTACHMENT_TYPE_RENDERBUFFER && fb_object->color_attachment.ptr.rb_obj == rb_object)
	{
		/* we found an attached RB_object - just detach it. */
		err_return = _gles_framebuffer_internal_detach( &fb_object->color_attachment, fb_object );
	}

	/* detach from depth attachment point */
	if( fb_object->depth_attachment.attach_type == GLES_ATTACHMENT_TYPE_RENDERBUFFER && fb_object->depth_attachment.ptr.rb_obj == rb_object)
	{
		/* we found an attached RB_object - just detach it. */
		mali_err_code err_detach = _gles_framebuffer_internal_detach( &fb_object->depth_attachment, fb_object );
		if( err_return == MALI_ERR_NO_ERROR ) err_return = err_detach;
	}

	/* finally, detach from stencil attachment point */
	if( fb_object->stencil_attachment.attach_type == GLES_ATTACHMENT_TYPE_RENDERBUFFER && fb_object->stencil_attachment.ptr.rb_obj == rb_object)
	{
		/* we found an attached RB_object - just detach it. */
		mali_err_code err_detach = _gles_framebuffer_internal_detach( &fb_object->stencil_attachment, fb_object );
		if( err_return == MALI_ERR_NO_ERROR ) err_return = err_detach;
	}

	return err_return;
}

mali_err_code _gles_internal_purge_texture_from_framebuffer(struct gles_framebuffer_object* fb_object,
                                                   struct gles_texture_object* tex_object )
{
	mali_err_code err_return = MALI_ERR_NO_ERROR;

	MALI_DEBUG_ASSERT_POINTER( fb_object );
	MALI_DEBUG_ASSERT_POINTER( tex_object );

	/* detach from color attachment points */
	if( fb_object->color_attachment.attach_type == GLES_ATTACHMENT_TYPE_TEXTURE && fb_object->color_attachment.ptr.tex_obj == tex_object )
	{
		/* we found an attached RB_object - just detach it. */
		err_return = _gles_framebuffer_internal_detach( &fb_object->color_attachment, fb_object );
	}

	/* detach from depth attachment point */
	if( fb_object->depth_attachment.attach_type == GLES_ATTACHMENT_TYPE_TEXTURE && fb_object->depth_attachment.ptr.tex_obj == tex_object )
	{
		/* we found an attached RB_object - just detach it. */
		mali_err_code err_detach = _gles_framebuffer_internal_detach( &fb_object->depth_attachment, fb_object );
		if( err_return == MALI_ERR_NO_ERROR ) err_return = err_detach;
	}

	/* finally, detach from stencil attachment point */
	if( fb_object->stencil_attachment.attach_type == GLES_ATTACHMENT_TYPE_TEXTURE && fb_object->stencil_attachment.ptr.tex_obj == tex_object)
	{
		/* we found an attached RB_object - just detach it. */
		mali_err_code err_detach = _gles_framebuffer_internal_detach( &fb_object->stencil_attachment, fb_object );
		if( err_return == MALI_ERR_NO_ERROR ) err_return = err_detach;
	}

	return err_return;
}

mali_err_code _gles_internal_bind_framebuffer( struct gles_context *ctx, gles_framebuffer_object* fb_obj, GLuint fb_obj_id )
{
	mali_err_code err = MALI_ERR_NO_ERROR;
	gles_framebuffer_object* old_fbo;
	struct gles_framebuffer_state* fb_state;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	fb_state = &ctx->state.common.framebuffer;
	old_fbo = fb_state->current_object;

	/* flush out old bound object, unless retain_commands_on_unbind is set */
	if( NULL != old_fbo && NULL != old_fbo->draw_frame_builder && MALI_FALSE == old_fbo->retain_commands_on_unbind )
	{
		/* glDiscardFramebufferEXT: no need to restore discarded attachments
		 * here, since we're binding different FBO here anyway */

		err = _mali_frame_builder_flush( old_fbo->draw_frame_builder );

		if ( MALI_ERR_NO_ERROR != err )
		{
			/* We have run out of memory while flushing the frame_builder, and are resetting the frame_builder to free up some memory */
			_mali_frame_builder_reset( old_fbo->draw_frame_builder );
		}

		#if MALI_INSTRUMENTED
	    _profiling_count(INST_GL_NUM_FLUSHES, 1);
		#endif
	}

	/* change bound object */
	fb_state->current_object = fb_obj;
	fb_state->current_object_id = fb_obj_id;

	/* We need to recalculate the viewport when changing fbos as the screen origin might change */
	mali_statebit_set( &ctx->state.common, MALI_STATE_PROJECTION_VIEWPORT_MATRIX_UPDATE_PENDING ); /* needed/used in GLES1 only */
	mali_statebit_set( &ctx->state.common, MALI_STATE_VS_VERTEX_UNIFORM_UPDATE_PENDING);
	mali_statebit_set( &ctx->state.common, MALI_STATE_VS_POLYGON_OFFSET_UPDATE_PENDING );

	/* increase reference count of new binding */
	if(NULL != fb_obj)  _gles_framebuffer_object_addref( fb_obj );
	if(NULL != old_fbo) _gles_framebuffer_object_deref( old_fbo );

	return err;
}

MALI_STATIC void _gles_framebuffer_attachment_init(gles_framebuffer_attachment* attachment, u32 point, u32 wbx_index)
{
	MALI_DEBUG_ASSERT_POINTER(attachment);

	attachment->attach_type         = GLES_ATTACHMENT_TYPE_NONE;
	attachment->name                = 0;
	attachment->completeness_dirty  = MALI_TRUE;
	attachment->point               = point;
	attachment->wbx_index           = wbx_index;
	attachment->discarded           = MALI_FALSE;
}

/**
 * Allocates and returns a new framebuffer object
 * @return A fresh new allocated framebuffer object, or NULL if out of memory
 */
gles_framebuffer_object* _gles_framebuffer_object_new( gles_context *ctx, mali_base_ctx_handle base_ctx, mali_bool implicit_fbuilder )
{

	/* allocate object */
	gles_framebuffer_object* retval = _mali_sys_malloc(sizeof(struct gles_framebuffer_object) );

	MALI_DEBUG_ASSERT_POINTER( ctx );

	if( NULL == retval) return NULL;

	/* nil object - for safety */
	_mali_sys_memset(retval, 0, sizeof(struct gles_framebuffer_object) );

	/* set all members - apart from the refcount, this is pointless. */
	_mali_sys_atomic_initialize(&retval->ref_count, 1);/* we have one reference to this object - its creator. */

	/* initialize attachments */
	_gles_framebuffer_attachment_init(&retval->depth_attachment, MALI_OUTPUT_DEPTH, MALI_DEFAULT_DEPTH_WBIDX);
	_gles_framebuffer_attachment_init(&retval->stencil_attachment, MALI_OUTPUT_STENCIL, MALI_DEFAULT_STENCIL_WBIDX);
	_gles_framebuffer_attachment_init(&retval->color_attachment, MALI_OUTPUT_COLOR, MALI_DEFAULT_COLOR_WBIDX);

	/* initially no discarded attachments */
	retval->discarded_attachments = 0;

	retval->implicit_framebuilder = implicit_fbuilder;

	if(implicit_fbuilder != MALI_FALSE)
	{
		/* allocate a new frame builder */
		retval->completeness_dirty = MALI_TRUE;
		retval->draw_frame_builder = _mali_frame_builder_alloc( MALI_FRAME_BUILDER_TYPE_GLES_FRAMEBUFFER_OBJECT,
																base_ctx, 3,
																MALI_FRAME_BUILDER_PROPS_ROTATE_ON_FLUSH,
																ctx->egl_funcptrs );
		retval->read_frame_builder = retval->draw_frame_builder;
		retval->retain_commands_on_unbind = MALI_FALSE;
		retval->draw_supersample_scalefactor = 1; /* non-default FBOs don't support FSAA */
		if( NULL == retval->draw_frame_builder )
		{
			_mali_sys_free(retval);
			return NULL;
		}
	} else {
		/* the non-implicit framebuilder will be set by eglMakeCurrent sometime later */
		retval->completeness_dirty = MALI_FALSE;
		retval->retain_commands_on_unbind = MALI_TRUE;
		retval->read_frame_builder = NULL;
		retval->draw_frame_builder = NULL;
		retval->draw_supersample_scalefactor = 1;
	}

#if HARDWARE_ISSUE_7571_7572
	retval->framebuffer_has_triangles = MALI_FALSE;
#endif

	return retval;
}

MALI_STATIC void _gles_framebuffer_object_free_detach( gles_framebuffer_object* fb_obj, gles_framebuffer_attachment* att_pnt )
{
	mali_err_code err;

	MALI_DEBUG_ASSERT_POINTER( att_pnt );

	err = _gles_framebuffer_internal_detach( att_pnt, fb_obj );
	MALI_DEBUG_ASSERT( (err == MALI_ERR_NO_ERROR), ("error when detaching in free") );
	MALI_IGNORE( err ); /* ignore in release mode, free must be allowed to complete */

	MALI_DEBUG_PRINT(3, ("%s: frame builder %p: setting output %d to NULL\n", MALI_FUNCTION, fb_obj->draw_frame_builder, att_pnt->wbx_index));
	_mali_frame_builder_set_output( fb_obj->draw_frame_builder, att_pnt->wbx_index, NULL, 0);
	_mali_frame_builder_set_readback( fb_obj->draw_frame_builder, att_pnt->wbx_index, NULL, 0);
}

/**
 * Deallocates a framebuffer object. Note: the associated framebuilder should already be flushed prior to this.
 * @param fb_obj the framebuffer object to deallocate.
 * @note Make sure that the object is completely dereferenced prior to calling this function.
 *       Typically, you want to call _gles_framebuffer_object_deref() instead, which
 *       handles the dereferencing and calls this function if refcount reaches zero.
 */
MALI_STATIC void _gles_framebuffer_object_free( gles_framebuffer_object* fb_obj )
{
	MALI_DEBUG_ASSERT_POINTER( fb_obj );
	MALI_DEBUG_ASSERT( _mali_sys_atomic_get( &fb_obj->ref_count ) == 0, ("At this point, there should be no more references on this object!"));

	/* The default FBO has an implicit framebuilder, which should be cleaned up prior to exit */
	if( fb_obj->implicit_framebuilder != MALI_FALSE )
	{
		_gles_framebuffer_object_free_detach(fb_obj, &fb_obj->color_attachment);
		_gles_framebuffer_object_free_detach(fb_obj, &fb_obj->depth_attachment);
		_gles_framebuffer_object_free_detach(fb_obj, &fb_obj->stencil_attachment);

 	  /* Finally, Free the framebuilder and frame if present.
	   *  The freeing of frames lead to hanging pointers -
	   *  so don't inject any code inside this block unless you know what you are doing.
	   *
	   * NOTE _mali_frame_builder_free will implictly wait for all remaining frames to complete
	   */

		_mali_frame_builder_free(fb_obj->draw_frame_builder);
	}

	fb_obj->draw_frame_builder = NULL;
	fb_obj->read_frame_builder = NULL;

	_mali_sys_free( fb_obj );
	fb_obj = NULL;
}

void _gles_framebuffer_object_list_entry_delete(struct gles_wrapper *wrapper)
{
	MALI_DEBUG_ASSERT_POINTER( wrapper );

	if ( NULL != wrapper->ptr.fbo )
	{
		/* dereference framebuffer object */
		/* Not useful to pass this error-code since this function-call originates in gles_delete_context and when this is done there is no way to retrieve the error */
		MALI_IGNORE( _mali_frame_builder_flush( wrapper->ptr.fbo->draw_frame_builder ) );
		_mali_frame_builder_wait( wrapper->ptr.fbo->draw_frame_builder );
		_gles_framebuffer_object_deref(wrapper->ptr.fbo);
		wrapper->ptr.fbo = NULL;
	}

	/* delete wrapper */
	_gles_wrapper_free( wrapper );
	wrapper = NULL;

}

void _gles_framebuffer_object_deref(gles_framebuffer_object *fb_obj)
{
	u32 ref_count = 0;
	MALI_DEBUG_ASSERT_POINTER( fb_obj );
	MALI_DEBUG_ASSERT((_mali_sys_atomic_get( &fb_obj->ref_count ) > 0 ), ("ref_count on framebuffer object is already 0, the object should have been deleted by now!\n"));

	/* dereference */
	ref_count = _mali_sys_atomic_dec_and_return( &fb_obj->ref_count );
	if ( ref_count > 0) return;

	_gles_framebuffer_object_free( fb_obj );
	return;
}

GLenum _gles_delete_framebuffers( struct gles_context *ctx, GLsizei n, const GLuint *framebuffers )
{
	GLsizei i;
	mali_err_code bind_err = MALI_ERR_NO_ERROR;
	mali_err_code mali_err = MALI_ERR_NO_ERROR;


	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->share_lists );

	/* if n < 0, set error to GL_INVALID_VALUE */
	if( n < 0 ) return GL_INVALID_VALUE;

	/* The GLES specification does not specify an error code for this. At least we avoid crashing */
	if( NULL == framebuffers ) return GL_NO_ERROR;

	/* delete all given objects by dereferencing them, then deleting the wrapper */
	for( i = 0; i < n; i++ )
	{
		struct gles_wrapper* name_wrapper;
		GLuint name = framebuffers[ i ];

		/* silently ignore name 0 */
		if(name == 0) continue;

		/* get the framebuffer object */
		name_wrapper = __mali_named_list_get( ctx->share_lists->framebuffer_object_list, name );
		if (NULL == name_wrapper) continue; /* name not found, silently ignore */

		if (NULL != name_wrapper->ptr.fbo)
		{
			/* remove all bindings from this framebuffer by....
			 * 1: unbind it from the bound framebuffer state - if bound
			 * 2: remove it from the framebuffer list
			 */

			/* 1 - unbind from active state by rebinding the default FBO */
			if (ctx->state.common.framebuffer.current_object == name_wrapper->ptr.fbo)
			{
				mali_err_code bind_err_internal = MALI_ERR_NO_ERROR;

				bind_err_internal = _gles_internal_bind_framebuffer(ctx, ctx->default_framebuffer_object, 0);

				if ( MALI_ERR_NO_ERROR == bind_err_internal )
				{
					_gles_fb_api_buffer_change(ctx);
				}

				/* Only report first error */
				bind_err = ( MALI_ERR_NO_ERROR == bind_err ? bind_err_internal : bind_err );

			}

			/* 2 - delete the program object list entry, wait for flushing to be complete */
			_mali_frame_builder_wait( name_wrapper->ptr.fbo->draw_frame_builder );
			_gles_check_for_rendering_errors( ctx );
			name_wrapper->ptr.fbo->is_deleted = MALI_TRUE;
			_gles_framebuffer_object_deref(name_wrapper->ptr.fbo);
			name_wrapper->ptr.fbo = NULL;
		}

		/* 2 continued - delete the wrapper */
		__mali_named_list_remove(ctx->share_lists->framebuffer_object_list, name);
		_gles_wrapper_free( name_wrapper );
	}
	MALI_CHECK( MALI_ERR_NO_ERROR == mali_err, _gles_convert_mali_err( mali_err ) );

	if (MALI_ERR_NO_ERROR != bind_err )
	{
		MALI_DEBUG_ASSERT( MALI_ERR_OUT_OF_MEMORY == bind_err, ("To get to this patch, you need to have an out of memory error"));
		MALI_ERROR( GL_OUT_OF_MEMORY );
	}

	GL_SUCCESS;
}

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
/* Check attachment format
 * Attachments have different allowed formats depending on the type(Renderbuffer or texture, color, depth or stencil attachment)*/
MALI_STATIC GLenum _gles_check_attachment_format( GLenum format, GLenum attachment_type)
{
	static const GLenum valid_color_attachment_format[] =
	{
		GL_RGB565,
		GL_RGBA4,
		GL_RGB5_A1,
#if EXTENSION_ARM_RGBA8_ENABLE || EXTENSION_RGB8_RGBA8_ENABLE
		GL_RGBA8_OES,
#endif
#if EXTENSION_RGB8_RGBA8_ENABLE
		GL_RGB8_OES,
#endif
	};

	static const GLenum valid_depth_attachment_format[] =
	{
		GL_DEPTH_COMPONENT16,
#if EXTENSION_DEPTH24_OES_ENABLE
		GL_DEPTH_COMPONENT24_OES,
#endif
#if EXTENSION_PACKED_DEPTH_STENCIL_ENABLE
		GL_DEPTH24_STENCIL8_OES,
#endif
	};
	static const GLenum valid_stencil_attachment_format[] =
	{
		GL_STENCIL_INDEX8,
#if EXTENSION_PACKED_DEPTH_STENCIL_ENABLE
		GL_DEPTH24_STENCIL8_OES,
#endif
	};

	/* will point to one of the three above */
	u32 format_array_size = 0;

	switch ( attachment_type ) {
		case GL_COLOR_ATTACHMENT0:
			format_array_size = (u32)MALI_ARRAY_SIZE( valid_color_attachment_format );
			MALI_CHECK( _gles_verify_enum( valid_color_attachment_format, format_array_size, format ), GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT );
			break;
		case GL_DEPTH_ATTACHMENT:
			format_array_size = (u32)MALI_ARRAY_SIZE( valid_depth_attachment_format );
			MALI_CHECK( _gles_verify_enum( valid_depth_attachment_format, format_array_size, format ), GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT );
			break;
		case GL_STENCIL_ATTACHMENT:
			format_array_size = (u32)MALI_ARRAY_SIZE( valid_stencil_attachment_format );
			MALI_CHECK( _gles_verify_enum( valid_stencil_attachment_format, format_array_size, format ), GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT );
			break;
		default:
			MALI_DEBUG_ASSERT(0, ("Illegal attachment_type; must be GL_COLOR_ATTACHMENT0, "
			                      "GL_DEPTH_ATTACHMENT or GL_STENCIL_ATTACHMENT"));
			break;
	}

	return GL_FRAMEBUFFER_COMPLETE;
}

/**
 * @brief Check one attachment for valid type, format and dimensions, according to the rules set up in the GLES spec, section 4.4.5
 *        You may attach most anything to anywhere, but you may not draw with an FBO until it is framebuffer complete.
 * @param fb_attachment A framebuffer attachment point.
 * @param fbo_width, fbo_height InOutput parameters, if sized = 0, will be set to the size of this attachment. If size != 0, compare size to this attachment.
 * @param attachment_type The type of this attachment point, be it a color_attachment, stencil_attach,ent or depth attachment.
 *        Denoted by a GLES attachment enum.
 * @return Returns a GL_FRAMEBUFFER_* error message, or GL_FRAMEBUFFER_COMPLETE if complete.
 **/
MALI_STATIC GLenum _gles_check_attachment_complete( gles_framebuffer_attachment *fb_attachment, u32 *fbo_width, u32 *fbo_height,GLenum attachment_type)
{
	GLenum err = GL_FRAMEBUFFER_COMPLETE;
	u32 width  = 0;
	u32 height = 0;

	switch ( fb_attachment->attach_type )
	{
		case GLES_ATTACHMENT_TYPE_RENDERBUFFER:

			err = _gles_check_attachment_format( fb_attachment->ptr.rb_obj->internalformat, attachment_type);

			width = fb_attachment->ptr.rb_obj->width;
			height = fb_attachment->ptr.rb_obj->height;
			if(width == 0 || height == 0) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;

			break;
		case GLES_ATTACHMENT_TYPE_TEXTURE:
			{
				struct mali_surface *surf= NULL;
				enum mali_pixel_format pixel_format;
				GLenum format;
				const GLenum depthtexture_formats[] =
				{
					#if EXTENSION_DEPTH_TEXTURE_ENABLE & EXTENSION_PACKED_DEPTH_STENCIL_ENABLE
					GL_DEPTH_COMPONENT, GL_DEPTH_STENCIL_OES,
					#elif EXTENSION_DEPTH_TEXTURE_ENABLE
					GL_DEPTH_COMPONENT,
					#endif
				};
				const GLenum stenciltexture_formats[] =
				{
					#if EXTENSION_DEPTH_TEXTURE_ENABLE & EXTENSION_PACKED_DEPTH_STENCIL_ENABLE
					GL_DEPTH_STENCIL_OES,
					#endif
				};

				int chain_index = _gles_texture_object_get_mipchain_index( fb_attachment->cubeface );

				surf = _gles_fb_texture_object_get_mali_surface(fb_attachment->ptr.tex_obj->internal, chain_index, fb_attachment->miplevel);
				if( NULL == surf ) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;

				width = fb_attachment->ptr.tex_obj->mipchains[chain_index]->levels[fb_attachment->miplevel]->width;
				height = fb_attachment->ptr.tex_obj->mipchains[chain_index]->levels[fb_attachment->miplevel]->height;
				format = fb_attachment->ptr.tex_obj->mipchains[chain_index]->levels[fb_attachment->miplevel]->format;
				if(width == 0 || height == 0) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;

				/* some texture formats cannot be written to by mali, disallowed */
				pixel_format = surf->format.pixel_format;				
				if( pixel_format == MALI_PIXEL_FORMAT_NONE ) return GL_FRAMEBUFFER_UNSUPPORTED;

				switch (attachment_type)
				{
					case GL_DEPTH_ATTACHMENT:
						/* Only depth_textures & packed_depth_stencil_textures can be attached as depth attachment */
						/* GL_DEPTH_STENCIL_OES is only valid if depth_textures extension is enabled */
						if(!_gles_verify_enum(depthtexture_formats, MALI_ARRAY_SIZE(depthtexture_formats), format)) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
						break;
					case GL_STENCIL_ATTACHMENT:
						/* Only packed_depth_stencil_textures can be attached as depth attachment */
						/* GL_DEPTH_STENCIL_OES is only valid if depth_textures extension is enabled */
						if(!_gles_verify_enum(stenciltexture_formats, MALI_ARRAY_SIZE(stenciltexture_formats), format)) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
						break;
					case GL_COLOR_ATTACHMENT0:
						/* all formats except the depth and stencil texture formats are legal */
						if(_gles_verify_enum(depthtexture_formats, MALI_ARRAY_SIZE(depthtexture_formats), format)) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
						if(_gles_verify_enum(stenciltexture_formats, MALI_ARRAY_SIZE(stenciltexture_formats), format)) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
						break;
				}

			}
			break;
		default:
			/* No valid type */
			MALI_DEBUG_ASSERT(0, ("Illegal use of function; type must be renderbuffer or texture"));
			break;
	}

	/* if no dimension found yet, set to this fbo size */
	if(*fbo_width == 0xffffffff) *fbo_width = width;
	if(*fbo_height == 0xffffffff) *fbo_height = height;

	/* assert that this attachment is sized equal to the FBO size */
	if(*fbo_width != width || *fbo_height != height) return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;

	return err;
}


#include <stdio.h>
#include <stdarg.h>

GLenum _gles_framebuffer_internal_complete( gles_framebuffer_object *fb_object )
{
	u32 attached_images = 0; /* At least one image must be attached to the framebuffer. */
	u32 fbo_width = 0xffffffff; /* Must be this value since we allow attachments of size 0, if we set to 0, dimension check can fail */
	u32 fbo_height = 0xffffffff;
	u32 samples[3] = {0, 0, 0};
	GLenum err;

	MALI_DEBUG_ASSERT_POINTER( fb_object );

	/* Framebuilders with a given framebuilder does not use the attachments, and are
	 * as such always complete */
	if(fb_object->implicit_framebuilder == MALI_FALSE) return GL_FRAMEBUFFER_COMPLETE;

	/* Scan through framebuffer color attachments */
	if( fb_object->color_attachment.attach_type != GLES_ATTACHMENT_TYPE_NONE)
	{
		MALI_DEBUG_ASSERT(MALI_TRUE == fb_object->implicit_framebuilder, ("Default FBO should never be dirty"));

		samples[attached_images] = fb_object->color_attachment.fsaa_samples;
		attached_images++;
		err = _gles_check_attachment_complete( &fb_object->color_attachment, &fbo_width, &fbo_height, GL_COLOR_ATTACHMENT0 );
		if( err != GL_FRAMEBUFFER_COMPLETE ) return err;
	}

	/* Check framebuffer depth attachment */
	if( fb_object->depth_attachment.attach_type != GLES_ATTACHMENT_TYPE_NONE)
	{
		MALI_DEBUG_ASSERT(MALI_TRUE == fb_object->implicit_framebuilder, ("Default FBO should never be dirty"));

		samples[attached_images] = fb_object->depth_attachment.fsaa_samples;
		attached_images++;
		err = _gles_check_attachment_complete( &fb_object->depth_attachment, &fbo_width, &fbo_height, GL_DEPTH_ATTACHMENT );
		if( err != GL_FRAMEBUFFER_COMPLETE ) return err;
	}

	/* Finally Check framebuffer stencil attachment */
	if( fb_object->stencil_attachment.attach_type != GLES_ATTACHMENT_TYPE_NONE )
	{
		MALI_DEBUG_ASSERT(MALI_TRUE == fb_object->implicit_framebuilder, ("Default FBO should never be dirty"));

		samples[attached_images] = fb_object->stencil_attachment.fsaa_samples;
		attached_images++;
		err = _gles_check_attachment_complete( &fb_object->stencil_attachment, &fbo_width, &fbo_height, GL_STENCIL_ATTACHMENT );
		if( err != GL_FRAMEBUFFER_COMPLETE ) return err;
	}

	if( 0==attached_images ) return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;

#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
	if( 2==attached_images && samples[0]!=samples[1] ) return MALI_GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT_V2;
	if( 3==attached_images && samples[0]!=samples[2] ) return MALI_GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT_V2;
#endif

	return GL_FRAMEBUFFER_COMPLETE;
}


/*
 * Check the framebuffer status for completeness
 */
GLenum _gles_check_framebuffer_status(
                                struct gles_framebuffer_state *fb_state,
                                GLenum target,
                                GLenum *status)
{
	MALI_DEBUG_ASSERT_POINTER( fb_state );
	MALI_DEBUG_ASSERT_POINTER( status );

	if( GL_FRAMEBUFFER != target )
	{
		*status = 0;
		return GL_INVALID_ENUM;
	}

	/* the default object is always complete */
	if( 0 == fb_state->current_object_id )
	{
		*status = GL_FRAMEBUFFER_COMPLETE;
		return GL_NO_ERROR;
	}

	*status = _gles_framebuffer_internal_complete( fb_state->current_object );

	return GL_NO_ERROR;
}
#endif /* EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE */

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
/**
 * Binds a framebuffer object as the active framebuffer object.
 * @param framebuffer_object_list The list containing all created framebuffer objects.
 * @param fb_state The state containing framebuffer related stuff
 * @param target - must be GL_FRAMEBUFFER
 * @param framebuffer - the framebuffer to set to.
 * @return An error value as specified in the GL spec.
 * @note This function is a wrapper for glBindFramebuffer
 */
GLenum _gles_bind_framebuffer( struct gles_context *ctx,
                               mali_named_list *framebuffer_object_list,
                               struct gles_framebuffer_state *fb_state,
                               GLenum target,
                               GLuint name )
{
	mali_base_ctx_handle  base_ctx;
	struct gles_wrapper  *name_wrapper;
	mali_err_code         error_code = MALI_ERR_NO_ERROR;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( fb_state );
	MALI_DEBUG_ASSERT_POINTER( framebuffer_object_list );

	base_ctx = ctx->base_ctx;
	MALI_DEBUG_ASSERT_HANDLE( base_ctx );

	/* target MUST be gl_framebuffer */
	if(target != GL_FRAMEBUFFER) return GL_INVALID_ENUM;

	/* early-out if we are re-binding the currently bound buffer */
	if ( fb_state->current_object_id == name && fb_state->current_object != NULL )
	{
		if( fb_state->current_object->is_deleted == MALI_FALSE )return GL_NO_ERROR;
	}

	if( 0 == name )
	{
		mali_err_code bind_error_code;

		/* change FBO, and possibly flush the accumulated rendering for the old fbo */
		bind_error_code = _gles_internal_bind_framebuffer(ctx, ctx->default_framebuffer_object, 0);

		if ( MALI_ERR_NO_ERROR == bind_error_code )
		{
			_gles_fb_api_buffer_change(ctx);
		}

		if( bind_error_code != MALI_ERR_NO_ERROR ) return _gles_convert_mali_err( bind_error_code );

		return GL_NO_ERROR;
	}

	/* get object from list */
	name_wrapper = __mali_named_list_get( framebuffer_object_list, name );
	if( NULL == name_wrapper)
	{
		/* name not found - we need to add it to the list! */
		name_wrapper = _gles_wrapper_alloc(WRAPPER_FRAMEBUFFER);

		MALI_CHECK_NON_NULL( name_wrapper, GL_OUT_OF_MEMORY );

		name_wrapper->ptr.fbo = NULL;

		/* Aaaand insert wrapper into list */
		error_code = __mali_named_list_insert(framebuffer_object_list, name, name_wrapper);
		if( error_code != MALI_ERR_NO_ERROR )
		{
			MALI_DEBUG_ASSERT(error_code == MALI_ERR_OUT_OF_MEMORY, ("The only legal error return code can be MALI_ERR_OUT_OF_MEMORY; never anything else"));
			_gles_wrapper_free( name_wrapper );
			name_wrapper = NULL;
			return GL_OUT_OF_MEMORY;
		}
	}

	/* at bind, we might need to create the object */
	if( NULL == name_wrapper->ptr.fbo )
	{
		name_wrapper->ptr.fbo = _gles_framebuffer_object_new( ctx, base_ctx, MALI_TRUE );
		MALI_CHECK_NON_NULL( name_wrapper->ptr.fbo, GL_OUT_OF_MEMORY );
	}

	/* Found the object we were looking for. Set it, and possibly flush the old FBO in the process */
	error_code = _gles_internal_bind_framebuffer(ctx, name_wrapper->ptr.fbo, name);
	if ( MALI_ERR_NO_ERROR == error_code )
	{
		_gles_fb_api_buffer_change(ctx);
		error_code = _gles_begin_frame(ctx);
	}

	return _gles_convert_mali_err( error_code );
}


/**
 * @brief Attaches a renderbuffer to the active framebuffer
 * @param fb_state The framebuffer state struct, so we can know which framebuffer is currently bound
 * @param framebuffer_object_list The framebuffer object list; mainly required for mutex locking
 * @param renderbuffr_object_list The renderbuffer object list; used to lookup the renderbuffer to bind
 * @param target Must be GL_FRAMEBUFFER
 * @param attachment Must be a valis attachment point
 * @param renderbuffertarget Must be GL_RENDERBUFFER
 * @param renderbuffertarget The name we want to attach.
 * @note This function is a wrapper around glFramebufferRenderbuffer
 */
GLenum _gles_framebuffer_renderbuffer( struct gles_framebuffer_state *fb_state,
                                      mali_named_list *framebuffer_object_list,
                                      mali_named_list *renderbuffer_object_list,
                                      mali_named_list *texture_object_list,
                                      GLenum target,
                                      GLenum attachment,
                                      GLenum renderbuffertarget,
                                      GLuint renderbuffer )
{
	gles_renderbuffer_object *obj = NULL;
	gles_framebuffer_attachment* attachment_point = NULL;
	mali_err_code err = MALI_ERR_NO_ERROR;

	MALI_DEBUG_ASSERT_POINTER(fb_state);
	MALI_DEBUG_ASSERT_POINTER(framebuffer_object_list);
	MALI_DEBUG_ASSERT_POINTER(renderbuffer_object_list);
	MALI_DEBUG_ASSERT_POINTER(texture_object_list);


	/* target must be GL_FRAMEBUFFER */
	if(target != GL_FRAMEBUFFER) return GL_INVALID_ENUM;

	/* if renderbuffer is non-zero then renderbuffertarget must be GL_RENDERBUFFER */
	if(renderbuffer != 0 && renderbuffertarget != GL_RENDERBUFFER) return GL_INVALID_ENUM;

	/* We cannot modify framebuffer 0 */
	if( fb_state->current_object_id == 0) return GL_INVALID_OPERATION;

	attachment_point = _gles_get_attachment_point(fb_state->current_object, attachment);

	/* if no attachment point was found, it was an illegal attachment point. Game over. */
	MALI_CHECK( attachment_point, GL_INVALID_ENUM );

	/* renderbuffer 0 means detach, and require special handling */
	if(renderbuffer != 0)
	{
		struct gles_wrapper* name_wrapper;
		/* get the renderbuffer object */
		name_wrapper = __mali_named_list_get( renderbuffer_object_list, renderbuffer );

		if( ( NULL == name_wrapper ) || ( NULL == name_wrapper->ptr.rbo ) )
		{
			err = _gles_framebuffer_internal_detach( attachment_point, fb_state->current_object );
			if(err != MALI_ERR_NO_ERROR) return GL_OUT_OF_MEMORY;
			return GL_INVALID_OPERATION;
		}

		obj = name_wrapper->ptr.rbo;
	}

	/* test whether the attachment point has this object attached already - if so, we can do an early-out
	 * This early out is to avoid unnescessary flushing to the other attachment point; after all there is no change! */
	MALI_CHECK( !( ( attachment_point->attach_type == GLES_ATTACHMENT_TYPE_RENDERBUFFER) && ( attachment_point->ptr.rb_obj == obj ) ), GL_NO_ERROR );

	/* attach the object to the attachment point; dereference the old attachment */
	err = _gles_framebuffer_internal_detach( attachment_point, fb_state->current_object );
	if(err != MALI_ERR_NO_ERROR) return GL_OUT_OF_MEMORY;

	/* Add new connection for renderbuffer */
	if ( renderbuffer != 0 )
	{
		err = _gles_fbo_bindings_add_binding( obj->fbo_connection, fb_state->current_object, attachment_point );

		MALI_CHECK( MALI_ERR_NO_ERROR == err, _gles_convert_mali_err( err ) );
	}

	fb_state->current_object->completeness_dirty = MALI_TRUE;
	attachment_point->completeness_dirty = MALI_TRUE;
#if EXTENSION_DISCARD_FRAMEBUFFER
	attachment_point->discarded = MALI_FALSE;
	_gles_framebuffer_update_discard_flags(fb_state->current_object, attachment_point);
#endif

	if(renderbuffer != 0)
	{
		/* attach new renderbuffer */
		attachment_point->attach_type = GLES_ATTACHMENT_TYPE_RENDERBUFFER;
		attachment_point->name = renderbuffer;
		attachment_point->ptr.rb_obj = obj;
#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
		attachment_point->fsaa_samples = obj->fsaa_samples;
#endif
		_gles_renderbuffer_object_addref(obj);
	}

	return GL_NO_ERROR;
}

GLenum _gles_framebuffer_texture2d( struct gles_context *ctx,
		struct gles_framebuffer_state *fb_state,
		mali_named_list *framebuffer_object_list,
		mali_named_list *renderbuffer_object_list,
		mali_named_list *texture_object_list,
		GLenum target,
		GLenum attachment,
		GLenum textarget,
		GLuint texture,
		GLint level )
{
	return _gles_framebuffer_texture2d_multisample(ctx, fb_state, framebuffer_object_list, renderbuffer_object_list, texture_object_list, target, attachment, textarget, texture, level, 0);
}

GLenum _gles_framebuffer_texture2d_multisample(   struct gles_context *ctx,
		struct gles_framebuffer_state *fb_state,
		mali_named_list *framebuffer_object_list,
		mali_named_list *renderbuffer_object_list,
		mali_named_list *texture_object_list,
		GLenum target,
		GLenum attachment,
		GLenum textarget,
		GLuint texture,
		GLint level,
		GLsizei samples )
{
	static const GLenum legal_cubefaces[] = { GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
	                                          GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
	                                          GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};
	mali_bool is_cubeface = _gles_verify_enum(legal_cubefaces, MALI_ARRAY_SIZE(legal_cubefaces), textarget);

	gles_texture_object *obj = NULL;
	gles_framebuffer_attachment* attachment_point = NULL;
	struct gles_wrapper* name_wrapper;
	mali_err_code mali_err = MALI_ERR_NO_ERROR;

	MALI_DEBUG_ASSERT_POINTER(fb_state);
	MALI_DEBUG_ASSERT_POINTER(framebuffer_object_list);
	MALI_DEBUG_ASSERT_POINTER(renderbuffer_object_list);
	MALI_DEBUG_ASSERT_POINTER(texture_object_list);


	/* textarget must be a cubeface or GL_TEXTURE_2D */
	if(textarget != GL_TEXTURE_2D && is_cubeface == MALI_FALSE ) return GL_INVALID_ENUM;

	/* target must be GL_FRAMEBUFFER */
	if(target != GL_FRAMEBUFFER) return GL_INVALID_ENUM;

	/* only mipmap level supported is 0 */
	if(level != 0 ) return GL_INVALID_VALUE;


	/* We cannot modify framebuffer 0 */
	if( fb_state->current_object_id == 0) return GL_INVALID_OPERATION;

	/* get the attachment point */
	attachment_point = _gles_get_attachment_point(fb_state->current_object, attachment);
	MALI_CHECK_NON_NULL( attachment_point, GL_INVALID_ENUM );

	mali_statebit_set( &ctx->state.common, GLES_STATE_COLOR_ATTACHMENT_DIRTY);

	/* texture 0 means detach, and require special handling */
	if(texture == 0)
	{
		mali_err = _gles_framebuffer_internal_detach(attachment_point, fb_state->current_object );

		fb_state->current_object->completeness_dirty = MALI_TRUE;
		attachment_point->completeness_dirty = MALI_TRUE;

		return _gles_convert_mali_err(mali_err);
	}

#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
	MALI_CHECK((samples<=GLES_MAX_FBO_SAMPLES), GL_INVALID_VALUE);
	attachment_point->fsaa_samples = GLES_NEXT_SUPPORTED_FBO_SAMPLES(samples);
#endif

	/* get the object */
	name_wrapper = __mali_named_list_get( texture_object_list, texture );
	if( ( NULL == name_wrapper ) || ( NULL == name_wrapper->ptr.tex ) )
	{
		mali_err = _gles_framebuffer_internal_detach(attachment_point, fb_state->current_object );
		if(mali_err != MALI_ERR_NO_ERROR) return GL_OUT_OF_MEMORY;
		return GL_INVALID_OPERATION;
	}

	obj = name_wrapper->ptr.tex;

	/* test whether the attachment point has this object attached already - if so, we can do an early-out */
	MALI_CHECK( !(	( attachment_point->attach_type == GLES_ATTACHMENT_TYPE_TEXTURE ) &&
					( attachment_point->ptr.tex_obj == obj ) &&
					( attachment_point->cubeface == textarget )
				 ), GL_NO_ERROR );

	/* assert that the texture dimensionality matches the given object */
	if( ( ( obj->dimensionality == GLES_TEXTURE_TARGET_2D ) && ( is_cubeface == MALI_TRUE ) ) ||
		( ( obj->dimensionality == GLES_TEXTURE_TARGET_CUBE ) && ( is_cubeface == MALI_FALSE ) ) )
	{
		mali_err = _gles_framebuffer_internal_detach(attachment_point, fb_state->current_object );
		if(mali_err != MALI_ERR_NO_ERROR) return GL_OUT_OF_MEMORY;
		return GL_INVALID_OPERATION;
	}

	/* attach the object to the attachment point; dereference the old attachment */
	mali_err = _gles_framebuffer_internal_detach( attachment_point, fb_state->current_object );

	fb_state->current_object->completeness_dirty = MALI_TRUE;
	attachment_point->completeness_dirty = MALI_TRUE;	
	{
		mali_err_code err_here;
		struct gles_mipmap_level *mip = _gles_texture_object_get_mipmap_level(obj, textarget, level);
		if ( NULL == mip )
		{
			mali_bool red_blue_swap, reverse_order;
			GLenum err = GL_NO_ERROR;

			/* retrieve the texture object red_blue_swap and reverse_order flags */
			_gles_texture_object_get_internal_component_flags(obj, &red_blue_swap, &reverse_order);

			/* create a new texture image */
			err = _gles_tex_image_2d_internal(obj, ctx, textarget, level, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE,
					(GLboolean)red_blue_swap, (GLboolean)reverse_order, NULL, 1*4 );

			if( GL_NO_ERROR != err ) return err;

			mip = _gles_texture_object_get_mipmap_level(obj, textarget, level);

			MALI_CHECK_NON_NULL( mip, GL_OUT_OF_MEMORY );
		}
		if ( NULL == mip->fbo_connection )
		{
			mip->fbo_connection = _gles_fbo_bindings_alloc( );
			MALI_CHECK_NON_NULL( mip->fbo_connection, GL_OUT_OF_MEMORY );
		}
		err_here = _gles_fbo_bindings_add_binding( mip->fbo_connection, fb_state->current_object, attachment_point );

		MALI_CHECK( MALI_ERR_NO_ERROR == err_here, _gles_convert_mali_err( err_here ) );
	}
	attachment_point->name = texture;
	attachment_point->attach_type = GLES_ATTACHMENT_TYPE_TEXTURE;
	attachment_point->ptr.tex_obj = obj;
	attachment_point->miplevel = level;
	attachment_point->cubeface = textarget;
#if EXTENSION_DISCARD_FRAMEBUFFER
	attachment_point->discarded = MALI_FALSE;
	_gles_framebuffer_update_discard_flags( fb_state->current_object, attachment_point );
#endif

	_gles_texture_object_addref(obj);

	return _gles_convert_mali_err(mali_err);
}

GLenum _gles_get_framebuffer_attachment_parameter( struct gles_framebuffer_state *fb_state,
		mali_named_list* framebuffer_object_list,
		GLenum target, GLenum attachment, GLenum pname, GLint *params )
{
	GLenum state;
	gles_framebuffer_attachment* attachment_point = NULL;

	MALI_DEBUG_ASSERT_POINTER(fb_state);
	MALI_DEBUG_ASSERT_POINTER(framebuffer_object_list);

	/* target MUST be GL_FRAMEBUFFER */
	if(target!= GL_FRAMEBUFFER) return GL_INVALID_ENUM;

	/* FBO 0 doesn't count. */
	if( fb_state->current_object_id == 0) return GL_INVALID_OPERATION;

	/* get attachment point. if not found, we had an illegal attachment enum */
	attachment_point = _gles_get_attachment_point( fb_state->current_object ,attachment);
	MALI_CHECK_NON_NULL( attachment_point, GL_INVALID_ENUM );

	/* handle all parameter names */
	switch(pname)
	{
		case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
		{
			switch(attachment_point->attach_type)
			{
				case GLES_ATTACHMENT_TYPE_NONE: *params = GL_NONE; break;
				case GLES_ATTACHMENT_TYPE_TEXTURE: *params = GL_TEXTURE; break;
				case GLES_ATTACHMENT_TYPE_RENDERBUFFER: *params = GL_RENDERBUFFER; break;
				default: MALI_DEBUG_ASSERT(0, ("Erroneous attachment type")); break;
			}
			state = GL_NO_ERROR;
			break;
		}
		case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
            switch(attachment_point->attach_type)
            {
				case GLES_ATTACHMENT_TYPE_TEXTURE:
				case GLES_ATTACHMENT_TYPE_RENDERBUFFER:
					*params = attachment_point->name;
					state = GL_NO_ERROR;
					break;
				case GLES_ATTACHMENT_TYPE_NONE:
					*params = GL_NONE;
					state = GL_INVALID_ENUM;
					break;
				default:
					MALI_DEBUG_ASSERT(0, ("Erroneous attachment type"));
					state = GL_NO_ERROR;
					break;
            }
            break;
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
		{
			if(attachment_point->attach_type == GLES_ATTACHMENT_TYPE_TEXTURE)
			{
				*params = attachment_point->miplevel;
				state = GL_NO_ERROR;
				break;
			} else {
				state = GL_INVALID_ENUM;
				break;
			}
		}
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
		{
			if(attachment_point->attach_type == GLES_ATTACHMENT_TYPE_TEXTURE)
			{
				if (GL_TEXTURE_2D == attachment_point->cubeface)
				{
					/* NOTE Refer to Khronos bugzilla #3308 for the details of this special case */
					*params = 0;
					state = GL_NO_ERROR;
				} else
				{
					*params = attachment_point->cubeface;
					state = GL_NO_ERROR;
				}
				break;
			} else {
				state = GL_INVALID_ENUM;
				break;
			}

		}
#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
		case MALI_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT_V1: /* fall-through */
		case MALI_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT_V2:
		{
			*params = attachment_point->fsaa_samples;
			state = GL_NO_ERROR;
			break;
		}
#endif

		default: state = GL_INVALID_ENUM; break;
	}

	return state;
}

/**
 * Resolves a completeness-dirty flag on the given attachment point by reattaching the surface of the attached object.
 * Calling this function will set the dirty flag to MALI_FALSE.
 * @param ctx The gles context
 * @param fbo the fbo this attachment is attached to
 * @param attachment_point the attachment point to resolve
 * @return MALI_ERR_NO_ERROR if no errors occurred. The render_attachment wraps a valid surface and is ready for use.
 *         MALI_ERR_OUT_OF_MEMORY if we ran out of memory while allocating the render attachment.
 *         MALI_ERR_FUNCTION_FAILED can happen if the specified attachment surface is not present.
 * @note The framebuilder related to the surface MUST be flushed and waited for prior to calling this function, and you must be 100% certain that no other
 *       instances can add more drawcalls to the framebuilder while this function does its thing. This function will attach/detach render attachments from the framebuilder and
 *       unless the framebuilder is flushed/waited, you risk removing - and thus deleting - surfaces that m200 is currently writing to. Which will cause HW segfaults.
 */
MALI_CHECK_RESULT MALI_STATIC mali_err_code _gles_fbo_attachment_resolve_completeness_dirty( struct gles_context* ctx, struct gles_framebuffer_object* fbo, struct gles_framebuffer_attachment *attachment_point )
{
	struct mali_surface* render_target = NULL;
	struct gles_mipmap_level *dmip;
	mali_err_code err = MALI_ERR_NO_ERROR;
	u32 usage = 0;

	MALI_DEBUG_ASSERT_POINTER( attachment_point );

	/* early out if nothing is dirty - this attachment point need no new render attachment */
	if(attachment_point->completeness_dirty == MALI_FALSE) return MALI_ERR_NO_ERROR;

	/* detach old attachment from the frambuilder */
	MALI_DEBUG_PRINT(3, ("%s: frame builder %p: setting output %d to NULL\n", MALI_FUNCTION, fbo->draw_frame_builder, attachment_point->wbx_index));
	_mali_frame_builder_set_output(fbo->draw_frame_builder, attachment_point->wbx_index, NULL, 0);
	_mali_frame_builder_set_readback( fbo->draw_frame_builder, attachment_point->wbx_index, NULL, 0);

	/* retrieve the render target of the object attached */
	switch( attachment_point->attach_type )
	{
		case GLES_ATTACHMENT_TYPE_TEXTURE:
			MALI_DEBUG_ASSERT_POINTER( attachment_point->ptr.tex_obj );
			dmip = _gles_texture_object_get_mipmap_level( attachment_point->ptr.tex_obj, attachment_point->cubeface, attachment_point->miplevel );
			if ( dmip == NULL ) return MALI_ERR_FUNCTION_FAILED;  /* No texture buffer was specified, early out with failure */
			err = _gles_texture_miplevel_set_renderable(ctx, attachment_point->ptr.tex_obj, attachment_point->cubeface, attachment_point->miplevel );
			if(err != MALI_ERR_NO_ERROR) return err;

			MALI_DEBUG_ASSERT_POINTER( attachment_point->ptr.tex_obj->internal );
			render_target = _gles_fb_texture_object_get_mali_surface( attachment_point->ptr.tex_obj->internal, _gles_texture_object_get_mipchain_index(attachment_point->cubeface), attachment_point->miplevel );
			MALI_DEBUG_ASSERT_POINTER( render_target );
			break;
		case GLES_ATTACHMENT_TYPE_RENDERBUFFER:
			if ( attachment_point->ptr.rb_obj->render_target == NULL ) return MALI_ERR_FUNCTION_FAILED; /* no storage specified, early out with failure */
			render_target = attachment_point->ptr.rb_obj->render_target;
			break;
		case GLES_ATTACHMENT_TYPE_NONE:
			break;
		default: MALI_DEBUG_ASSERT( 0, ("Invalid type of attachment_point") ); break;
	}

	usage = attachment_point->point;

#if EXTENSION_PACKED_DEPTH_STENCIL_ENABLE
	/* if the same attachment is attached to both the depth and stencil targets
	 * This is only legal if both surfaces are of depth/stencil renderable formats, otherwise it would've failed
	 * in the completeness check, and never hit this function.
	 * Note: this condition will also hit if both surfaces are NULL, but that is okay. */
	if( _gles_get_attachment_surface(&fbo->depth_attachment) == _gles_get_attachment_surface(&fbo->stencil_attachment))
	{
		/* if attachment point is DEPTH, then don't attach anything */
		/* if attachment point is STENCIL, then make sure to attach this as a depthstencil attachment */
		if(attachment_point->point == MALI_OUTPUT_STENCIL)
		{
			usage = MALI_OUTPUT_DEPTH | MALI_OUTPUT_STENCIL;
		}
		if(attachment_point->point == MALI_OUTPUT_DEPTH)
		{
			render_target = NULL; /* this makes the subsequent code not attach anything */
		}

	}
#endif

#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
	if(attachment_point->attach_type != GLES_ATTACHMENT_TYPE_NONE)
	{
		MALI_DEBUG_ASSERT( GLES_SUPPORTED_FBO_SAMPLES(attachment_point->fsaa_samples), ("Unsupported FSAA for FBO: %d", attachment_point->fsaa_samples));
#if 0
		if(attachment_point->fsaa_samples == 16)
		{
			fbo->draw_supersample_scalefactor = 2;
			usage |= MALI_OUTPUT_FSAA_4X;
		}
		else
#endif
		{
			fbo->draw_supersample_scalefactor = 1;
		}
	}
#endif

	/* update the render attachment corresponding to this fbo attachment point. it may be NULL */
	if (render_target)
	{
#if EXTENSION_DISCARD_FRAMEBUFFER
		/* if the attachment is discarded, set the render target to NULL to make sure it won't be used */
		if (GLES_ATTACHMENT_TYPE_NONE != attachment_point->attach_type && MALI_TRUE == attachment_point->discarded)
		{
			render_target = NULL;
		}
#endif
		MALI_DEBUG_PRINT(3, ("%s: frame builder %p: setting output %d of to %p\n", MALI_FUNCTION, fbo->draw_frame_builder, attachment_point->wbx_index, render_target));
		_mali_frame_builder_set_output(fbo->draw_frame_builder, attachment_point->wbx_index, render_target, usage);
		_mali_frame_builder_set_readback( fbo->draw_frame_builder, attachment_point->wbx_index, render_target, usage);
	}

	attachment_point->completeness_dirty = MALI_FALSE;
	return MALI_ERR_NO_ERROR;
}

/**
 * Resolves the FBO constraits dirty flag on a given FBO. This will update all the render attachemnts on attachment points that are dirty.
 * @param fbo the fbo to resolve
 * @return MALI_ERR_NO_ERROR if no errors occurred. The fbo is ready to be drawn to
 *         MALI_ERR_OUT_OF_MEMORY if we ran out of memory
 *         MALI_ERR_FUNCTION_FAILED can happen if either of the render attachment points point to a surface which is not available
 * @note The framebuilder related to the surface MUST be flushed and waited for prior to calling this function, and you must be 100% certain that no other
 *       instances can add more drawcalls to the framebuilder while this function does its thing. This function will attach/detach render attachments from the framebuilder and
 *       unless the framebuilder is flushed/waited, you risk removing - and thus deleting - surfaces that m200 is currently writing to. Which will cause HW segfaults.
 */
MALI_STATIC MALI_CHECK_RESULT mali_err_code _gles_fbo_resolve_completeness_dirty(struct gles_context* ctx, struct gles_framebuffer_object* fbo)
{
	mali_err_code error;

	/* third, update all the attachment points' render attachments */
	error = _gles_fbo_attachment_resolve_completeness_dirty( ctx, fbo, &fbo->color_attachment );
	if( error != MALI_ERR_NO_ERROR ) return error;

	error = _gles_fbo_attachment_resolve_completeness_dirty( ctx, fbo, &fbo->depth_attachment );
	if( error != MALI_ERR_NO_ERROR ) return error;

	error = _gles_fbo_attachment_resolve_completeness_dirty( ctx, fbo, &fbo->stencil_attachment );
	if( error != MALI_ERR_NO_ERROR ) return error;

	fbo->completeness_dirty = MALI_FALSE;

	return MALI_ERR_NO_ERROR;
}
#endif

GLenum _gles_fbo_internal_draw_setup( gles_context *ctx )
{
/* GLES 1 only builds do not have any way to make their attachments completeness dirty as the default FBO do not have any attachments
 * Filter out this code to marginally speed up this function. It no longer need to do anything. */
#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	gles_framebuffer_object *fb_object;
	GLenum                   err = GL_NO_ERROR;
	mali_err_code            error;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	fb_object = ctx->state.common.framebuffer.current_object;

	MALI_DEBUG_ASSERT_POINTER( fb_object );

	if(fb_object == ctx->default_framebuffer_object)
	{
		_mali_frame_builder_acquire_output( fb_object->draw_frame_builder );
	}

	/* Need to run a completeness check if the completeness-dirtyflag is set to true */
	if( fb_object->completeness_dirty == MALI_TRUE)
	{
		MALI_DEBUG_ASSERT(fb_object != ctx->default_framebuffer_object, ("The default FBO should never be flagged dirty"));

		/* run a proper completeness check to assert that it's worth setting up new attachments */
		err = _gles_framebuffer_internal_complete( fb_object );
		if( err != GL_FRAMEBUFFER_COMPLETE ) return GL_INVALID_FRAMEBUFFER_OPERATION;

		/* flush out the old FBO contents. This will also set the surface owners to NULL */
		error = _mali_frame_builder_flush( fb_object->draw_frame_builder );
		if( error != MALI_ERR_NO_ERROR ) return _gles_convert_mali_err( error );

		/* create new attachments on attachemnt points that have been dirtied */
		error = _gles_fbo_resolve_completeness_dirty( ctx, fb_object );
		if( error != MALI_ERR_NO_ERROR ) return _gles_convert_mali_err( error );

		/* Update fb with api buffer changes */
		_gles_fb_api_buffer_change( ctx );

		/* initialize the framebuilder */
		mali_statebit_set( &ctx->state.common, MALI_STATE_FBO_DIRTY );
	}

#if EXTENSION_DISCARD_FRAMEBUFFER
	/* Restore possibly discarded attachments. Must be done only after the flush above to
	 * make sure the discarded attachment stay discarded; don't do dirtiness
	 * resolution, as it'll be done right after this.
	 *
	 * Restoration has to be done here and in _gles_drawcall_begin, because these two functions
	 * are always called when drawing. The order in which they're called, depends on whether
	 * HARDWARE_ISSUE_7571_7572 fix is enabled or not. */
	if ( _gles_framebuffer_has_discarded_attachments( fb_object ) )
	{
		_gles_framebuffer_restore_discarded_attachments( ctx, fb_object, MALI_TRUE );
	}
#endif

#else
	MALI_IGNORE(ctx);
#endif

	return GL_NO_ERROR;
}

#if EXTENSION_DISCARD_FRAMEBUFFER
/* Map DiscardFramebufferEXT added attachment name aliases to normal attachment names.
 * Maps standard attachment names to themselves to make the function usable for validation
 * purposes as well */
MALI_STATIC GLenum _gles_resolve_attachment_alias( GLenum attachment )
{
	/* map the id to discarded attachment point */
	switch (attachment)
	{
		case GL_DEPTH_EXT:
		case GL_DEPTH_ATTACHMENT:
			return GL_DEPTH_ATTACHMENT;

		case GL_STENCIL_EXT:
		case GL_STENCIL_ATTACHMENT:
			return GL_STENCIL_ATTACHMENT;

		case GL_COLOR_EXT:
		case GL_COLOR_ATTACHMENT0:
			return GL_COLOR_ATTACHMENT0;
	}

	/* invalid attachment */
	return GL_NONE;
}

/* Updates the discarded attachments bitmask for given attachment */
MALI_STATIC_INLINE void _gles_framebuffer_update_discard_flags( struct gles_framebuffer_object* fb_obj, struct gles_framebuffer_attachment* ap_obj )
{
	MALI_DEBUG_ASSERT_POINTER( fb_obj );
	MALI_DEBUG_ASSERT_POINTER( ap_obj );

	MALI_DEBUG_ASSERT( MALI_FALSE == ap_obj->discarded || MALI_TRUE == ap_obj->discarded, \
	                   ("Attachment point %d (%p) corrupted", ap_obj->wbx_index, ap_obj ) );

	/* clear flag */
	fb_obj->discarded_attachments &= ~( 1 << ap_obj->wbx_index );
	/* set if needed */
	fb_obj->discarded_attachments |= ( ap_obj->discarded << ap_obj->wbx_index );
}

MALI_STATIC_INLINE u32 _gles_framebuffer_present_attachments( struct gles_framebuffer_object* fb_obj )
{
	MALI_DEBUG_ASSERT_POINTER( fb_obj );

	return ( ( GLES_ATTACHMENT_TYPE_NONE != fb_obj->color_attachment.attach_type )   << fb_obj->color_attachment.wbx_index ) |
	       ( ( GLES_ATTACHMENT_TYPE_NONE != fb_obj->depth_attachment.attach_type )   << fb_obj->depth_attachment.wbx_index ) |
	       ( ( GLES_ATTACHMENT_TYPE_NONE != fb_obj->stencil_attachment.attach_type ) << fb_obj->stencil_attachment.wbx_index );
}

/* Discard/restore attachment. Dirties both the attachment point and the
 * framebuffer object so that nex resolve-dirtiness call will attach/
 * detach the attachments whose state has changed */
MALI_STATIC mali_bool _gles_framebuffer_set_attachment_discarded( struct gles_framebuffer_object* fb_obj, struct gles_framebuffer_attachment* ap_obj, mali_bool discarded )
{
	MALI_DEBUG_ASSERT_POINTER( fb_obj );
	MALI_DEBUG_ASSERT_POINTER( ap_obj );

	/* default framebuffer has attachment type none for all attachment points,
	 * since it really doesn't have any attachments at all. this effectively
	 * makes all discard operations a NOP on default framebuffer */
    if ( GLES_ATTACHMENT_TYPE_NONE != ap_obj->attach_type &&  discarded != ap_obj->discarded )
	{

		/* flag as discarded and completeness-dirty to force attach/detach to/from the frame builder */
		ap_obj->discarded          = discarded;
		ap_obj->completeness_dirty = MALI_TRUE;
		fb_obj->completeness_dirty = MALI_TRUE;
		_gles_framebuffer_update_discard_flags( fb_obj, ap_obj );

		MALI_DEBUG_PRINT( 3, ( "%s: %s attachment (ap=%d, wbx_idx=%d)\n", \
		                  MALI_FUNCTION, discarded ? "discarding" : "restoring", \
		                  ap_obj->point, ap_obj->wbx_index ) );
		return MALI_TRUE;
	}
	else
	{
		return MALI_FALSE;
	}
}

/* Restore all (if any) discarded attachments in the FBO. Forces attach/detach
 * to/from frame builder so after calling this function the framebuffer is
 * in safe state for drawing. */
void _gles_framebuffer_restore_discarded_attachments( struct gles_context* ctx, struct gles_framebuffer_object* fb_obj, mali_bool resolve )
{
	int restored = 0;

	MALI_DEBUG_ASSERT_POINTER(fb_obj);

	/* restore all attachments */
	if(MALI_TRUE == _gles_framebuffer_set_attachment_discarded( fb_obj, &fb_obj->color_attachment, MALI_FALSE ) )
		restored++;
	if(MALI_TRUE == _gles_framebuffer_set_attachment_discarded( fb_obj, &fb_obj->depth_attachment, MALI_FALSE ) )
		restored++;
	if(MALI_TRUE == _gles_framebuffer_set_attachment_discarded( fb_obj, &fb_obj->stencil_attachment, MALI_FALSE ) )
		restored++;

	/* reality-check */
	MALI_DEBUG_ASSERT( 0 == fb_obj->discarded_attachments, \
	                   ("Framebuffer discarded-attachments-flags corrupted (value = %d)", \
	                   fb_obj->discarded_attachments ) );

	/* force reattach to frame builder. ignore any possible errors here */
	if ( resolve && restored > 0 )
	{
		mali_err_code err = _gles_fbo_resolve_completeness_dirty( ctx, fb_obj );
		MALI_IGNORE(err);
	}
}

/* Validate attachment list. There're two sets of valid attachment names: one
 * for the default framebuffer, one for FBOs. */
MALI_STATIC GLboolean _gles_validate_discard_framebuffer_attachments( GLboolean is_default, GLsizei count, const GLenum* attachments )
{
	const GLenum					valid_attachments[2][4] = {
	                                    /* FBO */
	                                    { GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT, GL_COLOR_ATTACHMENT0, GL_NONE },
	                                     /* default framebuffer */
	                                    { GL_DEPTH_EXT,        GL_STENCIL_EXT,        GL_COLOR_EXT,         GL_NONE }
	                                };
	GLuint							idx;

	MALI_DEBUG_ASSERT_POINTER( attachments );

	idx = !!is_default;

	/* iterate through the list and check that it exists in the list of valid attachments */
	while ( count > 0 )
	{
		const GLenum* valid = valid_attachments[idx];

		while ( GL_NONE != *valid )
		{
			if ( *valid == *attachments )
				break;
			valid++;
		}

		/* reached the end of the valid ones list without finding a match */
		if ( GL_NONE == *valid )
			return GL_FALSE;

		count--;
		attachments++;
	}
	return GL_TRUE;
}

/* Marks the requested framebuffer attachments as discarded. The actual frame builder bindings are
 * torn down in _gles_fbo_attachment_resolve_completeness_dirty. Re-attaching is done in
 * _gles_fbo_internal_draw_setup */
GLenum _gles_discard_framebuffer( struct gles_context *ctx, struct gles_framebuffer_state *fb_state, GLenum target, GLsizei count, const GLenum *attachments )
{
	gles_framebuffer_attachment*    ap_obj = NULL;
	gles_framebuffer_object*		fb_obj = NULL;
	GLsizei                         i;
	GLboolean						is_default;
	int			discarded = 0;

	MALI_DEBUG_ASSERT_POINTER( fb_state );
	MALI_DEBUG_ASSERT_POINTER( attachments );

	fb_obj = fb_state->current_object;
	MALI_DEBUG_ASSERT_POINTER( fb_obj );

	MALI_DEBUG_PRINT( 3, ( "%s: called for framebuffer(%p), id=%d, frame builder=%p\n", \
	                  MALI_FUNCTION, fb_state->current_object, fb_state->current_object_id, \
	                  fb_state->current_object->draw_frame_builder ) );

	/* validate arguments */
	if( target != GL_FRAMEBUFFER )
		return GL_INVALID_ENUM;

	/* The default framebuffer object should always have ID 0 and be marked to have explicit frame builder.
	 * FBOs should have ID != 0 and should have implicit (created by us, that is) frame builder */
	MALI_DEBUG_ASSERT( ( 0 == fb_state->current_object_id && MALI_FALSE == fb_obj->implicit_framebuilder ) || \
	                   ( 0 != fb_state->current_object_id && MALI_TRUE == fb_obj->implicit_framebuilder), \
	                   ( "Inconsistency between the FBO ID and frame builder implicity flag. The FBO code is not working." ) );

	/* the default framebuffer has explicit frame builder */
	is_default = !fb_obj->implicit_framebuilder;

	if ( !_gles_validate_discard_framebuffer_attachments( is_default, count, attachments ) )
	{
		MALI_DEBUG_PRINT( 3, ( "%s: invalid attachments for %s\n", \
		                  MALI_FUNCTION, is_default ? "default framebuffer" : "an FBO" ) );
		return GL_INVALID_ENUM;
	}

	if ( count < 0 )
		return GL_INVALID_VALUE;

	/* discard the requested attachments */
	for ( i = 0; i < count; i++ )
	{
		/* map the extension-introduced names to standard attachment names */
		GLenum aname = _gles_resolve_attachment_alias( attachments[ i ] );
		ap_obj = _gles_get_attachment_point( fb_obj, aname );
		if (_gles_framebuffer_set_attachment_discarded( fb_obj, ap_obj, MALI_TRUE ))
			discarded++;
	}


	/* force detach from frame builder, ignore any errors */
	if ( count && discarded > 0 )
	{
		mali_err_code err = _gles_fbo_resolve_completeness_dirty( ctx, fb_obj );
		MALI_IGNORE(err);
	}

	/* if all attachments are discarded, just reset the frame for the epic win */
	if ( _gles_framebuffer_present_attachments( fb_obj ) == fb_obj->discarded_attachments && fb_obj->discarded_attachments != 0 )
	{
		_mali_frame_builder_reset( fb_obj->draw_frame_builder );
	}

	return GL_NO_ERROR;
}

GLenum _gles_fence_flush( struct gles_context* ctx )
{
	GLenum error = GL_NO_ERROR;

	/* if incremental render fails, it means we cannot really complete fence
	 * creation properly. we must not set the error in GL state though, because
	 * this function is not called from any GL entry point, but from EGL */
	if ( MALI_ERR_NO_ERROR != _mali_incremental_render( ctx->default_framebuffer_object->draw_frame_builder ) )
	{
		error = GL_OUT_OF_MEMORY;
	}
	else
	{
		if ( 0 != ctx->state.common.framebuffer.current_object_id )
		{
			error = _gles_flush( ctx, MALI_TRUE );
		}
	}
	return error;
}
#endif
