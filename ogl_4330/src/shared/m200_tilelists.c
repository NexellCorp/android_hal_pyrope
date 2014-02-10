/*
 * This confidential and proprietary software may be used only as
 * authorized by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "m200_tilelists.h"
#include <regs/MALIGP2/mali_gp_core.h> /* for the MALIGP2_* constants */
#include <regs/MALI200/mali_pp_cmd.h> /* for the MALI_PL_CMD_* #defines */
#include <util/mali_math.h>
#include <shared/m200_gp_frame_builder.h>
#include <shared/m200_gp_frame_builder_struct.h>

/* Tuning for benchmarks at 720p has shown that traversing the bins in linear order is faster. Go figure.. */
/* This define is left in for tuning other benchmarks or for larger screens. */
/* #define HILBERT_ORDERING */
#define HILBERT_ORDERING_IN_BIN
#define FAVOUR_SQUARE_BINS

/**
 * Resets the pointer array to point into the beginning of the slave tile list memory
 * Expected inputs:
 * 	- The list->pointer_array_mem must be allocated
 * 	- The list->slave_tile_list_mem must be allocated
 * 	- Tilelist dimensions must be set up
 * Expected outputs:
 *  - Each pointer in the pointer array will be set to point to the slave tile list mem, at relevant offsets defined by MALI_TILELIST_CHUNK_BYTESIZE
 **/
MALI_STATIC mali_err_code setup_pointer_array(struct mali_tilelist* list)
{
	u32  i, base;
	u32 *pointer_array_mapped;
	u32 slave_tile_count = list->tile_pointers_to_load;

	MALI_DEBUG_ASSERT_HANDLE( list->pointer_array_mem );
	MALI_DEBUG_ASSERT_HANDLE( list->slave_tile_list_mem );

	/* map the pointer array into cpu accessible mem */
	pointer_array_mapped = (u32 *) _mali_mem_ptr_map_area( list->pointer_array_mem, 0, MALIGP2_POINTER_ARRAY_BYTE_SIZE, 8, MALI_MEM_PTR_WRITABLE );
	if( pointer_array_mapped == NULL ) return MALI_ERR_OUT_OF_MEMORY;

	base = _mali_mem_mali_addr_get( list->slave_tile_list_mem, 0 );

	for (i = 0; i < slave_tile_count; i++)
	{
		/* get the address of the first plbu cmd for each tile */
		pointer_array_mapped[ i ] = base + ( i * (MALI_TILELIST_CHUNK_BYTESIZE) );
	}

	/* unmap the memory before returning */
	_mali_mem_ptr_unmap_area( list->pointer_array_mem );
	return MALI_ERR_NO_ERROR;
}

/**
 * Resets the master tile lists to contain a commands of "begin tile (x,y); jump <relevant slave tile list>".
 * Each PP job will execute one <split_count>'th part of the generated master tile list each.
 *
 * Expected inputs:
 * 	- The list->master_tile_list_mem must be allocated
 * 	- The list->slave_tile_list_mem must be allocated
 * 	- The split count must be set
 * 	- Tilelist dimensions must be set up
 * Expected outputs:
 *  - The contents of the list->master_tile_list_mem will be updated to contain the new master tile list layout.
 **/

