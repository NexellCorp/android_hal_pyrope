/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_platform_android_v2.c
 * @brief Android backend
 */
#include <EGL/egl.h>
#include "egl_platform.h"
#include "egl_mali.h"
#include "egl_thread.h"
#include "egl_surface.h"

#include <mali_system.h>
#include <shared/mali_surface.h>
#include <shared/mali_image.h>
#include <shared/mali_shared_mem_ref.h>
#include <shared/mali_pixel_format.h>
#include <shared/m200_texel_format.h>
#include <shared/m200_td.h>
#include <shared/m200_gp_frame_builder_inlines.h>
#include <shared/m200_gp_frame_builder.h>
#include "egl_image_android.h"

#ifdef NDEBUG
/* org */
#define ANDROID_ENABLE_DEBUG 0
#else /* nexell add */
#define ANDROID_ENABLE_DEBUG 1
#endif

#ifdef HAVE_ANDROID_OS
#if MALI_ANDROID_API >= 15
#include <system/window.h>
#else
#include <ui/egl/android_natives.h>
#include <ui/android_native_buffer.h>
#endif
#include <hardware/gralloc.h>
#include <gralloc_priv.h>
#	ifdef MALI_USE_UNIFIED_MEMORY_PROVIDER
#		if (GRALLOC_ARM_DMA_BUF_MODULE != 1) && (GRALLOC_ARM_UMP_MODULE != 1)
#			error "Trying to compile ump driver without using the ump version of gralloc provided by ARM"
#		endif
#	endif
#include <linux/ioctl.h>
#endif

#if EGL_SURFACE_LOCKING_ENABLED
#include <fcntl.h>
#include <sys/ioctl.h>
#include "../devicedrv/umplock/umplock_ioctl.h"
#endif /* EGL_SURFACE_LOCKING_ENABLED */

#if MALI_USE_UNIFIED_MEMORY_PROVIDER
#include <ump/ump.h>
#if !defined(UMP_VERSION_MAJOR) || UMP_VERSION_MAJOR == 1
#include <ump/ump_ref_drv.h>
#endif
#endif

#include <utils/Log.h>
#include <cutils/properties.h>

#if MALI_ANDROID_API > 12
#define NUM_BUFFERS 3
#else
#define NUM_BUFFERS 2
#endif

#if EGL_ANDROID_recordable_ENABLE
#include "egl_recordable_android.h"
#endif

#if MALI_TIMELINE_PROFILING_ENABLED
#include <base/mali_profiling.h>
#endif

#if EGL_ANDROID_native_fencesync_ENABLE
#if MALI_EXTERNAL_SYNC != 1
#error "MALI_EXTERNAL_SYNC must be enabled to get EGL_ANDROID_native_fences_sync extension support"
#endif
#include "sync/mali_external_sync.h"
#endif

//added by nexell
#include <base/mali_memory.h>

/*
 * System property keys for controlling __egl_platform_dequeue_buffer() failure simulation
 *
 * fail_process specifies the applications to simulate failures in; you can use '*' as
 * a wildcard, e.g. com.android.*
 *
 * fail_first specifies when after how many calls to __egl_platform_dequeue_buffer first
 * failure should occur.
 *
 * fail_interval specifies after how many calls the failure repeats after the first occurance
 * */
#define PROP_MALI_TEST_FAIL_PROCESS   "mali.test.dequeue.fail_process"
#define PROP_MALI_TEST_FAIL_FIRST     "mali.test.dequeue.fail_first"
#define PROP_MALI_TEST_FAIL_INTERVAL  "mali.test.dequeue.fail_interval"

#define IS_FRAMEBUFFER( native_buf_ptr ) \
	(((MALI_REINTERPRET_CAST(private_handle_t const*)(native_buf_ptr)->handle)->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER) ? MALI_TRUE : MALI_FALSE)

#if MALI_ANDROID_API < 15
using namespace android;
#endif

/* global instance of gralloc module reference
 * initialized at platform initialization time
 */
gralloc_module_t const* module = NULL;

typedef enum
{
	BUFFER_EMPTY = 1,
	BUFFER_CLEAN = 2,
	BUFFER_LOCKED = 3,
	BUFFER_QUEUED = 4
} android_buffer_flags;

typedef enum
{
	STATE_OK,
	STATE_RECREATE_SINGLE_BUFFER,
	STATE_RECREATE_DOUBLE_BUFFER,
	STATE_RECREATE_TRIPLE_BUFFER,
} android_buffer_state;

/* Android specific surface data */
typedef struct android_platform_data
{
	mali_atomic_int dequeued_buffers;

	/* result of last android_native_window_t::dequeueBuffer() operation */
	mali_atomic_int buffer_valid;
	/*max allowed dequeue buffer number*/
	unsigned int max_allowed_dequeued_buffers;
} android_platform_data;

typedef struct android_format_mapping
{
	int android_pixel_format;
	m200_texel_format mali_pixel_format;
} android_format_mapping;

typedef struct _android_client_buffer
{
	android_native_buffer_t *native_buf;
	int fence_fd;
}android_client_buffer;

MALI_STATIC android_format_mapping android_format_mappings[] =
{
	{ HAL_PIXEL_FORMAT_RGBA_8888, M200_TEXEL_FORMAT_ARGB_8888 },
	{ HAL_PIXEL_FORMAT_RGBX_8888, M200_TEXEL_FORMAT_xRGB_8888 },
#if !RGB_IS_XRGB
	{ HAL_PIXEL_FORMAT_RGB_888,   M200_TEXEL_FORMAT_RGB_888 },
#endif
	{ HAL_PIXEL_FORMAT_RGB_565,   M200_TEXEL_FORMAT_RGB_565 },
#if !defined( NEXELL_FEATURE_KITKAT_USE )
	{ HAL_PIXEL_FORMAT_RGBA_5551, M200_TEXEL_FORMAT_ARGB_1555 },
	{ HAL_PIXEL_FORMAT_RGBA_4444, M200_TEXEL_FORMAT_ARGB_4444 }
#endif	
};

static int fd_umplock = -1;

void __egl_platform_dequeue_buffer( egl_surface *surface );
void __egl_platform_lock_buffer( egl_buffer *buffer );

/* fills in buffer information, such as physical and virtual address */
EGLBoolean native_buffer_get_info( android_native_buffer_t* buffer,
								   u32 *paddr, u32 *vaddr, u32 *offset, u32 *size,
								   mali_bool *is_framebuffer )
{
	private_handle_t const* hnd;

	/* on pre-ICS Androids we might come here w/ a NULL buffer (via ump_get_secure_id()) */
	if (!buffer || (!buffer->handle))
	{
		AINF("Invalid buffer (NULL)! bailing out.");
		return EGL_FALSE;
	}

	hnd = reinterpret_cast<private_handle_t const*>(buffer->handle);

	if ( hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER )
	{
		struct fb_fix_screeninfo fix_info;
		(void)ioctl( hnd->fd, FBIOGET_FSCREENINFO, &fix_info );
		*paddr = fix_info.smem_start;
		*offset = hnd->offset;
		*is_framebuffer = MALI_TRUE;
	}
	else if ( hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP )
	{
#if GRALLOC_ARM_UMP_MODULE != 0
		*paddr = 0;
		*offset = 0;
		*is_framebuffer = MALI_FALSE;
#else
		AERR( "Unable to use UMP buffer from handle %p. No UMP support in gralloc", hnd );
		return EGL_FALSE;
#endif
	}
	else if ( hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION )
	{
#if GRALLOC_ARM_DMA_BUF_MODULE != 0
		*paddr = NULL;
		*offset = 0;
		*is_framebuffer = MALI_FALSE;
#else
		AERR( "Unable to use ION buffer from handle 0x%x. No ION support in gralloc", (unsigned int)hnd );
		return EGL_FALSE;
#endif
	}
	else
	{
		MALI_DEBUG_ASSERT(MALI_FALSE, ("Unsupported buffer type. Please add support for gralloc with UMP. See Integration Guide for steps on how to do this."));
		return EGL_FALSE;
	}

	*vaddr = hnd->base;
	*size = hnd->size;

	return EGL_TRUE;
}

mali_mem_handle native_buffer_wrap( mali_base_ctx_handle base_ctx,
								   android_native_buffer_t* buffer,
								   u32 physical_address, u32 virtual_address, u32 offset, u32 mem_size )
{
	MALI_DEBUG_ASSERT_POINTER( base_ctx );
	mali_mem_handle mem_handle = MALI_NO_HANDLE;
	private_handle_t const* hnd;

	/* on pre-ICS Androids we might come here w/ a NULL buffer (via ump_get_secure_id()) */
	if (!buffer)
	{
		AINF("Invalid buffer (NULL)! bailing out.");
		return mem_handle;
	}

	hnd = reinterpret_cast<private_handle_t const*>(buffer->handle);

	if ( hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER )
	{
		MALI_DEBUG_ASSERT( NULL != (void*) physical_address, ("Null ptr physical address for hnd: %p", hnd ) );
		mem_handle = _mali_mem_add_phys_mem( base_ctx, physical_address, mem_size, (void*)(virtual_address - offset), MALI_PP_WRITE | MALI_PP_READ | MALI_CPU_READ | MALI_CPU_WRITE );
		ADBG(0, "create buffer from FB (0x%x + %i mem handle: 0x%x)", physical_address, offset, mem_handle);
	}
	else if ( hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP )
	{
#if GRALLOC_ARM_UMP_MODULE != 0
		MALI_DEBUG_ASSERT( UMP_INVALID_MEMORY_HANDLE != (ump_handle)hnd->ump_mem_handle, ("Invalid ump memory for handle %p", hnd ) );
		mem_handle = _mali_mem_wrap_ump_memory( base_ctx, (ump_handle)hnd->ump_mem_handle, 0 );
		ADBG(0, "color buffer from UMP (%i)", ump_secure_id_get( (ump_handle)hnd->ump_mem_handle ) );
#else
		AERR( "Unable to use UMP buffer from handle %p. No UMP support in gralloc", hnd );
#endif
	}
	else if ( hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION )
	{
#if GRALLOC_ARM_DMA_BUF_MODULE != 0
		MALI_DEBUG_ASSERT( hnd->share_fd > 0, ("Share fd is wrong: %d", hnd->share_fd ) );
		mem_handle = _mali_mem_wrap_dma_buf( base_ctx, hnd->share_fd, 0 );
		if ( MALI_NO_HANDLE == mem_handle )
		{
			AERR( "Could not wrap dma_buf from ION with fd: %d", hnd->share_fd );
		}
#else
		AERR( "Unable to wrap ION buffer from handle 0x%x. No ION support in gralloc", (unsigned int)hnd );
#endif
	}
	else
	{
		MALI_DEBUG_ASSERT( NULL != (void*) physical_address, ("Null ptr physical address for hnd: %p", hnd ) );
		mem_handle = _mali_mem_add_phys_mem( base_ctx, physical_address, mem_size, (void*)(virtual_address - offset), MALI_PP_WRITE | MALI_PP_READ | MALI_CPU_READ | MALI_CPU_WRITE );
		ADBG(0, "create buffer from physical memory (0x%x + %i mem handle: 0x%x)", physical_address, offset, mem_handle);
	}

	return mem_handle;
}

EGLBoolean __egl_platform_initialize(void)
{
	hw_module_t const* pmodule = NULL;

	hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &pmodule);
	module = reinterpret_cast<gralloc_module_t const*>(pmodule);

#if EGL_SURFACE_LOCKING_ENABLED
	fd_umplock = open("/dev/umplock", O_RDWR );
