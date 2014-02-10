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
 * @file egl_platform_x11.c
 * @brief X11 platform backend
 */

#include <mali_system.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xfixes.h>
#include <EGL/egl.h>

#include <X11/Xlibint.h>
#include <X11/extensions/dri2proto.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>
#include "base/mali_profiling.h"
#include "xf86drm.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>

#include "x11/libdri2/libdri2.h"
#include "egl_platform.h"
#include "util/mali_math.h"
#include <shared/mali_surface.h>
#include <shared/mali_shared_mem_ref.h>
#include <shared/mali_image.h>
#include <shared/m200_gp_frame_builder_inlines.h>
#include "egl_mali.h"
#if EGL_KHR_lock_surface_ENABLE
#include "egl_lock_surface.h"
#endif
#include "egl_handle.h"

#include "egl_common_memory.h"

#include "../devicedrv/umplock/umplock_ioctl.h"

#define DEBUG_DUMP_ERROR() MALI_DEBUG_PRINT( 0, ("[EGL-X11] error in %s    %s:%i\n", __FILE__, __func__, __LINE__) );

#define X_ERROR_FLUSH 1
#define X_ERROR_SYNC  2

#define NUM_BUFFERS   2
#define MAX_IMAGE_BUFFERS 5

#define X11_ENABLE_DEBUG 1
#define X11_DEBUG_LEVEL 2

#if defined(X11_ENABLE_DEBUG)
#define XDBG(level, fmt, args...) if ( level <= X11_DEBUG_LEVEL ) _mali_sys_printf("[EGL-X11] "fmt,args )
#else
#define XDBG(level, fmt, args...)
#endif

/* TODO, get drm_fd from native_display? */
static int drm_fd = -1;

typedef struct native_display_data
{
	int screen;
	int screen_width;
	int screen_height;
	int pixmap_format_count;
	int pixmap_index;
	int depth;
	Display *display;
	Window   root_window;
	int fd;

	XPixmapFormatValues *pixmap_formats;
	int num_pixmap_formats;

	XVisualInfo *visual_formats;
	int num_visual_formats;
	int references;
} native_display_data;

typedef struct native_platform_data
{
	mali_named_list *displays;
	Display *default_display;
	int fd_umplock;
} native_platform_data;

typedef struct native_surface_data
{
	Window window;
	Display *display;
	int pitch;
	char *data;
	char *surface_buffer[2];
	int current_surface_buffer;

	DRI2Buffer *dri2_buffers;
	egl_memory_handle egl_mem_handle[2];
	int dri2_width;
	int dri2_height;
	int dri2_num_buffers;

	/* cache window region */
	XRectangle rect;
	XserverRegion region;
} native_surface_data;

typedef struct
{
	unsigned long flags;
	unsigned long functions;
	unsigned long decorations;
	long inputMode;
	unsigned long status;
} Hints;

static native_platform_data *native_data = NULL;
static XErrorHandler x_error_handler_old = (XErrorHandler)0;
static EGLBoolean x_error_set;

static int x_error_handler( Display *display, XErrorEvent *event )
{
	char error_text[256];

	XGetErrorText( display, event->error_code, error_text, 256 );
	x_error_set = EGL_TRUE;

	return 0;
}

void x_init_error_handler()
{
	x_error_set = EGL_FALSE;
	x_error_handler_old = XSetErrorHandler( x_error_handler );
}

void x_deinit_error_handler( Display *display, int flags )
{
	if ( flags & X_ERROR_FLUSH ) XFlush( display );
	if ( flags & X_ERROR_SYNC ) XSync( display, False );

	(void)XSetErrorHandler( x_error_handler_old );
}

MALI_STATIC void __egl_platform_display_get_environment_variables()
{
}

EGLBoolean __egl_platform_initialize( void )
{
	int event_base = 0, error_base = 0;
	if ( NULL == native_data )
	{
		native_data = _mali_sys_calloc( 1, sizeof( native_platform_data ) );
		if ( NULL == native_data ) goto platform_init_failure;

		native_data->displays = __mali_named_list_allocate();
		if ( NULL == native_data->displays ) goto platform_init_failure;

		/* set up the default display */
		XInitThreads();

		native_data->default_display = XOpenDisplay( NULL );
		if ( NULL == native_data->default_display ) goto platform_init_failure;

		/* Check for DRI2 support */
		if ( !DRI2QueryExtension( native_data->default_display, &event_base, &error_base ) )
		{
			DEBUG_DUMP_ERROR();
			goto platform_init_failure;
		}

#if EGL_SURFACE_LOCKING_ENABLED
		/* open umplock device */
		native_data->fd_umplock = open("/dev/umplock", O_RDWR);
		if ( native_data->fd_umplock < 0 )
		{
			MALI_DEBUG_PRINT( 0, ("WARNING: unable to open /dev/umplock\n") );
			native_data->fd_umplock = 0;
		}
#endif
	}

	return EGL_TRUE;

platform_init_failure:
	DEBUG_DUMP_ERROR();
	__egl_platform_terminate();

	return EGL_FALSE;
}

void __egl_platform_terminate( void )
{
	if ( NULL != native_data )
	{
		MALI_DEBUG_ASSERT( __mali_named_list_size( native_data->displays ) == 0, ("Error in platform terminate - not all displays released") );
		if ( NULL != native_data->displays ) __mali_named_list_free( native_data->displays, NULL );
		if ( NULL != native_data->default_display ) XCloseDisplay( native_data->default_display );
#if EGL_SURFACE_LOCKING_ENABLED
		if ( native_data->fd_umplock ) close( native_data->fd_umplock );
#endif
		_mali_sys_free( native_data );
		native_data = NULL;
	}
}

void __egl_platform_power_event( void *event_data )
{
	MALI_IGNORE( event_data );
}

EGLNativeDisplayType __egl_platform_default_display( void )
{
	if ( EGL_FALSE == __egl_platform_initialize() ) return NULL;

	return MALI_REINTERPRET_CAST(EGLNativeDisplayType)native_data->default_display;
}

EGLBoolean __egl_platform_display_valid( EGLNativeDisplayType dpy )
{
	if ( NULL == dpy ) return EGL_FALSE;
	if ( (int)dpy <= 0 ) return EGL_FALSE;

	return EGL_TRUE;
}

void __egl_platform_set_display_orientation( EGLNativeDisplayType dpy, EGLint orientation )
{
	MALI_IGNORE( dpy );
	MALI_IGNORE( orientation );
}

EGLint __egl_platform_get_display_orientation( EGLNativeDisplayType dpy )
{
	MALI_IGNORE( dpy );
	return 0;
}

#if 0
static void egl_drm_print_version( int fd )
{
	drmVersionPtr version;

	version = drmGetVersion( fd );
	if ( version )
	{
		MALI_DEBUG_PRINT( 1, ("  Version information:\n") );
		MALI_DEBUG_PRINT( 1, ("    Name: %s\n", version->name ? version->name : "?") );
		MALI_DEBUG_PRINT( 1, ("    Version: %d.%d.%d\n",version->version_major,version->version_minor,version->version_patchlevel) );
		MALI_DEBUG_PRINT( 1, ("    Date: %s\n", version->date ? version->date : "?") );
		MALI_DEBUG_PRINT( 1, ("    Desc: %s\n", version->desc ? version->desc : "?") );
		drmFreeVersion( version );
	}
	else
	{
		MALI_DEBUG_PRINT( 1, ("  No version information available\n") );
	}
}
#endif

