/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "common/essl_target.h"

#include "mali200/mali200_target.h"
#include "maligp2/maligp2_target.h"

target_descriptor *_essl_new_target_descriptor(mempool *pool, target_kind kind, compiler_options *options)
{
	target_descriptor *desc = 0;
	switch(HW_REV_CORE(options->hw_rev))
	{
	case HW_REV_CORE_MALI200:
	case HW_REV_CORE_MALI400:
		switch(kind)
		{
		case TARGET_FRAGMENT_SHADER:
			ESSL_CHECK(desc = _essl_mali200_new_target_descriptor(pool, kind, options));
			break;
		case TARGET_VERTEX_SHADER:
			ESSL_CHECK(desc = _essl_maligp2_new_target_descriptor(pool, kind, options));
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return desc;
}
