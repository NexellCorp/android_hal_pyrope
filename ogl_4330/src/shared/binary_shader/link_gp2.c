/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_runtime.h>

#include <shared/binary_shader/link_gp2.h>
#include <shared/binary_shader/bs_object.h>
#include <shared/binary_shader/bs_symbol.h>
#include <shared/binary_shader/bs_error.h>

#include <base/mali_memory.h>

#include "util/mali_mem_ref.h"
#include <regs/MALIGP2/mali_gp_core.h>

#include <base/mali_byteorder.h>

/**
 * @brief helper function for replace_bits; extracts n bits from the given bitwise offset
 * @param buf A buffer of data
 * @param bit_offset bitwise offset where we start looking
 * @param n_bits number of bits to extract, must no more than 32
 */
MALI_STATIC u32 extract_bits(u8 *buf, u32 bit_offset, u32 n_bits)
{
	u32 temp_res = 0U;
	u32 i, mask;
	for(i=0;i<(n_bits+7)>>3;i++)
	{
		/* Must access Mali memory in little endian addresses */
		u32 A = _MALI_GET_U8_ENDIAN_SAFE(buf + (bit_offset >> 3) + i);
		A |= _MALI_GET_U8_ENDIAN_SAFE(buf + (bit_offset >> 3) + 1 + i) << 8;
		A >>= (bit_offset & 7);
		A &= 0xFFU;
		temp_res |= A << (i * 8);
	}
	mask = (1U << n_bits) - 1U;
	temp_res &= mask;

	return temp_res;
}

/**
 * @brief helper function for remap_varyings/link_gp2_attribs; replaces n bits from the given bitwise offset
 * @brief buf a data buffer to replace data in
 * @param n_bits the number of bits to replace, no more than 32
 * @param bit_offset Bitwise offset where to start replacing
 * @param value The value to replace into
 */
MALI_STATIC void replace_bits(u8 *buf, u32 bit_offset, u32 n_bits, u32 value)
{
	u64 val = value;
	u32 low_offset = bit_offset & 0x7;
	u32 high_offset;
	u32 i;
	val = extract_bits(buf, bit_offset - low_offset, low_offset) | (val<< low_offset);
	n_bits += low_offset;
	bit_offset -= low_offset;

	high_offset = 8 - (n_bits & 0x7);
	if(high_offset < 8)
	{
		val = val | (extract_bits(buf, bit_offset + n_bits, high_offset)<<n_bits);
		n_bits += high_offset;
	}
	for(i = 0; i < n_bits; i += 8)
	{
		/* Must access Mali memory in little endian addresses */
		_MALI_SET_U8_ENDIAN_SAFE(buf + ((bit_offset + i)/8), (u8)(val>>i));
	}

}

/**
 * @brief Perform varying remapping
 * @param po The program object to remap, must be linked and contain a gp2 program
 * @param varying_transformation_table A remapping table for varyings, on the form varying_transformation_table[old_location] = new_location
 * @param varying_transformation_table_size The size of the aforementioned table
 */
