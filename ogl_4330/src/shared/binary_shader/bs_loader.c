/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>

#include <shared/binary_shader/bs_loader.h>
#include <shared/binary_shader/bs_loader_internal.h>
#include <shared/binary_shader/bs_error.h>
#include <regs/MALIGP2/mali_gp_core.h>

/*****************************************************************************/
/* binary shader parsing                                                     */
/*****************************************************************************/

/* returns type of next header - assumes header is first in binary data */
enum blocktype bs_peek_header_name(struct bs_stream* stream)
{
	enum blocktype name;
	u32 size;
	char a[4];
	int i;
	u32 old_position;

	MALI_DEBUG_ASSERT_POINTER( stream );

	if(bs_stream_bytes_remaining(stream) < 8) return NO_BLOCK; /* need at least 8 bytes for it to be a new block here */

	old_position = stream->position; /* save old position, we don't want to modify this stream */

	for(i = 0; i < 4; i++)
	{
		a[i] = load_u8_value(stream);
	}

	name = (enum blocktype)MAKE_BLOCKTYPE( a[0], a[1], a[2], a[3] );
	size = load_u32_value(stream);
	stream->position = old_position;  /* done reading, restore old position */

	if(size + 8> bs_stream_bytes_remaining(stream)) return NO_BLOCK; /* the next block reports that it is bigger than the number of bytes remaining.
						That can't be right. Let's assume the stream has ended, and return NO_BLOCK. The loaders
						should pick up this hint and exit cleanly. */
	return name;
}

/* reads a header, and matches it against the expected header.
 * If there is a match, the returnvalue is "size", binary data is incremented with headersize (ie, 8) bytes, and bytes_remaining is decremented by the same value.
 * BUT - if there IS a match, but the required size does not fit in bytes_remaining, the match is invalidated,
 *       and bytes_remaining is set to zero (and binary_data moved appropriately). This safeguards against buffer underruns.
 * If there is NOT a match, the return value is NO_BLOCK, the block is SKIPPED by incrementing the stream appropriately.
 */
u32 bs_read_or_skip_header(struct bs_stream* stream, enum blocktype expected_header)
{
	enum blocktype name;
	u32 size;

	MALI_DEBUG_ASSERT_POINTER( stream );

	name = bs_peek_header_name(stream);
	if(!name)
	{
		/* we found no name! This means the stream is either:
		   - emptied out (no more data)
		   - faulty (no room for a new header or next header specifies a block too big to fit)
		   - contains a block with a block identifier equal to "\0\0\0\0"
		   All cases are terminally bad. So let's move to the end of the stream,
		   which should prompt any caller of this function to stop attempting to load more data, and exit gracefully.
		*/
		stream->position = stream->size;
		return 0;
	}

	/* skip blockname, we alrady peeked it */
	stream->position += 4;

	/* get blocksize */
	size = load_u32_value(stream);

	if(name != expected_header)
	{
		/* the block we wanted wasn't as expected. Skip this block */
		stream->position += size;
		return 0;
	}
	return size;
}

/* Creates a sufficiently large enough stream to contain only the next subblock.
 * In case of which the next subblock is to larger than the parent stream, or not large enough
 * to contain an entire header, the parent stream is considered corrupt, position set to the end
 * and error returned.
 */
mali_err_code bs_create_subblock_stream(struct bs_stream* parent, struct bs_stream* child)
{
	u32 size = 0;
	if(bs_stream_bytes_remaining(parent) < 8) {
		parent->position = parent->size;
		return MALI_ERR_FUNCTION_FAILED;
	}
	parent->position += 4; /* blocknames handled by whatever accepts the child stream */
	size = load_u32_value(parent);

	child->data         = ((char*) parent->data) + parent->position - 8;
	child->size         = size + 8; /* includes the header */
	child->position     = 0;

	if(parent->position + size > parent->size) {
		/* Corrupt datastream */
		child->data = NULL;
		child->size = 0;
		parent->position = parent->size;
		return MALI_ERR_FUNCTION_FAILED;
	}
	else
	{
		parent->position += size;
		return MALI_ERR_NO_ERROR;
	}
}

