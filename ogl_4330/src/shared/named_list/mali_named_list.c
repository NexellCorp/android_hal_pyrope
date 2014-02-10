/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_named_list.c
 * @brief Source file for hash-list implementation.
 * This implementation is based partially on:
 *   OpenVG's implementation of a hash-list(the fibonacci hash-indexing function)
 */

#include <shared/mali_named_list.h>

#include <mali_system.h>

/* PRIVATE DECLARATIONS  - Helper functions for hash-list operations */

typedef struct mali_hash_list_entry
{
	u32  name;   /**< The id of the hash-entry */
	void *data; /**< The pointer to the data the id represents */
} mali_hash_list_entry;

/* Define for calculating hash-index, known as fibonacci hashing, courtesy of TAOCP */
#define MALI_HASH_LIST_CALCULATE_HASH_INDEX( hlist, name ) ( (name * 0x9e406cb5U) >> (32 - hlist->log2_size) );

/* Define for calculating the iterator, set to 1 since the fibonacci hashing
    gives a good spread, and it is best for the cache when we have collisions */
#define MALI_HASH_LIST_CALCULATE_ITERATOR( hlist, name ) ( 1 )

/* Define for what we identify as a deleted value, the address of the list itself
    will never be a mali_hash_list_entry object in the hash-array, and thus we can use it
	as a "magic" number */
#define MALI_HASH_LIST_DELETED_VALUE( hlist ) ( ( mali_hash_list_entry * ) ( hlist ) )

/** @brief Doubles the array, and inserts the old entries into the new array
 * @param hlist The named-list to double the size of the hash-table of
 * @return Returns MALI_ERR_NO_ERROR if successful, an errorcode if not. If it fails the old list is preserved.
 */
MALI_STATIC mali_err_code MALI_CHECK_RESULT __mali_named_list_double_list( mali_named_list *hlist )
{
	u32 i;
	u32 old_log2_size;
	u32 old_size;
	u32 new_size;
	u32 old_num_elements;
	u32 old_num_elements_hash;
	mali_hash_list_entry **old_list;

	MALI_DEBUG_ASSERT_POINTER( hlist );

	old_log2_size = hlist->log2_size;
	old_size = hlist->size;
	old_list = hlist->list;
	old_num_elements = hlist->num_elements;
	old_num_elements_hash = hlist->num_elements_hash;


	new_size = old_size << 1;
	hlist->size = new_size;
	hlist->list = ( mali_hash_list_entry ** ) _mali_sys_malloc( new_size * sizeof( mali_hash_list_entry * ) );

	if ( NULL == hlist->list )
	{
		hlist->size = old_size;
		hlist->list = old_list;
		MALI_ERROR( MALI_ERR_OUT_OF_MEMORY );
	}

	_mali_sys_memset( hlist->list, 0, new_size * sizeof( mali_hash_list_entry * ) );

	hlist->log2_size++;
	hlist->num_elements = hlist->num_elements_flat;
	hlist->num_elements_hash = 0;

	for ( i = 0; i < old_size; i++ )
	{
		if ( NULL != old_list[ i ] && MALI_HASH_LIST_DELETED_VALUE( hlist ) != old_list[ i ] )
		{
			mali_err_code err = MALI_ERR_NO_ERROR;
			err = __mali_named_list_insert( hlist, old_list[ i ]->name, old_list[ i ]->data );

			if ( MALI_ERR_NO_ERROR != err )
			{
				/* Must delete the new list and set back to old one */
				for ( i = 0; i < new_size; i++ )
				{
					if ( NULL != hlist->list[ i ] )
					{
						_mali_sys_free( hlist->list[ i ] );
						hlist->list[ i ] = NULL;
					}
				}
				_mali_sys_free( hlist->list );

				hlist->size = old_size;
				hlist->list = old_list;
				hlist->num_elements = old_num_elements;
				hlist->num_elements_hash = old_num_elements_hash;
				hlist->log2_size = old_log2_size;

				MALI_DEBUG_ASSERT( MALI_ERR_OUT_OF_MEMORY == err, ("Something is wrong in the hash-list, we got an error-code signaling that multiple objects of the same name has been inserted while doubling list") );

				MALI_ERROR( err );
			}
		}
	}

	for ( i = 0; i < old_size; i++ )
	{
		if ( NULL != old_list[ i ] && MALI_HASH_LIST_DELETED_VALUE( hlist ) != old_list[ i ] )
		{
			old_list[ i ]->name = 0;
			old_list[ i ]->data = 0;
			_mali_sys_free( old_list[ i ] );
			old_list[ i ] = NULL;
		}
	}

	_mali_sys_free( old_list );

	MALI_SUCCESS;
}