#endif

	return EGL_TRUE;
}

void __egl_platform_terminate(void)
{
	module = NULL;
#if EGL_SURFACE_LOCKING_ENABLED
	if ( fd_umplock )
	{
		close( fd_umplock );
		fd_umplock = -1;
	}
#endif
}

EGLNativeDisplayType __egl_platform_default_display( void )
{
	return (EGLNativeDisplayType)EGL_DEFAULT_DISPLAY;
}

EGLBoolean __egl_platform_display_valid( EGLNativeDisplayType dpy )
{
	if ( dpy == EGL_DEFAULT_DISPLAY ) return EGL_TRUE;

	return EGL_FALSE;
}

EGLBoolean __egl_platform_init_display( EGLNativeDisplayType dpy, mali_base_ctx_handle base_ctx )
{
	MALI_IGNORE( dpy );
	MALI_IGNORE( base_ctx );

	return EGL_TRUE;
}

void __egl_platform_filter_configs( egl_display *dpy )
{
	egl_config *cfg = NULL;
	u32 iterator = 0;
	u32 i;

	/* update the visual id's for the configs */
	cfg = (egl_config *)__mali_named_list_iterate_begin( dpy->config, &iterator );
	while ( NULL != cfg )
	{
		unsigned int red_size, green_size, blue_size, alpha_size;
		EGLBoolean config_found = EGL_FALSE;
		EGLint native_visual_id = 0;

		if ( (cfg->surface_type & EGL_WINDOW_BIT) != 0 && cfg->native_visual_id == 0 )
		{
			m200_texel_format mali_texel_format = _mali_pixel_to_texel_format( cfg->pixel_format );

			for ( i = 0; i < (sizeof(android_format_mappings) / sizeof(android_format_mappings[0])); i++ )
			{
				if ( mali_texel_format == android_format_mappings[i].mali_pixel_format )
				{
					/* special case for RGBX internally treated as RGBA */
					if ( mali_texel_format == M200_TEXEL_FORMAT_ARGB_8888 && cfg->alpha_size == 0 ) native_visual_id = HAL_PIXEL_FORMAT_RGBX_8888;
#if EGL_ANDROID_recordable_ENABLE
					/*set RGBA8888 1x as a valid config for recordable surface, set EGL_SLOW_CONFIG caveat to indicate to render native window will result in color space conversion*/
					else if ( cfg->config_caveat == EGL_SLOW_CONFIG && cfg->recordable_android == 1)
					{
						native_visual_id = HAL_PIXEL_FORMAT_YV12;
					}
#endif
					else native_visual_id = android_format_mappings[i].android_pixel_format;

					/* update the visual id, if not already set */
					cfg->native_visual_id = native_visual_id;
					break;
				}
			}

		}

		cfg = (egl_config *)__mali_named_list_iterate_next( dpy->config, &iterator );
	}
}

EGLBoolean __egl_platform_surface_buffer_invalid( egl_surface* surface )
{
	EGLBoolean result = EGL_FALSE;

	/* buffer acquisition is done only for window surfaces */
	if ( MALI_EGL_WINDOW_SURFACE == surface->type )
	{
		int	valid = 0;
		android_platform_data *platform_data = (android_platform_data *)surface->platform;

		valid = _mali_sys_atomic_get( &platform_data->buffer_valid );
		MALI_DEBUG_ASSERT( EGL_FALSE == valid || EGL_TRUE == valid, \
		                   ( "surface %p buffer validity flag has invalid value (%d/0x%x)", \
		                     surface, valid, valid ) );
		result = valid ? EGL_FALSE : EGL_TRUE;
	}
	return result;
}

EGLBoolean __egl_platform_connect_surface( egl_surface *surface )
{
	EGLBoolean			result = EGL_TRUE;

	if ( MALI_EGL_WINDOW_SURFACE == surface->type )
	{
		android_native_window_t *native_win = static_cast<android_native_window_t*>(surface->win);
		android_platform_data *platform_data = (android_platform_data *)surface->platform;
		MALI_DEBUG_ASSERT_POINTER( surface->win );

		ADBG(0, "connecting to window %p\n", surface->win);

		if ( _mali_sys_atomic_get( &platform_data->dequeued_buffers ) == 0 )
		{
			native_window_connect(native_win, NATIVE_WINDOW_API_EGL);
			__egl_platform_begin_new_frame( surface );
		}
	}
	return result;
}

void __egl_platform_cancel_buffers( egl_surface *surface, mali_bool cancel_framebuffer )
{
	if ( MALI_EGL_WINDOW_SURFACE == surface->type )
	{
		u32 i;
		android_native_window_t *native_win = static_cast<android_native_window_t*>(surface->win);
		android_native_buffer_t *native_buf = NULL;
		android_platform_data *platform_data = (android_platform_data *)surface->platform;

		MALI_DEBUG_ASSERT_POINTER( surface->win );

		ADBG(0, "cancel buffers from window %p\n", surface->win);


		_egl_surface_release_all_dependencies( surface );

		for ( i=0; i<surface->num_buffers; i++ )
		{
			if ( surface->buffer[i].render_target != NULL )
			{
				if ( surface->buffer[i].flags == BUFFER_LOCKED || surface->buffer[i].flags == BUFFER_CLEAN )
				{
					mali_bool buffer_released = MALI_FALSE;
					native_buf = MALI_REINTERPRET_CAST(android_native_buffer_t*)surface->buffer[i].data;
					if ( MALI_FALSE == IS_FRAMEBUFFER( native_buf ) )
					{
						ADBG(0, "cancel UMP/dma_buf buffer at index %i buffer 0x%xn (flags: %i)\n", i, (int)surface->buffer[i].data, surface->buffer[i].flags );
						native_win->cancelBuffer( native_win, native_buf
#if MALI_ANDROID_API >= 17
							, -1
#endif
							);
						buffer_released = MALI_TRUE;
					}
					else if ( MALI_TRUE == cancel_framebuffer )
					{
						ADBG(0, "queue framebuffer at index %i buffer 0x%xn (flags: %i)\n", i, (int)surface->buffer[i].data, surface->buffer[i].flags );

						native_win->queueBuffer( native_win, native_buf
#if MALI_ANDROID_API >= 17
								, -1
#endif
								);
						buffer_released = MALI_TRUE;
					}

					if ( MALI_TRUE == buffer_released )
					{
						if (platform_data) _mali_sys_atomic_dec( &platform_data->dequeued_buffers );
						if ( NULL != surface->buffer[i].render_target ) _mali_surface_deref( surface->buffer[i].render_target );
						surface->buffer[i].render_target = NULL;
						surface->buffer[i].data = NULL;
						surface->buffer[i].flags = BUFFER_EMPTY;
						surface->buffer[i].fence_fd = -1;
					}
				}
			}
		}

		if ( platform_data && surface->frame_builder && (_mali_sys_atomic_get( &platform_data->dequeued_buffers ) == 0) )
		{
			native_win->common.decRef( &native_win->common );
			native_window_disconnect(native_win, NATIVE_WINDOW_API_EGL);
			ADBG(0, "disconnected from window 0x%x\n", (int)surface->win);
		}
	}
}

void __egl_platform_disconnect_surface( egl_surface *surface )
{
	/* cancel any pending buffers, except buffers using the framebuffer directly */
	if ( MALI_EGL_WINDOW_SURFACE == surface->type )
	{
		android_platform_data *platform_data = (android_platform_data *)surface->platform;

		if ( _mali_sys_atomic_get( &platform_data->dequeued_buffers ) == 0 )
		{
			android_native_window_t *native_win = static_cast<android_native_window_t*>(surface->win);

			native_window_disconnect(native_win, NATIVE_WINDOW_API_EGL);
			ADBG(0, "disconnected from window 0x%x\n", (int)surface->win);
		}
	}
}

mali_bool __egl_platform_surface_is_framebuffer( const egl_surface *surface )
{
	android_client_buffer *client_buffer = MALI_REINTERPRET_CAST(android_client_buffer *)(surface->client_buffer);
	android_native_buffer_t *buffer = client_buffer->native_buf;
	private_handle_t const* hnd;

	/* we might come here w/ a NULL buffer */
	if (!buffer)
	{
		AINF("Invalid buffer (NULL)! bailing out.");
		return EGL_FALSE;
	}

	hnd = reinterpret_cast<private_handle_t const*>(buffer->handle);

	if ( hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER ) return MALI_TRUE;

	return MALI_FALSE;
}

mali_bool __egl_platform_surface_is_deferred_release( const egl_surface * surface)
{
	/* deferred release is only necessary for render target if the surface is framebuffer
	 * for other window surfaces, the render target is always the one we dequeued from native window
	 * which ensures it is not used by surfaceflinger for compositing
	 */
	return __egl_platform_surface_is_framebuffer(surface);
}

void __egl_platform_begin_new_frame( egl_surface *surface )
{
#if MALI_ANDROID_API > 12
	_mali_frame_builder_set_acquire_output_callback( surface->frame_builder, MALI_REINTERPRET_CAST(mali_frame_cb_func)__egl_platform_dequeue_buffer, MALI_REINTERPRET_CAST(mali_frame_cb_param)surface);
	_mali_frame_builder_set_complete_output_callback( surface->frame_builder, MALI_REINTERPRET_CAST(mali_frame_cb_func)NULL, MALI_REINTERPRET_CAST(mali_frame_cb_param)NULL );
#endif
}

