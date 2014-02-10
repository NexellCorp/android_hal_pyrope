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
#include <regs/MALI200/mali200_core.h>

MALI_STATIC mali_bool validate_varying_values(struct bs_symbol *symbol, struct bs_symbol_table *name_comparison_set, int member_n)
{
	int i;
	switch(symbol->datatype)
	{
		case DATATYPE_MATRIX:
		case DATATYPE_FLOAT:
		case DATATYPE_INT:
		case DATATYPE_BOOL:
			switch(symbol->type.primary.bit_precision.vertex)
			{
				case 16:
				case 24:
				case 32:
					break;
				default:
					return MALI_FALSE;
			}
			switch(symbol->type.primary.essl_precision.vertex)
			{
				case PRECISION_LOW:
				case PRECISION_MEDIUM:
				case PRECISION_HIGH:
					break;
				default:
					return MALI_FALSE;
			}
			switch(symbol->type.primary.invariant)
			{
				case MALI_FALSE:
				case MALI_TRUE:
					break;
				default:
					return MALI_FALSE;
			}
			if(symbol->type.primary.vector_size==0) return MALI_FALSE;
			if(symbol->type.primary.vector_stride.vertex==0) return MALI_FALSE;
			break;
		/* varying struct and samples not allowed */
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
	for(i=0; i<member_n; ++i)
	{
		if(_mali_sys_strcmp(symbol->name, name_comparison_set->members[i]->name)==0) return MALI_FALSE;
	}
	return MALI_TRUE;
}

/**
 * @brief returns and allocates a varying variable
 * @param stream A streampointer. This will be incremented as we read.
 * @param output Returns a varying bs symbol.
 * @return a mali error code determining what occurred while loading this symbol
 * @note The function is well behaved, as it always reads a full block, or nothing at all, and will never
 * corrupt the stream.
 */
MALI_STATIC mali_err_code read_and_allocate_varying_variable(struct bs_stream* stream, struct bs_symbol** output)
{
	struct bs_symbol* retval = NULL;
	u32 size;
	u32 version;
	mali_err_code err;

	MALI_DEBUG_ASSERT_POINTER( stream );
	MALI_DEBUG_ASSERT_POINTER( output );
	*output = NULL;

	size = bs_read_or_skip_header(stream, VVAR);
	if(size == 0) return MALI_ERR_FUNCTION_FAILED;


	/* save the end of the block, any error will make us jump there. */

	{
		char* buffer;
		err = bs_read_and_allocate_string(stream, &buffer);
		if(err != MALI_ERR_NO_ERROR)
		{
			return err;
		}

		retval = bs_symbol_alloc(buffer);
		_mali_sys_free(buffer);
		buffer = NULL;
		if(!retval )
		{
			return MALI_ERR_OUT_OF_MEMORY; /* not enough memory */
		}
	}

	/* need at least 1 more byte in the stream to read version */
	if(bs_stream_bytes_remaining(stream) < 1)
	{
		bs_symbol_free(retval);
		return MALI_ERR_FUNCTION_FAILED;
	}

	/* version */
	version = load_u8_value(stream);

	if( 0==version || 2==version ) /* someday, replace with a switch/case */
	{
		/* Need at least 19 more bytes to read a version 0 varying */
		if(bs_stream_bytes_remaining(stream) < 19)
		{
			bs_symbol_free(retval);
			return MALI_ERR_FUNCTION_FAILED;
		}

		/* type */
		retval->datatype = (enum bs_datatype) load_u8_value(stream);


		if(retval->datatype != DATATYPE_STRUCT)
		{
			/* vector_size */
			retval->type.primary.vector_size = load_u16_value(stream);

			/* vector_aligned_size */
			retval->type.primary.vector_stride.vertex = load_u16_value(stream);
			retval->type.primary.vector_stride.fragment = retval->type.primary.vector_stride.vertex;
		} else {
			/* For structs, the vector size contains the number of children,
			 * and the stride is unused. Don't need either value, so let's just discard them */
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
		} else {
			/* don't need these values */
			stream->position += 3;
		}

		/* clamped */
		retval->extra.varying.clamped = load_u8_value(stream);

		/* skip two bytes */
		stream->position += 2;

		/* location */
		retval->location.vertex = load_s16_value(stream);
		retval->location.fragment = retval->location.vertex;

		/* attribute_copy*/
		retval->extra.varying.attribute_copy = load_s16_value(stream);


	} else {
		/* unknown/handled version */
		bs_symbol_free(retval);
		return MALI_ERR_FUNCTION_FAILED;

	}

	/* support for optional unknown blocks in later editions. All correctly declared blocks with allowed size will
	 * be skipped, but otherwise don't cause any error
	 */
	while(stream->position < stream->size)
	{
		enum blocktype blockname = bs_peek_header_name(stream);
		switch(blockname)
		{
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

/**
 * @brief setup varying streams.
 * @param po The program object where we want to set up the streams. Symbol tables must be loaded.
 */
MALI_STATIC mali_err_code __mali_binary_shader_setup_varying_streams(struct bs_program *po)
{
 	/* Set up the varying stream structures, traverse all symbols and setting up an accompanying stream. */
	u32 i;
	u32 max_stream_size = 1; /* used to align varying_streams.block_stride */
	u32 usage_table[MALI200_MAX_INPUT_STREAMS * 4]; /* create a temporary flagging table tracking which cells are in use */

	_mali_sys_memset(usage_table, 0, sizeof(u32)*MALI200_MAX_INPUT_STREAMS*4);

	MALI_DEBUG_ASSERT_POINTER( po );


	/* allocate and clear stream array */
	po->varying_streams.info = _mali_sys_malloc(sizeof(struct bs_varying_stream_info) * MALI200_MAX_INPUT_STREAMS);
	if(!po->varying_streams.info) return MALI_ERR_OUT_OF_MEMORY;
	_mali_sys_memset(po->varying_streams.info, 0, sizeof(struct bs_varying_stream_info) * MALI200_MAX_INPUT_STREAMS );

	po->varying_streams.count = 0;


	/* find number of streams used by traversing all symbols and flag all used cells */
	for(i = 0; i < po->varying_symbols->member_count; i++)
	{
		struct bs_symbol* symbol = po->varying_symbols->members[i];
		int c, y, x;
		int symbol_height = 1;

		MALI_DEBUG_ASSERT_POINTER(symbol);
		MALI_DEBUG_ASSERT(symbol->datatype != DATATYPE_STRUCT, ("struct varyings not allowed"));

		if( symbol->datatype == DATATYPE_MATRIX) symbol_height = symbol->type.primary.vector_size;

		for(c = 0; c < MAX(1, (int)symbol->array_size); c++)
		{
			int array_element_offset;
			if(symbol->location.fragment == -1) continue; /* ignore symbols not in fragment shader */
			array_element_offset = symbol->location.fragment + (c*symbol->array_element_stride.fragment);
			for(y = 0; y < symbol_height; y++)
			{
				int row_offset = array_element_offset + (y*symbol->type.primary.vector_stride.fragment);
				for(x = 0; x < (int)symbol->type.primary.vector_size; x++)
				{
					int cell_offset = row_offset + x;
					MALI_DEBUG_ASSERT(cell_offset < MALI200_MAX_INPUT_STREAMS*4, ("offset is wrong"));

					/* Once the bit precisions starts containing useful values, this if should be replaced with a
					 * usage_table[cell_offset] = MIN(symbol->type.primary.bit_precision.vertex, symbol->type.primary.bit_precision.fragment);
					 * For now, using the old calculation.
					 */
					if((symbol->type.primary.bit_precision.fragment > 16 && symbol->type.primary.essl_precision.vertex == PRECISION_HIGH) || ( MALI_TRUE == symbol->type.primary.invariant))
					{
						usage_table[cell_offset] = 32;
					} else {
						usage_table[cell_offset] = 16;
					}
				}
			}
		}
	}

	/* count streams, make streaminfo */
	po->varying_streams.count = 0;
	for(i = 0; i < MALI200_MAX_INPUT_STREAMS; i++)
	{
		struct bs_varying_stream_info *vsi = &po->varying_streams.info[i];

		/* if either of the last two components are used, this is a 4 component stream. 3 component streams are disabled */
		if( usage_table[(i*4)+3] > 0 || usage_table[(i*4)+2] > 0)
		{
			vsi->component_count = 4;
			po->varying_streams.count = i+1;

		/* else - if either of the first two components are used, this is a 2 component stream. 1 component streams are disabled */
		} else if( usage_table[(i*4)+1] > 0 || usage_table[(i*4)+0] > 0)
		{
			vsi->component_count = 2;
			po->varying_streams.count = i+1;
		}

		/* if this stream is in use, find out its bit depth. Equals the highest bit depth used. The compiler will align this */
		if( vsi->component_count > 0 )
		{
			int j;
			int highest = 0;
			for( j = 0; j < 4; j++)
			{
				highest = MAX(highest, (int)usage_table[(i*4)+j]);
			}

			if (highest < 32)
			{
				vsi->component_size = 2; /* use two bytes per component */
			} else {
				vsi->component_size = 4; /* use four bytes per component */
			}
		}
	}


	/* calculate stream offsets */
	if(po->varying_streams.count > 0)
	{
		u32 currsize = po->varying_streams.info[0].component_count * po->varying_streams.info[0].component_size;
		if(currsize < 8) currsize = 8;  /* M200 disallows streams of less than 8 bytes; pad if nescessary */
		po->varying_streams.info[0].offset = 0; /* varying stream 0 always begins at start of stream block */
		max_stream_size = MAX( max_stream_size, currsize );

		for(i = 1; i < po->varying_streams.count; i++)
		{
			struct bs_varying_stream_info *curr = &po->varying_streams.info[i];
			struct bs_varying_stream_info *prev = &po->varying_streams.info[i-1];
			u32 currsize = (curr->component_size * curr->component_count);
			u32 prevsize = (prev->component_size * prev->component_count);

			curr->offset = (prev->offset + prevsize + currsize - 1) & ~(currsize - 1);
			max_stream_size = MAX( max_stream_size, currsize );
		}
	}

	/* setup block stride */
	if(po->varying_streams.count > 0)
	{
		struct bs_varying_stream_info *fvs = &po->varying_streams.info[po->varying_streams.count-1];
		po->varying_streams.block_stride = ( (fvs->component_size * fvs->component_count) + fvs->offset + (max_stream_size-1) ) & ~(max_stream_size - 1);
	} else {
		po->varying_streams.block_stride = 0;
	}

#if HARDWARE_ISSUE_3805
	/* before M200 r0p3 there is an undocumented 128bit alignment-requirement on the stream stride in all cases. */
	po->varying_streams.block_stride = (po->varying_streams.block_stride + 15) & ~15;
#else
	/* Round stride up to the next multiple of 8 - there's only 4 bits space to encode it in the RSW */
	po->varying_streams.block_stride = (po->varying_streams.block_stride + 7) & ~7;

	if (max_stream_size == 4 * 4)
	{
		/*
		 * From the Mali200 TRM, Section 3.14.1, Subword 13:
		 * "The size of per-vertex varyings block field must be a multiple of two if there are any
		 *  4-component FP32 varyings present, otherwise these varyings are read incorrectly."
		 *
		 * So we round the stride up to the closest multiple of 16 (the stride should already be
		 * divided by 8 before setting -- see comment above)
		*/
		po->varying_streams.block_stride = (po->varying_streams.block_stride + 15) & ~15;
	}
#endif

	/* ensure that we set up legal streams */
	for(i = 0; i < po->varying_streams.count; i++)
	{
		struct bs_varying_stream_info* si = &(po->varying_streams.info[i]);
		if( si->component_count == 0 || si->component_count > 4 || si->component_size > 4 || si->component_size == 0 )
		{
			/* all streams must be legal. Illegal streams have weird component sizes or component_counts */
			bs_set_error(&po->log, BS_ERR_LINK_TOO_MANY_VARYINGS, "Inconsistent varying set detected, Fragment shader corrupt?");
			return MALI_ERR_FUNCTION_FAILED;
		}

	}

	return MALI_ERR_NO_ERROR;
}

/* allocated a symbol table based on the vertex_binary-data and the fragment_binary_data. */
MALI_EXPORT mali_err_code __mali_binary_shader_load_varying_table(bs_program *po, struct bs_stream* vertex_stream, struct bs_stream* fragment_stream)
{
	u32 fragment_vars, vertex_vars;
	u32 vertex_bytes_remaining;
	u32 fragment_bytes_remaining;
	u32 vars_loaded = 0;
	struct bs_symbol_table* retval = NULL;
	mali_err_code err;

	MALI_DEBUG_ASSERT_POINTER( po );

	/* load headers, or skip everything if no headers presented */
	if(bs_stream_bytes_remaining(vertex_stream) != 0)
	{
		/* guaranteed to be at least 8 bytes here (block is checked on compile). We can load this without any reservations. */
		vertex_bytes_remaining = bs_read_or_skip_header(vertex_stream, SVAR);
		if( vertex_bytes_remaining < 4)
		{
			bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader varying symbols are corrupt");
			return MALI_ERR_FUNCTION_FAILED;
		}
		vertex_vars = load_u32_value(vertex_stream);
	} else {
		vertex_bytes_remaining = 0;
		vertex_vars = 0;
	}
	if(bs_stream_bytes_remaining(fragment_stream) != 0)
	{
		/* guaranteed to be at least 8 bytes here (checked when compiled/loaded from binshader). We can load this without any reservations. */
		fragment_bytes_remaining = bs_read_or_skip_header(fragment_stream, SVAR);
		if( fragment_bytes_remaining < 4)
		{
			bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Fragment shader varying symbols are corrupt");
			return MALI_ERR_FUNCTION_FAILED;
		}
		fragment_vars= load_u32_value(fragment_stream);
	} else{
		fragment_bytes_remaining = 0;
		fragment_vars = 0;
	}

	/* allocate varying symbol table big enough to fit them all */
#if 0 && defined( HARDWARE_ISSUE_8690 )
	retval = bs_symbol_table_alloc( 0==vertex_vars ? vertex_vars + 1 : vertex_vars );
#else
	retval = bs_symbol_table_alloc( vertex_vars );
#endif
	if(!retval) return MALI_ERR_OUT_OF_MEMORY;

	/* load all vertex shader symbols */
	while(bs_stream_bytes_remaining(vertex_stream) > 0)
	{
		struct bs_symbol* symbol = NULL;
		struct bs_stream varying_stream;
		mali_err_code err;
		u32 blockname;
		err = bs_create_subblock_stream(vertex_stream, &varying_stream);
		if(MALI_ERR_NO_ERROR != err)
		{
			bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader is invalid; corrupt SVAR datastream detected");
			bs_symbol_table_free(retval);
			bs_symbol_free(symbol);
			return MALI_ERR_FUNCTION_FAILED;
		}

		blockname = bs_peek_header_name(&varying_stream);
		switch(blockname)
		{
			case VVAR:
				if( vars_loaded >= vertex_vars )
				{
					bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader invalid; mismatch between announced and found varyings");
					bs_symbol_table_free(retval);
					bs_symbol_free(symbol);
					return MALI_ERR_FUNCTION_FAILED;
				}

				err = read_and_allocate_varying_variable(&varying_stream, &symbol);

				if(err != MALI_ERR_NO_ERROR)
				{
					bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader is invalid; illegal VVAR block detected");
					bs_symbol_table_free(retval);
					bs_symbol_free(symbol);
					return MALI_ERR_FUNCTION_FAILED;
				}

				if( !validate_varying_values(symbol, retval, vars_loaded))
				{
					bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader corrupt; invalid varying values found");
					bs_symbol_table_free(retval);
					bs_symbol_free(symbol);
					return MALI_ERR_FUNCTION_FAILED;
				}
				/* assert the symbol adheres to the max number of GP streams limit by asserting the last cell in the symbol
				 * is within limits. We won't bother to check for aliased symbols here, that might be intentional someday */
				if(symbol->location.vertex + symbol->block_size.vertex > MALIGP2_MAX_VS_OUTPUT_REGISTERS * 4)
				{
					bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader varying symbol location exceeds hardware limit");
					bs_symbol_table_free(retval);
					bs_symbol_free(symbol);
					return MALI_ERR_FUNCTION_FAILED;
				}

				symbol->location.fragment = -1; /* we don't know where the fragment location is, flag it as "not found" */

				/* handle builtin symbols. These should never be added to the list */
				if(_mali_sys_strcmp(symbol->name, "gl_Position") == 0 )
				{
					bs_symbol_free(po->varying_position_symbol);  /* free old symbol if present. This prevents memleaks from malign shaders */
					po->varying_position_symbol = symbol;
					vertex_vars--;
					retval->member_count--;
					continue;
				}
				if(_mali_sys_strcmp(symbol->name, "gl_PointSize") == 0 )
				{
					bs_symbol_free(po->varying_pointsize_symbol);  /* Free old symbol if present */
					po->varying_pointsize_symbol = symbol;
					vertex_vars--;
					retval->member_count--;
					continue;
				}

				/* we accept this vertex symbol; go on and load the next symbol */
				retval->members[vars_loaded] = symbol;
				vars_loaded++;
				break;
			case NO_BLOCK:
				{
					bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader is invalid; corrupt SVAR block detected");
					bs_symbol_table_free(retval);
					bs_symbol_free(symbol);
					return MALI_ERR_FUNCTION_FAILED;
				}
			default:
				/* properly declared (but unknown) blocks will be elegantly skipped without processing or errors */
				;
		}
	}

	/* ensure we loaded as many vertex variables as we should've */
	if( vars_loaded != vertex_vars )
	{
		bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader invalid; contains less varyings than announced");
		bs_symbol_table_free(retval);
		return MALI_ERR_FUNCTION_FAILED;
	}

	/* next, load all fragment symbols; ensure that any overlaps are of the same type, and that all fragment symbols find a matching vertex symbol
	 * No validations of input symbol values not neccessary, since all fragment shader varyings, exist in the vertex shader and have already been verified */

	while(bs_stream_bytes_remaining(fragment_stream) > 0)
	{
		struct bs_symbol* symbol = NULL;
		struct bs_stream varying_stream;
		enum { NOT_FOUND, MATCH_IN_NAME_BUT_NOT_TYPE, MATCHED } matchstatus = NOT_FOUND;
		u32 i;
		u32 blockname;
		mali_err_code err;
		err = bs_create_subblock_stream(fragment_stream, &varying_stream);
		if(err != MALI_ERR_NO_ERROR)
		{
			bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Fragment shader is invalid; corrupt SVAR datastream detected");
			bs_symbol_table_free(retval);
			bs_symbol_free(symbol);
			return MALI_ERR_FUNCTION_FAILED;
		}

		blockname = bs_peek_header_name(&varying_stream);
		switch(blockname)
		{
			case VVAR:
				if(fragment_vars <= 0)
				{
					bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Fragment shader corrupt; mismatch between declared and found varyings");
					bs_symbol_table_free(retval);
					bs_symbol_free(symbol);
					return MALI_ERR_FUNCTION_FAILED;
				}

				err = read_and_allocate_varying_variable(&varying_stream, &symbol);

				if (MALI_ERR_NO_ERROR != err)
				{
					bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Fragment shader is invalid; illegal VVAR block detected");
					bs_symbol_table_free(retval);
					bs_symbol_free(symbol);
					return MALI_ERR_FUNCTION_FAILED;
				}

				/* check if symbol is already present */
				fragment_vars--;

				/* try to match this symbol up against a fragment varying */
				for(i = 0; i < vars_loaded; i++)
				{
					struct bs_symbol* listsymbol = NULL;

					listsymbol = retval->members[i];
					if(_mali_sys_strcmp(symbol->name, listsymbol->name) == 0)
					{
						#define BUFFERSIZE 512
						char buffer[BUFFERSIZE];
						mali_bool typeequal;
						typeequal = bs_symbol_type_compare(symbol, listsymbol, buffer, BUFFERSIZE);
						#undef BUFFERSIZE

						if(typeequal == MALI_FALSE)
						{
							/* we have a match in name, but not in type */
							matchstatus = MATCH_IN_NAME_BUT_NOT_TYPE;
							bs_set_program_link_error_type_mismatch_varying(po, buffer);
							bs_symbol_free(symbol);
							bs_symbol_table_free(retval);
							return MALI_ERR_FUNCTION_FAILED;
						} else {
							/* save new location/alignment in old symbol */
							matchstatus = MATCHED;
							bs_symbol_merge_shadertype_specifics(listsymbol, symbol);
						}
						break;
					}

				} /* end for each vertex symbol */

				if(symbol->location.fragment != -1 && symbol->location.fragment + symbol->block_size.fragment > MALI200_MAX_INPUT_STREAMS * 4)
				{
					bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Fragment shader varying symbol location exceeds hardware limit");
					bs_symbol_table_free(retval);
					bs_symbol_free(symbol);
					return MALI_ERR_FUNCTION_FAILED;
				}

				/* comparison of the fragment symbol is over. If we found a match, the match flag is MALI_TRUE.
				 * If not, we have a linker error because we have an unmatched fragment varying. Which must be handled. */
				if(matchstatus == NOT_FOUND)
				{
					bs_set_program_link_error_missing_vertex_shader_varying(po, symbol->name);
					bs_symbol_free(symbol);
					bs_symbol_table_free(retval);
					return MALI_ERR_FUNCTION_FAILED;
				}

				/* delete the newly loaded fragment symbol; we don't need it anymore */
				bs_symbol_free(symbol);
				symbol = NULL;

				/* if we found a perfect match, we can keep going with the linkage by restarting the loop */
				if(matchstatus == MATCHED ) continue;

				break;
			case NO_BLOCK:
				bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Fragment shader corrupt; damaged SVAR block detected");
				bs_symbol_table_free(retval);
				bs_symbol_free(symbol);
				return MALI_ERR_FUNCTION_FAILED;
			default:
				continue;
		}
	}
	if(fragment_vars != 0)
	{
		bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Fragment shader corrupt; mismatch between declared and found varyings");
		bs_symbol_table_free(retval);
		return MALI_ERR_FUNCTION_FAILED;
	}

#if 0 && defined( HARDWARE_ISSUE_8690 )
	if(vertex_vars == 0)
	{
		/**
		 * Inject a dummy 4 float varyng to avoid hitting the hardware issue
		 */
		struct bs_symbol* symbol = NULL;

		symbol = bs_symbol_alloc("?__mali_dummy_varying__");
		if(!symbol)
		{
			bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Linking error; run out of memory while setting the symbol table");
			bs_symbol_table_free(retval);
			bs_symbol_free(symbol);
			return MALI_ERR_OUT_OF_MEMORY; /* not enough memory */
		}

		symbol->datatype = DATATYPE_FLOAT;
		symbol->type.primary.bit_precision.vertex = 16;
		symbol->type.primary.bit_precision.fragment = 16;
		symbol->type.primary.essl_precision.vertex = PRECISION_HIGH;
		symbol->type.primary.essl_precision.fragment = PRECISION_HIGH;
		symbol->type.primary.invariant = MALI_FALSE;
		symbol->type.primary.vector_size = 4;
		symbol->type.primary.vector_stride.vertex = 4;
		symbol->type.primary.vector_stride.fragment = 4;
		symbol->array_element_stride.vertex = 4;
		symbol->array_element_stride.fragment = 4;
		symbol->array_size = 0;
		symbol->block_size.vertex = 4;
		symbol->block_size.fragment = 4;
		symbol->location.vertex = 0;
		symbol->location.fragment = 0;
		symbol->flags.varying.attribute_copy = -1;
		symbol->flags.varying.clamped = MALI_FALSE;

		if( !validate_varying_values(symbol, retval, vars_loaded))
		{
			bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader corrupt; invalid varying values found");
			bs_symbol_table_free(retval);
			bs_symbol_free(symbol);
			return MALI_ERR_FUNCTION_FAILED;
		}

		/*
		 * No need to assert that the symbol adheres to the max number of GP streams
		 * limit since we enter this only if there were no varyings
		 */

		/* we accept this vertex symbol; go on and load the next symbol */
		retval->members[vars_loaded] = symbol;
		retval->member_count++;
		vars_loaded++;
	}
#endif

	/* we have loaded a varying table. Rejoice */
	po->varying_symbols = retval;

	/* Setup the standard varying streams */
	err = __mali_binary_shader_setup_varying_streams(po);
	if(err != MALI_ERR_NO_ERROR) return err;


	/* The "special" special streams are supposed to be placed so that they follow the standard varying streams.
       This means they will start at register po->varying_streams.count. Let's just figure out WHERE
	   these streams are supposed to be placed. */
	po->vertex_pass.flags.vertex.num_output_registers = po->varying_streams.count;
	if(po->varying_position_symbol && po->varying_position_symbol->location.vertex >= 0)
	{
		po->vertex_pass.flags.vertex.position_register = po->vertex_pass.flags.vertex.num_output_registers;
		po->vertex_pass.flags.vertex.num_output_registers++;
	} else {
		po->vertex_pass.flags.vertex.position_register = -1;
	}
	if(po->varying_pointsize_symbol && po->varying_pointsize_symbol->location.vertex >= 0)
	{
		po->vertex_pass.flags.vertex.pointsize_register = po->vertex_pass.flags.vertex.num_output_registers;
		po->vertex_pass.flags.vertex.num_output_registers++;
	} else {
		po->vertex_pass.flags.vertex.pointsize_register = -1;
	}

	/* perform invariance checks */

	/* NOTE: The error check here is slightly relaxed. We allow all valid shaders,
	 * but we do not fail on shaders where gl_FragCoord and gl_PointCoord have
	 * mismatched invariance (described in detail below)
	 *
	 * Bugzilla 3252 covers this.
	 */
#if 0
		All varyings must match invariance on both inputs of the linker - already covered through symbol compare
		if gl_FragCoord.invariance == MALI_TRUE then gl_Position.invariance must be MALI_TRUE
		if gl_PointCoord.invariance == MALI_TRUE then gl_PointSize.invariance must be MALI_TRUE
		gl_FrontFacing.invariance can never be MALI_TRUE
		gl_FrontFacing.invariance must equal gl_Position.invariance

		Note that if either of the symbols compared does not exist, then the check will not be made. This way,
		gl_Position.invariance can be MALI_TRUE even if it must match gl_FrontFacing.invariance, which must be MALI_FALSE.
		All that is required is that the gl_FrontFacing symbol is not present.
#endif

	/* all done */
	return MALI_ERR_NO_ERROR;
}

