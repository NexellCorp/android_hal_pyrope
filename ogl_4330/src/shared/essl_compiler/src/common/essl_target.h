/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_TARGET_H
#define COMMON_TARGET_H

#include "common/essl_str.h"

#include "common/essl_node.h"
#include "common/essl_common.h"

#include "common/output_buffer.h"
#include "common/compiler_options.h"

/**
   Language variant of the target
*/

typedef enum {
	TARGET_UNKNOWN, /**< Unknown target kind */
	TARGET_VERTEX_SHADER, /**< Target is a vertex shader backend */
	TARGET_FRAGMENT_SHADER /**< Target is a fragment shader backend */
} target_kind;


/* List of cores - add new entries to the end of this enum */
typedef enum {
        CORE_MALI_GP = 1,
        CORE_MALI_GP2 = 2,
        CORE_MALI_55 = 3,
        CORE_MALI_110 = 4,
        CORE_MALI_200 = 5,
		CORE_MALI_400_GP = 6,
		CORE_MALI_400_PP = 7,
        /* insert new core here, do NOT alter the existing values */
} mali_core;

typedef enum {
	SERIALIZER_OPT_WRITE_STACK_USAGE = 1,
	SERIALIZER_OPT_WRITE_PROGRAM_ADDRESSES = 2,
	SERIALIZER_OPT_WRITE_REGISTER_USAGE = 4,
	SERIALIZER_OPT_WRITE_REGISTER_UNIFORM_MAPPING = 8,
} serializer_options;

typedef struct _tag_target_descriptor target_descriptor;
struct _tag_symbol;
struct _tag_translation_unit;
struct _tag_basic_block;
struct _tag_error_context;


typedef scalar_type(*constant_fold_fun)(const struct _tag_type_specifier *type, enum _tag_expression_operator op, scalar_type a, scalar_type b, scalar_type c);
typedef void(*constant_fold_sized_fun)(enum _tag_expression_operator op, scalar_type *ret, unsigned int size, scalar_type *a, /*@null@*/ scalar_type *b, /*@null@*/ scalar_type *c, const type_specifier *atype, /*@null@*/ const type_specifier *btype);
typedef scalar_type(*float_to_scalar_fun)(float a);
typedef scalar_type(*int_to_scalar_fun)(int a);
typedef scalar_type(*bool_to_scalar_fun)(int a);
typedef float(*scalar_to_float_fun)(scalar_type a);
typedef int(*scalar_to_int_fun)(scalar_type a);
typedef int(*scalar_to_bool_fun)(scalar_type a);
typedef scalar_type(*convert_scalar_fun)(const struct _tag_type_specifier *returntype, scalar_type value);
typedef memerr (*driver_fun)(mempool *pool, struct _tag_error_context *err, typestorage_context *ts_ctx, struct _tag_target_descriptor *desc, struct _tag_translation_unit *tu, output_buffer *out_buf, compiler_options *opts);

typedef unsigned int (*type_alignment_fun)(target_descriptor *desc, const type_specifier *t, address_space_kind kind);
typedef unsigned int (*type_size_fun)(target_descriptor *desc, const type_specifier *t, address_space_kind kind);
typedef unsigned int (*array_stride_fun)(target_descriptor *desc, const type_specifier *t, address_space_kind kind);
typedef unsigned int (*address_multiplier_fun)(target_descriptor *desc, const type_specifier *t, address_space_kind kind);
typedef int (*type_member_offset_fun)(target_descriptor *desc, const single_declarator *sd, address_space_kind kind);

typedef struct _tag_symbol *(*insert_entry_point_fun)(mempool *pool, typestorage_context *ts_ctx, struct _tag_translation_unit *tu, node *root, struct _tag_symbol *main);


typedef enum {
	JUMP_NOT_TAKEN,
	JUMP_TAKEN
} jump_taken_enum;

typedef int (*cycles_for_jump_fun)(jump_taken_enum jump_taken);
typedef int (*cycles_for_block_fun)(struct _tag_basic_block *b);

typedef void (*custom_cycle_count_fun)(mempool *pool, struct _tag_translation_unit *tu);

typedef essl_bool(*is_variable_in_indexable_memory_fun)(struct _tag_symbol *sym);

typedef int (*op_weight_fun)(node *op);


typedef unsigned int control_dependency_options; /* bitfield */
#define CONTROL_DEPS_ACROSS_BASIC_BLOCKS 2
#define CONTROL_DEPS_NO_STACK 4



