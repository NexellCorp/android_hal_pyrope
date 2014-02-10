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
#include "common/essl_common.h"
#include "common/essl_type.h"
#include "common/essl_target.h"

type_specifier *_essl_new_type(mempool *pool)
{
	type_specifier *ts;
	ESSL_CHECK_NONFATAL(ts = _essl_mempool_alloc(pool, sizeof(*ts)));
	return ts;
}

void _essl_init_qualifier_set(qualifier_set *tq)
{
	tq->variable = VAR_QUAL_NONE;
	tq->varying = VARYING_QUAL_NONE;
	tq->precision = PREC_UNKNOWN;
	tq->direction = DIR_NONE;

	tq->group.ptr = "";
	tq->group.len = 0;
}


/*@null@*/ const type_specifier *_essl_new_basic_type(mempool *pool, type_basic type, unsigned int vec_size, scalar_size_specifier sz, integer_signedness_specifier int_signedness)
{
	type_specifier *ts = _essl_new_type(pool);
	ESSL_CHECK_NONFATAL(ts);
	ts->basic_type = type;
	ts->u.basic.vec_size = vec_size;
	ts->u.basic.scalar_size = sz;
	assert(type == TYPE_INT || int_signedness == INT_SIGNED);
	ts->u.basic.int_signedness = int_signedness;

	return ts;
}

memerr _essl_typestorage_init(typestorage_context *ctx, mempool *pool)
{
	unsigned int i;
	ctx->pool = pool;
	for (i = 0; i < 4; ++i)
	{
		unsigned vec_size = i + 1;
		ESSL_CHECK_NONFATAL(ctx->sint_types_8[i] = _essl_new_basic_type(ctx->pool, TYPE_INT, vec_size, SIZE_BITS8, INT_SIGNED));
		ESSL_CHECK_NONFATAL(ctx->uint_types_8[i] = _essl_new_basic_type(ctx->pool, TYPE_INT, vec_size, SIZE_BITS8, INT_UNSIGNED));
		ESSL_CHECK_NONFATAL(ctx->bool_types_8[i] = _essl_new_basic_type(ctx->pool, TYPE_BOOL, vec_size, SIZE_BITS8, INT_SIGNED));

		ESSL_CHECK_NONFATAL(ctx->sint_types_16[i] = _essl_new_basic_type(ctx->pool, TYPE_INT, vec_size, SIZE_BITS16, INT_SIGNED));
		ESSL_CHECK_NONFATAL(ctx->uint_types_16[i] = _essl_new_basic_type(ctx->pool, TYPE_INT, vec_size, SIZE_BITS16, INT_UNSIGNED));
		ESSL_CHECK_NONFATAL(ctx->bool_types_16[i] = _essl_new_basic_type(ctx->pool, TYPE_BOOL, vec_size, SIZE_BITS16, INT_SIGNED));
		ESSL_CHECK_NONFATAL(ctx->float_types_16[i] = _essl_new_basic_type(ctx->pool, TYPE_FLOAT, vec_size, SIZE_BITS16, INT_SIGNED));

		ESSL_CHECK_NONFATAL(ctx->sint_types_32[i] = _essl_new_basic_type(ctx->pool, TYPE_INT, vec_size, SIZE_BITS32, INT_SIGNED));
		ESSL_CHECK_NONFATAL(ctx->uint_types_32[i] = _essl_new_basic_type(ctx->pool, TYPE_INT, vec_size, SIZE_BITS32, INT_UNSIGNED));
		ESSL_CHECK_NONFATAL(ctx->bool_types_32[i] = _essl_new_basic_type(ctx->pool, TYPE_BOOL, vec_size, SIZE_BITS32, INT_SIGNED));
		ESSL_CHECK_NONFATAL(ctx->float_types_32[i] = _essl_new_basic_type(ctx->pool, TYPE_FLOAT, vec_size, SIZE_BITS32, INT_SIGNED));

		ESSL_CHECK_NONFATAL(ctx->sint_types_64[i] = _essl_new_basic_type(ctx->pool, TYPE_INT, vec_size, SIZE_BITS64, INT_SIGNED));
		ESSL_CHECK_NONFATAL(ctx->uint_types_64[i] = _essl_new_basic_type(ctx->pool, TYPE_INT, vec_size, SIZE_BITS64, INT_UNSIGNED));
		ESSL_CHECK_NONFATAL(ctx->float_types_64[i] = _essl_new_basic_type(ctx->pool, TYPE_FLOAT, vec_size, SIZE_BITS64, INT_SIGNED));
	}
	return MEM_OK;
}

