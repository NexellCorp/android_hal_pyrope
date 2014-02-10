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
#include "backend/address_allocator.h"
#include "backend/interference_graph.h"
#include "common/essl_str.h"
#include "common/ptrset.h"


/* allocate addresses to symbols according to the alignment needs of these symbols. 
   can and will reorder the symbols 
*/

typedef struct {
	symbol *symbols[4];
} row_symbol_set;

typedef struct {
	unsigned n_rows;
	unsigned n_used_rows;
	unsigned int *rows;
	row_symbol_set *row_symbols;
	unsigned n_allocated_rows;
	mempool *pool;
} row_set;


static memerr row_set_init(row_set *rs, mempool *pool, unsigned n_initial_rows, essl_bool track_row_symbols)
{
	unsigned n_to_alloc = ESSL_MAX(n_initial_rows, 4);
	ESSL_CHECK(rs->rows = _essl_mempool_alloc(pool, n_to_alloc*sizeof(*rs->rows)));
	memset(rs->rows, 0, n_to_alloc*sizeof(*rs->rows));
	rs->n_allocated_rows = n_to_alloc;
	rs->n_rows = n_initial_rows;
	rs->n_used_rows = 0;
	rs->pool = pool;
	rs->row_symbols = NULL;
	if(track_row_symbols)
	{
		ESSL_CHECK(rs->row_symbols = _essl_mempool_alloc(pool, n_to_alloc*sizeof(*rs->row_symbols)));
	}
	return MEM_OK;
}

static memerr row_set_resize(row_set *rs, unsigned n_wanted_rows)
{
	if(n_wanted_rows > rs->n_allocated_rows)
	{
		unsigned int n_to_alloc = ESSL_MAX(rs->n_allocated_rows*2, n_wanted_rows);
		unsigned int *new_rows;
		ESSL_CHECK(new_rows = _essl_mempool_alloc(rs->pool, n_to_alloc*sizeof(*rs->rows)));
		memcpy(new_rows, rs->rows, rs->n_rows*sizeof(*rs->rows));
		rs->rows = new_rows;
		rs->n_allocated_rows = n_to_alloc;

		if(rs->row_symbols != NULL)
		{
			row_symbol_set *new_syms;
			ESSL_CHECK(new_syms = _essl_mempool_alloc(rs->pool, n_to_alloc*sizeof(*rs->row_symbols)));
			memcpy(new_syms, rs->row_symbols, rs->n_rows*sizeof(*rs->row_symbols));
			rs->row_symbols = new_syms;
		}
	}
 
	if(n_wanted_rows > rs->n_rows)
	{
		memset(&rs->rows[rs->n_rows], 0, (n_wanted_rows - rs->n_rows)*sizeof(*rs->rows));
		if(rs->row_symbols != NULL)
		{
			memset(&rs->row_symbols[rs->n_rows], 0, (n_wanted_rows - rs->n_rows)*sizeof(*rs->row_symbols));
		}
	}
	rs->n_rows = n_wanted_rows;
	return MEM_OK;
}

static essl_bool interferes(struct interference_graph_context *ctx, row_symbol_set *set, symbol *sym)
{
	unsigned j;
	assert(ctx != NULL);
	for(j = 0; j < 4; ++j)
	{
		if(set->symbols[j] == NULL) break;
		if(_essl_interference_graph_has_edge(ctx, set->symbols[j], sym))
		{
			return ESSL_TRUE;
		}
	}
	return ESSL_FALSE;
}

static essl_bool row_set_has_space(const row_set *big, const row_set *small, unsigned row_offset, unsigned column_offset, symbol *sym, struct interference_graph_context *interference)
{
	unsigned i;
	assert(row_offset + small->n_rows <= big->n_rows);
	assert(column_offset < 4);
	
	for(i = 0; i < small->n_rows; ++i)
	{
		unsigned int shifted_small_row = (small->rows[i]<<column_offset) & 0xF;
		if(i > 0)
		{
			shifted_small_row |= (small->rows[i-1]>>(4-column_offset));
		}
		assert(shifted_small_row <= 0xF);
		if(shifted_small_row & big->rows[i+row_offset]) return ESSL_FALSE;

		if(interference && interferes(interference, &big->row_symbols[i+row_offset], sym)) return ESSL_FALSE;

	}
	return ESSL_TRUE;
}


