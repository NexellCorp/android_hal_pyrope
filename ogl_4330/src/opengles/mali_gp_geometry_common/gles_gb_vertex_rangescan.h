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
#include "gles_gb_buffer_object.h"


/**
 * Sorts and merge range nodes in a bb binary tree
 * @param gb_ctx  geometry backend context
 * @param node_total number of nodes to scan
 * @param bb_array output bb binary tree range nodes array.
 * @param vs_range_buffer pointer to memory to be filled in with a vs_range.
 */
void _gles_gb_sort_and_merge_range( struct gles_gb_context* gb_ctx, u32 node_total, bb_binary_node* bb_array, const index_range *vs_range_buffer );


/**
 * @brief Scan through the indices and find the range of vertices that are referenced by
 *        them, i.e. the min and max indices
 * @param idx_vs_range A pointer to index range which contains the minimum and maximum in the set of indices
 * @param count The number of indices in the indices list
 * @param type The type of the indices
 * @param cohesion The factor to measure how randomly and disordered the indices are in the index buffer
 * @param indices The indices to scan through
 */
void _gles_scan_indices( index_range *idx_vs_range, GLint count, GLenum type, u32 *cohesion, const void *indices);

/**
 * @brief Scan through the indices and find the ranges of vertices that are referenced by
 *        them. 
 * @param idx_vs_range A pointer to index ranges which contains the minimum and maximum in the set of indices
 * @param count The number of indices in the indices list
 * @param vs_range_count The number of the subranges
 * @param type The type of the indices
 * @param indices The indices to scan through
 */
void _gles_scan_indices_range( index_range *idx_vs_range, GLint count, u32 *vs_range_count, GLenum type, const void *indices);

