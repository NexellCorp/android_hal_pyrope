/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_FB_TEXTURE_OBJECT_H
#define GLES_FB_TEXTURE_OBJECT_H

#include "../gles_base.h"
#include "mali_system.h"
#include "shared/m200_texture.h"
#include "base/mali_memory.h"
#include "../gles_config.h"
#include "../gles_texture_object.h"
#include <shared/mali_surface_specifier.h>
#include "gles_fb_texture_memory.h"


/**
 * @defgroup ForwardDeclarations Forward declare structures.
 * We're not using the pointers here, but we need to pass them around
 * @{
 */
struct mali_surface;
struct gles_texture_object;
struct mali_frame_builder;
struct mali_surface_specifier;
struct egl_image;
/** @} */

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
#define GLES_FB_TDS_COUNT 3
#else
#define GLES_FB_TDS_COUNT 1
#endif

/* The "backend texture object" - aka a texture instance */
struct gles_fb_texture_object
{
	enum gles_texture_target         dimensionality;
	mali_base_ctx_handle             base_ctx;

	struct gles_fb_texture_memory    texmem[MALI_TD_MIPLEVEL_POINTERS][GLES_FB_TDS_COUNT]; /**< Each holds a memory block whose address will be assigned to the TD */

	unsigned int                     num_uploaded_surfaces;                     /**< The number of pointers in the texmem blocks that are not NULL */
	unsigned int                     used_planes;                               /**< How many planes will be required for this texobj? Always 1 except for YUV external images */
	m200_td                          tds[GLES_FB_TDS_COUNT];                    /**< The texture descriptors associated with this texture (one for each texture plane) */
	mali_mem_handle                  mali_tds[GLES_FB_TDS_COUNT];               /**< The texture descriptors associated with this texture (one for each texture plane), in mali memory */
	mali_base_frame_id               tds_last_used_frame;                       /**< The last used frame for the TDs, for callback reasons */

	/* these fields are only valid if num_uploaded_surfaces > 0 */
	mali_bool                        red_blue_swap;                             /**< Swap red and blue color channels in the TD - set for the first upload call, used throughout */
	mali_bool                        order_invert;                              /**< Invert the order of the color channels in the TD - set for every texture upload call */
	m200_texture_addressing_mode     layout;                                    /**< Texture object default layout as seen in m200_td.h */
	unsigned int                     linear_pitch_lvl0;                         /**< If using linear layout, then this field contains the pitch of lvl 0, in bytes. Otherwise 0. */
	mali_bool                        using_td_pitch_field;                      /**< MALI_TRUE if using the M400 only pitch state. True on M400 linear textures, false otherwise
	                                                                                 If set true, all miplevels in linear layouts need to have the same pitch.
																					 If set false, all miplevels in linear layouts need to have pitch half the size of the previous level. */

	
	/* internal object state tracking */
	mali_bool                        need_constraint_resolve;                   /**< True if we absolutely need a constraint resolver. 
	                                                                                 Set when a new assign don't match old state. Cleared on the next draw */
	mali_bool                        using_renderable_miplevels;                /**< True if any of the sublevels are flagged as true */
	mali_atomic_int                  ref_count;                                 /**< reference count */
	mali_base_frame_id               to_last_used_frame;                        /**< The last used frame for the TO, for callback reasons */

};

/**
 * @brief Allocates storage for the framebuffer texture object structure
 * @param dimensionality Texture target
 * @param base_ctx The base ctx
 * @return A pointer to a initialized framebuffer texture object
 * @note none
 */
MALI_CHECK_RESULT struct gles_fb_texture_object *_gles_fb_texture_object_alloc(enum gles_texture_target dimensionality, mali_base_ctx_handle base_ctx);

/**
 * @brief Frees the allocated storage for the framebuffer texture object structure
 * @param tex_obj The framebuffer texture object pointer
 * @return void
 * @note none
 */
void _gles_fb_texture_object_free(struct gles_fb_texture_object *tex_obj);