mali_surface* __egl_platform_create_surface_from_native_buffer( android_native_buffer_t *buffer, egl_surface *surface, mali_base_ctx_handle base_ctx )
{
	int page_size = sysconf( _SC_PAGESIZE );
	u32 physical_address = 0, virtual_address = 0, offset = 0, size = 0;
	void **vaddr;
	mali_mem_handle mem_handle = MALI_NO_HANDLE;
	mali_shared_mem_ref *mem_ref = NULL;
	mali_surface_specifier sformat;
	mali_pixel_format pixelformat = MALI_PIXEL_FORMAT_R5G6B5;
	u32 mem_offset = 0, mem_size = 0, bpp = 2;
	mali_surface *color_buffer = NULL;
	mali_bool red_blue_swap = MALI_FALSE;
	mali_bool reverse_order = MALI_FALSE;
	mali_bool is_framebuffer = MALI_FALSE;
	/* lock it for access, retrieve buffer information */
	if (private_handle_t::validate(buffer->handle) < 0)
	{
		AERR("invalid buffer handle given (0x%x)", (int)buffer->handle);
		return NULL;
	}

	ADBG(0, "creating a new surface from a buffer 0x%x size %i x %i\n", (int)buffer, buffer->width, buffer->height);

	if (native_buffer_get_info( buffer, &physical_address, &virtual_address, &offset, &size, &is_framebuffer ) == EGL_FALSE )
	{
		AERR("failed to retrieve buffer information (0x%x)", (int)buffer);
		return NULL;
	}

	vaddr = (void**)virtual_address;
	module->lock(module, buffer->handle, buffer->usage, 0, 0, buffer->width, buffer->height, vaddr);

	switch (buffer->format) {
		case HAL_PIXEL_FORMAT_BGRA_8888:
			pixelformat = MALI_PIXEL_FORMAT_A8R8G8B8;
			bpp = 4;
			break;

		case HAL_PIXEL_FORMAT_RGBA_8888:
			pixelformat = MALI_PIXEL_FORMAT_A8R8G8B8;
			red_blue_swap = MALI_TRUE;
			bpp = 4;
			break;

		case HAL_PIXEL_FORMAT_RGBX_8888:
			pixelformat = MALI_PIXEL_FORMAT_A8R8G8B8;
			red_blue_swap = MALI_TRUE;
			bpp = 4;
			break;

		case HAL_PIXEL_FORMAT_RGB_565:
			pixelformat = MALI_PIXEL_FORMAT_R5G6B5;
			bpp = 2;
			break;
			
#if !defined( NEXELL_FEATURE_KITKAT_USE )
		case HAL_PIXEL_FORMAT_RGBA_4444:
			pixelformat = MALI_PIXEL_FORMAT_A4R4G4B4;
			red_blue_swap = MALI_TRUE;
			reverse_order = MALI_TRUE;
			bpp = 2;
			break;

		case HAL_PIXEL_FORMAT_RGBA_5551:
			pixelformat = MALI_PIXEL_FORMAT_A1R5G5B5;
			red_blue_swap = MALI_TRUE;
			reverse_order = MALI_TRUE;
			bpp = 2;
			break;
#endif
		default:
			AERR("unknown pixel format given (0x%x)", buffer->format);
			module->unlock(module, buffer->handle);
			return NULL;
			break;
	}
	_mali_surface_specifier_ex( &sformat, buffer->width, buffer->height, buffer->stride*bpp,
								pixelformat, _mali_pixel_to_texel_format(pixelformat),
								MALI_PIXEL_LAYOUT_LINEAR, M200_TEXTURE_ADDRESSING_MODE_LINEAR, red_blue_swap,
								reverse_order, MALI_SURFACE_COLORSPACE_sRGB, MALI_SURFACE_ALPHAFORMAT_NONPRE, surface->config->alpha_size == 0 ? MALI_TRUE : MALI_FALSE );
	color_buffer = _mali_surface_alloc_empty( MALI_SURFACE_FLAG_DONT_MOVE, &sformat, base_ctx );
	if ( NULL == color_buffer )
	{
		AERR("failed to allocate empty mali surface for buffer (0x%x)", (int)buffer);
		module->unlock(module, buffer->handle);
		return NULL;
	}

	mem_size = size+offset;
	mem_size = (mem_size + (page_size-1)) & ~(page_size-1);

	mem_handle = native_buffer_wrap( base_ctx, buffer, physical_address, virtual_address, offset, mem_size );

	if ( MALI_NO_HANDLE == mem_handle )
	{
		AERR("failed to map physical memory (0x%x + %i)", physical_address, offset);
		_mali_surface_deref( color_buffer );
		module->unlock(module, buffer->handle);
		return NULL;
	}

	color_buffer->mem_ref = _mali_shared_mem_ref_alloc_existing_mem( mem_handle );
	if ( NULL == color_buffer->mem_ref )
	{
		AERR("failed to allocate shared memory reference from existing memory handle (0x%x)", (int)mem_handle);
		_mali_mem_free( mem_handle );
		_mali_surface_deref( color_buffer );
		module->unlock(module, buffer->handle);
		return NULL;
	}

	color_buffer->mem_offset = offset;
	color_buffer->datasize = _mali_mem_size_get( color_buffer->mem_ref->mali_memory ) - color_buffer->mem_offset;

	module->unlock(module, buffer->handle);

	ADBG(0, "allocated new buffer of size %i x %i", color_buffer->format.width, color_buffer->format.height);

	return color_buffer;
}

EGLBoolean __egl_platform_create_surface_window( egl_surface *surface, mali_base_ctx_handle base_ctx )
{
	android_platform_data *platform_data = (android_platform_data *)surface->platform;
	android_native_window_t *native_win = static_cast<android_native_window_t*>(surface->win);
	android_native_buffer_t *buffer = NULL;
	mali_surface *color_buffer[NUM_BUFFERS];
	int win_width, win_height, win_format, win_type = NATIVE_WINDOW_FRAMEBUFFER, j;
	unsigned int i;
	int win_min_undequeued = 0;
	enum mali_frame_builder_type type;
	int result = 0;
	MALI_DEBUG_ASSERT_POINTER( surface );

	native_window_set_usage(native_win, GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_2D | GRALLOC_USAGE_SW_READ_RARELY );
	native_win->query(native_win, NATIVE_WINDOW_WIDTH, &win_width);
	native_win->query(native_win, NATIVE_WINDOW_HEIGHT, &win_height );
	native_win->query(native_win, NATIVE_WINDOW_FORMAT, &win_format );

	/* Get the concrete type of the native window.
	 * Possible return values are NATIVE_WINDOW_FRAMEBUFFER, NATIVE_WINDOW_SURFACE, NATIVE_WINDOW_SURFACE_TEXTURE_CLIENT
	 */
	native_win->query(native_win, NATIVE_WINDOW_CONCRETE_TYPE, &win_type );
	surface->num_buffers = NUM_BUFFERS;
	surface->num_frames = NUM_BUFFERS + 1;
    
#if MALI_ANDROID_API > 13
	native_win->query(native_win, NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &win_min_undequeued);
	surface->num_buffers = ((win_min_undequeued + NUM_BUFFERS) > MAX_EGL_BUFFERS)? MAX_EGL_BUFFERS: (win_min_undequeued + NUM_BUFFERS);
	surface->num_frames = win_min_undequeued == 0 ? 2 : (surface->num_buffers - win_min_undequeued);
	native_window_set_buffer_count( native_win, surface->num_buffers);
#elif MALI_ANDROID_API > 8
	native_window_set_buffer_count( native_win, NUM_BUFFERS );
#else
#endif
	platform_data->max_allowed_dequeued_buffers = surface->num_frames;

	surface->caps = SURFACE_CAP_DIRECT_RENDERING;

	if ( NATIVE_WINDOW_FRAMEBUFFER == win_type ) type = MALI_FRAME_BUILDER_TYPE_EGL_COMPOSITOR;
	else type = MALI_FRAME_BUILDER_TYPE_EGL_WINDOW;

	/* set up the framebuilder, but do not create any buffers yet */
	surface->frame_builder = __egl_mali_create_frame_builder( base_ctx, surface->config, surface->num_frames, surface->num_buffers, NULL, MALI_FALSE, type );

	if ( NULL == surface->frame_builder )
	{
		AERR("failed to create framebuilder for surface (0x%x)", (int)surface);
		return EGL_FALSE;
	}

	for ( i=0; i<surface->num_buffers; i++ )
	{
		surface->buffer[i].render_target = NULL;
		surface->buffer[i].surface = surface;
		surface->buffer[i].id = i;
		surface->buffer[i].flags = BUFFER_EMPTY;
		surface->buffer[i].data = NULL;
		surface->buffer[i].fence_fd = -1;
	}

	/* EGL now has a reference to this window */

	native_window_connect(native_win, NATIVE_WINDOW_API_EGL);
	native_win->common.incRef( &native_win->common );

	__egl_platform_begin_new_frame( surface );

	_mali_sys_atomic_initialize(&platform_data->dequeued_buffers, 0);
	_mali_sys_atomic_initialize(&platform_data->buffer_valid, EGL_FALSE);

	ADBG(0, "window surface created (0x%x) size %i x %i", (int)surface, win_width, win_height );

	return EGL_TRUE;
}

EGLBoolean __egl_platform_create_surface_pbuffer( egl_surface *surface, mali_base_ctx_handle base_ctx )
{
	mali_surface *buf = NULL;
	mali_surface_specifier sformat;

	MALI_DEBUG_ASSERT_POINTER( surface );

	surface->frame_builder = NULL;

	__egl_surface_to_surface_specifier( &sformat, surface );

	if ( 0 == sformat.width ) sformat.width = 1;
	if ( 0 == sformat.height ) sformat.height = 1;

	/* if a pbuffer is created from a VGImage, we are presented with a valid surface */
	buf = (mali_surface*) surface->bound_texture_obj;

	if ( NULL == buf )
	{
		buf = _mali_surface_alloc( MALI_SURFACE_FLAGS_NONE, &sformat, 0, base_ctx );
		if ( NULL == buf ) return EGL_FALSE;
	}
	else
	{
		_mali_surface_addref( buf );
	}

	surface->frame_builder = __egl_mali_create_frame_builder( base_ctx, surface->config, NUM_BUFFERS, 1, &buf, MALI_TRUE, MALI_FRAME_BUILDER_TYPE_EGL_PBUFFER );

	if ( NULL == surface->frame_builder )
	{
		_mali_surface_deref(buf);
		return EGL_FALSE;
	}

	surface->buffer[0].render_target = buf;
	surface->buffer[0].surface = surface;
	surface->buffer[0].id = 0;
	surface->buffer[0].fence_fd = -1;
	return EGL_TRUE;
}

EGLBoolean __egl_platform_create_surface_pixmap( egl_surface* surface, mali_base_ctx_handle base_ctx )
{
#ifdef NEXELL_ANDROID_FEATURE_PIXMAP_USE
	mali_surface* buf = NULL;
	mali_surface_specifier sformat;

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	fbdev_pixmap* fb_pixmap = NULL;
	ump_handle pixmap_ump_handle = UMP_INVALID_MEMORY_HANDLE;
	mali_mem_handle pixmap_mem_handle = MALI_NO_HANDLE;
	mali_shared_mem_ref* pixmap_mem_ref = NULL;
#endif /* MALI_USE_UNIFIED_MEMORY_PROVIDER != 0 */

	MALI_DEBUG_ASSERT_POINTER( surface );

	__egl_surface_to_surface_specifier( &sformat, surface );

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap*)surface->pixmap;
	MALI_DEBUG_ASSERT_POINTER( fb_pixmap );
	if ( FBDEV_PIXMAP_SUPPORTS_UMP & fb_pixmap->flags )
	{
		__egl_main_context *egl = NULL;
		egl = __egl_get_main_context();

		/* UMP-pixmap */
		pixmap_ump_handle = MALI_REINTERPRET_CAST(ump_handle)fb_pixmap->data;
		if ( UMP_INVALID_MEMORY_HANDLE == pixmap_ump_handle ) return EGL_FALSE;

		pixmap_mem_handle = _mali_mem_wrap_ump_memory( base_ctx, pixmap_ump_handle, 0 );
		if ( MALI_NO_HANDLE == pixmap_mem_handle )
		{
			return EGL_FALSE;
		}

		pixmap_mem_ref = _mali_shared_mem_ref_alloc_existing_mem( pixmap_mem_handle );
		if ( NULL == pixmap_mem_ref )
		{
			_mali_mem_free( pixmap_mem_handle );
			return EGL_FALSE;
		}

		buf = _mali_surface_alloc_ref( MALI_SURFACE_FLAG_DONT_MOVE, &sformat, pixmap_mem_ref, 0 , egl->base_ctx );
		if ( NULL == buf )
		{
			_mali_shared_mem_ref_usage_deref( pixmap_mem_ref );
			return EGL_FALSE;
		}
	}
	else
#endif /* MALI_USE_UNIFIED_MEMORY_PROVIDER != 0 */
	{
		/* Not a UMP-pixmap */
		buf = _mali_surface_alloc( MALI_SURFACE_FLAGS_NONE, &sformat, 0, base_ctx );
		if ( NULL == buf ) return EGL_FALSE;
	}

	surface->frame_builder = __egl_mali_create_frame_builder( base_ctx, surface->config, 2, 1, &buf, MALI_FALSE, MALI_FRAME_BUILDER_TYPE_EGL_PIXMAP );
	if ( NULL == surface->frame_builder )
	{
		_mali_surface_deref( buf );
		return EGL_FALSE;
	}

	surface->buffer[0].render_target = buf;
	surface->buffer[0].surface = surface;
	surface->buffer[0].id = 0;

	return EGL_TRUE;

