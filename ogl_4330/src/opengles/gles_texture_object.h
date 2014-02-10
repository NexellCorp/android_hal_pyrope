/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_texture_object.h
 * @brief Contains structures, enumerations and
 */

#ifndef _GLES_TEXTURE_OBJECT_H_
#define _GLES_TEXTURE_OBJECT_H_

#include "gles_base.h"
#include "gles_ftype.h"
#include "gles_config.h"
#include "gles_config_extension.h"
#include "gles_fbo_bindings.h"
#include <shared/mali_linked_list.h>
#include "base/mali_dependency_system.h"

#include <base/mali_context.h>
#include <shared/mali_named_list.h>
#if EXTENSION_EGL_IMAGE_OES_ENABLE
#include <shared/egl_image_status.h>
#if defined(__SYMBIAN32__)
#include "GLES2/mali_gl2ext.h"
#endif
#endif

struct gles_fb_texture_object;
struct gles_framebuffer_state;
struct gles_context;
struct gles_texture_environment;
struct gles_wrapper;
struct egl_image;
struct mali_surface;

/** structure used to keep track of texture completeness */
typedef struct gles_mipmap_level
{
	GLint width, height; /**< The dimensions of the mipmap-level */
	GLenum format, type; /**< The format and type specifies how the texture was stored on mali */

#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	/**
	 * Render to texture functionality will require a render attachment and render target
	 * per mipmap level. This require us to have this pointer here; it is used by the
	 * FBO, clear, flush and drawing calls. However, unless this mipmap level is attached
	 * as a rendertarget in a FBO, this pointer will be NULL.
	 * Any access to the inside of the render_attachment_wrapper struct require
	 * the framebuffer object list lock to be set.
	 */
	mali_linked_list *fbo_connection;
#endif /* EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE */

	/* miplevels may be locked (happens when we want to write to them from the CPU). If the miplevel is renderable,
	 * then this will require the use of dependencies. To add dependencies, a temp consumer is needed. This temp
	 * consumer is stored here. In regular cases, this pointer is NULL but after being miplevel_locked, it will
	 * be set, and cleared again on unlock */
	mali_ds_consumer_handle temp_consumer;

} gles_mipmap_level;

/* a mipchain keeps track of all mipmap level data within a texture. */
#define GLES_MAX_MIPCHAINS 6
typedef struct
{
	gles_mipmap_level *levels[GLES_MAX_MIPMAP_LEVELS];
} gles_mipchain;

/** an enum containing all possible texture targets */
enum gles_texture_target
{
	GLES_TEXTURE_TARGET_INVALID = -1,

#if EXTENSION_TEXTURE_1D_OES_ENABLE
	GLES_TEXTURE_TARGET_1D,
#endif
	GLES_TEXTURE_TARGET_2D,

#if EXTENSION_TEXTURE_3D_OES_ENABLE
	GLES_TEXTURE_TARGET_3D,
#endif

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	GLES_TEXTURE_TARGET_EXTERNAL,
#endif

	GLES_TEXTURE_TARGET_CUBE,
	GLES_TEXTURE_TARGET_COUNT
};

enum gles_colorrange
{
	GLES_COLORRANGE_INVALID = -1,
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	GLES_COLORRANGE_REDUCED,
	GLES_COLORRANGE_FULL
#endif
};

enum gles_colorspace
{
	GLES_COLORSPACE_INVALID = -1,
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	GLES_COLORSPACE_BT_601,
	GLES_COLORSPACE_BT_709
#endif
};

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL || EXTENSION_VIDEO_CONTROLS_ARM_ENABLE
typedef struct gles_yuv_info
{
#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	enum gles_colorrange colorrange;
	enum gles_colorspace colorspace;
	u32 image_units_count;
	mali_bool is_rgb;
#endif

#if EXTENSION_VIDEO_CONTROLS_ARM_ENABLE
	float brightness;
	float contrast;
	float saturation;
#endif
} gles_yuv_info;
#endif

