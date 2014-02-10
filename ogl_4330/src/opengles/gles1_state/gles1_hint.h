/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles1_hint.h
 * @brief Hints for settings in GLES driver
 */
#ifndef GLES1_HINT_H
#define GLES1_HINT_H

#include <gles_base.h>

struct gles_state;

typedef struct gles1_hint
{
	GLenum fog;                     /**< The hint for fog */
	GLenum generate_mipmap;         /**< The hint for generation of mipmaps */
	GLenum line_smooth;             /**< The hint for state of line-smooth */
	GLenum perspective_correction;  /**< The hint for perspective-correction */
	GLenum point_smooth;            /**< The hint for state of point-smooth */
} gles1_hint;

/**
 * @brief Give the user a hint about which settings to use
 * @param state The gles state containing the gles1_hint struct to add the given hint to
 * @param target Which behavior they want a hint about
 * @param mode What do they want(speed vs. smoothness)
 * @return An errorcode.
 * @note This implements the GLES entrypoint glHint().
 */
GLenum _gles1_hint( struct gles_state *state, GLenum target, GLenum mode ) MALI_CHECK_RESULT;

/**
 * @brief Initializes the hint-state
 * @param hint The pointer to the hint-state
 */
void _gles1_hint_init( gles1_hint *hint );

#endif /* GLES1_HINT_H */
