/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_gb_buffer_object.h"
#include "gles_gb_vertex_rangescan.h"
#include "../gles_gb_interface.h"
#include "../gles_buffer_object.h"
#include "gles_geometry_backend_context.h"
#include "../gles_util.h"
#include <math.h>

#include "base/mali_runtime.h"

#define LD 4

#define GLES_CACHELINES_PER_BUCKET 2
#define GLES_CACHE_LINE_SIZE 64
#define GLES_NODE_OVERHEAD ( 4 + 4 ) /* 4 bytes for next-pointer + 4 bytes for num */
#define GLES_RANGES_PER_BUCKET ( ( ( GLES_CACHELINES_PER_BUCKET * GLES_CACHE_LINE_SIZE ) - GLES_NODE_OVERHEAD ) / sizeof( struct gles_range_cache_data ) )


#define GLES_BB_PER_BUCKET  0x100

/*It is a threshold for average distance between the indices in the index buffer*/
#define COHERENCE_THRESHOLD               300
typedef struct gles_range_cache_data
{
	/* search data*/
	u32 count;
	u32 offset;
	u32 type;
	/* useful data*/
	u16 range_count;
    u32 coherence;
	index_range* idx_range_cache;
} gles_range_cache_data;


#ifdef MALI_BB_TRANSFORM
/* bounding box cache structure*/
typedef struct gles_bb_cache_data
{
	/* search data*/
	u32 count;
	u32 offset;
	u32 stride;
	/* useful data*/
    u32 node_cnt;
	bb_binary_node *bb_cache;
} gles_bb_cache_data;
#endif

#if MALI_BIG_ENDIAN
MALI_STATIC void _gles_gl_buffer_data_replace(struct gles_gb_buffer_object_data *vbo_data, GLintptr offset, GLsizeiptr size, const GLvoid * data);
#endif

#if 0
MALI_STATIC void dump_buffer(u8 *buffer, u32 size)
{
	u32 i;
	u32 *p = (u32*)buffer;
	_mali_sys_printf("Buffer %08x %06d----------------------\n", buffer, size);
	for(i = 0; i < size; i+= 4){
		if(i != 0 && (i % 16) == 0)
			_mali_sys_printf("\n");
		_mali_sys_printf("%08x ", *p++);
	}

	_mali_sys_printf("\n--------------------------------------------\n");
}
#endif

/*
   @brief Function compares data in a cache element with the provided element (by search part)
             and assigns the useful part of data to destination
   @param destination - first comparison operand
   @param source - second comparison operand
   @return  0 if key and data key are identical, not 0 otherwise
*/
MALI_STATIC u32 _gles_gb_range_compare(void*  destination, const void* const source)
{
	gles_range_cache_data * obj1 = destination;
	const gles_range_cache_data * const obj2 = source;

	
	MALI_DEBUG_ASSERT_POINTER(obj1);
	MALI_DEBUG_ASSERT_POINTER(obj2);

	if (obj1->count == obj2->count && obj1->offset == obj2->offset && obj1->type == obj2->type)
	{
		obj1->range_count = obj2->range_count;
		obj1->idx_range_cache = obj2->idx_range_cache;
        obj1->coherence = obj2->coherence;
		return 0;
	}

	return 1;
}


#ifdef MALI_BB_TRANSFORM

/*
   @brief Function compares data in a cache element with the provided element (by search part)
             and assigns the useful part of data to destination
   @param destination - first comparison operand
   @param source - second comparison operand
   @return  0 if key and data key are identical, not 0 otherwise
*/
MALI_STATIC u32 _gles_gb_bb_compare(void*  destination, const void* const source)
{
	gles_bb_cache_data * obj1 = destination;
	const gles_bb_cache_data * const obj2 = source;

	MALI_DEBUG_ASSERT_POINTER(destination);
	MALI_DEBUG_ASSERT_POINTER(source);

	if (obj1->count == obj2->count && obj1->offset == obj2->offset 
		&& obj1->stride == obj2->stride)
	{
		obj1->bb_cache = obj2->bb_cache;
        obj1->node_cnt = obj2->node_cnt;
		return 0;
	}

	return 1;
}
#endif

/*
   @brief Hash function for the range cache
   @param count  number of elements 
   @param offset offset in index vbo  
   @returns  hash value
*/
MALI_STATIC_INLINE u32 _data_range_hash_function(u32 count, u32 offset)
{
    return  count ^ (offset>> 2 );
}

/*
   @brief Function allocates an element in the range cache
   @param cache_element a pointer to range cache data structure
   @param key The key value to copare the data key with
   @returns  A pointer to allocated data
*/
MALI_STATIC void* _gles_gb_allocate_range_cache_entry(void* cache_element)
{

	gles_range_cache_data* data = (gles_range_cache_data*)cache_element;
	gles_range_cache_data* pdata;

	MALI_DEBUG_ASSERT_POINTER(data);
	MALI_DEBUG_ASSERT_POINTER(data->idx_range_cache);
	MALI_DEBUG_ASSERT(data->range_count > 0, ("range count must be greater then zero"));
	
	
	pdata = _mali_sys_malloc(sizeof(gles_range_cache_data) + ( sizeof( index_range ) * data->range_count ));
	if (pdata == NULL)
	{
		return NULL;
	}
	
	pdata->count = data->count;
	pdata->offset = data->offset;
	pdata->type = data->type;
	pdata->range_count = data->range_count;

	pdata->idx_range_cache =  (index_range*) (&pdata[1]); /* a pointer to the data after the gles_range_cache_data struct */
 	
	_mali_sys_memcpy( pdata->idx_range_cache, data->idx_range_cache, (data->range_count) * sizeof(index_range));
	
    pdata->coherence = data->coherence;
	
	return pdata;
}