MALI_STATIC mali_err_code setup_master_tile_list(struct mali_tilelist* list)
{
	u32 master_tile_count = list->master_tile_width * list->master_tile_height;
	mali_tilelist_cmd *m_tile_list_ptrs[MALI_MAX_PP_SPLIT_COUNT];
	int entries_padded;
	mali_tilelist_cmd *m_total_tile_list;
	int pp_curr, pp_inc = 1;
	u32 curr_in_bin_tile;
	u32 curr_slave_tile;
	u32 tile_scale_x = list->binning_scale_x;
	u32 tile_scale_y = list->binning_scale_y;
	int pp_split_count = list->split_count;

	u32 bin_width = 1 << tile_scale_x;
	u32 bin_height = 1 << tile_scale_y;
	u32 slave_tile_count = list->slave_tile_width * list->slave_tile_height;

	int num_cmnds_per_split =  ( (master_tile_count/pp_split_count + 1) * 2) + 1;

#ifdef HILBERT_ORDERING 	/* setup some constants for the tiling-process */
	unsigned int maxdims = MAX(list->slave_tile_width, list->slave_tile_height);
	int ns = _mali_log_base2(_mali_ceil_pow2(maxdims));
	int ss = 0;
#endif
#ifdef HILBERT_ORDERING_IN_BIN
	unsigned int maxdim = MAX(list->master_tile_width, list->master_tile_height);
	int n_start = _mali_log_base2(_mali_ceil_pow2(maxdim));
#endif

	/* correct num_cmnds_per_split for alignment */
	num_cmnds_per_split = (num_cmnds_per_split + 3) & ~3; /* align each split list to 4 commands */

	/* unnecessary initialization, for warning removal purposes only */
	_mali_sys_memset(m_tile_list_ptrs, 0, sizeof(m_tile_list_ptrs));

	/* check input */
	MALI_DEBUG_ASSERT_HANDLE( list->master_tile_list_mem );
	MALI_DEBUG_ASSERT_HANDLE( list->slave_tile_list_mem );

	/* map the master tile list into cpu accessible mem */
	entries_padded = num_cmnds_per_split * pp_split_count;
	m_total_tile_list = (mali_tilelist_cmd *) _mali_mem_ptr_map_area(	list->master_tile_list_mem, 0,
																		entries_padded * sizeof( mali_tilelist_cmd ),
																		8, MALI_MEM_PTR_WRITABLE );
	if (m_total_tile_list == NULL) return MALI_ERR_OUT_OF_MEMORY;

	/* setup pointers to master tile lists for each PP */
	{
		int offset = 0;

		for (pp_curr = 0; pp_curr < pp_split_count; pp_curr++)
		{
			/* set up the m_tile_list_ptr (write pointer) and the offset to the beginning */
			list->pp_master_tile_list_offsets[pp_curr] = offset;
			m_tile_list_ptrs[pp_curr] = &m_total_tile_list[offset];

			/* update offset */
			offset += num_cmnds_per_split;
		}
	}

	/* pp_curr holds the index of the PP which the tile will be allocated to. This is incremented modulo number of PPs */
	pp_curr = 0;

	for (curr_slave_tile = 0; curr_slave_tile < slave_tile_count; curr_slave_tile++)
	{
		u32 jmp_destination;
		int s_tile_x, s_tile_y, s_tile;
		u32 x_width, y_height;
		u32 num_tiles_in_bin;
#ifdef HILBERT_ORDERING_IN_BIN
		int s = 0;
#endif
#ifdef HILBERT_ORDERING
		unsigned int xs, ys;
		do
		{
			int i;
			unsigned int states, rows;

			states = 0;
			xs = ys = 0;
			for (i = 2 * ns - 2; i >= 0; i -= 2)
			{
				rows = (4 * states) | ((ss >> i) & 3);
				xs = (xs << 1) | ((0x936c >> rows) & 1);
				ys = (ys << 1) | ((0x39c6 >> rows) & 1);
				states = (0x3e6b94c1 >> 2 * rows) & 3;
			}

			ss++;
		}
		while (xs >= list->slave_tile_width || ys >= list->slave_tile_height);
#else
		unsigned int xs = curr_slave_tile % list->slave_tile_width;
		unsigned int ys = curr_slave_tile / list->slave_tile_width;
#endif

		s_tile_x = xs;
		s_tile_y = ys;
		s_tile   = (s_tile_y * list->slave_tile_width) + s_tile_x;

		/* find the target destination address */
		jmp_destination = _mali_mem_mali_addr_get(
			list->slave_tile_list_mem,
			s_tile * MALI_TILELIST_CHUNK_BYTESIZE
		);

		/* Calculate the explicit number of tiles within this bin */		
		x_width = list->master_tile_width - (xs << tile_scale_x);
		y_height = list->master_tile_height - (ys << tile_scale_y);
		if( x_width > bin_width ) x_width = bin_width;
		if( y_height > bin_height ) y_height = bin_height;

		num_tiles_in_bin = y_height*x_width;

		for (curr_in_bin_tile = 0; curr_in_bin_tile < num_tiles_in_bin; curr_in_bin_tile++)
		{

#ifdef HILBERT_ORDERING_IN_BIN
			unsigned int x, y;
			do
			{
				int i;
				unsigned int state, row;

				state = 0;
				x = y = 0;
				for (i = 2 * n_start - 2; i >= 0; i -= 2)
				{
					row = (4 * state) | ((s >> i) & 3);
					x = (x << 1) | ((0x39c6 >> row) & 1);
					y = (y << 1) | ((0x936c >> row) & 1);
					state = (0x3e6b94c1 >> 2 * row) & 3;
				}

				s++;
			}
			while (x >= x_width || y >= y_height);
#else
			unsigned int x = curr_in_bin_tile % x_width;
			unsigned int y = curr_in_bin_tile / x_width;
#endif

			x += s_tile_x << tile_scale_x;
			y += s_tile_y << tile_scale_y;


			MALI_DEBUG_ASSERT(x < list->master_tile_width && y < list->master_tile_height, ("x or y out of bin bounds!"));
			MALI_DEBUG_ASSERT(pp_curr >= 0 && pp_curr < pp_split_count, ("pp_curr out of bounds!"));
			MALI_DEBUG_ASSERT(
				&m_tile_list_ptrs[pp_curr][2] <= &m_total_tile_list[entries_padded],
				("writing outside of master tile list")
			);

			/* Disable the entire code if asserts are disabled. Removes a warning */
#ifndef MALI_DEBUG_SKIP_ASSERT
			if ( pp_curr != pp_split_count-1 )
			{
				int next_pp_offset = list->pp_master_tile_list_offsets[pp_curr+1];
				MALI_DEBUG_ASSERT(pp_curr >= 0 && pp_curr < pp_split_count, ("pp_curr out of bounds!"));
				MALI_DEBUG_ASSERT(
					&m_tile_list_ptrs[pp_curr][2] <= &m_total_tile_list[next_pp_offset],
					("writing into next PP's master tile list")
				);
			}
#endif

			/* insert command to signal the start of a new unique tile */
			MALI_PL_CMD_BEGIN_NEW_TILE( *m_tile_list_ptrs[pp_curr]++, x, y );

			/* insert call into the slave tile list */
			MALI_DEBUG_ASSERT(pp_curr >= 0 && pp_curr < pp_split_count, ("pp_curr out of bounds!"));
			MALI_PL_CMD_UNCONDITIONAL_CALL( *m_tile_list_ptrs[pp_curr]++, jmp_destination );

			/* next pp for next tile */
			pp_curr += pp_inc;
			if (pp_curr >= pp_split_count)
			{
			 	pp_curr = pp_split_count - 1;
				pp_inc = -1;
			}
			else if (pp_curr < 0)
			{
			 	pp_curr = 0;
				pp_inc = 1;
			}

			MALI_DEBUG_ASSERT(pp_curr >= 0 && pp_curr < pp_split_count, ("pp_curr out of bounds!"));
		}
	}

	/* Insert end command in all the lists */
	for (pp_curr = 0; pp_curr < pp_split_count; pp_curr++)
	{
#ifndef MALI_DEBUG_SKIP_ASSERT
		if ( pp_curr != pp_split_count-1 )
		{
			int next_pp_offset = list->pp_master_tile_list_offsets[pp_curr+1];
			MALI_DEBUG_ASSERT(
				&m_tile_list_ptrs[pp_curr][1] <= &m_total_tile_list[next_pp_offset],
				("writing into next PP's master tile list")
			);
		}
#endif
		MALI_DEBUG_ASSERT(
			&m_tile_list_ptrs[pp_curr][1] <= &m_total_tile_list[entries_padded],
			("writing outside of master tile list")
		);

		MALI_PL_CMD_END_OF_LIST( (*m_tile_list_ptrs[pp_curr]) );
	}

	/* unmap the memory before returning */
	_mali_mem_ptr_unmap_area( list->master_tile_list_mem );
	return MALI_ERR_NO_ERROR;
}

