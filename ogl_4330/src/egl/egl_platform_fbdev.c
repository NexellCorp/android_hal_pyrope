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
 * @file egl_platform_fbdev.c
 * @brief linux fbdev backend
 */

#include <mali_system.h>
#include <EGL/mali_egl.h>
#include <EGL/fbdev_window.h>
#include "egl_platform.h"
#include "egl_mali.h"
#include "egl_convert.h"
#include "egl_platform_backend_swap.h"
#if EGL_KHR_lock_surface_ENABLE
#include "egl_lock_surface.h"
#endif
#include "egl_instrumented.h"

#include <mali_system.h>
#include <shared/mali_surface.h>
#include <shared/mali_image.h>
#include <shared/mali_shared_mem_ref.h>
#include <shared/mali_pixel_format.h>
#include <shared/m200_texel_format.h>
#include <shared/m200_td.h>
#include <shared/m200_gp_frame_builder_inlines.h>

#include <base/mali_vsync.h>

#include <sys/types.h>

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
#include <ump/ump.h>
#ifdef ARM_INTERNAL_BUILD
/* Interface to the device-driver linux counterpart of this driver */
#include "../devicedrv/pl111-drv/pl111-clcd.h"
#endif
#endif /* MALI_USE_UNIFIED_MEMORY_PROVIDER */

#if EGL_SURFACE_LOCKING_ENABLED != 0
#include "../devicedrv/umplock/umplock_ioctl.h"
#endif

#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/fb.h>

#ifdef EGL_SIMULATE_VSYNC
#include <pthread.h>
#endif

#ifndef __USE_SVID
#define __USE_SVID
#endif

#define BUFFER_SIZE 128
#define NULL_WINDOW_WIDTH 320
#define NULL_WINDOW_HEIGHT 240

#define ENVVAR "MALI_FBDEV" /* Environment variable queried for window position information */

#define DEFAULT_DR_BUFFER_COUNT 2

/** framebuffer display */
typedef struct _fbdev_display
{
#if MALI_USE_DMA_BUF
	struct fbdev_dma_buf *dma_buf;
#elif MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	ump_handle ump_mem;
#endif
	mali_mem_handle fb_mem[MAX_EGL_BUFFERS]; /* mali wrapped framebuffer memory */
	mali_shared_mem_ref *fb_mem_ref[MAX_EGL_BUFFERS]; /* mali framebuffer memory */
	u32 fb_mem_offset[MAX_EGL_BUFFERS];
	u16 *fb;
	u32 id;
	s32 framebuffer_device;
	u32 orientation;
	u32 offset;
	u32 x_ofs, y_ofs;
	u32 num_dr_buffers_requested; /**< # of buffers the user specified using the MALI_MAX_WINDOW_BUFFERS env var */
	u32 num_dr_buffers; /**< The actual # of buffers used in the direct rendering swap chain */
	struct fb_var_screeninfo var_info;
	struct fb_fix_screeninfo fix_info;
	struct _fbdev_display *next;
} _fbdev_display;

/** fbdev context */
typedef struct _fbdev
{
	_fbdev_display *display;
} _fbdev;

static _fbdev *fbdev = NULL;

#if EGL_SURFACE_LOCKING_ENABLED
static int fd_umplock = -1;
#endif

typedef struct egl_surface_platform
{
#ifdef EGL_KHR_lock_surface_ENABLE
	EGLBoolean previous_frame_copied; /**< Signals that the pixel buffer contents already have been copied from the previous frame */
	EGLBoolean blit_on_swap;
	unsigned int visible_buffer;
	unsigned char* mapped_fb_memory[2]; /**< Pointers to video memory pages */
	unsigned int display_pitch;
	unsigned char* offscreen_buffer; /**< Allocated if blit_on_swap is true */
#endif
} egl_surface_platform;

static mali_bool vsync_supported;

#ifdef EGL_SIMULATE_VSYNC
/* When vsync simulation is enabled, a thread is created that calls back into a function
 * that increments the vsync ticker at given intervals (given in Hz)
 */
static pthread_t    vsync_simulation_thread;
static mali_bool    vsync_simulate = MALI_FALSE;
static u32          vsync_hertz;
static mali_bool    vsync_busy;
static u32          vsync_ticker;
static mali_mutex_handle   vsync_mutex;

static void __egl_platform_vsync_reset( void );
static u32 __egl_platform_vsync_get( void );
static void __egl_platform_vsync_callback( void );
static void *__egl_platform_vsync_simulation_thread( void *params );
static EGLBoolean __egl_platform_vsync_simulation_start( u32 hertz );
static void __egl_platform_vsync_simulation_stop( void);
static void __egl_platform_vsync_wait( u32 interval );
#endif /* defined(EGL_SIMULATE_VSYNC) */
/* forward declarations */
MALI_STATIC void __egl_platform_swap_buffers_direct_rendering( mali_base_ctx_handle base_ctx, EGLNativeDisplayType native_dpy, egl_surface *surface, struct mali_surface *target, EGLint interval );
MALI_STATIC void __egl_platform_swap_buffers_buffer_blit( mali_base_ctx_handle base_ctx, EGLNativeDisplayType native_dpy, egl_surface *surface, struct mali_surface *target, EGLint interval );

MALI_STATIC void __egl_platform_swap_buffers_nop( mali_base_ctx_handle base_ctx, EGLNativeDisplayType native_dpy, egl_surface *surface, struct mali_surface *target, EGLint interval );
MALI_STATIC void __egl_platform_copy_buffers_blit( mali_base_ctx_handle base_ctx, EGLNativeDisplayType native_dpy, egl_surface *surface, struct mali_surface *target, EGLNativePixmapType native_pixmap );
MALI_STATIC void __egl_platform_matching_mali_format( _fbdev_display *display, mali_pixel_format *format );

MALI_STATIC MALI_CHECK_RESULT _fbdev_display* __egl_platform_display_find( u32 id )
{
	_fbdev_display *display = NULL;

	display = fbdev->display;

	while ( NULL != display )
	{
		if ( display->id == id ) break;
		display = display->next;
	}

	return display;
}

EGLBoolean __egl_platform_initialize(void)
{

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	if ( UMP_OK != ump_open() ) return EGL_FALSE;
#endif

	if ( NULL == fbdev )
	{
		/* allocate memory for the fbdev context */
		fbdev = (_fbdev *)_mali_sys_malloc( sizeof( _fbdev ) );
		if ( NULL == fbdev )
		{
#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
			ump_close();
#endif
			return EGL_FALSE;
		}
		fbdev->display = NULL;
	}

#if EGL_SURFACE_LOCKING_ENABLED
		/* open umplock device */
		fd_umplock = open("/dev/umplock", O_RDWR);
		if ( fd_umplock < 0 )
		{
			MALI_DEBUG_PRINT( 0, ("WARNING: unable to open /dev/umplock\n") );
			fd_umplock = -1;
		}
#endif

	return EGL_TRUE;
}

void __egl_platform_terminate(void)
{
	if ( NULL != fbdev )
	{
		_mali_sys_free( fbdev );
		fbdev = NULL;
	}

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	ump_close();
#endif

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
	return EGL_DEFAULT_DISPLAY;
}

EGLBoolean __egl_platform_display_valid( EGLNativeDisplayType dpy )
{
#if defined( NEXELL_LEAPFROG_FEATURE_VIEWPORT_SCALE_EN ) || defined( NEXELL_LEAPFROG_FEATURE_FBO_SCALE_EN )/* nexell for leapfrog */
	if ( (dpy == EGL_DEFAULT_DISPLAY) || (dpy == (EGLNativeDisplayType)1/*fb1*/) ) return EGL_TRUE;
#else /* org */
	if ( dpy == EGL_DEFAULT_DISPLAY ) return EGL_TRUE;
#endif
	return EGL_FALSE;
}

void __egl_platform_set_display_orientation( EGLNativeDisplayType dpy, EGLint orientation )
{
	_fbdev_display *display = NULL;
	u32 type = MALI_REINTERPRET_CAST(u32)dpy;

	display = __egl_platform_display_find( type );
	if ( NULL == display ) return;

	display->orientation= orientation;
}

EGLint __egl_platform_get_display_orientation( EGLNativeDisplayType dpy )
{
	_fbdev_display *display = NULL;
	u32 type = MALI_REINTERPRET_CAST(u32)dpy;

	display = __egl_platform_display_find( type );
	if ( NULL == display ) return 0;

	return display->orientation;
}

MALI_CHECK_RESULT MALI_STATIC u32 __egl_platform_display_get_num_buffers( _fbdev_display* display )
{
	MALI_DEBUG_ASSERT_POINTER( display );
	return display->var_info.yres_virtual / display->var_info.yres;
}

MALI_CHECK_RESULT EGLBoolean __egl_platform_display_map_framebuffer_memory( u32 num_buffers, _fbdev_display *display, mali_base_ctx_handle base_ctx )
{
	u32 i = 0, j = 0;
	u32 frame_size = 0;
	EGLBoolean clear_display = EGL_TRUE;
	const char *env = NULL;
	int page_size = sysconf(_SC_PAGESIZE);
	mali_pixel_format pixelformat = MALI_PIXEL_FORMAT_NONE;

	MALI_DEBUG_ASSERT_POINTER( display );

	frame_size = display->var_info.xres * display->var_info.yres*(display->var_info.bits_per_pixel>>3);

	__egl_platform_matching_mali_format( display, &pixelformat );
	if ( MALI_PIXEL_FORMAT_NONE == pixelformat )
	{
		MALI_DEBUG_PRINT( 0, ("EGL: framebuffer pixel format not compatible with direct rendering\n") );
		return EGL_FALSE;
	}

	/* clear the framebuffer if MALI_NOCLEAR is not set */
	env = _mali_sys_config_string_get("MALI_NOCLEAR");
	if( (NULL != env) && (*env != '0')  ) clear_display = EGL_FALSE;
	_mali_sys_config_string_release(env);

	for ( i=0; i<num_buffers; i++ )
	{
		void *mapping = NULL;
		u32 physical_address = 0;
		u32 offset = 0;
		u32 map_size;

		physical_address = display->fix_info.smem_start + i*frame_size;

		/* adjust size for page alignment */
		map_size = frame_size;

		/* we need both a physical address which is page aligned - and a size that is page aligned */
		while ( ((physical_address%page_size) || (map_size%page_size))
		     && (physical_address >= display->fix_info.smem_start) && (map_size < num_buffers*frame_size) )
		{
			map_size += (map_size%page_size);
			physical_address -= (physical_address%page_size);
		}

		offset = (display->fix_info.smem_start + i*frame_size) - physical_address;

		/* NOTE: release builds will not trigger assert, but will fail setting up ump / map phys mem - and thus fallback to blitting */
		MALI_DEBUG_ASSERT( (physical_address%page_size) == 0, ("physical address is not page aligned\n") );
		MALI_DEBUG_ASSERT( (map_size%page_size) == 0, ("map size is not page aligned\n") );

#if MALI_USE_DMA_BUF
		if ( NULL != display->dma_buf )
		{
			/* TODO need to map with offset? */
			display->fb_mem[i] = _mali_mem_wrap_dma_buf(base_ctx, display->dma_buf->fd, 0);
		}
#elif MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
		if ( UMP_INVALID_MEMORY_HANDLE != display->ump_mem )
		{
			display->fb_mem[i] = _mali_mem_wrap_ump_memory(base_ctx, display->ump_mem, (physical_address - display->fix_info.smem_start) );
		}
#endif

		/* fallback to _mali_mem_add_phys_mem if UMP did not succeed, or if we do not have ump support */
		if ( MALI_NO_HANDLE == display->fb_mem[i] )
		{
			display->fb_mem[i] = _mali_mem_add_phys_mem( base_ctx, physical_address, map_size, (void*)((u32)display->fb + (physical_address - display->fix_info.smem_start)), MALI_PP_WRITE | MALI_CPU_READ | MALI_CPU_WRITE );
		}

		if ( MALI_NO_HANDLE == display->fb_mem[i] )
		{
			for ( j=0; j<i; j++ )
			{
				_mali_shared_mem_ref_owner_deref( display->fb_mem_ref[j] );
				display->fb_mem_ref[j] = MALI_NO_HANDLE;
			}
			return EGL_FALSE;
		}

		display->fb_mem_ref[i] = _mali_shared_mem_ref_alloc_existing_mem( display->fb_mem[i] );
		display->fb_mem_offset[i] = offset;
		if ( NULL == display->fb_mem_ref[i] )
		{
			for ( j=0; j<i; j++ )
			{
				_mali_shared_mem_ref_owner_deref( display->fb_mem_ref[j] );
				display->fb_mem_ref[j] = MALI_NO_HANDLE;
			}
			_mali_mem_free( display->fb_mem[i] );
			return EGL_FALSE;
		}

		/* clear the buffers while we keep creating them */
		if ( EGL_TRUE == clear_display )
		{
			mapping = _mali_mem_ptr_map_area(display->fb_mem[i], 0, display->var_info.xres * display->var_info.yres*(display->var_info.bits_per_pixel>>3), 0, MALI_MEM_PTR_WRITABLE | MALI_MEM_PTR_NO_PRE_UPDATE );
			if ( NULL != mapping )
			{
				MALI_DEBUG_PRINT( 2, ("EGL : clearing color buffer %d\n", i) );
				_mali_sys_memset( mapping, 0, frame_size );
				_mali_mem_ptr_unmap_area( display->fb_mem[i] );
			}
			else
			{
				MALI_DEBUG_PRINT( 0, ("EGL : failed to clear color buffer %d\n", i) );
			}
		}
	}

	MALI_DEBUG_PRINT( 2, ("EGL : direct rendering setup ok, cleared color buffers\n") );

	return EGL_TRUE;
}