/** The main texture object struct; this contains all there is to know about a texture, and is what is going into the named list */
typedef struct gles_texture_object
{
	enum gles_texture_target dimensionality;

	GLenum        wrap_s, wrap_t;                 /**< The parameters specifying how the texture is wrapped */
#if EXTENSION_TEXTURE_3D_OES_ENABLE
	GLenum        wrap_r;
#endif

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
	gles_yuv_info yuv_info;                       /**< Encapsulated information for YUV textures */
#endif

	GLenum        min_filter, mag_filter;         /**< The mipmap filters used when there are several levels in the texture */
	GLboolean     generate_mipmaps;               /**< is GL_GENERATE_MIPMAP enabled? */
	GLftype       max_anisotropy;                 /**< for ANISOTROPIC FILTERING extension. actual parameter */
#if EXTENSION_DRAW_TEX_OES_ENABLE
	GLint crop_rect[ 4 ];                         /**< The crop_rect is used by glDrawTexOES to specify which texels are to be drawn
	                                                   from this texture, values are in texture-coordinates, ranging from
	                                                   [0,0] to [tex_width,tex_height]. Parameters are in the following order { x, y, width, height },
	                                                   note: width and height are not the dimensions of the texture, but specify how
	                                                   many texels in each dimension is to be used */
#endif

	gles_mipchain *mipchains[GLES_MAX_MIPCHAINS]; /**< Stores all mipmap-levels to find out if they are uploaded correctly(width, height, depth, format, type) */

	struct gles_fb_texture_object *internal;      /**< The fragment backend specific struct which holds the texel-data */

	mali_bool     dirty;                          /**< Dirty flag */
	mali_bool     completeness_check_dirty;       /**< Has anything been changed so that we need to check if the texture is complete? */
	u32           complete_levels;                /**< Last completeness check approved this amount of mipmap levels as "ready for draw and in use". 
	                                                   Not a valid number if completeness check is dirty. */
	mali_bool     mipgen_dirty;                   /**< Did we need to regenerate mipmap levels? */
	mali_bool     is_complete;                    /**< Flag indicating if the texture-object was complete the last-time the completeness-check was performed */
	mali_bool     is_deleted;                     /**< Flag indicating if the texture-object has been marked for deletion */
	mali_bool     paletted;                       /**< paletted textures (including FLXTC) requires special handling */
	mali_bool     paletted_mipmaps;               /**< MALI_TRUE if paletted textures the paletted texture has been uploaded with mipmaps */

	mali_atomic_int ref_count;                    /**< How many pointers are there to this texture-object? Used to find out when no pointers are pointing to it, then it is deleted */


} gles_texture_object;

/**
 * @brief Conversion function that converts a GLenum target into a gles_texture_target
 * @param target A GLenum
 * @param api_version The current API version
 * @return the corresponding gles_texture_target
 * @note This function can be used for input checking, and will never assertfault.
 */
MALI_STATIC_FORCE_INLINE enum gles_texture_target _gles_get_dimensionality(GLenum target, enum gles_api_version api_version)
{
	MALI_DEBUG_ASSERT(GLES_API_VERSION_NO_VERSION != api_version, (""));
	switch (target)
	{
#if EXTENSION_TEXTURE_1D_OES_ENABLE
		case GL_TEXTURE_1D: return GLES_TEXTURE_TARGET_1D;
#endif
		case GL_TEXTURE_2D: return GLES_TEXTURE_TARGET_2D;

#if EXTENSION_OES_EGL_IMAGE_EXTERNAL
		case GL_TEXTURE_EXTERNAL_OES: return GLES_TEXTURE_TARGET_EXTERNAL;
#endif

#if EXTENSION_TEXTURE_3D_OES_ENABLE
		case GL_TEXTURE_3D_OES: return GLES_TEXTURE_TARGET_3D;
#endif

		case GL_TEXTURE_CUBE_MAP:
			MALI_CHECK(GLES_API_VERSION_1==api_version || GLES_API_VERSION_2==api_version, GLES_TEXTURE_TARGET_INVALID);
#if !EXTENSION_TEXTURE_CUBE_MAP_ENABLE
			if(GLES_API_VERSION_1 == api_version)
				return GLES_TEXTURE_TARGET_INVALID;
#endif
			return GLES_TEXTURE_TARGET_CUBE;

		default:
			/* this function is used to check if the input-dimensionality is valid or not, so it can't assert on this. */
			return GLES_TEXTURE_TARGET_INVALID;
	}
}


/**
 * @brief Binds the given texture to the current texture-unit
 * @param ctx A pointer to the GLES context
 * @param target The target to bind the texture to
 * @param name The identifier/name of the texture to bind
 * @return GL_NO_ERROR if successful, error code if not
 */
