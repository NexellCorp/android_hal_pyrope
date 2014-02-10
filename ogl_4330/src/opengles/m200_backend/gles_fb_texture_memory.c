/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_fb_texture_memory.h" 
#include <shared/mali_surface.h>

void _gles_fb_texture_memory_init(struct gles_fb_texture_memory *texmem, enum gles_texture_target dimensionality, unsigned int sublevels, mali_base_ctx_handle base_ctx)
{
	MALI_DEBUG_ASSERT_POINTER(texmem);
	MALI_DEBUG_ASSERT_POINTER(base_ctx);

	_mali_sys_memset(texmem, 0, sizeof(struct gles_fb_texture_memory));
	if(dimensionality == GLES_TEXTURE_TARGET_CUBE ) 
	{
		texmem->faces = 6; 
	} 
	else 
	{
		texmem->faces = 1;
	}
	texmem->sublevels = sublevels;
	texmem->dimensionality = dimensionality;
	texmem->base_ctx = base_ctx;
	texmem->frame_id_last_added = MALI_BAD_FRAME_ID; 
}

void _gles_fb_texture_memory_reset(struct gles_fb_texture_memory *texmem)
{
	unsigned int face, level;
	MALI_DEBUG_ASSERT_POINTER(texmem);

	/* keep the faces/sublevels/dimensionality state */

	/* free all memory in the struct, reset it to initialized values */
	if(texmem->mipmaps_mem) 
	{
		_mali_shared_mem_ref_owner_deref(texmem->mipmaps_mem);
		texmem->frame_id_last_added = MALI_BAD_FRAME_ID; /* new memory assigned, so invalidate the old memory tracker */
		texmem->mipmaps_mem = NULL;
	}

	for(level = 0; level < texmem->sublevels; level++)
	{
		for(face = 0; face < texmem->faces; face++)
		{
			if(texmem->surfaces[face][level])
			{
				_mali_surface_deref(texmem->surfaces[face][level]);
				texmem->surfaces[face][level] = NULL;
			}
		}
	}

	/* also reset the flags */
	texmem->flag_read_only = MALI_FALSE;
	texmem->flag_dont_replace = MALI_FALSE;
	texmem->flag_renderable = MALI_FALSE;
	texmem->flag_need_constraint_resolve = MALI_FALSE;
}

/**
 * Calculates the offset in a memory block as required by the HW. 
 * @param texmem - The texmem object to query
 * @param face - The face to find the offset to
 * @param sublevel - The level to find the offset to. 
 * @returns the offset to the face and level, in bytes
 */
MALI_STATIC unsigned int calculate_offset(const struct gles_fb_texture_memory *texmem, unsigned int face, unsigned int sublevel)
{
	MALI_DEBUG_ASSERT_POINTER(texmem);
	MALI_DEBUG_ASSERT(face < texmem->faces, ("Input error: face is out of range. Is %i, max is %i", face, texmem->faces-1));
	MALI_DEBUG_ASSERT(sublevel < texmem->sublevels, ("Input error: sublevel is out of range. Is %i, max is %i", sublevel, texmem->sublevels-1));

	switch(texmem->dimensionality)
	{
		case GLES_TEXTURE_TARGET_CUBE:
			{
				unsigned int roundup_texture_size = MALI_ALIGN(texmem->mipmaps_format[sublevel].width, MALI200_TILE_SIZE);
				unsigned int texelsize = __m200_texel_format_get_bpp(texmem->mipmaps_format[sublevel].texel_format); 
				MALI_DEBUG_ASSERT(texmem->mipmaps_format[0].width == texmem->mipmaps_format[0].height, ("Cubemaps must have equal width and height")); 
				MALI_DEBUG_ASSERT(texmem->mipmaps_format[sublevel].width == texmem->mipmaps_format[sublevel].height, ("Cubemaps must have equal width and height")); 

				/* formulas given by the TRM, chapter on texture descriptor offsets */
				return (sublevel * GLES_SPACING_BETWEEN_MIPMAPS_CUBEMAPS_TARGETS) + ((((face * roundup_texture_size * roundup_texture_size) * texelsize) + 7) / 8);
			}
		case GLES_TEXTURE_TARGET_2D:
		case GLES_TEXTURE_TARGET_EXTERNAL: 
			return (sublevel * GLES_SPACING_BETWEEN_MIPMAPS_OTHER_TARGETS) ;
		default:
			MALI_DEBUG_ASSERT(0, ("Dimensionality not supported"));
			return 0;
	}
}

