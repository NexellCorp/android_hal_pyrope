/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#define _XOPEN_SOURCE 600 /* for setenv */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#elif _POSIX_C_SOURCE < 200112L
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
/* #define _POSIX_C_SOURCE 199309L */

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#if MALI_USE_UNIFIED_MEMORY_PROVIDER
#include <ump/ump.h>
#if !defined(UMP_VERSION_MAJOR) || UMP_VERSION_MAJOR == 1
#include <ump/ump_ref_drv.h>
#endif
#endif
#if MALI_USE_DMA_BUF
#include "../../base_api_tests/memory/mali_base_apitest_umm_telib.h"
#endif

#include <mali_system.h>
#include "egl_helpers.h"
#ifndef EXCLUDE_EGL_SPECIFICS
#include "EGL/fbdev_window.h"
#endif
#include <shared/mali_image.h>
/**
 * 'Invalid' native display
 */
#define INVALID_NATIVE_DPY_VALUE ((EGLNativeDisplayType)-1)

/**
 * Native dpys that should never be returned by egl_helper_get_native_display.
 * Mainly used to catch use-after-release.
 *
 * @note this is not the same as an INVALID display, which is a correct output
 * value of egl_helper_get_native_display
 */
#define BAD_NATIVE_DPY_VALUE ((EGLNativeDisplayType)-2)


#define ENVVAR "MALI_FBDEV" /* Environment variable queried for window position information */

#define SHELL_PROG "sh"
#define SHELL_PATH "/bin"
#define SHELL_ARG "-c"

/** Windowing abstraction state for FBDev
 * @note at present, this requires nothing other than the test_suite itself */
struct egl_helper_windowing_state_T
{
	#ifndef EXCLUDE_UNIT_FRAMEWORK
	suite *test_suite; /**< Test suite pointer, for making memory allocations and fails/passes */
	#else
	int dummy;
	#endif
};

#if !defined(EXCLUDE_UNIT_FRAMEWORK) && !defined(EXCLUDE_EGL_SPECIFICS)
/**
 * Wrapper for pixmap specifier.
 *
 * Contains additional private data not required by the platform-independent
 * pixmap functions. Created by egl_helper_nativepixmap_prepare_data and
 * destroyed by egl_helper_nativepixmap_release_data.
 */
typedef struct egl_helper_prepared_pixmap_handle_t
{
	pixmap_spec ps;             /**< Platform-independent pixmap specifier. 
	                                 Must be the first member! */
	EGLNativePixmapType pixmap; /**< For egl_helper_nativepixmap_release_data */
}
egl_helper_prepared_pixmap_handle_t;
#endif /* !defined(EXCLUDE_UNIT_FRAMEWORK) && !defined(EXCLUDE_EGL_SPECIFICS) */ 

/**
 * Display abstraction
 */
#ifndef EXCLUDE_EGL_SPECIFICS
EGLBoolean egl_helper_get_native_display( suite *test_suite, EGLNativeDisplayType *ret_native_dpy, egl_helper_dpy_flags flags )
{
	EGLNativeDisplayType nativedpy;
	EGL_HELPER_ASSERT_POINTER( ret_native_dpy );

	/* an invalid display, for testing purposes */
	if ( 0 != (flags & EGL_HELPER_DPY_FLAG_INVALID_DPY) )
	{
		nativedpy = INVALID_NATIVE_DPY_VALUE;
	}
	else
	{
		/* prepare a valid display */
		nativedpy = EGL_DEFAULT_DISPLAY;
	}

	*ret_native_dpy = nativedpy;

	return EGL_TRUE;
}

void egl_helper_release_native_display( suite *test_suite, EGLNativeDisplayType *inout_native_dpy )
{
	EGL_HELPER_ASSERT_POINTER( inout_native_dpy );
	EGL_HELPER_ASSERT_MSG( BAD_NATIVE_DPY_VALUE != *inout_native_dpy, ("egl_helper_release_native_display: Native display was not valid. Already released?") );

	/* No releasing need happen */

	*inout_native_dpy = (EGLNativeDisplayType)BAD_NATIVE_DPY_VALUE;
}
#endif /* EXCLUDE_EGL_SPECIFICS */


/**
 * Pixmap abstraction
 * @note - not ported to work without the unit framework
 */
#ifndef EXCLUDE_UNIT_FRAMEWORK 
#ifndef EXCLUDE_EGL_SPECIFICS

EGLBoolean egl_helper_nativepixmap_supported( display_format format )
{
    /* All pixmap formats are supported */
    return EGL_TRUE;
}