MALI_STATIC MALI_CHECK_RESULT mali_surface* __egl_platform_display_create_mali_surface_from_framebuffer(
	u32 buffer_index,
	_fbdev_display *display,
	egl_surface const *fb_surface,
	u32 width,
	u32 height,
	mali_base_ctx_handle base_ctx )
{
	mali_surface_specifier sformat;
	mali_surface *surface = NULL;
	mali_pixel_format pixelformat = MALI_PIXEL_FORMAT_NONE;
	u32 pitch;

	MALI_DEBUG_ASSERT_POINTER( display );
	MALI_DEBUG_ASSERT_POINTER( fb_surface );
	MALI_DEBUG_ASSERT( buffer_index < MAX_EGL_BUFFERS, ("EGL only supports max %i buffers\n", MAX_EGL_BUFFERS) );

	__egl_platform_matching_mali_format( display, &pixelformat );
	if ( MALI_PIXEL_FORMAT_NONE == pixelformat ) return NULL;

	_mali_surface_specifier_ex( &sformat, width, height, 0, pixelformat, _mali_pixel_to_texel_format(pixelformat),
	                            MALI_PIXEL_LAYOUT_LINEAR, _mali_pixel_layout_to_texel_layout(MALI_PIXEL_LAYOUT_LINEAR),
								MALI_FALSE, MALI_FALSE,
								( EGL_COLORSPACE_sRGB == fb_surface->colorspace ) ? MALI_SURFACE_COLORSPACE_sRGB : MALI_SURFACE_COLORSPACE_lRGB,
								( EGL_ALPHA_FORMAT_PRE == fb_surface->alpha_format ) ? MALI_SURFACE_ALPHAFORMAT_PRE : MALI_SURFACE_ALPHAFORMAT_NONPRE,
								( fb_surface->config->alpha_size == 0) ? MALI_TRUE : MALI_FALSE );

	MALI_CHECK( NULL != display->fb_mem_ref[buffer_index], NULL );

	pitch = display->var_info.xres * ( _mali_surface_specifier_bpp( &sformat ) / 8 );

	sformat.pitch = pitch;

	surface = _mali_surface_alloc_empty( MALI_SURFACE_FLAG_DONT_MOVE, &sformat, base_ctx );
	if ( surface == NULL) return NULL;

	surface->mem_ref  = display->fb_mem_ref[buffer_index];
	surface->mem_offset = display->fb_mem_offset[buffer_index] + display->offset * (display->var_info.bits_per_pixel>>3);
	surface->datasize = _mali_mem_size_get( surface->mem_ref->mali_memory ) - surface->mem_offset;

	_mali_shared_mem_ref_owner_addref( display->fb_mem_ref[buffer_index] );

	return surface;
}

#ifdef ARM_INTERNAL_BUILD
#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
MALI_STATIC MALI_CHECK_RESULT _fbdev_display* __egl_platform_display_search_and_create_ump( u32 num, mali_base_ctx_handle base_ctx )
{
	s32 fb_dev;
	_fbdev_display *display = NULL;
	char path[BUFFER_SIZE];
	ump_handle ump_mem = UMP_INVALID_MEMORY_HANDLE;
	ump_secure_id ump_id = UMP_INVALID_SECURE_ID;
	char* fb_dev_filename;
	MALI_IGNORE( base_ctx );

	/* choose which framebuffer device file to open */
	if ( 0 == num ) /* EGL_DEFAULT_DISPLAY */
	{
		fb_dev_filename = (char *)_mali_sys_config_string_get("FRAMEBUFFER");
		if (NULL == fb_dev_filename)
		{
			fb_dev_filename = "/dev/fb0";
		}
	}
	else
	{
		/* change u32 num to a const char* pointing to device filename? */
		_mali_sys_snprintf( path, BUFFER_SIZE, "/dev/fb%i", (int)num );
		fb_dev_filename = path;
	}


	/* open the fb device */
	fb_dev = open( fb_dev_filename, O_RDWR );
	if ( fb_dev < 0 ) return NULL;

	/* get the secure ID for the framebuffer */
	(void)ioctl( fb_dev, PL111_IOCTL_GET_FB_UMP_SECURE_ID, &ump_id );
	if ( UMP_INVALID_SECURE_ID == ump_id)
	{
		(void)close( fb_dev );
		return NULL;
	}
	ump_mem = ump_handle_create_from_secure_id(ump_id);
	if ( UMP_INVALID_MEMORY_HANDLE == ump_mem )
	{
		(void)close( fb_dev );
		return NULL;
	}

	display = (_fbdev_display *)_mali_sys_calloc( 1, sizeof ( _fbdev_display ) );
	if ( NULL == display )
	{
		ump_reference_release(ump_mem);
		(void)close( fb_dev );
		return NULL;
	}

	display->framebuffer_device = fb_dev;
	display->ump_mem = ump_mem;

	return display;
}
#endif /* MALI_USE_UNIFIED_MEMORY_PROVIDER != 0 */
#endif /* defined(ARM_INTERNAL_BUILD) */

MALI_STATIC MALI_CHECK_RESULT _fbdev_display* __egl_platform_display_search_and_create_map( u32 num, mali_base_ctx_handle base_ctx )
{
	s32 fb_dev;
	u16 *fb = NULL;
	_fbdev_display *display = NULL;
	char path[BUFFER_SIZE];
	size_t size;
	struct fb_var_screeninfo var_info;
	char* fb_dev_filename;
	MALI_IGNORE( base_ctx );

	/* choose which framebuffer device file to open */
	if ( 0 == num ) /* EGL_DEFAULT_DISPLAY */
	{
		fb_dev_filename = (char *)_mali_sys_config_string_get("FRAMEBUFFER");
		if ( NULL == fb_dev_filename )
		{
			fb_dev_filename = "/dev/fb0";
		}
	}
	else
	{
		/* change u32 num to a const char* pointing to device filename? */
		_mali_sys_snprintf( path, BUFFER_SIZE, "/dev/fb%i", (int)num );
		fb_dev_filename = path;
	}

	/* open the fb device */
	fb_dev = open( fb_dev_filename, O_RDWR );
	if ( fb_dev < 0 )
	{
		MALI_DEBUG_PRINT( 0, ("EGL : could not open framebuffer device %s\n", fb_dev_filename) );
		return EGL_FALSE;
	}

	/* get variable screen info (size, bpp, etc) */
	if ( -1 == ioctl( fb_dev, FBIOGET_VSCREENINFO, &var_info ) )
	{
		MALI_DEBUG_PRINT( 0, ("EGL : error getting framebuffer information") );
		return EGL_FALSE;
	}

	size = (size_t)( var_info.xres * var_info.yres_virtual * ( var_info.bits_per_pixel>>3 ) );

	fb = (u16 *)mmap( (void*)0, size, PROT_WRITE, MAP_SHARED, fb_dev, 0 );
	if ( MAP_FAILED == fb )
	{
		(void)close( fb_dev );
		return NULL;
	}

	display = (_fbdev_display *)_mali_sys_calloc( 1, sizeof ( _fbdev_display ) );
	if ( NULL == display)
	{
		(void)munmap( fb, size );
		(void)close( fb_dev );
		return NULL;
	}

	display->framebuffer_device = fb_dev;
	display->fb = fb;

	return display;
}

MALI_STATIC void __egl_platform_display_get_environment_variables( _fbdev_display *display )
{
	const char *env = NULL;
	unsigned int quadrant = 0;
	unsigned int orientation = 0;

	/* Store the nr of buffers requested by the user, so we can
	 * give a warning later if we're not able to mmap the
	 * requested amount of buffers */
	display->num_dr_buffers_requested = (u32)_mali_sys_config_string_get_s64( "MALI_MAX_WINDOW_BUFFERS", DEFAULT_DR_BUFFER_COUNT, 2, MAX_EGL_BUFFERS );

	env = _mali_sys_config_string_get( ENVVAR );
	if ( NULL != env )
	{
		switch ( env[0] )
		{
			case 'Q':
				/* Expect Q1, 2, 3 or 4 */
				quadrant = env[1] - '1';
				if ( quadrant < 4 )
				{
					int half_xres = display->var_info.xres >> 1;
					int half_yres = display->var_info.yres >> 1;
					/* quadrant is 0..3 */

					/* xoffset */
					display->x_ofs = (half_xres ) * (quadrant & 1);
					display->offset += display->x_ofs;

					/* yoffset */
					display->y_ofs = (half_yres) * ((quadrant & 2 ) >> 1 );
					display->offset += display->var_info.xres * display->y_ofs;

					/* align the display->offset to MALI200_SURFACE_ALIGNMENT */
					display->offset = ((display->offset+MALI200_SURFACE_ALIGNMENT-1)/MALI200_SURFACE_ALIGNMENT)*MALI200_SURFACE_ALIGNMENT;
				}
				break;

			case 'R':
				orientation = env[1] - '0';
				if ( 0 == orientation  ) display->orientation = EGL_NATIVE_DISPLAY_ROTATE_0;
				else if ( 1 == orientation ) display->orientation = EGL_NATIVE_DISPLAY_ROTATE_90;
				else if ( 2 == orientation ) display->orientation = EGL_NATIVE_DISPLAY_ROTATE_180;
				else if ( 3 == orientation ) display->orientation = EGL_NATIVE_DISPLAY_ROTATE_270;
				break;

			default:
				break;
		}
	}
}

MALI_STATIC MALI_CHECK_RESULT EGLBoolean __egl_platform_display_search_and_create( u32 num, mali_base_ctx_handle base_ctx )
{
	_fbdev_display *display = NULL;
	_fbdev_display *display_ptr = NULL;
	u32 i;
	u32 max_fb_buffers_available;

	if ( NULL != __egl_platform_display_find( num ) ) return EGL_TRUE; /* display exists, this is no error */

#ifdef ARM_INTERNAL_BUILD
#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	display = __egl_platform_display_search_and_create_ump( num, base_ctx );
	if ( NULL == display )
	{
		MALI_DEBUG_PRINT( 3, ("Warning: EGL failed to utilize UMP\n") );
	}
#endif
#endif

	/* display will be null both if UMP is not enabled and if UMP fails */
	if ( NULL == display )
	{
		display = __egl_platform_display_search_and_create_map( num, base_ctx );
		if ( NULL == display ) return EGL_FALSE; /* failed to create new display */
		/* if we entered a fallback from UMP to map, update the display structure to have an invalid ump handle, or else it will be undefined */
#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
		display->ump_mem = UMP_INVALID_MEMORY_HANDLE;
#endif
	}

	/* Initialize display defaults */
	display->id = num;
	display->orientation = EGL_NATIVE_DISPLAY_ROTATE_0;
	display->offset = 0;
	display->x_ofs = 0;
	display->y_ofs = 0;
	display->next = NULL;
	display->num_dr_buffers_requested = DEFAULT_DR_BUFFER_COUNT;
	for ( i=0; i<MAX_EGL_BUFFERS; ++i )
	{
		display->fb_mem[i] = MALI_NO_HANDLE;
		display->fb_mem_ref[i] = NULL;
		display->fb_mem_offset[i] = 0;
	}

	(void)ioctl( display->framebuffer_device, FBIOGET_VSCREENINFO, &display->var_info );
	(void)ioctl( display->framebuffer_device, FBIOGET_FSCREENINFO, &display->fix_info );

#ifndef FBDEV_FULLSCREEN
	__egl_platform_display_get_environment_variables( display );
#endif

	max_fb_buffers_available = __egl_platform_display_get_num_buffers( display );
	/* Cap max_fb_buffers_available to a predefined maximumum */
	max_fb_buffers_available = MIN( max_fb_buffers_available, MAX_EGL_BUFFERS );
	display->num_dr_buffers = MIN( max_fb_buffers_available, display->num_dr_buffers_requested );

	/* try to setup direct rendering if we have virtual y resolution larger than physical resolution */
	if ( 1 < max_fb_buffers_available )
	{
		if ( display->num_dr_buffers != display->num_dr_buffers_requested )
		{
			MALI_DEBUG_PRINT( 1, ("EGL : Warning: %i direct rendering buffers were requested, but only %i are available\n", display->num_dr_buffers_requested, display->num_dr_buffers) );
		}
		if ( EGL_FALSE == __egl_platform_display_map_framebuffer_memory( display->num_dr_buffers, display, base_ctx ) )
		{
			MALI_DEBUG_PRINT( 0, ("Warning: EGL unable to setup direct rendering - fallback to blit\n") );
			for ( i=0; i<MAX_EGL_BUFFERS; ++i )
			{
				display->fb_mem[i] = MALI_NO_HANDLE;
				display->fb_mem_ref[i] = NULL;
			}
		}
	}

	if ( NULL != fbdev->display )
	{
		/* attach the display to the end of the last display found */
		display_ptr = fbdev->display;
		while ( NULL != display_ptr->next ) display_ptr = display_ptr->next;
		display_ptr->next = display;
	}
	else
	{
		/* this display is now the first one */
		fbdev->display = display;
	}

	return EGL_TRUE;
}

/* dummy version of __egl_platform_display_search_and_create() which avoids dependencies
 * on /dev/fbdev (Palladium kernels boot faster without fbdev)
 */
