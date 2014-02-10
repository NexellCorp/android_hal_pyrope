/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <arm_neon.h>
#include <base/mali_types.h>

/* HELPER FUNCTIONS */

/* lRGB to sRGB LUT */
const u8 _mali_convert_intrinsics_linear_to_nonlinear_lut[256] =
{
	0, 13, 22, 28, 33, 38, 42, 46, 49, 53, 56, 58, 61, 64, 66, 68, 71,
	73, 75, 77, 79, 81, 83, 85, 86, 88, 90, 91, 93, 95, 96, 98, 99,
	101, 102, 103, 105, 106, 108, 109, 110, 112, 113, 114, 115, 116, 118, 119, 120,
	121, 122, 123, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137,
	138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 147, 148, 149, 150, 151, 152,
	153, 154, 154, 155, 156, 157, 158, 159, 159, 160, 161, 162, 163, 163, 164, 165,
	166, 167, 167, 168, 169, 170, 170, 171, 172, 173, 173, 174, 175, 175, 176, 177,
	178, 178, 179, 180, 180, 181, 182, 182, 183, 184, 184, 185, 186, 186, 187, 188,
	188, 189, 190, 190, 191, 192, 192, 193, 194, 194, 195, 195, 196, 197, 197, 198,
	199, 199, 200, 200, 201, 202, 202, 203, 203, 204, 205, 205, 206, 206, 207, 207,
	208, 209, 209, 210, 210, 211, 211, 212, 213, 213, 214, 214, 215, 215, 216, 216,
	217, 218, 218, 219, 219, 220, 220, 221, 221, 222, 222, 223, 223, 224, 224, 225,
	226, 226, 227, 227, 228, 228, 229, 229, 230, 230, 231, 231, 232, 232, 233, 233,
	234, 234, 235, 235, 236, 236, 237, 237, 237, 238, 238, 239, 239, 240, 240, 241,
	241, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 247, 247, 248, 248,
	249, 249, 250, 250, 251, 251, 251, 252, 252, 253, 253, 254, 254, 255, 255
};

/* sRGB to lRGB LUT */
const u8 _mali_convert_intrinsics_nonlinear_to_linear_lut[256] =
{
	0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4,
	4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 8,
	8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 12, 12, 12, 13, 13,
	14, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 18, 19, 19, 20, 21,
	21, 22, 22, 23, 23, 24, 24, 25, 26, 26, 27, 27, 28, 29, 29, 30,
	31, 31, 32, 33, 33, 34, 35, 35, 36, 37, 38, 38, 39, 40, 41, 41,
	42, 43, 44, 45, 45, 46, 47, 48, 49, 50, 51, 51, 52, 53, 54, 55,
	56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
	72, 73, 74, 76, 77, 78, 79, 80, 81, 82, 84, 85, 86, 87, 88, 90,
	91, 92, 93, 95, 96, 97, 99, 100, 101, 103, 104, 105, 107, 108, 109, 111,
	112, 114, 115, 116, 118, 119, 121, 122, 124, 125, 127, 128, 130, 131, 133, 134,
	136, 138, 139, 141, 142, 144, 146, 147, 149, 151, 152, 154, 156, 157, 159, 161,
	163, 164, 166, 168, 170, 172, 173, 175, 177, 179, 181, 183, 184, 186, 188, 190,
	192, 194, 196, 198, 200, 202, 204, 206, 208, 210, 212, 214, 216, 218, 220, 222,
	224, 226, 229, 231, 233, 235, 237, 239, 242, 244, 246, 248, 250, 253, 255
};

/* Helper function for converting from linear to nonlinear (lRGB->sRGB) colorspace, or vice versa.
 * @param row ringle RGBA row.
 * @param linear_to_nonlinear MALI_TRUE if conversion is from linear to nonlinear, MALI_FALSE for nonlinear to linear.
 */
MALI_STATIC_INLINE void _mali_convert_intrinsics_linear_nonlinear(uint8x8x4_t *row, mali_bool linear_to_nonlinear)
{
	/* TODO: Optimization: vshr #3 the row values, find distinct values and only fetch the corresponding rows
	 * in the LUT. At most 8, instead of 16 like now */
	/* TODO: Optimization: Do two argb-rows at a time instead of one like now. There should be enough free
	 * NEON registers to achieve this */
	u32 i;
	u8 *lut_ptr;
	uint8x8x2_t lut_row0, lut_row1;
	uint8x16_t lut_ld_row;
	uint8x8_t temp;
	uint8x8x3_t temp_rgb;

	if (MALI_TRUE == linear_to_nonlinear)
	{
		lut_ptr = (u8*)_mali_convert_intrinsics_linear_to_nonlinear_lut;
	}
	else
	{
		lut_ptr = (u8*)_mali_convert_intrinsics_nonlinear_to_linear_lut;
	}

	/* Unrolled the first iteration of the loop to avoid branch when setting temp_rgb */
	lut_ld_row = vld1q_u8(lut_ptr);
	lut_ptr += 16;
	/* This _should_ compile into reinterpreting the q register as two d registers */
	lut_row0.val[0] = vget_low_u8(lut_ld_row);
	lut_row0.val[1] = vget_high_u8(lut_ld_row);
	/* Optimally, one should load two q registers in one operation. NEON supports this, intrinsics do not */
	lut_ld_row = vld1q_u8(lut_ptr);
	lut_ptr += 16;
	lut_row1.val[0] = vget_low_u8(lut_ld_row);
	lut_row1.val[1] = vget_high_u8(lut_ld_row);

	/* Red */
	/* TODO: Optimization: use a single vtbl4_u8 instead of two vtbl2_u8's */
	temp = vtbl2_u8(lut_row0, row->val[0]);
	temp_rgb.val[0] = vshl_n_u8(temp, 0);
	temp = vdup_n_u8(0x10);
	row->val[0] = vsub_u8(row->val[0], temp);
	temp = vtbl2_u8(lut_row1, row->val[0]);
	temp_rgb.val[0] = vorr_u8(temp_rgb.val[0], temp);
	temp = vdup_n_u8(0x10);
	row->val[0] = vsub_u8(row->val[0], temp);

	/* Green */
	temp = vtbl2_u8(lut_row0, row->val[1]);
	temp_rgb.val[1] = vshl_n_u8(temp, 0);
	temp = vdup_n_u8(0x10);
	row->val[1] = vsub_u8(row->val[1], temp);
	temp = vtbl2_u8(lut_row1, row->val[1]);
	temp_rgb.val[1] = vorr_u8(temp_rgb.val[1], temp);
	temp = vdup_n_u8(0x10);
	row->val[1] = vsub_u8(row->val[1], temp);

	/* Blue */
	temp = vtbl2_u8(lut_row0, row->val[2]);
	temp_rgb.val[2] = vshl_n_u8(temp, 0);
	temp = vdup_n_u8(0x10);
	row->val[2] = vsub_u8(row->val[2], temp);
	temp = vtbl2_u8(lut_row1, row->val[2]);
	temp_rgb.val[2] = vorr_u8(temp_rgb.val[2], temp);
	temp = vdup_n_u8(0x10);
	row->val[2] = vsub_u8(row->val[2], temp);

	for (i = 2; i < 16; i+=2)
	{
		lut_ld_row = vld1q_u8(lut_ptr);
		lut_ptr += 16;
		lut_row0.val[0] = vget_low_u8(lut_ld_row);
		lut_row0.val[1] = vget_high_u8(lut_ld_row);
		lut_ld_row = vld1q_u8(lut_ptr);
		lut_ptr += 16;
		lut_row1.val[0] = vget_low_u8(lut_ld_row);
		lut_row1.val[1] = vget_high_u8(lut_ld_row);

		/* Red */
		temp = vtbl2_u8(lut_row0, row->val[0]);
		temp_rgb.val[0] = vorr_u8(temp_rgb.val[0], temp);
		temp = vdup_n_u8(0x10);
		row->val[0] = vsub_u8(row->val[0], temp);
		temp = vtbl2_u8(lut_row1, row->val[0]);
		temp_rgb.val[0] = vorr_u8(temp_rgb.val[0], temp);
		temp = vdup_n_u8(0x10);
		row->val[0] = vsub_u8(row->val[0], temp);

		/* Green */
		temp = vtbl2_u8(lut_row0, row->val[1]);
		temp_rgb.val[1] = vorr_u8(temp_rgb.val[1], temp);
		temp = vdup_n_u8(0x10);
		row->val[1] = vsub_u8(row->val[1], temp);
		temp = vtbl2_u8(lut_row1, row->val[1]);
		temp_rgb.val[1] = vorr_u8(temp_rgb.val[1], temp);
		temp = vdup_n_u8(0x10);
		row->val[1] = vsub_u8(row->val[1], temp);

		/* Blue */
		temp = vtbl2_u8(lut_row0, row->val[2]);
		temp_rgb.val[2] = vorr_u8(temp_rgb.val[2], temp);
		temp = vdup_n_u8(0x10);
		row->val[2] = vsub_u8(row->val[2], temp);
		temp = vtbl2_u8(lut_row1, row->val[2]);
		temp_rgb.val[2] = vorr_u8(temp_rgb.val[2], temp);
		temp = vdup_n_u8(0x10);
		row->val[2] = vsub_u8(row->val[2], temp);
	}

	row->val[0] = vshl_n_u8(temp_rgb.val[0], 0);
	row->val[1] = vshl_n_u8(temp_rgb.val[1], 0);
	row->val[2] = vshl_n_u8(temp_rgb.val[2], 0);
}

