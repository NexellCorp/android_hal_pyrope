/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_FB_TEXTURE_MEMORY_H_
#define GLES_FB_TEXTURE_MEMORY_H_

#include "../gles_base.h"
#include "../gles_texture_object.h"
#include <shared/mali_surface_specifier.h>
#include <base/mali_memory.h>
#include <shared/mali_shared_mem_ref.h>
#include <shared/m200_gp_frame_builder.h>
#include <shared/m200_gp_frame_builder_inlines.h>

/* max number of supported sublevels */
#define GLES_FB_TEXTURE_MEMORY_MAX_SUBLEVELS 3

/**
 * This file holds a structure describing a "texture memory block". Or texmem for short here after. 
 *
 * The HW has some constraints for textures. Where a GLES texture object has 13 miplevels and 6 faces at most, 
 * the Mali TD only has 10 pointers. The first 9 pointers point to a memory block holding 6 faces worth
 * of texture data for the matching miplevel. The last pointer holds all the remaining data. 
 *
 * Sorting out this is tricky, and what constraint resolving is all about. To simplify the headache, 
 * breaking up the problem is a good start, and this is what this struct attempts to do. Each
 * texture backend object has 10 of these structures, each responsible for providing the matching 
 * TD pointer. Within each texmem structure, the relevant surfaces are added, and constraint resolved into 
 * a single memory block readable by the hardware. This is the main job of this module. 
 *
 * --
 *
 * The driver also has a constraint. It does not want to addref and deref more surfaces than necessary. 
 * If a drawcall uses a texture, addrefing 13*6 surfaces will eat up a whole lot of CPU. Even addrefing 
 * 10 memory pointers is too much work. So instead, it attempts to only addref the "texture backend object". 
 *
 * For static textures, this works perfectly. Addrefing the backend object implicitly addref's all the
 * surfaces and texmem stored within. This way, multiple addref's are replaced with a single addref.
 *
 * But some textures are modified a lot during execution. Miplevels are replaced by calling glTexImage2D, 
 * and anything renderable by the GPU tend to be riddled with copy-on-writes. And since we addref 
 * the entire texture backend object, there is no way to modify this after it is addref'ed. 
 *
 * The solution for this is a "deep copy". We simply create a new texture backend object, referring to
 * the same surfaces as the old one, minus the one we want to replace. This operation isn't really
 * doing any copying, but gives us a new texture backend object which that noone is using. And which 
 * thus can be modified. At least until the texture is used in another drawcall. 
 *
 * One exception to this desire: any gpu-renderable surfaces will always be addref'ed in every drawcall. 
 *
 * --
 *
 * Finally, there is a third constraint from being a deferred renderer. If a memory block has been used
 * by the GPU at some point, we cannot modify the content of that memory. For GPU-renderable memory
 * chunks, this means we can never modify the memory content at all because we just don't know if it
 * has been used by the GPU at any point. For other memory chunks, modification cannot occur after the 
 * GPU has set up any drawcall reading from a memory block. 
 * 
 * --
 *
 * All of this is incredibly relevant to this module. The HW constraints require us to move data to 
 * GPU-readable contiguous memory chunks. The SW constraints require us to not replace any buffers in the general case. 
 * The deferred constraints prevent us from modifying memory. These three constraints work against eachother. 
 *
 * To resolve this headache we need some state. 
 *
 * Each texmem struct has four states internally to deal with these constraints. 
 *
 *  - flag_read_only
 *    Tracks whether the GPU has read the memory block at some point. 
 *    Set on drawcalls, never cleared. To clear, a miplevel replacement (glTexImage2D or similar) is required.  
 *    While set, modifying the memory content is not legal. 
 *    While unset, modifying the memory content is always legal (see flag_renderable for exception)
 *
 *  - flag_dont_replace
 *    Tracks whether the memory can be replaced. Generally memory cannot be replaced once the texture backend
 *    object has been addref'ed to a frame, as replacing memory means losing that memory. 
 *    Set on drawcalls. Cleared on "deep copies", but only on the copy. 
 *    While set, replacing the memory content is illegal. (see flag_renderable for exception)
 *    While unset, replacing the memory is okay. 
 *
 *  - flag_renderable
 *    Tracks whether the memory is GPU writeable. This means it can be CoW'ed, and/or read from
 *    outside of the control of the GLES driver. if this is set, a constraint resolve is always needed. 
 *    Memory flagged this way is also addref'ed individually by all drawcalls using it, so replacing such memory
 *    blocks is always allowed. But modifying such memory blocks is never allowed. 
 *
 *  - flag_need_constraint_resolve
 *    Tracks whether the memory is HW compatible. Rebuffering required before the HW can read it if this is not the case. 
 *    The driver will attempt to allocate texture memory so that this is not set in the first place, 
 *    but the app may make that impossible. 
 *    Set on assign(), cleared on resolve(). 
 *    If this is set, no (successful) drawcall has yet happened, so this state cannot be set at the same time as 
 *    either of the previous two states. 
 *
 *    Do note: this is a bit, but technically constraint resolving happens up to only the largest used level. 
 *    The largest used level can change, but not without replacing mipmap levels, which will trigger a new 
 *    need_constraint_resolve. So one bit for this is sufficient. 
 *
 **/


