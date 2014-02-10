/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <EGL/egl.h>
#include <mali_system.h>
#include "egl_linker.h"
#include <dlfcn.h>
#include "egl_config_extension.h"

EGLBoolean egl_linker_init_vg( egl_linker *linker )
{
#ifdef EGL_MALI_VG

	linker->vg_func.initialize = _vg_initialize;
	linker->vg_func.shutdown = _vg_shutdown;
	linker->vg_func.is_valid_image_handle = _vg_is_valid_image_handle;
	linker->vg_func.get_parent = _vg_get_parent;
	linker->vg_func.image_lock = _vg_image_lock;
	linker->vg_func.image_unlock = _vg_image_unlock;
	linker->vg_func.lock_image_ptrset = _vg_lock_image_ptrset;
	linker->vg_func.unlock_image_ptrset = _vg_unlock_image_ptrset;
	linker->vg_func.image_ref = _vg_image_ref;
	linker->vg_func.image_deref = _vg_image_deref;
	linker->vg_func.image_pbuffer_to_clientbuffer = _vg_image_pbuffer_to_clientbuffer;
	linker->vg_func.image_match_egl_config = _vg_image_match_egl_config;
	linker->vg_func.create_global_context = _vg_create_global_context;
	linker->vg_func.destroy_global_context = _vg_destroy_global_context;
	linker->vg_func.create_context = _vg_create_context;
	linker->vg_func.destroy_context = _vg_destroy_context;
	linker->vg_func.context_resize_acquire = _vg_context_resize_acquire;
	linker->vg_func.context_resize_rollback = _vg_context_resize_rollback;
	linker->vg_func.context_resize_finish = _vg_context_resize_finish;
	linker->vg_func.make_current = _vg_make_current;
	linker->vg_func.flush = _vg_flush;
	linker->vg_func.finish= _vg_finish;
	linker->vg_func.get_proc_address = _vg_get_proc_address;
	linker->vg_func.set_frame_builder = _vg_set_frame_builder;
	linker->vg_func.image_get_mali_surface = _vg_image_get_mali_surface;
#if EGL_KHR_vg_parent_image_ENABLE
	linker->vg_func.setup_egl_image = _vg_setup_egl_image;
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

	linker->gles_func[i].initialize = _gles_initialize;
	linker->gles_func[i].shutdown = _gles_shutdown;
	linker->gles_func[i].make_current = _gles_make_current;
	linker->gles_func[i].finish = _gles_finish;
	linker->gles_func[i].flush = _gles_flush;
	linker->gles_func[i].set_draw_frame_builder = _gles_set_draw_frame_builder;
	linker->gles_func[i].set_read_frame_builder = _gles_set_read_frame_builder;
	linker->gles_func[i].scissor = _gles_scissor;
#if EXTENSION_EGL_IMAGE_OES_ENABLE
	linker->gles_func[i].glEGLImageTargetTexture2DOES = glEGLImageTargetTexture2DOES;
#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	linker->gles_func[i].glEGLImageTargetRenderbufferStorageOES = glEGLImageTargetRenderbufferStorageOES;
#endif
#endif

	if ( ver == 1 )
	{
		linker->gles_func[i].create_context = _gles1_create_context;
		linker->gles_func[i].delete_context = _gles1_delete_context;
		linker->gles_func[i].get_proc_address = _gles1_get_proc_address;
		linker->gles_func[i].viewport = _gles1_viewport_for_egl;
		linker->gles_func[i].update_viewport_state = _gles1_update_viewport_state_for_egl;
		linker->gles_func[i].copy_tex_image_2d = _gles1_copy_texture_image_2d;
		linker->gles_func[i].get_error = _gles1_get_error;

#if EGL_KHR_gl_texture_2D_image_GLES1_ENABLE
		linker->gles_func[i].setup_egl_image_from_texture = _gles_setup_egl_image_from_texture;
#endif

#if EGL_KHR_gl_renderbuffer_image_GLES1_ENABLE
		linker->gles_func[i].setup_egl_image_from_renderbuffer = _gles_setup_egl_image_from_renderbuffer;
#endif
	}
	else if ( ver == 2 )
	{
		linker->gles_func[i].create_context = _gles2_create_context;
		linker->gles_func[i].delete_context = _gles2_delete_context;
		linker->gles_func[i].get_proc_address = _gles2_get_proc_address;
		linker->gles_func[i].viewport = _gles2_viewport_for_egl;
		linker->gles_func[i].update_viewport_state = _gles2_update_viewport_state_for_egl;
		linker->gles_func[i].copy_tex_image_2d = _gles2_copy_texture_image_2d;
		linker->gles_func[i].get_error = _gles2_get_error;

#if EGL_KHR_gl_texture_2D_image_GLES2_ENABLE
		linker->gles_func[i].setup_egl_image_from_texture = _gles_setup_egl_image_from_texture;
#endif

#if EGL_KHR_gl_renderbuffer_image_GLES2_ENABLE
		linker->gles_func[i].setup_egl_image_from_renderbuffer = _gles_setup_egl_image_from_renderbuffer;
#endif
	}

	return EGL_TRUE;
#else
	return EGL_FALSE;
#endif
}

EGLBoolean egl_linker_init_shared( egl_linker *linker )
{
	return EGL_TRUE;
}


EGLBoolean egl_linker_init( egl_linker *linker )
{
#ifdef EGL_MALI_VG
	MALI_DEBUG_PRINT(2, ("* Support for OpenVG found\n"));
	if ( EGL_FALSE == egl_linker_init_vg( linker ) )
	{
		MALI_DEBUG_PRINT(2, ("** Unable to link symbols\n"));
		return EGL_FALSE;
	}
	else linker->caps |= _EGL_LINKER_OPENVG_BIT;
#endif

#ifdef EGL_MALI_GLES
#if MALI_USE_GLES_1
	MALI_DEBUG_PRINT(2, ("* Support for OpenGL ES 1.x found\n"));
	linker->gles_func[0].get_proc_address = NULL;
	if ( EGL_FALSE == egl_linker_init_gles( linker, 1 ) )
	{
		MALI_DEBUG_PRINT(2, ("** Unable to link symbols\n"));
		return EGL_FALSE;
	}
	else linker->caps |= _EGL_LINKER_OPENGL_ES_BIT;
#endif /* MALI_USE_GLES_1 */

#if MALI_USE_GLES_2
	MALI_DEBUG_PRINT(2, ("* Support for OpenGL ES 2.x found\n"));
	linker->gles_func[1].get_proc_address = NULL;
	if ( EGL_FALSE == egl_linker_init_gles( linker, 2 ) )
	{
		MALI_DEBUG_PRINT(2, ("** Unable to link symbols\n"));
		return EGL_FALSE;
	}
	else linker->caps |= _EGL_LINKER_OPENGL_ES2_BIT;
#endif /* MALI_USE_GLES_2 */
#endif /* EGL_MALI_GLES */

	if ( EGL_FALSE == egl_linker_init_shared(linker) )
	{
		MALI_DEBUG_PRINT(2, ("** Unable to link symbols\n"));
		return EGL_FALSE;
	}

	MALI_DEBUG_PRINT(2, ("* caps = 0x%x\n", linker->caps));

	return EGL_TRUE;
}

void egl_linker_deinit( egl_linker *linker )
{
}