/* PUBLIC FUNCTIONS */

MALI_EXPORT mali_named_list* __mali_named_list_allocate(void)
{
	mali_named_list* hlist = NULL;

	hlist = _mali_sys_malloc(sizeof(mali_named_list));
	if( NULL == hlist )
	{
		return NULL;
	}

	hlist->log2_size = MALI_HASH_LIST_INITIAL_LOG2_SIZE;
	hlist->size = 1 << hlist->log2_size;
	hlist->list = ( mali_hash_list_entry ** ) _mali_sys_malloc( hlist->size * sizeof( mali_hash_list_entry * ) );
	if ( NULL == hlist->list )
	{
		_mali_sys_free( hlist );
		return NULL;
	}

	hlist->list_mutex = _mali_sys_mutex_create();
	if ( NULL == hlist->list_mutex )
	{
		_mali_sys_free( hlist->list );
		_mali_sys_free( hlist );
		return NULL;
	}

	hlist->num_elements = 0;
	hlist->num_elements_flat = 0;
	hlist->num_elements_hash = 0;
	hlist->max = 0;

	_mali_sys_memset( hlist->list, 0, hlist->size * sizeof( mali_hash_list_entry * ) );
	_mali_sys_memset( hlist->flat, 0, MALI_NUM_VALUES_FLAT * sizeof( void * ) );


	return hlist;
}


MALI_EXPORT void __mali_named_list_free(mali_named_list *hlist, void (*freefunc)())
{
	u32 i;

	MALI_DEBUG_ASSERT_POINTER( hlist );

	/* check if any entries exist before going through the list */
	if ( 0 != hlist->num_elements_hash)
	{
        for ( i = 0; i < hlist->size; i++ )
        {
            if ( NULL != hlist->list[ i ] && MALI_HASH_LIST_DELETED_VALUE( hlist ) != hlist->list[ i ] )
            {
                if ( NULL != freefunc )
                {
                    freefunc( hlist->list[ i ]->data );
                }
                hlist->list[ i ]->data = NULL;
                hlist->list[ i ]->name = 0;
                _mali_sys_free( hlist->list[ i ] );
                hlist->list[ i ] = NULL;
            }
        }
	}

	_mali_sys_free( hlist->list );
	hlist->list = NULL;

    if ( 0 != hlist->num_elements_flat)
    {
        for ( i = 0; i < MALI_NUM_VALUES_FLAT; i++ )
        {
            if ( NULL != hlist->flat[ i ] && NULL != freefunc )
            {
                freefunc( hlist->flat[ i ] );
            }
            hlist->flat[ i ] = NULL;
        }
    }

	_mali_sys_mutex_destroy( hlist->list_mutex );

	_mali_sys_free( hlist );
}