/* allocate the tile list memory blocks */
MALI_STATIC mali_err_code allocate_memoryblocks(struct mali_tilelist* list, mali_base_ctx_handle base_ctx)
{
	u32 slave_tile_count = list->tile_pointers_to_load;

/* DLBU used instead of master tile list for Mali-450 */
#if !defined(USING_MALI450)
	u32 master_tile_count = list->master_tile_width * list->master_tile_height;
	u32 master_tile_list_size;
	/* 2 commands per tile plus 1 end command */
	u32 num_cmnds_per_split =  ( (master_tile_count/list->split_count + 1) * 2) + 1;

	/* correct num_cmnds_per_split for alignment */
	num_cmnds_per_split = (num_cmnds_per_split + 3) & ~3; /* align each split list to 4 commands */

	/* alloc enough space for the number of split lists */
	master_tile_list_size = num_cmnds_per_split * list->split_count * sizeof(mali_tilelist_cmd);

 	/* allocate mem for begin_new_tile & call_addr commands for each tile, plus 1 end_of_list command */
	list->master_tile_list_mem = _mali_mem_alloc( base_ctx,
	                                              master_tile_list_size, 64,
	                                              MALI_PP_READ | MALI_CPU_WRITE );
	if(list->master_tile_list_mem == MALI_NO_HANDLE) return MALI_ERR_OUT_OF_MEMORY;
#endif /* !defined(USING_MALI450) */

	/* allocate the initial slave tile list, aligned to the maximum possible tile_list_block_byte_size */
	list->slave_tile_list_mem = _mali_mem_alloc( base_ctx, slave_tile_count * MALI_TILELIST_CHUNK_BYTESIZE, 1024,
	                                             MALI_PP_READ | MALI_GP_WRITE );
	if(list->slave_tile_list_mem == MALI_NO_HANDLE) return MALI_ERR_OUT_OF_MEMORY;

	/* constant size pointer array for GP2 */
	list->pointer_array_mem = _mali_mem_alloc( base_ctx, MALIGP2_POINTER_ARRAY_BYTE_SIZE, 16,
	                                           MALI_GP_READ | MALI_GP_WRITE);
	if(list->pointer_array_mem == MALI_NO_HANDLE) return MALI_ERR_OUT_OF_MEMORY;

	return MALI_ERR_NO_ERROR;
}