/**
 * Helper function for swapping two uint8x8_t vectors.
 * @param vec1
 * @param vec2
 */
MALI_STATIC_INLINE void _mali_convert_intrinsics_swap_uint8x8_t(uint8x8_t *vec1, uint8x8_t *vec2)
{
	*vec1 = veor_u8(*vec1, *vec2);
	*vec2 = veor_u8(*vec2, *vec1);
	*vec1 = veor_u8(*vec1, *vec2);
}

/**
 * Helper function for dividing one uint16x8_t vector by another. The result is stored in the divident vector.
 * @note This function assumes that all elements in the divisor are greater than zero. This has to be checked by the caller.
 * @param divident vector containing the dividents.
 * @param divisor vector containing the divisors.
 */
MALI_STATIC_INLINE void _mali_convert_intrinsics_div_uint16x8_t(uint16x8_t *divident, uint16x8_t *divisor)
{
	/* This function assumes that all values in divisor are greater than zero */
	uint16x8_t remainder, quotient, mask, cmp_mask, temp;

	mask = vdupq_n_u16(0x8000);
	remainder = vshrq_n_u16(*divident, 15);
	cmp_mask = vcgeq_u16(remainder, *divisor);
	temp = vandq_u16(cmp_mask, *divisor);
	remainder = vsubq_u16(remainder, temp);
	quotient = vandq_u16(cmp_mask, mask);
	mask = vshrq_n_u16(mask, 1);

	/* Because vshrq_n_u16 only accepts constants, have to unroll the loop */	

	remainder = vshlq_n_u16(remainder, 1);
	temp = vandq_u16(*divident, mask);
	temp = vshrq_n_u16(temp, 14);
	remainder = vorrq_u16(remainder, temp);
	cmp_mask = vcgeq_u16(remainder, *divisor);
	temp = vandq_u16(cmp_mask, *divisor);
	remainder = vsubq_u16(remainder, temp);
	temp = vandq_u16(cmp_mask, mask);
	quotient = vorrq_u16(quotient, temp);
	mask = vshrq_n_u16(mask, 1);

	remainder = vshlq_n_u16(remainder, 1);
	temp = vandq_u16(*divident, mask);
	temp = vshrq_n_u16(temp, 13);
	remainder = vorrq_u16(remainder, temp);
	cmp_mask = vcgeq_u16(remainder, *divisor);
	temp = vandq_u16(cmp_mask, *divisor);
	remainder = vsubq_u16(remainder, temp);
	temp = vandq_u16(cmp_mask, mask);
	quotient = vorrq_u16(quotient, temp);
	mask = vshrq_n_u16(mask, 1);

	remainder = vshlq_n_u16(remainder, 1);
	temp = vandq_u16(*divident, mask);
	temp = vshrq_n_u16(temp, 12);
	remainder = vorrq_u16(remainder, temp);
	cmp_mask = vcgeq_u16(remainder, *divisor);
	temp = vandq_u16(cmp_mask, *divisor);
	remainder = vsubq_u16(remainder, temp);
	temp = vandq_u16(cmp_mask, mask);
	quotient = vorrq_u16(quotient, temp);
	mask = vshrq_n_u16(mask, 1);

	remainder = vshlq_n_u16(remainder, 1);
	temp = vandq_u16(*divident, mask);
	temp = vshrq_n_u16(temp, 11);
	remainder = vorrq_u16(remainder, temp);
	cmp_mask = vcgeq_u16(remainder, *divisor);
	temp = vandq_u16(cmp_mask, *divisor);
	remainder = vsubq_u16(remainder, temp);
	temp = vandq_u16(cmp_mask, mask);
	quotient = vorrq_u16(quotient, temp);
	mask = vshrq_n_u16(mask, 1);

	remainder = vshlq_n_u16(remainder, 1);
	temp = vandq_u16(*divident, mask);
	temp = vshrq_n_u16(temp, 10);
	remainder = vorrq_u16(remainder, temp);
	cmp_mask = vcgeq_u16(remainder, *divisor);
	temp = vandq_u16(cmp_mask, *divisor);
	remainder = vsubq_u16(remainder, temp);
	temp = vandq_u16(cmp_mask, mask);
	quotient = vorrq_u16(quotient, temp);
	mask = vshrq_n_u16(mask, 1);

	remainder = vshlq_n_u16(remainder, 1);
	temp = vandq_u16(*divident, mask);
	temp = vshrq_n_u16(temp, 9);
	remainder = vorrq_u16(remainder, temp);
	cmp_mask = vcgeq_u16(remainder, *divisor);
	temp = vandq_u16(cmp_mask, *divisor);
	remainder = vsubq_u16(remainder, temp);
	temp = vandq_u16(cmp_mask, mask);
	quotient = vorrq_u16(quotient, temp);
	mask = vshrq_n_u16(mask, 1);

	remainder = vshlq_n_u16(remainder, 1);
	temp = vandq_u16(*divident, mask);
	temp = vshrq_n_u16(temp, 8);
	remainder = vorrq_u16(remainder, temp);
	cmp_mask = vcgeq_u16(remainder, *divisor);
	temp = vandq_u16(cmp_mask, *divisor);
	remainder = vsubq_u16(remainder, temp);
	temp = vandq_u16(cmp_mask, mask);
	quotient = vorrq_u16(quotient, temp);
	mask = vshrq_n_u16(mask, 1);

	remainder = vshlq_n_u16(remainder, 1);
	temp = vandq_u16(*divident, mask);
	temp = vshrq_n_u16(temp, 7);
	remainder = vorrq_u16(remainder, temp);
	cmp_mask = vcgeq_u16(remainder, *divisor);
	temp = vandq_u16(cmp_mask, *divisor);
	remainder = vsubq_u16(remainder, temp);
	temp = vandq_u16(cmp_mask, mask);
	quotient = vorrq_u16(quotient, temp);
	mask = vshrq_n_u16(mask, 1);

	remainder = vshlq_n_u16(remainder, 1);
	temp = vandq_u16(*divident, mask);
	temp = vshrq_n_u16(temp, 6);
	remainder = vorrq_u16(remainder, temp);
	cmp_mask = vcgeq_u16(remainder, *divisor);
	temp = vandq_u16(cmp_mask, *divisor);
	remainder = vsubq_u16(remainder, temp);
	temp = vandq_u16(cmp_mask, mask);
	quotient = vorrq_u16(quotient, temp);
	mask = vshrq_n_u16(mask, 1);

	remainder = vshlq_n_u16(remainder, 1);
	temp = vandq_u16(*divident, mask);
	temp = vshrq_n_u16(temp, 5);
	remainder = vorrq_u16(remainder, temp);
	cmp_mask = vcgeq_u16(remainder, *divisor);
	temp = vandq_u16(cmp_mask, *divisor);
	remainder = vsubq_u16(remainder, temp);
	temp = vandq_u16(cmp_mask, mask);
	quotient = vorrq_u16(quotient, temp);
	mask = vshrq_n_u16(mask, 1);

	remainder = vshlq_n_u16(remainder, 1);
	temp = vandq_u16(*divident, mask);
	temp = vshrq_n_u16(temp, 4);
	remainder = vorrq_u16(remainder, temp);
	cmp_mask = vcgeq_u16(remainder, *divisor);
	temp = vandq_u16(cmp_mask, *divisor);
	remainder = vsubq_u16(remainder, temp);
	temp = vandq_u16(cmp_mask, mask);
	quotient = vorrq_u16(quotient, temp);
	mask = vshrq_n_u16(mask, 1);

	remainder = vshlq_n_u16(remainder, 1);
	temp = vandq_u16(*divident, mask);
	temp = vshrq_n_u16(temp, 3);
	remainder = vorrq_u16(remainder, temp);
	cmp_mask = vcgeq_u16(remainder, *divisor);
	temp = vandq_u16(cmp_mask, *divisor);
	remainder = vsubq_u16(remainder, temp);
	temp = vandq_u16(cmp_mask, mask);
	quotient = vorrq_u16(quotient, temp);
	mask = vshrq_n_u16(mask, 1);

	remainder = vshlq_n_u16(remainder, 1);
	temp = vandq_u16(*divident, mask);
	temp = vshrq_n_u16(temp, 2);
	remainder = vorrq_u16(remainder, temp);
	cmp_mask = vcgeq_u16(remainder, *divisor);
	temp = vandq_u16(cmp_mask, *divisor);
	remainder = vsubq_u16(remainder, temp);
	temp = vandq_u16(cmp_mask, mask);
	quotient = vorrq_u16(quotient, temp);
	mask = vshrq_n_u16(mask, 1);

	remainder = vshlq_n_u16(remainder, 1);
	temp = vandq_u16(*divident, mask);
	temp = vshrq_n_u16(temp, 1);
	remainder = vorrq_u16(remainder, temp);
	cmp_mask = vcgeq_u16(remainder, *divisor);
	temp = vandq_u16(cmp_mask, *divisor);
	remainder = vsubq_u16(remainder, temp);
	temp = vandq_u16(cmp_mask, mask);
	quotient = vorrq_u16(quotient, temp);
	mask = vshrq_n_u16(mask, 1);

	remainder = vshlq_n_u16(remainder, 1);
	temp = vandq_u16(*divident, mask);
	remainder = vorrq_u16(remainder, temp);
	cmp_mask = vcgeq_u16(remainder, *divisor);
	temp = vandq_u16(cmp_mask, *divisor);
	remainder = vsubq_u16(remainder, temp);
	temp = vandq_u16(cmp_mask, mask);
	quotient = vorrq_u16(quotient, temp);

	*divident = vshlq_n_u16(quotient, 0);
}