#ifdef MALI_BB_TRANSFORM
/*
   @brief Function allocates an element in the bb cache
   @param cache_element a pointer to range cache data structure
   @param key The key value to copare the data key with
   @returns  A pointer to allocated data
*/
MALI_STATIC void* _gles_gb_allocate_bb_cache_entry(void* cache_element)
{

	gles_bb_cache_data* data = (gles_bb_cache_data*)cache_element;
	gles_bb_cache_data* pdata = _mali_sys_malloc(sizeof(gles_bb_cache_data));

	if (pdata == NULL)
	{
		return NULL;
	}

	MALI_DEBUG_ASSERT_POINTER(data);
	MALI_DEBUG_ASSERT_POINTER(data->bb_cache);
	
	pdata->count = data->count;
	pdata->offset = data->offset;
	pdata->stride= data->stride;
    pdata->bb_cache = data->bb_cache;
    pdata->node_cnt = data->node_cnt;
	
	return pdata;
}
#endif

/*
   @brief Function releases an element in the range cache
   @param cache_element a pointer to range cache data structure
*/
MALI_STATIC void _gles_gb_release_range_cache_entry(void* cache_element)
{

	gles_range_cache_data* data = (gles_range_cache_data*)cache_element;

	MALI_DEBUG_ASSERT_POINTER(cache_element);

	_mali_sys_free(data);
}

#ifdef MALI_BB_TRANSFORM
MALI_STATIC void _gles_gb_release_bb_cache_entry(void* cache_element)
{
    gles_bb_cache_data *data = (gles_bb_cache_data*)cache_element;
    MALI_DEBUG_ASSERT_POINTER(cache_element);

    if(NULL != data->bb_cache)
    {
        _mali_sys_free(data->bb_cache);
    }

    _mali_sys_free(data);
}
#endif

struct gles_gb_buffer_object_data *_gles_gb_buffer_data(mali_base_ctx_handle base_ctx, GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage)
{
	struct gles_gb_buffer_object_data *temp;

	/* silence some warnings */
	MALI_IGNORE(target);
	MALI_IGNORE(usage);
	temp = _mali_sys_calloc( sizeof( struct gles_gb_buffer_object_data ), 1 );
	if (NULL == temp) return NULL;


	temp->range_cache = gles_gb_cache_alloc(_gles_gb_allocate_range_cache_entry, _gles_gb_release_range_cache_entry,
	                                                                     GLES_GB_INDEX_RANGE_CACHE_SIZE, GLES_RANGES_PER_BUCKET);
	if ( NULL == temp->range_cache )
	{
		_mali_sys_free( temp );
		temp = NULL;
		return NULL;
	}

	gles_gb_cache_enable_oom_invalidation(temp->range_cache, 10 * 1024  /* 10 mb */, sizeof(gles_range_cache_data));

#ifdef MALI_BB_TRANSFORM
	temp->bounding_box_cache = gles_gb_cache_alloc(_gles_gb_allocate_bb_cache_entry, _gles_gb_release_bb_cache_entry, GLES_BB_HASHTABLE_SIZE, GLES_BB_PER_BUCKET);
	if ( NULL == temp->bounding_box_cache)
	{
		gles_gb_cache_free( temp->range_cache );
		_mali_sys_free( temp );
		temp = NULL;
		return NULL;
	}

	gles_gb_cache_enable_oom_invalidation(temp->bounding_box_cache, 1 * 1024 /* 1 mb */, sizeof(gles_bb_cache_data));
#endif

	/* 32bit align pointer, since this is required by maligp2 for index buffers and reduce likelyness of us having to re-align data. */
	temp->mem = _mali_mem_ref_alloc_mem(base_ctx, size, 4, (mali_mem_usage_flag)(MALI_GP_READ | MALI_CPU_READ | MALI_CPU_WRITE) );

	if (NULL == temp->mem)
	{
		/* free preallocated memory */
		gles_gb_cache_free( temp->range_cache );
#ifdef MALI_BB_TRANSFORM
		gles_gb_cache_free( temp->bounding_box_cache);
#endif
		/* free internal buffer object */
		_mali_sys_free(temp);
		temp = NULL;

		/* return NULL (out of memory) */
		return NULL;
	}

	MALI_DEBUG_ASSERT(_mali_mem_ref_get_ref_count(temp->mem) == 1, ("ref count initialized to %d, not 1 as expected", _mali_mem_ref_get_ref_count(temp->mem)));

	/* copy size bytes of memory from data to temp->mem.mali_mem */
	if(data != NULL)
	{
		/* At this point data is copied byte-for-byte in mali memory and
		 * endianess is handled when job is being started since element size of an array
		 * will not be known earlier
		 */
		MALI_DEBUG_PRINT(LD, ("buffer data %x size %d \n", temp->mem->mali_memory->mali_address, size));
		_mali_mem_write( temp->mem->mali_memory, 0, (GLvoid *)data, size);
	}

	return temp;
}

struct gles_gb_buffer_object_data * _gles_gb_buffer_sub_data( mali_base_ctx_handle base_ctx, struct gles_gb_buffer_object_data *vbo_data, GLuint vbo_size, GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data)
{
	struct mali_mem_ref *new_mem_ref;

	MALI_DEBUG_ASSERT_POINTER(vbo_data);

