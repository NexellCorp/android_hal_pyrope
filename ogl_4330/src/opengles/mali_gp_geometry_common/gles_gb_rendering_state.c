/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */


#include "gles_gb_rendering_state.h"

MALI_STATIC void _gles_gb_constant_setup_varying_streams(struct gles_program_rendering_state *prs, u32 *streams)
{
	unsigned int i;

	MALI_DEBUG_ASSERT_POINTER( streams );
	MALI_DEBUG_ASSERT_POINTER( prs );
	MALI_DEBUG_ASSERT_RANGE((s32)prs->binary.varying_streams.count, 0, GP_OUTPUT_STREAM_COUNT - 1);

	/* clear all output streams */
	for( i = 0; i < GP_OUTPUT_STREAM_COUNT; i++ )
	{
		streams[ GP_VS_CONF_REG_OUTP_ADDR(i) ] = _SWAP_ENDIAN_U32_U32(0x0);
		streams[ GP_VS_CONF_REG_OUTP_SPEC(i) ] = _SWAP_ENDIAN_U32_U32(GP_VS_VSTREAM_FORMAT_NO_DATA);
	}

	/* set up real output streams */
	for (i = 0; i < prs->binary.varying_streams.count; ++i)
	{
		u32 format;
		const struct bs_varying_stream_info *var_info = &prs->binary.varying_streams.info[i];
		u32 component_count  = var_info->component_count;
		u32 component_size   = var_info->component_size;
		u32 component_offset = var_info->offset;

		MALI_DEBUG_ASSERT(component_size == 4 || component_size == 2, ("invalid component size"));
		MALI_DEBUG_ASSERT_RANGE(component_count, 1, 4);
		format = (component_size == 4) ? GP_VS_VSTREAM_FORMAT_FP32(component_count) : GP_VS_VSTREAM_FORMAT_FP16(component_count);

		streams[GP_VS_CONF_REG_OUTP_SPEC(i)] = _SWAP_ENDIAN_U32_U32(GP_VS_CONF_REG_OUTP_SPEC_CREATE(format, 0, prs->binary.varying_streams.block_stride));
		streams[GP_VS_CONF_REG_OUTP_ADDR(i)] = _SWAP_ENDIAN_U32_U32(component_offset); /* need to add the base address later */

	}

}


struct gles_gb_program_rendering_state* _gles_gb_alloc_program_rendering_state( struct gles_program_rendering_state* prs )
{
	struct gles_gb_program_rendering_state* gb_prs = _mali_sys_malloc(sizeof(struct gles_gb_program_rendering_state));
	if( gb_prs == NULL) return NULL;
	_mali_sys_memset(gb_prs, 0, sizeof(struct gles_gb_program_rendering_state));

	/* set vertex program parameters */
	gb_prs->vs_setup_cmds[gb_prs->num_vs_setup_cmds++] = GP_VS_COMMAND_LOAD_SHADER(
	   _mali_mem_mali_addr_get(prs->binary.vertex_pass.shader_binary_mem->mali_memory, 0), /* shader address */
	   0,                                                                                  /* destination instruction */
	   prs->binary.vertex_pass.flags.vertex.instruction_end                                /* length */
	);
	gb_prs->vs_setup_cmds[gb_prs->num_vs_setup_cmds++] = GP_VS_COMMAND_WRITE_CONF_REG(
	   (prs->binary.vertex_pass.flags.vertex.instruction_start) |                          /* start instruction of execution */
	   ((prs->binary.vertex_pass.flags.vertex.instruction_end - 1) << 10) |                /* end instruction of execution */
	   ((prs->binary.vertex_pass.flags.vertex.instruction_last_touching_input - 1) << 20), /* last instruction to use input registers */
	   GP_VS_CONF_REG_PROG_PARAM
	);
#if HARDWARE_ISSUE_4326
	gb_prs->num_input_streams = prs->binary.vertex_pass.flags.vertex.num_input_registers * 2;
#else
	gb_prs->num_input_streams = prs->binary.vertex_pass.flags.vertex.num_input_registers;
#endif
	gb_prs->num_output_streams = prs->binary.vertex_pass.flags.vertex.num_output_registers;

	MALI_DEBUG_ASSERT(gb_prs->num_input_streams<=MALIGP2_MAX_VS_INPUT_REGISTERS, ("Mali GP only supports 16 input streams, this error should have been caught by the linker"));
	MALI_DEBUG_ASSERT(gb_prs->num_output_streams > 0, ("Num output streams (%d) must be at least 1", prs->binary.vertex_pass.flags.vertex.num_output_registers));
	gb_prs->vs_setup_cmds[gb_prs->num_vs_setup_cmds++] = GP_VS_COMMAND_WRITE_CONF_REG(
		GP_VS_CONF_REG_OPMOD_CREATE(gb_prs->num_input_streams, gb_prs->num_output_streams),
		GP_VS_CONF_REG_OPMOD
	);
	MALI_DEBUG_ASSERT(gb_prs->num_vs_setup_cmds <= VS_SETUP_CMDS_MAX_SIZE, ("commands written outside command buffer!"));

