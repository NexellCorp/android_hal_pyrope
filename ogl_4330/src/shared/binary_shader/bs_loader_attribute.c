/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include <shared/binary_shader/bs_loader.h>
#include <shared/binary_shader/bs_loader_internal.h>
#include <shared/binary_shader/bs_symbol.h>
#include <shared/binary_shader/bs_error.h>

#include <regs/MALIGP2/mali_gp_core.h>

mali_err_code bs_setup_attribute_streams(struct bs_program* po)
{

	u32 i;
	u32 one_past_highest_stream_id_used = 0;
	MALI_DEBUG_ASSERT_POINTER(po);

	/* create stream table. Since this can be called multiple times, it need to clean up too. */
	po->attribute_streams.count = 0;
	if(po->attribute_streams.info)
	{
		_mali_sys_free(po->attribute_streams.info);
		po->attribute_streams.info = NULL;
	}
	po->attribute_streams.info = _mali_sys_malloc(sizeof(struct bs_attribute_stream_info) * MALIGP2_MAX_VS_INPUT_REGISTERS); /* large enough, might not use all */
	if( po->attribute_streams.info == NULL ) return MALI_ERR_OUT_OF_MEMORY;

	/* get number of streams */
	for(i = 0; i < po->attribute_symbols->member_count; i++)
	{
		u32 j;
		u32 symbol_size_in_cells = po->attribute_symbols->members[i]->block_size.vertex;
		u32 symbol_size_in_streams = (symbol_size_in_cells + 3 ) / 4;
		u32 symbol_stream_start =  (po->attribute_symbols->members[i]->location.vertex / 4);
		u32 symbol_one_past_highest_stream_used =  symbol_size_in_streams + symbol_stream_start;
		if(symbol_one_past_highest_stream_used>one_past_highest_stream_id_used) one_past_highest_stream_id_used = symbol_one_past_highest_stream_used;
		/* loop through all streams used by this symbol */
		for(j = symbol_stream_start; j < symbol_one_past_highest_stream_used; j++)
		{
			int k;
			mali_bool stream_already_used = MALI_FALSE;
			struct bs_attribute_stream_info* info = &po->attribute_streams.info[po->attribute_streams.count];
			/* assert that we do not have any other aliased symbol using this stream */
			for(k = 0; k <  (int)po->attribute_streams.count; k++)
			{
				MALI_DEBUG_ASSERT(k < MALIGP2_MAX_VS_INPUT_REGISTERS, ("using too many streams!"));
				if(po->attribute_streams.info[k].index == j)
				{
					stream_already_used = MALI_TRUE;
					break;
				}
			}
			if(stream_already_used) continue;
			/* no other symbol use this stream; add it */
			info->index = j;
			info->symbol =  po->attribute_symbols->members[i];
			po->attribute_streams.count++;
		}

	}

	/**
	 * Mali GP2 does not support 0 attribute streams. Attempts to pass 0 streams will
	 * be treated as if passing 16 attribute streams. In this case, make a dummy stream
	 */
	if(one_past_highest_stream_id_used == 0) one_past_highest_stream_id_used = 1;

	po->vertex_pass.flags.vertex.num_input_registers = (u32)one_past_highest_stream_id_used;

	return MALI_ERR_NO_ERROR;
}


MALI_STATIC mali_bool validate_attribute_values(struct bs_symbol *symbol, struct bs_symbol_table *name_comparison_set, int member_n)
{
	int i;
	switch(symbol->datatype)
	{
		case DATATYPE_MATRIX:
			/* matrix symbols must occupy subsequent streams */
			/* this is handled by vector_stride.vertex < 4 below */
		case DATATYPE_FLOAT:
		case DATATYPE_INT:
		case DATATYPE_BOOL:
			switch(symbol->type.primary.bit_precision.vertex) {
				case 16:
				case 24:
				case 32:
					break;
				default:
					return MALI_FALSE;
			}
			switch(symbol->type.primary.essl_precision.vertex) {
				case PRECISION_LOW:
				case PRECISION_MEDIUM:
				case PRECISION_HIGH:
					break;
				default:
					return MALI_FALSE;
			}
			switch(symbol->type.primary.invariant) {
				case MALI_FALSE:
				case MALI_TRUE:
					break;
				default:
					return MALI_FALSE;
			}
			if(symbol->type.primary.vector_size==0) return MALI_FALSE;
			if(symbol->type.primary.vector_stride.vertex==0 || symbol->type.primary.vector_stride.vertex > 4) return MALI_FALSE;
			break;
		/* attribute struct and samples not allowed */
		case DATATYPE_SAMPLER:
		case DATATYPE_SAMPLER_CUBE:
		case DATATYPE_SAMPLER_SHADOW:
		case DATATYPE_SAMPLER_EXTERNAL:
		case DATATYPE_STRUCT:
		default:
			return MALI_FALSE;
	}
	/*  The following fields is posed no validations on
	if(symbol->array_element_stride.vertex > XXXX) return MALI_FALSE;
	if(symbol->array_size == 0) return MALI_FALSE;
	if(symbol->block_size.vertex > XXXX) return MALI_FALSE;
	*/
	if(symbol->location.vertex < -1) return MALI_FALSE;
	if(symbol->location.vertex != -1 && ((symbol->location.vertex + symbol->block_size.vertex)) >= MALIGP2_MAX_VS_INPUT_REGISTERS*4) return MALI_FALSE; /* symbol exceeding HW limits */
	if(!(symbol->location.vertex %4 == 0 || symbol->location.vertex == -1)) return MALI_FALSE;
	for(i=0; i<member_n; ++i)
	{
		if(_mali_sys_strcmp(symbol->name, name_comparison_set->members[i]->name)==0) return MALI_FALSE;
	}
	return MALI_TRUE;
}