	/* silence a warning */
	MALI_IGNORE(target);
	if ( 1 != _mali_mem_ref_get_ref_count( vbo_data->mem ) )
	{
		/* 32bit align pointer, since this is required by maligp2 for index buffers and reduce likelyness of us having to re-align data. */
		new_mem_ref = _mali_mem_ref_alloc_mem(base_ctx, vbo_size, 4, (mali_mem_usage_flag)(MALI_GP_READ | MALI_CPU_READ | MALI_CPU_WRITE) );
		if (NULL == new_mem_ref) return NULL;

		MALI_DEBUG_ASSERT(_mali_mem_ref_get_ref_count(new_mem_ref) == 1, ("ref count initialized to %d, not 1 as expected", _mali_mem_ref_get_ref_count(new_mem_ref)));

		/* Copy data as is, no endian awareness is needed */
		_mali_mem_copy(
			new_mem_ref->mali_memory,   0, /* output */
			vbo_data->mem->mali_memory, 0, /* input */
			vbo_size
		);

		/* update the vbo-structure */
		_mali_mem_ref_deref(vbo_data->mem); /* deref old mem ref */
		vbo_data->mem = new_mem_ref;	/* insert new mem ref */
		vbo_data->last_used_frame = MALI_BAD_FRAME_ID; 

#if MALI_BIG_ENDIAN
		/* Update the copy with the sub-data */
		if(!GLES_GB_BUFFER_IS_ENDIANESS_CONVERTED(vbo_data))
		{
			MALI_DEBUG_PRINT(LD, ("sub_data easy if %x\n", vbo_data->mem->mali_memory->mali_address));
			/* buffer is still in cpu byte order, simply write data over existing part of buffer */
			_mali_mem_write( vbo_data->mem->mali_memory, offset, (GLvoid *)data, size);
		}
		else
		{
			MALI_DEBUG_PRINT(LD, ("sub_data hard if %x\n", vbo_data->mem->mali_memory->mali_address));
			/* buffer is still in mali byte order, replacing must be done in more complex way */
			_gles_gl_buffer_data_replace(vbo_data, offset, size, data);
		}
#else
		_mali_mem_write( vbo_data->mem->mali_memory, offset, (GLvoid *)data, size);
#endif

		gles_gb_cache_invalidate(vbo_data->range_cache);
#ifdef MALI_BB_TRANSFORM
		gles_gb_cache_invalidate(vbo_data->bounding_box_cache);
#endif
	}
	else
	{

#if MALI_BIG_ENDIAN
		if(!GLES_GB_BUFFER_IS_ENDIANESS_CONVERTED(vbo_data))
		{
			/* simple case, the buffer has not been endian converted, just copy data */
			MALI_DEBUG_PRINT(LD, ("sub_data easy else %x\n", vbo_data->mem->mali_memory->mali_address));
			_mali_mem_write( vbo_data->mem->mali_memory, offset, (GLvoid *)data, size);
		}
		else
		{
			MALI_DEBUG_PRINT(LD, ("sub_data hard else %x\n", vbo_data->mem->mali_memory->mali_address));
			_gles_gl_buffer_data_replace(vbo_data, offset, size, data);
		}
#else
		_mali_mem_write( vbo_data->mem->mali_memory, offset, (GLvoid *)data, size);
#endif

		gles_gb_cache_invalidate(vbo_data->range_cache);
#ifdef MALI_BB_TRANSFORM
		gles_gb_cache_invalidate(vbo_data->bounding_box_cache);
#endif
	}

	return vbo_data;
}

void _gles_gb_free_buffer_data( struct gles_gb_buffer_object_data *vbo_data)
{
	MALI_DEBUG_ASSERT_POINTER(vbo_data);

	/* free mali mem */
	_mali_mem_ref_deref(vbo_data->mem);
	vbo_data->mem = NULL;

	/* When dirtying the cache, we will free all malloced range_nodes except pre_alloced */
	gles_gb_cache_free( vbo_data->range_cache );
	vbo_data->range_cache = NULL;

#ifdef MALI_BB_TRANSFORM
	gles_gb_cache_free( vbo_data->bounding_box_cache);
	vbo_data->bounding_box_cache = NULL;
#endif

	/* free the struct itself */
	_mali_sys_free(vbo_data);
	vbo_data = NULL;
}

/* find a bounding box and mamin index range from an array of indices */
void gles_create_bounding_box_from_indices(struct gles_context *ctx,
										void *indices,
										float* vertices,
										u32 float_stride,
										bb_binary_node *bb_array)
{

    float*	minmax;
    u32		count_m1;
	u32*	bits;
	u32		min_idx;
	u32		max_idx;
    u16*	idx = indices;

    MALI_DEBUG_ASSERT_POINTER( indices );
    MALI_DEBUG_ASSERT_POINTER( vertices );
    MALI_DEBUG_ASSERT_POINTER( bb_array );
    MALI_DEBUG_ASSERT_POINTER( ctx );
    MALI_DEBUG_ASSERT_POINTER( ctx->bit_buffer );
    
    count_m1 = bb_array->count - 1;
    minmax = (float *)bb_array->bb.minmax;
	idx += bb_array->offset;
	
	bits = ctx->bit_buffer;

    {
		u16 indx_hold = *idx++;
		u32 hold_mask_index = indx_hold >> 5;
		u32 buffered_bits = bits[hold_mask_index];
		/* these max mins are the indices into the dirty buffer not the indices themselves */
		min_idx = hold_mask_index; 
		max_idx = hold_mask_index;
		buffered_bits |= 1 << (indx_hold & 0x1F);
		
		/* the dirty bit buffer must be all zeros before this call */
		MALI_DEBUG_CODE(
			{
				int i;
				for( i = 0; i < 2048; i++)
				{
					MALI_DEBUG_ASSERT( bits[i] == 0,("gles_context bit buffer is corrupt!\n"));
				}
			}
		)
		
		/* this loop records all indices in the dirty bit buffer */
		while(count_m1--)
		{
			u16 indx = *idx++;
			u32 mask_index = indx >> 5;
			u32 bit_mask = 1 << (indx & 0x1F);
			if(mask_index == hold_mask_index)
			{
				/* here we only mask to the register holding the contents of the bit buffer */
				buffered_bits |= bit_mask;
			    continue;
			}
			/* must write the bits out */
			bits[hold_mask_index] = buffered_bits;
			
			/* read the new mask in */
			buffered_bits = bits[mask_index];
			/* record max and min for the index of the new 32 bit field coming from the buffer */
			if( min_idx > mask_index )min_idx = mask_index;
			if( max_idx < mask_index )max_idx = mask_index;
			hold_mask_index = mask_index;
			/* write the current index into the bitfield */
			buffered_bits |= bit_mask;

		}
		/* write out the last bitfield */
		bits[hold_mask_index] = buffered_bits;
	}
	/* find the max and min indices from the dirty bits */
	bb_array->rng.min = (min_idx << 5) + _mali_count_trailing_zeros( bits[min_idx] );
	bb_array->rng.max = (max_idx << 5) + ( 31 - __builtin_clz( bits[max_idx] ));

	/* below we look up each vertex in order, from the dirty bits, and test maxmin */		
    {
		u32 buffered_bits = bits[min_idx];
		float *src = vertices + (min_idx << 5) * float_stride;
		/* zero the bit field after use */
		bits[min_idx] = 0;
		
		/* initialise the maxmins after we find the first dirty bit  */
		while (buffered_bits)
		{
			u32 bit = buffered_bits & 1;
			buffered_bits >>= 1;
			if(bit)
			{
				minmax[0] = minmax[3] = *(src);
				minmax[1] = minmax[4] = *(src + 1);
				minmax[2] = minmax[5] = *(src + 2);
				src += float_stride;
				break;
			}
			src += float_stride;
		}
		
		/* we enter this way because we have the state in local variables and we might */
		/* need to do more vertices in the first dirty field (buffered_bits) */
		while (1)
		{
			/* shift the bits of the right side until there are none left */
			while (buffered_bits)
			{
				/* this operation is hopefully a single instruction */
				/* shift right one and put the result in the carry bit */
				u32 bit = buffered_bits & 1;
				buffered_bits >>= 1;
				if(bit)
				{
					if (src[0] > minmax[0]) minmax[0] = src[0];
					else if (src[0] < minmax[3]) minmax[3] = src[0];

					if (src[1] > minmax[1]) minmax[1] = src[1];
					else if (src[1] < minmax[4]) minmax[4] = src[1];

					if (src[2] > minmax[2]) minmax[2] = src[2];
					else if (src[2] < minmax[5]) minmax[5] = src[2];
				}
				src += float_stride;
			}
			/* this loop spins over the u32 in the ditry_bit buffer and returns when done */
			while(1)
			{
				min_idx++;
				if(min_idx > max_idx) return; /* the exit from this loop and the outer loop*/

				buffered_bits = bits[min_idx];
				if(!buffered_bits) continue;
				
				/* zero the bit field after use */
				bits[min_idx] = 0;
				src = vertices + (min_idx << 5) * float_stride;
				break; /* we have a bit field to work on and the pointer is set to the positon of the first bit */
			}
		}
	}
}

