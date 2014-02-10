/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _MALI_CONVERT_INTRINSICS_H_
#define _MALI_CONVERT_INTRINSICS_H_

#if (MALI_PLATFORM_ARM_NEON)
#include <arm_neon.h>

/* LOAD/STORE FUNCTIONS */

/* 8-bit */

/**
 * Function for loading a single 8 texel long L8 row to NEON intrinsics.
 * @param src_ptr pointer to start of row.
 * @param row NEON registers to be filled in an RGBA fashion.
 * @param surfspec packed surface specifier for the src based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_load_l8(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec);

/**
 * Function for storing a single NEON intrinsics row in L8.
 * @param dst_ptr pointer to start of 8x8 tile.
 * @param row single RGBA row.
 * @param surfspec packed surface specifier for the dst based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_store_l8(u8 *dst_ptr, uint8x8x4_t *row, const u16 surfspec);

/**
 * Function for loading a single 8 texel long A8 row to NEON intrinsics.
 * @param src_ptr pointer to start of row.
 * @param row NEON registers to be filled in an RGBA fashion.
 * @param surfspec packed surface specifier for the src based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_load_a8(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec);

/**
 * Function for loading a single 8 texel long I8 row to NEON intrinsics.
 * @param src_ptr pointer to start of row.
 * @param row NEON registers to be filled in an RGBA fashion.
 * @param surfspec packed surface specifier for the src based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_load_i8(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec);

/**
 * Function for storing a single NEON intrinsics row in A8 or I8.
 * @param dst_ptr pointer to start of 8x8 tile.
 * @param row single RGBA row.
 * @param surfspec packed surface specifier for the dst based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_store_a8_i8(u8 *dst_ptr, uint8x8x4_t *row, const u16 surfspec);

/* 16-bit */

/**
 * Function for loading a single 8 texel long RGB565 row to NEON intrinsics.
 * @param src_ptr pointer to start of row.
 * @param row NEON registers to be filled in an RGBA fashion.
 * @param surfspec packed surface specifier for the src based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_load_rgb565(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec);

/**
 * Function for storing a single NEON intrinsics row in RGB565.
 * @param dst_ptr pointer to start of 8x8 tile.
 * @param row single RGBA row.
 * @param surfspec packed surface specifier for the dst based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_store_rgb565(u8 *dst_ptr, uint8x8x4_t *row, const u16 surfspec);

/**
 * Function for loading a single 8 texel long ARGB1555 row to NEON intrinsics.
 * @param src_ptr pointer to start of row.
 * @param row NEON registers to be filled in an RGBA fashion.
 * @param surfspec packed surface specifier for the src based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_load_argb1555(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec);

/**
 * Function for storing a single NEON intrinsics row in ARGB4444.
 * @param dst_ptr pointer to start of 8x8 tile.
 * @param row single RGBA row.
 * @param surfspec packed surface specifier for the dst based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_store_argb1555(u8 *dst_ptr, uint8x8x4_t *row, const u16 surfspec);

/**
 * Function for loading a single 8 texel long ARGB1555 row to NEON intrinsics.
 * @param src_ptr pointer to start of row.
 * @param row NEON registers to be filled in an RGBA fashion.
 * @param surfspec packed surface specifier for the src based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_load_argb4444(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec);

/**
 * Function for storing a single NEON intrinsics row in ARGB4444.
 * @param dst_ptr pointer to start of 8x8 tile.
 * @param row single RGBA row.
 * @param surfspec packed surface specifier for the dst based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_store_argb4444(u8 *dst_ptr, uint8x8x4_t *row, const u16 surfspec);

/**
 * Function for loading a single 8 texel long AL88 row to NEON intrinsics.
 * @param src_ptr pointer to start of row.
 * @param row NEON registers to be filled in an RGBA fashion.
 * @param surfspec packed surface specifier for the src based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_load_al88(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec);

/**
 * Function for storing a single NEON intrinsics row in AL88.
 * @param dst_ptr pointer to start of 8x8 tile.
 * @param row single RGBA row.
 * @param surfspec packed surface specifier for the dst based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_store_al88(u8 *dst_ptr, uint8x8x4_t *row, const u16 surfspec);

/* 24-bit */

