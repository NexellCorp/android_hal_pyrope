/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */


#include "gles_renderbuffer_object.h"
#include "gles_framebuffer_object.h"
#include "gles_util.h"
#include "gles_object.h"
#include "gles_config_extension.h"

#include "gles_share_lists.h"
#include "egl/egl_image.h"
#include "shared/mali_surface.h"
#include "shared/egl_image_status.h"
#include "gles_fbo_bindings.h"
#include "gles_convert.h"
#include "egl/api_interface_egl.h"

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
/******************************* STATE FUNCTIONS *****************************/

/**
 * Initialises the render buffer state members
 */
void _gles_renderbuffer_state_init( gles_renderbuffer_state* rb_state )
{
	MALI_DEBUG_ASSERT_POINTER( rb_state );

	/* set all members */
	rb_state->current_object = NULL; /* FBO 0 is bound */
	rb_state->current_object_id = 0; /* FBO 0 is bound */

}

/*********************** RENDERBUFFER OBJECT FUNCTIONS ************************/

/**
 * Deallocates a renderbuffer object
 * @param rb_obj the renderbuffer object to deallocate.
 * @note Make sure that the object is completely dereferenced prior to calling this function.
 *       Typically, you want to call _gles_renderbuffer_object_deref() instead, which
 *       handles the dereferencing and calls this function if refcount reaches zero.
 */
MALI_STATIC void _gles_renderbuffer_object_delete( gles_renderbuffer_object* rb_obj )
{
	/* handle NULL object silently */
	if( NULL == rb_obj ) return;


	MALI_DEBUG_ASSERT( _mali_sys_atomic_get( &rb_obj->ref_count ) == 0, ("At this point, there should be no more references on this object!"));

	if ( rb_obj->fbo_connection != NULL ) _gles_fbo_bindings_free( rb_obj->fbo_connection );
	rb_obj->fbo_connection = NULL;

	if ( rb_obj->render_target != NULL )
	{
		_mali_surface_deref( rb_obj->render_target );
		rb_obj->render_target = NULL;
	}

	_mali_sys_free( rb_obj );
	rb_obj = NULL;

}

/**
 * Allocates and returns a new renderbuffer object
 * @return A fresh new allocated renderbuffer object, or NULL if out of memory
 */
MALI_STATIC gles_renderbuffer_object* _gles_renderbuffer_object_new( void )
{
	gles_renderbuffer_object* retval = NULL;

	/* Allocate object */
	retval = _mali_sys_malloc( sizeof(struct gles_renderbuffer_object) );
	MALI_CHECK( NULL != retval, NULL );

	/* for safety, nil the entire object */
	_mali_sys_memset(retval, 0, sizeof(struct gles_renderbuffer_object) );

	/* set all members */
	_mali_sys_atomic_initialize(&retval->ref_count, 1); /* we obviously have one reference to this object - the function that just created it */
	retval->internalformat = GL_RGBA4;
	retval->width = 0;
	retval->height = 0;
	retval->fsaa_samples = 0;
	retval->render_target = NULL;
	retval->is_egl_image_sibling = MALI_FALSE;
	retval->fbo_connection = _gles_fbo_bindings_alloc( );

	if ( retval->fbo_connection == NULL )
	{
		_gles_renderbuffer_object_deref( retval );

		return NULL;
	}

	return retval;
}

MALI_STATIC mali_err_code gles_texel_format_from_renderbuffer_format(m200_texel_format pixel_format, GLenum *gles_format)
{
	MALI_DEBUG_ASSERT_POINTER( gles_format );

	switch(pixel_format)
	{
		case M200_TEXEL_FORMAT_RGB_565:		*gles_format = GL_RGB565;	break;
		case M200_TEXEL_FORMAT_ARGB_4444:	*gles_format = GL_RGBA4;	break;
		case M200_TEXEL_FORMAT_ARGB_1555:	*gles_format = GL_RGB5_A1;	break;
		case M200_TEXEL_FORMAT_ARGB_8888:	*gles_format = GL_RGBA8_OES;	break;
#if !RGB_IS_XRGB
		case M200_TEXEL_FORMAT_RGB_888:		*gles_format = GL_RGB8_OES;	break;
#endif
		case M200_TEXEL_FORMAT_xRGB_8888:	*gles_format = GL_RGB8_OES;	break;
		default:				 return MALI_ERR_FUNCTION_FAILED;
	}

	return MALI_ERR_NO_ERROR;
}