MALI_STATIC MALI_CHECK_RESULT EGLBoolean __egl_platform_display_search_and_create_dummy( u32 num )
{
	u16 *fb = NULL;
	_fbdev_display *display = NULL;
	_fbdev_display *display_new = NULL;
	size_t size;
	u32 i;

	if ( NULL != __egl_platform_display_find( num ) ) return EGL_TRUE; /* display exists */

	/* 320 x 240, 16 bpp
	 */
	size = (size_t)( 320 * 240 * 2 );

	/* fake mapping of framebuffer
	 */
	fb = (u16 *) _mali_sys_malloc( size );

	display_new = (_fbdev_display *)_mali_sys_calloc( 1, sizeof ( _fbdev_display ) );
	if ( NULL == display_new )
	{
		_mali_sys_free( fb );
		return EGL_FALSE;
	}

	display_new->framebuffer_device = -1;
	display_new->fb = fb;
	display_new->id = num;
	display_new->orientation = EGL_NATIVE_DISPLAY_ROTATE_0;
	for ( i=0; i<MAX_EGL_BUFFERS; ++i )
	{
		display_new->fb_mem[i] = MALI_NO_HANDLE;
		display_new->fb_mem_ref[i] = NULL;
	}
	display_new->offset = 0;
	display_new->next = NULL;

	display_new->var_info.red.offset = 11;
	display_new->var_info.red.length = 5;
	display_new->var_info.red.msb_right = 0;

	display_new->var_info.green.offset = 5;
	display_new->var_info.green.length = 6;
	display_new->var_info.green.msb_right = 0;

	display_new->var_info.blue.offset = 0;
	display_new->var_info.blue.length = 5;
	display_new->var_info.blue.msb_right = 0;

	display_new->var_info.transp.offset = 0;
	display_new->var_info.transp.length = 0;
	display_new->var_info.transp.msb_right = 0;

	if ( NULL != fbdev->display )
	{
		/* attach the display to the last display found */
		display = fbdev->display;
		while ( NULL != display->next ) display = display->next;
		display->next = display_new;
	}
	else
	{
		/* this display is the first one */
		fbdev->display = display_new;
	}

	return EGL_TRUE;
}

EGLBoolean __egl_platform_init_display( EGLNativeDisplayType dpy, mali_base_ctx_handle base_ctx )
{
	__egl_main_context *egl = NULL;
	EGLBoolean retval = EGL_FALSE;

	egl = __egl_get_main_context();
	MALI_DEBUG_ASSERT_POINTER( egl );

	if ( MALI_FALSE == egl->never_blit ) retval = __egl_platform_display_search_and_create( (u32)dpy, base_ctx );
	else retval = __egl_platform_display_search_and_create_dummy( (u32)dpy );

	return retval;
}

void __egl_platform_deinit_display( EGLNativeDisplayType dpy )
{
	EGLBoolean ump_success = EGL_FALSE;
	EGLBoolean found = EGL_FALSE;
	_fbdev_display *display = NULL;
	_fbdev_display *display_next = NULL;
	_fbdev_display *display_prev = NULL;
	__egl_main_context *egl = NULL;
	u32 type = MALI_REINTERPRET_CAST(u32)dpy;
	u32 i;

	egl = __egl_get_main_context();
	MALI_DEBUG_ASSERT_POINTER( egl );

	if ( NULL == fbdev ) return;
	if ( NULL == fbdev->display ) return;

	/* search through the list of displays looking for the native display type */
	display = fbdev->display;
	do
	{
		if ( display->id == type )
		{
			found = EGL_TRUE;
			break;
		}
		display_prev = display;
		display = display->next;
	} while ( NULL != display->next );

	if ( EGL_FALSE == found ) return;

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	if ( UMP_INVALID_MEMORY_HANDLE != display->ump_mem )
	{
		ump_success = EGL_TRUE;
		ump_reference_release( display->ump_mem );
		display->ump_mem = UMP_INVALID_MEMORY_HANDLE;
	}
#endif /* MALI_USE_UNIFIED_MEMORY_PROVIDER != 0 */

	if ( EGL_FALSE == ump_success )
	{
		if ( NULL == display->fb ) return;
		if( MALI_FALSE == egl->never_blit )
		{
			(void)munmap( display->fb, (size_t)( display->var_info.xres *
			                                     display->var_info.yres_virtual *
			                                     ( display->var_info.bits_per_pixel>>3 ) ) );
		}
		else
		{
			_mali_sys_free( display->fb );
		}
		display->fb = NULL;
	}

	for ( i=0; i<MAX_EGL_BUFFERS; ++i )
	{
		if ( NULL != display->fb_mem_ref[i] )
		{
			_mali_shared_mem_ref_owner_deref( display->fb_mem_ref[i] );
			display->fb_mem[i] = MALI_NO_HANDLE;
			display->fb_mem_ref[i] = NULL;
		}
	}

	(void)close( display->framebuffer_device );
	display->framebuffer_device = -1;

	/* remove the display from the list */
	if ( display == fbdev->display )
	{
		/* display is first, and has a linked display */
		if ( NULL != display->next )
		{
			display_next = display->next;
			_mali_sys_free( display );
			fbdev->display = display_next;
		}
		else /* display is first, and has no linked displays */
		{
			_mali_sys_free( display );
			fbdev->display = NULL;
		}
	} else
	{
		if ( NULL != display->next )
		{
			/* set the previous display to point to this displays
			 * next display. */
			display_next = display->next;
			_mali_sys_free( display );
			if ( NULL != display_prev ) display_prev->next = display_next;
		}
		else
		{
			_mali_sys_free( display );
			if ( NULL != display_prev ) display_prev->next = NULL;
		}
	}
}

void __egl_platform_display_get_format( EGLNativeDisplayType dpy,
                                        egl_display_native_format *format )
{
	_fbdev_display *display = NULL;
	u32 type = MALI_REINTERPRET_CAST(u32)dpy;

	if ( NULL == format ) return;

	display = __egl_platform_display_find( type );
	if ( NULL == display ) return;

	format->red_size = display->var_info.red.length;
	format->red_offset = display->var_info.red.offset;

	format->green_size = display->var_info.green.length;
	format->green_offset = display->var_info.green.offset;

	format->blue_size = display->var_info.blue.length;
	format->blue_offset = display->var_info.blue.offset;
}

EGLBoolean __egl_platform_wait_native(EGLint engine)
{
	MALI_IGNORE(engine);

	return EGL_TRUE;
}

MALI_STATIC EGLBoolean __egl_platform_supports_direct_rendering( egl_surface *surface, _fbdev_display *display, __egl_main_context *egl)
{
	u32 i;

	/* verify that direct rendering is supported */
	if ( (RENDER_METHOD_DIRECT_RENDERING != surface->config->render_method) ) return EGL_FALSE;

	/* verify that MALI_NEVERBLIT envvar is not set */
	if ( MALI_TRUE == egl->never_blit ) return EGL_FALSE;

	/* surface has to be double buffered */
	if ( EGL_BACK_BUFFER != surface->render_buffer ) return EGL_FALSE;

	/* verify that we do in fact have a virtual display at least two times larger than physical display */
	if ( __egl_platform_display_get_num_buffers( display ) < 2 ) return EGL_FALSE;

	/* verify that the surface width&height is less than or equal to fbdev display */
	if ( (surface->width > display->var_info.xres) || (surface->height > display->var_info.yres) ) return EGL_FALSE;

	/* verify that we have the color buffer set up correctly */
	for ( i=0; i<display->num_dr_buffers; ++i )
	{
		if ( MALI_NO_HANDLE == display->fb_mem[i] ) return EGL_FALSE;
	}

#if HARDWARE_ISSUE_4472
	/* direct rendering in combination with supersampling for non-tile aligned sizes will not work for <= r0p4 */
	if ( (surface->height % MALI200_TILE_SIZE) && (surface->config->samples > 4) )
	{
		MALI_DEBUG_PRINT( 0, ("EGL: Direct rendering disabled for non tile-aligned surfaces with supersampling enabled for <= r0p4 hardware revision. See hardware issue 4472.\n") );
		return EGL_FALSE;
	}
#endif /* HARDWARE_ISSUE_4472 */

	/* direct rendering is okay if we have reached this far */
	return EGL_TRUE;
}

EGLBoolean __egl_platform_supports_single_buffer_rendering( egl_surface *surface, _fbdev_display *display, __egl_main_context *egl )
{
	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_DEBUG_ASSERT_POINTER( display );
	MALI_DEBUG_ASSERT_POINTER( egl );

	/* verify that single buffered rendering is requested */
	if ( EGL_SINGLE_BUFFER != surface->render_buffer ) return EGL_FALSE;

	/* verify that the surface bpp matches the display bpp */
	if ( (u32)surface->config->buffer_size != display->var_info.bits_per_pixel ) return EGL_FALSE;

	/* verify that the surface is no larger than the physical display */
	if ( (surface->width > display->var_info.xres) || (surface->height > display->var_info.yres) ) return EGL_FALSE;

	/* get phys mem, if not already retrieved */
	if ( MALI_NO_HANDLE == display->fb_mem[0] )
	{
		if ( EGL_FALSE == __egl_platform_display_map_framebuffer_memory( 1, display, egl->base_ctx ) )
		{
			MALI_DEBUG_PRINT( 0, ("EGL: Failed to setup single buffered direct rendering\n") );
			return EGL_FALSE;
		}
	}

	return EGL_TRUE;
}

#if EGL_KHR_lock_surface_ENABLE
/** Generic surface copy. No pixel format conversion.
 * @param from source area
 * @param from_pitch number of bytes separating lines in source area
 * @param to target area
 * @param to_pitch number of bytes separating lines in target area
 * @param line_length length of one line, in bytes
 * @param num_lines the total number of lines to copy
 */
MALI_STATIC void __egl_platform_locksurface_blit(
	unsigned char* from,
	unsigned int from_pitch,
	unsigned char* to,
	unsigned int to_pitch,
	unsigned int line_length,
	unsigned int num_lines )
{
	unsigned int line;
	if ( ( from_pitch == to_pitch ) && ( line_length == to_pitch ) )
	{
		/* full window copy. */
		_mali_sys_memcpy( to, from, line_length * num_lines );
		return;
	}

	/* Surface pitches do not match, or the window is not full screen.
	 * Do a line-by-line copy.
	 */
	for (line = 0; line < num_lines; ++line, from += from_pitch, to += to_pitch)
	{
		_mali_sys_memcpy( to, from, line_length );
	}
}

MALI_STATIC void __egl_platform_locksurface_blit_to_window( mali_base_ctx_handle base_ctx,
                                                            EGLNativeDisplayType native_dpy,
                                                            egl_surface *surface,
                                                            struct mali_surface *target,
                                                            EGLint interval )
{
	egl_surface_platform* platform = NULL;
	unsigned int line_length;
	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_DEBUG_ASSERT_POINTER( surface->lock_surface );
	platform = MALI_REINTERPRET_CAST(egl_surface_platform*)surface->platform;
	MALI_DEBUG_ASSERT_POINTER( platform );
	MALI_IGNORE( base_ctx );
	MALI_IGNORE( native_dpy );
	MALI_IGNORE( target );
	if ( 0 != interval && vsync_supported )
	{
#ifdef EGL_SIMULATE_VSYNC
		__egl_platform_vsync_wait( interval );
#endif
	}
	line_length = surface->width * surface->config->buffer_size / 8;
	__egl_platform_locksurface_blit(
		platform->offscreen_buffer,
		surface->lock_surface->bitmap_pitch,
		platform->mapped_fb_memory[0],
		platform->display_pitch,
		line_length,
		surface->height
	);
}

