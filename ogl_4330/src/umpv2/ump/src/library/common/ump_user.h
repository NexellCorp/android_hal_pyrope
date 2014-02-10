/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/***********************************************/
/*This file has not to be released to customers*/
/*The one for release is in a different PATH   */
/***********************************************/

#ifndef _UMP_USER_H_
#define _UMP_USER_H_

#include <ump/ump.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
/* for release remove "#include <cdbg/mali_cdbg_assert.h>" below*/

#if MALI_UNIT_TEST
#include <cdbg/mali_cdbg_assert.h>
#define UMP_PRINT_WARN printf
				/* in #define UMP_ASSERT_MSG(expr, ...) for release
				 * to replace #define UMP_ASSERT_MSG CDBG_ASSERT_MSG
				 * with the one below commented.
				 *
				 * assert();\
				 *
				 * */
#define UMP_ASSERT_MSG CDBG_ASSERT_MSG
/*#define UMP_ASSERT_MSG(expr, ...)\
	do\
	{\
	 if(!(expr))\
			{\
				printf("UMP ASSERT"__VA_ARGS__"\n");\
				assert((bool_t)0);\
			}\
		}while(0)*/
#define UMP_ASSERT(expr)\
		UMP_ASSERT_MSG(expr, #expr)
#else
#define UMP_PRINT_WARN(...) ((void)#__VA_ARGS__)
#define UMP_ASSERT(...) ((void)#__VA_ARGS__)
#define UMP_ASSERT_MSG(expr, ...) ((void)#__VA_ARGS__)
#endif

typedef uint32_t bool_t;
#define INLINE __inline__
typedef bool_t bool;

typedef struct ump_mem
{
	ump_secure_id id; 		/* secure id of the allocation */
	volatile u32 refcnt; 		/* handle local usage count (to avoid uneeded kernel calls) */
	ump_alloc_flags flags;	/* Flags passed during allocation */

	/* used by v1 API */
	void * mapped_mem;   	/* Mapped memory; all read and write use this */
} ump_mem;

extern int ump_fd;

/**
 * Validates UMP allocation flags
 *
 * Asserts on invalid allocation flags
 *   - at least one flag should be set
 *   - at least one device must be reading from the allocated memory
 *   - at least one device must be writing to the allocated memory
 *
 * Sets implicit flags
 *    - Hint flags implicitly set PROT flags
 *
 * Clears flags not in use by the UMP
 *
 * @param  flags UMP allocation flags
 * @return flags updated for implicitly set flags
 */
static INLINE ump_alloc_flags umpp_validate_alloc_flags(ump_alloc_flags flags)
{
	/* Clear any flags not in use by the UMP API */
	u32 device_shift[] = { UMP_DEVICE_CPU_SHIFT,
						   UMP_DEVICE_W_SHIFT,
						   UMP_DEVICE_X_SHIFT,
						   UMP_DEVICE_Y_SHIFT,
	 					   UMP_DEVICE_Z_SHIFT};
	u32 i;

	flags &= ~UMPP_ALLOCBITS_UNUSED;

	for(i = 0; i < (sizeof(device_shift)/sizeof(device_shift[0])); i++)
	{
		/* Hint flags implicitly set Prot flags */
		if( flags & (UMP_HINT_DEVICE_RD << device_shift[i]) )
		{
			flags |= UMP_PROT_DEVICE_RD << device_shift[i];
		}
		if( flags & (UMP_HINT_DEVICE_WR << device_shift[i]) )
		{
			flags |= UMP_PROT_DEVICE_WR << device_shift[i];
		}
	}

	UMP_ASSERT_MSG( ((UMP_PROT_CPU_RD | UMP_PROT_W_RD | UMP_PROT_X_RD | UMP_PROT_Y_RD | UMP_PROT_Z_RD)
				& flags) != 0, "There must be at least one device with read right (flags 0x%x)", flags);
	UMP_ASSERT_MSG( ((UMP_PROT_CPU_WR | UMP_PROT_W_WR | UMP_PROT_X_WR | UMP_PROT_Y_WR | UMP_PROT_Z_WR)
				& flags) != 0, "There must be at least one device with write right (flags 0x%x)", flags);

	return flags;
}

#endif /* _UMP_USER_H_ */
