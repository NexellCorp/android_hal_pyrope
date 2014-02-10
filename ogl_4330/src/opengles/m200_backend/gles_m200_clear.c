/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "base/mali_debug.h"
#include "base/pp/mali_pp_job.h"
#include "mali_render_regs.h"

#include "shared/m200_gp_frame_builder.h"
#include "shared/m200_gp_frame_builder_inlines.h"
#include "shared/m200_quad.h"

#include "../gles_context.h"
#include "../gles_framebuffer_object.h"
#include "../gles_gb_interface.h"
#include "gles_fb_context.h"
#include "gles_m200_draw_common.h"

#define clear_shader_size 0x78
#define clear_shader_binary_start 0x64
#define clear_shader_binary_size 0x14

MALI_STATIC const unsigned char clear_shader[] = {
	0x4d, 0x42, 0x53, 0x31, 0x70, 0x0, 0x0, 0x0, 0x43, 0x46, 0x52, 0x41, 0x68, 0x0, 0x0, 0x0,
	0x7, 0x0, 0x0, 0x0, 0x46, 0x53, 0x54, 0x41, 0x8, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
	0x1, 0x0, 0x0, 0x0, 0x46, 0x44, 0x49, 0x53, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	0x46, 0x42, 0x55, 0x55, 0x8, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	0x53, 0x55, 0x4e, 0x49, 0x8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	0x53, 0x56, 0x41, 0x52, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x44, 0x42, 0x49, 0x4e,
	0x14, 0x0, 0x0, 0x0, 0x25, 0x10, 0x2, 0x0, 0xc, 0x0, 0x0, 0x0, 0xcf, 0x7, 0xc0, 0x3,
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};

MALI_STATIC void _gles_fb_clear_setup_rsw(m200_rsw *rsw, GLbitfield mask, gles_context *ctx)
{
	/* color mask */
	if( mask & GL_COLOR_BUFFER_BIT )
	{
		__m200_rsw_encode( rsw, M200_RSW_R_WRITE_MASK, ctx->state.common.framebuffer_control.color_is_writeable[ 0 ] );
		__m200_rsw_encode( rsw, M200_RSW_G_WRITE_MASK, ctx->state.common.framebuffer_control.color_is_writeable[ 1 ] );
		__m200_rsw_encode( rsw, M200_RSW_B_WRITE_MASK, ctx->state.common.framebuffer_control.color_is_writeable[ 2 ] );
		__m200_rsw_encode( rsw, M200_RSW_A_WRITE_MASK, ctx->state.common.framebuffer_control.color_is_writeable[ 3 ] );
	}
	else __m200_rsw_encode( rsw, M200_RSW_ABGR_WRITE_MASK, 0x0 );

	/* depth test settings */
	__m200_rsw_encode( rsw, M200_RSW_Z_COMPARE_FUNC, M200_TEST_ALWAYS_SUCCEED );
	if ( mask & GL_DEPTH_BUFFER_BIT ) __m200_rsw_encode( rsw, M200_RSW_Z_WRITE_MASK, 0x1);
	else                              __m200_rsw_encode( rsw, M200_RSW_Z_WRITE_MASK, 0x0);

	/* stencil settings. For correct behavior the test needs to pass even if we're not clearing the stencil buffer */
	__m200_rsw_encode( rsw, M200_RSW_STENCIL_FRONT_COMPARE_FUNC, M200_TEST_ALWAYS_SUCCEED );
	__m200_rsw_encode( rsw, M200_RSW_STENCIL_BACK_COMPARE_FUNC,  M200_TEST_ALWAYS_SUCCEED );
	if ( mask & GL_STENCIL_BUFFER_BIT )
	{
		/* clearing the buffer : drawing a stencil quad with ref.value = clear value */
		__m200_rsw_encode( rsw, M200_RSW_STENCIL_FRONT_OP_IF_ZPASS, M200_STENCIL_OP_SET_TO_REFERENCE );
		__m200_rsw_encode( rsw, M200_RSW_STENCIL_BACK_OP_IF_ZPASS,  M200_STENCIL_OP_SET_TO_REFERENCE );
		__m200_rsw_encode( rsw, M200_RSW_STENCIL_FRONT_WRITE_MASK,  ctx->state.common.framebuffer_control.stencil_writemask );
		__m200_rsw_encode( rsw, M200_RSW_STENCIL_BACK_WRITE_MASK,   ctx->state.common.framebuffer_control.stencil_writemask );
		__m200_rsw_encode( rsw, M200_RSW_STENCIL_FRONT_REF_VALUE,   (u32)ctx->state.common.framebuffer_control.stencil_clear_value & ( ( 1U << GLES_MAX_STENCIL_BITS ) - 1 ));
		__m200_rsw_encode( rsw, M200_RSW_STENCIL_BACK_REF_VALUE,    (u32)ctx->state.common.framebuffer_control.stencil_clear_value & ( ( 1U << GLES_MAX_STENCIL_BITS ) - 1 ));
	}
	else
	{
		/* leave the stencil buffer as-is */
		__m200_rsw_encode( rsw, M200_RSW_STENCIL_FRONT_OP_IF_ZPASS,  M200_STENCIL_OP_KEEP_CURRENT );
		__m200_rsw_encode( rsw, M200_RSW_STENCIL_BACK_OP_IF_ZPASS,   M200_STENCIL_OP_KEEP_CURRENT );
	}
}

