/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES_GB_CACHE_H
#define _GLES_GB_CACHE_H

/*
  This module provides an interface for cache  in gles driver. Caller has to specify it own 
   hash_function - it generates the hash value by the key 
   alloc/free  functions  - function to allocate and release a single cache element,
   comparison_and_assign function  - function to compare 2 objects in a cache by search parts, and to assign a useful part from cache it match found
*/

#include "../gles_gb_interface.h"

typedef u32    (*hash_func)();
typedef void   (*release_cache_entry_func) (void* cache_element);
typedef u32    (*comp_and_assign_func) (void* obj1, const void* const obj2);
typedef void* (*alloc_cache_entry_func) (void* cache_element);

struct cache_entries_node;

/* cache entries list */
typedef struct cache_entries_node
{
	void** cache_data;
	u32 num;
	struct cache_entries_node* next;
} cache_entries_node;

typedef struct cache_entries_list
{
	struct cache_entries_node* first;
	struct cache_entries_node* last;
} cache_entries_list;


typedef struct cache_main_struct{
	u16 table_size;
	u16 elements_per_node;
	u32 out_of_memory_threshold; /* threshold in Kb, to release the data in case of out of memory. i.e. if we run of out memory and cache is too huge,
	it makes sense to invalidate it*/
	u32 entry_size;                           /* size of one cache entry*/
	u32 total_entries;                           /* total allocated entries*/
	alloc_cache_entry_func alloc_entry_func; 
	release_cache_entry_func release_entry_func; 
	cache_entries_list* hash_table;
} cache_main_struct; 


/*
 @brief Function creates the cache working structure pointer 
 @param alloc_entry_func a function pointer to allocator for cache element
 @param release_entry_func function to release a single cache element
 @param size num of elements in a cache table
 @param elements_per_node  number of elements in a cache bucket.
 @return a pointer to a working structure cache_main_struct
 */
cache_main_struct* gles_gb_cache_alloc(alloc_cache_entry_func alloc_entry_func,  release_cache_entry_func release_entry_func, u16 size, u16 elements_per_node);

/*
 @brief Function enables control mechanizm for out of memory 
 @param cache_structure A pointer to working cache structure
 @param threshold_in_kb  threshold value to invalidate the cache in case of out of memory ( in kb).
 @param entry_size size of cache entry for out of memory control ( in bytes)
 */
void gles_gb_cache_enable_oom_invalidation(cache_main_struct* cache_structure, u32 threshold_in_kb ,  u32 entry_size)
;

/*
 @brief Retrieves a cache element by key value
 @param cache_structure A pointer to working cache structure
 @param value The hash value to retrieve the data
 @param comp_and_assign - function to assign the data from the cache to return pointer if search data matches
 @param return_object - object which contains the unique data to fetch the object by and is being filled out if the data matches
 @return  A mali_bool value. MALI_TRUE if data has been founded in a cache, MALI_FALSE otherwise
*/
mali_bool gles_gb_cache_get(cache_main_struct* cache_structure, u32 value, comp_and_assign_func comp_and_assign, void* return_object);


/*
 @brief Inserts cache element by key value
 @param cache_structure A pointer to working cache structure
 @param hash_key The hash value to insert the data
 @param  data A pointer to data for insertion into cache
 @return  A mali_bool value. MALI_TRUE if data has been inserted into a cache, MALI_FALSE otherwise
*/
mali_bool gles_gb_cache_insert(cache_main_struct* cache_structure, u32 hash_value, void* data);


/*
 @brief Invalidates the cache
 @param cache_structure A pointer to working cache structure
*/
void gles_gb_cache_invalidate(cache_main_struct* cache_structure);

/*
 @brief Function releases the entire cache and working structure 
 @param cache_structure A pointer to working cache structure
 */
void gles_gb_cache_free(cache_main_struct* cache_structure);



#endif
 
