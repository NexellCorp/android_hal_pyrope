/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_geometry_backend.h"

#include "mali_system.h"

#include "base/mali_debug.h"
#include "base/mali_memory.h"

#include "regs/MALIGP2/mali_gp_vs_config.h" /* needed for stream-config */
#include "regs/MALIGP2/mali_gp_plbu_config.h" /* needed for stream-config */
#include "shared/binary_shader/bs_object.h"
#include "shared/binary_shader/bs_symbol.h"

#include "../gles_config.h"
#include "../gles_context.h"
#include "../gles_util.h"

#include "../m200_backend/gles_fb_context.h"
#include "gles_geometry_backend_context.h"
#include "../gles_gb_interface.h"
#include "gles_instrumented.h"
#include "../gles_draw.h"
#include "gles_framebuffer_object.h"
#include "shared/m200_gp_frame_builder_inlines.h"

#include "gles_gb_bounding_box.h"


#define GLES_GB_BB_REJECT_THRESHOLD 700 /* min number of vertices in a drawcall to reject it */


MALI_STATIC mali_err_code _gles_gb_mali_mem_alloc_position_varyings(struct gles_context *ctx, int vertex_count)
{
    gles_fb_context *fb_ctx;
    bs_program *program_bs;

    /* fetch state */
    MALI_DEBUG_ASSERT_POINTER( ctx );
    fb_ctx = ctx->fb_ctx;
    MALI_DEBUG_ASSERT_POINTER( fb_ctx );

    MALI_DEBUG_ASSERT_POINTER( ctx->state.common.current_program_rendering_state );
    program_bs = &(ctx->state.common.current_program_rendering_state->binary);
    MALI_DEBUG_ASSERT_POINTER( program_bs );

    MALI_DEBUG_ASSERT( ctx->frame_pool == _mali_frame_builder_frame_pool_get( _gles_get_current_draw_frame_builder( ctx ) ),
            ("Wrong Frame_pool!\n") );

    /* make sure we have enough vertex memory */
    {
        /* calculate sizes */
        u32 position_buffer_size = vertex_count * sizeof(float) * 4;  /* position is a 4 component 32bit float stream */
        u32 varyings_buffer_size = vertex_count * program_bs->varying_streams.block_stride;

        if ( NULL == _mali_mem_pool_alloc(ctx->frame_pool, position_buffer_size + varyings_buffer_size, &fb_ctx->position_addr) ) return MALI_ERR_OUT_OF_MEMORY;
        fb_ctx->varyings_addr = fb_ctx->position_addr + position_buffer_size;
    }

    /* update varyings-pointer */
    {
        m200_rsw *rsw = &ctx->rsw_raster->mirror;
        u32 varyings_block_size = program_bs->varying_streams.block_stride / 8;

        /* If there's no varyings, simply encode a NULL-pointer */
        u32 varyings_base_address = 0;
        if( 0 != varyings_block_size ) varyings_base_address = fb_ctx->varyings_addr;

        __m200_rsw_encode( rsw, M200_RSW_VARYINGS_BASE_ADDRESS, varyings_base_address );

        /* commit the new rsw */
        {
            u32 count = sizeof(m200_rsw) / sizeof(u32);
            u32* rsw_ptr_mirror = (u32*)rsw;

            m200_rsw* rsw_ptr = _mali_mem_pool_alloc(ctx->frame_pool, sizeof(m200_rsw), &fb_ctx->rsw_addr);
            u32* rsw_ptr_local = (u32*)rsw_ptr;

            if (NULL == rsw_ptr) return MALI_ERR_OUT_OF_MEMORY;

            /* copy the composed rsw to the gp memory */
            while(count--)
            {
                *rsw_ptr_local++ = *rsw_ptr_mirror++;
            }
        }

        /* set the new vertex array base address and rsw address in the gb context*/
        {
            gles_gb_context *gb_ctx = _gles_gb_get_draw_context( ctx );

            MALI_DEBUG_ASSERT_POINTER( gb_ctx );

            gb_ctx->parameters.vertex_array_base_addr	= fb_ctx->position_addr;
            gb_ctx->parameters.rsw_base_addr			= fb_ctx->rsw_addr;
            gb_ctx->position_addr						= fb_ctx->position_addr;
            gb_ctx->varyings_addr						= fb_ctx->varyings_addr;
        }
    }

    return MALI_ERR_NO_ERROR;
}

