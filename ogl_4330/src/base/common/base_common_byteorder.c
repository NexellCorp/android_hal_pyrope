/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <base/mali_types.h>
#include <base/mali_memory.h>

#include <base/mali_byteorder.h>

#ifndef DBG_LVL
#define DBG_LVL 7
#endif
/* ----------------------------------------------------------------------------------------------
 *  Exported routine definitions
 */

#if MALI_BIG_ENDIAN

#include <base_arch_byteorder_inline_be.h>

MALI_EXPORT
void * _mali_byteorder_copy_cpu_to_mali(void * destination, const void * source, u32 num, u32 element_size)
{
	MALI_DEBUG_PRINT(DBG_LVL, ("_mali_byteorder_copy_cpu_to_mali(%p, %p, %d, %d)\n", destination, source, num, element_size));

	MALI_DEBUG_ASSERT((num % element_size) == 0, ("number of bytes must be multiple of element size"));

	switch( element_size )
	{
	case 4:
		return _mali_byteorder_write_endian_safe_u32(destination, source, num/element_size);
	case 2:
		return _mali_byteorder_write_endian_safe_u16(destination, source, num/element_size);
	case 1:
		return _mali_byteorder_write_endian_safe_u8(destination, source, num);
	case 8:
		return _mali_byteorder_write_endian_safe_u64(destination, source, num/element_size);
	default:
		MALI_DEBUG_ASSERT( 0, ("illegal type size %d!\n", element_size) );
	}
	return 0;
}

MALI_EXPORT
void * _mali_byteorder_copy_mali_to_cpu(void * destination, const void * source, u32 num, u32 element_size)
{
	MALI_DEBUG_PRINT(DBG_LVL, ("_mali_byteorder_copy_mali_to_cpu(%p, %p, %d, %d)\n", destination, source, num, element_size));

	MALI_DEBUG_ASSERT((num % element_size) == 0, ("number of bytes must be multiple of element size"));

	/* Note: 8- and 16-bit transfer have to be done differently
	 * than copying from cpu to mali
	 */
	switch( element_size )
	{
	case 4:
		return _mali_byteorder_write_endian_safe_u32(destination, source, num/element_size);
	case 2:
		return _mali_byteorder_read_endian_safe_u16(destination, source, num/element_size);
	case 1:
		return _mali_byteorder_read_endian_safe_u8(destination, source, num);
	case 8:
		return _mali_byteorder_write_endian_safe_u64(destination, source, num/element_size);
	default:
		MALI_DEBUG_ASSERT( 0, ("illegal type size %d!\n", element_size) );
	}
	return 0;
}

MALI_EXPORT
void * _mali_byteorder_copy_mali_to_mali(void * destination, const void * source, u32 num, u32 element_size)
{
	MALI_DEBUG_PRINT(DBG_LVL, ("_mali_byteorder_copy_mali_to_mali(%p, %p, %d, %d)\n", destination, source, num, element_size));

	MALI_DEBUG_ASSERT((num % element_size) == 0, ("number of bytes must be multiple of element size"));

	switch( element_size )
	{
	case 4:
		return _mali_byteorder_copy_mali_to_mali_u32(destination, source, num/element_size);
	case 2:
		return _mali_byteorder_copy_mali_to_mali_u16(destination, source, num/element_size);
	case 1:
		return _mali_byteorder_copy_mali_to_mali_u8(destination, source, num);
	case 8:
		return _mali_byteorder_copy_mali_to_mali_u64(destination, source, num/element_size);
	default:
		MALI_DEBUG_ASSERT( 0, ("illegal type size %d!\n", element_size) );
	}

	return 0;

}


MALI_EXPORT
void _mali_byteorder_swap_endian(void * destination, u32 bytes, u32 element_size)
{
	MALI_DEBUG_PRINT(DBG_LVL, ("_mali_byteorder_swap_endian(%p, %d, %d)\n", destination, bytes, element_size));

	MALI_DEBUG_ASSERT((bytes % element_size) == 0, ("number of bytes must be multiple of element size"));

	switch( element_size )
	{
	case 4:
		return _mali_byteorder_swap_endian_u32_u32(destination, bytes);
	case 2:
		return _mali_byteorder_swap_endian_u32_u16(destination, bytes);
	case 1:
		return _mali_byteorder_swap_endian_u32_u8(destination, bytes);
	case 8:
		return _mali_byteorder_swap_endian_u64_u32(destination, bytes);
	default:
		MALI_DEBUG_ASSERT( 0, ("illegal type size %d!\n", element_size) );
	}

	return;
}


MALI_EXPORT
void * _mali_byteorder_memset(void *p, u32 val, u32 n)
{
	u8* dst = (u8*)p;
	u32 size_head;
	u32 size_tail = 0;
	u32 size_bulk = 0;

	MALI_DEBUG_PRINT(DBG_LVL, ("_mali_sys_memset_safe: p = %x, val = %x, n = %d\n", p, (char)val, n));

	size_head = (4 - ((u32)dst & 0x3)) & 0x3;
	if (size_head > n)
	{
		size_head = n;
	}
	else
	{
		size_tail = (n - size_head) & 0x3;
		size_bulk = n - size_head - size_tail;
	}

	/* handle unaligned head */
	while ( 0 < size_head-- )
	{
		_MALI_SET_U8_ENDIAN_SAFE(dst, (u8)val);
		dst++;
	}

	/* bulk set */
	if( size_bulk != 0 )
	{
		MALI_DEBUG_ASSERT_ALIGNMENT(dst, 4);
		MALI_DEBUG_ASSERT((size_bulk % 4) == 0, ("number of bytes must be multiple of 4"));

		_mali_sys_memset(dst, val, size_bulk);

		dst += size_bulk;
	}

	/* tail */
	while ( 0 < size_tail-- )
	{
		_MALI_SET_U8_ENDIAN_SAFE(dst, (u8)val);
		dst++;
	}

	return p;
}

#endif /* MALI_BIG_ENDIAN */

