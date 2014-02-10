/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "../gles_context.h"
#include "../gles_util.h"
#include "gles_m200_uniforms.h"
#include "gles_fb_context.h"
#include "gles_instrumented.h"
#include "gles_framebuffer_object.h"
#include "util/mali_math.h"
#include <shared/m200_gp_frame_builder_inlines.h>
#include <base/mali_memory.h>
#include <shared/m200_projob.h>

void init_projob_rsw(m200_rsw *rsw, struct gles_context* ctx, struct gles_program_rendering_state* prs, struct bs_shaderpass* projob)
{
	/* RSW setters usually do a lot of bitwise ops resulting in reads. 
	 * Allocating a local CPU copy of the RSW and then copying it over is usually more efficient than
	 * setting RSW state right on the frame pool. In time we want the framepool to be cached! 
	 */

	u32 uniformlog2size = _mali_log_base2(_mali_ceil_pow2( 
	                            (prs->binary.fragment_shader_uniforms.cellsize + 3) / 4  /* number of regular uniforms */
	                      ));

	MALI_DEBUG_ASSERT_POINTER(prs);
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(rsw);
	MALI_DEBUG_ASSERT_POINTER(projob);

	/* subword 0-1: Constant color for blending. Not used, left at 0 */
	/* subword 2: blend parameters - set to disabled, aka Source*1 + Destination*0. And color writemask */
	    /* rgb */
		__m200_rsw_encode( rsw, M200_RSW_RGB_BLEND_FUNC,            M200_BLEND_CsS_pCdD );
		__m200_rsw_encode( rsw, M200_RSW_RGB_S_SOURCE_SELECT,       M200_SOURCE_0 );
		__m200_rsw_encode( rsw, M200_RSW_RGB_S_SOURCE_1_M_X,        1);
		__m200_rsw_encode( rsw, M200_RSW_RGB_S_SOURCE_ALPHA_EXPAND, 0);
		__m200_rsw_encode( rsw, M200_RSW_RGB_D_SOURCE_SELECT,       M200_SOURCE_0 );
		__m200_rsw_encode( rsw, M200_RSW_RGB_D_SOURCE_1_M_X,        0);
		__m200_rsw_encode( rsw, M200_RSW_RGB_D_SOURCE_ALPHA_EXPAND, 0);

		/* alpha */
		__m200_rsw_encode( rsw, M200_RSW_ALPHA_BLEND_FUNC,          M200_BLEND_CsS_pCdD );
		__m200_rsw_encode( rsw, M200_RSW_ALPHA_S_SOURCE_SELECT,     M200_SOURCE_0 );
		__m200_rsw_encode( rsw, M200_RSW_ALPHA_S_SOURCE_1_M_X,      1);
		__m200_rsw_encode( rsw, M200_RSW_ALPHA_D_SOURCE_SELECT,     M200_SOURCE_0 );
		__m200_rsw_encode( rsw, M200_RSW_ALPHA_D_SOURCE_1_M_X,      0);
	
		/* also: color writemask */
		__m200_rsw_encode( rsw, M200_RSW_ABGR_WRITE_MASK, 0xF );

	/* subword 3-4: depth parameters. Set to always pass, no writemask, z clamped to 0-1 */
		__m200_rsw_encode( rsw, M200_RSW_Z_COMPARE_FUNC, M200_TEST_ALWAYS_SUCCEED );
		__m200_rsw_encode( rsw, M200_RSW_Z_WRITE_MASK, 0x0);
		__m200_rsw_encode( rsw, M200_RSW_Z_NEAR_BOUND, 0x0);
		__m200_rsw_encode( rsw, M200_RSW_Z_FAR_BOUND,  0xffff);

	/* subword 5-8: stencil/alphatest parameters. Always pass, no alphatest, no stencil writemask. */
	    __m200_rsw_encode( rsw, M200_RSW_STENCIL_FRONT_COMPARE_FUNC, M200_TEST_ALWAYS_SUCCEED );
		__m200_rsw_encode( rsw, M200_RSW_STENCIL_BACK_COMPARE_FUNC,  M200_TEST_ALWAYS_SUCCEED );
		__m200_rsw_encode( rsw, M200_RSW_ALPHA_TEST_FUNC, M200_TEST_ALWAYS_SUCCEED );
		
		__m200_rsw_encode( rsw, M200_RSW_STENCIL_FRONT_WRITE_MASK,      0x0 );
		__m200_rsw_encode( rsw, M200_RSW_STENCIL_BACK_WRITE_MASK,       0x0 );

		__m200_rsw_encode( rsw, M200_RSW_STENCIL_FRONT_OP_IF_ZPASS,  M200_STENCIL_OP_KEEP_CURRENT );
		__m200_rsw_encode( rsw, M200_RSW_STENCIL_FRONT_OP_IF_ZFAIL,  M200_STENCIL_OP_KEEP_CURRENT );
		__m200_rsw_encode( rsw, M200_RSW_STENCIL_FRONT_OP_IF_SFAIL,  M200_STENCIL_OP_KEEP_CURRENT );
		__m200_rsw_encode( rsw, M200_RSW_STENCIL_BACK_OP_IF_ZPASS,   M200_STENCIL_OP_KEEP_CURRENT );
		__m200_rsw_encode( rsw, M200_RSW_STENCIL_BACK_OP_IF_ZFAIL,   M200_STENCIL_OP_KEEP_CURRENT );
		__m200_rsw_encode( rsw, M200_RSW_STENCIL_BACK_OP_IF_SFAIL,   M200_STENCIL_OP_KEEP_CURRENT );

	/* subword 8: multisample properties. Output all samples, no MSAA */
		__m200_rsw_encode( rsw, M200_RSW_MULTISAMPLE_WRITE_MASK,  0xf);
	
	/* subword 9: shader parameters: Setting up the projob shader */
		MALI_DEBUG_ASSERT(projob->flags.fragment.size_of_first_instruction > 0, ("The first instruction in the shader should be > 0"));
		__m200_rsw_encode( rsw, M200_RSW_FIRST_SHADER_INSTRUCTION_LENGTH, projob->flags.fragment.size_of_first_instruction);
		MALI_DEBUG_ASSERT_POINTER(projob->shader_binary_mem);
		MALI_DEBUG_ASSERT_POINTER(projob->shader_binary_mem->mali_memory);
		__m200_rsw_encode( rsw, M200_RSW_SHADER_ADDRESS,  _mali_mem_mali_addr_get( projob->shader_binary_mem->mali_memory, 0 ) );

	/* subword 10: varying format field. We use no varyings, left as 0 */
	/* subword 11: pointer to uniform remap table, and size. Using the same uniform remap table as the mainjob */
	    __m200_rsw_encode( rsw, M200_RSW_UNIFORMS_REMAPPING_TABLE_ADDRESS, ctx->fb_ctx->current_uniform_remap_table_addr );
		__m200_rsw_encode( rsw, M200_RSW_UNIFORMS_REMAPPING_TABLE_SIZE, 1 );
		__m200_rsw_encode( rsw, M200_RSW_UNIFORMS_REMAPPING_TABLE_LOG2_SIZE, uniformlog2size );
		__m200_rsw_encode( rsw, M200_RSW_HINT_FETCH_SHADER_UNIFORMS_REMAPPING_TABLE, 1);

	/* subword 12: texture descriptor remap table, and td remap table size. Same as the mainjob! */
		__m200_rsw_encode( rsw, M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_ADDRESS, ctx->fb_ctx->current_td_remap_table_address );
		__m200_rsw_encode( rsw, M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_SIZE,    prs->binary.samplers.td_remap_table_size);
		__m200_rsw_encode( rsw, M200_RSW_TEX_DESCRIPTOR_REMAPPING_TABLE_LOG2_SIZE,  0 );
		__m200_rsw_encode( rsw, M200_RSW_HINT_FETCH_TEX_DESCRIPTOR_REMAPPING_TABLE, (prs->binary.samplers.num_samplers > 0));

}