#else
	MALI_IGNORE( surface );
	MALI_IGNORE( base_ctx );

	AERR("no support for pixmap surfaces (0x%x)", (int)surface);

	return EGL_FALSE;
#endif	
}

EGLBoolean __egl_platform_create_surface( egl_surface *surface, mali_base_ctx_handle base_ctx )
{
	android_platform_data *platform_data = NULL;
	android_client_buffer *client_buffer = NULL;
	MALI_DEBUG_ASSERT_POINTER( surface );

	surface->platform = NULL;
	surface->client_buffer = NULL;

	platform_data = (android_platform_data *) _mali_sys_malloc( sizeof(android_platform_data) );
	if ( NULL == platform_data )
	{
		AERR("unable to allocate platform data for surface (0x%x)", (int)surface);
		return EGL_FALSE;
	}
	client_buffer = (android_client_buffer *)_mali_sys_malloc( sizeof(android_client_buffer) );
	if ( NULL == client_buffer )
	{
		 _mali_sys_free( platform_data );
		AERR("unable to allocate client buffer for surface ( 0x%x)", (int)surface);
		return EGL_FALSE;
	}
	client_buffer->native_buf = NULL;
	client_buffer->fence_fd = -1;
	surface->num_buffers = 1;
	surface->copy_func = __egl_platform_copy_buffers;
	surface->platform = platform_data;
	surface->client_buffer = client_buffer;
	switch ( surface->type )
	{
		case MALI_EGL_WINDOW_SURFACE:  return __egl_platform_create_surface_window( surface, base_ctx );
		case MALI_EGL_PBUFFER_SURFACE: return __egl_platform_create_surface_pbuffer( surface, base_ctx );
		case MALI_EGL_PIXMAP_SURFACE:  return __egl_platform_create_surface_pixmap( surface, base_ctx );
		default:
			break;
	}

	if ( NULL != surface->platform) _mali_sys_free(surface->platform);
	surface->platform = NULL;
	if ( NULL != surface->client_buffer) _mali_sys_free(surface->client_buffer);
	surface->client_buffer = NULL;
	return EGL_FALSE;
}

void __egl_platform_destroy_surface(egl_surface *surface)
{
	MALI_DEBUG_ASSERT_POINTER( surface );

	ADBG(0, "window surface destroy (0x%x)", (int)surface);

	if ( surface->win != NULL )
	{
		android_native_window_t *native_win = (android_native_window_t *)surface->win;
		__egl_platform_cancel_buffers( surface, MALI_TRUE );
	}

	if( NULL != surface->platform ) _mali_sys_free(surface->platform);
	if( NULL != surface->client_buffer) _mali_sys_free( surface->client_buffer);
	if( NULL != surface->frame_builder ) __egl_mali_destroy_frame_builder( surface );

	surface->frame_builder = NULL;
	surface->platform = NULL;
	surface->client_buffer = NULL;
}

/* will not be called for this backend, since resize is done in platform layer */
EGLBoolean __egl_platform_resize_surface( egl_surface *surface, u32 *width, u32 *height, mali_base_ctx_handle base_ctx )
{
	MALI_IGNORE( surface );
	MALI_IGNORE( width );
	MALI_IGNORE( height );
	MALI_IGNORE( base_ctx );

	return EGL_TRUE;
}


EGLBoolean __egl_platform_resize_surface_android( egl_surface *surface, u32 *width, u32 *height, android_buffer_state buffer_state, mali_base_ctx_handle base_ctx )
{
	mali_surface           *new_color_target[NUM_BUFFERS];
	mali_surface           *old_color_target[NUM_BUFFERS];
	android_native_window_t *native_win = (android_native_window_t *)surface->win;
	android_native_buffer_t *buffer = NULL;
	u32 i, usage = 0;
	__egl_main_context *main_ctx = __egl_get_main_context();
	android_client_buffer *client_buffer = NULL;
	int fence_fd = -1;
#if GRALLOC_ARM_DMA_BUF_MODULE && DMA_BUF_ON_DEMAND
	private_handle_t const* hnd;
#endif
	MALI_DEBUG_ASSERT_POINTER( surface );

	if ( !(surface->caps & SURFACE_CAP_DIRECT_RENDERING) ) return EGL_TRUE;

	ADBG(0, "resize triggered for surface (0x%x)", surface);

	/* */
	for ( i=0; i<surface->num_buffers; i++ )
	{
		new_color_target[i] = old_color_target[i] = NULL;
	}

	_egl_surface_release_all_dependencies( surface );

	for ( i=0; i<surface->num_buffers; i++ ) old_color_target[i] = surface->buffer[i].render_target;

	client_buffer = MALI_REINTERPRET_CAST(android_client_buffer *)surface->client_buffer;
	buffer = client_buffer->native_buf;
	fence_fd = client_buffer->fence_fd;
	if(!buffer)
	{
		AINF("Invalid native buffer (NULL)! resize ignored");
		return EGL_FALSE;
	}

	new_color_target[0] = __egl_platform_create_surface_from_native_buffer( buffer, surface, base_ctx );
	if ( NULL == new_color_target[0] )
	{
		AERR("unable to allocate render target for buffer (0x%x)", (int)buffer);
		return EGL_FALSE;
	}

	if ( STATE_RECREATE_TRIPLE_BUFFER == buffer_state )
	{
		ADBG(0, "triple-buffer resize (0x%x)", buffer);
		new_color_target[1] = old_color_target[1];
		new_color_target[2] = old_color_target[0];
		old_color_target[0] = NULL; /* make sure not to free the previous targets */
		old_color_target[1] = NULL; /* make sure not to free the previous targets */

		surface->buffer[2].flags = surface->buffer[0].flags;
		surface->buffer[2].data = surface->buffer[0].data;
		surface->buffer[2].fence_fd = surface->buffer[0].fence_fd;
	}
	else if ( STATE_RECREATE_DOUBLE_BUFFER == buffer_state )
	{
		ADBG(0, "double-buffer resize (0x%x)", buffer);
		new_color_target[1] = old_color_target[0];
		old_color_target[0] = NULL; /* make sure not to free the previous target */

		surface->buffer[1].flags = surface->buffer[0].flags;
		surface->buffer[1].data = surface->buffer[0].data;
		surface->buffer[1].fence_fd = surface->buffer[0].fence_fd;

		if ( surface->num_buffers > 2 ) surface->buffer[2].flags = BUFFER_EMPTY;
	}
	else if ( STATE_RECREATE_SINGLE_BUFFER == buffer_state )
	{
		ADBG(0, "single-buffer resize (0x%x)", buffer);
		surface->buffer[1].flags = surface->buffer[0].flags = BUFFER_EMPTY;
		if ( surface->num_buffers > 2 ) surface->buffer[2].flags = BUFFER_EMPTY;
	}

	surface->buffer[0].data = buffer;
	surface->buffer[0].fence_fd = fence_fd;
	surface->width = buffer->width;
	surface->height = buffer->height;
	surface->current_buffer = 0;

#if GRALLOC_ARM_DMA_BUF_MODULE && DMA_BUF_ON_DEMAND
	hnd = reinterpret_cast<private_handle_t const*>(buffer->handle);
	MALI_DEBUG_ASSERT_POINTER( hnd );
	if( hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION)
	{
		new_color_target[0]->flags |= MALI_SURFACE_FLAG_TRACK_SURFACE;
	}
#endif

	for ( i=0; i<surface->num_buffers; i++ )
	{
		if ( NULL != old_color_target[i] ) _mali_surface_deref( old_color_target[i] );
		surface->buffer[i].render_target = new_color_target[i];
	}

	_mali_frame_builder_get_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, &usage );
	_mali_frame_builder_set_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, surface->buffer[surface->current_buffer].render_target, usage);
	_mali_frame_builder_reset( surface->frame_builder );

#if EGL_ANDROID_ENABLE_DEBUG
	for ( i=0; i<surface->num_buffers; i++)
	{
		android_native_buffer_t *native_buf = NULL;
		if ( NULL != surface->buffer[i].render_target ) native_buf = MALI_REINTERPRET_CAST(android_native_buffer_t*)surface->buffer[i].data;
		ADBG(0, "   buffer 0x%x at index %i has flags %i (render_target: 0x%x)\n", (int)native_buf, i, surface->buffer[i].flags, surface->buffer[i].render_target);
	}
#endif /* EGL_ANDROID_ENABLE_DEBUG */

#if MALI_ANDROID_API >= 17 && EGL_ANDROID_native_fencesync_ENABLE	
	/* we need to wait for fence to be signaled before start pp job*/
	_mali_frame_builder_set_output_fence( surface->frame_builder, (client_buffer->fence_fd >= 0 )? mali_fence_import_fd(client_buffer->fence_fd) :MALI_FENCE_INVALID_HANDLE );
#endif

	return EGL_TRUE;
}

void __egl_platform_queue_buffer( mali_base_ctx_handle base_ctx, egl_buffer *buffer )
{
	android_native_window_t *native_win = MALI_REINTERPRET_CAST(android_native_window_t *)buffer->surface->win;
	android_native_buffer_t *native_buf = MALI_REINTERPRET_CAST(android_native_buffer_t *)buffer->data;
	android_platform_data *platform_data = (android_platform_data *)buffer->surface->platform;

#if EGL_ANDROID_LOG_TIME
	u32 time_start, time_end;
	time_start = (u32)_mali_sys_get_time_usec();
#endif

#if EGL_ANDROID_recordable_ENABLE
	if ( EGL_TRUE == __egl_recordable_color_conversion_needed( native_win, buffer->surface))
	{
		if (private_handle_t::validate(native_buf->handle) < 0)
		{
			AERR("invalid buffer handle given (0x%x)", (int)native_buf->handle);
			return;
		}
		void *yuv_vaddr;
		module->lock(module, native_buf->handle, native_buf->usage, 0, 0, native_buf->width, native_buf->height, (void **)&yuv_vaddr);
		__egl_platform_convert_color_space_to_native((void *)native_buf, (void *)buffer->render_target , (void *)yuv_vaddr);

#if 0
	   /* keep it for pure debug usage*/
		u32 size = native_buf->width * native_buf->height +( ((native_buf->stride / 2) + 0xf) & ~0xf) * native_buf->height ;
		void *outbuf = _mali_sys_malloc(size);
		_mali_sys_memcpy(outbuf,yuv_vaddr,size);
		_mali_sys_free(outbuf);
#endif

		module->unlock(module, native_buf->handle);
	}
#endif

#if MALI_TIMELINE_PROFILING_ENABLED
    _mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_SUSPEND|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_SUSPEND_RESUME_SW_ICS_QUEUE_BUFFER, 0, 0, (u32)native_win, (u32)native_buf, 0);
#endif
	ADBG(3, "queue 0x%x win 0x%x width=%d height=%d \n", native_buf, native_win, native_buf->width, native_buf->height);
	if ( native_win->queueBuffer( native_win, native_buf
#if MALI_ANDROID_API >= 17
				, -1
#endif
				) < 0 )
	{
		AERR("unable to queue buffer (0x%x)", (int)native_buf);
	}
	ADBG(1, "queued 0x%x width=%d height=%d (render target: 0x%x)\n", native_buf, native_buf->width, native_buf->height, buffer->render_target);

#if MALI_TIMELINE_PROFILING_ENABLED
    _mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_RESUME|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_SUSPEND_RESUME_SW_ICS_QUEUE_BUFFER, 0, 0, (u32)native_win, (u32)native_buf, 0);