MALI_CHECK_RESULT GLenum _gles_bind_texture(
	struct gles_context *ctx,
	GLenum target,
	GLuint name );

/**
 * @brief Calculates the pitch for a pixel surface based on the width, format,
 *        type and unpack_alignment.
 * @param unpack_alignment Integer containing information to use to unpack the pixel data from memory
 * @param width The width of the pixel surface
 * @param format The format of the pixels in the surface
 * @param type The type of the pixels in the surface
 * @return The pitch of the pixel surface.
 */
int _gles_unpack_alignment_to_pitch(int unpack_alignment, int width, GLenum format, GLenum type);

/**
 * @brief Retrieves a mipmap level from supplied texture object. If allocates a new miplevel if none was found.
 * @param gles_texture_object The texture object to retrieve the mipmap level for
 * @param chain The texture target chain ID to retrieve the mipmap level for
 * @param level The mipmap level to retrieve
 * @param width The width of the pixel surface
 * @param height The height of the pixel surface
 * @param format The format of the pixel surface
 * @param type The type of the pixel surface
 * @return New or existing mipmap level, updated to the input parameters
 */
gles_mipmap_level *_gles_texture_object_get_mipmap_level_assure( gles_texture_object *tex_obj, int chain, int level, GLint width, GLint height, GLenum format, GLenum type);

/**
 * @brief checks completeness criterias
 * @param mipchain mipmap levels to be checked
 * @param uses_miplevels True if this texture object has a minfilter requiring miplevels (anything except GL_LINEAR or GL_NEAREST)
 */
mali_bool _gles_mipchain_is_complete(struct gles_texture_object* tex_obj, int chain, mali_bool uses_miplevels);

/**
 * @brief Retrieves the index into the mipchain array for a given texture target
 * @see gles_texture_object
 * @param target the texture target to retrive the index for
 * @return the index into the mipchain array
 */
MALI_PURE int _gles_texture_object_get_mipchain_index(GLenum target);

/**
 * @brief Retrieves a mipchain pointer from texture object. The mipchain is allocated if it does not already exist.
 * @see _gles_texture_object_get_mipchain_index
 * @param tex_obj the texture object to retrive the pointer from
 * @param chain_index the mipchain index
 * @return a pointer to the mipchain, or NULL if the allocation failed.
 */
MALI_CHECK_RESULT gles_mipchain *_gles_texture_object_get_mipmap_chain(gles_texture_object *tex_obj, int chain_index);

/**
 * @brief Retrieves a specific mipmap level
 * @param tex_obj the texture object to retrieve the mipmap level for
 * @param target the texture target to retrieve the mipmap level for
 * @param level the mipmap level to retrieve the mipmap level for
 * @return a pointer to the specified mipmap level (can be NULL)
 */
MALI_CHECK_RESULT gles_mipmap_level *_gles_texture_object_get_mipmap_level(gles_texture_object *tex_obj, GLenum target, int level);

/**
 * @brief Retrieves the red_blue_swap and order_invert flags for the texture objects' internal object.
 * This is used by the mipmap since internal structure is not available from mipmap.
 * @param tex_obj pointer to the texture object that contains the internal object we want to set
 * @param red_blue_swap pointer to the boolean with the retrieved red_blue_swap value
 * @param reverse_order pointer to the boolean with the retrieved order_invert value
 */
void _gles_texture_object_get_internal_component_flags( gles_texture_object *tex_obj, mali_bool *red_blue_swap, mali_bool *reverse_order);

/**
 * @brief Initialize a texture object
 * @param obj the texture object to initialize
 * @param dimensionality the texture target
 */
void _gles_texture_object_init(gles_texture_object *obj, enum gles_texture_target dimensionality);

/**
 * @brief Create a texture object
 * @param dimensionality - The dimensionality of the texture object
 * @param base_ctx - The current base context
 * @return the new texture object
 */
MALI_CHECK_RESULT gles_texture_object *_gles_texture_object_new( enum gles_texture_target dimensionality, mali_base_ctx_handle base_ctx );

/**
 * @brief Delete a texture object
 * @param obj the texture object to delete
 */
void _gles_texture_object_delete( gles_texture_object *obj );

/**
 * @brief Create a mipmap level
 * @param width the width of the miplevel
 * @param height the height of the miplevel
 */
