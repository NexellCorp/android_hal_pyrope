/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2005, 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_platform.h
 * @brief EGL platform abstraction layer.
 */

#ifndef _EGL_PLATFORM_H_
#define _EGL_PLATFORM_H_

/** @defgroup egl_platform_api EGL platform API
 * @{ */

#include <EGL/mali_egl.h>
#include <base/mali_types.h>
#include <base/mali_context.h>
#include <base/mali_macros.h>
#include <base/mali_context.h>
#include <shared/mali_named_list.h>
#include <shared/mali_surface_specifier.h>
#include "egl_surface.h"
#include "egl_display.h"
#include "egl_context.h"
#include "egl_image.h"

/* Forward declaration of the mali_surface struct */
struct mali_surface;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Native platform initializer.
 * @note This is called once when the main context in egl is initialized.
 * @return EGL_TRUE on success, EGL_FALSE otherwise.
 */
EGLBoolean __egl_platform_initialize( void ) MALI_CHECK_RESULT;

/**
 * @brief Native platform terminator.
 * @note This is called once, when the main context in egl is destroyed.
 */
void __egl_platform_terminate( void );

/**
 * @brief Native platform power event.
 * @param event_data A void pointer to custom event data.
 * @note This can be set up as a callback for any custom power management event.
 * @note Has to call __egl_main_power_event( event ) in order to invalidate contexts.
 * @deprecated
 */
void __egl_platform_power_event( void *event_data );

/**
 * @brief Returns the default native display.
 * @return A native display handle.
 * @note Called by EGL core as part of @ref eglGetDisplay.
 */
EGLNativeDisplayType __egl_platform_default_display( void ) MALI_CHECK_RESULT;

/**
 * @brief Checks is a native display is valid.
 * @param dpy A native display handle.
 * @return EGL_TRUE for a valid display. EGL_FALSE otherwise.
 */
EGLBoolean __egl_platform_display_valid( EGLNativeDisplayType dpy ) MALI_CHECK_RESULT;

/**
 * @brief Sets the orientation of a native display.
 * @param dpy A native display to set orientation for.
 * @param orientation accepted values are EGL_NATIVE_DISPLAY_ROTATE_0,
 * EGL_NATIVE_DISPLAY_ROTATE_90, EGL_NATIVE_DISPLAY_ROTATE_180,
 * EGL_NATIVE_DISPLAY_ROTATE_270, EGL_NATIVE_DISPLAY_FLIP_X,
 * EGL_NATIVE_DISPLAY_FLIP_Y.
 * @deprecated
 */
void __egl_platform_set_display_orientation( EGLNativeDisplayType dpy, EGLint orientation );

/**
 * @brief Retrieves a native display orientation.
 * @param dpy A native display to retrieve orientation for.
 * @return An orientation specifier.
 * @deprecated
 */
EGLint __egl_platform_get_display_orientation( EGLNativeDisplayType dpy );

/**
 * @brief Initializes a native display type.
 * @param dpy The native display type to initialize.
 * @param base_ctx A handle to a mali base context.
 * @return EGL_TRUE on success, EGL_FALSE otherwise.
 * @note Called by EGL core as part of @ref eglInitialize.
 */
EGLBoolean __egl_platform_init_display( EGLNativeDisplayType dpy, mali_base_ctx_handle base_ctx ) MALI_CHECK_RESULT;

#if EGL_BACKEND_X11 || HAVE_ANDROID_OS
/**
 * @brief Filters the list of configs attached to a given display.
 * @param dpy A pointer to egl_display to filter configs for.
 *
 * Depending on your specific platform backend, you might
 * have a need to filter the EGL configs, based on supported display bit depths etc.
 * Typically this filtering either involves removing the config altogether, or marking it as slow if color conversion is required.
 * Called as part of @ref eglInitialize when a new display is initialized.
 *
 * @note Optional to implement.
 */
void __egl_platform_filter_configs( egl_display *dpy );

/**
 * @brief Flushes a given native display.
 * @param dpy The native display type to flush.
 * @note This can be used whenever asynchroneous native platform operations has to complete.
 *
 * Called as part of @ref eglCreateWindowSurface, @ref eglCreatePixmapSurface and @ref eglCreatePbufferFromClientBuffer to synchronize the native display.
 */
void __egl_platform_flush_display( EGLNativeDisplayType dpy );
#endif