EGLBoolean __egl_platform_init_display( EGLNativeDisplayType dpy, mali_base_ctx_handle base_ctx )
{
	native_display_data *native_display;

	MALI_IGNORE( base_ctx );

	/* Look up to see if there is already a mapping to this native display */
	native_display = __mali_named_list_get( native_data->displays, (u32)dpy );
	if ( NULL == native_display )
	{
		drm_magic_t magic;
		mali_err_code retval;
		char *driver_name, *device_name;

		/* create new entry for this new display */
		native_display = (native_display_data *)_mali_sys_calloc( 1, sizeof( native_display_data ) );
		if ( NULL == native_display )
		{
			DEBUG_DUMP_ERROR();
			return EGL_FALSE;
		}
		native_display->references = 0;
		x_init_error_handler();

		/* Cache some native properties */
		native_display->display = dpy;
		native_display->screen = DefaultScreen( native_display->display );
		native_display->depth = DefaultDepth( native_display->display, native_display->screen );
		native_display->screen_width = DisplayWidth( native_display->display, native_display->screen );
		native_display->screen_height = DisplayHeight( native_display->display, native_display->screen );
		native_display->root_window = DefaultRootWindow( native_display->display );

		if ( !DRI2Connect( native_display->display, native_display->root_window, &driver_name, &device_name ) )
		{
			DEBUG_DUMP_ERROR();
			_mali_sys_free( native_display );
			x_deinit_error_handler( native_display->display, X_ERROR_FLUSH | X_ERROR_SYNC );
			return EGL_FALSE;
		}

		native_display->fd = drm_fd = open( device_name, O_RDWR );
		if ( native_display->fd < 0 )
		{
			DEBUG_DUMP_ERROR();
			_mali_sys_free( native_display );
			x_deinit_error_handler( native_display->display, X_ERROR_FLUSH | X_ERROR_SYNC );
			return EGL_FALSE;
		}

		if ( drmGetMagic( native_display->fd, &magic ) )
		{
			DEBUG_DUMP_ERROR();
			close( native_display->fd );
			_mali_sys_free( native_display );
			x_deinit_error_handler( native_display->display, X_ERROR_FLUSH | X_ERROR_SYNC );
			return EGL_FALSE;
		}

		if ( !DRI2Authenticate( native_display->display, native_display->root_window, magic ) )
		{
			DEBUG_DUMP_ERROR();
			close( native_display->fd );
			_mali_sys_free( native_display );
			x_deinit_error_handler( native_display->display, X_ERROR_FLUSH | X_ERROR_SYNC );
			return EGL_FALSE;
		}

		/*egl_drm_print_version( native_display->fd );*/

		/* get supported pixmap formats */
		native_display->pixmap_formats = XListPixmapFormats( native_display->display, &native_display->num_pixmap_formats );

		/* get supported visuals */
		native_display->visual_formats = XGetVisualInfo( native_display->display, 0, NULL, &native_display->num_visual_formats );

		__egl_platform_display_get_environment_variables();

		x_deinit_error_handler( native_display->display, X_ERROR_FLUSH | X_ERROR_SYNC );

		if ( EGL_TRUE == x_error_set )
		{
			close( native_display->fd );
			_mali_sys_free( native_display );
			return EGL_FALSE;
		}

		/* insert the new mapping for this native display */
		retval = __mali_named_list_insert( native_data->displays, (u32)dpy, native_display );
		if ( MALI_ERR_NO_ERROR != retval )
		{
			DEBUG_DUMP_ERROR();
			close( native_display->fd );
			_mali_sys_free( native_display );
			return EGL_FALSE;
		}
	}

	native_display->references++;

	return EGL_TRUE;
}

void __egl_platform_filter_configs( egl_display *dpy )
{
	egl_config *cfg = NULL;
	u32 iterator = 0;
	int i;
	native_display_data *native_display = NULL;

	native_display = __mali_named_list_get( native_data->displays, (u32)dpy->native_dpy );

	/* update the visual id's for the configs */
	cfg = (egl_config *)__mali_named_list_iterate_begin( dpy->config, &iterator );
	while ( NULL != cfg )
	{
		unsigned int red_size, green_size, blue_size, alpha_size;
		EGLBoolean config_found = EGL_FALSE;
		XWindowAttributes window_attributes;

		_mali_pixel_format_get_bpc( cfg->pixel_format, &red_size, &green_size, &blue_size, &alpha_size, NULL, NULL );

		/* remove window bit from configs which do not have a visual id */
		for ( i=0; i<native_display->num_visual_formats; i++ )
		{
			int clz[3];
			int col_size[3];
			clz[0] = _mali_clz( native_display->visual_formats[i].red_mask );
			clz[1] = _mali_clz( native_display->visual_formats[i].green_mask );
			clz[2] = _mali_clz( native_display->visual_formats[i].blue_mask );

			col_size[0] = clz[1] - clz[0];
			col_size[1] = clz[2] - clz[1];
			col_size[2] = 32 - clz[2];
			if ( (cfg->red_size == col_size[0]) && (cfg->green_size == col_size[1]) && (cfg->blue_size == col_size[2]) )
			{
				/* add visual ID to config if it has the window bit set */
				config_found = EGL_TRUE;
				if ( (cfg->surface_type & EGL_WINDOW_BIT) != 0 )
				{
					cfg->native_visual_id = native_display->visual_formats[i].visualid;
				}
			}
		}

		if ( EGL_FALSE == config_found )
		{
			if ( cfg->surface_type & EGL_WINDOW_BIT )
			{
				cfg->surface_type = ~EGL_WINDOW_BIT;
				/*MALI_DEBUG_PRINT( 0, ("Removed EGL_WINDOW_BIT from config 0x%i - it has no matching visual (%i %i %i %i)\n", cfg->config_id, cfg->red_size, cfg->green_size, cfg->blue_size, cfg->alpha_size) );*/
			}
		}

		/* remove pixmap bit from configs which is not compatible with a native pixmap format */
		config_found = EGL_FALSE;
		for ( i=0; i<native_display->num_pixmap_formats; i++ )
		{
			int pixmap_depth = native_display->pixmap_formats[i].depth;
			if ( pixmap_depth == cfg->buffer_size ) config_found = EGL_TRUE;
		}

		if ( EGL_FALSE == config_found )
		{
			if ( cfg->surface_type & EGL_PIXMAP_BIT )
			{
				cfg->surface_type = ~EGL_PIXMAP_BIT;
				/*MALI_DEBUG_PRINT( 0, ("Removed EGL_PIXMAP_BIT from config 0x%i - it has no matching native pixmap format (%i %i %i %i)\n", cfg->config_id, cfg->red_size, cfg->green_size, cfg->blue_size, cfg->alpha_size) );*/
			}
		}

		/* add slow caveat if the config does not match the display bit depth */

		XGetWindowAttributes( native_display->display, DefaultRootWindow( native_display->display ), &window_attributes );
		if ( (window_attributes.depth != cfg->buffer_size) )
		{
			/* allow 32bit config usage on a 24bit window */
			if ( (window_attributes.depth != 24) || (cfg->buffer_size != 32) )
			{
				cfg->config_caveat |= EGL_SLOW_CONFIG;
				/*MALI_DEBUG_PRINT( 0, ("Marked config %i as slow (%i%i%i%i)\n", cfg->config_id, cfg->red_size, cfg->green_size, cfg->blue_size, cfg->alpha_size) );*/
			}
		}

		cfg = (egl_config *)__mali_named_list_iterate_next( dpy->config, &iterator );
	}
}

void __egl_platform_flush_display( EGLNativeDisplayType dpy )
{
	XFlush( dpy );
	XSync( dpy, False );
}