MALI_STATIC mali_err_code remap_varyings(bs_program *po, s32 *varying_transformation_table, u32 varying_transformation_table_size)
{
	/* NOTE:
	   quite brute force, it just iterates through the instruction words and relocates output writes as it encounters them
	*/
	u32 i;
	u32 vertex_shader_size = po->vertex_pass.shader_size * 8;

	u8 *vertex_shader;

	MALI_DEBUG_ASSERT_POINTER(po);
	MALI_DEBUG_ASSERT_POINTER(po->vertex_pass.shader_binary_mem);
	MALI_DEBUG_ASSERT_POINTER(po->vertex_pass.shader_binary_mem->mali_memory);

	vertex_shader = _mali_mem_ptr_map_area(
		po->vertex_pass.shader_binary_mem->mali_memory, 0,
		po->vertex_pass.shader_size, 1,
		MALI_MEM_PTR_READABLE | MALI_MEM_PTR_WRITABLE
	);

	/* check if mapping failed */
	if (NULL == vertex_shader) return MALI_ERR_OUT_OF_MEMORY;

#define WRITE_DISABLED 7

	for(i = 0; i < vertex_shader_size; i += 128) /* one 128-bit instruction word at a time */
	{
		int pre_write_selects[4];
		int post_write_selects[4] = { WRITE_DISABLED, WRITE_DISABLED, WRITE_DISABLED, WRITE_DISABLED };
		int need_relocation = 0;
		int pre_address = -1;
		int post_address = -1;
		u32 j, k;
		for(k = 0; k < 4; ++k)
		{
			pre_write_selects[k] = extract_bits(vertex_shader, i+71+k*3, 3);
		}
		for(j = 0; j < 2; ++j)
		{
			int output_destination = extract_bits(vertex_shader, i+90+j*5, 5); /* output reg. 0..15 are work registers, 16..31 are output registers */
			if(output_destination >= 16 && (pre_write_selects[j*2+0] != WRITE_DISABLED || pre_write_selects[j*2+1] != WRITE_DISABLED))
			{
				need_relocation = 1;
				if(pre_address == -1)
				{
					pre_address = output_destination - 16;
				} else {
					/* varying write that's impossible to relocate, bail out */
					if(pre_address != output_destination - 16)
					{
						/* unmap pointer */
						_mali_mem_ptr_unmap_area(po->vertex_pass.shader_binary_mem->mali_memory);
						vertex_shader = NULL;

						return MALI_ERR_FUNCTION_FAILED;
					}
				}
			}
		}

		if(need_relocation)
		{
			for(k = 0; k < 4; ++k)
			{
				if(pre_write_selects[k] != WRITE_DISABLED)
				{
					int pre_offset = pre_address*4 + k;
					if(pre_offset >= 0 && (unsigned int)pre_offset < varying_transformation_table_size)
					{
						int post_offset = varying_transformation_table[pre_offset];
						if(post_offset >= 0)
						{
							if(post_address == -1)
							{
								post_address = post_offset/4;
							} else {
								if(post_address != post_offset/4)
								{
									/* unmap pointer */
									_mali_mem_ptr_unmap_area(po->vertex_pass.shader_binary_mem->mali_memory);
									vertex_shader = NULL;

									return MALI_ERR_FUNCTION_FAILED;
								}
							}
							if(post_write_selects[post_offset%4] != WRITE_DISABLED)
							{
								_mali_mem_ptr_unmap_area(po->vertex_pass.shader_binary_mem->mali_memory);
								return MALI_ERR_FUNCTION_FAILED;
							}
							post_write_selects[post_offset%4] = pre_write_selects[k];
						}
					}
				}
			}
			if(post_address != -1)
			{
				for(j = 0; j < 2; ++j)
				{
					replace_bits(vertex_shader, i+90+5*j, 5, post_address+16);
				}
			}
			for(k = 0; k < 4; ++k)
			{
				MALI_DEBUG_ASSERT(post_address != -1 || post_write_selects[k] == WRITE_DISABLED, ("post_address != -1 || post_write_selects[k] == WRITE_DISABLED"));
				replace_bits(vertex_shader, i+71+3*k, 3, post_write_selects[k]);
			}
		}

	}

	/* unmap pointer */
	_mali_mem_ptr_unmap_area(po->vertex_pass.shader_binary_mem->mali_memory);
	vertex_shader = NULL;

	return MALI_ERR_NO_ERROR;
}


