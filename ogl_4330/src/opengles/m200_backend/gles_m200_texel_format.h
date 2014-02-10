/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_M200_TEXEL_FORMAT_H
#define GLES_M200_TEXEL_FORMAT_H

#include "shared/m200_texel_format.h"
#include "../gles_base.h"

struct mali_surface_specifier;

/**
 * @brief Converts a GL type and format pair (as passed to a glTexImage2D function call) 
 *        to a mali surface format that is suitable to describe the input surface,
 *        regardless of the actual hardware support for such format.
 *        Use _gles_m200_get_storage_surface_format to get the actual format that is used by hardware.
 * @param sformat the mali surface format returned, output parameter
 * @param type a GL type, fx. GL_UNSIGNED_BYTE
 * @param format a GL format, fx GL_RGBA
 */
void _gles_m200_get_input_surface_format(struct mali_surface_specifier* sformat, GLenum type, GLenum format, u16 width, u16 height);

/**
 * @brief Converts a GL type and format pair (as passed to a glTexImage2D function call) 
 *        to a mali surface format that can be used by the hardware.
 *        If the virtual format returned by _gles_m200_get_virtual_surface_format is not
 *        usable by the hardware, the data must be converted.
 * @param sformat the mali surface format returned, output parameter
 * @param type a GL type, fx. GL_UNSIGNED_BYTE
 * @param format a GL format, fx GL_RGBA
 */
void _gles_m200_get_storage_surface_format(struct mali_surface_specifier* sformat, GLenum type, GLenum format, u16 width, u16 height);

/**
 * @brief Retrieves GL type and format matching a Mali200 texel format.
 * @note This is required for EGLImage support (EGLImages do not necessarily have a GL type and format).
 * @param internal_format The Mali200 texel format to retrieve type and format for
 * @param type A pointer to where the matching GL type will be written. Must not be NULL.
 * @param format A pointer to where the matching GL format will be written. Must not be NULL.
 */
void _gles_m200_get_gles_type_and_format(m200_texel_format internal_format, GLenum *type, GLenum *format);

/**
 * @brief Retrieves GL flags matching a GL type and format pair.
 * @note This is required for EGLImage support (EGLImages do not necessarily have a GL red_blue_swap and reverse_order flag).
 * @param type a GL type, fx. GL_UNSIGNED_BYTE
 * @param format a GL format, fx GL_RGBA
 * @param red_blue_swap A pointer to where the matching GL red_blue_swap flag will be written. Must not be NULL.
 * @param reverse_order A pointer to where the matching GL reverse_order flag will be written. Must not be NULL.
 */
void _gles_m200_get_gles_input_flags(GLenum type, GLenum format, GLboolean *red_blue_swap, GLboolean *reverse_order);

/**
 * @brief Retrieves the bytes needed by a texel for the specified GLES type and format.
 * @param type a GL type, fx. GL_UNSIGNED_BYTE
 * @param format a GL format, fx GL_RGBA
 * @return The number of bytes per texel for input data. 0 on Error.
 */
GLint _gles_m200_get_input_bytes_per_texel(GLenum type, GLenum format);

/**
 * @brief Retrieves the texels that can be stored in a single byte
 * for the specified GLES type and format (used by compressed texture).
 * @param type a GL type, fx. GL_UNSIGNED_BYTE
 * @param format a GL format, fx GL_RGBA
 * @return The number of texels per byte for input data. 0 on Error.
 */
GLint _gles_m200_get_input_texels_per_byte(GLenum type, GLenum format);

#endif /* GLES_M200_TEXEL_FORMAT_H */