/**
 * Accepts one surface specifier for sublevel 0.  
 * Allocates a mipmaps_mem block capable of holding a full set of surfaces like that one. 
 * This memory is assigned to the texmem block, deallocating old memory in the process,
 * and clearing the read only flag. 
 *
 * The function also updates the specifier for each sublevel in the texmem struct. 
 *
 * @return MALI_ERR_NO_ERROR on success
 *         MALI_ERR_OUT_OF_MEMORY if OOM.
 */
MALI_STATIC mali_err_code allocate_mipmaps_mem_based_on_specifier(struct gles_fb_texture_memory* texmem, mali_surface_specifier* lvl_0_spec)
{
	unsigned int i; 
	u32 mem_size;

	MALI_DEBUG_ASSERT_POINTER(texmem);
	MALI_DEBUG_ASSERT(!texmem->flag_dont_replace || texmem->flag_renderable, ("Cannot rebuffer the texmem"));
	texmem->flag_read_only = MALI_FALSE; /* falsified as a part of rebuffering */

	/* remove old memory if present */
	if(texmem->mipmaps_mem) 
	{
		_mali_shared_mem_ref_owner_deref(texmem->mipmaps_mem);
		texmem->frame_id_last_added = MALI_BAD_FRAME_ID; /* new memory assigned, so invalidate the old memory tracker */
		texmem->mipmaps_mem = NULL;
	}

	/* we got the lvl 0 spec, save it in the texmem obj */
	texmem->mipmaps_format[0] = *lvl_0_spec; /* struct copy */

	/* generate the specs for the other levels */
	for(i = 1; i < texmem->sublevels; i++)
	{
		_mali_surface_specifier_mipmap_reduce(&texmem->mipmaps_format[i], &texmem->mipmaps_format[i-1]);
	}
			
	/* now generate a memory block large enough to hold all possible surface data.
	 * We basically calculate the size of one face, and multiply by number of faces and levels. 
	 * Note that very small faces need larger than expected offsets. */
	mem_size = _mali_surface_specifier_datasize(lvl_0_spec);
	switch(texmem->dimensionality)
	{
		case GLES_TEXTURE_TARGET_CUBE:     
			mem_size = MAX(mem_size * texmem->faces, GLES_SPACING_BETWEEN_MIPMAPS_CUBEMAPS_TARGETS); 
			break;
		case GLES_TEXTURE_TARGET_2D:       /* fall through */
		case GLES_TEXTURE_TARGET_EXTERNAL: 
			MALI_DEBUG_ASSERT(texmem->faces == 1, ("Only cubemaps have > 1 face"));
			mem_size = MAX(mem_size, GLES_SPACING_BETWEEN_MIPMAPS_OTHER_TARGETS); 
			break;
		default: MALI_DEBUG_ASSERT(0, ("unknown dimensionality")); return MALI_ERR_FUNCTION_FAILED;
	}
	mem_size *= texmem->sublevels;

#ifdef HARDWARE_ISSUE_9427
	/* Hardware writes outside the given memory area; increase size of memory area in cases where this can happen */
	if( texmem->mipmaps_format[0].pixel_format != MALI_PIXEL_FORMAT_NONE && /* writeable */
		texmem->mipmaps_format[0].pixel_layout == MALI_PIXEL_LAYOUT_LINEAR &&  /* linear */
		((texmem->mipmaps_format[0].width % 16) != 0) ) /* width not divisible by 16 */
	{
		mem_size += HARDWARE_ISSUE_9427_EXTRA_BYTES;
	}
#endif
	texmem->mipmaps_offset = 0;
	texmem->frame_id_last_added = MALI_BAD_FRAME_ID; /* new memory assigned, so invalidate the old memory tracker */
	texmem->mipmaps_mem = _mali_shared_mem_ref_alloc_mem(texmem->base_ctx, mem_size, MALI200_SURFACE_ALIGNMENT, MALI_PP_WRITE | MALI_PP_READ | MALI_CPU_READ | MALI_CPU_WRITE );
	if(texmem->mipmaps_mem == MALI_NO_HANDLE) return MALI_ERR_OUT_OF_MEMORY; /* OOM */

	return MALI_ERR_NO_ERROR;	
}