mali_err_code bs_read_VPRO_block(struct bs_stream* stream, u32* out_data)
{
   u32 datasize = bs_read_or_skip_header(stream, VPRO);

   if (datasize == 0)
   {
      return MALI_ERR_FUNCTION_FAILED;
   }

   *out_data =  (u32)load_s32_value(stream);

   return MALI_ERR_NO_ERROR;
}

/* reads a string into a buffer  */
mali_err_code bs_read_and_allocate_string(struct bs_stream* stream, char** output)
{
	u32 size;

	MALI_DEBUG_ASSERT_POINTER( stream );
	MALI_DEBUG_ASSERT_POINTER( output );

	size = bs_read_or_skip_header(stream, STRI);
	/* a string should be at least 1 byte long (nullterminator char); anything less means an error loading */
	if(size == 0) return MALI_ERR_FUNCTION_FAILED;

	*output = _mali_sys_malloc(size);
	/* we have to be able to load all the string into ram */
	if( *output == NULL ) return MALI_ERR_OUT_OF_MEMORY;

	_mali_sys_memcpy(*output, bs_stream_head(stream), size);

	/* should be unnescessary, but ensure that string is terminated. Can never trust external data */
	(*output)[size-1] = '\0';

	/* we just read the entire block, offset stream accordingly */
	stream->position += size;

	return MALI_ERR_NO_ERROR;
}

/* read and allocate a binary block */
mali_err_code bs_read_and_allocate_binary_block_contents( struct bs_stream* stream, void* *out_data, u32* out_size )
{

	MALI_DEBUG_ASSERT_POINTER( stream );
	MALI_DEBUG_ASSERT_POINTER( out_data );
	MALI_DEBUG_ASSERT_POINTER( out_size );

	/* get size of block */
	*out_size = bs_read_or_skip_header(stream, DBIN);
	if( *out_size == 0 )
	{
		return MALI_ERR_FUNCTION_FAILED;
	}

	/* allocate buffer to hold block */
	*out_data = _mali_sys_malloc( *out_size );
	if( *out_data == NULL )
	{
		/* skip this block to preserve stream consistency */
		stream->position += *out_size;
		return MALI_ERR_OUT_OF_MEMORY;
	}

	/* read data from block */
	_mali_sys_memcpy(*out_data, bs_stream_head(stream), *out_size); /* copy uniformblock from shaderdata to shader */

	/* block read, jump to beginning of next block */
	stream->position += *out_size;
	return MALI_ERR_NO_ERROR;
}

mali_err_code bs_copy_binary_block( struct bs_stream* stream, enum blocktype blockname, void** out_data, u32* out_size )
{
	u32 datasize;
	u32 offset_blockstart;

	MALI_DEBUG_ASSERT_POINTER( stream );
	MALI_DEBUG_ASSERT_POINTER( blockname );
	MALI_DEBUG_ASSERT_POINTER( out_data );
	MALI_DEBUG_ASSERT_POINTER( out_size );

	/* initialize output */
	*out_size = 0;

	/* since were saving this entire block, header and all, keep track of where the block starts */
	offset_blockstart = stream->position;

	/* read block header */
	datasize = bs_read_or_skip_header(stream, blockname);
	if( datasize == 0 )
	{
		return MALI_ERR_FUNCTION_FAILED;
	}

	/* allocate block copy sized datasize + header */
	*out_data = _mali_sys_malloc( 8 + datasize );
	if( *out_data == NULL ) return MALI_ERR_OUT_OF_MEMORY;

	/* copy block */
	_mali_sys_memcpy(*out_data, ((const u8*) stream->data) + offset_blockstart, datasize + 8);

	/* offset stream */
	stream->position += datasize;

	/* the amount of data read is datasize + header */
	*out_size = datasize + 8;

	return MALI_ERR_NO_ERROR;
}


