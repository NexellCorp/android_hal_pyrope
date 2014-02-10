/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles_gb_interface.h
 * @brief The standardized interface for geometry backends. these functions must be implemented in all backends
 */

#ifndef _GLES_GEOMETRY_BACKEND_H_
#define _GLES_GEOMETRY_BACKEND_H_


#include <mali_system.h>
#include <gles_base.h>
#include <gles_state.h>
#include "gles_context.h"	/* needed for struct gles_gb_context */

/* byte alignment of context (extra space allocated to ensure this) */
#define CONTEXT_ALIGNMENT 0x20


#define BB_VERTICES_ELEMENTS_COUNT 8*4  /* 8 bb vertices, each has 4 components x,y,z,w */

/* opaque type for the buffer object data to be used as a handle */
typedef struct gles_gb_buffer_object_data gles_gb_buffer_object_data;

struct gles_gb_program_rendering_state;

typedef struct index_range
{
	u16 min;          /*the min index of this range*/ 
	u16 max;          /*the max index of this range*/
} index_range;

typedef struct plbu_range
{
    u32 start;        /*the offset to the beginning of the index buffer */
    u32 count;        /*the count of indices */
} plbu_range;

typedef struct gles_gb_bounding_box
{
	/* bounding box, 6 maxmins. */ 
	/* xmax, ymax, zmax, xmin, ymin, zmin */
	float minmax[6];       
} gles_gb_bounding_box;

/**
 * @brief initialize the geometry backend
 * @param ctx The pointer to the GLES context
 * @return A boolean telling us if the initialization failed or not.
 */
mali_err_code MALI_CHECK_RESULT _gles_gb_init( struct gles_context *ctx );

/**
 * @brief free all memory used by the geometry backend
 * @param ctx The pointer to the GLES context
 */
void _gles_gb_free( struct gles_context *ctx );

/**
 * @brief draw indexed geometry with ranges
 * @param ctx The pointer to the GLES context
 * @param mode The Primitive to be drawn(GL_TRIANGLES, GL_POINTS, etc.)
 * @param idx_range Index ranges used to draw geometry
 * @param range_count How many index ranges
 * @param count The number of indices to be used
 * @param transformed_count The number of transformed vertex count
 * @param type The type of the indices in the array
 * @param cohesion The factor implies the cohesion status of index buffer
 * @param indices The array of indices to be used
 * @retun Tels us if drawing with indexed range failed
 */
mali_err_code MALI_CHECK_RESULT _gles_gb_draw_indexed_range(  gles_context *ctx, 
																u32 mode,
																const index_range *idx_vs_range,
																const index_range *vs_range_buffer,
																u32 vs_range_count,
																u32 count,
																u32 *transformed_count,
																u32 type,
																u32 coherence,
																const void *indices );

/**
 * @brief draw indexed geometry
 * @param ctx The pointer to the GLES context
 * @param mode The Primitive to be drawn(GL_TRIANGLES, GL_POINTS, etc.)
 * @param count The number of indices to be used
 * @param type The type of the indices in the array
 * @param indices The array of indices to be used
 * @return Tells us if drawing failed
 */
mali_err_code MALI_CHECK_RESULT _gles_gb_draw_indexed( struct gles_context *ctx,GLenum mode, u32 count, GLenum type, const void *indices );

/**
 * @brief draw non-indexed geometry
 * @param ctx The pointer to the GLES context
 * @param mode The primitive to be drawn
 * @param first The index of the first vertex to be drawn
 * @param count The number of vertices to be drawn
 * @return Tells us if drawing failed
 */
mali_err_code MALI_CHECK_RESULT _gles_gb_draw_nonindexed( struct gles_context *ctx, GLenum mode, s32 first, u32 count );

/**
 * @brief allocate quad coordinates from frame pool
 * @param ctx The pointer to the GLES context
 * @param pool The pointer to the frame pool
 * @param out_position_mem_addr Returns the gpu address of the allcated memory.
 */
mali_err_code MALI_CHECK_RESULT _gles_gb_alloc_position(struct gles_context *ctx, mali_mem_pool* pool, mali_addr *out_position_mem_addr);