/**
 * @brief converts a renderbuffer pixel size format (ex, GL_RGBA4) into a mali pixel format (ex, MALI_PIXEL_FORMAT_A4R4G4B4)
 * @return a mali pixel format
 */
MALI_STATIC mali_pixel_format gles_renderbuffer_format_to_pixel_format(GLenum glformat)
{
	switch(glformat)
	{
		case GL_RGB565:			return  MALI_PIXEL_FORMAT_R5G6B5;
		case GL_RGB:			return MALI_PIXEL_FORMAT_A8R8G8B8;
		case GL_RGBA4:			return MALI_PIXEL_FORMAT_A4R4G4B4;
		case GL_RGB5_A1:		return MALI_PIXEL_FORMAT_A1R5G5B5;
#if EXTENSION_RGB8_RGBA8_ENABLE
		case GL_RGB8_OES:		return MALI_PIXEL_FORMAT_A8R8G8B8;
#endif
#if EXTENSION_ARM_RGBA8_ENABLE || EXTENSION_RGB8_RGBA8_ENABLE
		case GL_RGBA8_OES:		return MALI_PIXEL_FORMAT_A8R8G8B8;
#endif
		case GL_DEPTH_COMPONENT16:	return MALI_PIXEL_FORMAT_S8Z24;
		case GL_DEPTH_COMPONENT24_OES:	return MALI_PIXEL_FORMAT_S8Z24;
		case GL_STENCIL_INDEX4_OES:	return MALI_PIXEL_FORMAT_S8Z24;
		case GL_STENCIL_INDEX8:		return MALI_PIXEL_FORMAT_S8Z24;
#if EXTENSION_PACKED_DEPTH_STENCIL_ENABLE
		case GL_DEPTH24_STENCIL8_OES:	return MALI_PIXEL_FORMAT_S8Z24;
#endif
		default:
			MALI_DEBUG_ASSERT( MALI_FALSE, ("Illegal renderbuffer format detected - %d.", glformat));
			return MALI_PIXEL_FORMAT_COUNT; /* illegal value, legal enum - will never happen, but keeps some compilers happy*/
	}
}

/**
 * @brief converts a renderbuffer pixel size format (ex, GL_RGBA4) into a mali texel format (ex, MALI_TEXEL_FORMAT_ARGB_4444)
 * @return a mali pixel format
 */
MALI_STATIC enum m200_texel_format gles_renderbuffer_format_to_texel_format(GLenum glformat)
{
	switch(glformat)
	{
		case GL_RGB565:			return M200_TEXEL_FORMAT_RGB_565;
		case GL_RGBA4:			return M200_TEXEL_FORMAT_ARGB_4444;
		case GL_RGB5_A1:		return M200_TEXEL_FORMAT_ARGB_1555;
#if EXTENSION_RGB8_RGBA8_ENABLE
		case GL_RGB8_OES:		return M200_TEXEL_FORMAT_xRGB_8888;
#endif
#if EXTENSION_ARM_RGBA8_ENABLE || EXTENSION_RGB8_RGBA8_ENABLE
		case GL_RGBA8_OES:		return M200_TEXEL_FORMAT_ARGB_8888;
#endif
		case GL_DEPTH_COMPONENT16:	return M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8;
		case GL_DEPTH_COMPONENT24_OES:	return M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8;
		case GL_STENCIL_INDEX4_OES:	return M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8;
		case GL_STENCIL_INDEX8:		return M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8;
#if EXTENSION_PACKED_DEPTH_STENCIL_ENABLE
		case GL_DEPTH24_STENCIL8_OES:	return M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8;
#endif
		default:
			MALI_DEBUG_ASSERT( MALI_FALSE, ("Illegal renderbuffer format detected - %d.", glformat));
			return M200_TEXEL_FORMAT_NO_TEXTURE; /* illegal value, legal enum - will never happen, but keeps some compilers happy*/
	}
}

