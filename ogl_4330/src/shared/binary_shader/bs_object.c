/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_system.h"
#include "base/mali_memory.h"
#include "shared/binary_shader/bs_object.h"
#include "shared/binary_shader/online_compiler_integration.h"
#include "shared/binary_shader/bs_symbol.h"
#include "shared/binary_shader/bs_loader.h"
#include "shared/binary_shader/bs_loader_internal.h"
#include "shared/binary_shader/bs_error.h"
#include "shared/binary_shader/link_gp2.h"
#include "shared/mali_named_list.h"
#include <regs/MALIGP2/mali_gp_core.h>

MALI_EXPORT void __mali_shader_binary_state_init(bs_shader *bs)
{
	MALI_DEBUG_ASSERT_POINTER( bs );
	_mali_sys_memset(bs, 0, sizeof(bs_shader));
}

MALI_EXPORT void __mali_shader_binary_state_reset(bs_shader *bs)
{
	MALI_DEBUG_ASSERT_POINTER( bs );
	if(bs->shader_block)
	{
		_mali_sys_free(bs->shader_block);
		bs->shader_block = NULL;
	}

	if(bs->binary_block)
	{
		_mali_sys_free(bs->binary_block);
		bs->binary_block = NULL;
		bs->binary_size = 0;
	}

	if(bs->varyings)
	{
		_mali_sys_free(bs->varyings);
		bs->varyings = NULL;
	}

	if(bs->attributes)
	{
		_mali_sys_free(bs->attributes);
		bs->attributes = NULL;
	}

#ifdef MALI_BB_TRANSFORM
	if(bs->cpu_operations_stream)
	{
		_mali_sys_free(bs->cpu_operations_stream);
		bs->cpu_operations_stream= NULL;
	}
#endif

	if(bs->uniforms)
	{
		_mali_sys_free(bs->uniforms);
		bs->uniforms = NULL;
	}

	if(bs->projobs)
	{
		_mali_sys_free(bs->projobs);
		bs->projobs = NULL;
	}


	 bs_clear_error(&bs->log);
	__mali_shader_binary_state_init(bs);
}

MALI_EXPORT void __mali_program_binary_state_init(bs_program *bs)
{
	MALI_DEBUG_ASSERT_POINTER( bs );
	_mali_sys_memset(bs, 0, sizeof(bs_program));
}

#ifdef MALI_BB_TRANSFORM
MALI_STATIC_INLINE void _release_vertex_shader_precalc_expression(struct binary_shader_expression_item** stack, int size)
{
        int i;

        MALI_DEBUG_ASSERT_POINTER( stack );
        
        for (i =0; i<size; i++ ) 
        {
           _mali_sys_free(stack[i]->data);
           _mali_sys_free(stack[i]);
           stack[i] = NULL;
        }

        _mali_sys_free(stack);
}
#endif

