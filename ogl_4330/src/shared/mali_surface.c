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
#include <shared/mali_surface.h>
#include <shared/mali_shared_mem_ref.h>
#include "shared/m200_gp_frame_builder.h"

/**
 * Frees the surface, and its resources. Its mem_ref is dereferenced once.
 * NOTE: You should not call this directly, use the deref function instead
 * @param buffer The surface to deallocate
 */
MALI_EXPORT void _mali_surface_free( mali_surface* buffer )
{
	MALI_DEBUG_ASSERT_POINTER( buffer );
	
	_mali_surface_trigger_event(buffer, NULL, MALI_SURFACE_EVENT_DESTROY);
	
	if ( NULL != buffer->mem_ref )
	{
		_mali_shared_mem_ref_owner_deref( buffer->mem_ref );
		buffer->mem_ref = NULL;
	}

	if( MALI_NO_HANDLE != buffer->access_lock )
	{
		_mali_sys_mutex_destroy( buffer->access_lock );
		buffer->access_lock = MALI_NO_HANDLE;
	}

	if (MALI_NO_HANDLE != buffer->ds_resource)
	{
		mali_ds_resource_set_owner(buffer->ds_resource, NULL);
		mali_ds_resource_release_connections(buffer->ds_resource, MALI_DS_RELEASE, MALI_DS_ABORT_NONE);
		buffer->ds_resource = MALI_NO_HANDLE;
	}
#ifdef DEBUG
	/* reset memory to trap dangling pointers if any */
	_mali_sys_memset( buffer, 0, sizeof ( mali_surface ) );
#endif /* DEBUG */

	_mali_sys_free( buffer );
}

MALI_EXPORT struct mali_surface* _mali_surface_alloc_empty( enum mali_surface_flags flags, const mali_surface_specifier* format, mali_base_ctx_handle base_ctx )
{
	u32 bpp;
	struct mali_surface* retval = _mali_sys_calloc(1, sizeof(struct mali_surface));
	if(retval == NULL) return NULL;

	MALI_DEBUG_ASSERT_POINTER( format );
	MALI_DEBUG_ASSERT((format->width <= MALI200_MAX_TEXTURE_SIZE), ("Can't create a mali surface with such width: %d", format->width));
	MALI_DEBUG_ASSERT((format->height <= MALI200_MAX_TEXTURE_SIZE), ("Can't create a mali surface with such height: %d", format->height));

	/* set certain values */
	retval->format = *format;
	retval->base_ctx = base_ctx;
	retval->flags = flags;

	bpp = _mali_surface_specifier_bpp( format );

	/* pitch will be autocalculated if zero and layout = LINEAR*/
	if(format->pixel_layout == MALI_PIXEL_LAYOUT_LINEAR && format->pitch == 0)
	{
		retval->format.pitch = _mali_surface_specifier_calculate_minimum_pitch(format);
	}

	/* for layouts != LINEAR, the pitch should be 0 */
	MALI_DEBUG_ASSERT( (MALI_PIXEL_LAYOUT_LINEAR != format->pixel_layout) || (0 != retval->format.pitch) || (0 == format->width), ("W>0 linear layouts require pitch != 0" ));
	MALI_DEBUG_ASSERT( (MALI_PIXEL_LAYOUT_LINEAR == format->pixel_layout) || (0 == retval->format.pitch), ("NonLinear layouts layout require pitch = 0"));

	/* For renderable surfaces with layouts == LINEAR, the pitch alignment
	   must conform to Mali writeback unit requirements */
	if( format->pixel_format != MALI_PIXEL_FORMAT_NONE &&
	    format->pixel_layout == MALI_PIXEL_LAYOUT_LINEAR &&
	    retval->format.pitch % MALI200_SURFACE_PITCH_ALIGNMENT_LINEAR_LAYOUT != 0 )
	{
		MALI_DEBUG_ASSERT( 0, ("Pitch %u is not aligned to hardware requirement of %d bytes",
		                       retval->format.pitch, MALI200_SURFACE_PITCH_ALIGNMENT_LINEAR_LAYOUT));
		_mali_surface_free( retval );
		return NULL;
	}

	if (format->pixel_layout == MALI_PIXEL_LAYOUT_LINEAR)
	{
		/* Make sure pitch is aligned properly with pixel format.
		 * Table from Mali Tech reference manual --> 3.15.5. Texture descriptor data structures
		 */
		switch (bpp)
		{
			case 16:
			case 48:
				MALI_DEBUG_ASSERT( ((retval->format.pitch & 1) == 0), ("Pitch value must be multiple of 2!") );
				break;
			case 32:
				MALI_DEBUG_ASSERT( ((retval->format.pitch & 3) == 0), ("Pitch value must be multiple of 4!") );
				break;
			case 64:
				MALI_DEBUG_ASSERT( ((retval->format.pitch & 7) == 0), ("Pitch value must be multiple of 8!") );
				break;
		}
	}
#ifdef USING_MALI200
	if( format->pixel_layout == MALI_PIXEL_LAYOUT_LINEAR)
	{
		MALI_DEBUG_ASSERT( retval->format.pitch % ((bpp+7)/8) == 0, ("pitch for linear surfaces must be aligned to full pixels on Mali200"));
	}
#endif

	/* calculate datasize */
	retval->datasize = _mali_surface_specifier_datasize(&retval->format);

	/* atomic integers and mutexes */
	_mali_sys_atomic_initialize(&retval->ref_count, 1);
	retval->access_lock = _mali_sys_mutex_create();
	if(MALI_NO_HANDLE == retval->access_lock)
	{
		_mali_surface_free( retval );
		return NULL;
	}

	retval->ds_resource = mali_ds_resource_allocate( base_ctx, retval, MALI_STATIC_CAST(mali_ds_cb_func_resource)NULL);
	if (MALI_NO_HANDLE == retval->ds_resource)
	{
		_mali_surface_free( retval );
		return NULL;
	}

	/* Assert that the flags given are sane. We do not want to allow flags that give undefined hardware behavior. */ 
#ifndef NDEBUG
	{
		mali_bool support_rbswap, support_revorder;
		__m200_texel_format_flag_support(format->texel_format, &support_rbswap, &support_revorder);
		MALI_DEBUG_ASSERT( ! (format->red_blue_swap == MALI_TRUE && support_rbswap == MALI_FALSE), ("Texel format %i does not support red blue swap = TRUE", format->texel_format));
		MALI_DEBUG_ASSERT( ! (format->reverse_order == MALI_TRUE && support_revorder == MALI_FALSE), ("Texel format %i does not support reverse order = TRUE", format->texel_format));
	}
#endif
	return retval;
}