EGLBoolean egl_helper_create_nativepixmap_internal( suite *test_suite, u32 width, u32 height, display_format format, EGLBoolean ump, EGLNativePixmapType* out_pixmap )
{
	unsigned int size = 0;
	fbdev_pixmap *pixmap = (fbdev_pixmap*)_test_mempool_alloc( &test_suite->fixture_pool, sizeof( fbdev_pixmap ) );
	if ( NULL == pixmap )
	{
		warn( test_suite, dsprintf(test_suite, "egl_helper_create_nativepixmap_internal: unable to allocate memory for fbdev_pixmap" ) );
		return EGL_FALSE;
	}

	if ( NULL == out_pixmap)
	{
		warn( test_suite, dsprintf(test_suite, "egl_helper_create_nativepixmap_internal: out_pixmap is NULL" ) );
		return EGL_FALSE;
	}

	memset(pixmap, 0, sizeof( fbdev_pixmap ));
	pixmap->width = width;
	pixmap->height = height;
	pixmap->flags = FBDEV_PIXMAP_ALPHA_FORMAT_PRE | FBDEV_PIXMAP_COLORSPACE_sRGB;
	switch ((unsigned int)format)
	{
		case (unsigned int)DISPLAY_FORMAT_RGB565:
			pixmap->alpha_size = 0;
			pixmap->red_size = 5;
			pixmap->green_size = 6;
			pixmap->blue_size = 5;
			pixmap->luminance_size = 0;
			pixmap->buffer_size = pixmap->red_size + pixmap->green_size + pixmap->blue_size + pixmap->alpha_size + pixmap->luminance_size;
			pixmap->bytes_per_pixel = ((pixmap->buffer_size)+7)/8;
			break;
		case (unsigned int)DISPLAY_FORMAT_ARGB1555:
			pixmap->alpha_size = 1;
			pixmap->red_size = 5;
			pixmap->green_size = 5;
			pixmap->blue_size = 5;
			pixmap->luminance_size = 0;
			pixmap->buffer_size = pixmap->red_size + pixmap->green_size + pixmap->blue_size + pixmap->alpha_size + pixmap->luminance_size;
			pixmap->bytes_per_pixel = ((pixmap->buffer_size)+7)/8;
			break;
		case (unsigned int)DISPLAY_FORMAT_ARGB4444:
			pixmap->alpha_size = 4;
			pixmap->red_size = 4;
			pixmap->green_size = 4;
			pixmap->blue_size = 4;
			pixmap->luminance_size = 0;
			pixmap->buffer_size = pixmap->red_size + pixmap->green_size + pixmap->blue_size + pixmap->alpha_size + pixmap->luminance_size;
			pixmap->bytes_per_pixel = ((pixmap->buffer_size)+7)/8;
			break;
		case (unsigned int)DISPLAY_FORMAT_ARGB8888:
			pixmap->alpha_size = 8;
			pixmap->red_size = 8;
			pixmap->green_size = 8;
			pixmap->blue_size = 8;
			pixmap->luminance_size = 0;
			pixmap->buffer_size = pixmap->red_size + pixmap->green_size + pixmap->blue_size + pixmap->alpha_size + pixmap->luminance_size;
			pixmap->bytes_per_pixel = ((pixmap->buffer_size)+7)/8;
			break;
		case (unsigned int)DISPLAY_FORMAT_ARGB8885:
			pixmap->alpha_size = 8;
			pixmap->red_size = 8;
			pixmap->green_size = 8;
			pixmap->blue_size = 5;
			pixmap->luminance_size = 0;
			/* Bogus */
			pixmap->buffer_size = 32;
			pixmap->bytes_per_pixel = 32/8;
			break;
		case (unsigned int)DISPLAY_FORMAT_ARGB8880:
			pixmap->alpha_size = 8;
			pixmap->red_size = 8;
			pixmap->green_size = 8;
			pixmap->blue_size = 0;
			pixmap->luminance_size = 0;
			/* Bogus */
			pixmap->buffer_size = 32;
			pixmap->bytes_per_pixel = 32/8;
			break;
		case (unsigned int)DISPLAY_FORMAT_ARGB8808:
			pixmap->alpha_size = 8;
			pixmap->red_size = 8;
			pixmap->green_size = 0;
			pixmap->blue_size = 8;
			pixmap->luminance_size = 0;
			/* Bogus */
			pixmap->buffer_size = 32;
			pixmap->bytes_per_pixel = 32/8;
			break;
		case (unsigned int)DISPLAY_FORMAT_ARGB8088:
			pixmap->alpha_size = 8;
			pixmap->red_size = 0;
			pixmap->green_size = 8;
			pixmap->blue_size = 8;
			pixmap->luminance_size = 0;
			/* Bogus */
			pixmap->buffer_size = 32;
			pixmap->bytes_per_pixel = 32/8;
			break;
		case (unsigned int)DISPLAY_FORMAT_ARGB0888:
			pixmap->alpha_size = 0;
			pixmap->red_size = 8;
			pixmap->green_size = 8;
			pixmap->blue_size = 8;
			pixmap->luminance_size = 0;
			/* Bogus */
			pixmap->buffer_size = 32;
			pixmap->bytes_per_pixel = 32/8;
			break;
		case (unsigned int)DISPLAY_FORMAT_ARGB0000:
			pixmap->alpha_size = 8;
			pixmap->red_size = 8;
			pixmap->green_size = 8;
			pixmap->blue_size = 8;
			pixmap->luminance_size = 0;
			/* Bogus */
			pixmap->buffer_size = 0;
			pixmap->bytes_per_pixel = 32/8;
			break;

		case (unsigned int)DISPLAY_FORMAT_RGB560:
			pixmap->alpha_size = 0;
			pixmap->red_size = 5;
			pixmap->green_size = 6;
			pixmap->blue_size = 0;
			pixmap->luminance_size = 0;
			/* Bogus */
			pixmap->buffer_size = 16;
			pixmap->bytes_per_pixel = 16/8;
			break;
		case (unsigned int)DISPLAY_FORMAT_RGB505:
			pixmap->alpha_size = 0;
			pixmap->red_size = 5;
			pixmap->green_size = 0;
			pixmap->blue_size = 5;
			pixmap->luminance_size = 0;
			/* Bogus */
			pixmap->buffer_size = 16;
			pixmap->bytes_per_pixel = 16/8;
			break;
		case (unsigned int)DISPLAY_FORMAT_RGB065:
			pixmap->alpha_size = 0;
			pixmap->red_size = 0;
			pixmap->green_size = 6;
			pixmap->blue_size = 5;
			pixmap->luminance_size = 0;
			/* Bogus */
			pixmap->buffer_size = 16;
			pixmap->bytes_per_pixel = 16/8;
			break;
		case (unsigned int)DISPLAY_FORMAT_RGB000:
			pixmap->alpha_size = 0;
			pixmap->red_size = 5;
			pixmap->green_size = 6;
			pixmap->blue_size = 5;
			pixmap->luminance_size = 0;
			/* Bogus */
			pixmap->buffer_size = 0;
			pixmap->bytes_per_pixel = 16/8;
			break;
		case (unsigned int)DISPLAY_FORMAT_L8:
			pixmap->alpha_size = 0;
			pixmap->red_size = 0;
			pixmap->green_size = 0;
			pixmap->blue_size = 0;
			pixmap->luminance_size = 8;
			pixmap->buffer_size = pixmap->red_size + pixmap->green_size + pixmap->blue_size + pixmap->alpha_size + pixmap->luminance_size;
			pixmap->bytes_per_pixel = ((pixmap->buffer_size)+7)/8;
			break;
		case (unsigned int)DISPLAY_FORMAT_AL88:
			pixmap->alpha_size = 8;
			pixmap->red_size = 0;
			pixmap->green_size = 0;
			pixmap->blue_size = 0;
			pixmap->luminance_size = 8;
			pixmap->buffer_size = pixmap->red_size + pixmap->green_size + pixmap->blue_size + pixmap->alpha_size + pixmap->luminance_size;
			pixmap->bytes_per_pixel = ((pixmap->buffer_size)+7)/8;
			break;
		case (unsigned int)DISPLAY_FORMAT_A8:
			pixmap->alpha_size = 8;
			pixmap->red_size = 0;
			pixmap->green_size = 0;
			pixmap->blue_size = 0;
			pixmap->luminance_size = 0;
			pixmap->buffer_size = pixmap->red_size + pixmap->green_size + pixmap->blue_size + pixmap->alpha_size + pixmap->luminance_size;
			pixmap->bytes_per_pixel = ((pixmap->buffer_size)+7)/8;
			break;
#if MALI_EGL_IMAGE_SUPPORT_NATIVE_YUV
		case (unsigned int)EGL_YUV420P_KHR:
			pixmap->alpha_size = 0;
			pixmap->red_size = 0;
			pixmap->green_size = 0;
			pixmap->blue_size = 0;
			pixmap->luminance_size = 8;
			pixmap->buffer_size = pixmap->red_size + pixmap->green_size + pixmap->blue_size + pixmap->alpha_size + pixmap->luminance_size;
			pixmap->bytes_per_pixel = ((pixmap->buffer_size)+7)/8;
			pixmap->format = EGL_YUV420P_KHR;
		break;
		case (unsigned int)EGL_YUV420SP_KHR:
			pixmap->alpha_size = 0;
			pixmap->red_size = 0;
			pixmap->green_size = 0;
			pixmap->blue_size = 0;
			pixmap->luminance_size = 8;
			pixmap->buffer_size = pixmap->red_size + pixmap->green_size + pixmap->blue_size + pixmap->alpha_size + pixmap->luminance_size;
			pixmap->bytes_per_pixel = ((pixmap->buffer_size)+7)/8;
			pixmap->format = EGL_YUV420SP_KHR;
			break;
		case (unsigned int)EGL_YVU420SP_KHR:
			pixmap->alpha_size = 0;
			pixmap->red_size = 0;
			pixmap->green_size = 0;
			pixmap->blue_size = 0;
			pixmap->luminance_size = 8;
			pixmap->buffer_size = pixmap->red_size + pixmap->green_size + pixmap->blue_size + pixmap->alpha_size + pixmap->luminance_size;
			pixmap->bytes_per_pixel = ((pixmap->buffer_size)+7)/8;
			pixmap->format = EGL_YVU420SP_KHR;
		break;
		case (unsigned int)EGL_YV12_KHR:
			pixmap->alpha_size = 0;
			pixmap->red_size = 0;
			pixmap->green_size = 0;
			pixmap->blue_size = 0;
			pixmap->luminance_size = 8;
			pixmap->buffer_size = pixmap->red_size + pixmap->green_size + pixmap->blue_size + pixmap->alpha_size + pixmap->luminance_size;
			pixmap->bytes_per_pixel = ((pixmap->buffer_size)+7)/8;
			pixmap->format = EGL_YV12_KHR;
		break;
		#ifdef NEXELL_ANDROID_FEATURE_EGL_YV12_KHR_444_FORMAT
		case (unsigned int)EGL_YV12_KHR_444:
			pixmap->alpha_size = 0;
			pixmap->red_size = 0;
			pixmap->green_size = 0;
			pixmap->blue_size = 0;
			pixmap->luminance_size = 8;
			pixmap->buffer_size = pixmap->red_size + pixmap->green_size + pixmap->blue_size + pixmap->alpha_size + pixmap->luminance_size;
			pixmap->bytes_per_pixel = ((pixmap->buffer_size)+7)/8;
			pixmap->format = EGL_YV12_KHR_444;
		break;
		#endif	
#endif
		default:
			/* Unsupported - fail */
			warn( test_suite, dsprintf(test_suite, "egl_helper_create_pixmap: unsupported display format: %d",format) );
			return EGL_FALSE;
	}

#if MALI_EGL_IMAGE_SUPPORT_NATIVE_YUV
	#ifdef NEXELL_ANDROID_FEATURE_EGL_YV12_KHR_444_FORMAT
	if( (format >= EGL_YUV420P_KHR) && (format <= EGL_YV12_KHR_444) )
	#else
	if( (format >= EGL_YUV420P_KHR) && (format <= EGL_YV12_KHR) )
	#endif	
	{
		yuv_format_info* yuv_fmts = mali_image_get_yuv_info(format);
		EGL_HELPER_ASSERT_POINTER( yuv_fmts );
		/* total size for example YUV420P = (Y + U + V) * bytes_per_pixel
		 * Y:  WxH
		 * U: (W/2)x(H/2) (offset: WxH)
		 * V: (W/2)x(H/2) (offset: U offset + (W/2)x(H/2))
		 */
		size = (unsigned int) ((( pixmap->width * yuv_fmts->plane[0].width_scale ) * ( pixmap->height * yuv_fmts->plane[0].width_scale ))  /* Y */
				+ (( pixmap->width * yuv_fmts->plane[1].width_scale ) * ( pixmap->height * yuv_fmts->plane[1].width_scale ))   /* U */
				+ (( pixmap->width * yuv_fmts->plane[2].width_scale ) * ( pixmap->height * yuv_fmts->plane[2].width_scale )))  /* V */
				* pixmap->bytes_per_pixel;
	}
	else
#endif
	{
		size = pixmap->width * pixmap->height * pixmap->bytes_per_pixel;
	}

#if MALI_USE_DMA_BUF
	if ( EGL_TRUE == ump )
	{
		struct fbdev_dma_buf *buf = malloc(sizeof(struct fbdev_dma_buf));
		const int PAGE_SIZE = 4096;
		if (NULL == buf)
		{
			fail(test_suite, dsprintf(test_suite, "unable to allocate dma-buf test allocator") );
			return EGL_FALSE;
		}
		buf->size = MALI_ALIGN(size, PAGE_SIZE);
		buf->fd = umm_te_alloc(buf->size / PAGE_SIZE);

		if ( 0 >= buf->fd )
		{
			fail( test_suite, dsprintf(test_suite, "unable to allocate from dma-buf test allocator") );
			return EGL_FALSE;
		}
		pixmap->data = (unsigned short *)buf;
		pixmap->flags |= FBDEV_PIXMAP_DMA_BUF;
	}
	else
#elif MALI_USE_UNIFIED_MEMORY_PROVIDER
	if ( EGL_TRUE == ump )
	{
		ump_handle ump;
		if ( UMP_OK != ump_open() )
		{
			fail( test_suite, dsprintf(test_suite, "unable to open UMP interface") );
			return EGL_FALSE;
		}

		ump = ump_ref_drv_allocate( size, UMP_REF_DRV_CONSTRAINT_NONE );
		pixmap->data = (ump_handle)ump;
		if ( UMP_INVALID_MEMORY_HANDLE == pixmap->data )
		{
			fail( test_suite, dsprintf(test_suite, "unable to allocate from UMP interface") );
			ump_close();
			return EGL_FALSE;
		}
		pixmap->flags |= FBDEV_PIXMAP_SUPPORTS_UMP;
	}
	else
#endif /* MALI_USE_UNIFIED_MEMORY_PROVIDER */
	{
		pixmap->data = _test_mempool_alloc( &test_suite->fixture_pool, size );
		if ( NULL == pixmap->data )
		{
			warn( test_suite, dsprintf(test_suite, "egl_helper_create_nativepixmap_internal: unable to allocate memory for non-ump pixmap data" ) );
			return EGL_FALSE;
		}
	}

	*out_pixmap = (EGLNativePixmapType)pixmap;
	return EGL_TRUE;
}