MALI_EXPORT mali_err_code __mali_named_list_insert( mali_named_list* hlist, u32 name, void* data )
{
	int hash;
	int i;
	void *previous;

	MALI_DEBUG_ASSERT_POINTER( hlist );
	if ( name > hlist->max )
	{
		hlist->max = name;
	}

	/* First look through the flat-list and see if we can insert it there or it exists there already */
	if ( name < MALI_NUM_VALUES_FLAT )
	{
		if ( NULL == hlist->flat[ name ] )
		{
			hlist->flat[ name ] = data;
			hlist->num_elements_flat++;
			hlist->num_elements++;
			return MALI_ERR_NO_ERROR;
		}
		else
		{
			return MALI_ERR_FUNCTION_FAILED;
		}
	}

	previous = __mali_named_list_get( hlist, name );

	/* Need to check that we don't overflow the list, this may happen if we haven't got enough space to create a new larger list */
	MALI_CHECK( hlist->num_elements_hash < hlist->size - 1, MALI_ERR_OUT_OF_MEMORY );

	/* Check that the name isn't already present in the list */
	MALI_CHECK( previous == NULL, MALI_ERR_FUNCTION_FAILED );

    hash = MALI_HASH_LIST_CALCULATE_HASH_INDEX( hlist, name );

	i = hash;

	/* Insert the new element since it does not already exist in the list */
	do
	{
		/* If the entry has never been set or has been deleted we can insert here */
		if ( NULL == hlist->list[ i ] || MALI_HASH_LIST_DELETED_VALUE( hlist ) == hlist->list[ i ] )
		{
			hlist->list[ i ] = ( mali_hash_list_entry * ) _mali_sys_malloc( sizeof( mali_hash_list_entry ) );
			MALI_CHECK_NON_NULL( hlist->list[ i ], MALI_ERR_OUT_OF_MEMORY );
			hlist->list[ i ]->name = name;
			hlist->list[ i ]->data = data;
			hlist->num_elements++;
			hlist->num_elements_hash++;

			/* If we have filled half the array, we try to double the list */
			if ( hlist->num_elements_hash >= ( hlist->size >> 1 ) )
			{
				mali_err_code err = __mali_named_list_double_list( hlist );
				if ( MALI_ERR_OUT_OF_MEMORY != err )
				{
					MALI_ERROR( err );
				} /* else: We haven't actually run out of memory yet, but probably will soon,
				      this function will return out of memory when the list is filled up,
					  which happens if we can't double the list */
			}

			MALI_SUCCESS;
		}
		i = ( i + MALI_HASH_LIST_CALCULATE_ITERATOR( hlist, name ) ) % hlist->size;
	} while( i != hash );

	/* Possibly a DEBUG_ASSERT, This should never happen, since num_elements_hash < size - 1,
	    there should always be room for one element, possibly an incorrect hash-iterator-function
		has been selected so that it does not traverse all elements */

	MALI_DEBUG_ASSERT( 0, ("Not enough room in the hash-list, even though it hasn't been filled up") );

	MALI_ERROR( MALI_ERR_FUNCTION_FAILED );
}

MALI_EXPORT void* __mali_named_list_remove( mali_named_list* hlist, u32 name)
{
	void *retval = NULL;
	int hash;
	int i;

	MALI_DEBUG_ASSERT_POINTER( hlist );

	/* First look in the flat-array(if the name is in the range) */
	if ( name < MALI_NUM_VALUES_FLAT )
	{
		if ( NULL == hlist->flat[ name ] )
		{
			return NULL;
		}
		else
		{
			retval = hlist->flat[ name ];
			hlist->flat[ name ] = NULL;
			hlist->num_elements_flat--;
			hlist->num_elements--;
			return retval;
		}
	}

	hash = MALI_HASH_LIST_CALCULATE_HASH_INDEX( hlist, name );

	/* Try to remove it directly, in most cases this will happen, or else
	    we need to change the MALI_HASH_LIST_CALCULATE_HASH_INDEX */
	if ( NULL != hlist->list[ hash ] && (mali_hash_list_entry*)MALI_HASH_LIST_DELETED_VALUE( hlist ) != hlist->list[ hash ] && name == hlist->list[ hash ]->name )
	{
		retval = hlist->list[ hash ]->data;
		_mali_sys_free( hlist->list[ hash ] );
		hlist->list[ hash ] = MALI_HASH_LIST_DELETED_VALUE( hlist );
		hlist->num_elements--;
		hlist->num_elements_hash--;

		return retval;
	}
	i = ( hash + MALI_HASH_LIST_CALCULATE_ITERATOR( hlist, name ) ) % hlist->size;
	/* It was not found, now iterate through the list to find the next possible location,
	    if we find a NULL value, it hasn't been inserted or already deleted */
	while( i != hash && NULL != hlist->list[ i ] )
	{
		/* If it has been deleted we move on, otherwise we check if this entry contains the name we are searching for */
		if ( (mali_hash_list_entry*)MALI_HASH_LIST_DELETED_VALUE( hlist ) != hlist->list[ i ] && name == hlist->list[ i ]->name )
		{
			retval = hlist->list[ i ]->data;
			_mali_sys_free( hlist->list[ i ] );
			hlist->list[ i ] = MALI_HASH_LIST_DELETED_VALUE( hlist );
			hlist->num_elements--;
			hlist->num_elements_hash--;

			return retval;
		}
		i = ( i + MALI_HASH_LIST_CALCULATE_ITERATOR( hlist, name ) ) % hlist->size;
	}

	return NULL;
}