void __egl_platform_deinit_display( EGLNativeDisplayType dpy )
{
	native_display_data *native_display = NULL;

	if ( NULL == native_data ) return;

	native_display = __mali_named_list_get( native_data->displays, (u32)dpy );
	if ( NULL == native_display ) return;

	native_display->references--;
	if ( 0 == native_display->references )
	{
		close( native_display->fd );
		_mali_sys_free( native_display );
		__mali_named_list_remove( native_data->displays, (u32)dpy );
	}
}

void __egl_platform_display_get_format( EGLNativeDisplayType dpy, egl_display_native_format *format )
{
	MALI_IGNORE( dpy );
	MALI_IGNORE( format );
}

EGLBoolean __egl_platform_wait_native( EGLint engine )
{
	u32 iterator;
	native_display_data *native_display = NULL;

	MALI_IGNORE( engine);

	/* go through the list of displays making sure X11 has finished */
	native_display = __mali_named_list_iterate_begin( native_data->displays, &iterator );
	while( NULL != native_display )
	{
		XFlush( native_display->display );
		XSync( native_display->display, False );
		native_display = __mali_named_list_iterate_next( native_data->displays, &iterator );
	}

	/* also flush the default display */
	XFlush( native_data->default_display );
	XSync( native_data->default_display, False );

	return EGL_TRUE;
}

EGLBoolean __egl_platform_create_surface_pbuffer( egl_surface *surface, mali_base_ctx_handle base_ctx )
{
	mali_surface* buf = NULL;
	mali_surface_specifier sformat;
	egl_buffer *callback_data = NULL;

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

mali_surface* __egl_platform_map_dri2_buffer( mali_surface_specifier *sformat, egl_buffer_name egl_buf_name, mali_base_ctx_handle base_ctx, egl_memory_handle *egl_mem_handle, int offset )
{
	mali_mem_handle mali_mem = MALI_NO_HANDLE;
	mali_shared_mem_ref *mali_mem_ref = NULL;
	mali_surface *mali_surf = NULL;

	*egl_mem_handle = _egl_memory_create_handle_from_name(drm_fd, egl_buf_name);
	if ( EGL_INVALID_MEMORY_HANDLE == *egl_mem_handle )
	{
		DEBUG_DUMP_ERROR();
		return NULL;
	}

	mali_mem = _egl_memory_create_mali_memory_from_handle(base_ctx, *egl_mem_handle, offset);
	if ( MALI_NO_HANDLE == mali_mem )
	{
		_egl_memory_release_reference( *egl_mem_handle );
		DEBUG_DUMP_ERROR();

		return NULL;
	}

	mali_mem_ref = _mali_shared_mem_ref_alloc_existing_mem( mali_mem );
	if ( NULL == mali_mem_ref )
	{
		_mali_mem_free( mali_mem );
		_egl_memory_release_reference( *egl_mem_handle );
		DEBUG_DUMP_ERROR();

		return NULL;
	}

	mali_surf = _mali_surface_alloc_ref(  MALI_SURFACE_FLAG_DONT_MOVE, sformat, mali_mem_ref, 0, base_ctx );
	if ( NULL == mali_surf )
	{
		_mali_mem_free( mali_mem );
		_egl_memory_release_reference( *egl_mem_handle );
		_mali_shared_mem_ref_owner_deref( mali_mem_ref );
		DEBUG_DUMP_ERROR();

		return NULL;
	}
	XDBG( 3, "[%i] DRI2 BUFFER MAP SECURE ID 0x%x OFFSET %i\n", _mali_sys_get_pid(), _egl_memory_get_name_from_handle(drm_fd, *egl_mem_handle), offset );

	return mali_surf;
}

void __egl_platform_acquire_buffer( egl_surface *surface )
{

	EGLBoolean reset_frame = EGL_FALSE;
	__egl_main_context *egl = __egl_get_main_context();
	mali_base_ctx_handle base_ctx = egl->base_ctx;

	egl_buffer *callback_data = NULL;
	uint32_t attachments[1] = { DRI2BufferBackLeft };
	native_surface_data *native_surface = NULL;
	native_display_data *native_display = NULL;
	mali_surface *buf =  NULL;
	mali_surface_specifier sformat;
	Drawable drawable = MALI_REINTERPRET_CAST(Drawable)NULL;
	u32 i, slot_index, usage, offset = 0;
	u32 native_width = 0, native_height = 0;

	native_display = __mali_named_list_get( native_data->displays, (u32)surface->dpy->native_dpy );
	MALI_DEBUG_ASSERT_POINTER( native_display );

	/* reset the callback so that we don't call it multiple times for the same frame */
	_mali_frame_builder_set_acquire_output_callback( surface->frame_builder, MALI_REINTERPRET_CAST(mali_frame_cb_func)NULL, MALI_REINTERPRET_CAST(mali_frame_cb_param)NULL );

	/* Update Framebuilder output because now we get new Back */
	switch ( surface->type )
	{
		case MALI_EGL_WINDOW_SURFACE:
			drawable = MALI_REINTERPRET_CAST(Drawable)surface->win;
			attachments[0] = DRI2BufferBackLeft;
			__egl_platform_get_window_size( surface->win, surface->platform, &native_width, &native_height );
			break;
		case MALI_EGL_PIXMAP_SURFACE:
			drawable = MALI_REINTERPRET_CAST(Drawable)surface->pixmap;
			attachments[0] = DRI2BufferFrontLeft;
			__egl_platform_get_pixmap_size( native_display->display, surface->pixmap, &native_width, &native_height, NULL );
			break;
		default:
			break;
	}
	native_surface = surface->platform;

	native_surface->dri2_width = surface->width;
	native_surface->dri2_height = surface->height;
	native_surface->display = native_display->display;

	/* check if we set all the render_target */
	for ( slot_index = 0; (u32)slot_index < surface->num_buffers; slot_index++ ) if ( surface->buffer[slot_index].render_target == NULL ) break;

	if ( surface->width != native_width || surface->height != native_height )
	{
		XDBG( 3, "[%d] surface has been resized, clean up all the render target\n", _mali_sys_get_pid() );
		/* surface has been resized, clean up all the render targets */
		for ( i = 0; i < surface->num_buffers; i++ )
		{
			if ( NULL != surface->buffer[i].render_target )
			{
				egl_memory_handle egl_mem_handle_old = _egl_memory_get_handle_from_mali_memory( surface->buffer[i].render_target->mem_ref->mali_memory );
				if ( EGL_INVALID_MEMORY_HANDLE != egl_mem_handle_old) _egl_memory_release_reference( egl_mem_handle_old );
				_mali_surface_deref( surface->buffer[i].render_target );

				surface->buffer[i].render_target = NULL;
			}
		}

		slot_index = 0;
		reset_frame = EGL_TRUE;
	}

	/* found empty slot */
	if ( (u32)slot_index < surface->num_buffers )
	{
		_egl_surface_wait_for_jobs( surface );
		x_init_error_handler();

		DRI2CreateDrawable( native_display->display, drawable );
		native_surface->dri2_buffers = DRI2GetBuffers( native_display->display, drawable, &native_surface->dri2_width, &native_surface->dri2_height, attachments, 1, &native_surface->dri2_num_buffers);

		x_deinit_error_handler( native_display->display, X_ERROR_FLUSH | X_ERROR_SYNC );

		if ( native_surface->dri2_buffers == NULL )
		{
			DRI2DestroyDrawable( native_display->display, drawable );
			_mali_sys_free( native_surface );
			DEBUG_DUMP_ERROR();

			return;
		}

		/* return if we get the same secure_id as previous, output will be set in next acquire_buffer */
		for (i = 0; i < slot_index; i++)
		{
			if ( NULL != surface->buffer[i].render_target )
			{
				egl_memory_handle egl_mem_handle_prev = _egl_memory_get_handle_from_mali_memory( surface->buffer[i].render_target->mem_ref->mali_memory );
				if ( EGL_INVALID_MEMORY_HANDLE != egl_mem_handle_prev)
				{
					if ( native_surface->dri2_buffers[0].name == _egl_memory_get_name_from_handle( drm_fd, egl_mem_handle_prev ) )
					{
						XDBG( 3, "[%d] slot_index=%d, get the same buffer (0x%x)\n", _mali_sys_get_pid(), slot_index, native_surface->dri2_buffers[0].name);
						return;
					}
				}
			}
		}

		__egl_surface_to_surface_specifier( &sformat, surface );

		surface->width	= native_surface->dri2_width;
		surface->height = native_surface->dri2_height;
		sformat.width	= native_surface->dri2_width;
		sformat.height	= native_surface->dri2_height;
		sformat.pitch	= native_surface->dri2_buffers[0].pitch;

		/*
		 * the offset should be dri2_buffer.flags for all the surfaces, but customer may reuse dri2_buffer.flags in ddx driver.
		 * for us, pixmap surface's offset always is 0, but full windwo surface's offset is determined by ddx driver.
		 */
		if ( surface->type != MALI_EGL_PIXMAP_SURFACE )
			offset = native_surface->dri2_buffers[0].flags;

		buf = __egl_platform_map_dri2_buffer( &sformat, native_surface->dri2_buffers[0].name, base_ctx, &native_surface->egl_mem_handle[0], offset );

		XDBG( 3, "[%i] DRI2 UMP ID 0x%x retrieved\n", _mali_sys_get_pid(), native_surface->dri2_buffers[0].name );

		if ( NULL == buf )
		{
			_mali_surface_deref( buf );
			_mali_sys_free( native_surface );

			DEBUG_DUMP_ERROR();
			return;
		}

		surface->buffer[slot_index].render_target = buf;
	}
	else
	{
		/* we have two render_targets, so now pick up which one should be next output */
		slot_index = __egl_platform_get_buffer( surface );
	}

	surface->current_buffer = slot_index;

	_mali_frame_builder_get_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, &usage );
	_mali_frame_builder_set_output( surface->frame_builder, MALI_DEFAULT_COLOR_WBIDX, surface->buffer[surface->current_buffer].render_target, usage);
	if( reset_frame == EGL_TRUE) _mali_frame_builder_reset( surface->frame_builder );

	callback_data = &surface->buffer[surface->current_buffer];
	callback_data->render_target->flags |= MALI_SURFACE_FLAG_TRACK_SURFACE;

	_mali_surface_set_event_callback( callback_data->render_target,
	                                  MALI_SURFACE_EVENT_GPU_WRITE,
	                                  MALI_REINTERPRET_CAST(mali_surface_eventfunc)_egl_surface_gpu_write_callback,
	                                  NULL );
	_mali_surface_set_event_callback( callback_data->render_target,
	                                  MALI_SURFACE_EVENT_GPU_WRITE_DONE,
	                                  MALI_REINTERPRET_CAST(mali_surface_eventfunc)_egl_surface_gpu_write_done_callback,
	                                  NULL );

	_mali_surface_set_event_callback( callback_data->render_target,
	                                  MALI_SURFACE_EVENT_CPU_ACCESS,
	                                  MALI_REINTERPRET_CAST(mali_surface_eventfunc)_egl_surface_cpu_access_callback,
	                                  NULL );
	_mali_surface_set_event_callback( callback_data->render_target,
	                                  MALI_SURFACE_EVENT_CPU_ACCESS_DONE,
	                                  MALI_REINTERPRET_CAST(mali_surface_eventfunc)_egl_surface_cpu_access_done_callback,
	                                  NULL );
}

