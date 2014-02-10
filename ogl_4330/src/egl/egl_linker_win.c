/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <winbase.h>
#include <EGL/egl.h>
#include <mali_system.h>
#include "egl_linker.h"
#include "egl_config_extension.h"

#define LINKER_LOAD(lib_handle, func_int, func_ext, cast ) \
do \
{ \
	func_int = (cast)GetProcAddress( lib_handle, TEXT(#func_ext) );\
	if ( NULL == (func_int) )\
	{\
    fprintf( stderr, "Failed to resolve address for %s (error 0x%x)\n", #func_ext, GetLastError() );\
		return EGL_FALSE;\
	}\
	MALI_DEBUG_PRINT( 2, ("  * Loaded %s (%p)\n", #func_ext, func_int));\
} while(0)\

#define LINKER_TRY_LOAD(lib_handle, func_int, func_ext, cast ) \
do \
{ \
	func_int = (cast)GetProcAddress( lib_handle, TEXT(#func_ext) );\
	if ( NULL != (func_int) ) MALI_DEBUG_PRINT( 2, ("* Loaded %s (0x%x)\n", #func_ext, func_int));\
} while(0)\

EGLBoolean egl_linker_init_vg( egl_linker *linker )
{
#ifdef EGL_MALI_VG
	LINKER_LOAD( linker->handle_vg, linker->vg_func.initialize, _vg_initialize, mali_err_code (*)(struct egl_vg_global_data*));
	LINKER_LOAD( linker->handle_vg, linker->vg_func.shutdown, _vg_shutdown, void (*)(struct egl_vg_global_data*));
	LINKER_LOAD( linker->handle_vg, linker->vg_func.is_valid_image_handle, _vg_is_valid_image_handle, VGboolean (*)(struct vg_context*, struct vg_image*) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.get_parent, _vg_get_parent, VGImage (*)(struct vg_context*, VGImage) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.image_lock, _vg_image_lock, VGboolean (*)(VGImage) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.image_unlock, _vg_image_unlock, void (*)(VGImage) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.lock_image_ptrset, _vg_lock_image_ptrset, void (*)(struct vg_context *) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.unlock_image_ptrset, _vg_unlock_image_ptrset, void (*)(struct vg_context *) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.image_ref, _vg_image_ref, void (*)(struct vg_context *, struct vg_image *) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.image_deref, _vg_image_deref, void (*)(struct vg_context *, struct vg_image * ));

	LINKER_LOAD( linker->handle_vg, linker->vg_func.image_pbuffer_to_clientbuffer, _vg_image_pbuffer_to_clientbuffer, VGboolean (*)(VGImage, mali_render_attachment*) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.image_match_egl_config, _vg_image_match_egl_config, VGboolean (*)(VGImage, s32, s32, s32, s32, s32, VGboolean*, VGboolean*, s32*, s32*, mali_pixel_format*, mali_bool*) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.create_global_context, _vg_create_global_context, void* (*)(mali_base_ctx_handle) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.destroy_global_context, _vg_destroy_global_context, void (*)(void*) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.create_context, _vg_create_context, struct vg_context* (*)(mali_base_ctx_handle, struct vg_context*, void*) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.destroy_context, _vg_destroy_context, void (*)(struct vg_context*) );

	LINKER_LOAD( linker->handle_vg, linker->vg_func.context_resize_acquire, _vg_context_resize_acquire, VGboolean (*)(struct vg_context*, u32, u32) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.context_resize_rollback, _vg_context_resize_rollback, void (*)(struct vg_context*, u32, u32) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.context_resize_finish, _vg_context_resize_finish, void (*)(struct vg_context*, u32, u32) );

	LINKER_LOAD( linker->handle_vg, linker->vg_func.make_current, _vg_make_current, VGboolean (*)(struct vg_context*, mali_frame_builder*, mali_egl_surface_type, s32, s32, s32, s32, s32, s32) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.flush, _vg_flush, void (*)(struct vg_context*) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.finish, _vg_finish, void (*)(struct vg_context*) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.get_proc_address, _vg_get_proc_address, void (*(*)(const char*))(void) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.set_frame_builder, _vg_set_frame_builder, VGboolean (*)(struct vg_context *, mali_frame_builder *) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.image_get_mali_surface , _vg_image_get_mali_surface , void (*)(struct vg_context *, struct vg_image * ));
#if EGL_KHR_vg_parent_image_ENABLE
	LINKER_LOAD( linker->handle_vg, linker->vg_func.setup_egl_image, _vg_setup_egl_image, enum egl_image_from_vg_image_status (*)(struct vg_context*, VGImage img, struct egl_image*) );
#endif
	return EGL_TRUE;
#else
	return EGL_FALSE;
#endif
}

EGLBoolean egl_linker_init_gles( egl_linker *linker, EGLint ver )
{
#ifdef EGL_MALI_GLES
	void *lib_handle = NULL;
	EGLint i = 0;
	if ( ver == 1 ) lib_handle = linker->handle_gles1;
	else if ( ver == 2 ) lib_handle = linker->handle_gles2;
	else
	{
		MALI_DEBUG_ASSERT( 0, ("Unknown GLES client version given to egl_linker_init_gles\n") );
		return EGL_FALSE;
	}
	i = ver - 1;

	LINKER_LOAD( lib_handle, linker->gles_func[i].initialize, _gles_initialize, mali_err_code (*)(egl_gles_global_data*) );
	LINKER_LOAD( lib_handle, linker->gles_func[i].shutdown, _gles_shutdown, void (*)(egl_gles_global_data* ) );
	LINKER_LOAD( lib_handle, linker->gles_func[i].make_current, _gles_make_current, mali_err_code (*)(struct gles_context*) );
	LINKER_LOAD( lib_handle, linker->gles_func[i].finish, _gles_finish, GLenum (*)(struct gles_context*) );
	LINKER_LOAD( lib_handle, linker->gles_func[i].flush, _gles_flush, GLenum (*)(struct gles_context*,mali_bool) );
	LINKER_LOAD( lib_handle, linker->gles_func[i].set_draw_frame_builder, _gles_set_draw_frame_builder, mali_err_code (*)(struct gles_context*, mali_frame_builder*, struct egl_gles_surface_capabilities ) );
	LINKER_LOAD( lib_handle, linker->gles_func[i].set_read_frame_builder, _gles_set_read_frame_builder, mali_err_code (*)(struct gles_context*, mali_frame_builder*, mali_egl_surface_type) );
	LINKER_LOAD( lib_handle, linker->gles_func[i].scissor, _gles_scissor, void (*)(struct gles_context *ctx, GLint, GLint, GLsizei, GLsizei) );
#if EXTENSION_EGL_IMAGE_OES_ENABLE
	LINKER_LOAD( lib_handle, linker->gles_func[i].glEGLImageTargetTexture2DOES, glEGLImageTargetTexture2DOES, void (*)(GLenum, GLeglImageOES) );
#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	LINKER_LOAD( lib_handle, linker->gles_func[i].glEGLImageTargetRenderbufferStorageOES, glEGLImageTargetRenderbufferStorageOES, void (*)(GLenum, GLeglImageOES) );
#endif
#endif

	if ( 1 == ver )
	{
		LINKER_LOAD( lib_handle, linker->gles_func[i].create_context, _gles1_create_context, struct gles_context* (*)( mali_base_ctx_handle, struct gles_context*, struct egl_api_shared_function_ptrs *) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].delete_context, _gles1_delete_context, void (*)(struct gles_context*) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].get_proc_address, _gles1_get_proc_address, void (*(*)(const char*))(void) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].bind_tex_image, _gles1_bind_tex_image, void (*)(struct gles_context*, GLenum, GLenum, mali_bool, mali_surface*) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].unbind_tex_image, _gles1_unbind_tex_image, void (*)(struct gles_context* ) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].viewport, _gles1_viewport_for_egl, void (*)(struct gles_context*, GLint, GLint, GLsizei, GLsizei) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].update_viewport_state, _gles1_update_viewport_state_for_egl, void (*)(struct gles_context*) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].copy_tex_image_2d, _gles1_copy_texture_image_2d, void (*)(struct gles_context*, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].get_error, _gles1_get_error, GLenum (*)(struct gles_context *ctx) );

#if EGL_KHR_gl_texture_2D_image_GLES1_ENABLE
		LINKER_LOAD( lib_handle, linker->gles_func[i].setup_egl_image_from_texture, _gles_setup_egl_image_from_texture, enum egl_image_from_gles_surface_status (*)(struct gles_context*, enum egl_image_gles_target, GLuint, u32, struct egl_image*) );
#endif

#if EGL_KHR_gl_renderbuffer_image_GLES1_ENABLE
		LINKER_LOAD( lib_handle, linker->gles_func[i].setup_egl_image_from_renderbuffer, _gles_setup_egl_image_from_renderbuffer, enum egl_image_from_gles_surface_status (*)(struct gles_context*, GLuint, struct egl_image*) );
#endif

	}
	else if ( 2 == ver )
	{
		LINKER_LOAD( lib_handle, linker->gles_func[i].bind_tex_image, _gles2_bind_tex_image, void (*)(struct gles_context*, GLenum, GLenum, mali_bool, mali_surface*) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].unbind_tex_image, _gles2_unbind_tex_image, void (*)(struct gles_context* ) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].create_context, _gles2_create_context, struct gles_context* (*)( mali_base_ctx_handle, struct gles_context*, struct egl_api_shared_function_ptrs *) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].delete_context, _gles2_delete_context, void (*)(struct gles_context*) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].get_proc_address, _gles2_get_proc_address, void (*(*)(const char*))(void) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].viewport, _gles2_viewport_for_egl, void (*)(struct gles_context*, GLint, GLint, GLsizei, GLsizei) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].update_viewport_state, _gles2_update_viewport_state_for_egl, void (*)(struct gles_context*) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].copy_tex_image_2d, _gles2_copy_texture_image_2d, void (*)(struct gles_context*, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].get_error, _gles2_get_error, GLenum (*)(struct gles_context *ctx) );