const type_specifier *_essl_get_type_with_size_and_signedness(const typestorage_context *ctx, const type_basic type, unsigned int vec_size, scalar_size_specifier scalar_size, integer_signedness_specifier int_signedness)
{
	unsigned idx = vec_size - 1;
	if (type == TYPE_FLOAT && vec_size >= 1 && vec_size <= 4)
	{	
		switch(scalar_size)
		{
			case SIZE_FP16:
				return ctx->float_types_16[idx];
			case SIZE_FP32:
				return ctx->float_types_32[idx];
			case SIZE_FP64:
				return ctx->float_types_64[idx];
			default:
				break;
		}
	}
	else if (type == TYPE_INT && vec_size >= 1 && vec_size <= 4)
	{
		if(int_signedness == INT_UNSIGNED)
		{
			switch(scalar_size)
			{
			case SIZE_INT8:
				return ctx->uint_types_8[idx];
			case SIZE_INT16:
				return ctx->uint_types_16[idx];
			case SIZE_INT32:
				return ctx->uint_types_32[idx];
			case SIZE_INT64:
				return ctx->uint_types_64[idx];
			default:
				break;
			}
		} else {
			switch(scalar_size)
			{
			case SIZE_INT8:
				return ctx->sint_types_8[idx];
			case SIZE_INT16:
				return ctx->sint_types_16[idx];
			case SIZE_INT32:
				return ctx->sint_types_32[idx];
			case SIZE_INT64:
				return ctx->sint_types_64[idx];
			default:
				break;
			}
		}
	}
	else if (type == TYPE_BOOL && vec_size >= 1 && vec_size <= 4)
	{
		switch(scalar_size)
		{
			case SIZE_BITS8:
				return ctx->bool_types_8[idx];
			case SIZE_BITS16:
				return ctx->bool_types_16[idx];
			case SIZE_BITS32:
				return ctx->bool_types_32[idx];
			default:
				break;
		}
	}


	/* default if no cache */
	assert(type != TYPE_STRUCT); /* a const type_specifier of struct with no struct-specific fields filled out is not very useful */
	return _essl_new_basic_type(ctx->pool, type, vec_size, scalar_size, INT_SIGNED);
}

const type_specifier *_essl_get_type_with_size(const typestorage_context *ctx, const type_basic type, unsigned int vec_size, scalar_size_specifier scalar_size)
{
	return _essl_get_type_with_size_and_signedness(ctx, type, vec_size, scalar_size, INT_SIGNED);
}

const type_specifier *_essl_get_type(const typestorage_context *ctx, type_basic type, unsigned int vec_size)
{
	const type_specifier *t = _essl_get_type_with_size(ctx, type, vec_size, SIZE_BITS32);
	return t;
}

const type_specifier *_essl_get_type_with_default_size_for_target(const typestorage_context *ctx, const type_basic type, unsigned int vec_size, struct _tag_target_descriptor *desc)
{
	const type_specifier *t;
	scalar_size_specifier sz;
	ESSL_CHECK(t = _essl_get_type(ctx, type, vec_size));
	sz = desc->get_size_for_type_and_precision;
	
	return _essl_get_type_with_given_size(ctx, t, sz);
}