MALI_EXPORT void __mali_program_binary_state_reset(bs_program *bs)
{
	u32 i;
	MALI_DEBUG_ASSERT_POINTER( bs );
	bs->linked = MALI_FALSE;

	bs_clear_error(&bs->log);

	/* samplers */
	if(bs->samplers.info)
	{
		_mali_sys_free(bs->samplers.info);
		bs->samplers.info = NULL;
	}
	bs->samplers.num_samplers = 0;
	bs->samplers.td_remap_table_size = 0;

	/* symbols */
	if(bs->attribute_symbols)
	{
		bs_symbol_table_free(bs->attribute_symbols);
		bs->attribute_symbols = NULL;
	}
	if(bs->uniform_symbols)
	{
		bs_symbol_table_free(bs->uniform_symbols);
		bs->uniform_symbols= NULL;
	}

	if(bs->varying_symbols)
	{
		bs_symbol_table_free(bs->varying_symbols);
		bs->varying_symbols = NULL;
	}

#ifdef MALI_BB_TRANSFORM
	if(bs->cpu_pretrans)
	{
	 _release_vertex_shader_precalc_expression(bs->cpu_pretrans, bs->cpu_pretrans_size);
	}

	  if (bs->cpu_pass_parsed != NULL) 
	  	_mali_sys_free(bs->cpu_pass_parsed);
#endif

	/* std varying streams */
	bs->varying_streams.count = 0;
	bs->varying_streams.block_stride = 0;
	if(bs->varying_streams.info)
	{
		_mali_sys_free(bs->varying_streams.info);
		bs->varying_streams.info = NULL;
	}
	bs->attribute_streams.count = 0;
	if(bs->attribute_streams.info)
	{
		_mali_sys_free(bs->attribute_streams.info);
		bs->attribute_streams.info = NULL;
	}

	/* special varying streams */
	if( bs->varying_position_symbol )
	{
		bs_symbol_free(bs->varying_position_symbol);
		bs->varying_position_symbol = NULL;
	}
	if(bs->varying_pointsize_symbol)
	{
		bs_symbol_free(bs->varying_pointsize_symbol);
		bs->varying_pointsize_symbol = NULL;
	}

	/* programs and uniform arrays */
	if (NULL != bs->vertex_pass.shader_binary_mem)
	{
		_mali_mem_ref_deref(bs->vertex_pass.shader_binary_mem);
		bs->vertex_pass.shader_binary_mem = NULL;
	}
	bs->vertex_pass.shader_size = 0;

	_mali_sys_memset(&bs->vertex_pass.flags, 0, sizeof(union bs_shader_object_flags));
	if(bs->vertex_shader_uniforms.array)
	{
		_mali_sys_free(bs->vertex_shader_uniforms.array);
		bs->vertex_shader_uniforms.array = NULL;
	}
	bs->vertex_shader_uniforms.cellsize = 0;

	/* free mem ref for fragment shader */
	if (NULL != bs->fragment_pass.shader_binary_mem)
	{
		_mali_mem_ref_deref(bs->fragment_pass.shader_binary_mem);
		bs->fragment_pass.shader_binary_mem = NULL;
	}
	bs->fragment_pass.shader_size = 0;

	_mali_sys_memset(&bs->fragment_pass.flags, 0, sizeof(union bs_shader_object_flags));
	if(bs->fragment_shader_uniforms.array)
	{
		_mali_sys_free(bs->fragment_shader_uniforms.array);
		bs->fragment_shader_uniforms.array = NULL;
	}
	bs->fragment_shader_uniforms.cellsize = 0;

	/* projob passes */
	if(bs->fragment_projob_passes) 
	{
		for(i = 0; i < bs->num_fragment_projob_passes; i++)
		{	
			if(bs->fragment_projob_passes[i].shader_binary_mem) 
			{
				_mali_mem_ref_deref( bs->fragment_projob_passes[i].shader_binary_mem );
				bs->fragment_projob_passes[i].shader_binary_mem = NULL;
			}
		}
		_mali_sys_free(bs->fragment_projob_passes);
		bs->fragment_projob_passes = NULL;
	}

	if(bs->vertex_projob_passes) 
	{
		for(i = 0; i < bs->num_vertex_projob_passes; i++)
		{	
			if(bs->vertex_projob_passes[i].shader_binary_mem) 
			{
				_mali_mem_ref_deref( bs->vertex_projob_passes[i].shader_binary_mem );
				bs->vertex_projob_passes[i].shader_binary_mem = NULL;
			}
		}
		_mali_sys_free(bs->vertex_projob_passes);
		bs->vertex_projob_passes = NULL;
	}


	/* simple overkill to ensure all possible alignment-generated holes are filled and initialized.  */
	__mali_program_binary_state_init(bs);
}

/* create copy of the "raw" shader binary image and store that in the shader object. needed in 
 * linking phase when merging the VS & FS objects into single program object. */
