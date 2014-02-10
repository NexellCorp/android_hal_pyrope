/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _M200_QUAD_H_
#define _M200_QUAD_H_

#include <mali_system.h>
#include <base/mali_memory.h>
#include <shared/m200_gp_frame_builder.h>
#include <regs/MALI200/mali_pp_cmd.h>

/**
 * Draw a quad onto the current frame.
 *
 * The call will set up a PLBU job on the current frame in the given 
 * framebuilder. That frame must be in the FRAME_DIRTY state. Callers should 
 * ensure that _frame_builder_write_lock has been called.  
 *
 * The drawcall will be a QUAD style drawcall using three vertex coordinates as 
 * defined in the TRM. 
 * Typical setup, w and h is given in pre-downscaled framebuilder coordinates:
 *    Coordinate 1: < x+w,  y,    0,  1 >
 *    Coordinate 0: <   x,  y,    0,  1 >
 *    Coordinate 2: <   x,  y+h,  0,  1 >
 *
 * The caller need to fill a memory block (or pool, whatever) with these 
 * coordinate data. The mali_addr of this memory block must be passed into 
 * the function. 3*4*sizeof(float) bytes
 * will be read by the HW starting from the given address. 
 *
 * The caller also need to set up a RSW describing the drawcall. This RSW 
 * must contain all data the PP will need, possibly including per-vertex 
 * varying data. A full sizeof(RSW) bytes worth of data will be read from 
 * the given address. 
 *
 * @param frame_builder The framebuilder to draw to. 
 * @param mali_vertex_address The address where Mali can fetch the position data
 * @param mali_rsw_address The address where Mali can fetch the RSW data
 **/
MALI_EXPORT mali_err_code _mali200_draw_quad( mali_frame_builder*      frame_builder,
                                              mali_addr                mali_vertex_address,
                                              mali_addr                mali_rsw_address);


/**
 * Draw a quad onto a given master tile list. 
 *
 * This function is featurewise identical to the one above, but adds the drawcall
 * to a master tile list memory area instead of a frame's GP PLBU job. 
 *
 * Keep in mind, the PP job content will normally be overwritten by the PLBU if 
 * you have a matching GP job. So this is mostly interesting in cases where you set
 * up the PP job yourself. Case in point: projobs.
 *
 * This function will add 16 bytes of data to the pointer given pointer at the given offset. 
 * Sizes are passed in for safety reasons. 
 *
 * @param master_tile_list_mem - A CPU memory pointer. Should probably point to some mapped GPU memory 
 * @param tile_list_mem_offset - The offset of the memory to place the commands onto. 
 *                               Inout parameter, will be increased by 16 bytes after calling. 
 * @param tile_list_mem_size   - The size of the master_tile_list_mem
 * @param mali_vertex_address  - The GPU address of the vertex buffer
 * @param mali_rsw_address     - The GPU address of the RSW 
 *
 * Note: There must be at least 16 available bytes in the master tile list mem block. An assert will 
 * trigger otherwise. 
 *
 */
MALI_STATIC_INLINE void _mali200_draw_pp_job_quad( u64*                    master_tile_list_mem,
                                            u32*                    tile_list_mem_offset, /* inout parameter */
                                            u32                     tile_list_mem_size,
                                            mali_addr               mali_vertex_address,
                                            mali_addr               mali_rsw_address)
{
	u64* ptr = (u64*) (((u8*)master_tile_list_mem) + *tile_list_mem_offset);

	MALI_DEBUG_ASSERT_POINTER(master_tile_list_mem);
	MALI_DEBUG_ASSERT(*tile_list_mem_offset + 2*sizeof(u64) <= tile_list_mem_size, ("Not enough room in master tile list")); 
	MALI_IGNORE(tile_list_mem_size); /* for release builds */

	MALI_PL_CMD_SET_BASE_VB_RSW(ptr[0], mali_vertex_address, mali_rsw_address);
	MALI_PL_PRIMITIVE(ptr[1], 
	                  MALI_PRIMITIVE_TYPE_PIXEL_RECT, 
					  0, /* rsw index, always equal to the base RSW at this point */
					  0, /* vertex0 index*/
					  1, /* vertex1 index*/ 
					  2  /* vertex2 index*/);
	/* update the inout ptr */
	*tile_list_mem_offset += 2*sizeof(u64);

}


#endif /* _M200_QUAD_H_ */