/**
 * @brief Deinitialize a native display.
 * @param dpy The native display type to deinitialize.
 * @note Called as part of @ref eglTerminate to deinitialize any native resources initialized in @ref __egl_platform_init_display.
 */
void __egl_platform_deinit_display( EGLNativeDisplayType dpy );

/**
 * @brief Retrieves information about the native display color format.
 * @param dpy A native display handle.
 * @param format A native display format struct.
 * @note This is used to retrieve information about color component sizes and offsets for the native display.
 * @see egl_display_native_format
 */
void __egl_platform_display_get_format( EGLNativeDisplayType dpy, egl_display_native_format *format );

/**
 * @brief Waits for any native rendering engine operations to complete.
 * @param engine Identifier for the native rendering engine to wait for.
 * @return EGL_TRUE on success, EGL_FALSE otherwise.
 * @note Called as part of @ref eglWaitNative. Only EGL_CORE_NATIVE_ENGINE supported as native engine identifier.
 */
EGLBoolean __egl_platform_wait_native( EGLint engine ) MALI_CHECK_RESULT;

/**
 * @brief Creates the platform specific parts of an egl surface.
 * @param surface Pointer to an @ref egl_surface structure.
 * @param base_ctx A mali base context handle.
 * @return EGL_TRUE on success, EGL_FALSE otherwise.
 *
 * This is function is intended to create platform specific parts of an egl_surface. This also includes creating the framebuilder.\n
 * Any type of EGL surface can be created, including \em MALI_EGL_WINDOW_SURFACE, \em MALI_EGL_PIXMAP_SURFACE and \em MALI_EGL_PBUFFER_SURFACE.\n
 *
 * If <em>surface->lock_surface != NULL</em>, the surface is a lockable surface, and a number of attributes should be updated.\n
 *
 * <em>surface->lock_surface->bitmap_origin</em> must be set to one of the following:
 * \li \em EGL_UPPER_LEFT_KHR if the first byte of the mapped surface pointer corresponds to the upper left corner of the surface
 * \li \em EGL_LOWER_LEFT_KHR if the first byte of the mapped surface pointer corresponds to the lower left corner of the surface
 *
 * <em>surface->lock_surface->bitmap_pixmap</em>:
 * \li <em>surface->lock_surface->bitmap_pixel_<red,green,blue,alpha,luminance>_offset</em> must be set to the offset
 * corresponding corresponding color component is shifted by, in the native surface pixel format.
 *
 * @note See one of the supported platform backends for detailed examples.
 */
EGLBoolean __egl_platform_create_surface( egl_surface *surface, mali_base_ctx_handle base_ctx ) MALI_CHECK_RESULT;

/**
 * @brief Destroys the platform-specific parts of an egl_surface.
 * @param surface Pointer to the egl surface to destroy.
 * @note Called as part of @ref eglDestroySurface. Intended to clean up what was previously allocated in @ref __egl_platform_create_surface.
 */
void __egl_platform_destroy_surface( egl_surface *surface );

/**
 * @brief Resizes the platform-specific parts of an egl surface.
 * @param surface The surface to be resized.
 * @param width The width of the new size, in pixels.
 * @param height The height of the new size, in pixels.
 * @return EGL_TRUE if the surface was resized successfully, EGL_FALSE otherwise.
 * @note Called as an implicit part of eglSwapBuffers for the fbdev and earlier versions of the Android backend.
 *
 * Consider using another apporach to resizing the surface if possible.
 * Using _mali_frame_builder_set_acquire_output_callback to get informed when a new buffer is needed is the recommended approach.
 */
EGLBoolean __egl_platform_resize_surface( egl_surface *surface, u32 *width, u32 *height, mali_base_ctx_handle base_ctx ) MALI_CHECK_RESULT;

/**
 * @brief Retrieves the size of a native pixmap.
 * @param display The native display.
 * @param pixmap The native pixmap to retrieve the size of.
 * @param width A pointer to store the width of the pixmap.
 * @param height A pointer to store the height of the pixmap.
 * @param pitch A pointer to store the pitch of the pixmap.
 * @note \em NULL can be specified for any of the output parameters.
 *
 * Width and height of pixmap should be retrieved in pixels.\n
 * Pitch of pixmap should be retrieved in bytes per row.
 */
void __egl_platform_get_pixmap_size( EGLNativeDisplayType display, EGLNativePixmapType pixmap, u32 *width, u32 *height, u32 *pitch );