mali_err_code __mali_shader_binary_store_binary_block(bs_shader* bs, const void* block, const u32 size)
{
	void* temp;

	MALI_DEBUG_ASSERT_POINTER( bs );
	MALI_DEBUG_ASSERT_POINTER( block );
	MALI_DEBUG_ASSERT( size > 0, ("Input shader binary block size is 0"));
	
	temp = _mali_sys_malloc(size);
	if (NULL == temp) return MALI_ERR_OUT_OF_MEMORY;

	/* free the old one if necessary */
	if (bs->binary_block) _mali_sys_free(bs->binary_block);

	bs->binary_block = temp;

	/* save the binary, bit by bit, byte by byte, ... */
	_mali_sys_memcpy(bs->binary_block, block, size);
	bs->binary_size  = size;

	return MALI_ERR_NO_ERROR;
}

#ifdef MALI_BB_TRANSFORM

MALI_STATIC_INLINE mali_err_code parse_vertex_shader_expression_block(struct bs_stream* stream, struct binary_shader_chunk_vertex_expression* expr)
{
     MALI_DEBUG_ASSERT_POINTER( stream );
     MALI_DEBUG_ASSERT_POINTER( expr );

     if ( bs_read_or_skip_header(stream, EXPR) == 4)
     {
       expr->expression_kind = load_u8_value(stream);
       expr->expression_res_size = load_u8_value(stream);
       expr->expression_num_opnd = load_u8_value(stream);
       expr->swizzle = load_u8_value(stream);
       return MALI_ERR_NO_ERROR;
     }

     return MALI_ERR_FUNCTION_FAILED;
}

MALI_STATIC_INLINE mali_err_code parse_vertex_shader_operand_block(struct bs_stream* stream, struct binary_shader_chunk_vertex_operand* opnd)
{

     MALI_DEBUG_ASSERT_POINTER( stream );
     MALI_DEBUG_ASSERT_POINTER( opnd );


     if ( bs_read_or_skip_header(stream, OPND) == 12)
     {
       opnd->operand_kind = load_u8_value(stream);
       opnd->operand_basic_type= load_u8_value(stream);
       opnd->padding = load_u16_value(stream);
       opnd->padding = 0xFF;
       opnd->operand_index = load_u32_value(stream);
       opnd->operand_value = load_u32_value(stream);
       return MALI_ERR_NO_ERROR;
     }

     return MALI_ERR_FUNCTION_FAILED;

}


