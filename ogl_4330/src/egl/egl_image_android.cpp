/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <utils/Log.h>

#include "egl_config_extension.h"
#if EGL_ANDROID_image_native_buffer_ENABLE
#include <mali_system.h>
#include "egl_image_android.h"
#include "egl_thread.h"
#include "egl_misc.h"
#include "egl_config.h"
#include "egl_main.h"
#include "gles_context.h"
#include "shared/mali_surface.h"
#include "shared/mali_shared_mem_ref.h"
#include "shared/mali_egl_image.h"

#include <linux/ioctl.h>

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
#include "egl_pixmap_mapping_anative.h"

#if MALI_ANDROID_API < 15
using namespace android;
#endif

#if 0 /* nexell add */
int __g_is_pid_system_surfaceflinger_exist = -1;
int __g_system_surfaceflinger_pid = 0;
#endif

struct mali_image* _egl_android_map_native_buffer_yuv( android_native_buffer_t *buffer )
{
	android_native_buffer_t *native_buffer = (android_native_buffer_t *) buffer;
	private_handle_t *hnd = (private_handle_t *) native_buffer->handle;
	egl_android_pixel_format *format_mapping = NULL;
	mali_surface_specifier sformat;
	mali_image *image = NULL;
	mali_mem_handle mem_handle = MALI_NO_HANDLE;
	mali_base_ctx_handle base_ctx;
	m200_texture_addressing_mode texel_layout = M200_TEXTURE_ADDRESSING_MODE_LINEAR;
	u32 width = 0, height = 0, pitch = 0;
	__egl_main_context *egl = __egl_get_main_context();

	if (NULL == hnd) return NULL;

	base_ctx = egl->base_ctx;

	width = buffer->width;
	height = buffer->height;
	pitch = buffer->stride;

	format_mapping = _egl_android_get_native_buffer_format( buffer );
	MALI_CHECK_NON_NULL( format_mapping, NULL );

	if ( !hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP )
	{
		MALI_DEBUG_ASSERT(MALI_FALSE, ("Unsupported buffer type. Need UMP support for this to work\n"));
		return NULL;
	}

	_mali_surface_specifier_ex( &sformat, 
					width,
					height,
					pitch,
					MALI_PIXEL_FORMAT_NONE, 
					format_mapping->egl_pixel_format,
					MALI_PIXEL_LAYOUT_LINEAR, 
					texel_layout, 
					format_mapping->red_blue_swap, 
					format_mapping->reverse_order, 
					MALI_SURFACE_COLORSPACE_lRGB, 
					MALI_SURFACE_ALPHAFORMAT_NONPRE, 
					MALI_FALSE );


	image = mali_image_create( 1 /* miplevels */, MALI_SURFACE_FLAG_DONT_MOVE, &sformat, format_mapping->yuv_format, base_ctx );
	if ( NULL == image )
	{
		AERR( "Unable to allocate memory for YUV image (%i x %i %i)\n", width, height, format_mapping->yuv_format );
		return NULL;
	}

	/* wrap the given native buffer into mali memory suitable for mali_image */
	switch ( format_mapping->yuv_format )
	{
#if GRALLOC_ARM_UMP_MODULE
		case EGL_YVU420SP_KHR:
		{
			/* semi planar format, separate buffers for Y and same buffer for U,V */
			u32 ofs_y = hnd->offset, ofs_uv = 0;
			ofs_uv = buffer->stride * buffer->height + hnd->offset;


			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 0, 0, ofs_y, (void*)hnd->ump_mem_handle ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 3, 0, ofs_uv, (void*)hnd->ump_mem_handle ) ) goto cleanup;

