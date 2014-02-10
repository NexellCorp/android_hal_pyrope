/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifdef MALI_BB_TRANSFORM

#include "gles_gb_bounding_box.h"
#include "gles_gb_vertex_rangescan.h"
#include "gles_gb_buffer_object.h"

#include "../gles_buffer_object.h"
#include <gles_state.h>

#include "../gles_draw.h"

typedef enum bb_clip_state
{
    TOTALLY_REJECT,       /*The bounding box is totally outside of the viewport */
    TOTALLY_ACCEPT,       /*The bounding box is totally inside of the viewport */
    PARTIALLY_ACCEPT      /*The bounding box is partially outside of the viewport */
} bb_clip_state;



void _gles_float_matrix4x4_vector3_transform_and_produce_clip_bits( u32* input_params )
{
    MALI_DEBUG_ASSERT_POINTER( input_params );
    {    
        const float * src =  (const float*)input_params[0]; /* source vectors*/
        float* dst = (float*)input_params[2];  /* transformed vectors*/
        const float* mvp = (const float*)(input_params[1]); /* transformation matrix*/
        const float value1 =  _gles_convert_binary_to_fp32(input_params[3]); /* constant to expand the vector to 4x1*/

        u32 i;
        mali_float s0;
        mali_float s1;
        mali_float s2;
        mali_float s3[4];

		u32 mask =0x3F; /* 0x3f is 111111, each one stands for a side of a cube */
		u32 total_vert_mask = 0;
		 
        MALI_DEBUG_ASSERT_POINTER(mvp);
        MALI_DEBUG_ASSERT_POINTER(src);
        MALI_DEBUG_ASSERT_POINTER(dst);

        
        s3[0] = value1 * mvp[0 + 12];
        s3[1] = value1 * mvp[1 + 12];
        s3[2] = value1 * mvp[2 + 12];
        s3[3] = value1 * mvp[3 + 12];
        
	    for (i=0; i < 8; i++)
        {
            u32 vert_mask = 0;
            u32 dst_x = 4*i;
            float w;

            s0 = src[0 + 3* ((i & 4)>>2)];
            s1 = src[1 + 3* ((i & 2)>>1)];
            s2 = src[2 + 3*  (i & 1)];
           
             /* floating point matrix*vector multiply */
            dst[dst_x]   = s0 * mvp[0] + s1 * mvp[0 + 4] + s2 * mvp[0 + 8] + s3[0];
            dst[dst_x+1] = s0 * mvp[1] + s1 * mvp[1 + 4] + s2 * mvp[1 + 8] + s3[1];
            dst[dst_x+2] = s0 * mvp[2] + s1 * mvp[2 + 4] + s2 * mvp[2 + 8] + s3[2];
            dst[dst_x+3] = s0 * mvp[3] + s1 * mvp[3 + 4] + s2 * mvp[3 + 8] + s3[3];
            
            w = dst[dst_x+3];

            if ( dst[dst_x] < -w  ) /* point is on the left of viewport*/ 
            { 
                vert_mask |= (1 << 0); 
            }

             if ( dst[dst_x] > w  ) /* point is on the right of viewport*/
            {
                vert_mask |= (1 << 1);  
            }
            
            if ( dst[dst_x+1] > w )      /* point is on the top of viewport*/
            { 
                vert_mask |= (1 << 2); 
            }

            if ( dst[dst_x+1] < -w )     /* point is below the viewport*/
            {
                vert_mask |= (1 << 3); 
			}

			if ( dst[dst_x+2] < -w ) vert_mask |= (1 << 4); /* point is outside the near plane*/
			if ( dst[dst_x+2] >  w ) vert_mask |= (1 << 5); /* point is outside the far plane*/
			
			mask &= vert_mask;
			total_vert_mask |= vert_mask;
			
       	}
       	*((u32*)input_params[BB_CLIP_BITS_AND_OUTPUT]) = mask;
       	*((u32*)input_params[BB_CLIP_BITS_OR_OUTPUT]) = total_vert_mask;
       	
    }
}

/**
 * @brief Get bounding box from the cache or create one and try to clip it against viewport borders
 * @return  MALI_FALSE if no intersections with viewport, MALI_TRUE otherwise
 * @param attr_array  attribute array, which contains vertices data for the current drawcall
 * @param ctx gles context
 * @param input_params  represent the data required for bounding box vertices transformation into clip space
 **/