MALI_STATIC  mali_err_code  _load_vertex_shader_precalc_expression(bs_program *po, bs_shader* shader_obj)
{

	struct bs_stream stream;
	u32 count = 0;

	MALI_DEBUG_ASSERT_POINTER( shader_obj );
	MALI_DEBUG_ASSERT( shader_obj->cpu_operations_stream != NULL, ("Program Object must be init/resetted before calling this function"));

	stream.data = shader_obj->cpu_operations_stream;
	stream.position = 0;
	stream.size = shader_obj->cpu_operations_stream_size;

	if ( bs_read_or_skip_header(&stream, VSOP) < 4 )
	{
		return MALI_ERR_EARLY_OUT;
	}

	po->cpu_pretrans = _mali_sys_malloc(sizeof(struct binary_shader_expression_item*) * BS_MAXMEMORY_FOR_PATTERNS ); /* maximum memory for 16 mvp*vec1(x,a,b,c) patterns */

	if( NULL == po->cpu_pretrans ) 
	{
	  return MALI_ERR_OUT_OF_MEMORY;
	}

	po->cpu_pass_parsed =_mali_sys_calloc ( 1, MALIGP2_MAX_VS_INPUT_REGISTERS* sizeof (struct gles_gb_cpu_pretrans_pass) ); /* for parsed content*/
	
	if( NULL == po->cpu_pass_parsed ) 
	{
	  _mali_sys_free( po->cpu_pretrans );
	  po->cpu_pretrans = NULL;
	  return MALI_ERR_OUT_OF_MEMORY;
	}

	while ( bs_read_or_skip_header(&stream, VACT) > 0 )
	{
		enum blocktype name = bs_peek_header_name(&stream);
		
		switch (name)
		{
			/*expression*/
			case EXPR:
				{
					struct binary_shader_expression_item* item = _mali_sys_malloc(sizeof(struct binary_shader_expression_item));
					struct binary_shader_chunk_vertex_expression* expr = _mali_sys_malloc(sizeof(struct binary_shader_chunk_vertex_expression));
					mali_err_code err = MALI_ERR_NO_ERROR;
					if(item!= NULL && expr!=NULL)
					{
						err = parse_vertex_shader_expression_block(&stream, expr);
					}
					if (item==NULL || expr==NULL || err != MALI_ERR_NO_ERROR)
					{
						if(item!=NULL) _mali_sys_free(item);
						if(expr!=NULL) _mali_sys_free(expr);
						_release_vertex_shader_precalc_expression(po->cpu_pretrans, count);
						_mali_sys_free( po->cpu_pass_parsed );
						po->cpu_pretrans = NULL;
						po->cpu_pass_parsed = NULL;
						return err;
					}

					item->type = BS_VERTEX_TYPE_EXPRESSION;
					item->data = expr;
					po->cpu_pretrans[count++] = item;
					po->cpu_pretrans[count] = NULL;
				}
				break;
			/*operand*/
			case OPND:
				{
					struct binary_shader_expression_item* item = _mali_sys_malloc(sizeof(struct binary_shader_expression_item));
					struct binary_shader_chunk_vertex_operand* opnd = _mali_sys_malloc(sizeof(struct binary_shader_chunk_vertex_operand));
					mali_err_code err = MALI_ERR_NO_ERROR;
					if(item!= NULL && opnd!=NULL)
					{
						err = parse_vertex_shader_operand_block(&stream, opnd);
					}
					if (item==NULL || opnd==NULL || err != MALI_ERR_NO_ERROR)
					{
						if(item!=NULL) _mali_sys_free(item);
						if(opnd!=NULL) _mali_sys_free(opnd);
						_release_vertex_shader_precalc_expression(po->cpu_pretrans, count);
						_mali_sys_free( po->cpu_pass_parsed );
						po->cpu_pretrans = NULL;
						po->cpu_pass_parsed = NULL;
						return err;
					}

					item->type = BS_VERTEX_TYPE_OPERAND;
					item->data = opnd;
					po->cpu_pretrans[count++] = item;
					po->cpu_pretrans[count] = NULL;
				}
				break;
			default:
				MALI_DEBUG_ASSERT(0, ("VACT block contains invalid blockname"));
				_release_vertex_shader_precalc_expression(po->cpu_pretrans, count);
				_mali_sys_free( po->cpu_pass_parsed );
				po->cpu_pretrans = NULL;
				po->cpu_pass_parsed = NULL;
				return MALI_ERR_FUNCTION_FAILED;
		};
		
	}

	po->cpu_pretrans_size = count;
	po->gl_position_item= -1;
	
	return MALI_ERR_NO_ERROR;
}
#endif