EGLBoolean egl_helper_create_nativepixmap_extended( suite *test_suite, u32 width, u32 height, display_format format, EGLint renderable_type, EGLNativePixmapType* pixmap )
{
	MALI_IGNORE( renderable_type );
	return egl_helper_create_nativepixmap_internal( test_suite, width, height, format, EGL_FALSE, pixmap );
}

EGLBoolean egl_helper_create_nativepixmap_ump_extended( suite *test_suite, u32 width, u32 height, display_format format, EGLint renderable_type, EGLNativePixmapType* pixmap )
{
	MALI_IGNORE( renderable_type );
	return egl_helper_create_nativepixmap_internal( test_suite, width, height, format, EGL_TRUE, pixmap );
}

EGLBoolean egl_helper_valid_nativepixmap( EGLNativePixmapType pixmap )
{
    return pixmap != NULL ? EGL_TRUE : EGL_FALSE;
}

/** Zero for success, non-zero for failure */
int egl_helper_free_nativepixmap( EGLNativePixmapType pixmap )
{
	fbdev_pixmap *fbpixmap = (fbdev_pixmap*)pixmap;
	if ( NULL == fbpixmap )
	{
		return 1;
	}

	if ( FBDEV_PIXMAP_DMA_BUF & fbpixmap->flags )
	{
#if MALI_USE_DMA_BUF
		/* TODO: Add status check? */
		struct fbdev_dma_buf *buf = (struct fbdev_dma_buf*)fbpixmap->data;
		close(buf->fd);
#endif
	}
	else if ( FBDEV_PIXMAP_SUPPORTS_UMP & fbpixmap->flags )
	{
#if MALI_USE_UNIFIED_MEMORY_PROVIDER
		ump_reference_release( (ump_handle)fbpixmap->data );
		ump_close();
#endif
	}
	else
	{
		/* Would free the data here, but it's handled by the test mem pool */
	}

	/* Would free the pixmap here, but it's handled by the test mem pool */
	return 0;
}