MALI_EXPORT MALI_CHECK_RESULT mali_err_code _mali_gp2_link_attribs( bs_program* po, int attrib_remap[], mali_bool rewrite_symbols)
{
	u32 i;
	u32 vertex_shader_size = po->vertex_pass.shader_size * 8;
	u8 *vertex_shader = _mali_mem_ptr_map_area(
		po->vertex_pass.shader_binary_mem->mali_memory, 0,
		po->vertex_pass.shader_size, 1,
		MALI_MEM_PTR_READABLE | MALI_MEM_PTR_WRITABLE
	);

	/* check if mapping failed */
	if (NULL == vertex_shader) return MALI_ERR_OUT_OF_MEMORY;

	MALI_DEBUG_ASSERT( (vertex_shader_size >= 128), ("vertex shader size too small: %d", vertex_shader_size));
	for(i = 0; i < vertex_shader_size; i += 128) /* one 128-bit instruction word at a time */
	{
		/* bits 58:62 contain load0_idx, the only place where input streams can be loaded.
		   If this value is in the range 16-31, it means that input stream 0-15 is loaded
		*/
		int load0_idx = extract_bits(vertex_shader, i+58, 5);
		if(load0_idx >= 16)
		{
			int pre_input_stream = load0_idx - 16;
			int post_input_stream;
			MALI_DEBUG_ASSERT_RANGE(pre_input_stream, 0, 16);
			post_input_stream = attrib_remap[pre_input_stream];
			MALI_DEBUG_ASSERT((post_input_stream >= 0) && (post_input_stream < 16), ("attrib %d is out of range (%d)", pre_input_stream, post_input_stream));
			replace_bits(vertex_shader, i+58, 5, post_input_stream+16);

		}
	}

	MALI_DEBUG_ASSERT_POINTER(po->vertex_pass.shader_binary_mem);
	MALI_DEBUG_ASSERT_POINTER(po->vertex_pass.shader_binary_mem->mali_memory);

	/* unmap pointer */
	_mali_mem_ptr_unmap_area(po->vertex_pass.shader_binary_mem->mali_memory);
	vertex_shader = NULL;

	/* rewrite all symbols */
	if(rewrite_symbols == MALI_TRUE)
	{
		MALI_DEBUG_ASSERT_POINTER(po->attribute_symbols);

		/* remap symbols */
		for (i = 0; i < po->attribute_symbols->member_count; ++i)
		{
			int location;
			struct bs_symbol* symb = po->attribute_symbols->members[i];
			if(symb != NULL)
			{
				MALI_DEBUG_ASSERT(symb->location.vertex % 4 == 0, ("Attribute locations must be divisible by 4"));
				location = attrib_remap[symb->location.vertex/4];

#ifdef MALI_BB_TRANSFORM
				/* assigning remapped attributes locations to operand_index*/
				if (po->cpu_pretrans != NULL )
				{
					int pos;
					struct binary_shader_expression_item* entry  = NULL;
					for ( pos = 0; pos < po->cpu_pretrans_size ; pos++) 
					{
						entry = po->cpu_pretrans[pos];
						if (entry->type == BS_VERTEX_TYPE_OPERAND)
						{
							struct binary_shader_chunk_vertex_operand* operand =  (struct binary_shader_chunk_vertex_operand*)entry->data;
							if (operand->operand_kind == OPND_VATT && operand->operand_index == (u32)symb->location.vertex && operand->padding != i )
							{
								operand->operand_value  = 0;
								operand->operand_index = location;
								operand->padding = i;
								break;
							}
							if (operand->operand_index == BS_POSITION_ATTRIBUTE_INDEX) 
							{
								operand->operand_value = BS_POSITION_ATTRIBUTE;
								operand->operand_index--;
							}
						}
					}
				}
#endif
				symb->location.vertex = location * 4;
			}
		}

		/* remap stream table */
		for (i = 0; i < po->attribute_streams.count; ++i)
		{
			int location = attrib_remap[po->attribute_streams.info[i].index];
			po->attribute_streams.info[i].index = location;

		}

	}

	return MALI_ERR_NO_ERROR;
}