MALI_STATIC_INLINE void _gles_fb_setup_clear_by_blending_color(m200_rsw *rsw, const float clear_color[4], mali_bool fp16_format)
{

#if EXTENSION_TEXTURE_HALF_FLOAT_OES_ENABLE
	if (!fp16_format)
#endif
	{
		__m200_rsw_encode( rsw, M200_RSW_CONSTANT_BLEND_COLOR_RED,   MALI_STATIC_CAST(u32) (clear_color[0]*255 + 0.5f));
		__m200_rsw_encode( rsw, M200_RSW_CONSTANT_BLEND_COLOR_GREEN, MALI_STATIC_CAST(u32)(clear_color[1]*255 + 0.5f));
		__m200_rsw_encode( rsw, M200_RSW_CONSTANT_BLEND_COLOR_BLUE,  MALI_STATIC_CAST(u32)(clear_color[2]*255 + 0.5f));
		__m200_rsw_encode( rsw, M200_RSW_CONSTANT_BLEND_COLOR_ALPHA, MALI_STATIC_CAST(u32)(clear_color[3]*255 + 0.5f));
	}
#if EXTENSION_TEXTURE_HALF_FLOAT_OES_ENABLE
else
	{
		__m200_rsw_encode( rsw, M200_RSW_CONSTANT_BLEND_COLOR_RED,   MALI_STATIC_CAST(u32)_gles_fp32_to_fp16_predefined (clear_color[0]) );
		__m200_rsw_encode( rsw, M200_RSW_CONSTANT_BLEND_COLOR_GREEN, MALI_STATIC_CAST(u32)_gles_fp32_to_fp16_predefined (clear_color[1]));
		__m200_rsw_encode( rsw, M200_RSW_CONSTANT_BLEND_COLOR_BLUE,  MALI_STATIC_CAST(u32)_gles_fp32_to_fp16_predefined (clear_color[2]));
		__m200_rsw_encode( rsw, M200_RSW_CONSTANT_BLEND_COLOR_ALPHA, MALI_STATIC_CAST(u32)_gles_fp32_to_fp16_predefined (clear_color[3]));       
	}
#else
	MALI_IGNORE(fp16_format);
#endif

}

mali_err_code _gles_draw_clearquad( gles_context *ctx, GLbitfield mask )
{
	mali_err_code        err;
	mali_internal_frame* frame;
	mali_frame_builder  *frame_builder;
	
	mali_addr            position_addr;
	void*                clear_shader_ptr;
	mali_addr            clear_shader_addr;
	m200_rsw             clear_rsw;
	m200_rsw            *clear_rsw_ptr;
	mali_addr            clear_rsw_addr;

	mali_bool scissor_closed;
	u32 scissor_boundaries[4];
	mali_bool fp16_enabled;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT( (mask & ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT )) != 0,
			("invalid clearflags. this should have been caught further up in the pipeline\n" ) );

	frame_builder = _gles_get_current_draw_frame_builder( ctx );
	MALI_DEBUG_ASSERT_POINTER( frame_builder );
	frame = GET_CURRENT_INTERNAL_FRAME(frame_builder);

	/* create a clear shder */
	clear_shader_ptr = _mali_mem_pool_alloc(&frame->frame_pool, sizeof( clear_shader ), &clear_shader_addr); 
	if (NULL == clear_shader_ptr) return MALI_ERR_OUT_OF_MEMORY;
	_mali_sys_memcpy( clear_shader_ptr, clear_shader+clear_shader_binary_start, clear_shader_binary_size);

	/* update clear rsw */
	_mali_sys_memset(&clear_rsw, 0x0, sizeof(m200_rsw));
	_gles_fb_setup_rsw_const(&clear_rsw);
	fp16_enabled = _mali_frame_builder_get_fp16_flag(frame_builder);
	_gles_fb_setup_clear_by_blending_color(&clear_rsw, ctx->state.common.framebuffer_control.color_clear_value, fp16_enabled );
	_gles_fb_setup_rsw_shader(&clear_rsw, clear_shader_addr, clear_shader[clear_shader_binary_start] & 0x1F);
	_gles_fb_clear_setup_rsw(&clear_rsw, mask, ctx);

	clear_rsw_ptr = (m200_rsw*)_mali_mem_pool_alloc(&frame->frame_pool, sizeof(m200_rsw), &clear_rsw_addr);
	if( NULL == clear_rsw_ptr ) return MALI_ERR_OUT_OF_MEMORY;
	_mali_sys_memcpy( clear_rsw_ptr, &clear_rsw, sizeof(m200_rsw) );

	err = _gles_gb_alloc_position(ctx, &frame->frame_pool, &position_addr);

	_gles_gb_extract_scissor_parameters( ctx, frame_builder, MALI_FALSE, scissor_boundaries, &scissor_closed );

	/* This interface differs depending on hw rev. Using scissor settings in this specialcase */
	err = _mali_frame_builder_viewport(
		frame_builder,
		0, 
		0,
		GLES_MAX_RENDERBUFFER_SIZE,
		GLES_MAX_RENDERBUFFER_SIZE,
		NULL,
		NULL,
		0
	);  

	err = _mali_frame_builder_scissor(
		frame_builder,
		scissor_boundaries[0],
		scissor_boundaries[3],
		scissor_boundaries[1],
		scissor_boundaries[2],
		NULL,
		NULL,
		0
	);

	mali_statebit_set(&(ctx->state.common), MALI_STATE_SCISSOR_DIRTY );

	err = _mali200_draw_quad( frame_builder, position_addr, clear_rsw_addr );
	mali_statebit_set(  & ctx->state.common, MALI_STATE_PLBU_VIEWPORT_UPDATE_PENDING );

	return err;
}