single_declarator *_essl_new_single_declarator(mempool *pool, const type_specifier *type, qualifier_set qualifier, string *name, const type_specifier *parent_type, int source_offset)
{
	single_declarator *sd;
	ESSL_CHECK_NONFATAL(sd = _essl_mempool_alloc(pool, sizeof(single_declarator)));
	sd->type = type;
	sd->parent_type = parent_type;
	sd->qualifier = qualifier;
	sd->index = 0;
	sd->source_offset = source_offset;
	if (name)
	{
		sd->name = *name;
	}
	return sd;
}

type_specifier *_essl_clone_type(mempool *pool, const type_specifier *t)
{
	type_specifier *ts;
	ESSL_CHECK_NONFATAL(ts = _essl_new_type(pool));
	memcpy(ts, t, sizeof(type_specifier));
	return ts;
}

const type_specifier *_essl_get_unqualified_type(mempool *pool, const type_specifier *t)
{
	type_specifier *ts;
	if(t->type_qual == TYPE_QUAL_NONE) return t;
	ESSL_CHECK_NONFATAL(ts = _essl_clone_type(pool, t));
	ts->type_qual = TYPE_QUAL_NONE;
	return ts;
}



unsigned int _essl_get_type_size(const type_specifier *t)
{
	assert(t->basic_type != TYPE_UNRESOLVED_ARRAY_OF);
	if(t->basic_type == TYPE_ARRAY_OF)
	{
		return t->u.array_size * _essl_get_type_size(t->child_type);
	} else if(t->basic_type == TYPE_MATRIX_OF)
	{
		return t->u.matrix_n_columns * _essl_get_type_size(t->child_type);
	} else if(t->basic_type == TYPE_STRUCT)
	{
		single_declarator *memb = t->members;
		unsigned int elem_size = 0;
		while(memb)
		{
			elem_size += _essl_get_type_size(memb->type);
				memb = memb->next;
		}
		return elem_size;
	} else 
	{
		return GET_VEC_SIZE(t);
	}
}


unsigned _essl_get_specified_samplers_num(const type_specifier *t, type_basic sampler_type)
{
	switch(t->basic_type)
	{
	  case TYPE_MATRIX_OF:
		return GET_MATRIX_N_COLUMNS(t) * _essl_get_specified_samplers_num(t->child_type, sampler_type);

	  case TYPE_ARRAY_OF:
		return t->u.array_size * _essl_get_specified_samplers_num(t->child_type, sampler_type);

	  case TYPE_STRUCT:
		{
			unsigned res = 0;
			single_declarator *m;
			for(m = t->members; m != NULL; m = m ->next)
			{
				res += _essl_get_specified_samplers_num(m->type, sampler_type);
			}
			return res;
		}

	  default:
		break;
	}

	if(t->basic_type == sampler_type)
	{
		return 1;
	}

	assert(t->basic_type != TYPE_UNKNOWN && t->basic_type != TYPE_UNRESOLVED_ARRAY_OF);
	return 0;
}


unsigned int _essl_get_type_vec_size(const type_specifier *t)
{
	assert(t->basic_type != TYPE_MATRIX_OF &&
		   t->basic_type != TYPE_STRUCT &&
		   t->basic_type != TYPE_ARRAY_OF &&
		   t->basic_type != TYPE_UNRESOLVED_ARRAY_OF &&
		   t->basic_type != TYPE_UNKNOWN);
	return t->u.basic.vec_size;
}

essl_bool _essl_type_has_vec_size(const type_specifier *t)
{
	return t->basic_type == TYPE_FLOAT || t->basic_type == TYPE_INT || t->basic_type == TYPE_BOOL;
}

unsigned int _essl_get_matrix_n_columns(const type_specifier *t)
{
	assert(t->basic_type == TYPE_MATRIX_OF);
	return t->u.matrix_n_columns;
}