/**
 * Delete the render buffer object from the list
 */
void _gles_renderbuffer_object_list_entry_delete(struct gles_wrapper *wrapper)
{
	MALI_DEBUG_ASSERT_POINTER( wrapper );

	if ( NULL != wrapper->ptr.rbo )
	{
		/* dereference renderbuffer object */
		_gles_renderbuffer_object_deref(wrapper->ptr.rbo);
		wrapper->ptr.rbo = NULL;
	}

	/* delete wrapper */
	_gles_wrapper_free( wrapper );
	wrapper = NULL;

}


/**
 * @brief Decrement reference count for a renderbuffer object, and delete the object if zero.
 * @param rb_obj The pointer to the renderbuffer-object we want to dereference.
 */
void _gles_renderbuffer_object_deref(gles_renderbuffer_object *rb_obj)
{
	u32 ref_count = 0;
	MALI_DEBUG_ASSERT_POINTER( rb_obj );
	MALI_DEBUG_ASSERT((_mali_sys_atomic_get( &rb_obj->ref_count ) > 0 ), ("ref_count on renderbuffer object is already 0, the object should have been deleted by now!\n"));

	/* dereference */
	ref_count = _mali_sys_atomic_dec_and_return( &rb_obj->ref_count );
	if (ref_count > 0) return;

	_gles_renderbuffer_object_delete( rb_obj );
	rb_obj = NULL;

}

GLenum _gles_delete_renderbuffers( struct mali_named_list *renderbuffer_object_list,
		struct gles_renderbuffer_state *rb_state,
		struct gles_framebuffer_state *fb_state,
		GLsizei n, const GLuint *renderbuffers )
{
	GLsizei i;
	mali_err_code mali_err = MALI_ERR_NO_ERROR;
	GLenum gl_err = GL_NO_ERROR;
	MALI_DEBUG_ASSERT_POINTER( rb_state );
	MALI_DEBUG_ASSERT_POINTER( fb_state );
	MALI_DEBUG_ASSERT_POINTER( renderbuffer_object_list );


	/* if n < 0, set error to GL_INVALID_VALUE */
	if( n < 0 ) return GL_INVALID_VALUE;

	/* The GLES specification does not specify an error code for this. At least we avoid crashing */
	if( NULL == renderbuffers ) return GL_NO_ERROR;

	/* delete all given objects by dereferencing them, then deleting the wrapper */
	for( i = 0; i < n; i++ )
	{
		struct gles_wrapper* name_wrapper;
		GLuint name = renderbuffers[ i ];

		/* silently ignore name 0 */
		if(name == 0) continue;

		/* get the renderbuffer object */
		name_wrapper = __mali_named_list_get( renderbuffer_object_list, name );
		if (NULL == name_wrapper) continue; /* name not found, silently ignore */

		if (NULL != name_wrapper->ptr.rbo)
		{
			/* remove all bindings from this renderbuffer by....
			 * 1: unbind it from the bound renderbuffer state - if bound
			 * 2: unattach it from the active framebuffer (wherever it is attached) - if attached.
			 * 3: remove it from the renderbuffer list
			 */

			/* 1 - unbind from active state */
			if( rb_state->current_object == name_wrapper->ptr.rbo ) _gles_internal_bind_renderbuffer(rb_state, NULL, 0);

			/* 2 - unattach it from any attached points on the active framebuffer. */
			mali_err = _gles_internal_purge_renderbuffer_from_framebuffer(fb_state->current_object, name_wrapper->ptr.rbo );
			if( gl_err == GL_NO_ERROR ) gl_err = _gles_convert_mali_err(mali_err);

			/* 3 - delete the program object list entry */
			_gles_renderbuffer_object_deref(name_wrapper->ptr.rbo);
			name_wrapper->ptr.rbo = NULL;
		}

		/* 3 continued - delete the wrapper */
		__mali_named_list_remove(renderbuffer_object_list, name);
		_gles_wrapper_free( name_wrapper );
		name_wrapper = NULL;
	}

	return gl_err;
}