	/* setup varying streams */
	_gles_gb_constant_setup_varying_streams(prs, gb_prs->varying_streams);

	return gb_prs;
}

#if HARDWARE_ISSUE_8733
u32 _gles_gb_assure_aligned_last_stream(  mali_gp_job_handle gp_job, struct gles_gb_program_rendering_state* gb_prs, u32 *streams )
{
	u32 num_vs_setup_cmds;

	MALI_DEBUG_ASSERT_POINTER(gb_prs);
	MALI_DEBUG_ASSERT_POINTER(gp_job);
	MALI_DEBUG_ASSERT_POINTER(streams);

	num_vs_setup_cmds = gb_prs->num_vs_setup_cmds;

	{
		/*
		 * Bugzilla 8733 can only trigger if the last specified stream uses unaligned vertex data.
		 * Unaligned vertex data can only occur for 3-component streams and for interleaved VBOs
		 * streams where the stride is non-default (i.e. it has a different stride than
		 * data_type_size*num_components).
		 *
		 * The workaround is to (when possible) add another NO_DATA input stream.
		 * Therefore we decode the last used input stream. If that could generate unaligned data,
		 * we increase the number of streams by one, and add a new VS command to set this (overriding
		 * the initial setting from _gles_gb_alloc_program_rendering_state).
		 *
		 * See Table 3.286. GP_VS_CONF_REG_INP_SPEC(X) Register bit assignments in the Mali-400
		 * TRM (Revision r1p0).
		 * The stream type is encoded in the lowermost 6 bits, where the lowermost 2 bits encode
		 * the stream size (1-4 components) as (size-1).
		 *
		 */
		const u32 gp_vs_input_stream_size_mask  = 0x03;
		const u32 gp_vs_input_stream_size_3comp = 0x02;
		const u32 gp_vs_input_stream_type_mask  = 0x3C;
		int num_input_streams = gb_prs->num_input_streams;
		u32 stream_spec       =
				_SWAP_ENDIAN_U32_U32(streams[  GP_VS_CONF_REG_INP_SPEC( num_input_streams-1 ) ]);
		u32 format            = (stream_spec & gp_vs_input_stream_type_mask);
		u32 num_components    = (stream_spec & gp_vs_input_stream_size_mask);
		u32 stride            = (stream_spec >> MALI_STRIDE_SHIFT) & MALI_STRIDE_MASK;
		u32 data_size         = 0;

		/* we always do the workaround for 3 component streams, so we do not have to check
		 * the stride in that case
		 */
		if ( num_components != gp_vs_input_stream_size_3comp )
		{
			switch (format)
			{
				case GP_VS_VSTREAM_FORMAT_1_FIX_S8:
					/* fall-through */
				case GP_VS_VSTREAM_FORMAT_1_NORM_S8:
					/* fall-through */
				case GP_VS_VSTREAM_FORMAT_1_FIX_U8:
					/* fall-through */
				case GP_VS_VSTREAM_FORMAT_1_NORM_U8:
					data_size = 1;
				break;
				case GP_VS_VSTREAM_FORMAT_1_FIX_S16:
					/* fall-through */
				case GP_VS_VSTREAM_FORMAT_1_NORM_S16:
					/* fall-through */
				case GP_VS_VSTREAM_FORMAT_1_FIX_U16:
					/* fall-through */
				case GP_VS_VSTREAM_FORMAT_1_NORM_U16:
					data_size = 2;
				break;
				case GP_VS_VSTREAM_FORMAT_1_FIX_S32:
					/* fall-through */
				case GP_VS_VSTREAM_FORMAT_1_FP32:
					data_size = 4;
				break;
				default:
					MALI_DEBUG_ASSERT(0, ("Invalid format 0x%08x\n", format) );
			}
		}

		if ( (num_components == gp_vs_input_stream_size_3comp) ||
		     (stride != num_components * data_size) )
		{
			MALI_DEBUG_ASSERT(num_input_streams < MALIGP2_MAX_VS_INPUT_REGISTERS, ("There should always be at least one available stream"));
			num_input_streams++;

			gb_prs->vs_setup_cmds[num_vs_setup_cmds++] = GP_VS_COMMAND_WRITE_CONF_REG(
				GP_VS_CONF_REG_OPMOD_CREATE(num_input_streams, gb_prs->num_output_streams),
				GP_VS_CONF_REG_OPMOD );
			MALI_DEBUG_ASSERT(gb_prs->num_vs_setup_cmds < VS_SETUP_CMDS_MAX_SIZE, ("commands written outside command buffer!"));

			/* one more command written */
			return 1; 
		}
	}

	/* no changes to the stream count or number of commands */
	return 0; 

}
#endif /* HARDWARE_ISSUE_8733 */

void _gles_gb_free_program_rendering_state( struct gles_gb_program_rendering_state* gb_prs )
{
	_mali_sys_free(gb_prs);
}
