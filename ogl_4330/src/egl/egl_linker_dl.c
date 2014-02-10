/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <mali_system.h>
#include <EGL/mali_egl.h>
#include <dlfcn.h>
#include "egl_linker.h"
#include "egl_config_extension.h"


#ifndef MALI_BUILD_ANDROID_MONOLITHIC
#define MALI_GLES_NAME_WRAP(a) a
#else
#define MALI_GLES_NAME_WRAP(a) shim_##a
#endif


#ifdef MALI_MONOLITHIC
/* Here we attempt to fill in the function table using refrences to the funcs
 * This would allow us not to export the names needed in a dsym lookup */

#ifdef EGL_MALI_GLES
extern mali_err_code		_gles_initialize				(egl_gles_global_data*)	;
extern void			_gles_shutdown					(egl_gles_global_data* );
extern mali_err_code		_gles_make_current 				(struct gles_context*)	;
extern GLenum			_gles_finish 					(struct gles_context*)	;
extern GLenum			_gles_flush 					(struct gles_context*,mali_bool)	;
extern mali_err_code		_gles_set_draw_frame_builder			(struct gles_context*, mali_frame_builder*, struct egl_gles_surface_capabilities);
extern mali_err_code		_gles_set_read_frame_builder			(struct gles_context*, mali_frame_builder*, mali_egl_surface_type);
extern GLenum			_gles_scissor 					(struct gles_context*, GLint, GLint, GLsizei, GLsizei);

extern void				MALI_GLES_NAME_WRAP(glEGLImageTargetTexture2DOES)		(GLenum, GLeglImageOES);
extern void				MALI_GLES_NAME_WRAP(glEGLImageTargetRenderbufferStorageOES)	(GLenum, GLeglImageOES);

extern enum egl_image_from_gles_surface_status _gles_setup_egl_image_from_texture(struct gles_context*, enum egl_image_gles_target, GLuint, u32, struct egl_image*) ;
extern enum egl_image_from_gles_surface_status  _gles_setup_egl_image_from_renderbuffer(struct gles_context*, GLuint, struct egl_image*)		;

extern struct gles_context*		_gles1_create_context			( mali_base_ctx_handle, struct gles_context*, egl_api_shared_function_ptrs *, int robust_access) ;
extern void				_gles1_delete_context			(struct gles_context*) ;
extern void (*				_gles1_get_proc_address			(const char*))() ;
extern GLenum				_gles1_bind_tex_image			(struct gles_context*, GLenum, GLenum, mali_bool, mali_surface*, void**) ;
extern void				_gles1_unbind_tex_image			(struct gles_context*, void* ) ;
extern GLenum				_gles1_viewport_for_egl			(struct gles_context*, GLint, GLint, GLsizei, GLsizei) ;
extern void				_gles1_update_viewport_state_for_egl(struct gles_context*);
extern GLenum				_gles1_copy_texture_image_2d		(struct gles_context*, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint) ;
extern GLenum				_gles1_get_error			(struct gles_context*) ;

extern 	GLenum				_gles2_bind_tex_image			(struct gles_context*, GLenum, GLenum, mali_bool, mali_surface*, void**) ;
extern 	void				_gles2_unbind_tex_image 		(struct gles_context*, void* ) ;
extern 	struct gles_context*		_gles2_create_context			( mali_base_ctx_handle, struct gles_context*, egl_api_shared_function_ptrs *, int robust_access) ;
extern 	void				_gles2_delete_context 			(struct gles_context*) ;
extern 	void (*				_gles2_get_proc_address 		(const char*))() ;
extern 	GLenum				_gles2_viewport_for_egl 		(struct gles_context*, GLint, GLint, GLsizei, GLsizei) ;
extern void					_gles2_update_viewport_state_for_egl(struct gles_context*);
extern 	GLenum				_gles2_copy_texture_image_2d		(struct gles_context*, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint) ;
extern 	GLenum				_gles2_get_error			(struct gles_context*) ;
extern GLenum 				_gles_fence_flush			( struct gles_context* );

#endif

#ifdef EGL_MALI_VG