mali_err_code _gles_m200_setup_fragment_projobs( mali_mem_pool *frame_pool,
                                        struct gles_context *ctx,
                                        struct gles_program_rendering_state* prs,
										u32* uniform_remap_table_ptr)   /* output parameter */
{
	mali_addr rsw_mem_addr;
	m200_rsw* rsw_local_ptr;
	m200_rsw rsw_template;
	mali_frame_builder* current_fbuilder;
	u32 i;
	
	MALI_DEBUG_ASSERT_POINTER(frame_pool);
	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(prs);

	current_fbuilder = _gles_get_current_draw_frame_builder(ctx);
	MALI_DEBUG_ASSERT_POINTER(current_fbuilder); /* we already passed all checks to verify that the current FBO is valid */


	/****************/

	for(i = 0; i < prs->binary.num_fragment_projob_passes; i++)
	{
		/* Setup the projob RSW. 
		 * This should be set up based on the projob flags. One RSW per projob. 
		 * TODO: can optimize this code when we have more projobs than one!
		 */
	    _mali_sys_memset( rsw_template, 0x0, sizeof( m200_rsw ) );
		init_projob_rsw( &rsw_template, ctx, prs, &prs->binary.fragment_projob_passes[i]);
    
		rsw_local_ptr = _mali_mem_pool_alloc(frame_pool, sizeof(m200_rsw), &rsw_mem_addr);
		if(rsw_local_ptr == NULL) return MALI_ERR_OUT_OF_MEMORY;

		_mali_sys_memcpy(rsw_local_ptr, rsw_template, sizeof( m200_rsw ) );

		/* and setup the real job */
		uniform_remap_table_ptr[i+1] = _mali_projob_add_pp_drawcall(current_fbuilder, rsw_mem_addr);
		if(uniform_remap_table_ptr[i+1] == 0) return MALI_ERR_OUT_OF_MEMORY;
	}

	return MALI_ERR_NO_ERROR;
}


