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
 * @file gles_fb_interface.h
 * @brief Interface for fragment-backends, set up framebuffer context, clear and draw.
 */

#ifndef _GLES_FB_INTERFACE_H_
#define _GLES_FB_INTERFACE_H_

#include <mali_system.h>
#include <gles_base.h>
#include <gles_state.h>
#include <gles_framebuffer_object.h>
#include <m200_backend/gles_m200_td.h>
#include <m200_backend/gles_m200_texel_format.h>

/* forward declarations */
struct gles_fb_program_rendering_state; /* this type is encapsulated in the backend */

/**
 * Initializes the fb for drawing.
 * @param ctx The pointer to the GLES context
 * @param mode The draw-mode(GL_POINTS/LINES/...)
 * @return GL_NO_ERROR if succesful, errorcode if not
 */
mali_err_code MALI_CHECK_RESULT _gles_fb_init_draw_call( struct gles_context *ctx, GLenum mode );

/**
 * Allocates a framebuffer context
 * @param ctx The pointer to the GLES context
 * @return The framebuffer context
 */
struct gles_fb_context *_gles_fb_alloc( struct gles_context *ctx );

/**
 * Deletes the framebuffer context
 * @param fb_ctx The FB context to delete
 */
void _gles_fb_free( struct gles_fb_context *fb_ctx );

/**
 * Clears the framebuffer
 * @param ctx The GLES-context to clear the FB for
 * @param mask The mask saying which buffers to clear(STENCIL/COLOR/DEPTH)
 * @return GL_NO_ERROR if succesful, errorcode if not.
 */
mali_err_code MALI_CHECK_RESULT _gles_draw_clearquad( struct gles_context *ctx, GLbitfield mask );

/**
 * @brief Draws a dummy quad which should not alter framebuffer at all
 * @param ctx The pointer to the GLES context
 * @return An errorcode if it fails, MALI_ERR_NO_ERROR if not
 */
mali_err_code MALI_CHECK_RESULT _gles_draw_dummyquad( gles_context *ctx );

/**
 * @brief Draws a quad with texture with hardware quad drawing method 
 * @param ctx The pointer to the GLES context
 * @param x   The x coordinate in screen space for the quad position
 * @param y   The y coordinate in screen space for the quad position
 * @param z   The z coordinate in screen space for the quad position
 * @param width  The Width of the affected screen rectangle in pixels
 * @param height The height of the affected screen rectangle in pixels
 * @param texUnit The unit of the existed texture
 * @param texTarget The target of the existed texture
 */
mali_err_code MALI_CHECK_RESULT _gles_draw_texquad( gles_context *ctx, GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height, GLint texUnit, GLint texTarget);

/* PROGRAM RENDERING STATE SPECIFIC */

/**
 * Creates a backend specific program rendering state
 * @param prs The program rendering state to create a FB-specific part from. This state must be successfully linked.
 * @return the created object or NULL on failure.
 */
struct gles_fb_program_rendering_state* _gles_fb_alloc_program_rendering_state( struct gles_program_rendering_state* prs );

/**
 * Frees a backend-specific program rendering state. This undoes the creator above.
 * @param prs the object to free
 */
void _gles_fb_free_program_rendering_state( struct gles_fb_program_rendering_state* fb_prs );

/* OPTIONAL INTERFACE */

/**
 * @brief Checks if a texture object is complete or not
 * @param tex_obj The texture object to check if is complete or not
 * @return MALI_TRUE if complete and MALI_FALSE if not
 */
mali_bool MALI_CHECK_RESULT _gles_m200_is_texture_usable( struct gles_texture_object *tex_obj );


#endif /* _GLES_FB_INTERFACE_H_ */