mali_surface* _gles_fb_texture_memory_allocate(struct gles_fb_texture_memory* texmem, unsigned int face, unsigned int sublevel, mali_surface_specifier* spec)
{
	mali_surface* new_surface;

	/* validate input */
	MALI_DEBUG_ASSERT_POINTER(texmem);
	MALI_DEBUG_ASSERT(face < texmem->faces, ("Input error: face is out of range. Is %i, max is %i", face, texmem->faces-1));
	MALI_DEBUG_ASSERT(sublevel < texmem->sublevels, ("Input error: sublevel is out of range. Is %i, max is %i", sublevel, texmem->sublevels-1));
	MALI_DEBUG_ASSERT_POINTER(spec);

	/**
	 * If this texmem block only holds one surface
	 * Or if we're HW constraint resolve blocked anyway (no point in even trying)
	 * Or if the existing block is flagged as "read only" (meaning we cannot allocate new surfaces off it, as those surfaces need to be filled with data)
	 * Or if there is an allocated memory block, but the input surface spec doesn't fit the curent surface specifier 
	 * Then just allocate something somewhere. Assign will have to deal with the headache. */
	if(texmem->sublevels == 1 && texmem->faces == 1)                                                        goto trivial_allocation;
	if(texmem->flag_read_only)                                                                              goto trivial_allocation;
	if(texmem->flag_need_constraint_resolve)                                                                goto trivial_allocation;
	if(texmem->mipmaps_mem && _mali_surface_specifier_cmp(spec, &texmem->mipmaps_format[sublevel]) != 0)    goto trivial_allocation;

	/* If there is no memory block in place, allocate one fitting the input specifier.
	 * This path will be taken the first time anyone allocates anything.*/
	if(texmem->mipmaps_mem == MALI_NO_HANDLE)
	{
		mali_err_code err;
			
		/* If we don't have the lvl 0 specifier, this is gonna get tricky as we will have to guess - and probably fail - on the lvl 0 texture size.  
		 * If so, this scenario is best resolved through the constraint resolver. */
		if(sublevel != 0) goto trivial_allocation;

		err = allocate_mipmaps_mem_based_on_specifier(texmem, spec);
		if(err != MALI_ERR_NO_ERROR) return NULL;
			
		/* verify that we got the right spec in the right position */
		MALI_DEBUG_ASSERT(_mali_surface_specifier_cmp(spec, &texmem->mipmaps_format[sublevel]) == 0, ("These should always be equivalent"));

	}
	
	/* we know that the new allocation cat fit into the memory block, so just allocate a surface off that memory */
	MALI_DEBUG_ASSERT_POINTER(texmem->mipmaps_mem) ; /* at this point, the memory must be there */
	MALI_DEBUG_ASSERT(_mali_surface_specifier_cmp(spec, &texmem->mipmaps_format[sublevel]) == 0, ("Specifier isn't compatible ? This was checked just earlier"));


	new_surface = _mali_surface_alloc_ref( MALI_SURFACE_FLAGS_NONE, spec, texmem->mipmaps_mem, calculate_offset(texmem, face, sublevel), texmem->base_ctx);
	if(new_surface == NULL) return NULL; /* OOM */
		
	/* the surface alloc function eats one reference if successful, so need to counteract this by addref'ing */
	_mali_shared_mem_ref_owner_addref(texmem->mipmaps_mem);
	
	return new_surface;

trivial_allocation:
	/* allocate a new surface */
	new_surface = _mali_surface_alloc(MALI_SURFACE_FLAGS_NONE, spec, 0, texmem->base_ctx);
	if(new_surface == NULL) return NULL;

	/* and just return it. Whether it fits or not is a problem for assign() */
	return new_surface;

}