/**
 * @brief Returns a pointer to a specific texture memory block within the texobj.
 *        The returned pointer has a lifetime equal to the texobj. 
 * @param texobj - The texture backend object to query
 * @param level - The miplevel to fetch a texmem block from. 
 * @param plane - The plane to fetch a texmem block from. 
 * @param sublevel [OUT] - OUTPUT parameter. A texmem block may hold several miplevels of data. This will tell which sublevel matches the input level. 
 *                Leave as NULL if this info is not interesting 
 * @return The matching texmem object. This pointer will never be NULL.
 **/
MALI_STATIC_INLINE struct gles_fb_texture_memory* _gles_fb_texture_object_get_memblock(struct gles_fb_texture_object *texobj, u32 level, u32 plane, u32* sublevel)
{
	u32 texmem_index = MIN(level, MALI_TD_MIPLEVEL_POINTERS-1); /* index 0 through MALI_TD_MIPLEVEL_POINTERS-1 inclusive */
	MALI_DEBUG_ASSERT_POINTER(texobj);
	if(sublevel) *sublevel = level - texmem_index; 
	return &texobj->texmem[texmem_index][plane];
}

/**
 * @brief Increases the reference texture objects count
 * @param tex_obj The framebuffer texture object pointer
 * @return void
 * @note none
 */
MALI_STATIC_INLINE void _gles_fb_texture_object_addref(struct gles_fb_texture_object *tex_obj)
{
	MALI_DEBUG_ASSERT_POINTER(tex_obj);
	MALI_DEBUG_ASSERT(_mali_sys_atomic_get( &tex_obj->ref_count ) >= 1, ("inconsistent ref counting"));
	_mali_sys_atomic_inc( &tex_obj->ref_count );
}

/**
 * @brief Decreases the reference texture objects count
 * @param tex_obj The framebuffer texture object pointer
 * @return void
 * @note none
 */
void _gles_fb_texture_object_deref(struct gles_fb_texture_object *tex_obj);

/**
 * @brief Sets a texture image to a hardware surface
 * @param frame_builder The frame_builder to use if hw swizzling is performed
 * @param tex_obj The framebuffer texture object pointer
 * @param surface The mali surface to write into
 * @param width The width of the image
 * @param height The height of the image
 * @param format The format of the texel-data(GL_RGB, GL_R, GL_RGBA, etc. )
 * @param type Which type is the texture-object(UNSIGNED_BYTE, UNSIGNED_SHORT_X, etc. )
 * @param src_red_blue_swap Which red_blue_swap flag has the source (FALSE or TRUE)
 * @param src_reverse_order Which reverse_order flag has the source (FALSE or TRUE)
 * @param texels The array of texels
 * @param src_pitch The pitch of the input data
 * @return MALI_ERR_NO_ERROR if no error; the error code otherwise.
 * @note none
 */
MALI_CHECK_RESULT mali_err_code _gles_fb_tex_image_2d(
	struct mali_frame_builder *frame_builder,
	struct gles_fb_texture_object *tex_obj,
	struct mali_surface* surface,
	GLsizei width,
	GLsizei height,
	GLenum format,
	GLenum type,
	mali_bool src_red_blue_swap,
	mali_bool src_reverse_order,
	const void *texels,
	GLuint src_pitch
);

/**
 * @brief Upload ETC data to a mali_surface
 * @param tex_obj The framebuffer texture object pointer
 * @param surface The mali surface to write into
 * @param width The width of the image
 * @param height The height of the image
 * @param image_size The image size
 * @param texels The array of texels
 * @return MALI_ERR_NO_ERROR if no error; the error code otherwise.
 * @note none
 */
MALI_CHECK_RESULT mali_err_code _gles_fb_compressed_texture_image_2d_etc(
		struct gles_fb_texture_object *tex_obj,
		struct mali_surface* surface,
		int width,
		int height,
		int image_size,
		const void *texels );

/**
 * @brief Upload paletted data to a mali_surface. The data will be uncompressed.
 * @param tex_obj The framebuffer texture object pointer
 * @param surface The mali surface to write into
 * @param format The type of paletted texture this upload contains
 * @param width The width of the image
 * @param height The height of the image
 * @param miplevel_to_extract_from_texels The miplevel we are attempting to upload
 * @param image_size The image size
 * @param texels The array of texels
 * @return MALI_ERR_NO_ERROR if no error; the error code otherwise.
 * @note none
 */