MALI_EXPORT struct mali_surface* _mali_surface_alloc( enum mali_surface_flags flags, const mali_surface_specifier* format, u32 mem_offset, mali_base_ctx_handle base_ctx )
{
	struct mali_surface* retval;

	MALI_DEBUG_ASSERT_POINTER( format );

	MALI_DEBUG_ASSERT((flags & MALI_SURFACE_FLAG_DONT_MOVE) == 0, ("_mali_surface_alloc allocates a surface in dynamic memory. Declaring this surface with MALI_SURFACE_FLAG_DONT_MOVE is a logical error, as nothing prevents this memory from being moved around."));

	retval = _mali_surface_alloc_empty(flags, format, base_ctx );
	if(retval == NULL) return NULL;

	/* allocate the mali memory required. */
	if( retval->datasize > 0 )
	{
		int actual_datasize = retval->datasize;
#ifdef HARDWARE_ISSUE_9427
		/* Hardware writes outside the given memory area; increase size of memory area in cases where this can happen */
		if( format->pixel_format != MALI_PIXEL_FORMAT_NONE && /* writeable */
			format->pixel_layout == MALI_PIXEL_LAYOUT_LINEAR &&  /* linear */
			((format->width % 16) != 0) ) /* width not divisible by 16 */
		{
			actual_datasize += HARDWARE_ISSUE_9427_EXTRA_BYTES;
		}		
#endif
		actual_datasize += mem_offset;
		retval->mem_ref = _mali_shared_mem_ref_alloc_mem(base_ctx, actual_datasize,
    	                         MALI200_SURFACE_ALIGNMENT,  MALI_PP_WRITE | MALI_PP_READ | MALI_CPU_READ | MALI_CPU_WRITE );
		if( retval->mem_ref == NULL )
		{
			/* clean up and return */
			_mali_surface_free(retval);
			return NULL;
		}

		retval->mem_offset = mem_offset;
	}

	return retval;
}