/**
 * Function for converting from regular color values to premultiplied alpha.
 * @param row single RGBA row.
 */
MALI_STATIC_INLINE void _mali_convert_intrinsics_to_premul_alpha(uint8x8x4_t *row)
{
	/* u32 ca = component*alpha; */
	/* return ((ca>>8) + ca + 128)>>8; */
	
	/* red */
	uint16x8_t temp, temp2;
	temp = vmull_u8(row->val[0], row->val[3]);
	temp2 = vshrq_n_u16(temp, 8);
	temp2 = vaddq_u16(temp2, temp);
	temp = vdupq_n_u16(128);
	temp2 = vaddq_u16(temp2, temp);
	temp2 = vshrq_n_u16(temp2, 8);
	row->val[0] = vmovn_u16(temp2);

	/* green */
	temp = vmull_u8(row->val[1], row->val[3]);
	temp2 = vshrq_n_u16(temp, 8);
	temp2 = vaddq_u16(temp2, temp);
	temp = vdupq_n_u16(128);
	temp2 = vaddq_u16(temp2, temp);
	temp2 = vshrq_n_u16(temp2, 8);
	row->val[1] = vmovn_u16(temp2);

	/* blue */
	temp = vmull_u8(row->val[2], row->val[3]);
	temp2 = vshrq_n_u16(temp, 8);
	temp2 = vaddq_u16(temp2, temp);
	temp = vdupq_n_u16(128);
	temp2 = vaddq_u16(temp2, temp);
	temp2 = vshrq_n_u16(temp2, 8);
	row->val[2] = vmovn_u16(temp2);
}

/**
 * Function for converting from premultiplied alpha to regular color values.
 * @param row single RGBA row.
 */
MALI_STATIC_INLINE void _mali_convert_intrinsics_from_premul_alpha(uint8x8x4_t *row)
{
	/* return (alpha ? (component*0xFF + (alpha/2)) / alpha : 0); */
	uint8x8_t mask;
	/* TODO: Remove the alpha_to_zero and temp indirection workarounds.
	 *
	 * The workarounds are done in order to avoid what I can only assume is a variant of bug #43722 in the GCC Bugzilla.
	 * When checking for the ARM errata 754319 and 754320, a patched compiler based on GCC 4.4.6 is used. This produces an
	 * ICE in the following code, similar to the one in bug #43722. The ICE is only reproducable for CONFIG=release.
	 *
	 * The ICE does not manifest for the regular compiler (i.e. arm-none-linux-gnueabi-gcc (Sourcery G++ Lite 2011.03-41) 4.5.2).
	 * As such, the workrounds should be removed upon updating the patched compiler.
	 */
	uint16x8_t alpha_mask, a, b;
	uint16x8_t *alpha_no_zero = &a;
	uint16x8_t *temp = &b;

	/* Clamp the component to its alpha value */
	row->val[0] = vmin_u8(row->val[0], row->val[3]);
	row->val[1] = vmin_u8(row->val[1], row->val[3]);
	row->val[2] = vmin_u8(row->val[2], row->val[3]);

	*alpha_no_zero = vmovl_u8(row->val[3]);
	alpha_mask = vdupq_n_u16(0x00);
	alpha_mask = vceqq_u16(*alpha_no_zero, alpha_mask);
	*alpha_no_zero = vorrq_u16(*alpha_no_zero, alpha_mask);
	alpha_mask = vmvnq_u16(alpha_mask);
	
	/* red */
	mask = vdup_n_u8(0xFF);
	*temp = vmull_u8(row->val[0], mask);
	mask = vshr_n_u8(row->val[3], 1);
	*temp = vaddq_u16(*temp, vmovl_u8(mask));
	_mali_convert_intrinsics_div_uint16x8_t(temp, alpha_no_zero);
	*temp = vandq_u16(*temp, alpha_mask);
	row->val[0] = vmovn_u16(*temp);

	/* green */
	mask = vdup_n_u8(0xFF);
	*temp = vmull_u8(row->val[1], mask);
	mask = vshr_n_u8(row->val[3], 1);
	*temp = vaddq_u16(*temp, vmovl_u8(mask));
	_mali_convert_intrinsics_div_uint16x8_t(temp, alpha_no_zero);
	*temp = vandq_u16(*temp, alpha_mask);
	row->val[1] = vmovn_u16(*temp);

	/* blue */
	mask = vdup_n_u8(0xFF);
	*temp = vmull_u8(row->val[2], mask);
	mask = vshr_n_u8(row->val[3], 1);
	*temp = vaddq_u16(*temp, vmovl_u8(mask));
	_mali_convert_intrinsics_div_uint16x8_t(temp, alpha_no_zero);
	*temp = vandq_u16(*temp, alpha_mask);
	row->val[2] = vmovn_u16(*temp);
}

/* LOAD/STORE FUNCTIONS */

#define ALPHA_TO_ONE_BIT 2048
#define REVERSE_ORDER_BIT 1024
#define RBSWAPPED_BIT 512

/* 8-bit */

void _mali_convert_intrinsics_load_l8(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec)
{
	row->val[0] = vld1_u8(src_ptr);
	/* vmov lacks in intrinsics, use vshl_n of 0 instead, which disassembles into vmov */
	row->val[1] = vshl_n_u8(row->val[0], 0);
	row->val[2] = vshl_n_u8(row->val[0], 0);
	row->val[3] = vdup_n_u8(0xFF);
}

void _mali_convert_intrinsics_store_l8(u8 *dst_ptr, uint8x8x4_t *row, const u16 surfspec)
{
	/* For OpenVG: Conversion in colorspaces done by main function
	 * For OpenGLES: we only need to save the red value */
	vst1_u8(dst_ptr, row->val[0]);
}

void _mali_convert_intrinsics_load_a8(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec)
{
	row->val[3] = vld1_u8(src_ptr);
	/* vmov lacks in intrinsics, use vshl_n of 0 instead, which disassembles into vmov */
	row->val[0] = vdup_n_u8(0x00);
	row->val[1] = vshl_n_u8(row->val[0], 0);
	row->val[2] = vshl_n_u8(row->val[0], 0);
}

void _mali_convert_intrinsics_load_i8(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec)
{
	row->val[3] = vld1_u8(src_ptr);
	/* vmov lacks in intrinsics, use vshl_n of 0 instead, which disassembles into vmov */
	row->val[0] = vdup_n_u8(0xFF);
	row->val[1] = vshl_n_u8(row->val[0], 0);
	row->val[2] = vshl_n_u8(row->val[0], 0);
}

void _mali_convert_intrinsics_store_a8_i8(u8 *dst_ptr, uint8x8x4_t *row, const u16 surfspec)
{
	vst1_u8(dst_ptr, row->val[3]);
}

/* 16-bit */

void _mali_convert_intrinsics_load_rgb565(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec)
{
	/* RGB565 does not support the REVERSE_ORDER flag */
	uint8x8x2_t temp;
	uint8x8_t mask;

	temp = vld2_u8(src_ptr);
	/* temp.val[0] contains [gggbbbbb] and temp.val[1] contains [rrrrrggg], given no flags */

	mask = vdup_n_u8(0xF8);
	if (surfspec & RBSWAPPED_BIT)
	{
		/* Red bits */
		row->val[0] = vshl_n_u8(temp.val[0], 3);
		/* Blue bits */
		row->val[2] = vand_u8(temp.val[1], mask);
	}
	else
	{
		/* Red bits */
		row->val[0] = vand_u8(temp.val[1], mask);
		/* Blue bits */
		row->val[2] = vshl_n_u8(temp.val[0], 3);
	}
	/* Green bits */
	row->val[1] = vshl_n_u8(temp.val[1], 5);
	row->val[1] = vsri_n_u8(row->val[1], temp.val[0], 3);

	/* Replicate the MSB */
	row->val[0] = vsri_n_u8(row->val[0], row->val[0], 5);
	row->val[1] = vsri_n_u8(row->val[1], row->val[1], 6);
	row->val[2] = vsri_n_u8(row->val[2], row->val[2], 5);

	/* Load alpha values */
	row->val[3] = vdup_n_u8(0xFF);
}