#endif

#if EGL_ANDROID_LOG_TIME
	time_end = (u32)_mali_sys_get_time_usec();
	ADBG(3, "TIME QUEU %5i us\n", time_end-time_start);
#endif

	buffer->flags = BUFFER_QUEUED;
	_mali_sys_atomic_dec( &platform_data->dequeued_buffers );
}

void __egl_platform_surface_wait_and_reset( egl_surface *surface, mali_bool reset )
{
	unsigned int i;

	_egl_surface_release_all_dependencies( surface );

	if ( MALI_TRUE == reset )
	{
		/* reset the list of buffers */
		for ( i=0; i<surface->num_buffers; i++ )
		{
			if ( surface->buffer[i].render_target != NULL
				&& surface->buffer[i].render_target->mem_ref!= NULL
				&& surface->buffer[i].render_target->mem_ref->mali_memory != NULL)
					_mali_surface_deref( surface->buffer[i].render_target );
			surface->buffer[i].render_target = NULL;
			surface->buffer[i].surface = surface;
			surface->buffer[i].id = i;
			surface->buffer[i].flags = BUFFER_EMPTY;
			surface->buffer[i].data = NULL;
			surface->buffer[i].fence_fd = -1;
		}
	}
}

int __egl_platform_surface_get_buffer_index( egl_surface *surface, android_native_buffer_t *native_buffer )
{
	unsigned int i;

	for ( i=0; i<surface->num_buffers; i++ )
	{
		if ( surface->buffer[i].data == native_buffer ) return i;
	}

	return -1;
}

#if MALI_TEST_API

/* Match string against a pattern. Supports simple wildcard ('*') matching */
MALI_STATIC mali_bool __match_string(const char* string, const char* pattern)
{
	mali_bool result = MALI_FALSE;

	if (!string || !pattern)
	{
		return MALI_FALSE;
	}

	result = MALI_TRUE;

	while (*string && *pattern)
	{
		if (*pattern == '*')
		{
			pattern++;
			/* skip input until in sync w/ pattern */
			while (*string && (*pattern != *string))
				string++;

			/* mismatch */
			if (*pattern != *string)
				break;
		}
		else if (*pattern != *string)
		{
			/* mismatch */
			break;
		}
		else
		{
			/* match; proceed */
			pattern++;
			string++;
		}
	}

	if (*string != *pattern)
	{
		result = MALI_FALSE;
	}
	return result;
}

/* Inject failures to buffer dequeueing. Controlled by two system properties, "mali.egl.first_fail"
 * and "mali.egl.fail_period". To enable simulated failures, give these properties some > 0
 * values, say N and M, and start the application you wish to test. Failure will first happen on
 * iteration N, and then after every M iterations */
MALI_STATIC mali_bool __egl_platform_should_fail_dequeue_buffer()
{

	static unsigned int 			call_count;
	unsigned int					first_fail, fail_period;
	mali_bool						fail = MALI_FALSE, precond = MALI_TRUE;
	char							target[PROPERTY_VALUE_MAX];

	++call_count;

	/* read the system properties that control failure simulation */
	{
		char prop_value[PROPERTY_VALUE_MAX];

		if (property_get(PROP_MALI_TEST_FAIL_FIRST, prop_value, "0") > 0)
		{
			sscanf(prop_value, "%u", &first_fail);
		}

		if (property_get(PROP_MALI_TEST_FAIL_INTERVAL, prop_value, "0") > 0)
		{
			sscanf(prop_value, "%u", &fail_period);
		}

		/* check possible name filter */
		if (property_get(PROP_MALI_TEST_FAIL_PROCESS, prop_value, "0") > 0)
		{
			FILE* handle;

			/* open the command line file for reading the process name */
			{
				char filename[32];
				sprintf(filename, "/proc/%u/cmdline", _mali_sys_get_pid());
				handle = fopen(filename, "r");
			}

			if (handle)
			{
				char cmdline[128];
				int len;

				/* read the process name */
				len = fread(cmdline, 1, 127, handle);
				fclose(handle);

				if (len > 0)
				{
					cmdline[len] = 0;
					/* match against the filter */
					precond = __match_string(cmdline, prop_value);
				}
				fclose(handle);
			}
		}
	}


	if (first_fail > 0 && precond)
	{
		AINF("iteration %u (fail=%u, period=%u)\n", call_count, first_fail, fail_period);

		fail = 	(call_count == first_fail) ||
				(call_count > first_fail && fail_period > 0 && 0 == (call_count - first_fail) % fail_period);

		fail = fail && precond;

		if (fail)
		{
			AINF("failed dequeueBuffer on iteration #%d\n", call_count);
		}

	}
	return fail;
}
#endif

void __egl_platform_dequeue_buffer( egl_surface *surface )
{
	android_native_window_t *native_win = NULL;
	android_native_buffer_t *native_buf = NULL;
	android_platform_data *platform_data = (android_platform_data *)surface->platform;
	mali_surface *target = NULL, *new_target = NULL;
	egl_buffer *callback_data = NULL;
	int errnum;
	int buffer_index = 0;
	int win_width = 0, win_height = 0;
	int surface_width = 0, surface_height = 0;
	mali_bool need_reset = MALI_FALSE;
	u32 usage = 0, i;
	__egl_main_context *main_ctx = __egl_get_main_context();
	mali_base_ctx_handle base_ctx = main_ctx->base_ctx;
	android_buffer_state buffer_state = STATE_OK;
	android_client_buffer *client_buffer = NULL;
#if GRALLOC_ARM_DMA_BUF_MODULE && DMA_BUF_ON_DEMAND
	private_handle_t const* hnd;
#endif

#if EGL_ANDROID_LOG_TIME
	u32 time_start, time_end;
#endif
	int fence_fd = -1;
	__egl_thread_state *tstate = NULL;

	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_NOP );

	native_win = MALI_REINTERPRET_CAST(android_native_window_t *)surface->win;
	MALI_DEBUG_ASSERT_POINTER( native_win );



	if ( NULL == native_win )
	{
		AERR("native window is NULL for surface (0x%x)", (int)surface);
		return;
	}

#if MALI_ANDROID_API > 13
	while ( _mali_sys_atomic_get( &platform_data->dequeued_buffers ) >= platform_data->max_allowed_dequeued_buffers )
	{
		ADBG(1,"dequeued too many buffers, must throttle (%i dequeued)", _mali_sys_atomic_get(&platform_data->dequeued_buffers));
		_mali_sys_yield();
	}
#endif

#if EGL_ANDROID_LOG_TIME
	time_start = (u32)_mali_sys_get_time_usec();
#endif

	ADBG(1, "dequeue win 0x%x", native_win);

#if MALI_TEST_API
	if (__egl_platform_should_fail_dequeue_buffer())
	{
		errnum = -1;

		/* NOTE: possible side-effects of dequeueBuffer() are not simulated */
	}
	else
#endif

#if MALI_TIMELINE_PROFILING_ENABLED
    _mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_SUSPEND|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_SUSPEND_RESUME_SW_ICS_DEQUEUE_BUFFER, 0, 0, (u32)native_win, (u32)native_buf, 0);
#endif
    
	errnum = native_win->dequeueBuffer( native_win, &native_buf
#if MALI_ANDROID_API >= 17
			, &fence_fd 
#endif
			);
	if ( NULL != native_buf && errnum >= 0 )
	{
		ADBG(3, "dequeued 0x%x win 0x%x width=%d height=%d\n", native_buf, native_win, native_buf->width, native_buf->height);
	}

	#if MALI_TIMELINE_PROFILING_ENABLED
    _mali_base_profiling_add_event(VR_PROFILING_EVENT_TYPE_RESUME|VR_PROFILING_EVENT_CHANNEL_SOFTWARE|VR_PROFILING_EVENT_REASON_SUSPEND_RESUME_SW_ICS_DEQUEUE_BUFFER, 0, 0, (u32)native_win, (u32)native_buf, 0);
#endif

#if EGL_ANDROID_LOG_TIME
	time_end = (u32)_mali_sys_get_time_usec();
	ADBG(3, "TIME DEQU %5i us", time_end-time_start);
#endif

	target = _mali_frame_builder_get_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, &usage);

	if (errnum < 0 || NULL == native_buf)
	{
		int win_type = 1;
	    int	win_min_undeq_num = 0;
		int err = -1;
#if MALI_ANDROID_API >= 17
		err = native_win->query(native_win, NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER, &win_type );
		if (err == 0 && win_type == 0)
		{
			/* Compositor surface: adjust the max allowed dequeue buffer number for it.
			 max_allowed_dequeued_buffers = framebuffer number - compositor window win_min_undeq_num,
			*/
			err = native_win->query(native_win, NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &win_min_undeq_num);
			if (err == 0)
			{
				MALI_DEBUG_ASSERT( win_min_undeq_num < NUM_FB_BUFFERS, (" FATAL: NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS exceeds NUM_FB_BUFFERS for an EGL Surface" ));
				if( win_min_undeq_num < NUM_FB_BUFFERS )
					platform_data->max_allowed_dequeued_buffers = NUM_FB_BUFFERS - win_min_undeq_num;
			}
		}
#endif
		ADBG(3,"failed to dequeue buffer from native window (0x%x); err = %d, buf = %p, window type %s, max_allowed_dequeued_buffers %d\n", (int)native_win, errnum, native_buf, (win_type == 0)?"frame_buffer window":"application window" , platform_data->max_allowed_dequeued_buffers);
		/*_mali_frame_builder_set_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, NULL, usage);*/
		
		_mali_sys_atomic_set( &platform_data->buffer_valid, EGL_FALSE );
		_mali_frame_builder_reset( surface->frame_builder );

		return;
	}

	_mali_sys_atomic_inc( &platform_data->dequeued_buffers );
	_mali_sys_atomic_set( &platform_data->buffer_valid, EGL_TRUE );
	
	MALI_DEBUG_ASSERT_POINTER(surface->client_buffer);
	client_buffer = (android_client_buffer *)surface->client_buffer;
	client_buffer->native_buf = native_buf;
	client_buffer->fence_fd = fence_fd;

