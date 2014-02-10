/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "base/mali_debug.h"
#include "base/pp/mali_pp_job.h"
#include "mali_render_regs.h"

#include "shared/m200_gp_frame_builder.h"
#include "shared/m200_gp_frame_builder_struct.h"
#include "shared/m200_quad.h"

#include "../gles_context.h"
#include "../gles_framebuffer_object.h"
#include "../gles_gb_interface.h"
#include "gles_fb_context.h"
#include "gles_m200_draw_common.h"

#if HARDWARE_ISSUE_7571_7572

MALI_STATIC void _gles_fb_dummy_setup_rsw_const(m200_rsw *rsw)
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

	__m200_rsw_encode( rsw, M200_RSW_Z_COMPARE_FUNC, M200_TEST_ALWAYS_FAIL );
	__m200_rsw_encode( rsw, M200_RSW_ALLOW_EARLY_Z_AND_STENCIL_UPDATE, 0x1 );
}

mali_err_code _gles_draw_dummyquad( gles_context *ctx )
{
	const u32	    dummy_shader[] = { 0x00020425, 0x0000000C, 0x01E007CF, 0xB0000000, 0x000005F5 };
	mali_frame_builder  *frame_builder;
	mali_internal_frame *frame;
	mali_err_code	err;

	mali_addr           position_addr;
	void               *dummy_shader_ptr;
	mali_addr           dummy_shader_addr;
	m200_rsw            dummy_rsw;
	m200_rsw           *dummy_rsw_ptr;
	mali_addr           dummy_rsw_addr;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	frame_builder = _gles_get_current_draw_frame_builder( ctx );
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	frame = GET_CURRENT_INTERNAL_FRAME(frame_builder);

	/* create a dummy shder */
	dummy_shader_ptr = _mali_mem_pool_alloc(&frame->frame_pool, sizeof( dummy_shader ), &dummy_shader_addr);
	if (NULL == dummy_shader_ptr) return MALI_ERR_OUT_OF_MEMORY;
	_mali_sys_memcpy( dummy_shader_ptr, dummy_shader, sizeof(dummy_shader));

	/* update the dummy rsw */
	_mali_sys_memset(&dummy_rsw, 0x0, sizeof(m200_rsw));
	_gles_fb_dummy_setup_rsw_const(&dummy_rsw);
	_gles_fb_setup_rsw_shader(&dummy_rsw, dummy_shader_addr, dummy_shader[ 0 ] & 0x1f );
	dummy_rsw_ptr = (m200_rsw*)_mali_mem_pool_alloc(&frame->frame_pool, sizeof(m200_rsw), &dummy_rsw_addr);
	if( NULL == dummy_rsw_ptr ) return MALI_ERR_OUT_OF_MEMORY;
	_mali_sys_memcpy( dummy_rsw_ptr, &dummy_rsw, sizeof(m200_rsw) );

	err = _gles_gb_alloc_position(ctx, &frame->frame_pool, &position_addr);
	MALI_CHECK_NO_ERROR(err);

	err = _mali200_draw_quad( frame_builder, position_addr, dummy_rsw_addr );
	MALI_CHECK_NO_ERROR(err);

	mali_statebit_set(  & ctx->state.common, MALI_STATE_PLBU_VIEWPORT_UPDATE_PENDING );

	return err;
}

#endif