/* structure holding all memory within a mipmap level */
struct gles_fb_texture_memory
{
	/* these fields are immutable */
	enum gles_texture_target dimensionality;	
	unsigned int faces;
	unsigned int sublevels;
	mali_base_ctx_handle base_ctx;

	/* an array of faces * sublevels mali_surfaces.
	 * The surfaces are the mali_surfaces attached to this miplevel */
	struct mali_surface* surfaces[GLES_MAX_MIPCHAINS][GLES_FB_TEXTURE_MEMORY_MAX_SUBLEVELS];

	/* The memory held by this struct. 
	 * This memory's mali_addr + offset is what will be written to the TD */
	struct mali_shared_mem_ref* mipmaps_mem;
	u32 mipmaps_offset; /* usually 0, but sometimes external memory blocks have a > 0 offset */
	
	/* The memory has a single surface format. 
	 * Since the width/height will differ from sublevel to sublevel, 
	 * we store one specifier for each sublevel in the memory block */
	struct mali_surface_specifier mipmaps_format[GLES_FB_TEXTURE_MEMORY_MAX_SUBLEVELS]; 

	/* The read-only bit state. Set on the first drawcall. Cleared if the memory is replaced. */ 
	mali_bool flag_read_only; 

	/* The dont replace bit state. Set on the first drawcall. Cleared on the copy after a "deep copy" */
	mali_bool flag_dont_replace; 

	/* The need constraint resolve bit. Set on assign() if failing, cleared on resolve() */
	mali_bool flag_need_constraint_resolve; 

	/* The renderable bit. Set on assign() if miplevel is renderable. Never cleared once set. */
	mali_bool flag_renderable; 

	/* We only want to addref memory to a frame once, so this tracks the last frame ID added */
	mali_base_frame_id frame_id_last_added;

};

/**
 * @brief Initializes a texmem block from an undefined state.  
 *
 * @param texmem - The memory block to initialize
 * @param dimensionality - Texture dimensionality, used to determine number of faces
 * @param sublevels - Set to 1 for all miplevels, except level 10, which is set to 3. 
 *                    This parameter tracks how many possible miplevels can be attached to this texmep.
 *                    HW constraints mandate that level 10 thourgh 12 is packed into one memory block.
 * @param base_ctx - The base context, stored off for allocation reasons.
 */
void _gles_fb_texture_memory_init(struct gles_fb_texture_memory *texmem, enum gles_texture_target dimensionality, unsigned int sublevels, mali_base_ctx_handle base_ctx);

