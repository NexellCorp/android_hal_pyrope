/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_gb_vs_attribs.h"
#include "regs/MALIGP2/mali_gp_vs_config.h" /* needed for stream-config */
#include "../gles_gb_interface.h"
#include "gles_instrumented.h"

#include <gles_util.h>
#include <util/mali_math.h>

#include "gles_gb_buffer_object.h"
#include "../gles_buffer_object.h"
#include "shared/m200_gp_frame_builder_inlines.h"

MALI_STATIC void* _gles_gb_allocate_and_fill_temp_memory(const gles_gb_context *gb_ctx, GLsizeiptr size, const GLvoid *data, u32 *temp_mem_addr, u32 typesize)
{
	void *temp_mem_ptr;

	MALI_DEBUG_ASSERT_POINTER(data);

	temp_mem_ptr = _mali_mem_pool_alloc(gb_ctx->frame_pool, size, temp_mem_addr);
	if (NULL == temp_mem_ptr) return NULL;

	MALI_DEBUG_PRINT(2, ("+++++ _gles_gb_allocate_and_fill_temp_memory: allocated %d bytes @ %p\n", size, *temp_mem_addr));

	_mali_sys_memcpy_cpu_to_mali(temp_mem_ptr, (GLvoid*)data, size, typesize);
	return temp_mem_ptr;
}

/* a temp block declares a memory area spanning several overlapping CPU-side streams */
typedef struct temp_block
{
	const u8* pointer;
	u32 length;
	u32 typesize;

	/* will be filled after allocation on the frame pool */
	void* mem_ptr;
	u32 mem_addr;
} temp_block;

/* a block mapping declares how a stream map to a tempblock. */
typedef struct blockmapping
{
	s32 temp_block_id; /* telling which tempblock this stream maps to. Or -1 if no mapping exists */
	s32 offset;        /* telling at which offset this stream is starting, offset from the temp block's start address */
} blockmapping;

/**
 * Adds a new tempblock to the list of tempblocks based on some input stream.
 * @param tempblocks        The array of tempblocks (size=GLES_VERTEX_ATTRIB_COUNT). INOUT parameter.
 * @param temp_blocks_used  A number telling how many tempblocks are in use. Will be incremented by one. INOUT parameter.
 * @param pointer           The pointer to the memory the tempblock will contain
 * @param length            The size of the tempblock in bytes
 */
MALI_STATIC_INLINE void tempblock_create(temp_block* tempblocks, u32* temp_blocks_used, const u8* pointer, u32 length)
{
	MALI_DEBUG_ASSERT(*temp_blocks_used < GLES_VERTEX_ATTRIB_COUNT, ("Too many tempblocks used"));
	tempblocks[*temp_blocks_used].pointer = pointer;
	tempblocks[*temp_blocks_used].length = length;

	(*temp_blocks_used)++;
}

/**
 * Merge interleaved blocks based on info set by the _gles_gb_modify_attribute_stream function
 *
 * @param gb_ctx           The geometry backend context
 * @param streams          The gles state describing the stream setup
 * @param vertex_count     The number of vertices used for drawing.
 * @param tempblocks       An array (size=GLES_VERTEX_ATTRIB_COUNT) of struct temp blocks, that will be filled out by this function. Return parameter.
 * @param maps             An array (size=GLES_VERTEX_ATTRIB_COUNT) of struct blockmapping, modified out by this function. In/out parameter. 
 */