void _mali_convert_intrinsics_store_rgb565(u8 *dst_ptr, uint8x8x4_t *row, const u16 surfspec)
{
	/* RGB565 does not support the REVERSE_ORDER flag */
	uint8x8x2_t temp;
	uint8x8_t mask;

	/* temp.val[0] shall contain [gggbbbbb] and temp.val[1] shall contain [rrrrrggg], given no flags */

	/* Green bits */
	temp.val[1] = vshr_n_u8(row->val[1], 5);
	temp.val[0] = vshl_n_u8(row->val[1], 3);

	mask = vdup_n_u8(0xF8);
	if (surfspec & RBSWAPPED_BIT)
	{
		/* Red bits */
		temp.val[0] = vsri_n_u8(temp.val[0], row->val[0], 3);
		/* Blue bits */
		temp.val[1] = vorr_u8(temp.val[1], vand_u8(row->val[2], mask));
	}
	else
	{
		/* Red bits */
		temp.val[1] = vorr_u8(temp.val[1], vand_u8(row->val[0], mask));
		/* Blue bits */
		temp.val[0] = vsri_n_u8(temp.val[0], row->val[2], 3);
	}

	vst2_u8(dst_ptr, temp);
}

void _mali_convert_intrinsics_load_argb1555(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec)
{
	uint8x8x2_t temp;
	uint8x8_t mask;

	temp = vld2_u8(src_ptr);
	/* temp.val[0] contains [gggbbbbb] and temp.val[1] contains [arrrrrgg], given no flags */

	switch (surfspec & (REVERSE_ORDER_BIT|RBSWAPPED_BIT))
	{
		case REVERSE_ORDER_BIT|RBSWAPPED_BIT: /* gBA|Rg */
			/* Red */
			mask = vdup_n_u8(0xF8);
			row->val[0] = vand_u8(temp.val[1], mask);
			/* Blue */
			mask = vdup_n_u8(0x3E);
			row->val[2] = vand_u8(temp.val[0], mask);
			row->val[2] = vshl_n_u8(row->val[2], 2);
			/* Alpha */
			mask = vdup_n_u8(0x01);
			row->val[3] = vand_u8(temp.val[0], mask);
			row->val[3] = vshl_n_u8(row->val[3], 7);
			/* Green */
			row->val[1] = vshl_n_u8(temp.val[1], 5);
			mask = vdup_n_u8(0xC0);
			temp.val[0] = vand_u8(temp.val[0], mask);
			row->val[1] = vsri_n_u8(row->val[1], temp.val[0], 3);
			break;
		case REVERSE_ORDER_BIT: /* gRA|Bg */
			/* Red */
			mask = vdup_n_u8(0x3E);
			row->val[0] = vand_u8(temp.val[0], mask);
			row->val[0] = vshl_n_u8(row->val[0], 2);
			/* Blue */
			mask = vdup_n_u8(0xF8);
			row->val[2] = vand_u8(temp.val[1], mask);
			/* Alpha */
			mask = vdup_n_u8(0x01);
			row->val[3] = vand_u8(temp.val[0], mask);
			row->val[3] = vshl_n_u8(row->val[3], 7);
			/* Green */
			row->val[1] = vshl_n_u8(temp.val[1], 5);
			mask = vdup_n_u8(0xC0);
			temp.val[0] = vand_u8(temp.val[0], mask);
			row->val[1] = vsri_n_u8(row->val[1], temp.val[0], 3);
			break;
		case RBSWAPPED_BIT: /* gR|ABg */
			/* Red */
			mask = vdup_n_u8(0x1F);
			row->val[0] = vand_u8(temp.val[0], mask);
			row->val[0] = vshl_n_u8(row->val[0], 3);
			/* Blue */
			mask = vdup_n_u8(0x7C);
			row->val[2] = vand_u8(temp.val[1], mask);
			row->val[2] = vshl_n_u8(row->val[2], 1);
			/* Alpha */
			mask = vdup_n_u8(0x80);
			row->val[3] = vand_u8(temp.val[1], mask);
			/* Green */
			row->val[1] = vshl_n_u8(temp.val[1], 6);
			mask = vdup_n_u8(0xE0);
			temp.val[0] = vand_u8(temp.val[0], mask);
			row->val[1] = vsri_n_u8(row->val[1], temp.val[0], 2);
			break;
		default: /* gB|ARg */
			/* Red */
			mask = vdup_n_u8(0x7C);
			row->val[0] = vand_u8(temp.val[1], mask);
			row->val[0] = vshl_n_u8(row->val[0], 1);
			/* Blue */
			mask = vdup_n_u8(0x1F);
			row->val[2] = vand_u8(temp.val[0], mask);
			row->val[2] = vshl_n_u8(row->val[2], 3);
			/* Alpha */
			mask = vdup_n_u8(0x80);
			row->val[3] = vand_u8(temp.val[1], mask);
			/* Green */
			row->val[1] = vshl_n_u8(temp.val[1], 6);
			mask = vdup_n_u8(0xE0);
			temp.val[0] = vand_u8(temp.val[0], mask);
			row->val[1] = vsri_n_u8(row->val[1], temp.val[0], 2);
	}

	/* Replicate */
	row->val[0] = vsri_n_u8(row->val[0], row->val[0], 5);
	row->val[1] = vsri_n_u8(row->val[1], row->val[1], 5);
	row->val[2] = vsri_n_u8(row->val[2], row->val[2], 5);
	row->val[3] = vqshl_n_u8(row->val[3], 1);
}


void _mali_convert_intrinsics_store_argb1555(u8 *dst_ptr, uint8x8x4_t *row, const u16 surfspec)
{
	uint8x8x2_t temp;
	uint8x8_t mask;

	/* temp.val[0] shall contain [gggbbbbb] and temp.val[1] shall contain [arrrrrgg], given no flags */

	switch (surfspec & (REVERSE_ORDER_BIT|RBSWAPPED_BIT))
	{
		case REVERSE_ORDER_BIT|RBSWAPPED_BIT: /* gBA|Rg */
			/* Red */
			mask = vdup_n_u8(0xF8);
			temp.val[1] = vand_u8(row->val[0], mask);
			/* Blue */
			temp.val[0] = vshr_n_u8(row->val[2], 2);
			/* Alpha */
			mask = vdup_n_u8(0x7F);
			temp.val[0] = vsri_n_u8(temp.val[0], vcge_u8(row->val[3], mask), 7);
			/* Green */
			temp.val[1] = vsri_n_u8(temp.val[1], row->val[1], 5);
			mask = vdup_n_u8(0xC0);
			temp.val[0] = vorr_u8(temp.val[0], vand_u8(vshl_n_u8(row->val[1], 3), mask));
			break;
		case REVERSE_ORDER_BIT: /* gRA|Bg */
			/* Red */
			temp.val[0] = vshr_n_u8(row->val[0], 2);
			/* Blue */
			mask = vdup_n_u8(0xF8);
			temp.val[1] = vand_u8(row->val[2], mask);
			/* Alpha */
			mask = vdup_n_u8(0x7F);
			temp.val[0] = vsri_n_u8(temp.val[0], vcge_u8(row->val[3], mask), 7);
			/* Green */
			temp.val[1] = vsri_n_u8(temp.val[1], row->val[1], 5);
			mask = vdup_n_u8(0xC0);
			temp.val[0] = vorr_u8(temp.val[0], vand_u8(vshl_n_u8(row->val[1], 3), mask));
			break;
		case RBSWAPPED_BIT: /* gR|ABg */
			/* Red */
			temp.val[0] = vshr_n_u8(row->val[0], 3);
			/* Blue */
			temp.val[1] = vshr_n_u8(row->val[2], 1);
			/* Alpha */
			mask = vdup_n_u8(0x7F);
			temp.val[1] = vsli_n_u8(temp.val[1], vcge_u8(row->val[3], mask), 7);
			/* Green */
			temp.val[1] = vsri_n_u8(temp.val[1], row->val[1], 6);
			mask = vdup_n_u8(0xE0);
			temp.val[0] = vorr_u8(temp.val[0], vand_u8(vshl_n_u8(row->val[1], 2), mask));
			break;
		default: /* gB|ARg */
			/* Red */
			temp.val[1] = vshr_n_u8(row->val[0], 1);
			/* Blue */
			temp.val[0] = vshr_n_u8(row->val[2], 3);
			/* Alpha */
			mask = vdup_n_u8(0x7F);
			temp.val[1] = vsli_n_u8(temp.val[1], vcge_u8(row->val[3], mask), 7);
			/* Green */
			temp.val[1] = vsri_n_u8(temp.val[1], row->val[1], 6);
			mask = vdup_n_u8(0xE0);
			temp.val[0] = vorr_u8(temp.val[0], vand_u8(vshl_n_u8(row->val[1], 2), mask));
	}

	vst2_u8(dst_ptr, temp);
}