void _gles_fb_texture_memory_assign(struct gles_fb_texture_memory* texmem, unsigned int face, unsigned int sublevel, mali_surface* surf)
{
	/* validate input */
	MALI_DEBUG_ASSERT_POINTER(texmem);
	MALI_DEBUG_ASSERT(face < texmem->faces, ("Input error: face is out of range. Is %i, max is %i", face, texmem->faces-1));
	MALI_DEBUG_ASSERT(sublevel < texmem->sublevels, ("Input error: sublevel is out of range. Is %i, max is %i", sublevel, texmem->sublevels-1));
	MALI_DEBUG_ASSERT_POINTER(surf); /* this function is NOT used to unassign surfaces. Handled on a higher level */

	/* access lock the input surface. No external changes to the surface should be possible while the access lock is held, meaning
	 * we can extract memory pointers and offsets from it safely. */
	_mali_surface_access_lock(surf);

	MALI_DEBUG_ASSERT_POINTER(surf->mem_ref); /* surface must have memory; cannot assign a surface without memory. */
	
	/**
	 * Assign will, by nature, replace memory unless the input memory is already a fit.  
	 * Replacing memory is only allowed if renderable or replaceable is set. To be a fit, the pointer and offset must be correct. 
	 * If neither is true, calling assign() is not legal, and the caller must preempt this by doing a deep copy to clear the dont_replace flag. */ 
	MALI_DEBUG_ASSERT( (!texmem->flag_dont_replace) || (texmem->flag_renderable) || 
	                   (surf->mem_ref == texmem->mipmaps_mem && surf->mem_offset == texmem->mipmaps_offset + calculate_offset(texmem, face, sublevel)), 
					   ("Cannot replace non-renderable dontmove memory"));

	/* just one level to replace? Then just replace it. We already verified that this is safe by the assert above. */
	if(texmem->faces == 1 && texmem->sublevels == 1)
	{
		/* update mipmaps mem assign */
		_mali_shared_mem_ref_owner_addref(surf->mem_ref);
		if(texmem->mipmaps_mem) _mali_shared_mem_ref_owner_deref(texmem->mipmaps_mem);
		texmem->mipmaps_offset = surf->mem_offset;
		texmem->frame_id_last_added = MALI_BAD_FRAME_ID; /* new memory assigned, so invalidate the old memory tracker */
		texmem->mipmaps_mem = surf->mem_ref;

		/* update mipmaps mem specifier */
		texmem->mipmaps_format[0] = surf->format; /* struct copy */
	
		/* update mali_surface assign */
		/* _mali_surface_addref(surf); */ /* function eats one surface reference, so this line does not happen */
		if(texmem->surfaces[face][sublevel]) _mali_surface_deref(texmem->surfaces[face][sublevel]);
		texmem->surfaces[face][sublevel] = surf;

		_mali_surface_access_unlock(surf);
		return;
	}

	/* If there is a memory block, and it is the same format as the mali_surface, and the mali_surface is stored on the right location,
	 * just assign the surface without flagging any constraint resolver. No work needed on the mipmaps_mem block.  */
	if(   texmem->mipmaps_mem != NULL                                                                 /* previous memory exists (created in alloc()! )*/
	   && texmem->mipmaps_mem == surf->mem_ref                                                        /* same memory */
	   && _mali_surface_specifier_cmp(&surf->format, &texmem->mipmaps_format[sublevel]) == 0      /* same format */
	   && surf->mem_offset == calculate_offset(texmem, face, sublevel) + texmem->mipmaps_offset)  /* proper offset */
	{
		/* update the surface assign */
		/* _mali_surface_addref(surf); */ /* function eats one surface reference, so this line does not happen */
		if(texmem->surfaces[face][sublevel]) _mali_surface_deref(texmem->surfaces[face][sublevel]);
		texmem->surfaces[face][sublevel] = surf;

		
		/* no updates to texmem->mipmaps_mem needed, everything is already in the right slot */
		_mali_surface_access_unlock(surf);
		return;
	}

	/* otherwise, we have a constraint resolve issue. Flag the constraint resolver, but assign the surface */

	/* update the surface assign */
	/* _mali_surface_addref(surf); */ /* function eats one surface reference, so this line does not happen */
	if(texmem->surfaces[face][sublevel]) _mali_surface_deref(texmem->surfaces[face][sublevel]);
	texmem->surfaces[face][sublevel] = surf;
		
	/* set constraint resolver flag */
	texmem->flag_need_constraint_resolve = MALI_TRUE;

	_mali_surface_access_unlock(surf);
}