MALI_STATIC void _create_bounding_box_from_children( bb_binary_node *left, bb_binary_node *right, bb_binary_node * result)
{
 
    MALI_DEBUG_ASSERT_POINTER( left );
    MALI_DEBUG_ASSERT_POINTER( right );
    MALI_DEBUG_ASSERT_POINTER( result );

    result->offset = left->offset;
    result->count = left->count + right->count;

    /* just find the max and mins from each children */

    result->bb.minmax[0] = MAX( left->bb.minmax[0], right->bb.minmax[0] );
    result->bb.minmax[1] = MAX( left->bb.minmax[1], right->bb.minmax[1] );
    result->bb.minmax[2] = MAX( left->bb.minmax[2], right->bb.minmax[2] );
    result->bb.minmax[3] = MIN( left->bb.minmax[3], right->bb.minmax[3] );
    result->bb.minmax[4] = MIN( left->bb.minmax[4], right->bb.minmax[4] );
    result->bb.minmax[5] = MIN( left->bb.minmax[5], right->bb.minmax[5] );

}

MALI_STATIC void _parse_index_buffer_data( struct gles_gb_buffer_object_data *buffer_data, GLenum mode, u32 offset, u32 count, GLenum type, index_range *idx_range, u32 *coherence, u32 *range_count)
{
    void* indices = NULL;
    u16 tri_count = 0;

    MALI_DEBUG_ASSERT_POINTER(buffer_data);
    MALI_DEBUG_ASSERT_POINTER(buffer_data->mem);
    MALI_DEBUG_ASSERT_POINTER(range_count);
    MALI_DEBUG_ASSERT_POINTER(coherence);
    MALI_DEBUG_ASSERT_POINTER(idx_range);

    indices = _mali_mem_ptr_map_area( buffer_data->mem->mali_memory, offset, count, 1, MALI_MEM_PTR_READABLE);

   _gles_scan_indices( idx_range, count, type, coherence, indices);

    tri_count = (mode == GL_TRIANGLES) ? count / 3 : count;
    
    /* we use a larger factor for range_scan decision, 4, because this is the VBO path and a range scan will be done only once */
    if( (u32)((idx_range[0].max - idx_range[0].min + 1)*4)  > tri_count )
    {
        _gles_scan_indices_range( idx_range, count, range_count, type, indices );
    }

    _mali_mem_ptr_unmap_area(buffer_data->mem->mali_memory);	 

}

#ifdef MALI_BB_TRANSFORM

MALI_STATIC void _create_bounding_box_from_ranges( u8* data,
													u32 stride,
													index_range *idx_range,
													u32 range_count,
													gles_gb_bounding_box* result)
{
    u32 i, k;
    float* minmax;
    float* first_vector;
    u8* local_data = data;

    MALI_DEBUG_ASSERT_POINTER(data);
    MALI_DEBUG_ASSERT_POINTER(result);
    
    minmax = result->minmax;
    first_vector = (float*)(local_data + stride*idx_range[0].min);

    minmax[0] = minmax[3] = *first_vector;
    minmax[1] = minmax[4] = *(first_vector + 1);
    minmax[2] = minmax[5] = *(first_vector + 2);
    
    for (k=0; k < range_count; k++)
    {
		u16 count = idx_range[k].max - idx_range[k].min;
		u16 start = idx_range[k].min;         

		u8* src_ptr = (u8*)local_data + start*stride;
		for (i=1; i < count; i++)
		{           
			float* src = (float*)src_ptr;
		    if (src[0] > minmax[0]) minmax[0] = src[0]; /* x max */             
		    if (src[0] < minmax[3]) minmax[3] = src[0];             

		    if (src[1] > minmax[1]) minmax[1] = src[1]; /* y max*/             
		    if (src[1] < minmax[4]) minmax[4] = src[1];             

		    if (src[2] > minmax[2]) minmax[2] = src[2];  /* z max*/             
		    if (src[2] < minmax[5]) minmax[5] = src[2];
		    
		    src_ptr += stride;                                      
		}
	}
}