MALI_STATIC EGLBoolean __egl_platform_create_lockable_window_surface( _fbdev_display* display, egl_surface* surface )
{
	egl_surface_platform* platform = NULL;
	/* Verify that the surface config matches actual pixel format, if not return with error */
	switch ( surface->config->match_format_khr )
	{
		case EGL_FORMAT_RGB_565_EXACT_KHR:
			/* R5 G6 B5 in MSB to LSB order */
			if ( ( display->var_info.red.length   != 5 ) ||
			     ( display->var_info.red.offset   != 11 ) ||
			     ( display->var_info.green.length != 6 ) ||
			     ( display->var_info.green.offset != 5 ) ||
			     ( display->var_info.blue.length  != 5 ) ||
			     ( display->var_info.blue.offset  != 0 ) )
			{
				return EGL_FALSE;
			}
			break;
		case EGL_FORMAT_RGB_565_KHR:
			/* A1 R5 G5 B5 in MSB to LSB order */
			if ( ( display->var_info.red.length   != 5 ) ||
			     ( display->var_info.red.offset   != 10 ) ||
			     ( display->var_info.green.length != 5 ) ||
			     ( display->var_info.green.offset != 5 ) ||
			     ( display->var_info.blue.length  != 5 ) ||
			     ( display->var_info.blue.offset  != 0 ) )
			{
				return EGL_FALSE;
			}
			break;
		case EGL_FORMAT_RGBA_8888_EXACT_KHR:
			/* The byte order in memory should be BGRA */
			if ( ( display->var_info.red.length   != 8 ) ||
			     ( display->var_info.red.offset   != 8 ) ||
			     ( display->var_info.green.length != 8 ) ||
			     ( display->var_info.green.offset != 16 ) ||
			     ( display->var_info.blue.length  != 8 ) ||
			     ( display->var_info.blue.offset  != 24 ) )
			{
				return EGL_FALSE;
			}
			break;
		case EGL_FORMAT_RGBA_8888_KHR:
			/* The byte order in memory should be ARGB */
			if ( ( display->var_info.red.length   != 8 ) ||
			     ( display->var_info.red.offset   != 16 ) ||
			     ( display->var_info.green.length != 8 ) ||
			     ( display->var_info.green.offset != 8 ) ||
			     ( display->var_info.blue.length  != 8 ) ||
			     ( display->var_info.blue.offset  != 0 ) )
			{
				return EGL_FALSE;
			}
			break;
		default:
			return EGL_FALSE;
	}
	/* Fill in component shift values */
	surface->lock_surface->bitmap_pixel_red_offset = display->var_info.red.offset;
	surface->lock_surface->bitmap_pixel_green_offset = display->var_info.green.offset;
	surface->lock_surface->bitmap_pixel_blue_offset = display->var_info.blue.offset;
	surface->lock_surface->bitmap_pixel_alpha_offset = display->var_info.transp.offset;
	surface->lock_surface->bitmap_pixel_luminance_offset = 0;
	/* First mapped pixel is at the top-left corner. */
	surface->lock_surface->bitmap_origin = EGL_UPPER_LEFT_KHR;
	surface->lock_surface->bitmap_pixel_size = __mali_pixel_format_get_bpp(surface->config->pixel_format);
	surface->num_buffers = 0; /* not used by locksurface. */

	platform = MALI_REINTERPRET_CAST(egl_surface_platform*)_mali_sys_calloc(1, sizeof(egl_surface_platform));
	if ( NULL == platform) return EGL_FALSE;

	surface->platform = (void*)platform;

	platform->mapped_fb_memory[0] = MALI_REINTERPRET_CAST(unsigned char*)display->fb;
	platform->display_pitch = display->var_info.xres*(display->var_info.bits_per_pixel >> 3);

	if ( __egl_platform_display_get_num_buffers( display ) < 2 )
	{
		/* Not enough framebuffer memory to reserve a backbuffer.
		 * Must draw to non-video memory and blit on swapbuffers
		 */
		platform->blit_on_swap = EGL_TRUE;
		platform->mapped_fb_memory[1] = NULL;
		surface->lock_surface->bitmap_pitch = surface->width * surface->config->buffer_size / 8;
		platform->offscreen_buffer = (unsigned char*)_mali_sys_malloc(surface->height * surface->lock_surface->bitmap_pitch);
		if ( NULL == platform->offscreen_buffer )
		{
			_mali_sys_free(platform);
			return EGL_FALSE;
		}
		_mali_sys_memset( platform->offscreen_buffer, 0, surface->width * surface->config->buffer_size );
		surface->swap_func = __egl_platform_locksurface_blit_to_window;
	}
	else
	{
		/* Map backbuffer and pan on swapbuffers */
		platform->blit_on_swap = EGL_FALSE;
		platform->offscreen_buffer = NULL;
		platform->visible_buffer = 0 == display->var_info.yoffset ? 0 : 1;
		surface->lock_surface->bitmap_pitch = platform->display_pitch;
		platform->mapped_fb_memory[1] = platform->mapped_fb_memory[0] + display->var_info.yres * surface->lock_surface->bitmap_pitch;
		surface->swap_func = __egl_platform_swap_buffers_direct_rendering;
	}
	return EGL_TRUE;
}

EGLBoolean __egl_platform_lock_surface_map_buffer( EGLNativeDisplayType dpy, egl_surface* surface, EGLBoolean preserve_contents )
{
	egl_surface_platform* platform;
#if MALI_USE_DMA_BUF
	fbdev_pixmap* fb_pixmap = NULL;
#elif MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
		fbdev_pixmap* fb_pixmap = NULL;
		ump_handle pixmap_ump_handle = UMP_INVALID_MEMORY_HANDLE;
#endif /* MALI_USE_UNIFIED_MEMORY_PROVIDER != 0 */

	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_IGNORE( dpy );

	if ( surface->type == MALI_EGL_WINDOW_SURFACE )
	{
		MALI_DEBUG_ASSERT_POINTER( surface->platform );
		platform = MALI_REINTERPRET_CAST(egl_surface_platform*)surface->platform;

		if ( EGL_TRUE == preserve_contents &&
		     EGL_FALSE == platform->previous_frame_copied &&
		     EGL_FALSE == platform->blit_on_swap )
		{
			/* Copy visible buffer to backbuffer, to preserve pixel contents */
			const unsigned int backbuffer = 1 - platform->visible_buffer;
			const unsigned int line_length = surface->width * surface->config->buffer_size / 8;
			__egl_platform_locksurface_blit(
				platform->mapped_fb_memory[platform->visible_buffer],
				platform->display_pitch,
				platform->mapped_fb_memory[backbuffer],
				platform->display_pitch,
				line_length,
				surface->height
			);
			platform->previous_frame_copied = EGL_TRUE;
		}

		/* surface->lock_surface->bitmap_pitch is updated on surface creation */
		if ( EGL_TRUE == platform->blit_on_swap )
		{
			surface->lock_surface->mapped_pointer = MALI_REINTERPRET_CAST(void*)platform->offscreen_buffer;
		}
		else
		{
			const unsigned int backbuffer = 1 - platform->visible_buffer;
			surface->lock_surface->mapped_pointer = MALI_REINTERPRET_CAST(void*)platform->mapped_fb_memory[backbuffer];
		}
	}
	else if ( surface->type == MALI_EGL_PIXMAP_SURFACE )
	{
#if MALI_USE_DMA_BUF
		fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap*)surface->pixmap;
		MALI_DEBUG_ASSERT_POINTER( fb_pixmap );
		if ( FBDEV_PIXMAP_SUPPORTS_UMP & fb_pixmap->flags )
		{
			struct fbdev_dma_buf *buf = (struct fbdev_dma_buf *)fb_pixmap->data;
			if (NULL == buf) return EGL_FALSE;

			surface->lock_surface->mapped_pointer = buf->ptr;
			surface->lock_surface->bitmap_pitch = fb_pixmap->width*fb_pixmap->bytes_per_pixel;

		}
#elif MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
		fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap*)surface->pixmap;
		MALI_DEBUG_ASSERT_POINTER( fb_pixmap );
		if ( FBDEV_PIXMAP_SUPPORTS_UMP & fb_pixmap->flags )
		{
			/* UMP-pixmap */
			pixmap_ump_handle = MALI_REINTERPRET_CAST(ump_handle)fb_pixmap->data;
			if ( UMP_INVALID_MEMORY_HANDLE == pixmap_ump_handle ) return EGL_FALSE;
			surface->lock_surface->mapped_pointer = (void *)ump_mapped_pointer_get( pixmap_ump_handle );
			surface->lock_surface->bitmap_pitch = fb_pixmap->width*fb_pixmap->bytes_per_pixel;
		}
#endif /* MALI_USE_UNIFIED_MEMORY_PROVIDER != 0 */
	}

	return EGL_TRUE;
}

EGLBoolean __egl_platform_lock_surface_unmap_buffer( EGLNativeDisplayType dpy, egl_surface* surface )
{
	MALI_IGNORE( dpy );
	MALI_IGNORE( surface );
	return EGL_TRUE;
}

#endif

EGLBoolean __egl_platform_create_surface_window( egl_surface *surface, mali_base_ctx_handle base_ctx, _fbdev_display *display )
{
	__egl_main_context *egl = NULL;
	mali_surface *buf_direct[MAX_EGL_BUFFERS];
	mali_surface *buf_implicit = NULL;
	mali_bool implicit_buffer = MALI_FALSE;
	const char *env;
	u32 i;
	u32 num_buffers = 0;
	mali_pixel_format wbconv_pformat = MALI_PIXEL_FORMAT_NONE;

	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_DEBUG_ASSERT_POINTER( display );

#if EGL_KHR_lock_surface_ENABLE
	if ((surface->config->surface_type & EGL_LOCK_SURFACE_BIT_KHR) != 0)
	{
		return __egl_platform_create_lockable_window_surface( display, surface );
	}
#endif

	egl = __egl_get_main_context();
	MALI_DEBUG_ASSERT_POINTER( egl );

	for ( i=0; i<MAX_EGL_BUFFERS; ++i ) buf_direct[i] = NULL;

	/* set up single buffered direct rendering if the env variable MALI_SINGLEBUFFER is set */
	env = _mali_sys_config_string_get( "MALI_SINGLEBUFFER" );
	if ( (NULL != env) && (*env != '0') ) surface->render_buffer = EGL_SINGLE_BUFFER;
	_mali_sys_config_string_release( env );
	surface->frame_builder = NULL;

	/* check if dummy framebuffer */
	if ( MALI_TRUE == egl->never_blit )
	{
	    /* adjust display parameters here to pass further checks in the 32 bit mode */
		if ( surface->config->red_size == 8 )
		{
			display->var_info.red.length = 8;
			display->var_info.green.length = 8;
			display->var_info.blue.length = 8;
			display->var_info.transp.length = 8;
			display->var_info.red.offset = 0;
			display->var_info.green.offset = 8;
			display->var_info.blue.offset = 16;
			display->var_info.transp.offset = 24;
		}
	}

	/* enable writeback conversion if display vs surface format does not match */
	if ( ((u32)surface->config->red_size != display->var_info.red.length) ||
			((u32)surface->config->green_size != display->var_info.green.length) ||
			((u32)surface->config->blue_size != display->var_info.blue.length) )
	{
		/* check if we have a matching mali format for the framebuffer format */
		__egl_platform_matching_mali_format( display, &wbconv_pformat );
		if ( MALI_PIXEL_FORMAT_NONE == wbconv_pformat )
		{
			MALI_DEBUG_PRINT( 0, ("EGL: Direct rendering with writeback conversion not enabled. Could not find a matching mali pixel format for display\n") );
			return EGL_FALSE;
		}

		surface->caps |= SURFACE_CAP_WRITEBACK_CONVERSION;
		MALI_DEBUG_PRINT( 2, ("EGL: Writeback conversion enabled\n") );
	}

	/* the following conditions has to be met in order to have direct rendering:
	 * - config supports direct rendering
	 * - virtual display larger than physical display
	 * - surface bpp matches fbdev bpp, or Mali able to convert to the fbdev format
	 * - surface width&height less or equal to fbdev display
	 */
	if ( EGL_TRUE == __egl_platform_supports_direct_rendering( surface, display, egl ) )
	{			
		display->var_info.yoffset = 0;
		_mali_base_vsync_event_report(MALI_VSYNC_EVENT_BEGIN_WAIT);

		if( !egl_platform_backend_swap( display->framebuffer_device, &display->var_info ) )
		{
			_mali_base_vsync_event_report(MALI_VSYNC_EVENT_END_WAIT);
			MALI_DEBUG_PRINT(3, ("EGL: Fail to update display scanout Yoffset\n"));
			return EGL_FALSE;
		}
		_mali_base_vsync_event_report(MALI_VSYNC_EVENT_END_WAIT);

		for ( i=0; i<display->num_dr_buffers; ++i )
		{
			buf_direct[i] = __egl_platform_display_create_mali_surface_from_framebuffer(
					(i+1) % display->num_dr_buffers,
					display,
					surface,
					surface->width,
					surface->height,
					base_ctx );
		}
		num_buffers = display->num_dr_buffers;
		surface->swap_func = __egl_platform_swap_buffers_direct_rendering;
		surface->caps |= SURFACE_CAP_DIRECT_RENDERING;
	}
	else if ( EGL_TRUE == __egl_platform_supports_single_buffer_rendering( surface, display, egl ) )
	{
		buf_direct[0] = __egl_platform_display_create_mali_surface_from_framebuffer( 0, display, surface, surface->width, surface->height, base_ctx);

		/* make sure we are at zero offset if we have virtual resolution */
		display->var_info.yoffset = 0;
		_mali_base_vsync_event_report(MALI_VSYNC_EVENT_BEGIN_WAIT);
		if( !egl_platform_backend_swap( display->framebuffer_device, &display->var_info ) )
		{
			_mali_base_vsync_event_report(MALI_VSYNC_EVENT_END_WAIT);
			MALI_DEBUG_PRINT(3, ("EGL: Fail to update display scanout Yoffset\n"));
			return EGL_FALSE;
		}
		_mali_base_vsync_event_report(MALI_VSYNC_EVENT_END_WAIT);

		surface->swap_func = __egl_platform_swap_buffers_nop;
		surface->caps |= SURFACE_CAP_DIRECT_RENDERING;
		num_buffers = 1;
	}
	else
	{
		/* if direct rendering (double/single buffered) fails, fallback to buffer blit */
		surface->swap_func = __egl_platform_swap_buffers_buffer_blit;
		surface->caps = SURFACE_CAP_NONE;
		implicit_buffer = MALI_TRUE;
		num_buffers = 1;
	}

	/* override any previous setting if we are not supposed to blit */
	if ( MALI_TRUE == implicit_buffer || MALI_TRUE == egl->never_blit || surface->caps & SURFACE_CAP_WRITEBACK_CONVERSION )
	{
		mali_surface_specifier sformat;

		__egl_surface_to_surface_specifier( &sformat, surface );

		buf_implicit = _mali_surface_alloc( MALI_SURFACE_FLAGS_NONE, &sformat, 0, base_ctx );
		if ( NULL == buf_implicit )
		{
			for ( i=0; i<MAX_EGL_BUFFERS; ++i )
			{
				if ( NULL != buf_direct[i] ) _mali_surface_deref( buf_direct[i] );
			}
			return EGL_FALSE;
		}

		if ( MALI_TRUE == egl->never_blit ) surface->swap_func = __egl_platform_swap_buffers_nop;
		if ( MALI_FALSE == implicit_buffer) surface->internal_target = buf_implicit;
	}

	if ( (EGL_FALSE == implicit_buffer && NULL == buf_direct[ 0 ]) ||
	     ( EGL_FALSE == implicit_buffer && EGL_SINGLE_BUFFER != surface->render_buffer && NULL == buf_direct[ 1 ] ) )
	{
		if ( NULL != buf_implicit ) _mali_surface_deref( buf_implicit );
		for ( i=0; i<MAX_EGL_BUFFERS; ++i )
		{
			if ( NULL != buf_direct[i] ) _mali_surface_deref( buf_direct[i] );
		}
		return EGL_FALSE;
	}

	if ( MALI_TRUE == implicit_buffer )
	{
		surface->frame_builder = __egl_mali_create_frame_builder( base_ctx, surface->config, 2, 1, &buf_implicit, MALI_FALSE, MALI_FRAME_BUILDER_TYPE_EGL_WINDOW );
		if ( NULL == surface->frame_builder ) goto cleanup;
		surface->frame_builder->samples = (u32) surface->config->samples;

		MALI_DEBUG_ASSERT ( 1 == num_buffers, ("More than one implicit buffer?") );
		MALI_DEBUG_ASSERT ( NULL == surface->internal_target, ("The implicit buffer should not be used as internal target") );

		surface->buffer[0].render_target = buf_implicit;
		surface->buffer[0].surface = surface;
		surface->buffer[0].id = 0;
	}
	else
	{
		surface->frame_builder = __egl_mali_create_frame_builder( base_ctx, surface->config, num_buffers + 1, num_buffers, buf_direct, MALI_FALSE, MALI_FRAME_BUILDER_TYPE_EGL_WINDOW );
		if ( NULL == surface->frame_builder ) goto cleanup;

		for ( i=0; i<num_buffers; i++ )
		{
			surface->buffer[i].render_target = buf_direct[i];
			surface->buffer[i].surface = surface;
			surface->buffer[i].id = i;
		}
	}

	surface->num_buffers = num_buffers;

	/* If we have an internal target, make it the default output surface of the FB */
	if ( surface->internal_target )
	{
		u32 usage;
		_mali_frame_builder_get_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, &usage );
		_mali_frame_builder_set_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, surface->internal_target, usage );
	}

	return EGL_TRUE;