			break;
		}
		case EGL_YV12_KHR:
		{
			/* planar format, separate buffers for Y,U,V */
			u32 ofs_y = hnd->offset, ofs_u = 0, ofs_v = 0;

			ofs_v = buffer->stride * buffer->height + hnd->offset;
			ofs_u = ofs_v + MALI_ALIGN(buffer->stride/2, 16) * buffer->height/2;

			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 0, 0, ofs_y, (void*)hnd->ump_mem_handle ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 1, 0, ofs_u, (void*)hnd->ump_mem_handle ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 2, 0, ofs_v, (void*)hnd->ump_mem_handle ) ) goto cleanup;

			break;
		}
		#ifdef NEXELL_ANDROID_FEATURE_EGL_YV12_KHR_444_FORMAT
		#ifdef NEXELL_ANDROID_FEATURE_EGL_TEMP_YUV444_FORMAT
		case EGL_YV12_KHR_444:
		{
			/* planar format, separate buffers for Y,U,V */
			u32 ofs_y = hnd->offset, ofs_u = 0, ofs_v = 0;

			ofs_u = buffer->stride * buffer->height + hnd->offset;
			ofs_v = ofs_u + buffer->stride * buffer->height;

			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 0, 0, ofs_y, (void*)hnd->ump_mem_handle ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 1, 0, ofs_u, (void*)hnd->ump_mem_handle ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 2, 0, ofs_v, (void*)hnd->ump_mem_handle ) ) goto cleanup;

			break;
		}
		#else
		case EGL_YV12_KHR_444:
		{
			/* planar format, separate buffers for Y,U,V */
			u32 ofs_y = hnd->offset, ofs_u = 0, ofs_v = 0;

			ofs_v = buffer->stride * buffer->height + hnd->offset;
			ofs_u = ofs_v + MALI_ALIGN(buffer->stride, 16) * buffer->height/2;

			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 0, 0, ofs_y, (void*)hnd->ump_mem_handle ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 1, 0, ofs_u, (void*)hnd->ump_mem_handle ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 2, 0, ofs_v, (void*)hnd->ump_mem_handle ) ) goto cleanup;

			break;
		}
		#endif
		#endif
#elif GRALLOC_ARM_DMA_BUF_MODULE
		case EGL_YVU420SP_KHR:
		{
			/* semi planar format, separate buffers for Y and same buffer for U,V */
			u32 ofs_y = hnd->offset, ofs_uv = 0;
			ofs_uv = buffer->stride * buffer->height + hnd->offset;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 0, 0, ofs_y, (void*)hnd->share_fd ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 3, 0, ofs_uv, (void*)hnd->share_fd ) ) goto cleanup;
#if DMA_BUF_ON_DEMAND
			image->pixel_buffer[0][0]->flags |=MALI_SURFACE_FLAG_TRACK_SURFACE;
			image->pixel_buffer[3][0]->flags |=MALI_SURFACE_FLAG_TRACK_SURFACE;
#endif
			break;
		}
		case EGL_YV12_KHR:
		{
#if defined( NEXELL_ANDROID_FEATURE_YUV_SEPERATE_FD ) /* nexell add */
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 0, 0, 0, (void*)hnd->share_fds[0] ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 1, 0, 0, (void*)hnd->share_fds[1] ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 2, 0, 0, (void*)hnd->share_fds[2] ) ) goto cleanup;
#else /* org */
			/* planar format, separate buffers for Y,U,V */
			u32 ofs_y = hnd->offset, ofs_u = 0, ofs_v = 0;

			ofs_v = buffer->stride * buffer->height + hnd->offset;
			ofs_u = ofs_v + MALI_ALIGN(buffer->stride/2, 16) * buffer->height/2;

			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 0, 0, ofs_y, (void*)hnd->share_fd ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 1, 0, ofs_u, (void*)hnd->share_fd ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 2, 0, ofs_v, (void*)hnd->share_fd ) ) goto cleanup;
#endif

#if DMA_BUF_ON_DEMAND
			image->pixel_buffer[0][0]->flags |=MALI_SURFACE_FLAG_TRACK_SURFACE;
			image->pixel_buffer[1][0]->flags |=MALI_SURFACE_FLAG_TRACK_SURFACE;
			image->pixel_buffer[2][0]->flags |=MALI_SURFACE_FLAG_TRACK_SURFACE;
#endif
			break;
		}
		#ifdef NEXELL_ANDROID_FEATURE_EGL_YV12_KHR_444_FORMAT
		#ifdef NEXELL_ANDROID_FEATURE_EGL_TEMP_YUV444_FORMAT
		case EGL_YV12_KHR_444:
		{
#if defined( NEXELL_ANDROID_FEATURE_YUV_SEPERATE_FD ) /* nexell add */
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 0, 0, 0, (void*)hnd->share_fds[0] ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 1, 0, 0, (void*)hnd->share_fds[1] ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 2, 0, 0, (void*)hnd->share_fds[2] ) ) goto cleanup;
#else /* org */
			/* planar format, separate buffers for Y,U,V */
			u32 ofs_y = hnd->offset, ofs_u = 0, ofs_v = 0;

			ofs_u = buffer->stride * buffer->height + hnd->offset;
			ofs_v = ofs_u + buffer->stride * buffer->height;

			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 0, 0, ofs_y, (void*)hnd->share_fd ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 1, 0, ofs_u, (void*)hnd->share_fd ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 2, 0, ofs_v, (void*)hnd->share_fd ) ) goto cleanup;
#endif