MALI_EXPORT void* __mali_named_list_get_non_flat( mali_named_list* hlist, u32 name)
{
	int hash;
	int i;

	hash = MALI_HASH_LIST_CALCULATE_HASH_INDEX( hlist, name );

	/* We then try to find it directly */
	if ( NULL != hlist->list[ hash ] && (mali_hash_list_entry*)MALI_HASH_LIST_DELETED_VALUE( hlist ) != hlist->list[ hash ] && name == hlist->list[ hash ]->name )
	{
		return hlist->list[ hash ]->data;
	}
	i = ( hash + MALI_HASH_LIST_CALCULATE_ITERATOR( hlist, name ) ) % hlist->size;

	/* It was not found directly, we need to search for it until we get to a NULL value which proves that it can't have been inserted */
	while( i != hash && NULL != hlist->list[ i ] )
	{
		/* If it has been deleted we continue, otherwise we check if the name corresponds */
		if ( (mali_hash_list_entry*)MALI_HASH_LIST_DELETED_VALUE( hlist ) != hlist->list[ i ] && name == hlist->list[ i ]->name )
		{
			return hlist->list[ i ]->data;
		}
		i = ( i + MALI_HASH_LIST_CALCULATE_ITERATOR( hlist, name ) ) % hlist->size;
	}

	return NULL;
}



MALI_EXPORT mali_err_code __mali_named_list_set( mali_named_list* hlist, u32 name, void* data )
{
	int hash;
	int i;

	MALI_DEBUG_ASSERT_POINTER( hlist );

	/* We first try to find the entry in the flat-array(if the name is in the range) */
	if ( name < MALI_NUM_VALUES_FLAT )
	{
		if ( NULL == hlist->flat[ name ] )
		{
			hlist->num_elements_flat++;
			hlist->num_elements++;
		}
		hlist->flat[ name ] = data;

		MALI_SUCCESS;
	}

	hash = MALI_HASH_LIST_CALCULATE_HASH_INDEX( hlist, name );

	/* We then try to find it directly */
	if ( NULL != hlist->list[ hash ] && (mali_hash_list_entry*)MALI_HASH_LIST_DELETED_VALUE( hlist ) != hlist->list[ hash ] && name == hlist->list[ hash ]->name )
	{
		hlist->list[ hash ]->data = data;

		MALI_SUCCESS;
	}
	else if ( NULL == hlist->list[ hash ] )
	{
		/* We found that it has not been inserted, so we insert it */
		return __mali_named_list_insert( hlist, name, data );
	}
	i = ( hash + MALI_HASH_LIST_CALCULATE_ITERATOR( hlist, name ) ) % hlist->size;

	/* It was not found, but it could be present still, so we search for it */
	while( i != hash && NULL != hlist->list[ i ] )
	{
		if ( (mali_hash_list_entry*)MALI_HASH_LIST_DELETED_VALUE( hlist ) != hlist->list[ i ] && name == hlist->list[ i ]->name )
		{
			hlist->list[ i ]->data = data;

			MALI_SUCCESS;
		}
		i = ( i + MALI_HASH_LIST_CALCULATE_ITERATOR( hlist, name ) ) % hlist->size;
	}

	return __mali_named_list_insert( hlist, name, data );

}

MALI_EXPORT void* __mali_named_list_iterate_begin(mali_named_list* hlist, u32 *iterator)
{
	u32 i;

	MALI_DEBUG_ASSERT_POINTER( hlist );

	if ( 0 == hlist->num_elements )
	{
		return NULL;
	}

	/* We first iterate through the flat-array */
	if ( 0 != hlist->num_elements_flat )
	{
		for ( i = 0; i < MALI_NUM_VALUES_FLAT; i++ )
		{
			if ( NULL != hlist->flat[ i ] )
			{
				*(iterator) = i;
				hlist->num_elements_iterated_flat_array = 1;
				return hlist->flat[ i ];
			}
		}
	}

	/* It was not found in the flat-array, so we find the first element in the array */
	if ( 0 != hlist->num_elements_hash )
	{
		for ( i = 0; i < hlist->size; i++ )
		{
			if ( NULL != hlist->list[ i ] && (mali_hash_list_entry*)MALI_HASH_LIST_DELETED_VALUE( hlist ) != hlist->list[ i ] )
			{
				hlist->last_hash_index = i;
				(*iterator) = hlist->list[ i ]->name;
				return hlist->list[ i ]->data;
			}
		}
	}

	return NULL;
}