unsigned int _essl_get_matrix_n_rows(const type_specifier *t)
{
	assert(t->basic_type == TYPE_MATRIX_OF);
	assert(t->child_type != NULL);
	assert(t->child_type->basic_type == TYPE_FLOAT || t->child_type->basic_type == TYPE_INT || t->child_type->basic_type == TYPE_BOOL);

	return t->child_type->u.basic.vec_size;
}

scalar_size_specifier _essl_get_scalar_size_for_type(const type_specifier *t)
{
	while(t->child_type != NULL)
	{
		t = t->child_type;
	}
	assert(t->basic_type != TYPE_STRUCT);
	return t->u.basic.scalar_size;
}


int _essl_get_type_member_offset(const type_specifier *t, const single_declarator *sd)
{
	unsigned int cur_offset = 0;
	single_declarator *memb;
	assert(t->basic_type == TYPE_STRUCT);

	for (memb = t->members; memb; memb = memb->next)
	{
		if (memb == sd)
		{
			return (int)cur_offset;
		}
		cur_offset += _essl_get_type_size(memb->type);
	}

	return -1;
}

int _essl_type_equal(const type_specifier *a, const type_specifier *b)
{
	assert(a != 0);
	assert(b != 0);
	if(a == b) return 1;
	if(a->basic_type != b->basic_type) return 0;
	switch(a->basic_type)
	{
	case TYPE_STRUCT:
		if(a->name.ptr == NULL || b->name.ptr == NULL) return 0; /* no name => no equality */
		if(_essl_string_cmp(a->name, b->name)) return 0;
		if(a->members != b->members) return 0;
		return 1;
	case TYPE_ARRAY_OF:
		if(a->u.array_size != b->u.array_size) return 0;
		return _essl_type_equal(a->child_type, b->child_type);
	case TYPE_MATRIX_OF:
		if(a->u.matrix_n_columns != b->u.matrix_n_columns) return 0;
		return _essl_type_equal(a->child_type, b->child_type);

	case TYPE_UNRESOLVED_ARRAY_OF:
		assert(0 && "Can't compare unresolved arrays");
		return 1;
	default:
		assert(a->child_type == NULL);
		return a->u.basic.vec_size == b->u.basic.vec_size;

	}
}

essl_bool _essl_type_with_scalar_size_equal(const type_specifier *a, const type_specifier *b)
{
	assert(a != 0);
	assert(b != 0);
	if(a == b) return 1;
	if(a->basic_type != b->basic_type) return ESSL_FALSE;
	switch(a->basic_type)
	{
	case TYPE_STRUCT:
		if(a->name.ptr == NULL || b->name.ptr == NULL) return 0; /* no name => no equality */
		if(_essl_string_cmp(a->name, b->name)) return ESSL_FALSE;
		if(a->members != b->members) return ESSL_FALSE;
		return 1;
	case TYPE_ARRAY_OF:
		if(a->u.array_size != b->u.array_size) return ESSL_FALSE;
		return _essl_type_equal(a->child_type, b->child_type);
	case TYPE_MATRIX_OF:
		if(a->u.matrix_n_columns != b->u.matrix_n_columns) return ESSL_FALSE;
		return _essl_type_equal(a->child_type, b->child_type);

	case TYPE_UNRESOLVED_ARRAY_OF:
		assert(0 && "Can't compare unresolved arrays");
		return ESSL_TRUE;
	default:
		assert(a->child_type == NULL);
		if(a->u.basic.scalar_size != b->u.basic.scalar_size) return ESSL_FALSE;
		return a->u.basic.vec_size == b->u.basic.vec_size;

	}
}