MALI_STATIC mali_err_code optimize_non_vbo_interleaved_attribute_streams(
                        const gles_gb_context *gb_ctx,
                        temp_block* tempblocks,
						blockmapping* maps
	)
{
	u32 i, j;
	u32 num_tempblocks_used = 0;
	u32 num_attribute_temp_block_candidates;
	u32 attribute_temp_block_candidates;


       u32 candidate_list_size = 0;

	const gles_vertex_attrib_array* streams = gb_ctx->vertex_array->attrib_array;
	u32 vertex_count = gb_ctx->parameters.vertex_count;

	MALI_DEBUG_ASSERT_POINTER(streams);

	MALI_DEBUG_ASSERT(gb_ctx->num_attribute_temp_block_candidates >= 2, ("VBOs should not enter optimize_non_vbo_interleaved_attribute_streams"));

	/* use local variable for code efficiency */
	num_attribute_temp_block_candidates = gb_ctx->num_attribute_temp_block_candidates;
	attribute_temp_block_candidates = gb_ctx->attribute_temp_block_candidates;

	/* build a list of all the candidates */
	for( i = 0; i < gb_ctx->prs->binary.attribute_streams.count; i++ )
	{
		int bs_stream = gb_ctx->prs->binary.attribute_streams.info[i].index;
		int gl_stream;
		
		MALI_DEBUG_ASSERT( bs_stream != -1, ("The binary shader has set up a stream that isn't available. This should never happen"));
		MALI_DEBUG_ASSERT_RANGE( bs_stream, 0, MALIGP2_MAX_VS_INPUT_REGISTERS - 1);

		gl_stream = gb_ctx->prs->reverse_attribute_remap_table[ bs_stream ];

		/* only consider used streams which are set to valid values */
		if( 0 != ( attribute_temp_block_candidates & (1<<gl_stream)) && streams[gl_stream].pointer != NULL)
		{

	        /* create tempblocks, merge candidates */
		u32 streamindex = gl_stream;

		int streamstride = streams[streamindex].stride;
		u32 typesize = streams[streamindex].elem_bytes;
		int ptr_offset = streamstride * gb_ctx->parameters.start_index;
		u32 streamlength = (streamstride * (vertex_count - 1)) + (streams[streamindex].size * typesize);
		u32 sptr = (u32)streams[streamindex].pointer + ptr_offset;

		/* always add the new tempblock, make the candidate point to it */
		maps[streamindex].temp_block_id = num_tempblocks_used;
		maps[streamindex].offset = 0;

		tempblock_create(tempblocks, &num_tempblocks_used, (const u8*)sptr, streamlength);

		/* but check if this stream fits into any existing tempblocks instead */
		for(j = 0; j < num_tempblocks_used-1; j++) /* -1 to exclude the one we just added */
		{
			tempblocks[j].typesize = typesize;

			/* check if there is an overlap */
			if(  /* if the streampointer is within any existing block */
			     sptr >= (u32)tempblocks[j].pointer &&
				 sptr <  (u32)tempblocks[j].pointer + tempblocks[j].length
			  )
			{
				/* merge the modified stream with the block in question by simply extending the matching blocks' length if needed*/
				int offset = sptr - (u32)tempblocks[j].pointer;
				maps[streamindex].temp_block_id = j;
				maps[streamindex].offset += offset;
				if( sptr + streamlength > (u32)tempblocks[j].pointer + tempblocks[j].length)
				{
					tempblocks[j].length = streamlength + offset;
				}

				num_tempblocks_used--; /* remove the tempblock created earlier */
				break;
			}
			if(  /* if the existing block is within a new pointer */
			     (u32)tempblocks[j].pointer >= sptr &&
				 (u32)tempblocks[j].pointer <  sptr + streamstride 
			  )
			{
				/* move the tempblock backwards to the new pointer, update length of block, update all offsets of streams referring to this tempblock */
				u32 k;
				int negativeoffset = (u32)tempblocks[j].pointer - sptr;

				/* move pointer backwards, update block length accordingly */
				tempblocks[j].pointer = (const u8*) sptr;
				if( (u32) tempblocks[j].length + negativeoffset > streamlength)
				{
					tempblocks[j].length += negativeoffset;
				} else {
					tempblocks[j].length = streamlength;
				}


				/* offset all streams referring to this block already */
				for(k = 0; k < GLES_VERTEX_ATTRIB_COUNT; k++)
				{
					if(maps[k].temp_block_id == (int)j) maps[k].offset += negativeoffset;
				}

				/* assign the stream to this block, but keep the offset set earlier */
				maps[streamindex].temp_block_id = j;

				num_tempblocks_used--; /* remove the tempblock created earlier */
				break;
			}
		} /* for each j=block */
				candidate_list_size++;
				if(candidate_list_size == num_attribute_temp_block_candidates) break; 
			} 
		}

	/* upload tempblocks into the frame pool */
	for(i = 0; i < num_tempblocks_used; i++)
	{
		_profiling_enter( INST_GL_NON_VBO_DATA_COPY_TIME );
		{
			tempblocks[i].mem_ptr = _gles_gb_allocate_and_fill_temp_memory(gb_ctx, tempblocks[i].length, tempblocks[i].pointer, &tempblocks[i].mem_addr, tempblocks[i].typesize);
		}
		_profiling_leave( INST_GL_NON_VBO_DATA_COPY_TIME);

		MALI_CHECK_NON_NULL( tempblocks[i].mem_ptr, MALI_ERR_OUT_OF_MEMORY );
	}

	MALI_SUCCESS;
}