/**
 * Binds the render buffer to the new object
 * and increases the reference count
 */
void _gles_internal_bind_renderbuffer( gles_renderbuffer_state* rb_state, gles_renderbuffer_object* rb_obj, GLuint rb_obj_id )
{
	MALI_DEBUG_ASSERT_POINTER( rb_state );

	/* dereference old bound object */
	if(rb_state->current_object != NULL) _gles_renderbuffer_object_deref(rb_state->current_object);

	/* change bound object */
	rb_state->current_object = rb_obj;
	rb_state->current_object_id = rb_obj_id;

	/* increase reference count of new binding */
	if(rb_state->current_object != NULL) _gles_renderbuffer_object_addref(rb_state->current_object);
}

MALI_STATIC void renderbuffer_update_state( gles_renderbuffer_state *rb_state, GLenum internalformat, GLsizei width, GLsizei height, mali_bool is_sibling, GLsizei samples )
{
	MALI_DEBUG_ASSERT_POINTER( rb_state );

	/* modify active renderbuffer */
	rb_state->current_object->internalformat       = internalformat;
	rb_state->current_object->width                = width;
	rb_state->current_object->height               = height;
	rb_state->current_object->is_egl_image_sibling = is_sibling;
	rb_state->current_object->fsaa_samples         = samples;

	switch(internalformat)
	{
#if EXTENSION_ARM_RGBA8_ENABLE || EXTENSION_RGB8_RGBA8_ENABLE
		case GL_RGBA8_OES:
			rb_state->current_object->red_bits = 8;
			rb_state->current_object->green_bits = 8;
			rb_state->current_object->blue_bits = 8;
			rb_state->current_object->alpha_bits = 8;
			break;
#endif
#if EXTENSION_RGB8_RGBA8_ENABLE
		case GL_RGB8_OES:
			rb_state->current_object->red_bits = 8;
			rb_state->current_object->green_bits = 8;
			rb_state->current_object->blue_bits = 8;
			rb_state->current_object->alpha_bits = 0;
			break;
#endif
		case GL_RGB5_A1:
			rb_state->current_object->red_bits = 5;
			rb_state->current_object->green_bits = 5;
			rb_state->current_object->blue_bits = 5;
			rb_state->current_object->alpha_bits = 1;
			break;
		case GL_RGB565:
			rb_state->current_object->red_bits = 5;
			rb_state->current_object->green_bits = 6;
			rb_state->current_object->blue_bits = 5;
			rb_state->current_object->alpha_bits = 0;
			break;
		case GL_RGBA4:
			rb_state->current_object->red_bits = 4;
			rb_state->current_object->green_bits = 4;
			rb_state->current_object->blue_bits = 4;
			rb_state->current_object->alpha_bits = 4;
			break;
		default:
			rb_state->current_object->red_bits = 0;
			rb_state->current_object->green_bits = 0;
			rb_state->current_object->blue_bits = 0;
			rb_state->current_object->alpha_bits = 0;
			break;
	}
	switch(internalformat)
	{
		/* According to the OpenGL ES 2.0 specification (6.1.3) glGetRenderbufferParameter 
		 * must return the actual number of bits provided, which is 24 for GL_DEPTH_COMPONENT16 
		 * since it uses M200_TEXEL_FORMAT_DEPTH_STENCIL_24_8
		 */
		case GL_DEPTH_COMPONENT16: rb_state->current_object->depth_bits = 24; break;
#if EXTENSION_DEPTH24_OES_ENABLE
		case GL_DEPTH_COMPONENT24_OES: rb_state->current_object->depth_bits = 24; break;
#endif
#if EXTENSION_PACKED_DEPTH_STENCIL_ENABLE
		case GL_DEPTH24_STENCIL8_OES: rb_state->current_object->depth_bits = 24; break;
#endif
		default: rb_state->current_object->depth_bits = 0; break;
	}
	switch(internalformat)
	{
		case GL_STENCIL_INDEX8: rb_state->current_object->stencil_bits = 8; break;
#if EXTENSION_PACKED_DEPTH_STENCIL_ENABLE
		case GL_DEPTH24_STENCIL8_OES: rb_state->current_object->stencil_bits = 8; break;
#endif
		default: rb_state->current_object->stencil_bits = 0; break;
	}
}

