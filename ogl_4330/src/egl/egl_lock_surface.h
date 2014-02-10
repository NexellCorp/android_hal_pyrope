/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _EGL_LOCK_SURFACE_H_
#define _EGL_LOCK_SURFACE_H_

/**
 * @file egl_lock_surface.h
 * @brief EGLLockSurface extension.
 */

#include <EGL/egl.h>
#include "egl_thread.h"
#include "egl_surface.h"
#include "egl_misc.h"

typedef struct egl_lock_surface_attributes
{
	EGLBoolean is_locked;                 /**< Is EGL_TRUE if the surface is locked with eglLockSurfaceKHR */
	EGLBoolean map_preserve_pixels;       /**< The value of the EGL_MAP_PRESERVE_PIXELS_KHR attribute given to eglLockSurfaceKHR */
	EGLint lock_usage_hint;               /**< The value of the EGL_LOCK_USAGE_HINT_KHR attribute given to eglLockSurfaceKHR */
	/* The variables below are setup by the platform layer when the surface
	 * is created or when the buffer is mapped */
	void* mapped_pointer;                 /**< Is not NULL if the surface is both locked and mapped.*/
	EGLint bitmap_pitch;                  /** Updated when the surface is mapped */
	EGLint bitmap_origin;                 /** EGL_UPPER_LEFT_KHR or EGL_LOWER_LEFT_KHR. Set by platform layer */
	EGLint bitmap_pixel_size;             /** number of bits per pixel */
	EGLint bitmap_pixel_red_offset;       /** Set by platform layer */
	EGLint bitmap_pixel_green_offset;     /** Set by platform layer */
	EGLint bitmap_pixel_blue_offset;      /** Set by platform layer */
	EGLint bitmap_pixel_alpha_offset;     /** Set by platform layer */
	EGLint bitmap_pixel_luminance_offset; /** Set by platform layer */
} egl_lock_surface_attributes;

/**
 * @brief Locks an EGLSurface, so that the color buffer may subsequently be mapped by using eglQuerySurface
 * @param display The EGLDisplay the surface belongs to
 * @param surface The EGLSurface to lock
 * @param attrib_list An array of EGLint's specifying the behavior of the locked surface. Terminated by EGL_NONE. attrib_list may be NULL.
 * @param tstate EGL internal thread state
 * @return EGL_TRUE of the locking succeeded, EGL_FALSE otherwise.
 */
EGLBoolean _egl_lock_surface_KHR( EGLDisplay display, EGLSurface surface, const EGLint* attrib_list, __egl_thread_state* tstate );

/**
 * @brief Unlocks a locked EGLSurface, allowing other EGL operations on the surface yet again
 * @param display The EGLDisplay the surface belongs to
 * @param surface The EGLSurface to unlock
 * @param tstate EGL internal thread state
 * @return EGL_TRUE if the surface is successfully unlocked.
 */
EGLBoolean _egl_unlock_surface_KHR( EGLDisplay display, EGLSurface surface, __egl_thread_state* tstate );

/**
 * @brief Maps a locked surface and returns a pointer to the color buffer
 * If the surface is previously mapped, the same pointer is returned
 * If the surface isn't locked, EGL_BAD_ACCESS is set
 * @param surface The surface that should be mapped
 * @param tstate EGL internal thread state
 * @return EGL_TRUE if buffer was mapped, and surface->ext_lock_surface.{pitch,mapped_pointer} was updated
 */
EGLBoolean __egl_lock_surface_map_buffer( egl_surface* surface, __egl_thread_state* tstate);

/**
 * @brief Unmaps a locked surface.
 * On completion surface->lock_surface->mapped_pointer is set to NULL
 * @param surface A lockable surface
 * @tstate EGL thread state
 * @return EGL_TRUE if successful
 */
EGLBoolean __egl_lock_surface_unmap_buffer( egl_surface* surface, __egl_thread_state* tstate );

/**
 * @brief Iterates over configs, and marks the EGL_SURFACE_TYPE of those
 * matching the display's pixel format with the EGL_KHR_OPTIMAL_FORMAT_BIT
 * @param display The display used
 */
void __egl_lock_surface_initialize_configs( egl_display* display );

/**
 * @brief Allocates internal memory and initializes lock surface attribute defaults
 * @param surface The surface that should be initialized
 * @return EGL_FALSE if out-of-memory. EGL_TRUE otherwise.
 */
EGLBoolean __egl_lock_surface_initialize( egl_surface* surface );

/**
 * @brief Deallocates lock-surface specific data
 * @param surface The surface to clean up
 */
void __egl_lock_surface_release( egl_surface* surface );

/**
 * @brief Check's if a surface is locked using eglLockSurfaceKHR.
 * Only eglQuerySurface and eglUnlockSurfaceKHR is allowed when the surface is locked
 * @param surface The surface to check
 * @return EGL_TRUE if the surface is a lockable surface, and it is locked. EGL_FALSE otherwise
 */
EGLBoolean __egl_lock_surface_is_locked( const egl_surface* surface );

/**
 * @brief Perform any checking/handling needed for lock surfaces during a call to eglSurfaceAttrib
 * @param surface the surface on which to set the attribute on
 * @param attribute EGLSurface attribute to set as passed to eglSurfaceAttrib
 * @param value the value to set for the attribute as passed to eglSurfaceAttrib
 * @param result pointer to EGLBoolean memory that is used to indicate success/failure of a valid attribute
 * @param tstate thread state pointer
 * @return EGL_TRUE to indicate that attribute was handled by this function, EGL_FALSE otherwise
 * @note This function uses two EGLBoolean variables to indicate to the caller what action should be taken. If the
 * function returns EGL_TRUE then this function has finished the call to eglSurfaceAttrib and the value of result
 * should be returned to the caller (an error may have been set).
 */
EGLBoolean __egl_lock_surface_attrib( egl_surface *surface, EGLint attribute, EGLint value, EGLBoolean *result, __egl_thread_state* tstate );

#endif /* defined(_EGL_LOCK_SURFACE_H_) */