/**
 * @brief Checks all symbols in array; asserts that no symbols overlap and are within legal range values
 * @return MALI_TRUE if no overlap, MALI_FALSE if there is an overlap
 */

MALI_STATIC mali_bool validate_attribute_locations(struct bs_symbol_table *table)
{
	u32 i, j;
	int stream_used[MALIGP2_MAX_VS_INPUT_REGISTERS];

	for(i=0; i<MALIGP2_MAX_VS_INPUT_REGISTERS; ++i)
	{
		stream_used[i] = 0;
	}

	for(i = 0; i < table->member_count; i++)
	{
		struct bs_symbol* symbol = table->members[i];
		if(symbol == NULL) continue; /* skip holes */
		if(symbol->location.vertex == -1) continue; /* skip undefined symbols */
		for(j = 0; j < ((symbol->block_size.vertex+3)/4); j++)
		{
			int streamid = (symbol->location.vertex/4) + j;
			MALI_DEBUG_ASSERT(streamid < MALIGP2_MAX_VS_INPUT_REGISTERS, ("cannot use cells outside the legal range, should've been caught earlier"));
			if(stream_used[streamid] == 1) return MALI_FALSE;
			stream_used[streamid] = 1;
		}
	}
	return MALI_TRUE;
}

/**
 * @brief Loads a binary attribute symbol from a stream.
 * @param stream a streampointer. The stream pointer will be incremented as we read.
 * @return a binary attribute symbol.
 * @note The function is well-behaved when it comes to stream reading, you will always read an entire block
 * or nothing at all, regardless of whether the stream is okay. If there is not enough data for a complete block,
 * the stream is emptied.
 */
MALI_STATIC mali_err_code read_and_allocate_attribute_variable(struct bs_stream* stream, struct bs_symbol** output)
{
	u32 blocksize;
	u32 version;
	mali_err_code err;
	struct bs_symbol *retval = NULL;

	MALI_DEBUG_ASSERT_POINTER( stream );
	MALI_DEBUG_ASSERT_POINTER( output );

	/* read the block */
	blocksize = bs_read_or_skip_header(stream, VATT);
	if(blocksize == 0) return MALI_ERR_FUNCTION_FAILED; /* did not find VATT block */

	{
		char* buffer;
		/* load the symbol name into temp memory */
		err = bs_read_and_allocate_string(stream,  &buffer);
		if(err != MALI_ERR_NO_ERROR)
		{
			return err; /* bad stream */
		}

		/* allocate symbol (will copy the symbol name) */
		retval = bs_symbol_alloc(buffer);

		/* free the temp symbol name - no longer needed */
		_mali_sys_free(buffer);
		buffer = NULL;

		/* not enough memory */
		if( retval == NULL )
		{
			return MALI_ERR_OUT_OF_MEMORY;
		}
	}

	/* read and handle version */
	if(bs_stream_bytes_remaining(stream) < 1)
	{
		bs_symbol_free( retval );
		return MALI_ERR_FUNCTION_FAILED;
	}
	version = load_u8_value(stream);

	if( 0==version || 2==version ) /* someday, replace with a switch/case */
	{
		/* Type 0 require at least 15 more bytes. */
		if(bs_stream_bytes_remaining(stream) < 15)
		{
			/* If this triggers, the stream is bad */
			bs_symbol_free( retval );
			return MALI_ERR_FUNCTION_FAILED;
		}

		/* type */
		retval->datatype = (enum bs_datatype) load_u8_value(stream);

		if(retval->datatype != DATATYPE_STRUCT)
		{
			/* vector_size */
			retval->type.primary.vector_size = load_u16_value(stream);

			/* vector_aligned_size */
			retval->type.primary.vector_stride.vertex= load_u16_value(stream);
			retval->type.primary.vector_stride.fragment = retval->type.primary.vector_stride.vertex;
		} else {
			/* don't need these values */
			stream->position += 4;
		}

		/* array_size */
		retval->array_size = load_u16_value(stream);

		/* array_element_aligned_size */
		retval->array_element_stride.vertex = load_u16_value(stream);
		retval->array_element_stride.fragment = retval->array_element_stride.vertex;

		/* set block size */
		bs_update_symbol_block_size(retval);

		if(retval->datatype != DATATYPE_STRUCT)
		{
			/* precision */
			retval->type.primary.bit_precision.vertex = load_u8_value(stream);
			retval->type.primary.bit_precision.fragment = retval->type.primary.bit_precision.vertex;

			/* essl_precision */
			retval->type.primary.essl_precision.vertex = load_u8_value(stream);
			retval->type.primary.essl_precision.fragment = retval->type.primary.essl_precision.vertex;

			/* invariant */
			retval->type.primary.invariant = load_u8_value(stream);

			/* skip one byte */
			stream->position += 1;
		} else {
			/* don't need these values */
			stream->position += 4;
		}


		/* location */
		retval->location.vertex = load_s16_value(stream);
		retval->location.fragment = -1; /* attributes have no location in fragment shaders */

	} else { /* Illegal version! */
		/* cleanup */
		bs_symbol_free( retval );
		return MALI_ERR_FUNCTION_FAILED;
	}

	/* support for optional unknown blocks in later editions. All correctly declared blocks with allowed size will
	 * be skipped, but otherwise don't cause any error
	 */
	while(stream->position < stream->size)
	{
		enum blocktype blockname = bs_peek_header_name(stream);
		u32 size = 0;
		switch(blockname) {
			case NO_BLOCK: /* error while loading block */
				bs_symbol_free(retval);
				return MALI_ERR_FUNCTION_FAILED;
			default: /* ignore all correctly declared blocks */
				size = bs_read_or_skip_header(stream, blockname);
				stream->position += size;
				break;
		}
	}

	*output = retval;

	return MALI_ERR_NO_ERROR;
}

