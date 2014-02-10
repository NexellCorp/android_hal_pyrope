/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2012 ARM Limited
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
	u8*      dst,
	const u8 *src,
	u32      src_pitch );

#define MALI_TEX32_L_TO_TEX32_B_XBGR_FULL_BLOCK_DEFINED 1
void _mali_osu_tex32_l_to_tex32_b_xbgr_full_block(
	u8*      dst,
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

#define MALI_DOWNSAMPLE_VIANEON_DEFINED 1

extern void _mali_osu_downsample8888(
	const void* src, 
	void* dst, 
	u32 sample_mask);

extern void _mali_osu_downsample888(
	const void* src, 
	void* dst, 
	u32 sample_mask);

extern void _mali_osu_downsample88(
	const void* src, 
	void* dst, 
	u32 sample_mask);

extern void _mali_osu_downsample8(
	const void* src, 
	void* dst, 
	u32 sample_mask);

#define MALI_OSU_MATH 1

void _mali_osu_matrix4x4_multiply(float* dst, float* src1, const float *src2);
void _mali_osu_matrix4x4_copy(float* dst, const float* src1);
void _mali_osu_matrix4x4_transpose(float* dst, float* src);
void _mali_osu_matrix4x4_translate( float* src, float* x, float* y, float* z );
void _mali_osu_matrix4x4_scale(float* src, float* x, float* y, float* z );

#ifdef MALI_BB_TRANSFORM
#define MALI_BB_TRANSFORM_ON_NEON 1
void _mali_neon_matrix4x4_vector3_multiply_f32(u32* params);
void _mali_neon_matrix4x4_vector3_multiply_and_vp_convert(u32* params);
void _mali_neon_minmax_4f32(void* input, u32 stride, u32 count, void* output);
void _mali_neon_transform_and_produce_clip_bits(u32* params);
#endif

#endif /* (MALI_PLATFORM_ARM_NEON) */

