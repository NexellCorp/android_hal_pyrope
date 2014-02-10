/*
 * $Copyright:
 * ----------------------------------------------------------------------------
 *  This confidential and proprietary software may be used only as authorized
 *  by a licensing agreement from ARM Limited.
 *          (C) COPYRIGHT 2011-2012 ARM Limited , ALL RIGHTS RESERVED
 *  The entire notice above must be reproduced on all authorized copies and
 *  copies may only be made to the extent permitted by a licensing agreement
 *  from ARM Limited.
 * ----------------------------------------------------------------------------
 * $
 */
#include <mali_system.h>
#if MALI_ANDROID_API >= 15
 #include <system/window.h>
 #include <gui/ISurfaceComposer.h>
 #include <gui/SurfaceComposerClient.h>
 #if !defined( NEXELL_FEATURE_KITKAT_USE )
 #include <gui/SurfaceTexture.h>
 #include <gui/SurfaceTextureClient.h>
 #endif 
#if MALI_ANDROID_API >= 17
 #include <ui/DisplayInfo.h>
#endif
#else
 #include <ui/android_native_buffer.h>
 #include <surfaceflinger/Surface.h>
 #include <surfaceflinger/ISurface.h>
 #include <surfaceflinger/SurfaceComposerClient.h>
#endif

#include <ui/FramebufferNativeWindow.h>

#define MALI_TPI_EGL_API MALI_TPI_EXPORT
#include <mali_tpi.h>
#include <mali_tpi_egl.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#define FB_PATH "/dev/graphics/fb0"

#define MAX_WINDOW_COUNT 32
#define WINDOW_LAYER_VALUE 0x4000000
using namespace android;

/* Global client for Surface Composer */

typedef struct TPIWindow {
	sp<SurfaceControl> control;
	sp<Surface> surface;
	ANativeWindow *win;
}TPIWindow;

typedef struct _TPIDisplayInfo {
	int width;
	int height;
}TPIDisplayInfo;

static sp<SurfaceComposerClient> client = NULL;
static TPIWindow windows[MAX_WINDOW_COUNT];

/* Local functions */
static mali_tpi_bool tpip_init(void);
static mali_tpi_bool tpip_create_window(int width, 
					int height, 
					int pixel_format, 
					int layer,
					TPIWindow *window);

