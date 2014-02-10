/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES_READ_PIXELS_H_
#define _GLES_READ_PIXELS_H_

/**
 * @file gles_read_pixels.h
 * @brief pixel reading routine
 */

#include "gles_context.h"

/**
 * Reads the specified portion of the screen to the specified array
 * @param ctx GLES context pointer
 * @param src_x_offset, The x-offset when reading from the output buffer, in screen coordinates
 * @param src_y_offset, The y-offset when reading from the output buffer, in screen coordinates
 * @param dst_x_offset, The x-offset when writing to the dst buffer, in screen coordinates
 * @param dst_y_offset, The y-offset when writing to the dst buffer, in screen coordinates
 * @param write_width The width of the area to write to, in screen coordinates
 * @param write_height The height of the area to write to, in screen coordinates
 * @param dst_width The width of the destination buffer, in screen coordinates
 * @param dst_height The height of the destinatino buffer, in screen coordinates
 * @param dst_pitch The pitch of the area to write to, in bytes. Only valid for linear dst_formats.
 * @param dst_format The format/layout/flags of the destination buffer.
 * @param dst_pixels The array to return the pixels in
 * @return An errorcode.
 */
MALI_CHECK_RESULT GLenum _gles_read_pixels_internal(
 gles_context *ctx,
 int src_x_offset,
 int src_y_offset,
 int dst_x_offset,
 int dst_y_offset,
 int write_width,
 int write_height,
 const mali_surface_specifier* dst_format,
 void *dst_pixels);

/**
 * Reads the specified portion of the screen to the specified array
 * @param ctx GLES context pointer
 * @param x The x-offset in screen coordinates
 * @param y The y-offset in screen coordinates
 * @param width The width in screen coordinates
 * @param height The height in screen coordinates
 * @param format The format to be read(GL_RGB, GL_RGBA, etc)
 * @param type The data-type they want the data to be returned as
 * @param pixels The array to return the pixels in
 * @return An errorcode.
 * @note This is a wrapper around the GLES-entrypoint glReadPixels()
 */
MALI_CHECK_RESULT GLenum _gles_read_pixels(
 gles_context *ctx,
 GLint x,
 GLint y,
 GLsizei width,
 GLsizei height,
 GLenum format,
 GLenum type,
 GLvoid *pixels );

/**
 * Reads the specified portion of the screen to the specified array
 * @param ctx GLES context pointer
 * @param x The x-offset in screen coordinates
 * @param y The y-offset in screen coordinates
 * @param width The width in screen coordinates
 * @param height The height in screen coordinates
 * @param format The format to be read(GL_RGB, GL_RGBA, etc)
 * @param type The data-type they want the data to be returned as
 * @param bufSize The buffer size limitation
 * @param pixels The array to return the pixels in
 * @return An errorcode.
 * @note This is a wrapper around the GLES-entrypoint glReadPixels()
 */
MALI_CHECK_RESULT GLenum _gles_read_n_pixels_ext(
 gles_context *ctx,
 GLint x,
 GLint y,
 GLsizei width,
 GLsizei height,
 GLenum format,
 GLenum type,
 GLsizei bufSize,
 GLvoid *pixels );

#endif /* _GLES_READ_PIXELS_H_ */