MALI_STATIC GLenum renderbuffer_check_valid( gles_renderbuffer_state *rb_state, GLenum target, GLenum internalformat, GLsizei width, GLsizei height )
{
	/* a list of legal formats */
	const GLenum legal_formats[] =
	{
		GL_RGB565,
		GL_RGBA4,
		GL_RGB5_A1,
		GL_DEPTH_COMPONENT16,
		GL_STENCIL_INDEX8,
#if EXTENSION_ARM_RGBA8_ENABLE || EXTENSION_RGB8_RGBA8_ENABLE
		GL_RGBA8_OES,
#endif
#if EXTENSION_RGB8_RGBA8_ENABLE
		GL_RGB8_OES,
#endif
#if EXTENSION_DEPTH24_OES_ENABLE
		GL_DEPTH_COMPONENT24_OES,
#endif
#if EXTENSION_PACKED_DEPTH_STENCIL_ENABLE
		GL_DEPTH24_STENCIL8_OES,
#endif
	};

	MALI_DEBUG_ASSERT_POINTER( rb_state );

	/* if we're working on object 0, we cannot alter anything */
	if( NULL == rb_state->current_object ) return GL_INVALID_OPERATION;

	/* Target MUST be GL_RENDERBUFFER */
	if( target != GL_RENDERBUFFER ) return GL_INVALID_ENUM;

	/* test whether internalformat is one of the legal formats */
	if( ! _gles_verify_enum( legal_formats, MALI_ARRAY_SIZE(legal_formats), internalformat ) ) return GL_INVALID_ENUM;

	/* assert width/height dimensions */
	if(width  < 0) return GL_INVALID_VALUE;
	if(height < 0) return GL_INVALID_VALUE;
	if(width  > GLES_MAX_RENDERBUFFER_SIZE) return GL_INVALID_VALUE;
	if(height > GLES_MAX_RENDERBUFFER_SIZE) return GL_INVALID_VALUE;

	return GL_NO_ERROR;
}

/**
 * Maintains the render buffer objects list
 */
GLenum _gles_renderbuffer_storage(
		mali_base_ctx_handle base_ctx,
		mali_named_list *renderbuffer_object_list,
		gles_renderbuffer_state *rb_state,
		GLenum target,
		GLenum internalformat,
		GLsizei width,
		GLsizei height )
{
	return _gles_renderbuffer_storage_multisample(base_ctx, renderbuffer_object_list, rb_state, target, 0, internalformat, width, height);
}

/**
 * Maintains the render buffer objects list
 */
