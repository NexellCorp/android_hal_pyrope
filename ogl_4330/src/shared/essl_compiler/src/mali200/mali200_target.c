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
#include "mali200/mali200_target.h"

#include "backend/arithmetic.h"
#include "mali200/mali200_driver.h"
#include "mali200/mali200_instruction.h"
#include "mali200/mali200_scheduler.h"
#include "common/essl_mem.h"
#include "common/essl_str.h"
#include "common/essl_common.h"
#include "mali200/mali200_entry_point.h"
#include "mali200/mali200_type.h"
#include "compiler_version.h"

static int cycles_for_jump(jump_taken_enum jump_taken)
{
	IGNORE_PARAM(jump_taken);
	return 0;
}

static int cycles_for_block(basic_block *b)
{
	m200_instruction_word *earliest = b->earliest_instruction_word;
	m200_instruction_word *latest = b->latest_instruction_word;
	if(earliest == NULL || latest == NULL) return 0;
	return earliest->cycle - latest->cycle + 1;
}


static essl_bool is_variable_in_indexable_memory(struct _tag_symbol *sym)
{
	IGNORE_PARAM(sym);
	return ESSL_TRUE;
}

target_descriptor *_essl_mali200_new_target_descriptor(mempool *pool, target_kind kind, compiler_options *options)
{
	target_descriptor *desc = _essl_mempool_alloc(pool, sizeof(target_descriptor));
	IGNORE_PARAM(kind);
	if(!desc) return 0;
	assert(kind == TARGET_FRAGMENT_SHADER);
	desc->name = "mali200";
	desc->kind = TARGET_FRAGMENT_SHADER;
	switch (HW_REV_CORE(options->hw_rev))
	{
	case HW_REV_CORE_MALI200:
		desc->core = CORE_MALI_200;
		break;
	case HW_REV_CORE_MALI400:
		desc->core = CORE_MALI_400_PP;
		break;
	default:
		assert(0 && "Unknown core");
	}
	desc->options = options;
	desc->has_high_precision = 0;
	desc->fragment_side_has_high_precision = 0;
	desc->has_entry_point = 1;
	desc->has_texturing_support = 1;
	desc->csel_weight_limit = 7;
	desc->blockelim_weight_limit = 4;
	desc->branch_condition_subcycle = 0;
	desc->control_dep_options = 0;
	desc->expand_builtins_options = EXPAND_BUILTIN_FUNCTIONS_HAS_FAST_MUL_BY_POW2;

	desc->constant_fold = _essl_backend_constant_fold;
	desc->constant_fold_sized = _essl_backend_constant_fold_sized;
	desc->float_to_scalar = _essl_backend_float_to_scalar;
	desc->int_to_scalar = _essl_backend_int_to_scalar;
	desc->bool_to_scalar = _essl_backend_bool_to_scalar;
	desc->scalar_to_float = _essl_backend_scalar_to_float;
	desc->scalar_to_int = _essl_backend_scalar_to_int;
	desc->scalar_to_bool = _essl_backend_scalar_to_bool;
	desc->convert_scalar = _essl_backend_convert_scalar;
	desc->driver = _essl_mali200_driver;
	desc->get_type_alignment = _essl_mali200_get_type_alignment;
	desc->get_type_size = _essl_mali200_get_type_size;
	desc->get_type_member_offset = _essl_mali200_get_type_member_offset;
	desc->get_address_multiplier = _essl_mali200_get_address_multiplier;
	desc->get_array_stride = _essl_mali200_get_array_stride;
	desc->get_size_for_type_and_precision = SIZE_FP16;

	desc->insert_entry_point = _essl_mali200_insert_entry_point;

	desc->cycles_for_jump = cycles_for_jump;
	desc->cycles_for_block = cycles_for_block;

	desc->is_variable_in_indexable_memory = is_variable_in_indexable_memory;

	desc->get_op_weight_scheduler = _essl_mali200_op_weight;
	desc->get_op_weight_realistic = _essl_mali200_op_weight;
	desc->serializer_opts = SERIALIZER_OPT_WRITE_STACK_USAGE;
	desc->no_elimination_of_statically_indexed_arrays = ESSL_FALSE;
#ifdef ENABLE_PILOT_SHADER_SUPPORT
	desc->enable_proactive_shaders = ESSL_TRUE;
#else
	desc->enable_proactive_shaders = ESSL_FALSE;
#endif
	desc->enable_vscpu_calc = ESSL_FALSE;

	return desc;
}
