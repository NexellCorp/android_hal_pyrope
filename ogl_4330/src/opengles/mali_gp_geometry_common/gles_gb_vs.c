/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include <base/mali_debug.h>
#include <base/mali_memory.h>
#include <base/gp/mali_gp_job.h>

#include <regs/MALIGP2/mali_gp_vs_config.h>
#include <shared/binary_shader/bs_symbol.h>

#include <gles_util.h>

#include "gles_geometry_backend_context.h"
#include "gles_gb_vs_attribs.h"
#include "gles_gb_vs_varyings.h"
#include "gles_instrumented.h"
#include "gles_gb_rendering_state.h"
#include "gles_state.h"
#include "gles_framebuffer_object.h"

#include "gles_geometry_backend.h"
#include "shared/m200_gp_frame_builder_inlines.h"

#include "gles_sw_counters.h"
#include <shared/m200_projob.h>

mali_err_code MALI_CHECK_RESULT _gles_gb_vs_arrays_semaphore_begin( gles_gb_context *gb_ctx )
{
    mali_gp_job_handle gp_job;
    mali_err_code err;

    MALI_DEBUG_ASSERT_POINTER( gb_ctx );
    MALI_DEBUG_ASSERT_POINTER( gb_ctx->frame_builder );
    MALI_DEBUG_ASSERT( MALI_FALSE==gb_ctx->parameters.indexed, ("Invalid draw mode" ) );

    gp_job = _mali_frame_builder_get_gp_job( gb_ctx->frame_builder );
    MALI_DEBUG_ASSERT_POINTER( gp_job );

    err = _mali_gp_job_add_vs_cmd( gp_job, GP_VS_COMMAND_WAIT_INC_SEM( MALI_GP2_VS_SEMAPHORE_NO_TIMEOUT, 2, 0 ) );
    if (MALI_ERR_NO_ERROR != err) return err;
    err = _mali_gp_job_add_vs_cmd( gp_job, GP_VS_COMMAND_WAIT_INC_SEM( 1, 0, 0 ) );
    if (MALI_ERR_NO_ERROR != err) return err;

    return MALI_ERR_NO_ERROR;
}

mali_err_code MALI_CHECK_RESULT _gles_gb_vs_arrays_semaphore_end( gles_gb_context *gb_ctx )
{
    mali_gp_job_handle gp_job;
    mali_err_code err;

    MALI_DEBUG_ASSERT_POINTER( gb_ctx );
    MALI_DEBUG_ASSERT_POINTER( gb_ctx->frame_builder );
    MALI_DEBUG_ASSERT( MALI_FALSE==gb_ctx->parameters.indexed, ("Invalid draw mode" ) );

    gp_job = _mali_frame_builder_get_gp_job( gb_ctx->frame_builder );
    MALI_DEBUG_ASSERT_POINTER( gp_job );

    err = _mali_gp_job_add_vs_cmd( gp_job, GP_VS_COMMAND_WAIT_INC_SEM(  0, 0, 0 ) );
    if (MALI_ERR_NO_ERROR != err) return err;

    MALI_SUCCESS;
}

