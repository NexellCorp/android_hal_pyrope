/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <shared/mali_shared_mem_ref.h>


MALI_STATIC mali_err_code _mali_shared_mem_ref_init( struct mali_shared_mem_ref *mem_ref )
{
	MALI_DEBUG_ASSERT_POINTER( mem_ref );

	mem_ref->mali_memory = MALI_NO_HANDLE;

	_mali_sys_atomic_initialize( &mem_ref->owners, 1 );
	_mali_sys_atomic_initialize( &mem_ref->usage, 0 );

	return MALI_ERR_NO_ERROR;
}

MALI_EXPORT struct mali_shared_mem_ref* _mali_shared_mem_ref_alloc( void )
{
	struct mali_shared_mem_ref *mem_ref;

	mem_ref = _mali_sys_calloc( 1, sizeof( struct mali_shared_mem_ref) );
	if ( NULL == mem_ref ) return NULL;

	if ( MALI_ERR_NO_ERROR != _mali_shared_mem_ref_init( mem_ref ) )
	{
		_mali_sys_free( mem_ref );
		return NULL;
	}

	return mem_ref;
}

MALI_EXPORT struct mali_shared_mem_ref* _mali_shared_mem_ref_alloc_existing_mem( mali_mem_handle mem )
{
	struct mali_shared_mem_ref *mem_ref;

	mem_ref = _mali_shared_mem_ref_alloc();
	if ( NULL == mem_ref ) return NULL;

	mem_ref->mali_memory = mem;

	return mem_ref;
}

MALI_EXPORT struct mali_shared_mem_ref* _mali_shared_mem_ref_alloc_mem( mali_base_ctx_handle base_ctx, size_t size, u32 pow2_alignment, u32 mali_access )
{
	struct mali_shared_mem_ref *mem_ref;

	mem_ref = _mali_shared_mem_ref_alloc();
	if ( NULL == mem_ref ) return NULL;

	mem_ref->mali_memory = _mali_mem_alloc( base_ctx, size, pow2_alignment, mali_access );
	if ( MALI_NO_HANDLE == mem_ref->mali_memory )
	{
		_mali_shared_mem_ref_owner_deref( mem_ref );
		return NULL;
	}

	return mem_ref;
}

MALI_EXPORT void _mali_shared_mem_ref_usage_deref( struct mali_shared_mem_ref *mem_ref )
{
	MALI_DEBUG_ASSERT_POINTER( mem_ref );
	MALI_DEBUG_ASSERT( 0 < _mali_sys_atomic_get( &mem_ref->usage ), ("inconsistent usage ref count"));

	_mali_sys_atomic_dec( &mem_ref->usage );

	_mali_shared_mem_ref_owner_deref( mem_ref );
}

MALI_EXPORT void _mali_shared_mem_ref_owner_deref( struct mali_shared_mem_ref *mem_ref )
{
	int new_value = 0;
	MALI_DEBUG_ASSERT_POINTER( mem_ref );
	MALI_DEBUG_ASSERT( 0 < _mali_sys_atomic_get( &mem_ref->owners), ("inconsistent owner ref count: %d", mem_ref->owners));

	/* destroy mem_ref if owner count has reached zero */
	new_value = _mali_sys_atomic_dec_and_return( &mem_ref->owners );
	if ( 0 == new_value )
	{
		MALI_DEBUG_ASSERT( 0 == _mali_sys_atomic_get( &mem_ref->usage ), ("Owner count reached zero while usage is %d", _mali_sys_atomic_get( &mem_ref->usage ) ) );

		_mali_mem_free( mem_ref->mali_memory );
		mem_ref->mali_memory = MALI_NO_HANDLE;

#ifdef DEBUG
		/* reset memory to trap dangling pointers if any */
		_mali_sys_memset( mem_ref, 0, sizeof( mali_shared_mem_ref ) );
#endif /* DEBUG */

		_mali_sys_free( mem_ref );
	}
}