GLenum _gles_renderbuffer_storage_multisample(
                                mali_base_ctx_handle base_ctx,
                                struct mali_named_list *renderbuffer_object_list,
                                struct gles_renderbuffer_state *rb_state,
                                GLenum target,
                                GLsizei samples,
                                GLenum internalformat,
                                GLsizei width,
                                GLsizei height )
{
	GLenum err = GL_NO_ERROR;
	mali_err_code mali_err = MALI_ERR_NO_ERROR;
	mali_surface_specifier sformat;
	mali_surface *surface = NULL;

	MALI_DEBUG_ASSERT_POINTER( renderbuffer_object_list );
	MALI_DEBUG_ASSERT_POINTER( rb_state );

	/* check that the state and input parameters are valid */
	err = renderbuffer_check_valid( rb_state, target, internalformat, width, height );
#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
	MALI_CHECK((samples<=GLES_MAX_FBO_SAMPLES), GL_INVALID_VALUE);
#endif
	if ( err != GL_NO_ERROR )
	{
		return err;
	}

	/* allocate new render target */
	_mali_surface_specifier_ex(
			&sformat,
			width,
			height,
			0, 
			gles_renderbuffer_format_to_pixel_format(internalformat),
			gles_renderbuffer_format_to_texel_format(internalformat),
			MALI_PIXEL_LAYOUT_INTERLEAVED_BLOCKS, 
			M200_TEXTURE_ADDRESSING_MODE_16X16_BLOCKED,
			MALI_FALSE, 
			MALI_FALSE,
			MALI_SURFACE_COLORSPACE_sRGB, 
			MALI_SURFACE_ALPHAFORMAT_NONPRE, 
			MALI_FALSE );

	surface = _mali_surface_alloc( MALI_SURFACE_FLAGS_NONE, &sformat, 0, base_ctx);
	if( NULL == surface ) return GL_OUT_OF_MEMORY;

	/* remove old render target if existing */
	if( NULL != rb_state->current_object->render_target )
	{
		/* deref this surface from this RBO */
		_mali_surface_deref( rb_state->current_object->render_target );
	}

	rb_state->current_object->render_target = surface;

#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
	renderbuffer_update_state( rb_state, internalformat, width, height, MALI_FALSE, GLES_NEXT_SUPPORTED_FBO_SAMPLES(samples) );
#else
	renderbuffer_update_state( rb_state, internalformat, width, height, MALI_FALSE, 0 );
#endif

	_gles_fbo_bindings_surface_changed( rb_state->current_object->fbo_connection );

	return _gles_convert_mali_err(mali_err);
}

GLenum _gles_bind_renderbuffer( struct mali_named_list *renderbuffer_object_list,
		struct gles_renderbuffer_state *rb_state,
		GLenum target, GLuint name)
{
	struct gles_wrapper* name_wrapper = NULL;

	MALI_DEBUG_ASSERT_POINTER(rb_state);
	MALI_DEBUG_ASSERT_POINTER(renderbuffer_object_list);

	/* target MUST be GL_RENDERBUFFER */
	if(target != GL_RENDERBUFFER) return GL_INVALID_ENUM;

	/* Bind default renderbuffer. Detach current framebuffer and return */
	if( 0 == name )
	{
		_gles_internal_bind_renderbuffer(rb_state, NULL, name);
		return GL_NO_ERROR;
	}

	/* get object from list */
	name_wrapper = __mali_named_list_get( renderbuffer_object_list, name );
	if (NULL == name_wrapper)
	{
		mali_err_code error_code;
		/* didn't exist. This means we have to create the entry */
		name_wrapper = _gles_wrapper_alloc( WRAPPER_RENDERBUFFER );
		MALI_CHECK_NON_NULL( name_wrapper, GL_OUT_OF_MEMORY );
		name_wrapper->ptr.rbo = NULL;

		/* Aaaand insert wrapper into list */
		error_code = __mali_named_list_insert(renderbuffer_object_list, name, name_wrapper);
		if( error_code != MALI_ERR_NO_ERROR )
		{
			_gles_wrapper_free( name_wrapper );
			name_wrapper = NULL;
			return GL_OUT_OF_MEMORY;
		}
	}

	/* maybe we need to create the object from the wrapper */
	if(name_wrapper->ptr.rbo == NULL)
	{
		name_wrapper->ptr.rbo = _gles_renderbuffer_object_new();
		MALI_CHECK_NON_NULL( name_wrapper->ptr.rbo, GL_OUT_OF_MEMORY );
	}


	/* all right. we found the object we were looking for. Set it */
	_gles_internal_bind_renderbuffer(rb_state, name_wrapper->ptr.rbo, name);

	return GL_NO_ERROR;
}