/*  loading the constant registers */
MALI_STATIC_INLINE u32  _gles_gb_setup_vs_config_normal_uniforms(gles_gb_context *gb_ctx, u64* cmds )
{
    u32 num_cmnds=0;
    MALI_DEBUG_ASSERT_POINTER( gb_ctx );
    MALI_DEBUG_ASSERT( ( (gb_ctx->parameters.indexed == 0) || (gb_ctx->parameters.indexed == 1) ),
            ("mali_bool must be 0 or 1, value is %d\n", gb_ctx->parameters.indexed) );


    /* load constant registers */
    /*  gb_ctx->const_reg_addr is always 0 for gles2, only gb_ctx->uniforms_reg_addr is in use*/
    if ( gb_ctx->const_reg_addr != 0 )
    {
        /* Each value consists of 4 values in FP32 floating-point format */

        MALI_DEBUG_ASSERT_ALIGNMENT(gb_ctx->const_reg_addr, 16);
        MALI_DEBUG_ASSERT_ALIGNMENT(gb_ctx->uniforms_reg_addr, 16);
        MALI_DEBUG_ASSERT((gb_ctx->uniform_regs_to_load & 3) == 0, ("uneven uniform_regs_to_load: %d",  gb_ctx->uniform_regs_to_load));
        MALI_DEBUG_ASSERT((gb_ctx->uniform_regs_to_load>> 2) < (1 << 12), ("trying to load more regs than allowed to encode in command: %d", gb_ctx->uniform_regs_to_load));


        cmds[num_cmnds++] = GP_VS_COMMAND_LOAD_CONST_REGS(gb_ctx->const_reg_addr,  0, gb_ctx->const_regs_to_load>> 2);
        cmds[num_cmnds++] = GP_VS_COMMAND_LOAD_CONST_REGS(gb_ctx->uniforms_reg_addr,  gb_ctx->const_regs_to_load >> 2, gb_ctx->uniform_regs_to_load >> 2);

    }
    else if (gb_ctx->uniforms_reg_addr != 0)
    {

        MALI_DEBUG_ASSERT_ALIGNMENT(gb_ctx->uniforms_reg_addr, 16);
        MALI_DEBUG_ASSERT((gb_ctx->uniform_regs_to_load & 3) == 0, ("uneven uniform_regs_to_load: %d",  gb_ctx->uniforms_reg_addr));
        MALI_DEBUG_ASSERT((gb_ctx->uniform_regs_to_load>> 2) < (1 << 12), ("trying to load more regs than allowed to encode in command: %d", gb_ctx->uniforms_reg_addr));

        cmds[num_cmnds++] = GP_VS_COMMAND_LOAD_CONST_REGS(gb_ctx->uniforms_reg_addr,  0, gb_ctx->uniform_regs_to_load >> 2);

    }

    return num_cmnds;

}


/**
 * Setup vertex shader configuration registers.
 * Sets up operation mode, program parameters, input prefetch.
 * Also triggers the shade vertices command, flushes the output, increment the semaphore if indexed mode
 */
MALI_STATIC_INLINE u32 _gles_gb_setup_vs_config_registers_common( gles_gb_context *gb_ctx, u64* cmds, mali_addr streams_mem_in, mali_addr streams_mem_out, u32 stream_size )
{
    u32  vs_range_count = gb_ctx->parameters.indexed ? gb_ctx->parameters.vs_range_count : 1;
    u32 num_cmds = 0;

    MALI_DEBUG_ASSERT_POINTER( cmds );
    MALI_DEBUG_ASSERT_POINTER( gb_ctx );
    MALI_DEBUG_ASSERT_POINTER( gb_ctx->prs );
    MALI_DEBUG_ASSERT( ( (gb_ctx->parameters.indexed == 0) || (gb_ctx->parameters.indexed == 1) ),
            ("mali_bool must be 0 or 1, value is %d\n", gb_ctx->parameters.indexed) );

    /* configure input & output streams  */
    {
        u32 range_index;
        u32 num_input_streams = 0;
        const gles_gb_program_rendering_state* gb_prs = gb_ctx->prs->gb_prs;

        MALI_DEBUG_ASSERT_POINTER( gb_prs );
        /* set the desired input prefetch, set to maximum for now. sending value 3 allows for 4 cache lines and 16 32 bit words burst length */
#if HARDWARE_ISSUE_4326
        cmds[num_cmds++] = GP_VS_COMMAND_WRITE_CONF_REG( 0, GP_VS_CONF_REG_PREFETCH );
#else
        cmds[num_cmds++] = GP_VS_COMMAND_WRITE_CONF_REG( 3, GP_VS_CONF_REG_PREFETCH );
#endif

        num_input_streams = gb_prs->num_input_streams;
#if HARDWARE_ISSUE_8733
        num_input_streams++;
#endif

        for( range_index = 0; range_index < vs_range_count; range_index++ )
        {
            cmds[num_cmds++] = GP_VS_COMMAND_WRITE_INPUT_OUTPUT_CONF_REGS(streams_mem_in + range_index * stream_size, 0, num_input_streams);
            MALI_DEBUG_ASSERT(!((streams_mem_in + range_index * stream_size)%16), ("The address should be 16-byte-aligned"));

            cmds[num_cmds++] = GP_VS_COMMAND_WRITE_INPUT_OUTPUT_CONF_REGS(streams_mem_out + range_index * stream_size, 1, gb_prs->num_output_streams);
            MALI_DEBUG_ASSERT(!((streams_mem_out + range_index * stream_size)%16), ("The address should be 16-byte-aligned"));

            /* shade vertices : operation mode, number_of_verts */
            cmds[num_cmds++] = GP_VS_COMMAND_SHADE_VERTICES( gb_ctx->parameters.indexed, gb_ctx->parameters.indexed ? 
                    (u32)((gb_ctx->parameters.idx_vs_range[range_index].max - gb_ctx->parameters.idx_vs_range[range_index].min + 1)):
                    gb_ctx->parameters.vertex_count );

            /* flush the output */
#if HARDWARE_ISSUE_7320
            if ( gb_ctx->parameters.indexed )
            {
                cmds[num_cmds++] = GP_VS_COMMAND_LIST_CALL(_mali_frame_builder_get_flush_subroutine(gb_ctx->frame_builder));
            }
            else 
            {
                cmds[num_cmds++] = GP_VS_COMMAND_FLUSH_WRITEBACK_BUF();
            }
#else
            cmds[num_cmds++] = GP_VS_COMMAND_FLUSH_WRITEBACK_BUF();
#endif
        }
    }

    /* Increment semaphore but do not wait, so wait value is set to max. */
    if ( gb_ctx->parameters.indexed )
    {
        cmds[num_cmds++] = GP_VS_COMMAND_WAIT_INC_SEM(  MALI_GP2_VS_SEMAPHORE_NO_TIMEOUT, 1, 0 );
    }

    MALI_DEBUG_ASSERT( num_cmds <= (3 + vs_range_count * 4), ("Buffer overrun in _gles_gb_setup_vs_config_registers_common") );

    return num_cmds;
}