#if DMA_BUF_ON_DEMAND
			image->pixel_buffer[0][0]->flags |=MALI_SURFACE_FLAG_TRACK_SURFACE;
			image->pixel_buffer[1][0]->flags |=MALI_SURFACE_FLAG_TRACK_SURFACE;
			image->pixel_buffer[2][0]->flags |=MALI_SURFACE_FLAG_TRACK_SURFACE;
#endif
			break;
		}
		#else
		case EGL_YV12_KHR_444:
		{
#if defined( NEXELL_ANDROID_FEATURE_YUV_SEPERATE_FD ) /* nexell add */
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 0, 0, 0, (void*)hnd->share_fds[0] ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 1, 0, 0, (void*)hnd->share_fds[1] ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 2, 0, 0, (void*)hnd->share_fds[2] ) ) goto cleanup;
#else /* org */
			/* planar format, separate buffers for Y,U,V */
			u32 ofs_y = hnd->offset, ofs_u = 0, ofs_v = 0;

			ofs_v = buffer->stride * buffer->height + hnd->offset;
			ofs_u = ofs_v + MALI_ALIGN(buffer->stride, 16) * buffer->height/2;

			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 0, 0, ofs_y, (void*)hnd->share_fd ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 1, 0, ofs_u, (void*)hnd->share_fd ) ) goto cleanup;
			if ( MALI_IMAGE_ERR_NO_ERROR != mali_image_set_data( image, 2, 0, ofs_v, (void*)hnd->share_fd ) ) goto cleanup;
#endif

#if DMA_BUF_ON_DEMAND
			image->pixel_buffer[0][0]->flags |=MALI_SURFACE_FLAG_TRACK_SURFACE;
			image->pixel_buffer[1][0]->flags |=MALI_SURFACE_FLAG_TRACK_SURFACE;
			image->pixel_buffer[2][0]->flags |=MALI_SURFACE_FLAG_TRACK_SURFACE;
#endif
			break;
		}
		#endif
		#endif

		#else
		AERR( "Can't set data for image: %p (unsupported memory type)", image );
#endif
	}

	return image;

cleanup:
	AERR( "Unable to set data for YUV image (format: %d)", format_mapping->yuv_format );
	mali_image_deref( image );

	return NULL;
}

struct mali_image* _egl_android_map_native_buffer_rgb( android_native_buffer_t *buffer )
{
	android_native_buffer_t *native_buffer = (android_native_buffer_t *) buffer;
	private_handle_t *hnd = (private_handle_t *) native_buffer->handle;
	egl_android_pixel_format *format_mapping = NULL;
	mali_surface_specifier sformat;
	mali_image *image = NULL;
	mali_mem_handle mem_handle = MALI_NO_HANDLE;
	mali_base_ctx_handle base_ctx;
	m200_texture_addressing_mode texel_layout = M200_TEXTURE_ADDRESSING_MODE_LINEAR;
	u32 bytes_per_pixel = 0;
	u32 width = 0, height = 0;
	u32 page_size = sysconf(_SC_PAGESIZE);
	void *vaddr = NULL;
	__egl_main_context *egl = __egl_get_main_context();
	unsigned long phys = 0;
	
	if (NULL == hnd)
	{
		return NULL;
	}

	base_ctx = egl->base_ctx;

	vaddr = (void *) hnd->base;

	width = buffer->width;
	height = buffer->height;

	format_mapping = _egl_android_get_native_buffer_format( buffer );
	MALI_CHECK_NON_NULL( format_mapping, NULL );

	bytes_per_pixel = __m200_texel_format_get_size( format_mapping->egl_pixel_format );

