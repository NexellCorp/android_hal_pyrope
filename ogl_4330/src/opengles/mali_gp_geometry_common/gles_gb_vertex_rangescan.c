/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_gb_vertex_rangescan.h"


MALI_STATIC  void _gles_scan_indices_loop_bit8(u8* ptr, u32 count, index_range *idx_range, u32 shiftcount, u16 min_idx)
{
    u16 idx = *(ptr)++;
    u32 range_index = (idx - min_idx) >> shiftcount;
    u32 i = count;
    index_range *range_addr = &idx_range[ range_index ];
    u16 min = range_addr->min;
    u16 max = range_addr->max;
    while(1)
    {
        if(min > idx) min = idx;
        if(max < idx) max = idx;
        i--;
        if(!i) break;
        idx = *(ptr)++;
        range_index = (idx - min_idx) >> shiftcount;
        if( range_addr == &idx_range[ range_index ])
        {
            continue;
        }
        range_addr->min = min;
        range_addr->max = max;
        range_addr = &idx_range[ range_index ];
        min = range_addr->min;
        max = range_addr->max;
    }
    range_addr->min = min;
    range_addr->max = max;
}    

MALI_STATIC  void _gles_scan_indices_loop_bit16(u16* ptr, u32 count, index_range *idx_range, u32 shiftcount, u16 min_idx)
{
    u16 idx = *(ptr)++;
    u32 range_index = (idx - min_idx) >> shiftcount;
    u32 i = count;
    index_range *range_addr = &idx_range[ range_index ];
    u16 min = range_addr->min;
    u16 max = range_addr->max;
    while(1)
    {
        if(min > idx) min = idx;
        if(max < idx) max = idx;
        i--;
        if(!i) break;
        idx = *(ptr)++;
        range_index = (idx - min_idx) >> shiftcount;
        if( range_addr == &idx_range[ range_index ])
        {
            continue;
        }
        range_addr->min = min;
        range_addr->max = max;
        range_addr = &idx_range[ range_index ];
        min = range_addr->min;
        max = range_addr->max;
    }
    range_addr->min = min;
    range_addr->max = max;
}

MALI_STATIC void _gles_gb_merge_range( struct gles_gb_context* gb_ctx, index_range* sorted_array, u32 array_count )
{
    u32 i;
    u32 vs_index_range = 0;

    MALI_DEBUG_ASSERT_POINTER(gb_ctx);
    MALI_DEBUG_ASSERT_POINTER(sorted_array);

    for( i = 1; i < array_count; i++ )
    {
        MALI_DEBUG_ASSERT( sorted_array[i].min >= sorted_array[i-1].min, ("The nodes are not ordered"));

        if( sorted_array[i].min - sorted_array[i-1].max > GLES_THRESHOLD_TO_START_NEW_INDEX_RANGE )
        {
            vs_index_range++;
            sorted_array[vs_index_range].max = sorted_array[i].max;
            sorted_array[vs_index_range].min = sorted_array[i].min;
        }
        else
        {
            sorted_array[vs_index_range].max = MAX( sorted_array[vs_index_range].max, sorted_array[i].max);
        }
    }

    MALI_DEBUG_ASSERT( vs_index_range < MALI_LARGEST_INDEX_RANGE, ("vs_index_range is larger than the MALI_LARGEST_INDEX_RANGE"));
    gb_ctx->parameters.vs_range_count = vs_index_range + 1;
    gb_ctx->parameters.idx_vs_range = sorted_array;
}

void _gles_gb_sort_and_merge_range( struct gles_gb_context* gb_ctx,
									 u32 node_total,
									 bb_binary_node* bb_array,
									 const index_range *vs_range_buffer )
{
    u32 i;
    u32 j;
    int k = 0;
    u32 array_count = 0;
    u32 start_node;
    u32 index_count;
    u32 leaves_node_cnt = (node_total + 1)/2;
    u32 leaves_node_start = node_total - leaves_node_cnt;
    u32 leaves_node_size = bb_array[leaves_node_start].count;

    /* The largest number of leaves traversed is the number of bottom leaves */
    /* This is the same as the largest number of vs_ranges */
    index_range* sorted_array = (index_range *) vs_range_buffer;
    index_range tmp_node;

    MALI_DEBUG_ASSERT_POINTER(gb_ctx);
    MALI_DEBUG_ASSERT_POINTER(bb_array);
    MALI_DEBUG_ASSERT( node_total > 1, ("If the binary tree only has one node, this function is not needed"));

    /* Insertion Sort the range of a leaf node into sorted_array */
    for( i = 0; i < gb_ctx->parameters.plbu_range_count; i++)
    {
        index_count = gb_ctx->parameters.idx_plbu_range[i].count;
        start_node = gb_ctx->parameters.idx_plbu_range[i].start /leaves_node_size + leaves_node_start;

        for( j = start_node; j < MIN(start_node + index_count/leaves_node_size, node_total); j++ )
        {
            if( !i && j == start_node )
            {
                sorted_array[0] = bb_array[j].rng;
                array_count++;
                continue;
            }

            tmp_node = bb_array[j].rng;
            k = array_count - 1;
            while(k >= 0 && tmp_node.min < sorted_array[k].min)
            {
                sorted_array[k + 1] = sorted_array[k];
                k--;
            }

            sorted_array[k+1] = tmp_node;
            array_count++;
        }
    }

    /*Merge the sorted_array*/
    _gles_gb_merge_range( gb_ctx, sorted_array, array_count);
}