MALI_CHECK_RESULT mali_err_code _gles_fb_compressed_texture_image_2d_paletted(
		struct gles_fb_texture_object *tex_obj,
		struct mali_surface* surface,
		GLenum format,
		int width,
		int height,
		int miplevel_to_extract_from_texels,
		int image_size,
		const void *texels );

/**
 * @brief Updates a region of a compressed image to a hardware surface
 * @param tex_obj The texture object to operate on
 * @param miplevel The miplevel to update
 * @param xoffset The x-coordinates offset
 * @param yoffset The y-coordinates offset
 * @param width The width of the region
 * @param height The height of the region
 * @param format The format of the texel-data(GL_RGB, GL_R, GL_RGBA, etc. )
 * @param image_size The image size
 * @param data The array of texels
 * @return GL_NO_ERROR if succesful, errorcode if not
 */
 MALI_CHECK_RESULT GLenum _gles_fb_compressed_texture_sub_image_2d(
	struct gles_fb_texture_object *tex_obj,
	GLint miplevel,
	GLint xoffset,
	GLint yoffset,
	GLsizei width,
	GLsizei height,
	GLenum format,
	GLsizei imageSize,
	const GLvoid *texels);

/**
 * @brief Updates a region of a texture image to a hardware surface
 * @param tex_obj The framebuffer texture object pointer
 * @param surface The mali surface to write into
 * @param xoffset The x-coordinates offset
 * @param yoffset The y-coordinates offset
 * @param width The width of the input data
 * @param height The height of the input data
 * @param format The format of the texel-data(GL_RGB, GL_R, GL_RGBA, etc. )
 * @param type Which type is the texture-object(UNSIGNED_BYTE, UNSIGNED_SHORT_X, etc. )
 * @param src_red_blue_swap Which red_blue_swap flag has the source (FALSE or TRUE)
 * @param src_reverse_order Which reverse_order flag has the source (FALSE or TRUE)
 * @param texels The array of texels
 * @param pitch The pitch
 * @return MALI_ERR_NO_ERROR if succesful, errorcode if not
 * @note none
 */
MALI_CHECK_RESULT mali_err_code _gles_fb_tex_sub_image_2d(
	struct gles_fb_texture_object *tex_obj,
	struct mali_surface* surface,
	GLint xoffset,
	GLint yoffset,
	GLsizei width,
	GLsizei height,
	GLenum format,
	GLenum type,
	GLboolean src_red_blue_swap,
	GLboolean src_reverse_order,
	const void *texels,
	GLuint pitch );

/**
 * @brief Makes a copy of an internal texture object.
 *        The copy will contain all the miplevels from the src,
 *        but the copied miplevels will be tagged as "read only"
 *
 * @param src The source internal texture object pointer
 * @return A pointer to the copied framebuffer texture object
 * @note none
 */
MALI_CHECK_RESULT struct gles_fb_texture_object* _gles_fb_texture_object_copy(
	struct gles_fb_texture_object *src );

/**
 * @brief Satisfies all hardware constraints imposed by the texture descriptor
 * @param tex_obj The texture object pointer
 * @return GL_NO_ERROR if succesful, errorcode if not
 * @note Refer to document: Mali200 TRM (Issue A,31 Jan. 2007), section 2.3.5 Texture Descriptor.
 */
MALI_CHECK_RESULT mali_err_code _gles_texture_object_resolve_constraints(struct gles_texture_object *tex_obj);

/**
 * @brief Gets the mali_surface at a given face/miplevel
 * @param tex_obj The texture object pointer
 * @param chain_index The mipmap chain index (cubemap face)
 * @param mipmap_level The mipmap level
 * @return a pointer to a mali_surface, or NULL if not present
 */
MALI_CHECK_RESULT struct mali_surface* _gles_fb_texture_object_get_mali_surface(
	struct gles_fb_texture_object *tex_obj,
	u16 chain_index,
	u16 mipmap_level);

/**
 * @brief Gets the mali_surface at a given face/miplevel
 * @param tex_obj The texture object pointer
 * @param chain_index The mipmap chain index (cubemap face)
 * @param mipmap_level The mipmap level
 * @param mipmap_plane The mipmap plane
 * @return a pointer to a mali_surface, or NULL if not present
 */