MALI_EXPORT MALI_CHECK_RESULT mali_err_code __mali_gp2_rewrite_vertex_shader_varying_locations( bs_program* po )
{

	mali_err_code err;
	u32 i;
	s32* varying_transformation_table = NULL;  /**< The transformation table is a converter lookup table for varyings */
	u32  varying_transformation_table_size = 256;  /* num cells used by the GP2 output register - prior to rewrite (can be any number)*/
	                                               /* Max size is 12 varyings * 4 components + 4 componets position + 4 components pointsize. Playing it big. */

	/************************** set up varying transform mapping table **************************/
	varying_transformation_table = _mali_sys_malloc( varying_transformation_table_size * sizeof(s32) );
	if(!varying_transformation_table) return MALI_ERR_OUT_OF_MEMORY;
	_mali_sys_memset(varying_transformation_table, MALI_REINTERPRET_CAST(u32)-1, varying_transformation_table_size * sizeof(s32));



	/* Next out is to actually fill this table. This is done by walking through every output stream,
	   writing the matching fragment location into the remap array for every vertex location. We
	   need to rewrite both the "normal" varying symbols as well as the position/pointsize streams */

	/* The "normal" varying streams are simple; new location in the vertex shader is given by the
	   location in the fragment shader. Iterate all varying symbols, for each cell in symbol,
	   write transform to transformation table. */
	for(i = 0; i < po->varying_symbols->member_count; i++)
	{
		struct bs_symbol* symbol;
		symbol = po->varying_symbols->members[i];
		if(symbol->location.vertex != -1 && symbol->location.fragment != -1)
		{
			u32 h, k, j;
			u32 count = symbol->array_size;
			u32 element_height= (symbol->datatype==DATATYPE_MATRIX) ? symbol->type.primary.vector_size : 1;
			if(!count) count = 1;
			for(k = 0; k < count ; k++)
			{ /* for each vector in array */
				for(h = 0; h < element_height; h++)
				{ /* for each element in vector */
					for(j = 0; j < symbol->type.primary.vector_size; j++)
					{ /* for each cell in element */
						int foffset = j + h*symbol->type.primary.vector_stride.fragment + (k*symbol->array_element_stride.fragment);
						int voffset = j + h*symbol->type.primary.vector_stride.vertex + (k*symbol->array_element_stride.vertex);
						varying_transformation_table[symbol->location.vertex + voffset] = symbol->location.fragment + foffset;
					}
				}
			}
		}
	}


	/*
	   The special streams require some handling. The old locations are given by the register locations. New location is given
	   by the position/pointsize register variable. Old location is given by the symbol, which unlike the other symbols are not
	   present in the varying symbol table (got special entries). All we need to do is to ensure the remapping is done for them
	   as well. Otherwise, they might be remapped to random undefined locations, causing general mayhem and little sensible output.  */
	if( po->varying_position_symbol  && po->varying_position_symbol->location.vertex != -1)
	{
		for(i = 0; i < 4; i++)
		{
			varying_transformation_table[ po->varying_position_symbol->location.vertex + i ] = (po->vertex_pass.flags.vertex.position_register * 4) + i;
		}
	}
	if( po->varying_pointsize_symbol && po->varying_pointsize_symbol->location.vertex != -1)
	{
		varying_transformation_table[ po->varying_pointsize_symbol->location.vertex ] = (po->vertex_pass.flags.vertex.pointsize_register * 4) ;
	}

	/* call remap_varyings, which does the actual remapping in the program binaryr */
	err = remap_varyings(po, varying_transformation_table, varying_transformation_table_size);

	/* Update the locations of the varying positions in the symbols as well. This is done by setting all vertex
	   locations to their fragment location counterparts. Or, just do the lookup in the table we just created. */
	{
		u32 i;
		for(i = 0; i < po->varying_symbols->member_count; i++)
		{
			struct bs_symbol* symbol;
			symbol = po->varying_symbols->members[i];
			if(symbol->location.vertex == -1) continue;
			symbol->location.vertex = varying_transformation_table[symbol->location.vertex];
		}
	}
	if( po->varying_position_symbol  && po->varying_position_symbol->location.vertex != -1)
	{
		po->varying_position_symbol->location.vertex = varying_transformation_table[po->varying_position_symbol->location.vertex];
	}
	if( po->varying_pointsize_symbol && po->varying_pointsize_symbol->location.vertex != -1)
	{
		po->varying_pointsize_symbol->location.vertex = varying_transformation_table[po->varying_pointsize_symbol->location.vertex];
	}

	/* free the local transformation table */
	_mali_sys_free(varying_transformation_table);

	return err;
}

