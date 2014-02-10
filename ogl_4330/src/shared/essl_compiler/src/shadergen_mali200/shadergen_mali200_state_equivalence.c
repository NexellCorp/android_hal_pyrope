/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"

#include "shadergen_state.h"

int _fragment_shadergen_states_equivalent(const fragment_shadergen_state *state1, const fragment_shadergen_state *state2) 
{
	int i;
	const unsigned int* istate1 = state1->bits;
	const unsigned int* istate2 = state2->bits;
	unsigned int enablebits;
		
	/* first check the control register. This holds amongst other things the register of enabled stages */
	if(istate1[0] != istate2[0]) return 0;

	/* control register is identical. Now we need to check all the enabled stages for equality as well. 64 bits per stage */
	
	enablebits = fragment_shadergen_decode(state1, FRAGMENT_SHADERGEN_ALL_STAGE_ENABLE_BITS);
	for(i = 0; enablebits != 0; i++, enablebits >>= 2)
	{
		if(enablebits & 0x3) 
		{
			essl_bool identical = (istate1[2*i+1] == istate2[2*i+1]) && (istate1[2*i+2] == istate2[2*i+2]);
			if(!identical) return 0;
		}
				
	}
	return 1;
	
}
