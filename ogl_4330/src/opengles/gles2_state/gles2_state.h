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
 * @file gles2_state.h
 * @brief Defines the structures that build up the gles2-state.
 */

#ifndef _GLES2_STATE_H_
#define _GLES2_STATE_H_

#include "gles2_hint.h"
#include "gles2_vertex_shader.h"
#include "gles2_program_object.h"
#include "../gles_texture_object.h"

struct gles_context;

/**
 * @brief The part of the OpenGL ES 2.x state that it does not share with OpenGL ES 1.x.
 * @note For the rest of the OpenGL ES 2.x state, see the struct gles_state in gles_state.h
 */
typedef struct gles2_state
{
	/* sub states - initialized by the gles2_state_init function */
	gles2_hint                          hint;                /**< The hint-state */
	gles2_vertex_shader                 vertex_shader;       /**< The vertex shader state */
	gles2_program_environment           program_env;         /**< The program environment */

	u32                                 previous_draw_mode;  /**< Tells us what the previous draw-mode was */
} gles2_state;

/**
 * @brief Initialize the GLES state
 * @param ctx The pointer to the GLES context
 */
void _gles2_state_init( struct gles_context *ctx );

/**
 * @brief UnInitialize the GLES state. Currently only deinits the vertex array state.
 * @param ctx The pointer to the GLES context
 */
void _gles2_state_deinit( struct gles_context *ctx );

#endif /* _GLES2_STATE_H_ */
