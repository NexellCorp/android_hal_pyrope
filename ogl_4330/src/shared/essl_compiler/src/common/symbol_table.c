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
#include "symbol_table.h"
#include "common/essl_mem.h"
#include "common/essl_common.h"
#include "common/essl_str.h"

memerr _essl_symbol_scope_init(scope *table, mempool *pool)
{
	ESSL_CHECK(table);
	ESSL_CHECK(_essl_dict_init(&table->symbols, pool));

	table->parent = NULL;
	table->pool = pool;

	return MEM_OK;
}

memerr _essl_symbol_scope_insert(scope *table, string key, symbol *binding)
{
	ESSL_CHECK(table);

	return _essl_dict_insert(&table->symbols, key, binding);
}

symbol* _essl_symbol_table_lookup(scope *table, string key)
{
	while (table)
	{
		symbol* value = (symbol*)_essl_dict_lookup(&table->symbols, key);
		if (value)
		{
			return value;
		}
		table = table->parent;
	}
	return NULL;
}

symbol *_essl_symbol_table_lookup_current_scope(scope *table, string key)
{
	if (!table)
	{
		return NULL;
	}

	return _essl_dict_lookup(&table->symbols, key);
}


scope *_essl_symbol_table_begin_scope(scope *table)
{
	scope* newscope = _essl_mempool_alloc(table->pool, sizeof(scope));
	if (!newscope)
	{
		return NULL;
	}
	ESSL_CHECK(_essl_dict_init(&newscope->symbols, table->pool));

	newscope->parent = table;
	newscope->pool = table->pool;

	return newscope;
}

/* This function must never perform operations that could fail, other code relies on it */
scope *_essl_symbol_table_end_scope(scope *table)
{
	if (!table)
	{
		return NULL;
	}

	return table->parent;
}

symbol* _essl_new_symbol(mempool *pool, string name, symbol_kind kind, int source_offset)
{
	symbol *s = _essl_mempool_alloc(pool, sizeof(symbol));
	if (!s) return 0;
	s->name = name;
	s->kind = kind;
	s->address_space = ADDRESS_SPACE_UNKNOWN;
	s->source_offset = source_offset;
	s->address = -1;
	s->opt.external_index = -1;
	s->opt.grad_index = -1;
	s->reg_locations = NULL;
	_essl_init_qualifier_set(&s->qualifier);

	/* Set explicitly for memory checker */
	s->is_invariant = 0;
	s->is_used = 0;
	s->is_persistent_variable = 0;
	s->next = 0;
	s->body = 0;
	s->max_read_bit_precision = 0;
	s->keep_symbol = ESSL_FALSE;
	s->is_used_by_machine_instructions = ESSL_FALSE;
	s->opt.is_indexed_statically = ESSL_FALSE;
	s->opt.pilot.proactive_uniform_num = -1;

	return s;
}

symbol* _essl_new_builtin_function_symbol(mempool *pool, string name, const type_specifier *type, int source_offset)
{
	symbol *s = _essl_new_symbol(pool, name, SYM_KIND_BUILTIN_FUNCTION, source_offset);
	if (!s) return 0;
	s->type = type;
	return s;
}

symbol* _essl_new_builtin_function_name_symbol(mempool *pool, string name, int source_offset)
{
	symbol *s = _essl_new_symbol(pool, name, SYM_KIND_BUILTIN_FUNCTION_NAME, source_offset);
	if (!s) return 0;
	return s;
}

symbol* _essl_new_function_symbol(mempool *pool, string name, const type_specifier *type, qualifier_set return_qualifier, int source_offset)
{
	symbol *s = _essl_new_symbol(pool, name, SYM_KIND_FUNCTION, source_offset);
	if (!s) return 0;
	s->type = type;
	s->qualifier = return_qualifier;
	return s;
}

/*@null@*/ symbol* _essl_new_variable_symbol_with_default_qualifiers(mempool *pool, string name, const type_specifier *type, scope_kind scope, address_space_kind address_space, int source_offset)
{
	symbol *s = _essl_new_symbol(pool, name, SYM_KIND_VARIABLE, source_offset);
	ESSL_CHECK(s);
	s->opt.is_indexed_statically = ESSL_TRUE;
	s->type = type;
	s->scope = scope;
	s->address_space = address_space;
	return s;
}

/*@null@*/ symbol* _essl_new_variable_symbol(mempool *pool, string name, const type_specifier *type, qualifier_set qual, scope_kind scope, address_space_kind address_space, int source_offset)
{
	symbol *s = _essl_new_symbol(pool, name, SYM_KIND_VARIABLE, source_offset);
	ESSL_CHECK(s);
	s->opt.is_indexed_statically = ESSL_TRUE;
	s->type = type;
	s->scope = scope;
	s->address_space = address_space;
	s->qualifier = qual;
	return s;
}

