/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _M200_TILELIST_H_
#define _M200_TILELIST_H_

#include <mali_system.h>
#include <regs/MALIGP2/mali_gp_plbu_config.h>
#include <base/mali_memory.h>

/**
 * Each tilelist has a chunk format, which decides the size of each
 * chunk written by the PLBU. Currently this format is hardcoded. 
 *
 * This format decides both the size of the initial slave tile list chunks,
 * as well as the size of each chunk allocated off the PLBU heap. While 
 * it is technically possible to change it run-time, this is not recommended
 * by the HW guys. 
 *
 * The value is needed by the GP_PLBU_CONF_REG_PARAMS register, 
 * which is used by the API drivers a lot. The getter here is 
 * for easy access. 
 *
 * The four possible values defined in the TRM are as follows: 
 *  - 0: 128 bytes/block   (room for 16 commands)
 *  - 1: 256 bytes/block   (room for 32 commands)
 *  - 2: 512 bytes/block   (room for 64 commands)
 *  - 3: 1024 bytes/block  (room for 128 commands)
 * Each command will use 8 bytes. 
 * The pointer array list values must be aligned to the chunksize. 
 */
#define MALI_TILELIST_CHUNK_FORMAT 2
#define MALI_TILELIST_CHUNK_BYTESIZE (128 << MALI_TILELIST_CHUNK_FORMAT)

/**
 * This file provides a structure holding all information about the tile lists. 
 *
 * Each frame in the framebuilder is assigned to exactly one tilelist object. 
 * In fact, since the relation is a 1:1, the frame has one of these structs 
 * held within it. But the key point is, it can be replaced within that frame.
 *
 * The tilelist object holds the master tile lists, slave tile lists and 
 * the pointer array memory block used by the GP. It also holds the RSW cache. 
 *
 * These structures are built based on the following parameters
 *  - Frame builder dimensions 
 *  - PP split count
 * 
 * The framebuilder dimensions are passed in as a parameter when creating this object. 
 * They are constant from that point on, and cannot change. 
 *
 * The split count can change, if modified with the relevant setter (see below). 
 * When modifying it, the master tile lists are regenerated, but the slave and 
 * pointer array are not. It is not legal to change the split count if the 
 * associated frame is in use by the GPU. An assert will ensure this. 
 *
 */

struct mali_tilelist
{
	/* ALL of these fields are READ ONLY. Do not modify! */

	/* memory blocks */
	mali_mem_handle master_tile_list_mem; /* holds the setup for the master tile lists. Depending on splut count and resolution */
	mali_mem_handle slave_tile_list_mem; /* holds the setup for the slave tile lists. Depending on resolution */ 
	mali_mem_handle pointer_array_mem; /* holds the master pointer array memory. Depending on resolution */

	/* dimension parameters */
	u32 width;
	u32 height;
	u32 master_tile_width;
	u32 master_tile_height;
	u32 slave_tile_width;
	u32 slave_tile_height;
	u32 binning_scale_x;      /* equal to (master_tile_width / slave_tile_width) - 1 */
	u32 binning_scale_y;      /* equal to (master_tile_height / slave_tile_height) -1 */
#if !defined(USING_MALI200)
	mali_polygon_list_primitive_format polygonlist_format;
#endif
	u32 tile_pointers_to_load;  /* equal to MALI_ALIGN(slave_tile_height * slave_tile_height, 2) on all cores except M200, where it is 300  */

	/* split count parameters */
	u32 split_count;
	u32 pp_master_tile_list_offsets[MALI_MAX_PP_SPLIT_COUNT]; /* the offset into the master_tile_list_mem where each PP job starts */
};

/**
 * Allocate and initialize a new tilelist object.
 * @param width The width of the frame defining this tilelist
 * @param height The height of the frame defining this tilelist
 * @param split_count The desired splitcount for this frame
 * @param base_ctx The base context from which to allocate more ram 
 * @return The new mali_tilelist object. 
 */
MALI_CHECK_RESULT struct mali_tilelist* _mali_tilelist_alloc(
							   u32 width,
							   u32 height, 
                               u32 split_count,
                               mali_base_ctx_handle base_ctx);

/**
 * De-init and free the given tilelist object. 
 *
 * Callers should not call this directly, but rather use _mali_tilelist_deref
 *
 * @param list The tilelist to free
 */
void _mali_tilelist_free( struct mali_tilelist* list);

/** 
 * Reset the given tilelist. This will set the pointer array back to its initial values, but not touch the master/slave lists setup.
 * Effectively, this is throwing out all contents of the tile lists, while maintaining their layout. 
 * Should be called at the first use of a tilelist within a frame, at least before submitting the job to HW.
 *
 * @param list The tilelist to reset 
 * @return MALI_ERR_NO_ERROR, or MALI_ERR_OUT_OF_MEMORY
 * In case of the latter, the object is in an unusable state, but a later successful reset can restore it
 */
MALI_CHECK_RESULT mali_err_code _mali_tilelist_reset(struct mali_tilelist* list);

/**
 * Re-initialize the data structure, aligning it to a new split count. 
 * This function is equivalent to freeing the list, allocating a new one with the old list's 
 * dimensions and the new_splitcount as parameters. But notably faster. 
 * This function really only needs to set up a new master tile list layout and update the offsets, 
 * while the free/alloc option also require a new setup of all the pointers. 
 *
 * It is a very bad idea to call this function while the tilelist is in use. Avoiding this is left
 * as an exercise to the caller. Seriously, don't do it. No assert for this is available since 
 * availability is a question outside the scope of this structure. 
 *
 * @param list The list to modify
 * @param new_splitcount The new split count to use. If equal to the old split count, the function will early out. 
 * @return MALI_ERR_NO_ERROR, or MALI_ERR_OUT_OF_MEMORY.
 *         In case of the latter, the tilelist object is not modified in any way. 
 **/
MALI_CHECK_RESULT mali_err_code  _mali_tilelist_change_splitcount(struct mali_tilelist* list, u32 new_splitcount);



#endif /* _M200_TILELIST_H_ */