#define NODE_TRIANGLE_CNT_THRESHOLD       128
MALI_STATIC bb_binary_node* _build_up_binary_tree_for_bounding_box(struct gles_context *ctx, struct gles_gb_buffer_object_data *gb_data, int vbo_offset, u32 stride, u32 vertex_size, u16 *node_total)
{
    u32 i;
    u32 line_idx;
    u32 tri_count;
    void *indices = NULL;
    gles_gb_context *gb_ctx = NULL;
    u32 total_depth = 0;
    u32 leaves_node_cnt;
    u32 leaves_node_size;
    u8 *vb_data = NULL;
    bb_binary_node *bb_array = NULL;

    MALI_DEBUG_ASSERT_POINTER(ctx);
    MALI_DEBUG_ASSERT_POINTER(gb_data);
    MALI_DEBUG_ASSERT_POINTER(gb_data->mem );
    MALI_DEBUG_ASSERT_POINTER( gb_data->mem->mali_memory );
    MALI_DEBUG_ASSERT( vertex_size >= sizeof(float) * 3,   ("_create_bounding_box, at least 3 components in a stream are required"));
    MALI_DEBUG_ASSERT_POINTER(node_total);

    gb_ctx = _gles_gb_get_draw_context( ctx );

    tri_count = gb_ctx->parameters.index_count/3;

    /* vertex data in cpu memory, only mali_mem and offset are useful parameters */
    vb_data = (u8*)_mali_mem_ptr_map_area( gb_data->mem->mali_memory,
            vbo_offset,
            (stride * ( gb_ctx->parameters.vertex_count - 1)) + (vertex_size),
            64 ,MALI_MEM_PTR_READABLE);

    if( NULL == vb_data )
    {
        MALI_DEBUG_ERROR(("Could not map memory in _gles_gb_get_attribute_data_pointer.\n"));
        return NULL;
    }

    if(gb_ctx->parameters.coherence > COHERENCE_THRESHOLD )
    {
        *node_total = 1;
        bb_array = _mali_sys_malloc( sizeof(bb_binary_node));
        if( NULL == bb_array )
        {
            _mali_mem_ptr_unmap_area(gb_data->mem->mali_memory);
            return NULL;
        }

        _create_bounding_box_from_ranges( vb_data,
        									stride,
        									gb_ctx->parameters.idx_vs_range,
        									gb_ctx->parameters.vs_range_count,
        									&bb_array[0].bb);
        									
        
        bb_array[0].offset = 0;
        bb_array[0].count = tri_count*3;
        _mali_mem_ptr_unmap_area(gb_data->mem->mali_memory);
        return bb_array;
    }

    leaves_node_cnt = ((1 << (31-__builtin_clz(tri_count))) + NODE_TRIANGLE_CNT_THRESHOLD - 1)/ NODE_TRIANGLE_CNT_THRESHOLD;
    
    if( leaves_node_cnt > MALI_LARGEST_INDEX_RANGE) 
    {
    	leaves_node_cnt = MALI_LARGEST_INDEX_RANGE;
    }
    leaves_node_size = (tri_count / leaves_node_cnt) * 3;
    total_depth = _mali_sys_log2( leaves_node_cnt );

    *node_total = pow(2, total_depth+1) - 1;
    bb_array = _mali_sys_malloc((*node_total) * sizeof(bb_binary_node));
    if( NULL == bb_array )
    {
        _mali_mem_ptr_unmap_area(gb_data->mem->mali_memory);
        return NULL;
    }

    indices = _mali_mem_ptr_map_area( ctx->state.common.vertex_array.element_vbo->gb_data->mem->mali_memory, (u32)gb_ctx->parameters.indices, gb_ctx->parameters.index_count, 1, MALI_MEM_PTR_READABLE);

	{ 
		/* we have to reset the beginning of the vb_data because the indices refer from this base */   
		float* vertices = (float*)(vb_data - ( gb_ctx->parameters.start_index * stride) );
		u32 float_stride = stride/sizeof(float);
	 
		/*Calculate the bounding box for all the leaves */
		for( i = 0; i < leaves_node_cnt; i++)
		{
		    u32 real_index = *node_total - leaves_node_cnt + i;
		    bb_array[real_index].offset = leaves_node_size  * i;

		    if(i == leaves_node_cnt - 1)
		    {
		        bb_array[real_index].count = gb_ctx->parameters.index_count - leaves_node_size * i;
		    }
		    else
		    {
		        bb_array[real_index].count = leaves_node_size;
		    }

		    gles_create_bounding_box_from_indices(	ctx,
												indices, 
												vertices,
												float_stride,
												&bb_array[real_index]);
		}
	}
    _mali_mem_ptr_unmap_area(gb_data->mem->mali_memory);

    /*Caulculate the bounding box for non-leaf nodes*/
    for( i = *node_total - leaves_node_cnt - 1; (int)i >=0 ; i--)
    {
        u32 depth = _mali_sys_log2(i+1);
        line_idx = i - pow(2, depth) + 1;

        bb_array[i].offset = (total_depth - depth + 1) * leaves_node_size * line_idx ;
        if(line_idx == pow(2, depth) - 1)
        {
            bb_array[i].count = gb_ctx->parameters.index_count - leaves_node_size * (total_depth - depth + 1) * line_idx;
        }
        else
        {
            bb_array[i].count = leaves_node_size * (total_depth - depth + 1);
        }

        _create_bounding_box_from_children( &(bb_array[2*i+1]), &(bb_array[2*i+2]), &(bb_array[i]) );
    }

    _mali_mem_ptr_unmap_area(ctx->state.common.vertex_array.element_vbo->gb_data->mem->mali_memory);
    return bb_array;
}