void _gles_gb_calculate_vs_viewport_transform( struct gles_context * const ctx, float * const vp )
{
    struct gles_common_state * const state = & ctx->state.common;
    struct gles_viewport * const state_vp = & state->viewport;

    MALI_DEBUG_ASSERT_POINTER( ctx );

    if( mali_statebit_get( state, MALI_STATE_VS_VIEWPORT_UPDATE_PENDING ))
    {
        state_vp->half_width  = (float) state_vp->width  * 0.5f;
        state_vp->half_height = (float) state_vp->height * 0.5f;
        state_vp->offset_x = (float) state_vp->x + state_vp->half_width;
        state_vp->offset_y = (float) state_vp->y + state_vp->half_height;

        /* Since we're going to cache the result of this
         * calculation we'll ignore supersampling.  Supersampling can
         * be applied as needed post-cache.
         */
        state_vp->viewport_transform[0] = state_vp->half_width;
        state_vp->viewport_transform[2] = (state_vp->z_nearfar[1] - state_vp->z_nearfar[0]) * 0.5f;
        state_vp->viewport_transform[3] = 1.0f;
        state_vp->viewport_transform[4] = state_vp->offset_x;
        state_vp->viewport_transform[6] = (state_vp->z_nearfar[1] + state_vp->z_nearfar[0]) * 0.5f;
        state_vp->viewport_transform[7] = 0.0f;

        mali_statebit_unset( state, MALI_STATE_VS_VIEWPORT_UPDATE_PENDING );

        /* we have modified depth range state, raise up depth range flag to recalcualte depthrange also */
        mali_statebit_set( state, MALI_STATE_VS_DEPTH_RANGE_UPDATE_PENDING );

    }

    /* Copy the cached result to the destination before adjusting
     * for multisampling, FBO's and polygon-offset.
     */
    _mali_sys_memcpy_32( vp, &state_vp->viewport_transform[0], sizeof( float ) * 8 );

    /* and then do the adjustments */
    {
        mali_frame_builder * const frame_builder = _gles_get_current_draw_frame_builder( ctx );
        float scale_y = 1.0f;

        if( mali_statebit_get( state, MALI_STATE_MODE_SUPERSAMPLE_16X ) )
        {
            const float scale_x = 2.0f;
            scale_y = 2.0f;

            vp[0] *= scale_x;
            vp[4] *= scale_y;
        }

        if( state->framebuffer.current_object->draw_flip_y )
        {
            /* flip Y when rendering to the default target
             */
            scale_y = -scale_y;

            vp[1] = state_vp->half_height * scale_y;
            vp[5] = (state_vp->offset_y  * scale_y) + _mali_frame_builder_get_height( frame_builder );
        }
        else
        {
            /* to get the orientation of rendertarget textures correct we don't flip when rendering to FBOs
             */
            vp[1] = state_vp->half_height * scale_y;
            vp[5] = state_vp->offset_y * scale_y;
        }
    }

    if( _gles_fb_enables_get( ctx->rsw_raster, M200_POLYGON_OFFSET_FILL ) &&
            mali_statebit_get( & ctx->state.common, MALI_STATE_MODE_TRIANGLE_TYPE ))
    {
        const mali_float scale = 1.0f / ((1 << 24) - 1);

        vp[6] += (_gles_fb_get_polygon_offset_units( ctx )) * scale;
    }
}

/**
 * Store point size override, point size scale and line width parameters in the geometry context.
 * This routine mainly copies state parameters from context to gb_context.
 * @param ctx - the gl context
 * @param gb_ctx - the geometry backend context
 * @param supersample_factor - the supersample downscale factor. Used to check if we need to scale the scissor box
 */
