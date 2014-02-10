/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */


#include "gles_fb_rendering_state.h"

/**
 * RSW value setter.
 * This calls the default RSW setter, but also sets a corresponding mask,
 * flagging the bits it set so that these can be transferred to the master RSW later
 **/
MALI_STATIC_FORCE_INLINE void rsw_encode( struct gles_fb_program_rendering_state* fb_prs, u32 subword, u32 bitmask, u32 shift, u32 value )
{
	__m200_rsw_encode( &fb_prs->program_rsw, subword, bitmask, shift, value );
	__m200_rsw_encode( &fb_prs->program_rsw_mask, subword, bitmask, shift, bitmask ); /* write all 1's */
}


MALI_STATIC void _gles_m200_rsw_set_constant_varying_format(struct gles_fb_program_rendering_state* fb_prs, const int varying_index, const int format)
{
	MALI_DEBUG_ASSERT_POINTER(fb_prs);
	MALI_DEBUG_ASSERT_RANGE(varying_index, 0, 11);

	switch ( varying_index )
	{
		case 11:
			rsw_encode( fb_prs, M200_RSW_VARYING11_FORMAT, format);
			break;

		case 10:
			rsw_encode( fb_prs, M200_RSW_VARYING10_FORMAT_BITS_0_1, ( format & 0x3 ) );
			rsw_encode( fb_prs, M200_RSW_VARYING10_FORMAT_BIT_2,    ( format >> 2 ) );
			break;

		case 9:
			rsw_encode( fb_prs, M200_RSW_VARYING9_FORMAT, format );
			break;

		case 8:
			rsw_encode( fb_prs, M200_RSW_VARYING8_FORMAT, format  );
			break;

		case 7:
			rsw_encode( fb_prs, M200_RSW_VARYING7_FORMAT, format  );
			break;

		case 6:
			rsw_encode( fb_prs, M200_RSW_VARYING6_FORMAT, format  );
			break;

		case 5:
			rsw_encode( fb_prs, M200_RSW_VARYING5_FORMAT, format  );
			break;

		case 4:
			rsw_encode( fb_prs, M200_RSW_VARYING4_FORMAT, format  );
			break;

		case 3:
			rsw_encode( fb_prs, M200_RSW_VARYING3_FORMAT, format  );
			break;

		case 2:
			rsw_encode( fb_prs, M200_RSW_VARYING2_FORMAT, format  );
			break;

		case 1:
			rsw_encode( fb_prs, M200_RSW_VARYING1_FORMAT, format  );
			break;

		case 0:
			rsw_encode( fb_prs, M200_RSW_VARYING0_FORMAT, format  );
			break;
	}
}



MALI_STATIC void _gles_m200_rsw_set_constant_varyings_parameters(struct gles_fb_program_rendering_state* fb_prs, bs_program *program_bs )
{
	int i;
	u32 varyings_block_size;
	int num_varyings = program_bs->varying_streams.count;

	MALI_DEBUG_ASSERT( num_varyings >= 0 && num_varyings <= 11, ("max 12 varyings: found %i", num_varyings) );
	for (i = 0; i < num_varyings; ++i)
	{
		const struct bs_varying_stream_info *var_info = &program_bs->varying_streams.info[i];

		/* get the things that can affect stream format selection */
		u32 component_count          = var_info->component_count;
		u32 component_size           = var_info->component_size;
		u32 stream_format;

		MALI_DEBUG_ASSERT_RANGE(component_count, 1, 4);
		MALI_DEBUG_ASSERT((component_count != 1) && (component_count != 3),
				("Component count may not be 3 or 1 - was %i", component_count));

		/* find the correct varying format */
		if (component_size == 4)
		{
			if (component_count > 2) stream_format = M200_VARYING_FORMAT_FP32_4C;
			else                     stream_format = M200_VARYING_FORMAT_FP32_2C;
		}
		else
		{
			if (component_count > 2) stream_format = M200_VARYING_FORMAT_FP16_4C;
			else                     stream_format = M200_VARYING_FORMAT_FP16_2C;
		}

		_gles_m200_rsw_set_constant_varying_format(fb_prs, i, stream_format);
	}

	/* block stride/size should be rounded up to the next multiple of 8 (already done in linker) */
	MALI_DEBUG_ASSERT((program_bs->varying_streams.block_stride & 7) == 0, ("varying stream stride should always be a multiple of 8"));
	varyings_block_size = program_bs->varying_streams.block_stride / 8;

	/* Encode block stride */
	rsw_encode( fb_prs, M200_RSW_PER_VERTEX_VARYING_BLOCK_SIZE, varyings_block_size );
}


struct gles_fb_program_rendering_state* _gles_fb_alloc_program_rendering_state( struct gles_program_rendering_state* prs )
{
	struct gles_fb_program_rendering_state* fb_prs;
	struct bs_fragment_flags *fs_flags;
	u32 shader_address;

	MALI_DEBUG_ASSERT_POINTER(prs);
	MALI_DEBUG_ASSERT(prs->binary.linked == MALI_TRUE, ("binary program must be linked successfully"));

	fb_prs= _mali_sys_malloc( sizeof(struct gles_fb_program_rendering_state) );
	if(fb_prs == NULL) return NULL;
	_mali_sys_memset(fb_prs, 0, sizeof(struct gles_fb_program_rendering_state) );

	fs_flags = &prs->binary.fragment_pass.flags.fragment;