cleanup:
	if( NULL != surface->frame_builder ) __egl_mali_destroy_frame_builder( surface );

	for ( i=0; i<MAX_EGL_BUFFERS; ++i )
	{
		if ( NULL != buf_direct[i] ) _mali_surface_deref( buf_direct[i] );
	}
	if ( NULL != buf_implicit ) _mali_surface_deref( buf_implicit );

	surface->internal_target = NULL;
	surface->frame_builder   = NULL;

	return EGL_FALSE;
}

EGLBoolean __egl_platform_create_surface_pbuffer( egl_surface *surface, mali_base_ctx_handle base_ctx, _fbdev_display *display )
{
	mali_surface* buf = NULL;
	mali_surface_specifier sformat;

	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_DEBUG_ASSERT_POINTER( display );

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

	surface->frame_builder = __egl_mali_create_frame_builder( base_ctx, surface->config, 2, 1, &buf, MALI_TRUE, MALI_FRAME_BUILDER_TYPE_EGL_PBUFFER );

	if ( NULL == surface->frame_builder )
	{
		_mali_surface_deref( buf );
		return EGL_FALSE;
	}

	surface->buffer[0].render_target = buf;
	surface->buffer[0].surface = surface;
	surface->buffer[0].id = 0;

	return EGL_TRUE;
}

EGLBoolean __egl_platform_create_surface_pixmap( egl_surface* surface, mali_base_ctx_handle base_ctx, _fbdev_display* display )
{
	mali_surface* buf = NULL;
	mali_surface_specifier sformat;

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	fbdev_pixmap* fb_pixmap = NULL;
	ump_handle pixmap_ump_handle = UMP_INVALID_MEMORY_HANDLE;
	mali_mem_handle pixmap_mem_handle = MALI_NO_HANDLE;
	mali_shared_mem_ref* pixmap_mem_ref = NULL;
#endif /* MALI_USE_UNIFIED_MEMORY_PROVIDER != 0 */
	MALI_IGNORE( display );

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
}

EGLBoolean __egl_platform_create_surface( egl_surface *surface, mali_base_ctx_handle base_ctx )
{
	u32 id;
	_fbdev_display *display = NULL;
	EGLBoolean retval = EGL_FALSE;

	MALI_DEBUG_ASSERT_POINTER( surface );

	id = MALI_REINTERPRET_CAST(u32)surface->dpy->native_dpy;
	display = __egl_platform_display_find( id );
	if ( NULL == display ) return EGL_FALSE;

	surface->num_buffers = 1;
	surface->platform = NULL;

	switch ( surface->type )
	{
		case MALI_EGL_WINDOW_SURFACE:
			retval = __egl_platform_create_surface_window( surface, base_ctx, display );
			break;

		case MALI_EGL_PBUFFER_SURFACE:
			retval = __egl_platform_create_surface_pbuffer( surface, base_ctx, display );
			break;

		case MALI_EGL_PIXMAP_SURFACE:
			retval = __egl_platform_create_surface_pixmap( surface, base_ctx, display );
			break;

		default:
			break;
	}

	surface->copy_func = __egl_platform_copy_buffers_blit;

	return retval;
}

void __egl_platform_destroy_surface(egl_surface *surface)
{
#if EGL_KHR_lock_surface_ENABLE
	egl_surface_platform* platform;
#endif
	MALI_DEBUG_ASSERT_POINTER( surface );

	if( NULL != surface->frame_builder ) __egl_mali_destroy_frame_builder( surface );
	surface->frame_builder = NULL;

	if ( EGL_TRUE == surface->is_null_window )
	{
		fbdev_window *fb_win = NULL;
		fb_win = MALI_REINTERPRET_CAST(fbdev_window *)(surface->win);
		if ( NULL != fb_win ) _mali_sys_free( fb_win );
		surface->win = NULL;
	}

	if ( NULL != surface->platform)
	{
#if EGL_KHR_lock_surface_ENABLE
		platform = MALI_REINTERPRET_CAST(egl_surface_platform*)surface->platform;
		if ( NULL != platform->offscreen_buffer) _mali_sys_free(platform->offscreen_buffer);
#endif
		_mali_sys_free(surface->platform);
		surface->platform = NULL;
	}
}

EGLBoolean __egl_platform_resize_surface( egl_surface *surface, u32 *width, u32 *height, mali_base_ctx_handle base_ctx )
{
	u32 i;
	mali_surface   *color_target[MAX_EGL_BUFFERS];

	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_DEBUG_ASSERT_POINTER( width );
	MALI_DEBUG_ASSERT_POINTER( height );

	for ( i=0; i<MAX_EGL_BUFFERS; ++i )
	{
		color_target[i]     = NULL;
	}

	if ( surface->caps & SURFACE_CAP_DIRECT_RENDERING )
	{
		_fbdev_display *fb_display = NULL;

		fb_display = __egl_platform_display_find( MALI_REINTERPRET_CAST(u32)surface->dpy->native_dpy );
		if ( NULL == fb_display ) return EGL_FALSE;

		/* clamp width and height to visible display size */
		if ( *width > fb_display->var_info.xres ) *width = fb_display->var_info.xres;
		if ( *height > fb_display->var_info.yres ) *height = fb_display->var_info.yres;

		for ( i=0; i<surface->num_buffers; ++i )
		{
			color_target[i] = __egl_platform_display_create_mali_surface_from_framebuffer( i, fb_display, surface, *width, *height, base_ctx );
			if ( NULL == color_target[i] )
			{
				/* allocation failed, thus resizing must fail. Clean up what we have allocated so far */
				while ( i )
				{
					--i;
					_mali_surface_deref( color_target[i] );
				}
				return EGL_FALSE;
			}
		}

		if ( (surface->width > *width) || (surface->height > *height) )
		{
			for ( i=0; i<surface->num_buffers; i++ )
			{
				void *mapping = NULL;
				mapping = _mali_mem_ptr_map_area(fb_display->fb_mem_ref[i]->mali_memory, 0, fb_display->var_info.xres * fb_display->var_info.yres*(fb_display->var_info.bits_per_pixel>>3), 0, MALI_MEM_PTR_WRITABLE | MALI_MEM_PTR_NO_PRE_UPDATE);
				if ( NULL != mapping )
				{
					u32 y = 0;
					u32 fb_bpp = fb_display->var_info.bits_per_pixel>>3;
					u32 fb_xres = fb_display->var_info.xres;
					u32 constrained_height = MIN( fb_display->var_info.yres, surface->height );
					u32 constrained_width = MIN( fb_display->var_info.xres, surface->width );
					/* clear the areas that are left from the old surface */

					/* compensate for new width */
					if ( constrained_width > *width )
					{
						for ( y = 0; y<constrained_height; y++)
						{
							_mali_sys_memset( (void*)((u32)mapping + (*width + y*fb_xres)*fb_bpp),
							                  0,
							                  (constrained_width - *width) * fb_bpp );
						}
					}
					/* compensate for new height */
					if ( constrained_height > *height )
					{
						for ( y = *height; y<constrained_height; y++)
						{
							_mali_sys_memset( (void*)((u32)mapping + y*fb_xres*fb_bpp),
							                  0,
							                  *width*fb_bpp );
						}
					}
					_mali_mem_ptr_unmap_area( fb_display->fb_mem_ref[i]->mali_memory );
				}
				else
				{
					MALI_DEBUG_PRINT( 0, ("EGL : Unable to clear old surface (%i)\n", i) );
				}
			}
		}
	}
	else
	{
		/* Resizing non-fbdev output surface */
		mali_surface_specifier sformat;

		MALI_DEBUG_ASSERT( __egl_platform_swap_buffers_buffer_blit == surface->swap_func ||
		                   __egl_platform_swap_buffers_nop == surface->swap_func, ("Non-direct rendering expects buffer blit or nop swap function!"));

		__egl_surface_to_surface_specifier( &sformat, surface );
		for ( i=0; i<surface->num_buffers; ++i )
		{
			sformat.width = *width;
			sformat.height = *height;
			sformat.pitch = 0;
			color_target[i] = _mali_surface_alloc( MALI_SURFACE_FLAGS_NONE, &sformat, 0, base_ctx );
			if ( NULL == color_target[i] )
			{
				/* allocation failed, thus resizing must fail. Clean up what we have allocated so far */
				while ( i )
				{
					--i;
					_mali_surface_deref( color_target[i] );
				}
				return EGL_FALSE;
			}
		}
	}

	/* release old data and assign new */
	for ( i=0; i<surface->num_buffers; ++i )
	{
		_mali_surface_deref( surface->buffer[i].render_target );
		surface->buffer[i].render_target = color_target[i];
	}

	surface->current_buffer = 0;

	return EGL_TRUE;
}

EGLenum __egl_platform_get_pixmap_colorspace( EGLNativePixmapType pixmap )
{
	fbdev_pixmap *fb_pixmap = NULL;
	MALI_DEBUG_ASSERT_POINTER( pixmap );
	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap *)pixmap;

	return ( fb_pixmap->flags & FBDEV_PIXMAP_COLORSPACE_sRGB ) ? EGL_COLORSPACE_sRGB : EGL_COLORSPACE_LINEAR;
}

EGLenum __egl_platform_get_pixmap_alphaformat( EGLNativePixmapType pixmap )
{
	fbdev_pixmap *fb_pixmap = NULL;
	MALI_DEBUG_ASSERT_POINTER( pixmap );
	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap *)pixmap;

	return ( fb_pixmap->flags & FBDEV_PIXMAP_ALPHA_FORMAT_PRE ) ? EGL_ALPHA_FORMAT_PRE : EGL_ALPHA_FORMAT_NONPRE;
}

void __egl_platform_get_pixmap_size( EGLNativeDisplayType display, EGLNativePixmapType pixmap, u32 *width, u32 *height, u32 *pitch )
{
	fbdev_pixmap *fb_pixmap = NULL;
	MALI_DEBUG_ASSERT_POINTER( pixmap );
	MALI_IGNORE( display );

	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap *)pixmap;

	if (NULL != width)  *width  = (u32)fb_pixmap->width;
	if (NULL != height) *height = (u32)fb_pixmap->height;
	if (NULL != pitch)  *pitch  = (u32)fb_pixmap->bytes_per_pixel * fb_pixmap->width;
}

void __egl_platform_get_pixmap_format( EGLNativeDisplayType display, EGLNativePixmapType pixmap, mali_surface_specifier *sformat )
{
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
}

EGLBoolean __egl_platform_pixmap_valid( EGLNativePixmapType pixmap )
{
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
}

/* TODO: rename? ump + dma-buf */
EGLBoolean __egl_platform_pixmap_support_ump( EGLNativePixmapType pixmap )
{
	fbdev_pixmap *fb_pixmap = NULL;
	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap*)pixmap;

	MALI_DEBUG_ASSERT_POINTER( fb_pixmap );

	if ( (FBDEV_PIXMAP_DMA_BUF | FBDEV_PIXMAP_SUPPORTS_UMP) & fb_pixmap->flags ) return EGL_TRUE;

	return EGL_FALSE;
}

EGLBoolean __egl_platform_pixmap_config_compatible( EGLNativeDisplayType display, EGLNativePixmapType pixmap, egl_config *config, EGLBoolean renderable_usage )
{
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
}

EGLBoolean __egl_platform_pixmap_surface_compatible( EGLNativeDisplayType display, EGLNativePixmapType pixmap, egl_surface *surface, EGLBoolean alpha_check  )
{
	fbdev_pixmap *fb_pixmap = NULL;
	MALI_DEBUG_ASSERT_POINTER( pixmap );
	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_IGNORE( display );

    if ( EGL_TRUE == alpha_check ) return EGL_TRUE;

	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap *)pixmap;

	if ( fb_pixmap->width != surface->width ) return EGL_FALSE;
	if ( fb_pixmap->height != surface->height ) return EGL_FALSE;

	return EGL_TRUE;
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
}