mali_err_code _gles_gb_get_bb_from_cache( struct gles_context *ctx, struct gles_gb_buffer_object_data *gb_data, int vbo_offset, u32 stride, u32 vertex_size, bb_binary_node** ret, u16 *node_total)
{
    gles_bb_cache_data fetch_obj;
    gles_gb_context *gb_ctx = NULL;
    u32 count;
    u32 first;

    MALI_DEBUG_ASSERT_POINTER(ctx);
    MALI_DEBUG_ASSERT_POINTER(gb_data);
    MALI_DEBUG_ASSERT_POINTER(ret);
    MALI_DEBUG_ASSERT_POINTER(node_total);

    gb_ctx = _gles_gb_get_draw_context( ctx );

    MALI_DEBUG_ASSERT_POINTER(gb_ctx);
    count = gb_ctx->parameters.index_count;
    first = gb_ctx->parameters.start_index;

    /* Find the range in the bucket specified by hash, assign the search data */
    fetch_obj.count = count;
    fetch_obj.offset = first;
    fetch_obj.stride = stride;

    if (gles_gb_cache_get(gb_data->bounding_box_cache, 
                _data_range_hash_function(count, first), /* it's ok to use same hash function as range cache uses*/
                _gles_gb_bb_compare, 
                &fetch_obj))
    {
        *ret = fetch_obj.bb_cache; /* copy a pointer to bb*/
        *node_total = fetch_obj.node_cnt;
        return MALI_ERR_NO_ERROR;
    }

    fetch_obj.bb_cache = _build_up_binary_tree_for_bounding_box(ctx, gb_data, vbo_offset, stride, vertex_size, node_total);
    if( NULL == fetch_obj.bb_cache )
    {
        return MALI_ERR_OUT_OF_MEMORY;
    }

    fetch_obj.node_cnt = *node_total;

    /* update cache */
    {
        *ret = fetch_obj.bb_cache;

        /* creates a copy of fetch_obj in cache structures*/
        if (gles_gb_cache_insert(gb_data->bounding_box_cache, _data_range_hash_function(count, first), &fetch_obj) == MALI_FALSE)
        {
            _mali_sys_free( *ret );
            return MALI_ERR_OUT_OF_MEMORY;
        }
    }

    return MALI_ERR_NO_ERROR;
}
#endif

void _gles_gb_buffer_object_data_range_cache_get(struct gles_gb_buffer_object_data *buffer_data,
												 GLenum mode,
												 u32 offset,
												 u32 count,
												 GLenum type,
												 index_range **ret,
												 u32 *coherence,
												 u32 *range_count)
{
    gles_range_cache_data fetch_obj;

    MALI_DEBUG_ASSERT_POINTER(buffer_data);
    MALI_DEBUG_ASSERT_POINTER(ret);
    MALI_DEBUG_ASSERT_POINTER(coherence);
    MALI_DEBUG_ASSERT_POINTER(range_count);

    /* Find the range in the bucket specified by hash, assign the search data */
    fetch_obj.count = count;
    fetch_obj.offset = offset;
    fetch_obj.type = type;

    if (gles_gb_cache_get(buffer_data->range_cache, 
                _data_range_hash_function(count, offset), 
                _gles_gb_range_compare, 
                &fetch_obj))
    {
        *range_count = fetch_obj.range_count;
        *coherence = fetch_obj.coherence;
        MALI_DEBUG_ASSERT_POINTER(fetch_obj.idx_range_cache);
        *ret = fetch_obj.idx_range_cache;
        return;
    }

    _parse_index_buffer_data( buffer_data, mode, offset, count, type, *ret, coherence, range_count);

    /* update cache */
    {
        fetch_obj.idx_range_cache = *ret;
        fetch_obj.range_count = *range_count;
        fetch_obj.coherence = *coherence;

        /* creates a copy of fetch_obj in cache structures */
        /* If the insertion fails due to out of memory, the object won't be found */
        /* in the cache the next time and will be rebuilt. In all cases the valid */ 
        /* vs_range is still returned on the stack */
        if(gles_gb_cache_insert(buffer_data->range_cache, _data_range_hash_function(count, offset), &fetch_obj) == MALI_FALSE)
        {
            MALI_DEBUG_PRINT(3,("Gb cache allocation for range_cache failed!"));
        }
    }
    return;
}


#if MALI_BIG_ENDIAN
#include "opengles/gles_config.h"
#include "opengles/gles_buffer_object.h"

MALI_STATIC void perform_swap_endian(u8 *mali_buffer, u8 *mem_buf, u32 size, gles_gb_buffer_elem_conversion_info *elems, u32 nelems)
{
	if(nelems == 1 && elems[0].stride == elems[0].num_elems * elems[0].elem_size)
	{
		MALI_DEBUG_PRINT(LD, ("perform_swap_endian %x %x\n", size, elems[0].elem_size));
		if(mem_buf)
			_mali_sys_memcpy(mali_buffer, mem_buf, size);
		/* all elements are in linear array, conversion is simple */
		_mali_byteorder_swap_endian(mali_buffer, size, elems[0].elem_size);
		return;
	}
	else
	{
		/* more complex case, convert data field by field */
		u32 i, j, pos;
		u8 *work_mem, *src, *dst;

		if(NULL == mem_buf)
		{
			work_mem = _mali_sys_malloc(size);
			_mali_sys_memcpy(work_mem, mali_buffer, size);
		}
		else
			work_mem = mem_buf;

		for(i = 0; i < nelems; i++)
		{
			u32 rec_size;

			/* duplicate src buffer */
			src = work_mem + elems[i].offset;
			dst = mali_buffer + elems[i].offset;
			rec_size = elems[i].num_elems * elems[i].elem_size;
			pos = elems[i].offset;
			/* loop through all elements */
			while(pos + rec_size <= size)
			{
				u8 *src_item = src;
				u8 *dst_item = dst;

				for(j = 0; j < elems[i].num_elems; j++)
				{
					switch(elems[i].elem_size)
					{
					case 1:
						_MALI_SET_U8_ENDIAN_SAFE(dst_item, *(u8 *)src_item);
						dst_item += 1;
						src_item += 1;
						break;
					case 2:
						_MALI_SET_U16_ENDIAN_SAFE((u16 *)dst_item, *(u16 *)src_item);
						dst_item += 2;
						src_item += 2;
						break;
					case 4:
						_MALI_SET_U32_ENDIAN_SAFE((u32 *)dst_item, *(u32 *)src_item);
						dst_item += 4;
						src_item += 4;
						break;
					default:
						MALI_DEBUG_ASSERT(MALI_FALSE, ("unsupported size"));
						break;
					}
				}
				dst += elems[i].stride;
				src += elems[i].stride;
				pos += elems[i].stride;
			}
		}

		if(NULL == mem_buf){
			_mali_sys_free(work_mem);
		}

	}
}