mali_err_code _gles_m200_update_fragment_uniforms( mali_mem_pool *frame_pool,
                                          struct gles_context *ctx,
                                          struct gles_program_rendering_state* prs )
{
	int uniform_count;
	u32 cellsize;
	int uniform_block_size;
	gles_fp16 *uniform_ptr;
	mali_addr  uniform_addr;

	u32 *uniform_remap_table_ptr = NULL;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( ctx->fb_ctx );
	MALI_DEBUG_ASSERT_POINTER( prs );

	cellsize = prs->binary.fragment_shader_uniforms.cellsize;
	MALI_DEBUG_ASSERT(0 != cellsize, ("Should not enter _gles_m200_update_uniforms with cellsize zero.") );

	MALI_DEBUG_ASSERT_POINTER ( prs->fp16_cached_fs_uniforms );

	/* round up to nearest 4-fp16 block (64bit alignment required) */
	uniform_count = (cellsize + 3) / 4;
	MALI_DEBUG_ASSERT(0 != uniform_count, ("zero-size uniform block"));

	uniform_block_size = sizeof(gles_fp16) * 4 * uniform_count;

	/* create a one-entry remap table, and a uniform table. Both aligned at 16 */
	uniform_remap_table_ptr = _mali_mem_pool_alloc(frame_pool, uniform_block_size + 16 + (prs->binary.num_fragment_projob_passes*sizeof(void*)), &ctx->fb_ctx->current_uniform_remap_table_addr);
	uniform_ptr = ( gles_fp16* )( ((u8*)uniform_remap_table_ptr) + 16 );
	uniform_addr =  (u32)ctx->fb_ctx->current_uniform_remap_table_addr + 16;

	if ( NULL == uniform_remap_table_ptr )
	{
		return MALI_ERR_OUT_OF_MEMORY;
	}

	MALI_DEBUG_ASSERT_ALIGNMENT(uniform_addr, 16); /* make sure that the uniforms are 16 bytes aligned */
	MALI_DEBUG_ASSERT_ALIGNMENT(uniform_remap_table_ptr, 16); /* make sure that the pointer is 16 bytes aligned */

	*uniform_remap_table_ptr = uniform_addr;

	_mali_sys_memcpy_cpu_to_mali(uniform_ptr, prs->fp16_cached_fs_uniforms, cellsize * sizeof(gles_fp16), sizeof(gles_fp16));

	_profiling_count(INST_GL_UNIFORM_BYTES_COPIED_TO_MALI, cellsize * sizeof(gles_fp16));

	/* if there is a projob, set up a renderpass as appropriate. This must happen AFTER the regular uniforms are set up */
	if(prs->binary.num_fragment_projob_passes > 0)
	{
		mali_err_code err;
		err = _gles_m200_setup_fragment_projobs(frame_pool, ctx, prs, uniform_remap_table_ptr);
		if(err != MALI_ERR_NO_ERROR) return err;
	}

	return MALI_ERR_NO_ERROR;
}
