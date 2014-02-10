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
#include "common/essl_common.h"
#include "common/compiler_options.h"
#include "compiler.h"


compiler_options *_essl_new_compiler_options(mempool *pool)
{
	compiler_options* opts;
	ESSL_CHECK(opts = _essl_mempool_alloc(pool, sizeof(compiler_options)));

	/* Set default values */
#define COMPILER_OPTION(name,field,type,defval) opts->field = defval;
#include "common/compiler_option_definitions.h"
#undef COMPILER_OPTION

	return opts;
}

essl_bool _essl_set_compiler_option_value(compiler_options *opts, compiler_option option, int value)
{
	switch (option) {
#define COMPILER_OPTION(name,field,type,defval) \
	case COMPILER_OPTION_##name: \
		opts->field = (type)value; \
		return ESSL_TRUE;
#include "common/compiler_option_definitions.h"
#undef COMPILER_OPTION
	default:
		return ESSL_FALSE;
	}
}

/**
 * Verify that the given core/revision combination is valid
 * @param hw_rev Hardware version
 * @return ESSL_TRUE if successful, ESSL_FALSE if an illegal/unknown core/revision is requested
 */
essl_bool _essl_validate_hw_rev(unsigned int hw_rev)
{
	switch (HW_REV_CORE(hw_rev))
	{
	case HW_REV_CORE_MALI200:
		if (hw_rev < HW_REV_MALI200_R0P1)
		{
			/* Mali200 r0p0 not supported */
			return ESSL_FALSE;
		}
		return ESSL_TRUE;
	case HW_REV_CORE_MALI400:
		return ESSL_TRUE;
	case HW_REV_CORE_MALI300:
		return ESSL_TRUE;
	case HW_REV_CORE_MALI450:
		return ESSL_TRUE;
	default:
		/* Unknown core */
		return ESSL_FALSE;
	}
}

/**
 * Activate the needed compiler options depending on hardware version
 * @param ctx Compiler context to modify
 * @param hw_rev Hardware version
 */
void _essl_set_compiler_options_for_hw_rev(compiler_options *opts, unsigned int hw_rev)
{
	opts->hw_rev = hw_rev;

	assert(_essl_validate_hw_rev(hw_rev));

	switch (HW_REV_CORE(hw_rev))
	{
	case HW_REV_CORE_MALI200:
		opts->mali200_unsafe_store_error = ESSL_TRUE;

		switch (hw_rev)
		{
		case HW_REV_MALI200_R0P1:
			opts->n_maligp2_instruction_words = 256;
			opts->maligp2_add_workaround = ESSL_TRUE;
			/*@fall-through@*/
		case HW_REV_MALI200_R0P2:
			opts->maligp2_exp2_workaround = ESSL_TRUE;
			opts->mali200_store_workaround = ESSL_TRUE;
			opts->maligp2_constant_store_workaround = ESSL_TRUE;
			opts->mali200_unsafe_store_report = ESSL_TRUE;
			/*@fall-through@*/
		case HW_REV_MALI200_R0P3:
		case HW_REV_MALI200_R0P4:
		case HW_REV_MALI200_R0P5:
			opts->mali200_add_with_scale_overflow_workaround = ESSL_TRUE;
			/*@fall-through@*/
		case HW_REV_MALI200_R0P6:
		default:
			opts->mali200_pointcoord_scalebias = ESSL_TRUE;
			opts->mali200_fragcoord_scale = ESSL_TRUE;
			opts->mali200_derivative_scale = ESSL_TRUE;
			break;
		}
		break;
	case HW_REV_CORE_MALI400:
		switch (hw_rev)
		{
		case HW_REV_MALI400_R0P0:
		case HW_REV_MALI400_R0P1:
		case HW_REV_MALI400_R1P0:
		case HW_REV_MALI400_R1P1:
		default:
			break;
		}
		break;
	case HW_REV_CORE_MALI300:
	switch (hw_rev)
		{
		case HW_REV_MALI300_R0P0:
		default:
			break;
		}
		break;
	case HW_REV_CORE_MALI450:
	switch (hw_rev)
		{
		case HW_REV_MALI450_R0P0:
		default:
			break;
		}
		break;
	}
}
