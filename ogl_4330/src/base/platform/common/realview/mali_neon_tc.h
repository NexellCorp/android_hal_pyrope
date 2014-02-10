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
 * OS + toolchain specific implementations goes into this file
 * This file is a part of 4 similar files, included in this order:
 * src/base/platform/<os>/<toolchain>/mali_neon_os_tc.h - OS and toolchain specific
 * src/base/platform/common/<toolchain>/mali_neon_tc.h - Toolchain specific
 * src/base/platform/<os>/common/mali_neon_os.h - OS specific
 * src/base/platform/common/common/mali_neon_cmn.h - Common for all OSes and toolchains
 */
#if (MALI_PLATFORM_ARM_NEON)
#define MALI_TEX8_L_TO_TEX8_B_FULL_BLOCK_DEFINED 1
extern void _mali_osu_tex8_l_to_tex8_b_full_block(
	u8*       dst,
	const u8* src,
	u32       src_pitch );

#define MALI_TEX16_L_TO_TEX16_B_FULL_BLOCK_DEFINED 1
extern void _mali_osu_tex16_l_to_tex16_b_full_block(
	u8*       dst,
	const u8* src,
	u32       src_pitch );

#define MALI_TEX24_L_TO_TEX24_B_FULL_BLOCK_DEFINED 1
extern void _mali_osu_tex24_l_to_tex24_b_full_block(
	u8*       dst,
	const u8* src,
	u32       src_pitch );

#define MALI_TEX32_L_TO_TEX32_B_FULL_BLOCK_DEFINED 1
extern void _mali_osu_tex32_l_to_tex32_b_full_block(
	u8*       dst,
	const u8* src,
	u32       src_pitch );

#define MALI_TEX32_L_TO_TEX32_L_SWAP 1
extern void _mali_osu_tex32_l_to_tex32_l_swap_2_4(
	u8*	  dst,
	const u8* src);

extern void _mali_osu_tex32_l_to_tex32_l_swap_1_3(
	u8*	  dst,
	const u8* src);
	

#define MALI_TEX24_L_TO_TEX32_B_FULL_BLOCK_DEFINED 1
extern void _mali_osu_tex24_l_to_tex32_b_full_block(
	u8*       dst,
	const u8* src,
	u32       src_pitch );

#define MALI_TEX32_L_TO_TEX32_B_FULL_BLOCK_PREMULT_DEFINED 1
void _mali_osu_tex32_l_to_tex32_b_full_block_premult(
	u8*      dst_pre,
	u8*      dst_nonpre,
	const u8 *src,
	u32      src_pitch );

#define MALI_TEX32_L_TO_TEX32_B_FULL_BLOCK_PREMULT_REVERSE_DEFINED 1
void _mali_osu_tex32_l_to_tex32_b_full_block_premult_reverse(
	u8*      dst_pre,
	u8*      dst_nonpre,
	const u8 *src,
	u32      src_pitch );

#define MALI_TEX32_L_TO_TEX32_B_RGBX_FULL_BLOCK_DEFINED 1
void _mali_osu_tex32_l_to_tex32_b_rgbx_full_block(
	u8*      dst_pre,
	const u8 *src,
	u32      src_pitch );

#define MALI_TEX32_L_TO_TEX32_B_XBGR_FULL_BLOCK_DEFINED 1
void _mali_osu_tex32_l_to_tex32_b_xbgr_full_block(
	u8*      dst_pre,
	const u8 *src,
	u32      src_pitch );

#define MALI_TEX32_L_TO_TEX32_B_RGBX_PREMULT_FULL_BLOCK_DEFINED 1
void _mali_osu_tex32_l_to_tex32_b_rgbx_premult_full_block(
	u8*      dst_pre,
	const u8 *src,
	u32      src_pitch,
	u8*      dst_nonpre );

#define MALI_TEX32_L_TO_TEX32_B_XBGR_PREMULT_FULL_BLOCK_DEFINED 1
void _mali_osu_tex32_l_to_tex32_b_xbgr_premult_full_block(
	u8*      dst_pre,
	const u8 *src,
	u32      src_pitch,
	u8*      dst_nonpre );

#endif /* ((MALI_PLATFORM_ARM_NEON) */