MALI_STATIC_INLINE gles_mipmap_level *_gles_mipmap_level_new(GLint width, GLint height) {
	gles_mipmap_level *level = (gles_mipmap_level*)_mali_sys_malloc(sizeof(gles_mipmap_level));
	if( level == NULL) return NULL;

	level->width = width;
	level->height = height;
	level->type = 0; /*Invalid*/
	level->format = 0; /*Invalid*/
#if EXTENSION_FRAMEBUFFER_OBJECT_OES_ENABLE
	level->fbo_connection = NULL;
#endif

	return level;

}


/**
 * @brief Destroys a mipmap level pointer
 */
MALI_STATIC_INLINE  void _gles_mipmap_level_delete(gles_mipmap_level *level) {
	MALI_DEBUG_ASSERT_POINTER( level );
	_mali_sys_free(level);
}

/**
 * @brief Delete a texture name wrapper and its associated texture object
 * @param wrapper the texture wrapper object to delete
 * @note this is mainly used on exit to clean up the entire set of allocated texture resources.
 */
void _gles_texture_object_list_entry_delete(struct gles_wrapper *wrapper);

/**
 * @brief Increments reference count for a texture object.
 * @param tex_obj The pointer to the texture-object we want to reference.
 */
MALI_STATIC_FORCE_INLINE void _gles_texture_object_addref( gles_texture_object *tex_obj )
{
	MALI_DEBUG_ASSERT_POINTER( tex_obj );
	MALI_DEBUG_ASSERT( _mali_sys_atomic_get( &tex_obj->ref_count ) > 0, ( "invalid ref count ( %d )", _mali_sys_atomic_get( &tex_obj->ref_count )) );
	_mali_sys_atomic_inc( &tex_obj->ref_count );
}

/**
 * @brief Decrement reference count for a texture object, and delete the object if zero.
 * @param tex_obj The pointer to the texture-object we want to dereference.
 */
void _gles_texture_object_deref( gles_texture_object *tex_obj );

/**
 * @brief return a valid texture object by the name we're given. allocate if missing, but take care of default object
 * @param ctx A pointer to the GLES context
 * @param name The identifier/name of the texture to get
 * @param dimensionality The dimensionality of the texture to get
 * @return The pointer to the texture object queried for
 */
MALI_CHECK_RESULT gles_texture_object *_gles_get_texobj(
	struct gles_context *ctx,
	GLint name,
	enum gles_texture_target dimensionality);

/**
 * @brief See _gles_tex_image_2d (internal function)
 * @param tex_obj The pointer to the texture-object
 * @param ctx A pointer to the GLES context
 * @param target The target specifier for the texture-object
 * @param level The level specifier(for ETC: 0..n loads a single level, for palette and FLXTC 0..-n loads -level levels at once)
 * @param internalformat The internalformat of the texture-object, iow how to store the image on mali.
 * @param width The width of the image
 * @param height The height of the image
 * @param format The format of the texel-data(GL_RGB, GL_R, GL_RGBA, etc. )
 * @param type Which type is the texture-object(UNSIGNED_BYTE, UNSIGNED_SHORT_X, etc. )
 * @param src_red_blue_swap Which red_blue_swap flag has the source (FALSE or TRUE)
 * @param src_reverse_order Which reverse_order flag has the source (FALSE or TRUE)
 * @param pixels The array of texels
 * @param pitch The pitch value
 * @return GL_NO_ERROR if successful, errorcode if not
 */
GLenum _gles_tex_image_2d_internal(
	gles_texture_object *tex_obj,
	struct gles_context *ctx,
	GLenum target,
	GLint level,
	GLsizei width,
	GLsizei height,
	GLenum format,
	GLenum type,
	GLboolean src_red_blue_swap,
	GLboolean src_reverse_order,
	GLvoid *pixels,
	int pitch);

/**
 * @brief See _gles_tex_sub_image_2d (internal function)
 * @param tex_obj The pointer to the texture-object
 * @param ctx A pointer to the GLES context
 * @param target The target specifier for the texture-object
 * @param level The level specifier
 * @param xoffset The x-coordinates offset
 * @param yoffset The y-coordinates offset
 * @param width The width of the image
 * @param height The height of the image
 * @param format The format of the texel-data(GL_RGB, GL_R, GL_RGBA, etc. )
 * @param type Which type is the texture-object(UNSIGNED_BYTE, UNSIGNED_SHORT_X, etc. )
 * @param src_red_blue_swap Which red_blue_swap flag has the source (FALSE or TRUE)
 * @param src_reverse_order Which reverse_order flag has the source (FALSE or TRUE)
 * @param pixels The array of texels
 * @param pitch The pitch value
 * @return GL_NO_ERROR if successful, error code if not
 */