static int find_space_in_column(const row_set *rs, unsigned column, unsigned n_rows_needed, symbol *sym, struct interference_graph_context *interference)
{
	unsigned j, i;
	int found_space = -1;
	for(j = 0; j < rs->n_rows - n_rows_needed && found_space == -1; ++j)
	{
		for(i = 0; i < n_rows_needed; ++i)
		{
			if((rs->rows[j+i]>>column)&1)
			{
				found_space = -1;
				break;
			} else if(interference && interferes(interference, &rs->row_symbols[j+i], sym))
			{
				found_space = -1;
				break;
			} else {
				found_space = j;
			}
		}
	}
	return found_space;
}


static unsigned count_space_in_column(const row_set *rs, unsigned column)
{
	unsigned n_spaces = 0;
	unsigned j;
	for(j = 0; j < rs->n_rows; ++j)
	{

		if(((rs->rows[j]>>column)&1) == 0)			
		{
			++n_spaces;
		}
	}
	return n_spaces;
}



static void row_set_place(row_set *big, const row_set *small, unsigned row_offset, unsigned column_offset, symbol *sym)
{
	int i;
	assert(row_offset + small->n_rows <= big->n_rows);
	assert(column_offset < 4);
	
	for(i = 0; i < (int)small->n_rows; ++i)
	{
		unsigned int shifted_small_row = (small->rows[i]<<column_offset)&0xF;
		if(i - 1 >= 0) shifted_small_row |= (small->rows[i-1]>>(4-column_offset))&0xF;
		assert(shifted_small_row < (1<<4));
		assert((shifted_small_row & big->rows[i+row_offset]) == 0);
		big->rows[i+row_offset] |= shifted_small_row;
	}

	if(big->row_symbols != NULL)
	{
		for(i = 0; i < (int)small->n_rows; ++i)
		{
			unsigned j;
			for(j = 0; j < 4; ++j)
			{
				if(big->row_symbols[i+row_offset].symbols[j] == NULL)
				{
					big->row_symbols[i+row_offset].symbols[j] = sym;
					break;
				}
			}
			assert(j < 4);
		}

	}


	if (row_offset + small->n_rows > big->n_used_rows)
	{
		big->n_used_rows = row_offset + small->n_rows;
	}
}

static void row_set_find_dimensions(row_set *rs, /*@out@*/ unsigned *n_rows, /*@out@*/ unsigned *n_cols)
{
	unsigned n_rows_so_far = 0, n_cols_so_far = 0;
	unsigned i, j;
	for(i = 0; i < rs->n_rows; ++i)
	{
		unsigned n_in_this_col = 0;
		for(j = 0; j < 4; ++j)
		{
			if((rs->rows[i]>>j) != 0)
			{
				n_in_this_col = j+1;
			}
		}
		n_cols_so_far = ESSL_MAX(n_cols_so_far, n_in_this_col);
		if(n_in_this_col != 0) n_rows_so_far = i+1;
	}
	if(n_rows != NULL) *n_rows = n_rows_so_far;
	if(n_cols != NULL) *n_cols = n_cols_so_far;
}


static memerr row_set_calc_type_image_helper(row_set *rs, target_descriptor *desc, const type_specifier *t, address_space_kind kind, unsigned offset)
{
	if(t->basic_type == TYPE_ARRAY_OF || t->basic_type == TYPE_MATRIX_OF )
	{
		/* array or matrix */
		unsigned i;
		unsigned array_stride;
		unsigned n_elems;
		type_specifier reduced_type;

		if(t->basic_type == TYPE_ARRAY_OF)
		{
			reduced_type = *t->child_type;
			n_elems = t->u.array_size;
		} else {
			reduced_type = *t;
			reduced_type.basic_type = TYPE_FLOAT;
			reduced_type.u.basic.vec_size = GET_MATRIX_N_ROWS(t);
			n_elems = GET_MATRIX_N_COLUMNS(t);
		}

		array_stride = desc->get_array_stride(desc, &reduced_type, kind);
		for(i = 0; i < n_elems; ++i)
		{
			ESSL_CHECK(row_set_calc_type_image_helper(rs, desc, &reduced_type, kind, i*array_stride+offset));
		}
		return MEM_OK;
	}

	if(t->basic_type == TYPE_STRUCT)
	{
		single_declarator *memb;
		for(memb = t->members; memb != 0; memb = memb->next)
		{
			int member_offset = desc->get_type_member_offset(desc, memb, kind);
			ESSL_CHECK(row_set_calc_type_image_helper(rs, desc, memb->type, kind, offset+member_offset));
			
		}
		return MEM_OK;
	}

	/* okay, only basic vectors left, fill away */
	{
		unsigned i;
		unsigned n_elems = desc->get_type_size(desc, t, kind);
		int n_elems_left = n_elems;
		while(n_elems_left > 0)
		{
			unsigned row_offset = offset / 4;
			unsigned col_offset = offset % 4;
			unsigned n_to_fill = n_elems_left;
			if(n_to_fill > 4 - col_offset) 
			{
				n_to_fill = 4 - col_offset;
			}
			if(row_offset >= rs->n_rows)
			{
				ESSL_CHECK(row_set_resize(rs, row_offset+1));
			}
			for(i = 0; i < n_to_fill; ++i)
			{
				rs->rows[row_offset] |= 1<<(i+col_offset);
			}
			n_elems_left -= n_to_fill;
		}

	}
	return MEM_OK;
}