/**
 * Calculate the master and slave tile list sizes and format.
 * The function will modify the master and slave tile list members of the input
 * parameter 'list', based on the width and height parameter given.
 */
MALI_STATIC void calculate_list_sizes_and_format(struct mali_tilelist* list)
{
	u32 m_tile_width, m_tile_height;
	u32 s_tile_width, s_tile_height;
	u32 tile_scale_x, tile_scale_y;

	MALI_DEBUG_ASSERT_POINTER(list);
	MALI_DEBUG_ASSERT( list->width > 0, ("list width must be > 0"));
	MALI_DEBUG_ASSERT( list->height > 0, ("list height must be > 0"));

	/* round up to nearest 16 pixels, then divide by it */
	m_tile_width  = (list->width + (MALI200_TILE_SIZE-1)) / MALI200_TILE_SIZE;
	m_tile_height = (list->height + (MALI200_TILE_SIZE-1)) /MALI200_TILE_SIZE;

	MALI_DEBUG_ASSERT( m_tile_width > 0, ("tile width must be > 0"));
	MALI_DEBUG_ASSERT( m_tile_height > 0, ("tile height must be > 0"));

	/* find the size of the slave tile list */
	s_tile_width  = m_tile_width;
	s_tile_height = m_tile_height;
	tile_scale_x  = tile_scale_y = 0;
#if !defined(USING_MALI450)
	while( s_tile_width * s_tile_height > MALIGP2_MAX_TILES )
	{
#ifdef FAVOUR_SQUARE_BINS
		/* The tests for s_tile width/height are so we don't increase the tile_scale needlessly for very thin or shallow output buffers */
		if( tile_scale_x != tile_scale_y && s_tile_width != 1 && s_tile_height != 1)
		{
			if( tile_scale_x < tile_scale_y )
			{
				tile_scale_x++;
				s_tile_width = (m_tile_width+(1<<tile_scale_x)-1) >> tile_scale_x;
			}
			else
			{
				tile_scale_y++;
				s_tile_height = (m_tile_height+(1<<tile_scale_y)-1) >> tile_scale_y;
			}
		}
		else
#endif
		{
			if( s_tile_width > s_tile_height )
			{
				tile_scale_x++;
				s_tile_width = (m_tile_width+(1<<tile_scale_x)-1) >> tile_scale_x;
			}
			else
			{
				tile_scale_y++;
				s_tile_height = (m_tile_height+(1<<tile_scale_y)-1) >> tile_scale_y;
			}
		}
	}
#else
	/**Hack for MALI450 dumps to be able to use the DynamicLoadBalancing Unit */
	while( s_tile_width * s_tile_height > MALIGP2_MAX_TILES )
	{
		if( s_tile_width > s_tile_height )
		{
			tile_scale_x*=2;
			if (!tile_scale_x) tile_scale_x = 1;
			s_tile_width = (m_tile_width+(1<<tile_scale_x)-1) >> tile_scale_x;
		}
		else
		{
			tile_scale_y*=2;
			if (!tile_scale_y) tile_scale_y = 1;
			s_tile_height = (m_tile_height+(1<<tile_scale_y)-1) >> tile_scale_y;
		}
	}
#endif /* !defined(USING_MALI450) */

	/* max 6 bits for each scale factor. This is a HW limit */
	MALI_DEBUG_ASSERT( !((tile_scale_x & ~0x3f) | (tile_scale_y & ~0x3f)), ("master tile list is too big\n") );

#if !defined(USING_MALI200)
	/* now, determine the best polygon list primitive format and max number of RSW indices */
	{
		u32 min_scale = (tile_scale_x < tile_scale_y) ? tile_scale_x : tile_scale_y;

		if (min_scale > MALI_PLIST_FORMAT_SUPER_TILING_4X4)
		{
			list->polygonlist_format = MALI_PLIST_FORMAT_SUPER_TILING_4X4;
		}
		else
		{
			list->polygonlist_format = (mali_polygon_list_primitive_format)min_scale;
		}
	}

#else
	/* else don't set anything. Scary, but that's what the old code did. */
#endif

#if defined(USING_MALI200)
	/* On M200, the number of tiles is hardcoded to 300 */
	list->tile_pointers_to_load = MALIGP2_MAX_TILES;
#else
	/* On M300, M400 and M450, the number of tiles must be an even number */
	list->tile_pointers_to_load = MALI_ALIGN(s_tile_width * s_tile_height,2);
#endif

	/* store all the parameteres*/
	list->master_tile_width  = m_tile_width;
	list->master_tile_height = m_tile_height;
	list->slave_tile_width   = s_tile_width;
	list->slave_tile_height  = s_tile_height;
	list->binning_scale_x = tile_scale_x;
	list->binning_scale_y = tile_scale_y;

}