	/* setup Z and stencil write enable: RSW subword 3 */
	rsw_encode( fb_prs, M200_RSW_SHADER_Z_REPLACE_ENABLE, fs_flags->depth_write ? 1 : 0 );
	rsw_encode( fb_prs, M200_RSW_SHADER_STENCIL_REPLACE_ENABLE, fs_flags->stencil_write ? 1 : 0 );

	/* setup shader executable pointer: RSW subword 9 */
	MALI_DEBUG_ASSERT_POINTER( prs->binary.fragment_pass.shader_binary_mem );
	shader_address = _mali_mem_mali_addr_get( prs->binary.fragment_pass.shader_binary_mem->mali_memory, 0 );
	MALI_DEBUG_ASSERT_ALIGNMENT(shader_address, 64);
	rsw_encode( fb_prs, M200_RSW_SHADER_ADDRESS, shader_address);

	/* setup size of first instruction: RSW subword 9 */
	rsw_encode( fb_prs, M200_RSW_FIRST_SHADER_INSTRUCTION_LENGTH, fs_flags->size_of_first_instruction);

	/* setup varying stream information: RSW subword 10 */
	_gles_m200_rsw_set_constant_varyings_parameters( fb_prs, &prs->binary );

	/* setup uniform remapping table size: RSW subword 11 */
	if( prs->binary.fragment_shader_uniforms.cellsize > 0 || prs->binary.num_fragment_projob_passes > 0)
	{
		/* find the number of uniform rows, expand to the nearest pow2 size, get log2 of that number */
		/* uniforms are stored in 1 large block containing N uniforms */
		u32 uniformlog2size = _mali_log_base2(_mali_ceil_pow2(
								(prs->binary.num_fragment_projob_passes)?
								 prs->binary.projob_uniform_offset+3/4:                   /* offset given by the projobs */
                                 (prs->binary.fragment_shader_uniforms.cellsize + 3) / 4  /* number of regular uniforms */
		                      ));
		
		rsw_encode( fb_prs, M200_RSW_UNIFORMS_REMAPPING_TABLE_SIZE,              1 + prs->binary.num_fragment_projob_passes );
		rsw_encode( fb_prs, M200_RSW_UNIFORMS_REMAPPING_TABLE_LOG2_SIZE,         uniformlog2size );
		rsw_encode( fb_prs, M200_RSW_HINT_FETCH_SHADER_UNIFORMS_REMAPPING_TABLE, 1 );
		/* M200_RSW_UNIFORMS_REMAPPING_TABLE_ADDRESS must be set at runtime  */
	} else {
		rsw_encode( fb_prs, M200_RSW_UNIFORMS_REMAPPING_TABLE_SIZE,              0 );
		rsw_encode( fb_prs, M200_RSW_UNIFORMS_REMAPPING_TABLE_LOG2_SIZE,         0 );
		rsw_encode( fb_prs, M200_RSW_HINT_FETCH_SHADER_UNIFORMS_REMAPPING_TABLE, 0 );
	}

	/* setup texture descriptor remapping table size: RSW subword 12 */
	if( prs->binary.samplers.num_samplers > 0 )
	{
		u32 td_remap_table_size = prs->binary.samplers.td_remap_table_size;
		/* TD's are stored in N blocks each containing 1 TD */
		rsw_encode( fb_prs, M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_SIZE,       td_remap_table_size );
		rsw_encode( fb_prs, M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_LOG2_SIZE,  0 );
		rsw_encode( fb_prs, M200_RSW_HINT_FETCH_TEX_DESCRIPTOR_REMAPPING_TABLE, 1);
		/* M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_ADDRESS must be set at runtime */
	} else {
		rsw_encode( fb_prs, M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_SIZE,       0 );
		rsw_encode( fb_prs, M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_LOG2_SIZE,  0 );
		rsw_encode( fb_prs, M200_RSW_HINT_FETCH_TEX_DESCRIPTOR_REMAPPING_TABLE, 0);
	}


	/* setup flags: RSW subword 13 */
	/* M200_RSW_FORWARD_PIXEL_KILL_PERMISSIBLE depends on blendmode and alphatesting, and must be set at runtime */
	/* M200_RSW_HINT_SHADER_LACKS_RENDEZVOUS is not used by the HW, omitting (see bugzilla entry 8032) */
	/* M200_RSW_HINT_SHADER_CONTAINS_DISCARD is not used by the HW, omitting (see bugzilla entry 8032) */
	/* M200_RSW_ALLOW_EARLY_Z_AND_STENCIL_UPDATE depends on alphatesting, and must be set at runtime */
#if HARDWARE_ISSUE_4924
	rsw_encode( fb_prs, M200_RSW_ALLOW_EARLY_Z_AND_STENCIL_TEST, 0 );
#else
	rsw_encode( fb_prs, M200_RSW_ALLOW_EARLY_Z_AND_STENCIL_TEST, fs_flags->stencil_write || fs_flags->depth_write ? 0 : 1 );
#endif
	/* M200_RSW_HINT_FETCH_TEX_DESCRIPTOR_REMAPPING_TABLE is set earlier, along with subword 12 states */
	rsw_encode( fb_prs, M200_RSW_HINT_NO_SHADER_PRESENT, 0 );
	/* M200_RSW_HINT_FETCH_SHADER_UNIFORMS_REMAPPING_TABLE set earlier, along with subword 11 states*/



	return fb_prs;
}

void _gles_fb_free_program_rendering_state( struct gles_fb_program_rendering_state* fb_prs )
{
	_mali_sys_free(fb_prs);
}