EGLBoolean __egl_platform_create_surface_pixmap( egl_surface *surface, mali_base_ctx_handle base_ctx )
{
	native_display_data *native_display = NULL;
	native_surface_data *native_surface = NULL;

	native_display = __mali_named_list_get( native_data->displays, (u32)surface->dpy->native_dpy );
	MALI_DEBUG_ASSERT_POINTER( native_display );

	native_surface = (native_surface_data *)_mali_sys_calloc( 1, sizeof( native_surface_data ) );
	if ( NULL == native_surface )
	{
		DEBUG_DUMP_ERROR();

		return EGL_FALSE;
	}

	native_surface->dri2_width = surface->width;
	native_surface->dri2_height = surface->height;
	native_surface->display = native_display->display;

	surface->frame_builder = __egl_mali_create_frame_builder( base_ctx, surface->config, 2, 1, NULL, MALI_FALSE, MALI_FRAME_BUILDER_TYPE_EGL_PIXMAP );

	if ( NULL == surface->frame_builder )
	{
		_mali_sys_free( native_surface );
		DEBUG_DUMP_ERROR();

		return EGL_FALSE;
	}

	surface->num_buffers = 1;
	surface->platform = native_surface;

	surface->buffer[0].render_target = NULL;
	surface->buffer[0].surface = surface;
	surface->buffer[0].id = 0;
	surface->buffer[0].data = NULL;

	surface->copy_func = __egl_platform_copy_buffers;

	__egl_platform_acquire_buffer( surface );

	__egl_platform_begin_new_frame( surface );

	return EGL_TRUE;
}

EGLBoolean __egl_platform_create_surface_window( egl_surface *surface, mali_base_ctx_handle base_ctx )
{
	native_surface_data *native_surface = NULL;
	native_display_data *native_display = NULL;
	u32 i;

	native_display = __mali_named_list_get( native_data->displays, (u32)surface->dpy->native_dpy );
	MALI_DEBUG_ASSERT_POINTER( native_display );

	native_surface = (native_surface_data *)_mali_sys_calloc( 1, sizeof( native_surface_data ) );
	if ( NULL == native_surface )
	{
		DEBUG_DUMP_ERROR();
		return EGL_FALSE;
	}

	native_surface->dri2_width = surface->width;
	native_surface->dri2_height = surface->height;
	native_surface->display = native_display->display;

	surface->frame_builder = __egl_mali_create_frame_builder( base_ctx, surface->config, 2, 2, NULL, MALI_FALSE, MALI_FRAME_BUILDER_TYPE_EGL_WINDOW );

	if ( NULL == surface->frame_builder )
	{
		_mali_sys_free( native_surface );
		DEBUG_DUMP_ERROR();

		return EGL_FALSE;
	}

	surface->num_buffers = NUM_BUFFERS;
	surface->num_frames  = NUM_BUFFERS + 1;

	for ( i=0; i<surface->num_buffers; i++ )
	{
		surface->buffer[i].render_target = NULL;
		surface->buffer[i].surface = surface;
		surface->buffer[i].id = i;
		surface->buffer[i].data = NULL;
	}

	surface->swap_func = __egl_platform_swap_buffers;
	surface->caps |= SURFACE_CAP_DIRECT_RENDERING;
	surface->platform = native_surface;

	native_surface->rect.x = 0;
	native_surface->rect.y = 0;
	native_surface->rect.width = surface->width;
	native_surface->rect.height = surface->height;
	native_surface->region = XFixesCreateRegion( native_display->display, &native_surface->rect, 1 );

	__egl_platform_begin_new_frame( surface );

	return EGL_TRUE;
}