/**
 * @brief finish frame
 * @param ctx The pointer to the GLES context
 */
void _gles_gb_finish( struct gles_context *ctx );

/**
 * @brief create a new VBO-dataset with given data
 * @param target Specifies which buffer is to be altered
 * @param size The size of the buffer
 * @param data Pointer to the buffer-data
 * @param usage Indication of how the buffer will be used
 * @return A pointer to where the buffer-data is stored internally
 */
gles_gb_buffer_object_data *_gles_gb_buffer_data( mali_base_ctx_handle base_ctx, GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);

/**
 * @brief fill a part of a VBO with data by creating a copy of current VBO-dataset with given data
 * @param target Specifies which buffer is to be altered
 * @param vbo_data The pointer to the original buffer-data
 * @param vbo_size The size of the original buffer-data
 * @param target Specifies which buffer is to be altered
 * @param offset The offset to start to cpy the new data to
 * @param size The size of the new data
 * @param data The new buffer-data
 */
gles_gb_buffer_object_data * _gles_gb_buffer_sub_data(mali_base_ctx_handle base_ctx, gles_gb_buffer_object_data *vbo_data, GLuint vbo_size, GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data);

/**
 * @brief free a VBO-dataset
 * @param vbo_data Pointer to the buffer-data
 */
void _gles_gb_free_buffer_data(gles_gb_buffer_object_data *vbo_data);

/**
 * Gets the draw context from the gles context, must be used as the draw context
 * may be padded to ensure certain alignment.
 * param ctx The pointer to the GLES-context to get the draw context
 */
MALI_STATIC_INLINE struct gles_gb_context * _gles_gb_get_draw_context( struct gles_context * const ctx )
{
	MALI_DEBUG_ASSERT_POINTER( ctx );

	return ctx->gb_ctx;
}


/* Program specific parts */

/**
 * @brief creates a new backend-specific program rendering state
 * @param prs the GLES program rendering state to create the backend specific part from
 * @return NULL on error, otherwise a valid object
 */
struct gles_gb_program_rendering_state* _gles_gb_alloc_program_rendering_state( struct gles_program_rendering_state* prs );

/**
 * @brief frees a backend-specific program rendering state
 * @param gb_prs the backend to free
 */
void _gles_gb_free_program_rendering_state( struct gles_gb_program_rendering_state* gb_prs );

/* interleaved non-vbo packing statepushing interface */

/**
 * Notify the backend that a vertex attribute pointer has been modified in the state.
 * This function will calculate which attribute pointers are non-vbo interleaved
 * in order to optimize the performance of subsequent drawcalls.
 * @param ctx		The GLES context
 * @param streams	The gles vertex stream table containing all the stream state
 * @param modified	The number of the stream that was modified, triggering this call
 */
void _gles_gb_modify_attribute_stream(struct gles_context * const ctx, struct gles_vertex_attrib_array* streams, u32 modified);


/* OPTIONAL INTERFACE */

/**
 * Store scissor parameters parameters in the geometry context. Only updates parameters if scissor state
 * is dirty. Uses frame size if scissor is not enabled. Clamps the scissor box to the screen size and scales
 * the scissor box in case we have AA enabled
 * This routine mainly copies state parameters from context to gb_context.
 * @param ctx - the gl context
 * @param frame_builder - the frame builder to get screen dimensions from
 * @param scissor_boundaries [out] - the return value of this function: an array for storing scissor's left, right, bottom, top values
 * @param scissor_closed - if 0, it scissors away everything and makes no sense to draw anything 
 */
void _gles_gb_extract_scissor_parameters(
                struct gles_context *ctx,
                struct mali_frame_builder *frame_builder,
                mali_bool intersect_viewport,
                u32 *scissor_boundaries,
                mali_bool *scissor_closed);

/*
* @param ctx - the gl context
* @param frame_builder - the frame builder to get screen dimensions from
* @param viewport_dimensions [out] - the return value of this function: an array for storing viewport's left, right, bottom, top values
*/
void _gles_gb_extract_viewport_dimensions(
        gles_context *ctx,      mali_frame_builder  *frame_builder,
        s32 *viewport_boundaries );


#endif /* _GLES_GEOMETRY_BACKEND_H_ */