MALI_STATIC void _gles_gb_setup_point_and_line_parameters(
        gles_context *ctx,
        gles_gb_context *gb_ctx)
{
    /* We need the "float tmp" variable, and also must use "float supersample_scalef"
     * rather than "int supersample_scale", to work around a bugzilla in RVCT 2.2.
     */
    float tmp;
    int supersample_scalef = _gles_get_current_draw_supersample_scalefactor(ctx);

    /* Setup point size override, always 1 if in line mode*/
    gb_ctx->parameters.point_size_override = 1;
    gb_ctx->parameters.fixed_point_size = 1.0;

    if( mali_statebit_get( & ctx->state.common, MALI_STATE_MODE_POINT_TYPE ))
    {
        if( gb_ctx->prs->binary.vertex_pass.flags.vertex.pointsize_register != -1)
        {
            gb_ctx->parameters.point_size_override = 0;
        } else {
            gb_ctx->parameters.fixed_point_size = CLAMP( ctx->state.common.rasterization.point_size,
                    ctx->state.common.rasterization.point_size_min,
                    ctx->state.common.rasterization.point_size_max );
        }
    }

    /* setup line width and point size, scale if 16xAA. */
    gb_ctx->parameters.pointsize_scale = supersample_scalef;
    gb_ctx->parameters.pointsize_min   = ctx->state.common.rasterization.point_size_min;
    gb_ctx->parameters.pointsize_max   = ctx->state.common.rasterization.point_size_max;

    /* The assignment to tmp in the following code is needed to work around a bugzilla in RVCT 2.2. */
    tmp = FTYPE_TO_FLOAT(ctx->state.common.rasterization.line_width);
    gb_ctx->parameters.line_width      =
        CLAMP(
                tmp,
                GLES_ALIASED_LINE_WIDTH_MIN,
                GLES_ALIASED_LINE_WIDTH_MAX
             ) * supersample_scalef;
}

/**
 * Copy and Setup all parameters used by the backend to draw.
 * This routine mainly copies state parameters from context to gb_context.
 */
MALI_STATIC_INLINE void _gles_gb_setup_parameters(gles_context *ctx)
{
    gles_gb_context    *gb_ctx;
    mali_frame_builder *frame_builder;
    mali_bool triangle_type;

    MALI_DEBUG_ASSERT_POINTER( ctx );
    MALI_DEBUG_ASSERT_POINTER( ctx->fb_ctx );

    gb_ctx = _gles_gb_get_draw_context( ctx );
    MALI_DEBUG_ASSERT_POINTER( gb_ctx );

    frame_builder = _gles_get_current_draw_frame_builder( ctx );
    MALI_DEBUG_ASSERT_POINTER( frame_builder );

    gb_ctx->frame_builder = frame_builder;

    triangle_type = mali_statebit_get( & ctx->state.common, MALI_STATE_MODE_TRIANGLE_TYPE );

    /* Setup vertex array */
    gb_ctx->vertex_array = &ctx->state.common.vertex_array;

    if( ! triangle_type)
    {
        _gles_gb_setup_point_and_line_parameters(ctx, gb_ctx );
    }

    /* Setup the cull face parameters */
    gb_ctx->parameters.cull_face      = ctx->state.common.rasterization.cull_face;
    gb_ctx->parameters.cull_face_mode = ctx->state.common.rasterization.cull_face_mode;
    gb_ctx->parameters.front_face     = ctx->state.common.rasterization.front_face;

    /* flip front-facing setting when rendering to texture */
    if ( ! ctx->state.common.framebuffer.current_object->draw_flip_y )
    {
        gb_ctx->parameters.front_face = (gb_ctx->parameters.front_face == GL_CCW) ? GL_CW : GL_CCW;
    }

}

