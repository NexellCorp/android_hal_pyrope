/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file egl_platform_android.c
 * @brief Android backend
 */
#include <utils/Log.h>

#include <EGL/egl.h>
#include "egl_platform.h"
#include "egl_mali.h"

#include <mali_system.h>
#include <shared/mali_surface.h>
#include <shared/mali_image.h>
#include <shared/mali_shared_mem_ref.h>
#include <shared/mali_pixel_format.h>
#include <shared/m200_texel_format.h>
#include <shared/m200_td.h>
#include <shared/m200_gp_frame_builder_inlines.h>

#ifdef HAVE_ANDROID_OS
#include <ui/egl/android_natives.h>
#include <ui/android_native_buffer.h>
#include <hardware/gralloc.h>
#include <gralloc_priv.h>
#	ifdef MALI_USE_UNIFIED_MEMORY_PROVIDER
#		if GRALLOC_ARM_UMP_MODULE != 1
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

#include <unistd.h>
#include <stdlib.h>

#include <ump/ump.h>
#include <ump/ump_ref_drv.h>

#define NUM_BUFFERS   2
#define STABLE_SEQUENCE_LIMIT 4
#ifdef NDEBUG
/* org */
#define ANDROID_ENABLE_DEBUG 0
#else /* nexell add */
#define ANDROID_ENABLE_DEBUG 1
#endif

using namespace android;

/* global instance of gralloc module reference
 * initialized at platform initialization time
 */
gralloc_module_t const* module = NULL;
typedef enum
{
	STATE_OK,
	STATE_RECREATE_SINGLE_BUFFER,
	STATE_RECREATE_DOUBLE_BUFFER,
} android_buffer_state;

typedef struct android_platform_data
{
	android_native_buffer_t* last_buffers[NUM_BUFFERS];
	ump_secure_id last_ump_ids[NUM_BUFFERS];
	android_buffer_state buffer_state;
	int stable_sequences;
	mali_bool oom;
} android_platform_data;

typedef struct android_format_mapping
{
	int android_pixel_format;
	m200_texel_format mali_pixel_format;
} android_format_mapping;

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