/**
 * Function for loading a single 8 texel long RGB565 row to NEON intrinsics.
 * @param src_ptr pointer to start of row.
 * @param row NEON registers to be filled in an RGBA fashion.
 * @param surfspec packed surface specifier for the src based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_load_rgb888(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec);

/**
 * Function for storing a single NEON intrinsics row in RGB888.
 * @param dst_ptr pointer to start of 8x8 tile.
 * @param row single RGBA row.
 * @param surfspec packed surface specifier for the dst based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_store_rgb888(u8 *dst_ptr, uint8x8x4_t *row, const u16 surfspec);

/* 32-bit */

/**
 * Function for loading a single 8 texel long ARGB8888 row to NEON intrinsics.
 * @param src_ptr pointer to start of row.
 * @param row NEON registers to be filled in an RGBA fashion.
 * @param surfspec packed surface specifier for the src based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_load_argb8888(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec);

/**
 * Function for storing a single NEON intrinsics row in ARGB8888.
 * @param dst_ptr pointer to start of 8x8 tile.
 * @param row single RGBA row.
 * @param surfspec packed surface specifier for the dst based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_store_argb8888(u8 *dst_ptr, uint8x8x4_t *row, const u16 surfspec);

/**
 * Helper function for loading NEON data. Pretty much contains a switch and calls to the other load functions.
 * @param stc_ptr pointer to start of 8x4 tile.
 * @param src_offset the number of bytes each row is offset by. equal to src_pitch for LINEAR; equal to 8xsrc_bytespp for 16x16BLOCKED.
 * @param row0 NEON registers to be filled in an RGBA fashion.
 * @param row1 NEON registers to be filled in an RGBA fashion.
 * @param row2 NEON registers to be filled in an RGBA fashion.
 * @param row3 NEON registers to be filled in an RGBA fashion.
 * @param surfspec packed surface specifier for the src based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_load_8x4(
	u8 *src_ptr,
	u32 src_offset,
	uint8x8x4_t *row0,
	uint8x8x4_t *row1,
	uint8x8x4_t *row2,
	uint8x8x4_t *row3,
	const u16 src_surfspec );

/**
 * Helper function for storing NEON data. Pretty much contains a switch and calls to the other store functions.
 * @param dst_ptr pointer to start of 8x4 tile.
 * @param dst_offset the number of bytes each row is offset by. equal to dst_pitch for LINEAR; equal to 8xdst_bytespp for 16x16BLOCKED.
 * @param row0 single RGBA row.
 * @param row1 single RGBA row.
 * @param row2 single RGBA row.
 * @param row3 single RGBA row.
 * @param dst_surfspec packed surface specifier for the dst based on the PACK_SURFSPEC macro.
 */
void _mali_convert_intrinsics_store_8x4(
	u8 *dst_ptr,
	u32 dst_offset,
	uint8x8x4_t *row0,
	uint8x8x4_t *row1,
	uint8x8x4_t *row2,
	uint8x8x4_t *row3,
	const u16 dst_surfspec );

/* MAIN FUNCTIONS */

/**
 * Main function for converting a 8x8 tile using NEON intrinsics
 * @param src_ptr
 * @param src_offset the number of bytes each row is offset by. equal to src_pitch for LINEAR; equal to 8xsrc_bytespp for 16x16BLOCKED.
 * @param src_surfspec packed surface specifier for the src based on the PACK_SURFSPEC macro.
 * @param dst_ptr
 * @param dst_nonpre_ptr set if nonpremultiplied alpha textures should be saved, NULL otherwise.
 * @param dst_offset the number of bytes each row is offset by. equal to dst_pitch for LINEAR; equal to 8xdst_bytespp for 16x16BLOCKED.
 * @param dst_surfspec packed surface specifier for the dst based on the PACK_SURFSPEC macro.
 * @param conv_rules packed conversion rules, made by the _mali_convert_setup_conversion_rules()-function.
 * @param swizzle MALI_TRUE if swizzling should be done, MALI_FALSE otherwise.
 * @param deswizzle MALI_TRUE if deswizzling should be done, MALI_FALSE otherwise.
 */
void _mali_convert_intrinsics_8x8_tile(
	u8 *src_ptr,
	u32 src_offset,
       	const u16 src_surfspec,
	u8 *dst_ptr,
	u8 *dst_nonpre_ptr,
	u32 dst_offset,
	const u16 dst_surfspec,
	const u8 source,
	const u32 conv_rules,
	mali_bool swizzle,
	mali_bool deswizzle );

#endif /* (MALI_PLATFORM_ARM_NEON) */
#endif /* _MALI_CONVERT_INTRINSICS_H_ */