MALI_EXPORT struct mali_surface* _mali_surface_alloc_surface( mali_surface* old, mali_bool copy )
{
	struct mali_surface* retval;
	enum mali_surface_flags flags;

	MALI_DEBUG_ASSERT_POINTER( old );
	MALI_DEBUG_ASSERT(_mali_sys_mutex_try_lock( old->access_lock ) == MALI_ERR_FUNCTION_FAILED, ("Old surface must be locked before calling this constructor"));

	/* clear the MALI_SURFACE_FLAG_DONT_MOVE flag from the copy, it is not required on a dynamically allocated surface */
	flags = old->flags & (~MALI_SURFACE_FLAG_DONT_MOVE);

	retval = _mali_surface_alloc(flags, &old->format, old->mem_offset, old->base_ctx );
	if(retval == NULL) return NULL;

	if(copy == MALI_TRUE)
	{
		_mali_mem_copy( retval->mem_ref->mali_memory, retval->mem_offset, old->mem_ref->mali_memory, old->mem_offset, old->datasize);
	}

	/* assert that the allocated memory area is aligned properly */
	MALI_DEBUG_ASSERT_ALIGNMENT(_mali_mem_mali_addr_get(retval->mem_ref->mali_memory, retval->mem_offset), MALI200_SURFACE_ALIGNMENT); 

	return retval;
}

MALI_EXPORT struct mali_surface* _mali_surface_alloc_ref( enum mali_surface_flags flags, const mali_surface_specifier* format,
                                                          struct mali_shared_mem_ref *mem_ref, u32 offset, mali_base_ctx_handle base_ctx )
{
	struct mali_surface* retval;

	MALI_DEBUG_ASSERT_POINTER( format );

	retval = _mali_surface_alloc_empty(flags, format, base_ctx);
	if(retval == NULL) return NULL;

	MALI_DEBUG_ASSERT_POINTER( mem_ref );

	/* assert that the provided memory area is large enough */
	MALI_DEBUG_ASSERT( (_mali_mem_size_get( mem_ref->mali_memory ) >= retval->datasize + offset),
							("explicit rendertarget buffer is too small, have %i bytes, need %i bytes!\n",
							 _mali_mem_size_get( mem_ref->mali_memory ), retval->datasize + offset) );

	/* assert that the provided memory area is aligned properly */
	MALI_DEBUG_ASSERT_ALIGNMENT(_mali_mem_mali_addr_get(mem_ref->mali_memory, offset), MALI200_SURFACE_ALIGNMENT);

	/* use it */
	retval->mem_ref  = mem_ref;
	retval->mem_offset = offset;

	return retval;
}

MALI_EXPORT void _mali_surface_access_lock( struct mali_surface *buffer )
{
	MALI_DEBUG_ASSERT_POINTER( buffer );
	_mali_sys_mutex_lock( buffer->access_lock );
}


MALI_EXPORT void _mali_surface_access_unlock( struct mali_surface *buffer )
{
	MALI_DEBUG_ASSERT_POINTER( buffer );
	MALI_DEBUG_ASSERT(_mali_sys_mutex_try_lock( buffer->access_lock ) == MALI_ERR_FUNCTION_FAILED, ("Lock must already be taken"));
	_mali_sys_mutex_unlock( buffer->access_lock );
}

MALI_EXPORT void* _mali_surface_map( struct mali_surface *buffer, u32 flag )
{
	void* retval = NULL;

	MALI_DEBUG_ASSERT_POINTER( buffer );
	MALI_DEBUG_ASSERT_POINTER( buffer->mem_ref );
	MALI_DEBUG_ASSERT_POINTER( buffer->mem_ref->mali_memory );

	MALI_DEBUG_ASSERT(_mali_sys_mutex_try_lock( buffer->access_lock ) == MALI_ERR_FUNCTION_FAILED, ("Access lock must already be taken, _mali_surface_map is accessing the memref"));

	retval = _mali_mem_ptr_map_area(
					buffer->mem_ref->mali_memory,
					buffer->mem_offset,
					buffer->datasize,
					MALI200_SURFACE_ALIGNMENT,
					flag);

	_mali_surface_trigger_event( buffer, buffer->mem_ref, MALI_SURFACE_EVENT_CPU_ACCESS);

	return retval;
}