/**
 * @brief Loads a CVER block, resulting in a binary shader object
 * @param stream The stream to read from. The stream offset will increment as we load data.
 * @param so The output parameter; the result will be placed here. Must be an empty shader object
 * @note The function is well-behaved, it either reads an entire block or nothing at all. If we ever run out of streamdata,
 * the stream is emptied, and we stop loading, graciously
 */
MALI_STATIC mali_err_code read_vertex_shader_block( struct bs_stream* stream, bs_shader *so)
{
	u32 size;
	u32 position_end_of_block;
	mali_err_code err = MALI_ERR_NO_ERROR;
	/* duplication of blocks should fail */
	mali_bool fpoi_found = MALI_FALSE;
	mali_bool ftra_found = MALI_FALSE;
	mali_bool svar_found = MALI_FALSE;
	mali_bool suni_found = MALI_FALSE;
	mali_bool satt_found = MALI_FALSE;
	mali_bool duplication_found = MALI_FALSE;
	mali_bool fins_found = MALI_FALSE; /* check to see if we have a FINS block or not; shaders without this need to be given the instruction flags */
	mali_bool dbin_found = MALI_FALSE; /* checks to see if a DBIN block is present. Mandatory as of 17.07.2008. See bugzilla 5846 */
	mali_bool dpro_found = MALI_FALSE;
#ifdef MALI_BB_TRANSFORM
	mali_bool vsop_found = MALI_FALSE; /* checks to see if a VSOP block is present. it contains patterns for pretransforming on cpu*/
#endif

	so->compiled = MALI_FALSE;

	MALI_DEBUG_ASSERT_POINTER( stream );
	MALI_DEBUG_ASSERT_POINTER( so );

	size = bs_read_or_skip_header(stream, CVER);

	if(size < 4) /*need at least 4 more bytes to read all version core data */
	{
		bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "The vertex shader is damaged, corrupt CVER block detected");
		return MALI_ERR_FUNCTION_FAILED; /* need at least four bytes of data for a version tag */
	}

	/* get the position of the end of the block */
	position_end_of_block = stream->position + size;


	/* read core */
	so->flags.vertex.core = (enum mali_core) load_u32_value(stream);
	switch(so->flags.vertex.core)
	{
		case MALI_GP2:
			break; /* do nothing */
#if defined(USING_MALI400) || defined(USING_MALI450)
		case MALI_400GP:
			break; /* do nothing */
#endif
		default:
			bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "The vertex shader is compiled for an unknown core");
			return MALI_ERR_FUNCTION_FAILED; /* unsupported/unknown vertex shader core */
	}


	while(position_end_of_block > stream->position )
	{
		enum blocktype dataname;
		u32 datasize;
		struct bs_stream subblock_stream;
		err = bs_create_subblock_stream(stream, &subblock_stream);
		if(err != MALI_ERR_NO_ERROR) {
			bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "The vertex shader is damaged, corrupt CVER block detected");
			return err;
		}

		dataname = bs_peek_header_name(&subblock_stream);

		switch(dataname)
		{
			case FPOI:
				duplication_found = fpoi_found;
				datasize = bs_read_or_skip_header(&subblock_stream, dataname);
				/* FPOI value no longer used; just skip entire block */
				fpoi_found = MALI_TRUE;
				break;
			case FTRA:
				duplication_found = ftra_found;
				datasize = bs_read_or_skip_header(&subblock_stream, dataname);
				if(datasize != 4)
				{
					bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "The vertex shader is damaged, corrupt FTRA block detected.");
					return MALI_ERR_FUNCTION_FAILED;
				}
				so->flags.vertex.transform_status = load_u32_value(&subblock_stream);
				ftra_found = MALI_TRUE;
				break;
			case SUNI:
				duplication_found = suni_found;
				err =  bs_copy_binary_block( &subblock_stream, SUNI, &so->uniforms, &so->uniform_size );
				if(err != MALI_ERR_NO_ERROR) return err;
				suni_found = MALI_TRUE;
				break;
			case SATT:
				duplication_found = satt_found;
				err =  bs_copy_binary_block( &subblock_stream, SATT, &so->attributes, &so->attribute_size );
				if(err != MALI_ERR_NO_ERROR) return err;
				satt_found = MALI_TRUE;
				break;