/**
 * Compute the GP2 stream specifier given the stream attribute structure.
 *
 * @param attrib	The stream attribute structure.
 * @param stride	The actual stride may differ from the attrib->stride
 *					if the stride was too large for GP2.
 */
MALI_STATIC u32 _gles_gb_get_input_stream_spec( const gles_vertex_attrib_array * const attrib, const GLsizei stride )
{
	const int comma_loc_bits = 6;

#if HARDWARE_ISSUE_4591
	const u16 stream_type[] = { GP_VS_VSTREAM_FORMAT_1_FIX_S8,
								GP_VS_VSTREAM_FORMAT_1_FIX_S8  | ( 7 << comma_loc_bits ),
								GP_VS_VSTREAM_FORMAT_1_FIX_U8,
								GP_VS_VSTREAM_FORMAT_1_FIX_U8  | ( 8 << comma_loc_bits ),
								GP_VS_VSTREAM_FORMAT_1_FIX_S16,
								GP_VS_VSTREAM_FORMAT_1_FIX_S16 | ( 15 << comma_loc_bits ),
								GP_VS_VSTREAM_FORMAT_1_FIX_U16,
								GP_VS_VSTREAM_FORMAT_1_FIX_U16 | ( 16 << comma_loc_bits ),
								GP_VS_VSTREAM_FORMAT_1_FP32,
								GP_VS_VSTREAM_FORMAT_1_FP32,
								GP_VS_VSTREAM_FORMAT_1_FIX_S32 | ( 16 << comma_loc_bits ),
								GP_VS_VSTREAM_FORMAT_1_FIX_S32 | ( 16 << comma_loc_bits )	};
#else
	const u16 stream_type[] = { GP_VS_VSTREAM_FORMAT_1_FIX_S8,
								GP_VS_VSTREAM_FORMAT_1_NORM_S8,
								GP_VS_VSTREAM_FORMAT_1_FIX_U8,
								GP_VS_VSTREAM_FORMAT_1_NORM_U8,
								GP_VS_VSTREAM_FORMAT_1_FIX_S16,
								GP_VS_VSTREAM_FORMAT_1_NORM_S16,
								GP_VS_VSTREAM_FORMAT_1_FIX_U16,
								GP_VS_VSTREAM_FORMAT_1_NORM_U16,
								GP_VS_VSTREAM_FORMAT_1_FP32,
								GP_VS_VSTREAM_FORMAT_1_FP32,
								GP_VS_VSTREAM_FORMAT_1_FIX_S32 | ( 16 << comma_loc_bits ),
								GP_VS_VSTREAM_FORMAT_1_FIX_S32 | ( 16 << comma_loc_bits ),	};
#endif
	MALI_DEBUG_ASSERT_POINTER( attrib );
	{
		const mali_bool normalized	= attrib->normalized;

		const u32 key = (attrib->elem_type << 1) | normalized;

		u32 stream_spec = 0;

		MALI_DEBUG_ASSERT( normalized == (normalized & 1), ("Illegal boolean value") );
		MALI_DEBUG_ASSERT( stride > 0, ("illegal stride %d", stride) );
		MALI_DEBUG_ASSERT( (stride & MALI_STRIDE_MASK) == stride, ("invalid stride %d", stride) );
		MALI_DEBUG_ASSERT_RANGE( attrib->size, 1, 4 );
		MALI_DEBUG_ASSERT( attrib->elem_type < MALI_ARRAY_SIZE(stream_type), ("Illegal type value") );

		stream_spec = (u32) stream_type[key];

		/* Merge the vector-length
		 */
		stream_spec |= attrib->size - 1;

		/* Encode the stride (low bits only)
		 */
		stream_spec |= (stride & MALI_STRIDE_MASK) << MALI_STRIDE_SHIFT;

		return stream_spec;
	}
}