/**
 * @brief Retrieves the color space of a native pixmap.
 * @param pixmap The native pixmap to retrieve the color space of.
 * @return An EGLenum representing the color space.
 * @note Valid return values are \em EGL_COLORSPACE_sRGB and \em EGL_COLORSPACE_LINEAR.
 */
EGLenum __egl_platform_get_pixmap_colorspace( EGLNativePixmapType pixmap );

/**
 * @brief Retrieves the alpha format of a native pixmap.
 * @param pixmap The native pixmap to retrieve the alpha format of.
 * @return An EGLenum representing the alpha format.
 * @note Valid return values are \em EGL_ALPHA_FORMAT_PRE and \em EGL_ALPHA_FORMAT_NONPRE.
 */
EGLenum __egl_platform_get_pixmap_alphaformat( EGLNativePixmapType pixmap );

/**
 * @brief Retrieves format specifications for a native pixmap.
 * @param display The native display.
 * @param pixmap The native pixmap to retrieve format specifications for.
 * @param sformat Pointer to store the surface format specification in.
 * @see mali_surface_specifier
 * @note Used by API tracing functionality.
 */
void __egl_platform_get_pixmap_format( EGLNativeDisplayType display, EGLNativePixmapType pixmap, mali_surface_specifier *sformat );

/**
 * @brief Verify whether a native pixmap is valid.
 * @param pixmap The native pixmap to verify.
 * @return EGL_TRUE if the pixmap is valid, EGL_FALSE otherwise.
 */
EGLBoolean __egl_platform_pixmap_valid( EGLNativePixmapType pixmap ) MALI_CHECK_RESULT;

/**
 * @brief Checks if a native pixmap is supported / allocated by the unified memory provider (UMP).
 * @param pixmap The native pixmap to check UMP support for.
 * @return EGL_TRUE if the pixmap supports UMP, EGL_FALSE otherwise.
 * @note Used to detmine if Mali GPU can directly operate on the backing memory or not.
 */
EGLBoolean __egl_platform_pixmap_support_ump( EGLNativePixmapType pixmap ) MALI_CHECK_RESULT;

/**
 * @brief Verify wheter a native pixmap is compatible with the internal \ref egl_config structure.
 * @param display The native display.
 * @param pixmap The native pixmap to verify.
 * @param config A pointer to the internal configuration to compare the pixmap with.
 * @param renderable_usage Specifies whether compatability with the config EGL_RENDERABLE_TYPE should be checked or not.
 * @return EGL_TRUE if the pixmap is compatible, EGL_FALSE otherwise.
 * @note Verification will typically involve checking if the pixmap and the \ref egl_config have the same color component sizes.
 *
 * Internally called as part of \ref eglCreatePixmapSurface.
 */
EGLBoolean __egl_platform_pixmap_config_compatible( EGLNativeDisplayType display, EGLNativePixmapType pixmap, egl_config *config, EGLBoolean renderable_usage ) MALI_CHECK_RESULT;

/**
 * @brief Verifies wheter a native pixmap is compatible with the internal \ref egl_surface structure.
 * @param display The native display.
 * @param pixmap The native pixmap to verify.
 * @param surface A pointer to the internal surface structure to compare the pixmap with.
 * @param alpha_check Specifies whether compatability with the surface alpha format and pixmap should be checked or not.
 * @return EGL_TRUE if the pixmap is compatible, EGL_FALSE otherwise.
 * @note Verification will typically involve checking if the pixmap and the \ref egl_surface have the same dimensions.
 */
EGLBoolean __egl_platform_pixmap_surface_compatible( EGLNativeDisplayType display, EGLNativePixmapType pixmap, egl_surface *surface, EGLBoolean alpha_check ) MALI_CHECK_RESULT;

/**
 * @brief Determine wheter a surface can be copied onto a native pixmap.
 * @param display The native display.
 * @param pixmap The native pixmap to check for compatibility.
 * @param surface A pointer to the surface to check for compatibility with the native pixmap.
 * @return EGL_TRUE if copying is possible, EGL_FALSE otherwise.
 * @note If copying is possible, but the color depths and sizes of the pixmap
 * do not match that of the surface, then we should <em>"define an EGL extension
 * specifying which destination formats are supported, and specifying the
 * conversion arithmetic used." </em> - Section 3.9.3 'Posting Semantics', EGL 1.4 January 13, 2009.
 *
 * Called as part of the \ref eglCopyBuffers implementation in core EGL.
 */