MALI_EXPORT void _mali_surface_unmap( struct mali_surface *buffer )
{
	MALI_DEBUG_ASSERT_POINTER( buffer );

	MALI_DEBUG_ASSERT(_mali_sys_mutex_try_lock( buffer->access_lock ) == MALI_ERR_FUNCTION_FAILED, ("Access lock must already be taken, _mali_surface_unmap is accessing the memref"));

	if(buffer->mem_ref != MALI_NO_HANDLE) _mali_mem_ptr_unmap_area( buffer->mem_ref->mali_memory );

	_mali_surface_trigger_event( buffer, buffer->mem_ref, MALI_SURFACE_EVENT_CPU_ACCESS_DONE );
}

MALI_EXPORT mali_ds_resource_handle _mali_surface_clear_dependencies(mali_surface* surface, mali_surface_deep_cow_descriptor *deep_cow_desc)
{
	mali_surface* copy;
	mali_ds_resource_handle tempresource;
	struct mali_shared_mem_ref *temp_mem_ref;
	u32 temp_mem_offset;
	
	MALI_DEBUG_ASSERT(_mali_sys_mutex_try_lock( surface->access_lock ) == MALI_ERR_FUNCTION_FAILED, ("Lock must already be taken"));
	MALI_DEBUG_ASSERT((surface->flags & MALI_SURFACE_FLAG_DONT_MOVE) == 0, ("Surfaces flagged with MALI_SURFACE_FLAG_DONT_MOVE may not be moved. Until bugzilla 6400/6401 is resolved, _mali_surface_clear_dependencies can not handle these kind of surfaces"));

	/* create a copy of the surface, never copy content directly */
	copy = _mali_surface_alloc_surface(surface, MALI_FALSE);
	if(copy == NULL) return NULL;

	/* switch the memory area */
	temp_mem_ref = surface->mem_ref;
	surface->mem_ref = copy->mem_ref;
	copy->mem_ref = temp_mem_ref;
	temp_mem_offset = surface->mem_offset;
	surface->mem_offset = copy->mem_offset;
	copy->mem_offset = temp_mem_offset;
	
	/* switch the depsystem module. This means the copy texture gets all the dependencies, and the original texture gets none of them */
	tempresource = surface->ds_resource;
	surface->ds_resource = copy->ds_resource;
	copy->ds_resource = tempresource;
	
	/* update the new depsystem owners */
	mali_ds_resource_set_owner(copy->ds_resource, copy);
	mali_ds_resource_set_owner(surface->ds_resource, surface);

	/* Deep CoW needs to use the dependency system.
	   At this point we can _only_ prepare the necessary information (old place & new place).
	   Execution of the data copy must be delayed until all outstanding writes to the source
	   are finished
	*/
	if ( NULL != deep_cow_desc )
	{
		deep_cow_desc->src_mem_ref = copy->mem_ref;
		deep_cow_desc->mem_offset  = surface->mem_offset;
		deep_cow_desc->data_size   = surface->datasize;
		deep_cow_desc->dest_mem_ref= surface->mem_ref;
		_mali_shared_mem_ref_owner_addref(deep_cow_desc->src_mem_ref);
	}

	/* dereference the copy surface, now coupled with the old ds_resource and old mem_ref.
	 * This operation will delete the copy surface. The ds_resource and the memref are
	 * still addref'ed through jobs using them. Once those jobs finish, they will vanish too */
	_mali_surface_deref(copy);

	surface->timestamp++;

	/* handle API specific CoW if needed */
	_mali_surface_trigger_event(surface, NULL, MALI_SURFACE_EVENT_COPY_ON_WRITE);

	return surface->ds_resource;
}

#if MALI_INSTRUMENTED 
/**
 * Bit replication macro for expanding 4,5 and 6 bit values to 8-bit.
 * @note The MSB is replicated.
 * @param a		value to bit-replicate
 * @param bits	number of bits in value
 * @param shift	shift amount for value
 */