essl_bool _essl_type_scalar_part_equal(const type_specifier *a, const type_specifier *b)
{
	assert(a != 0);
	assert(b != 0);
	if(a == b) return 1;
	if(a->basic_type != b->basic_type) return ESSL_FALSE;
	switch(a->basic_type)
	{
	case TYPE_STRUCT:
		if(a->name.ptr == NULL || b->name.ptr == NULL) return 0; /* no name => no equality */
		if(_essl_string_cmp(a->name, b->name)) return ESSL_FALSE;
		if(a->members != b->members) return ESSL_FALSE;
		return 1;
	case TYPE_ARRAY_OF:
		if(a->u.array_size != b->u.array_size) return ESSL_FALSE;
		return _essl_type_equal(a->child_type, b->child_type);
	case TYPE_MATRIX_OF:
		if(a->u.matrix_n_columns != b->u.matrix_n_columns) return ESSL_FALSE;
		return _essl_type_equal(a->child_type, b->child_type);

	case TYPE_UNRESOLVED_ARRAY_OF:
		assert(0 && "Can't compare unresolved arrays");
		return ESSL_TRUE;
	default:
		assert(a->child_type == NULL);
		return a->u.basic.scalar_size == b->u.basic.scalar_size;

	}
}

const type_specifier* _essl_get_type_with_given_size(const typestorage_context *ctx, const type_specifier *a, scalar_size_specifier sz)
{
	if(a->basic_type == TYPE_STRUCT)
	{
		assert(0);
		return NULL;
	} else if(a->child_type != NULL)
	{
		type_specifier *ts;
		const type_specifier *new_child_type;
		ESSL_CHECK(new_child_type = _essl_get_type_with_given_size(ctx, a->child_type, sz));
		ESSL_CHECK(ts = _essl_clone_type(ctx->pool, a));
		ts->child_type = new_child_type;
		return ts;
	}

	assert(a->child_type == NULL);
	if(a->u.basic.scalar_size == sz)
	{
		return a;
	}
	else
	{
		/* if it's unqualified then return the basic type */
		if(a->type_qual == TYPE_QUAL_NONE)
		{
			return _essl_get_type_with_size(ctx, a->basic_type, a->u.basic.vec_size, sz);
		}
		/* if it is then clone the type specifier and change the size */
		else
		{
			type_specifier* ts;
			ESSL_CHECK_NONFATAL(ts = _essl_clone_type(ctx->pool, a));
			ts->u.basic.scalar_size = sz;
			return ts;
		}
	}
}

const type_specifier* _essl_get_type_with_given_vec_size(const typestorage_context *ctx, const type_specifier *a, unsigned vec_size)
{
	assert(a->child_type == NULL && a->basic_type != TYPE_STRUCT);
	if(a->child_type != NULL) return NULL;
	if(a->basic_type == TYPE_STRUCT) return NULL;

	if(a->u.basic.vec_size == vec_size)
	{
		return a;
	}
	else
	{
		/* if it's not an array or a struct then return the basic type */
		if(a->type_qual == TYPE_QUAL_NONE)
		{
			return _essl_get_type_with_size_and_signedness(ctx, a->basic_type, vec_size, a->u.basic.scalar_size, a->u.basic.int_signedness);
		}
		else
		{
			type_specifier* ts;
			ESSL_CHECK_NONFATAL(ts = _essl_clone_type(ctx->pool, a));
			ts->u.basic.vec_size = vec_size;
			return ts;
		}
	}
}

/*@null@*/ const type_specifier *_essl_get_single_matrix_column_type(const type_specifier *mat_type)
{
	assert(mat_type->basic_type == TYPE_MATRIX_OF);
	return mat_type->child_type;
}


/*@null@*/ type_specifier *_essl_new_matrix_of_type(struct _tag_mempool *pool, const type_specifier *child_type, unsigned n_columns)
{
	type_specifier *t;
	ESSL_CHECK(t = _essl_new_type(pool));
	t->basic_type = TYPE_MATRIX_OF;
	t->child_type = child_type;
	t->type_qual = child_type->type_qual;
	t->u.matrix_n_columns = n_columns;
	return t;
}

/*@null@*/ type_specifier *_essl_new_array_of_type(struct _tag_mempool *pool, const type_specifier *child_type, unsigned array_size)
{
	type_specifier *t;
	ESSL_CHECK(t = _essl_new_type(pool));
	t->basic_type = TYPE_ARRAY_OF;
	t->child_type = child_type;
	t->type_qual = child_type->type_qual;
	t->u.array_size = array_size;
	return t;
}