GLenum _gles_tex_sub_image_2d_internal(
	gles_texture_object *tex_obj,
	struct gles_context *ctx,
	GLenum target,
	GLint level,
	GLint xoffset,
	GLint yoffset,
	GLsizei width,
	GLsizei height,
	GLenum format,
	GLenum type,
	GLboolean red_blue_swap,
	GLboolean reverse_order,
	const GLvoid *pixels,
	int pitch);

/**
 * @brief Loads a 2d texture-image to MALI
 * @param tex_obj The texture object to work on
 * @param ctx The pointer to the GLES context
 * @param target The target specifier for the texture-object
 * @param level The level specifier
 * @param internalformat The internalformat of the texture-object, iow. how to store it on MALI
 * @param width The width of the image
 * @param height The height of the image
 * @param border Are border-texels enabled?
 * @param format The format of the texel-data(GL_RGB, GL_R, GL_RGBA, etc. )
 * @param type Which type is the texture-object(UNSIGNED_BYTE, UNSIGNED_SHORT_X, etc. )
 * @param pixels The array of texels
 * @param pitch The pitch of the surface
 * @return GL_NO_ERROR if successful, errorcode if not
 */
MALI_CHECK_RESULT GLenum _gles_tex_image_2d(
	gles_texture_object *tex_obj,
	struct gles_context *ctx,
	GLenum target,
	GLint level,
	GLint internalformat,
	GLsizei width,
	GLsizei height,
	GLint border,
	GLenum format,
	GLenum type,
	const GLvoid *pixels,
	int pitch);

/**
 * @brief Loads a sub-image to a 2d texture-image on MALI
 * @param tex_obj The texture object to work on
 * @param ctx The pointer to the GLES-context
 * @param target The target specifier for the texture-object
 * @param level The level specifier
 * @param xoffset The offset in x in texels to where to start replacing the image
 * @param yoffset The offset in y in texels to where to start replacing the image
 * @param width The width of the sub-image
 * @param height The height of the sub-image
 * @param format The format of the texel-data(GL_RGB, GL_R, GL_RGBA, etc. )
 * @param type Which type is the texture-object(UNSIGNED_BYTE, UNSIGNED_SHORT_X, etc. )
 * @param pixels The array of texels
 * @param unpack_alignment The unpack alignment to use when reading the array of texels
 * @return GL_NO_ERROR if successful, errorcode if not
 */
MALI_CHECK_RESULT GLenum _gles_tex_sub_image_2d(
	gles_texture_object *tex_obj,
	struct gles_context *ctx,
	GLenum target,
	GLint level,
	GLint xoffset,
	GLint yoffset,
	GLsizei width,
	GLsizei height,
	GLenum format,
	GLenum type,
	const GLvoid *pixels,
	GLint unpack_alignment );

/**
 * @brief Loads a compressed 2d texture-image to MALI
 * @param tex_obj The texture object to work on
 * @param ctx The pointer to the GLES-context
 * @param target The target specifier for the texture-object
 * @param level The level specifier(for ETC: 0..n loads a single level, for palette and FLXTC 0..-n loads -level levels at once)
 * @param internalformat The internalformat of the texture-object, iow how to store the image on mali.
 * @param width The width of the image
 * @param height The height of the image
 * @param border Is border-texels enabled
 * @param imageSize The user-specified size of the image, mainly used to check that the user knows which format it is and that there is enough data to load the texture.
 * @param data The array of texels, and possibly palette.
 * @return GL_NO_ERROR if successful, errorcode if not
 */
MALI_CHECK_RESULT GLenum _gles_compressed_texture_image_2d(
	gles_texture_object *tex_obj,
	struct gles_context *ctx,
	GLenum target,
	GLint level,
	GLenum internalformat,
	GLsizei width,
	GLsizei height,
	GLint border,
	GLsizei imageSize,
	const GLvoid *data);

