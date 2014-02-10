/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2007, 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include "shared/m200_gp_frame_builder.h"
#include "shared/m200_gp_frame_builder_inlines.h"
#include "shared/binary_shader/bs_object.h"
#include "base/mali_debug.h"
#include "gles_fb_context.h"
#include "gles_m200_shaders.h"
#include <gles_common_state/gles_program_rendering_state.h>


mali_err_code _gles_m200_update_shader( gles_program_rendering_state* prs, mali_frame_builder *frame_builder)
{
	mali_err_code err;
	bs_program *shader_bs = NULL;

	MALI_DEBUG_ASSERT_POINTER( prs );
	MALI_DEBUG_ASSERT_POINTER( frame_builder );

	shader_bs = &(prs->binary);	

	/* verify that the program object is valid/legal */
	MALI_DEBUG_ASSERT_POINTER( shader_bs );
	MALI_DEBUG_ASSERT_POINTER( shader_bs->fragment_pass.shader_binary_mem );
	MALI_DEBUG_ASSERT_POINTER( shader_bs->vertex_pass.shader_binary_mem );
	MALI_DEBUG_ASSERT_POINTER( shader_bs->fragment_pass.shader_binary_mem->mali_memory );
	MALI_DEBUG_ASSERT_POINTER( shader_bs->vertex_pass.shader_binary_mem->mali_memory );

	{
		/* add deref fragment shader */
		err = _mali_frame_builder_add_callback(frame_builder, 
		                                       (mali_frame_cb_func)_gles_program_rendering_state_deref, 
		                                       (mali_frame_cb_param)prs);
		if (MALI_ERR_NO_ERROR != err)
		{
			MALI_DEBUG_ASSERT(err == MALI_ERR_OUT_OF_MEMORY, 
			                  ("inconsistent error code returned (got 0x%X, expected 0x%X)", err, MALI_ERR_OUT_OF_MEMORY));
			return err;
		}

		/* counter that by addrefing */
		_gles_program_rendering_state_addref(prs);
		
	}

	/* update stack requirements for the shaders */
	_mali_frame_builder_update_fragment_stack(
		frame_builder,
		shader_bs->fragment_pass.flags.fragment.initial_stack_offset,
		shader_bs->fragment_pass.flags.fragment.stack_size
	);

	return MALI_ERR_NO_ERROR;
}