static memerr row_set_calc_type_image(row_set *rs, target_descriptor *desc, const type_specifier *t, address_space_kind kind)
{
	ESSL_CHECK(row_set_resize(rs, 0));
	return row_set_calc_type_image_helper(rs, desc, t, kind, 0);
}

/*
static void row_set_print(const row_set *rs)
{
	unsigned i, j;
	fprintf(stderr, "Row set of %d rows\n", rs->n_rows);
	for(j = 0; j < rs->n_rows; ++j)
	{
		for(i = 0; i < 4; ++i)
		{
			fprintf(stderr, "%s", ((rs->rows[j]>>i)&1)?".":" ");

		}
		fprintf(stderr, "\n");

	}
	fprintf(stderr, "\n");

}
*/

static int type_score(const type_specifier *t)
{
	switch(t->basic_type)
	{
	case TYPE_STRUCT:
		return 0;
	case TYPE_MATRIX_OF:
		switch(GET_MATRIX_N_COLUMNS(t))
		{
		case 4: return -1;
		case 2: return -2;
		default: return -4;
		}
	case TYPE_FLOAT:
	case TYPE_INT:
	case TYPE_BOOL:
		switch(GET_VEC_SIZE(t))
		{
		case 4: return -3;
		case 3: return -5;
	    case 2: return -6;
		default: return -7;
		}
	case TYPE_SAMPLER_2D:
	case TYPE_SAMPLER_3D:
	case TYPE_SAMPLER_CUBE:
	case TYPE_SAMPLER_2D_SHADOW:
	case TYPE_SAMPLER_EXTERNAL:
		return -7;
	case TYPE_ARRAY_OF:
		return type_score(t->child_type);

	case TYPE_UNRESOLVED_ARRAY_OF:
	case TYPE_VOID:
	case TYPE_UNKNOWN:
		assert(0);
		break;
	}

	return -7;	
}


static int compare( generic_list *va, generic_list *vb)
{
	symbol *a = (symbol*)va;
	symbol *b = (symbol*)vb;
	int score_a, score_b;
	int group_cmp, size_cmp, name_cmp;
	int a_array_size = 0;
	int b_array_size = 0;
	group_cmp = _essl_string_cmp(a->qualifier.group, b->qualifier.group);
	if (group_cmp != 0) return group_cmp;
	score_a = type_score(a->type);
	score_b = type_score(b->type);
	if(score_a != score_b) return score_b - score_a;

	if(a->type->basic_type == TYPE_ARRAY_OF) a_array_size = a->type->u.array_size;
	if(b->type->basic_type == TYPE_ARRAY_OF) b_array_size = b->type->u.array_size;
	if(b_array_size != a_array_size) return b_array_size - a_array_size;

	size_cmp = (int)_essl_get_type_size((b)->type) - (int)_essl_get_type_size((a)->type);
	if (size_cmp != 0) return size_cmp;
	name_cmp = _essl_string_cmp(a->name, b->name);
	return name_cmp;
}