extern mali_err_code		_vg_initialize 						(struct egl_vg_global_data*);
extern void			_vg_shutdown 						(struct egl_vg_global_data*);
extern VGboolean		_vg_is_valid_image_handle 				(struct vg_context*, struct vg_image*) ;
extern VGImage			_vg_get_parent 						(struct vg_context*, VGImage) ;
extern VGboolean		_vg_image_lock 						(VGImage) ;
extern void			_vg_image_unlock 					(VGImage) ;
extern void			_vg_lock_image_ptrset					(struct vg_context *) ;
extern void			_vg_unlock_image_ptrset					(struct vg_context *) ;
extern void			_vg_image_ref						(struct vg_context *, struct vg_image *) ;
extern void			_vg_image_deref						(struct vg_context *, struct vg_image * );

extern VGboolean			_vg_image_pbuffer_to_clientbuffer	 	(VGImage, mali_surface*) ;
extern VGboolean			_vg_image_match_egl_config 			(VGImage, s32, s32, s32, s32, s32, VGboolean*, VGboolean*, s32*, s32*, mali_pixel_format*) ;
extern void*				_vg_create_global_context 			(mali_base_ctx_handle) ;
extern void				_vg_destroy_global_context 			(void*) ;
extern struct vg_context*		_vg_create_context 				(mali_base_ctx_handle, struct vg_context*, void*) ;
extern void				_vg_destroy_context 				(struct vg_context*) ;

extern VGboolean			_vg_context_resize_acquire 			(struct vg_context*, u32, u32) ;
extern void				_vg_context_resize_rollback	 		(struct vg_context*, u32, u32) ;
extern void				_vg_context_resize_finish 			(struct vg_context*, u32, u32) ;

extern VGboolean			_vg_make_current 				(struct vg_context*, mali_frame_builder*, mali_egl_surface_type, s32, s32, s32, s32, s32, s32) ;
extern void				_vg_flush					(struct vg_context*) ;
extern void				_vg_finish					(struct vg_context*) ;
extern void		(*		_vg_get_proc_address				(const char*))() ;
extern VGboolean			_vg_set_frame_builder 				(struct vg_context *, mali_frame_builder *) ;

extern struct mali_surface*		_vg_image_get_mali_surface 			(struct vg_context *, struct vg_image * );

extern enum egl_image_from_vg_image_status _vg_setup_egl_image 				(struct vg_context*, VGImage img, struct egl_image*) ;

#endif