/* compact streams with too high stride */
MALI_STATIC MALI_CHECK_RESULT mali_err_code _gles_gb_compact_stream(
	const gles_gb_context *gb_ctx,
	const void *mem_ptr,
	mali_mem_ref *mem_ref,
	GLint size,
	GLenum type,
	GLsizei new_stride,
	GLsizei *stride,
	u32 *mali_addr,
	int *mem_offset)
{
	int i;
	mali_err_code err = MALI_ERR_NO_ERROR;
	const int type_size = _gles_get_type_size(type);
	const int vertex_count = gb_ctx->parameters.vertex_count;
	int old_stride = *stride;
	const void *old_mem_ptr = mem_ptr;
	u32 block_offset = 0;
	void *new_mem_ptr;
	u32 new_mem_addr;

	MALI_DEBUG_ASSERT_POINTER(gb_ctx);
	MALI_DEBUG_ASSERT_POINTER(stride);
	MALI_DEBUG_ASSERT_POINTER(mali_addr);
	MALI_DEBUG_ASSERT_POINTER(mem_offset);
	MALI_DEBUG_ASSERT( (NULL==mem_ptr) ^ (NULL==mem_ref), ("Input data is both a VBO and not a VBO") );

	new_mem_ptr = _mali_mem_pool_alloc(gb_ctx->frame_pool, (vertex_count - 1) * new_stride + size*type_size, &new_mem_addr);
	if (NULL == new_mem_ptr) return MALI_ERR_OUT_OF_MEMORY;

	if ( mem_ref != NULL )
	{old_mem_ptr = _mali_mem_ptr_map_area( mem_ref->mali_memory,
		                                      *mem_offset,
		                                      (vertex_count - 1) * old_stride + size*type_size,
		                                      64,
		                                      MALI_MEM_PTR_READABLE );
	}
	else
	{
		/* In the case of a tempblock, we already have the mali memory block
		 * mapped, but we need to make sure we offset into the memory block the
		 * correct amount.
		 */
		block_offset = *mem_offset;
	}


	MALI_DEBUG_ASSERT_POINTER(old_mem_ptr);

	for (i = 0; i < vertex_count; ++i)
	{
		/* Note! copy from LE memory to LE memory using BE CPU  */
		{
			u8* src = (u8*)((u32)old_mem_ptr + block_offset + i*old_stride);
			u8* dst = (u8*)((u32)new_mem_ptr + i*new_stride);

			_mali_sys_memcpy_mali_to_mali(dst, src, size, type_size);
		}
	}

	if ( mem_ref != NULL )
	{
		_mali_mem_ptr_unmap_area( mem_ref->mali_memory );
	}

	*mali_addr  = new_mem_addr;
	*stride     = new_stride;
	*mem_offset = 0;

	MALI_INSTRUMENTED_LOG((_instrumented_get_context(), MALI_LOG_LEVEL_WARNING,
	                       "Attribute stride too large or element crossed a cache boundary, repacking was necessary"));

	return err;
}

