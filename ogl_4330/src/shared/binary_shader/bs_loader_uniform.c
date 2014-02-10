/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <shared/binary_shader/bs_loader.h>
#include <shared/binary_shader/bs_loader_internal.h>
#include <shared/binary_shader/bs_symbol.h>
#include <shared/binary_shader/bs_error.h>
#include <regs/MALIGP2/mali_gp_core.h>
#include <regs/MALI200/mali200_core.h>

MALI_STATIC mali_bool validate_uniform_values(struct bs_symbol *symbol, struct bs_symbol_table *name_comparison_set, int member_n, mali_bool fragment_shader)
{
	int i;
	switch(symbol->datatype)
	{
		case DATATYPE_MATRIX:
		case DATATYPE_FLOAT:
		case DATATYPE_INT:
		case DATATYPE_BOOL:
		case DATATYPE_SAMPLER:
		case DATATYPE_SAMPLER_CUBE:
		case DATATYPE_SAMPLER_SHADOW:
		case DATATYPE_SAMPLER_EXTERNAL:
			if(fragment_shader)
			{
				switch(symbol->type.primary.bit_precision.fragment)
				{
					case 16:
					case 24:
					case 32:
						break;
					default:
						return MALI_FALSE;
				}
				switch(symbol->type.primary.essl_precision.fragment)
				{
					case PRECISION_LOW:
					case PRECISION_MEDIUM:
					case PRECISION_HIGH:
						break;
					default:
						return MALI_FALSE;
				}
				switch(symbol->type.primary.bit_precision.fragment)
				{
					case 16:
					case 24:
					case 32:
						break;
					default:
 					return MALI_FALSE;
				}
				if(symbol->type.primary.vector_stride.fragment==0) return MALI_FALSE;
			}
			else /* vertex shader */
			{
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
				switch(symbol->type.primary.bit_precision.vertex)
				{
					case 16:
					case 24:
					case 32:
						break;
					default:
 					return MALI_FALSE;
				}
				if(symbol->type.primary.vector_stride.vertex==0) return MALI_FALSE;
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
			break;
		case DATATYPE_STRUCT:
			for(i=0; i<(int)symbol->type.construct.member_count; ++i)
			{
				if(!validate_uniform_values(symbol->type.construct.members[i], &(symbol->type.construct), i, fragment_shader)) return MALI_FALSE;
			}
			break;
		default:
			return MALI_FALSE;
	}
	/**
	 * The following fields is posed no validations on
	 * if(symbol->array_element_stride.vertex > XXXX) return MALI_FALSE;
	 * if(symbol->array_size == 0) return MALI_FALSE;
	 * if(symbol->block_size.vertex > XXXX) return MALI_FALSE;
	 */
	if(symbol->location.vertex < -1) return MALI_FALSE;
	for(i=0; i<member_n; ++i)
	{
		if(_mali_sys_strcmp(symbol->name, name_comparison_set->members[i]->name)==0) return MALI_FALSE;
	}
	return MALI_TRUE;
}

/**
 * @brief Retrieves all sampler info from the uniform sybols symbol table
 * @param po The program object
 * @return whether the operation succeeded or not
 * @note This function creates all sampler handles as needed by the program object.
 *       Function also handles the redirection so that all toplevel samplers are placed at their specified locations.
 */
MALI_STATIC mali_err_code bs_loader_cache_sampler_info(struct bs_program* po)
{
	u32 i;
	u32 samplercount = 0;
	s32 yuv_coefficients_fs_location = -1;
	s32 texture_size_fs_location = -1;

	MALI_DEBUG_ASSERT_POINTER(po);

	samplercount = bs_symbol_count_samplers( po->uniform_symbols );

	bs_symbol_lookup(po->uniform_symbols, "gl_mali_YUVCoefficients", NULL, &yuv_coefficients_fs_location, NULL);
	bs_symbol_lookup(po->uniform_symbols, "gl_mali_textureGRADEXT_sizes", NULL, &texture_size_fs_location , NULL);

	po->samplers.num_samplers = 0;

	if ( samplercount == 0 ) return MALI_ERR_NO_ERROR; /* early out if no samplers used */

	if ( samplercount > 1024 )
	{
		/* check excessive number of samplers,using too many may trigger bugzilla 5476, so we limit it to a low number */
		bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Current Mali driver support no more than 1024 samplers");
		return MALI_ERR_FUNCTION_FAILED;
	}

	po->samplers.info = _mali_sys_malloc(samplercount * sizeof(struct bs_sampler_info));
	if(!po->samplers.info) return MALI_ERR_OUT_OF_MEMORY;

	po->samplers.num_samplers = samplercount;

	_mali_sys_memset( po->samplers.info, 0, samplercount * sizeof(struct bs_sampler_info) ); /* null the info structure */

	/* extract all sampler info, put in sampler array */
	for(i = 0; i < samplercount; i++)
	{
		struct bs_sampler_info* sampler = &po->samplers.info[i];
		mali_bool optimized;
		int samplersize = 1;

		sampler->symbol = bs_symbol_get_nth_sampler(po->uniform_symbols, i, &sampler->fragment_location, &optimized);

		MALI_DEBUG_ASSERT_POINTER( sampler->symbol ); /* symbol is always valid, or getnumsamplers lied! */
		MALI_DEBUG_ASSERT( bs_symbol_is_sampler( sampler->symbol), ("should be a sampler") ); /* symbol is also a sampler */

		/* YUV samplers require special handling */
		if(sampler->symbol->datatype == DATATYPE_SAMPLER_EXTERNAL)
		{
			if(yuv_coefficients_fs_location != -1) 
			{
				samplersize = 3; /* YUV samplers require 3 slots in the TD remap table */
				sampler->YUV = MALI_TRUE;

				sampler->YUV_metadata_index = yuv_coefficients_fs_location + 4*sampler->symbol->extra.uniform.external_index;	/* 4 float4 */
			}
			/* else we are using SAMPLER_EXTERNAL for RGB images in GLES1 */
		}

		/* Handle the support for grad sampling functions for texture lod extension */
		sampler->is_grad = MALI_FALSE;
		if( -1 != texture_size_fs_location )
		{
			if(	( sampler->symbol->datatype == DATATYPE_SAMPLER) || 
				( sampler->symbol->datatype == DATATYPE_SAMPLER_CUBE ) )
			{
				sampler->is_grad = MALI_TRUE;
				sampler->grad_metadata_index = texture_size_fs_location + sampler->symbol->extra.uniform.grad_index;	/* single float4 */
			}
			else
			{
				/* TODO: we need a defect test to check that we don't need to pass the texture size for SAMPLER_SHADOW and SAMPLER_EXTERNAL */
			}
		}

		/* Figure out if the sampler is optimized. Optimized samplers will be removed from the fragment uniform array
		 * Eventually we should handle this differently, but doing so will require a binshader format change. */
		/* only non-optimized samplers have a fragment location */
		if( MALI_TRUE == optimized )
		{
			sampler->fragment_location = -1;
		}

		/* And increase size of remap table. Each sampler increase remap table size by one. YUV by 3. */
		po->samplers.td_remap_table_size += samplersize;

		sampler->API_value = 0;
	}

	/**
	 * Right now, the samplers are all laid out nicely, but YUV samplers require some stricter rules.
	 * So, reshuffle the sampler TD remap table indices so that the samplers appear in this order:
	 * 1: optimized normal samplers
	 * 2: optimzied YUV samplers
	 * 3: nonoptimizeed YUV samplers
	 * 4: nonoptimized normal samplers
	 */
	{
		int td_index = 0;
		int yuv_metadata_index = 0;

		/* optimized normal samplers */
		for ( i = 0; i < po->samplers.num_samplers; i++ )
		{
			struct bs_sampler_info* sampler = &po->samplers.info[ i ];
			if ( -1 == sampler->fragment_location && sampler->YUV == MALI_FALSE )
			{
				sampler->td_remap_table_index = td_index;
				td_index += 1;
			}
		}
		/* optimized YUV samplers */
		for ( i = 0; i < po->samplers.num_samplers; i++ )
		{
			struct bs_sampler_info* sampler = &po->samplers.info[ i ];
			if ( -1 == sampler->fragment_location && sampler->YUV == MALI_TRUE )
			{
				sampler->td_remap_table_index = td_index;
				td_index += 3;
				sampler->YUV_metadata_index = yuv_coefficients_fs_location + 16*yuv_metadata_index; /* 16 cells per uniform*/
				yuv_metadata_index++;
			}
		}
		/* nonoptimized YUV samplers */
		for ( i = 0; i < po->samplers.num_samplers; i++ )
		{
			struct bs_sampler_info* sampler = &po->samplers.info[ i ];
			if ( -1 != sampler->fragment_location && sampler->YUV == MALI_TRUE )
			{
				sampler->td_remap_table_index = td_index;
				td_index += 3;
				sampler->YUV_metadata_index = yuv_coefficients_fs_location + 16*yuv_metadata_index; /* 16 cells per uniform*/
				yuv_metadata_index++;
				/* write to the uniform table; this value will never be changed again */
				MALI_DEBUG_ASSERT_POINTER(po->fragment_shader_uniforms.array);
				MALI_DEBUG_ASSERT(sampler->fragment_location < (s32)po->fragment_shader_uniforms.cellsize,
				                  ("location > array size"));
				po->fragment_shader_uniforms.array[sampler->fragment_location] = (float)sampler->td_remap_table_index;
			}
		}
		/* nonoptimized normal samplers */
		for ( i = 0; i < po->samplers.num_samplers; i++ )
		{
			struct bs_sampler_info* sampler = &po->samplers.info[ i ];
			if ( -1 != sampler->fragment_location && sampler->YUV == MALI_FALSE )
			{
				sampler->td_remap_table_index = td_index;
				td_index += 1;
				/* write to the uniform table; this value will never be changed again */
				MALI_DEBUG_ASSERT_POINTER(po->fragment_shader_uniforms.array);
				MALI_DEBUG_ASSERT(sampler->fragment_location < (s32)po->fragment_shader_uniforms.cellsize,
				                  ("location > array size"));
				po->fragment_shader_uniforms.array[sampler->fragment_location] = (float)sampler->td_remap_table_index;
			}
		}
	}

	return MALI_ERR_NO_ERROR;
}

/**
 * @brief Reads a VINI block, using the data from the symbol and fills in the uniform array with the read values
 * @param retval The return value; a uniform bs_symbol
 * @param uniform_array The corresponding uniform array for this symbol; will be used to save initializers.
 * @param binary_data A pointer to a streampointer, this will be incremented as we load data
 * @param bytes_remaining A pointer to a streamsize, this will be decremented as we load data.
 * @return MALI_ERR_NO_ERROR or MALI_ERR_FUNCTION_FAILED.
 * @note The function is well-behaved, ie it always loads a complete block or nothing at all; we can never leave the stream hanging in mid-air.
 */
MALI_STATIC mali_err_code read_uniform_initializer(struct bs_symbol* retval, void* uniform_array, int uniform_array_size, struct bs_stream* stream)
{
	u32 size;
	u32 array_count, height, width, number_of_initializers;
	u32 i, x, y;

	size = bs_read_or_skip_header(stream, VINI);
	if(size < 4)
	{	/* need at least 4 bytes to read the size header */
		/* not enough bytes to read the size header. Bad block, probably. Ignore it and skip the block */
		stream->position += size;
		return MALI_ERR_FUNCTION_FAILED;
	}

	/* read header */
	number_of_initializers = load_u32_value(stream);

	/* ensure we have enough data for all initializers */
	if( size - 4 < number_of_initializers * sizeof(float))
	{
		/* just jump to the end of the stream - loading failed */
		stream->position = stream->size;
		return MALI_ERR_FUNCTION_FAILED;
	}


	/* if STRUCT, initializers are not alowed. Ignore block */
	if(retval->datatype == DATATYPE_STRUCT)
	{
		stream->position += number_of_initializers * sizeof(float);

		return MALI_ERR_FUNCTION_FAILED;
	}

	/* setup dimensions */
	array_count = retval->array_size;
	if(!array_count) array_count = 1;
	width = retval->type.primary.vector_size;
	height = 1;
	if(retval->datatype == DATATYPE_MATRIX)
	{
		height = width;
	}

	/* assert dimensions */
	if(  number_of_initializers != width*height*array_count)
	{ /* incorrect number of initializers or space left for initializers */
		stream->position += number_of_initializers * sizeof(float);
		return MALI_ERR_FUNCTION_FAILED;
	}

	/* load initializers */
	for(i = 0; i < array_count; i++)
	{ /* for every array element */
		for(y = 0; y < height; y++)
		{ /* for every row in element */
			for(x = 0; x < width; x++)
			{ /* for every cell in row */
				/**
				 * Note: We are using the fragment stride, because we do not know which type of symbol this is,
				 * and both strides are equal and set to whatever the symbol was defined as. This will be altered
				 * further down in the loader, but at this stage, the fragment stride is good.
				 */
				s32 pos = (i*retval->array_element_stride.fragment) + (retval->type.primary.vector_stride.fragment*y) + x + retval->location.fragment;
				if(pos < uniform_array_size)
				{
					float value = load_float_value(stream);
					((float*)uniform_array)[pos] = value;
				} else {
					/* skip data */
					stream->position += 4;
				}
			}
		}
	}
	return MALI_ERR_NO_ERROR;
}

/**
 * @brief Reads and allocates one uniform variable.
 * @param binary_data Pointer to the stream pointer
 * @param bytes_remaining pointer to stream size counter
 * @param out_symbol output bs_symbol of the uniform variable loaded.
 * @param parent output parameter; The parent of this uniform.
 * @param uniform_array The array of uniform VALUES - for initializers
 * @param uniform_array_size The size of the unifrom value array
 * @return a mali error code depicting how the loading went.
 * @note This function does not really load structs perfectly; only single variables.
 *       Structs must be manually assembled once all uniform variables are loaded.
 *       Which is what the parent parameter is for.
 *       So don't expect struct type symbols to be complete until that is done.
 * @note Only top level nodes (parent == -1) can have initializers. The initializers apply for all subnodes if present.
 */

MALI_STATIC mali_err_code read_and_allocate_uniform_variable(struct bs_stream* stream, struct bs_symbol** output,
		s32* parent, void* uniform_array, int uniform_array_size,
		mali_bool from_fragment_shader)
{
	struct bs_symbol* retval = NULL;
	u32 size;
	u32 version;
	mali_err_code err;
	u32 next_block_position;

	MALI_DEBUG_ASSERT_POINTER( stream );
	MALI_DEBUG_ASSERT_POINTER( output );
	MALI_DEBUG_ASSERT_POINTER( parent );
	MALI_DEBUG_ASSERT( uniform_array_size == 0 || uniform_array != NULL, ("If there are uniforms, the uniform array must be legal") );

	size = bs_read_or_skip_header(stream, VUNI);
	if(size == 0) return MALI_ERR_FUNCTION_FAILED;

	/* save next block position, no matter what happens in this function the next function will read from the next block */
	next_block_position = stream->position + size;
	{
		char* buffer;

		/* read string block */
		err = bs_read_and_allocate_string(stream, &buffer);
		if(err != MALI_ERR_NO_ERROR)
		{
			stream->position = next_block_position;
			return err;
		}

		/* allocate symbol */
		retval = bs_symbol_alloc(buffer);
		_mali_sys_free(buffer);
		buffer = NULL;
		if(!retval)
		{
			stream->position = next_block_position;
			return MALI_ERR_OUT_OF_MEMORY; /* not enough memory */
		}
	}

	/* after loading the string, the rest of the block is numbers. These numbers require a fixed
	 * size of data loaded from the stream. So We'll just update the size to the remaining blocksize */
	size = next_block_position - stream->position;

	/* version - requires at least 1 byte */
	if(size < 1)
	{
		bs_symbol_free(retval);
		stream->position = next_block_position;
		return MALI_ERR_FUNCTION_FAILED;
	}
	version = load_u8_value(stream);

	if( 0==version || 2==version ) /* someday, replace with a switch/case */
	{
		/* version 0 data require 20 bytes datasize (plus more for initializers)*/
		if(size < 20)
		{
			bs_symbol_free(retval);
			stream->position = next_block_position;
			return MALI_ERR_FUNCTION_FAILED;
		}

		/* type */
		retval->datatype = (enum bs_datatype) load_u8_value(stream);

		if(retval->datatype != DATATYPE_STRUCT)
		{
			/* primary datatypes can be read directly */

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
		retval->array_size= load_u16_value(stream);

		/* array_aligned_size */
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

			/* skip three bytes */
			stream->position += 3;
		} else {
			/* structs don't need either of these values */
			stream->position += 6;
		}
		/* location */
		retval->location.vertex = load_s16_value(stream);
		retval->location.fragment = retval->location.vertex;

		/* parent */
		*parent = load_s16_value(stream);

		/* adjust alignments for non-structmember non-struct non-matrix non-sampler non-array symbols.
		 * This is to accomodate that symbols placed in the end of the uniform block do not
		 * overlap the end of the uniform array, triggering the illegal shader error.
		 * Compiler integration issue, because the compiler cannot give us the correct minimal alignments,
		 * and the linker sorta require them. This is the best solution, TBH.  */
		if( *parent == -1 && retval->array_size == 0 &&
				( retval->datatype == DATATYPE_FLOAT ||
				  retval->datatype == DATATYPE_INT ||
				  retval->datatype == DATATYPE_BOOL ) )
		{
			retval->type.primary.vector_stride.vertex = retval->type.primary.vector_size;
			retval->type.primary.vector_stride.fragment = retval->type.primary.vector_size;
			retval->array_element_stride.vertex = retval->type.primary.vector_size;
			retval->array_element_stride.fragment = retval->type.primary.vector_size;
			bs_update_symbol_block_size(retval);
		}

		/* Negative locations will give segmentation fault during reading of the initializers */
		if(retval->location.vertex < -1)
		{
			bs_symbol_free(retval);
			return MALI_ERR_FUNCTION_FAILED;
		}

		/* support for later additions of any optional blocks. Currently only VINI and VIDX blocks are handled, all other blocks
		 * are ignored if correctly declared with header and size.
		 */
		while(stream->position < next_block_position)
		{
			enum blocktype blockname = bs_peek_header_name(stream);
			switch(blockname)
			{
				case VINI:
					/* read initializers. Only legal if parent == -1 */
					if(*parent == -1)
					{	/* initializer present, read and set */
						err = read_uniform_initializer(retval, uniform_array, uniform_array_size, stream);
						if(err != MALI_ERR_NO_ERROR)
						{
							bs_symbol_free(retval);
							stream->position = next_block_position;
							return err;
						}
					}
					else
					{
						/* cleanup */
						bs_symbol_free(retval);
						stream->position = next_block_position;
						return MALI_ERR_FUNCTION_FAILED;
					}
					break;
				case VIDX:
					if (from_fragment_shader)
					{
						size = bs_read_or_skip_header(stream, blockname);
						if((3*sizeof(u32))==size)
						{
							retval->extra.uniform.td_index = load_u32_value(stream);
							retval->extra.uniform.external_index = load_u32_value(stream);
							retval->extra.uniform.grad_index = load_u32_value(stream);
/*_mali_sys_printf("VIDX block found in a fragment shader ld_remap=%d, yuv=%d, grad=%d\n", retval->extra.uniform.td_index, retval->extra.uniform.external_index, retval->extra.uniform.grad_index);*/
						}
						else
						{
						/* TODO: this piece of code should never be hit! */
							stream->position += size;
						}
					}
					else
					{
						/* TODO: this piece of code should never be hit, but right now it is hit a lot! */
						size = bs_read_or_skip_header(stream, blockname);
						stream->position += size;
					}
					break;
				case NO_BLOCK: /* error while loading block */
					bs_symbol_free(retval);
					stream->position = next_block_position;
					return MALI_ERR_FUNCTION_FAILED;
				default: /* ignore all correctly declared blocks */
					size = bs_read_or_skip_header(stream, blockname);
					stream->position += size;
					break;
			}
		}
	}
	else
	{ /* Illegal version! */
		/* cleanup */
		bs_symbol_free(retval);
		stream->position = next_block_position;
		return MALI_ERR_FUNCTION_FAILED;
	}

	*output = retval;
	return MALI_ERR_NO_ERROR;
}

/* struct for sorting an array of strings */
struct sortable {
	u32 index;
	const char *str;
} ;

/* function for sorting the sortable struct with qsort */
MALI_STATIC int sort_sortable_struct(const void *a, const void *b)
{
	const struct sortable *x = a;
	const struct sortable *y = b;
	return _mali_sys_strcmp(x->str, y->str);
}

/**
 * @brief function for building a uniform tree out of a list of symbols and a parent index/graph table
 * @param temp_array A list of symbols. This function NULLs the elements in this array as they are placed in the symbol table.
 *               This allows for easy check of which symbols that were left at the end of the day.
 * @param temp_size Size (in elements) of the temp array. Must be at least 1.
 * @param temp_parent An array of parent relations. If node 4 has parent 3, temp_parent[4] = 3. Elements with parent -1 are top level nodes.
 * @param current index The index that we are currently examining. Starts off at -1. For each recursive call, set to the index of the struct node
 *               we are adding subnodes to.
 * @param output This is the symbolic table that we are adding nodes to. On recursive calls, this is the symbol we are adding symbols to.
 *
 * */

MALI_STATIC mali_err_code recursively_build_uniform_tree(
		struct bs_symbol** temp_array,
		u32 temp_size,
		s32* temp_parent,
		s32 current_index,
		struct bs_symbol_table* output)
{
	u32 i, j;
	struct sortable *temp_sort;

	MALI_DEBUG_ASSERT_POINTER( temp_array );
	MALI_DEBUG_ASSERT_POINTER( output );

	/* These two cannot hit through maligned data, only driver use errors.*/
	MALI_DEBUG_ASSERT(current_index >= -1, ("Index must be >= -1"));
	MALI_DEBUG_ASSERT(current_index < 0 || (u32)current_index < temp_size, ("Index must be < temp_size"));
	MALI_DEBUG_ASSERT(temp_size > 0, ("temp_size should be at least one"));

	/* indexes beyond level -1 are to be removed from the temp array */
	if ( current_index > -1 )
	{
		temp_array[current_index] = NULL; /* flag node as traversed */
	}

	/* locate the number of symbols on this level */
	output->member_count = 0;
	for(i = 0; i < temp_size; i++)
	{
		if(temp_parent[i] == current_index)	output->member_count++;;
	}

	/* create an array big enough to hold all uniforms */
	output->members = _mali_sys_malloc( sizeof(struct bs_symbol*) * output->member_count );
	temp_sort = _mali_sys_malloc( sizeof(struct sortable) * output->member_count );
	if ( NULL == output->members || NULL == temp_sort)
	{
		if ( NULL != output->members )
		{
			_mali_sys_free( output->members );
			output->members = NULL;
		}
		if ( NULL != temp_sort )
		{
			_mali_sys_free( temp_sort );
		}
		output->member_count = 0;
		return MALI_ERR_OUT_OF_MEMORY;
	}
	_mali_sys_memset(output->members, 0, sizeof(struct bs_symbol*) * output->member_count );

	/* Sort all symbols on current level */
	j = 0;
	for(i = 0; i < temp_size ; i++)
	{
		if(temp_parent[i] == current_index)
		{
			temp_sort[j].index = i;
			temp_sort[j].str   = temp_array[i]->name;
			j++;
		}
	}
	_mali_sys_qsort(temp_sort, output->member_count, sizeof(struct sortable), sort_sortable_struct);

	/* walk through temp_sort, insert each element at the correct location */
	for(i = 0; i < output->member_count; i++)
	{
		j = temp_sort[i].index;
		output->members[i] = temp_array[j];

		/* recursively handle all struct nodes */
		if(temp_array[j]->datatype == DATATYPE_STRUCT)
		{
			mali_err_code error_code;
			error_code = recursively_build_uniform_tree(temp_array, temp_size, temp_parent,
														j, &temp_array[j]->type.construct);
			if(error_code != MALI_ERR_NO_ERROR)
			{
				_mali_sys_free(temp_sort);
				return error_code;
			}
		}

		/* remove from temp array */
		temp_array[j] = NULL;

	}
	_mali_sys_free(temp_sort);

	/* and we're done! No errors, hurrah! */
	return  MALI_ERR_NO_ERROR;
}

/* load a uniform table based on the two input data chunks */
MALI_EXPORT mali_err_code __mali_binary_shader_load_uniform_table(bs_program *po, struct bs_stream* vertex_stream, struct bs_stream* fragment_stream)
{
	u32 vertex_vars, fragment_vars, num_loaded_vars = 0;
	u32 vertex_size, fragment_size;
	u32 vertex_bytes_remaining ;
	u32 fragment_bytes_remaining ;

	/* these arrays hold the flat structure loaded symbols */
	struct bs_symbol_table *vertex_temp_table=NULL;
	struct bs_symbol_table *fragment_temp_table=NULL;
	s32                    *vertex_parents = NULL;
	s32                    *fragment_parents = NULL;

	/* these arrays hold the threaded structure loaded symbols */
	struct bs_symbol_table* temp_vertex_tree = NULL;
	struct bs_symbol_table* temp_fragment_tree = NULL;

	/* this array hold the merged structure loaded symbols */
	struct bs_symbol_table *retval=NULL;

	mali_err_code error_code;
	u32 i,x;

	MALI_DEBUG_ASSERT_POINTER( po );

	/**************************** INITIALIZE STREAMS **********************/

	/* read SUNI block from vertex data */
	if(bs_stream_bytes_remaining(vertex_stream) != 0)
	{
		vertex_bytes_remaining = bs_read_or_skip_header(vertex_stream, SUNI);
		if(vertex_bytes_remaining < 8 )
		{	/* not enough room for headers */
			bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader uniform data are corrupt.");
			return MALI_ERR_FUNCTION_FAILED;
		}
		vertex_vars = load_u32_value(vertex_stream);
		vertex_size = load_u32_value(vertex_stream);

		/* assert vertex_size is within legal range for the MaliGP2 */
		if( vertex_size >= MALIGP2_MAX_VS_CONSTANT_REGISTERS*4 )
		{
			int buffersize = 1000; /* big enough for this temp buffer */
			char* buffer = _mali_sys_malloc(buffersize);
			if(buffer == NULL)
			{
				bs_set_error_out_of_memory(&po->log);
				return MALI_ERR_OUT_OF_MEMORY;
			}
			_mali_sys_snprintf(buffer, buffersize,
								"Too many uniforms used in vertex shader. Used %i uniform registers, but HW limit is %i",
								vertex_size,
								MALIGP2_MAX_VS_CONSTANT_REGISTERS );
			bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, buffer);
			_mali_sys_free(buffer);
			return MALI_ERR_FUNCTION_FAILED;
		}

	} else
	{
		vertex_bytes_remaining = 0;
		vertex_vars = 0;
		vertex_size = 0;
	}

	/* read SUNI block from fragment data */
	if(bs_stream_bytes_remaining(fragment_stream) != 0)
	{
		fragment_bytes_remaining = bs_read_or_skip_header(fragment_stream, SUNI);
		if(fragment_bytes_remaining < 8 )
		{	/* not enough room for headers */
			bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Fragment shader uniform data are corrupt.");
			return MALI_ERR_FUNCTION_FAILED;
		}
		fragment_vars = load_u32_value(fragment_stream);
		fragment_size = load_u32_value(fragment_stream);

		/* assert fragment_size is within legal range. We do not check that it is a power of two; that's for the backends to handle */
		if( fragment_size >= MALI200_MAX_UNIFORM_BLOCKSIZE  )
		{
			int buffersize = 1000; /* big enough for this tempbuffer */
			char* buffer = _mali_sys_malloc(buffersize);
			if(buffer == NULL)
			{
				bs_set_error_out_of_memory(&po->log);
				return MALI_ERR_OUT_OF_MEMORY;
			}
			_mali_sys_snprintf(buffer, buffersize, "Fragment shader corrupt. Uniform table is sized %i, must be no larger than %i.",
			                   fragment_size, MALI200_MAX_UNIFORM_BLOCKSIZE);
			bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, buffer);
			_mali_sys_free(buffer);
			return MALI_ERR_FUNCTION_FAILED;
		}
	} else
	{
		fragment_bytes_remaining = 0;
		fragment_vars = 0;
		fragment_size = 0;
	}

	/**************************** ALLOCATE FLAT STRUCTURE WORKING TABLES **********************/

	/* allocate uniform data arrays */
	if ( vertex_size > 0 )
	{
		po->vertex_shader_uniforms.array = _mali_sys_malloc( vertex_size * sizeof(float) );
		if ( NULL == po->vertex_shader_uniforms.array )
		{ /* no freeing, we are handling memleaks through the reset command */
			return MALI_ERR_OUT_OF_MEMORY;
		}
		_mali_sys_memset(po->vertex_shader_uniforms.array, 0, vertex_size *  sizeof(float) );
	}

	if ( fragment_size > 0 )
	{
		po->fragment_shader_uniforms.array = _mali_sys_malloc( fragment_size * sizeof(float) );
		if ( NULL == po->fragment_shader_uniforms.array )
		{ /* no freeing, we are handling memleaks through the reset command */
			return MALI_ERR_OUT_OF_MEMORY;
		}
		_mali_sys_memset(po->fragment_shader_uniforms.array, 0, fragment_size *  sizeof(float) );
	}

	/* place these arrays in program object */
	po->vertex_shader_uniforms.cellsize = vertex_size;
	po->fragment_shader_uniforms.cellsize = fragment_size;

	/* allocate a local uniform symbol table big enough to fit all vertex shader symbols (flat structure) */
	vertex_temp_table = bs_symbol_table_alloc(vertex_vars);
	if(!vertex_temp_table) return MALI_ERR_OUT_OF_MEMORY;
	if ( vertex_vars > 0 )
	{
		/* allocate a parents array */
		vertex_parents = _mali_sys_malloc( sizeof(s32)* vertex_vars );
		if ( NULL == vertex_parents )
		{
			bs_symbol_table_free( vertex_temp_table );
			return MALI_ERR_OUT_OF_MEMORY;
		}
	}

	/* allocate a local uniform symbol table big enough to fit all fragment shader symbols (flat structure) */
	fragment_temp_table = bs_symbol_table_alloc(fragment_vars);
	if( !fragment_temp_table )
	{
		bs_symbol_table_free( vertex_temp_table );
		if(vertex_parents) _mali_sys_free(vertex_parents);
		return MALI_ERR_OUT_OF_MEMORY;
	}
	if( fragment_vars > 0 )
	{
		fragment_parents = _mali_sys_malloc( sizeof(s32) * fragment_vars );
		if( NULL == fragment_parents )
		{
			bs_symbol_table_free( vertex_temp_table );
			bs_symbol_table_free( fragment_temp_table );
			if(vertex_parents) _mali_sys_free(vertex_parents);
			return MALI_ERR_OUT_OF_MEMORY;
		}

	}

	/**************************** LOAD FLAT STRUCTURE SYMBOLS **********************/

	/* load all fragment shader symbols and initializers*/
	while(bs_stream_bytes_remaining(fragment_stream) > 0)
	{
		struct bs_symbol* symbol;
		struct bs_stream uniform_stream;
		mali_err_code err;
		u32 blockname;
		err = bs_create_subblock_stream(fragment_stream, &uniform_stream);
		if(err == MALI_ERR_FUNCTION_FAILED)
		{
			bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Fragment shader is invalid; corrupt SUNI datastream detected");
			bs_symbol_table_free( vertex_temp_table );
			bs_symbol_table_free( fragment_temp_table );
			if(vertex_parents) _mali_sys_free( vertex_parents );
			if(fragment_parents) _mali_sys_free( fragment_parents );
			return err;
		}
		blockname = bs_peek_header_name(&uniform_stream);
		switch(blockname)
		{
			case VUNI:
				if(num_loaded_vars < fragment_vars)
				{
					err = read_and_allocate_uniform_variable(
							&uniform_stream, &symbol,
							&fragment_parents[num_loaded_vars],
							po->fragment_shader_uniforms.array,
							po->fragment_shader_uniforms.cellsize,
							MALI_TRUE);
				}
				else
				{
					bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Fragment shader corrupt; mistmatch between declared and detected uniforms");
					bs_symbol_table_free( vertex_temp_table );
					bs_symbol_table_free( fragment_temp_table );
					if(vertex_parents) _mali_sys_free(vertex_parents);
					if(fragment_parents) _mali_sys_free(fragment_parents);
					return MALI_ERR_FUNCTION_FAILED;
				}
				if(err == MALI_ERR_NO_ERROR)
				{	/* we found a symbol. Keep it! */
					symbol->location.vertex = -1;
					fragment_temp_table->members[num_loaded_vars] = symbol;
					num_loaded_vars++;

					/* assert symbol in legal range */
					if( symbol->location.fragment + symbol->block_size.fragment > fragment_size )
					{
						int buffersize = 1000 + _mali_sys_strlen(symbol->name);
						char* buffer = _mali_sys_malloc(buffersize);
						if(buffer == NULL)
						{
							bs_set_error_out_of_memory(&po->log);
							bs_symbol_table_free( vertex_temp_table );
							bs_symbol_table_free( fragment_temp_table );
							if(vertex_parents) _mali_sys_free(vertex_parents);
							if(fragment_parents) _mali_sys_free(fragment_parents);
							return MALI_ERR_OUT_OF_MEMORY;
						}
						_mali_sys_snprintf( buffer, buffersize,
											"Fragment shader corrupt. Shader uniform '%s' uses register %i, but there are only %i registers declared in the shader.",
											symbol->name, symbol->location.fragment + symbol->block_size.fragment,
											fragment_size );
						bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, buffer);
						_mali_sys_free(buffer);
						bs_symbol_table_free( vertex_temp_table );
						bs_symbol_table_free( fragment_temp_table );
						if(vertex_parents) _mali_sys_free(vertex_parents);
						if(fragment_parents) _mali_sys_free(fragment_parents);
						return MALI_ERR_FUNCTION_FAILED;
					}
				}
				if(err == MALI_ERR_OUT_OF_MEMORY)
				{
					bs_symbol_table_free( vertex_temp_table );
					bs_symbol_table_free( fragment_temp_table );
					if(vertex_parents) _mali_sys_free(vertex_parents);
					if(fragment_parents) _mali_sys_free(fragment_parents);
					return err;
				}
				if(err == MALI_ERR_FUNCTION_FAILED)
				{
					bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Fragment shader corrupt, unannounced version mismatch between compiler and linker?");
					bs_symbol_table_free( vertex_temp_table );
					bs_symbol_table_free( fragment_temp_table );
					if(vertex_parents) _mali_sys_free(vertex_parents);
					if(fragment_parents) _mali_sys_free(fragment_parents);
					return err;
				}
				break;
			case NO_BLOCK:
				bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Fragment shader corrupt, unannounced version mismatch between compiler and linker?");
				bs_symbol_table_free( vertex_temp_table );
				bs_symbol_table_free( fragment_temp_table );
				if(vertex_parents) _mali_sys_free(vertex_parents);
				if(fragment_parents) _mali_sys_free(fragment_parents);
				return MALI_ERR_FUNCTION_FAILED;
			default:
				/* properly declared (but unknown) blocks will be elegantly skipped without processing or errors */
				;
		}
	}
	if(fragment_vars != num_loaded_vars)
	{
		bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Fragment shader corrupt, mismatch between expected and found uniforms");
		bs_symbol_table_free( vertex_temp_table );
		bs_symbol_table_free( fragment_temp_table );
		if(vertex_parents) _mali_sys_free(vertex_parents);
		if(fragment_parents) _mali_sys_free(fragment_parents);
		return MALI_ERR_FUNCTION_FAILED;
	}
	num_loaded_vars = 0;

	/* load all vertex shader symbols and initializers*/
	while(bs_stream_bytes_remaining(vertex_stream) > 0)
	{
		struct bs_symbol* symbol;
		struct bs_stream uniform_stream;
		mali_err_code err;
		u32 blockname;
		err = bs_create_subblock_stream(vertex_stream, &uniform_stream);
		if(err == MALI_ERR_FUNCTION_FAILED)
		{
			bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader is invalid; corrupt SUNI datastream detected");
			bs_symbol_table_free( vertex_temp_table );
			bs_symbol_table_free( fragment_temp_table );
			if(vertex_parents) _mali_sys_free( vertex_parents );
			if(fragment_parents) _mali_sys_free( fragment_parents );
			return err;
		}
		blockname = bs_peek_header_name(&uniform_stream);
		switch(blockname)
		{
			case VUNI:
				if(num_loaded_vars < vertex_vars)
				{
					err = read_and_allocate_uniform_variable(
							&uniform_stream, &symbol,
							&vertex_parents[num_loaded_vars],
							po->vertex_shader_uniforms.array,
							po->vertex_shader_uniforms.cellsize,
							MALI_FALSE);
				}
				else
				{
					bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader corrupt; mismatch between declared and detected uniforms");
					bs_symbol_table_free( vertex_temp_table );
					bs_symbol_table_free( fragment_temp_table );
					if(vertex_parents) _mali_sys_free(vertex_parents);
					if(fragment_parents) _mali_sys_free(fragment_parents);
					return MALI_ERR_FUNCTION_FAILED;
				}
				if(err == MALI_ERR_NO_ERROR)
				{	/* we found a symbol. Keep it! */
					symbol->location.fragment = -1;
					vertex_temp_table->members[num_loaded_vars] = symbol;
					num_loaded_vars++;

					/* assert symbol in legal range */
					if( symbol->location.vertex + symbol->block_size.vertex > vertex_size )
					{
						int buffersize = 1000 + _mali_sys_strlen(symbol->name);
						char* buffer = _mali_sys_malloc(buffersize);
						if(buffer == NULL)
						{
							bs_set_error_out_of_memory(&po->log);
							bs_symbol_table_free( vertex_temp_table );
							bs_symbol_table_free( fragment_temp_table );
							if(vertex_parents) _mali_sys_free(vertex_parents);
							if(fragment_parents) _mali_sys_free(fragment_parents);
							return MALI_ERR_OUT_OF_MEMORY;
						}
						_mali_sys_snprintf(buffer, buffersize,
											"Vertex shader corrupt. Shader uniform '%s' uses register %i, but there are only %i registers declared in the shader.",
											symbol->name, symbol->location.vertex + symbol->block_size.vertex,
											vertex_size );
						bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, buffer);
						_mali_sys_free(buffer);
						bs_symbol_table_free( vertex_temp_table );
						bs_symbol_table_free( fragment_temp_table );
						if(vertex_parents) _mali_sys_free(vertex_parents);
						if(fragment_parents) _mali_sys_free(fragment_parents);
						return MALI_ERR_FUNCTION_FAILED;
					}
				}
				if(err == MALI_ERR_OUT_OF_MEMORY)
				{
					bs_symbol_table_free( vertex_temp_table );
					bs_symbol_table_free( fragment_temp_table );
					if(vertex_parents) _mali_sys_free(vertex_parents);
					if(fragment_parents) _mali_sys_free(fragment_parents);
					return err;
				}
				if(err == MALI_ERR_FUNCTION_FAILED )
				{
					bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader corrupt, unannounced version mismatch between compiler and linker?");
					bs_symbol_table_free( vertex_temp_table );
					bs_symbol_table_free( fragment_temp_table );
					if(vertex_parents) _mali_sys_free(vertex_parents);
					if(fragment_parents) _mali_sys_free(fragment_parents);
					return err;
				}
				break;
			case NO_BLOCK:
				bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader corrupt, unannounced version mismatch between compiler and linker?");
				bs_symbol_table_free( vertex_temp_table );
				bs_symbol_table_free( fragment_temp_table );
				if(vertex_parents) _mali_sys_free(vertex_parents);
				if(fragment_parents) _mali_sys_free(fragment_parents);
				return MALI_ERR_FUNCTION_FAILED;
			default:
				/* properly declared (but unknown) blocks will be elegantly skipped without processing or errors */
				;
		}
	}
	if(vertex_vars != num_loaded_vars)
	{
		bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader corrupt, mismatch between expected and found uniforms");
		bs_symbol_table_free( vertex_temp_table );
		bs_symbol_table_free( fragment_temp_table );
		if(vertex_parents) _mali_sys_free(vertex_parents);
		if(fragment_parents) _mali_sys_free(fragment_parents);
		return MALI_ERR_FUNCTION_FAILED;
	}
	num_loaded_vars = 0;


	/**************************** ALLOCATE THREADED STRUCTURE SYMBOL TABLES **********************/

	temp_vertex_tree = bs_symbol_table_alloc( 0 );
	if(!temp_vertex_tree)
	{
		bs_symbol_table_free( vertex_temp_table );
		bs_symbol_table_free( fragment_temp_table );
		if(vertex_parents) _mali_sys_free(vertex_parents);
		if(fragment_parents) _mali_sys_free(fragment_parents);
		return MALI_ERR_OUT_OF_MEMORY;
	}
	temp_fragment_tree = bs_symbol_table_alloc( 0 );
	if(!temp_fragment_tree)
	{
		bs_symbol_table_free( vertex_temp_table );
		bs_symbol_table_free( fragment_temp_table );
		if(vertex_parents) _mali_sys_free(vertex_parents);
		if(fragment_parents) _mali_sys_free(fragment_parents);
		bs_symbol_table_free( temp_vertex_tree );
		return MALI_ERR_OUT_OF_MEMORY;
	}

	/**************************** BUILD THREADED STRUCTURE SYMBOL TABLES **********************/

	if(vertex_vars > 0)
	{
		error_code = recursively_build_uniform_tree( vertex_temp_table->members, vertex_temp_table->member_count, vertex_parents, -1, temp_vertex_tree );
		if ( error_code != MALI_ERR_NO_ERROR )
		{ /* something went wrong during building of tree (out of memory?). Abort, and clean up all allocated arrays */

			bs_symbol_table_free( vertex_temp_table );
			bs_symbol_table_free( fragment_temp_table );
			if(vertex_parents) _mali_sys_free(vertex_parents);
			if(fragment_parents) _mali_sys_free(fragment_parents);
			bs_symbol_table_free( temp_vertex_tree );
			bs_symbol_table_free( temp_fragment_tree );
			if(error_code == MALI_ERR_FUNCTION_FAILED)
			{
				bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Corrupt uniform construct detected in vertex shader.");
			}
			return error_code;
		}
		/* Iterate the flat structure to make sure every symbol was used */
		for(i=0; i<vertex_temp_table->member_count; ++i)
		{
			if(NULL != vertex_temp_table->members[i])
			{
				bs_symbol_table_free( vertex_temp_table );
				bs_symbol_table_free( fragment_temp_table );
				if(vertex_parents) _mali_sys_free(vertex_parents);
				if(fragment_parents) _mali_sys_free(fragment_parents);
				bs_symbol_table_free( temp_vertex_tree );
				bs_symbol_table_free( temp_fragment_tree );

				bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Invalid uniform structure dependencies detected in vertex shader");
				return MALI_ERR_FUNCTION_FAILED;
			}
		}
	}

	if(fragment_vars > 0)
	{
		error_code = recursively_build_uniform_tree( fragment_temp_table->members, fragment_temp_table->member_count, fragment_parents, -1, temp_fragment_tree );
		if ( error_code != MALI_ERR_NO_ERROR )
		{ /* something went wrong during building of tree (out of memory?). Abort, and clean up all allocated arrays */

			bs_symbol_table_free( vertex_temp_table );
			bs_symbol_table_free( fragment_temp_table );
			if(vertex_parents) _mali_sys_free(vertex_parents);
			if(fragment_parents) _mali_sys_free(fragment_parents);
			bs_symbol_table_free( temp_vertex_tree );
			bs_symbol_table_free( temp_fragment_tree );
			if(error_code == MALI_ERR_FUNCTION_FAILED)
			{
				bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Corrupt uniform construct detected in fragment shader.");
			}
			return error_code;
		}
		/* Iterate the flat structure to make sure every symbol was used */
		for(i=0; i<fragment_temp_table->member_count; ++i)
		{
			if(NULL != fragment_temp_table->members[i])
			{
				bs_symbol_table_free( vertex_temp_table );
				bs_symbol_table_free( fragment_temp_table );
				if(vertex_parents) _mali_sys_free(vertex_parents);
				if(fragment_parents) _mali_sys_free(fragment_parents);
				bs_symbol_table_free( temp_vertex_tree );
				bs_symbol_table_free( temp_fragment_tree );

				bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Invalid uniform structure dependencies detected in fragment shader");
				return MALI_ERR_FUNCTION_FAILED;
			}
		}
	}

	/**************************** DELETE FLAT STRUCTURE ALLOCATIONS **********************/

	bs_symbol_table_free( fragment_temp_table );
	fragment_temp_table = NULL;
	if ( fragment_parents ) _mali_sys_free( fragment_parents );
	fragment_parents = NULL;
	bs_symbol_table_free( vertex_temp_table );
	vertex_temp_table = NULL;
	if ( vertex_parents ) _mali_sys_free( vertex_parents );
	vertex_parents = NULL;

	/**************************** VALIDATE UNIFROM VALUES *******************************/

	{
		int i;
		for(i=0; i<(int)temp_fragment_tree->member_count; ++i)
		{
			if(!validate_uniform_values(temp_fragment_tree->members[i], temp_fragment_tree, i, MALI_TRUE))
			{
				bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Corrupt fragment shader; Invalid uniform values");
				bs_symbol_table_free(temp_vertex_tree);
				bs_symbol_table_free(temp_fragment_tree);
				return MALI_ERR_FUNCTION_FAILED;
			}
		}
		for(i=0; i<(int)temp_vertex_tree->member_count; ++i)
		{
			if(!validate_uniform_values(temp_vertex_tree->members[i], temp_vertex_tree, i, MALI_FALSE))
			{
				bs_set_error(&po->log, BS_ERR_LP_SYNTAX_ERROR, "Corrupt vertex shader; Invalid uniform values");
				bs_symbol_table_free(temp_vertex_tree);
				bs_symbol_table_free(temp_fragment_tree);
				return MALI_ERR_FUNCTION_FAILED;
			}
		}
	}

	/**************************** MERGE THREADED STRUCTURE SYMBOLS **********************/

	/* GENERAL STRATEGY:
	 *
	 * 2: allocate retval->members big enough to hold all vertex members + all fragment members at the same level
	 * 3: for each vertex symbol, locate matching fragment symbol
	 * 4: assert symbols equal type though compare function
	 * 5: if equal types + offset, move symbol branch from fragment to retval, update vertex location. free/nill vertex branch.
	 * 6: if not found in fragment shader, move vertex branch from vertex to retval. Nill vertex branch.
	 * 7. Move all remaining symbols in fragment to shader.
	 * 8. Optional. Resize size
	 *
	 * 9. free vertex and fragment
	 *
	 */

	/* allocate symbol */
	retval = bs_symbol_table_alloc(vertex_vars + fragment_vars);
	if(!retval)
	{
		bs_symbol_table_free(temp_vertex_tree);
		bs_symbol_table_free(temp_fragment_tree);
		return MALI_ERR_OUT_OF_MEMORY;
	}
	retval->member_count = 0;

	/* merge symbols into retval */
	for(i = 0; i < temp_vertex_tree->member_count; i++)
	{
		struct bs_symbol* vsymbol = temp_vertex_tree->members[i]; /* this is our vertex shader symbol */
		mali_bool found = MALI_FALSE;

		for(x = 0; x < temp_fragment_tree->member_count; x++)
		{
			struct bs_symbol* fsymbol = temp_fragment_tree->members[x]; /* this is our fragment shader symbol */
			if( !fsymbol ) continue;
			if( _mali_sys_strcmp(vsymbol->name, fsymbol->name) == 0)
			{
				char buffer[512];

				if( ! bs_symbol_type_compare(vsymbol, fsymbol, buffer, 512) ||
					! bs_symbol_precision_compare(vsymbol, fsymbol, buffer, 512)
				  )
				{ /* a match, but symbols are not equal in type or precision (bufferlog has all the details) */
					int buffersize = 600; /* other buffer is 512, this should hold that and the word "uniform" perfectly well */
					char* tempbuffer = _mali_sys_malloc(buffersize);
					if(tempbuffer == NULL)
					{
						bs_symbol_table_free(temp_vertex_tree);
						bs_symbol_table_free(temp_fragment_tree);
						bs_symbol_table_free(retval);
						bs_set_error_out_of_memory(&po->log);
						return MALI_ERR_OUT_OF_MEMORY;
					}
					_mali_sys_snprintf(tempbuffer, buffersize, "Uniform %s", buffer);
					bs_set_error(&po->log, BS_ERR_LINK_UNIFORM_TYPE_MISMATCH, tempbuffer);
					_mali_sys_free(tempbuffer);
					bs_symbol_table_free(temp_vertex_tree);
					bs_symbol_table_free(temp_fragment_tree);
					bs_symbol_table_free(retval);
					return MALI_ERR_FUNCTION_FAILED;
				}

				/* update match info (sets locations and alignments) */
				bs_symbol_merge_shadertype_specifics(vsymbol, fsymbol);

				/* move symbol from fragment shader to retval, free vertex symbol */
				found = MALI_TRUE;
				fsymbol = retval->members[retval->member_count] = temp_fragment_tree->members[x]; /* double assignment, update fsymbol pointer AND tree structure */
				temp_fragment_tree->members[x] = NULL;   /* remove symbol from fragment temp tree */
				retval->member_count++;
				bs_symbol_free( temp_vertex_tree->members[i] );  /* free vertex symbol; no longer used */
				temp_vertex_tree->members[i] = NULL;
				break;
			}
		}

		if (!found)
		{	/* no matching symbol, just add to table */
			retval->members[retval->member_count] = vsymbol;
			retval->member_count++;
			temp_vertex_tree->members[i] = NULL;
		}
	}

	/* add missing fragment symbols */
	for(i = 0; i < temp_fragment_tree->member_count; i++)
	{
		if(temp_fragment_tree->members[i])
		{
			retval->members[retval->member_count] = temp_fragment_tree->members[i];
			retval->member_count++;
			temp_fragment_tree->members[i] = NULL;
		}
	}

	bs_symbol_table_free(temp_fragment_tree);
	temp_fragment_tree = NULL;
	bs_symbol_table_free(temp_vertex_tree);
	temp_vertex_tree = NULL;

	po->uniform_symbols = retval;

	/**************************** ADD CACHES AND POSTPROCESSING **********************/

	/* at this point, the uniforms_symbols symbol table is complete. But there are more to uniforms than this. */

	/* add sampler info to the sampler_info array */
	error_code =  bs_loader_cache_sampler_info(po);
	if(error_code != MALI_ERR_NO_ERROR) return error_code;

	return MALI_ERR_NO_ERROR;
}