struct mali_tilelist* _mali_tilelist_alloc(
                   u32 width,
                   u32 height,
                   u32 split_count,
				   mali_base_ctx_handle base_ctx)
{
	struct mali_tilelist* retval = NULL;
	mali_err_code err;

	retval = _mali_sys_calloc(1, sizeof(struct mali_tilelist));
	if(retval == NULL) return retval;

	MALI_DEBUG_ASSERT(split_count <= MALI_MAX_PP_SPLIT_COUNT, ("split count > MALI_MAX_PP_SPLIT_COUNT is not legal"));

	retval->width = width;
	retval->height = height;
	retval->split_count = split_count;

	calculate_list_sizes_and_format(retval);

	err = allocate_memoryblocks(retval, base_ctx);
	if(err != MALI_ERR_NO_ERROR)
	{
		_mali_tilelist_free(retval);
		return NULL;
	}

/* DLBU used instead of master tile list for Mali-450 */
#if !defined(USING_MALI450)
	err = setup_master_tile_list(retval);
	if(err != MALI_ERR_NO_ERROR)
	{
		_mali_tilelist_free(retval);
		return NULL;
	}
#endif /* !defined(USING_MALI450) */

	err = setup_pointer_array(retval);
	if(err != MALI_ERR_NO_ERROR)
	{
		_mali_tilelist_free(retval);
		return NULL;
	}


	return retval;
}

void _mali_tilelist_free( struct mali_tilelist* list)
{
	MALI_DEBUG_ASSERT_POINTER( list );

	if(list->master_tile_list_mem) _mali_mem_free( list->master_tile_list_mem );
	if(list->slave_tile_list_mem) _mali_mem_free( list->slave_tile_list_mem );
	if(list->pointer_array_mem) _mali_mem_free( list->pointer_array_mem );

	_mali_sys_free(list);
}

mali_err_code _mali_tilelist_reset( struct mali_tilelist* list)
{
	MALI_DEBUG_ASSERT_POINTER( list );
	return setup_pointer_array(list);
}

mali_err_code  _mali_tilelist_change_splitcount(struct mali_tilelist* list, u32 new_split_count)
{
	mali_err_code err;
	u32 old_split_count;
	MALI_DEBUG_ASSERT_POINTER( list );
	if(new_split_count == list->split_count) return MALI_ERR_NO_ERROR; /* early out, no change needed */

	old_split_count = list->split_count;
	list->split_count = new_split_count;

/* DLBU used instead of master tile list for Mali-450 */
#if !defined(USING_MALI450)
	err = setup_master_tile_list( list );
	if(err != MALI_ERR_NO_ERROR)
	{
		/* set object back to its original state */
		list->split_count = old_split_count;
		return err;
	}
#endif /* !defined(USING_MALI450) */

	return MALI_ERR_NO_ERROR;
}
