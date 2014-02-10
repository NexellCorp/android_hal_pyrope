/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "piecegen_maligp2/piecegen_maligp2_driver.h"

#include "common/essl_mem.h"
#include "common/essl_str.h"
#include "common/essl_common.h"
#include "maligp2/maligp2_target.h"


target_descriptor *_essl_piecegen_maligp2_new_target_descriptor(mempool *pool, target_kind kind)
{
	target_descriptor *desc;
	compiler_options *opts;
	ESSL_CHECK(opts = _essl_new_compiler_options(pool));
	_essl_set_compiler_options_for_hw_rev(opts, HW_REV_GENPIECES);
	ESSL_CHECK(desc = _essl_maligp2_new_target_descriptor(pool, kind, opts));
	desc->name = "maligp2 pieces";

	desc->has_entry_point = 0;
	desc->insert_entry_point = NULL;
	desc->no_store_load_forwarding_optimisation = ESSL_FALSE;
	desc->enable_proactive_shaders = ESSL_FALSE;
	desc->enable_vscpu_calc = ESSL_FALSE;

	desc->driver = _essl_piecegen_maligp2_driver;
	return desc;
}