/**
 * @brief Resets a texmem block from an defined state, cleaning up all memory attached to the structure.  
 * @param texmem - The memory block to reset 
 */
void _gles_fb_texture_memory_reset(struct gles_fb_texture_memory *texmem);

/**
 * @brief Allocates a mali_surface (given a specifier) that will fit into the texmem block seamlessly
 * @param texmem - The memory container to allocate the surface into
 * @param face - Which face should this surface fit into? Must be less than the constructor "faces"
 * @param sublevel - Which sublevel should this surface fit into? Must be less than the constructor "sublevels"
 * @param spec - Surface specified, defining the surface properties.
 * @return The allocated surface with the memory assigned, or NULL on failure. 
 *
 * @note Allocating a surface will not assign the surface to the memory block.
 */
struct mali_surface* _gles_fb_texture_memory_allocate(struct gles_fb_texture_memory* texmem, unsigned int face, unsigned int sublevel, mali_surface_specifier* spec);

/**
 * @brief Assigns a mali_surface to a memory block. Should the surface memory not fit into the existing mipmaps_mem, the constraint dirty flag will be set. 
 * @param texmem - The memory container to assign the surface into. 
 * @param face - The face to assign the surface into
 * @param sublevel - The sublevel to assign the surface into
 * @param surf - The surface to assign. 
 *
 * @note: this function will reference track the input surface, and will eat one reference. 
 */
void _gles_fb_texture_memory_assign(struct gles_fb_texture_memory* texmem, unsigned int face, unsigned int sublevel, struct mali_surface* surf);

/**
 * @brief Removes a mali_surface from a texmem block. The memory itself remains in place. 
 * @param texmem - The texture memory block to modify
 * @param face - The face to unassign
 * @param sublevel - The sublevel to unassign 
 */
MALI_STATIC_INLINE void _gles_fb_texture_memory_unassign(struct gles_fb_texture_memory* texmem, unsigned int face, unsigned int sublevel)
{
	mali_surface* surf;
	MALI_DEBUG_ASSERT_POINTER(texmem);
	MALI_DEBUG_ASSERT(face < texmem->faces, ("face index too large. Is %i, expected less than %i", face, texmem->faces));
	MALI_DEBUG_ASSERT(sublevel < texmem->sublevels, ("sublevel index too large. Is %i, expected less than %i", sublevel, texmem->sublevels));
	
	surf = texmem->surfaces[face][sublevel];
	if(surf)
	{
		_mali_surface_deref(surf);
		texmem->surfaces[face][sublevel] = NULL;
	}
}

/**
 * Flags the texture memory as renderable. Renderable memory is individually tracked by the depsystem. 
 * @param texmem - The texture memory to modify
 */
MALI_STATIC_INLINE void _gles_fb_texture_memory_set_renderable(struct gles_fb_texture_memory* texmem)
{
	MALI_DEBUG_ASSERT_POINTER(texmem);
	texmem->flag_renderable |= MALI_TRUE; /* set, but never cleared */
}

/**
 * Fetches the texture memory renderable state.
 * @param texmem - The texture memory to query
 * @return MALI_TRUE if renderable, otherwise MALI_FALSE
 */
MALI_STATIC_INLINE mali_bool _gles_fb_texture_memory_get_renderable(struct gles_fb_texture_memory* texmem)
{
	MALI_DEBUG_ASSERT_POINTER(texmem);
	return texmem->flag_renderable;
}

/**
 * Resolves all constraint resolve issues on this texmem block, by copying and moving surfaces into the correct memory locations. 
 *
 * @param texmem - The memory container to resolve
 * @return MALI_ERR_NO_ERROR or MALI_ERR_OUT_OF_MEMORY on oom. 
 *
 * @note Will trigger an assert if the surfaces cannot be moved due to differences in formats, layouts etc. 
 *       This function should only be called after a texture completeness check to ensure this is okay. 
 *
 * @note Splitting this function in an inlined and normal function to speed up the common usecase. Do not call the internal 
 *       function directly. 
 */