EGLBoolean __egl_platform_create_surface( egl_surface *surface, mali_base_ctx_handle base_ctx )
{
	EGLBoolean retval = EGL_FALSE;
	native_display_data *native_display = NULL;

	MALI_DEBUG_ASSERT_POINTER( surface );

	native_display = __mali_named_list_get( native_data->displays, (u32)surface->dpy->native_dpy );
	MALI_DEBUG_ASSERT_POINTER( native_display );

	surface->num_buffers = 1;
	surface->platform = NULL;

	x_init_error_handler();
	switch ( surface->type )
	{
		case MALI_EGL_WINDOW_SURFACE:  retval = __egl_platform_create_surface_window( surface, base_ctx );  break;
		case MALI_EGL_PBUFFER_SURFACE: retval = __egl_platform_create_surface_pbuffer( surface, base_ctx ); break;
		case MALI_EGL_PIXMAP_SURFACE:  retval = __egl_platform_create_surface_pixmap( surface, base_ctx );  break;
		default:
			break;
	}
	x_deinit_error_handler( native_display->display, X_ERROR_FLUSH | X_ERROR_SYNC );

	if ( EGL_TRUE == x_error_set )
	{
		if ( EGL_TRUE == retval ) __egl_platform_destroy_surface( surface );
	}

	return retval;
}

EGLBoolean __egl_platform_resize_surface( egl_surface *surface, u32 *width, u32 *height, mali_base_ctx_handle base_ctx )
{
	MALI_IGNORE( surface );
	MALI_IGNORE( width );
	MALI_IGNORE( height );
	MALI_IGNORE( base_ctx );

	/* resize is handled in acquire buffer callback for this platform */

	return EGL_TRUE;
}

void __egl_platform_destroy_surface( egl_surface *surface )
{
	Drawable drawable = MALI_REINTERPRET_CAST(Drawable)NULL;
	Display *display;
	native_display_data *native_display = NULL;

	MALI_DEBUG_ASSERT_POINTER( surface );

	if ( NULL != surface->dpy )
	{
		native_display = __mali_named_list_get( native_data->displays, (u32)surface->dpy->native_dpy );
		MALI_DEBUG_ASSERT_POINTER( native_display );

		display = native_display->display;
	}
	else
	{
		display = native_data->default_display;
	}

	x_init_error_handler();
	if ( MALI_EGL_PIXMAP_SURFACE == surface->type )
	{
		drawable = MALI_REINTERPRET_CAST(Drawable)surface->pixmap;
	}
	else if ( MALI_EGL_WINDOW_SURFACE == surface->type )
	{
		drawable = MALI_REINTERPRET_CAST(Drawable)surface->win;
	}

	if ( NULL != MALI_REINTERPRET_CAST(void *)drawable )
	{
		DRI2DestroyDrawable( display, drawable );
	}

	if( NULL != surface->frame_builder ) __egl_mali_destroy_frame_builder( surface );
	surface->frame_builder = NULL;

	if ( EGL_TRUE == surface->is_null_window )
	{
		XUnmapWindow( display, surface->win );
		XDestroyWindow( display, surface->win );
	}

	if ( NULL != surface->platform )
	{
		int i;
		native_surface_data *native_surface = surface->platform;

		for ( i=0; i<native_surface->dri2_num_buffers; i++ )
		{
			if ( EGL_INVALID_MEMORY_HANDLE != native_surface->egl_mem_handle[i] ) _egl_memory_release_reference( native_surface->egl_mem_handle[i] );
		}
		_mali_sys_free( surface->platform );
		surface->platform = NULL;
		surface->pixmap = (EGLNativePixmapType)NULL;
		surface->win = (EGLNativeWindowType)NULL;
	}

	x_deinit_error_handler( display, X_ERROR_FLUSH | X_ERROR_SYNC );
}

#if EGL_KHR_lock_surface_ENABLE
EGLBoolean __egl_platform_lock_surface_map_buffer(EGLNativeDisplayType display, egl_surface *surface, EGLBoolean preserve_contents)
{
	native_surface_data *native_surface = NULL;

	MALI_IGNORE( display );
	MALI_IGNORE( preserve_contents );

	native_surface = surface->platform;

/* TODO, for dma_buf, need mmap interface */
#if MALI_USE_UNIFIED_MEMORY_PROVIDER
	surface->lock_surface->mapped_pointer = (void *)ump_mapped_pointer_get( native_surface->egl_mem_handle[0] );
#endif
	surface->lock_surface->bitmap_pitch = native_surface->dri2_buffers[0].pitch;

	if ( NULL == surface->lock_surface->mapped_pointer ) return EGL_FALSE;

	return EGL_TRUE;
}

EGLBoolean __egl_platform_lock_surface_unmap_buffer(EGLNativeDisplayType display, egl_surface *surface)
{
	native_surface_data *native_surface = NULL;

	MALI_IGNORE( display );

	native_surface = surface->platform;

	return EGL_TRUE;
}
#endif

EGLenum __egl_platform_get_pixmap_colorspace( EGLNativePixmapType pixmap )
{
	MALI_IGNORE( pixmap );

	return EGL_COLORSPACE_sRGB;
}

EGLenum __egl_platform_get_pixmap_alphaformat( EGLNativePixmapType pixmap )
{
	MALI_IGNORE( pixmap );

	return EGL_ALPHA_FORMAT_PRE;
}

void __egl_platform_get_pixmap_size( EGLNativeDisplayType display, EGLNativePixmapType pixmap, u32 *width, u32 *height, u32 *pitch )
{
	Window root_window;
	int pixmap_x, pixmap_y;
	unsigned int pixmap_width, pixmap_height;
	unsigned int pixmap_border_width;
	unsigned int pixmap_depth;
	if ( NULL == display )
		display = __egl_platform_default_display();

	x_init_error_handler();

	XGetGeometry( display, pixmap, &root_window, &pixmap_x, &pixmap_y, &pixmap_width, &pixmap_height, &pixmap_border_width, &pixmap_depth );

	if ( NULL != width ) *width = pixmap_width;
	if ( NULL != height ) *height = pixmap_height;
	if ( NULL != pitch ) *pitch = pixmap_width * pixmap_depth;

	x_deinit_error_handler( display, X_ERROR_FLUSH | X_ERROR_SYNC );
}

EGLBoolean __egl_platform_pixmap_valid( EGLNativePixmapType pixmap )
{
	if ( !pixmap ) return EGL_FALSE;

	return EGL_TRUE;
}

EGLBoolean __egl_platform_pixmap_support_ump( EGLNativePixmapType pixmap )
{
	MALI_IGNORE( pixmap );

	return EGL_TRUE;
}

EGLBoolean __egl_platform_pixmap_config_compatible( EGLNativeDisplayType display, EGLNativePixmapType pixmap, egl_config *config, EGLBoolean renderable_usage )
{
	Window root_window;
	int x, y;
	unsigned int width, height;
	unsigned int border_width;
	unsigned int depth;

	MALI_IGNORE( renderable_usage );

	if ( NULL == display )
		display = __egl_platform_default_display();

	x_init_error_handler();

	XGetGeometry( display, pixmap, &root_window, &x, &y, &width, &height, &border_width, &depth );

	x_deinit_error_handler( display, X_ERROR_FLUSH | X_ERROR_SYNC );

	if ( depth != (u32)config->buffer_size )
	{
		/* allow 32bit config usage on a 24bit window */
		if ( (depth != 24) || (config->buffer_size != 32) ) return EGL_FALSE;
	}

	return EGL_TRUE;
}

EGLBoolean __egl_platform_pixmap_surface_compatible( EGLNativeDisplayType display, EGLNativePixmapType pixmap, egl_surface *surface, EGLBoolean alpha_check )
{
	Window root_window;
	int x, y;
	unsigned int width, height;
	unsigned int border_width;
	unsigned int depth;

	if ( NULL == display )
		display = __egl_platform_default_display();

	x_init_error_handler();

	if ( EGL_TRUE == alpha_check ) return EGL_TRUE; /* ? */

	XGetGeometry( display, pixmap, &root_window, &x, &y, &width, &height, &border_width, &depth );

	x_deinit_error_handler( display, X_ERROR_FLUSH | X_ERROR_SYNC );

	if ( width != surface->width ) return EGL_FALSE;
	if ( height != surface->height ) return EGL_FALSE;

	return EGL_TRUE;
}