#define MAX_PLBU_RANGE_COUNT 256
mali_err_code _gles_gb_draw_indexed_range( gles_context *ctx, 
											u32 mode,
											const index_range *idx_vs_range,
											const index_range *vs_range_buffer,
											u32 vs_range_count,
											u32 count,
											u32 *transformed_count,
											u32 type,
											u32 coherence,
											const void *indices )
{
    mali_err_code err;
    gles_gb_context *gb_ctx;
    u32 range_index;
    plbu_range idx_plbu_range[MAX_PLBU_RANGE_COUNT];

    MALI_DEBUG_ASSERT( count != 0, ("illegal count should be handled earlier\n") );
    MALI_DEBUG_ASSERT_POINTER( ctx );
    MALI_DEBUG_ASSERT_POINTER( ctx->gb_ctx );

    MALI_IGNORE(range_index);

    gb_ctx = _gles_gb_get_draw_context( ctx );
    MALI_DEBUG_ASSERT_POINTER( gb_ctx );
    gb_ctx->frame_pool = ctx->frame_pool;
    gb_ctx->prs = ctx->state.common.current_program_rendering_state;

    /* set the draw parameters for this call */
    gb_ctx->parameters.mode         = mode;
    gb_ctx->parameters.indexed      = MALI_TRUE;
    gb_ctx->parameters.index_type   = type;
    gb_ctx->parameters.indices      = (void *) indices;
    gb_ctx->parameters.idx_vs_range    = (index_range*)idx_vs_range;
    gb_ctx->parameters.vs_range_count  = vs_range_count;
    gb_ctx->parameters.idx_plbu_range  = idx_plbu_range; 
    gb_ctx->parameters.output_buffer_offset = 0;
    gb_ctx->parameters.start_index  = idx_vs_range[0].min;
    gb_ctx->parameters.end_index    = idx_vs_range[vs_range_count - 1].max;
    gb_ctx->parameters.index_count  = count;
    gb_ctx->parameters.vertex_count = gb_ctx->parameters.end_index - gb_ctx->parameters.start_index + 1;
    gb_ctx->parameters.transformed_vertex_count = 0;
    gb_ctx->parameters.plbu_range_count = 1;
    gb_ctx->parameters.idx_plbu_range[0].start = 0;
    gb_ctx->parameters.idx_plbu_range[0].count = gb_ctx->parameters.index_count;
    gb_ctx->parameters.coherence = coherence;

    MALI_DEBUG_ASSERT_POINTER(idx_vs_range);

#if MALI_INSTRUMENTED || MALI_SW_COUNTERS_ENABLED
    for( range_index = 0; range_index < vs_range_count; range_index++ )
    {
        gb_ctx->parameters.transformed_vertex_count += idx_vs_range[range_index].max - idx_vs_range[range_index].min + 1;
    }

    if( NULL != transformed_count )
    {
        *transformed_count = gb_ctx->parameters.transformed_vertex_count;
    }
#endif

    /* make sure we have enough memory for varyings */
    MALI_CHECK_NO_ERROR( _gles_gb_mali_mem_alloc_position_varyings(ctx, gb_ctx->parameters.vertex_count ) );

    _gles_gb_setup_parameters(ctx);

#ifdef MALI_BB_TRANSFORM
/* start criteria for BB, drawing mode is GL_TRIANGLES or we have exceeded the threshold*/
    if (gb_ctx->prs->binary.cpu_pretrans != NULL && 
          gb_ctx->parameters.vertex_count > GLES_GB_BB_REJECT_THRESHOLD && 
          mode == GL_TRIANGLES &&
          type == GL_UNSIGNED_SHORT &&
          ctx->api_version == GLES_API_VERSION_2
          )
    {
        mali_bool bb_has_collision;

        MALI_CHECK_NO_ERROR(_gles_gb_try_reject_drawcall(ctx, vs_range_buffer, &bb_has_collision) );

        if (!bb_has_collision) MALI_SUCCESS; /* bounding box is outside the viewport, don't proceed with dc */
    }
#endif

    /* add vs and plbu jobs  */
    err = _gles_gb_vs_setup( ctx );
    MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == err, reset );

    err = _gles_gb_plbu_setup( ctx );
    MALI_CHECK_GOTO( MALI_ERR_NO_ERROR == err, reset );

    mali_statebit_unset( &ctx->state.common, MALI_STATE_VS_PRS_UPDATE_PENDING );

    MALI_SUCCESS;

reset:
    {
        /* reset the frame to avoid undefined state */
        mali_err_code merr;
        merr = _gles_reset_frame(ctx);
        MALI_DEBUG_ASSERT( (MALI_ERR_NO_ERROR==merr) || (MALI_ERR_OUT_OF_MEMORY==merr), ("Unexpected error code: 0x%08x", merr) );
        MALI_IGNORE(merr);
    }

    return err;
}

