/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_SYMBOL_H
#define COMMON_SYMBOL_H

#include "common/essl_str.h"
#include "common/essl_type.h"
#include "common/essl_node.h"
#include "common/essl_list.h"

typedef struct _tag_symbol symbol;
struct _tag_control_flow_graph;
struct call_graph;
struct relocation;


/** Enumeration of the possible symbols types
*/
typedef enum {
	SYM_KIND_UNKNOWN,              /**< Unknown kind */
	SYM_KIND_VARIABLE,             /**< Variable symbol */
	SYM_KIND_FUNCTION,             /**< Function symbol */
	SYM_KIND_BUILTIN_FUNCTION,     /**< Built-in function symbol */
	SYM_KIND_BUILTIN_FUNCTION_NAME,/**< Placeholder for all built-in functions with the same name */
	SYM_KIND_TYPE,                 /**< User-defined type (struct) symbol */
	SYM_KIND_PRECISION             /**< default precision specifier */
} symbol_kind;

typedef struct register_location {
	struct register_location *next;
	int reg_idx;
	scalar_size_specifier elem_size;
	unsigned start_offset;
	unsigned n_elems;
	signed char indices[1]; /* flexible array */
} register_location;

ASSUME_ALIASING(register_location, generic_list);

/** Symbol type representing possible defined names
*/
struct _tag_symbol {
	/*@null@*/ struct _tag_symbol *next; /**< For function symbols with overloads, this
										points to next function symbol with identical name */
	unsigned kind:4;            /**< Kind of symbol */
	unsigned scope:5;      /**< When the symbol is a variable: the scope of the variable */
	unsigned address_space:5;      /**< When the symbol is a variable: kind of variable */
	unsigned is_invariant:1;       /**< When the symbol is a variable,
									  whether the variable must be calculated invariantly */
	unsigned is_used:1;            /**< Used for flagging invariant errors when
									  a symbol is used before it is declared invariant */
	unsigned is_persistent_variable:1; /**< Marks a global variable as persistent, i.e. preserved across vertex shader instances. Used by the ARM_persistent_globals extension */
	string name;                   /**< Name of the symbol */
	const type_specifier *type;    /**< type of the symbol */
	struct _tag_qualifier_set qualifier; /**< The qualifiers of the symbol */
	/*@null@*/ node *body;         /**< used for values of const variables
									  and function bodies */
	int source_offset;                 /**< byte offset into concatenated source strings. For functions, this is the offset of the first prototype, not the definition */

	expression_operator builtin_function; /**< When the symbol is a built-in function,
											 this stores the opcode */
	/*@null@*/ single_declarator *parameters; /**< Formal parameters for function symbols */

	/*@null@*/ struct _tag_control_flow_graph *control_flow_graph;
	                               /**< For function symbols: the control flow graph */
	/*@null@*/ struct call_graph* calls_from;   /**< to this from those listed */
	/*@null@*/ struct call_graph* calls_to;     /**< from this to those listed, directly */
	/*@null@*/ struct call_graph* calls_to_via; /**< from this, to listed, via any */
	int call_count;                /**< Number of places this function is called */
	unsigned max_read_bit_precision; /**< Maximum number of bits precision utilized when reading this variable */


	int address;                   /**< target-specific address of this symbol */
	register_location *reg_locations;
	struct relocation *relocations;
	struct
	{
		essl_bool is_indexed_statically;	/**< necessary only  for arrays to check if they are indexed only in static way */
		int external_index; 		/**<expected index in the external coefficients table */
		int grad_index; 			/**<expected index in the Grad coefficients (texture width, texture height) table */

		struct
		{
			essl_bool is_proactive_func;
			int proactive_uniform_num;
			int one_past_program_end_address;
		}pilot;

	}opt;

	unsigned is_used_by_machine_instructions:1; /**< Used by backend to know whether a symbol is referred to by a machine instruction and need an address or not */
	unsigned keep_symbol:1; /**< Mark a symbol so that it is not eliminated, even if it is not referenced by a machine instruction. */

}; /* symbol */
ASSUME_ALIASING(symbol, generic_list);


typedef struct _tag_symbol_list
{
	struct _tag_symbol_list *next;
	symbol *sym;
} symbol_list;

ASSUME_ALIASING(generic_list, symbol_list);




#endif /* COMMON_SYMBOL_H */
