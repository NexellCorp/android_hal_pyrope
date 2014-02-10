/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef COMMON_TRANSLATION_UNIT_H
#define COMMON_TRANSLATION_UNIT_H

#include "common/essl_list.h"
#include "frontend/lang.h"
#include "common/essl_symbol.h"
#include "common/essl_node.h"

struct output_buffer;

typedef struct
{
	unsigned int stack_size;
	unsigned int initial_stack_pointer;
} stack_flag_set;

typedef struct
{
	int program_start_address;
	int one_past_program_end_address; /**< one past the address of the last instruction of the program */
	int one_past_last_input_read_address;

} program_addresses;


typedef struct
{
	essl_bool color_read;
	essl_bool color_write;
	essl_bool depth_read;
	essl_bool depth_write;
	essl_bool stencil_read;
	essl_bool stencil_write;

} buffer_usage_flags;

typedef struct {
	unsigned n_work_registers;
	unsigned n_uniform_registers;
 } register_usage;


typedef struct _tag_translation_unit
{
	/*@null@*/ symbol_list *uniforms;
	/*@null@*/ symbol_list *vertex_attributes;
	/*@null@*/ symbol_list *vertex_varyings;
	/*@null@*/ symbol_list *fragment_varyings;
	/*@null@*/ symbol_list *fragment_specials;
	/*@null@*/ symbol_list *fragment_outputs;
	/*@null@*/ symbol_list *globals; /**< global variables */
	/*@null@*/ symbol_list *cpu_processed;


	symbol_list *functions; /**< all functions reachable from the entry point, as a topological sort of the call graph */
	symbol *entry_point; /**< entry point function */
	symbol_list *proactive_funcs;

	/* legacy */
	node *root;

	/* possible additions */
	language_descriptor *lang_desc;
	target_descriptor *desc;

	stack_flag_set stack_flags;
	essl_bool discard_used;
	buffer_usage_flags buffer_usage;
	program_addresses program_address;
	register_usage reg_usage;
	struct output_buffer *code_section;
	unsigned code_section_start;
	unsigned proactive_uniform_init_addr; /* initial address of proactive uniforms */


} translation_unit;


#endif /* COMMON_TRANSLATION_UNIT_H */