#define LINKER_LOAD(lib_handle, func_int, func_ext, cast ) \
do \
{ \
	func_int = func_ext ;\
	if ( func_int == NULL )\
	{\
		MALI_DEBUG_PRINT( 0, ("not found ") );\
		return EGL_FALSE;\
	}\
	MALI_DEBUG_PRINT( 2, ("  * Loaded %s (%p)\n", #func_ext, func_int));\
} while(0)\

#define LINKER_TRY_LOAD(lib_handle, func_int, func_ext, cast ) \
do \
{ \
	func_int = func_ext ;\
	if ( func_int != NULL )\
	{\
		MALI_DEBUG_PRINT( 2, ("  * Loaded %s (%p)\n", #func_ext, func_int));\
	}\
} while(0)\


#else /* not MALI_MONOLITHIC */

/* macro using libdl to load a given symbol, given
 * a handle to the library, the internal function to assign
 * the symbol to, the external symbol to load, and
 * the correct cast to use
 */
#define LINKER_LOAD(lib_handle, func_int, func_ext, cast ) \
do \
{ \
	char *error = NULL;\
	func_int = (cast)dlsym( lib_handle, #func_ext );\
	if ( ( error = (char*)dlerror() ) != NULL )\
	{\
		MALI_DEBUG_PRINT( 0, ("%s\n", error) );\
		return EGL_FALSE;\
	}\
	MALI_DEBUG_PRINT( 2, ("  * Loaded %s (%p)\n", #func_ext, func_int));\
	MALI_IGNORE(error);\
} while(0)\

/* works the same way as the macro above, except that it
 * will allow the symbol to be nonexisting
 */
#define LINKER_TRY_LOAD(lib_handle, func_int, func_ext, cast ) \
do \
{ \
	func_int = (cast)dlsym( lib_handle, #func_ext );\
	if ( NULL != dlerror()) MALI_DEBUG_PRINT( 2, ("* Loaded %s (%p)\n", #func_ext, func_int));\
} while(0)\

#endif /* MALI_MONOLITHIC */

EGLBoolean egl_linker_init_vg( egl_linker *linker )
{
#ifdef EGL_MALI_VG
	LINKER_LOAD( linker->handle_vg, linker->vg_func.initialize,				_vg_initialize, 			mali_err_code (*)(struct egl_vg_global_data*));
	LINKER_LOAD( linker->handle_vg, linker->vg_func.shutdown,				_vg_shutdown, 				void (*)(struct egl_vg_global_data*));
	LINKER_LOAD( linker->handle_vg, linker->vg_func.is_valid_image_handle, 			_vg_is_valid_image_handle, 		VGboolean (*)(struct vg_context*, struct vg_image*) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.get_parent, 				_vg_get_parent, 			VGImage (*)(struct vg_context*, VGImage) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.image_lock, 				_vg_image_lock, 			VGboolean (*)(VGImage) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.image_unlock, 				_vg_image_unlock, 			void (*)(VGImage) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.lock_image_ptrset, 			_vg_lock_image_ptrset, 			void (*)(struct vg_context *) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.unlock_image_ptrset, 			_vg_unlock_image_ptrset, 		void (*)(struct vg_context *) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.image_ref, 				_vg_image_ref, 				void (*)(struct vg_context *, struct vg_image *) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.image_deref, 				_vg_image_deref, 			void (*)(struct vg_context *, struct vg_image * ));
	LINKER_LOAD( linker->handle_vg, linker->vg_func.image_pbuffer_to_clientbuffer,	 	_vg_image_pbuffer_to_clientbuffer,	VGboolean (*)(VGImage, mali_surface*) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.image_match_egl_config, 		_vg_image_match_egl_config, 		VGboolean (*)(VGImage, s32, s32, s32, s32, s32, VGboolean*, VGboolean*, s32*, s32*, mali_pixel_format*) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.create_global_context, 			_vg_create_global_context, 		void* (*)(mali_base_ctx_handle) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.destroy_global_context, 		_vg_destroy_global_context, 		void (*)(void*) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.create_context, 			_vg_create_context, 			struct vg_context* (*)(mali_base_ctx_handle, struct vg_context*, void*) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.destroy_context, 			_vg_destroy_context, 			void (*)(struct vg_context*) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.context_resize_acquire, 		_vg_context_resize_acquire, 		VGboolean (*)(struct vg_context*, u32, u32) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.context_resize_rollback, 		_vg_context_resize_rollback, 		void (*)(struct vg_context*, u32, u32) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.context_resize_finish, 			_vg_context_resize_finish, 		void (*)(struct vg_context*, u32, u32) );

	LINKER_LOAD( linker->handle_vg, linker->vg_func.make_current, 				_vg_make_current, 			VGboolean (*)(struct vg_context*, mali_frame_builder*, mali_egl_surface_type, s32, s32, s32, s32, s32, s32) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.flush, 					_vg_flush, 				void (*)(struct vg_context*) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.finish, 				_vg_finish, 				void (*)(struct vg_context*) );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.get_proc_address, 			_vg_get_proc_address, 			void (*(*)(const char*))() );
	LINKER_LOAD( linker->handle_vg, linker->vg_func.set_frame_builder, 			_vg_set_frame_builder, 			VGboolean (*)(struct vg_context *, mali_frame_builder *) );

	LINKER_LOAD( linker->handle_vg, linker->vg_func.image_get_mali_surface, 		_vg_image_get_mali_surface,	 	struct mali_surface* (*)(struct vg_context *, struct vg_image * ));
#if EGL_KHR_vg_parent_image_ENABLE
	LINKER_LOAD( linker->handle_vg, linker->vg_func.setup_egl_image, 			_vg_setup_egl_image, 			enum egl_image_from_vg_image_status (*)(struct vg_context*, VGImage img, struct egl_image*) );
#endif
	return EGL_TRUE;
#else
	return EGL_FALSE;
#endif
}

EGLBoolean egl_linker_init_gles( egl_linker *linker, EGLint ver )
{
#ifdef EGL_MALI_GLES
	EGLint i = 0;
#ifndef MALI_MONOLITHIC
	void *lib_handle = NULL;
	if ( ver == 1 ) lib_handle = linker->handle_gles1;
	else if ( ver == 2 ) lib_handle = linker->handle_gles2;
#else
	if ( ver == 1 ||  ver == 2 );
#endif
	else
	{
		MALI_DEBUG_ASSERT( 0, ("Unknown GLES client version given to egl_linker_init_gles\n") );
		return EGL_FALSE;
	}
	i = ver - 1;

	LINKER_LOAD( lib_handle, linker->gles_func[i].initialize, 								_gles_initialize, 					mali_err_code (*)(egl_gles_global_data*) );
	LINKER_LOAD( lib_handle, linker->gles_func[i].shutdown, 								_gles_shutdown, 					void (*)(egl_gles_global_data* ) );
	LINKER_LOAD( lib_handle, linker->gles_func[i].make_current, 							_gles_make_current, 				mali_err_code (*)(struct gles_context*) );
	LINKER_LOAD( lib_handle, linker->gles_func[i].finish, 									_gles_finish, 						GLenum (*)(struct gles_context*) );
	LINKER_LOAD( lib_handle, linker->gles_func[i].flush, 									_gles_flush, 						GLenum (*)(struct gles_context*, mali_bool) );
	LINKER_LOAD( lib_handle, linker->gles_func[i].set_draw_frame_builder, 					_gles_set_draw_frame_builder, 		mali_err_code (*)(struct gles_context*, mali_frame_builder*, struct egl_gles_surface_capabilities) );
	LINKER_LOAD( lib_handle, linker->gles_func[i].set_read_frame_builder, 					_gles_set_read_frame_builder,		mali_err_code (*)(struct gles_context*, mali_frame_builder*, mali_egl_surface_type) );
	LINKER_LOAD( lib_handle, linker->gles_func[i].scissor, 									_gles_scissor, 						GLenum (*)(struct gles_context*, GLint, GLint, GLsizei, GLsizei) );
	LINKER_LOAD( lib_handle, linker->gles_func[i].fence_flush, 								_gles_fence_flush,	 				GLenum (*)(struct gles_context* ));
#if EXTENSION_EGL_IMAGE_OES_ENABLE
#ifndef MALI_BUILD_ANDROID_MONOLITHIC
	LINKER_LOAD( lib_handle, linker->gles_func[i].glEGLImageTargetTexture2DOES, 	        glEGLImageTargetTexture2DOES,		void (*)(GLenum, GLeglImageOES) );
#else
	LINKER_LOAD( lib_handle, linker->gles_func[i].glEGLImageTargetTexture2DOES, 	        shim_glEGLImageTargetTexture2DOES,		void (*)(GLenum, GLeglImageOES) );
#endif
#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
#ifndef MALI_BUILD_ANDROID_MONOLITHIC
	LINKER_LOAD( lib_handle, linker->gles_func[i].glEGLImageTargetRenderbufferStorageOES, 	glEGLImageTargetRenderbufferStorageOES,    void (*)(GLenum, GLeglImageOES) );
#else
	LINKER_LOAD( lib_handle, linker->gles_func[i].glEGLImageTargetRenderbufferStorageOES, 	shim_glEGLImageTargetRenderbufferStorageOES,    void (*)(GLenum, GLeglImageOES) );
#endif
#endif
#endif
#if !defined MALI_MONOLITHIC || defined MALI_USE_GLES_1
	if ( 1 == ver )
	{
		LINKER_LOAD( lib_handle, linker->gles_func[i].create_context, 				_gles1_create_context,			struct gles_context* (*)( mali_base_ctx_handle, struct gles_context*, egl_api_shared_function_ptrs *, int ) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].delete_context, 				_gles1_delete_context,			void (*)(struct gles_context*) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].get_proc_address, 			_gles1_get_proc_address,		void (*(*)(const char*))() );
		LINKER_LOAD( lib_handle, linker->gles_func[i].bind_tex_image, 				_gles1_bind_tex_image,			GLenum (*)(struct gles_context*, GLenum, GLenum, mali_bool, mali_surface*, void**) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].unbind_tex_image, 			_gles1_unbind_tex_image,		void (*)(struct gles_context*, void* ) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].viewport, 					_gles1_viewport_for_egl,		GLenum (*)(struct gles_context*, GLint, GLint, GLsizei, GLsizei) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].update_viewport_state, 		_gles1_update_viewport_state_for_egl,	void (*)(struct gles_context*) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].copy_tex_image_2d, 			_gles1_copy_texture_image_2d,	GLenum (*)(struct gles_context*, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].get_error, 					_gles1_get_error,				GLenum (*)(struct gles_context*) );

#if EGL_KHR_gl_texture_2D_image_GLES1_ENABLE
		LINKER_LOAD( lib_handle, linker->gles_func[i].setup_egl_image_from_texture, _gles_setup_egl_image_from_texture, enum egl_image_from_gles_surface_status (*)(struct gles_context*, enum egl_image_gles_target, GLuint, u32, struct egl_image*) );
#endif

#if EGL_KHR_gl_renderbuffer_image_GLES1_ENABLE
		LINKER_LOAD( lib_handle, linker->gles_func[i].setup_egl_image_from_renderbuffer, _gles_setup_egl_image_from_renderbuffer, enum egl_image_from_gles_surface_status (*)(struct gles_context*, GLuint, struct egl_image*) );
#endif

	}
#endif
#if !defined MALI_MONOLITHIC || defined MALI_USE_GLES_2
	if ( 2 == ver )
	{
		LINKER_LOAD( lib_handle, linker->gles_func[i].bind_tex_image, 				_gles2_bind_tex_image, 				GLenum (*)(struct gles_context*, GLenum, GLenum, mali_bool, mali_surface*, void**) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].unbind_tex_image, 			_gles2_unbind_tex_image, 			void (*)(struct gles_context*, void* ) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].create_context, 				_gles2_create_context, 				struct gles_context* (*)( mali_base_ctx_handle, struct gles_context*, egl_api_shared_function_ptrs *, int ) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].delete_context, 				_gles2_delete_context, 				void (*)(struct gles_context*) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].get_proc_address, 			_gles2_get_proc_address, 			void (*(*)(const char*))() );
		LINKER_LOAD( lib_handle, linker->gles_func[i].viewport, 					_gles2_viewport_for_egl, 			GLenum (*)(struct gles_context*, GLint, GLint, GLsizei, GLsizei) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].update_viewport_state, 		_gles2_update_viewport_state_for_egl,	void (*)(struct gles_context*) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].copy_tex_image_2d, 			_gles2_copy_texture_image_2d, 		GLenum (*)(struct gles_context*, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint) );
		LINKER_LOAD( lib_handle, linker->gles_func[i].get_error, 					_gles2_get_error, 					GLenum (*)(struct gles_context*) );