MALI_STATIC  mali_err_code _load_DPRO_block_for_shader(mali_base_ctx_handle base_ctx, bs_program *po, bs_shader* shader_obj, u32 shader_type )
{
	MALI_DEBUG_ASSERT_POINTER( shader_obj );
	if (shader_type != BS_FRAGMENT_SHADER && shader_type != BS_VERTEX_SHADER)
	{
		MALI_DEBUG_ASSERT( 0 , ("shader's type neither fragment nor vertex. Impossible here.") );
		return MALI_ERR_FUNCTION_FAILED;
	}

	 if(shader_obj->projobs != NULL && shader_obj->projob_size != 0 )
	  {
				unsigned int i;
				unsigned int j;
				/*projob passes, uniforms offset/stride assignment*/
				u32 num_projob_passes;
				u32 projob_uniform_offset = 0;
				u32 projob_uniform_stride = 0;
				struct bs_shaderpass* projob_passes = NULL;
				
				unsigned int dproblock_datasize;
				struct bs_stream stream;
				stream.data = shader_obj->projobs;
				stream.position = 0;
				stream.size = shader_obj->projob_size;
				
				dproblock_datasize = bs_read_or_skip_header(&stream, DPRO);
				if(stream.position == stream.size || dproblock_datasize < 4)
				{
					bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "The fragment shader is damaged, corrupt DPRO block detected.");
					return MALI_ERR_FUNCTION_FAILED;
				}

				/* load DPRO block contents */
				num_projob_passes = load_u32_value(&stream);
				
				if( num_projob_passes > 0 ) 
				{

					projob_passes = _mali_sys_malloc( num_projob_passes * sizeof(struct bs_shaderpass));
					if( NULL == projob_passes ) return MALI_ERR_OUT_OF_MEMORY;
					_mali_sys_memset(projob_passes, 0, num_projob_passes * sizeof(struct bs_shaderpass));
				
					projob_uniform_offset = load_u32_value(&stream);
					projob_uniform_stride = load_u32_value(&stream);
					
					if ( projob_uniform_offset & (projob_uniform_offset-1)) 
					{
						/* must be a POT */
						bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "The fragment shader is damaged, corrupt DPRO block detected (invalid uniform offset).");
						_mali_sys_free(projob_passes);
						return MALI_ERR_FUNCTION_FAILED;
					}

					for(i = 0; i < num_projob_passes; i++)
					{
						void* shaderdata;
						mali_err_code err;
						struct bs_shaderpass* pass = &( *(projob_passes +i));
						err = bs_read_and_allocate_binary_block_contents( &stream, &shaderdata, &pass->shader_size);
						if(err != MALI_ERR_NO_ERROR) 
						{
						        for (j=0; j < i; j++)  _mali_mem_ref_deref ( (*(projob_passes+j)).shader_binary_mem );
							_mali_sys_free(projob_passes);
							return err;
						}
						pass->flags = shader_obj->flags;

						if (shader_type == BS_FRAGMENT_SHADER)
						{
							pass->flags.fragment.size_of_first_instruction = ((unsigned char*)shaderdata)[0] & 0x1F;
						}
						else if (shader_type == BS_VERTEX_SHADER)
						{
						      mali_err_code err = bs_read_VPRO_block(&stream, &pass->flags.vertex.instruction_end);
							if(err != MALI_ERR_NO_ERROR) 
							{
								_mali_sys_free(shaderdata);
								for (j=0; j < i; j++)  _mali_mem_ref_deref ( (*(projob_passes+j)).shader_binary_mem );
								_mali_sys_free(projob_passes);
								return err;
							}

							pass->flags.vertex.num_input_registers = 0;
							pass->flags.vertex.num_output_registers = 0;
                                                        pass->flags.vertex.instruction_start = 0;
							pass->flags.vertex.instruction_last_touching_input =  pass->flags.vertex.instruction_end;
						}
					

						(*(projob_passes+i)).shader_binary_mem = _mali_mem_ref_alloc_mem(base_ctx, shader_obj->projob_size, 64, MALI_PP_READ | MALI_CPU_WRITE | MALI_CPU_READ);
						if (NULL == (*(projob_passes+i)).shader_binary_mem) 
						{
							_mali_sys_free(shaderdata);
							for (j=0; j < i; j++)  _mali_mem_ref_deref ( (*(projob_passes+j)).shader_binary_mem );
							_mali_sys_free(projob_passes);
							return MALI_ERR_OUT_OF_MEMORY;
						}

						/* copy shader binary */
						_mali_mem_write_cpu_to_mali(
							(*(projob_passes +i)).shader_binary_mem->mali_memory,
							0,
							shaderdata,
							pass->shader_size,
							1
						);
						_mali_sys_free(shaderdata);
					}
				} /*  > 0 passes */

				switch (shader_type)
				{
				case  BS_FRAGMENT_SHADER:
					po->num_fragment_projob_passes = num_projob_passes;
					po->projob_uniform_offset =  projob_uniform_offset;
					po->projob_uniform_stride =  projob_uniform_stride;
					po->fragment_projob_passes = projob_passes;
					break;
				case BS_VERTEX_SHADER:
					po->num_vertex_projob_passes = num_projob_passes;
					po->vertex_projob_passes = projob_passes;
					break;
				default:
					break;
				}
			} /* if DPRO block present */

			return MALI_ERR_NO_ERROR;
}

