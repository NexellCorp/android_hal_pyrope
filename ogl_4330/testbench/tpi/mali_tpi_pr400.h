/*
 * $Copyright:
 * ----------------------------------------------------------------------------
 *  This confidential and proprietary software may be used only as authorized
 *  by a licensing agreement from ARM Limited.
 *          (C) COPYRIGHT 2009-2010, 2012 ARM Limited , ALL RIGHTS RESERVED
 *  The entire notice above must be reproduced on all authorized copies and
 *  copies may only be made to the extent permitted by a licensing agreement
 *  from ARM Limited.
 * ----------------------------------------------------------------------------
 * $
 */
 
#ifndef _MALI_TPI_PR400_H_
#define _MALI_TPI_PR400_H_

#ifdef __cplusplus
extern "C" {
#endif

void mali_tpi_abort(void);

void * mali_tpi_memcpy(void * destination, const void * source, u32 num);

#ifdef __cplusplus
};
#endif

#define TPI_ASSERT_MSG(expr, ...)\
	do \
	{ \
		if(!(expr)) \
		{ \
			mali_tpi_printf(__VA_ARGS__); \
			mali_tpi_abort(); \
		} \
	}while(0)

#define TPI_ASSERT_POINTER( ptr )\
	TPI_ASSERT_MSG( NULL != (ptr), "\"%s\" is NULL", #ptr)


#endif /* End (_MALI_TPI_PR400_H_) */
