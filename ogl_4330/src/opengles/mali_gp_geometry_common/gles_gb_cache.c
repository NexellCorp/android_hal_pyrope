/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_gb_cache.h"
#include "base/mali_runtime.h"


/*
 @brief Function invalidates the cache if memory threshold is exceeded 
 @param cache_structure A pointer to working cache structure
 */
MALI_STATIC_INLINE  void gles_gb_cache_invalidate_if_required(cache_main_struct* cache_structure)
{
	MALI_DEBUG_ASSERT_POINTER(cache_structure);

	if ( ( (cache_structure->entry_size * cache_structure->total_entries)  >> 10 ) > cache_structure->out_of_memory_threshold)
	{
		gles_gb_cache_invalidate( cache_structure ); /* since we run out of memory we should try to invalidate the cache, to release memory,
		                                                                             if too huge amout of cache data is allocated most of this data may not be in use already*/
	}
}

cache_main_struct* gles_gb_cache_alloc(
										alloc_cache_entry_func alloc_entry_func,   
										release_cache_entry_func release_entry_func, 
										u16 size,
										u16 elements_per_node)
{
	cache_main_struct*  result = _mali_sys_calloc(1, sizeof(cache_main_struct));

	if (result == NULL)
	{
	 	return NULL;
	}

	MALI_DEBUG_ASSERT_POINTER(alloc_entry_func);
	MALI_DEBUG_ASSERT_POINTER(release_entry_func);
	MALI_DEBUG_ASSERT( ((size)&(size-1)) == 0, ("hash table size is not a power of 2"));

	result->release_entry_func = release_entry_func;
	result->alloc_entry_func = alloc_entry_func;
	result->table_size = size;
	result->elements_per_node = elements_per_node;
	result->out_of_memory_threshold = 0;
	result->entry_size = 0;
	result->total_entries =0;

	result->hash_table = _mali_sys_calloc(1, sizeof(cache_entries_list)*size);

	if (result->hash_table== NULL)
	{
		_mali_sys_free(result);
		return NULL;
	}

	return result;
}

void gles_gb_cache_enable_oom_invalidation(cache_main_struct* cache_structure, u32 threshold_in_kb ,  u32 entry_size)
{
	MALI_DEBUG_ASSERT_POINTER(cache_structure);

	cache_structure->out_of_memory_threshold = threshold_in_kb;
	cache_structure->entry_size = entry_size;
}

void gles_gb_cache_free(cache_main_struct* cache_structure)
{
	MALI_DEBUG_ASSERT_POINTER(cache_structure);
	MALI_DEBUG_ASSERT_POINTER(cache_structure->hash_table);

	gles_gb_cache_invalidate(cache_structure);
	if (cache_structure->hash_table != NULL) _mali_sys_free(cache_structure->hash_table);
	cache_structure->hash_table = NULL;

	_mali_sys_free(cache_structure);
}

void gles_gb_cache_invalidate(cache_main_struct* cache_structure)
{
	u32 i,j;
	MALI_DEBUG_ASSERT_POINTER(cache_structure);
	MALI_DEBUG_ASSERT_POINTER(cache_structure->hash_table);
	MALI_DEBUG_ASSERT_POINTER(cache_structure->release_entry_func);

	for (i = 0; i < cache_structure->table_size; ++i)
	{
		cache_entries_node *p;
		cache_entries_node *next;
		cache_entries_list *cache;

		cache = &cache_structure->hash_table[ i ];

		p = cache->first;

		while( NULL != p )
		{
			MALI_DEBUG_ASSERT_POINTER(p->cache_data);
			next = p->next;
			p->next = NULL;
			for ( j = 0; j < p->num; j++ )
			{
				if( NULL != p->cache_data[j] )
				{
					cache_structure->release_entry_func( p->cache_data[j] );
				}
			}
			_mali_sys_free(p->cache_data);
			_mali_sys_free( p );
			p = next;
		}
		cache->first = cache->last = NULL;
	}

	cache_structure->total_entries= 0;
}

mali_bool gles_gb_cache_get(cache_main_struct* cache_structure, u32 hash_value, comp_and_assign_func comp_and_assign, void* return_object)
{

	int i;
	cache_entries_node *p;
	cache_entries_list *cache;
	void *data;
	u32 hash;


	MALI_DEBUG_ASSERT_POINTER(return_object);
	MALI_DEBUG_ASSERT_POINTER(comp_and_assign);
	MALI_DEBUG_ASSERT_POINTER(cache_structure);

	hash = hash_value & (cache_structure->table_size-1);  /* avoid out of bounds access*/

	cache = &cache_structure->hash_table[ hash ];

	/* Find the range in the bucket specified by hash */
	p = cache->first;
	while( NULL != p )
	{
		MALI_DEBUG_ASSERT( p->num <= cache_structure->elements_per_node, ("Invalid count for number of elements in a bucket-node") );
		for ( i = p->num-1; i >= 0; i-- )
		{
			data = p->cache_data[ i ];
			if ( comp_and_assign(return_object, data ) == 0   )
			{
				return MALI_TRUE;
			}
		}
		p = p->next;
	}

	return MALI_FALSE;
}

mali_bool gles_gb_cache_insert(cache_main_struct* cache_structure, u32 hash_value, void* data)
{

	cache_entries_node *new_node;
	u32 hash;
	cache_entries_list *cache;

	MALI_DEBUG_ASSERT_POINTER(cache_structure);
	MALI_DEBUG_ASSERT_POINTER(cache_structure->alloc_entry_func);
	MALI_DEBUG_ASSERT_POINTER(data);

	hash = hash_value & (cache_structure->table_size-1);  /* avoid out of bounds access*/
	cache = &cache_structure->hash_table[ hash ];

	MALI_DEBUG_ASSERT_POINTER(cache);

	new_node = cache->last;

	if ( NULL != new_node && new_node->num < cache_structure->elements_per_node)
	{
		new_node->cache_data[ new_node->num ] = cache_structure->alloc_entry_func(data);
		if (new_node->cache_data[ new_node->num ] == NULL) /* no memory to allocate the cache entry*/
		{
			gles_gb_cache_invalidate_if_required(cache_structure);
			return MALI_FALSE;
		}
		new_node->num++;
	}
	else
	{
		new_node = ( cache_entries_node * ) _mali_sys_calloc( 1, sizeof( cache_entries_node ) );

		if ( NULL == new_node )
		{
			gles_gb_cache_invalidate_if_required(cache_structure);
			return MALI_FALSE;
		}

		new_node->cache_data = _mali_sys_calloc( 1, sizeof( void*)*cache_structure->elements_per_node);

		if ( NULL == new_node->cache_data )
		{
			_mali_sys_free(new_node);
			gles_gb_cache_invalidate_if_required(cache_structure);
			return MALI_FALSE;
		}

		new_node->cache_data[ 0 ] = cache_structure->alloc_entry_func(data);

		if (new_node->cache_data[ 0 ] == NULL) /* no memory to allocate a cache entry*/
		{
			_mali_sys_free(new_node->cache_data);
			_mali_sys_free(new_node);
			gles_gb_cache_invalidate_if_required(cache_structure);
			return MALI_FALSE;
		}

		new_node->next = NULL;
		new_node->num  = 1;

		if ( NULL != cache->last )
		{
			cache->last->next = new_node;
			cache->last = new_node;
		}
		if ( NULL == cache->first )
		{
			cache->first = cache->last = new_node;
		}
	}

	cache_structure->total_entries++;
	return MALI_TRUE;
}

