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
 * @file gles1_state.h
 * @brief Defines the structures that build up the gles-state.
 */

#ifndef _GLES1_STATE_H_
#define _GLES1_STATE_H_

#include "gles1_current.h"
#include "gles1_coloring.h"
#include "gles1_lighting.h"
#include "gles1_vertex_array.h"
#include "gles1_transform.h"
#include "gles1_rasterization.h"
#include "gles1_hint.h"
#include "gles1_error.h"
#include "gles1_tex_state.h"

/**
 * @brief The part of the OpenGL ES 1.x state that it does not share with OpenGL ES 2.x.
 * @note For the rest of the OpenGL ES 1.x state, see the struct gles_state in gles_state.h
 */
typedef struct gles1_state
{
	gles1_current                       current;             /**< The current-state */
	gles1_transform                     transform;           /**< The transformation-state */
	gles1_coloring                      coloring;            /**< The coloring-state */
	gles1_lighting                      lighting;            /**< The lighting-state */
	gles1_rasterization                 rasterization;       /**< The rasterization-state */
	gles1_texture_environment           texture_env;         /**< The texture-environment-state */
	gles1_hint                          hint;                /**< The hint-state */
} gles1_state;

/**
 * @brief Initialize the GLES state
 * @param ctx The pointer to the GLES context
 */
void _gles1_state_init( struct gles_context *ctx );

/**
 * @brief UnInitialize the GLES state. Currently only deinits the vertex array state.
 * @param ctx The pointer to the GLES context
 */
void _gles1_state_deinit( struct gles_context *ctx );



#endif /* _GLES1_STATE_H_ */