/**
 * @brief Loads a sub-image to a compressed 2d texture-image on MALI, does nothing since no compressed textures we support supports this function
 * @param tex_obj The texture object to work on
 * @param ctx The pointer to the GLES-context
 * @param target The target specifier for the texture-object
 * @param level The level specifier
 * @param xoffset The offset in x in texels to where to start replacing the image
 * @param yoffset The offset in y in texels to where to start replacing the image
 * @param width The width of the image
 * @param height The height of the image
 * @param format The format of the texel-data(GL_RGB, GL_R, GL_RGBA, etc. )
 * @param imageSize The user-specified size of the image.
 * @param data The array of texels.
 * @return GL_NO_ERROR if successful, errorcode if not
 */
MALI_CHECK_RESULT GLenum _gles_compressed_texture_sub_image_2d(
	gles_texture_object *tex_obj,
	struct gles_context *ctx,
	GLenum target,
	GLint level,
	GLint xoffset,
	GLint yoffset,
	GLsizei width,
	GLsizei height,
	GLenum format,
	GLsizei imageSize,
	const GLvoid *data);

/**
 * @brief Constructs a texture-object from the content in the FB at the given coordinates(x, y, w, h)
 * @param tex_obj The texture object to work on
 * @param ctx The pointer to the GLES-context
 * @param target The target specifier for the texture-object
 * @param level The level specifier
 * @param internalformat The internalformat of the texture-object
 * @param x The x offset to where to start copying the FB
 * @param y The y offset to where to start copying the FB
 * @param width The width of the image
 * @param height The height of the image
 * @param border Are border-texels enabled.
 * @return GL_NO_ERROR if successful, errorcode if not
 */
MALI_CHECK_RESULT GLenum _gles_copy_texture_image_2d(
	gles_texture_object *tex_obj,
	struct gles_context *ctx,
	GLenum target,
	GLint level,
	GLenum internalformat,
	GLint x,
	GLint y,
	GLsizei width,
	GLsizei height,
	GLint border );

/**
 * @brief Replaces parts of a texture-object from the content in the FB at the given coordinates(x, y, w, h)
 * @param tex_obj The texture object to work on
 * @param ctx The pointer to the GLES-context
 * @param target The target specifier for the texture-object
 * @param level The level specifier
 * @param xoffset The x offset in the texture to where to start
 * @param yoffset The y offset in the texture to where to start
 * @param x The x offset to where to start copying the FB
 * @param y The y offset to where to start copying the FB
 * @param width The width of the image
 * @param height The height of the image
 * @return GL_NO_ERROR if successful, errorcode if not
 */
MALI_CHECK_RESULT GLenum _gles_copy_texture_sub_image_2d(
	gles_texture_object *tex_obj,
	struct gles_context *ctx,
	GLenum target,
	GLint level,
	GLint xoffset,
	GLint yoffset,
	GLint x,
	GLint y,
	GLsizei width,
	GLsizei height );

/**
 * @brief checks completeness criterias
 *   Sets the tex_object is_complete flag to it's correct value.
 * @param tex_obj the texture object to check
 */
void _gles_texture_object_check_completeness( gles_texture_object *tex_obj );

/**
 * @brief checks completeness criterias quickly.
 * @param tex_obj the texture object to check
 * @return true if the texture object is complete, false otherwise
 */
MALI_STATIC_INLINE mali_bool _gles_texture_object_is_complete(	gles_texture_object *tex_obj )
{
	/* If it is not dirty, it has been verified previously that it was complete,
	   and we also know that nothing has changed since last time */
	if ( MALI_TRUE == tex_obj->completeness_check_dirty )
	{
		_gles_texture_object_check_completeness( tex_obj );
	}

	return tex_obj->is_complete;
}


#if MALI_USE_GLES_2 || EXTENSION_TEXTURE_CUBE_MAP_ENABLE
/**
 * @brief checks completeness criterias
 * @param tex_obj the texture object to check
 */
mali_bool _gles_texture_object_is_cube_complete(
	struct gles_texture_object *tex_obj);
#endif


/**
 * @brief binds a surface as a texture
 * @param tex_obj The texture object to work on
 * @param ctx A pointer to the GLES context
 * @param target The texture target to load
 * @param internalformat The internalformat of the texture-object
 * @param egl_mipmap_texture TRUE is the texture has mipmaps
 * @param surface to bound
 */
GLenum _gles_bind_tex_image(
	gles_texture_object *tex_obj,
	struct gles_context *ctx,
	GLenum target,
	GLenum internalformat,
	mali_bool egl_mipmap_texture,
	struct mali_surface *surface );

