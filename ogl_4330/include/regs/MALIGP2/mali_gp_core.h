/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _MALIGP2_CONROL_REGS_H_
#define _MALIGP2_CONROL_REGS_H_

/**
 * Core specific constants
 */

#if defined(USING_MALI200)

#define MALIGP2_MAX_TILES                   300

#if (MALI200_HWVER >= 0x0002)
#define MALIGP2_MAX_VS_INSTRUCTION_COUNT    512
#else
#define MALIGP2_MAX_VS_INSTRUCTION_COUNT    256
#endif

#elif defined(USING_MALI400)

#define MALIGP2_MAX_TILES                   512
#define MALIGP2_MAX_VS_INSTRUCTION_COUNT    512

#elif defined(USING_MALI450)

#define MALIGP2_MAX_TILES                   4096
#define MALIGP2_MAX_VS_INSTRUCTION_COUNT    512

#else
#error "No supported mali core defined"
#endif

/** The required alignment of memory addresses that are added to GP registers */
#define GP_VARYINGS_BASE_ADDRESS 64

#define MALIGP2_VS_INSTRUCTION_SIZE			 16

#define MALIGP2_MAX_VS_OUTPUT_REGISTERS      16
#define MALIGP2_MAX_VS_CONSTANT_REGISTERS    304
#define MALIGP2_MAX_VS_INPUT_REGISTERS       16

/** 
 * Mali200 HW can read more data than necessary, causing page faults. 
 * The "+ 4" below prevents the read out of memory boundary to happen 
 */
#define MALIGP2_POINTER_ARRAY_BYTE_SIZE ((MALIGP2_MAX_TILES + 4) * 4)

#endif /* _MALIGP2_CONROL_REGS_H_ */
