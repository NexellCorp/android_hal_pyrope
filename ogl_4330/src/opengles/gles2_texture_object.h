/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles2_texture_object.h
 * @brief Contains structures, enumerations and @function prototypes related
 *        to gles1 texture objects
 */

#ifndef _GLES2_TEXTURE_OBJECT_H_
#define _GLES2_TEXTURE_OBJECT_H_

#include "gles_base.h"
#include "gles_texture_object.h"

struct gles_context;

/**
 * @brief Loads a 2d texture-image to MALI
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
 * @param unpack_alignment The unpack alignment to use when reading the array of texels
 * @return GL_NO_ERROR if succesful, errorcode if not
 */
MALI_CHECK_RESULT GLenum _gles2_tex_image_2d(
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
	GLint unpack_alignment );

/**
 * @brief Loads a sub-image to a 2d texture-image on MALI
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
 * @return GL_NO_ERROR if succesful, errorcode if not
 */
MALI_CHECK_RESULT GLenum _gles2_tex_sub_image_2d(
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
 * @param ctx The pointer to the GLES-context
 * @param target The target specifier for the texture-object
 * @param level The level specifier(for ETC: 0..n loads a single level, for palette and FLXTC 0..-n loads -level levels at once)
 * @param internalformat The internalformat of the texture-object, iow how to store the image on mali.
 * @param width The width of the image
 * @param height The height of the image
 * @param border Is border-texels enabled
 * @param imageSize The user-specified size of the image, mainly used to check that the user knows which format it is and that there is enough data to load the texture.
 * @param data The array of texels, and possibly palette.
 * @return GL_NO_ERROR if succesful, errorcode if not
 */
MALI_CHECK_RESULT GLenum _gles2_compressed_texture_image_2d(
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
 * @return GL_NO_ERROR if succesful, errorcode if not
 */
MALI_CHECK_RESULT GLenum _gles2_compressed_texture_sub_image_2d(
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

GLenum _gles2_bind_tex_image(
   struct gles_context *ctx,
   GLenum target,
   GLenum internalformat,
	mali_bool egl_mipmap_texture,
   struct mali_surface *surface,
   void** tex_obj);

void _gles2_unbind_tex_image( struct gles_context *ctx, void* tex_obj );

/**
 * @brief Constructs a texture-object from the content in the FB at the given coordinates(x, y, w, h)
 * @param ctx The pointer to the GLES-context
 * @param target The target specifier for the texture-object
 * @param level The level specifier
 * @param internalformat The internalformat of the texture-object
 * @param x The x offset to where to start copying the FB
 * @param y The y offset to where to start copying the FB
 * @param width The width of the image
 * @param height The height of the image
 * @param border Are border-texels enabled.
 * @return GL_NO_ERROR if succesful, errorcode if not
 */
MALI_CHECK_RESULT GLenum _gles2_copy_texture_image_2d(
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
 * @param ctx The pointer to the GLES-context
 * @param target The target specifier for the texture-object
 * @param level The level specifier
 * @param xoffset The x offset in the texture to where to start
 * @param yoffset The y offset in the texture to where to start
 * @param x The x offset to where to start copying the FB
 * @param y The y offset to where to start copying the FB
 * @param width The width of the image
 * @param height The height of the image
 * @return GL_NO_ERROR if succesful, errorcode if not
 */
MALI_CHECK_RESULT GLenum _gles2_copy_texture_sub_image_2d(
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
 * @brief Delete texture objects based on an array of texture names
 * @param ctx A pointer to the GLES context
 * @param n the number of objects to delete
 * @param textures pointer to the name-array of objects to delete
 * @note This is a wrapper around the GLES-entrypoint glBindTexture()
 */
MALI_CHECK_RESULT GLenum _gles2_delete_textures(
	struct gles_context *ctx,
	GLsizei n,
	const GLuint *textures );

#if EXTENSION_EGL_IMAGE_OES_ENABLE
/**
 * @brief Loads a texture from an EGLImage
 * @param ctx A pointer to the GLES context
 * @param target the texture target to load
 * @param image the EGLImage that holds the image data
 * @return An error code
 */
GLenum _gles2_egl_image_target_texture_2d(
	struct gles_context *ctx,
	GLenum target,
	GLeglImageOES image );

#endif /* EXTENSION_EGL_IMAGE_OES_ENABLE */

#endif /* _GLES2_TEXTURE_OBJECT_H_ */
