/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <mali_system.h>
#include <base/mali_context.h>
#include "egl_common_memory.h"

#if MALI_USE_UNIFIED_MEMORY_PROVIDER
#include "ump/ump_ref_drv.h"
#elif MALI_USE_DMA_BUF
#if defined(EGL_BACKEND_X11)
#include <xf86drm.h>
#elif defined(EGL_BACKEND_ANDROID)
#endif
#endif

egl_memory_handle _egl_memory_create_handle_from_name( int drm_fd, egl_buffer_name buf_name )
{
	egl_memory_handle mem_handle = EGL_INVALID_MEMORY_HANDLE;

#if MALI_USE_UNIFIED_MEMORY_PROVIDER
	mem_handle = (egl_memory_handle)ump_handle_create_from_secure_id( (ump_secure_id)buf_name );
#elif MALI_USE_DMA_BUF
#if defined(EGL_BACKEND_X11)
	struct drm_gem_open  open;
	struct drm_gem_close close;
	struct drm_prime_handle prime_handle;

	/* Get gem_handle from gem_name */
	_mali_sys_memset(&open, 0, sizeof(open));
	open.name = buf_name;
	if ( ioctl( drm_fd, DRM_IOCTL_GEM_OPEN, &open ) < 0 )
	{
		_mali_sys_printf("Failed to open GEM name=%d\n", open.name);
		return 0;
	}

	/* Get the dma_buf fd from the gem_handle */
	_mali_sys_memset(&prime_handle, 0, sizeof(prime_handle));
	prime_handle.handle = open.handle;
	prime_handle.fd 	= 0;
	if ( ioctl( drm_fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime_handle ) < 0 )
	{
		_mali_sys_printf("Failed to get fd from GEM handle=%d (GEM name = %d)\n", open.handle, open.name);

		_mali_sys_memset(&close, 0, sizeof(close));
		close.handle = open.handle;
		if (ioctl( drm_fd, DRM_IOCTL_GEM_CLOSE, &close ) < 0)
			_mali_sys_printf("Failed to close GEM handle=%d\n", close.handle);

		return 0;
	}

	mem_handle = (egl_memory_handle)prime_handle.fd;
#endif
#else
	MALI_IGNORE(drm_fd);
	MALI_IGNORE(buf_name);
#endif

	return mem_handle;
}

mali_mem_handle _egl_memory_create_mali_memory_from_handle( mali_base_ctx_handle ctx, egl_memory_handle mem_handle, u32 offset )
{
	mali_mem_handle mali_mem = MALI_NO_HANDLE;

#if MALI_USE_UNIFIED_MEMORY_PROVIDER
	mali_mem = _mali_mem_wrap_ump_memory( ctx, (ump_handle)mem_handle, offset );
#elif MALI_USE_DMA_BUF
	mali_mem = _mali_mem_wrap_dma_buf( ctx, (int)mem_handle, offset );
#else
	MALI_IGNORE(ctx);
	MALI_IGNORE(mem_handle);
	MALI_IGNORE(offset);
#endif

	return mali_mem;
}

egl_memory_handle _egl_memory_get_handle_from_mali_memory( mali_mem_handle mali_mem )
{
	egl_memory_handle mem_handle = EGL_INVALID_MEMORY_HANDLE;

#if MALI_USE_UNIFIED_MEMORY_PROVIDER
	mem_handle = (egl_memory_handle)_mali_mem_get_ump_memory( mali_mem );
#elif MALI_USE_DMA_BUF
	mem_handle = (egl_memory_handle)_mali_mem_get_dma_buf_descriptor( mali_mem );
#else
	MALI_IGNORE(mali_mem);
#endif

	return mem_handle;
}

egl_buffer_name _egl_memory_get_name_from_handle( int drm_fd, egl_memory_handle mem_handle )
{
	egl_buffer_name buf_name = (egl_buffer_name)EGL_INVALID_BUFFER_NAME;

#if MALI_USE_UNIFIED_MEMORY_PROVIDER
	MALI_IGNORE(drm_fd);
	buf_name = ump_secure_id_get( (ump_handle)mem_handle );
#elif MALI_USE_DMA_BUF
#if defined(EGL_BACKEND_X11)
	struct drm_prime_handle prime_handle;
	struct drm_gem_flink flink;

	/* Get the gem_handle from the dma_buf */
	_mali_sys_memset(&prime_handle, 0, sizeof(prime_handle));
	prime_handle.fd 	= (unsigned int)mem_handle;
	prime_handle.handle = 0;
	if ( ioctl( drm_fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &prime_handle ) < 0 )
		_mali_sys_printf("Failed to get GEM handle from fd = %d\n", mem_handle);

	_mali_sys_memset(&flink, 0, sizeof(flink));
	flink.handle = prime_handle.handle;
	if ( ioctl (drm_fd, DRM_IOCTL_GEM_FLINK, &flink) < 0 )
		_mali_sys_printf("Failed to flink GEM name from GEM handle = %x\n", flink.handle);

	buf_name = flink.name;
#endif
#else
	MALI_IGNORE(drm_fd);
	MALI_IGNORE(mem_handle);
#endif

	return buf_name;
}

void _egl_memory_release_reference( egl_memory_handle mem_handle )
{
#if MALI_USE_UNIFIED_MEMORY_PROVIDER
	ump_reference_release( (ump_handle)mem_handle );
#elif MALI_USE_DMA_BUF
#if defined(EGL_BACKEND_X11)
	close((unsigned int)mem_handle);
#endif
#else
	MALI_IGNORE(mem_handle);
#endif
}
