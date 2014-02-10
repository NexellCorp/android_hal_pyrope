/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_GEOMETRY_BACKEND_RENDERING_STATE_H
#define GLES_GEOMETRY_BACKEND_RENDERING_STATE_H

#include "mali_system.h"

#include "../gles_gb_interface.h"
#include <regs/MALIGP2/mali_gp_vs_config.h>
#include <base/gp/mali_gp_job.h>

/* size of the vs setup commands buffer. Need to be "large enough" */
#define VS_SETUP_CMDS_MAX_SIZE 7

typedef struct gles_gb_program_rendering_state
{
	int num_input_streams;
	int num_output_streams;
	u32 num_vs_setup_cmds;
	u64 vs_setup_cmds[VS_SETUP_CMDS_MAX_SIZE];
	u32 varying_streams[(2*GP_INPUT_STREAM_COUNT) + (2*GP_OUTPUT_STREAM_COUNT)];

} gles_gb_program_rendering_state;


#if HARDWARE_ISSUE_8733

/**
 * Assures that the last stream only contains aligned vertex data.
 * It may add another stream and put an extra command in the vs command stream.
 * It will put at most one command in the program rendering state vs_setup_cmds.
 *	@param gp_job The GP job to add vs setup code to. Inout parameter.
 *	@param gb_prs The geometry backend specific program rendering state to take the state from.
 *	@param streams A 32*2 elements wide array that describes the input/output streams
 *	@return the number of commands written.
 */
u32 _gles_gb_assure_aligned_last_stream( mali_gp_job_handle gp_job, struct gles_gb_program_rendering_state* gb_prs, u32* streams );

#endif /* HARDWARE_ISSUE_8733 */

/**
 * Writes shader-related state to the stream table
 *	@param streams Output stream to be filled in. Output parameter.
 *	@param gb_prs The geometry backend specific program rendering state to take the state from.
 */
MALI_STATIC_INLINE void _gles_gb_apply_program_rendering_state_output_streams( u32* streams, struct gles_gb_program_rendering_state* gb_prs )
{
	int size;

	MALI_DEBUG_ASSERT_POINTER(streams);
	MALI_DEBUG_ASSERT_POINTER(gb_prs);

	size = gb_prs->num_output_streams*2*sizeof(u32);

	/* copy out all output stream specifiers */
	_mali_sys_memcpy( streams, &gb_prs->varying_streams[2*GP_INPUT_STREAM_COUNT], size );
}

#endif /* GLES_GEOMETRY_BACKEND_RENDERING_STATE_H */