/**
 * Copies data from the source @ source_offset into the memory in texmem. 
 * The copy operation will be stored in the texmem's current memory block, at the offset given by the face and sublevel. 
 *
 * The surf* parameter (which is not a parameter, but just fetched from the texmem object) holds the data container for the 
 * surface, but the memory in the surface may have been replaced by a later copy-on-write or similar operation. For this 
 * reason, the memory at the time of the "snapshot" is also passed into the function. This is the actual memory we want to 
 * read from. 
 *
 */
MALI_STATIC void do_copy_on_read(struct gles_fb_texture_memory* texmem, unsigned int face, unsigned int sublevel, mali_shared_mem_ref* source, u32 source_offset)
{
	mali_surface* surf;
	u32 dst_offset;
	u32 datasize;

	MALI_DEBUG_ASSERT_POINTER(texmem);
	MALI_DEBUG_ASSERT_POINTER(texmem->mipmaps_mem); /* must exist at this point */
	MALI_DEBUG_ASSERT_POINTER(source);
	
	surf = texmem->surfaces[face][sublevel];
	
	MALI_DEBUG_ASSERT_POINTER(surf);
	
	datasize = surf->datasize;
	dst_offset = texmem->mipmaps_offset + calculate_offset(texmem, face, sublevel); 

	if(texmem->flag_renderable)
	{
		/* Renderable buffers may have pending writes, and if we just memcpy without involving the depsystem, 
		 * we may not be copying the right data. 
		 *
		 * It is safe to wait for such copies... but this is effectively doing a blocking wait.
		 * To avoid this wait, we instead want to set up a scheme for deferring such CPU operations until the 
		 * depsystem deems it ready. 
		 *
		 * JIRA ticket MJOLL-2841 tracks this issue.
		 */
		_mali_mem_copy( texmem->mipmaps_mem->mali_memory, dst_offset, source->mali_memory, source_offset, datasize);

	} else {
		/* normal surfaces always have the correct content right now, and can just be copied immediately */
		_mali_mem_copy( texmem->mipmaps_mem->mali_memory, dst_offset, source->mali_memory, source_offset, datasize);
	}


	/* next, update the surface to point to the new memory block.
	 * Note: surfaces tagged as DONTMOVE cannot be moved. 
	 * These surfaces will be kept as-is, but 
	 * that means that this copy-on-read operation will have to happen
	 * on every single drawcall using it. */
	if(! surf->flags & MALI_SURFACE_FLAG_DONT_MOVE )
	{
		_mali_surface_access_lock(surf);

		/* we re-lock the surface, after the snapshot operation is done. 
		 * Since the snapshot, the surface may have changed memory. This can happen
		 * if some other instance did a copy-on-write or copy-on-read. 
		 *
		 * If it was a CoW, then that should take presedence. 
		 * If it was a CoR, then we don't know if there also was a CoW somewhere, so
		 * we just keep it as-is, entering the same path as a DONTMOVE surface. 
		 * This issue will then be resolved the next time we do this CoR business. 
		 *
		 * Either way: if the surface is different than what it was, keep it as is. 
		 */
		if(surf->mem_ref == source)
		{
			_mali_shared_mem_ref_owner_addref(texmem->mipmaps_mem); /* give texture memory to surface */
			_mali_shared_mem_ref_owner_deref(surf->mem_ref); /* old surface memory is derefed */
			surf->mem_ref = texmem->mipmaps_mem;
			surf->mem_offset = dst_offset;
		}
		_mali_surface_access_unlock(surf);
	}


}