/* fills in buffer information, such as physical and virtual address */
EGLBoolean native_buffer_get_info( android_native_buffer_t* buffer,
                                   u32 *paddr, u32 *vaddr, u32 *offset, u32 *size
#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
                                   , ump_handle* ump_mem_handle
#endif
)
{
	private_handle_t const* hnd;

	/* we might come here w/ a NULL buffer (via ump_get_secure_id()) */
	if (!buffer)
	{
		LOGI("Invalid buffer (NULL)! bailing out.");
		return EGL_FALSE;
	}

	hnd = reinterpret_cast<private_handle_t const*>(buffer->handle);

	if ( hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER )
	{
		struct fb_fix_screeninfo fix_info;
		(void)ioctl( hnd->fd, FBIOGET_FSCREENINFO, &fix_info );
		*paddr = fix_info.smem_start;
		*offset = hnd->offset;
#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
		*ump_mem_handle = UMP_INVALID_MEMORY_HANDLE;
#endif
	}
	else if ( hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP )
	{
#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
		*paddr = NULL;
		*offset = 0;
		*ump_mem_handle = (ump_handle)hnd->ump_mem_handle;
#else
		MALI_DEBUG_ASSERT(MALI_FALSE, ("Unable to use UMP buffer, no UMP support built into Mali driver"));
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

ump_secure_id get_buffer_ump_id(android_native_buffer_t* buffer)
{
#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	u32 physical_address = 0, virtual_address = 0, offset = 0, size = 0;
	ump_handle ump_mem_handle = UMP_INVALID_MEMORY_HANDLE;

	if (native_buffer_get_info( buffer, &physical_address, &virtual_address, &offset, &size, &ump_mem_handle ) == EGL_FALSE )
	{
		LOGE("EGL Failed to retrieve buffer information in %s", __func__ );
	}

	if(UMP_INVALID_MEMORY_HANDLE !=ump_mem_handle)
	{
		return ump_secure_id_get(ump_mem_handle);
	}
#endif

	return UMP_INVALID_SECURE_ID;
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

void __egl_platform_power_event( void *event_data )
{
	u32 event = MALI_REINTERPRET_CAST(u32)event_data;
	__egl_main_power_event( event );
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


void __egl_platform_deinit_display( EGLNativeDisplayType dpy )
{
	MALI_IGNORE( dpy );
}

void __egl_platform_display_get_format( EGLNativeDisplayType dpy,
                                        egl_display_native_format *format )
{
}

EGLBoolean __egl_platform_wait_native(EGLint engine)
{
	MALI_IGNORE(engine);

	return EGL_TRUE;
}

EGLBoolean __egl_platform_surface_buffer_invalid( egl_surface* surface )
{
	/* Not applicable to Androids prior to beeswax */
	return EGL_FALSE;
}

EGLBoolean __egl_platform_connect_surface( egl_surface *surface )
{
	if ( MALI_EGL_WINDOW_SURFACE == surface->type )
	{
		android_native_window_t *native_win = static_cast<android_native_window_t*>(surface->win);

		MALI_DEBUG_ASSERT_POINTER( surface->win );

		native_window_connect(native_win, NATIVE_WINDOW_API_EGL);

#if ANDROID_ENABLE_DEBUG
		LOGD("{0x%08X} EGL ANDROID connecting to native window (0x%x)", surface->platform, native_win );
#endif
	}
	return EGL_TRUE;
}

void __egl_platform_disconnect_surface( egl_surface *surface )
{
	if ( MALI_EGL_WINDOW_SURFACE == surface->type )
	{
		android_native_window_t *native_win = static_cast<android_native_window_t*>(surface->win);

		MALI_DEBUG_ASSERT_POINTER( surface->win );

		native_window_disconnect(native_win, NATIVE_WINDOW_API_EGL);

#if ANDROID_ENABLE_DEBUG
		LOGD("{0x%08X} EGL ANDROID disconnecting from native window (0x%x)", surface->platform, native_win );
#endif
	}
}

mali_bool __egl_platform_surface_is_framebuffer( const egl_surface *surface )
{
	android_native_buffer_t *buffer = MALI_REINTERPRET_CAST(android_native_buffer_t *)(surface->client_buffer);
	private_handle_t const* hnd;

	/* we might come here w/ a NULL buffer */
	if (!buffer)
	{
		LOGI("Invalid buffer (NULL)! bailing out.");
		return EGL_FALSE;
	}

	hnd = reinterpret_cast<private_handle_t const*>(buffer->handle);

	if ( hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER ) return MALI_TRUE;

	return MALI_FALSE;
}

void __egl_platform_begin_new_frame( egl_surface *surface )
{
	MALI_IGNORE( surface );
}

mali_bool __egl_platform_surface_is_deferred_release ( const egl_surface *surface )
{
	/* Bugzilla 11215: we don't have a way to determine if the render target of the surface is currently used by surfaceflinger for compositing or not
	 * enabled deferred release so that display dependency will not be released until the next swap in order to make sure pp job won't
	 * happen on buffers for compositing
	 */
	return MALI_TRUE;
}

/* Android does not allow the usage of so called "null windows" */
EGLNativeWindowType __egl_platform_create_null_window( EGLNativeDisplayType dpy )
{
	return NULL;
}

mali_surface* __egl_platform_create_surface_from_native_buffer( android_native_buffer_t *buffer, egl_surface *surface, mali_base_ctx_handle base_ctx )
{
	int page_size = sysconf( _SC_PAGESIZE );
	u32 physical_address = 0, virtual_address = 0, offset = 0, size = 0;
#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	ump_handle ump_mem_handle = UMP_INVALID_MEMORY_HANDLE;
#endif
	mali_mem_handle mem_handle = MALI_NO_HANDLE;
	mali_shared_mem_ref *mem_ref = NULL;
	mali_surface_specifier sformat;
	mali_pixel_format pixelformat = MALI_PIXEL_FORMAT_R5G6B5;
	u32 mem_offset = 0, mem_size = 0, bpp = 2;
	mali_surface *color_buffer = NULL;
	mali_bool red_blue_swap = MALI_FALSE;
	mali_bool reverse_order = MALI_FALSE;

	/* lock it for access, retrieve buffer information */
	if (private_handle_t::validate(buffer->handle) < 0)
	{
		LOGE( "EGL was given an invalid buffer handle (0x%x) in %s", (int)buffer->handle, __func__ );
		return NULL;
	}
#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	if (native_buffer_get_info( buffer, &physical_address, &virtual_address, &offset, &size, &ump_mem_handle ) == EGL_FALSE )
#else
	if (native_buffer_get_info( buffer, &physical_address, &virtual_address, &offset, &size ) == EGL_FALSE )
#endif
	{
		LOGE( "EGL failed to retrieve buffer information in %s", __func__ );
		return NULL;
	}

	module->lock(module, buffer->handle, buffer->usage, 0, 0, buffer->width, buffer->height, (void**)&virtual_address);

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
			bpp = 4;
			red_blue_swap = MALI_TRUE;
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
			LOGE("EGL given unknown pixel format 0x%x in %s", buffer->format, __func__);
			module->unlock(module, buffer->handle);
			return NULL;
			break;
	}
	_mali_surface_specifier_ex( &sformat, buffer->width, buffer->height, buffer->stride*bpp,
								pixelformat, _mali_pixel_to_texel_format(pixelformat),
								MALI_PIXEL_LAYOUT_LINEAR, M200_TEXTURE_ADDRESSING_MODE_LINEAR, red_blue_swap,
								reverse_order, MALI_SURFACE_COLORSPACE_sRGB, MALI_SURFACE_ALPHAFORMAT_NONPRE, surface->config->alpha_size == 0 ? MALI_TRUE : MALI_FALSE );
	color_buffer = _mali_surface_alloc_empty(  MALI_SURFACE_FLAG_DONT_MOVE, &sformat, base_ctx );
	if ( NULL == color_buffer )
	{
		LOGE( "EGL failed to allocate empty mali surface in %s", __func__ );
		module->unlock(module, buffer->handle);
		return NULL;
	}

	mem_size = size+offset;
	mem_size = (mem_size + (page_size-1)) & ~(page_size-1);

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	if (UMP_INVALID_MEMORY_HANDLE == ump_mem_handle)
	{
		mem_handle = _mali_mem_add_phys_mem( base_ctx, physical_address, mem_size, (void*)(virtual_address - offset), MALI_PP_WRITE | MALI_PP_READ | MALI_CPU_READ | MALI_CPU_WRITE );
#if ANDROID_ENABLE_DEBUG
		LOGD("{0x%08X} EGL ANDROID create buffer from FB %p (0x%x + %i mem handle: 0x%x)", 0, buffer, physical_address, offset, mem_handle, buffer);
#endif
	}
	else /* UMP */
	{
		mem_handle = _mali_mem_wrap_ump_memory( base_ctx, ump_mem_handle, 0 );
#if ANDROID_ENABLE_DEBUG
		LOGD("{0x%08X} EGL ANDROID color buffer from UMP %p (%i)", 0, buffer, ump_secure_id_get( ump_mem_handle ) );
#endif
	}
#else
	mem_handle = _mali_mem_add_phys_mem( base_ctx, physical_address, mem_size, (void*)(virtual_address - offset), MALI_PP_WRITE | MALI_PP_READ | MALI_CPU_READ | MALI_CPU_WRITE );
#endif

	if ( MALI_NO_HANDLE == mem_handle )
	{
		LOGE( "EGL failed to map physical memory in %s", __func__ );
		_mali_surface_deref( color_buffer );
		module->unlock(module, buffer->handle);
		return NULL;
	}

	color_buffer->mem_ref = _mali_shared_mem_ref_alloc_existing_mem( mem_handle );
	if ( NULL == color_buffer->mem_ref )
	{
		LOGE( "EGL failed to allocated shared memory reference from existing memory handle in %s", __func__ );
		_mali_mem_free( mem_handle );
		_mali_surface_deref( color_buffer );
		module->unlock(module, buffer->handle);
		return NULL;
	}

	color_buffer->mem_offset = offset;
	color_buffer->datasize = _mali_mem_size_get( color_buffer->mem_ref->mali_memory ) - color_buffer->mem_offset;

	module->unlock(module, buffer->handle);

#if ANDROID_ENABLE_DEBUG
	LOGD("{0x%08X} EGL ANDROID allocated new buffer mali_surface %p , %i x %i", 0, color_buffer, color_buffer->format.width, color_buffer->format.height);
#endif

	return color_buffer;
}

EGLBoolean __egl_platform_create_surface_window( egl_surface *surface, mali_base_ctx_handle base_ctx )
{
	android_native_window_t *native_win = static_cast<android_native_window_t*>(surface->win);
	android_native_buffer_t *buffer = NULL;
	android_platform_data *platform_data = (android_platform_data *)surface->platform;
	mali_surface *color_buffer[NUM_BUFFERS];
	ump_secure_id secure_id = UMP_INVALID_SECURE_ID;
	int win_width, win_height, i, j;
	enum mali_frame_builder_type type;

	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_DEBUG_ASSERT_POINTER( platform_data );

	platform_data->last_ump_ids[0] = UMP_INVALID_SECURE_ID;
	platform_data->last_ump_ids[1] = UMP_INVALID_SECURE_ID;
	platform_data->last_buffers[0] = NULL;
	platform_data->last_buffers[1] = NULL;
	platform_data->buffer_state = STATE_OK;
	platform_data->stable_sequences = 0;
	platform_data->oom = MALI_FALSE;

	_mali_sys_usleep(50000); /* give android some time to synchronize the window size */

	native_window_set_usage(native_win, GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_2D | GRALLOC_USAGE_SW_READ_RARELY );

	/* dequeue a buffer */
	if ( 0 != native_win->dequeueBuffer( native_win, &buffer ) )
	{
		LOGE( "EGL unable to dequeue buffer in %s", __func__ );
		return EGL_FALSE;
	}
	native_win->lockBuffer( native_win, buffer );

	secure_id = get_buffer_ump_id( buffer );

	for ( i=0; i<NUM_BUFFERS; i++ )
	{
		color_buffer[i] = __egl_platform_create_surface_from_native_buffer( buffer, surface, base_ctx );
		platform_data->last_ump_ids[0] = secure_id;

		if ( NULL == color_buffer[i] )
		{
			LOGE( "EGL unable to allocate surface in %s", __func__);
			for ( j=0; j<i; j++ ) _mali_surface_deref( color_buffer[j] );
			return EGL_FALSE;
		}
	}
	platform_data->last_buffers[0] = buffer;

	native_win->query(native_win, NATIVE_WINDOW_WIDTH, &win_width);
	native_win->query(native_win, NATIVE_WINDOW_HEIGHT, &win_height );

	surface->width = win_width;
	surface->height = win_height;

	surface->client_buffer = buffer;
	surface->frame_builder = NULL;

	surface->caps = SURFACE_CAP_DIRECT_RENDERING;

	/* render the first number of frames in immediate mode */
	surface->immediate_mode = EGL_TRUE;
	surface->num_buffers = 2;

	type = ( !__egl_platform_surface_is_framebuffer( surface)) ? MALI_FRAME_BUILDER_TYPE_EGL_WINDOW : MALI_FRAME_BUILDER_TYPE_EGL_COMPOSITOR;
	surface->frame_builder = __egl_mali_create_frame_builder( base_ctx, surface->config, NUM_BUFFERS+1, NUM_BUFFERS, color_buffer, MALI_FALSE, type );

	if ( NULL == surface->frame_builder )
	{
		/* queue buffer back to android */
		LOGE( "EGL failed to create framebuilder in %s", __func__ );
		native_win->queueBuffer( native_win, buffer);
		_mali_surface_deref( color_buffer[0] );
		return EGL_FALSE;
	}

	for ( i=0; i<NUM_BUFFERS; i++ )
	{
		surface->buffer[i].render_target = color_buffer[i];
		surface->buffer[i].surface = surface;
		surface->buffer[i].id = i;
	}

	/* EGL now has a reference to this window */
	native_win->common.incRef( &native_win->common );

#if ANDROID_ENABLE_DEBUG
	LOGD("{0x%08X} EGL ANDROID window surface created. (0x%x). size = %i x %i, UMP = {%i, %i}", surface->platform, (int)surface, win_width, win_height, platform_data->last_ump_ids[0], platform_data->last_ump_ids[1]);
#endif

	return EGL_TRUE;
}

EGLBoolean __egl_platform_create_surface_pbuffer( egl_surface *surface, mali_base_ctx_handle base_ctx )
{
	mali_surface *buf = NULL;
	mali_surface_specifier sformat;

	MALI_DEBUG_ASSERT_POINTER( surface );

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

	return EGL_TRUE;
}

EGLBoolean __egl_platform_create_surface_pixmap( egl_surface* surface, mali_base_ctx_handle base_ctx )
{
	MALI_IGNORE( surface );
	MALI_IGNORE( base_ctx );

	LOGE("NO SUPPORT FOR PIXMAP SURFACES!");

	return EGL_FALSE;
}

EGLBoolean __egl_platform_create_surface( egl_surface *surface, mali_base_ctx_handle base_ctx )
{
	android_platform_data *platform_data = NULL;
	MALI_DEBUG_ASSERT_POINTER( surface );

	platform_data = (android_platform_data *) _mali_sys_malloc( sizeof(android_platform_data) );
	if ( NULL == platform_data )
	{
		LOGE("EGL failed to allocate platform data in %s", __func__);
		return EGL_FALSE;
	}

	surface->num_buffers = 1;
	surface->copy_func = __egl_platform_copy_buffers;
	surface->platform = platform_data;

	switch ( surface->type )
	{
		case MALI_EGL_WINDOW_SURFACE:  return __egl_platform_create_surface_window( surface, base_ctx );
		case MALI_EGL_PBUFFER_SURFACE: return __egl_platform_create_surface_pbuffer( surface, base_ctx );
		case MALI_EGL_PIXMAP_SURFACE:  return __egl_platform_create_surface_pixmap( surface, base_ctx );
		default:
			break;
	}

	return EGL_FALSE;
}

void __egl_platform_destroy_surface(egl_surface *surface)
{
	MALI_DEBUG_ASSERT_POINTER( surface );

#if ANDROID_ENABLE_DEBUG
	LOGD("{0x%08X} EGL ANDROID window surface destroy (0x%x)", surface->platform, (int)surface);
#endif

	if ( (surface->client_buffer != NULL) && surface->win != NULL )
	{
		android_native_buffer_t *buffer = (android_native_buffer_t *)surface->client_buffer;
		android_native_window_t *native_win = (android_native_window_t *)surface->win;

#if ANDROID_ENABLE_DEBUG
		LOGD("{0x%08X} EGL ANDROID window surface destroy (0x%x) cancelling buffer. UMP = %i", surface->platform, (int)surface, get_buffer_ump_id(buffer));
#endif

#if MALI_ANDROID_API > 8
		if ( get_buffer_ump_id(buffer) != UMP_INVALID_SECURE_ID ) native_win->cancelBuffer( native_win, buffer );
#else
		native_win->queueBuffer(native_win, buffer);
#endif
		native_win->common.decRef( &native_win->common );
    }

	if( NULL != surface->platform ) _mali_sys_free(surface->platform);
	if( NULL != surface->frame_builder ) __egl_mali_destroy_frame_builder( surface );

	surface->frame_builder = NULL;
}

EGLBoolean __egl_platform_resize_surface( egl_surface *surface, u32 *width, u32 *height, mali_base_ctx_handle base_ctx )
{
	mali_frame_builder     *frame_builder = surface->frame_builder;
	mali_surface           *color_target[NUM_BUFFERS] = {NULL, NULL};
	mali_surface           *old_color_target[NUM_BUFFERS] = {NULL, NULL};
	android_native_window_t *native_win = (android_native_window_t *)surface->win;
	android_native_buffer_t *buffer = NULL;
	android_platform_data *platform_data = (android_platform_data *)surface->platform;
	int i, j;
	int win_width, win_height;
	ump_secure_id secure_id = UMP_INVALID_SECURE_ID;

	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_DEBUG_ASSERT_POINTER( platform_data );

	if ( !(surface->caps & SURFACE_CAP_DIRECT_RENDERING) ) return EGL_TRUE;

	old_color_target[0] = surface->buffer[0].render_target;
	old_color_target[1] = surface->buffer[1].render_target;

	native_win->query(native_win, NATIVE_WINDOW_WIDTH, &win_width);
	native_win->query(native_win, NATIVE_WINDOW_HEIGHT, &win_height );
	buffer = MALI_REINTERPRET_CAST(android_native_buffer_t *)surface->client_buffer;

	if(!buffer)
	{
		LOGI("Invalid native buffer ( NULL )! resize ignored.");
		return EGL_FALSE;
	}
	secure_id = get_buffer_ump_id( buffer );

	if ( STATE_RECREATE_DOUBLE_BUFFER == platform_data->buffer_state )
	{
		color_target[0] = __egl_platform_create_surface_from_native_buffer( buffer, surface, base_ctx );
		color_target[1] = old_color_target[0];

		if ( NULL == color_target[0] )
		{
			LOGE("EGL unable to allocate render target in %s", __func__);
			return EGL_FALSE;
		}

		for ( i=0; i<surface->num_buffers; i++ ) surface->buffer[i].render_target = color_target[i];

		/* release old data */
		_mali_surface_deref( old_color_target[1] );

		surface->width = buffer->width;
		surface->height = buffer->height;
		surface->current_buffer = 0;

		platform_data->last_ump_ids[1] = platform_data->last_ump_ids[0];
		platform_data->last_ump_ids[0] = secure_id;
		platform_data->buffer_state = STATE_OK;

#if ANDROID_ENABLE_DEBUG
		LOGD("{0x%08X} EGL ANDROID resize double buffer OK. UMP={%i, %i}", surface->platform, platform_data->last_ump_ids[0], platform_data->last_ump_ids[1]);
#endif
	}
	else if ( STATE_RECREATE_SINGLE_BUFFER == platform_data->buffer_state )
	{
		color_target[0] = __egl_platform_create_surface_from_native_buffer( buffer, surface, base_ctx );
		color_target[1] = __egl_platform_create_surface_from_native_buffer( buffer, surface, base_ctx );

		if ( NULL == color_target[0] || NULL == color_target[1] )
		{
			LOGE("EGL unable to allocate render target in %s", __func__);
			if ( NULL != color_target[0] ) _mali_surface_deref( color_target[0] );
			if ( NULL != color_target[1] ) _mali_surface_deref( color_target[1] );
			return EGL_FALSE;
		}

		for ( i=0; i<NUM_BUFFERS; i++ ) surface->buffer[i].render_target = color_target[i];

		/* release old data */
		_mali_surface_deref( old_color_target[0] );
		_mali_surface_deref( old_color_target[1] );

		surface->width = buffer->width;
		surface->height = buffer->height;
		surface->current_buffer = 0;

		platform_data->last_ump_ids[0] = secure_id;
		platform_data->last_ump_ids[1] = UMP_INVALID_SECURE_ID;
		platform_data->last_buffers[0] = buffer;
		platform_data->last_buffers[1] = NULL;
		platform_data->buffer_state = STATE_OK;

#if ANDROID_ENABLE_DEBUG
		LOGD("{0x%08X} EGL ANDROID resize single buffer OK. UMP={%i, %i}", surface->platform, platform_data->last_ump_ids[0], platform_data->last_ump_ids[1]);
#endif
	}
	{
		u32 usage = 0;
		_mali_frame_builder_get_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, &usage );
		_mali_frame_builder_set_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, surface->buffer[surface->current_buffer].render_target, usage);
	}

	return EGL_TRUE;
}

void __egl_platform_get_pixmap_size( EGLNativeDisplayType display, EGLNativePixmapType pixmap, u32 *width, u32 *height, u32 *pitch )
{
	MALI_IGNORE( display );
	MALI_IGNORE( pixmap );
	MALI_IGNORE( width );
	MALI_IGNORE ( height );
	MALI_IGNORE ( pitch );
}

EGLenum __egl_platform_get_pixmap_colorspace( EGLNativePixmapType pixmap )
{
	return EGL_COLORSPACE_sRGB;
}

EGLenum __egl_platform_get_pixmap_alphaformat( EGLNativePixmapType pixmap )
{
	return EGL_ALPHA_FORMAT_PRE;
}

void __egl_platform_get_pixmap_format( EGLNativeDisplayType display, EGLNativePixmapType pixmap, mali_surface_specifier *sformat )
{
	MALI_IGNORE( display );
	MALI_IGNORE( pixmap );
	MALI_IGNORE( sformat );
}

EGLBoolean __egl_platform_pixmap_valid( EGLNativePixmapType pixmap )
{
	MALI_IGNORE( pixmap );
	return EGL_FALSE;
}

EGLBoolean __egl_platform_pixmap_support_ump( EGLNativePixmapType pixmap )
{
	MALI_IGNORE( pixmap );

	return EGL_FALSE;
}

EGLBoolean __egl_platform_pixmap_config_compatible( EGLNativeDisplayType display, EGLNativePixmapType pixmap, egl_config *config, EGLBoolean renderable_usage )
{
	MALI_IGNORE( display );
	MALI_IGNORE( pixmap );
	MALI_IGNORE( config );
	MALI_IGNORE( renderable_usage );

	return EGL_FALSE;
}

EGLBoolean __egl_platform_pixmap_surface_compatible( EGLNativeDisplayType display, EGLNativePixmapType pixmap, egl_surface *surface, EGLBoolean alpha_check )
{
	MALI_IGNORE( display );
	MALI_IGNORE( pixmap );
	MALI_IGNORE( surface );
	MALI_IGNORE( alpha_check );
	return EGL_FALSE;
}

EGLBoolean __egl_platform_pixmap_egl_image_compatible( EGLNativePixmapType pixmap, egl_context *context )
{
	MALI_IGNORE(pixmap);
	MALI_IGNORE(context);

	return EGL_TRUE;
}

EGLBoolean __egl_platform_pixmap_copybuffers_compatible( EGLNativeDisplayType display, EGLNativePixmapType pixmap, egl_surface *surface )
{
	MALI_IGNORE( display );
	MALI_IGNORE( pixmap );
	MALI_IGNORE( surface );

	return EGL_FALSE;
}

struct mali_image* __egl_platform_map_pixmap( EGLNativeDisplayType display, struct egl_image *egl_img, EGLNativePixmapType pixmap )
{
	MALI_IGNORE( display );
	MALI_IGNORE( egl_img );
	MALI_IGNORE( pixmap );

	return NULL;
}

void __egl_platform_unmap_pixmap( EGLNativePixmapType pixmap, struct egl_image *egl_img )
{
	MALI_IGNORE( pixmap );
    MALI_IGNORE( egl_img );
}

static void __egl_platform_invalidate_android_buffers(android_buffer_state state, egl_surface *surface, android_platform_data *platform_data)
{
		platform_data->buffer_state = state;
		surface->force_resize = EGL_TRUE;
		surface->immediate_mode = EGL_TRUE;
		platform_data->stable_sequences = 0;
}

void __egl_platform_swap_buffers( mali_base_ctx_handle base_ctx,
                                  EGLNativeDisplayType native_dpy,
                                  egl_surface *surface,
                                  struct mali_surface *target,
                                  EGLint interval)
{
	android_native_window_t *native_win;
	android_native_buffer_t *buffer;
	android_platform_data *platform_data = NULL;
	int width, height;

	MALI_DEBUG_ASSERT_POINTER( surface );
	platform_data = (android_platform_data *)surface->platform;
	MALI_DEBUG_ASSERT_POINTER( platform_data );

	native_win = MALI_REINTERPRET_CAST(android_native_window_t *)surface->win;
	buffer = MALI_REINTERPRET_CAST(android_native_buffer_t *) surface->client_buffer;

	if ( platform_data->oom == MALI_TRUE )
	{
#if ANDROID_ENABLE_DEBUG
		LOGE("EGL ANDROID discovered OOM in %s:%i\n", __func__, __LINE__);
#endif
		__egl_platform_invalidate_android_buffers(STATE_RECREATE_SINGLE_BUFFER, surface, platform_data);
		return;
	}

	if ( EGL_TRUE == surface->force_resize )
	{
#if ANDROID_ENABLE_DEBUG
		LOGD("{0x%08X} EGL ANDROID callback pending on resize. Internal buffer is %i x %i (%p), given buffer is %i x %i (%p)", surface->platform, target->format.width, target->format.height, target, buffer->width, buffer->height, buffer );
		LOGD("{0x%08X} EGL ANDROID callback pending on resize. Discarding buffer %p. UMP = %i", surface->platform, buffer, get_buffer_ump_id(buffer));
#endif

#if MALI_ANDROID_API > 8
		if ( get_buffer_ump_id(buffer) != UMP_INVALID_SECURE_ID ) native_win->cancelBuffer( native_win, buffer );
		else native_win->queueBuffer(native_win, buffer);
#else
		native_win->queueBuffer( native_win, buffer );
#endif
		if ( 0 != native_win->dequeueBuffer( native_win, &buffer ) )
		{
#if ANDROID_ENABLE_DEBUG
			LOGE( "EGL ANDROID unable to dequeue buffer in %s:%i", __func__, __LINE__ );
#endif
			__egl_platform_invalidate_android_buffers(STATE_RECREATE_SINGLE_BUFFER, surface, platform_data);
			surface->client_buffer = NULL;
			platform_data->oom = MALI_TRUE;
			return;
		}

		native_win->lockBuffer( native_win, buffer );

#if ANDROID_ENABLE_DEBUG
		LOGD("{0x%08X} EGL ANDROID callback pending on resize. Dequeued new buffer %p. UMP = %i", surface->platform, buffer, get_buffer_ump_id(buffer));
#endif

		platform_data->last_buffers[1] = platform_data->last_buffers[0];
		platform_data->last_buffers[0] = buffer;

		surface->client_buffer = buffer;

		return;
	}

	/* cache the old buffer dimensions */
	width = buffer->width;
	height = buffer->height;

	native_win->queueBuffer( native_win, buffer );

	if ( 0 != native_win->dequeueBuffer( native_win, &buffer ) )
	{
#if ANDROID_ENABLE_DEBUG
		LOGE( "EGL ANDROID unable to dequeue buffer in %s:%i", __func__, __LINE__ );
#endif
		__egl_platform_invalidate_android_buffers(STATE_RECREATE_SINGLE_BUFFER, surface, platform_data);
		surface->client_buffer = NULL;
		platform_data->oom = MALI_TRUE;
		return;
	}
	native_win->lockBuffer( native_win, buffer );

	if ( width == buffer->width && height == buffer->height )
	{ /* same size */
		/* if new and old buffer have equal size and we had only one buffer, resize with two buffers */
		if( ( platform_data->last_ump_ids[1] == UMP_INVALID_SECURE_ID && platform_data->last_buffers[1] == NULL ) )
		{
#if ANDROID_ENABLE_DEBUG
			LOGD("{0x%08X} EGL ANDROID callback detected second buffer %p, same size. Set resize with two color buffers", surface->platform, buffer);
#endif
			__egl_platform_invalidate_android_buffers(STATE_RECREATE_DOUBLE_BUFFER, surface, platform_data);
		}
		else
		{
			/* the common case, we had two buffers, were given a new one, with same size as previous */
			u32 paddr, vaddr, ofs, size;
			ump_handle ump_mem_handle = UMP_INVALID_MEMORY_HANDLE;
			ump_secure_id secure_id = UMP_INVALID_SECURE_ID;

			native_buffer_get_info( buffer, &paddr, &vaddr, &ofs, &size, &ump_mem_handle );

			/* if not a framebuffer, check if we still have the same pair of UMP ids */
			if ( UMP_INVALID_MEMORY_HANDLE != ump_mem_handle )
			{
				secure_id = ump_secure_id_get(ump_mem_handle);
				if ( platform_data->last_ump_ids[0] != secure_id && platform_data->last_ump_ids[1] != secure_id )
				{
					/* the given buffer is not part of the previous UMP id sequence */
					/* mismatching UMP ids, but same size */
#if ANDROID_ENABLE_DEBUG
					LOGD("{0x%08X} EGL ANDROID callback detected mismatching UMP sequence, same size. UMP = %i vs UMP={%i, %i}", surface->platform, secure_id, platform_data->last_ump_ids[0], platform_data->last_ump_ids[1]);
					LOGD("{0x%08X} EGL ANDROID callback set resize with two color buffers", surface->platform);
#endif
					__egl_platform_invalidate_android_buffers(STATE_RECREATE_DOUBLE_BUFFER, surface, platform_data);
				}
			}

			/* go back to deferred rendering if we've had a long enough sequence of stable buffers */
#if ANDROID_ENABLE_DEBUG
			if ( (EGL_TRUE == surface->immediate_mode) && (!(platform_data->stable_sequences > STABLE_SEQUENCE_LIMIT)) ) LOGD("{0x%08X} EGL ANDROID callback detected stable sequence (%i entries)", surface->platform, platform_data->stable_sequences);
#endif
			surface->immediate_mode = platform_data->stable_sequences > STABLE_SEQUENCE_LIMIT ? EGL_FALSE : EGL_TRUE;
			if ( EGL_TRUE == surface->immediate_mode ) platform_data->stable_sequences++;
		}
	} /* end same size */
	else
	{ /* different size */
#if ANDROID_ENABLE_DEBUG
		LOGD("{0x%08X} EGL ANDROID callback detected new size", surface->platform);
		LOGD("{0x%08X} EGL ANDROID callback set resize with one color buffer", surface->platform);
#endif
		/* the dequeued buffer has got different size from the previous one */
		__egl_platform_invalidate_android_buffers(STATE_RECREATE_SINGLE_BUFFER, surface, platform_data);
	} /* end different size */

	platform_data->last_buffers[1] = platform_data->last_buffers[0];
	platform_data->last_buffers[0] = buffer;

	surface->client_buffer = buffer;
#if ANDROID_ENABLE_DEBUG
	LOGD("{0x%08X} EGL ANDROID callback end - surface %p now having client buffer %p", surface->platform, surface, surface->client_buffer);
#endif
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

EGLBoolean __egl_platform_get_window_size( EGLNativeWindowType win, void *platform, u32 *width, u32 *height )
{
	android_native_window_t *native_win = NULL;
	android_platform_data *platform_data = (android_platform_data *)platform;
	int w_width, w_height;

	MALI_DEBUG_ASSERT_POINTER( win );

	native_win = static_cast<android_native_window_t*>(win);

	if ( NULL != platform_data && platform_data->last_buffers[0] != NULL )
	{
		android_native_buffer_t *buffer = MALI_REINTERPRET_CAST(android_native_buffer_t *)platform_data->last_buffers[0];
		if ( NULL != width ) *width = buffer->width;
		if ( NULL != height ) *height = buffer->height;
	}
	else
	{
		native_win->query(native_win, NATIVE_WINDOW_WIDTH, &w_width );
		native_win->query(native_win, NATIVE_WINDOW_HEIGHT, &w_height );

		if ( NULL != width ) *width = w_width;
		if ( NULL != height ) *height = w_height;
	}

	return EGL_TRUE;
}

EGLBoolean __egl_platform_window_valid( EGLNativeDisplayType display, EGLNativeWindowType win )
{
	/* return EGL_FALSE if
	 * window is NULL / 0
	 * window does not have the proper magic value set
	 */

	MALI_IGNORE( display );

	if ( 0 == win ) return EGL_FALSE;

	if ( static_cast<android_native_window_t*>(win)->common.magic != ANDROID_NATIVE_WINDOW_MAGIC ) return EGL_FALSE;

	return EGL_TRUE;
}

EGLBoolean __egl_platform_window_compatible( EGLNativeDisplayType display, EGLNativeWindowType win, egl_config *config )
{
	MALI_IGNORE( display );
	MALI_IGNORE( win );
	MALI_IGNORE( config );

	if ( !(config->renderable_type & EGL_WINDOW_BIT) ) return EGL_FALSE;

	return EGL_TRUE;
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

unsigned int __egl_platform_get_buffer(egl_surface* surface)
{
    unsigned int current_buffer = (surface->current_buffer + 1);
	if ( current_buffer >= surface->num_buffers)
	{
		current_buffer = 0;
	}
    return current_buffer;
}

void __egl_platform_update_image(struct mali_surface * surface, void* data)
{
	MALI_IGNORE( surface );
	MALI_IGNORE( data );
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

	ioctl( fd_umplock, LOCK_IOCTL_PROCESS, &item );
#endif
}

void __egl_platform_release_lock_item( egl_surface_lock_item *lock_item )
{
#if EGL_SURFACE_LOCKING_ENABLED && MALI_USE_UNIFIED_MEMORY_PROVIDER
	_lock_item_s item;

	item.secure_id = lock_item->secure_id;
	item.usage = MALI_REINTERPRET_CAST(_lock_access_usage)lock_item->usage;

	ioctl( fd_umplock, LOCK_IOCTL_RELEASE, &item );
#endif
}