#ifdef MALI_BB_TRANSFORM
			case VSOP:
				duplication_found = vsop_found;
				err =  bs_copy_binary_block( &subblock_stream, VSOP, &so->cpu_operations_stream, &so->cpu_operations_stream_size);
				if(err != MALI_ERR_NO_ERROR) return err;
				vsop_found = MALI_TRUE;
				break;
#endif
			case SVAR:
				duplication_found = svar_found;
				err =  bs_copy_binary_block( &subblock_stream, SVAR, &so->varyings, &so->varying_size );
				if(err != MALI_ERR_NO_ERROR) return err;
				svar_found = MALI_TRUE;
				break;
			case DBIN:
				duplication_found = dbin_found;
				err = bs_read_and_allocate_binary_block_contents( &subblock_stream, &so->shader_block, &so->shader_size );
				if(err != MALI_ERR_NO_ERROR) return err;
				if(so->shader_size < MALIGP2_VS_INSTRUCTION_SIZE) {
					bs_set_error(&so->log, BS_ERR_NOTE_MESSAGE, "Vertex shader binary block too small");
					return MALI_ERR_FUNCTION_FAILED;
				}
				dbin_found = MALI_TRUE;
				break;
			case DPRO:
				duplication_found = dpro_found;
				err =  bs_copy_binary_block( &subblock_stream, DPRO, &so->projobs, &so->projob_size );
				if(err != MALI_ERR_NO_ERROR) return err;
				dpro_found = MALI_TRUE;
				break;
			case FINS:
				duplication_found = fins_found;
				datasize = bs_read_or_skip_header(&subblock_stream, dataname);
				if(datasize != 12)
				{
					bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "The vertex shader is damaged, corrupt FINS block detected.");
					return MALI_ERR_FUNCTION_FAILED;
				}
				so->flags.vertex.instruction_start =  load_u32_value(&subblock_stream);
				so->flags.vertex.instruction_end =  load_u32_value(&subblock_stream);
				so->flags.vertex.instruction_last_touching_input =  load_u32_value(&subblock_stream);
				fins_found = MALI_TRUE;
				break;
			case NO_BLOCK: /* error while loading block */
				bs_set_error(&so->log, BS_ERR_NOTE_MESSAGE, "Vertex shader is corrupted");
				return MALI_ERR_FUNCTION_FAILED;
			default:
				/* illegal block, let's just ignore it for now */
				bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "The vertex shader contains unknown blocks. Shader version mismatch?");
				datasize = bs_read_or_skip_header(&subblock_stream, dataname);
				break;
		}
		if(duplication_found)
		{
			bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "Vertex shader damaged. Duplicate blocks detected.");
			return MALI_ERR_FUNCTION_FAILED;
		}
	}

	/* sanity handle the FINS block */
	{
		u32 one_past_last_instruction = so->shader_size / MALIGP2_VS_INSTRUCTION_SIZE;
		if( fins_found == MALI_FALSE )
		{
			so->flags.vertex.instruction_start = 0;
			so->flags.vertex.instruction_end = one_past_last_instruction;
			so->flags.vertex.instruction_last_touching_input = so->flags.vertex.instruction_end;
		}
		/* handle corrupt FINS block */
		if( so->flags.vertex.instruction_start > one_past_last_instruction  ||
			so->flags.vertex.instruction_end > one_past_last_instruction  ||
			so->flags.vertex.instruction_start > so->flags.vertex.instruction_end  ||
			so->flags.vertex.instruction_last_touching_input > so->flags.vertex.instruction_end
		  )
		{
			bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "The vertex shader is damaged, illegal values found in the FINS block.");
			return MALI_ERR_FUNCTION_FAILED;
		}
	}

	if(!dbin_found) {
		stream->position = position_end_of_block;
		bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "The vertex shader is damaged, no DBIN block detected.");
		return MALI_ERR_FUNCTION_FAILED;
	}

	so->compiled = MALI_TRUE;
	return MALI_ERR_NO_ERROR;

}