mali_err_code _gles_fb_texture_memory_resolve_internal(struct gles_fb_texture_memory* texmem);
MALI_STATIC_INLINE mali_err_code _gles_fb_texture_memory_resolve(struct gles_fb_texture_memory* texmem)
{
	MALI_DEBUG_ASSERT_POINTER(texmem);
	/* if we do not need constraint resolving, just early out */
	if(!texmem->flag_need_constraint_resolve && !texmem->flag_renderable) return MALI_ERR_NO_ERROR;	
	return _gles_fb_texture_memory_resolve_internal(texmem);
}

/**
 * Returns whether the memory block defined by a texmem object can be used by a TD
 * @param texmem - The texture memory block to query
 * @return MALI_TRUE if it can be used. MALI_FALSE otherwise. 
 *
 * To be tagged as valid, all uploaded textures must fit, and there must be a memory block. 
 * This is not the same as a completeness check, which also verifies that all miplevels are uploaded. 
 */
MALI_STATIC_INLINE mali_bool _gles_fb_texture_memory_valid(const struct gles_fb_texture_memory* texmem)
{
	MALI_DEBUG_ASSERT_POINTER(texmem);
	/* to be valid, the following things must be true: */
	return (
		texmem->mipmaps_mem                   /* the memory block must exist */
		&& 
		!texmem->flag_need_constraint_resolve /* no pending constraint resolve must be waiting */
		);
}

/**
 * @brief Returns whether a memory block is renderable or not. 
 *
 * @param texmem - The texmem block to query
 * @return MALI_TRUE if renderable, MALI_FALSE otherwise. 
 */
MALI_STATIC_INLINE mali_bool _gles_fb_texture_memory_renderable(const struct gles_fb_texture_memory* texmem)
{
	MALI_DEBUG_ASSERT_POINTER(texmem);
	return texmem->flag_renderable;	
}

/**
 * @brief Fetches the mali address that this object represents, at the current time
 *
 * @param texmem - The memory container to query
 * @return The mali address that can be written to a TD.
 *
 * @note: the read only and dont replace flags will be set as a part of this call. 
 */
MALI_STATIC_INLINE mali_addr _gles_fb_texture_memory_get_addr(const struct gles_fb_texture_memory* texmem)
{
	MALI_DEBUG_ASSERT_POINTER(texmem);

	/** 
	 * The memory pointer is fetched twice. 
	 * Once in the statepushing scheme, where the mem_ref may or may not be allocated properly - yet. 
	 * And once more in the drawcall, but only in case of constraints resolve is required. 
	 * But in both cases, the call should only happen if the memory is valid. 
	 */
	MALI_DEBUG_ASSERT(_gles_fb_texture_memory_valid(texmem), ("Cannot fetch memory from invalid texture memory - should never be called"));
	MALI_DEBUG_ASSERT_POINTER(texmem->mipmaps_mem); /* no memory allocated; never called glTexImage2D or equiv. Should never have passed completeness.  */
	
	return _mali_mem_mali_addr_get(texmem->mipmaps_mem->mali_memory, texmem->mipmaps_offset); 
}

/**
 * @brief Flag the texmem as "being used by a hw job". 
 *        This should happen on glDraw when using the texture. 
 *        For performance reasons, it only really have to happen once, 
 *        so it should be called when creating a mali_td based on the texture
 *        object.
 *
 *        The exception is for renderable surfaces, where this must happen
 *        on every drawcall. A refcount scheme should prevent the memory
 *        form being addref'ed to the same frame more than once. 
 *
 * @param texmem - The object to flag
 * @param frame_builder - The framebuilder to draw to
 * @param changed_memory [OUT] - Output parameter. MALI_TRUE if the texmem changed memory as a part of this draw operation. 
 *
 * @return MALI_ERR_NO_ERROR on success, or MALI_ERR_OUT_OF_MEMORY on oom. 
 */