MALI_STATIC mali_bool _gles_check_bounding_box(gles_vertex_attrib_array * attr_array, 
        struct gles_context *ctx,
        gles_bb_input input_params,
        const index_range *vs_range_buffer)
{
    float mem_out[BB_VERTICES_ELEMENTS_COUNT];
    u32 stride = attr_array->stride;
    u16 node_total = 0;
    bb_clip_state clip_state = TOTALLY_ACCEPT;
    struct gles_gb_context * gb_ctx = NULL;
    u32 rej_index = 0;
    u32 range_index = 0;
    u32 count;

    u16 idx_stack[16];
    bb_binary_node *bb_array = NULL;

    MALI_DEBUG_ASSERT_POINTER(ctx);
    MALI_DEBUG_ASSERT_POINTER(attr_array);
    MALI_DEBUG_ASSERT_POINTER(attr_array->vbo);
    MALI_DEBUG_ASSERT(attr_array->type == GL_FLOAT, ("non-float data is not allowed for bounding box feature"));

    gb_ctx = _gles_gb_get_draw_context( ctx );

    MALI_DEBUG_ASSERT_POINTER(gb_ctx);

    count = gb_ctx->parameters.index_count;

    /* get bounding box from the cache*/      
    if ( _gles_gb_get_bb_from_cache( ctx,
    								 attr_array->vbo->gb_data,
    								 ((int)attr_array->pointer),
    								 stride,
    								 attr_array->size * attr_array->elem_bytes + stride * gb_ctx->parameters.start_index,
    								 &bb_array,
    								 &node_total) != MALI_ERR_NO_ERROR )
    {
        return MALI_TRUE; /* no-reject*/
    }

    {
        u32 clip_and=0, clip_or=0;
        int index;
        plbu_range* plbu_range = gb_ctx->parameters.idx_plbu_range;
        
        index = 0;
        idx_stack[0] = 0;
        
       _gles_pack_bbt_input_parameter(input_params, BB_CLIP_BITS_AND_OUTPUT, (u32)&clip_and);
       _gles_pack_bbt_input_parameter(input_params, BB_CLIP_BITS_OR_OUTPUT, (u32)&clip_or);

        /*Do depth-first traversal for the binary tree */
        while( index >= 0 )
        {
        	int idx_cur_value = idx_stack[index];

            MALI_DEBUG_ASSERT( idx_stack[index] < node_total, ("out of range"));

            _gles_pack_bbt_input_parameter(input_params, BB_INPUT_STREAM, (u32)bb_array[idx_cur_value].bb.minmax);
            _gles_pack_bbt_input_parameter(input_params, BB_OUTPUT_STREAM, (u32)(mem_out));
            
#if MALI_BB_TRANSFORM_ON_NEON
            _mali_neon_transform_and_produce_clip_bits( input_params );
#else
           _gles_float_matrix4x4_vector3_transform_and_produce_clip_bits( input_params );
#endif
 
            /*clip_state = _gles_clip_bounding_box(clip_limit, mem_out, &c_or_mask);*/

        /* all points are inside the frustum. */
        if( clip_or == 0 )
        {
            clip_state =  TOTALLY_ACCEPT;
        }
        
        /* all points are one side of the frustum. */
        else if( clip_and != 0)
        {
            clip_state = TOTALLY_REJECT;
        }
         /* one vertex is completely inside */
        else
        {
            clip_state =  PARTIALLY_ACCEPT;
        }


            if( TOTALLY_REJECT == clip_state || TOTALLY_ACCEPT == clip_state )
            {
                /* Judge whether it is root node */
                if(!index && !bb_array[idx_cur_value].offset)
                {
                    if( TOTALLY_REJECT == clip_state )
                    {
                        return MALI_FALSE; /*reject all*/
                    }
                    else
                    {
                        return MALI_TRUE;
                    }
                }

                if( TOTALLY_REJECT == clip_state )
                {
                    rej_index += bb_array[idx_cur_value].count;

                    /*if the first range has been rejected or the adjacent range has been rejected */
                    if( !bb_array[idx_cur_value].offset || (bb_array[idx_cur_value].offset == plbu_range[range_index].start))
                    {
                       plbu_range[range_index].start += bb_array[idx_cur_value].count;
                       plbu_range[range_index].count = count - plbu_range[range_index].start;
                    }
                    else
                    {
                        plbu_range[range_index].count = bb_array[idx_cur_value].offset - plbu_range[range_index].start;
                        range_index++;
                        plbu_range[range_index].start = bb_array[idx_cur_value].offset + bb_array[idx_cur_value].count;
                        plbu_range[range_index].count = count - plbu_range[range_index].start;
                    }

                    if(!index) range_index--;
                }
				/* pop the stack */
                index--;
            }
            else
            {
                /* Only when the clip state is PARTIALLY_ACCEPT should this branch be fallen into. */

                if( (idx_cur_value * 2 + 1) < node_total)
                {
                    /* overwrite the top of the stack with the right branch index */
                    idx_stack[index] = idx_cur_value * 2 + 2;
                    
                 	MALI_DEBUG_ASSERT( idx_stack[index] < node_total, ("out of range"));
                 	
               		/* push the left address onto top of the stack */
                    idx_stack[++index] = idx_cur_value * 2 + 1;
                }
                else
                {
                	/* pop the stack */
                    index--;
                }

            }
        }
    }

    /* the draw call could be totally rejected if the whole index range is divided into small ones */
    if(rej_index == count)
    {
        return MALI_FALSE;
    } 
    else
    {
        gb_ctx->parameters.plbu_range_count = ++range_index;
    }

    /* if there is no rejection for the index, we don't need to do sort and merge things */
    if(rej_index)
    {
        _gles_gb_sort_and_merge_range(gb_ctx, node_total, bb_array, vs_range_buffer);
    }

    return MALI_TRUE;
}