MALI_CHECK_RESULT struct mali_surface* _gles_fb_texture_object_get_mali_surface_at_plane(
	struct gles_fb_texture_object *tex_obj,
	u16 chain_index,
	u16 mipmap_level,
	u16 mipmap_plane);

/**
 * @brief Assign a surface to a backend object. 
 * @param tex_obj A pointer to the texture object to modify
 * @param mipchain The mipchain index of the target face
 * @param miplevel The miplevel to upload. Must be 0 when uploading egl images.
 * @param planes_count The number of planes to upload to this mipmap level. Must be 1 when not uploading egl images. Can be 1, 2 or 3 only when uploading egl external images.
 * @param surfaces In the surfaces array there is a surface to upload to each plane. This will be used as-is without modifications.
 * @note this function updates the num_uploaded_surfaces
 */
void _gles_fb_texture_object_assign(
	struct gles_fb_texture_object *tex_obj,
	int mipchain, int miplevel, unsigned int planes_count,
	struct mali_surface **surfaces);	

/**
 * @brief Attempt to allocate a miplevel that can fit into this backend texture object. 
 * @param tex_obj - The texture object to allocate off
 * @param mipchain - The mipchain to fit into
 * @param miplevel - The miplevel to fit into
 * @param sformat - The specifier of the texture to allocate. 
 * @return a surface pointer, or NULL on OOM. 
 *
 * @note The only thing the returned surface can be used for is to assign it to the texture object. 
 *       Failure to do so means the memory in the texture may be modified 
 *       without notice to the surface or its dependency system. 
 */
MALI_CHECK_RESULT struct mali_surface* _gles_fb_surface_alloc( 
	struct gles_fb_texture_object *tex_obj, 
	u32 mipchain, 
	u32 miplevel, 
	struct mali_surface_specifier* sformat);

/**
 * @brief Make a texture object's miplevel set as 'renderable'
 *        Renderable surfaces are tracked individually by dependency tracking. 
 * @param tex_obj - The texture object to modify
 * @param mipchain - The mipchain to modify
 * @param miplevel - The miplevel to modify
 */
void _gles_fb_texture_object_set_renderable(
	struct gles_fb_texture_object *tex_obj, 
	u32 mipchain, 
	u32 miplevel );

/**
 * @brief Queries whether a texture object's miplevel is set as 'renderable'
 *        Renderable surfaces are tracked individually by dependency tracking. 
 * @param tex_obj - The texture object to query
 * @param mipchain - The mipchain to query
 * @param miplevel - The miplevel to query
 */
MALI_CHECK_RESULT mali_bool _gles_fb_texture_object_get_renderable(
	struct gles_fb_texture_object *tex_obj, 
	u32 mipchain, 
	u32 miplevel );


#if EXTENSION_EGL_IMAGE_OES_ENABLE

/**
 * @brief Checks whether the supplied mali texel format is accepted as a GLES texture source
 * @param texel_format the texel format to check
 * @return TRUE if the texel format is valid
 */
MALI_CHECK_RESULT mali_bool _gles_fb_egl_image_texel_format_valid( m200_texel_format texel_format );

/**
 * @brief Fills an EGLImage structure with data from an existing mipmap level
 * When this returns, the specified target and miplevel will be an EGLImage sibling (a target)
 *
 * @param tex_obj A pointer to the (backend-specific) texture object
 * @param target The target to take data from
 * @param miplevel The miplevel to take data from
 * @param image A pointer to the EGLImage structure that should be filled in
 * @param removed_egl_image_ref If there was an egl_image_ref present it will be returned in this parameter, otherwise it will be NULL
 * @return MALI_TRUE on success, MALI_FALSE on error
 */
mali_bool _gles_fb_texture_setup_egl_image(
	struct gles_fb_texture_object *tex_obj,
	GLint mipchain,
	GLint miplevel,
	struct egl_image *image
   );


#endif /* EXTENSION_EGL_IMAGE_OES_ENABLE */

#endif /* GLES_FB_TEXTURE_OBJECT_H */