#if EGL_KHR_gl_texture_2D_image_GLES2_ENABLE
		LINKER_LOAD( lib_handle, linker->gles_func[i].setup_egl_image_from_texture, _gles_setup_egl_image_from_texture, enum egl_image_from_gles_surface_status (*)(struct gles_context*, enum egl_image_gles_target, GLuint, u32, struct egl_image*) );
#endif

#if EGL_KHR_gl_renderbuffer_image_GLES2_ENABLE
		LINKER_LOAD( lib_handle, linker->gles_func[i].setup_egl_image_from_renderbuffer, _gles_setup_egl_image_from_renderbuffer, enum egl_image_from_gles_surface_status (*)(struct gles_context*, GLuint, struct egl_image*) );
#endif
	}
#endif
	return EGL_TRUE;
#else
	return EGL_FALSE;
#endif
}

EGLBoolean egl_linker_init_shared( egl_linker *linker )
{
	MALI_IGNORE( linker );
	return EGL_TRUE;
}

#ifdef MALI_MONOLITHIC
#ifdef EGL_MALI_VG
EGLBoolean egl_linker_verify_monolithic_vg( egl_linker *linker, void *handle_monolithic )
{
	egl_vg_api_functions vg_monolithic;

	LINKER_LOAD( handle_monolithic, vg_monolithic.finish, _vg_finish, void (*)(struct vg_context*) );
	if ( vg_monolithic.finish != linker->vg_func.finish )
	{
		#ifdef NEXELL_FEATURE_ENABLE
		MALI_DEBUG_PRINT( 0, ("OpenVG library functions differ - are you sure all libraries are symbolic links of libVR?\n") );
		#else
		MALI_DEBUG_PRINT( 0, ("OpenVG library functions differ - are you sure all libraries are symbolic links of libMali?\n") );
		#endif
		return EGL_FALSE;
	}

	return EGL_TRUE;
}
#endif /* EGL_MALI_VG */