/**
 * Setup the depthrange constant register
 * @param const_reg_src The constant register to map values to
 * @param depthrange The array containing the depthrange state
 * @param depthrange_uniform_location_near The location of the near depthrange uniform
 * @param depthrange_uniform_location_far The location of the far depthrange uniform
 * @param depthrange_uniform_location_diff The location of the diff depthrange uniform
 */
MALI_STATIC_INLINE void _gles_gb_setup_depthrange_constants(
        float *const_reg_src,
        float *depthrange,
        s32 depthrange_uniform_location_near,
        s32 depthrange_uniform_location_far,
        s32 depthrange_uniform_location_diff)
{
    /* setup the depthrange mapping constants */
    if( depthrange_uniform_location_near != -1 )
    {
        const_reg_src[ depthrange_uniform_location_near ] = depthrange[0];
    }
    if( depthrange_uniform_location_far != -1 )
    {
        const_reg_src[ depthrange_uniform_location_far ] = depthrange[1];
    }
    if( depthrange_uniform_location_diff != -1 )
    {
        const_reg_src[ depthrange_uniform_location_diff ] = depthrange[1] - depthrange[0];
    }
}



MALI_STATIC_INLINE void  update_sw_counters( gles_context *ctx,  u32 regs_to_load )
{

    MALI_DEBUG_ASSERT_POINTER( ctx );

#if MALI_SW_COUNTERS_ENABLED
    {
        mali_sw_counters *counters = _gles_get_sw_counters(ctx);

        MALI_DEBUG_ASSERT_POINTER( counters );

        MALI_PROFILING_INCREMENT_COUNTER(counters, GLES, UNIFORM_BYTES_COPIED_TO_VR, regs_to_load * sizeof( float ));
    }
#endif /* MALI_SW_COUNTERS_ENABLED */

    _profiling_count(INST_GL_UNIFORM_BYTES_COPIED_TO_MALI, regs_to_load * sizeof( float ) );
}


/* allocate memory for const registers block and fill it with the data*/
#define ALLOC_CHECK_AND_COPY_MEM(frame_pool, ptr, src, elements_to_load, const_reg_addr) \
{ \
    ptr = _mali_mem_pool_alloc(frame_pool, elements_to_load , &(const_reg_addr)); \
    MALI_CHECK_NON_NULL( ptr, MALI_ERR_OUT_OF_MEMORY ); \
    _mali_sys_memcpy_cpu_to_mali_runtime_32( ptr, src , elements_to_load , sizeof(float) ); \
}

/**
 * Allocates the mali memory for needed constant register part
 * @param ctx A pointer to the GLES context
 * @param regs_to_load A full number of registers to load
 */
