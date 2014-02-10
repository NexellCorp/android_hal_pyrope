/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES_GEOMETRY_BACKEND_CONTEXT_H_
#define _GLES_GEOMETRY_BACKEND_CONTEXT_H_

#include <mali_system.h>

#include <shared/binary_shader/bs_object.h>
#include "../gles_config.h"
#include "../gles_context.h"
#include "../gles_common_state/gles_program_rendering_state.h"
#include "../gles_gb_interface.h"

/* mali_gp_plbu_cmd requires...
 */
#include <regs/MALIGP2/mali_gp_plbu_config.h>

#define MALI_LARGEST_INDEX_RANGE       256 

/*The indices difference between the adjacent two sub index ranges*/
#define GLES_THRESHOLD_TO_START_NEW_INDEX_RANGE   8 

typedef struct gles_gb_draw_parameters
{
    GLenum      mode;
    mali_bool   indexed;
    GLenum      index_type;
    void        *indices;
    index_range *idx_vs_range;          /* the index range used to setup the vs command */
    plbu_range  *idx_plbu_range;        /* the index range used to setup the plbu command */
    u32         index_count;
    u32         vertex_count;           /* vertex count from minimum index to maximum index */
    u32         transformed_vertex_count;     /*vertex count transformed by vertex shader */
    u32         vs_range_count;            /* count of the index ranges */
    u32         plbu_range_count;
    u32         start_index;            /* the minimum index of all the vertices */
    u32         end_index;              /* the maximum index of all the vertices */
    u32         output_buffer_offset;   /* use with extreme care! */
    u32         coherence;               /* the measurement for the coherence of index buffer */
    mali_addr   rsw_base_addr;
    mali_addr   vertex_array_base_addr;
    GLboolean   cull_face;						/**< Is GL_CULL_FACE enabled? */
    GLenum      cull_face_mode;					/**< Specifies which face we are culling */
    GLenum      front_face;						/**< Specifies what is the current front-face */
    float		line_width;
    float 		pointsize_scale;		/* used for setting the pointsize scale uniform used in the vertex shader to scale points when using AA */
    float		pointsize_min;
    float		pointsize_max;
    mali_bool point_size_override;
    float       fixed_point_size; /* point size in effect when no point size stream is set up (point_size_override is true) */

} gles_gb_draw_parameters;

struct gles_gb_job;

typedef struct gles_gb_context
{
	gles_gb_draw_parameters parameters;
	mali_mem_pool *frame_pool;

	enum gles_api_version api_version;

	mali_base_ctx_handle base_ctx;  /**< Holds the base driver context */

	mali_frame_builder   *frame_builder;

	struct gles_program_rendering_state* prs;

	mali_addr       varyings_addr;
	mali_addr       position_addr;
	mali_addr       point_size_addr;

	struct gles_vertex_array *vertex_array;

	/* the following states are used by the attribute stream interleaved non-vbo optimizations
	 * and are modified every time someone modified a VertexPointer in the state. */
	u32 attribute_temp_block_candidates;
	u32 num_attribute_temp_block_candidates;
	
	/* VS uniform reupload workaround tempstorage */
	  /* this is for gles1 only*/
	 mali_addr const_reg_addr;
	 u32 const_regs_to_load;
       /* this is for gles1 and gles2*/
	 mali_addr uniforms_reg_addr;
	 u32 uniform_regs_to_load;
	  	
	mali_base_frame_id vs_uniform_table_last_frame_id;

	/* PLBU scissor / viewport setup */
	mali_base_frame_id plbu_scissor_viewport_last_frame_id;
	mali_bool plbu_prev_call_is_triangle;

} gles_gb_context;

/**
 * Setup VS for a draw call
 * @param ctx - a pointer to the GLES context
 */
MALI_CHECK_RESULT mali_err_code _gles_gb_vs_setup( gles_context * const ctx );

/**
 * Setup PLBU for a draw call
 * @param ctx - a pointer to the GLES context
 */
MALI_CHECK_RESULT mali_err_code _gles_gb_plbu_setup( struct gles_context * const ctx );

/**
 * Sets up the VS semaphore synchronization at the start of arrays mode job.
 * The following steps must be used:
 *  1. _gles_gb_vs_arrays_semaphore_begin
 *  2. _gles_gb_plbu_arrays_semaphore_begin
 *  3. _gles_gb_vs_setup
 *  4. _gles_gb_plbu_setup
 *  5. _gles_gb_vs_arrays_semaphore_end
 *  6. _gles_gb_plbu_arrays_semaphore_end
 * Steps 3-4 can be repeated multiple times, as long as all the jobs are added
 * to the same GP job.
 * (the relative order of 1&2 and 5&6 does not matter)
 *
 * @param gb_ctx - a pointer to the geometry backend context
 * @return an error code
 */
mali_err_code MALI_CHECK_RESULT _gles_gb_vs_arrays_semaphore_begin( gles_gb_context * gb_ctx );

/**
 * Sets up the VS semaphore synchronization at the end of arrays mode job.
 * @see _gles_gb_vs_arrays_semaphore_begin
 * @param gb_ctx - a pointer to the geometry backend context
 * @return an error code
 */
mali_err_code MALI_CHECK_RESULT _gles_gb_vs_arrays_semaphore_end( gles_gb_context * gb_ctx );

/**
 * Sets up the PLBU semaphore synchronization at the start of arrays mode job.
 * @see _gles_gb_vs_arrays_semaphore_begin
 * @param gb_ctx - a pointer to the geometry backend context
 * @return an error code
 */
mali_err_code MALI_CHECK_RESULT _gles_gb_plbu_arrays_semaphore_begin( gles_gb_context * gb_ctx );

/*
 * Sets up the PLBU semaphore synchronization at the end of arrays mode job.
 * @see _gles_gb_vs_arrays_semaphore_begin
 * @param gb_ctx - a pointer to the geometry backend context
 * @return an error code
 */
mali_err_code MALI_CHECK_RESULT _gles_gb_plbu_arrays_semaphore_end( gles_gb_context * gb_ctx );


#endif	/* _GLES_GEOMETRY_BACKEND_CONTEXT_H_ */
