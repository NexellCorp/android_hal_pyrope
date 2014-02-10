/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include "m200_quad.h"

#include <shared/mali_convert.h>
#include <shared/m200_gp_frame_builder.h>
#include <shared/m200_gp_frame_builder_inlines.h>
#include <regs/MALIGP2/mali_gp_plbu_config.h>

#define QUAD_PLBU_COMMAND_COUNT 8

/**
 * Adds the PLBU commands required to draw a quad to the PLBU command list.
 *
 * @param gp_job                    - The geometry job to add the quad PLBU commands to
 * @param rsw                       - A pointer to the RSW to use for drawing the quad
 * @param vertex_array_base_address - The address to the location of the transformed position
 *                                    vertices of the quad
 */
MALI_STATIC MALI_CHECK_RESULT mali_err_code setup_plbu_quad_job( mali_gp_job_handle   gp_job,
                                                                 mali_addr            vertex_address,
                                                                 mali_addr            index_address,
                                                                 mali_addr            rsw_address)
{
	u64           commands[QUAD_PLBU_COMMAND_COUNT];
	u32           current_command = 0;
	mali_err_code err;

	MALI_DEBUG_ASSERT_HANDLE( gp_job );

	/* update rsw and vertex array base address */
	commands[current_command++] = GP_PLBU_COMMAND_RSW_VERTEX_ADDR( rsw_address, vertex_address );

	/* Setup PLBU config register */
	commands[current_command++] = GP_PLBU_COMMAND_WRITE_CONF_REG( (0) | MALI_TILELIST_CHUNK_FORMAT << 8, GP_PLBU_CONF_REG_PARAMS );

	/* hardcode z near and far to [ 0, 1] */
	commands[current_command++] = GP_PLBU_COMMAND_WRITE_CONF_REG( _mali_convert_fp32_to_binary(0.0f), GP_PLBU_CONF_REG_Z_NEAR );

	commands[current_command++] = GP_PLBU_COMMAND_WRITE_CONF_REG( _mali_convert_fp32_to_binary(1.0f), GP_PLBU_CONF_REG_Z_FAR );

	/* Setup post index bias */
	commands[current_command++] = GP_PLBU_COMMAND_WRITE_CONF_REG( 0, GP_PLBU_CONF_REG_OFFSET_VERTEX_ARRAY );
	
	/* setup index/vertex array pointer */
	commands[current_command++] = GP_PLBU_COMMAND_WRITE_CONF_REG( index_address, GP_PLBU_CONF_REG_INDEX_ARRAY_ADDR );
	commands[current_command++] = GP_PLBU_COMMAND_WRITE_CONF_REG( vertex_address, GP_PLBU_CONF_REG_VERTEX_ARRAY_ADDR );

	/* process 1 pixel rectangle (3 vertices) in indexed mode (can't use non-indexed since that require VS cooperation) */
	commands[current_command++] = GP_PLBU_COMMAND_PROCESS_SHADED_VERTICES( 0, 3,
	                                                                       GP_PLBU_PRIM_TYPE_PIXEL_RECTANGLES, 1 );

	MALI_DEBUG_ASSERT( QUAD_PLBU_COMMAND_COUNT == current_command, ("Invalid number of quad PLBU commands") );

	err = _mali_gp_job_add_plbu_cmds(gp_job, commands, MALI_ARRAY_SIZE(commands));
	if(err != MALI_ERR_NO_ERROR) return err;

	return MALI_ERR_NO_ERROR;
}

MALI_EXPORT mali_err_code _mali200_draw_quad( mali_frame_builder*      frame_builder,
                                              u32                      mali_vertex_address,
                                              u32                      mali_rsw_address)
{
	mali_internal_frame* frame;
	mali_err_code err;
	u8* index_mem;
	mali_addr mali_index_address;

	MALI_DEBUG_ASSERT_HANDLE( frame_builder );
	frame = GET_CURRENT_INTERNAL_FRAME(frame_builder);

	MALI_DEBUG_ASSERT(frame->state == FRAME_DIRTY, ("draw quad can only be done on dirty frames"));

	/* setup index buffer */
    index_mem = _mali_mem_pool_alloc(&frame->frame_pool, 3*sizeof(u8), &mali_index_address);
	if(index_mem == NULL) return MALI_ERR_OUT_OF_MEMORY;

	index_mem[0] = 0;
	index_mem[1] = 1;
	index_mem[2] = 2;

	/* add a PLBU job */
	err = setup_plbu_quad_job( frame->gp_job, mali_vertex_address, mali_index_address, mali_rsw_address);
	if(err != MALI_ERR_NO_ERROR) return err;

	return MALI_ERR_NO_ERROR;
}