/**
 * Function returns render buffer paraments
 */
GLenum _gles_get_renderbuffer_parameter( gles_renderbuffer_state *rb_state, GLenum target, GLenum pname, GLint *params )
{
	GLenum state = GL_NO_ERROR;
#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
	int temp_addr = 0;
#endif
	MALI_DEBUG_ASSERT_POINTER(rb_state);

	/* target must be GL_RENDERBUFFER */
	if(target != GL_RENDERBUFFER) return GL_INVALID_ENUM;

	/* renderbuffer 0 is not legal to lookup */
	MALI_CHECK_NON_NULL( rb_state->current_object, GL_INVALID_OPERATION );

	switch(pname)
	{
		case GL_RENDERBUFFER_WIDTH:           if (NULL != params) *params = rb_state->current_object->width; break;
		case GL_RENDERBUFFER_HEIGHT:          if (NULL != params) *params = rb_state->current_object->height; break;
		case GL_RENDERBUFFER_INTERNAL_FORMAT: if (NULL != params) *params = rb_state->current_object->internalformat; break;
		case GL_RENDERBUFFER_RED_SIZE:        if (NULL != params) *params = rb_state->current_object->red_bits; break;
		case GL_RENDERBUFFER_GREEN_SIZE:      if (NULL != params) *params = rb_state->current_object->green_bits; break;
		case GL_RENDERBUFFER_BLUE_SIZE:       if (NULL != params) *params = rb_state->current_object->blue_bits; break;
		case GL_RENDERBUFFER_ALPHA_SIZE:      if (NULL != params) *params = rb_state->current_object->alpha_bits; break;
		case GL_RENDERBUFFER_DEPTH_SIZE:      if (NULL != params) *params = rb_state->current_object->depth_bits; break;
		case GL_RENDERBUFFER_STENCIL_SIZE:    if (NULL != params) *params = rb_state->current_object->stencil_bits; break;
#if EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE
		case MALI_GL_RENDERBUFFER_SAMPLES_EXT_V1:   /* fall-through */
		case MALI_GL_RENDERBUFFER_SAMPLES_EXT_V2:     
			if (NULL != params) *params = rb_state->current_object->fsaa_samples; 
			break;
#endif
		default: state = GL_INVALID_ENUM; break;
	}

	return state;
}

#if EXTENSION_EGL_IMAGE_OES_ENABLE
GLenum _gles_egl_image_target_renderbuffer_storage(
		struct gles_context *ctx,
		GLenum target,
		GLeglImageOES image)
{
	egl_image *img                              = NULL;
	mali_image *image_mali                      = NULL;
	mali_surface *surface                       = NULL;
	gles_renderbuffer_state *rb_state;
	GLenum internalformat                       = GL_RGB565;
	mali_err_code malierr                       = MALI_ERR_NO_ERROR;
	egl_api_shared_function_ptrs *egl_funcptrs;

	if( target != GL_RENDERBUFFER) return GL_INVALID_ENUM;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->share_lists );

	egl_funcptrs = (egl_api_shared_function_ptrs*)ctx->egl_funcptrs;

	MALI_DEBUG_ASSERT_POINTER( egl_funcptrs );
	MALI_DEBUG_ASSERT_POINTER( egl_funcptrs->get_eglimage_ptr );

	rb_state = &ctx->state.common.renderbuffer;

	MALI_DEBUG_ASSERT_POINTER( rb_state );

	img = egl_funcptrs->get_eglimage_ptr( (EGLImageKHR)image, 0 );
	MALI_CHECK_NON_NULL( img, GL_INVALID_VALUE );

	image_mali = img->image_mali;
	MALI_CHECK_NON_NULL( image_mali, GL_INVALID_OPERATION );

	surface = image_mali->pixel_buffer[0][0];
	MALI_CHECK_NON_NULL( surface, GL_INVALID_OPERATION );
	MALI_CHECK_NON_NULL( surface->mem_ref, GL_INVALID_OPERATION );

	MALI_CHECK( surface->format.pixel_format != MALI_PIXEL_FORMAT_NONE , GL_INVALID_OPERATION );

	/* convert EGL format to renderbuffer format */
	malierr = gles_texel_format_from_renderbuffer_format( surface->format.texel_format, &internalformat );
	if ( malierr != MALI_ERR_NO_ERROR ) return GL_INVALID_OPERATION;

	/* size must be within limits */
	if(surface->format.width  > GLES_MAX_RENDERBUFFER_SIZE || surface->format.height > GLES_MAX_RENDERBUFFER_SIZE) return GL_INVALID_OPERATION;

	/* if we're working on object 0, we cannot alter anything */
	MALI_CHECK_NON_NULL( rb_state->current_object, GL_INVALID_OPERATION );
	if ( rb_state->current_object->render_target != NULL )
	{
		_mali_surface_deref( rb_state->current_object->render_target );
		rb_state->current_object->render_target = NULL;
	}

	rb_state->current_object->render_target = surface;
	_mali_surface_addref(surface);

	renderbuffer_update_state( rb_state, internalformat, surface->format.width, surface->format.height, MALI_TRUE, 0 );

	_gles_fbo_bindings_surface_changed( rb_state->current_object->fbo_connection );

	return _gles_convert_mali_err(malierr);
}

