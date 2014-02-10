/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*
 * Toolchain specific (but OS neutral) implementations goes into this file.
 * This file is a part of 4 similar files, included in this order:
 * src/base/platform/<os>/<toolchain>/mali_intrinsics_os_tc.h - OS and toolchain specific
 * src/base/platform/common/<toolchain>/mali_intrinsics_tc.h - Toolchain specific
 * src/base/platform/<os>/common/mali_intrinsics_os.h - OS specific
 * src/base/platform/common/common/mali_intrinsics_cmn.h - Common for all OSes and toolchains
 */

/* Make use of ARMv6 SIMD using inlined assembly code */
#if (MALI_PLATFORM_ARM_ARCH >= 6)

#define MALI_SATADDU8_DEFINED 1
MALI_STATIC_INLINE u32 _mali_osu_sataddu8(u32 x, u32 y)
{
	u32 retval;
	__asm__( "UQADD8 %0, %1, %2"
			: "=r" (retval)     /* output */
			: "r" (x), "r" (y)  /* input */
			);
	return retval;
}

#define MALI_SATSUBU8_DEFINED 1
MALI_STATIC_INLINE u32 _mali_osu_satsubu8(u32 x, u32 y)
{
	u32 retval;
	__asm__( "UQSUB8 %0, %1, %2"
			: "=r" (retval)     /* output */
			: "r" (x), "r" (y)  /* input */
			);
	return retval;
}

#endif