#ifdef EGL_MALI_GLES
EGLBoolean egl_linker_verify_monolithic_gles( egl_linker *linker, void *handle_monolithic, EGLint ver )
{
	egl_gles_api_functions gles_monolithic;

	LINKER_LOAD( handle_monolithic, gles_monolithic.finish, _gles_finish, GLenum (*)(struct gles_context*) );
	if ( gles_monolithic.finish != linker->gles_func[ver-1].finish )
	{
		#ifdef NEXELL_FEATURE_ENABLE
		MALI_DEBUG_PRINT( 0, ("OpenGL ES %i.x library functions differ - are you sure all libraries are symbolic links of libVR?\n", ver) );
		#else
		MALI_DEBUG_PRINT( 0, ("OpenGL ES %i.x library functions differ - are you sure all libraries are symbolic links of libMali?\n", ver) );
		#endif
		return EGL_FALSE;
	}

	return EGL_TRUE;
}
#endif /* EGL_MALI_GLES */

EGLBoolean egl_linker_verify_monolithic( egl_linker *linker )
{
	void *handle_monolithic;
#ifdef NEXELL_FEATURE_ENABLE
	handle_monolithic = dlopen("libVR.so", RTLD_LAZY );
	(void)dlerror(); /* drop error messages */

	if ( NULL == handle_monolithic )
	{
		MALI_DEBUG_PRINT( 0, ("Failed to open libVR\n"));
		return EGL_FALSE;
	}
#else
	handle_monolithic = dlopen("libMali.so", RTLD_LAZY );
	(void)dlerror(); /* drop error messages */

	if ( NULL == handle_monolithic )
	{
		MALI_DEBUG_PRINT( 0, ("Failed to open libMali\n"));
		return EGL_FALSE;
	}
#endif

#ifdef EGL_MALI_VG
	if ( linker->caps & _EGL_LINKER_OPENVG_BIT )
	{
		if ( EGL_FALSE == egl_linker_verify_monolithic_vg( linker, handle_monolithic ) )
		{
			dlclose( handle_monolithic );
			return EGL_FALSE;
		}
	}
#endif /* EGL_MALI_VG */

#ifdef EGL_MALI_GLES
	if ( linker->caps & _EGL_LINKER_OPENGL_ES_BIT )
	{
		if ( EGL_FALSE == egl_linker_verify_monolithic_gles( linker, handle_monolithic, 1 ) )
		{
			dlclose( handle_monolithic );
			return EGL_FALSE;
		}
	}

	if ( linker->caps & _EGL_LINKER_OPENGL_ES2_BIT )
	{
		if ( EGL_FALSE == egl_linker_verify_monolithic_gles( linker, handle_monolithic, 2 ) )
		{
			dlclose( handle_monolithic );
			return EGL_FALSE;
		}
	}
#endif /* EGL_MALI_GLES */

	dlclose( handle_monolithic );

	return EGL_TRUE;
}
#endif /* MALI_MONOLITHIC */