EGLBoolean __egl_platform_pixmap_copybuffers_compatible( EGLNativeDisplayType display, EGLNativePixmapType pixmap, egl_surface *surface )
{
	/* This platform does not support conversion for eglCopyBuffers */

	/* check if target pixmap is compatible with config */
	if ( EGL_FALSE == __egl_platform_pixmap_config_compatible( display, pixmap, surface->config, EGL_TRUE ) ) return EGL_FALSE;

	/* check if target pixmap is compatible with surface */
	if ( EGL_FALSE == __egl_platform_pixmap_surface_compatible( display, pixmap, surface, EGL_FALSE ) ) return EGL_FALSE;

	return EGL_TRUE;
}

EGLBoolean __egl_platform_pixmap_egl_image_compatible( EGLNativePixmapType pixmap, egl_context *context )
{
	MALI_IGNORE( pixmap );
	MALI_IGNORE( context );

	return EGL_TRUE;
}

void __egl_platform_update_image( struct mali_surface * surface, void* data )
{
	__egl_main_context *egl = __egl_get_main_context();
	mali_base_ctx_handle base_ctx = egl->base_ctx;

	uint32_t attachments[1] = { DRI2BufferFrontLeft };
	int dri2_width = 0, dri2_height = 0, dri2_num_buffers = 1;
	DRI2Buffer *dri2_buffer;
	Drawable drawable;
	mali_surface_specifier sformat;
	egl_memory_handle egl_mem_handle;
	egl_buffer_name   egl_buf_name;
	mali_mem_handle mali_mem = MALI_NO_HANDLE;
	struct mali_shared_mem_ref *mem_ref;

	x_init_error_handler();

	drawable = MALI_REINTERPRET_CAST(Drawable)data;

	__egl_platform_get_pixmap_format(native_data->default_display, drawable, &sformat);

	DRI2CreateDrawable( native_data->default_display, drawable );
	dri2_buffer = DRI2GetBuffers( native_data->default_display, drawable, &dri2_width, &dri2_height, attachments, 1, &dri2_num_buffers);

	x_deinit_error_handler( native_data->default_display, X_ERROR_FLUSH | X_ERROR_SYNC );

	if ( (NULL == dri2_buffer) || !dri2_buffer[0].name)
	{
		DEBUG_DUMP_ERROR();
		return;
	}

	egl_mem_handle = _egl_memory_get_handle_from_mali_memory( surface->mem_ref->mali_memory );
	egl_buf_name = _egl_memory_get_name_from_handle( drm_fd, egl_mem_handle );

	if ( egl_buf_name != dri2_buffer[0].name )
	{
		egl_mem_handle = _egl_memory_create_handle_from_name( drm_fd, dri2_buffer[0].name );
		if ( EGL_INVALID_MEMORY_HANDLE == egl_mem_handle )
		{
			DEBUG_DUMP_ERROR();
			return;
		}

		mali_mem = _egl_memory_create_mali_memory_from_handle( base_ctx, egl_mem_handle, 0 );

		_egl_memory_release_reference( egl_mem_handle );

		if ( MALI_NO_HANDLE == mali_mem )
		{
			DEBUG_DUMP_ERROR();
			return;
		}

		mem_ref = _mali_shared_mem_ref_alloc_existing_mem( mali_mem );
		if ( NULL == mem_ref )
		{
			_mali_mem_free(mali_mem);
			DEBUG_DUMP_ERROR();
			return;
		}

		if ( surface->mem_ref )
		{
			_mali_shared_mem_ref_owner_deref( surface->mem_ref );
		}

		_mali_surface_access_lock( surface );
		surface->mem_ref = mem_ref;
		_mali_surface_access_unlock( surface );
	}
}

struct mali_image* __egl_platform_map_pixmap( EGLNativeDisplayType display, struct egl_image *egl_img, EGLNativePixmapType pixmap )
{
	__egl_main_context *egl = __egl_get_main_context();
	mali_base_ctx_handle base_ctx;
	Drawable drawable;
	DRI2Buffer *dri2_buffer;
	uint32_t attachments[1] = { DRI2BufferFrontLeft };
	int dri2_width = 0, dri2_height = 0, dri2_num_buffers = 1;
	mali_image *image = NULL;
	mali_surface_specifier sformat;
	egl_memory_handle egl_mem_handle = EGL_INVALID_MEMORY_HANDLE;

	MALI_IGNORE( egl_img );

	if ( NULL == display )
		display = __egl_platform_default_display();

	base_ctx = egl->base_ctx;

	x_init_error_handler();

	__egl_platform_get_pixmap_format(display, pixmap, &sformat);

	drawable = MALI_REINTERPRET_CAST(Drawable)pixmap;
	DRI2CreateDrawable( display, drawable );
	dri2_buffer = DRI2GetBuffers( display, drawable, &dri2_width, &dri2_height, attachments, 1, &dri2_num_buffers);
	x_deinit_error_handler( display, X_ERROR_FLUSH | X_ERROR_SYNC );

	XDBG( 3, "[%i] DRI2 MAP PIXMAP SECURE ID %d\n", _mali_sys_get_pid(), dri2_buffer[0].name );
	if ( NULL == dri2_buffer )
	{
		DEBUG_DUMP_ERROR();
		return EGL_FALSE;
	}

	egl_mem_handle = _egl_memory_create_handle_from_name( drm_fd, dri2_buffer[0].name );
	if ( EGL_INVALID_MEMORY_HANDLE == egl_mem_handle )
	{
		DEBUG_DUMP_ERROR();
		return EGL_FALSE;
	}

	sformat.width  = dri2_width;
	sformat.height = dri2_height;
	sformat.pitch  = dri2_buffer[0].pitch;

	image = mali_image_create_from_ump_or_mali_memory( MALI_SURFACE_FLAG_DONT_MOVE,
	                                                &sformat,
#if MALI_USE_DMA_BUF
	                                                MALI_IMAGE_DMA_BUF,
#else
	                                                MALI_IMAGE_UMP_MEM,
#endif
	                                                egl_mem_handle, 0 /* offset */,
	                                                base_ctx );

	MALI_CHECK_NON_NULL( image, NULL );

	_egl_memory_release_reference( egl_mem_handle );

	return image;
}

void __egl_platform_unmap_pixmap( EGLNativePixmapType pixmap, struct egl_image *egl_img )
{
	MALI_IGNORE( pixmap );
	MALI_IGNORE( egl_img );
}

void __egl_platform_unmap_image_buffer( void *buffer )
{
	MALI_IGNORE( buffer );
}

void __egl_platform_swap_buffers( mali_base_ctx_handle base_ctx,
                                  EGLNativeDisplayType native_dpy,
                                  egl_surface *surface,
                                  struct mali_surface *target,
                                  EGLint interval)
{
	int swap_count;
	Drawable drawable = MALI_REINTERPRET_CAST(Drawable)surface->win;

	MALI_IGNORE( base_ctx );
	MALI_IGNORE( target );
	MALI_IGNORE( interval );

	DRI2SwapBuffers(native_data->default_display, drawable, 0, 0, 0, (CARD64*)&swap_count);
}