/**
 * @brief Loads a CFRA block, resulting in a binary shader object
 * @param binary_data A pointer to a streampointer. The streampointer will increment as we load data.
 * @param byest_remanining A pointer to a streamsize. The streamsize will decrement as we load data.
 * @param so The output parameter; the result will be placed here. Must be an empty shader object
 * @note The function is well-behaved, it either reads an entire block or nothing at all. If we ever run out of streamdata,
 * the stream is emptied, and we stop loading, graciously
 */
MALI_STATIC mali_err_code read_fragment_shader_block(struct bs_stream* stream, bs_shader *so)
{
	u32 size;
	u32 position_end_of_block;
	mali_err_code err = MALI_ERR_NO_ERROR;
	/* duplication of blocks should fail */
	mali_bool fdis_found = MALI_FALSE;
	mali_bool fbuu_found = MALI_FALSE;
	mali_bool svar_found = MALI_FALSE;
	mali_bool suni_found = MALI_FALSE;
	mali_bool duplicate_block_found = MALI_FALSE;
	mali_bool fsta_found = MALI_FALSE; /* checks to see if a FSTA block is present. Mandatory for all M200 Fragment shaders */
	mali_bool dbin_found = MALI_FALSE; /* checks to see if a DBIN block is present. Mandatory as of 17.07.2008. See bugzilla 5846 */
	mali_bool dpro_found = MALI_FALSE; /* checks to see if a DPRO block is present. Experimental as of 28.02.2012. See MJOLL-1617 */
	so->compiled= MALI_FALSE;

	MALI_DEBUG_ASSERT_POINTER( stream );
	MALI_DEBUG_ASSERT_POINTER( so );

	size = bs_read_or_skip_header(stream, CFRA);

	if(size < 4) /*need at least 4 more bytes to read all version core data */
	{
		bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "The fragment shader is damaged, corrupt CFRA block detected.");
		return MALI_ERR_FUNCTION_FAILED; /* need at least four bytes of data for a version tag */
	}

	/* get the end of the block */
	position_end_of_block = stream->position + size;

	/* read core */
	so->flags.fragment.core = (enum mali_core) load_u32_value(stream);
	switch(so->flags.fragment.core)
	{
		case MALI_200:
			break; /* do nothing */
#if defined(USING_MALI400) || defined(USING_MALI450)
		case MALI_400PP:
			break; /* do nothing */
#endif
		default:
			bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "The fragment shader is compiled for an unknown core.");
			return MALI_ERR_FUNCTION_FAILED; /* unsupported/unknown fragment shader core */
	}

	/* read the rest of the block! */
	while(position_end_of_block > stream->position )
	{
		enum blocktype dataname;
		u32 datasize;
		struct bs_stream subblock_stream;
		err = bs_create_subblock_stream(stream, &subblock_stream);
		if(err != MALI_ERR_NO_ERROR) {
			bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "The fragment shader is damaged, corrupt CVER block detected");
			return err;
		}
		dataname = bs_peek_header_name(&subblock_stream);
		switch(dataname)
		{
			case FSTA:
				duplicate_block_found = fsta_found;
				datasize = bs_read_or_skip_header(&subblock_stream, dataname);
				if(datasize != 8)
				{
					bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "The fragment shader is damaged, corrupt FSTA block detected.");
					return MALI_ERR_FUNCTION_FAILED;
				}
				so->flags.fragment.stack_size = load_u32_value(&subblock_stream);
				so->flags.fragment.initial_stack_offset = load_u32_value(&subblock_stream);
				fsta_found = MALI_TRUE;
				break;
			case FDIS:
				duplicate_block_found = fdis_found;
				datasize = bs_read_or_skip_header(&subblock_stream, dataname);
				if(datasize != 4)
				{
					bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "The fragment shader is damaged, corrupt FDIS block detected.");
					return MALI_ERR_FUNCTION_FAILED;
				}
				so->flags.fragment.used_discard = load_u32_value(&subblock_stream);
				fdis_found = MALI_TRUE;
				break;
			case FBUU:
				duplicate_block_found = fbuu_found;
				datasize = bs_read_or_skip_header(&subblock_stream, dataname);
				if(datasize != 8)
				{
					bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "The fragment shader is damaged, corrupt FBUU block detected.");
					return MALI_ERR_FUNCTION_FAILED;
				}
				so->flags.fragment.color_read = load_u8_value(&subblock_stream);
				so->flags.fragment.color_write = load_u8_value(&subblock_stream);
				so->flags.fragment.depth_read = load_u8_value(&subblock_stream);
				so->flags.fragment.depth_write = load_u8_value(&subblock_stream);
				so->flags.fragment.stencil_read = load_u8_value(&subblock_stream);
				so->flags.fragment.stencil_write = load_u8_value(&subblock_stream);
				subblock_stream.position += 2;
				fbuu_found = MALI_TRUE;
				break;
			case SUNI:
				duplicate_block_found = suni_found;
				err =  bs_copy_binary_block( &subblock_stream, SUNI, &so->uniforms, &so->uniform_size );
				if(err != MALI_ERR_NO_ERROR) return err;
				suni_found = MALI_TRUE;
				break;
			case SVAR:
				duplicate_block_found = svar_found;
				err =  bs_copy_binary_block( &subblock_stream, SVAR, &so->varyings, &so->varying_size );
				if(err != MALI_ERR_NO_ERROR) return err;
				svar_found = MALI_TRUE;
				break;
			case DBIN:
				duplicate_block_found = dbin_found;
				err = bs_read_and_allocate_binary_block_contents( &subblock_stream, &so->shader_block, &so->shader_size );
				if(err != MALI_ERR_NO_ERROR) return err;
				if(so->shader_size > 0) {
					so->flags.fragment.size_of_first_instruction = ((unsigned char*)so->shader_block)[0] & 0x1F; /* note */
					so->flags.fragment.rendezvous_flag = ((unsigned char*)so->shader_block)[0] & 0x40; /* note */
					if(so->shader_size < so->flags.fragment.size_of_first_instruction*4) {
						bs_set_error(&so->log, BS_ERR_NOTE_MESSAGE, "Fragment shader binary block not large enough to contain first instruction");
						return MALI_ERR_FUNCTION_FAILED;
					}
				} else {
					bs_set_error(&so->log, BS_ERR_NOTE_MESSAGE, "Fragment shader binary block of zero size");
					return MALI_ERR_FUNCTION_FAILED;
				}
				dbin_found = MALI_TRUE;
				break;
			case DPRO:
				duplicate_block_found = dpro_found;
				err =  bs_copy_binary_block( &subblock_stream, DPRO, &so->projobs, &so->projob_size );
				if(err != MALI_ERR_NO_ERROR) return err;
				dpro_found = MALI_TRUE;
				break;
			case NO_BLOCK: /* error while loading block */
				bs_set_error(&so->log, BS_ERR_NOTE_MESSAGE, "Fragment shader is corrupted");
				return MALI_ERR_FUNCTION_FAILED;
			default:
				/* illegal block, let's just ignore it for now */
				datasize = bs_read_or_skip_header(&subblock_stream, dataname);
				bs_set_error(&so->log, BS_ERR_NOTE_MESSAGE, "Unknown data detected while loading CFRA block in fragment shader");
				break;
		}
		if(duplicate_block_found)
		{
			bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "Fragment shader damaged. Duplicate blocks detected.");
			return MALI_ERR_FUNCTION_FAILED;
		}

	}
	if(!fsta_found) {
		stream->position = position_end_of_block;
		bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "The fragment shader is damaged, no FSTA block detected.");
		return MALI_ERR_FUNCTION_FAILED;
	}
	if(!dbin_found) {
		stream->position = position_end_of_block;
		bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "The fragment shader is damaged, no DBIN block detected.");
		return MALI_ERR_FUNCTION_FAILED;
	}
	so->compiled = MALI_TRUE;
	return MALI_ERR_NO_ERROR;
}