mali_err_code _gles_gb_draw_nonindexed_split( gles_context *ctx, u32 mode, s32 first, u32 count)
{

    mali_err_code err;

    int vertex_reuse = 0; /* for strips and loops - the amount of vertices to reuse between splitted batches */
    int vertices_per_primitive = 0;
    int original_first = first;
    int original_count = count;
    int pre_fixup_vertices = 0;
    gles_gb_context    *gb_ctx;

    MALI_DEBUG_ASSERT( count != 0, ("illegal count should be handled earlier\n") );
    MALI_DEBUG_ASSERT_POINTER( ctx );

    gb_ctx = _gles_gb_get_draw_context( ctx );
    MALI_DEBUG_ASSERT_POINTER( gb_ctx );

    gb_ctx->prs = ctx->state.common.current_program_rendering_state;
    MALI_DEBUG_ASSERT_POINTER( gb_ctx->prs );

    /* get configuration */
    switch (mode)
    {
        case GL_POINTS:
            vertices_per_primitive = 1;
            break;

        case GL_LINES:
            vertices_per_primitive = 2;
            break;

        case GL_LINE_LOOP:
            gb_ctx->parameters.mode =  GL_LINE_STRIP; /* use strip-mode for this emulation */
            /* Fall through to the strip-case, as the rest of the setup is identical. */
        case GL_LINE_STRIP:
            vertices_per_primitive = 1;
            vertex_reuse = 1;
            break;

        case GL_TRIANGLES:
            vertices_per_primitive = 3;
            break;

        case GL_TRIANGLE_STRIP:
            vertices_per_primitive = 1;
            vertex_reuse = 2;
            break;

        case GL_TRIANGLE_FAN:
            vertices_per_primitive = 1;
            vertex_reuse = 2;
            pre_fixup_vertices = 1;
            break;
    }

    _gles_gb_setup_parameters(ctx);

    MALI_CHECK_NO_ERROR( _gles_gb_vs_arrays_semaphore_begin(gb_ctx) );
    MALI_CHECK_NO_ERROR( _gles_gb_plbu_arrays_semaphore_begin(gb_ctx) );

    /* while at least one primitive left */
    while (count >= (u32)(vertex_reuse + vertices_per_primitive))
    {
        /* calculate batch_count; the count parameter for this draw-batch */
        int batch_count = MIN(GLES_GB_MAX_VERTICES, count);
        batch_count = _gles_round_down_vertex_count(mode, batch_count);

        MALI_DEBUG_ASSERT(batch_count <= GLES_GB_MAX_VERTICES, ("Invalid batch size: %d", batch_count));

        /* assure that tristrips aren't flipping orientation per split (should not happen as long
           as GLES_GB_MAX_VERTICES is a multiple of two bigger than four) */
        if (mode == GL_TRIANGLE_STRIP)
        {
            MALI_DEBUG_ASSERT(0 == (batch_count - 2) % 2, ("Triangle strip splitter is flipping orientation!"));
        }

        /* make sure we have enough memory for varyings */
        MALI_CHECK_NO_ERROR( _gles_gb_mali_mem_alloc_position_varyings(ctx, batch_count) );

        if (0 < pre_fixup_vertices)
        {
            /* transform the first vertex */
            gb_ctx->parameters.vertex_count = pre_fixup_vertices;
            gb_ctx->parameters.start_index  = original_first;
            gb_ctx->parameters.output_buffer_offset = 0;
            MALI_CHECK_NO_ERROR( _gles_gb_vs_setup(ctx) );
        }

        /* transform "normal" vertices */
        gb_ctx->parameters.vertex_count = batch_count - pre_fixup_vertices;
        gb_ctx->parameters.start_index  = first + pre_fixup_vertices;
        gb_ctx->parameters.output_buffer_offset = pre_fixup_vertices;
        MALI_CHECK_NO_ERROR( _gles_gb_vs_setup(ctx) );

        /* start plbu job */
        gb_ctx->parameters.start_index  = first;
        gb_ctx->parameters.index_count  = batch_count;
        MALI_CHECK_NO_ERROR( _gles_gb_plbu_setup(ctx) );

        /* update first and count for the next batch */
        count -= batch_count - vertex_reuse;
        first += batch_count - vertex_reuse;
    }

    /* close line loops */
    if (GL_LINE_LOOP == mode)
    {
        /* make sure we have enough memory for varyings */
        if (MALI_ERR_NO_ERROR != (err = _gles_gb_mali_mem_alloc_position_varyings(ctx, 2))) return err;

        /* transform the last vertex */
        gb_ctx->parameters.vertex_count = 1;
        gb_ctx->parameters.start_index  = original_first + original_count - 1;
        gb_ctx->parameters.output_buffer_offset = 0;
        MALI_CHECK_NO_ERROR( _gles_gb_vs_setup(ctx) );

        /* transform the first vertex */
        gb_ctx->parameters.vertex_count = 1;
        gb_ctx->parameters.start_index  = original_first;
        gb_ctx->parameters.output_buffer_offset = 1;
        MALI_CHECK_NO_ERROR( _gles_gb_vs_setup(ctx) );

        /* start plbu job */
        gb_ctx->parameters.index_count  = 2;
        gb_ctx->parameters.start_index  = 0; /* irrelevant for the PLBU in DrawArrays mode */
        MALI_CHECK_NO_ERROR( _gles_gb_plbu_setup(ctx) );
    }

    MALI_CHECK_NO_ERROR( _gles_gb_vs_arrays_semaphore_end(gb_ctx) );
    MALI_CHECK_NO_ERROR( _gles_gb_plbu_arrays_semaphore_end(gb_ctx) );
    MALI_SUCCESS;
}