void _gles_gb_buffer_object_process_endianess(gles_gb_buffer_object_data *vbo, u32 array_size, u8 num_elements, u8 element_size, u8 stride, u8 offset)
{
	u8 *buffer;

	if(GLES_GB_BUFFER_IS_ENDIANESS_CONVERTED(vbo)){
		MALI_DEBUG_PRINT(LD, ("vbo %x already processed\n", vbo->mem->mali_memory->mali_address));
		/* this buffer has already been processed */
		return;
	}
	MALI_DEBUG_PRINT(1, ("process vbo %x %d %d\n", vbo->mem->mali_memory->mali_address, array_size, element_size));

	vbo->elems[0].elem_size = element_size;
	vbo->elems[0].num_elems = num_elements;
	vbo->elems[0].stride = stride;
	vbo->elems[0].offset = offset;

	/* map mali memory */
	buffer = (u8 *)_mali_mem_ptr_map_area(vbo->mem->mali_memory, 0, array_size, 2, MALI_MEM_PTR_WRITABLE);
	MALI_DEBUG_ASSERT_POINTER(buffer);
	MALI_DEBUG_PRINT(LD, ("perform_swap_endian %x %d\n", vbo->mem->mali_memory->mali_address, array_size));

	_mali_byteorder_swap_endian(buffer, array_size, element_size);
	_mali_mem_ptr_unmap_area(vbo->mem->mali_memory);
}

void _gles_gb_buffer_object_process_endianess_for_va(gles_vertex_attrib_array *vas, u32 num_vas)
{
	gles_buffer_object *processable[GLES_VERTEX_ATTRIB_COUNT];
	u32 i, j, num_process = 0;
	_mali_sys_memset(processable, 0, sizeof(processable));

	/* collect all buffers that have not been processed yet as multiple vas[] may point
	 * to same vbos */
	for(i = 0; i < GLES_VERTEX_ATTRIB_COUNT; i++){
		if(NULL == vas[i].vbo)
			continue;

		if(!GLES_GB_BUFFER_IS_ENDIANESS_CONVERTED(vas[i].vbo->gb_data)){
			MALI_DEBUG_PRINT(LD, ("need to convert %x\n", vas[i].vbo->gb_data->mem->mali_memory->mali_address));
		}
		else{
			MALI_DEBUG_PRINT(LD, ("no need to convert %x\n", vas[i].vbo->gb_data->mem->mali_memory->mali_address));
		}
		if(/*vas[i].vbo->gb_data->elems[0].num_elems == 0 &&*/
				!GLES_GB_BUFFER_IS_ENDIANESS_CONVERTED(vas[i].vbo->gb_data) &&
				vas[i].vbo->gb_data->elems[0].offset != 0xff) {
			/* collect vbo to processables and flag that is was collected */
			processable[num_process++] = vas[i].vbo;
			vas[i].vbo->gb_data->elems[0].offset = 0xff;
			MALI_DEBUG_PRINT(LD, ("process va %d %x\n", i, vas[i].vbo->gb_data->mem->mali_memory->mali_address));

		}
	}

	if(num_process == 0)
		return;

	MALI_DEBUG_PRINT(1, ("process va num=%d\n", num_process));
	/* compose element information for each processable va */
	for(i = 0; i < num_process; i++){
		int nelem = 0;
		u8 *buffer;
		/* map mali memory */
		buffer = (u8 *)_mali_mem_ptr_map_area(processable[i]->gb_data->mem->mali_memory, 0, processable[i]->size, 2, MALI_MEM_PTR_WRITABLE);

		MALI_DEBUG_PRINT(LD, ("--------------------------------------\n"));
		for(j = 0; j < GLES_VERTEX_ATTRIB_COUNT; j++){
			if(vas[j].vbo == processable[i]){
				u32 k;
				MALI_DEBUG_ASSERT_POINTER(processable[i]->gb_data);
				MALI_DEBUG_ASSERT((u32)vas[j].stride >= (u32)vas[j].pointer + vas[j].elem_bytes * vas[j].size, ("stride less than record len %d %d %d",
										(u32)vas[j].pointer, vas[j].elem_bytes, vas[j].size));

				/* make sure that there are no similar copy element already */
				for(k = 0; k < nelem; k++){
					/* check if similar element is defined, if so do not place duplicates
					 * in the process array
					 */
					if(processable[i]->gb_data->elems[k].elem_size == vas[j].elem_bytes &&
							processable[i]->gb_data->elems[k].num_elems == vas[j].size &&
							processable[i]->gb_data->elems[k].offset == (u8)((u32)vas[j].pointer) &&
							processable[i]->gb_data->elems[k].stride == vas[j].stride){
						MALI_DEBUG_PRINT(LD, ("duplicate elem size %d elems %d offs %d stride %d\n",
								j, vas[j].elem_bytes, vas[j].size, vas[j].pointer, vas[j].stride));
						break;
					}
				}

				if(k == nelem){
					/* save element types for endian processing and later use in glBufferSubData */
					MALI_DEBUG_PRINT(LD, ("%d elem size %d elems %d offs %d stride %d\n",
							j, vas[j].elem_bytes, vas[j].size, vas[j].pointer, vas[j].stride));
					processable[i]->gb_data->elems[nelem].num_elems = vas[j].size;
					processable[i]->gb_data->elems[nelem].elem_size = vas[j].elem_bytes;
					processable[i]->gb_data->elems[nelem].offset = (u8)((u32)vas[j].pointer);
					processable[i]->gb_data->elems[nelem].stride = vas[j].stride;
					nelem++;
				}
			}
		}
		MALI_DEBUG_PRINT(LD, ("perform_swap_endian va %x %d\n", processable[i]->gb_data->mem->mali_memory->mali_address, nelem));
		perform_swap_endian(buffer, NULL, processable[i]->size, processable[i]->gb_data->elems, nelem);
		_mali_mem_ptr_unmap_area(processable[i]->gb_data->mem->mali_memory);
	}
}