typedef unsigned int expand_builtin_functions_options; /* bitfield */
#define EXPAND_BUILTIN_FUNCTIONS_HAS_FAST_MUL_BY_POW2 2
/**
   The target descriptor is the mechanism the frontend uses for
   querying the target-specific backend for capabilities and asking
   the backend to perform specific tasks.

   For instance, constant folding must be performed with the precision of the target.
*/



struct _tag_target_descriptor {
	const char *name; /**< For debugging, the name of the backend */
	target_kind kind; /**< Target language variant */
	mali_core core; /**< Target core */
	compiler_options *options; /**< Options for this compilation */
	essl_bool has_high_precision; /**< Whether the target supports high precision variables */
	essl_bool fragment_side_has_high_precision; /**< Whether the fragment side of the target pair supports high precision variables. Used to set the GL_FRAGMENT_PRECISION_HIGH variable */
	essl_bool has_multi_precision; /**< Whether the target supports more than one bit precision */
	int has_entry_point; /**< Whether there is a single entry point to a shader compiled for this target */
	int has_texturing_support; /**< Whether there is texturing support (samplers) for this target */
	int csel_weight_limit; /**< A number to be compared to the total op weight to determine if a csel rewrite will be beneficial */
	int blockelim_weight_limit; /**< A number to be compared to the total op weight to determine if a block elilmination will be beneficial */
	int branch_condition_subcycle; /**< In which subcycle is the condition for a branch read? Used by phi node eliminator. */

	control_dependency_options control_dep_options;
	expand_builtin_functions_options expand_builtins_options;
	essl_bool no_store_load_forwarding_optimisation;

	essl_bool no_elimination_of_statically_indexed_arrays; /* prohibit optimization of statically indexed arrays */
	essl_bool enable_proactive_shaders;	/* allow proactive shader optimization */
	essl_bool enable_vscpu_calc;

	constant_fold_fun constant_fold; /**< Callback for scalar constant folding */
	constant_fold_sized_fun constant_fold_sized; /**< Callback for constant folding that cannot be rewritten as scalar operations */
	float_to_scalar_fun float_to_scalar; /**< Callback for converting a float to a scalar type */
	int_to_scalar_fun int_to_scalar; /**< Callback for converting an integer to a scalar type */
	bool_to_scalar_fun bool_to_scalar; /**< Callback for converting a boolean to a scalar type */
	scalar_to_float_fun scalar_to_float; /**< Callback for converting a scalar type to a float */
	scalar_to_int_fun scalar_to_int; /**< Callback for converting a scalar type to an integer */
	scalar_to_bool_fun scalar_to_bool; /**< Callback for converting a scalar type to a boolean */
	convert_scalar_fun convert_scalar; /**< Callback for scalar type to scalar type conversion. Used for constant folding of built-in constructors */

	driver_fun driver;
	insert_entry_point_fun insert_entry_point;

	/* callbacks for getting target-specific type sizes and alignments */
	type_alignment_fun get_type_alignment;
	type_size_fun get_type_size;
	type_member_offset_fun get_type_member_offset;
	array_stride_fun get_array_stride;
	address_multiplier_fun get_address_multiplier;
	scalar_size_specifier get_size_for_type_and_precision;


	/* callbacks for cycle counting (used by backend/static_cycle_count.c) */
	cycles_for_block_fun cycles_for_block;
	cycles_for_jump_fun cycles_for_jump;
	custom_cycle_count_fun custom_cycle_count;

	/* callback for knowing whether a variable is stored in memory that is directly indexable, or if we need to make a local copy first (used by make_basic_blocks) */
	is_variable_in_indexable_memory_fun is_variable_in_indexable_memory;

	op_weight_fun get_op_weight_scheduler; /**< Used for scheduling */
	op_weight_fun get_op_weight_realistic; /**< Used to determine if a rewrite to csel is beneficial */


	/** Serializer switches */
	serializer_options serializer_opts;

	/** Integer type that can hold a pointer. Used for determining the precision of address arithmetic */
	const type_specifier *pointer_sized_integer;

	/* used for targets that split uniforms into uniform buffers */
	unsigned uniform_buffer_address_stride;
	unsigned max_uniform_buffers;
};


/* create a default target descriptor for the given target */
/*@null@*/ target_descriptor *_essl_new_target_descriptor(mempool *pool, target_kind kind, compiler_options *options);


#endif /* COMMON_TARGET_H */


