/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "piecegen_mali200/piecegen_mali200_driver.h"

#include "common/essl_mem.h"
#include "common/essl_str.h"
#include "common/essl_common.h"
#include "mali200/mali200_target.h"


target_descriptor *_essl_piecegen_mali200_new_target_descriptor(mempool *pool, target_kind kind)
{
	target_descriptor *desc;
	compiler_options *opts;
	ESSL_CHECK(opts = _essl_new_compiler_options(pool));
	_essl_set_compiler_options_for_hw_rev(opts, HW_REV_MALI200_R0P5);
	opts->mali200_fragcoord_scale = ESSL_FALSE;
	ESSL_CHECK(desc = _essl_mali200_new_target_descriptor(pool, kind, opts));
	desc->name = "mali200 pieces";

	desc->has_entry_point = 0;
	desc->insert_entry_point = NULL;

	desc->driver = _essl_piecegen_mali200_driver;
	return desc;
}
