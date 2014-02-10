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
 * OS and toolchain neutral implementations goes into this file.
 * This file is a part of 4 similar files, included in this order:
 * src/base/platform/<os>/<toolchain>/mali_intrinsics_os_tc.h - OS and toolchain specific
 * src/base/platform/common/<toolchain>/mali_intrinsics_tc.h - Toolchain specific
 * src/base/platform/<os>/common/mali_intrinsics_os.h - OS specific
 * src/base/platform/common/common/mali_intrinsics_cmn.h - Common for all OSes and toolchains
 */

#if !MALI_SATADDU8_DEFINED
MALI_STATIC_INLINE u32 _mali_osu_sataddu8(u32 x, u32 y)
{
	static const u32 signmask = 0x80808080;
	u32 t0 = (y ^ x) & signmask;
	u32 t1 = (y & x) & signmask;
	x &= ~signmask;
	y &= ~signmask;
	x += y;
	t1 |= t0 & x;
	t1 = (t1 << 1) - (t1 >> 7);
	return (x ^ t0) | t1;
}
#endif

#if !MALI_SATSUBU8_DEFINED
MALI_STATIC_INLINE u32 _mali_osu_satsubu8(u32 x, u32 y)
{
	static const u32 signmask = 0x80808080;
	u32 t0;
	u32 t1;
	y = ~y;
	t0 = (x ^ y) & signmask;
	t1 = (x & y) & signmask;
	x &= ~signmask;
	y &= ~signmask;
	x += y;
	t1 |= ((t0) & (x));
	t1 = ((t1 >> 7) - (t1 << 1)) - 1;
	return (((x+0x01010101) ^ t0) & (~t1));
}
#endif