/**
 * @brief Flag a miplevel as renderable. Renderable miplevels are dependency
 *        tracked individually of the texture object itself, and modifying one
 *        will not trigger a deep copy of the texture object in question.
 *        EGL Images, Miplevels bound to FBOs and surfaces bound to
 *        eglBindTexture should be flagged renderable.
 *        Textures written to by glTexSubImage2D need not be flagged renderable.
 *        The only way to lose the renderable flag is to replace the mipmap level.
 * @param ctx The gles context
 * @param tex_obj The internal texture object to modify.
 * @param target The target you want to flag, f.ex GL_TEXTURE_2D
 * @param miplevel the miplevel you want to flag
 * @return  MALI_ERR_NO_ERROR on success. In case this flagging triggers a deep copy,
 *        the function may also run out of memory. This will be shown as a
 *        MALI_ERR_OUT_OF_MEMORY return value, and if this happens, no
 *        change will be done to the input texture object.
 */
mali_err_code _gles_texture_miplevel_set_renderable(
                struct gles_context* ctx,
                struct gles_texture_object *tex_obj,
                GLenum target,
                GLint miplevel);

/**
 * @brief Function that assign a surface to a mipmap level in a mipmap chain.
 *        This function assign the input surface to the specified mipmap level
 *        of the input texture object.
 *        Any old mipmap level will be disconnected from the surface,
 *        and the new miplevel will be connected. FBOs will be updated
 *        to point / render to the new surface, GLES dependencies will
 *        be tackled, and any pixmaps needing an incremental render
 *        as a result will be handled to boot. Also, frontend part
 *        will be updated to match the new surface.
 *
 *        All functions seeking to assign or replace a mipmap level
 *        should do so through this entrypoint.
 *
 *        The assign will remove one mali reference added using fetch
 *        when we try to replace an existing surface with itself.
 *
 * @param ctx The GLES context, used for incremental rendering
 * @param tex_obj The GLES texture object to modify
 * @param mipchain see below
 * @param miplevel The mipmap level to replace
 * @param format The GLES format enum for the new miplevel
 * @param type The GLES type enum for the new miplevel
 * @param surfaces_count The number of planes in the surfaces array.
 * @param surfaces An array of mali_surface objects containing the new miplevel's contents
 *
 * @return MALI_ERR_NO_ERROR or MALI_ERR_OUT_OF_MEMORY
 */
mali_err_code _gles_texture_miplevel_assign (
				struct gles_context* ctx,
				struct gles_texture_object *tex_obj,
				int mipchain,
				int miplevel,
				GLenum format,
				GLenum type,
				int surfaces_count,
				struct mali_surface** surfaces);

/**
 * @brief Clears out all contents in a texture object, resetting
 *        all mipmap levels to a default state where applicable.
 *        All mipchains and mipmap levels will be detached.
 *        FBO bindings will be maintained, but the objects they
 *        point to will be invalidated.
 *
 * @param ctx The GLES context
 * @param tex_obj The texture object to reset.
 * @return MALI_ERR_OUT_OF_MEMORY or MALI_ERR_NO_ERROR
 */
mali_err_code _gles_texture_reset(
	struct gles_context* ctx,
	struct gles_texture_object *tex_obj );

/*
 * @brief Allocate a mali_surface designed to fit into an existing
 *        gles texture object at a given location. The function tries
 *        its best to create a surface which will cause no HW
 *        constraint resolve to take place.
 *
 *        This function does not modify the texture object. To actually
 *        fit the surface into the texobj, you also need to call assign.
 *        By using this function, the assign operation will go smooth,
 *        but calling it is under no circumstances needed.
 *
 * @param ctx The GLES context
 * @param tex_obj The texture object to use.
 * @param mipchain The mipmap chain from where the texture shoul be fetched
 * @param miplevel The mipmap level to fetch
 * @param width The width to use for new surface creation
 * @param height The height to use for new surface creation
 * @param format The GLES format enum for the new miplevel
 * @param type The GLES type enum for the new miplevel
 * @param do_not_replace Some function like glTexSubImage can ask to don't replace the mipmap level surface
 * @return A surface which can be assigned or dereferenced.
 *         NULL is returned if the system runs out of memory
 */
struct mali_surface* _gles_texture_miplevel_allocate(
	struct gles_context* ctx,
	struct gles_texture_object *tex_obj,
	int mipchain,
	int miplevel,
	int width,
	int height,
	GLenum format,
	GLenum type);