	if ( hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER )
	{
		u32 fb_buffer_size = hnd->size+hnd->offset;
		/* page-align size */
		fb_buffer_size = (fb_buffer_size + (page_size-1)) & ~(page_size-1);

		struct fb_fix_screeninfo fix_info;
		(void)ioctl( hnd->fd, FBIOGET_FSCREENINFO, &fix_info );
		phys = fix_info.smem_start;
		mem_handle = _mali_mem_add_phys_mem( base_ctx, phys, fb_buffer_size, (void *)((int)vaddr - hnd->offset), MALI_PP_WRITE | MALI_PP_READ | MALI_CPU_READ | MALI_CPU_WRITE );
		MALI_DEBUG_CODE(
				if ( MALI_NO_HANDLE == mem_handle )
				{
					ADBG( 1, "Failed to map physical memory (phys: 0x%x virt: 0x%x size: %u bytes (%i x %i x %i) - fallback to copy\n", (unsigned int)phys, (unsigned int)vaddr, fb_buffer_size, width, height, bytes_per_pixel );
				}
			);
	}
	else if ( hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP )
	{
#if GRALLOC_ARM_UMP_MODULE != 0
		mem_handle = _mali_mem_wrap_ump_memory( base_ctx, (ump_handle)hnd->ump_mem_handle, 0 );
		MALI_DEBUG_CODE(
				if ( MALI_NO_HANDLE == mem_handle )
				{
					ADBG( 1, "Failed to use UMP memory for handle %d - fallback to copy\n", (ump_handle)hnd->ump_mem_handle );
				}
			);
#else
		AERR( "Unable to use UMP buffer from hnd: 0x%x, no UMP support built into Gralloc", (unsigned int)hnd );
#endif
	}
	else if ( hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION )
	{
#if GRALLOC_ARM_DMA_BUF_MODULE != 0
		mem_handle = _mali_mem_wrap_dma_buf(base_ctx, hnd->share_fd, 0);
		MALI_DEBUG_CODE(
				if ( MALI_NO_HANDLE == mem_handle )
				{
				ADBG( 1, "Failed to use DMA memory - fallback to copy\n", NULL );
				}
				);
#else
		AERR( "Unable to use DMA buffer from hnd: 0x%x, no DMA support built into Gralloc", (unsigned int)hnd );
#endif
	}
	else
	{
		MALI_DEBUG_ASSERT(MALI_FALSE, ("Unsupported buffer type. Please add support for UMP. See Integration Guide for steps on how to do this."));
		phys = 0;
		return NULL;
	}

	_mali_surface_specifier_ex( 
			&sformat, 
			width,
			height,
			buffer->stride*bytes_per_pixel,
			_mali_texel_to_pixel_format(format_mapping->egl_pixel_format), 
			format_mapping->egl_pixel_format,
			_mali_texel_layout_to_pixel_layout(texel_layout), 
			texel_layout, 
			format_mapping->red_blue_swap, 
			format_mapping->reverse_order, 
			MALI_SURFACE_COLORSPACE_sRGB, 
			MALI_SURFACE_ALPHAFORMAT_NONPRE, 
			MALI_FALSE );

	if ( MALI_NO_HANDLE != mem_handle )
	{
		image = mali_image_create_from_ump_or_mali_memory( 
				MALI_SURFACE_FLAG_DONT_MOVE, 
				&sformat, 
				MALI_IMAGE_MALI_MEM, 
				mem_handle, 
				hnd->offset, 
				base_ctx );

		if ( NULL == image ) 
		{
			AERR( "Unable to allocate memory for EGLImage (%i x %i %i)", width, height, format_mapping->egl_pixel_format );
			return NULL;
		}
		image->pixel_buffer[0][0]->mem_offset = hnd->offset;
		image->pixel_buffer[0][0]->format.pitch = buffer->stride*bytes_per_pixel;
		image->pixel_buffer[0][0]->datasize = _mali_mem_size_get( image->pixel_buffer[0][0]->mem_ref->mali_memory ) - hnd->offset;
#if GRALLOC_ARM_DMA_BUF_MODULE != 0 && DMA_BUF_ON_DEMAND
		image->pixel_buffer[0][0]->flags |= MALI_SURFACE_FLAG_TRACK_SURFACE;
#endif
		
	}
	else
	{
		ADBG( 3, "*** using CPU memory for EGLImage (%i x %i %i) ***", width, height, format_mapping->egl_pixel_format );
		image = mali_image_create_from_external_memory( width, height, MALI_SURFACE_FLAG_DONT_MOVE, &sformat, NULL, MALI_IMAGE_CPU_MEM, base_ctx );
		if ( NULL == image ) 
		{
			AERR("unable to allocate memory for EGLImage backed by CPU (%i x %i)", width, height );
			return NULL;
		}
	}
	MALI_CHECK_NON_NULL( image, NULL );

	if ( MALI_NO_HANDLE == mem_handle )
	{
		ADBG( 3, "*** going through copy pass for CPU memory (%i x %i x %i) ***\n", width, height, bytes_per_pixel);
		if ( bytes_per_pixel <= 2 )
		{
			_mali_mem_write( image->pixel_buffer[0][0]->mem_ref->mali_memory, 0, vaddr, width*height*bytes_per_pixel );
		}
		else if ( bytes_per_pixel == 4 )
		{
			_mali_mem_write( image->pixel_buffer[0][0]->mem_ref->mali_memory, 0, (unsigned long*)((unsigned long)vaddr), width*height*bytes_per_pixel );
		}
	}
	
	return image;
}

struct mali_image* _egl_android_map_native_buffer( android_native_buffer_t *buffer )
{
	egl_android_pixel_format *format_mapping = NULL;
	mali_image *image = NULL;