u32 egl_helper_get_nativepixmap_width( EGLNativePixmapType pixmap )
{
	fbdev_pixmap *fbpixmap = (fbdev_pixmap*)pixmap;
	return fbpixmap->width;
}


u32 egl_helper_get_nativepixmap_height( EGLNativePixmapType pixmap )
{
	fbdev_pixmap *fbpixmap = (fbdev_pixmap*)pixmap;
	return fbpixmap->height;
}

u32 egl_helper_get_nativepixmap_bits_per_pixel( EGLNativePixmapType pixmap )
{
	fbdev_pixmap *fbpixmap = (fbdev_pixmap*)pixmap;
	return fbpixmap->buffer_size;
}

u32 egl_helper_get_nativepixmap_bytes_per_pixel( EGLNativePixmapType pixmap )
{
	fbdev_pixmap *fbpixmap = (fbdev_pixmap*)pixmap;
	return fbpixmap->bytes_per_pixel;
}

u32 egl_helper_get_nativepixmap_pitch( EGLNativePixmapType pixmap )
{
	fbdev_pixmap *fbpixmap = (fbdev_pixmap*)pixmap;
	return (fbpixmap->width * fbpixmap->buffer_size + 7)/8;
}


pixmap_spec *egl_helper_nativepixmap_prepare_data( suite *test_suite, EGLNativePixmapType pixmap )
{
	egl_helper_prepared_pixmap_handle_t *pixmap_info = NULL;
	fbdev_pixmap *fbpixmap = MALI_REINTERPRET_CAST(fbdev_pixmap*)pixmap;
	display_format format;

	EGL_HELPER_ASSERT_POINTER( test_suite );
	EGL_HELPER_ASSERT_POINTER( fbpixmap );
	
	format =  egl_helper_get_display_format(fbpixmap->red_size, fbpixmap->green_size, fbpixmap->blue_size, fbpixmap->alpha_size, fbpixmap->luminance_size);
	if (DISPLAY_FORMAT_INVALID != format)
	{
		void *mapped = egl_helper_nativepixmap_map_data(pixmap);
		if ( NULL != mapped )
		{
			pixmap_info = _test_mempool_alloc( &test_suite->fixture_pool, sizeof(*pixmap_info) );
			if ( NULL != pixmap_info )
			{
				/* Initialise pixmap specifier for platform-independent functions */
				pixmap_info->ps.test_suite = test_suite;
				pixmap_info->ps.format = format;
				pixmap_info->ps.pixels = mapped;
				pixmap_info->ps.pitch = (fbpixmap->width * fbpixmap->buffer_size + CHAR_BIT-1) / CHAR_BIT;
				pixmap_info->ps.width = fbpixmap->width;
				pixmap_info->ps.height = fbpixmap->height;
				pixmap_info->ps.alpha = EGL_TRUE; /* support non-maximal alpha values */

				/* Store pixmap handle for egl_helper_nativepixmap_release_data */
				pixmap_info->pixmap = pixmap;
			}
		}
	}

	/* Return a pointer to the platform-independent pixmap specifier,
	   or NULL on failure */
	return NULL == pixmap_info ? NULL : &pixmap_info->ps;
}