#if MALI_EGL_IMAGE_SUPPORT_NATIVE_YUV
EGLBoolean __egl_platform_pixmap_is_yuv( EGLNativePixmapType pixmap )
{
	fbdev_pixmap *fb_pixmap = NULL;

	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap *)pixmap;
	MALI_DEBUG_ASSERT_POINTER( fb_pixmap );

	return mali_image_supported_yuv_format( fb_pixmap->format );
}

struct mali_image* __egl_platform_map_pixmap_yuv( EGLNativeDisplayType display, EGLNativePixmapType pixmap )
{
	u32 width, height;
	u32 miplevels = 1;
	u32 yuv_format;
	u32 ofs_u = 0, ofs_v = 0, ofs_uv = 0;
	m200_texel_format texel_format = M200_TEXEL_FORMAT_ARGB_8888;
	m200_texture_addressing_mode texel_layout = M200_TEXTURE_ADDRESSING_MODE_LINEAR;
	mali_image *image = NULL;
	mali_surface_specifier sformat;
	fbdev_pixmap *fb_pixmap = NULL;
	mali_base_ctx_handle base_ctx;
#if MALI_USE_DMA_BUF
	struct fbdev_dma_buf *mem_handle;
#elif MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	ump_handle mem_handle;
#else
	void *mem_handle;
#endif

	__egl_main_context *egl = __egl_get_main_context();

	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap *)pixmap;
	MALI_DEBUG_ASSERT_POINTER( fb_pixmap );

	__egl_platform_get_pixmap_format(display, pixmap, &sformat);

	width = fb_pixmap->width;
	height = fb_pixmap->height;
	yuv_format = fb_pixmap->format;
	base_ctx = egl->base_ctx;
	mem_handle = (void *)fb_pixmap->data;

	_mali_surface_specifier_ex( &sformat, width, height, 0, MALI_PIXEL_FORMAT_NONE, texel_format,
								MALI_PIXEL_LAYOUT_LINEAR, texel_layout,
								MALI_FALSE, MALI_FALSE,
								MALI_SURFACE_COLORSPACE_lRGB, MALI_SURFACE_ALPHAFORMAT_NONPRE,
								fb_pixmap->alpha_size == 0 ? MALI_TRUE : MALI_FALSE );

	image = mali_image_create( miplevels, MALI_SURFACE_FLAG_DONT_MOVE, &sformat, yuv_format, base_ctx );
	MALI_CHECK_NON_NULL( image, NULL );

	/* wrap the given native buffer into mali memory suitable for mali_image */
	switch ( yuv_format )
	{
		case EGL_YUV420P_KHR:
			/* planar format, separate buffers for Y,U,V */

			ofs_u = width * height;
			ofs_v = ofs_u + ( (width/2) * (height/2) );

			if ( __egl_platform_pixmap_support_ump( pixmap ) )
			{
				if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 0, 0, 0, mem_handle ) ) goto cleanup;
				if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 1, 0, ofs_u, mem_handle ) ) goto cleanup;
				if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 2, 0, ofs_v, mem_handle ) ) goto cleanup;
			}
			else
			{
				/* buffers will be created implicitly */
				MALI_DEBUG_PRINT( 0, ("Warning: EGL was given a native pixmap without UMP support! Implicitly allocating buffers\n") );
			}

			break;
		case EGL_YVU420SP_KHR:
		case EGL_YUV420SP_KHR:
			/* semi planar format, separate buffers for Y and same buffer for U,V */

			ofs_uv = width * height;

			if ( __egl_platform_pixmap_support_ump( pixmap ) )
			{
				if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 0, 0, 0, mem_handle ) ) goto cleanup;
				if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 3, 0, ofs_uv, mem_handle ) ) goto cleanup;
			}
			else
			{
				/* buffers will be created implicitly */
				MALI_DEBUG_PRINT( 0, ("Warning: EGL was given a native pixmap without UMP support! Implicitly allocating buffers\n") );
			}

			break;
		case EGL_YV12_KHR:
			/* planar format, separate buffers for Y,U,V */

			ofs_v = width * height;
			ofs_u = ofs_v + ( (width/2) * (height/2) );

			if ( __egl_platform_pixmap_support_ump( pixmap ) )
			{
				if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 0, 0, 0, mem_handle ) ) goto cleanup;
				if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 1, 0, ofs_v, mem_handle ) ) goto cleanup;
				if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 2, 0, ofs_u, mem_handle ) ) goto cleanup;
			}
			else
			{
				/* buffers will be created implicitly */
				MALI_DEBUG_PRINT( 0, ("Warning: EGL was given a native pixmap without UMP support! Implicitly allocating buffers\n") );
			}

			break;
		#ifdef NEXELL_ANDROID_FEATURE_EGL_YV12_KHR_444_FORMAT
		#ifdef NEXELL_ANDROID_FEATURE_EGL_TEMP_YUV444_FORMAT
		case EGL_YV12_KHR_444:
			/* planar format, separate buffers for Y,U,V */
		
			ofs_u = width * height;
			ofs_v = ofs_u + ( width * height );
		
			if ( __egl_platform_pixmap_support_ump( pixmap ) )
			{
				if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 0, 0, 0, mem_handle ) ) goto cleanup;
				if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 1, 0, ofs_v, mem_handle ) ) goto cleanup;
				if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 2, 0, ofs_u, mem_handle ) ) goto cleanup;
			}
			else
			{
				/* buffers will be created implicitly */
				MALI_DEBUG_PRINT( 0, ("Warning: EGL was given a native pixmap without UMP support! Implicitly allocating buffers\n") );
			}
		
			break;
		#else
		case EGL_YV12_KHR_444:
			/* planar format, separate buffers for Y,U,V */
		
			ofs_v = width * height;
			ofs_u = ofs_v + ( (width) * (height/2) );
		
			if ( __egl_platform_pixmap_support_ump( pixmap ) )
			{
				if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 0, 0, 0, mem_handle ) ) goto cleanup;
				if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 1, 0, ofs_v, mem_handle ) ) goto cleanup;
				if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 2, 0, ofs_u, mem_handle ) ) goto cleanup;
			}
			else
			{
				/* buffers will be created implicitly */
				MALI_DEBUG_PRINT( 0, ("Warning: EGL was given a native pixmap without UMP support! Implicitly allocating buffers\n") );
			}
		
			break;
		#endif	
		#endif
		default:
			MALI_DEBUG_PRINT( 0, ("Error: EGL was given an unknown YUV format in %s (0x%x)\n", __func__, yuv_format) );
			mali_image_deref( image );
			return NULL;

			break;
	}


	return image;

cleanup:
	MALI_DEBUG_PRINT( 0, ("Error: EGL failed to create compatible YUV image\n") );
	mali_image_deref( image );

	return NULL;
}
#endif

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
	                                                pixmap_support_ump == EGL_TRUE ? *(fb_pixmap->data) : NULL,
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
	MALI_IGNORE( egl_img );
#if MALI_EGL_IMAGE_SUPPORT_NATIVE_YUV
	if ( EGL_TRUE == __egl_platform_pixmap_is_yuv( pixmap ) ) return __egl_platform_map_pixmap_yuv( display, pixmap );
#endif

	return __egl_platform_map_pixmap_rgb( display, pixmap );
}

void __egl_platform_unmap_pixmap( EGLNativePixmapType pixmap, struct egl_image *egl_img )
{
	fbdev_pixmap *fb_pixmap = NULL;

	MALI_IGNORE( egl_img );

	fb_pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap*)pixmap;

	if ( fb_pixmap->flags & FBDEV_PIXMAP_EGL_MEMORY )
	{
		/* free this memory if owned by egl */
		_mali_sys_free( fb_pixmap );
	}
}


void __egl_platform_unmap_image_buffer( void *buffer )
{
#if EGL_KHR_image_system_ENABLE
	if( buffer )
	{
		/* buffer if non NULL can only be egl_image_system_buffer_properties so no need to protect it with flags */
		egl_image_system_buffer_properties *image_buffer  = (egl_image_system_buffer_properties *)buffer;
		if( image_buffer->ump )
		{
			ump_reference_release( image_buffer->ump );
		}
		/* free this memory */
		_mali_sys_free( buffer );
	}
#endif
}


MALI_STATIC void __egl_platform_swap_buffers_nop( mali_base_ctx_handle base_ctx,
                                             EGLNativeDisplayType native_dpy,
                                             egl_surface *surface,
                                             struct mali_surface *target,
                                             EGLint interval)
{
	MALI_IGNORE( base_ctx );
	MALI_IGNORE( native_dpy );
	MALI_IGNORE( surface );
	MALI_IGNORE( target );
	MALI_IGNORE( interval );
}

MALI_STATIC void __egl_platform_swap_buffers_direct_rendering( mali_base_ctx_handle base_ctx,
                                                          EGLNativeDisplayType native_dpy,
                                                          egl_surface *surface,
                                                          struct mali_surface *target,
                                                          EGLint interval)
{
	_fbdev_display *display = NULL;
	u32 type = MALI_REINTERPRET_CAST(u32)native_dpy;
	MALI_DEBUG_ASSERT( surface != NULL, ("No surface specified") );
	MALI_DEBUG_ASSERT_POINTER( fbdev->display );
	MALI_IGNORE( base_ctx );
	MALI_IGNORE( target );

	/* locate the display */
	display = __egl_platform_display_find( type );
	MALI_DEBUG_ASSERT_POINTER( display );

#if EGL_KHR_lock_surface_ENABLE
	if ((surface->config->surface_type & EGL_LOCK_SURFACE_BIT_KHR) != 0)
	{
		egl_surface_platform* platform = MALI_REINTERPRET_CAST(egl_surface_platform*)surface->platform;
		MALI_DEBUG_ASSERT_POINTER( platform );
		platform->visible_buffer = 1 - platform->visible_buffer;
		display->var_info.yoffset = display->var_info.yres * platform->visible_buffer;
		/* Force copy of frontbuffer to backbuffer if EGL_PRESERVE_PIXELS_KHR
		 * is passed to eglLockSurfaceKHR later */
		platform->previous_frame_copied = EGL_FALSE;
	}
	else
#endif
	{
		display->var_info.yoffset = (display->var_info.yoffset + display->var_info.yres) % (display->var_info.yres * surface->num_buffers );
	}

	if ( 0 != interval && vsync_supported )
	{
#ifdef EGL_SIMULATE_VSYNC
		__egl_platform_vsync_wait( interval );
#endif
	}

	_mali_base_vsync_event_report(MALI_VSYNC_EVENT_BEGIN_WAIT);

	if(!egl_platform_backend_swap( display->framebuffer_device, &display->var_info ))
	{
		_mali_base_vsync_event_report(MALI_VSYNC_EVENT_END_WAIT);
		MALI_DEBUG_PRINT(3, ("EGL: Fail to update display scanout Yoffset\n"));
		return;
	}

	_mali_base_vsync_event_report(MALI_VSYNC_EVENT_END_WAIT);

	/* the framebuilder takes care of swapping the render targets */
}

MALI_STATIC void __egl_platform_matching_mali_format( _fbdev_display *display, mali_pixel_format *format )
{
	u32 fbdev_bits_per_pixel = 0;

	MALI_DEBUG_ASSERT_POINTER( display );
	MALI_DEBUG_ASSERT_POINTER( format );

	fbdev_bits_per_pixel = display->var_info.bits_per_pixel;

	switch( fbdev_bits_per_pixel )
	{
		case 16:
			if ( ((display->var_info.red.offset == 11) && (display->var_info.green.offset == 5) && (display->var_info.blue.offset == 0 )) )
			{
				*format = MALI_PIXEL_FORMAT_R5G6B5;
			}
			else if ( (display->var_info.red.offset == 8) && (display->var_info.green.offset == 4) && (display->var_info.blue.offset == 0) )
			{
				*format = MALI_PIXEL_FORMAT_A4R4G4B4;
			}
			else if ( (display->var_info.red.offset == 10) && (display->var_info.green.offset == 5) && (display->var_info.blue.offset == 0) )
			{
				*format = MALI_PIXEL_FORMAT_A1R5G5B5;
			}
			break;

		case 32:
			if ( (display->var_info.red.offset == 16) && (display->var_info.green.offset == 8) && (display->var_info.blue.offset == 0) )
			{
				*format = MALI_PIXEL_FORMAT_A8R8G8B8;
			}
			break;
	}
}

#ifdef NEXELL_LEAPFROG_FEATURE_FBO_SCALE_EN
#include <gles1_draw.h>
#include <gles1_state/gles1_tex_state.h>
#include <gles_framebuffer_object.h>
#include <shared/mali_named_list.h>
#include <gles_share_lists.h>
#include <gles_common_state/gles_framebuffer_state.h>
#include "gles1_state/gles1_enable.h"