#if EGL_KHR_gl_texture_2D_image_GLES2_ENABLE
		LINKER_LOAD( lib_handle, linker->gles_func[i].setup_egl_image_from_texture, _gles_setup_egl_image_from_texture, enum egl_image_from_gles_surface_status (*)(struct gles_context*, enum egl_image_gles_target, GLuint, u32, struct egl_image*) );
#endif

#if EGL_KHR_gl_renderbuffer_image_GLES2_ENABLE
		LINKER_LOAD( lib_handle, linker->gles_func[i].setup_egl_image_from_renderbuffer, _gles_setup_egl_image_from_renderbuffer, enum egl_image_from_gles_surface_status (*)(struct gles_context*, GLuint, struct egl_image*) );
#endif
	}

	return EGL_TRUE;
#else
	return EGL_FALSE;
#endif
}

EGLBoolean egl_linker_init( egl_linker *linker )
{
#ifdef EGL_MALI_VG
	linker->handle_vg = LoadLibrary( TEXT("libOpenVG.dll") );
	if ( NULL != linker->handle_vg )
	{
		MALI_DEBUG_PRINT(2, ("* Support for OpenVG found\n"));
		if ( EGL_FALSE == egl_linker_init_vg( linker ) )
		{
			MALI_DEBUG_PRINT(0, ("** Unable to link symbols for libOpenVG.dll\n"));
			return EGL_FALSE;
		}
		else linker->caps |= _EGL_LINKER_OPENVG_BIT;
	}
	else
	{
	    MALI_DEBUG_PRINT(0, ("** Unable to open library libOpenVG.dll\n"));
	}
#endif

#ifdef EGL_MALI_GLES
#if MALI_USE_GLES_1
	linker->handle_gles1 = LoadLibrary( TEXT("libGLESv1_CM.dll") );
	if ( NULL != linker->handle_gles1 )
	{
		MALI_DEBUG_PRINT(2, ("* Support for OpenGL ES 1.x found\n"));
		linker->gles_func[0].get_proc_address = NULL;
		if ( EGL_FALSE == egl_linker_init_gles( linker, 1 ) )
		{
			MALI_DEBUG_PRINT(0, ("** Unable to link symbols for libGLESv1_CM.dll\n"));
			return EGL_FALSE;
		}
		else linker->caps|= _EGL_LINKER_OPENGL_ES_BIT;
	}
	else
	{
	    MALI_DEBUG_PRINT(0, ("** Unable to open library libGLESv1_CM.dll\n"));
	}
#endif /* MALI_USE_GLES_1 */

#if MALI_USE_GLES_2
	linker->handle_gles2 = LoadLibrary( TEXT("libGLESv2.dll") );
	if ( NULL != linker->handle_gles2 )
	{
		MALI_DEBUG_PRINT(2, ("* Support for OpenGL ES 2.x found\n"));
		linker->gles_func[1].get_proc_address = NULL;
		if ( EGL_FALSE == egl_linker_init_gles( linker, 2 ) )
		{
			MALI_DEBUG_PRINT(0, ("** Unable to link symbols for libGLESv2.dll\n"));
			return EGL_FALSE;
		}
		else linker->caps |= _EGL_LINKER_OPENGL_ES2_BIT;
	}
	else
	{
	    MALI_DEBUG_PRINT(0, ("** Unable to open library libGLESv2.dll\n"));
	}
#endif /* MALI_USE_GLES_2 */
#endif /* EGL_MALI_GLES */

	MALI_DEBUG_PRINT(2, ("* Capabilities=0x%x\n", linker->caps));

	return EGL_TRUE;
}

void egl_linker_deinit( egl_linker *linker )
{
	if ( NULL == linker ) return;

	if ( NULL != linker->handle_vg )
	{
		FreeLibrary( linker->handle_vg );
		linker->handle_vg = NULL;
	}
	if ( NULL != linker->handle_gles1 )
	{
		FreeLibrary( linker->handle_gles1 );
		linker->handle_gles1 = NULL;
	}
	if ( NULL != linker->handle_gles2 )
	{
		FreeLibrary( linker->handle_gles2 );
		linker->handle_gles2 = NULL;
	}
}