	format_mapping = _egl_android_get_native_buffer_format( buffer );
	MALI_CHECK_NON_NULL( format_mapping, NULL );

	if ( 0 != format_mapping->yuv_format ) image = _egl_android_map_native_buffer_yuv( buffer );
	else image = _egl_android_map_native_buffer_rgb( buffer );

	return image;
}


egl_image* _egl_create_image_ANDROID_native_buffer( egl_display *display, 
                                                    egl_context *context, 
                                                    EGLClientBuffer buffer, 
                                                    EGLint *attr_list, 
                                                    void *thread_state )
{
	__egl_thread_state *tstate = (__egl_thread_state *)thread_state;
	EGLint *attrib = (EGLint*)attr_list;
	egl_image *image = NULL;
	mali_bool done = MALI_FALSE;
	EGLint error = EGL_SUCCESS; 
	__egl_main_context *egl = __egl_get_main_context();
	android_native_buffer_t *native_buffer = (android_native_buffer_t *) buffer;

	if ( MALI200_MAX_TEXTURE_SIZE < native_buffer->width || MALI200_MAX_TEXTURE_SIZE < native_buffer->height )
	{
		ADBG( 2, "Native_buffer dimension (%d,%d) is too large\n", native_buffer->width, native_buffer->height );
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return NULL;
	}

	/* verify that it is a valid android native buffer */
	if ( ANDROID_NATIVE_BUFFER_MAGIC != native_buffer->common.magic )
	{
		ADBG( 2, "magic number incorrect (%d vs %d)\n", ANDROID_NATIVE_BUFFER_MAGIC, native_buffer->common.magic );
		__egl_set_error( EGL_BAD_PARAMETER, tstate );
		return NULL;
	}

	if ( sizeof( android_native_buffer_t ) != native_buffer->common.version )
	{
		ADBG( 2, "structure version number mismatch (%d vs %d)\n", sizeof(android_native_buffer_t), native_buffer->common.version );
		__egl_set_error( EGL_BAD_PARAMETER, tstate );
		return NULL;
	}

	if ( NULL != attr_list )
	{
		while ( done != MALI_TRUE )
		{
			switch ( attrib[0] )
			{
				case EGL_IMAGE_PRESERVED_KHR:
					break;

				case EGL_NONE: done = MALI_TRUE; break;
				default:
					ADBG( 3, "bad attribute in list (0x%x)\n", attrib[0] );
					__egl_set_error( EGL_BAD_PARAMETER, tstate );
					return NULL;
			}
			attrib += 2;
		}
	}

	image = _egl_create_image();
	if ( NULL == image)
	{
		AERR("out of memory allocating %i x %i sized EGLImage\n", native_buffer->width, native_buffer->height );
		__egl_set_error(EGL_BAD_ALLOC, tstate);
		return NULL;
	}

	image->image_mali = _egl_android_map_native_buffer( native_buffer );
	if ( NULL == image->image_mali )
	{
		ADBG( 1, "failed to map native buffer %d\n", native_buffer );
		_egl_destroy_image( image, EGL_TRUE );
		__egl_set_error( EGL_BAD_ALLOC, tstate );
		return NULL;
	}

	/* nexell add */
	#if 0
	if(image->image_mali->width == 1920)
	{
		image->;
		ADBG( 3, "====> %dx%d, test_addr(0x%x), addr(0x%x), offset(0x%x)\n", image->image_mali->width, image->image_mali->height, (unsigned int)(image->image_mali->pixel_buffer[0][0].mem_ref.mali_memory.cpu_address) + image->image_mali->pixel_buffer[0][0].mem_offset,
			       image->image_mali->pixel_buffer[0][0].mem_ref.mali_memory.cpu_address, image->image_mali->pixel_buffer[0][0].mem_offset);
	}
	#endif

	image->is_pixmap = EGL_TRUE;
	image->buffer = buffer;

#if MALI_EGL_IMAGE_SUPPORT_NATIVE_YUV
	_egl_image_set_default_properties( image->prop );
#endif

	MALI_DEBUG_ASSERT_POINTER( native_buffer->common.incRef );

	/* the EGLImage now has an additional reference to the native buffer */
	native_buffer->common.incRef( &native_buffer->common );

	return image;
}

void _egl_image_unmap_ANDROID_native_buffer( EGLClientBuffer buffer )
{
	android_native_buffer_t *native_buffer = (android_native_buffer_t *) buffer;

	MALI_DEBUG_ASSERT_POINTER( native_buffer->common.decRef );
	native_buffer->common.decRef( &native_buffer->common );
}

#endif /* #if EGL_ANDROID_native_buffer_ENABLE */