u16* egl_helper_nativepixmap_map_data( EGLNativePixmapType pixmap )
{
	fbdev_pixmap *fbpixmap = (fbdev_pixmap*)pixmap;

#if MALI_USE_DMA_BUF
	if ( FBDEV_PIXMAP_DMA_BUF & fbpixmap->flags )
	{
		struct fbdev_dma_buf *buf = (struct fbdev_dma_buf*)fbpixmap->data;
		u16 *data = NULL;
		buf->ptr = mmap(NULL, buf->size, PROT_READ|PROT_WRITE, MAP_SHARED, buf->fd, 0);
		data = (u16*)buf->ptr;
		if ( NULL == data ) fprintf(stderr, "%s : could not mmap dma-buf\n", MALI_FUNCTION);
		return data;
	}
#endif
#if MALI_USE_UNIFIED_MEMORY_PROVIDER
	if ( FBDEV_PIXMAP_SUPPORTS_UMP & fbpixmap->flags )
	{
		void *ptr = NULL;
		u16 *data = NULL;
		ptr = ump_mapped_pointer_get( (ump_handle)fbpixmap->data );
		data = (u16*)ptr;
		if ( NULL == data ) fprintf(stderr, "%s : ump_get_mapped_pointer returned NULL\n", MALI_FUNCTION);
		return data;
	}
#endif

	return fbpixmap->data;
}

void egl_helper_nativepixmap_unmap_data( EGLNativePixmapType pixmap )
{
#if MALI_USE_DMA_BUF
	fbdev_pixmap *fbpixmap = (fbdev_pixmap*)pixmap;
	if ( FBDEV_PIXMAP_DMA_BUF & fbpixmap->flags )
	{
		struct fbdev_dma_buf *buf = (struct fbdev_dma_buf*)fbpixmap->data;
		munmap(buf->ptr, buf->size);
	}
#endif
#if MALI_USE_UNIFIED_MEMORY_PROVIDER
	fbdev_pixmap *fbpixmap = (fbdev_pixmap*)pixmap;
	if ( FBDEV_PIXMAP_SUPPORTS_UMP & fbpixmap->flags )
	{
		ump_mapped_pointer_release( (ump_handle)fbpixmap->data );
	}
#endif
}

void egl_helper_nativepixmap_release_data( pixmap_spec *ps )
{
	egl_helper_prepared_pixmap_handle_t *pixmap_info = MALI_REINTERPRET_CAST(egl_helper_prepared_pixmap_handle_t*)ps;

	EGL_HELPER_ASSERT_POINTER( pixmap_info );
	egl_helper_nativepixmap_unmap_data( pixmap_info->pixmap );

}

EGLBoolean egl_helper_nativepixmap_directly_renderable( void )
{
    EGLBoolean ret = EGL_FALSE;
#if MALI_USE_UNIFIED_MEMORY_PROVIDER || MALI_USE_DMA_BUF
    ret =  EGL_TRUE;
#endif
    return ret;
}
#endif /* EXCLUDE_EGL_SPECIFICS */

/**
 * Process and threading abstraction
 */

/**
 * Parameter wrapper for threadproc wrapp
 */
typedef struct theadproc_param_wrapper_T
{
	egl_helper_threadproc *actual_threadproc;
	void *actual_param;
	suite * test_suite;
} threadproc_param_wrapper;

/**
 * Pthread Static wrapper for platform independant egl_helper_threadproc functions
 * A pointer to this function is of type void *(*)(void*).
 * Purpose is an example of how you would wrap it, and to ensure the type is all correct;
 */
static void* egl_helper_threadproc_wrapper( void *threadproc_params )
{
	threadproc_param_wrapper *params;
	void *retcode;

	EGL_HELPER_ASSERT_POINTER( threadproc_params );

	params = (threadproc_param_wrapper *)threadproc_params;

	retcode = (void*)params->actual_threadproc(params->actual_param);

	if ( NULL == params->test_suite )
	{
		/* we are not using test suite so we need to free allocated resources */
		FREE( params );
	}
	else
	{
		MALI_IGNORE( params->test_suite );
	}
	
	return retcode;
}

int egl_helper_create_process_simple()
{
	pid_t pid;

	pid = fork();

	/* The Parent - pid != 0, pid != -1 */
	return (int)pid;
}

void egl_helper_exit_process()
{
	exit(0);
}

int egl_helper_create_msgq( int key, int flags )
{
	flags |= IPC_CREAT;                  /* Create the message queue only if it does not exist already */
	return msgget((key_t) key, flags);
}

int egl_helper_msgsnd(int msgq_id, const void *from, u32 size, int flags)
{
	flags |= MSG_NOERROR;
	return msgsnd(msgq_id, from, size, flags);
}

u32 egl_helper_msgrcv(int msgq_id, void *to, u32 size, long msg_id, int flags)
{
	flags |= MSG_NOERROR;
	return msgrcv(msgq_id, to, size, msg_id, flags);
}

u32 egl_helper_create_process( suite *test_suite, char* executable, char* args, char* log_basename )
{
	pid_t pid;

	MALI_IGNORE( test_suite );

	pid = fork();

	if ( -1 == pid )
	{
		/* Error! */
		return 0;
	}

	/* The child */
	if ( 0 == pid )
	{
		char buffer[1024]; /* Could alloc on heap instead */
		char stdout_logname[256];
		char stderr_logname[256];

		mali_tpi_snprintf( stdout_logname, sizeof(stdout_logname)/sizeof(stdout_logname[0]), "%s.out", log_basename );
		mali_tpi_snprintf( stderr_logname, sizeof(stderr_logname)/sizeof(stderr_logname[0]), "%s.err", log_basename );

		/* Concatenate executable and args into a string, separated by a space. Assume in current dir */
		mali_tpi_snprintf( buffer, sizeof(buffer)/sizeof(buffer[0]), "./%s %s > %s 2> %s", executable, args, stdout_logname, stderr_logname );
		execl( SHELL_PATH "/" SHELL_PROG, SHELL_PROG, SHELL_ARG, buffer, NULL );

		mali_tpi_printf( "Error: Child process continued after execl()!\n" );
		exit(1);
	}
	/* The Parent - pid != 0, pid != -1 */

	return (u32)pid;
}