void _mali_convert_intrinsics_load_argb4444(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec)
{
	uint8x8x2_t temp;
	uint8x8_t mask;

	temp = vld2_u8(src_ptr);
	/* temp.val[0] contains [ggggbbbb] and temp.val[1] contains [aaaarrrr], given no flags */

	mask = vdup_n_u8(0xF0);
	switch (surfspec & (REVERSE_ORDER_BIT|RBSWAPPED_BIT))
	{
		case REVERSE_ORDER_BIT|RBSWAPPED_BIT: /* BARG */
			row->val[0] = vand_u8(temp.val[1], mask);
			row->val[1] = vshl_n_u8(temp.val[1], 4);
			row->val[2] = vand_u8(temp.val[0], mask);
			row->val[3] = vshl_n_u8(temp.val[0], 4);
			break;
		case REVERSE_ORDER_BIT: /* RABG */
			row->val[0] = vand_u8(temp.val[0], mask);
			row->val[1] = vshl_n_u8(temp.val[1], 4);
			row->val[2] = vand_u8(temp.val[1], mask);
			row->val[3] = vshl_n_u8(temp.val[0], 4);
			break;
		case RBSWAPPED_BIT: /* GRAB */
			row->val[0] = vshl_n_u8(temp.val[0], 4);
			row->val[1] = vand_u8(temp.val[0], mask);
			row->val[2] = vshl_n_u8(temp.val[1], 4);
			row->val[3] = vand_u8(temp.val[1], mask);
			break;
		default: /* GBAR */
			row->val[0] = vshl_n_u8(temp.val[1], 4);
			row->val[1] = vand_u8(temp.val[0], mask);
			row->val[2] = vshl_n_u8(temp.val[0], 4);
			row->val[3] = vand_u8(temp.val[1], mask);
	}

	/* Replicate */
	row->val[0] = vsri_n_u8(row->val[0], row->val[0], 4);
	row->val[1] = vsri_n_u8(row->val[1], row->val[1], 4);
	row->val[2] = vsri_n_u8(row->val[2], row->val[2], 4);
	row->val[3] = vsri_n_u8(row->val[3], row->val[3], 4);
}

void _mali_convert_intrinsics_store_argb4444(u8 *dst_ptr, uint8x8x4_t *row, const u16 surfspec)
{
	uint8x8_t mask;
	uint8x8x2_t temp_texel;
	uint8x8x4_t temp_color;

	/* temp.val[0] shall contain [ggggbbbb] and temp.val[1] shall contain [aaaarrrr], given no flags */

	u32 i;
	uint16x8_t temp16x8;
	uint16x4_t temp16x4_high, temp16x4_low, temp16x4_2;
	uint32x4_t temp32x4_1, temp32x4_2;
	/* For each color channel */
	for (i = 0; i < 4; i++)
	{
		/* ((x * 3855 + 32768) >> 16); this is equal to floor(x/255.f * 15.f + 0.5f) for x in [0..255] */
		temp16x8 = vmovl_u8(row->val[i]);
		temp16x4_high = vget_high_u16(temp16x8);
		temp16x4_low = vget_low_u16(temp16x8);
		temp16x4_2 = vdup_n_u16(3855);
		temp32x4_2 = vdupq_n_u32(32768);
		temp32x4_1 = vmull_u16(temp16x4_high, temp16x4_2);
		temp32x4_1 = vaddq_u32(temp32x4_1, temp32x4_2);
		temp16x4_high = vshrn_n_u32(temp32x4_1, 16);
		temp32x4_1 = vmull_u16(temp16x4_low, temp16x4_2);
		temp32x4_1 = vaddq_u32(temp32x4_1, temp32x4_2);
		temp16x4_low = vshrn_n_u32(temp32x4_1, 16);
		temp16x8 = vcombine_u16(temp16x4_low, temp16x4_high);
		temp_color.val[i] = vmovn_u16(temp16x8);
	}

	mask = vdup_n_u8(0x0F);
	switch (surfspec & (REVERSE_ORDER_BIT|RBSWAPPED_BIT))
	{
		case REVERSE_ORDER_BIT|RBSWAPPED_BIT: /* BARG */
			temp_texel.val[0] = vand_u8(temp_color.val[3], mask);
			temp_texel.val[0] = vsli_n_u8(temp_texel.val[0], temp_color.val[2], 4);
			temp_texel.val[1] = vand_u8(temp_color.val[1], mask);
			temp_texel.val[1] = vsli_n_u8(temp_texel.val[1], temp_color.val[0], 4);
			break;
		case REVERSE_ORDER_BIT: /* RABG */
			temp_texel.val[0] = vand_u8(temp_color.val[3], mask);
			temp_texel.val[0] = vsli_n_u8(temp_texel.val[0], temp_color.val[0], 4);
			temp_texel.val[1] = vand_u8(temp_color.val[1], mask);
			temp_texel.val[1] = vsli_n_u8(temp_texel.val[1], temp_color.val[2], 4);
			break;
		case RBSWAPPED_BIT: /* GRAB */
			temp_texel.val[0] = vand_u8(temp_color.val[0], mask);
			temp_texel.val[0] = vsli_n_u8(temp_texel.val[0], temp_color.val[1], 4);
			temp_texel.val[1] = vand_u8(temp_color.val[2], mask);
			temp_texel.val[1] = vsli_n_u8(temp_texel.val[1], temp_color.val[3], 4);
			break;
		default: /* GBAR */
			temp_texel.val[0] = vand_u8(temp_color.val[2], mask);
			temp_texel.val[0] = vsli_n_u8(temp_texel.val[0], temp_color.val[1], 4);
			temp_texel.val[1] = vand_u8(temp_color.val[0], mask);
			temp_texel.val[1] = vsli_n_u8(temp_texel.val[1], temp_color.val[3], 4);
	}
	
	vst2_u8(dst_ptr, temp_texel);
}

void _mali_convert_intrinsics_load_al88(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec)
{
	/* AL88 does not support the RBSWAPPED flag */
	uint8x8x2_t temp;

	temp = vld2_u8(src_ptr);
	/* temp.val[0] contains L and temp.val[1] contains A given no flags */

	switch (surfspec & REVERSE_ORDER_BIT)
	{
		case REVERSE_ORDER_BIT: /* AL */
			row->val[0] = vshl_n_u8(temp.val[1], 0);
			row->val[3] = vshl_n_u8(temp.val[0], 0);
			break;
		default: /* LA */
			row->val[0] = vshl_n_u8(temp.val[0], 0);
			row->val[3] = vshl_n_u8(temp.val[1], 0);
	}
	row->val[1] = vshl_n_u8(row->val[0], 0);
	row->val[2] = vshl_n_u8(row->val[0], 0);
}

void _mali_convert_intrinsics_store_al88(u8 *dst_ptr, uint8x8x4_t *row, const u16 surfspec)
{
	/* AL88 does not support the RBSWAPPED flag */
	uint8x8x2_t temp;

	/* temp.val[0] shall contain L and temp.val[1] shall contain A, given no flags */

	switch (surfspec & REVERSE_ORDER_BIT)
	{
		case REVERSE_ORDER_BIT: /* AL */
			temp.val[0] = vshl_n_u8(row->val[3], 0);
			temp.val[1] = vshl_n_u8(row->val[0], 0);
			break;
		default: /* LA */
			temp.val[0] = vshl_n_u8(row->val[0], 0);
			temp.val[1] = vshl_n_u8(row->val[3], 0);
	}

	vst2_u8(dst_ptr, temp);
}	

/* 24-bit */

void _mali_convert_intrinsics_load_rgb888(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec)
{
	/* RGB888 does not support the REVERSE_ORDER flag */
	uint8x8x3_t temp;
	temp = vld3_u8(src_ptr);

	/* temp.val[0] contains B, temp.val[1] contains G and temp.val[2] contains R, given no flags */

	switch (surfspec & RBSWAPPED_BIT)
	{
		case RBSWAPPED_BIT: /* RGB */
			break;
		default: /* BGR */
			_mali_convert_intrinsics_swap_uint8x8_t(&temp.val[0], &temp.val[2]);
	}
	
	row->val[0] = temp.val[0];
	row->val[1] = temp.val[1];
	row->val[2] = temp.val[2];
	row->val[3] = vdup_n_u8(0xFF);
}