mali_err_code _gles_fb_texture_memory_draw_internal_renderable(struct gles_fb_texture_memory* texmem, mali_frame_builder* frame_builder, mali_bool* changed_memory);
MALI_STATIC_INLINE mali_err_code _gles_fb_texture_memory_draw(struct gles_fb_texture_memory* texmem, mali_frame_builder* frame_builder, mali_bool* changed_memory)
{
	MALI_DEBUG_ASSERT_POINTER(texmem);
	MALI_DEBUG_ASSERT_POINTER(changed_memory);
	
	*changed_memory = MALI_FALSE;

	texmem->flag_read_only = MALI_TRUE;
	texmem->flag_dont_replace = MALI_TRUE;

	if(texmem->flag_renderable)
	{
		return _gles_fb_texture_memory_draw_internal_renderable(texmem, frame_builder, changed_memory);
	} else {
		/* resolve should already have happened */
		MALI_DEBUG_ASSERT(_gles_fb_texture_memory_valid(texmem), ("Cannot draw with invalid memory"));
	}
	return MALI_ERR_NO_ERROR;
}

/**
 * @brief Returns the specifier of a surface (matching sublevel 0). 
 * @param texmem - The texmem block to query
 * @return A surface specifier. Will never be NULL.
 */
MALI_STATIC_INLINE const mali_surface_specifier* _gles_fb_texture_memory_get_specifier(const struct gles_fb_texture_memory* texmem)
{
	MALI_DEBUG_ASSERT_POINTER(texmem);
	MALI_DEBUG_ASSERT(_gles_fb_texture_memory_valid(texmem), ("Cannot fetch specifier from invalid texture memory - should never be called"));
	return &texmem->mipmaps_format[0];
}

/**
 * @brief Returns the surface at a given sublevel and face. 
 * @param texmem - The texmem block to query
 * @param face - the face to query
 * @param sublevel - The sublevel to query
 * @return A surface specifier. Will be NULL if no surface was uploaded. 
 */
MALI_STATIC_INLINE struct mali_surface* _gles_fb_texture_memory_get_surface(const struct gles_fb_texture_memory* texmem, unsigned int face, unsigned int sublevel)
{
	MALI_DEBUG_ASSERT_POINTER(texmem);
	MALI_DEBUG_ASSERT(face < texmem->faces, ("face index too large. Is %i, expected less than %i", face, texmem->faces));
	MALI_DEBUG_ASSERT(sublevel < texmem->sublevels, ("sublevel index too large. Is %i, expected less than %i", sublevel, texmem->sublevels));
	return texmem->surfaces[face][sublevel];
}

/**
 * @brief Copy one data container into another. The entire dst container will be overwritten
 *
 * @param dst - the location to store the copy
 * @param src - the location to copy
 */
MALI_STATIC_INLINE void _gles_fb_texture_memory_copy( struct gles_fb_texture_memory* dst, struct gles_fb_texture_memory* src)
{
	unsigned int face, level;

	MALI_DEBUG_ASSERT_POINTER(src);
	MALI_DEBUG_ASSERT_POINTER(dst);


	*dst = *src; /* struct copy */

	/* addref output memory */
	if(dst->mipmaps_mem) _mali_shared_mem_ref_owner_addref(dst->mipmaps_mem);

	/* addref all surfaces */
	for(face = 0; face < dst->faces; face++)
	{
		for(level = 0; level < dst->sublevels; level++)
		{
			if(dst->surfaces[face][level]) _mali_surface_addref(dst->surfaces[face][level]);
		}
	}

	/* it is now safe to replace this memory */
	dst->flag_dont_replace = MALI_FALSE;
	
}


#endif /* GLES_FB_TEXTURE_MEMORY_H */