#define REPLICATE_BITS( a, bits, shift ) \
	((a) << (shift)) | (((a) >> ((bits)-(shift))) & ((1 << (shift))-1))

void _mali_surface_fprintf( struct mali_surface *surface, const char *filename )
{
	mali_file *file;
	u8 r, g, b;
	u8 *source_bytes, *output_bytes, *output_bytes_ptr;
	u32 x, y;
	enum mali_pixel_format pixel_format;
	u32                    fb_width, fb_height;
	u16                    fb_pitch;

	mali_bool invert_component_order = MALI_FALSE;
	mali_bool swap_red_blue = MALI_FALSE;

	pixel_format = surface->format.pixel_format;

	invert_component_order =  surface->format.reverse_order;
	swap_red_blue =  surface->format.red_blue_swap;

	/* check for supported format */
	switch ( pixel_format )
	{
	case MALI_PIXEL_FORMAT_R5G6B5:
	case MALI_PIXEL_FORMAT_A8R8G8B8: /* fallthrough */
		break;
	case MALI_PIXEL_FORMAT_NONE:
		pixel_format = _mali_texel_to_pixel_format ( surface->format.texel_format);
		break;
	default:
		MALI_DEBUG_PRINT(0, ("Framebuffer-dump: unsupported format\n"));
		return;
	}

	file = _mali_sys_fopen(filename, "wb");
	if (NULL == file) return;

	fb_width  = surface->format.width;
	fb_height = surface->format.height;
	fb_pitch = surface->format.pitch;

	MALI_DEBUG_ASSERT( surface->format.pixel_layout == MALI_PIXEL_LAYOUT_LINEAR, ("This code only works in linear mode"));

	/* write the header */
	_mali_sys_fprintf(file, "P6 %u %u 255\n", fb_width, fb_height);

	/* No need to writelock the surface as this is handled by the callback calling this function */
	source_bytes = _mali_mem_ptr_map_area( surface->mem_ref->mali_memory, surface->mem_offset, surface->datasize, MALI200_SURFACE_ALIGNMENT, MALI_MEM_PTR_READABLE);

	/* map a pointer to the framebuffer */
	if (NULL != source_bytes)
	{
		output_bytes_ptr = output_bytes = _mali_sys_malloc(fb_width*3);
		if (NULL != output_bytes)
		{
			for (y = 0; y < fb_height; ++y, source_bytes += fb_pitch)
			{

				/* write the framebuffer data */
				switch ( pixel_format )
				{
				case MALI_PIXEL_FORMAT_R5G6B5:
					{
						u16 *scanline = MALI_STATIC_CAST(u16*)source_bytes;
						u16 pixel = 0;

						for (x = 0; x < fb_width; ++x)
						{
							pixel = scanline[x];

							if (invert_component_order)
							{
								b = REPLICATE_BITS( (pixel >> 11), 5, 3 );
								g = REPLICATE_BITS( ((pixel >> 5) & 0x3F), 6, 2 );
								r = REPLICATE_BITS( (pixel & 0x1F), 5, 3 );
							}
							else
							{
								r = REPLICATE_BITS( (pixel >> 11), 5, 3 );
								g = REPLICATE_BITS( ((pixel >> 5) & 0x3F), 6, 2 );
								b = REPLICATE_BITS( (pixel & 0x1F), 5, 3 );
							}
							if (swap_red_blue)
							{
								*output_bytes_ptr++ = b;
								*output_bytes_ptr++ = g;
								*output_bytes_ptr++ = r;
							}
							else
							{
								*output_bytes_ptr++ = r;
								*output_bytes_ptr++ = g;
								*output_bytes_ptr++ = b;
							}
						}

						_mali_sys_fwrite(output_bytes, 1, fb_width*3, file);
						output_bytes_ptr = output_bytes;
						break;
					}
				case MALI_PIXEL_FORMAT_A8R8G8B8:
					{
						u32 *scanline = MALI_STATIC_CAST(u32*)source_bytes;
						u32 pixel = 0;

						for (x = 0; x < fb_width; ++x)
						{
							pixel = scanline[x];

							if (invert_component_order)
							{
								r = (pixel >> 8) & 0xFF;
								g = (pixel >> 16) & 0xFF;
								b = (pixel >> 24) & 0xFF;
							}
							else
							{
								r = (pixel >> 16) & 0xFF;
								g = (pixel >> 8) & 0xFF;
								b = pixel & 0xFF;
							}
							if (swap_red_blue)
							{
								*output_bytes_ptr++ = b;
								*output_bytes_ptr++ = g;
								*output_bytes_ptr++ = r;
							}
							else
							{
								*output_bytes_ptr++ = r;
								*output_bytes_ptr++ = g;
								*output_bytes_ptr++ = b;
							}
						}
						_mali_sys_fwrite(output_bytes, 1, fb_width*3, file);
						output_bytes_ptr = output_bytes;
						break;
					}
				case MALI_PIXEL_FORMAT_NONE:
				{
					switch ( surface->format.texel_format )
					{
#if !RGB_IS_XRGB
					case M200_TEXEL_FORMAT_RGB_888:
						{
							u8 *scanline = MALI_STATIC_CAST(u8*)source_bytes;

							for (x = 0; x < fb_width*3; x=x+3)
							{
								if (invert_component_order)
								{
									r = scanline[x];
									b = scanline[x+1];
									g = scanline[x+2];
								}
								else
								{
									r = scanline[x+2];
									b = scanline[x+1];
									g = scanline[x];
								}
								if (swap_red_blue)
								{
									*output_bytes_ptr++ = b;
									*output_bytes_ptr++ = g;
									*output_bytes_ptr++ = r;
								}
								else
								{
									*output_bytes_ptr++ = r;
									*output_bytes_ptr++ = g;
									*output_bytes_ptr++ = b;
								}
							}
							_mali_sys_fwrite(output_bytes, 1, fb_width*3, file);
							output_bytes_ptr = output_bytes;
							break;
						}
#endif
					default:
						MALI_DEBUG_ASSERT( MALI_FALSE, (
							"Unsupported texel layout was not handled \n"));
						break;
					}
					break;
				}
				default:
					MALI_DEBUG_ASSERT( MALI_FALSE, (
						"Unsupported format was not handled before writing to file\n"));
					break;
				}
			}
			_mali_sys_free(output_bytes);
		}
		else 
		{
			MALI_DEBUG_TPRINT(0, ("Could not allocate output_bytes\n"));
		}

		_mali_mem_ptr_unmap_area(surface->mem_ref->mali_memory );
	}
	else 
	{
		MALI_DEBUG_TPRINT(0, ("Could not map fb_handle\n"));
	}

	_mali_sys_fclose(file);
}

