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
#include "compiler_version.h"
#include "maligp2/maligp2_target.h"

#include "backend/arithmetic.h"
#include "maligp2/maligp2_driver.h"
#include "common/essl_mem.h"
#include "common/essl_str.h"
#include "common/essl_common.h"
#include "maligp2/maligp2_entry_point.h"
#include "common/basic_block.h"
#include "maligp2/maligp2_instruction.h"
#include "maligp2/maligp2_scheduler.h"


static essl_bool is_writeable_constant_register_variable(address_space_kind kind)
{
	switch(kind)
	{
	case ADDRESS_SPACE_THREAD_LOCAL:
	case ADDRESS_SPACE_GLOBAL:
	case ADDRESS_SPACE_REGISTER:
		return ESSL_TRUE;

	case ADDRESS_SPACE_UNIFORM:
	case ADDRESS_SPACE_ATTRIBUTE:
	case ADDRESS_SPACE_VERTEX_VARYING:
	case ADDRESS_SPACE_FRAGMENT_VARYING:
	case ADDRESS_SPACE_FRAGMENT_SPECIAL:
	case ADDRESS_SPACE_FRAGMENT_OUT:
	case ADDRESS_SPACE_UNKNOWN:
		return ESSL_FALSE;
	}
	return ESSL_TRUE;
}

#define CONSTANT_STORE_WORKAROUND_PAD 2
unsigned int _essl_maligp2_get_type_alignment(target_descriptor *desc, const type_specifier *t, address_space_kind kind)
{
	unsigned int alignment = 1;
	if(t->basic_type == TYPE_ARRAY_OF) return _essl_maligp2_get_type_alignment(desc, t->child_type, kind);

	if(t->basic_type == TYPE_STRUCT)
	{
		single_declarator *memb;
		alignment = 4; /* we need 4-float alignment to stop bad effects of load coalescing */
		for(memb = t->members; memb != 0; memb = memb->next)
		{
			unsigned int member_alignment = _essl_maligp2_get_type_alignment(desc, memb->type, kind);
			if(member_alignment > alignment) alignment = member_alignment;
		}
		return alignment;
	}
	if(t->basic_type == TYPE_MATRIX_OF)
	{
		return _essl_maligp2_get_type_alignment(desc, t->child_type, kind);
	} else {
		alignment = GET_VEC_SIZE(t);
	}

	if(alignment == 3) alignment = 4; /* vec3s align as vec4s */

	if(kind == ADDRESS_SPACE_ATTRIBUTE)
	{
		/* don't pack attributes tightly */
		alignment = 4;
	}

	if(desc->options->maligp2_constant_store_workaround)
	{
		if(is_writeable_constant_register_variable(kind))
		{
			/* pad all writeable variables placed in constant registers to the nearest vec2. */
			alignment = (alignment+CONSTANT_STORE_WORKAROUND_PAD-1)& (~(CONSTANT_STORE_WORKAROUND_PAD - 1));
		}
	}


	return alignment;
}

static unsigned int get_type_size_noarray(target_descriptor *desc, const type_specifier *t, address_space_kind kind)
{
	unsigned int elem_size = 0;

	if(t->basic_type == TYPE_STRUCT)
	{
		single_declarator *memb;
		elem_size = 0;
		for (memb = t->members; memb != 0; memb = memb->next)
		{
			unsigned alignment = _essl_maligp2_get_type_alignment(desc, memb->type, kind);
			elem_size = (elem_size + alignment - 1) & ~(alignment - 1);
			elem_size += _essl_maligp2_get_type_size(desc, memb->type, kind);
		}
		

		return elem_size;
	} else if(t->basic_type == TYPE_MATRIX_OF)
	{
		/* rows always take up 4 in space */
		elem_size = 4 * GET_MATRIX_N_COLUMNS(t);
	} else {
		elem_size = GET_VEC_SIZE(t);
	}

	if(desc->options->maligp2_constant_store_workaround)
	{
		if(is_writeable_constant_register_variable(kind))
		{
			/* pad all writeable variables placed in constant registers to the nearest vec2. */
			elem_size = (elem_size+CONSTANT_STORE_WORKAROUND_PAD-1)& (~(CONSTANT_STORE_WORKAROUND_PAD - 1));
		}
	}

	return elem_size;

}


unsigned int _essl_maligp2_get_type_size(target_descriptor *desc, const type_specifier *t, address_space_kind kind)
{
	

	if(t->basic_type == TYPE_ARRAY_OF) 
	{
		return (unsigned)(t->u.array_size * _essl_maligp2_get_array_stride(desc, t->child_type, kind));
	}
	return get_type_size_noarray(desc, t, kind);
}