#if MALI_ANDROID_API > 13
	native_win->query(native_win, NATIVE_WINDOW_WIDTH, &win_width);
	native_win->query(native_win, NATIVE_WINDOW_HEIGHT, &win_height );

	surface_width = (int)surface->width;
	surface_height = (int)surface->height;

	if ( surface_width != win_width || surface_height != win_height || native_buf->width != surface_width || native_buf->height != surface_height )
	{
		ADBG(0, "resize on window 0x%x", native_win);
		__egl_platform_surface_wait_and_reset( surface, MALI_TRUE );
		need_reset = MALI_TRUE;
	}

	buffer_index = __egl_platform_surface_get_buffer_index( surface, native_buf );

	if ( buffer_index < 0 )
	{
		/* We got a buffer we have not seen before - reset the buffer slots if we're already full */
		for ( i=0; i<surface->num_buffers; i++ ) if ( surface->buffer[i].render_target == NULL ) break;
		__egl_platform_surface_wait_and_reset( surface, i == surface->num_buffers ? MALI_TRUE : MALI_FALSE );

#if EGL_ANDROID_recordable_ENABLE
		if( EGL_TRUE == __egl_recordable_color_conversion_needed(native_win,surface))
		{
			new_target = __egl_platform_create_recordable_surface_from_native_buffer(native_buf, surface, base_ctx);
		}
		else
		{
			new_target = __egl_platform_create_surface_from_native_buffer( native_buf, surface, base_ctx );
		}
#else
		new_target = __egl_platform_create_surface_from_native_buffer( native_buf, surface, base_ctx );
#endif

		if ( NULL == new_target )
		{
#if MALI_ANDROID_API >= 17 && EGL_ANDROID_native_fencesync_ENABLE
			if ( fence_fd >= 0 )
			{
				/*release the fence*/
				mali_fence_release( mali_fence_import_fd( fence_fd ));
			}
#endif
			AERR("Failed to create a surface from native buffer (%p)", native_buf);
			/* we are now in a state where we have dequeued a buffer from android - but failed to wrap this into a renderable surface */
			/* cancel the buffer if possible, and mark the buffer as invalid */

			if ( MALI_FALSE == IS_FRAMEBUFFER( native_buf ) )
				native_win->cancelBuffer( native_win, native_buf
#if MALI_ANDROID_API >= 17
						, -1
#endif
						);
			else native_win->queueBuffer( native_win, native_buf
#if MALI_ANDROID_API >= 17
					, -1
#endif
					);

			_mali_sys_atomic_set( &platform_data->buffer_valid, EGL_FALSE );
			_mali_sys_atomic_dec( &platform_data->dequeued_buffers );
			_mali_frame_builder_reset( surface->frame_builder );
			if( surface->client_buffer)
				_mali_sys_free( surface->client_buffer );
			surface->client_buffer = NULL;
			return;
		}

		/* find an available slot for the buffer */
		for ( i=0; i<surface->num_buffers; i++ ) if ( surface->buffer[i].render_target == NULL ) break;

		buffer_index = i;

		surface->buffer[buffer_index].data = native_buf;
		surface->buffer[buffer_index].render_target = new_target;
#if MALI_ANDROID_API >= 17
		surface->buffer[buffer_index].fence_fd = fence_fd;
#else
		surface->buffer[buffer_index].fence_fd = -1;
#endif
		surface->width = native_buf->width;
		surface->height = native_buf->height;
		need_reset = MALI_TRUE;
	}

	surface->current_buffer = buffer_index;

	callback_data = &surface->buffer[surface->current_buffer];
	callback_data->flags = BUFFER_CLEAN;
	callback_data->swap_worker = tstate->worker;

	_mali_frame_builder_set_lock_output_callback( surface->frame_builder, MALI_REINTERPRET_CAST(mali_frame_cb_func)NULL, MALI_REINTERPRET_CAST(mali_frame_cb_param)NULL, NULL );
	_mali_frame_builder_set_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, surface->buffer[surface->current_buffer].render_target, usage);

#if MALI_ANDROID_API >= 17 && EGL_ANDROID_native_fencesync_ENABLE
	_mali_frame_builder_set_output_fence( surface->frame_builder, (client_buffer->fence_fd >= 0 )? mali_fence_import_fd(client_buffer->fence_fd) :MALI_FENCE_INVALID_HANDLE );
#endif

#if GRALLOC_ARM_DMA_BUF_MODULE && DMA_BUF_ON_DEMAND
	hnd = reinterpret_cast<private_handle_t const*>(native_buf->handle);
	MALI_DEBUG_ASSERT_POINTER(hnd);
	if( hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION)
	{
		callback_data->render_target->flags |= MALI_SURFACE_FLAG_TRACK_SURFACE;
	}
#endif
	if ( MALI_TRUE == need_reset ) _mali_frame_builder_reset( surface->frame_builder );

#else

	if ( MALI_TRUE == IS_FRAMEBUFFER( native_buf ) ) surface->num_buffers = 2;

	if ( NULL == target || native_buf->width != target->format.width || native_buf->height != target->format.height ) buffer_state = STATE_RECREATE_SINGLE_BUFFER;
	else if ( NULL == surface->buffer[0].render_target ) buffer_state = STATE_RECREATE_SINGLE_BUFFER;
	else if ( NULL == surface->buffer[1].render_target ) buffer_state = STATE_RECREATE_DOUBLE_BUFFER;
	else if ( surface->num_buffers > 2 && NULL == surface->buffer[2].render_target ) buffer_state = STATE_RECREATE_TRIPLE_BUFFER;

	if ( STATE_OK != buffer_state ) __egl_platform_resize_surface_android( surface, 0, 0, buffer_state, base_ctx );
	else
	{
		mali_bool native_buffers_changed = MALI_TRUE;

		/* the native buffers can change */
		for ( i = 0; i<surface->num_buffers; i++ ) if ( native_buf == surface->buffer[i].data ) native_buffers_changed = MALI_FALSE;
		if ( MALI_TRUE == native_buffers_changed ) __egl_platform_resize_surface_android( surface, 0, 0, STATE_RECREATE_SINGLE_BUFFER, base_ctx );
	}

	callback_data = &surface->buffer[surface->current_buffer];
	callback_data->flags = BUFFER_CLEAN;

	_mali_frame_builder_set_lock_output_callback( surface->frame_builder, MALI_REINTERPRET_CAST(mali_frame_cb_func)__egl_platform_lock_buffer, MALI_REINTERPRET_CAST(mali_frame_cb_param)callback_data, &callback_data->ds_consumer_pp_render );
	_mali_frame_builder_set_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, surface->buffer[surface->current_buffer].render_target, usage);
#endif

	/* Should the buffer acquisition fail and the callback is reset earlier than here, in Launcher's case, this will lead
	 * to situation, where nothing is drawn, since the callback is set when new frame is initialized. This only happens
	 * when surface is created or set as active draw surface. Launcher's surface is created when it's first started and
	 * activation happens only randomly.
	 */
	/* reset the callback so that we don't try to dequeue twice for the same frame */
	_mali_frame_builder_set_acquire_output_callback( surface->frame_builder, MALI_REINTERPRET_CAST(mali_frame_cb_func)NULL, MALI_REINTERPRET_CAST(mali_frame_cb_param)NULL );


	if ( _mali_sys_atomic_get(&surface->do_readback) )
	{
		mali_err_code err;
		_mali_sys_atomic_set(&surface->do_readback, 0);
		MALI_DEBUG_ASSERT( NULL != surface->internal_target, ( "There must be an internal_target to read back from at this point" ) );
		err = _mali_frame_builder_write_lock( surface->frame_builder, MALI_FRAME_BUILDER_BUFFER_BIT_COLOR_ALL_CHANNELS );
		MALI_IGNORE( err );
		_mali_frame_builder_write_unlock( surface->frame_builder );

		__egl_mali_readback_surface( surface, surface->internal_target, usage, 0, 0 );

		/* In case swap_behavior was set to buffer destroyed between eglSwapBuffers and this point we have
		 * to keep the internal_target . Thus, we will dereference it and set the internal_target to NULL
		 * if we are not doing writeback conversion */
		if ( EGL_BUFFER_DESTROYED == surface->swap_behavior && 0 == ( surface->caps & SURFACE_CAP_WRITEBACK_CONVERSION ) )
		{
			_mali_surface_deref(surface->internal_target);
			surface->internal_target = NULL;
		}
	}
}

void __egl_platform_lock_buffer_worker( egl_buffer *buffer )
{
	android_native_buffer_t *native_buf = NULL;
	android_native_window_t *native_win = NULL;
#if EGL_ANDROID_LOG_TIME
	u32 time_start, time_end;
#endif

	native_win = MALI_REINTERPRET_CAST(android_native_window_t*)buffer->surface->win;
	native_buf = MALI_REINTERPRET_CAST(android_native_buffer_t*)buffer->data;

#if EGL_ANDROID_LOG_TIME
	time_start = (u32)_mali_sys_get_time_usec();
#endif

	ADBG(1, "locking 0x%x", native_buf);
#if MALI_ANDROID_API < 17
	native_win->lockBuffer( native_win, native_buf );
#endif
	ADBG(1, "locked 0x%x (render_target: 0x%x)\n", native_buf, buffer->render_target);

#if EGL_ANDROID_LOG_TIME
	time_end = (u32)_mali_sys_get_time_usec();
	ADBG(3, "TIME LOCK %5i us\n", time_end-time_start);
#endif

	mali_ds_consumer_activation_ref_count_change(buffer->ds_consumer_pp_render, MALI_DS_REF_COUNT_RELEASE);
	buffer->flags = BUFFER_LOCKED;
}

void __egl_platform_lock_buffer( egl_buffer *buffer )
{
	android_native_buffer_t *native_buf = NULL;
	android_native_window_t *native_win = NULL;
#if EGL_ANDROID_LOG_TIME
	u32 time_start, time_end;
#endif
	__egl_thread_state *tstate = NULL;

	MALI_DEBUG_ASSERT_POINTER( buffer );

	native_win = MALI_REINTERPRET_CAST(android_native_window_t*)buffer->surface->win;
	native_buf = MALI_REINTERPRET_CAST(android_native_buffer_t*)buffer->data;

	if ( buffer->flags == BUFFER_LOCKED ) return;

	/* offload to thread-specific worker thread */
	tstate = __egl_get_current_thread_state( EGL_MAIN_MUTEX_NOP );

	/* add a dependency to PP job start, to that the PP jobs does not start until this dependency is met */
	mali_ds_consumer_activation_ref_count_change(buffer->ds_consumer_pp_render, MALI_DS_REF_COUNT_GRAB);

	if ( MALI_ERR_NO_ERROR != _mali_base_worker_task_add( tstate->worker, MALI_REINTERPRET_CAST(mali_base_worker_task_proc)__egl_platform_lock_buffer_worker, buffer ) )
	{
		AERR("failed to add lock buffer job - performing manual locking of buffer (0x%x)", (int)buffer);
		__egl_platform_lock_buffer_worker( buffer );
	}

	/* make sure we are not calling it more times */
	_mali_frame_builder_set_lock_output_callback( buffer->surface->frame_builder,
	                                              MALI_REINTERPRET_CAST(mali_frame_cb_func)NULL,
	                                              MALI_REINTERPRET_CAST(mali_frame_cb_param)NULL, NULL );
}

EGLBoolean __egl_platform_get_window_size( EGLNativeWindowType win, void *platform, u32 *width, u32 *height )
{
	android_native_window_t *native_win = NULL;
	android_platform_data *platform_data = (android_platform_data *)platform;
	int w_width, w_height;

	MALI_DEBUG_ASSERT_POINTER( win );

	native_win = static_cast<android_native_window_t*>(win);

	native_win->query(native_win, NATIVE_WINDOW_WIDTH, &w_width );
	native_win->query(native_win, NATIVE_WINDOW_HEIGHT, &w_height );

	if ( NULL != width ) *width = w_width;
	if ( NULL != height ) *height = w_height;

	return EGL_TRUE;
}

EGLBoolean __egl_platform_window_valid( EGLNativeDisplayType display, EGLNativeWindowType win )
{
	MALI_IGNORE( display );

	if ( 0 == win ) return EGL_FALSE;

	if ( static_cast<android_native_window_t*>(win)->common.magic != ANDROID_NATIVE_WINDOW_MAGIC ) return EGL_FALSE;

	return EGL_TRUE;
}

EGLBoolean __egl_platform_window_compatible( EGLNativeDisplayType display, EGLNativeWindowType win, egl_config *config )
{
	MALI_IGNORE( display );
	MALI_IGNORE( win );

	if ( !(config->renderable_type & EGL_WINDOW_BIT) ) return EGL_FALSE;

	return EGL_TRUE;
}

unsigned int __egl_platform_get_buffer(egl_surface* surface)
{
#if MALI_ANDROID_API > 13
	return surface->current_buffer;
#else
	unsigned int current_buffer = (surface->current_buffer + 1);
	if ( current_buffer >= surface->num_buffers)
	{
		current_buffer = 0;
	}

	return current_buffer;
#endif
}

