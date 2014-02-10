/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <regs/MALI200/mali200_core.h>
#include <shared/mali_image.h>
#include <shared/mali_named_list.h>
#include <base/mali_dependency_system.h>

/**
 * @brief The YUV format list
 */
#if MALI_EGL_IMAGE_SUPPORT_NATIVE_YUV
MALI_STATIC yuv_format_info _yuv_formats[] = {
	/**** Planar YUV formats ****/
	{
		EGL_YUV420P_KHR,
		3,
		{
			{ MALI_TRUE,  1.0f,           1.0f, 1.0f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   Y */
			{ MALI_TRUE,  1.0f/2.0f, 1.0f/2.0f, 0.5f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   U */
			{ MALI_TRUE,  1.0f/2.0f, 1.0f/2.0f, 0.5f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   V */
			{ MALI_FALSE, 0.0f,           0.0f, 0.0f, M200_TEXEL_FORMAT_NO_TEXTURE, MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*  UV */
			{ MALI_FALSE, 0.0f,           0.0f, 0.0f, M200_TEXEL_FORMAT_NO_TEXTURE, MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }  /* YUV */
		},
	},
	{
		EGL_YV12_KHR,
		3,
		{
			{ MALI_TRUE,  1.0f,           1.0f, 1.0f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   Y */
			{ MALI_TRUE,  1.0f/2.0f, 1.0f/2.0f, 0.5f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   V */
			{ MALI_TRUE,  1.0f/2.0f, 1.0f/2.0f, 0.5f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   U */
			{ MALI_FALSE, 0.0f,           0.0f, 0.0f, M200_TEXEL_FORMAT_NO_TEXTURE, MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*  UV */
			{ MALI_FALSE, 0.0f,           0.0f, 0.0f, M200_TEXEL_FORMAT_NO_TEXTURE, MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }  /* YUV */
		},
	},
	#ifdef NEXELL_ANDROID_FEATURE_EGL_YV12_KHR_444_FORMAT
	#ifdef NEXELL_ANDROID_FEATURE_EGL_TEMP_YUV444_FORMAT
	{
		EGL_YV12_KHR_444,
		3,
		{
			{ MALI_TRUE,  1.0f,           1.0f, 1.0f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   Y */
			{ MALI_TRUE,  1.0f,           1.0f, 1.0f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   U */
			{ MALI_TRUE,  1.0f,           1.0f, 1.0f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   V */
			{ MALI_FALSE, 0.0f,           0.0f, 0.0f, M200_TEXEL_FORMAT_NO_TEXTURE, MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*  UV */
			{ MALI_FALSE, 0.0f,           0.0f, 0.0f, M200_TEXEL_FORMAT_NO_TEXTURE, MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }  /* YUV */
		},
	},
	#else
	{
		EGL_YV12_KHR_444,
		3,
		{
			{ MALI_TRUE,  1.0f,           1.0f, 1.0f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   Y */
			{ MALI_TRUE,  1.0f/2.0f, 1.0f/2.0f, 1.0f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   V */
			{ MALI_TRUE,  1.0f/2.0f, 1.0f/2.0f, 1.0f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   U */
			{ MALI_FALSE, 0.0f,           0.0f, 0.0f, M200_TEXEL_FORMAT_NO_TEXTURE, MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*  UV */
			{ MALI_FALSE, 0.0f,           0.0f, 0.0f, M200_TEXEL_FORMAT_NO_TEXTURE, MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }  /* YUV */
		},
	},
	#endif
	#endif
	{
		EGL_YUV422P_KHR,
		3,
		{
			{ MALI_TRUE,  1.0f,           1.0f, 1.0f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   Y */
			{ MALI_TRUE,  1.0f/2.0f,      1.0f, 0.5f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   U */
			{ MALI_TRUE,  1.0f/2.0f,      1.0f, 0.5f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   V */
			{ MALI_FALSE, 0.0f,           0.0f, 0.0f, M200_TEXEL_FORMAT_NO_TEXTURE, MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*  UV */
			{ MALI_FALSE, 0.0f,           0.0f, 0.0f, M200_TEXEL_FORMAT_NO_TEXTURE, MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }  /* YUV */
		},
	},
	{
		EGL_YUV444P_KHR,
		3,
		{
			{ MALI_TRUE,  1.0f,           1.0f, 1.0f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   Y */
			{ MALI_TRUE,  1.0f,           1.0f, 1.0f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   U */
			{ MALI_TRUE,  1.0f,           1.0f, 1.0f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   V */
			{ MALI_FALSE, 0.0f,           0.0f, 0.0f, M200_TEXEL_FORMAT_NO_TEXTURE, MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*  UV */
			{ MALI_FALSE, 0.0f,           0.0f, 0.0f, M200_TEXEL_FORMAT_NO_TEXTURE, MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }  /* YUV */
		},
	},


	/**** SemiPlanar YUV formats ****/
	{
		EGL_YUV420SP_KHR,
		2,
		{
			{ MALI_TRUE,  1.0f,           1.0f, 1.0f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   Y            */
			{ MALI_TRUE,  1.0f/2.0f, 1.0f/2.0f, 1.0f, M200_TEXEL_FORMAT_AL_88,      MALI_FALSE, MALI_FALSE,  3 }, 						/*   U aliases UV */
			{ MALI_TRUE,  1.0f/2.0f, 1.0f/2.0f, 1.0f, M200_TEXEL_FORMAT_AL_88,      MALI_TRUE,  MALI_FALSE,  3 }, 						/*   V aliases UV */
			{ MALI_TRUE,  1.0f/2.0f, 1.0f/2.0f, 0.0f, M200_TEXEL_FORMAT_AL_88,      MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*  UV            */
			{ MALI_FALSE, 0.0f,           0.0f, 0.0f, M200_TEXEL_FORMAT_NO_TEXTURE, MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }  /* YUV            */
		},
	},
	{
		EGL_YVU420SP_KHR,
		2,
		{
			{ MALI_TRUE,  1.0f,           1.0f, 1.0f, M200_TEXEL_FORMAT_L_8,        MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*   Y            */
			{ MALI_TRUE,  1.0f/2.0f, 1.0f/2.0f, 1.0f, M200_TEXEL_FORMAT_AL_88,      MALI_TRUE,  MALI_FALSE,  3 }, 						/*   U aliases UV */
			{ MALI_TRUE,  1.0f/2.0f, 1.0f/2.0f, 1.0f, M200_TEXEL_FORMAT_AL_88,      MALI_FALSE, MALI_FALSE,  3 }, 						/*   V aliases UV */
			{ MALI_TRUE,  1.0f/2.0f, 1.0f/2.0f, 0.0f, M200_TEXEL_FORMAT_AL_88,      MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*  UV            */
			{ MALI_FALSE, 0.0f,           0.0f, 0.0f, M200_TEXEL_FORMAT_NO_TEXTURE, MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }  /* YUV            */
		},
	},


	/**** Interleaved YUV formats ****/
	{
		EGL_YUV422I_KHR,
		1,
		{
			{ MALI_TRUE,  1.0f,           1.0f, 2.0f, M200_TEXEL_FORMAT_AL_88,      MALI_FALSE, MALI_FALSE,  4 }, 						/*   Y aliases YUV */
			{ MALI_TRUE,  1.0f/2.0f,      1.0f, 2.0f, M200_TEXEL_FORMAT_ARGB_8888,  MALI_FALSE, MALI_TRUE,   4 }, 						/*   U aliases YUV */
			{ MALI_TRUE,  1.0f/2.0f,      1.0f, 2.0f, M200_TEXEL_FORMAT_ARGB_8888,  MALI_TRUE,  MALI_FALSE,  4 }, 						/*   V aliases YUV */
			{ MALI_FALSE, 0.0f,           0.0f, 0.0f, M200_TEXEL_FORMAT_NO_TEXTURE, MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*  UV             */
			{ MALI_TRUE,  1.0f/2.0f,      1.0f, 0.0f, M200_TEXEL_FORMAT_ARGB_8888,  MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }  /* YUV             */
		},
	},
#if !RGB_IS_XRGB
	{
		EGL_YUV444I_KHR,
		1,
		{
			{ MALI_TRUE,  1.0f,           1.0f, 3.0f, M200_TEXEL_FORMAT_RGB_888,    MALI_FALSE, MALI_TRUE,   4 }, 						/*   Y aliases YUV */
			{ MALI_TRUE,  1.0f,           1.0f, 3.0f, M200_TEXEL_FORMAT_RGB_888,    MALI_FALSE, MALI_FALSE,  4 }, 						/*   U aliases YUV */
			{ MALI_TRUE,  1.0f,           1.0f, 3.0f, M200_TEXEL_FORMAT_RGB_888,    MALI_FALSE, MALI_TRUE,   4 }, 						/*   V aliases YUV */
			{ MALI_FALSE, 0.0f,           0.0f, 0.0f, M200_TEXEL_FORMAT_NO_TEXTURE, MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }, /*  UV             */
			{ MALI_TRUE,  1.0f,           1.0f, 0.0f, M200_TEXEL_FORMAT_RGB_888,    MALI_FALSE, MALI_FALSE, MALI_IMAGE_PLANE_INVALID }  /* YUV             */
		},
	}
#endif
};
#endif /* MALI_EGL_IMAGE_SUPPORT_NATIVE_YUV */

MALI_EXPORT yuv_format_info* mali_image_get_yuv_info( u32 yuv_format )
{
#if MALI_EGL_IMAGE_SUPPORT_NATIVE_YUV
	u32 i;

	for ( i = 0; i < sizeof(_yuv_formats)/sizeof(_yuv_formats[0]); i++ )
	{
		if ( yuv_format == _yuv_formats[i].format )
		{
			return &_yuv_formats[i];
		}
	}
#endif /* MALI_EGL_IMAGE_SUPPORT_NATIVE_YUV */

	MALI_IGNORE(yuv_format);

	return NULL;
}

MALI_EXPORT mali_bool mali_image_supported_yuv_format( u32 yuv_format )
{
	if ( NULL == mali_image_get_yuv_info( yuv_format ) ) return MALI_FALSE;

	return MALI_TRUE;
}

/**
 * @brief Updates a mali_surface_specifier pointer with format according to yuv format specifications for given plane
 * @param image pointer to mali image
 * @param plane the plane to extract format for
 * @param sformat pointer to mali surface format structure
 */
MALI_EXPORT void mali_image_set_plane_format( mali_image *image, u32 plane, mali_surface_specifier *sformat )
{
	MALI_DEBUG_ASSERT_POINTER( image );
	MALI_DEBUG_ASSERT_POINTER( sformat );

	if ( NULL != image->yuv_info )
	{
		sformat->texel_format = image->yuv_info->plane[plane].texel_format;
		sformat->red_blue_swap = image->yuv_info->plane[plane].red_blue_swap;
		sformat->reverse_order = image->yuv_info->plane[plane].reverse_order;
	}
}

/**
 * @brief Retrieves the buffer size of a specified plane and miplevel
 * @param image pointer to mali image
 * @param plane the plane to retrieve buffer size of
 * @param miplevel the miplevel to retrieve buffer size of
 * @param width pointer to storage of width
 * @param height pointer to storage of height
 * @return MALI_TRUE on success, MALI_FALSE on failure
 */
MALI_EXPORT mali_bool mali_image_get_buffer_size( mali_image *image, u32 plane, u32 miplevel, u16 *width, u16 *height )
{
	MALI_DEBUG_ASSERT_POINTER( image );
	MALI_DEBUG_ASSERT_POINTER( width );
	MALI_DEBUG_ASSERT_POINTER( height );

	*width = 0;
	*height = 0;

	/* YUV formats may have different sizes per plane */
	if ( NULL != image->yuv_info )
	{
		if ( MALI_TRUE == image->yuv_info->plane[plane].active )
		{
			*width = (u16)(image->yuv_info->plane[plane].width_scale*image->width);
			*height = (u16)(image->yuv_info->plane[plane].height_scale*image->height);
		}
	}
	else
	{
		/* RGB images only have the first plane active */
		if ( 0 == plane )
		{
			*width = image->width;
			*height = image->height;
		}
	}

	if ( (0 == *width) || (0 == *height) ) return MALI_FALSE; /* buffer does not exist */

	/* scale width and height according to mipmap level */
	if ( miplevel > 0 )
	{
		EGLint mip_width, mip_height;
		EGLint div = (EGLint)_mali_sys_pow( 2, (float)miplevel );

		mip_width  = MAX( (*width)  / div, 1);
		mip_height = MAX( (*height) / div, 1);

		*width  = mip_width;
		*height = mip_height;
	}

	return MALI_TRUE;
}

MALI_STATIC void mali_image_surface_destroy_callback( mali_surface* surface, enum mali_surface_event event, void *mem_ref, void* data )
{
	mali_image* image = MALI_STATIC_CAST(mali_image*)data;

	MALI_IGNORE( surface );
	MALI_IGNORE( event );
	MALI_IGNORE( mem_ref );
	MALI_DEBUG_ASSERT_POINTER( image );

	mali_image_deref( image );
}

/**
 * @brief Will set up the aliasing of mem refs for the given buffer
 * @param image pointer to mali image
 * @param plane the plane to update aliasing for
 * @param miplevel the miplevel to update aliasing for
 */
MALI_STATIC void mali_image_update_aliased_buffers( mali_image *image, u32 plane, u32 miplevel )
{
	s32 i;
	mali_surface *aliased_surface = NULL;

	MALI_DEBUG_ASSERT_POINTER( image );

	/* only YUV formats will aliased buffers */
	if ( NULL == image->yuv_info ) return;

	aliased_surface = image->pixel_buffer[plane][miplevel];

	MALI_DEBUG_ASSERT_POINTER( aliased_surface );

	for ( i = 0; i < MALI_IMAGE_MAX_PLANES; i++ )
	{
		if ( image->yuv_info->plane[i].plane_alias == plane )
		{
			if(aliased_surface->mem_ref) _mali_shared_mem_ref_owner_addref(aliased_surface->mem_ref);
			if(image->pixel_buffer[i][miplevel]->mem_ref) _mali_shared_mem_ref_owner_deref(image->pixel_buffer[i][miplevel]->mem_ref);
			image->pixel_buffer[i][miplevel]->mem_ref = aliased_surface->mem_ref;
			MALI_DEBUG_PRINT( 2, ("MALI_IMAGE: alised buffer at plane %i for image 0x%X at plane %i, miplevel %i\n", plane, image, i, miplevel) );
		}
	}
}

MALI_EXPORT mali_bool mali_image_allocate_buffer( mali_image *image, u32 plane, u32 miplevel )
{
	mali_surface *surface = NULL;

	MALI_DEBUG_ASSERT_POINTER( image );
	MALI_DEBUG_ASSERT_POINTER( image->pixel_buffer[plane][miplevel] );

	surface = image->pixel_buffer[plane][miplevel];
	if ( NULL == surface->mem_ref )
	{
		if ( NULL == image->yuv_info || MALI_IMAGE_PLANE_INVALID == image->yuv_info->plane[plane].plane_alias )
		{
			int actual_datasize = surface->datasize;
#ifdef HARDWARE_ISSUE_9427
			mali_surface_specifier* format = &surface->format;
			/* Hardware writes outside the given memory area; increase size of memory area in cases where this can happen */
			if( format->pixel_format != MALI_PIXEL_FORMAT_NONE && /* writeable */
				format->pixel_layout == MALI_PIXEL_LAYOUT_LINEAR &&  /* linear */
				((format->width % 16) != 0) ) /* width not divisible by 16 */
			{
				actual_datasize += HARDWARE_ISSUE_9427_EXTRA_BYTES;
			}
#endif
			actual_datasize += surface->mem_offset;

			surface->mem_ref = _mali_shared_mem_ref_alloc_mem( image->base_ctx, actual_datasize, MALI200_SURFACE_ALIGNMENT,  MALI_PP_READ | MALI_CPU_READ | MALI_CPU_WRITE );
			if ( NULL == surface->mem_ref ) return MALI_FALSE;

			MALI_DEBUG_PRINT( 2, ("MALI_IMAGE: allocated pixel buffer for image 0x%X at plane %i, miplevel %i (%i x %i) size: %i bytes\n", image, plane, miplevel, surface->format.width, surface->format.height, actual_datasize) );

			mali_image_update_aliased_buffers( image, plane, miplevel );
		}
	}

	return MALI_TRUE;
}

MALI_EXPORT mali_bool mali_image_allocate_buffers( mali_image *image )
{
	u16 i, j;

	for ( i=0; i<MALI_IMAGE_MAX_PLANES; i++ )
	{
		for ( j=0; j<MALI_IMAGE_MAX_MIPLEVELS; j++ )
		{
			if ( NULL != image->pixel_buffer[i][j] )
			{
				if ( MALI_FALSE == mali_image_allocate_buffer( image, i, j ) ) return MALI_FALSE;
			}
		}
	}

	return EGL_TRUE;
}

/**
 * @brief Allocate memory for a Mali image
 *
 * Allocates memory for a mali_image and its associated list of current
 * locks. Initialises members of the structure to sensible default values
 * and sets its array of pixel buffer pointers to NULL. Intended for use
 * by other constructors.
 *
 * @param[in] width     Width of the image, in pixels.
 * @param[in] height    Height of the image, in pixels.
 * @param[in] mode      Mode of image creation (e.g. from an existing
 *                      surface or external memory).
 * @param[in] base_ctx  Mali base context handle.
 * @return  Pointer to a mali_image if successful or NULL if a memory
 *          allocation failed.
 */
MALI_STATIC mali_image *mali_image_alloc( u32 width, u32 height,
                                          mali_image_creation_mode mode,
                                          mali_base_ctx_handle base_ctx )
{
	mali_image *image = _mali_sys_calloc( 1, sizeof( *image ) );
	if ( NULL != image )
	{
		image->locklist = __mali_named_list_allocate();
		if ( NULL == image->locklist )
		{
			_mali_sys_free( image );
			image = NULL;
		}
		else
		{
			image->width = width;
			image->height = height;
			image->miplevels = 1;
			image->creation_mode = mode;
			image->base_ctx = base_ctx;

			_mali_sys_atomic_set( &image->references, 1 );
			_mali_sys_atomic_set( &image->mei_locked, 0);
		}
	}
	return image;
}

MALI_EXPORT mali_image* mali_image_create( u32 miplevels, enum mali_surface_flags flags, mali_surface_specifier *sformat,
                                           u32 yuv_format, mali_base_ctx_handle base_ctx )
{
	u32 plane, miplevel;
	u16 surface_width = 0, surface_height = 0, surface_pitch = 0;
	mali_image *image;

	image = mali_image_alloc( sformat->width, sformat->height, MALI_IMAGE_CREATED_IMPLICIT, base_ctx );
	MALI_CHECK_NON_NULL( image, NULL );

	image->miplevels = miplevels;
	image->yuv_info = mali_image_get_yuv_info( yuv_format );

	/* initialize the buffers */
	for ( plane = 0; plane < MALI_IMAGE_MAX_PLANES; plane++ )
	{
		/* get format specifications for current plane */
		mali_image_set_plane_format( image, plane, sformat );

		for ( miplevel = 0; miplevel < image->miplevels; miplevel++ )
		{
			mali_surface_specifier sf;
			mali_surface* surface;
			
			sf = *sformat;

			if ( MALI_FALSE == mali_image_get_buffer_size( image, plane, miplevel, &surface_width, &surface_height ) ) continue;

			surface_pitch = sf.pitch;
			if(NULL != image->yuv_info)
			{
				surface_pitch = (u16)(image->yuv_info->plane[plane].pitch_scale*sf.pitch);;
#ifdef HAVE_ANDROID_OS
				surface_pitch = MALI_ALIGN( surface_pitch, 16 );
#endif
			}

			
			sf.width  = surface_width;
			sf.height = surface_height;
			sf.pitch  = surface_pitch;

			surface = _mali_surface_alloc_empty( flags, & sf, base_ctx );
			if ( NULL == surface )
			{
				/* deref previously allocated surfaces */
				mali_image_deref_surfaces( image );
				/* Refcount is currently 1. 0 will free */
				mali_image_deref( image );
				return NULL;
			}
			image->pixel_buffer[plane][miplevel] = surface;
			_mali_sys_atomic_inc( &image->references );
			_mali_surface_set_event_callback( surface, MALI_SURFACE_EVENT_DESTROY, mali_image_surface_destroy_callback, MALI_STATIC_CAST(void*)image);
			
			MALI_DEBUG_PRINT(2, ("MALI_IMAGE: activated pixel buffer for image 0x%X at plane %i, miplevel %i (%i x %i) format 0x%x\n", image, plane, miplevel, surface_width, surface_height, sformat->texel_format) );
		}
	}

	mali_image_allocate_buffers(image);

	return image;
}

MALI_EXPORT mali_image* mali_image_create_from_surface( mali_surface *surface, mali_base_ctx_handle base_ctx )
{
	mali_image *image;

	MALI_DEBUG_ASSERT_POINTER( surface );

	/* RGB images will have only one miplevel, and no YUV info attached */
	image = mali_image_alloc( surface->format.width, surface->format.height,
	                          MALI_IMAGE_CREATED_FROM_SURFACE, base_ctx );
	if ( NULL != image )
	{
		/* First plane, first miplevel is always used for RGB images */
		image->pixel_buffer[0][0] = surface;
		_mali_sys_atomic_inc( &image->references );
		_mali_surface_set_event_callback( surface, MALI_SURFACE_EVENT_DESTROY, mali_image_surface_destroy_callback, MALI_STATIC_CAST(void*)image);
	}

	return image;
}

/**
 * @brief Checks if surface is mapped (ie. has a locked area)
 * @param image pointer to image
 * @param surface pointer to surface
 * @return MALI_TRUE if mapped, MALI_FALSE else
 */
MALI_EXPORT mali_bool mali_image_surface_is_mapped( mali_image *image, mali_surface *surface )
{
	u32 iterator;
	mali_image_lock_session *lock_session = NULL;

	MALI_DEBUG_ASSERT_POINTER( image );
	MALI_DEBUG_ASSERT_POINTER( surface );

	__mali_named_list_lock( image->locklist );
	lock_session = __mali_named_list_iterate_begin( image->locklist, &iterator );
	while ( NULL != lock_session )
	{
		if ( surface == lock_session->surface ) return MALI_TRUE;
		lock_session = __mali_named_list_iterate_next( image->locklist, &iterator );
	}
	__mali_named_list_unlock( image->locklist );

	return MALI_FALSE;
}

MALI_EXPORT mali_image_err_code mali_image_set_data( mali_image *image, u32 plane, u32 miplevel, u32 offset, void *data )
{
	mali_surface *surface = NULL;
	mali_mem_handle mem_handle = MALI_NO_HANDLE;
	u32 address, aligned_address;

	if ( NULL == data ) return MALI_IMAGE_ERR_BAD_PARAMETER;

	surface = mali_image_get_buffer( image, plane, miplevel, MALI_TRUE );
	MALI_CHECK_NON_NULL( surface, MALI_IMAGE_ERR_BAD_BUFFER );

	/* verify that image is not currently mapped */
	if ( MALI_TRUE == mali_image_surface_is_mapped( image, surface ) ) return MALI_IMAGE_ERR_BAD_ACCESS_MODE;

	/* verify that surface is not in use */
	if ( NULL != surface->mem_ref )
	{
		if ( _mali_shared_mem_ref_get_usage_ref_count( surface->mem_ref ) > 0 )
		{
			return MALI_IMAGE_ERR_IN_USE;
		}
	}

	/* wrap the memory into mali mem and update image data */
#if MALI_USE_UNIFIED_MEMORY_PROVIDER
	mem_handle = _mali_mem_wrap_ump_memory( image->base_ctx, (ump_handle)data, offset );

	if( MALI_NO_HANDLE != mem_handle )
	{
		/* _mali_mem_wrap_ump_memory increases the ump ref count, so it will work all fine to free
		 * the mali memory no matter if we are dealing with host or external memory
		 */
		if ( NULL != surface->mem_ref )
		{
			_mali_mem_free( surface->mem_ref->mali_memory );
			surface->mem_ref->mali_memory = mem_handle;
		}
		else
		{
			surface->mem_ref = _mali_shared_mem_ref_alloc_existing_mem( mem_handle );
		}
	}
#elif MALI_USE_DMA_BUF
	/*TODO: waiting for base to provide an interface to accept offset as param[in]*/
	mem_handle = _mali_mem_wrap_dma_buf( image->base_ctx, (int)data, offset);
   
	if( MALI_NO_HANDLE != mem_handle )
	{
		/* _mali_mem_wrap_ump_memory increases the ump ref count, so it will work all fine to free
		 * the mali memory no matter if we are dealing with host or external memory
		 */
		if ( NULL != surface->mem_ref )
		{
			_mali_mem_free( surface->mem_ref->mali_memory );
			surface->mem_ref->mali_memory = mem_handle;
		}
		else
		{
			surface->mem_ref = _mali_shared_mem_ref_alloc_existing_mem( mem_handle );
		}
	}
#else /* MALI_USE_UNIFIED_MEMORY_PROVIDER */
	MALI_IGNORE( offset );
	MALI_DEBUG_PRINT( 0, ("MALI_IMAGE: UMP is not enabled, can't wrap memory\n") );
#endif

	if ( MALI_NO_HANDLE == mem_handle ) return MALI_IMAGE_ERR_BAD_PARAMETER;

	/*adjust the mali memory address to 64byte aligned*/
	address = _mali_mem_mali_addr_get( surface->mem_ref->mali_memory, surface->mem_offset );
	aligned_address = address  & ~(MALI200_SURFACE_ALIGNMENT -1 );
	MALI_DEBUG_ASSERT( (address >= aligned_address), ("memory alignment error !"));
	MALI_DEBUG_ASSERT( (offset >= ( address-aligned_address)), ("invalid memory access!")); 

	MALI_DEBUG_PRINT(2,("surface->offset originally set to %x, address at 0x%x, datasize 0x%x", surface->mem_offset, address, surface->datasize));
	/*adjust surface->offset so that mali memory will always start at aligned address.*/
	surface->mem_ref->mali_memory->mali_address -= ( address - aligned_address );
	MALI_DEBUG_PRINT(2,("surface->offset now set to %x, address now at 0x%x, datasize is now 0x%x", surface->mem_offset, aligned_address, surface->datasize));

	/* update the aliases */
	{
		s32 i, j;
		for ( i=0; i<MALI_IMAGE_MAX_PLANES; i++ )
		{
			for ( j=0; j<MALI_IMAGE_MAX_MIPLEVELS; j++ )
			{
				if ( NULL != image->pixel_buffer[i][j] )
				{
					mali_image_update_aliased_buffers(image, i, j);
				}
			}
		}
	}

	return MALI_IMAGE_ERR_NO_ERROR;
}

MALI_EXPORT struct mali_image *mali_image_create_from_cpu_memory(
                                   enum mali_surface_flags flags,
                                   const mali_surface_specifier *sformat,
                                   mali_base_ctx_handle base_ctx )
{
	mali_image *image;

	MALI_DEBUG_ASSERT_POINTER( sformat );
	MALI_DEBUG_ASSERT_POINTER( base_ctx );

	image = mali_image_alloc( sformat->width, sformat->height, MALI_IMAGE_CREATED_FROM_EXTMEM, base_ctx );
	if ( NULL != image )
	{
		/* Allocate memory for a new surface with the required properties */
		mali_surface *surface = _mali_surface_alloc( flags, sformat, 0, base_ctx );
		if ( NULL == surface )
		{
			/* Failed to create a Mali surface, so destroy the nascent image */
			mali_image_deref( image );
			image = NULL; /* indicate failure to caller */
		}
		else
		{
			/* Attach the new Mali surface to the image as plane 0, mip level 0 */
			image->pixel_buffer[0][0] = surface;
			_mali_sys_atomic_inc( &image->references );
			_mali_surface_set_event_callback( surface, MALI_SURFACE_EVENT_DESTROY, mali_image_surface_destroy_callback, MALI_STATIC_CAST(void*)image);
		}
	}
	return image;
}

MALI_EXPORT struct mali_image *mali_image_create_from_ump_or_mali_memory(
                                   enum mali_surface_flags flags,
                                   const mali_surface_specifier *sformat,
                                   mali_image_memory_type memory_type,
                                   void *ext_mem, u32 offset,
                                   mali_base_ctx_handle base_ctx )
{
	mali_image *image;

	MALI_DEBUG_ASSERT_POINTER( sformat );
	MALI_DEBUG_ASSERT_POINTER( ext_mem );
	MALI_DEBUG_ASSERT( sformat->pitch != 0, ( "Zero pitch in %s", MALI_FUNCTION ) );
	MALI_DEBUG_ASSERT_POINTER( base_ctx );

	image = mali_image_alloc( sformat->width, sformat->height, MALI_IMAGE_CREATED_FROM_EXTMEM, base_ctx );
	if ( NULL != image )
	{
		mali_shared_mem_ref *mem_ref = NULL;
		mali_mem_handle mem_handle = MALI_NO_HANDLE;
		mali_surface *surface = NULL;

		switch ( memory_type )
		{
			case MALI_IMAGE_DMA_BUF:
#if MALI_USE_DMA_BUF
				mem_handle = _mali_mem_wrap_dma_buf( image->base_ctx, (int)ext_mem, offset );
				if ( MALI_NO_HANDLE != mem_handle )
				{
					mem_ref = _mali_shared_mem_ref_alloc_existing_mem( mem_handle );
					if ( NULL == mem_ref ) _mali_mem_free( mem_handle );
				}
#endif
			break;
			case MALI_IMAGE_UMP_MEM:
#if MALI_USE_UNIFIED_MEMORY_PROVIDER != 0
				mem_handle = _mali_mem_wrap_ump_memory( image->base_ctx, (ump_handle)ext_mem, offset );
				if ( MALI_NO_HANDLE != mem_handle )
				{
					mem_ref = _mali_shared_mem_ref_alloc_existing_mem( mem_handle );
					if ( NULL == mem_ref ) _mali_mem_free( mem_handle );
				}
#endif /* MALI_USE_UNIFIED_MEMORY_PROVIDER != 0 */
			break;

			case MALI_IMAGE_MALI_MEM:
				mem_handle = (mali_mem_handle)ext_mem;
				if ( MALI_NO_HANDLE != mem_handle ) mem_ref = _mali_shared_mem_ref_alloc_existing_mem( mem_handle );
				break;

			default:
				MALI_DEBUG_ASSERT( 0, ( "Unsupported memory type %d in %s",
				                        memory_type, MALI_FUNCTION ) );
				break;
		}
		if ( NULL != mem_ref )
		{
			/* Wrap existing memory as a surface with the required properties */
			surface = _mali_surface_alloc_ref( flags, sformat, mem_ref, offset, base_ctx );

			if ( NULL == surface ) _mali_shared_mem_ref_owner_deref( mem_ref );
		}
		if ( NULL == surface )
		{
			/* Failed to create a Mali surface, so destroy the nascent image */
			mali_image_deref( image );
			image = NULL; /* indicate failure to caller */
		}
		else
		{
			/* Attach the new Mali surface to the image as plane 0, mip level 0 */
			_mali_sys_atomic_inc( &image->references );
			_mali_surface_set_event_callback( surface, MALI_SURFACE_EVENT_DESTROY, mali_image_surface_destroy_callback, MALI_STATIC_CAST(void*)image);
			image->pixel_buffer[0][0] = surface;
		}
	}
	return image;
}

MALI_EXPORT struct mali_image *mali_image_create_from_external_memory( u32 width, u32 height,
                                                                       enum mali_surface_flags flags,
                                                                       mali_surface_specifier *sformat, void *ext_mem,
                                                                       mali_image_memory_type memory_type,
                                                                       mali_base_ctx_handle base_ctx )
{
	mali_image *image;
	MALI_DEBUG_ASSERT_POINTER( sformat );
	MALI_DEBUG_ASSERT_POINTER( base_ctx );

	sformat->width = width;
	sformat->height = height;
	sformat->pitch = width * (_mali_surface_specifier_bpp( sformat ) / 8);

	if ( MALI_IMAGE_CPU_MEM == memory_type )
	{
		/* The memory given is of CPU type and does not need to be given aligned.
		 * The data has to be manually copied into the mali memory.
		 * Align the calculated pitch to hardware requirements.
		 */
		sformat->pitch = MALI_ALIGN( sformat->pitch, MALI200_SURFACE_PITCH_ALIGNMENT_LINEAR_LAYOUT );
		image = mali_image_create_from_cpu_memory( flags, sformat, base_ctx );
	}
	else
	{
		image = mali_image_create_from_ump_or_mali_memory( flags, sformat, memory_type, ext_mem, 0, base_ctx );
	}
	return image;
}

MALI_EXPORT void mali_image_release( mali_image *image )
{
	MALI_DEBUG_ASSERT_POINTER( image );

	if ( NULL != image->locklist)
	{
		__mali_named_list_free( image->locklist, NULL );
		image->locklist = NULL;
	}
	_mali_sys_free( image );
}

MALI_EXPORT void mali_image_deref_surfaces( mali_image* image )
{
	unsigned int plane, miplevel;
	MALI_DEBUG_ASSERT_POINTER( image );

	for ( plane=0; plane<MALI_IMAGE_MAX_PLANES; ++plane )
	{
		for ( miplevel=0; miplevel<MALI_IMAGE_MAX_MIPLEVELS; ++miplevel )
		{
			mali_surface* surface = image->pixel_buffer[plane][miplevel];
			if ( NULL != surface )
			{
				_mali_surface_deref( surface );
				image->pixel_buffer[plane][miplevel] = NULL;
			}
		}
	}
}

MALI_EXPORT mali_bool mali_image_deref( mali_image* image )
{
	mali_bool image_deleted;

	MALI_DEBUG_ASSERT_POINTER( image );

	image_deleted = _mali_sys_atomic_dec_and_return( &image->references ) == 0 ? MALI_TRUE : MALI_FALSE;

	if ( MALI_TRUE == image_deleted )
	{
		mali_image_release( image );
	}

	return image_deleted;
}

MALI_EXPORT mali_surface* mali_image_get_buffer( mali_image *image, u32 plane, u32 mipmap, mali_bool exclude_aliased_buffers )
{
	MALI_DEBUG_ASSERT_POINTER( image );
	MALI_DEBUG_ASSERT( plane < MALI_IMAGE_MAX_PLANES, ("plane index above %i", MALI_IMAGE_MAX_PLANES) );
	MALI_DEBUG_ASSERT( mipmap < MALI_IMAGE_MAX_MIPLEVELS, ("mipmap level above %i", MALI_IMAGE_MAX_MIPLEVELS) );

	if ( mipmap > (image->miplevels-1) ) return NULL;

	if ( MALI_TRUE == exclude_aliased_buffers )
	{
		if ( NULL != image->yuv_info )
		{
			if ( MALI_IMAGE_PLANE_INVALID != image->yuv_info->plane[plane].plane_alias ) return NULL; /* this surface is an aliased surface */
		}
	}

	return image->pixel_buffer[plane][mipmap];
}

/**
 * @brief Checks if two locking regions overlaps
 * @param lock_session existing lock session
 * @param new_lock_session appending lock session
 * @return MALI_TRUE if regions overlap, MALI_FALSE if not
 */
MALI_STATIC mali_bool mali_image_overlapping_locks( mali_image_lock_session *lock_session, mali_image_lock_session *new_lock_session )
{
	u32 outside = 0;

	/* instead of checking for intersecting rectangles, we check if they exclude each other */

	/* new region is left of current */
	outside |= lock_session->x > (new_lock_session->x + new_lock_session->width );

	/* new region is right of current */
	outside |= new_lock_session->x > (lock_session->x + lock_session->width );

	/* new region is above current */
	outside |= lock_session->y > (new_lock_session->y + new_lock_session->height );

	/* new region is below current */
	outside |= new_lock_session->y > (lock_session->y + lock_session->height );

	return (outside == 1) ? MALI_FALSE: MALI_TRUE;
}

/**
 * @brief Checks if a given locking region is completely inside another
 * @param lock_session existing lock session
 * @param unlock_session session which has to be completely inside the existing lock session
 * @return MALI_TRUE if the region is completely inside, MALI_FALSE else
 */
MALI_STATIC mali_bool mali_image_contained_locks( mali_image_lock_session *lock_session, mali_image_lock_session *unlock_session )
{
	u32 outside = 0;

	/* we only need to check upper left and bottom right */
	outside |= unlock_session->x < lock_session->x; /* top unlock left of lock */
	outside |= unlock_session->y < lock_session->y; /* top unlock above lock */
	outside |= (unlock_session->x + unlock_session->width)  > (lock_session->x + lock_session->width); /* bottom unlock right of lock */
	outside |= (unlock_session->y + unlock_session->height) > (lock_session->y + lock_session->height); /* bottom unlock belove lock */

	return (outside == 0) ? MALI_TRUE: MALI_FALSE;
}

MALI_EXPORT mali_image_err_code mali_image_lock( mali_image *image,
                                                 mali_image_access_mode access_mode,
                                                 u16 plane, u16 miplevel,
                                                 s32 x, s32 y, s32 width, s32 height,
                                                 mali_bool multiple_write_locks_allowed,
                                                 mali_bool multiple_read_locks_allowed,
                                                 s32 *session_id, void **data )
{
	s32 lock_handle = 0;
	u32 size = 0;
	u32 iterator = 0;
	u32 access_rights = MALI_MEM_PTR_NO_PRE_UPDATE;
	mali_image_lock_session *lock_session = NULL;
	mali_image_lock_session *new_lock_session = NULL;
	mali_err_code retval = MALI_ERR_NO_ERROR;
	mali_surface *surface = NULL;
	void *mapping = NULL;

	MALI_DEBUG_ASSERT_POINTER( image );
	MALI_DEBUG_ASSERT_POINTER( session_id );

	/* verify the attributes */
	if ( access_mode & ~( MALI_IMAGE_ACCESS_READ_ONLY | MALI_IMAGE_ACCESS_WRITE_ONLY | MALI_IMAGE_ACCESS_READ_WRITE ) )
	{
		return MALI_IMAGE_ERR_BAD_PARAMETER;
	}

	/* verify that buffer exist */
	surface = mali_image_get_buffer( image, plane, miplevel, MALI_TRUE );
	if ( NULL == surface )
	{
		return MALI_IMAGE_ERR_BAD_BUFFER;
	}

	/* create a new lock session structure */
	new_lock_session = (mali_image_lock_session*)_mali_sys_malloc( sizeof(mali_image_lock_session) );
	if ( NULL == new_lock_session )
	{
		return MALI_IMAGE_ERR_BAD_ALLOC;
	}

	/* fill in new lock session */
	new_lock_session->surface = surface;
	new_lock_session->access_mode = access_mode;
	new_lock_session->x = x;
	new_lock_session->y = y;
	new_lock_session->width = width;
	new_lock_session->height = height;
	new_lock_session->session_id = -1;

	__mali_named_list_lock( image->locklist );

	lock_session = __mali_named_list_iterate_begin( image->locklist, &iterator );
	while ( NULL != lock_session )
	{
		if ( MALI_FALSE == multiple_write_locks_allowed )
		{
			if ( lock_session->access_mode & (MALI_IMAGE_ACCESS_WRITE_ONLY | MALI_IMAGE_ACCESS_READ_WRITE ) )
			{
				/* check if write area overlaps */
				if ( MALI_TRUE == mali_image_overlapping_locks( lock_session, new_lock_session ) )
				{
					_mali_sys_free( new_lock_session );
					__mali_named_list_unlock( image->locklist );
					return MALI_IMAGE_ERR_BAD_LOCK;
				}
			}
		}

		if ( MALI_FALSE == multiple_read_locks_allowed )
		{
			if ( lock_session->access_mode & (MALI_IMAGE_ACCESS_READ_ONLY | MALI_IMAGE_ACCESS_READ_WRITE ) )
			{
				/* check if read area overlaps */
				if ( MALI_TRUE == mali_image_overlapping_locks( lock_session, new_lock_session ) )
				{
					_mali_sys_free( new_lock_session );
					__mali_named_list_unlock( image->locklist );
					return MALI_IMAGE_ERR_BAD_LOCK;
				}
			}
		}
		lock_session = __mali_named_list_iterate_next( image->locklist, &iterator );
	}

	/* verify that buffer is not in use */
	if ( NULL != surface->mem_ref )
	{
		if ( _mali_shared_mem_ref_get_usage_ref_count( surface->mem_ref ) > 0 )
		{
			_mali_sys_free( new_lock_session );
			__mali_named_list_unlock( image->locklist );
			return MALI_IMAGE_ERR_IN_USE;
		}
	}

	/* implicitly allocate memory for the image buffer if it has not been done yet */
	if ( MALI_FALSE == mali_image_allocate_buffer( image, plane, miplevel ) )
	{
		_mali_sys_free( new_lock_session );
		__mali_named_list_unlock( image->locklist );
		return MALI_IMAGE_ERR_BAD_ALLOC;
	}

	surface = mali_image_get_buffer( image, plane, miplevel, MALI_TRUE );
	MALI_DEBUG_ASSERT_POINTER( surface );

	/* overlapping regions checking has passed, so we can now add the lock */
	lock_handle = __mali_named_list_get_unused_name( image->locklist );
	new_lock_session->session_id = lock_handle;

	/* map the data into the given pointer before adding the lock session */
	if ( new_lock_session->access_mode & (MALI_IMAGE_ACCESS_READ_ONLY | MALI_IMAGE_ACCESS_READ_WRITE ) ) access_rights |= MALI_MEM_PTR_READABLE;
	if ( new_lock_session->access_mode & (MALI_IMAGE_ACCESS_WRITE_ONLY | MALI_IMAGE_ACCESS_READ_WRITE ) ) access_rights |= MALI_MEM_PTR_WRITABLE;
	size = surface->format.height * surface->format.width * (_mali_surface_specifier_bpp( &surface->format ) / 8);

	mapping = _mali_mem_ptr_map_area( surface->mem_ref->mali_memory, 0 /* ofs */, size, 0 /* align_pow2 */, access_rights );
	if ( NULL == mapping )
	{
		_mali_sys_free( new_lock_session );
		__mali_named_list_unlock( image->locklist );
		return MALI_IMAGE_ERR_BAD_ALLOC;
	}

	retval = __mali_named_list_insert( image->locklist, lock_handle, new_lock_session );
	if ( MALI_ERR_NO_ERROR != retval )
	{
		_mali_mem_ptr_unmap_area( surface->mem_ref->mali_memory );
		_mali_sys_free( new_lock_session );
		__mali_named_list_unlock( image->locklist );
		return MALI_IMAGE_ERR_BAD_ALLOC;
	}

	*session_id = lock_handle;
	*data = mapping;

	_mali_surface_addref( surface );
	__mali_named_list_unlock( image->locklist );

	return MALI_IMAGE_ERR_NO_ERROR;
}

MALI_EXPORT mali_image_err_code mali_image_unlock( mali_image *image,
                                                   u16 plane, u16 miplevel,
                                                   s32 x, s32 y, s32 width, s32 height,
                                                   s32 session_id )
{
	mali_image_lock_session *lock_session = NULL;
	mali_image_lock_session unlock_session;
	mali_surface *surface = NULL;
	mali_image_err_code retval = MALI_IMAGE_ERR_NO_ERROR;

	MALI_DEBUG_ASSERT_POINTER( image );

	/* verify that buffer exist */
	surface = mali_image_get_buffer( image, plane, miplevel, EGL_TRUE );
	if ( NULL == surface )
	{
		return MALI_IMAGE_ERR_BAD_BUFFER;
	}

	/* fetch the lock session associated with the session id */
	__mali_named_list_lock( image->locklist );
	lock_session = __mali_named_list_get( image->locklist, session_id );
	if ( NULL == lock_session )
	{
		__mali_named_list_unlock( image->locklist );
		return MALI_IMAGE_ERR_BAD_LOCK;
	}

	/* check given region if access mode contains write access */
	if ( lock_session->access_mode & (MALI_IMAGE_ACCESS_WRITE_ONLY | MALI_IMAGE_ACCESS_READ_WRITE ) )
	{
		/* the unlock region must be completely inside or at edge of locked region */
		unlock_session.x = x;
		unlock_session.y = y;
		unlock_session.width = width;
		unlock_session.height = height;
		if ( MALI_FALSE == mali_image_contained_locks( lock_session, &unlock_session ) )
		{
			retval = MALI_IMAGE_ERR_BAD_PARAMETER; /* should still be unmapped, but error will be given */
		}
	}

	/* do the actual unmap */
	_mali_mem_ptr_unmap_area( surface->mem_ref->mali_memory );
	_mali_surface_deref( surface );

	/* remove the lock from the lock list */
	__mali_named_list_remove( image->locklist, session_id );
	__mali_named_list_unlock( image->locklist );

	_mali_sys_free( lock_session );

	return retval;
}

MALI_EXPORT mali_image_err_code mali_image_unlock_all_sessions( mali_image *image )
{
	u32 iterator = 0;
	mali_image_lock_session *lock_session = NULL;

	if ( NULL == image ) return MALI_IMAGE_ERR_NO_ERROR;

	lock_session = __mali_named_list_iterate_begin( image->locklist, &iterator );
	while ( NULL != lock_session )
	{
		_mali_mem_ptr_unmap_area( lock_session->surface->mem_ref->mali_memory );
		_mali_surface_deref( lock_session->surface );

		__mali_named_list_remove( image->locklist, lock_session->session_id );
		_mali_sys_free( lock_session );

		lock_session = __mali_named_list_iterate_begin( image->locklist, &iterator );
	}

	return MALI_IMAGE_ERR_NO_ERROR;
}

MALI_EXPORT mali_bool _mali_image_create_sync_handle( struct mali_image *surface )
{
	/* setup callbacks on the usecount */
	_mali_sys_atomic_set(&surface->mei_locked,0);
	return MALI_TRUE;
}

MALI_EXPORT mali_bool _mali_image_lock_sync_handle( mali_image *surface )
{
	MALI_DEBUG_ASSERT_POINTER( surface );

	/* if not locked, lock it */
	if ( _mali_sys_atomic_get(&surface->mei_locked) == 0 )
	{
		_mali_sys_atomic_set(&surface->mei_locked, 1);
		return MALI_TRUE;
	}

	return MALI_FALSE;
}

MALI_EXPORT mali_bool _mali_image_unlock_sync_handle( mali_image *surface )
{
	MALI_DEBUG_ASSERT_POINTER( surface );

	/* if not unlocked, unlock it */
	if ( _mali_sys_atomic_get(&surface->mei_locked) == 1 )
	{
		_mali_sys_atomic_set(&surface->mei_locked, 0);
		return MALI_TRUE;
	}

	return MALI_FALSE;
}


