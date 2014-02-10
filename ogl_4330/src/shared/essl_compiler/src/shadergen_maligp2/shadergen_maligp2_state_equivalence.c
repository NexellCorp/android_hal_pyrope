/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"

#include "shadergen_state.h"

int _vertex_shadergen_states_equivalent(const vertex_shadergen_state *state1, const vertex_shadergen_state *state2) 
{
	const unsigned int* istate1 = state1->bits;
	const unsigned int* istate2 = state2->bits;
		
	if(istate1[0] != istate2[0]) return 0;
	if(istate1[1] != istate2[1]) return 0;

	return 1;
	
}