void _mali_convert_intrinsics_store_rgb888(u8 *dst_ptr, uint8x8x4_t *row, const u16 surfspec)
{
	/* RGB888 does not support the REVERSE_ORDER flag */
	uint8x8x3_t temp;

	/* temp.val[0] shall contain B, temp.val[1] shall contain G and temp.val[2] shall contain R, given no flags */

	temp.val[0] = row->val[0];
	temp.val[1] = row->val[1];
	temp.val[2] = row->val[2];

	switch (surfspec & RBSWAPPED_BIT)
	{
		case RBSWAPPED_BIT: /* RGB */
			break;
		default: /* BGR */
			_mali_convert_intrinsics_swap_uint8x8_t(&temp.val[0], &temp.val[2]);
	}


	vst3_u8(dst_ptr, temp);
}

/* 32-bit */

void _mali_convert_intrinsics_load_argb8888(u8 *src_ptr, uint8x8x4_t *row, const u16 surfspec)
{
	uint8x8x4_t temp = vld4_u8(src_ptr);
	*row = temp;
	/* row->val[0] contains B, row->val[1] contains G, row->val[2] contains R and row->val[3] contains A, given no flags */

	switch (surfspec & (REVERSE_ORDER_BIT|RBSWAPPED_BIT))
	{
		case REVERSE_ORDER_BIT|RBSWAPPED_BIT: /* ABGR */
			_mali_convert_intrinsics_swap_uint8x8_t(&row->val[0], &row->val[3]);
			_mali_convert_intrinsics_swap_uint8x8_t(&row->val[1], &row->val[2]);
			break;
		case REVERSE_ORDER_BIT: /* ARGB */
			_mali_convert_intrinsics_swap_uint8x8_t(&row->val[0], &row->val[1]);
			_mali_convert_intrinsics_swap_uint8x8_t(&row->val[1], &row->val[2]);
			_mali_convert_intrinsics_swap_uint8x8_t(&row->val[2], &row->val[3]);
			break;
		case RBSWAPPED_BIT: /* RGBA */
			break;
		default: /* BGRA */
			_mali_convert_intrinsics_swap_uint8x8_t(&row->val[0], &row->val[2]);
	}
}

void _mali_convert_intrinsics_store_argb8888(u8 *dst_ptr, uint8x8x4_t *row, const u16 surfspec)
{
	uint8x8x4_t temp = *row;
	/* row->val[0] shall contain B, row->val[1] shall contain G, row->val[2] shall contain R and row->val[3] shall contain A, given no flags */
	switch (surfspec & (REVERSE_ORDER_BIT|RBSWAPPED_BIT))
	{
		case REVERSE_ORDER_BIT|RBSWAPPED_BIT: /* ABGR */
			_mali_convert_intrinsics_swap_uint8x8_t(&temp.val[0], &temp.val[3]);
			_mali_convert_intrinsics_swap_uint8x8_t(&temp.val[1], &temp.val[2]);
			break;
		case REVERSE_ORDER_BIT: /* ARGB */
			_mali_convert_intrinsics_swap_uint8x8_t(&temp.val[2], &temp.val[3]);
			_mali_convert_intrinsics_swap_uint8x8_t(&temp.val[1], &temp.val[2]);
			_mali_convert_intrinsics_swap_uint8x8_t(&temp.val[0], &temp.val[1]);
			break;
		case RBSWAPPED_BIT: /* RGBA */
			break;
		default: /* BGRA */
			_mali_convert_intrinsics_swap_uint8x8_t(&temp.val[0], &temp.val[2]);
	}
	vst4_u8(dst_ptr, temp);
}