MALI_EXPORT void* __mali_named_list_iterate_next(mali_named_list* hlist, u32* iterator)
{
	u32 i;

	MALI_DEBUG_ASSERT_POINTER( hlist );
	MALI_DEBUG_ASSERT_POINTER( iterator );

	/* We first search through the flat-array(if iterator is in the range) */
	if ( *iterator < MALI_NUM_VALUES_FLAT )
	{
		u32 i;

		if ( hlist->num_elements_iterated_flat_array != hlist->num_elements_flat )
		{
			for ( i = *iterator+1; i < MALI_NUM_VALUES_FLAT; i++ )
			{
				if ( NULL != hlist->flat[ i ] )
				{
					*(iterator) = i;
					hlist->num_elements_iterated_flat_array++;
					return hlist->flat[ i ];
				}
			}
		}

		/* We need to find the first element in the hash-array */
		if ( 0 != hlist->num_elements_hash )
		{
			for ( i = 0; i < hlist->size; i++ )
			{
				if ( NULL != hlist->list[ i ] && (mali_hash_list_entry*)MALI_HASH_LIST_DELETED_VALUE( hlist ) != hlist->list[ i ] )
				{
					(*iterator) = hlist->list[ i ]->name;
					hlist->last_hash_index = i;
					return hlist->list[ i ]->data;
				}
			}
		}

		return NULL;
	}

	/* Searching for the next entry */
	for ( i = hlist->last_hash_index + 1; i < hlist->size; i++ )
	{
		if ( NULL != hlist->list[ i ] && (mali_hash_list_entry*)MALI_HASH_LIST_DELETED_VALUE( hlist ) != hlist->list[ i ] )
		{
			(*iterator) = hlist->list[ i ]->name;
			hlist->last_hash_index = i;
			return hlist->list[ i ]->data;
		}
	}

	return NULL;
}

MALI_EXPORT u32 __mali_named_list_get_unused_name( mali_named_list *hlist )
{
	int i;
	MALI_DEBUG_ASSERT_POINTER( hlist );

	/* Searching through the entire flat-array so that we can utilize it the most
        Excluding name=0 since this is reserved */
	if ( MALI_NUM_VALUES_FLAT != hlist->num_elements_flat )
	{
		for ( i = 1; i < MALI_NUM_VALUES_FLAT; i++ )
		{
			if ( NULL == hlist->flat[ i ] )
			{
				return i;
			}
		}
	}

	if ( hlist->max != 0xffffffff )
	{
		return hlist->max + 1;
	}
	else
	{
		u32 i;
		/* If this is to be optimized, I suggest using _mali_sys_qsort or radix-sort on it and saving
		    the result in hlist, and this list is dirtied whenever something is inserted or deleted */
		for ( i = 1; i < 0xffffffff; i++ )
		{
			void *value = __mali_named_list_get( hlist, i );
			if ( NULL == value )
			{
				return i;
			}
		}
	}

	return 0;

}

MALI_EXPORT void __mali_named_list_lock( mali_named_list *hlist )
{
	MALI_DEBUG_ASSERT_POINTER( hlist );

	_mali_sys_mutex_lock(hlist->list_mutex);
}

MALI_EXPORT void __mali_named_list_unlock( mali_named_list *hlist )
{
	MALI_DEBUG_ASSERT_POINTER( hlist );

	_mali_sys_mutex_unlock(hlist->list_mutex);
}

MALI_EXPORT u32 __mali_named_list_size(mali_named_list* hlist)
{
	MALI_DEBUG_ASSERT_POINTER( hlist );

	MALI_DEBUG_ASSERT( ( hlist->num_elements_flat + hlist->num_elements_hash ) == hlist->num_elements, ("Incorrect counting of objects in list") );

	return hlist->num_elements_hash + hlist->num_elements_flat;
}