MALI_STATIC mali_err_code __mali_internal_link_binary_shaders( mali_base_ctx_handle base_ctx, bs_program *po, bs_shader* vert_obj, bs_shader* frag_obj)
{
	/* NOTE: This function accepts any combination of vert_objs and frag_objs being NULL.
	 * Do not expect the linked result to be complete in such cases, though (ie, no varying streams and such) */
	mali_err_code error_code;

	MALI_DEBUG_ASSERT_POINTER( po );
	MALI_DEBUG_ASSERT( NULL != vert_obj || NULL != frag_obj, ("most likely you want the vert\'s or the frag\'s or even both") );

#ifndef NDEBUG
	/* assert the program object is completely cleared prior to usage - this debug assert block should reduce accidental usage
	 * by asserting that every single byte in the program binary state is set to 0 before entering this function */
	{
		u8* bytedata = (u8*) po;
		u8* bytedata_start = bytedata;
		while( bytedata < bytedata_start + sizeof(bs_program))
		{
			MALI_DEBUG_ASSERT(*bytedata == 0, ("Program object not completely cleared prior to calling mali_link_binary_shaders"));
			bytedata++;
		}
	}

#endif

	/* set up linked status objects */
	if(vert_obj)
	{
		if( ! vert_obj->compiled )
		{
			bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader not compiled prior to linking");
			return MALI_ERR_FUNCTION_FAILED;
		}

		/* assert program size within legal range */
		if( vert_obj->shader_size / MALIGP2_VS_INSTRUCTION_SIZE > MALIGP2_MAX_VS_INSTRUCTION_COUNT )
		{
			bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader too large");
			return MALI_ERR_FUNCTION_FAILED;
		}
		if(vert_obj->shader_size == 0)
		{
			po->vertex_pass.shader_binary_mem    = NULL;
			po->vertex_pass.shader_size          = 0;
		}
		else
		{
			mali_err_code err;

			/* allocate memory */
			po->vertex_pass.shader_binary_mem = _mali_mem_ref_alloc_mem(base_ctx, vert_obj->shader_size, 8, MALI_GP_READ | MALI_CPU_WRITE | MALI_CPU_READ);
			if (NULL == po->vertex_pass.shader_binary_mem) return MALI_ERR_OUT_OF_MEMORY;

			/* copy shader to program object */
			_mali_mem_write_cpu_to_mali(
				po->vertex_pass.shader_binary_mem->mali_memory,
				0,
				vert_obj->shader_block,
				vert_obj->shader_size,
				1
			);

			/* update size */
			po->vertex_pass.shader_size = vert_obj->shader_size;

			/* copy flags */
			_mali_sys_memcpy(&po->vertex_pass.flags.vertex, &vert_obj->flags.vertex, sizeof(struct bs_vertex_flags));

			err = _load_DPRO_block_for_shader(base_ctx, po, vert_obj, BS_VERTEX_SHADER);
			if (err != MALI_ERR_NO_ERROR)
			{
				return err;
			}

		}
	}

	if(frag_obj)
	{
		if( ! frag_obj->compiled )
		{
			bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Fragment shader not compiled prior to linking");
			return MALI_ERR_FUNCTION_FAILED;
		}
		if( frag_obj->shader_size == 0)
		{
			po->fragment_pass.shader_binary_mem  = NULL;
			po->fragment_pass.shader_size        = 0;
		}
		else
		{
		 	mali_err_code err;
			/* allocate memory */
			po->fragment_pass.shader_binary_mem = _mali_mem_ref_alloc_mem(base_ctx, frag_obj->shader_size, 64, MALI_PP_READ | MALI_CPU_WRITE | MALI_CPU_READ);
			if (NULL == po->fragment_pass.shader_binary_mem) return MALI_ERR_OUT_OF_MEMORY;

			/* copy shader binary */
			_mali_mem_write_cpu_to_mali(
				po->fragment_pass.shader_binary_mem->mali_memory,
				0,
				frag_obj->shader_block,
				frag_obj->shader_size,
				1
			);

			/* update size */
			po->fragment_pass.shader_size = frag_obj->shader_size;

			/* copy flags */
			_mali_sys_memcpy(&po->fragment_pass.flags.fragment, &frag_obj->flags.fragment, sizeof(struct bs_fragment_flags));

			/* The link for the DPRO load */
			err = _load_DPRO_block_for_shader(base_ctx, po, frag_obj, BS_FRAGMENT_SHADER);
			if (err != MALI_ERR_NO_ERROR)
			{
				return err;
			}
		
		}
	}

	/* "Active" Attributes are checked for legality; attribute symbolic table is built */
	if ( NULL != vert_obj )
	{
		struct bs_stream stream;
		stream.data = vert_obj->attributes;
		stream.position = 0;
		stream.size = vert_obj->attribute_size;

		error_code = __mali_binary_shader_load_attribute_table(po, &stream);
		if(error_code != MALI_ERR_NO_ERROR) return error_code;
	}

	/* "Active" Uniforms are checked for legality; uniform symbolic table and values are built */
	{
		struct bs_stream vert_stream, frag_stream;

		vert_stream.data = NULL;
		vert_stream.position = 0;
		vert_stream.size = 0;
		frag_stream.data = NULL;
		frag_stream.position = 0;
		frag_stream.size = 0;

		if(vert_obj)
		{
			vert_stream.data = vert_obj->uniforms;
			vert_stream.size = vert_obj->uniform_size;
		}
		if(frag_obj)
		{
			frag_stream.data = frag_obj->uniforms;
			frag_stream.size = frag_obj->uniform_size;
		}

		error_code = __mali_binary_shader_load_uniform_table(po, &vert_stream, &frag_stream);
		if(error_code != MALI_ERR_NO_ERROR) return error_code;
	}

#ifdef MALI_BB_TRANSFORM
	if ( NULL != vert_obj && vert_obj->cpu_operations_stream!= NULL)
	{
		error_code = _load_vertex_shader_precalc_expression(po, vert_obj);
		if(error_code != MALI_ERR_NO_ERROR) return error_code;
	}
#endif

	/* "Active" varyings are checked for legality; varying symbolic table is built */
	{
		struct bs_stream vert_stream, frag_stream;

		vert_stream.data = NULL;
		vert_stream.position = 0;
		vert_stream.size = 0;
		frag_stream.data = NULL;
		frag_stream.position = 0;
		frag_stream.size = 0;

		if(vert_obj)
		{
			vert_stream.data = vert_obj->varyings;
			vert_stream.size = vert_obj->varying_size;
		}
		if(frag_obj)
		{
			frag_stream.data = frag_obj->varyings;
			frag_stream.size = frag_obj->varying_size;
		}

		error_code = __mali_binary_shader_load_varying_table(po, &vert_stream, &frag_stream);
		if(error_code != MALI_ERR_NO_ERROR) return error_code;

		/* rewrite the vertex shader to reflect the new locations */
		if(vert_obj && frag_obj && vert_obj->shader_size>0 && frag_obj->shader_size>0) /* no point unless both shaders are present */
		{
			mali_err_code error_code = __mali_gp2_rewrite_vertex_shader_varying_locations( po );
			if(error_code == MALI_ERR_OUT_OF_MEMORY) return error_code;
			MALI_DEBUG_ASSERT(error_code == MALI_ERR_FUNCTION_FAILED || error_code == MALI_ERR_NO_ERROR, ("Unsupported error msg"));
			if(error_code == MALI_ERR_FUNCTION_FAILED)
			{
				bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Relocation of Varyings in Vertex shader failed."  );
				return error_code;
			}
		}
	}

	po->linked = MALI_TRUE;

	return MALI_ERR_NO_ERROR;
}