mali_mem_handle __egl_platform_pixmap_get_mali_memory( EGLNativeDisplayType display, EGLNativePixmapType native_pixmap, mali_base_ctx_handle base_ctx, egl_surface *surface )
{
	egl_memory_handle egl_mem_handle;
	mali_mem_handle mali_mem = MALI_NO_HANDLE;
	uint32_t attachments[1] = { DRI2BufferFrontLeft };
	Drawable drawable;
	DRI2Buffer *dri2_buffer;
	int dri2_width, dri2_height, dri2_num_buffers = 1;

	MALI_IGNORE( surface );

	/* get pixmap UMP handle */
	drawable = MALI_REINTERPRET_CAST(Drawable)native_pixmap;
	DRI2CreateDrawable( display, drawable );
	dri2_buffer = DRI2GetBuffers( display, drawable, &dri2_width, &dri2_height, attachments, 1, &dri2_num_buffers);

	if ( dri2_buffer == NULL )
	{
		DEBUG_DUMP_ERROR();
		return EGL_FALSE;
	}

	egl_mem_handle = _egl_memory_create_handle_from_name( drm_fd, dri2_buffer[0].name );
	if ( EGL_INVALID_MEMORY_HANDLE == egl_mem_handle )
	{
		DEBUG_DUMP_ERROR();
		return EGL_FALSE;
	}

	/* wrap handle into mali memory */
	mali_mem = _egl_memory_create_mali_memory_from_handle( base_ctx, egl_mem_handle, 0 );
	if ( MALI_NO_HANDLE == mali_mem )
	{
		DEBUG_DUMP_ERROR();
		return EGL_FALSE;
	}

	return mali_mem;
}

void __egl_platform_copy_buffers( mali_base_ctx_handle base_ctx,
                                  EGLNativeDisplayType dpy,
                                  egl_surface *surface,
                                  struct mali_surface *target,
                                  EGLNativePixmapType native_pixmap )
{
	mali_mem_handle target_buffer = MALI_NO_HANDLE;
	mali_mem_handle pixmap_mem_handle = MALI_NO_HANDLE;
	u32 pixmap_pitch, pixmap_offset = 0;
	u32 target_pitch, target_offset = 0;
	u32 y;
	Window pixmap_root_window;
	int pixmap_x, pixmap_y;
	unsigned int pixmap_width, pixmap_height;
	unsigned int pixmap_border_width;
	unsigned int pixmap_depth;
	native_display_data *native_display = NULL;
	__egl_main_context *egl = __egl_get_main_context();

	MALI_IGNORE( dpy );

	native_display = __mali_named_list_get( native_data->displays, (u32)surface->dpy->native_dpy );
	MALI_DEBUG_ASSERT_POINTER( native_display );

	XGetGeometry( native_display->display, native_pixmap, &pixmap_root_window, &pixmap_x, &pixmap_y, &pixmap_width, &pixmap_height, &pixmap_border_width, &pixmap_depth );

	/* It is assumed that any pixmap entering this path has been allocated in such a way that UMP can access it */
	pixmap_mem_handle = __egl_platform_pixmap_get_mali_memory( native_display->display, native_pixmap, base_ctx, surface );

	target_pitch = target->format.pitch;
	pixmap_pitch = pixmap_width*pixmap_depth;

	target_buffer = target->mem_ref->mali_memory;

	if ( EGL_FALSE == egl->flip_pixmap )
	{
		/* fast-path for aligned surfaces */
		if ( target_pitch == pixmap_pitch )
		{
			_mali_mem_copy( pixmap_mem_handle, 0, target_buffer, 0, target_pitch*surface->height );
		}
		else
		{
			for ( y=0; y<pixmap_height; y++, pixmap_offset+=pixmap_pitch, target_offset+=target_pitch )
			{
				_mali_mem_copy( pixmap_mem_handle, pixmap_offset, target_buffer, target_offset, pixmap_pitch );
			}
		}
	}
	else
	{
		target_offset = (surface->height - 1) * target_pitch;
		for ( y=0; y<pixmap_height; y++, pixmap_offset+=pixmap_pitch, target_offset-=target_pitch )
		{
			_mali_mem_copy( pixmap_mem_handle, pixmap_offset, target_buffer, target_offset, pixmap_pitch );
		}
	}
}

void __egl_platform_set_window_size( EGLNativeWindowType win, u32 width, u32 height )
{
	MALI_IGNORE( win );
	MALI_IGNORE( width );
	MALI_IGNORE( height );
}

EGLBoolean __egl_platform_get_window_size( EGLNativeWindowType win, void *platform, u32 *width, u32 *height )
{
	XWindowAttributes window_attributes;
	Window x11_win = (Window)win;
	native_surface_data *native_surface = platform;

	if ( platform == NULL ) XGetWindowAttributes( native_data->default_display, x11_win, &window_attributes );
	else
	{
		XGetWindowAttributes( native_surface->display, x11_win, &window_attributes );
	}

	*width = window_attributes.width;
	*height = window_attributes.height;

	return EGL_TRUE;
}

EGLBoolean __egl_platform_window_valid( EGLNativeDisplayType display, EGLNativeWindowType win )
{
	XWindowAttributes window_attributes;
	Window x11_win = (Window)win;

	if ( (Window)NULL == win ) return EGL_FALSE; /* in case of NULL window given */

	if ( (int)win < 0) return EGL_FALSE;

	x_init_error_handler();
	if ( BadWindow == XGetWindowAttributes( display, x11_win, &window_attributes ) )
	{
		x_deinit_error_handler( display, X_ERROR_FLUSH | X_ERROR_SYNC );
		return EGL_FALSE;
	}
	x_deinit_error_handler( display, X_ERROR_FLUSH | X_ERROR_SYNC );

	if ( EGL_TRUE == x_error_set )
	{
		return EGL_FALSE;
	}

	return EGL_TRUE;
}

EGLBoolean __egl_platform_window_compatible( EGLNativeDisplayType display, EGLNativeWindowType win, egl_config *config )
{
	XWindowAttributes window_attributes;

	/* even an invalid window is not compatible */
	if ( (Window)NULL == win ) return EGL_TRUE; /* in case of NULL window given */

	if ( (int)win < 0) return EGL_FALSE;

	x_init_error_handler();
	XGetWindowAttributes( display, (Window)win, &window_attributes );
	if ( (window_attributes.depth != config->buffer_size) )
	{
		/* allow 32bit config usage on a 24bit window */
		if ( (window_attributes.depth != 24) || (config->buffer_size != 32) )
		{
			x_deinit_error_handler( display, X_ERROR_FLUSH | X_ERROR_SYNC );
			DEBUG_DUMP_ERROR();
			return EGL_FALSE;
		}
	}
	x_deinit_error_handler( display, X_ERROR_FLUSH | X_ERROR_SYNC );

	if ( EGL_TRUE == x_error_set )
	{
		DEBUG_DUMP_ERROR();
		return EGL_FALSE;
	}

	return EGL_TRUE;
}

void __egl_platform_begin_new_frame( egl_surface *surface )
{
	_mali_frame_builder_set_acquire_output_callback( surface->frame_builder,
	                                                MALI_REINTERPRET_CAST(mali_frame_cb_func)__egl_platform_acquire_buffer,
	                                                MALI_REINTERPRET_CAST(mali_frame_cb_param)surface );
}

