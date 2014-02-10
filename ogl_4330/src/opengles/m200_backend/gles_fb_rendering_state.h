/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_FRAGMENT_BACKEND_RENDERING_STATE_H
#define GLES_FRAGMENT_BACKEND_RENDERING_STATE_H

#include "mali_system.h"

#include "../gles_config.h"
#include "../gles_context.h"
#include <mali_rsw.h>
#include "base/mali_debug.h"
#include "../gles_fb_interface.h"

typedef struct gles_fb_program_rendering_state
{
	/* RSW containing all shader-related states, setup in linktime */
	m200_rsw program_rsw;
	m200_rsw program_rsw_mask;

} gles_fb_program_rendering_state;


/**
 * Writes shader-related state to an RSW.
 * @param target_rsw The RSW to write the program specific state to.
 * @param fb_prs The fragmentbackend specific program rendering state to take the state from.
 */
MALI_STATIC_INLINE void _gles_fb_apply_program_rendering_state(
	m200_rsw* target_rsw,
	struct gles_fb_program_rendering_state* fb_prs )
{
	int i;
	u32 *rsw         = &(*target_rsw)[0];
	u32 *mask        = &fb_prs->program_rsw_mask[0];
	u32 *program_rsw = &fb_prs->program_rsw[0];
	for(i = 15; i >= 0; i--)
	{
		u32 subword = *rsw;
		subword &= ~(*mask++);
		subword |= *program_rsw++;
		*rsw++ = subword;
	}
}

#endif /* GLES_FRAGMENT_BACKEND_RENDERING_STATE_H */