mali_err_code _gles_fb_texture_memory_resolve_internal(struct gles_fb_texture_memory* texmem)
{
	unsigned int verify_levels = 0; 

	/* validate input */
	MALI_DEBUG_ASSERT_POINTER(texmem);
	MALI_DEBUG_ASSERT_POINTER(texmem->surfaces[0][0]); /* the first surface must be available, otherwise we cannot have passed completeness */ 
	
	/* The inlined function has an early out path for this condition, so it cannot be true. */ 
	MALI_DEBUG_ASSERT(texmem->flag_need_constraint_resolve || texmem->flag_renderable, ("One of these bits must be set at this point. Did you call _gles_fb_texture_memory_resolve ?"));

	/**
	 * At this point, the following things are true
	 *  - Every surface has been uploaded for every "used" miplevel, count depending on minfilter and size. (enforced by completeness check)
	 *  - Every used surface has the same red/blue and reverse order flags (enforced by GLES texture object storage format scheme)
	 *  - Every used surface has the same format and layout (enforced by GLES texture object storage format scheme)
	 *
	 * These things can be wrong. 
	 *  #1) The texmem->mipmaps_mem block may not be allocated at all, or be of the wrong size (need constraint resolve bit)
	 *  #2) One or more surfaces are not allocated in contiguous memory (true if #1 is true, otherwise is renderable bit)
	 *
	 * This function seeks to redeem both of these problems. 
	 *
	 * As mentioned in detail in the header file, this function need to resolve the HW constraint requirements. But it cannot
	 * break the SW constraint requirements, nor can it break the deferred renderer constraints. Depending on flags, we are either
	 * allowed to replace memory or allowed to modify memory. 
	 *
	 * It is not possible that the flag_dont_replace condition is set at the same time as the flag_need_constraint_resolve condition. 
	 * The dont_replace flag is only set on drawcalls, and we cannot do a drawcall unless we clear the constraint resolver first.
	 * If we later replace a face in the mipmap level though glTeximage2D or similar, the dont_replace flag will be cleared as a
	 * part of the deep copy on that texmem struct. 
	 *
	 * Also: if the surface is renderable, it is always allowed to replace the memory. 
	 *
	 * And since either the flag_need_constraint_resolve or the flag_renderable is set (see previous assert), 
	 * we are always allowed to replace memory in this function! Let's formalize that as a logical requirement:
	 */
	MALI_DEBUG_ASSERT(texmem->flag_renderable || !texmem->flag_dont_replace, ("Constraint resolver must be allowed to replace memory"));

	/**
	 * That out of the way, let's talk about the implementation strategy of this function.
	 *   - Main goal is to fill in or replace the texmem->mipmaps_mem block so that it is HW readable. 
	 *     What is in the mali_surface ultimately do not matter; though we should seek to keep them in sync to avoid doing this work 
	 *     every single drawcall. The MALI_SURFACE_FLAG_DONT_MOVE may prevent that, but that is okay, as long as the mipmaps_mem block is good. 
	 *
	 * If the memory block does not have a format that matches the surfaces (only need to check one)
	 *  OR
	 * If any of the surface is stored outside the memory block, and the memory block is readonly
	 *   - replace the existing memory block and clear the "flag_read_only" bit.
	 *
	 * At this point we have a writeable mipmaps mem block that fits and the function can work with
	 *   - Each surface should be CoR'ed into this memory if it is not already in the right spot. 
	 *       - This copy must be a depsystem-controlled copy if the renderable bit is set. 
	 *   - If the surface does not allow CoR, just copy the surface content instead, but leave the surface as-is.
	 *     The same work would have to be done on the next frame as well, but that's the cost of not supporting CoR 
	 *     In this case, the flag_renderable but is set anyway. 
	 *       - This copy must be a depsystem-controlled copy if the renderable bit is set. 
	 *
	 * At this point, we have a perfect mipmaps_mem block. 
	 *   - Clear the need_constraint_resolve flag, and return. 
	 *
	 * Special case: 
	 *   - If the mipmaps_block only contains ONE surface, we can really use that surface directly.
	 *     Instead of CoR'ing the surface into a mipmaps_mem block, we point the mipmaps_mem block 
	 *     into the mali_surface->mem_ref. No data copy of changes required.
	 *   - If renderable, this scheme only works if the outlying system adds a read dependency. Which it does, but cannot be asserted. 
	 */

	/**
	 * Final topic: locking. 
	 * To fetch the surface memory from each surface, the surface must be locked. This lock should only happen exactly once per surface
	 * and only one surface should be locked at the same time. Once the memory is refcounted as the mipmaps_mem, the mipmaps_mem
	 * does not need locking to be stored. 
	 */

	/* one surface special case: Just change the texmem block to point to the surface memory. Then it is resolved. */
	if(texmem->faces == 1 && texmem->sublevels == 1) 
	{
		mali_surface* surf = texmem->surfaces[0][0]; 
		MALI_DEBUG_ASSERT_POINTER(surf);

		_mali_surface_access_lock(surf); /* must only be taken once */
		MALI_DEBUG_ASSERT_POINTER(surf->mem_ref);
			
		/* do we need up update any pointers?*/
		if(texmem->mipmaps_mem != surf->mem_ref) /* no point in checking offsets */
		{
			_mali_shared_mem_ref_owner_addref(surf->mem_ref);
			if(texmem->mipmaps_mem) _mali_shared_mem_ref_owner_deref(texmem->mipmaps_mem);
			texmem->mipmaps_offset = surf->mem_offset;
			texmem->frame_id_last_added = MALI_BAD_FRAME_ID; /* new memory assigned, so invalidate the old memory tracker */
			texmem->mipmaps_mem = surf->mem_ref;

			/* also update the format */
			texmem->mipmaps_format[0] = surf->format; /* struct copy */
		}
		_mali_surface_access_unlock(surf);

		texmem->flag_need_constraint_resolve = MALI_FALSE;
		return MALI_ERR_NO_ERROR;
	}

	/* how many miplevels must we look at ? 
	 * Look at miplevels until we run out, or size == 1. 
	 * If sublevel[0] is size 4, then we have log2(4)+1==3 sublevels to deal with*/
	MALI_DEBUG_ASSERT_POINTER(texmem->surfaces[0][0]);
	verify_levels = MIN(texmem->sublevels, _mali_sys_log2(MAX(texmem->surfaces[0][0]->format.width, texmem->surfaces[0][0]->format.height))+1);
	MALI_DEBUG_ASSERT(verify_levels <= texmem->sublevels, ("too many verify_levels"));

	/* Do we need a rebuffer? To do that we need to verify if any pointer is outside their place in the mipmaps_mem block. 
	 *
	 * There is a practical problem here. We cannot check if the surfaces are outside the memory buffer unless we read and extract their pointers.
	 * To do that, they must be locked. But we can only lock one surface at a time or we face a potential deadlock situation. 
	 * And if we extract their pointers, and find that we need to move the surface, we must later lock the surface again to find 
	 * the pointer to read from. Which may have changed, which will be a PITA to deal with! So we really need a step that simply 
	 * extracts all the surfaces and addref's them in a local stack storage. Think of this as snapshotting the memory of the texture 
	 * at this point, and what is snapshotted will be used in the texture read op. */
	{
		struct mali_shared_mem_ref* local_memory[GLES_MAX_MIPCHAINS][GLES_FB_TEXTURE_MEMORY_MAX_SUBLEVELS];
		u32 local_offset[GLES_MAX_MIPCHAINS][GLES_FB_TEXTURE_MEMORY_MAX_SUBLEVELS];
		u32 surface_in_right_spot = MALI_TRUE;
		unsigned int level;
		unsigned int face;

		/* extract all surface memory blocks */
		for(level = 0; level < verify_levels; level++) /* only extract up to the number of levels needed */
		{
			for(face = 0; face < texmem->faces; face++)
			{
				mali_surface* surf = texmem->surfaces[face][level];
				MALI_DEBUG_ASSERT_POINTER(surf); /* surface must exist, or completeness check didn't run */

				_mali_surface_access_lock(surf);
				local_memory[face][level] = surf->mem_ref;
				local_offset[face][level] = surf->mem_offset;
				_mali_shared_mem_ref_owner_addref(local_memory[face][level]);
				_mali_surface_access_unlock(surf);

				/* verify in right spot */
				if(local_memory[face][level] != texmem->mipmaps_mem) surface_in_right_spot = MALI_FALSE;
				if(local_offset[face][level] != texmem->mipmaps_offset + calculate_offset(texmem, face, level)) surface_in_right_spot = MALI_FALSE;
			}
		}

		/* do we need a rebuffer? */
		if( texmem->mipmaps_mem == MALI_NO_HANDLE                          /* no memory allocated */
		    || (!surface_in_right_spot && texmem->flag_read_only)          /* read only memory, but some surfaces in the wrong spot */
		    || _mali_surface_specifier_cmp(&texmem->mipmaps_format[0], &texmem->surfaces[0][0]->format) != 0 ) /* surface format doesn't match the mipmaps_format */
		{
			/* do rebuffer */
			mali_err_code err = allocate_mipmaps_mem_based_on_specifier(texmem, &texmem->surfaces[0][0]->format);
			if(err != MALI_ERR_NO_ERROR)
			{
				/* undo addref in extraction */
				for(level = 0; level < verify_levels; level++) /* only unextract up to the number of levels needed */
				{
					for(face = 0; face < texmem->faces; face++)
					{
						_mali_shared_mem_ref_owner_deref(local_memory[face][level]);
					}
				}
				return err;
			} /* if error on allocation*/
		} /* if rebuffer */

		MALI_DEBUG_ASSERT(_mali_surface_specifier_cmp(&texmem->mipmaps_format[0], &texmem->surfaces[0][0]->format) == 0, ("texmem memory is now the right type/size"));

		/* go through every surface, verify if in right spot, CoR when needed, then deref the stack reference */
		for(level = 0; level < verify_levels; level++) /* only extract up to the number of levels needed */
		{
			for(face = 0; face < texmem->faces; face++)
			{
				u32 expected_offset = texmem->mipmaps_offset + calculate_offset(texmem, face, level);
				if(texmem->mipmaps_mem != local_memory[face][level] ||
				   expected_offset != local_offset[face][level])
				{
					do_copy_on_read(texmem, face, level, local_memory[face][level], local_offset[face][level]);
				}
				_mali_shared_mem_ref_owner_deref(local_memory[face][level]);
			}
		}

		/* and finally clear the constraint resolve flag */
		texmem->flag_need_constraint_resolve = MALI_FALSE;

	} /* end of rebuffer block */
	return MALI_ERR_NO_ERROR;
}


