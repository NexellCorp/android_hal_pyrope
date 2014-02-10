/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles2_hint.h
 * @brief Hints for settings in GLES2 driver
 */

#ifndef GLES2_HINT_H
#define GLES2_HINT_H

#include <mali_system.h>
#include <GLES2/mali_gl2.h>

struct gles_state;

typedef struct gles2_hint
{
	GLenum generate_mipmap;			/**< The hint for generation of mipmaps */
	GLenum fragment_shader_derivative;	/**< The hint for derivate fragment shaders */
} gles2_hint;

/**
 * @brief Give the user a hint about which settings to use
 * @param state The gles state containing the gles2_hint struct to add the given hint to
 * @param target Which behavior they want a hint about
 * @param mode What do they want(speed vs. smoothness)
 * @return An errorcode.
 * @note This is a wrapper function for the GLES2 entrypoint glHint().
 */
GLenum _gles2_hint( struct gles_state *state, GLenum target, GLenum mode ) MALI_CHECK_RESULT;

/**
 * @brief Initializes the hint-state
 * @param hint The pointer to the hint-state
 */
void _gles2_hint_init( gles2_hint *hint );

#endif /* GLES2_HINT_H */