memerr _essl_allocate_addresses(mempool *pool, target_descriptor *desc, int start_address, int max_end_address, symbol *symbols, /*@null@*/ int *actual_end_address, /*@null@*/ struct interference_graph_context *interference)
{

	symbol *sym;
	row_set address_space;
	row_set symbol_pattern;
	string prev_group = { "", 0 };
	int capacity = max_end_address != ADDRESS_ALLOCATOR_NO_MAX_END_ADDRESS ? (max_end_address-start_address+3)/4 : 0;
	if (capacity < 0) capacity = 0;
	assert((start_address & 3) == 0);
	ESSL_CHECK(row_set_init(&address_space, pool, capacity, interference != NULL));
	ESSL_CHECK(row_set_init(&symbol_pattern, pool, 0, ESSL_FALSE));

	for(sym = symbols; sym != NULL; /*empty*/)
	{
		unsigned pattern_n_rows = 0, pattern_n_cols = 0;
		unsigned alignment = 0;
		int address_found = -1;

		if (_essl_string_cmp(sym->qualifier.group, prev_group) != 0)
		{
			/* New group - start new block in allocation */
			start_address = start_address + address_space.n_used_rows*4;
			capacity = max_end_address != ADDRESS_ALLOCATOR_NO_MAX_END_ADDRESS ? (max_end_address-start_address+3)/4 : 0;
			if (capacity < 0) capacity = 0;
			ESSL_CHECK(row_set_init(&address_space, pool, capacity, interference != NULL));
			prev_group = sym->qualifier.group;
		}

		ESSL_CHECK(row_set_calc_type_image(&symbol_pattern, desc, sym->type, sym->address_space));
		row_set_find_dimensions(&symbol_pattern, &pattern_n_rows, &pattern_n_cols);
		alignment = desc->get_type_alignment(desc, sym->type, sym->address_space);
		if(address_space.n_rows < pattern_n_rows)
		{
			ESSL_CHECK(row_set_resize(&address_space, pattern_n_rows));
		}
	retry_place:
		if(pattern_n_cols != 1 || alignment > 1)
		{
			int i, j;
			j = 0;
			for(i = 0; i < (int)(address_space.n_rows - pattern_n_rows + 1); ++i)
			{
				if(row_set_has_space(&address_space, &symbol_pattern, i, j, sym, interference))
				{
					row_set_place(&address_space, &symbol_pattern, i, j, sym);
					address_found = i*4+j;
					break;
				}
			}
			if(alignment == 2 && address_found == -1)
			{
				j = 2;
				for(i = address_space.n_rows - pattern_n_rows; i >= 0; --i)
				{
					if(row_set_has_space(&address_space, &symbol_pattern, i, j, sym, interference))
					{
						row_set_place(&address_space, &symbol_pattern, i, j, sym);
						address_found = i*4+j;
						break;
					}
				}

			}
		} else {
			/* 1-column case */
			int best_column = -1;
			int best_column_row = -1;
			int best_column_spaces = 99999999;
			unsigned i;
			for(i = 0; i < 4; ++i)
			{
				int column_row;
				int n_spaces;
				column_row = find_space_in_column(&address_space, i, pattern_n_rows, sym, interference);
				if(column_row == -1) continue;
				n_spaces = count_space_in_column(&address_space, i);
				if(n_spaces < best_column_spaces)
				{
					best_column_spaces = n_spaces;
					best_column_row = column_row;
					best_column = i;
				}

			}
			if(best_column != -1)
			{
				row_set_place(&address_space, &symbol_pattern, best_column_row, best_column, sym);
				address_found = best_column_row*4+best_column;

			}

		}
		if(address_found != -1)
		{
			sym->address = start_address + address_found;
			assert((sym->address & (alignment-1)) == 0);
			/*
			if (sym->type->stor_qual == QUAL_UNIFORM) printf("Variable '%s' placed at address %d\n", _essl_string_to_cstring(pool, sym->name), sym->address);
			*/
			/*
			fprintf(stderr, "placed %s at %d\n", _essl_string_to_cstring(pool, sym->name), sym->address);
			row_set_print(&address_space);
			*/
			sym = sym->next;
			
		} else {
			ESSL_CHECK(row_set_resize(&address_space, address_space.n_rows+1)); /* try again with more space */
			goto retry_place;
		}

	}
	if(actual_end_address)
	{
		*actual_end_address = start_address + address_space.n_used_rows*4;
	}

	return MEM_OK;
}

memerr _essl_allocate_addresses_for_set(mempool *pool, target_descriptor *desc, int start_address, int max_end_address, ptrset *symbols, /*@null@*/ int *actual_end_address, /*@null@*/ struct interference_graph_context *interference)
{
	symbol *first = NULL;
	symbol **prev = &first;
	symbol *s;
	ptrset_iter it;
	int end_address = -1;
	
	_essl_ptrset_iter_init(&it, symbols);
	while((s = _essl_ptrset_next(&it)) != NULL)
	{
		s->next = NULL;
		*prev = s;
		prev = &s->next;

	}

	/* sort in insertion order */
	first = LIST_SORT(first, compare, symbol);

	/* First try to allocate as tightly as possible */
	ESSL_CHECK(_essl_allocate_addresses(pool, desc, start_address, ADDRESS_ALLOCATOR_NO_MAX_END_ADDRESS, first, &end_address, interference));

	if(max_end_address != ADDRESS_ALLOCATOR_NO_MAX_END_ADDRESS && end_address > max_end_address)
	{
		/* Tight allocation failed. Try to allocate into available space */
		ESSL_CHECK(_essl_allocate_addresses(pool, desc, start_address, max_end_address, first, &end_address, interference));
	}

	/*printf("End address: %d\n", end_address);*/
	if (actual_end_address) *actual_end_address = end_address;
	return MEM_OK;
}