MALI_STATIC_INLINE mali_err_code _gles_gb_draw_nonindexed_setup_command_lists( gles_context * const ctx )
{
    gles_gb_context    *gb_ctx;

    MALI_DEBUG_ASSERT_POINTER( ctx );

    gb_ctx = _gles_gb_get_draw_context( ctx );

    MALI_DEBUG_ASSERT_POINTER( gb_ctx );

    MALI_CHECK_NO_ERROR( _gles_gb_vs_arrays_semaphore_begin(gb_ctx) );
    MALI_CHECK_NO_ERROR( _gles_gb_plbu_arrays_semaphore_begin(gb_ctx) );
    MALI_CHECK_NO_ERROR( _gles_gb_vs_setup(ctx) );
    MALI_CHECK_NO_ERROR( _gles_gb_plbu_setup(ctx) );
    MALI_CHECK_NO_ERROR( _gles_gb_vs_arrays_semaphore_end(gb_ctx) );
    MALI_CHECK_NO_ERROR( _gles_gb_plbu_arrays_semaphore_end(gb_ctx) );

    MALI_SUCCESS;
}

mali_err_code _gles_gb_draw_nonindexed( gles_context *ctx, u32 mode, s32 first, u32 count )
{
    mali_err_code err;

    gles_gb_context    *gb_ctx;

    MALI_DEBUG_ASSERT_POINTER( ctx );
    MALI_DEBUG_ASSERT( count != 0, ("illegal count should be handled earlier\n") );

    gb_ctx = _gles_gb_get_draw_context( ctx );
    MALI_DEBUG_ASSERT_POINTER( gb_ctx );
    gb_ctx->frame_pool = ctx->frame_pool;

    /* set the common draw parameters for this call (no matter if we split or not) */
    gb_ctx->parameters.mode         = mode;
    gb_ctx->parameters.indexed      = MALI_FALSE;
    /* these are irrelevant. setting them for debugging cleanliness */
    gb_ctx->parameters.index_type   = 0x0;
    gb_ctx->parameters.indices      = NULL;
    gb_ctx->parameters.start_index  = first;

    if (count > GLES_GB_MAX_VERTICES)
    {
        /* add warning to the instrumented driver log */
        MALI_INSTRUMENTED_LOG(
                (_instrumented_get_context(),
                 MALI_LOG_LEVEL_WARNING,
                 "Splitting glDrawArray call due to high amount of vertices")
                );

        err = _gles_gb_draw_nonindexed_split(ctx, mode, first, count);
        if (MALI_ERR_NO_ERROR != err)
        {
            /* reset the frame to avoid undefined state */
            mali_err_code merr;
            merr = _gles_reset_frame(ctx);
            MALI_DEBUG_ASSERT( (MALI_ERR_NO_ERROR==merr) || (MALI_ERR_OUT_OF_MEMORY==merr), ("Unexpected error code: 0x%08x", merr) );
            MALI_IGNORE(merr);
            return err;
        }
    }
    else
    {
        /* make sure we have enough memory for varyings */
        MALI_CHECK_NO_ERROR( _gles_gb_mali_mem_alloc_position_varyings(ctx, count) );

        gb_ctx->parameters.vertex_count = count;
        gb_ctx->parameters.start_index  = first;
        gb_ctx->parameters.index_count  = count;
        gb_ctx->parameters.output_buffer_offset = 0;
        gb_ctx->prs = ctx->state.common.current_program_rendering_state;

        _gles_gb_setup_parameters(ctx);

        err = _gles_gb_draw_nonindexed_setup_command_lists(ctx);
        if (MALI_ERR_NO_ERROR != err)
        {
            /* reset the frame to avoid undefined state */
            mali_err_code merr;
            merr = _gles_reset_frame(ctx);
            MALI_DEBUG_ASSERT( (MALI_ERR_NO_ERROR==merr) || (MALI_ERR_OUT_OF_MEMORY==merr), ("Unexpected error code: 0x%08x", merr) );
            MALI_IGNORE(merr);
            return err;
        }
    }

    mali_statebit_unset( &ctx->state.common, MALI_STATE_VS_PRS_UPDATE_PENDING );

    MALI_SUCCESS;
}
