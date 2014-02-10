/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_m200_draw_common.h"
#include <base/mali_memory_types.h>

void _gles_fb_setup_rsw_const(m200_rsw *rsw)
{
	/* z near & far */
	__m200_rsw_encode( rsw, M200_RSW_Z_NEAR_BOUND, 0x0 );
	__m200_rsw_encode( rsw, M200_RSW_Z_FAR_BOUND,  M200_RSW_Z_BOUND_MAX );
	__m200_rsw_encode( rsw, M200_RSW_Z_FAR_DEPTH_BOUND_OP, M200_Z_BOUND_DISCARD );
	__m200_rsw_encode( rsw, M200_RSW_Z_NEAR_DEPTH_BOUND_OP, M200_Z_BOUND_DISCARD );

	/* write to all subsamples */
	__m200_rsw_encode( rsw, M200_RSW_MULTISAMPLE_WRITE_MASK, M200_RSW_MULTISAMPLE_WRITE_MASK_ALL );

	/* no varyings */
	__m200_rsw_encode( rsw, M200_RSW_PER_VERTEX_VARYING_BLOCK_SIZE, 0 );

	/* no uniforms */
	__m200_rsw_encode( rsw, M200_RSW_UNIFORMS_REMAPPING_TABLE_SIZE,              0 );
	__m200_rsw_encode( rsw, M200_RSW_HINT_FETCH_SHADER_UNIFORMS_REMAPPING_TABLE, 0 );

	/* no textures */
	__m200_rsw_encode( rsw, M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_SIZE,       0 );
	__m200_rsw_encode( rsw, M200_RSW_HINT_FETCH_TEX_DESCRIPTOR_REMAPPING_TABLE, 0 );

	/* no alpha masking */
	__m200_rsw_encode( rsw, M200_RSW_ALPHA_TEST_FUNC, M200_TEST_ALWAYS_SUCCEED );

	/* blending function = 1*ConstColor + DstColor*0 */
	__m200_rsw_encode( rsw, M200_RSW_RGB_BLEND_FUNC, M200_BLEND_CsS_pCdD );
	__m200_rsw_encode( rsw, M200_RSW_ALPHA_BLEND_FUNC, M200_BLEND_CsS_pCdD );
	__m200_rsw_encode( rsw, M200_RSW_RGB_S_SOURCE_COMBINED, M200_RSW_BLEND_CONSTANT_COLOR );
	__m200_rsw_encode( rsw, M200_RSW_ALPHA_S_SOURCE_COMBINED, M200_RSW_BLEND_CONSTANT_COLOR );
	__m200_rsw_encode( rsw, M200_RSW_RGB_D_SOURCE_COMBINED, M200_RSW_BLEND_ZERO );
	__m200_rsw_encode( rsw, M200_RSW_ALPHA_D_SOURCE_COMBINED, M200_RSW_BLEND_ZERO );

	__m200_rsw_encode( rsw, M200_RSW_RGB_RESULT_COLOR_CLAMP_0_1, 1 );
	__m200_rsw_encode( rsw, M200_RSW_ALPHA_RESULT_COLOR_CLAMP_0_1, 1 );
}

void _gles_fb_setup_rsw_shader(m200_rsw *rsw, mali_addr shader_mem, int first_instruction_length)
{
	__m200_rsw_encode( rsw, M200_RSW_HINT_NO_SHADER_PRESENT,          0 );
	__m200_rsw_encode( rsw, M200_RSW_FIRST_SHADER_INSTRUCTION_LENGTH, first_instruction_length);
	__m200_rsw_encode( rsw, M200_RSW_SHADER_ADDRESS,		  shader_mem );
}