int egl_helper_wait_for_process( suite *test_suite, u32 proc_handle )
{
	pid_t pid;
	int exitcode;

	MALI_IGNORE( test_suite );

	if ( 0 == proc_handle )
	{
		/* Bad handle */
		return -1;
	}

	pid = (pid_t)proc_handle;

	/* Wait and get exitcode */
	if ( pid != waitpid( pid, &exitcode, 0 ) )
	{
		/* Some fault or signal delivered */
		return -1;
	}

	return exitcode;
}

u32 egl_helper_create_thread( suite *test_suite, egl_helper_threadproc *threadproc, void *start_param )
{
	threadproc_param_wrapper *param_wrapper;
	pthread_t thread_handle; /* Type unsigned long int, so can use as handles without backing with memory */
	int ret;
	MALI_IGNORE( test_suite );
	EGL_HELPER_ASSERT_POINTER( threadproc );

	/* If not using mempools, we free this inside the wrapper threadproc */
	if ( NULL == test_suite )
	{
		param_wrapper = (threadproc_param_wrapper*)MALLOC( sizeof( threadproc_param_wrapper ) );
	}
	else
	{
		param_wrapper = (threadproc_param_wrapper*)_test_mempool_alloc( &test_suite->fixture_pool, sizeof( threadproc_param_wrapper ) );
	}
	if ( NULL == param_wrapper ) return 0;

	param_wrapper->actual_threadproc = threadproc;
	param_wrapper->actual_param = start_param;
	param_wrapper->test_suite = test_suite;

	ret = pthread_create( &thread_handle, NULL, egl_helper_threadproc_wrapper, param_wrapper );

	if ( 0 != ret )
	{
		/* If not using mempools we need to cleanup on failure */
		if ( NULL == test_suite )
		{
			FREE( param_wrapper );
		}
		return 0;
	}

	return (u32)thread_handle;
}

u32 egl_helper_wait_for_thread( suite *test_suite, u32 thread_handle )
{
	pthread_t pthread_handle; /* Type unsigned long int, so can use as handles without backing with memory */
	void *exitcode = NULL;

	MALI_IGNORE( test_suite );

	pthread_handle = (pthread_t)thread_handle;

	if ( 0 != pthread_join( pthread_handle, &exitcode ) )
	{
		/* Failure */
		return (u32)-1;
	}

	return (u32)exitcode;
}

u32 * egl_helper_mutex_create( suite *test_suite )
{
	pthread_mutex_t * mutex;

	MALI_IGNORE( test_suite );

	mutex = MALLOC(sizeof(pthread_mutex_t));
	if(mutex == NULL) return NULL;

	if (0 != pthread_mutex_init(mutex, NULL))
	{
		FREE(mutex);
		return NULL;
	}
	else return (u32 *)mutex;
}

void egl_helper_mutex_destroy( suite *test_suite, u32 * mutex )
{
	int call_result;

	MALI_IGNORE( test_suite );

	call_result = pthread_mutex_destroy((pthread_mutex_t *)mutex);
	if( 0 != call_result) printf("Incorrect mutex use detected: pthread_mutex_destroy call failed with error code %d\n", call_result);
	FREE(mutex);
	mutex = NULL;
}

void egl_helper_mutex_lock( suite *test_suite, u32 * mutex )
{
	int call_result;

	MALI_IGNORE( test_suite );

	call_result = pthread_mutex_lock((pthread_mutex_t *)mutex);
	if( 0 != call_result) printf("Incorrect mutex use detected: pthread_mutex_lock call failed with error code %d\n", call_result);
}

void egl_helper_mutex_unlock(  suite *test_suite, u32 * mutex )
{
	int call_result;

	MALI_IGNORE( test_suite );

	call_result = pthread_mutex_unlock((pthread_mutex_t *)mutex);
	if( 0 != call_result) printf("Incorrect mutex use detected: pthread_mutex_unlock call failed with error code %d\n", call_result);
}

void egl_helper_usleep( u64 usec )
{
	while (usec > 999999ULL)
	{
		usleep(999999UL);
		
		if (usec >= 999999ULL )
		  usec -= 999999ULL;
		else
		  break;
	}

	usleep(usec);
}

u32 egl_helper_yield( void )
{
	int status;
	status = sched_yield();
	if (0 == status)
		return 0;
	else
		return 1;
}

/* Wrap some simple mutex and condition functions */

u32* egl_helper_barrier_init(suite *test_suite, u32 number_of_threads )
{
	pthread_barrier_t * barrier;
	u32* retval;
	int status;

	MALI_IGNORE( test_suite );

	barrier = MALLOC(sizeof(pthread_barrier_t));
	if(barrier == NULL) return NULL;
	status = pthread_barrier_init(barrier, NULL, number_of_threads);

	if (0 != status)
	{
		printf("Incorrect barrier use detected: pthread_barrier_init call failed with error code %d\n", status);
		FREE(barrier);
		retval = NULL;
	} else
	{
		retval = (u32*) barrier;
	}
	return retval;
}

void egl_helper_barrier_destroy( suite *test_suite, u32 * barrier )
{
	int status;

	MALI_IGNORE( test_suite );

	status = pthread_barrier_destroy((pthread_barrier_t *)barrier);
	if( 0 != status) printf("Incorrect barrier use detected: pthread_barrier_destroy call failed with error code %d\n", status);
	FREE(barrier);
	barrier = NULL;
}

void egl_helper_barrier_wait( suite *test_suite, u32*barrier )
{
	int status;

	MALI_IGNORE( test_suite );

	status = pthread_barrier_wait((pthread_barrier_t *)barrier);
	if( 0 != status && PTHREAD_BARRIER_SERIAL_THREAD != status)
		printf("Incorrect barrier use detected: pthread_barrier_wait call failed with error code %d\n", status);
}

