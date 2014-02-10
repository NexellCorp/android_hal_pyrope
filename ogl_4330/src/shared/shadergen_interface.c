/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007, 2009-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <shared/shadergen_interface.h>
#include <base/mali_types.h>

MALI_EXPORT unsigned *_gles_vertex_shadergen_generate_shader(const struct vertex_shadergen_state *state, unsigned *size_out, alloc_func alloc, free_func free)
{
	return _vertex_shadergen_generate_shader(state, size_out, alloc, free);
}

MALI_EXPORT const uniform_initializer *_gles_piecegen_get_uniform_initializers(unsigned *n_inits)
{
	return _piecegen_get_uniform_initializers(n_inits);
}

MALI_EXPORT unsigned *_gles_fragment_shadergen_generate_shader(const struct fragment_shadergen_state *state, unsigned *size_out, unsigned int hw_rev, alloc_func alloc, free_func free)
{
	return _fragment_shadergen_generate_shader(state, size_out, hw_rev, alloc, free);
}

MALI_EXPORT int _gles_fragment_shadergen_states_equivalent(const struct fragment_shadergen_state *state1, const struct fragment_shadergen_state *state2)
{
	return _fragment_shadergen_states_equivalent(state1, state2);
}

MALI_EXPORT int _gles_vertex_shadergen_states_equivalent(const struct vertex_shadergen_state *state1, const struct vertex_shadergen_state *state2)
{
	return _vertex_shadergen_states_equivalent(state1, state2);
}