/*@null@*/ type_specifier *_essl_new_unresolved_array_of_type(struct _tag_mempool *pool, const type_specifier *child_type, union _tag_node *unresolved_array_size)
{
	type_specifier *t;
	ESSL_CHECK(t = _essl_new_type(pool));
	t->basic_type = TYPE_UNRESOLVED_ARRAY_OF;
	t->child_type = child_type;
	t->type_qual = child_type->type_qual;
	t->u.unresolved_array_size = unresolved_array_size;
	return t;
}

type_basic _essl_get_nonderived_basic_type(const type_specifier *t)
{
	while(t->child_type != NULL)
	{
		t = t->child_type;
	}
	return t->basic_type;
}

essl_bool _essl_is_sampler_type(const type_specifier *t)
{
	type_basic bt;
	while(t->basic_type == TYPE_ARRAY_OF)
	{
		t = t->child_type;
	}
	bt = t->basic_type;
	if(bt == TYPE_SAMPLER_2D 
	   || bt == TYPE_SAMPLER_3D 
	   || bt == TYPE_SAMPLER_CUBE
	   || bt == TYPE_SAMPLER_2D_SHADOW
	   || bt == TYPE_SAMPLER_EXTERNAL)
	{
		return ESSL_TRUE;
	}
	return ESSL_FALSE;
}

int _essl_size_of_scalar(scalar_size_specifier sz)
{
	switch(sz)
	{
	case SIZE_BITS8:
		return 1;
	case SIZE_BITS16:
		return 2;
	case SIZE_BITS32:
		return 4;
	case SIZE_BITS64:
		return 8;
	case SIZE_UNKNOWN:
		assert(0);
		return 0;
	}
	return 0;
}

/** Is access to this type inherently control dependent? */
essl_bool _essl_is_type_control_dependent(const type_specifier *t, essl_bool is_indexed_statically)
{
	if(t->basic_type == TYPE_ARRAY_OF)
	{
		if(is_indexed_statically)
		{
			return ESSL_FALSE;
		}else
		{
			return ESSL_TRUE;
		}
	}
	if(t->basic_type == TYPE_MATRIX_OF) return ESSL_TRUE;
	if(t->basic_type == TYPE_STRUCT)
	{
		single_declarator *member;
		for(member = t->members; member != 0; member = member->next)
		{
			if(_essl_is_type_control_dependent(member->type, ESSL_FALSE)) return ESSL_TRUE;
		}

	}
	return ESSL_FALSE;
}

/** Checks if given type is or contains a sampler
  * Checks if given type is a sampler (TYPE_SAMPLER, TYPE_SAMPLER_CUBE or TYPE_SAMPLER_SHADOW).
  * If it is TYPE_STRUCT, then each member is checked (recursivly)
  * @param type The type specifier to check
  * @return ESSL_TRUE if it is or contains a sampler, ESSL_FALSE if not
  */
essl_bool _essl_type_is_or_has_sampler(const type_specifier* type)
{
	if (type->basic_type == TYPE_SAMPLER_2D ||
	    type->basic_type == TYPE_SAMPLER_3D ||
	    type->basic_type == TYPE_SAMPLER_CUBE ||
	    type->basic_type == TYPE_SAMPLER_2D_SHADOW)
	{
		return ESSL_TRUE;
	}
	if(type->child_type != NULL) return _essl_type_is_or_has_sampler(type->child_type);

	if (type->basic_type == TYPE_STRUCT)
	{
		single_declarator* sub_decl = type->members;
		while (sub_decl != NULL)
		{
			if (_essl_type_is_or_has_sampler(sub_decl->type))
			{
				return ESSL_TRUE;
			}

			sub_decl = sub_decl->next;
		}
	}

	return ESSL_FALSE;
}