MALI_STATIC_INLINE mali_err_code  const_reg_memalloc( gles_context *ctx,  u32 regs_to_load )
{
    mali_base_frame_id frameid;
    gles_gb_context *gb_ctx;
    float *const_reg_src;
    gles_program_rendering_state* prs;
    void* const_reg_cpu_ptr;          

    MALI_DEBUG_ASSERT_POINTER( ctx );
    MALI_DEBUG_ASSERT( ( regs_to_load > 0 ),  ("regs_to_load must be > 0 \n") );

    gb_ctx = _gles_gb_get_draw_context( ctx );
    MALI_DEBUG_ASSERT_POINTER( gb_ctx );

    prs = gb_ctx->prs;
    MALI_DEBUG_ASSERT_POINTER( prs );

    const_reg_src = prs->binary.vertex_shader_uniforms.array;

    if (ctx->api_version == GLES_API_VERSION_2)
    {
        ALLOC_CHECK_AND_COPY_MEM (gb_ctx->frame_pool, const_reg_cpu_ptr, const_reg_src, (regs_to_load)*sizeof (float), gb_ctx->uniforms_reg_addr);
        gb_ctx->uniform_regs_to_load =  regs_to_load;
        update_sw_counters(ctx, regs_to_load);
        MALI_SUCCESS;
    }

    frameid = _mali_frame_builder_get_current_frame_id( ctx->state.common.framebuffer.current_object->draw_frame_builder );

    /* if frame has been changed  the memory is likely unmapped, need to re-alloc it fully */

    if (  gb_ctx->vs_uniform_table_last_frame_id != frameid  )
    {
        ALLOC_CHECK_AND_COPY_MEM (gb_ctx->frame_pool, const_reg_cpu_ptr, const_reg_src, (gb_ctx->const_regs_to_load)*sizeof(float), gb_ctx->const_reg_addr);

        gb_ctx->vs_uniform_table_last_frame_id = frameid;
    }

    /* allocate uniforms memory */
    ALLOC_CHECK_AND_COPY_MEM (gb_ctx->frame_pool, const_reg_cpu_ptr, const_reg_src + gb_ctx->const_regs_to_load,
            (regs_to_load - gb_ctx->const_regs_to_load) * sizeof(float), gb_ctx->uniforms_reg_addr);

    update_sw_counters(ctx, regs_to_load);

    MALI_SUCCESS;
}


/**
 * Allocates the constant register and writes the viewport, depthrange and point size scale
 * to the register. Finally it will load the register to MaliGP2
 * @param ctx A pointer to the GLES context
 */