/**
 * @brief Locks a mali_surface already assigned to a texture object.
 *        The entrypoint will clear outstanding dependencies, and in case
 *        of a renderable mali_surface, handle dependencies needed. The returned
 *        mali_surface can in turn be access locked by the caller and mapped at leisure.
 *
 *        A successful call to this function must always be undone with a matching unlock
 *
 *        It is an assert error to attempt to lock a miplevel which do not exist. Use
 *        _gles_fb_texture_object_get_mali_surface to ensure there is already a surface
 *        present at the point in the texture object.
 *
 * @param ctx The GLES context
 * @param tex_obj The texture object to modify
 * @param mipchain The mipchain id to lock
 * @param miplevel The miplevel id to lock
 * @return the mali surface the caller can then play around with.
 *         Or NULL if OOM.
 */
struct mali_surface* _gles_texture_miplevel_lock(
	struct gles_context* ctx,
	struct gles_texture_object* tex_obj,
	int mipchain,
	int miplevel );

/**
 * @brief Undoes the operations caused by _gles_texture_miplevel_lock.
 *
 * If the object is not locked already, the function will assertcrash.
 *
 * @param ctx The GLES context
 * @param tex_obj The texobj to unlock
 * @param mipchain The chain index to unlock
 * @param miplevel the level index to unlock
 */
void _gles_texture_miplevel_unlock(
	struct gles_context* ctx,
	struct gles_texture_object* tex_obj,
	int mipchain,
	int miplevel );

#if EXTENSION_EGL_IMAGE_OES_ENABLE
/**
 * @brief Loads a texture from an EGLImage
 * @param tex_obj The texture object to work on
 * @param ctx A pointer to the GLES context
 * @param target the texture target to load
 * @param image the EGLImage that holds the image data
 * @return An error code
 */
GLenum _gles_egl_image_target_texture_2d(
	gles_texture_object *tex_obj,
	struct gles_context *ctx,
	GLenum target,
	GLeglImageOES image );

/* @brief Setup an EGLImage from a texture
 * @param ctx A pointer to the GLES context
 * @param egl_target the texture target to load
 * @param name name of texture object
 * @param miplevel miplevel to use for EGLImage
 * @param image A pointer to an egl_image structure
 * @return An error code described by egl_image_from_gles_surface_status
 */
enum egl_image_from_gles_surface_status _gles_setup_egl_image_from_texture(
	struct gles_context *ctx,
	enum egl_image_gles_target egl_target,
	GLuint name,
	u32 miplevel,
	struct egl_image *image );

#endif /* EXTENSION_EGL_IMAGE_OES_ENABLE */

/**
 * checks if we need to re-generate the mipmaps levels for the specified texture object.
 * @param tex_obj, the gles_texture_object we want to re-generate the mipmaps for
 * @param target the texture target to retrieve the index for
 * @return MALI_TRUE if we need to regenerate the mipmaps, MALI_FALSE otherwise
 */
mali_bool _gles_texture_is_mipmap_generation_necessary(gles_texture_object* tex_obj, GLenum target);

/**
 * @brief This works just like _gles_get_dimensionality, except that it also handles the detailed sides of a cubemap
 * @param target A GLenum
 * @return the corresponding gles_texture_target
 * @note This function is an alternate version of gles_get_dimensionality that accepts cube faces instead of
 *       GL_TEXTURE_CUBE_MAP. Other than that there is no difference.
 */
MALI_PURE enum gles_texture_target _gles_get_dimensionality_cubemap_detail(GLenum target);

/**
 * @brief Retrieves the texture object that currently bound to the given target of the active texture unit in
 *        the given texture environment.
 * @param target The target to return the bound texture object of
 * @param texture_env A pointer to the texture environment to return the texture object currently bound to the
 *                    given target for.
 * @param output_texture_obj This is an output parameter pointer-pointer that will have been set to the
 *                           active and bound texture object if the function returned without an error
 * @return An error code. GL_NO_ERROR indicates that the texture object bound to the active texture unit was
 *         successfully retrieved.
 */
GLenum _gles_get_active_bound_texture_object( GLenum target, struct gles_texture_environment *texture_env, struct gles_texture_object **output_texture_obj);

#endif /* _GLES_TEXTURE_OBJECT_H_ */