EGLBoolean egl_linker_init( egl_linker *linker )
{
#ifdef EGL_MALI_VG
	linker->handle_vg = dlopen("libOpenVG.so", RTLD_LAZY );
	(void)dlerror(); /* drop error messages */
	if ( NULL != linker->handle_vg )
	{
		MALI_DEBUG_PRINT( 2, ("* Support for OpenVG found\n"));
		if ( EGL_FALSE == egl_linker_init_vg( linker ) )
		{
			MALI_DEBUG_PRINT( 0, ("** Unable to link symbols for libOpenVG.so\n"));
			return EGL_FALSE;
		}
		else linker->caps |= _EGL_LINKER_OPENVG_BIT;
	}
	else
	{
	    MALI_DEBUG_PRINT( 0, ("** Unable to open library libOpenVG.so\n"));
	}
#endif

#ifdef EGL_MALI_GLES
#if MALI_USE_GLES_1
#ifdef HAVE_ANDROID_OS
#ifdef NEXELL_FEATURE_ENABLE
	linker->handle_gles1 = dlopen("/system/lib/egl/libGLESv1_CM_vr.so", RTLD_LAZY );
#else
	linker->handle_gles1 = dlopen("/system/lib/egl/libGLESv1_CM_mali.so", RTLD_LAZY );
#endif
#else
	linker->handle_gles1 = dlopen("libGLESv1_CM.so", RTLD_LAZY );
#endif
	(void)dlerror(); /* drop error messages */
	if ( NULL != linker->handle_gles1 )
	{
		MALI_DEBUG_PRINT( 2, ("* Support for OpenGL ES 1.x found\n"));
		if ( EGL_FALSE == egl_linker_init_gles( linker, 1 ) )
		{
			MALI_DEBUG_PRINT( 0, ("** Unable to link symbols for libGLESv1_CM.so\n"));
			return EGL_FALSE;
		}
		else linker->caps |= _EGL_LINKER_OPENGL_ES_BIT;
	}
	else
	{
	    MALI_DEBUG_PRINT( 0, ("** Unable to open library libGLESv1_CM.so\n"));
	}
#endif /* MALI_USE_GLES_1 */

#if MALI_USE_GLES_2
#ifdef HAVE_ANDROID_OS
#ifdef NEXELL_FEATURE_ENABLE
	linker->handle_gles2 = dlopen("/system/lib/egl/libGLESv2_vr.so", RTLD_LAZY );
#else
	linker->handle_gles2 = dlopen("/system/lib/egl/libGLESv2_mali.so", RTLD_LAZY );
#endif
#else
	linker->handle_gles2 = dlopen("libGLESv2.so", RTLD_LAZY );
#endif
	(void)dlerror(); /* drop error messages */
	if ( NULL != linker->handle_gles2 )
	{
		MALI_DEBUG_PRINT( 2, ("* Support for OpenGL ES 2.x found\n"));
		if ( EGL_FALSE == egl_linker_init_gles( linker, 2 ) )
		{
			MALI_DEBUG_PRINT(0, ("** Unable to link symbols for libGLESv2.so\n"));
			return EGL_FALSE;
		}
		else linker->caps |= _EGL_LINKER_OPENGL_ES2_BIT;
	}
    else
    {
        MALI_DEBUG_PRINT( 0, ("** Unable to open library libGLESv2.so\n"));
    }
#endif /* MALI_USE_GLES_2 */
#endif /* EGL_MALI_GLES */

#ifdef MALI_MONOLITHIC
	if ( EGL_FALSE == egl_linker_verify_monolithic( linker ) )
	{
		return EGL_FALSE;
	}
#endif /* MALI_MONOLITHIC */

#ifdef NEXELL_FEATURE_ENABLE
	linker->handle_shared = dlopen("libVR.so", RTLD_LAZY);
	if ( NULL != linker->handle_shared )
	{
		MALI_DEBUG_PRINT( 2, ("* libVR.so loaded\n"));
		if ( EGL_FALSE == egl_linker_init_shared(linker) )
		{
			MALI_DEBUG_PRINT( 0, ("** Unable to link symbols for shared\n") );
			return EGL_FALSE;
		}
	}
	else
	{
		MALI_DEBUG_PRINT( 0, ("* Could not load libVR.so\n"));
	}
#else
	linker->handle_shared = dlopen("libMali.so", RTLD_LAZY);
	if ( NULL != linker->handle_shared )
	{
		MALI_DEBUG_PRINT( 2, ("* libMali.so loaded\n"));
		if ( EGL_FALSE == egl_linker_init_shared(linker) )
		{
			MALI_DEBUG_PRINT( 0, ("** Unable to link symbols for shared\n") );
			return EGL_FALSE;
		}
	}
	else
	{
		MALI_DEBUG_PRINT( 0, ("* Could not load libMali.so\n"));
	}
#endif

	MALI_DEBUG_PRINT(2, ("* caps = 0x%x\n", linker->caps));

	return EGL_TRUE;
}

void egl_linker_deinit( egl_linker *linker )
{
	if ( NULL == linker ) return;

	if ( NULL != linker->handle_vg )
	{
		dlclose( linker->handle_vg );
		linker->handle_vg = NULL;
	}

	if ( NULL != linker->handle_gles1 )
	{
		dlclose( linker->handle_gles1 );
		linker->handle_gles1 = NULL;
	}

	if ( NULL != linker->handle_gles2 )
	{
		dlclose( linker->handle_gles2 );
		linker->handle_gles2 = NULL;
	}

	if ( NULL != linker->handle_shared )
	{
		dlclose( linker->handle_shared );
		linker->handle_shared = NULL;
	}
}