void __egl_platform_get_pixmap_size( EGLNativeDisplayType display, EGLNativePixmapType pixmap, u32 *width, u32 *height, u32 *pitch )
{
#ifdef NEXELL_ANDROID_FEATURE_PIXMAP_USE
	fbdev_pixmap *fb_pixmap = NULL;
	MALI_DEBUG_ASSERT_POINTER( pixmap );
	MALI_IGNORE( display );

	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap *)pixmap;

	if (NULL != width)	*width	= (u32)fb_pixmap->width;
	if (NULL != height) *height = (u32)fb_pixmap->height;
	if (NULL != pitch)	*pitch	= (u32)fb_pixmap->bytes_per_pixel * fb_pixmap->width;
#else
	MALI_IGNORE( display );
	MALI_IGNORE( pixmap );
	MALI_IGNORE( width );
	MALI_IGNORE ( height );
	MALI_IGNORE ( pitch );
#endif	
}

EGLenum __egl_platform_get_pixmap_colorspace( EGLNativePixmapType pixmap )
{
#ifdef NEXELL_ANDROID_FEATURE_PIXMAP_USE
	fbdev_pixmap *fb_pixmap = NULL;
	MALI_DEBUG_ASSERT_POINTER( pixmap );
	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap *)pixmap;

	return ( fb_pixmap->flags & FBDEV_PIXMAP_COLORSPACE_sRGB ) ? EGL_COLORSPACE_sRGB : EGL_COLORSPACE_LINEAR;
#else
	return EGL_COLORSPACE_sRGB;
#endif	
}

EGLenum __egl_platform_get_pixmap_alphaformat( EGLNativePixmapType pixmap )
{
#ifdef NEXELL_ANDROID_FEATURE_PIXMAP_USE
	fbdev_pixmap *fb_pixmap = NULL;
	MALI_DEBUG_ASSERT_POINTER( pixmap );
	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap *)pixmap;

	return ( fb_pixmap->flags & FBDEV_PIXMAP_ALPHA_FORMAT_PRE ) ? EGL_ALPHA_FORMAT_PRE : EGL_ALPHA_FORMAT_NONPRE;
#else
	return EGL_ALPHA_FORMAT_PRE;
#endif	
}

void __egl_platform_get_pixmap_format( EGLNativeDisplayType display, EGLNativePixmapType pixmap, mali_surface_specifier *sformat )
{
#ifdef NEXELL_ANDROID_FEATURE_PIXMAP_USE
	fbdev_pixmap *fb_pixmap = NULL;
	unsigned int i, rsize, gsize, bsize, asize, lsize, buffer_size;
	m200_texel_format texel_format = M200_TEXEL_FORMAT_NO_TEXTURE;
	m200_texture_addressing_mode texel_layout = M200_TEXTURE_ADDRESSING_MODE_LINEAR;

	static const struct
	{
		m200_texel_format texel_format;
		u8 asize;       /* No. of bits for 'alpha' component (opacity) */
		u8 rsize;       /* No. of bits for red colour component */
		u8 gsize;       /* No. of bits for green colour component */
		u8 bsize;       /* No. of bits for blue colour component */
		u8 lsize;       /* No. of bits for luminance component */
		u8 buffer_size; /* No. of bits per pixel (including wastage) */
	}
	format_table[] =
	{
		{ M200_TEXEL_FORMAT_RGB_565,   0, 5, 6, 5, 0, 16 },
		{ M200_TEXEL_FORMAT_ARGB_8888, 8, 8, 8, 8, 0, 32 },
		{ M200_TEXEL_FORMAT_L_8,       0, 0, 0, 0, 8, 8 },
		{ M200_TEXEL_FORMAT_AL_88,     8, 0, 0, 0, 8, 16 },
		{ M200_TEXEL_FORMAT_A_8,       8, 0, 0, 0, 0, 8 },
		{ M200_TEXEL_FORMAT_ARGB_4444, 4, 4, 4, 4, 0, 16 },
		{ M200_TEXEL_FORMAT_ARGB_1555, 1, 5, 5, 5, 0, 16 },
		{ M200_TEXEL_FORMAT_ARGB_8888, 0, 8, 8, 8, 0, 32 },
#if !RGB_IS_XRGB /* RGB formats don't exist on M400-r0p0 */
		{ M200_TEXEL_FORMAT_RGB_888,   0, 8, 8, 8, 0, 24 }
#endif
	};

	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap *)pixmap;
	MALI_DEBUG_ASSERT_POINTER( fb_pixmap );
	MALI_DEBUG_ASSERT_POINTER( sformat );
	MALI_IGNORE( display );

	rsize = fb_pixmap->red_size;
	gsize = fb_pixmap->green_size;
	bsize = fb_pixmap->blue_size;
	asize = fb_pixmap->alpha_size;
	lsize = fb_pixmap->luminance_size;
	buffer_size = fb_pixmap->buffer_size;

	for (i = 0; i < MALI_ARRAY_SIZE(format_table); ++i)
	{
		if (format_table[i].rsize == rsize &&
		    format_table[i].gsize == gsize &&
		    format_table[i].bsize == bsize &&
		    format_table[i].asize == asize &&
		    format_table[i].lsize == lsize &&
		    format_table[i].buffer_size == buffer_size)
		{
			texel_format = format_table[i].texel_format;
			break;
		}
	}

	if (M200_TEXEL_FORMAT_NO_TEXTURE == texel_format)
	{
		MALI_DEBUG_PRINT(0, ("EGL : unsupported pixmap format: %i %i %i %i %i (RGBAL)\n", rsize, gsize, bsize, asize, lsize) );
	}

	_mali_surface_specifier_ex(
		sformat,
		0,0,0,
		_mali_texel_to_pixel_format(texel_format),
		texel_format,
		_mali_texel_layout_to_pixel_layout(texel_layout),
		texel_layout,
		MALI_FALSE,
		MALI_FALSE,
		( EGL_COLORSPACE_sRGB == __egl_platform_get_pixmap_colorspace( pixmap ) ) ? MALI_SURFACE_COLORSPACE_sRGB : MALI_SURFACE_COLORSPACE_lRGB,
		( EGL_ALPHA_FORMAT_PRE == __egl_platform_get_pixmap_alphaformat( pixmap ) ) ? MALI_SURFACE_ALPHAFORMAT_PRE : MALI_SURFACE_ALPHAFORMAT_NONPRE,
		asize == 0 ? MALI_TRUE : MALI_FALSE );
#else
	MALI_IGNORE( display );
	MALI_IGNORE( pixmap );
	MALI_IGNORE( sformat );
#endif	
}

EGLBoolean __egl_platform_pixmap_valid( EGLNativePixmapType pixmap )
{
#ifdef NEXELL_ANDROID_FEATURE_PIXMAP_USE
	fbdev_pixmap *fb_pixmap = NULL;

	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap*)pixmap;
	if ( NULL == fb_pixmap ) return EGL_FALSE;

#if MALI_USE_DMA_BUF == 0
	/* No dma-buf support. Pixmap shouldn't have the dma-buf flag set */
	if ( FBDEV_PIXMAP_DMA_BUF & fb_pixmap->flags )
	{
		return EGL_FALSE;
	}
#elif MALI_USE_UNIFIED_MEMORY_PROVIDER == 0
	/* No UMP-support. Pixmap shouldn't have the UMP-flag set */
	if ( FBDEV_PIXMAP_SUPPORTS_UMP & fb_pixmap->flags )
	{
		return EGL_FALSE;
	}
#endif
	if ( NULL == fb_pixmap->data )
	{
		/* fbdev_pixmap.data should be a valid pointer */
		return EGL_FALSE;
	}

	return EGL_TRUE;
#else
	MALI_IGNORE( pixmap );
	return EGL_FALSE;
#endif	
}

EGLBoolean __egl_platform_pixmap_support_ump( EGLNativePixmapType pixmap )
{
#ifdef NEXELL_ANDROID_FEATURE_PIXMAP_USE
	fbdev_pixmap *fb_pixmap = NULL;
	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap*)pixmap;

	MALI_DEBUG_ASSERT_POINTER( fb_pixmap );

	if ( (FBDEV_PIXMAP_DMA_BUF | FBDEV_PIXMAP_SUPPORTS_UMP) & fb_pixmap->flags ) return EGL_TRUE;

	return EGL_FALSE;
#else
	MALI_IGNORE( pixmap );

	return EGL_FALSE;
#endif	
}

EGLBoolean __egl_platform_pixmap_config_compatible( EGLNativeDisplayType display, EGLNativePixmapType pixmap, egl_config *config, EGLBoolean renderable_usage )
{
#ifdef NEXELL_ANDROID_FEATURE_PIXMAP_USE
	fbdev_pixmap *fb_pixmap = NULL;

	MALI_DEBUG_ASSERT_POINTER( pixmap );
	MALI_DEBUG_ASSERT_POINTER( config );
	MALI_IGNORE( renderable_usage );
	MALI_IGNORE( display );

	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap *)pixmap;

	if ( fb_pixmap->buffer_size != config->buffer_size ) return EGL_FALSE;
	if ( fb_pixmap->red_size != config->red_size ) return EGL_FALSE;
	if ( fb_pixmap->green_size != config->green_size ) return EGL_FALSE;
	if ( fb_pixmap->blue_size != config->blue_size ) return EGL_FALSE;
	if ( fb_pixmap->alpha_size != config->alpha_size ) return EGL_FALSE;
	if ( fb_pixmap->luminance_size != config->luminance_size ) return EGL_FALSE;
	if ( fb_pixmap->width <= 0 ) return EGL_FALSE;
	if ( fb_pixmap->height <= 0 ) return EGL_FALSE;

	return EGL_TRUE;
#else
	MALI_IGNORE( display );
	MALI_IGNORE( pixmap );
	MALI_IGNORE( config );
	MALI_IGNORE( renderable_usage );

	return EGL_FALSE;
#endif	
}

EGLBoolean __egl_platform_pixmap_surface_compatible( EGLNativeDisplayType display, EGLNativePixmapType pixmap, egl_surface *surface, EGLBoolean alpha_check )
{
#ifdef NEXELL_ANDROID_FEATURE_PIXMAP_USE
	fbdev_pixmap *fb_pixmap = NULL;
	MALI_DEBUG_ASSERT_POINTER( pixmap );
	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_IGNORE( display );

    if ( EGL_TRUE == alpha_check ) return EGL_TRUE;

	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap *)pixmap;

	if ( fb_pixmap->width != surface->width ) return EGL_FALSE;
	if ( fb_pixmap->height != surface->height ) return EGL_FALSE;

	return EGL_TRUE;
#else
	MALI_IGNORE( display );
	MALI_IGNORE( pixmap );
	MALI_IGNORE( surface );
	MALI_IGNORE( alpha_check );
	return EGL_FALSE;
#endif	
}

EGLBoolean __egl_platform_pixmap_egl_image_compatible( EGLNativePixmapType pixmap, egl_context *context )
{
	MALI_IGNORE(pixmap);
	MALI_IGNORE(context);

	return EGL_TRUE;
}