MALI_CHECK_RESULT mali_err_code _gles_gb_setup_vs_constant_registers( gles_context *ctx)
{
    s32                     const_reg_length;
    u32                     regs_to_load = 0;

    gles_gb_context *gb_ctx;
    gles_program_rendering_state* prs;

    MALI_DEBUG_ASSERT_POINTER( ctx );

    gb_ctx = _gles_gb_get_draw_context( ctx );
    MALI_DEBUG_ASSERT_POINTER( gb_ctx );

    prs = gb_ctx->prs;
    MALI_DEBUG_ASSERT_POINTER( prs );

    MALI_DEBUG_ASSERT( ( (gb_ctx->parameters.indexed == 0) || (gb_ctx->parameters.indexed == 1) ),
            ("mali_bool must be 0 or 1\n") );

    /* allocate GP accessible memory (linked to the VS-job) and copy values into it */
    const_reg_length = prs->binary.vertex_shader_uniforms.cellsize;


    if( const_reg_length > 0)
    {
        mali_base_frame_id frameid;
        s32 pointsize_param_offset = prs->pointsize_parameters_uniform_vs_location;
        mali_bool triangle_draw_type = mali_statebit_get( & ctx->state.common, MALI_STATE_MODE_TRIANGLE_TYPE );
        struct gles_common_state * const state = &ctx->state.common;
        const u32	dirty_bits = mali_statebit_raw_get( state,	(1 << MALI_STATE_VS_PRS_UPDATE_PENDING ) |
                (1 << MALI_STATE_VS_VIEWPORT_UPDATE_PENDING ) |
                (1 << MALI_STATE_VIEWPORT_SCISSOR_UPDATE_PENDING ) |
                (1 << MALI_STATE_VS_DEPTH_RANGE_UPDATE_PENDING ) |
                (1 << MALI_STATE_CONST_REG_REALLOCONLY_UPDATE_PENDING) |
                (1 << MALI_STATE_VS_VERTEX_UNIFORM_UPDATE_PENDING ) |
                (1 << MALI_STATE_VS_POLYGON_OFFSET_UPDATE_PENDING ) );

        MALI_DEBUG_ASSERT_POINTER(ctx);
        MALI_DEBUG_ASSERT_POINTER(ctx->state.common.framebuffer.current_object);
        MALI_DEBUG_ASSERT_POINTER(ctx->state.common.framebuffer.current_object->draw_frame_builder);

        frameid = _mali_frame_builder_get_current_frame_id( ctx->state.common.framebuffer.current_object->draw_frame_builder );
        regs_to_load               = (const_reg_length + 3) & ~3; /* round up to the next multiple of 4 */

        if (gb_ctx->vs_uniform_table_last_frame_id == frameid 
                && ( 0 == dirty_bits ) 
                && ( triangle_draw_type || pointsize_param_offset == -1 )) 
        {
            MALI_SUCCESS;
        }

        {
            float *const_reg_src       = prs->binary.vertex_shader_uniforms.array;
            mali_bool realloc_only =  ( dirty_bits & (~(1 << MALI_STATE_CONST_REG_REALLOCONLY_UPDATE_PENDING)) )  == 0;

            if (  ! (realloc_only && triangle_draw_type) )
            {

                if( 0 != ( dirty_bits & ~(1 << MALI_STATE_VS_VERTEX_UNIFORM_UPDATE_PENDING) ) )
                {
                    s32 vp_offset              = prs->viewport_uniform_vs_location;
                    MALI_DEBUG_ASSERT( ((0 <= vp_offset) && (const_reg_length >= vp_offset+(4*2))), ("no room for viewport constants\n"));

                    _gles_gb_calculate_vs_viewport_transform( ctx, & const_reg_src[ vp_offset ] );
                    mali_statebit_unset( state, MALI_STATE_VS_POLYGON_OFFSET_UPDATE_PENDING );
                }

                MALI_DEBUG_ASSERT_RANGE(prs->depthrange_uniform_vs_location_near, -1, const_reg_length - 1);
                MALI_DEBUG_ASSERT_RANGE(prs->depthrange_uniform_vs_location_far,  -1, const_reg_length - 1);
                MALI_DEBUG_ASSERT_RANGE(prs->depthrange_uniform_vs_location_diff, -1, const_reg_length - 1);

                if( 0 != ( dirty_bits & ((1 << MALI_STATE_VS_PRS_UPDATE_PENDING ) | (1 << MALI_STATE_VS_DEPTH_RANGE_UPDATE_PENDING )) ) )
                {
                    _gles_gb_setup_depthrange_constants(
                            const_reg_src,
                            ctx->state.common.viewport.z_nearfar,
                            prs->depthrange_uniform_vs_location_near,
                            prs->depthrange_uniform_vs_location_far,
                            prs->depthrange_uniform_vs_location_diff);

                    mali_statebit_unset( state, MALI_STATE_VS_DEPTH_RANGE_UPDATE_PENDING );
                    gb_ctx->const_regs_to_load  = prs->const_regs_to_load;
                    gb_ctx->uniform_regs_to_load =  ( regs_to_load - gb_ctx->const_regs_to_load );
                }

                if( pointsize_param_offset != -1 )
                {
                    MALI_DEBUG_ASSERT_RANGE(pointsize_param_offset, 0, const_reg_length - 3);

                    const_reg_src[ pointsize_param_offset + 0 ] = gb_ctx->parameters.pointsize_min;
                    const_reg_src[ pointsize_param_offset + 1 ] = gb_ctx->parameters.pointsize_max;
                    const_reg_src[ pointsize_param_offset + 2 ] = gb_ctx->parameters.pointsize_scale;
                }
            }

            {
                mali_err_code err =  const_reg_memalloc(ctx, regs_to_load );
                if (err != MALI_ERR_NO_ERROR)
                    MALI_ERROR(err);
            }

            mali_statebit_unset( state, MALI_STATE_VS_VERTEX_UNIFORM_UPDATE_PENDING );
            mali_statebit_unset( state, MALI_STATE_CONST_REG_REALLOCONLY_UPDATE_PENDING );
        }
    }
    else
    {
        gb_ctx->const_reg_addr = 0;
        gb_ctx->uniforms_reg_addr= 0;
    }

    MALI_SUCCESS;
}