MALI_EXPORT mali_err_code __mali_binary_shader_load(bs_shader *so, u32 shadertype, const void* binary_data, u32 binary_length)
{

	u32 size;
	struct bs_stream stream;
	mali_bool shader_block_found = MALI_FALSE;
	mali_err_code error = MALI_ERR_NO_ERROR;


	MALI_DEBUG_ASSERT_POINTER(so);
	MALI_DEBUG_ASSERT_POINTER( binary_data );

	stream.data = binary_data;
	stream.size = binary_length;
	stream.position = 0;

	/* start to load the structs */
	size = bs_read_or_skip_header(&stream, MBS1);
	if(size == 0)
	{
		if(stream.size == 0)
		{
			bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "Binary shader is empty");
		} else
		{
			bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "Not a Mali binary Shader");
		}
		return MALI_ERR_FUNCTION_FAILED;
	}

	/* spool through all blocks, looking for the shader we require */
	while(bs_stream_bytes_remaining(&stream) > 0)
	{
		u32 dataname;
		struct bs_stream subblock_stream;

		error = bs_create_subblock_stream(&stream, &subblock_stream);
		if(error != MALI_ERR_NO_ERROR)
		{
			bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "Corrupt binary shader stream");
			return error;
		}

		dataname = bs_peek_header_name(&subblock_stream);
		if(dataname == CFRA && shadertype == BS_FRAGMENT_SHADER)  /* we found a CFRA block, we need a CFRA block. Load it.  */
		{
			 /* clean up old states */
			 __mali_shader_binary_state_reset(so);

			 /* load new state */
			 error = read_fragment_shader_block(&subblock_stream, so);
			 shader_block_found = MALI_TRUE;
			 break;
		} 
		else if (dataname == CVER && shadertype == BS_VERTEX_SHADER) /* we found a CVER block, which was what we needed. Load it. */
		{
			 __mali_shader_binary_state_reset(so);

			 error = read_vertex_shader_block(&subblock_stream, so);
			 shader_block_found = MALI_TRUE;
			 break;
		}

		/* no match. Just skip this block. We're choosing the CVER/CFRA blocktype we're expecting, since we already
		 * checked if that is there. This blocktype is definitively wrong, and thus the block will be skipped. */
		if(dataname != CFRA && dataname != CVER)
		{ /* just give a friendly warning that something is amiss - this block has never before been detected ;) */
			bs_set_error(&so->log, BS_ERR_NOTE_MESSAGE, "Unknown data block detected in shader");
		}

	}
	
	/* if a shader block of requested type wasn't found, log an error */
	if (MALI_FALSE == shader_block_found)
	{
		/* Did not find any blocks matching this shader type */
		bs_set_error(&so->log, BS_ERR_LP_SYNTAX_ERROR, "The binary provided did not contain any shaders of the required type. ");
		error = MALI_ERR_FUNCTION_FAILED;
	}
	
	/* if everything went smoothly, stash the binary in shader object for later use in linker */
	if (MALI_ERR_NO_ERROR == error)
	{
		__mali_shader_binary_store_binary_block(so, binary_data, binary_length);
	}
	return error;
}
