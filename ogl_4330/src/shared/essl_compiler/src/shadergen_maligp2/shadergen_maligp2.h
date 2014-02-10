/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef SHADERGEN_MALIGP2_SHADERGEN_H
#define SHADERGEN_MALIGP2_SHADERGEN_H

/**
   Memory allocation and deallocation functions
*/
#ifndef ALLOCATION_FUNCTION_TYPES
#define ALLOCATION_FUNCTION_TYPES
typedef void * (*alloc_func)(unsigned int size);
typedef void (*free_func)(void *address);
#endif


struct vertex_shadergen_state;

unsigned *_vertex_shadergen_generate_shader(const struct vertex_shadergen_state* state, unsigned *size_out,
											alloc_func alloc, free_func free);

int _vertex_shadergen_states_equivalent(const struct vertex_shadergen_state *state1, const struct vertex_shadergen_state *state2);

unsigned _vertex_shadergen_state_hashcode(const struct vertex_shadergen_state *state);

#endif