MALI_STATIC_INLINE void _gles_gb_vs_get_stream_offset( const gles_gb_context *gb_ctx, u32 *streams, u32 stream_size, u32 *stride_array)
{
    int 			i;
    u32 			start_index;
    u32 			vs_range_count_m1;
    index_range* 	vs_range;

    /*Some compilers will do extra job if the parameters are directly used*/
    const u32* local_stride_array 	= stride_array;
    u32* const local_streams 		= streams;
    const u32 stream_size_words 	= stream_size/sizeof(u32);
    const u32 stream_count 			= stream_size/(sizeof(u32)*2);

    MALI_DEBUG_ASSERT_POINTER( gb_ctx );
    MALI_DEBUG_ASSERT_POINTER( local_streams );
    MALI_DEBUG_ASSERT_POINTER( local_stride_array );

    /* local storage for all info used in the loops */
    start_index 		= gb_ctx->parameters.start_index;
    vs_range_count_m1 	= gb_ctx->parameters.vs_range_count - 1;
    vs_range 			= gb_ctx->parameters.idx_vs_range;

    /* We do each stream individually because we are reading the specifier and offset from Mali memory which is uncached on the CPU */
    for( i = stream_count - 1; i >= 0; i--)
    {
        int range_index;

        /* Read the first offsets and specifier from uncached memory */
        u32 final_offset 	= local_streams[ 2 * i ];
        u32 final_specifier = local_streams[ 2 * i + 1];
        u32 stride 			= local_stride_array[i];

        /* the address of the last range in the stream setup */
        u32 *stream_offset = local_streams + stream_size_words * vs_range_count_m1;

        /* and the correct stream by two u32s */
        stream_offset += 2 * i;

        /* All ranges from the last to the first */
        for( range_index = vs_range_count_m1; range_index >= 0; range_index-- )
        {
            u32 vtx_offset = vs_range[range_index].min - start_index;
            if(vtx_offset != 0)
            {
                stream_offset[1] = final_specifier;
                stream_offset[0] = final_offset + stride * vtx_offset;
            }
            /* jump pointer to the previous range */
            stream_offset -= stream_size_words;
        }
    }
}