#endif /* EXCLUDE_UNIT_FRAMEWORK */

#ifndef EXCLUDE_EGL_SPECIFICS
/**
 * Windowing abstraction
 */
EGLBoolean egl_helper_windowing_init( egl_helper_windowing_state **state, suite *test_suite )
{
	egl_helper_windowing_state *windowing_state;

	EGL_HELPER_ASSERT_POINTER( state );

#ifndef EXCLUDE_UNIT_FRAMEWORK
	if ( NULL != test_suite )
	{
		if( NULL != test_suite->fixture_pool.last_block )
		{
			windowing_state =( egl_helper_windowing_state*) _test_mempool_alloc( &test_suite->fixture_pool, sizeof( egl_helper_windowing_state ) );
		}
		else
		{
			/** @note this required for vg_api_tests, which don't set up the fixture pool until after EGL is setup */
			windowing_state =( egl_helper_windowing_state*) _test_mempool_alloc( test_suite->parent_pool, sizeof( egl_helper_windowing_state ) );
		}

		if ( NULL == windowing_state)
		{
			return EGL_FALSE;
		}
		windowing_state->test_suite = test_suite;
	}
	else
#endif
	{
		MALI_IGNORE( test_suite );
		windowing_state = MALLOC( sizeof( egl_helper_windowing_state ) );
		if ( NULL == windowing_state )
		{
			return EGL_FALSE;
		}
		MEMSET( windowing_state, 0, sizeof( egl_helper_windowing_state ) );
	}
	/* Add any other necessary state initialization here */

	/* Success - writeout */
	*state = windowing_state;

	return EGL_TRUE;
}

EGLBoolean egl_helper_windowing_deinit( egl_helper_windowing_state **state )
{
	egl_helper_windowing_state *windowing_state;
	EGL_HELPER_ASSERT_POINTER( state );

	windowing_state = *state;
	EGL_HELPER_ASSERT_POINTER( windowing_state );


	/* Add any necessary windowing state de-init here */

#ifndef EXCLUDE_UNIT_FRAMEWORK
	if ( NULL != windowing_state->test_suite )
	{
		/* No need to free windowing_state, since it is part of the fixture's memory pool */
		MALI_IGNORE( windowing_state );
	} 
	else
#endif
	{
		FREE( windowing_state );
	}

	/* Zero the state pointer */
	*state = NULL;

	return EGL_TRUE;
}

EGLBoolean egl_helper_windowing_set_quadrant( egl_helper_windowing_state *state, int quadrant )
{
	int ret;
	char str[3];
	EGL_HELPER_ASSERT_POINTER( state );

	EGL_HELPER_ASSERT_MSG( quadrant >= 0 && quadrant < 4, ("Bad quadrant to egl_helper_windowing_set_quadrant\n") );

	mali_tpi_snprintf( str, sizeof(str)/sizeof(str[0]), "Q%d", quadrant+1 ); /**< In FBDev, quadrants start from 1 */
	ret = setenv( ENVVAR, str, 1);

	if ( 0 != ret )
	{
		return EGL_FALSE;
	}

	return EGL_TRUE;
}

EGLBoolean egl_helper_create_window( egl_helper_windowing_state *state, u32 width, u32 height, EGLNativeWindowType *win )
{
	EGLBoolean success = EGL_FALSE;
	fbdev_window *fbwin;

	EGL_HELPER_ASSERT_POINTER( state );

#ifndef EXCLUDE_UNIT_FRAMEWORK
	if ( NULL != state->test_suite )
	{
		suite *test_suite;
		test_suite = state->test_suite;
		EGL_HELPER_ASSERT_POINTER( test_suite );

		if( NULL != test_suite->fixture_pool.last_block )
		{
			fbwin = (fbdev_window*)_test_mempool_alloc( &test_suite->fixture_pool, sizeof( fbdev_window ) );
		}
		else
		{
			/** @note this required for vg_api_tests, which don't set up the fixture pool until after EGL is setup */
			fbwin = (fbdev_window*)_test_mempool_alloc( test_suite->parent_pool, sizeof( fbdev_window ) );
		}
	}
	else
#endif
	{
		MALI_IGNORE( state );
		fbwin = (fbdev_window*)MALLOC( sizeof( fbdev_window ) );
	}

	if ( NULL != fbwin )
	{
		fbwin->width = width;
		fbwin->height = height;
		success = EGL_TRUE;
	}

	/* For egl_helper_destroy_window, we ensure that fbwin is set to NULL on
	 * failure. This would allow us to cleanup properly, should we need to do
	 * our own cleanup. */

	*win = fbwin; /* No cast necessary - EGLNativeWindowType must be an fbdev_window here */

	return success;
}

EGLBoolean egl_helper_destroy_window( egl_helper_windowing_state *state, EGLNativeWindowType win )
{
	EGL_HELPER_ASSERT_POINTER( state );

	/* state not used at present, but presented for future expansion */
	MALI_IGNORE( state );

	/* If egl_helper_create_window failed, then win will be NULL, and we should
	 * not report any further error. */
	if ( NULL != win )
	{
#ifndef EXCLUDE_UNIT_FRAMEWORK
		if ( NULL != state->test_suite )
		{
			/* No need to free windowing_state or win, since it is part of the fixture's memory pool */

			/* @note This turns out to be identical to invalidate_window. However, in a
			 * system where we don't have auto-freeing mempools, we would really free
			 * the memory */
			win->width = 0;
			win->height = 0;
		}
		else
#endif
		{
			FREE( win );
		}
	}

	return EGL_TRUE;
}