EGLBoolean __egl_platform_pixmap_copybuffers_compatible( EGLNativeDisplayType display, EGLNativePixmapType pixmap, egl_surface *surface ) MALI_CHECK_RESULT;

/**
 * @brief Verifies that a native pixmap is suitable for use as an EGLImage source.
 * @param context The egl context to use the EGLImage in.
 * @param pixmap The native pixmap to check for compatibility.
 * @return EGL_TRUE if compatible, EGL_FALSE otherwise.
 * @note Verification will typically involve check of pixmap usage flags.
 * @deprecated
 */
EGLBoolean __egl_platform_pixmap_egl_image_compatible( EGLNativePixmapType pixmap, egl_context *context ) MALI_CHECK_RESULT;

/**
 * @brief Given a native pixmap, maps the resource into a \ref mali_image structure.
 * @param display The native display.
 * @param egl_img A pointer to an \ref egl_image structure.
 * @param pixmap The native pixmap to map.
 * @return pointer A pointer to a \ref mali_image structure with the given native pixmap mapped in.
 * @note The egl_image pointer can be omitted if the cause for mapping the pixmap is not related to EGLImageKHR extension.
 * @see __egl_platform_unmap_pixmap
 */
struct mali_image* __egl_platform_map_pixmap( EGLNativeDisplayType display, struct egl_image *egl_img, EGLNativePixmapType pixmap ) MALI_CHECK_RESULT;

/**
 * @brief Given a native pixmap, unmaps the resource.
 * @param pixmap The native pixmap to unmap.
 * @param egl_img A pointer to an \ref egl_image structure.
 * @note The egl_image pointer can be omitted if the cause for mapping the pixmap is not related to EGLImageKHR extension.
 * @see __egl_platform_map_pixmap
 */
void __egl_platform_unmap_pixmap( EGLNativePixmapType pixmap, struct egl_image *egl_img );

/**
 * @brief Unmaps any allocated buffers used for creating egl image.
 * @param buffer Pointer to buffer which is to be unmapped.
 * @note This entrypoint is only applicable to EGL_KHR_image_system.
 */
void __egl_platform_unmap_image_buffer( void *buffer );

/**
 * @brief Swaps the content of mali onto a native window.
 * @param base_ctx Handle to a mali base context.
 * @param native_dpy The native display to use.
 * @param surface The egl surface to swap onto the native window.
 * @param target Pointer to a mali render target containing the render output.
 * @param interval The swap interval to use for this swap.
 * @note This call is issued asynchronously by core EGL when eglSwapBuffers completes.
 *
 * Can be called from either the main process or a worker thread.
 */
void __egl_platform_swap_buffers( mali_base_ctx_handle base_ctx, EGLNativeDisplayType native_dpy, egl_surface *surface, struct mali_surface *target, EGLint interval );

#if MALI_ANDROID_API > 12
/**
 * @brief Queues an \ref egl_buffer back to the native buffer manager.
 * @param buffer Pointer to an egl_buffer structure containing the buffer to be queued back.
 * @note Android specific, executed instead of \ref __egl_platform_swap_buffers.
 *
 * Can be called from either the main process or a worker thread.
 */
void __egl_platform_queue_buffer( mali_base_ctx_handle base_ctx, egl_buffer *buffer );
#endif

/**
 * @brief Returns a mali_mem_handle wrapped around the pixmap memory.
 * @param display The native display.
 * @param native_pixmap The native pixmap to wrap into mali memory.
 * @param base_ctx Handle to a mali base context.
 * @param surface Pointer to egl_surface for which to use the wrapped memory with.
 * @return A mali_mem_handle to the memory.
 */
mali_mem_handle __egl_platform_pixmap_get_mali_memory( EGLNativeDisplayType display, EGLNativePixmapType native_pixmap, mali_base_ctx_handle base_ctx, egl_surface *surface );

/**
 * @brief Copies the content of mali rendered output onto a native pixmap.
 * @param base_ctx Handle to a mali base context.
 * @param native_dpy The native display.
 * @param surface Pointer to \ref egl_surface used to render the output.
 * @param target Pointer to a mali render target containing the render output.
 * @param native_pixmap The native pixmap to copy the output onto.
 */
void __egl_platform_copy_buffers( mali_base_ctx_handle base_ctx, EGLNativeDisplayType native_dpy, egl_surface *surface, struct mali_surface *target, EGLNativePixmapType native_pixmap );

