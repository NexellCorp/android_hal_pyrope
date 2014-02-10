/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include "mali_osu.h"

#include <stdlib.h>
#include <string.h> /* memcmp, memchr, memset */

/**
 * @file mali_osu_memory.c
 * File implements the user side of the OS interface
 */

void *_mali_osu_calloc( u32 n, u32 size )
{
	return calloc( n, size );
}

void *_mali_osu_malloc( u32 size )
{
	return malloc( size );
}

void *_mali_osu_realloc( void *ptr, u32 size )
{
	return realloc( ptr, size );
}

void _mali_osu_free( void *ptr )
{
	free( ptr );
}

void *_mali_osu_memcpy( void *dst, const void *src, u32	len )
{
	return memcpy( dst, src, len );
}

void *_mali_osu_memset( void *ptr, u32 chr, u32 size )
{
	return memset( ptr, chr, size );
}

int _mali_osu_memcmp( const void *ptr1, const void *ptr2, u32 size )
{
	return memcmp( ptr1, ptr2, size );
}