MALI_EXPORT mali_err_code __mali_binary_shader_load_attribute_table(bs_program *po, struct bs_stream* stream)
{

	struct bs_symbol_table* retval = NULL;
	u32 count;
	u32 num_read_symbols = 0;

	MALI_DEBUG_ASSERT_POINTER( po );
	MALI_DEBUG_ASSERT_POINTER( stream );
	MALI_DEBUG_ASSERT(po->attribute_symbols == NULL, ("Program Object must be init/resetted before calling this function"));

	/* read header */
	if(bs_stream_bytes_remaining(stream) != 0)
	{
		u32 size = bs_read_or_skip_header(stream, SATT);
		if( size < 4 )
		{	/* header was not found, stream is corrupt, or not enough data for the "count" variable loaded below - either case is bad */
			bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader attribute symbols are corrupt");
			return MALI_ERR_FUNCTION_FAILED;
		}
		count = load_u32_value(stream);
	} else {
		count = 0;
	}

       /* load attribute table */
	retval = bs_symbol_table_alloc(count);
	if(NULL == retval) return MALI_ERR_OUT_OF_MEMORY;

	while(bs_stream_bytes_remaining(stream) > 0)
	{
		struct bs_symbol* symbol = NULL;
		struct bs_stream attribute_stream;
		u32 blockname;
		mali_err_code err;
		err = bs_create_subblock_stream(stream, &attribute_stream);
		if(MALI_ERR_NO_ERROR != err)
		{
			bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader is invalid; corrupt SATT datastream detected");
			bs_symbol_table_free(retval);
			return err;
		}
		blockname = bs_peek_header_name(&attribute_stream);
		switch(blockname)
		{
			case VATT:
				if(num_read_symbols >= count)
				{
					bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader corrupt; mismatch between declared and found attributes");
					bs_symbol_table_free(retval);
					return MALI_ERR_FUNCTION_FAILED;
				}

				err = read_and_allocate_attribute_variable(&attribute_stream, &symbol);
				if(MALI_ERR_NO_ERROR != err)
				{
					bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex attribute symbols are corrupt");
					bs_symbol_table_free(retval);
					return err;
				}

				if( !validate_attribute_values(symbol, retval, num_read_symbols))
				{
					bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader attribute symbols are outside legal values");
					bs_symbol_table_free(retval);
					_mali_sys_free(symbol);
					return MALI_ERR_FUNCTION_FAILED;
				}
				retval->members[num_read_symbols] = symbol;
				++num_read_symbols;
				break;
			case NO_BLOCK:
				{
					bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR,  "Vertex shader attribute symbols are corrupt");
					bs_symbol_table_free(retval);
					return MALI_ERR_FUNCTION_FAILED;
				}
			default:
				/* properly declared (but unknown) blocks will be skipped without processing or errors */
				;
		}
	}

	/* ensure the correct number of symbols was loaded - otherwise faulty stream */
	if(num_read_symbols != count)
	{
		bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader attribute symbols are corrupt");
		bs_symbol_table_free(retval);
		return MALI_ERR_FUNCTION_FAILED;
	}
	if(num_read_symbols>0 && !validate_attribute_locations(retval) )
	{
		bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader attribute locations are corrupt");
		bs_symbol_table_free(retval);
		return MALI_ERR_FUNCTION_FAILED;
	}

	/* set retval to its rightful place in the program object */
	po->attribute_symbols = retval;

	/* setup the attribute streams */
	{
		mali_err_code err = bs_setup_attribute_streams(po);
		if(err != MALI_ERR_NO_ERROR) return err;
	}

	return MALI_ERR_NO_ERROR;

}