/**
 * @brief function  Parsing cpu pretransform patterns from compiler
 * @param po  - binary shader program
 * Note: this function is local to this file so could be static but we don't want the compiler inlining it. 
 */
void parse_cpupretrans_patterns(bs_program* po)
{
	struct binary_shader_chunk_vertex_operand* stack[BS_MAXMEMORY_FOR_PATTERNS];
	struct binary_shader_expression_item* entry;
	int pos = 0, j=0, i;

	MALI_DEBUG_ASSERT_POINTER(po);
	MALI_DEBUG_ASSERT_POINTER(po->cpu_pretrans);
	MALI_DEBUG_ASSERT_POINTER(po->cpu_pass_parsed);

	entry = po->cpu_pretrans[0];
	
	while (NULL != entry)
	{
		/*  get data from the list */
		if (entry->type == BS_VERTEX_TYPE_OPERAND)
		{
			stack[pos++] = (struct binary_shader_chunk_vertex_operand*)entry->data;
		} 
		else /* operation */
		{
			struct binary_shader_chunk_vertex_expression* operation =  (struct binary_shader_chunk_vertex_expression*)entry->data;

			 int value_pos = 1; /* constant value position in input_params array */

			MALI_DEBUG_ASSERT( operation->expression_kind != EXPR_SWIZZLE, ("VS NEON Transform. doesn't support swizzle operations "));

			if (operation->expression_kind == EXPR_ASSIGN)
			{
			 	if (stack[--pos]->operand_value == BS_POSITION_ATTRIBUTE)
			 	{
					po->gl_position_item = po->num_pass;
			 	}
				po->num_pass++;
			}
			else
			{
				for ( i=0; i < operation->expression_num_opnd; i++)
				{
					struct binary_shader_chunk_vertex_operand* operand = stack[--pos];
					MALI_DEBUG_ASSERT( operand->operand_kind == OPND_CONST ||
										operand->operand_basic_type == OPND_TYPE_FLOAT || 
										operand->operand_basic_type == OPND_TYPE_MATRIX, ("VS NEON Transform. non-float operation found "));


				if (pos == 0) /* this is the attribute for storing the result to*/
				{
					pos++;
					break;
				}

				switch ( operand->operand_kind)
				{ 
					case OPND_VATT:
					{
						po->cpu_pass_parsed[po->num_pass].attrib_array_pos = operand->operand_index; 
						/* in vertex shader we may have a pattern like mat4x4*pos3x1, which assumes mat4x4*vec4(pos3x1,1) */ 
						if (value_pos == 1)
						{
							po->cpu_pass_parsed[po->num_pass].value1 = 0x3F800000;
						}
					}
					break;

					case OPND_VUNI:
					{
						po->cpu_pass_parsed[po->num_pass].mvp_pos =  operand->operand_index; 
					}
					break;
					case OPND_CONST:
					{
						if (value_pos++ == 1)
						{
							po->cpu_pass_parsed[po->num_pass].value1 = operand->operand_value;
						}
						else
						{
							po->cpu_pass_parsed[po->num_pass].value2 = operand->operand_value;                          
						}
					}
					break;
				};              
				}
			}
		}

		/* get next entry */
		entry = po->cpu_pretrans[++j];
	}
}