memerr _essl_allocate_addresses_for_optimized_samplers(mempool *pool, target_descriptor *desc, int start_address, ptrset *symbols, /*@null@*/ int *actual_end_address)
{
	symbol *first = NULL;
	symbol **prev = &first;
	symbol *s;
	ptrset_iter it;
	row_set symbol_pattern;
	int end_address = -1;
	int prev_size;
	symbol *sym;
	
	_essl_ptrset_iter_init(&it, symbols);
	while((s = _essl_ptrset_next(&it)) != NULL)
	{
		s->next = NULL;
		*prev = s;
		prev = &s->next;

	}

	/* sort in insertion order */
	first = LIST_SORT(first, compare, symbol);

	/* just a sequential allocation as we don't need any alignment for optimized samplers */

	ESSL_CHECK(row_set_init(&symbol_pattern, pool, 0, ESSL_FALSE));

	end_address = start_address;
	prev_size = 0;
	for(sym = first; sym != NULL; /*empty*/)
	{
		unsigned pattern_n_rows = 0, pattern_n_cols = 0;
		ESSL_CHECK(row_set_calc_type_image(&symbol_pattern, desc, sym->type, sym->address_space));
		row_set_find_dimensions(&symbol_pattern, &pattern_n_rows, &pattern_n_cols);
		sym->address = end_address + prev_size;
		prev_size = (pattern_n_rows - 1)*4 + pattern_n_cols;
		end_address = sym->address;
		sym = sym->next;
	}
	end_address += prev_size;

	if(actual_end_address)
	{
		*actual_end_address = end_address;
	}

	return MEM_OK;
}


#ifdef UNIT_TEST
#include "maligp2/maligp2_target.h"
#include "common/symbol_table.h"
int main()
{
	size_t i;
    int actual_end_address;
	struct {
		const char *name;
		type_basic basic_type;
		int vec_size;
		int array_size;
		int address;
	} items[] = 
	  {
		  {"h", TYPE_FLOAT, 1, 0, 4*4+3},
		  {"g", TYPE_FLOAT, 1, 2, 4*4+2},
		  {"f", TYPE_FLOAT, 1, 3, 1*4+3},
		  {"e", TYPE_FLOAT, 2, 0, 7*4+0},
		  {"d", TYPE_FLOAT, 2, 2, 6*4+2},
		  {"c", TYPE_FLOAT, 2, 3, 4*4+0},
		  {"b", TYPE_MATRIX_OF,3, 0, 1*4+0},
		  {"a", TYPE_FLOAT, 4, 0, 0*4+0}
	  };
	const address_space_kind address_space = ADDRESS_SPACE_UNIFORM;
	target_descriptor *desc;
	compiler_options *opts;
	mempool_tracker tracker;
	mempool pool;
	symbol *first = NULL;
	symbol **curr = &first;
	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	assert(_essl_mempool_init(&pool, 0, &tracker));
	assert(opts = _essl_new_compiler_options(&pool));
	assert(desc = _essl_maligp2_new_target_descriptor(&pool, TARGET_VERTEX_SHADER, opts));
	for(i = 0; i < sizeof(items)/sizeof(items[0]); ++i)
	{	
		symbol *sym;

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
		ESSL_CHECK(sym = _essl_new_variable_symbol_with_default_qualifiers(&pool, _essl_cstring_to_string_nocopy(items[i].name), t, SCOPE_GLOBAL, address_space, UNKNOWN_SOURCE_OFFSET));
		*curr = sym;
		curr = &(*curr)->next;

	}
	{
		symbol *sym;
		memerr ret;
		first = LIST_SORT(first, compare, symbol);
		ret = _essl_allocate_addresses(&pool, desc, 0, 8*4, first, &actual_end_address, NULL);
		assert(ret);
		assert(actual_end_address == 8*4);
		for(sym = first; sym != NULL; sym = sym->next)
		{
			for(i = 0; i < sizeof(items)/sizeof(items[0]); ++i)
			{	
				if(!_essl_string_cmp(sym->name, _essl_cstring_to_string_nocopy(items[i].name)))
				{
					assert(sym->address == items[i].address);
					break;
				}
			}
			assert(i != sizeof(items)/sizeof(items[0]));
		}
	}
	fprintf(stderr, "All tests OK!\n");
	_essl_mempool_destroy(&pool);
	return 0;
}





#endif