/*
 * Set-up the input streams
 */
MALI_CHECK_RESULT mali_err_code _gles_gb_setup_input_streams( gles_context * const ctx, u32 *streams, u32 *stride_array)
{
	u32 i;
	mali_err_code err = MALI_ERR_NO_ERROR;
	int vertex_count;
	temp_block tempblocks[GLES_VERTEX_ATTRIB_COUNT];
	blockmapping maps[GLES_VERTEX_ATTRIB_COUNT];
	mali_bool  is_maps_initialized = MALI_FALSE;

	gles_gb_context *gb_ctx;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	gb_ctx = _gles_gb_get_draw_context( ctx );

	MALI_DEBUG_ASSERT_POINTER( gb_ctx );
	MALI_DEBUG_ASSERT_POINTER( gb_ctx->prs );
	MALI_DEBUG_ASSERT_POINTER( gb_ctx->prs->attribute_remap_table );
	MALI_DEBUG_ASSERT_POINTER( streams );
	MALI_DEBUG_ASSERT_POINTER( stride_array );

	vertex_count = gb_ctx->parameters.vertex_count;

#if HARDWARE_ISSUE_4326
    {
        /* Clear the input streams. Since we will only use every other stream (see code below),
         * we need to clear the ones we'll skip over so that no data is read from them.
         */
        const gles_gb_program_rendering_state* prs = gb_ctx->prs->gb_prs;
        const u32 num_repacked_input_streams = prs->num_input_streams*2;
        u32 *stream_in = streams;
        MALI_DEBUG_ASSERT( num_repacked_input_streams <= GLES_VERTEX_ATTRIB_COUNT, ("More input streams than expected for workaround.") );
        for ( i = 0; i < num_repacked_input_streams; i++ )
        {
            stream_in[0] = _SWAP_ENDIAN_U32_U32(0x0);
            stream_in[1] = _SWAP_ENDIAN_U32_U32(GP_VS_VSTREAM_FORMAT_NO_DATA);
            stream_in += 2;
        }
    }
#endif
#if HARDWARE_ISSUE_8733
	{
		/* Clear the input streams. Since we will use one extra,
		 * we need to clear all so that no data is read from them.
		 */
		u32 *stream_in = streams;
		for ( i = 0; i < GLES_VERTEX_ATTRIB_COUNT; i++ )
	 	{
	 		stream_in[0] = 0x0;
	 		stream_in[1] = GP_VS_VSTREAM_FORMAT_NO_DATA;
	 		stream_in += 2;
	 	}
	}
#endif


	/* if there are no temp block candidates available (or only one), no point in trying to merge anything.
	 * No temp blocks will be allocated, and the code will simply allocate a temporary VBO for each stream that isn't in a VBO already. */
	if(gb_ctx->num_attribute_temp_block_candidates >= 2)
	{
                /* clear some internal data structures */
		for( i = 0; i < GLES_VERTEX_ATTRIB_COUNT; i++ )
		{
			maps[i].temp_block_id = -1;
			/*	maps[i].offset = 0; not needed */ 
		}


		/* optimize non-vbo streams by merging them into temp blocks */
		optimize_non_vbo_interleaved_attribute_streams( gb_ctx, tempblocks, maps);

		is_maps_initialized = MALI_TRUE;
	}

	/* setup input streams to mali GP */
	for( i = 0; i < gb_ctx->prs->binary.attribute_streams.count; i++ )
	{
		int bs_stream = gb_ctx->prs->binary.attribute_streams.info[i].index;
		int gl_stream;
		gles_vertex_attrib_array * attrib_array;
		mali_mem_ref *mem = NULL;
		void *mem_ptr     = NULL;
		u32 mem_addr      = 0;
		int mem_offset    = 0;

        /* Explicitly non-const since stream compaction
         * may override the stride.
         */
        GLsizei stride    = 0;
		u32 stream_spec;

		MALI_DEBUG_ASSERT( bs_stream != -1, ("The binary shader has set up a stream that isn't available. This should never happen"));
		MALI_DEBUG_ASSERT_RANGE( bs_stream, 0, MALIGP2_MAX_VS_INPUT_REGISTERS - 1);

		gl_stream = gb_ctx->prs->reverse_attribute_remap_table[ bs_stream ];

		MALI_DEBUG_ASSERT( gl_stream != -1, ("The binary shader has a stream which is not exposed in GLES. This should never happen"));
		MALI_DEBUG_ASSERT_RANGE( gl_stream, 0, GLES_VERTEX_ATTRIB_COUNT - 1 );

		attrib_array = &gb_ctx->vertex_array->attrib_array[ gl_stream ];

		stream_spec = attrib_array->stream_spec;

		if ( MALI_FALSE == attrib_array->enabled )
		{
			/* use the default attribute value for all vertices by setting up stride = 0*/
			u8* stream_start = (u8*) attrib_array->value;
			int stream_len = 4 * sizeof(float);

 			_profiling_enter( INST_GL_NON_VBO_DATA_COPY_TIME );
 			{
				mem_ptr = _gles_gb_allocate_and_fill_temp_memory( gb_ctx, stream_len, stream_start, &mem_addr, 4 );
			}
			_profiling_leave( INST_GL_NON_VBO_DATA_COPY_TIME );

			MALI_CHECK_NON_NULL( mem_ptr, MALI_ERR_OUT_OF_MEMORY );
			mem_offset = 0;

			/* the format of the disabled attributes is always 4-component float with default stride */
			stream_spec = GP_VS_VSTREAM_FORMAT_4_FP32;
		}
		else
		{
			/* Use actual data per vertex
			 */
			const GLint	size	= attrib_array->size;
			const GLenum type	= attrib_array->type;

			const int type_size  = attrib_array->elem_bytes;
			const u32 minimum_stride = type_size * size;
			u32 repack_stride = minimum_stride;
			mali_bool repack_stream = MALI_FALSE;
			mali_base_frame_id frameid;

			stride 		= attrib_array->stride;

			if ( 0 != attrib_array->buffer_binding )
			{
				/* use a VBO */
				MALI_DEBUG_ASSERT_POINTER( attrib_array->vbo );	/* verify consistent state */

				/* If the vbo data is NULL, we ran out
				 * of memory when allocating buffer data.
				 */
				if( NULL == attrib_array->vbo->gb_data ) 
				{ 
					/* no stream data possible, just disable the stream. This will yield <0,0,0,1> */
					streams[ GP_VS_CONF_REG_INP_ADDR( bs_stream ) ] = 0x0;
					streams[ GP_VS_CONF_REG_INP_SPEC( bs_stream ) ] = GP_VS_VSTREAM_FORMAT_NO_DATA;
					continue;
				} 
				
				MALI_DEBUG_ASSERT_POINTER( attrib_array->vbo->gb_data );	
				MALI_DEBUG_ASSERT_POINTER( attrib_array->vbo->gb_data->mem );	
				
				mem      = attrib_array->vbo->gb_data->mem;
				mem_addr = _mali_mem_mali_addr_get( mem->mali_memory, 0 );

				/* ptr used as offset */
				mem_offset = (int)attrib_array->pointer;

				/* add the offset fom the pointer in the state to the first referenced vertex */
				mem_offset += stride * gb_ctx->parameters.start_index;

				/* addref memory */
				frameid = _mali_frame_builder_get_current_frame_id(gb_ctx->frame_builder);
				if ( attrib_array->vbo->gb_data->last_used_frame != frameid )
				{
					err = _mali_frame_builder_add_callback( gb_ctx->frame_builder, 
					                                        MALI_STATIC_CAST(mali_frame_cb_func)_mali_mem_ref_deref,
					                                        MALI_STATIC_CAST(mali_frame_cb_param)mem );
					if (MALI_ERR_NO_ERROR != err) break;
					_mali_mem_ref_addref(mem);
					attrib_array->vbo->gb_data->last_used_frame = frameid;
				}

			}
			else  if ( !is_maps_initialized || maps[gl_stream].temp_block_id == -1 )
			{
				/* Create a temporary VBO containing the data from the client-side
				 * pointer
				 */
				int ptr_offset = stride * gb_ctx->parameters.start_index;
				const u8 *stream_start = ((const u8*)attrib_array->pointer) + ptr_offset;
				int stream_len = (stride * ( vertex_count - 1)) + (size * type_size);

				MALI_DEBUG_ASSERT( vertex_count > 0, ("empty draw-calls should be discarded earlier"));

				if(attrib_array->pointer != NULL) 
				{
					_profiling_enter( INST_GL_NON_VBO_DATA_COPY_TIME );
					{
						mem_ptr = _gles_gb_allocate_and_fill_temp_memory( gb_ctx, stream_len, stream_start, &mem_addr, type_size );
					}
					_profiling_leave( INST_GL_NON_VBO_DATA_COPY_TIME );
				} 
				else 
				{
					/* handle NULL pointer non-VBO streams by not setting up anything */
					mem_addr = 0;
					mem_offset = 0;
					stream_spec = GP_VS_VSTREAM_FORMAT_NO_DATA;
				}

				MALI_CHECK_NON_NULL( mem_ptr, MALI_ERR_OUT_OF_MEMORY );
			}
			else
			{
				/* Assign stream to a tempblock
				 */
				int blockid = maps[gl_stream].temp_block_id;
				mem_ptr = tempblocks[blockid].mem_ptr;
				mem_addr = tempblocks[blockid].mem_addr;
				mem_offset = maps[gl_stream].offset;
			}
			/* Check if the stride is too large for MaliGP2.
			 *
			 * If too large, we must repack the stream and
			 * recompute the GP2 stream specifier.
			 */
			if ( 0 != (stride & ~MALI_STRIDE_MASK))
			{
				repack_stream = MALI_TRUE;
			}

#if HARDWARE_ISSUE_8241
			/* If a stream is normalized and a stream element crosses a cache line,
			 * there can be GP problems.
			 */
			if ( attrib_array->normalized )
			{
				/*
				 * The stride needs to be set to at least the size of the data type
				 * times the number of components per stream element.  If this is a
				 * power of two, we are ok, because this will never cross the cache
				 * boundary.  If it is not a power of two, we need to bump this up to
				 * the next larger power of two.
				 *
				 * Note: mem_ptr is in Mali memory and is 64-byte aligned.
				 *
				 * We also need to ensure that the starting offset for this stream
				 * allows for proper alignment of the data in the data cache line.
				 *
				 * So... ensure that when we advance one stride distance that the low
				 * bits are identical.  If not, successive elements will wander around
				 * and eventually cross the cache boundary.
				 */
				int fixed_stride = (stride & 63) > 0 ? stride : minimum_stride;
				mali_bool stable = ((fixed_stride / minimum_stride) * minimum_stride == fixed_stride) ||
					((64 / (fixed_stride & 63)) * (fixed_stride & 63) == 64);
				u32 cycle = (63 & fixed_stride) & minimum_stride;

				if ( cycle != 0 || !stable )
				{
					repack_stream = MALI_TRUE;
					repack_stride =_mali_ceil_pow2(minimum_stride);
				}
			}
#endif

			if (repack_stream)
			{

				err = _gles_gb_compact_stream( gb_ctx, mem_ptr, mem, size, type, repack_stride,
				                               &stride, &mem_addr, &mem_offset );

				if ( MALI_ERR_NO_ERROR != err ) break;

				mali_statebit_set( &ctx->state.common, MALI_STATE_GLESATTRIB0_STREAM_SPEC_DIRTY + gl_stream );
			}

			/* If the stream attribute has been altered in
			 * any way we need to recompute the GP2 stream
			 * specifier.
			 */
			if ( mali_statebit_get( &ctx->state.common, MALI_STATE_GLESATTRIB0_STREAM_SPEC_DIRTY + gl_stream ) )
			{
				stream_spec = _gles_gb_get_input_stream_spec( attrib_array, stride );

				attrib_array->stream_spec = stream_spec;

				mali_statebit_unset( &ctx->state.common, MALI_STATE_GLESATTRIB0_STREAM_SPEC_DIRTY + gl_stream );
			}
		}

#if HARDWARE_ISSUE_4326
		/* R0P1 HW will ruin every other stream in certain circumstances. The only way to work around this issue is to
		 * compensate by using every other stream only. The GLES driver handles this by:
		 *  - limiting the amount of available attribute streams to 8 (index 0..7) instead of 16 (index 0..15)
		 *    -- This is handled by defining GLES_VERTEX_ATTRIB_COUNT to half the otherwise available range
		 *    -- As a side effect, the number of texture units are reduced to 2.
		 *  - rewriting the vertex shader to read from stream 0,2,4..14 instead of stream 0,1,2..7. Symbols are not updated.
		 *    -- This is handled for gles1 in the shader generator backend
		 *    -- This is handled for gles2 in glLinkProgram
		 *  - implicitly mapping attribute stream 0,1,2..7 as seen by GLES to stream 0,2,4..14 as seen by the HW
		 *    -- This happens right here
		 */

		bs_stream *= 2;
#endif

		/* correct byte order in mali memory if needed */

		streams[ GP_VS_CONF_REG_STREAM_ADDR( bs_stream ) ] = _SWAP_ENDIAN_U32_U32((u32)mem_addr + mem_offset);
		streams[ GP_VS_CONF_REG_STREAM_SPEC( bs_stream ) ] = _SWAP_ENDIAN_U32_U32(stream_spec);
        stride_array[bs_stream] = stride;
	}

	
	/* if there is one stream and no stream symbols, we have a padded stream. This must always be initialized. See bugzilla 11170 for details. */
	if(gb_ctx->prs->binary.attribute_streams.count == 0 && gb_ctx->prs->binary.vertex_pass.flags.vertex.num_input_registers == 1 )
	{
		streams[0] = 0x0;
		streams[1] = GP_VS_VSTREAM_FORMAT_NO_DATA;
	}

	MALI_ERROR( err );
}