mali_err_code _gles_fb_texture_memory_draw_internal_renderable(struct gles_fb_texture_memory* texmem, mali_frame_builder* frame_builder, mali_bool* changed_memory)
{
	mali_err_code err;
	mali_base_frame_id frame_id;
	struct mali_shared_mem_ref* old_memory;
	unsigned int face, level;
	
	MALI_DEBUG_ASSERT_POINTER(texmem);
	MALI_DEBUG_ASSERT_POINTER(frame_builder);
	MALI_DEBUG_ASSERT_POINTER(changed_memory);
	MALI_DEBUG_ASSERT(texmem->flag_renderable, ("The internal_renderable function requires this to be set"));
	
	old_memory = texmem->mipmaps_mem;
	
	/* go through all surfaces and trigger the MALI_SURFACE_EVENT_GPU_TEXTURE, as well as a read dependency on each.  */
	for(face = 0; face < texmem->faces; face++)
	{
		for(level = 0; level < texmem->sublevels; level++)
		{
			mali_surface* surf = texmem->surfaces[face][level];
			if(surf)
			{
				_mali_surface_trigger_event(surf, NULL, MALI_SURFACE_EVENT_UPDATE_TEXTURE);
				err = _mali_frame_builder_add_surface_read_dependency(frame_builder, surf);	/* May trigger a COW */
				if(err != MALI_ERR_NO_ERROR) return err;
			}
		}
	}


	/* do a resolve at this point, we may need to move surfaces  */
	err = _gles_fb_texture_memory_resolve(texmem);
	if(err != MALI_ERR_NO_ERROR) return err;

	/* see if that changed memory? */
	if(texmem->mipmaps_mem != old_memory) *changed_memory = MALI_TRUE; 

	/* add texmem to frame, if not done so already */
	frame_id = _mali_frame_builder_get_current_frame_id(frame_builder);
	if(texmem->frame_id_last_added != frame_id)
	{
		err = _mali_frame_builder_add_callback( frame_builder,
	                                        (mali_frame_cb_func)_mali_shared_mem_ref_usage_deref,
    	                                    (mali_frame_cb_param)texmem->mipmaps_mem);
		if (MALI_ERR_NO_ERROR != err)
		{
			MALI_DEBUG_ASSERT(err == MALI_ERR_OUT_OF_MEMORY, ("inconsistent error code returned (got 0x%X, expected 0x%X)", err, MALI_ERR_OUT_OF_MEMORY));
			return err;
		}
		_mali_shared_mem_ref_usage_addref( texmem->mipmaps_mem );
			
		/* and update the frame id */
		texmem->frame_id_last_added = frame_id;
	}


	return MALI_ERR_NO_ERROR;

}