#if HARDWARE_ISSUE_4326

/**
 * @brief Rewrites the attributes of a vertex shader to avoid all stream corruption issues in revisions <= R0P3
 * Also updates your old remap table if you have one
 */
MALI_EXPORT mali_err_code __mali_gp2_rewrite_attributes_for_bug_4326( bs_program* po)
{
	/* Algorithm: Simple. Pad every other stream. This means multiplying all vertex locations with two
	 * The function may fail if
		 * Number of used streams > MALIGP2_MAX_VS_INPUT_REGISTERS / 2
		 * vector_element_stride % 4 != 0 and vecor_count * vector_element_stride > 4
		 * array_element_stride % 4 != 0 and arraysize * array_element_stride > 4
	 * In case of failures, the program object log will be updated
	 */

	int num_used_streams = 0;
	int a;
	for(a = 0; a < po->attribute_symbols->member_count; a++)
	{
		/* retrieve symbol stream usage */
		int largest_stream_used = -1;
		struct bs_symbol* symbol = po->attribute_symbols->members[a];
		int vector_count = 1;
		int array_count = 1;
		if(symbol == NULL) continue;

		largest_stream_used = (symbol->location.vertex + symbol->block_size.vertex) / 4;
		if(largest_stream_used > num_used_streams) num_used_streams = largest_stream_used;

		/* check that the symbol can be expanded */
		MALI_DEBUG_ASSERT(symbol->datatype != DATATYPE_STRUCT, ("Datatype struct should be caught earlier"));
		if(symbol->datatype == DATATYPE_MATRIX) vector_count = symbol->type.primary.vector_size;
		if( (symbol->type.primary.vector_stride.vertex % 4) != 0 && (vector_count * symbol->type.primary.vector_stride.vertex) > 4)
		{
			bs_set_error(&po->log, BS_ERR_LINK_TOO_MANY_ATTRIBUTES, "Mali200 HW revisions prior to R0P3 cannot use this program");
			return MALI_ERR_FUNCTION_FAILED;
		}
		if(symbol->array_size != 0) array_count = symbol->array_size;
		if( (symbol->array_element_stride.vertex % 4) != 0 && (array_count * symbol->array_element_stride.vertex) > 4)
		{
			bs_set_error(&po->log, BS_ERR_LINK_TOO_MANY_ATTRIBUTES, "Mali200 HW revisions prior to R0P3 cannot use this program");
			return MALI_ERR_FUNCTION_FAILED;
		}
	}

	/* are we using too many streams? */
	if(num_used_streams > MALIGP2_MAX_VS_INPUT_REGISTERS/2)
	{
		bs_set_error(&po->log, BS_ERR_LINK_TOO_MANY_ATTRIBUTES, "Mali200 HW revisions prior to R0P3 only support 8 attribute streams");
		return MALI_ERR_FUNCTION_FAILED;
	}

	/* and do the program rewrite */
	{
		mali_err_code err;
		int attrib_remap[MALIGP2_MAX_VS_INPUT_REGISTERS];
		for(a = 0; a < MALIGP2_MAX_VS_INPUT_REGISTERS/2; a++)
		{
			attrib_remap[a] = a*2;
		}
		for(a = MALIGP2_MAX_VS_INPUT_REGISTERS/2; a < MALIGP2_MAX_VS_INPUT_REGISTERS; a++)
		{
			attrib_remap[a] = -1;
		}
		err = _mali_gp2_link_attribs( po, attrib_remap, MALI_FALSE);
		if(err != MALI_ERR_NO_ERROR) return err;

	}
	return MALI_ERR_NO_ERROR;

}
#endif