void _gles_gb_modify_attribute_stream( gles_context * const ctx, struct gles_vertex_attrib_array* streams, u32 modified)
{
	gles_gb_context *gb_ctx;
	int minimum_stride;
	u32 bitmask = 1 << modified;

	MALI_DEBUG_ASSERT_POINTER( ctx );

	gb_ctx = _gles_gb_get_draw_context( ctx );

	MALI_DEBUG_ASSERT_POINTER(gb_ctx);
	MALI_DEBUG_ASSERT_POINTER(streams);
	MALI_DEBUG_ASSERT_RANGE((int)modified, 0, GLES_VERTEX_ATTRIB_COUNT-1);

	/* If this stream already was a candidate, lower the number by 1
	 */
	if( 0 != ( gb_ctx->attribute_temp_block_candidates & bitmask ) ) gb_ctx->num_attribute_temp_block_candidates--;
	gb_ctx->attribute_temp_block_candidates &= ~bitmask;

	minimum_stride = streams[modified].elem_bytes * streams[modified].size;

	/* check if this stream may be merged at all. Only non-vbo streams and streams not using the minimum stride can be merged */
	if( streams[modified].buffer_binding != 0 || streams[modified].stride <= minimum_stride || streams[modified].enabled == MALI_FALSE) return;

	gb_ctx->num_attribute_temp_block_candidates++;
	gb_ctx->attribute_temp_block_candidates |= bitmask;
}