unsigned int _essl_maligp2_get_address_multiplier(target_descriptor *desc, const type_specifier *t, address_space_kind kind)
{
	IGNORE_PARAM(desc);
	IGNORE_PARAM(t);
	IGNORE_PARAM(kind);
	return 4;
}


unsigned int _essl_maligp2_get_array_stride(target_descriptor *desc, const type_specifier *t, address_space_kind kind)
{
	unsigned alignment = _essl_maligp2_get_type_alignment(desc, t, kind);
	unsigned stride = (_essl_maligp2_get_type_size(desc, t, kind) + alignment-1) & ~(alignment-1);

	stride = ((stride+3)/4)*4;
	return stride; 
	

}

int _essl_maligp2_get_type_member_offset(target_descriptor *desc, const single_declarator *sd, address_space_kind kind)
{
	unsigned int cur_offset = 0;
	single_declarator *memb;
	assert(sd->parent_type != NULL && sd->parent_type->basic_type == TYPE_STRUCT);

	for (memb = sd->parent_type->members; memb != 0; memb = memb->next)
	{
		unsigned alignment = _essl_maligp2_get_type_alignment(desc, memb->type, kind);
		cur_offset = (cur_offset + alignment - 1) & ~(alignment - 1);
		if (memb == sd)
		{
			return (int)cur_offset;
		}
		cur_offset += _essl_maligp2_get_type_size(desc, memb->type, kind);
	}

	return -1;
}

static int cycles_for_jump(jump_taken_enum jump_taken)
{
	if(jump_taken == JUMP_TAKEN) return 5;
	return 0;
}

static int cycles_for_block(basic_block *b)
{
	maligp2_instruction_word *earliest = b->earliest_instruction_word;
	maligp2_instruction_word *latest = b->latest_instruction_word;
	if(earliest == NULL || latest == NULL) return 0;
	return earliest->cycle - latest->cycle + 1;
}

static essl_bool is_variable_in_indexable_memory(struct _tag_symbol *sym)
{
	return sym->address_space != ADDRESS_SPACE_ATTRIBUTE;
}


target_descriptor *_essl_maligp2_new_target_descriptor(mempool *pool, target_kind kind, compiler_options *options)
{
	target_descriptor *desc = _essl_mempool_alloc(pool, sizeof(target_descriptor));
	IGNORE_PARAM(kind);
	if(!desc) return 0;
	assert(kind == TARGET_VERTEX_SHADER);
	desc->name = "maligp2";
	desc->kind = TARGET_VERTEX_SHADER;
	switch (HW_REV_CORE(options->hw_rev))
	{
	case HW_REV_CORE_MALI200:
		desc->core = CORE_MALI_GP2;
		break;
	case HW_REV_CORE_MALI400:
		desc->core = CORE_MALI_400_GP;
		break;
	default:
		assert(0 && "Unknown core");
	}
	desc->options = options;
	desc->has_high_precision = 1;
	desc->fragment_side_has_high_precision = 0;
	desc->has_entry_point = 1;
	desc->has_texturing_support = 0;
	desc->csel_weight_limit = 65;
	desc->blockelim_weight_limit = 10;
	desc->branch_condition_subcycle = 1;
	desc->control_dep_options = CONTROL_DEPS_ACROSS_BASIC_BLOCKS | CONTROL_DEPS_NO_STACK;
	desc->no_store_load_forwarding_optimisation = ESSL_TRUE; /* this does not work well with temporary matrix insertion */
#ifdef ENABLE_PILOT_SHADER_SUPPORT
	desc->enable_proactive_shaders = ESSL_TRUE;
#else
	desc->enable_proactive_shaders = ESSL_FALSE;
#endif
#ifdef MALI_BB_TRANSFORM
	desc->enable_vscpu_calc = ESSL_TRUE;
#else
	desc->enable_vscpu_calc = ESSL_FALSE;
#endif
	desc->constant_fold = _essl_backend_constant_fold;
	desc->constant_fold_sized = _essl_backend_constant_fold_sized;
	desc->float_to_scalar = _essl_backend_float_to_scalar;
	desc->int_to_scalar = _essl_backend_int_to_scalar;
	desc->bool_to_scalar = _essl_backend_bool_to_scalar;
	desc->scalar_to_float = _essl_backend_scalar_to_float;
	desc->scalar_to_int = _essl_backend_scalar_to_int;
	desc->scalar_to_bool = _essl_backend_scalar_to_bool;
	desc->convert_scalar = _essl_backend_convert_scalar;
	desc->driver = _essl_maligp2_driver;
	desc->get_type_alignment = _essl_maligp2_get_type_alignment;
	desc->get_type_size = _essl_maligp2_get_type_size;
	desc->get_type_member_offset = _essl_maligp2_get_type_member_offset;
	desc->get_address_multiplier = _essl_maligp2_get_address_multiplier;
	desc->get_array_stride = _essl_maligp2_get_array_stride;
	desc->get_size_for_type_and_precision = SIZE_FP32;

	desc->insert_entry_point = _essl_maligp2_insert_entry_point;

	desc->cycles_for_jump = cycles_for_jump;
	desc->cycles_for_block = cycles_for_block;

	desc->is_variable_in_indexable_memory = is_variable_in_indexable_memory;

	desc->get_op_weight_scheduler = _essl_maligp2_op_weight_scheduler;
	desc->get_op_weight_realistic = _essl_maligp2_op_weight_realistic;
	desc->serializer_opts = SERIALIZER_OPT_WRITE_PROGRAM_ADDRESSES;
	desc->no_elimination_of_statically_indexed_arrays = ESSL_TRUE;
	return desc;
}