symbol* _essl_new_type_symbol(mempool *pool, string name, const type_specifier *type, int source_offset)
{
	symbol *s = _essl_new_symbol(pool, name, SYM_KIND_TYPE, source_offset);
	if (!s) return 0;
	s->type = type;
	return s;
}

void _essl_symbol_table_iter_init(scope_iter *it, const scope *s)
{
	_essl_dict_iter_init(&it->it, &s->symbols);
}
symbol *_essl_symbol_table_next(scope_iter *it)
{
	void *res = 0;
	string s = _essl_dict_next(&it->it, &res);
	if(!s.ptr) return 0;
	return res;

}


#ifdef UNIT_TEST

/*
unit tests:

manual testing:
cc -Wall -g -DUNIT_TEST -DVERBOSE_TESTING -I.. -o symbol_table symbol_table.c dict.c str.c mem.c && ./symbol_table

Expected output:

Full symbol table:
inner struct (0x615243)
  nested var (0x654321)
    hello world (0x1010)
    abc (0x5000)
    testing 123 (0x3333)

Removing innermost scope:
nested var (0x654321)
  hello world (0x1010)
  abc (0x5000)
  testing 123 (0x3333)

Adding two more scopes + variables:
var a (0xa)
var b (0xb)
var c (0xc)
    nested var (0x654321)
      hello world (0x1010)
      abc (0x5000)
      testing 123 (0x3333)

*/

static void print_scope(scope *scope, int indent, mempool *pool)
{
	string key;
	symbol *value;
	dict_iter it;
	int i;
	_essl_dict_iter_init(&it, &scope->symbols);
	while ((key = _essl_dict_next(&it, (void**)(void *)&value)).ptr)
	{
		for (i = 0; i < indent; ++i)
		{
			printf("  ");
		}
		printf("%s (%p)\n", _essl_string_to_cstring(pool, key), value);
	}
}

static void print_symbol_table(scope *table, mempool *pool)
{
	int level = 0;
	while (table)
	{
		print_scope(table, level++, pool);
		table = table->parent;
	}
}

#define tostr(a, b) _essl_cstring_to_string(b, a)

static void simple_test(mempool *pool, int verbose)
{
	scope global_scope;
	scope *table = &global_scope;
	_essl_symbol_scope_init(table, pool);
	_essl_symbol_scope_insert(table, tostr("hello world", pool), (symbol*)0x1010);
	_essl_symbol_scope_insert(table, tostr("testing 123", pool), (symbol*)0x3333);
	_essl_symbol_scope_insert(table, tostr("abc", pool), (symbol*)0x5000);
	table = _essl_symbol_table_begin_scope(table);
	_essl_symbol_scope_insert(table, tostr("nested var", pool), (symbol*)0x654321);
	table = _essl_symbol_table_begin_scope(table);
	_essl_symbol_scope_insert(table, tostr("inner struct", pool), (symbol*)0x615243);

	assert((symbol*)0x1010 == _essl_symbol_table_lookup(table, tostr("hello world", pool)));
	assert(!_essl_symbol_table_lookup_current_scope(table, tostr("hello world", pool)));
	assert((symbol*)0x654321 == _essl_symbol_table_lookup(table, tostr("nested var", pool)));
	assert(!_essl_symbol_table_lookup_current_scope(table, tostr("nested var", pool)));

	if (verbose)
	{
		printf("Full symbol table:\n");
		print_symbol_table(table, pool);
	}

	table = _essl_symbol_table_end_scope(table);

	if (verbose)
	{
		printf("\nRemoving innermost scope:\n");
		print_symbol_table(table, pool);
	}

	table = _essl_symbol_table_begin_scope(table);
	table = _essl_symbol_table_begin_scope(table);
	_essl_symbol_scope_insert(table, tostr("var c", pool), (symbol*)0xc);
	_essl_symbol_scope_insert(table, tostr("var a", pool), (symbol*)0xa);
	_essl_symbol_scope_insert(table, tostr("var b", pool), (symbol*)0xb);

	assert((symbol*)0xa == _essl_symbol_table_lookup(table, tostr("var a", pool)));
	assert((symbol*)0xa == _essl_symbol_table_lookup_current_scope(table, tostr("var a", pool)));

	if (verbose)
	{
		printf("\nAdding two more scopes + variables:\n");
		print_symbol_table(table, pool);
	}
}

int main(void)
{
	mempool_tracker tracker;
	mempool pool;
	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	_essl_mempool_init(&pool, 0, &tracker);

#ifdef VERBOSE_TESTING
	simple_test(&pool, 1);
#else
	simple_test(&pool, 0);
	printf("All tests OK!\n");
#endif
	_essl_mempool_destroy(&pool);

	return 0;
}

#endif /* UNIT_TEST */
