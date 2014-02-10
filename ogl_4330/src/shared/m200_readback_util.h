/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _M200_READBACK_UTIL_H_
#define _M200_READBACK_UTIL_H_

#include <shared/m200_gp_frame_builder.h>

/**
 * The main purpose of this file is to offer readback functionality.
 *
 * A readback is basically just reading some texture into the tilelists.
 * This is achieved by rendering a quad. For color readbacks, the
 * quad will contain the texture we want to readback. For depth readbacks,
 * a shader will have to write to the depth buffer with the texture value.
 * For stencil readbacks, we get the PP to write to the stencil value through
 * a shader, again based on the texture value.  This file handles all three
 * types. 
 *
 * The caller must ensure that the source and framebuilder dimensions match.
 * There is no explicit check for this, but the quad function will draw a quad from (0,0) to (src->width, src->height),
 * and if some parts of that quad is outside of the framebuilder resolution, then that part of the image is lost. 
 * If the quad is smaller than the framebuilder, then only parts of the framebuilder outputs will be covered. 
 *
 * Which type of readback happen is specified by toggling bits in the usage enum: 
 *  - MALI_OUTPUT_COLOR will specify a color readback
 *  - MALI_OUTPUT_DEPTH will specify a depth readback
 *  - MALI_OUTPUT_STENCIL will specify a stencil readback.
 *
 * You can specify multiple types of readbacks by toggling multiple bits, but 
 * the bits must match the surface format. Each bit set will result in one 
 * quad being drawn, textured with the relevant data. 
 *
 * In addition, you can specify 
 *  - MALI_OUTPUT_MRT. 
 *    If set, the input src surface will be treated as a MRT surface, assuming 
 *    a databuffer 4x the surface datasize. Each "page" in that databuffer will 
 *    contain information for one subsample. The readback will set up 
 *    four readback quads, each reading the data back to the matching subsample.
 *
 * TODO: Add support for these!
 * And finally, these flags are accepted: 
 *  - MALI_OUTPUT_DOWNSAMPLE_X_2X
 *  - MALI_OUTPUT_DOWNSAMPLE_X_4X
 *  - MALI_OUTPUT_DOWNSAMPLE_X_8X
 *  - MALI_OUTPUT_DOWNSAMPLE_Y_2X
 *  - MALI_OUTPUT_DOWNSAMPLE_Y_4X
 *  - MALI_OUTPUT_DOWNSAMPLE_Y_8X
 *  - MALI_OUTPUT_DOWNSAMPLE_Y_16X
 *  - MALI_OUTPUT_FSAA_4X (same as 2x in each direction)
 *  - MALI_OUTPUT_FSAA_16X (same as 4x in each direction)
 *    Normally, the readback function will write a quad from pixel (0,0) to pixel (src->width, src->height). 
 *    If either of these bits are set, the width and height position will be multiplied by the factors specified
 *    by these enums (texture coordinates will remain as-is). Note that reading back a downsampled texture will result in 
 *    loss of information as compared to reading back a non-downsampled output at full resolution. 
 *
 * Other bits set in the usage flag is ignored. 
 */

/**
 * This function will set up a readback based on one surface, and its usage enum. 
 * The usage will determine what kind of readback(s) happens.
 *
 * @param frame_builder [in] The frame builder to set up a readback on
 * @param src[in] The surface to read from
 * @param usage[in] And enum specifying what kind of readback is supposed to happen. 
 * @param offset_x Framebuilder's offset for x-axis when doing readback.
 * @param offset_y Framebuilder's offset for y-axis when doing readback.
 * @param width Framebuilder's width when doing readback.
 * @param height Framebuilder's height when doing readback. 
 * @return MALI_ERR_NO_ERROR, or MALI_ERR_OUT_OF_MEMORY if OOM.
 */
MALI_IMPORT MALI_CHECK_RESULT mali_err_code _mali_frame_builder_readback(
							   struct mali_frame_builder     *frame_builder,
							   struct mali_surface *src,
							   u32 usage,
							   u16 offset_x,
							   u16 offset_y,
							   u16 width,
							   u16 height );

#endif /* _M200_READBACK_UTIL_H_ */