EGLBoolean egl_helper_create_invalid_window( egl_helper_windowing_state *state, EGLNativeWindowType *win )
{
	EGLBoolean success = EGL_FALSE;
	fbdev_window* fbwin;

	EGL_HELPER_ASSERT_POINTER( state );
#ifndef EXCLUDE_UNIT_FRAMEWORK
	if ( NULL != state->test_suite )
	{
		suite *test_suite;
		test_suite = state->test_suite;
		EGL_HELPER_ASSERT_POINTER( test_suite );

		if( NULL != test_suite->fixture_pool.last_block )
		{
			fbwin = (fbdev_window*)_test_mempool_alloc( &test_suite->fixture_pool, sizeof( fbdev_window ) );
		}
		else
		{
			/** @note this required for vg_api_tests, which don't set up the fixture pool until after EGL is setup */
			fbwin = (fbdev_window*)_test_mempool_alloc( test_suite->parent_pool, sizeof( fbdev_window ) );
		}
	}
	else
#endif
	{
		MALI_IGNORE( state );
		fbwin = (fbdev_window*)MALLOC( sizeof( fbdev_window ) );
	}

	if ( NULL != fbwin )
	{
		fbwin->width = 0;
		fbwin->height = 0;
		success = EGL_TRUE;
	}

	/* For egl_helper_destroy_window, we ensure that fbwin is set to NULL on
	 * failure. This would allow us to cleanup properly, should we need to do
	 * our own cleanup. */

	*win = fbwin; /* No cast necessary - EGLNativeWindowType must be an fbdev_window here */

	return success;
}

EGLBoolean egl_helper_invalidate_window( egl_helper_windowing_state *state, EGLNativeWindowType win )
{
	EGL_HELPER_ASSERT_POINTER( state );

	/* state not used at present, but presented for future expansion */
	MALI_IGNORE( state );

	win->width = 0;
	win->height = 0;

	return EGL_TRUE;
}

u32 egl_helper_get_window_width( egl_helper_windowing_state *state, EGLNativeWindowType win )
{
	EGL_HELPER_ASSERT_POINTER( state );

	/* state not used at present, but presented for future expansion */
	MALI_IGNORE( state );

	if ( NULL != win )
	{
		return win->width;
	}

	return 0;
}

u32 egl_helper_get_window_height( egl_helper_windowing_state *state, EGLNativeWindowType win )
{
	EGL_HELPER_ASSERT_POINTER( state );

	/* state not used at present, but presented for future expansion */
	MALI_IGNORE( state );

	if ( NULL != win )
	{
		return win->height;
	}

	return 0;
}

EGLBoolean egl_helper_resize_window( egl_helper_windowing_state *state, u32 width, u32 height, EGLNativeWindowType win )
{
	EGLBoolean success = EGL_FALSE;

	EGL_HELPER_ASSERT_POINTER( state );

	/* state not used at present, but presented for future expansion */
	MALI_IGNORE( state );

	if ( NULL != win )
	{
		win->width = width;
		win->height = height;
		success = EGL_TRUE;
	}

	return success;
}
#endif /* EXCLUDE_EGL_SPECIFICS */

FILE *egl_helper_fopen(const char *filename, const char *mode)
{
	return fopen( filename, mode );
}

u32 egl_helper_fread(void *buf, u32 size, u32 count, FILE *fp)
{
	return fread(buf, size, count, fp);
}

/* NOTE: egl_helper_physical_memory_get/release are solely provided for the mf14 memory test in the base_api_main_suite
 *       they are not meant for any other purpose at this time.
 */
u32 egl_helper_physical_memory_get( suite *test_suite, void **mapped_pointer, u32 size, u32 *phys_addr )
{
	s32 fb_dev;
	void * mapped_buffer = NULL;

    EGL_HELPER_ASSERT_POINTER(test_suite);
    EGL_HELPER_ASSERT_POINTER(mapped_pointer);
    EGL_HELPER_ASSERT_POINTER(phys_addr);

	/* open the fb device */
	fb_dev = open( "/dev/fb0", O_RDWR );
	if(fb_dev >= 0)
	{
		int ioctl_result;
		struct fb_fix_screeninfo fix_info;
		struct fb_var_screeninfo var_info;
        u32 fb_size;

		ioctl_result = ioctl(fb_dev, FBIOGET_VSCREENINFO, &var_info);
        if (-1 == ioctl_result)
        {
            mali_tpi_printf("egl_helper_physical_memory_get: Could not query variable screen info\n");
            goto cleanup;
        }

		ioctl_result = ioctl(fb_dev, FBIOGET_FSCREENINFO, &fix_info);
        if (-1 == ioctl_result)
        {
            mali_tpi_printf("egl_helper_physical_memory_get: Could not query fixed screen info\n");
            goto cleanup;
        }

		if (var_info.yres_virtual < (2 * var_info.yres))
        {
            mali_tpi_printf("egl_helper_physical_memory_get: Test failure due to non-compatible direct rendering system\n");
            goto cleanup;
        }

		fb_size = (size_t)( var_info.xres * var_info.yres_virtual * ( var_info.bits_per_pixel>>3 ));
        if (size > fb_size)
        {
            mali_tpi_printf("egl_helper_physical_memory_get: frame buffer not large enough to cater for requested size\n");
            goto cleanup;
        }

		mapped_buffer = mmap((void*)0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fb_dev, 0);
        if (NULL != mapped_buffer)
        {
            *phys_addr = fix_info.smem_start;
        }
        else
        {
            mali_tpi_printf("egl_helper_physical_memory_get: mmap failed\n");
            goto cleanup;
        }
    }
    else
    {
        mali_tpi_printf("egl_helper_physical_memory_get: Could not open fbdev\n");
    }

cleanup:
    *mapped_pointer = mapped_buffer;
    if (fb_dev >= 0)
    {
		(void)close( fb_dev );
    }
    return 0; /* cookie not used */
}

/* NOTE: egl_helper_physical_memory_get/release are solely provided for the mf14 memory test in the base_api_main_suite
 *       they are not meant for any other purpose at this time.
 */
void egl_helper_physical_memory_release( suite *test_suite, void *mapped_pointer, u32 size, u32 cookie )
{
    EGL_HELPER_ASSERT_POINTER(test_suite);
    EGL_HELPER_ASSERT_POINTER(mapped_pointer);
    MALI_IGNORE(cookie);
	munmap(mapped_pointer, size);
}

unsigned long long egl_helper_get_ms(void)
{
	struct timeval tval;
	gettimeofday(&tval,NULL);
	return tval.tv_sec*1000000 + tval.tv_usec;
}

int egl_helper_initialize()
{
#if MALI_USE_DMA_BUF
	umm_te_init();
#endif
	return 0;
}

void egl_helper_terminate()
{
#if MALI_USE_DMA_BUF
	umm_te_term();
#endif
}
