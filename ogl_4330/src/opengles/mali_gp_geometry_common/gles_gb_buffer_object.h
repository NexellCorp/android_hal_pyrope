/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef GLES_GB_BUFFER_OBJECT_H
#define GLES_GB_BUFFER_OBJECT_H

#include "util/mali_mem_ref.h"
#include "opengles/gles_common_state/gles_vertex_array.h"
#include "../gles_gb_interface.h"
#include "gles_gb_cache.h"
#include "gles_geometry_backend_context.h"

#define GLES_THRESHOLD_RATIO_TO_ENABLE_OPTIMIZATION  2 
#define GLES_GB_INDEX_RANGE_CACHE_SIZE 0x100
#define GLES_BB_HASHTABLE_SIZE  0x100


typedef struct gles_gb_buffer_elem_conversion_info
{
	u8 num_elems;
	u8 elem_size;
	u8 stride;
	u8 offset;
} gles_gb_buffer_elem_conversion_info;

#define GLES_GB_BUFFER_IS_ENDIANESS_CONVERTED(b) ((b)->elems[0].num_elems != 0)
struct gles_gb_buffer_object_data
{
	mali_mem_ref *mem;                                            /**< The mali-memory reference */
	gles_gb_buffer_elem_conversion_info elems[16];                /**< Size of element in buffer */
	cache_main_struct* range_cache; /**< The list of range_caches to save result from calculating min and max of a range */
#ifdef MALI_BB_TRANSFORM
	cache_main_struct* bounding_box_cache; /**< The list of range_caches to save result for primitives bounding box */
#endif
	mali_base_frame_id last_used_frame;                           /**< The last used frame for this VBO, for callback reasons */
};

typedef struct bb_binary_node
{
    gles_gb_bounding_box bb;
    index_range rng;
    u32 offset; /*the distance in index buffer from the first valid index*/
    u32 count;  /*index count*/
} bb_binary_node;

#include "base/mali_byteorder.h"

#if MALI_BIG_ENDIAN

/**
 * @brief Converts data endianess in buffer object according to elements specified in vas.
 * @param vas Vertex attrib array associated with buffer.
 * @param num_vas Number of elements in vas.
 */
void _gles_gb_buffer_object_process_endianess_for_va(gles_vertex_attrib_array *vas, u32 num_vas);

/**
 * @brief Converts data order in buffer object that contains only one type of elements. This function is called only for indices arrays.
 * @param vbo The buffer object to convert.
 * @param buffer_size Number of bytes in buffer.
 * @param num_elements Number of data elements in buffer.
 * @param element_size Size of one element in bytes.
 * @param stride Stride for data row.
 * @param offset Offset at which the first element is located within row.
 */
void _gles_gb_buffer_object_process_endianess(struct gles_gb_buffer_object_data *vbo, u32 buffer_size, u8 num_elements, u8 element_size, u8 stride, u8 offset);

#else
#define _gles_gb_buffer_object_process_endianess_for_va(vas, num_vas) { } while(0)
#define _gles_gb_buffer_object_process_endianess(vbo, buffer_size, num_elements, element_size, stride, offset) { } while(0)
#endif

/**
 * @brief Gets the min and max address of the given range with the given parameters, returns a saved result from the cache if present
 * @param buffer_data The buffer to get min and max from
 * @param offset The offset into the buffer-datas memory
 * @param count The number of elements to scan
 * @param type The type of the data in the range we are scanning(GL_UNSIGNED_BYTE or GL_UNSIGNED_SHORT)
 * @param ret The pointer to return the index ranges
 * @param coherence  The factor implies the coherence of index buffer
 * @param vs_range_count The pointer to return the number of the ranges
 */
void _gles_gb_buffer_object_data_range_cache_get(struct gles_gb_buffer_object_data *buffer_data,
															 GLenum mode,
															 u32 offset,
															 u32 count,
															 GLenum type,
															 index_range **ret,
															 u32 *coherence,
															 u32 *range_count);

#ifdef MALI_BB_TRANSFORM
/**
 * @brief Gets the min and max values for drawcall bounding box, returns a saved result from the cache if present
 * @param ctx The gles context
 * @param gb_data The buffer (cache) to get min and max from
 * @param vbo_offset vbo offset specified by application
 * @param stride vertices data stride
 * @param vertex_size size required to store 1 vertex
 * @param type type of the vertices data
 * @param ret  returning bounding box data
 * @param node_total returning the number of the nodes in a binary tree
 */
mali_err_code _gles_gb_get_bb_from_cache( struct gles_context *ctx, struct gles_gb_buffer_object_data *gb_data, int vbo_offset, u32 stride, u32 vertex_size, bb_binary_node** ret, u16 *node_total);
#endif

/**
 * @Clears all the cached results
 * @param buffer_data The buffer to clear the cached result from
 */
void _gles_gb_buffer_object_data_range_cache_dirty(struct gles_gb_buffer_object_data *buffer_data);


#endif /* GLES_GB_BUFFER_OBJECT_H */
