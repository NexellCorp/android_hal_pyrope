/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2007, 2009-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles2_vertex_shader.h
 * @brief Vertex shader settings in GLES2 driver
 */
#ifndef GLES2_VERTEX_SHADER_H
#define GLES2_VERTEX_SHADER_H

#include <mali_system.h>
#include <GLES2/mali_gl2.h>

typedef struct gles2_vertex_shader
{
	int current_vertex_attrib;		/**< The active Vertex Attribute */
} gles2_vertex_shader;

/**
 * @brief Initializes the vertex shader-state
 * @param vs The pointer to the vertex shader-state
 */
void _gles2_vertex_shader_init( gles2_vertex_shader *vs );


#endif /* GLES2_VERTEX_SHADER_H */