/**
 * @brief Sets the native window size.
 * @param win Native window.
 * @param width The new width.
 * @param height The new height.
 * @deprecated
 */
void __egl_platform_set_window_size( EGLNativeWindowType win, u32 width, u32 height );

/**
 * @brief Retrieves the size of a native window.
 * @param win The native window to retrieve the size of.
 * @param platform Pointer to platform-specific surface data (surface->platform)
 * @param width A pointer to store the window width.
 * @param height A pointer to store the window height.
 * @return EGL_TRUE on success, EGL_FALSE otherwise.
 */
EGLBoolean __egl_platform_get_window_size( EGLNativeWindowType win, void *platform, u32 *width, u32 *height );

/**
 * @brief Verifies that a native window is valid.
 * @param display The native display this window exists on.
 * @param win The native window to verify.
 * @return EGL_TRUE if the native window is valid, EGL_FALSE otherwise.
 */
EGLBoolean __egl_platform_window_valid( EGLNativeDisplayType display, EGLNativeWindowType win ) MALI_CHECK_RESULT;

/**
 * @brief Checks if a native window is compatible with an \ref egl_config.
 * @param display The native display this window exists on.
 * @param win The native window to check for compatibility.
 * @param config The egl_config to check compatibility with.
 * @return EGL_TRUE if the window is compatible, EGL_FALSE otherwise.
 * @note This will typically involve comparisions of color format settings such as component sizes and offsets.
 */
EGLBoolean __egl_platform_window_compatible( EGLNativeDisplayType display, EGLNativeWindowType win, egl_config *config ) MALI_CHECK_RESULT;

#if HAVE_ANDROID_OS
/**
 * @brief Query surface's buffer invalidity (result of the last buffer acquisition operation).
 * @brief Query if the egl_surface has a valid buffer.
 * @param surface The egl surface to query for valid buffer.
 * @return EGL_TRUE if the surface has a valid buffer, EGL_FALSE otherwise.
 *
 * The success or failure of this query depends on the outcome of the latest attempt to dequeue a buffer.
 *
 * @note Android specific.
 */
EGLBoolean __egl_platform_surface_buffer_invalid( egl_surface* surface ) MALI_CHECK_RESULT;

/**
 * @brief Connects an egl surface.
 * @param surface The surface to connect.
 * @return EGL_TRUE if the connection was made successfull, EGL_FALSE otherwise.
 *
 * Called as part of eglMakeCurrent when the surface is made current.
 *
 * @note Android specific.
 */
EGLBoolean __egl_platform_connect_surface( egl_surface *surface );

/**
 * @brief Disconnects an egl surface.
 * @param surface The surface to disconnect.
 *
 * Called as part of eglMakeCurrent when a new surface is made current.
 *
 * @note Android specific.
 */
void __egl_platform_disconnect_surface( egl_surface *surface );

/**
 * @brief Checks if deferred release is required on the surface.
 * @param surface The surface to check for deferred release.
 * @return MALI_TRUE if deferred release is required, MALI_FALSE if not.
 *
 * Depending on the specific Android version. For later versions of Android, deferred release is enabled
 * if the surface is the framebuffer itself.
 *
 * @note Android specific.
 */
mali_bool __egl_platform_surface_is_deferred_release( const egl_surface * surface);
#endif

/**
 * @brief Called when a new frame is taken into use.
 * @param surface The surface for which a new frame is to be taken in use.
 * @note Typically called during eglSwapBuffers to start the next frame.\n
 *
 * This is a good place to setup per frame callbacks.
 *
 * @see _mali_frame_builder_set_complete_output_callback
 * @see _mali_frame_builder_set_acquire_output_callback
 */
void __egl_platform_begin_new_frame( egl_surface *surface );

/**
 * @brief Creates a NULL native window, possibly fullscreen.
 * @param dpy The native display to create the native window on.
 * @return An initialized native window.
 * @note Called if eglCreateWindowSurface is given NULL as native window.\n
 *
 * Does not need to be supported by platform backend, in which case an error
 * will be returned by eglCreateWindowSurface.
 *
 * @deprecated
 */
EGLNativeWindowType __egl_platform_create_null_window( EGLNativeDisplayType dpy ) MALI_CHECK_RESULT;

/**
 * @brief Checks if the platform supports vsync.
 * @return EGL_TRUE if supported, EGL_FALSE if not.
 */