void __nx_egl_platform_swap_buffers_set_gles(egl_display *dpy, egl_surface *draw, gles_context *pgles_ctx)
{
	EGLNativeDisplayType native_dpy = dpy->native_dpy;
	mali_pixel_format mali_fbdev_format = MALI_PIXEL_FORMAT_NONE;
	u32 width, height;
	_fbdev_display *display = NULL;
	u32 i = 0, display_type = 0;
	GLenum err = 0;
	
	display_type = MALI_REINTERPRET_CAST(u32)native_dpy;
	display = __egl_platform_display_find( display_type );
	if ( NULL == display ) return;

	MALI_DEBUG_ASSERT( display->fb != NULL, ("framebuffer is NULL") );

	/* use the surface width if less than display width (same for height) */
	width = display->var_info.xres-display->x_ofs;
	height = display->var_info.yres-display->y_ofs;

	/*
	_gles1_enable(pgles_ctx, GL_TEXTURE_2D, GL_TRUE);
	_gles1_active_texture( pgles_ctx, GL_TEXTURE0 );
	*/
	err = _gles_bind_texture( pgles_ctx, GL_TEXTURE_2D, 0);
	if(GL_NO_ERROR != err)
	{
		MALI_DEBUG_ERROR(("_gles_bind_texture failed(0x%x)", err));
		return;
	}	
	err = _gles_bind_framebuffer( pgles_ctx, pgles_ctx->share_lists->framebuffer_object_list, &pgles_ctx->state.common.framebuffer, GL_FRAMEBUFFER, 0 );
	if(GL_NO_ERROR != err)
	{
		MALI_DEBUG_ERROR(("_gles_bind_framebuffer failed(0x%x)", err));
		return;
	}	
	err = _gles_framebuffer_texture2d(pgles_ctx,
			&pgles_ctx->state.common.framebuffer,
			pgles_ctx->share_lists->framebuffer_object_list,
			pgles_ctx->share_lists->renderbuffer_object_list,
			pgles_ctx->share_lists->texture_object_list,
			GL_FRAMEBUFFER,
			GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D,
			0,
			0);	

	if(GL_NO_ERROR != err)
	{
		MALI_DEBUG_ERROR(("_gles_bind_framebuffer failed(0x%x)", err));
		return;
	}	

	_gles_clear(pgles_ctx, GL_COLOR_BUFFER_BIT);
	_gles1_draw_tex_oes(pgles_ctx, 0, 0, 0, width, height);
}

void __nx_egl_platform_swap_buffers_reset_gles(egl_display *dpy, egl_surface *draw, gles_context *pgles_ctx)
{
	EGLNativeDisplayType native_dpy = dpy->native_dpy;
	mali_pixel_format mali_fbdev_format = MALI_PIXEL_FORMAT_NONE;
	u32 width, height;
	_fbdev_display *display = NULL;
	u32 i = 0, display_type = 0;
	GLenum err = 0;
	
	/* todo */	
	/*
	_gles1_active_texture( pgles_ctx, GL_TEXTURE0 );
	err = _gles_bind_texture( pgles_ctx, GL_TEXTURE_2D, 0);
	*/
	err = _gles_bind_framebuffer( pgles_ctx, pgles_ctx->share_lists->framebuffer_object_list, &pgles_ctx->state.common.framebuffer, GL_FRAMEBUFFER, 1 );
	if(GL_NO_ERROR != err)
	{
		MALI_DEBUG_ERROR(("_gles_bind_framebuffer failed(0x%x)", err));
		return;
	}		
}