/* Replaces data in buffer object when data is already in mali byte order.
 * Allocates temporary buffer for those blocks inside which data is being replaced,
 * converts data back to cpu byte order, copies data (in cpu byte order) from client into temp buffer
 * and converts temp buffer back to mali byte order and replaces it in mali memory
 */

MALI_STATIC void _gles_gl_buffer_data_replace(struct gles_gb_buffer_object_data *vbo_data, GLintptr offset, GLsizeiptr size, const GLvoid * data)
{
	u32 i, j, start_block, end_block;
	u32 end_offset;
	u32 start_offset;
	u8 *src, *dst, *buffer;
	int nelems = 1;

	/* count elements and sanity check strides, all should be same */
	for(i = 1; i < GLES_VERTEX_ATTRIB_COUNT; i++)
	{
		if(0 != vbo_data->elems[i].num_elems)
		{
			nelems++;
			MALI_DEBUG_ASSERT(vbo_data->elems[0].stride == vbo_data->elems[i].stride, ("all strides must be the same"));
		}
	}

	MALI_DEBUG_PRINT(4, ("_gles_gl_buffer_data_replace nelems %d\n", nelems));
	MALI_DEBUG_PRINT(4, ("offs %d size %d\n", offset, size));
	MALI_DEBUG_PRINT(4, ("es %d ne %d offs %d str %d\n",
			vbo_data->elems[0].elem_size,
			vbo_data->elems[0].num_elems,
			vbo_data->elems[0].offset,
			vbo_data->elems[0].stride));

	start_block = (offset / vbo_data->elems[0].stride) * vbo_data->elems[0].stride;
	start_offset = offset % vbo_data->elems[0].stride;
	end_block = (offset + size) / vbo_data->elems[0].stride;
	end_offset = (offset + size) % vbo_data->elems[0].stride;
	if(0 != end_offset)
		end_block++;
	end_block *= vbo_data->elems[0].stride;
	end_offset = offset + size;

	MALI_DEBUG_PRINT(4, ("start_block %d start_offset %d end_block %d\n", start_block, start_offset, end_block));

	buffer = (u8 *)_mali_mem_ptr_map_area(vbo_data->mem->mali_memory, 0, end_block, 2, MALI_MEM_PTR_WRITABLE);

	src = (u8 *)data;
	dst = buffer + offset;

	if(nelems == 1 && vbo_data->elems[0].num_elems * vbo_data->elems[0].elem_size == vbo_data->elems[0].stride){
		/* simple case, just write to mali mem */
		_mali_byteorder_copy_cpu_to_mali(buffer + offset, data, size, vbo_data->elems[0].elem_size);
	}
	else{
		/* loop through all fields and replace data where applicapble */
		for(i = 0; i < nelems; i++)
		{
			u32 rec_size = vbo_data->elems[i].num_elems * vbo_data->elems[i].elem_size;
			u32 pos = vbo_data->elems[i].offset + start_block;
			/* loop through all elements */
			while(pos + rec_size <= end_block)
			{
				u32 cur_pos = 0;
				u8 *src_item = src;
				u8 *dst_item = dst;
				u32 use_end_offset;
				/* if this is the last block, use end offset */
				if(pos >= end_block - vbo_data->elems[0].stride)
					use_end_offset = end_offset;
				else
					use_end_offset = vbo_data->elems[0].stride;


				for(j = 0; j < vbo_data->elems[i].num_elems; j++)
				{
					switch(vbo_data->elems[i].elem_size)
					{
					case 1:
						MALI_DEBUG_PRINT(4, ("replace 1 at %d (%d %d %d)\n", pos, cur_pos, start_offset, use_end_offset));
						if(cur_pos >= start_offset && cur_pos + 1 <= use_end_offset)
							_MALI_SET_U8_ENDIAN_SAFE(dst_item, *(u8 *)src_item);
						dst_item += 1;
						src_item += 1;
						cur_pos += 1;
						break;
					case 2:
						MALI_DEBUG_PRINT(4, ("replace 2 at %d (%d %d %d)\n", pos, cur_pos, start_offset, use_end_offset));
						if(cur_pos >= start_offset && cur_pos + 2 <= use_end_offset)
							_MALI_SET_U16_ENDIAN_SAFE(dst_item, *(u16 *)src_item);
						dst_item += 2;
						src_item += 2;
						cur_pos += 2;
						break;
					case 4:
						MALI_DEBUG_PRINT(4, ("replace 4 at %d (%d %d %d)\n", pos, cur_pos, start_offset, use_end_offset));
						if(cur_pos >= start_offset && cur_pos + 4 <= use_end_offset)
							_MALI_SET_U32_ENDIAN_SAFE(dst_item, *(u32 *)src_item);
						dst_item += 4;
						src_item += 4;
						cur_pos += 4;
						break;
					default:
						MALI_DEBUG_ASSERT(MALI_FALSE, ("unsupported size"));
						break;
					}
				}
				start_offset = 0;
				dst += vbo_data->elems[i].stride;
				src += vbo_data->elems[i].stride;
				pos += vbo_data->elems[i].stride;
			}
		}
	}
	_mali_mem_ptr_unmap_area(vbo_data->mem->mali_memory);
}

#endif