EGLBoolean __egl_platform_supports_vsync( void ) MALI_CHECK_RESULT;

/**
 * @brief Sets swap region.
 * @param surface The surface to swap onto the native pixmap.
 * @param numrects Number of rectangles in the <rects> list.
 * @param rects Pointer to a list of 4-integer blocks defining rectangles.
 * @deprecated
 */
void __egl_platform_set_swap_region( egl_surface *surface, EGLint numrects, const EGLint *rects );

#if EGL_KHR_lock_surface_ENABLE
/**
 * @brief Maps a lockable surface.
 * @param dpy The native display the surface is associated with.
 * @param surface The surface to map.
 * @param preserve_contents Set to EGL_TRUE if the mapped buffer should contain the contents drawn to the buffer after a previous map. If EGL_FALSE, the contents of the mapped buffer is undefined.
 * @return EGL_TRUE on success, EGL_FALSE otherwise.
 * Called on a lockable surface when the client API user wants a pointer to the backbuffer of the surface\n
 * If successfull, this call should update:\n
 * \li \em surface->lock_surface->mapped_pointer with a pointer to the backbuffer of the surface
 * \li \em surface->lock_surface->bitmap_pitch with the pitch, that is, the byte distance between lines, of the backbuffer
 * @note This function is only called on lockable surfaces that have been previously locked.
 */
EGLBoolean __egl_platform_lock_surface_map_buffer( EGLNativeDisplayType dpy, egl_surface* surface, EGLBoolean preserve_contents );

/**
 * @brief Unmaps a lockable surface.
 * @param dpy The native display the surface is associated with.
 * @param surface The surface to unmap.
 * @return EGL_TRUE on success, EGL_FALSE otherwise.
 *
 * Called when the surface is unlocked, if it has been previously mapped.\n
 * When this function is called, the user changes done to the buffer pointed to by \em surface->lock_surface->mapped_pointer
 * are updated in the backbuffer of the surface. For example: A temporary buffer is copied to the real backbuffer.
 *
 * @note This function is only called on lockable surfaces that have been previously locked and mapped.
 */
EGLBoolean __egl_platform_lock_surface_unmap_buffer( EGLNativeDisplayType dpy, egl_surface *surface );

#endif  /* EGL_KHR_lock_surface_ENABLE */

/**
 * @brief Gets the next free buffer from the platform layer.
 * @param surface The surface to get the next free buffer for.
 * @return Index to the buffer.
 */
unsigned int __egl_platform_get_buffer( egl_surface *surface );

/**
 * @brief Register a lock item.
 * @param lock_item A pointer to an egl_surface_lock_item to register.
 *
 * Called when the lock item is prepared for use, but not yet used as render target or source.
 *
 * @note Optional to implement.
 */
void __egl_platform_register_lock_item( egl_surface_lock_item *lock_item );

/**
 * @brief Unregisters a lock item.
 * @param lock_item A pointer to an egl_surface_lcok_item to unregister.
 *
 * Typically called when a surface is initialized but not used, or drawcalls added with the item used as a texture but not processed.
 * The lock item has not yet been processed (locked) at this time
 *
 * @note Optional to implement.
 */
void __egl_platform_unregister_lock_item( egl_surface_lock_item *lock_item );

/**
 * @brief Process a lock item.
 * @param lock_item A pointer to an egl_surface_lock_item to process.
 *
 * Called when the lock item is taken in use by the hardware (CPU/GPU).
 *
 * @note Optional to implement.
 */
void __egl_platform_process_lock_item( egl_surface_lock_item *lock_item );

/**
 * @brief Release a lock item.
 * @param lock_item A pointer to an egl_surface_lock_item to release.
 *
 * Called when the lock item is no longer in use by the hardware (CPU/GPU).
 *
 * @note Optional to implement.
 */
void __egl_platform_release_lock_item( egl_surface_lock_item *lock_item );

/**
 * @brief Update an egl_image surface.
 * @param surface A pointer to a mali_surface structure for which to update.
 * @param data Pointer to platform specific data.
 *
 * Called to update the egl_image surface, in case surface locking is enabled.
 * Typically called before the GPU takes the texture in use. Used to make any last minute updates to the surface texture data.
 *
 * @note Optional to implement.
 */
void __egl_platform_update_image(struct mali_surface * surface, void *data);

#ifdef __cplusplus
}
#endif

/** @} */ /* end group egl_platform_api */

#endif /* _EGL_PLATFORM_H_ */