#include <egl/egl_handle.h>
void _nx_egl_platform_set_swap_n_scale( EGLDisplay __dpy, EGLSurface __draw, void *thread_state )
{
	egl_display *dpy = NULL;
	egl_surface *draw = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate), EGL_FALSE );
	MALI_CHECK( EGL_NO_SURFACE != (draw = __egl_get_check_surface( __draw, __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_FALSE );

	__nx_egl_platform_swap_buffers_set_gles( dpy, draw, (gles_context*)tstate->context_gles );	
}

void __nx_egl_platform_reset_swap_n_scale( EGLDisplay __dpy, EGLSurface __draw, void *thread_state )
{
	egl_display *dpy = NULL;
	egl_surface *draw = NULL;
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;

	MALI_CHECK( EGL_NO_DISPLAY != (dpy = __egl_get_check_display( __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_initialized( dpy, tstate), EGL_FALSE );
	MALI_CHECK( EGL_NO_SURFACE != (draw = __egl_get_check_surface( __draw, __dpy, tstate ) ), EGL_FALSE );
	MALI_CHECK( EGL_TRUE == __egl_check_display_not_terminating( dpy, tstate ), EGL_FALSE );

	__nx_egl_platform_swap_buffers_reset_gles( dpy, draw, (gles_context*)tstate->context_gles );	
}
#endif

MALI_STATIC void __egl_platform_swap_buffers_buffer_blit( mali_base_ctx_handle base_ctx,
                                                          EGLNativeDisplayType native_dpy,
                                                          egl_surface *surface,
                                                          struct mali_surface *target,
                                                          EGLint interval )
{
	mali_pixel_format mali_fbdev_format = MALI_PIXEL_FORMAT_NONE;
	mali_mem_handle mali_framebuffer;
	u32 mali_bytes_per_pixel = 0;
	u32 mali_pitch;
	u32 fbdev_bytes_per_pixel = 0;
	u32 fbdev_pitch;
	u32 width, height;
	_fbdev_display *display = NULL;
	u32 i = 0, display_type = 0;
#if MALI_INSTRUMENTED
	u32 time_start;
#endif

	MALI_IGNORE( base_ctx );
	display_type = MALI_REINTERPRET_CAST(u32)native_dpy;
	MALI_DEBUG_ASSERT( surface != NULL, ("No surface specified"));

	display = __egl_platform_display_find( display_type );
	if ( NULL == display ) return;

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	if ( UMP_INVALID_MEMORY_HANDLE != display->ump_mem )
	{
		display->fb = (u16 *)ump_mapped_pointer_get( display->ump_mem );
	}
#endif

	MALI_DEBUG_ASSERT( display->fb != NULL, ("framebuffer is NULL") );
	MALI_DEBUG_ASSERT_POINTER( target );

	mali_framebuffer = target->mem_ref->mali_memory;

	MALI_DEBUG_ASSERT_HANDLE( mali_framebuffer );

	/* make sure we always start at offset zero */
	if ( 0 != display->var_info.yoffset )
	{
		display->var_info.yoffset = 0;
		 _mali_base_vsync_event_report(MALI_VSYNC_EVENT_BEGIN_WAIT);
		ioctl(display->framebuffer_device, FBIOPAN_DISPLAY, &display->var_info);
		_mali_base_vsync_event_report(MALI_VSYNC_EVENT_END_WAIT);
	}

	mali_bytes_per_pixel = __mali_pixel_format_get_bpp( target->format.pixel_format ) >> 3;
	fbdev_bytes_per_pixel = (s32)display->var_info.bits_per_pixel >> 3;

	/* use the surface width if less than display width (same for height) */
	width = surface->width < (display->var_info.xres-display->x_ofs) ? surface->width : display->var_info.xres-display->x_ofs;
	height = surface->height < (display->var_info.yres-display->y_ofs) ? surface->height : display->var_info.yres-display->y_ofs;

#if MALI_INSTRUMENTED
	time_start = _mali_sys_get_timestamp(); /* we must call _instrumented_add_to_counter() manually later in this function, see comments there */
#endif

	mali_pitch = target->format.pitch;
	fbdev_pitch = display->var_info.xres*fbdev_bytes_per_pixel;

	__egl_platform_matching_mali_format( display, &mali_fbdev_format );

	if ( 0 != interval && vsync_supported )
	{
#ifdef EGL_SIMULATE_VSYNC
		__egl_platform_vsync_wait( interval );
#endif
	}

	if ( mali_fbdev_format == surface->config->pixel_format )
	{
		if ( display->var_info.xres == surface->width ) /* direct fullscreen copy */
		{
			_mali_mem_read( (u16*)( (u32)display->fb + display->offset * (display->var_info.bits_per_pixel>>3) ),
				(void *)mali_framebuffer,
				0,
				width*height*mali_bytes_per_pixel);
		}
		else /* direct windowed copy */
		{
			u32 fbdev_offset = display->offset * (display->var_info.bits_per_pixel>>3);
			u32 mali_offset = 0;
			for ( i=0; i<height; i++ )
			{
				_mali_mem_read( (u16*)( (u32)display->fb + fbdev_offset ),
					(void *)mali_framebuffer,
					mali_offset,
					width*mali_bytes_per_pixel );
				fbdev_offset += fbdev_pitch;
				mali_offset  += mali_pitch;
			}
		}
	}
	else /* copy with conversion */
	{
		int dst_shift[4];
		int dst_size[4];
		int fbdev_offset = display->offset * (display->var_info.bits_per_pixel>>3);
		u16 *src;
		dst_size[0] = display->var_info.red.length;
		dst_size[1] = display->var_info.green.length;
		dst_size[2] = display->var_info.blue.length;
		dst_size[3] = 0;

		dst_shift[0] = display->var_info.red.offset;
		dst_shift[1] = display->var_info.green.offset;
		dst_shift[2] = display->var_info.blue.offset;
		dst_shift[3] = 0;

		/* map mali to a pointer */
		_mali_surface_access_lock( target );
		src = _mali_mem_ptr_map_area( mali_framebuffer, 0, target->datasize, 0, MALI_MEM_PTR_READABLE );
		MALI_DEBUG_ASSERT( src != NULL, ("Unable to map framebuffer"));

		/* create color map if we are dealing with one byte per pixel */
		if ( 1 == fbdev_bytes_per_pixel )
		{
			u16 cdata_grey[256];
			struct fb_cmap cmap;
			for ( i=0; i<256; i++ ) cdata_grey[i] = i*256;
			cmap.start = 0;
			cmap.len = 256;
			cmap.red = cdata_grey;
			cmap.green = cdata_grey;
			cmap.blue = cdata_grey;
			cmap.transp = NULL;
			ioctl( display->framebuffer_device, FBIOPUTCMAP, &cmap );
		}

		switch ( mali_bytes_per_pixel )
		{
			case 4: /* Mali 32bit */
				if ( 4 == fbdev_bytes_per_pixel )
				{
					_egl_convert_32bit_to_32bit( (u32*)((u32)display->fb + fbdev_offset), (u8*)src, width, height, fbdev_pitch, mali_pitch, surface->config->pixel_format, dst_shift, dst_size );
				}
				else if ( 2 == fbdev_bytes_per_pixel )
				{
					_egl_convert_32bit_to_16bit( (u16*)((u32)display->fb + fbdev_offset), (u8*)src, width, height, fbdev_pitch, mali_pitch, surface->config->pixel_format, dst_shift, dst_size );
				}
				else
				{
					MALI_DEBUG_ASSERT( EGL_FALSE, ("EGL: unsupported fbdev format!\n") );
				}
				break;

			case 2: /* Mali 16bit */
				if ( 4 == fbdev_bytes_per_pixel )
				{
					_egl_convert_16bit_to_32bit( (u32*)((u32)display->fb + fbdev_offset), (u16*)src, width, height, fbdev_pitch, mali_pitch, surface->config->pixel_format, dst_shift, dst_size );
				}
				else if ( 2 == fbdev_bytes_per_pixel )
				{
					_egl_convert_16bit_to_16bit( (u16*)((u32)display->fb + fbdev_offset), (u16*)src, width, height, fbdev_pitch, mali_pitch, surface->config->pixel_format, dst_shift, dst_size );
				}
				else
				{
					MALI_DEBUG_ASSERT( EGL_FALSE, ("EGL: unsupported fbdev format!\n") );
				}
				break;
		}

		_mali_mem_ptr_unmap_area( mali_framebuffer );
		_mali_surface_access_unlock( target );
	}

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	if ( UMP_INVALID_MEMORY_HANDLE != display->ump_mem )
	{
		ump_mapped_pointer_release( display->ump_mem );
	}
#endif

#if MALI_INSTRUMENTED
	{
		/*
		 * We cannot use _profiling_enter/leave(INST_EGL_BLIT_TIME), because we (might) be in the callback thread
		 * and that would access a possibly deleted profiling buffer through the TLS.
		 * We add the counter directly into the frame instead, but that performance hit shouldn't be to critical I guess.
		 * Atleast that is better than crashing.
		 * PS: This is (currently) the only counter which needs this special handling.
		 */

		u32 time_end = _mali_sys_get_timestamp();

		/* We are in callback thread, so we use the active frame set by the frame builder to register counter */
		mali_instrumented_frame *instrumented_frame = _instrumented_get_active_frame();
		MALI_DEBUG_ASSERT_POINTER( instrumented_frame );

		if (NULL != instrumented_frame)
		{
			u32 diff;

			if (time_end < time_start)
			{
				/* Timer has wrapped */
				diff = 0xFFFFFFFF - time_start + time_end + 1;
			}
			else
			{
				diff = time_end - time_start;
			}

			_instrumented_add_to_counter(INST_EGL_BLIT_TIME, instrumented_frame, diff * (_mali_sys_get_timestamp_scaling_factor() * 1000000.0));
		}
	}
#endif /* MALI_INSTRUMENTED */
}

void __egl_platform_swap_buffers( mali_base_ctx_handle base_ctx,
                                  EGLNativeDisplayType native_dpy,
                                  egl_surface *surface,
                                  struct mali_surface *target,
                                  EGLint interval)
{
	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_DEBUG_ASSERT_POINTER( surface->swap_func );
	
	surface->swap_func( base_ctx, native_dpy, surface, target, interval );
}

mali_mem_handle __egl_platform_pixmap_get_mali_memory( EGLNativeDisplayType display, EGLNativePixmapType native_pixmap, mali_base_ctx_handle base_ctx, egl_surface *surface )
{
#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0 || MALI_USE_DMA_BUF
	fbdev_pixmap *pixmap = NULL;
#endif

	MALI_IGNORE( display );
	MALI_IGNORE( surface );

#if MALI_USE_DMA_BUF
	pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap*)native_pixmap;
	return _mali_mem_wrap_dma_buf(base_ctx, ((struct fbdev_dma_buf*)pixmap->data)->fd, 0);
#elif MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap*)native_pixmap;
	return _mali_mem_wrap_ump_memory( base_ctx, (ump_handle)pixmap->data, 0 );
#else
	return MALI_NO_HANDLE;
#endif
}

MALI_STATIC void __egl_platform_copy_buffers_blit( mali_base_ctx_handle base_ctx,
                                              EGLNativeDisplayType native_dpy,
                                              egl_surface *surface,
                                              struct mali_surface *target,
                                              EGLNativePixmapType native_pixmap )
{
	fbdev_pixmap *pixmap = NULL;
	_fbdev_display *display = NULL;
	mali_mem_handle target_buffer = MALI_NO_HANDLE;
	u32 pixmap_pitch, pixmap_offset = 0;
	u32 target_pitch, target_offset = 0;
	u32 y = 0;
	u32 type = 0;
	void* dest_pixmap_data = NULL;

	__egl_main_context *egl = __egl_get_main_context();

	MALI_IGNORE( base_ctx );

	MALI_DEBUG_ASSERT( surface != NULL, ("No surface specified") );
	MALI_DEBUG_ASSERT( target != NULL, ("No target specified") );
	MALI_DEBUG_ASSERT( native_pixmap != NULL, ("No native pixmap specified") );
	MALI_DEBUG_ASSERT_POINTER( egl );

	type = MALI_REINTERPRET_CAST(u32)native_dpy;
	display = __egl_platform_display_find( type );
	if ( NULL == display ) return;

	pixmap = MALI_REINTERPRET_CAST(fbdev_pixmap *)native_pixmap;

	target_pitch = target->format.pitch;
	pixmap_pitch = pixmap->width*pixmap->bytes_per_pixel;

	target_buffer = target->mem_ref->mali_memory;

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	if ( FBDEV_PIXMAP_SUPPORTS_UMP & pixmap->flags )
	{
		dest_pixmap_data = ump_mapped_pointer_get( (ump_handle)pixmap->data );
	}
	else
#endif
	{
		dest_pixmap_data = (void*)pixmap->data;
	}

	if ( EGL_FALSE == egl->flip_pixmap )
	{
		/* fast-path for aligned surfaces */
		if ( target_pitch == pixmap_pitch )
		{			
			_mali_mem_read( dest_pixmap_data, target_buffer, 0, target_pitch*surface->height);
		}
		else
		{
			target_offset = target->mem_offset;
			for ( y=0; y<pixmap->height; y++, pixmap_offset+=pixmap_pitch, target_offset+=target_pitch )
			{
				_mali_mem_read( (void *)((u32)dest_pixmap_data + pixmap_offset),
					target_buffer,
					target_offset,
					pixmap_pitch );
			}
		}
	}
	else
	{
		target_offset = (surface->height - 1) * target_pitch;
		target_offset += target->mem_offset;
		for ( y=0; y<pixmap->height; y++, pixmap_offset+=pixmap_pitch, target_offset-=target_pitch )
		{
			_mali_mem_read( (void *)((u32)dest_pixmap_data + pixmap_offset),
			                target_buffer,
			                target_offset,
			                pixmap_pitch );
		}
	}

#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
	if ( FBDEV_PIXMAP_SUPPORTS_UMP & pixmap->flags )
	{
		ump_mapped_pointer_release( (ump_handle)pixmap->data );
	}
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

	/* default to blit if not set */
	if ( NULL == surface->copy_func) surface->copy_func = __egl_platform_copy_buffers_blit;

	surface->copy_func( base_ctx, native_dpy, surface, target, native_pixmap );
}

void __egl_platform_set_window_size( EGLNativeWindowType win, u32 width, u32 height )
{
	fbdev_window *fb_win = NULL;

	MALI_DEBUG_ASSERT_POINTER( win );

	fb_win = MALI_REINTERPRET_CAST(fbdev_window *)win;
	fb_win->width = width;
	fb_win->height = height;
}

EGLBoolean __egl_platform_get_window_size( EGLNativeWindowType win, void *platform, u32 *width, u32 *height )
{
	fbdev_window *fb_win = NULL;

	MALI_DEBUG_ASSERT_POINTER( win );
	MALI_IGNORE( platform );
	MALI_DEBUG_ASSERT_POINTER( width );
	MALI_DEBUG_ASSERT_POINTER( height );

	fb_win = MALI_REINTERPRET_CAST(fbdev_window*)win;

	if ( NULL != width ) *width  = (u16)(fb_win->width);
	if ( NULL != height ) *height = (u16)(fb_win->height);

	return EGL_TRUE;
}

EGLBoolean __egl_platform_window_valid( EGLNativeDisplayType display, EGLNativeWindowType win )
{
	fbdev_window *win_native = MALI_REINTERPRET_CAST(fbdev_window *)win;
	MALI_CHECK_NON_NULL( win_native, EGL_FALSE );
	MALI_IGNORE( display );

	if ( ( win_native->width == 0 ) || (  win_native->height == 0 ) ) return EGL_FALSE;
	if ( ( win_native->width > EGL_SURFACE_MAX_WIDTH ) || ( win_native->height > EGL_SURFACE_MAX_HEIGHT ) ) return EGL_FALSE;

	return EGL_TRUE;
}

EGLBoolean __egl_platform_window_compatible( EGLNativeDisplayType display, EGLNativeWindowType win, egl_config *config )
{
	MALI_IGNORE( display );
	MALI_IGNORE( win );
	MALI_IGNORE( config );
	return EGL_TRUE;
}

void __egl_platform_begin_new_frame( egl_surface *surface )
{
	MALI_IGNORE( surface );
}

EGLNativeWindowType __egl_platform_create_null_window( EGLNativeDisplayType dpy )
{
	fbdev_window *fb_win = NULL;
	_fbdev_display *fb_display = NULL;
	EGLNativeWindowType win;

	fb_display = __egl_platform_display_find( (u32)dpy );
	if ( NULL == fb_display ) return NULL;

	fb_win = _mali_sys_malloc( sizeof ( fbdev_window ) );
	if ( NULL == fb_win ) return NULL;

#ifndef FBDEV_FULLSCREEN
	fb_win->width = NULL_WINDOW_WIDTH;
	fb_win->height = NULL_WINDOW_HEIGHT;
#else
	fb_win->width = fb_display->var_info.xres;
	fb_win->height = fb_display->var_info.yres;
#endif
	win = (EGLNativeWindowType)fb_win;

	return win;
}

#ifdef EGL_SIMULATE_VSYNC
static void *__egl_platform_vsync_simulation_thread( void *params )
{
	__egl_platform_vsync_reset();
	while ( vsync_simulate )
	{
		_mali_sys_usleep( 1000000 / vsync_hertz );
		__egl_platform_vsync_callback();
	}

	return NULL;
}

EGLBoolean __egl_platform_vsync_simulation_start( u32 hertz )
{
	int rc;
	vsync_hertz = hertz;
	vsync_simulate = MALI_TRUE;

	vsync_mutex = _mali_sys_mutex_create();
	if ( NULL == vsync_mutex )
	{
		MALI_DEBUG_PRINT( 1, ("Failed to create vsync mutex\n") );
		return EGL_FALSE;
	}

	rc = pthread_create( &vsync_simulation_thread, NULL, __egl_platform_vsync_simulation_thread, NULL );
	if (0 != rc)
	{
		MALI_DEBUG_PRINT( 1, ("Failed to create vsync simulation thread\n") );
		_mali_sys_mutex_destroy( vsync_mutex );
		vsync_mutex = NULL;
		return EGL_FALSE;
	}
	return EGL_TRUE;
}

void __egl_platform_vsync_simulation_stop( void )
{
	vsync_simulate = MALI_FALSE;
	pthread_join( vsync_simulation_thread, NULL );

	if ( NULL != vsync_mutex )
	{
		_mali_sys_mutex_destroy( vsync_mutex );
		vsync_mutex = NULL;
	}
}

EGLBoolean __egl_platform_supports_vsync( void )
{
	return EGL_TRUE;
}

static void __egl_platform_vsync_callback( void )
{
	_mali_sys_mutex_lock( vsync_mutex );
	vsync_busy = MALI_TRUE;
	vsync_ticker++;
	vsync_busy = MALI_FALSE;
	_mali_sys_mutex_unlock( vsync_mutex );
}

static u32 __egl_platform_vsync_get( void )
{
	while(1)
	{
		if ( vsync_busy )
		{
			_mali_sys_yield();
		}
		else
		{
			return vsync_ticker;
		}
	}
}

static void __egl_platform_vsync_reset( void )
{
	_mali_sys_mutex_lock( vsync_mutex );
	vsync_busy = MALI_TRUE;
	vsync_ticker = 0;
	vsync_busy = MALI_FALSE;
	_mali_sys_mutex_unlock( vsync_mutex );
}

static void __egl_platform_vsync_wait( u32 interval )
{
	u32 vsync_recorded = __egl_platform_vsync_get();

	while(1)
	{
		if ( vsync_busy )
		{
			_mali_sys_yield();
		}
		else
		{
			/* note: vsync_ticker is updated in the vsync simulation thread */
			if ( (vsync_ticker - vsync_recorded) >= interval ) break;
		}
	}
}
#else
EGLBoolean __egl_platform_supports_vsync( void )
{
	return EGL_FALSE;
}
#endif

void __egl_platform_set_swap_region( egl_surface *surface, EGLint numrects, const EGLint* rects )
{
	MALI_IGNORE( surface );
	MALI_IGNORE( numrects );
	MALI_IGNORE( rects );
}

unsigned int __egl_platform_get_buffer(egl_surface* surface)
{
    unsigned int current_buffer;

#if EGL_KHR_reusable_sync_ENABLE
	__egl_sync_mutex_lock();
#endif
	current_buffer = (surface->current_buffer + 1);
	if ( current_buffer >= surface->num_buffers)
	{
		current_buffer = 0;
	}

#if EGL_KHR_reusable_sync_ENABLE
	__egl_sync_mutex_unlock();
#endif
    return current_buffer;
}

void __egl_platform_update_image(struct mali_surface * surface, void* data)
{
	MALI_IGNORE( surface );
	MALI_IGNORE( data );
}

#if EGL_SURFACE_LOCKING_ENABLED
void __egl_platform_register_lock_item( egl_surface_lock_item *lock_item )
{
	_lock_item_s item;

	item.secure_id = lock_item->secure_id;
	item.usage = lock_item->usage;

	if ( fd_umplock >= 0 )
	{
		ioctl( fd_umplock, LOCK_IOCTL_CREATE, &item );
	}
}

void __egl_platform_unregister_lock_item( egl_surface_lock_item *lock_item )
{
	_lock_item_s item;

	item.secure_id = lock_item->secure_id;
	item.usage = lock_item->usage;

	/*ioctl( fd_umplock, LOCK_IOCTL_DESTROY, &item );*/
}

void __egl_platform_process_lock_item( egl_surface_lock_item *lock_item )
{
	_lock_item_s item;

	item.secure_id = lock_item->secure_id;
	item.usage = lock_item->usage;

	if ( fd_umplock >= 0 )
	{
#if MALI_TIMELINE_PROFILING_ENABLED
		_mali_base_profiling_add_event( VR_PROFILING_EVENT_TYPE_SINGLE | VR_PROFILING_EVENT_CHANNEL_SOFTWARE | VR_PROFILING_EVENT_REASON_SINGLE_SW_UMP_TRY_LOCK, 0, 0, item.secure_id, item.usage, 0);
#endif
		if ( ioctl( fd_umplock, LOCK_IOCTL_PROCESS, &item ) < 0 )
		{
			int max_retries = 5;
			MALI_DEBUG_PRINT(1, ("[%i] PROCESS lock item 0x%x failed to acquire lock, throttling\n", _mali_sys_get_pid(), item.secure_id) );
			while ( ioctl( fd_umplock, LOCK_IOCTL_PROCESS, &item ) && max_retries )
			{
				_mali_sys_usleep(2000);
				max_retries--;
			}
		}
#if MALI_TIMELINE_PROFILING_ENABLED
		_mali_base_profiling_add_event( VR_PROFILING_EVENT_TYPE_SINGLE | VR_PROFILING_EVENT_CHANNEL_SOFTWARE | VR_PROFILING_EVENT_REASON_SINGLE_SW_UMP_LOCK, 0, 0, item.secure_id, item.usage, 0);
#endif
		MALI_DEBUG_PRINT(3, ("[%i]  PROCESS %i (%i)\n", _mali_sys_get_pid(), item.secure_id, item.usage) );
	}
}

void __egl_platform_release_lock_item( egl_surface_lock_item *lock_item )
{
	_lock_item_s item;

	item.secure_id = lock_item->secure_id;
	item.usage = lock_item->usage;

	if ( fd_umplock >= 0 )
	{

		ioctl( fd_umplock, LOCK_IOCTL_RELEASE, &item );
#if MALI_TIMELINE_PROFILING_ENABLED
		_mali_base_profiling_add_event( VR_PROFILING_EVENT_TYPE_SINGLE | VR_PROFILING_EVENT_CHANNEL_SOFTWARE | VR_PROFILING_EVENT_REASON_SINGLE_SW_UMP_UNLOCK, 0, 0, item.secure_id, item.usage, 0);
#endif
		MALI_DEBUG_PRINT(3, ("[%i]   RELEASE %i (%i)\n", _mali_sys_get_pid(), item.secure_id, item.usage) );
	}
}
#endif /* EGL_SURFACE_LOCKING_ENABLED */