/* This is used to store the strides from input and output streams = 32 */
#define MAX_STRIDE_ARRAY_SIZE   (GP_INPUT_STREAM_COUNT + GP_OUTPUT_STREAM_COUNT)
MALI_CHECK_RESULT mali_err_code _gles_gb_vs_setup( gles_context * const ctx )
{
    mali_addr streams_mem_in;
    mali_addr streams_mem_out;
    u32 stream_size;
    u32 num_input_streams;
    u32 num_output_streams;

    u32 stride_array[MAX_STRIDE_ARRAY_SIZE];

    gles_gb_context *gb_ctx;

    MALI_DEBUG_ASSERT_POINTER( ctx );

    gb_ctx = _gles_gb_get_draw_context( ctx );

    MALI_DEBUG_ASSERT_POINTER( gb_ctx );
    MALI_DEBUG_ASSERT_POINTER( gb_ctx->frame_builder );
    MALI_DEBUG_ASSERT_POINTER( gb_ctx->prs );
    MALI_DEBUG_ASSERT_POINTER( gb_ctx->prs->gb_prs );
    MALI_DEBUG_ASSERT( gb_ctx->frame_pool == _mali_frame_builder_frame_pool_get( _gles_get_current_draw_frame_builder( ctx ) ),
            ("Wrong Frame_pool!\n") );

    MALI_CHECK_NO_ERROR( _gles_gb_setup_vs_constant_registers( ctx ) );

#if HARDWARE_ISSUE_4126

    MALI_CHECK_NO_ERROR( _mali_frame_builder_workaround_for_bug_4126( gb_ctx->frame_builder, gb_ctx->prs->binary.vertex_shader_flags.instruction_last_touching_input ) );

#endif

    {
        mali_gp_job_handle gp_job;

        u32 *streams_in;
        u32 *streams_out;
        u64* cmd_buf;
        u32  num_cmnds = 0, setup_cmnds; 
        u16 i;
        u32  vs_range_count = gb_ctx->parameters.indexed ? gb_ctx->parameters.vs_range_count : 1;

        /* num commands for vs setup*/
        setup_cmnds = gb_ctx->prs->gb_prs->num_vs_setup_cmds;

        gp_job = _mali_frame_builder_get_gp_job( gb_ctx->frame_builder );

        cmd_buf  =_mali_gp_job_vs_cmds_reserve( gp_job, setup_cmnds + 3 + 1 + vs_range_count * 4 + 8*gb_ctx->prs->binary.num_vertex_projob_passes);

        /* reserve space in the vs command stream. */
        if( NULL == cmd_buf )
        {
            return MALI_ERR_OUT_OF_MEMORY;
        }

        num_cmnds += _gles_gb_setup_vs_config_normal_uniforms(gb_ctx, cmd_buf);

        /*   setup pilot gp jobs */
        for ( i=0; i < gb_ctx->prs->binary.num_vertex_projob_passes; i++ )
        {
            u32 projob_cmnds  = _mali_projob_add_gp_drawcall(gb_ctx->frame_builder, cmd_buf + num_cmnds,  &gb_ctx->prs->binary.vertex_projob_passes[i]);
            if (projob_cmnds == 0)
            {
                return MALI_ERR_OUT_OF_MEMORY;
            }
            num_cmnds += projob_cmnds;
        }

        num_input_streams = gb_ctx->prs->gb_prs->num_input_streams;
        num_output_streams = gb_ctx->prs->gb_prs->num_output_streams;
#if HARDWARE_ISSUE_8733
        num_input_streams++;
#endif
#if HARDWARE_ISSUE_4326
        num_input_streams *= 2;
#endif

        /*Doing this for making the stream address 16-byte-aligned*/
        num_input_streams = ((num_input_streams + 1)/2) * 2;
        num_output_streams = ((num_output_streams + 1)/2) * 2;

        /* (num_input_streams output + num_output_streams input streams)*( 1 specifier + 1 address
         * register for each stream)*( each register 4 byte wide  ), See
         * Section 3.8 in TRM
         */ 
        stream_size = (num_input_streams + num_output_streams) * 8;

        /* need a buffer for the stream configurations */
        streams_in = _mali_mem_pool_alloc(gb_ctx->frame_pool, stream_size* vs_range_count, &streams_mem_in);
        if (NULL == streams_in)	return MALI_ERR_OUT_OF_MEMORY;

        streams_out = streams_in + num_input_streams * 2;
        streams_mem_out = streams_mem_in + num_input_streams * 8;

        _mali_sys_memset(stride_array, 0,stream_size/2); 
        MALI_CHECK_NO_ERROR( _gles_gb_setup_input_streams(ctx, streams_in, stride_array) );
        MALI_CHECK_NO_ERROR( _gles_gb_setup_output_streams(gb_ctx, streams_out, stride_array + num_input_streams) );

#if HARDWARE_ISSUE_8733

        num_cmnds += _gles_gb_assure_aligned_last_stream( gp_job, gb_ctx->prs->gb_prs, streams_in );

#endif /* HARDWARE_ISSUE_8733 */

        if( gb_ctx->parameters.indexed && (gb_ctx->parameters.idx_vs_range[0].min != gb_ctx->parameters.start_index || vs_range_count > 1))
        {
            _gles_gb_vs_get_stream_offset(gb_ctx, streams_in, stream_size, stride_array );
        }

        /* apply all gb program rendering states
         * NOTE the input streams must be configured before entering this section.
         */
        {
            /* this copies the vs setup commands and then writes the config registers directly to the cmnd list */
            u64* dst = cmd_buf + num_cmnds;
            u64* src  = gb_ctx->prs->gb_prs->vs_setup_cmds;	

            u32 knt = setup_cmnds;
            while (knt--)
            {
                *dst++ = *src++;
            }


            /* dst is passed into config_registers_common because it is the write location in the cmnd stream */
            num_cmnds += setup_cmnds;
            num_cmnds += _gles_gb_setup_vs_config_registers_common( gb_ctx, dst, streams_mem_in, streams_mem_out, stream_size );
        }

        /* confirm reserved amount */
        _mali_gp_job_vs_cmds_confirm( gp_job, num_cmnds );
    }
    MALI_SUCCESS;
}