#ifdef UNIT_TEST


int main()
{
	size_t i;
	struct {
		type_basic basic_type;
		int vec_size;
		int array_size;
		address_space_kind address_space;
		unsigned int size;
		unsigned int alignment;
		unsigned int address_multiplier;
		unsigned int array_stride;
	} items[] = 
	  {
		  {TYPE_FLOAT, 1, 0, ADDRESS_SPACE_UNIFORM,  1, 1, 4, 4},
		  {TYPE_FLOAT, 2, 0, ADDRESS_SPACE_UNIFORM,  2, 2, 4, 4},
		  {TYPE_FLOAT, 3, 0, ADDRESS_SPACE_UNIFORM,  3, 4, 4, 4},
		  {TYPE_FLOAT, 4, 0, ADDRESS_SPACE_UNIFORM,  4, 4, 4, 4},
		  {TYPE_FLOAT, 1, 6, ADDRESS_SPACE_UNIFORM,  4*6, 1, 4, 4*6},
		  {TYPE_FLOAT, 2, 6, ADDRESS_SPACE_UNIFORM,  4*6, 2, 4, 4*6},
		  {TYPE_FLOAT, 3, 6, ADDRESS_SPACE_UNIFORM,  4*6, 4, 4, 4*6},
		  {TYPE_FLOAT, 4, 6, ADDRESS_SPACE_UNIFORM,  4*6, 4, 4, 4*6}, 
		  {TYPE_MATRIX_OF, 2, 0, ADDRESS_SPACE_UNIFORM,  8, 2, 4, 8},
		  {TYPE_MATRIX_OF, 3, 0, ADDRESS_SPACE_UNIFORM, 12, 4, 4, 12},
		  {TYPE_MATRIX_OF, 4, 0, ADDRESS_SPACE_UNIFORM, 16, 4, 4, 16},
		  {TYPE_MATRIX_OF, 2, 6, ADDRESS_SPACE_UNIFORM,  8*6, 2, 4, 8*6},
		  {TYPE_MATRIX_OF, 3, 6, ADDRESS_SPACE_UNIFORM, 12*6, 4, 4, 12*6},
		  {TYPE_MATRIX_OF, 4, 6, ADDRESS_SPACE_UNIFORM, 16*6, 4, 4, 16*6}
	  };
	target_descriptor *desc;
	compiler_options *opts;
	mempool_tracker tracker;
	mempool pool;
	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	assert(_essl_mempool_init(&pool, 0, &tracker));
	assert(opts = _essl_new_compiler_options(&pool));
	assert(desc = _essl_maligp2_new_target_descriptor(&pool, TARGET_VERTEX_SHADER, opts));
	for(i = 0; i < sizeof(items)/sizeof(items[0]); ++i)
	{	
		address_space_kind address_space = items[i].address_space;
		type_specifier *t;
		ESSL_CHECK(t = _essl_new_type(&pool));
		t->basic_type = items[i].basic_type;

		if(t->basic_type == TYPE_MATRIX_OF)
		{
			t->basic_type = TYPE_FLOAT;
			t->u.basic.vec_size = items[i].vec_size;
			ESSL_CHECK(t = _essl_new_matrix_of_type(&pool, t, items[i].vec_size));
		} else {
			t->u.basic.vec_size = items[i].vec_size;
		}

		if(items[i].array_size != 0)
		{
			ESSL_CHECK(t = _essl_new_array_of_type(&pool, t, items[i].array_size));
		}
		assert(items[i].size == desc->get_type_size(desc, t, address_space));
		assert(items[i].alignment == desc->get_type_alignment(desc, t, address_space));
		assert(items[i].address_multiplier == desc->get_address_multiplier(desc, t, address_space));
		assert(items[i].array_stride == desc->get_array_stride(desc, t, address_space));

	}
	fprintf(stderr, "All tests OK!\n");
	_essl_mempool_destroy(&pool);
	return 0;
}

#endif