mali_err_code _gles_gb_try_reject_drawcall(  struct gles_context *ctx,
											 const index_range *vs_range_buffer,
											 mali_bool* bb_has_collision )
{
	gles_bb_input  input;

	struct gles_gb_cpu_pretrans_pass* cpu_pass_parsed;

	GLint size;
	struct gles_program_rendering_state* prs;
	struct gles_gb_context * gb_ctx;
	int pos;
	gles_vertex_attrib_array * attrib_array;

	*bb_has_collision = MALI_TRUE;

	MALI_DEBUG_ASSERT_POINTER( ctx );       
	gb_ctx = ctx->gb_ctx;
	MALI_DEBUG_ASSERT_POINTER( gb_ctx );
	prs = gb_ctx->prs;
	MALI_DEBUG_ASSERT_POINTER( prs );

	/* parse patterns if not parsed yet. it can be done on linking stage, 
	 however a lot of application (casual game e.t.c) will never exceed the BB vertices threshhold and we shouldn't waste 
	 time on parsing the VS patterns if  BB feature is not supposed to be used at all. 
	 gl_position_item field is an indicator ( if the pattern has been already parsed or not) */
	if ( prs->binary.gl_position_item == -1)
	{
		parse_cpupretrans_patterns(&prs->binary);
	}

	MALI_DEBUG_ASSERT( prs->binary.num_pass ==1 , ("each VS shader can contain exactly 1 pattern only") );

	cpu_pass_parsed = prs->binary.cpu_pass_parsed;


	pos = prs->reverse_attribute_remap_table[ cpu_pass_parsed[0].attrib_array_pos ];
	attrib_array =  &gb_ctx->vertex_array->attrib_array[ pos  ];

	MALI_DEBUG_ASSERT_POINTER( attrib_array );


	if (attrib_array->type != GL_FLOAT) /* we don't support non-float attribute streams in BBR feature*/
	{
		return MALI_ERR_NO_ERROR;
	}
	
	if( !attrib_array->buffer_binding ) /* non-VBO case not supported*/
	{
		return MALI_ERR_NO_ERROR;
	}

	MALI_DEBUG_ASSERT_POINTER(attrib_array->vbo); 
	if( !attrib_array->vbo->gb_data ) /* VBO exist, but never called glBufferData */
	{
		return MALI_ERR_NO_ERROR;
	}

	size = attrib_array->size;
	
	_gles_pack_bbt_input_parameter(input, BB_INPUT_STREAM, 0);
	_gles_pack_bbt_input_parameter(input, BB_MVP_MATRIX, (u32) ( &prs->binary.vertex_shader_uniforms.array[ cpu_pass_parsed[0].mvp_pos] ));
	_gles_pack_bbt_input_parameter(input, BB_OUTPUT_STREAM, 0);
	_gles_pack_bbt_input_parameter(input, BB_CONSTANT_VALUE_1, cpu_pass_parsed[0].value1);
	_gles_pack_bbt_input_parameter(input, BB_CONSTANT_VALUE_2, cpu_pass_parsed[0].value2);

	switch (size)
	{
		case 3: 
		{
			if ( !_gles_check_bounding_box( attrib_array, ctx, input, vs_range_buffer))
			{
				*bb_has_collision = MALI_FALSE;   
				return MALI_ERR_NO_ERROR;
			}
		}
		break;        
		default:
		break;
	}

	return MALI_ERR_NO_ERROR;
}

#endif