void _mali_surface_dump( struct mali_surface *surface, const char *filename_prefix )
{
	char filename[256];
	mali_file *file;
	u8* source_bytes;
	MALI_DEBUG_ASSERT_POINTER( surface );
	MALI_DEBUG_ASSERT_POINTER( filename_prefix );
	
	_mali_sys_snprintf(filename, 256, "%s.hex.metadata", filename_prefix);
	
	file = _mali_sys_fopen(filename, "w");
	if( NULL == file ) return;

	/* dump file metadata */
	if(surface->mem_ref) 
	{
		_mali_sys_fprintf(file, "Address:     %p\n", _mali_mem_mali_addr_get_full(surface->mem_ref->mali_memory, surface->mem_offset));
	} else {
		_mali_sys_fprintf(file, "Address:      N/A (not specified)\n" );
	}
	_mali_sys_fprintf(file, "Datasize:    %d bytes\n", surface->datasize);
	_mali_sys_fprintf(file, "\n");	
	
	{
		char* layout = "N/A (not specified)";
		if(surface->format.pixel_layout == MALI_PIXEL_LAYOUT_LINEAR) layout = "Linear";
		if(surface->format.pixel_layout == MALI_PIXEL_LAYOUT_INTERLEAVED_BLOCKS) layout = "Block-interleaved";
		_mali_sys_fprintf(file, "Layout:      %s\n", layout);
	}
	{
		char* format = "N/A (not specified)";

		switch(surface->format.pixel_format)
		{
			case MALI_PIXEL_FORMAT_R5G6B5:
				format = "16bpp RGB 565";
				if(surface->format.red_blue_swap) format = "16bpp BGR 565";
				break;
			case MALI_PIXEL_FORMAT_B8:
				format = "8bpp intensity";
				break;
			case MALI_PIXEL_FORMAT_G8B8:
				format = "16bpp intensity8/alpha8";
				break;
			case MALI_PIXEL_FORMAT_B_FP16:
				format = "16bpp FP16 intensity";
				break;
			case MALI_PIXEL_FORMAT_GB_FP16:
				format = "32bpp FP16 intensity/alpha";
				break;
			case MALI_PIXEL_FORMAT_S8:
				format = "8bpp stencil";
				break;
			case MALI_PIXEL_FORMAT_Z16:
				format = "16bpp depth";
				break;
			case MALI_PIXEL_FORMAT_S8Z24:
				format = "32bpp packed stencil8/depth24";
				break;
			case MALI_PIXEL_FORMAT_ARGB_FP16:
				format = "64bpp ARGB FP16";
				if(surface->format.red_blue_swap && !surface->format.reverse_order) format = "64bpp ABGR FP16";
				if(!surface->format.red_blue_swap && surface->format.reverse_order) format = "64bpp BGRA FP16";
				if(surface->format.red_blue_swap && surface->format.reverse_order) format = "64bpp RGBA FP16";
				break;
			case MALI_PIXEL_FORMAT_A1R5G5B5:
				format = "16bpp ARGB 1555";
				if(surface->format.red_blue_swap && !surface->format.reverse_order) format = "16bpp ABGR 1555";
				if(!surface->format.red_blue_swap && surface->format.reverse_order) format = "16bpp BGRA 5551";
				if(surface->format.red_blue_swap && surface->format.reverse_order) format = "16bpp RGBA 5551";
				break;
			case MALI_PIXEL_FORMAT_A4R4G4B4:
				format = "16bpp ARGB 4444";
				if(surface->format.red_blue_swap && !surface->format.reverse_order) format = "16bit ABGR 4444";
				if(!surface->format.red_blue_swap && surface->format.reverse_order) format = "16bit BGRA 4444";
				if(surface->format.red_blue_swap && surface->format.reverse_order) format = "64bpp RGBA 4444";
				break;
			case MALI_PIXEL_FORMAT_A8R8G8B8:
				format = "32bpp ARGB 8888";
				if(surface->format.red_blue_swap && !surface->format.reverse_order) format = "32bpp ABGR 8888";
				if(!surface->format.red_blue_swap && surface->format.reverse_order) format = "32bpp BGRA 8888";
				if(surface->format.red_blue_swap && surface->format.reverse_order) format = "32bpp RGBA 8888";
				break;
			default:
				break;
		}
		_mali_sys_fprintf(file, "Format:      %s\n", format);
		
	}
	
	_mali_sys_fprintf(file, "\n");	
	_mali_sys_fprintf(file, "Width:       %d\n", surface->format.width);
	_mali_sys_fprintf(file, "Height:      %d\n", surface->format.height);
	if(surface->format.pixel_layout == MALI_PIXEL_LAYOUT_LINEAR)
	{
		_mali_sys_fprintf(file, "Pitch:       %d\n", surface->format.pitch);
	} else {
		_mali_sys_fprintf(file, "Blockwidth:  %d\n", MALI_ALIGN( surface->format.width, MALI200_TILE_SIZE ));
		_mali_sys_fprintf(file, "Blockheight: %d\n", MALI_ALIGN( surface->format.height, MALI200_TILE_SIZE ));
	}

	
	_mali_sys_fclose(file);
	

	/* dump the hex file */
	_mali_sys_snprintf(filename, 256, "%s.hex", filename_prefix);
	file = _mali_sys_fopen(filename, "wb");
	if( NULL!=file && NULL!=surface->mem_ref ) 
	{
		source_bytes = _mali_mem_ptr_map_area( surface->mem_ref->mali_memory, surface->mem_offset, 
		                                       surface->datasize, 
		                                       MALI200_SURFACE_ALIGNMENT, MALI_MEM_PTR_READABLE);
		_mali_sys_fwrite(source_bytes, 1, surface->datasize, file);
		_mali_mem_ptr_unmap_area(surface->mem_ref->mali_memory );
	
		_mali_sys_fclose(file);
	}
	
}

#endif