EGLBoolean __egl_platform_pixmap_copybuffers_compatible( EGLNativeDisplayType display, EGLNativePixmapType pixmap, egl_surface *surface )
{
#ifdef NEXELL_ANDROID_FEATURE_PIXMAP_USE
	MALI_IGNORE( display );
	/* This platform does not support conversion for eglCopyBuffers */

	/* check if target pixmap is compatible with config */
	if ( EGL_FALSE == __egl_platform_pixmap_config_compatible( display, pixmap, surface->config, EGL_TRUE ) )
	{
		return EGL_FALSE;
	}

	/* check if target pixmap is compatible with surface */
	if ( EGL_FALSE == __egl_platform_pixmap_surface_compatible( display, pixmap, surface, EGL_FALSE ) )
	{
		return EGL_FALSE;
	}

	return EGL_TRUE;
#else
	MALI_IGNORE( display );
	MALI_IGNORE( pixmap );
	MALI_IGNORE( surface );

	return EGL_FALSE;
#endif	
}

struct mali_image* __egl_platform_map_pixmap_rgb( EGLNativeDisplayType display, EGLNativePixmapType pixmap )
{
	u32 pixmap_width = 0, pixmap_height = 0;
	fbdev_pixmap *fb_pixmap = NULL;
	mali_surface_specifier sformat;
	mali_image *image = NULL;
	mali_base_ctx_handle base_ctx;
	__egl_main_context *egl = __egl_get_main_context();
	EGLBoolean pixmap_support_ump = EGL_FALSE;

	base_ctx = egl->base_ctx;
	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap *)pixmap;
	MALI_DEBUG_ASSERT_POINTER( fb_pixmap );

	__egl_platform_get_pixmap_size(display, pixmap, &pixmap_width, &pixmap_height, NULL);

	pixmap_support_ump = __egl_platform_pixmap_support_ump( pixmap );

	if ( (((pixmap_width * fb_pixmap->bytes_per_pixel)% MALI200_SURFACE_PITCH_ALIGNMENT_LINEAR_LAYOUT) != 0) && pixmap_support_ump )
	{
		/* Pitch alignment must conform to Mali writeback unit requirements */
		MALI_DEBUG_PRINT( 1, ("EGL : pitch %lu is not aligned to hardware requirement of %d bytes.\n", MALI_STATIC_CAST(unsigned long)pixmap_width * fb_pixmap->bytes_per_pixel, MALI200_SURFACE_PITCH_ALIGNMENT_LINEAR_LAYOUT ));
		return NULL;
	}

	__egl_platform_get_pixmap_format(display, pixmap, &sformat);

	/* mali_image_create_from_external_memory doesn't cope
	   gracefully with invalid surface format descriptors. */
	if (M200_TEXEL_FORMAT_NO_TEXTURE == sformat.texel_format &&
	    MALI_PIXEL_FORMAT_NONE == sformat.pixel_format)
	{
		return NULL;
	}

	image = mali_image_create_from_external_memory( pixmap_width, pixmap_height,
	                                                pixmap_support_ump == EGL_TRUE ? MALI_SURFACE_FLAG_DONT_MOVE : MALI_SURFACE_FLAGS_NONE,
	                                                &sformat,
#if MALI_USE_DMA_BUF
	                                                pixmap_support_ump == EGL_TRUE ? (void*)(*((int*)(fb_pixmap->data))) : (void*)NULL,
	                                                pixmap_support_ump == EGL_TRUE ? MALI_IMAGE_DMA_BUF : MALI_IMAGE_CPU_MEM,
#else
	                                                pixmap_support_ump == EGL_TRUE ? fb_pixmap->data : NULL,
	                                                pixmap_support_ump == EGL_TRUE ? MALI_IMAGE_UMP_MEM : MALI_IMAGE_CPU_MEM,
#endif
	                                                base_ctx );
	MALI_CHECK_NON_NULL( image, NULL );

	/* copy the pixmap content into internal mali mem if ump is not supported */
	if ( EGL_FALSE == pixmap_support_ump )
	{	
		u32 src_pitch  = 0;
		u32 dst_offset = 0;
		u32 src_offset = 0;
		s32 y = 0;

		if ( EGL_TRUE == egl->flip_pixmap )
		{
			src_pitch = pixmap_width*fb_pixmap->bytes_per_pixel;
			y = pixmap_height-1;

			for ( ; y >= 0; --y )
			{
				/* Note, that data must be already in mali byteorder */
				dst_offset = image->pixel_buffer[0][0]->format.pitch*y;
				_mali_mem_write( image->pixel_buffer[0][0]->mem_ref->mali_memory , dst_offset, ((u8*)fb_pixmap->data) + src_offset, src_pitch );
				src_offset += src_pitch;
			}
		}
		else
		{
			src_pitch = pixmap_width*fb_pixmap->bytes_per_pixel;

			for ( y = 0; y < (s32)pixmap_height; y++ )
			{
				dst_offset = image->pixel_buffer[0][0]->format.pitch * y;
				_mali_mem_write( image->pixel_buffer[0][0]->mem_ref->mali_memory , dst_offset, ((u8*)fb_pixmap->data) + src_offset, src_pitch );
				src_offset += src_pitch;
			}
		}
	}
	else
	{
		MALI_DEBUG_ASSERT( EGL_FALSE == egl->flip_pixmap, ("UMP pixmap flipping not supported") );
	}
	return image;
}

struct mali_image* __egl_platform_map_pixmap( EGLNativeDisplayType display, struct egl_image *egl_img, EGLNativePixmapType pixmap )
{
#ifdef NEXELL_ANDROID_FEATURE_PIXMAP_USE
	MALI_IGNORE( egl_img );
	return __egl_platform_map_pixmap_rgb( display, pixmap );
#else
	MALI_IGNORE( display );
    MALI_IGNORE( egl_img );
	MALI_IGNORE( pixmap );

	return NULL;
#endif	
}

void __egl_platform_unmap_pixmap( EGLNativePixmapType pixmap, struct egl_image *egl_img )
{
#ifdef NEXELL_ANDROID_FEATURE_PIXMAP_USE
	fbdev_pixmap *fb_pixmap = NULL;

	MALI_IGNORE( egl_img );

	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap*)pixmap;

	if ( fb_pixmap->flags & FBDEV_PIXMAP_EGL_MEMORY )
	{
		/* free this memory if owned by egl */
		_mali_sys_free( fb_pixmap );
	}
#else
	MALI_IGNORE( pixmap );
    MALI_IGNORE( egl_img );
#endif	
}

void __egl_platform_power_event( void *event_data )
{
	u32 event = MALI_REINTERPRET_CAST(u32)event_data;
	__egl_main_power_event( event );
}

EGLBoolean __egl_platform_wait_native(EGLint engine)
{
	MALI_IGNORE(engine);

	return EGL_TRUE;
}

void __egl_platform_deinit_display( EGLNativeDisplayType dpy )
{
	MALI_IGNORE( dpy );
}

void __egl_platform_display_get_format( EGLNativeDisplayType dpy,
                                        egl_display_native_format *format )
{
	MALI_IGNORE( dpy );
	MALI_IGNORE( format );
}

EGLNativeWindowType __egl_platform_create_null_window( EGLNativeDisplayType dpy )
{
	MALI_IGNORE( dpy );
	return NULL;
}

void __egl_platform_swap_buffers( mali_base_ctx_handle base_ctx,
                                  EGLNativeDisplayType native_dpy,
                                  egl_surface *surface,
                                  struct mali_surface *target,
                                  EGLint interval)
{
}

void __egl_platform_copy_buffers( mali_base_ctx_handle base_ctx,
                                  EGLNativeDisplayType native_dpy,
                                  egl_surface *surface,
                                  struct mali_surface *target,
                                  EGLNativePixmapType native_pixmap )
{
	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_DEBUG_ASSERT_POINTER( surface->copy_func );

	MALI_IGNORE( base_ctx );
	MALI_IGNORE( native_dpy );
	MALI_IGNORE( surface );
	MALI_IGNORE( target );
	MALI_IGNORE( native_pixmap );
}

EGLBoolean __egl_platform_supports_vsync( void )
{
	return EGL_TRUE;
}

void __egl_platform_set_swap_region( egl_surface *surface, EGLint numrects, const EGLint* rects )
{
	MALI_IGNORE( surface );
	MALI_IGNORE( numrects );
	MALI_IGNORE( rects );
}

void __egl_platform_update_image(struct mali_surface * render_target, void* data)
{
#if EGL_SURFACE_LOCKING_ENABLED && MALI_USE_UNIFIED_MEMORY_PROVIDER
#elif GRALLOC_ARM_DMA_BUF_MODULE
	MALI_IGNORE( render_target );
	MALI_IGNORE( data );
#endif
}
void __egl_platform_register_lock_item( egl_surface_lock_item *lock_item )
{
#if EGL_SURFACE_LOCKING_ENABLED && MALI_USE_UNIFIED_MEMORY_PROVIDER
	_lock_item_s item;

	item.secure_id = lock_item->secure_id;
	item.usage = MALI_REINTERPRET_CAST(_lock_access_usage)lock_item->usage;

	ioctl( fd_umplock, LOCK_IOCTL_CREATE, &item );
#endif
}

void __egl_platform_unregister_lock_item( egl_surface_lock_item *lock_item )
{
#if EGL_SURFACE_LOCKING_ENABLED && MALI_USE_UNIFIED_MEMORY_PROVIDER
	_lock_item_s item;

	item.secure_id = lock_item->secure_id;
	item.usage = MALI_REINTERPRET_CAST(_lock_access_usage)lock_item->usage;

	/*ioctl( fd_umplock, LOCK_IOCTL_DESTROY, &item );*/
#endif
}

void __egl_platform_process_lock_item( egl_surface_lock_item *lock_item )
{
#if EGL_SURFACE_LOCKING_ENABLED && MALI_USE_UNIFIED_MEMORY_PROVIDER
	_lock_item_s item;

	item.secure_id = lock_item->secure_id;
	item.usage = MALI_REINTERPRET_CAST(_lock_access_usage)lock_item->usage;

#if MALI_TIMELINE_PROFILING_ENABLED
	_mali_base_profiling_add_event( VR_PROFILING_EVENT_TYPE_SINGLE | VR_PROFILING_EVENT_CHANNEL_SOFTWARE | VR_PROFILING_EVENT_REASON_SINGLE_SW_UMP_TRY_LOCK, 0, 0, 0, item.usage, item.secure_id );
#endif

	ioctl( fd_umplock, LOCK_IOCTL_PROCESS, &item );

#if MALI_TIMELINE_PROFILING_ENABLED
	_mali_base_profiling_add_event( VR_PROFILING_EVENT_TYPE_SINGLE | VR_PROFILING_EVENT_CHANNEL_SOFTWARE | VR_PROFILING_EVENT_REASON_SINGLE_SW_UMP_LOCK, 0, 0, 0, item.usage, item.secure_id );
#endif

#endif
}

void __egl_platform_release_lock_item( egl_surface_lock_item *lock_item )
{
#if EGL_SURFACE_LOCKING_ENABLED && MALI_USE_UNIFIED_MEMORY_PROVIDER
	_lock_item_s item;

	item.secure_id = lock_item->secure_id;
	item.usage = MALI_REINTERPRET_CAST(_lock_access_usage)lock_item->usage;

	ioctl( fd_umplock, LOCK_IOCTL_RELEASE, &item );

#if MALI_TIMELINE_PROFILING_ENABLED
	_mali_base_profiling_add_event( VR_PROFILING_EVENT_TYPE_SINGLE | VR_PROFILING_EVENT_CHANNEL_SOFTWARE | VR_PROFILING_EVENT_REASON_SINGLE_SW_UMP_UNLOCK, 0, 0, 0, item.usage, item.secure_id );
#endif

#endif
}