static void mali_tpi_get_display_info( TPIDisplayInfo *displayInfo)
{
	if ( displayInfo == NULL )
		return;

#if MALI_ANDROID_API < 17
	displayInfo->width = client->getDisplayWidth(0);
	displayInfo->height = client->getDisplayHeight(0);
#else
	DisplayInfo info;
	sp<IBinder> _display = client->getBuiltInDisplay(ISurfaceComposer::eDisplayIdMain);
	client->getDisplayInfo( _display, &info );
	displayInfo->width = info.w;
	displayInfo->height = info.h;
#endif
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_init()
{
	mali_tpi_bool ret = MALI_TPI_TRUE;

	if(0 != setenv("EGLP_WAIT_FOR_COMPOSITION", "1", 1))
	{
		ret = MALI_TPI_FALSE;	
		goto finish;
	}

	/* Only initialize once */
	if(NULL == client.get())
	{
		ret = tpip_init();
		if( MALI_TPI_FALSE == ret )
		{
			goto finish;
		}
	}

	ret = _mali_tpi_egl_init_internal();

finish:
	return ret;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_shutdown()
{
	/* Destroy the background surface */
	if(windows[0].win != NULL)
	{
		mali_tpi_egl_destroy_window(EGL_DEFAULT_DISPLAY, windows[0].win);
	}

	/* Clear references to SurfaceComposer */
	client.clear();
	client = NULL;

	return _mali_tpi_egl_shutdown_internal();
}

MALI_TPI_IMPL EGLNativeDisplayType mali_tpi_egl_get_default_native_display( void )
{
	/* Android only supports the default display */
	return EGL_DEFAULT_DISPLAY;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_get_invalid_native_display(EGLNativeDisplayType *native_display)
{
	*native_display= (EGLNativeDisplayType)-1;

	return MALI_TPI_TRUE;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_create_pixmap(
	EGLNativeDisplayType native_display,
	int width,
	int height,
	EGLDisplay display,
	EGLConfig config,
	EGLNativePixmapType *pixmap)
{
	return MALI_TPI_FALSE;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_destroy_pixmap(
	EGLNativeDisplayType native_display,
	EGLNativePixmapType pixmap)
{
	return MALI_TPI_FALSE;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_fill_pixmap(
	EGLNativeDisplayType native_display,
	EGLNativePixmapType pixmap,
	mali_tpi_egl_simple_color color,
	int to_fill)
{
	return MALI_TPI_FALSE;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_check_pixmap_pixel(
	EGLNativeDisplayType native_display,
	EGLNativePixmapType pixmap,
	int x,
	int y,
	mali_tpi_egl_simple_color color,
	mali_tpi_bool *result)
{
	return MALI_TPI_FALSE;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_create_window(
	EGLNativeDisplayType native_display,
	int width,
	int height,
	EGLDisplay display,
	EGLConfig config,
	EGLNativeWindowType *window )
{
	EGLBoolean egl_res;
	int pixel_format;
	mali_tpi_bool retval = MALI_TPI_FALSE;
	int i;

	/* TODO: MIDEGL-857 */
	if(NULL == client.get())
	{
		tpip_init();
	}	

	/* The EGL_NATIVE_VISUAL_ID should be the Android enum for the pixel format of the config */
	egl_res = eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &pixel_format);
	if(EGL_FALSE == egl_res)
	{
		goto out;
	}

	for(i=0;i<MAX_WINDOW_COUNT;i++)
	{
		if(windows[i].win == NULL)
		{
			break;
		}
	}

	if(i < MAX_WINDOW_COUNT)
	{
		retval = tpip_create_window(width, height, pixel_format, 
						WINDOW_LAYER_VALUE + 1, &windows[i]);
		if(MALI_TPI_TRUE == retval)
		{
			*window = reinterpret_cast<EGLNativeWindowType>(windows[i].win);
		}
	}
out:
	return retval;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_resize_window(
	EGLNativeDisplayType native_display,
	EGLNativeWindowType *window,
	int width,
	int height,
	EGLDisplay display,
	EGLConfig config)
{
	EGLBoolean egl_res;
	int pixel_format;
	mali_tpi_bool retval = MALI_TPI_FALSE;
	mali_tpi_bool res;
	ANativeWindow *w = reinterpret_cast<ANativeWindow*>(*window);
	int i;

	/* The EGL_NATIVE_VISUAL_ID should be the Android enum for the pixel format of the config */
	egl_res = eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &pixel_format);
	if(EGL_FALSE == egl_res)
	{
		/* We've not taken a reference on the Window so nothing to free */
		goto out;
	}

	for(i=0;i<MAX_WINDOW_COUNT;i++)
	{
		if(windows[i].win == w)
		{
			break;
		}
	}

	if(i < MAX_WINDOW_COUNT)
	{
		SurfaceComposerClient::openGlobalTransaction();
		windows[i].control->setSize(width,height);
		SurfaceComposerClient::closeGlobalTransaction();
		retval = MALI_TPI_TRUE;
	}

out:
	return retval;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_create_fullscreen_window(
	EGLNativeDisplayType native_display,
	EGLDisplay display,
	EGLConfig config,
	EGLNativeWindowType *window )
{
	mali_tpi_bool ret = MALI_TPI_TRUE;
	TPIDisplayInfo display_info;
	/* TODO: MIDEGL-857 */
	if(NULL == client.get())
	{
		ret = tpip_init();
	}

	if(MALI_TPI_TRUE == ret)
	{
		mali_tpi_get_display_info( &display_info );
		ret = mali_tpi_egl_create_window(native_display, display_info.width, display_info.height, display, config, window);
	}

	return ret;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_destroy_window(
	EGLNativeDisplayType native_display,
	EGLNativeWindowType window )
{
	mali_tpi_bool retval = MALI_TPI_FALSE;
	int i;
	/* we need to handle window == NULL like in the Linux implementation, for consistency */
	if (NULL == window)
	{
		return MALI_TPI_TRUE;
	}
	ANativeWindow *w = reinterpret_cast<ANativeWindow*>(window);


	for(i=0;i<MAX_WINDOW_COUNT;i++)
	{
		if(windows[i].win == w)
		{
			break;
		}
	}

	if(i < MAX_WINDOW_COUNT)
	{
		/* Remove SurfaceControl's references */
		windows[i].control->clear();

		/* Remove TPI's references */
		windows[i].surface.clear();
		windows[i].control.clear();

		windows[i].win = NULL;

		retval = MALI_TPI_TRUE;
	}	
	return retval;
}

MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_get_window_dimensions(
	EGLNativeDisplayType native_display,
	EGLNativeWindowType window,
	int *width,
	int *height)
{
	ANativeWindow *w = reinterpret_cast<ANativeWindow*>(window);

	w->query(w, NATIVE_WINDOW_WIDTH, width);
	w->query(w, NATIVE_WINDOW_HEIGHT, height);

	return MALI_TPI_TRUE;
}


static mali_tpi_bool _mali_tpi_egl_check_color_component(unsigned char *pixel_address, int bytes_per_pixel,
		struct fb_bitfield *bf, int expected)
{
	mali_tpi_bool retval;
	mali_tpi_uint32 pixel;
	mali_tpi_uint32 component;
	mali_tpi_uint32 component_mask = 0;

	/* Make sure we don't get an unaligned access */
	memcpy(&pixel, pixel_address, sizeof(pixel));

	/* Shift to the least significant end if necessary */
	/* TODO: MIDEGL-724 */
	/*pixel >>= sizeof(pixel) - bytes_per_pixel;*/

	component_mask = (1 << bf->length) - 1;

	component = pixel >> bf->offset;
	component &= component_mask;

	if (0 == expected)
	{
		if (0 == component)
		{
			retval = MALI_TPI_TRUE;
		}
		else
		{
			retval = MALI_TPI_FALSE;
		}
	}
	else
	{
		if (component_mask == component)
		{
			retval = MALI_TPI_TRUE;
		}
		else
		{
			retval = MALI_TPI_FALSE;
		}
	}

	return retval;
}

/** The current state of checking window pixels:
 *  We will be reading directly from the front buffer by changing the permissions of fbdev 
 *  to give global access (for testing purposes only). The windows we are creating in the application
 *  take up the entire screen and therefore we *should* be able to correctly read back test output.
 *  Once we are testing on an implementation that supports GLES1.1 with FBOs we *should* be able to use
 *  a feature of the surface composition to take screenshots meaning we won't need direct access to fbdev
 */
MALI_TPI_IMPL mali_tpi_bool mali_tpi_egl_check_window_pixel(
		EGLNativeDisplayType native_display,
		EGLNativeWindowType window,
		int x, int y,
		mali_tpi_egl_simple_color color,
		mali_tpi_bool *result)
{
	mali_tpi_bool retval = MALI_TPI_TRUE;
	int fd;
	int fb_res;
	int bytes_per_pixel;
	int window_height, window_width;
	struct fb_var_screeninfo vinfo;
	unsigned char *buffer;
	unsigned char *pixel_address;

	fd = open(FB_PATH, O_RDONLY);
	if (-1 == fd)
	{
		retval = MALI_TPI_FALSE;
		goto finish;
	}

	fb_res = ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
	if(-1 == fb_res)
	{
		close(fd);
		retval = MALI_TPI_FALSE;
		goto finish;
	}

	window_width = vinfo.xres;
	window_height = vinfo.yres;

	if ((x >= window_width) || (x < 0) || (y >= window_height) || (y < 0))
	{
		close(fd);
		retval = MALI_TPI_FALSE;
		goto finish;
	}

	bytes_per_pixel = vinfo.bits_per_pixel >> 3;

	buffer = (unsigned char *)mmap(NULL, vinfo.xres * vinfo.yres_virtual * bytes_per_pixel, PROT_READ, MAP_SHARED, fd, 0);
	if (MAP_FAILED == buffer)
	{
		close(fd);
		retval = MALI_TPI_FALSE;
		goto finish;
	}

	/* First get the top left of the area of the framebuffer currently being displayed */
	pixel_address = buffer + (vinfo.yoffset * vinfo.xres * bytes_per_pixel);
	/* Then the pixel address within the window - Windows are always placed in the upper left corner */
	pixel_address += (y * vinfo.xres + x) * bytes_per_pixel;

	*result = (_mali_tpi_egl_check_color_component(pixel_address, bytes_per_pixel, &vinfo.red,    0x8 & color) &&
	           _mali_tpi_egl_check_color_component(pixel_address, bytes_per_pixel, &vinfo.green,  0x4 & color) &&
	           _mali_tpi_egl_check_color_component(pixel_address, bytes_per_pixel, &vinfo.blue,   0x2 & color) &&
	           _mali_tpi_egl_check_color_component(pixel_address, bytes_per_pixel, &vinfo.transp, 0x1 & color));

	munmap(buffer, vinfo.xres * vinfo.yres_virtual * bytes_per_pixel);
	close(fd);

finish:
	return retval;
}

MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_create_external_pixmap(
	EGLNativeDisplayType native_display,
	int width,
	int height,
	EGLDisplay display,
	mali_tpi_egl_pixmap_format format,
	EGLNativePixmapType *pixmap)
{
	/* TODO: Implement */
	return MALI_FALSE;
}

MALI_TPI_EGL_API mali_tpi_bool mali_tpi_egl_load_pixmap(
	EGLNativePixmapType pixmap,
	unsigned int num_planes,
	void const * const data[])
{
	/* TODO: Implement */
	return MALI_FALSE;
}

MALI_TPI_EGL_API EGLint mali_tpi_egl_get_pixmap_type(void)
{
	/* TODO: Implement */
	return 0;
}


static mali_tpi_bool tpip_create_window(int width, 
					int height, 
					int pixel_format, 
					int layer,
					TPIWindow *window)
{
	mali_tpi_bool ret = MALI_TPI_TRUE;
	
	/* Create the Surface */
#if MALI_ANDROID_API < 17 
	window->control = client->createSurface(0, width,
				height, pixel_format, 
				GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN | 
				GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE );
#else
	window->control = client->createSurface(String8("tpiWindow"), width,
			height, pixel_format,
			GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN |
			GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE );
#endif

	if( (NULL == window->control.get()) || (window->control->isValid() == false))
	{
		ret = MALI_TPI_FALSE;
		goto exit;
	}

	/* Make the window visible at the requested layer */
	SurfaceComposerClient::openGlobalTransaction();
	window->control->setLayer(layer);
	window->control->show();
	SurfaceComposerClient::closeGlobalTransaction();

	/* Get the underlying Surface object */
	window->surface = window->control->getSurface();
	if(NULL == window->surface.get())
	{
		ret = MALI_TPI_FALSE;
		goto exit;
	}	

	/* Get the actual ANativeWindow object */
	window->win = window->surface.get();
	if(NULL == window->win)
	{
		ret = MALI_TPI_FALSE;
		goto exit;
	}
	
exit:
	return ret;
}

static mali_tpi_bool tpip_init(void)
{
	int i;
	mali_tpi_bool ret = MALI_TPI_FALSE;
	Surface::SurfaceInfo info;
	TPIDisplayInfo display_info;
	/* Open SurfaceFlinger connection */
	client = new SurfaceComposerClient();
	if(client->initCheck() != NO_ERROR)
	{
		client = NULL;
		ret = MALI_TPI_FALSE;
		return ret;	
	}
	
	/* Set-up window structure */
	for(i=0;i<MAX_WINDOW_COUNT;i++)
	{
		windows[i].win = NULL;
	}

	/* Create a background surface so that tests don't get interference from the UI */
    mali_tpi_get_display_info( &display_info );	
	ret = tpip_create_window(display_info.width, display_info.height ,
					PIXEL_FORMAT_RGBX_8888, WINDOW_LAYER_VALUE, &windows[0]);

	/* Make the window visible */
	if(MALI_TPI_TRUE == ret)
	{
		windows[0].surface->lock(&info);
		windows[0].surface->unlockAndPost();
	}

exit:
	return ret;
}