void _mali_convert_intrinsics_load_8x4(u8 *src_ptr, u32 src_offset, uint8x8x4_t *row0, uint8x8x4_t *row1, uint8x8x4_t *row2, uint8x8x4_t *row3, const u16 src_surfspec)
{
	switch (src_surfspec & 0xFF)
	{
		case 9: /*M200_TEXEL_FORMAT_L_8*/
			_mali_convert_intrinsics_load_l8(src_ptr, row0, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_l8(src_ptr, row1, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_l8(src_ptr, row2, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_l8(src_ptr, row3, src_surfspec);
			src_ptr += src_offset;
			break;
		case 10: /*M200_TEXEL_FORMAT_A_8*/
			_mali_convert_intrinsics_load_a8(src_ptr, row0, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_a8(src_ptr, row1, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_a8(src_ptr, row2, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_a8(src_ptr, row3, src_surfspec);
			src_ptr += src_offset;
			break;
		case 11: /*M200_TEXEL_FORMAT_I_8*/
			_mali_convert_intrinsics_load_i8(src_ptr, row0, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_i8(src_ptr, row1, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_i8(src_ptr, row2, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_i8(src_ptr, row3, src_surfspec);
			src_ptr += src_offset;
			break;
		case 14: /*M200_TEXEL_FORMAT_RGB_565*/
			_mali_convert_intrinsics_load_rgb565(src_ptr, row0, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_rgb565(src_ptr, row1, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_rgb565(src_ptr, row2, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_rgb565(src_ptr, row3, src_surfspec);
			src_ptr += src_offset;
			break;
		case 15: /*M200_TEXEL_FORMAT_ARGB_1555*/
			_mali_convert_intrinsics_load_argb1555(src_ptr, row0, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_argb1555(src_ptr, row1, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_argb1555(src_ptr, row2, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_argb1555(src_ptr, row3, src_surfspec);
			src_ptr += src_offset;
			break;
		case 16: /*M200_TEXEL_FORMAT_ARGB_4444*/
			_mali_convert_intrinsics_load_argb4444(src_ptr, row0, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_argb4444(src_ptr, row1, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_argb4444(src_ptr, row2, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_argb4444(src_ptr, row3, src_surfspec);
			src_ptr += src_offset;
			break;
		case 17: /*M200_TEXEL_FORMAT_AL_88*/
			_mali_convert_intrinsics_load_al88(src_ptr, row0, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_al88(src_ptr, row1, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_al88(src_ptr, row2, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_al88(src_ptr, row3, src_surfspec);
			src_ptr += src_offset;
			break;
		case 21: /*M200_TEXEL_FORMAT_RGB_888*/
		case 67: /*M200_TEXEL_FORMAT_VIRTUAL_RGB888*/
			_mali_convert_intrinsics_load_rgb888(src_ptr, row0, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_rgb888(src_ptr, row1, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_rgb888(src_ptr, row2, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_rgb888(src_ptr, row3, src_surfspec);
			src_ptr += src_offset;
			break;
		case 22: /*M200_TEXEL_FORMAT_ARGB_8888*/
		case 23: /*M200_TEXEL_FORMAT_xRGB_8888*/
			_mali_convert_intrinsics_load_argb8888(src_ptr, row0, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_argb8888(src_ptr, row1, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_argb8888(src_ptr, row2, src_surfspec);
			src_ptr += src_offset;
			_mali_convert_intrinsics_load_argb8888(src_ptr, row3, src_surfspec);
			src_ptr += src_offset;
			break;
	}
}

void _mali_convert_intrinsics_store_8x4(u8 *dst_ptr, u32 dst_offset, uint8x8x4_t *row0, uint8x8x4_t *row1, uint8x8x4_t *row2, uint8x8x4_t *row3, const u16 dst_surfspec)
{
	switch (dst_surfspec & 0xFF)
	{
		case 9: /*M200_TEXEL_FORMAT_L_8*/
		       	_mali_convert_intrinsics_store_l8(dst_ptr, row0, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_l8(dst_ptr, row1, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_l8(dst_ptr, row2, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_l8(dst_ptr, row3, dst_surfspec);
			dst_ptr += dst_offset;
			break;
		case 10: /*M200_TEXEL_FORMAT_A_8*/
		case 11: /*M200_TEXEL_FORMAT_I_8*/
		       	_mali_convert_intrinsics_store_a8_i8(dst_ptr, row0, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_a8_i8(dst_ptr, row1, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_a8_i8(dst_ptr, row2, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_a8_i8(dst_ptr, row3, dst_surfspec);
			dst_ptr += dst_offset;
			break;
		case 14: /*M200_TEXEL_FORMAT_RGB_565*/
		       	_mali_convert_intrinsics_store_rgb565(dst_ptr, row0, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_rgb565(dst_ptr, row1, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_rgb565(dst_ptr, row2, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_rgb565(dst_ptr, row3, dst_surfspec);
			dst_ptr += dst_offset;
			break;
		case 15: /*M200_TEXEL_FORMAT_ARGB_1555*/
		       	_mali_convert_intrinsics_store_argb1555(dst_ptr, row0, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_argb1555(dst_ptr, row1, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_argb1555(dst_ptr, row2, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_argb1555(dst_ptr, row3, dst_surfspec);
			dst_ptr += dst_offset;
			break;
		case 16: /*M200_TEXEL_FORMAT_ARGB_4444*/
		       	_mali_convert_intrinsics_store_argb4444(dst_ptr, row0, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_argb4444(dst_ptr, row1, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_argb4444(dst_ptr, row2, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_argb4444(dst_ptr, row3, dst_surfspec);
			dst_ptr += dst_offset;
			break;
		case 17: /*M200_TEXEL_FORMAT_AL_88*/
		       	_mali_convert_intrinsics_store_al88(dst_ptr, row0, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_al88(dst_ptr, row1, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_al88(dst_ptr, row2, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_al88(dst_ptr, row3, dst_surfspec);
			dst_ptr += dst_offset;
			break;
		case 21: /*M200_TEXEL_FORMAT_RGB_888*/
		case 67: /*M200_TEXEL_FORMAT_VIRTUAL_RGB888*/
		       	_mali_convert_intrinsics_store_rgb888(dst_ptr, row0, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_rgb888(dst_ptr, row1, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_rgb888(dst_ptr, row2, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_rgb888(dst_ptr, row3, dst_surfspec);
			dst_ptr += dst_offset;
			break;
		case 22: /*M200_TEXEL_FORMAT_ARGB_8888*/
		case 23: /*M200_TEXEL_FORMAT_xRGB_8888*/
		       	_mali_convert_intrinsics_store_argb8888(dst_ptr, row0, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_argb8888(dst_ptr, row1, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_argb8888(dst_ptr, row2, dst_surfspec);
			dst_ptr += dst_offset;
		       	_mali_convert_intrinsics_store_argb8888(dst_ptr, row3, dst_surfspec);
			dst_ptr += dst_offset;
			break;
	}
}

/* MAIN FUNCTIONS */

#define MALI_CONVERT_SRC_LINEAR 1
#define MALI_CONVERT_SRC_PREMULT 2
#define MALI_CONVERT_DST_LINEAR 4
#define MALI_CONVERT_DST_PREMULT 8
#define MALI_CONVERT_SRC_LUMINANCE 16
#define MALI_CONVERT_DST_LUMINANCE 32

#define MALI_CONVERT_SOURCE_OPENGLES 0
#define MALI_CONVERT_SOURCE_OPENVG 1
#define MALI_CONVERT_SOURCE_SHARED 2

void _mali_convert_intrinsics_8x8_tile(u8 *src_ptr, u32 src_offset, const u16 src_surfspec, u8 *dst_ptr, u8 *dst_nonpre_ptr, u32 dst_offset, const u16 dst_surfspec, const u8 source, const u32 conv_rules, mali_bool swizzle, mali_bool deswizzle)
{
	u32 i;
	/* For each 8x4 tile */
	for (i = 0; i < 2; i++)
	{
		uint8x8x4_t row0, row1, row2, row3;

		/* Load the registers */
		_mali_convert_intrinsics_load_8x4(src_ptr, src_offset, &row0, &row1, &row2, &row3, src_surfspec);
		src_ptr += 4 * src_offset;
		
		/* Set alpha to one */
		if (src_surfspec & ALPHA_TO_ONE_BIT)
		{
			row0.val[3] = vdup_n_u8(0xFF);
			row1.val[3] = vdup_n_u8(0xFF);
			row2.val[3] = vdup_n_u8(0xFF);
			row3.val[3] = vdup_n_u8(0xFF);
		}

		/* Colorspaces and premultiplied alpha are not defined in the OpenGLES spec */
		if (source == MALI_CONVERT_SOURCE_OPENVG || source == MALI_CONVERT_SOURCE_SHARED)
		{
			/* Convert from premultiplied alpha */
			if (conv_rules & MALI_CONVERT_SRC_PREMULT)
			{
				_mali_convert_intrinsics_from_premul_alpha(&row0);
				_mali_convert_intrinsics_from_premul_alpha(&row1);
				_mali_convert_intrinsics_from_premul_alpha(&row2);
				_mali_convert_intrinsics_from_premul_alpha(&row3);
			}
	
			/* Colorspace conversions */
			if ( 0xF == (dst_surfspec & 0xFF) )
			{
			    row0.val[3] = vcgt_u8(row0.val[3], vdup_n_u8(0x7F));
			    row1.val[3] = vcgt_u8(row1.val[3], vdup_n_u8(0x7F));
			    row2.val[3] = vcgt_u8(row2.val[3], vdup_n_u8(0x7F));
			    row3.val[3] = vcgt_u8(row3.val[3], vdup_n_u8(0x7F));
			}

	
			/* If src format is not luminance, while dst format is: have to do special conversion; See 3.4.2 in OpenVG 1.0.1 spec */
			if (MALI_CONVERT_DST_LUMINANCE == (conv_rules & (MALI_CONVERT_DST_LUMINANCE|MALI_CONVERT_SRC_LUMINANCE)))
			{
				/* TODO: Remove the temp_color and lum indirection workarounds.
				 * @note See description of workarounds in _mali_convert_intrinsics_from_premul_alpha()
				 */
				uint16x8_t a, b, val;
				uint16x8_t *temp_color = &a;
			       	uint16x8_t *lum = &b;
				uint8x8_t temp;
				/* sRGB->lRGB */
				if (!(conv_rules & MALI_CONVERT_SRC_LINEAR))
				{
					_mali_convert_intrinsics_linear_nonlinear(&row0, MALI_FALSE);
					_mali_convert_intrinsics_linear_nonlinear(&row1, MALI_FALSE);
					_mali_convert_intrinsics_linear_nonlinear(&row2, MALI_FALSE);
					_mali_convert_intrinsics_linear_nonlinear(&row3, MALI_FALSE);
				}
				/* lRGB -> lL. See 3.4.2 Color space defininitions in the OpenVG 1.0.1 spec for an explanation of the three constants. */
				/* lum = (r*54 + g*182 + b*19) / 255; */
				temp = vdup_n_u8(54);
				*lum = vmull_u8(row0.val[0], temp);
				temp = vdup_n_u8(182);
				*temp_color = vmull_u8(row0.val[1], temp);
				*lum = vaddq_u16(*lum, *temp_color);
				temp = vdup_n_u8(19);
				*temp_color = vmull_u8(row0.val[2], temp);
				*lum = vaddq_u16(*lum, *temp_color);
				val = vdupq_n_u16(255);
				_mali_convert_intrinsics_div_uint16x8_t(lum, &val);
				row0.val[0] = vmovn_u16(*lum);
				row0.val[1] = vshl_n_u8(row0.val[0], 0);
				row0.val[2] = vshl_n_u8(row0.val[0], 0);
	
				temp = vdup_n_u8(54);
				*lum = vmull_u8(row1.val[0], temp);
				temp = vdup_n_u8(182);
				*temp_color = vmull_u8(row1.val[1], temp);
				*lum = vaddq_u16(*lum, *temp_color);
				temp = vdup_n_u8(19);
				*temp_color = vmull_u8(row1.val[2], temp);
				*lum = vaddq_u16(*lum, *temp_color);
				_mali_convert_intrinsics_div_uint16x8_t(lum, &val);
				row1.val[0] = vmovn_u16(*lum);
				row1.val[1] = vshl_n_u8(row1.val[0], 0);
				row1.val[2] = vshl_n_u8(row1.val[0], 0);
	
				temp = vdup_n_u8(54);
				*lum = vmull_u8(row2.val[0], temp);
				temp = vdup_n_u8(182);
				*temp_color = vmull_u8(row2.val[1], temp);
				*lum = vaddq_u16(*lum, *temp_color);
				temp = vdup_n_u8(19);
				*temp_color = vmull_u8(row2.val[2], temp);
				*lum = vaddq_u16(*lum, *temp_color);
				_mali_convert_intrinsics_div_uint16x8_t(lum, &val);
				row2.val[0] = vmovn_u16(*lum);
				row2.val[1] = vshl_n_u8(row2.val[0], 0);
				row2.val[2] = vshl_n_u8(row2.val[0], 0);
	
				temp = vdup_n_u8(54);
				*lum = vmull_u8(row3.val[0], temp);
				temp = vdup_n_u8(182);
				*temp_color = vmull_u8(row3.val[1], temp);
				*lum = vaddq_u16(*lum, *temp_color);
				temp = vdup_n_u8(19);
				*temp_color = vmull_u8(row3.val[2], temp);
				*lum = vaddq_u16(*lum, *temp_color);
				_mali_convert_intrinsics_div_uint16x8_t(lum, &val);
				row3.val[0] = vmovn_u16(*lum);
				row3.val[1] = vshl_n_u8(row3.val[0], 0);
				row3.val[2] = vshl_n_u8(row3.val[0], 0);
	
				/* lL->sL */
				if (!(conv_rules & MALI_CONVERT_DST_LINEAR))
				{
					_mali_convert_intrinsics_linear_nonlinear(&row0, MALI_TRUE);
					_mali_convert_intrinsics_linear_nonlinear(&row1, MALI_TRUE);
					_mali_convert_intrinsics_linear_nonlinear(&row2, MALI_TRUE);
					_mali_convert_intrinsics_linear_nonlinear(&row3, MALI_TRUE);
				}
			}
			/* lRGB<->sRGB, lL<->sL */
			else
			{
				/* Convert l->s */
		                if (MALI_CONVERT_SRC_LINEAR == (conv_rules & (MALI_CONVERT_SRC_LINEAR | MALI_CONVERT_DST_LINEAR)))
				{
					_mali_convert_intrinsics_linear_nonlinear(&row0, MALI_TRUE);
					_mali_convert_intrinsics_linear_nonlinear(&row1, MALI_TRUE);
					_mali_convert_intrinsics_linear_nonlinear(&row2, MALI_TRUE);
					_mali_convert_intrinsics_linear_nonlinear(&row3, MALI_TRUE);
				}
				/* Convert s->l */
		                else if (MALI_CONVERT_DST_LINEAR == (conv_rules & (MALI_CONVERT_SRC_LINEAR | MALI_CONVERT_DST_LINEAR)))
				{
					_mali_convert_intrinsics_linear_nonlinear(&row0, MALI_FALSE);
					_mali_convert_intrinsics_linear_nonlinear(&row1, MALI_FALSE);
					_mali_convert_intrinsics_linear_nonlinear(&row2, MALI_FALSE);
					_mali_convert_intrinsics_linear_nonlinear(&row3, MALI_FALSE);
				}
			}
		}

		/* Swizzling, linear to blocked layout */
		if (MALI_TRUE == swizzle)
		{
			u8 j;
			/* For each color channel */
			for (j = 0; j < 4; j++)
			{
				uint16x4_t temp1, temp2;
				uint16x4x2_t temp3;
				uint32x2_t temp4, temp5;
				
				row1.val[j] = vrev16_u8(row1.val[j]);
				temp1 = vreinterpret_u16_u8(row0.val[j]);
				temp2 = vreinterpret_u16_u8(row1.val[j]);
				temp3 = vzip_u16(temp1, temp2);
				row0.val[j] = vreinterpret_u8_u16(temp3.val[0]);
				row1.val[j] = vreinterpret_u8_u16(temp3.val[1]);
				
				row3.val[j] = vrev16_u8(row3.val[j]);
				temp1 = vreinterpret_u16_u8(row2.val[j]);
				temp2 = vreinterpret_u16_u8(row3.val[j]);
				temp3 = vzip_u16(temp1, temp2);
				temp4 = vreinterpret_u32_u16(temp3.val[0]);
				temp5 = vreinterpret_u32_u16(temp3.val[1]);
				temp4 = vrev64_u32(temp4);
				temp5 = vrev64_u32(temp5);
				row2.val[j] = vreinterpret_u8_u32(temp4);
				row3.val[j] = vreinterpret_u8_u32(temp5);
				
				_mali_convert_intrinsics_swap_uint8x8_t(&row1.val[j], &row2.val[j]);
				/* If we are on the bottom 8x4 tile */
				if (i == 1)
				{
				        _mali_convert_intrinsics_swap_uint8x8_t(&row0.val[j], &row2.val[j]);
				        _mali_convert_intrinsics_swap_uint8x8_t(&row1.val[j], &row3.val[j]);
				}
			}
		}
		/* De-swizzling, blocked to linear layout; This is exactly the reversed swizzling operation */
		else if (MALI_TRUE == deswizzle)
		{
			u8 j;
			/* For each color channel */
			for (j = 0; j < 4; j++)
			{
				uint32x2_t temp1, temp2;
				uint16x4_t temp3, temp4;
				uint16x4x2_t temp5;

				/* If we are on the bottom 8x4 tile */
				if (i == 1)
				{
					_mali_convert_intrinsics_swap_uint8x8_t(&row0.val[j], &row2.val[j]);
					_mali_convert_intrinsics_swap_uint8x8_t(&row1.val[j], &row3.val[j]);
				}
				_mali_convert_intrinsics_swap_uint8x8_t(&row1.val[j], &row2.val[j]);

				temp1 = vreinterpret_u32_u8(row2.val[j]);
				temp2 = vreinterpret_u32_u8(row3.val[j]);
				temp1 = vrev64_u32(temp1);
				temp2 = vrev64_u32(temp2);
				temp3 = vreinterpret_u16_u32(temp1);
				temp4 = vreinterpret_u16_u32(temp2);
				temp5 = vuzp_u16(temp3, temp4);
				row2.val[j] = vreinterpret_u8_u16(temp5.val[0]);
				row3.val[j] = vreinterpret_u8_u16(temp5.val[1]);
				row3.val[j] = vrev16_u8(row3.val[j]);

				temp3 = vreinterpret_u16_u8(row0.val[j]);
				temp4 = vreinterpret_u16_u8(row1.val[j]);
				temp5 = vuzp_u16(temp3, temp4);
				row0.val[j] = vreinterpret_u8_u16(temp5.val[0]);
				row1.val[j] = vreinterpret_u8_u16(temp5.val[1]);
				row1.val[j] = vrev16_u8(row1.val[j]);
			}
		}

		/* Colorspaces and premultiplied alpha are not defined in the OpenGLES spec */
		if (source == MALI_CONVERT_SOURCE_OPENVG || source == MALI_CONVERT_SOURCE_SHARED)
		{
			/* Store nonpre before conversion back to premult */
			if (dst_nonpre_ptr != NULL)
			{
				if (dst_surfspec & ALPHA_TO_ONE_BIT)
				{
					/* Need to save the alphas before we do alpha_to_one as we need them for premulting */
					uint8x8_t alpha_r0, alpha_r1, alpha_r2, alpha_r3;
					alpha_r0 = vdup_n_u8(0xFF);
					alpha_r1 = vdup_n_u8(0xFF);
					alpha_r2 = vdup_n_u8(0xFF);
					alpha_r3 = vdup_n_u8(0xFF);
					_mali_convert_intrinsics_swap_uint8x8_t(&row0.val[3], &alpha_r0);
					_mali_convert_intrinsics_swap_uint8x8_t(&row1.val[3], &alpha_r1);
					_mali_convert_intrinsics_swap_uint8x8_t(&row2.val[3], &alpha_r2);
					_mali_convert_intrinsics_swap_uint8x8_t(&row3.val[3], &alpha_r3);
	
					_mali_convert_intrinsics_store_8x4(dst_nonpre_ptr, dst_offset, &row0, &row1, &row2, &row3, dst_surfspec);
	
					_mali_convert_intrinsics_swap_uint8x8_t(&row0.val[3], &alpha_r0);
					_mali_convert_intrinsics_swap_uint8x8_t(&row1.val[3], &alpha_r1);
					_mali_convert_intrinsics_swap_uint8x8_t(&row2.val[3], &alpha_r2);
					_mali_convert_intrinsics_swap_uint8x8_t(&row3.val[3], &alpha_r3);
				}
				else
				{
					_mali_convert_intrinsics_store_8x4(dst_nonpre_ptr, dst_offset, &row0, &row1, &row2, &row3, dst_surfspec);
				}
				dst_nonpre_ptr += 4 * dst_offset;
			}
	
			/* Convert to premultiplied alpha */
			if (conv_rules & MALI_CONVERT_DST_PREMULT)
			{
				_mali_convert_intrinsics_to_premul_alpha(&row0);
				_mali_convert_intrinsics_to_premul_alpha(&row1);
				_mali_convert_intrinsics_to_premul_alpha(&row2);
				_mali_convert_intrinsics_to_premul_alpha(&row3);
			}
		}

		if (dst_surfspec & ALPHA_TO_ONE_BIT)
		{
			row0.val[3] = vdup_n_u8(0xFF);
			row1.val[3] = vdup_n_u8(0xFF);
			row2.val[3] = vdup_n_u8(0xFF);
			row3.val[3] = vdup_n_u8(0xFF);
		}

		/* Store the data */
		_mali_convert_intrinsics_store_8x4(dst_ptr, dst_offset, &row0, &row1, &row2, &row3, dst_surfspec);
		dst_ptr += 4 * dst_offset;
	}
}

#undef RBSWAPPED_BIT
#undef REVERSE_ORDER_BIT
#undef ALPHA_TO_ONE_BIT
#undef MALI_CONVERT_SRC_LINEAR
#undef MALI_CONVERT_SRC_PREMULT
#undef MALI_CONVERT_DST_LINEAR
#undef MALI_CONVERT_DST_PREMULT
#undef MALI_CONVERT_SRC_LUMINANCE
#undef MALI_CONVERT_DST_LUMINANCE
#undef MALI_CONVERT_SOURCE_OPENGLES
#undef MALI_CONVERT_SOURCE_OPENVG
#undef MALI_CONVERT_SOURCE_SHARED