MALI_INTER_MODULE_API enum egl_image_from_gles_surface_status _gles_setup_egl_image_from_renderbuffer( struct gles_context *ctx, GLuint name, struct egl_image *image )
{
	gles_renderbuffer_object *rbo;
	mali_named_list *renderbuffer_object_list;
	struct gles_wrapper* name_wrapper;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(image);

	/* should result in a EGL_BAD_PARAMETER since it is disallowed to create EGLimages from the default object */
	if ( 0 == name ) return GLES_SURFACE_TO_EGL_IMAGE_STATUS_OBJECT_RESERVED;

	renderbuffer_object_list = ctx->share_lists->renderbuffer_object_list;
	MALI_DEBUG_ASSERT_POINTER(renderbuffer_object_list);

	name_wrapper = __mali_named_list_get( renderbuffer_object_list, name );
	MALI_CHECK_NON_NULL( name_wrapper, GLES_SURFACE_TO_EGL_IMAGE_STATUS_OBJECT_UNAVAILABLE );
	MALI_CHECK_NON_NULL( name_wrapper->ptr.rbo, GLES_SURFACE_TO_EGL_IMAGE_STATUS_OBJECT_UNAVAILABLE );

	/* rbo is know to be non-NULL now */
	rbo = name_wrapper->ptr.rbo;
	MALI_CHECK( MALI_TRUE != rbo->is_egl_image_sibling, GLES_SURFACE_TO_EGL_IMAGE_STATUS_ALREADY_SIBLING );

	MALI_CHECK_NON_NULL( rbo->render_target, GLES_SURFACE_TO_EGL_IMAGE_STATUS_OBJECT_INCOMPLETE );

	/* fill in data */
	{
		mali_surface *render_target = rbo->render_target;

		MALI_DEBUG_ASSERT_POINTER(render_target);

		image->image_mali = mali_image_create_from_surface( render_target, ctx->base_ctx );
		MALI_CHECK_NON_NULL( image->image_mali, GLES_SURFACE_TO_EGL_IMAGE_STATUS_OOM );

		_mali_surface_addref( image->image_mali->pixel_buffer[0][0] );

		/* the renderbuffer will now remain an EGLImage sibling until the next call to glRenderbufferStorage or glEGLImageTargetRenderbufferStorageOES */
		rbo->is_egl_image_sibling = MALI_TRUE;
	}

	return GLES_SURFACE_TO_EGL_IMAGE_STATUS_NO_ERROR;
}
#endif /* EXTENSION_EGL_IMAGE_OES_ENABLE */
#endif /* EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE */
