/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _M200_REGS_H_
#define _M200_REGS_H_

#include <mali_system.h>
#include <base/pp/mali_pp_job.h>
#include <regs/MALI200/mali_render_regs.h>

typedef u32 m200_reg;

/* the total number of registers per writeback unit */
#define WBx_REG_SPACE 0x40

/**
 * Return the address of the attachment for the specified write back unit
 * @param pp_job PP Job Handle
 * @param unit Write Back Unit
 * @return the attachment address
 * @note this function assumes that you get these registers from a pp_job
 */
u32 _m200_wb_attachment_address( mali_pp_job_handle pp_job, u32 unit );

/**
 * Reset a writeback attachment on a pp_job
 * @param pp_job PP Job Handle
 * @param unit Write Back Unit
 * @note this function assumes that you get these registers from a pp_job
 */
void _m200_wb_discard_attachment( mali_pp_job_handle pp_job, u32 unit );

/**
 * Writes a writeback unit register on a pp_job
 * @param wb_unit_regs the writeback registers for one of the writeback units.
 * @param reg the register to write
 * @param value the value to write to @a reg.
 * @note this function assumes that you get these registers from a pp_job
 * @note M200_WBx_REG_TARGET_PIXEL_FORMAT  takes @a mali_pixel_format as value.
 * @note M200_WBx_REG_TARGET_PIXEL_LAYOUT takes @a mali_pixel_layout as value.
 */
void _m200_wb_reg_write( mali_pp_job_handle pp_job, u32 unit, enum m200_wbx_reg_offset reg_offs, u32 value );

/**
 * Writes a frame register
 * @param regs pointer to the frame registers
 * @param reg the register to write
 * @param value the value to write to @a reg.
 * @note this function assumes that you get these registers from a pp_job
 */
void _m200_frame_reg_write_specific( mali_pp_job_handle pp_job_handle, int core_no, enum mali_reg_id reg, u32 value );

/**
 * Writes a frame register
 * @param regs pointer to the frame registers
 * @param reg the register to write
 * @param value the value to write to @a reg.
 * @note this function assumes that you get these registers from a pp_job
 */
void _m200_frame_reg_write_common( mali_pp_job_handle pp_job, enum mali_reg_id reg, u32 value );


#endif /* _M200_RSW_H_ */