MALI_EXPORT mali_err_code __mali_link_binary_shaders( mali_base_ctx_handle base_ctx, bs_program *po, bs_shader* vert_obj, bs_shader* frag_obj)
{
	/* extra layer of wrapping to catch and handle cases where the program error log is set to the out of memory.
	 * This way, MALI_ERR_OUT_OF_MEMORY gets propagated when an error setter runs out of memory */
	mali_err_code err = __mali_internal_link_binary_shaders( base_ctx, po, vert_obj, frag_obj);
	if(bs_is_error_log_set_to_out_of_memory(&po->log)) return MALI_ERR_OUT_OF_MEMORY;
	return err;
}

MALI_EXPORT mali_err_code __mali_merge_binary_shaders( void** binary_block, u32* binary_size, void* attrib_block, u32 attrib_size, bs_shader* vsBinary, bs_shader* fsBinary)
{
	char* position = NULL;

	MALI_DEBUG_ASSERT_POINTER(vsBinary);
	MALI_DEBUG_ASSERT_POINTER(fsBinary);
	MALI_DEBUG_ASSERT_POINTER(binary_block);
	MALI_DEBUG_ASSERT_POINTER(binary_size);
	MALI_DEBUG_ASSERT_POINTER(attrib_block);

	MALI_DEBUG_ASSERT(*binary_block == NULL, ("The binary block should not exist at this point"));

	/* allocate new block holding the MBS1 block of both the vshader, fshader and attribute block. 
	 * For simplicity, that's the size of the vshader and fshader MBS1 block, minus one MBS1 header (8 bytes), plus the size of the BATT block. 
	 * Ensuring that all three blocks are at least 1 header large (MBS1 or BATT)
	 */
	MALI_DEBUG_ASSERT(vsBinary->binary_size >= 8, ("VS block MBS1 not present! This should be a linker error earlier"));
	MALI_DEBUG_ASSERT(fsBinary->binary_size >= 8, ("FS block MBS1 not present! This should be a linker error earlier"));
	MALI_DEBUG_ASSERT(attrib_size >= 8, ("BATT block not present! This should be caught earlier"));

	/* allocate new block */
	*binary_size = vsBinary->binary_size + fsBinary->binary_size + attrib_size - 8;
	*binary_block = _mali_sys_malloc( *binary_size);
	if(!(*binary_block)) return MALI_ERR_OUT_OF_MEMORY;

	MALI_DEBUG_ASSERT(*binary_size >= 8, ("MBS1 block too small!"));
	
	/* fill it in by copying 
	 *  - first the vertex block (including the MBS1 header)
	 *  - then the fragment block (minus the MBS1 header)
	 *  - then the BATT block
	 */
	position = (char*)(*binary_block);

	_mali_sys_memcpy(position, vsBinary->binary_block, vsBinary->binary_size);
	position += vsBinary->binary_size;
	_mali_sys_memcpy(position, ((const u8*)fsBinary->binary_block) + 8, fsBinary->binary_size-8);
	position += fsBinary->binary_size - 8;
	_mali_sys_memcpy(position, attrib_block, attrib_size);

	/* and finally update the size of the MBS1 header to span all the data in the MBS1 block*/
	((u32*)(*binary_block))[1] = (*binary_size) - 8; /* all data minus MBS1 header size*/

	return MALI_ERR_NO_ERROR;
}