void _gles_scan_indices( index_range *idx_range, GLint count, GLenum type, u32 *coherence, const void *indices)
{
	GLint local_count = count - 1;

	MALI_DEBUG_ASSERT(local_count >= 0, ( "Indices count is zero!"));
	MALI_DEBUG_ASSERT_POINTER(indices);
	MALI_DEBUG_ASSERT_POINTER(idx_range);
	
	/* Indices buffer are in cpu byte order, it is converted to mali byte order
	 * in function _gles_gb_setup_plbu_index_array. */
	if(coherence != NULL)
	{	 
		u32 distance = 0;
		switch( type )
		{
			case GL_UNSIGNED_BYTE:
			{
				u8* idx_8bit = (u8 *) indices;
				u16  idx = *idx_8bit++;
				register u16 min = idx;
				register u16 max = idx;
				while( local_count-- )
				{
					u16 idx2 = *idx_8bit++;
					if ( idx2 < min ) min = idx2;
					if ( idx2 > max ) max = idx2;

	                distance += _mali_sys_abs(idx - idx2);
	                idx = idx2;
		        }
				idx_range[0].min = min;
				idx_range[0].max = max;
			}
			break;

			case GL_UNSIGNED_SHORT:
			{
				u16* idx_16bit = (u16 *) indices;
				u16  idx = *idx_16bit++;
				register u16 min = idx;
				register u16 max = idx;
				while( local_count-- )
				{
					u16 idx2 = *idx_16bit++;
					if ( idx2 < min ) min = idx2;
					if ( idx2 > max ) max = idx2;

	                distance += _mali_sys_abs(idx - idx2);
	                idx = idx2;
		        }
				idx_range[0].min = min;
				idx_range[0].max = max;
			}
			break;

			default: MALI_DEBUG_ASSERT(0, ("invalid type 0x%x", type));
		}

		*coherence = distance / count;
	}
	else
	{
		switch( type )
		{
			case GL_UNSIGNED_BYTE:
			{
				u8* idx_8bit = (u8 *) indices;
				u16  idx = *idx_8bit++;
				register u16 min = idx;
				register u16 max = idx;
				while( local_count-- )
				{
					idx = *idx_8bit++;
					if ( idx < min ) min = idx;
					if ( idx > max ) max = idx;
		        }
				idx_range[0].min = min;
				idx_range[0].max = max;
			}
			break;

			case GL_UNSIGNED_SHORT:
			{
				u16* idx_16bit = (u16 *) indices;
				u16  idx = *idx_16bit++;
				register u16 min = idx;
				register u16 max = idx;
				while( local_count-- )
				{
					idx = *idx_16bit++;
					if ( idx < min ) min = idx;
					if ( idx > max ) max = idx;
		        }
				idx_range[0].min = min;
				idx_range[0].max = max;
			}
			break;

			default: MALI_DEBUG_ASSERT(0, ("invalid type 0x%x", type));
		}
	}
}


void _gles_scan_indices_range( index_range *idx_range, GLint count, u32 *range_count, GLenum type, const void *indices)
{
    u16 *idx_16bit = NULL;
    u8  *idx_8bit = NULL;
    u8 range_count_bit;
    u32 i;
    u32 j = 0;
    u32 shiftcount;

    u16 min_idx = idx_range[0].min;
    u16 max_idx = idx_range[0].max;

    MALI_DEBUG_ASSERT_POINTER(indices);
    MALI_DEBUG_ASSERT_POINTER(idx_range);
    MALI_DEBUG_ASSERT_POINTER(range_count);

    range_count_bit = _mali_sys_log2(MIN( (max_idx - min_idx + 1)>>4, MIN(2*count,MALI_LARGEST_INDEX_RANGE)));
    *range_count <<= range_count_bit;

    /* Calculate the length of each sub range should be  (max_idx - min_idx + 1)/16,
       so range_index will be   (idx - min_idx)*16/(max_idx - min_idx + 1).
       Use shift instead of integer division since the range_index doesn't need to be precise */
    /* ceil(log2(n)) = 32 - __builtin_clz(n - 1)*/
    shiftcount = 32 - __builtin_clz(max_idx - min_idx ) - range_count_bit;

    for( i = 0; i < *range_count; i++)
    {
        idx_range[i].min = 0xffff;
        idx_range[i].max = 0x0;
    }

    switch(type)
    {
        case GL_UNSIGNED_BYTE:
            idx_8bit = (u8*)indices;
            _gles_scan_indices_loop_bit8(idx_8bit, count, idx_range, shiftcount, min_idx);
            break;

        case GL_UNSIGNED_SHORT:
            idx_16bit = (u16 *) indices;
            _gles_scan_indices_loop_bit16(idx_16bit, count, idx_range, shiftcount, min_idx);
            break;

        default: MALI_DEBUG_ASSERT(0, ("invalid type 0x%x", type));            
    }

    *range_count = ((max_idx - min_idx ) >> shiftcount) + 1;

    for( i = 1; i < *range_count; i++)
    {
        if(idx_range[i].min == 0xffff && idx_range[i].max != 0xffff)
        {
            continue;
        }

        if( idx_range[i].min - idx_range[j].max < GLES_THRESHOLD_TO_START_NEW_INDEX_RANGE )
        {
            idx_range[j].max = idx_range[i].max;
        }
        else
        {
            j++;
            idx_range[j].min = idx_range[i].min;
            idx_range[j].max = idx_range[i].max;
        }
    }

    *range_count = j + 1;

    MALI_DEBUG_ASSERT( *range_count <= MALI_LARGEST_INDEX_RANGE, ("RANGE COUNT exceed the limitation MALI_LARGEST_INDEX_RANGE"));
}