EGLNativeWindowType __egl_platform_create_null_window( EGLNativeDisplayType dpy )
{
	Window xwindow, root_xwindow;
	Display *xdisplay = NULL;
	Visual *visual = NULL;
	XSetWindowAttributes attributes;
	Pixmap bm_no;
	Colormap cmap;
	Cursor no_ptr;
	XColor black, dummy;
	static char bm_no_data[] = {0, 0, 0, 0, 0, 0, 0, 0};
	unsigned long valuemask = CWOverrideRedirect;
	int depth;
	int screen;
	int screen_width, screen_height;
	Hints hints;
	Atom property;

	XInitThreads();

	xdisplay = dpy;

	screen = DefaultScreen( xdisplay );
	screen_width = DisplayWidth( xdisplay, screen );
	screen_height = DisplayHeight( xdisplay, screen );

	depth = DefaultDepth( xdisplay, screen );
	root_xwindow = DefaultRootWindow( xdisplay );
	visual = DefaultVisual( xdisplay, screen );
	xwindow = XCreateSimpleWindow( xdisplay, RootWindow( xdisplay, screen ), 0, 0, screen_width, screen_height, 0, BlackPixel( xdisplay, screen ), WhitePixel( xdisplay, screen ) );

	XSelectInput( xdisplay, xwindow, KeyPressMask);

	hints.flags = 2; /* change window decorations */
	hints.decorations = 0; /* turn decorations off */
	property = XInternAtom( xdisplay, "_MOTIF_WM_HINTS", True );

	XChangeProperty( xdisplay, xwindow, property, property, 32, PropModeReplace, (unsigned char *)&hints, 5 );

	attributes.override_redirect = True;

	XChangeWindowAttributes( xdisplay, xwindow, valuemask, &attributes );

	/* hide mouse cursor */
	cmap = DefaultColormap( xdisplay, DefaultScreen(xdisplay));
	XAllocNamedColor(xdisplay, cmap, "black", &black, &dummy);
	bm_no = XCreateBitmapFromData(xdisplay, xwindow, bm_no_data, 8, 8);
	no_ptr = XCreatePixmapCursor(xdisplay, bm_no, bm_no, &black, &black, 0, 0);

	XDefineCursor(xdisplay, xwindow, no_ptr);
	XFreeCursor(xdisplay, no_ptr);
	if (bm_no != None) XFreePixmap(xdisplay, bm_no);
	XFreeColors(xdisplay, cmap, &black.pixel, 1, 0);


	XMapWindow( xdisplay, xwindow );
	XFlush( xdisplay );

	return (EGLNativeWindowType)xwindow;

}

void __egl_platform_get_pixmap_format( EGLNativeDisplayType display, EGLNativePixmapType pixmap, mali_surface_specifier *sformat )
{
	unsigned int i, rsize = 0, gsize = 0, bsize = 0, asize = 0, lsize = 0, buffer_size = 0;
	m200_texel_format texel_format = M200_TEXEL_FORMAT_NO_TEXTURE;
	m200_texture_addressing_mode texel_layout = M200_TEXTURE_ADDRESSING_MODE_LINEAR;
	Window root_window;
	int x, y;
	unsigned int width, height;
	unsigned int border_width;
	unsigned int depth;

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

	MALI_DEBUG_ASSERT_POINTER( sformat );

	if ( NULL == display )
		display = __egl_platform_default_display();

	/* extract what we can about the pixmap format */
	XGetGeometry( display, pixmap, &root_window, &x, &y, &width, &height, &border_width, &depth );

	switch ( depth )
	{
		case 32:
			rsize = gsize = bsize = asize = 8;
			lsize = 0;
			buffer_size = 32;
			break;

		case 24:
			rsize = gsize = bsize = 8; asize = 0;
			lsize = 0;
			buffer_size = 32;
			break;

		case 16:
			rsize = 5; gsize = 6; bsize = 5; asize = 0;
			lsize = 0;
			buffer_size = 16;
			break;

		case 8:
			rsize = 0; gsize = 0; bsize = 0; asize = 0;
			lsize = 8;
			buffer_size = 8;
			break;


		default:
			break;
	}

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
		0,
		0,
		0,
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

EGLBoolean __egl_platform_supports_vsync( void )
{
	return EGL_FALSE;
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

void __egl_platform_register_lock_item( egl_surface_lock_item *lock_item )
{
#if MALI_USE_UNIFIED_MEMORY_PROVIDER && EGL_SURFACE_LOCKING_ENABLED
	_lock_item_s item;

	item.secure_id = lock_item->buf_name;
	item.usage = lock_item->usage;

	if ( native_data->fd_umplock )
	{
		ioctl( native_data->fd_umplock, LOCK_IOCTL_CREATE, &item );
		XDBG(3, "[%i] CREATE %i (%i)\n", _mali_sys_get_pid(), item.secure_id, item.usage );
	}
#elif MALI_USE_DMA_BUF
#endif
}

void __egl_platform_unregister_lock_item( egl_surface_lock_item *lock_item )
{
#if MALI_USE_UNIFIED_MEMORY_PROVIDER && EGL_SURFACE_LOCKING_ENABLED
	_lock_item_s item;

	item.secure_id = lock_item->buf_name;
	item.usage = lock_item->usage;

	/*ioctl( native_data->fd_umplock, LOCK_IOCTL_DESTROY, &item );*/
	XDBG(3, "[%i] DESTROY %i (%i)\n", _mali_sys_get_pid(), item.secure_id, item.usage );
#elif MALI_USE_DMA_BUF
#endif
}

void __egl_platform_process_lock_item( egl_surface_lock_item *lock_item )
{
#if MALI_USE_UNIFIED_MEMORY_PROVIDER && EGL_SURFACE_LOCKING_ENABLED
	_lock_item_s item;

	item.secure_id = lock_item->buf_name;
	item.usage = lock_item->usage;

	if ( native_data->fd_umplock )
	{
#if MALI_TIMELINE_PROFILING_ENABLED
		_mali_base_profiling_add_event( VR_PROFILING_EVENT_TYPE_SINGLE | VR_PROFILING_EVENT_CHANNEL_SOFTWARE | VR_PROFILING_EVENT_REASON_SINGLE_SW_UMP_TRY_LOCK, 0, 0, item.secure_id, item.usage, 0);
#endif
		if ( ioctl( native_data->fd_umplock, LOCK_IOCTL_PROCESS, &item ) < 0 )
		{
			int max_retries = 5;
			XDBG(1, "[%i] PROCESS lock item 0x%x failed to acquire lock, throttling\n", _mali_sys_get_pid(), item.secure_id );
			while ( ioctl( native_data->fd_umplock, LOCK_IOCTL_PROCESS, &item ) && max_retries )
			{
				_mali_sys_usleep(2000);
				max_retries--;
			}
		}
#if MALI_TIMELINE_PROFILING_ENABLED
		_mali_base_profiling_add_event( VR_PROFILING_EVENT_TYPE_SINGLE | VR_PROFILING_EVENT_CHANNEL_SOFTWARE | VR_PROFILING_EVENT_REASON_SINGLE_SW_UMP_LOCK, 0, 0, item.secure_id, item.usage, 0);
#endif
		XDBG(3, "[%i]  PROCESS %i (%i)\n", _mali_sys_get_pid(), item.secure_id, item.usage );
	}
#elif MALI_USE_DMA_BUF
#endif
}

void __egl_platform_release_lock_item( egl_surface_lock_item *lock_item )
{
#if MALI_USE_UNIFIED_MEMORY_PROVIDER && EGL_SURFACE_LOCKING_ENABLED
	_lock_item_s item;

	item.secure_id = lock_item->buf_name;
	item.usage = lock_item->usage;

	if ( native_data->fd_umplock )
	{
		ioctl( native_data->fd_umplock, LOCK_IOCTL_RELEASE, &item );
#if MALI_TIMELINE_PROFILING_ENABLED
		_mali_base_profiling_add_event( VR_PROFILING_EVENT_TYPE_SINGLE | VR_PROFILING_EVENT_CHANNEL_SOFTWARE | VR_PROFILING_EVENT_REASON_SINGLE_SW_UMP_UNLOCK, 0, 0, item.secure_id, item.usage, 0);
#endif
		XDBG(3, "[%i]   RELEASE %i (%i)\n", _mali_sys_get_pid(), item.secure_id, item.usage );
	}
#elif MALI_USE_DMA_BUF
#endif
}
