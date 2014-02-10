/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef BASE_ARCH_BYTEORDER_H_
#define BASE_ARCH_BYTEORDER_H_


#ifndef MALI_DEBUG_SKIP_ASSERT
/*
 *  Macros for determining the endianess in run-time.
 *  Use these for debugging purposes only.
 */
static const int _mali_byteorder_check_const = 0x01;

/**
 * True if CPU is BIG endian
 */
#define _CPU_IS_BIG_ENDIAN()    ( (*(u8*)&_mali_byteorder_check_const == 0x00) )

/**
 * True if CPU is little endian
 */
#define _CPU_IS_LITTLE_ENDIAN() ( (*(u8*)&_mali_byteorder_check_const == 0x01) )

#endif

/*
 *
 *  Toolchain must define MALI_BIG_ENDIAN is compiled for
 *  big endian architecture
 */
#if MALI_BIG_ENDIAN

#define MALI_LITTLE_ENDIAN 0
#include <base_arch_byteorder_be.h>

#else
/*
 * Little-endian equivalents for all LE architectures
 *
 * All voids
 *
 */
#define MALI_LITTLE_ENDIAN 1

#define _SWAP_ENDIAN_U32_U32(x) (x)
#define _SWAP_ENDIAN_U32_U16(x) (x)
#define _SWAP_ENDIAN_U32_U8(x) (x)
#define _SWAP_ENDIAN_U64_U32(x) (x)
#define _MALI_GET_U8_ADDR_ENDIAN_SAFE(addr)  ( (addr)  )
#define _MALI_GET_U16_ADDR_ENDIAN_SAFE(addr) ( (addr) )
#define _MALI_GET_U32_ADDR_ENDIAN_SAFE(addr) ( (addr) )
#define _MALI_GET_U64_ADDR_ENDIAN_SAFE(addr) ( (addr) )

#define _MALI_GET_U64_ENDIAN_SAFE(addr)		 ( *(addr) )
#define _MALI_SET_U64_ENDIAN_SAFE(addr, val) ( *(addr)=(val) )

#endif /* MALI_BIG_ENDIAN */



#endif /* BASE_ARCH_BYTEORDER_H_ */
