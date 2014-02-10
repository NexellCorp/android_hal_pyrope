/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include "pixmap.h"
#if MALI_USE_UNIFIED_MEMORY_PROVIDER
#include <ump/ump.h>
#if !defined(UMP_VERSION_MAJOR) || UMP_VERSION_MAJOR == 1
#include <ump/ump_ref_drv.h>
#endif
#endif

fbdev_pixmap* create_pixmap_internal(int width, int height, int red, int green, int blue, int alpha, int luminance, int ump )
{
	int use_ump = ump;
	fbdev_pixmap* pixmap;

	pixmap = CALLOC( 1, sizeof(fbdev_pixmap) );
	if( NULL == pixmap ) return NULL;
	pixmap->flags = FBDEV_PIXMAP_DEFAULT;
	pixmap->width = width;
	pixmap->height = height;
	pixmap->red_size = red;
	pixmap->green_size = green;
	pixmap->blue_size = blue;
	pixmap->alpha_size = alpha;
	pixmap->luminance_size = luminance;
	pixmap->buffer_size = pixmap->red_size + pixmap->green_size + pixmap->blue_size + pixmap->alpha_size + pixmap->luminance_size;
	pixmap->bytes_per_pixel = pixmap->buffer_size / 8;

#if !MALI_USE_UNIFIED_MEMORY_PROVIDER
	if ( 1 == use_ump )
	{
		use_ump = 0;
		/*printf("WARNING: ump is not enabled, fallback to non-ump\n");*/
	}
#endif /* MALI_USE_UNIFIED_MEMORY_PROVIDER */

	if ( 1 == use_ump )
	{
#if MALI_USE_UNIFIED_MEMORY_PROVIDER
		if ( UMP_OK != ump_open() )
		{
			fprintf(stderr, "%s: unable to open UMP interface\n", MALI_FUNCTION);
			return NULL;
		}
		pixmap->data = ump_ref_drv_allocate( pixmap->width * pixmap->height * pixmap->bytes_per_pixel, UMP_REF_DRV_CONSTRAINT_NONE );
		if ( UMP_INVALID_MEMORY_HANDLE == pixmap->data )
		{
			fprintf(stderr, "%s: unable to allocate from UMP interface\n", MALI_FUNCTION);
			ump_close();
			return NULL;
		}
		pixmap->flags = FBDEV_PIXMAP_SUPPORTS_UMP;
#endif /* MALI_USE_UNIFIED_MEMORY_PROVIDER */
	}
	else
	{
		pixmap->data = MALLOC( pixmap->width*pixmap->height*pixmap->bytes_per_pixel );
		if ( NULL == pixmap->data )
		{
			FREE( pixmap );
			return NULL;
		}
	}

	return pixmap;
}

fbdev_pixmap* create_pixmap(int width, int height, int red, int green, int blue, int alpha, int luminance )
{
	return create_pixmap_internal( width, height, red, green, blue, alpha, luminance, 0 );
}

fbdev_pixmap* create_pixmap_ump(int width, int height, int red, int green, int blue, int alpha, int luminance )
{
	return create_pixmap_internal( width, height, red, green, blue, alpha, luminance, 1 );
}

u16* get_pixmap_data( fbdev_pixmap *pixmap )
{
#if MALI_USE_UNIFIED_MEMORY_PROVIDER
	if ( pixmap->flags == FBDEV_PIXMAP_SUPPORTS_UMP )
	{
		u16* ptr = (u16 *)ump_mapped_pointer_get( (ump_handle)pixmap->data );
		if ( NULL == ptr )
		{
			fprintf(stderr, "%s: ump_get_mapped_pointer returned NULL\n", MALI_FUNCTION);
			exit(1);
		}
		return ptr;
	}
#endif

	return pixmap->data;
}

u16* map_pixmap_data( fbdev_pixmap *pixmap )
{

#if MALI_USE_UNIFIED_MEMORY_PROVIDER
	if ( FBDEV_PIXMAP_SUPPORTS_UMP == pixmap->flags )
	{
		void *ptr = NULL;
		u16 *data = NULL;
		ptr = ump_mapped_pointer_get( (ump_handle)pixmap->data );
		data = (u16*)ptr;
		return data;
	}
#endif

	return pixmap->data;
}

void unmap_pixmap_data( fbdev_pixmap *pixmap )
{
#if MALI_USE_UNIFIED_MEMORY_PROVIDER
	if ( FBDEV_PIXMAP_SUPPORTS_UMP == pixmap->flags )
	{
		ump_mapped_pointer_release( (ump_handle)pixmap->data );
	}
#endif
}


void free_pixmap(fbdev_pixmap* pixmap)
{
	if ( FBDEV_PIXMAP_SUPPORTS_UMP == pixmap->flags )
	{
#if MALI_USE_UNIFIED_MEMORY_PROVIDER
		ump_reference_release( (ump_handle)pixmap->data );
		ump_close();
#endif
	}
	else
	{
		FREE(pixmap->data);
	}
	FREE(pixmap);
}

