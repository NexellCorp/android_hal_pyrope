/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_SYMBOL_TABLE_H
#define COMMON_SYMBOL_TABLE_H

#include "common/essl_dict.h"
#include "common/essl_str.h"
#include "common/essl_node.h"
#include "common/essl_symbol.h"
/**
 * Simple symbol table.
 * Stores one symbol dictionary for each scope.
*/

typedef struct _tag_scope scope; /* need to be this way because of forward declarations in common/node.h */
typedef struct _tag_scope_iter scope_iter;


/**
   Single scope for the symbol table
*/
struct _tag_scope {
	/*@null@*/ scope *parent; /**< pointer to the parent scope, or null if this is the outer-most scope */
	dict symbols; /**< dictionary holding the symbols */
	mempool *pool; /**< memory pool used for the dictionary and new scopes */
};

/**
   Dictionary iterator
*/
struct _tag_scope_iter {
	dict_iter it;
};

/**
   Initialize a symbol table.
*/
memerr _essl_symbol_scope_init(/*@out@*/ scope *table, mempool *pool);


/** Insert a symbol into the given scope */
memerr _essl_symbol_scope_insert(scope *table, string key, symbol *binding);


/** Create a new, generic symbol */
/*@null@*/ symbol* _essl_new_symbol(mempool *pool, string name, symbol_kind kind, int source_offset);

/** Create a built-in function symbol */
/*@null@*/ symbol* _essl_new_builtin_function_symbol(mempool *pool, string name, const type_specifier *type, int source_offset);

/** Create a built-in function name placeholder symbol */
/*@null@*/ symbol* _essl_new_builtin_function_name_symbol(mempool *pool, string name, int source_offset);

/** Create a function symbol */
/*@null@*/ symbol* _essl_new_function_symbol(mempool *pool, string name, const type_specifier *type, qualifier_set return_qualifier, int source_offset);

/** Create a variable symbol */
/*@null@*/ symbol* _essl_new_variable_symbol(mempool *pool, string name, const type_specifier *type, qualifier_set qual,  scope_kind scope, address_space_kind address_space, int source_offset);

/*@null@*/ symbol* _essl_new_variable_symbol_with_default_qualifiers(mempool *pool, string name, const type_specifier *type, scope_kind scope, address_space_kind address_space, int source_offset);

/** Create new user-defined type symbol */
/*@null@*/ symbol* _essl_new_type_symbol(mempool *pool, string name, const type_specifier *type, int source_offset);


/** Look up a symbol in the symbol table recursively */
/*@null@*/ symbol* _essl_symbol_table_lookup(scope *table, string key);

/** Look up a symbol in the symbol table, but fail if it isn't found in the given scope */
/*@null@*/ symbol* _essl_symbol_table_lookup_current_scope(scope *table, string key);

/** Create a new scope and enter it */
/*@null@*/ scope *_essl_symbol_table_begin_scope(scope *table);

/** Leave a scope */
/*@null@*/ scope *_essl_symbol_table_end_scope(scope *table);

/** Create a scope iterater */
void _essl_symbol_table_iter_init(/*@out@*/ scope_iter *it, const scope *s);

/** Go to the next symbol in the scope
	@param it iterator
	@return pointer to symbol if there is another symbol, null if the end of the scope has been reached
*/
/*@null@*/ symbol *_essl_symbol_table_next(scope_iter *it);


#endif /* COMMON_SYMBOL_TABLE_H */
