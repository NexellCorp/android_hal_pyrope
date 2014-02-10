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
#include "common/essl_target.h"
#include "backend/serializer.h"

#define VVAR_BLOCK_CURRENT_VERSION 2
#define VATT_BLOCK_CURRENT_VERSION 2
#define VUNI_BLOCK_CURRENT_VERSION 2

enum mali_essl_symbol_datatype_v0
{
        DATATYPE_V0_UNKNOWN = 0, /**< Unknown */   
        DATATYPE_V0_FLOAT = 1, /**< float, vec2, vec3, vec4 types */
        DATATYPE_V0_INT = 2, /**< int, ivec2, ivec3, ivec4 types */
        DATATYPE_V0_BOOL = 3, /**< bool, bvec2, bvec3, bvec4 types */
        DATATYPE_V0_MATRIX = 4, /**< mat2, mat3, mat4 types */
        DATATYPE_V0_SAMPLER = 5, /**< sampler1D, sampler2D, sampler3D types */
        DATATYPE_V0_SAMPLER_CUBE = 6, /**< samplerCube type */
        DATATYPE_V0_SAMPLER_SHADOW = 7, /**< sampler2DShadow type */
        DATATYPE_V0_STRUCT = 8, /**< struct types */
        DATATYPE_V0_SAMPLER_EXTERNAL = 9 /**< samplerEXTERNAL type */
};

enum mali_essl_symbol_datatype_v1
{
        DATATYPE_V1_UNKNOWN = 0, /**< Unknown */   
        DATATYPE_V1_FLOAT = 1, /**< float, vec2, vec3, vec4 types */
        DATATYPE_V1_INT = 2, /**< int, ivec2, ivec3, ivec4 types */
        DATATYPE_V1_BOOL = 3, /**< bool, bvec2, bvec3, bvec4 types */
		DATATYPE_V1_SAMPLER_1D = 4, /**< sampler1D type */
		DATATYPE_V1_SAMPLER_2D = 5, /**< sampler2D type */
		DATATYPE_V1_SAMPLER_3D = 6, /**< sampler3D type */
		DATATYPE_V1_SAMPLER_CUBE = 7, /**< samplerCube type */
		DATATYPE_V1_SAMPLER_2D_SHADOW = 8, /* sampler2DShadow type */
		DATATYPE_V1_SAMPLER_EXTERNAL = 9, /* samplerEXTERNAL / samplerExternalOES type */
        DATATYPE_V1_STRUCT = 10, /**< struct type */
        DATATYPE_V1_MATRIX_OF = 11, /**< matrix of child type */
        DATATYPE_V1_ARRAY_OF = 12 /**< array of child type */
};

enum mali_essl_symbol_precision {
        PRECISION_UNKNOWN = 0,
        PRECISION_LOW = 1,
        PRECISION_MEDIUM = 2,
        PRECISION_HIGH = 3
};

enum mali_cpu_operand_kind
{
	OPND_CONST = 0,	/* literal */
	OPND_LVAR = 1,	/* local variable */
	OPND_VUNI = 2,	/* uniform */
	OPND_VATT = 3,	/* attribute */
	OPND_VVAR = 4	/* varying */
};

enum mali_cpu_expression_kind
{
	EXPR_ADD = 0,/* addition */
	EXPR_SUB = 1,/* subtraction */
	EXPR_MUL = 2,/* multiplication */
	EXPR_DIV = 3,/* division */
	EXPR_SWIZZLE = 4,/* swizzle */
	EXPR_CONSTR = 5,/* vector/matrix constructor */
	EXPR_ASSIGN = 6,/* assignment */
	EXPR_INDEX = 7/* array subscript */
};

enum mali_cpu_vec_size
{
	EXPR_VEC_SIZE_1 = 1,	/* float, int , bool*/
	EXPR_VEC_SIZE_2 = 2,	/* vec2, ivec2, bvec2 */
	EXPR_VEC_SIZE_3 = 3,	/* vec3, ivec3, bvec3 */
	EXPR_VEC_SIZE_4 = 4,	/* vec4, ivec4, bvec4 */
	EXPR_MAT_SIZE_2 = 22,	/* mat2 */
	EXPR_MAT_SIZE_3 = 23,	/* mat3 */
	EXPR_MAT_SIZE_4 = 24	/* mat4 */
};



#define APPEND_INT8 _essl_output_buffer_append_int8
#define APPEND_INT16 _essl_output_buffer_append_int16
#define APPEND_INT32 _essl_output_buffer_append_int32

#define WRITE_HEADER(buf, id) size_t len_position; ESSL_CHECK(append_id(buf, id)); len_position = _essl_output_buffer_get_word_position(buf); ESSL_CHECK(APPEND_INT32(buf, 0))

#define WRITE_LEN(buf) (_essl_output_buffer_replace_bits(buf, len_position, 0, 32, (u32)(_essl_output_buffer_get_word_position(buf) - len_position - 1)*4))

static enum mali_essl_symbol_datatype_v0 convert_basic_type_to_v0(const type_specifier *t)
{
	switch(t->basic_type)
	{
	case TYPE_UNKNOWN:
	case TYPE_VOID:
	case TYPE_UNRESOLVED_ARRAY_OF:
		assert(0);
		return DATATYPE_V0_UNKNOWN;

	case TYPE_FLOAT:
		return DATATYPE_V0_FLOAT;
	case TYPE_INT:
		return DATATYPE_V0_INT;
	case TYPE_BOOL:
		return DATATYPE_V0_BOOL;
	case TYPE_MATRIX_OF:
		return DATATYPE_V0_MATRIX;
	case TYPE_SAMPLER_2D:
	case TYPE_SAMPLER_3D:
		return DATATYPE_V0_SAMPLER;
	case TYPE_SAMPLER_CUBE:
		return DATATYPE_V0_SAMPLER_CUBE;
	case TYPE_SAMPLER_2D_SHADOW:
		return DATATYPE_V0_SAMPLER_SHADOW;
	case TYPE_SAMPLER_EXTERNAL:
		return DATATYPE_V0_SAMPLER_EXTERNAL;
	case TYPE_STRUCT:
		return DATATYPE_V0_STRUCT;
	case TYPE_ARRAY_OF:
		return convert_basic_type_to_v0(t->child_type);
	}

	return DATATYPE_V0_UNKNOWN;
}

static enum mali_essl_symbol_datatype_v1 convert_basic_type_to_v1(const type_specifier *t)
{
	switch(t->basic_type)
	{
	case TYPE_UNKNOWN:
	case TYPE_VOID:
	case TYPE_UNRESOLVED_ARRAY_OF:
		assert(0);
		return DATATYPE_V1_UNKNOWN;

	case TYPE_FLOAT:
		return DATATYPE_V1_FLOAT;
	case TYPE_INT:
		return DATATYPE_V1_INT;
	case TYPE_BOOL:
		return DATATYPE_V1_BOOL;
	case TYPE_MATRIX_OF:
		return DATATYPE_V1_MATRIX_OF;
	case TYPE_SAMPLER_2D:
		return DATATYPE_V1_SAMPLER_2D;
	case TYPE_SAMPLER_3D:
		return DATATYPE_V1_SAMPLER_3D;
	case TYPE_SAMPLER_CUBE:
		return DATATYPE_V1_SAMPLER_CUBE;
	case TYPE_SAMPLER_2D_SHADOW:
		return DATATYPE_V1_SAMPLER_2D_SHADOW;
	case TYPE_SAMPLER_EXTERNAL:
		return DATATYPE_V1_SAMPLER_EXTERNAL;
	case TYPE_STRUCT:
		return DATATYPE_V1_STRUCT;
	case TYPE_ARRAY_OF:
		return DATATYPE_V1_ARRAY_OF;
	}

	return DATATYPE_V1_UNKNOWN;
}

static unsigned convert_type_size_v0(const type_specifier *type) {
	switch (type->basic_type)
	{
	case TYPE_SAMPLER_2D:
	case TYPE_SAMPLER_2D_SHADOW:
		return 2;
	case TYPE_SAMPLER_3D:
	case TYPE_SAMPLER_CUBE:
		return 3;
	case TYPE_STRUCT:
		return 1;
	case TYPE_MATRIX_OF:
		assert(GET_MATRIX_N_COLUMNS(type) == GET_MATRIX_N_ROWS(type));
		return GET_MATRIX_N_COLUMNS(type);
	default:
		return GET_VEC_SIZE(type);
	}
}

static unsigned convert_scalar_size(scalar_size_specifier scalar_size)
{
	return (unsigned)scalar_size - (unsigned)SIZE_BITS8;
}

static enum mali_essl_symbol_precision convert_precision(const type_specifier *t, qualifier_set qual)
{
	IGNORE_PARAM(t);
	switch(qual.precision)
	{
	case PREC_UNKNOWN:
		assert(t->basic_type != TYPE_INT && t->basic_type != TYPE_FLOAT && t->basic_type != TYPE_MATRIX_OF);
		return PRECISION_MEDIUM;
	case PREC_LOW:
		return PRECISION_LOW;
	case PREC_MEDIUM:
		return PRECISION_MEDIUM;
	case PREC_HIGH:
		return PRECISION_HIGH;
	}
	return PRECISION_UNKNOWN;
}

static unsigned convert_bit_precision(unsigned bit_precision)
{
	if(bit_precision < 16)
	{
		bit_precision = 16;
	}
	return bit_precision;
}

static memerr append_id(output_buffer *buf, const char *id)
{
	size_t i;
	assert(strlen(id) == 4);
	for(i = 0; i < 4; ++i)
	{
		ESSL_CHECK(APPEND_INT8(buf, (u32)id[i]));
	}
	return MEM_OK;
}


static memerr write_string(output_buffer *buf, string str)
{
	int i;
	WRITE_HEADER(buf, "STRI");
	for(i = 0; i < str.len; ++i)
	{
		ESSL_CHECK(APPEND_INT8(buf, (u32)str.ptr[i]));
	}
	
	ESSL_CHECK(APPEND_INT8(buf, 0)); /* at least one byte of zero termination */
	++i;

	for(; (i & 3) != 0; ++i)
	{
		ESSL_CHECK(APPEND_INT8(buf, 0));
	}

	WRITE_LEN(buf);
	return MEM_OK;
}

static memerr write_initializer(output_buffer *buf, target_descriptor *desc, node *initializer)
{
	union {
		float f;
		unsigned int i;
	} v;
	unsigned i;
	unsigned size = _essl_get_type_size(initializer->hdr.type);
	WRITE_HEADER(buf, "VINI");
	ESSL_CHECK(APPEND_INT32(buf, size));
	for(i = 0; i < size; ++i)
	{
		v.f = desc->scalar_to_float(initializer->expr.u.value[i]);
		ESSL_CHECK(APPEND_INT32(buf, v.i));
	}

	

	WRITE_LEN(buf);
	return MEM_OK;
}

static memerr write_type(output_buffer *buf, target_descriptor *desc, const type_specifier *t, qualifier_set qual, address_space_kind kind, essl_bool opt_sampler);

static memerr write_member(output_buffer *buf, target_descriptor *desc, single_declarator *mem, address_space_kind kind)
{
	int offset = desc->get_type_member_offset(desc, mem, kind);
	WRITE_HEADER(buf, "SMEM");
	ESSL_CHECK(write_string(buf, mem->name));
	ESSL_CHECK(offset != -1);
	ESSL_CHECK(APPEND_INT32(buf, offset));
	ESSL_CHECK(write_type(buf, desc, mem->type, mem->qualifier, kind, ESSL_FALSE));
	WRITE_LEN(buf);
	return MEM_OK;
}



static memerr write_type(output_buffer *buf, target_descriptor *desc, const type_specifier *t, qualifier_set qual, address_space_kind kind, essl_bool opt_sampler)
{
	unsigned n_elems;
	unsigned stride;
	enum mali_essl_symbol_datatype_v1 dtype = convert_basic_type_to_v1(t);
	WRITE_HEADER(buf, "TYPE");

	stride = desc->get_array_stride(desc, t, kind);
	switch(t->basic_type)
	{
	case TYPE_ARRAY_OF:
		n_elems = t->u.array_size;
		break;
	case TYPE_MATRIX_OF:
		n_elems = GET_MATRIX_N_COLUMNS(t);
		break;
	case TYPE_STRUCT:
		n_elems = _essl_list_length((generic_list*)t->members);
		break;
	default:
		n_elems = GET_VEC_SIZE(t);
		break;
	}

	
	ESSL_CHECK(APPEND_INT32(buf, dtype));
	ESSL_CHECK(APPEND_INT32(buf, n_elems));
	ESSL_CHECK(APPEND_INT32(buf, stride));

	if(t->child_type != NULL)
	{
		ESSL_CHECK(write_type(buf, desc, t->child_type, qual, kind, opt_sampler));
	} else if(t->basic_type == TYPE_STRUCT)
	{
		single_declarator *mem;
		for(mem = t->members; mem != NULL; mem = mem->next)
		{
			ESSL_CHECK(write_member(buf, desc, mem, kind));
		}
	} else {
		ESSL_CHECK(APPEND_INT8(buf, convert_scalar_size(t->u.basic.scalar_size)));
		ESSL_CHECK(APPEND_INT8(buf, t->u.basic.int_signedness == INT_UNSIGNED));
		ESSL_CHECK(APPEND_INT8(buf, convert_precision(t, qual)));
		ESSL_CHECK(APPEND_INT8(buf, 0)); /* pad */
		

		if(dtype >= DATATYPE_V1_SAMPLER_1D && dtype <= DATATYPE_V1_SAMPLER_EXTERNAL)
		{
			ESSL_CHECK(APPEND_INT8(buf, opt_sampler ? 1 : 0));
			ESSL_CHECK(APPEND_INT8(buf, 0));
			ESSL_CHECK(APPEND_INT16(buf, 0));
		}
	}

	WRITE_LEN(buf);
	return MEM_OK;
}

static memerr write_grd_index(output_buffer *buf, symbol *sym, int parent, int offset)
{
	int grd_idx;
	
	WRITE_HEADER(buf, "IGRD");

	grd_idx = sym->opt.grad_index;
	if(grd_idx != -1 && parent != -1) 
	{
		/* struct member */
		grd_idx += offset;
	}
	/* index to read from gl_mali_textureGRADEXT_sizes */
	ESSL_CHECK(APPEND_INT32(buf, grd_idx)); 

	WRITE_LEN(buf);
	return MEM_OK;
}


static memerr write_yuv_index(output_buffer *buf, symbol *sym, int parent, int offset)
{
	int ext_idx;
	
	WRITE_HEADER(buf, "IYUV");

	ext_idx = sym->opt.external_index;
	if(ext_idx != -1 && parent != -1)
	{
		/* struct member */
		ext_idx += offset;
	}

	ESSL_CHECK(APPEND_INT32(buf, ext_idx)); /* index to read from gl_mali_YUVCoefficients */

	WRITE_LEN(buf);
	return MEM_OK;
}

static memerr write_td_remap_table_index(output_buffer *buf)
{
	WRITE_HEADER(buf, "ITDR");

	/* index to read from the TD remap table, -1 for now, the driver should find it out on its own */

	ESSL_CHECK(APPEND_INT32(buf, (u32)-1)); 

	WRITE_LEN(buf);
	return MEM_OK;
}



static memerr write_indices(output_buffer *buf, symbol *sym, int parent, int offset)
{
	WRITE_HEADER(buf, "VIDX");

	ESSL_CHECK(write_td_remap_table_index(buf));

	ESSL_CHECK(write_yuv_index(buf, sym, parent, offset));

	ESSL_CHECK(write_grd_index(buf, sym, parent, offset));

	WRITE_LEN(buf);
	return MEM_OK;
}


static memerr write_uniform(output_buffer *buf, target_descriptor *desc, string name, const type_specifier *type, qualifier_set qual, symbol *sym, size_t *output_number, int address, int parent, /*@null@*/ int *max_address)
{
	size_t curr_num;
	size_t vec_size_position;
	unsigned array_stride;
	unsigned vector_stride;
	unsigned array_size = 0;
	const type_specifier *orig_type = type;
	enum mali_essl_symbol_datatype_v0 btype;
	WRITE_HEADER(buf,"VUNI");

	if(type->basic_type == TYPE_ARRAY_OF)
	{
		array_size = type->u.array_size;
		type = type->child_type;
	}
	array_stride = desc->get_array_stride(desc, type, sym->address_space);


	btype = convert_basic_type_to_v0(type);

	if(type->basic_type == TYPE_MATRIX_OF) 
	{
		type = type->child_type;
	}

	vector_stride = desc->get_array_stride(desc, type, sym->address_space);


	ESSL_CHECK(write_string(buf, name));
	ESSL_CHECK(APPEND_INT8(buf, VVAR_BLOCK_CURRENT_VERSION)); /* version */
	ESSL_CHECK(APPEND_INT8(buf, btype));
	vec_size_position = _essl_output_buffer_get_word_position(buf);
	ESSL_CHECK(APPEND_INT16(buf, convert_type_size_v0(type)));
	ESSL_CHECK(APPEND_INT16(buf, vector_stride));
	ESSL_CHECK(APPEND_INT16(buf, array_size));
	ESSL_CHECK(APPEND_INT16(buf, array_stride));
	ESSL_CHECK(APPEND_INT8(buf, convert_bit_precision(sym->max_read_bit_precision)));
	ESSL_CHECK(APPEND_INT8(buf, convert_precision(type, qual)));
	ESSL_CHECK(APPEND_INT8(buf, sym->is_invariant));
	ESSL_CHECK(APPEND_INT8(buf, 0)); /* reserved */
	ESSL_CHECK(APPEND_INT16(buf, 0)); /* reserved */
	assert(address >= 0);
	ESSL_CHECK(APPEND_INT16(buf, (u32)address));
	ESSL_CHECK(APPEND_INT16(buf, (u32)parent&0xffff)); /* parent */

	ESSL_CHECK(write_indices(buf, sym, parent, address));

	if(parent == -1 && sym->body != 0 && sym->body->hdr.kind == EXPR_KIND_CONSTANT)
	{
		ESSL_CHECK(write_initializer(buf, desc, sym->body));

	}
	WRITE_LEN(buf);

	if(max_address)
	{
		int my_max_address = address + desc->get_type_size(desc, orig_type, sym->address_space);
		if(my_max_address > *max_address)
			*max_address = my_max_address;
	}

	curr_num = *output_number;
	++(*output_number);
	if(btype == DATATYPE_V0_STRUCT)
	{
		u32 n_members = 0;
		single_declarator *member;
		for(member = type->members; member != 0; member = member->next, ++n_members)
		{
			int offset = desc->get_type_member_offset(desc, member, sym->address_space);
			ESSL_CHECK(offset != -1);
			ESSL_CHECK(write_uniform(buf, desc, member->name, member->type, member->qualifier, sym, output_number, offset, (int)curr_num, 0));
		}
		_essl_output_buffer_replace_bits(buf, vec_size_position, 16, 16, n_members);
		

	}
	
	return MEM_OK;
}

static memerr write_uniform_set(output_buffer *buf, translation_unit *tu)
{
	int max_address = 0;
	size_t count_position, max_address_position;
	size_t count = 0;
	symbol_list *sl;

	WRITE_HEADER(buf, "SUNI");

	count_position = _essl_output_buffer_get_word_position(buf);
	ESSL_CHECK(APPEND_INT32(buf, 0));
	max_address_position = _essl_output_buffer_get_word_position(buf);
	ESSL_CHECK(APPEND_INT32(buf, 0));
	for(sl = tu->uniforms; sl != 0; sl = sl->next)
	{
		symbol *s = sl->sym;
		if(s->address != -1)
		{
			ESSL_CHECK(write_uniform(buf, tu->desc, s->name, s->type, s->qualifier, s, &count, s->address, -1, &max_address));
		}

	}
	_essl_output_buffer_replace_bits(buf, count_position, 0, 32, (u32)count);
	max_address = (max_address+3)&~3; /* make sure that the number of addresses are a multiple of 4 (max stride multiple), as the driver assumes this. Fixes 3472. */
	_essl_output_buffer_replace_bits(buf, max_address_position, 0, 32, (u32)max_address);
	
	WRITE_LEN(buf);
	return MEM_OK;
}


static memerr write_attribute(output_buffer *buf, target_descriptor *desc, symbol *sym)
{
	const type_specifier *t = sym->type;

	unsigned array_stride;
	unsigned vector_stride;
	unsigned array_size = 0;
	type_specifier vec_type = *t;
	WRITE_HEADER(buf,"VATT");
	if(t->basic_type == TYPE_ARRAY_OF)
	{
		array_size = t->u.array_size;
		vec_type = *t->child_type;
	}

	array_stride = desc->get_array_stride(desc, &vec_type, sym->address_space);
	if(vec_type.basic_type == TYPE_MATRIX_OF) vec_type = *vec_type.child_type;
	vector_stride = desc->get_array_stride(desc, &vec_type, sym->address_space);

	
	ESSL_CHECK(write_string(buf, sym->name));

	ESSL_CHECK(APPEND_INT8(buf, VATT_BLOCK_CURRENT_VERSION)); /* version */
	ESSL_CHECK(APPEND_INT8(buf, convert_basic_type_to_v0(t)));
	ESSL_CHECK(APPEND_INT16(buf, GET_VEC_SIZE(&vec_type)));
	ESSL_CHECK(APPEND_INT16(buf, vector_stride));
	ESSL_CHECK(APPEND_INT16(buf, array_size));
	ESSL_CHECK(APPEND_INT16(buf, array_stride));

	ESSL_CHECK(APPEND_INT8(buf, convert_bit_precision(sym->max_read_bit_precision))); /* bit precision */
	ESSL_CHECK(APPEND_INT8(buf, convert_precision(t, sym->qualifier))); /* essl precision */
	ESSL_CHECK(APPEND_INT8(buf, sym->is_invariant));


	ESSL_CHECK(APPEND_INT8(buf, 0)); /* reserved */
	assert(sym->address >= 0);
	ESSL_CHECK(APPEND_INT16(buf, (u32)sym->address));
	WRITE_LEN(buf);
	return MEM_OK;
}


static memerr write_attribute_set(output_buffer *buf, translation_unit *tu)
{
	size_t count_position;
	u32 count = 0;
	symbol_list *sl;
	WRITE_HEADER(buf, "SATT");
	count_position = _essl_output_buffer_get_word_position(buf);
	ESSL_CHECK(APPEND_INT32(buf, 0));
	for(sl = tu->vertex_attributes; sl != 0; sl = sl->next)
	{
		symbol *s = sl->sym;
		if(s->address != -1)
		{
			ESSL_CHECK(write_attribute(buf, tu->desc, s));
			++count;
		}

	}
	_essl_output_buffer_replace_bits(buf, count_position, 0, 32, count);
	
	WRITE_LEN(buf);
	return MEM_OK;
}


static memerr write_varying(output_buffer *buf, target_descriptor *desc, symbol *sym)
{
	const type_specifier *t = sym->type;
	type_specifier vec_type = *t;
	unsigned array_size = 0;
	unsigned array_stride;
    unsigned vector_stride;
	WRITE_HEADER(buf,"VVAR");
	if(t->basic_type == TYPE_ARRAY_OF)
	{
		array_size = t->u.array_size;
		vec_type = *t->child_type;
	}

	array_stride = desc->get_array_stride(desc, &vec_type, sym->address_space);
	if(vec_type.basic_type == TYPE_MATRIX_OF) vec_type = *vec_type.child_type;
	vector_stride = desc->get_array_stride(desc, &vec_type, sym->address_space);
	ESSL_CHECK(write_string(buf, sym->name));
	
	ESSL_CHECK(APPEND_INT8(buf, VVAR_BLOCK_CURRENT_VERSION)); /* version */
	ESSL_CHECK(APPEND_INT8(buf, convert_basic_type_to_v0(t)));

	ESSL_CHECK(APPEND_INT16(buf, GET_VEC_SIZE(&vec_type)));
	ESSL_CHECK(APPEND_INT16(buf, vector_stride));
	ESSL_CHECK(APPEND_INT16(buf, array_size));
	ESSL_CHECK(APPEND_INT16(buf, array_stride));

	ESSL_CHECK(APPEND_INT8(buf, convert_bit_precision(sym->max_read_bit_precision))); /* bit precision */
	ESSL_CHECK(APPEND_INT8(buf, convert_precision(t, sym->qualifier)));

	ESSL_CHECK(APPEND_INT8(buf, sym->is_invariant));
	ESSL_CHECK(APPEND_INT8(buf, ESSL_FALSE)); /* varying clamped before assignment */

	ESSL_CHECK(APPEND_INT16(buf, 0)); /* reserved */

	assert(sym->address >= 0 || sym->address == -1);
	ESSL_CHECK(APPEND_INT16(buf, (u32)sym->address));
	ESSL_CHECK(APPEND_INT16(buf, 0xFFFF));

	WRITE_LEN(buf);
	return MEM_OK;
}


static memerr write_varying_set(output_buffer *buf, translation_unit *tu)
{
	size_t count_position;
	u32 count = 0;
	symbol_list *sl;
	WRITE_HEADER(buf, "SVAR");
	count_position = _essl_output_buffer_get_word_position(buf);
	ESSL_CHECK(APPEND_INT32(buf, 0));
	for(sl = tu->vertex_varyings ? tu->vertex_varyings : tu->fragment_varyings; sl != 0; sl = sl->next)
	{
		symbol *s = sl->sym;
		if(s->address != -1 || s->address_space == ADDRESS_SPACE_VERTEX_VARYING) /* also write out unused vertex varyings so the linker can report correct errors */
		{
			ESSL_CHECK(write_varying(buf, tu->desc, s));
			++count;
		}

	}
	_essl_output_buffer_replace_bits(buf, count_position, 0, 32, count);
	
	WRITE_LEN(buf);
	return MEM_OK;
}

static memerr write_shader_binary(error_context *e_ctx, output_buffer *buf, translation_unit *tu, shader_binary_writer writer)
{
	WRITE_HEADER(buf, "DBIN");
	ESSL_CHECK_NONFATAL(writer(e_ctx, buf, tu));

	WRITE_LEN(buf);
	return MEM_OK;
}

static memerr write_pro_shader_func(error_context *e_ctx, output_buffer *buf, translation_unit *tu, symbol *sym, shader_binary_func_writer writer)
{
	WRITE_HEADER(buf, "DBIN");
	ESSL_CHECK_NONFATAL(writer(e_ctx, buf, tu, sym));

	WRITE_LEN(buf);
	return MEM_OK;
}

static memerr write_pro_shader_func_flags(output_buffer *buf, symbol *sym)
{
	WRITE_HEADER(buf, "VPRO");

	ESSL_CHECK(APPEND_INT32(buf, sym->opt.pilot.one_past_program_end_address));

	WRITE_LEN(buf);
	return MEM_OK;
}


static memerr write_pro_shader_binary(error_context *e_ctx, output_buffer *buf, translation_unit *tu, shader_binary_func_writer writer)
{
	symbol_list *sl;
	unsigned init_addr = tu->proactive_uniform_init_addr;
	unsigned num = LIST_LENGTH(tu->proactive_funcs);
	WRITE_HEADER(buf, "DPRO");
	assert(num > 0);
	ESSL_CHECK(APPEND_INT32(buf, num)); /* number of proactive shaders (right now only 1!!!)*/
	assert(tu->desc->kind == TARGET_VERTEX_SHADER ||
					((init_addr != 0) && (init_addr &((~init_addr) + 1)) == init_addr)); /* init_addr should be a power of 2 for fragment shaders*/
	ESSL_CHECK(APPEND_INT32(buf, init_addr)); /* uniform offset */
	ESSL_CHECK(APPEND_INT32(buf, init_addr)); /* uniform stride (equal to offset for Mali-400) */

	/* Here must be several DBIN blocks, one per a proactive shader, but for now we have only 1 proactive shader*/
	for(sl = tu->proactive_funcs; sl != NULL; sl = sl->next)
	{
		ESSL_CHECK(write_pro_shader_func(e_ctx, buf, tu, sl->sym, writer));
		if(tu->desc->kind == TARGET_VERTEX_SHADER)
		{
			ESSL_CHECK(write_pro_shader_func_flags(buf, sl->sym));
		}
	}

	WRITE_LEN(buf);
	return MEM_OK;
}

static memerr write_instruction_flags(output_buffer *buf, translation_unit *tu)
{
	if(tu->program_address.program_start_address == -1)
	{ 
		return MEM_OK; /* no useful program address to serialize */
	} else {
		WRITE_HEADER(buf, "FINS");

		ESSL_CHECK(APPEND_INT32(buf, tu->program_address.program_start_address));
		ESSL_CHECK(APPEND_INT32(buf, tu->program_address.one_past_program_end_address));
		ESSL_CHECK(APPEND_INT32(buf, tu->program_address.one_past_last_input_read_address));

		WRITE_LEN(buf);
		return MEM_OK;
	}
}


static memerr write_stack_flags(output_buffer *buf, translation_unit *tu)
{
	WRITE_HEADER(buf, "FSTA");

	ESSL_CHECK(APPEND_INT32(buf, tu->stack_flags.stack_size));
	ESSL_CHECK(APPEND_INT32(buf, tu->stack_flags.initial_stack_pointer));

	WRITE_LEN(buf);
	return MEM_OK;
}


static memerr write_discard_flags(output_buffer *buf, translation_unit *tu)
{
	WRITE_HEADER(buf, "FDIS");

	ESSL_CHECK(APPEND_INT32(buf, tu->discard_used));


	WRITE_LEN(buf);
	return MEM_OK;
}

static memerr write_buffer_usage_flags(output_buffer *buf, translation_unit *tu)
{
	WRITE_HEADER(buf, "FBUU");

	ESSL_CHECK(APPEND_INT8(buf, tu->buffer_usage.color_read));
	ESSL_CHECK(APPEND_INT8(buf, tu->buffer_usage.color_write));
	ESSL_CHECK(APPEND_INT8(buf, tu->buffer_usage.depth_read));
	ESSL_CHECK(APPEND_INT8(buf, tu->buffer_usage.depth_write));
	ESSL_CHECK(APPEND_INT8(buf, tu->buffer_usage.stencil_read));
	ESSL_CHECK(APPEND_INT8(buf, tu->buffer_usage.stencil_write));
	ESSL_CHECK(APPEND_INT16(buf, 0)); /* reserved */

	WRITE_LEN(buf);
	return MEM_OK;
}

static memerr write_register_usage(output_buffer *buf, translation_unit *tu)
{
	WRITE_HEADER(buf, "REGU");

	ESSL_CHECK(APPEND_INT16(buf, tu->reg_usage.n_work_registers));
	ESSL_CHECK(APPEND_INT16(buf, tu->reg_usage.n_uniform_registers));


	WRITE_LEN(buf);
	return MEM_OK;
}


static memerr write_opt_blocks(output_buffer *buf, translation_unit *tu)
{
	serializer_options opts = tu->desc->serializer_opts;
	if(opts & SERIALIZER_OPT_WRITE_STACK_USAGE)
	{
		ESSL_CHECK(write_stack_flags(buf, tu));
	}
	
	if(opts & SERIALIZER_OPT_WRITE_PROGRAM_ADDRESSES)
	{
		ESSL_CHECK(write_instruction_flags(buf, tu));
	}

	if(opts & SERIALIZER_OPT_WRITE_REGISTER_USAGE)
	{
		ESSL_CHECK(write_register_usage(buf, tu));
	}

	return MEM_OK;
}

static enum mali_cpu_operand_kind get_cpu_operand_kind(symbol *sym)
{
	switch(sym->address_space)
	{
		case ADDRESS_SPACE_UNIFORM:
			return OPND_VUNI;
		case ADDRESS_SPACE_ATTRIBUTE:
			return OPND_VATT;
		case ADDRESS_SPACE_VERTEX_VARYING:
		case ADDRESS_SPACE_FRAGMENT_VARYING:
			return OPND_VVAR;
		default:
			assert(0);
			return OPND_LVAR;
	}
}

static memerr write_cpu_attr_opnd_chunk(output_buffer *buf, symbol *sym, node *n)
{
	WRITE_HEADER(buf, "OPND");

	if(sym != NULL)
	{
		
		ESSL_CHECK(APPEND_INT8(buf, get_cpu_operand_kind(sym)));
		ESSL_CHECK(APPEND_INT8(buf, convert_basic_type_to_v0(sym->type)));
		ESSL_CHECK(APPEND_INT16(buf, 0)); /*padding*/
		ESSL_CHECK(APPEND_INT32(buf, sym->address));
		ESSL_CHECK(APPEND_INT32(buf, (u32)-1));
	}else
	{
		assert(n->hdr.kind == EXPR_KIND_CONSTANT);
		assert(GET_NODE_VEC_SIZE(n) == 1);
		ESSL_CHECK(APPEND_INT8(buf, OPND_CONST));
		ESSL_CHECK(APPEND_INT8(buf, convert_basic_type_to_v0(n->hdr.type)));
		ESSL_CHECK(APPEND_INT16(buf, 0)); /*padding*/
		ESSL_CHECK(APPEND_INT32(buf, (u32)-1));
		ESSL_CHECK(APPEND_INT32(buf, n->expr.u.value[0].interpreter_int));
	}

	WRITE_LEN(buf);

	return MEM_OK;
}

static enum mali_cpu_expression_kind get_expr_cpu_kind(node *n)
{
	switch(n->hdr.kind)
	{
		case EXPR_KIND_BUILTIN_CONSTRUCTOR:
			return EXPR_CONSTR;
		case EXPR_KIND_UNARY:
			if(n->expr.operation == EXPR_OP_SWIZZLE)
			{
				return EXPR_SWIZZLE;
			}else
			{
				assert(0);
				return EXPR_SWIZZLE;
			}
		case EXPR_KIND_BINARY:
			switch(n->expr.operation)
			{
				case EXPR_OP_ADD:
					return EXPR_ADD;
				case EXPR_OP_SUB:
					return EXPR_SUB;
				case EXPR_OP_MUL:
					return EXPR_MUL;
				case EXPR_OP_DIV:
					return EXPR_DIV;
				default:
					assert(0);
					return EXPR_ADD;
			}
		default:
			assert(0);
			return EXPR_INDEX;
	}
}

static enum mali_cpu_vec_size get_expr_cpu_size(const type_specifier *type)
{
	if(type->basic_type == TYPE_MATRIX_OF)
	{
		switch(GET_MATRIX_N_COLUMNS(type))
		{
			case 2:
				return EXPR_MAT_SIZE_2;
			case 3:
				return EXPR_MAT_SIZE_3;
			case 4:
				return EXPR_MAT_SIZE_4;
			default:
				assert(0);
				return EXPR_VEC_SIZE_1;
		}
	}else
	{
		return (enum mali_cpu_vec_size)GET_VEC_SIZE(type);
	}
}

static unsigned char get_identity_swizzle_cpu_mask(unsigned vec_size)
{
	unsigned i;
	unsigned char res = 0;
	for(i = 0; i < vec_size; i++)
	{
		res |= i << (2 * i);
	}

	return res;
}

static unsigned char get_swizzle_cpu_mask_from_swizzle(node *n)
{
	unsigned i;
	unsigned char res = 0;
	signed char idx;
	assert(n->hdr.kind == EXPR_KIND_UNARY &&
		n->expr.operation == EXPR_OP_SWIZZLE);
	for(i = 0; i < GET_NODE_VEC_SIZE(n); i++)
	{
		idx = n->expr.u.swizzle.indices[i];
		if(idx >= 0)
		{
			res |= idx << (2 * i);
		}
	}

	return res;
}

static unsigned char get_swizzle_cpu_mask(node *n)
{
	unsigned char res = 0;
	if(n->hdr.kind == EXPR_KIND_UNARY &&
		n->expr.operation == EXPR_OP_SWIZZLE)
	{
		res = get_swizzle_cpu_mask_from_swizzle(n);
	}else
	{
		unsigned sz;
		if(n->hdr.type->basic_type == TYPE_MATRIX_OF)
		{
			sz = GET_MATRIX_N_COLUMNS(n->hdr.type);
		}else
		{
			sz = GET_NODE_VEC_SIZE(n);
		}
		res = get_identity_swizzle_cpu_mask(sz);
	}
	return res;
}

static memerr write_cpu_attr_expr_chunk(output_buffer *buf, node *n)
{
	WRITE_HEADER(buf, "EXPR");
	
	ESSL_CHECK(APPEND_INT8(buf, get_expr_cpu_kind(n)));
	ESSL_CHECK(APPEND_INT8(buf, get_expr_cpu_size(n->hdr.type)));
	ESSL_CHECK(APPEND_INT8(buf, GET_N_CHILDREN(n)));
	ESSL_CHECK(APPEND_INT8(buf, get_swizzle_cpu_mask(n))); /* swizzle */

	WRITE_LEN(buf);

	return MEM_OK;
}

static memerr write_cpu_final_assign_expr_chunk(output_buffer *buf, symbol *sym)
{
	WRITE_HEADER(buf, "EXPR");
	
	ESSL_CHECK(APPEND_INT8(buf, EXPR_ASSIGN));
	ESSL_CHECK(APPEND_INT8(buf, get_expr_cpu_size(sym->type)));
	ESSL_CHECK(APPEND_INT8(buf, 2));
	ESSL_CHECK(APPEND_INT8(buf, get_identity_swizzle_cpu_mask(GET_VEC_SIZE(sym->type)))); /* swizzle */

	WRITE_LEN(buf);

	return MEM_OK;
}

static memerr write_cpu_final_assign_chunk(output_buffer *buf, symbol *sym)
{
	WRITE_HEADER(buf, "VACT");
	ESSL_CHECK(write_cpu_final_assign_expr_chunk(buf, sym));
	WRITE_LEN(buf);

	return MEM_OK;
}



static memerr write_cpu_act_chunk(output_buffer *buf, symbol *sym, node *n, essl_bool is_expr)
{
	WRITE_HEADER(buf, "VACT");

	if(is_expr)
	{
		assert(n != NULL);
		ESSL_CHECK(write_cpu_attr_expr_chunk(buf, n));
	}else
	{
		assert(sym != NULL || n != NULL);
		ESSL_CHECK(write_cpu_attr_opnd_chunk(buf, sym, n));
	}
	WRITE_LEN(buf);

	return MEM_OK;
}

static memerr write_cpu_sym_body(output_buffer *buf, node *n)
{
	node *child;
	unsigned i;
	for(i = 0; i < GET_N_CHILDREN(n); i++)
	{
		ESSL_CHECK(child = GET_CHILD(n, i));
		ESSL_CHECK(write_cpu_sym_body(buf, child));
	}
	if(n->hdr.kind == EXPR_KIND_VARIABLE_REFERENCE)
	{
		ESSL_CHECK(write_cpu_act_chunk(buf, n->expr.u.sym, NULL, ESSL_FALSE));
	}else if(n->hdr.kind == EXPR_KIND_CONSTANT)
	{
		ESSL_CHECK(write_cpu_act_chunk(buf, NULL, n, ESSL_FALSE));
	}else
	{
		ESSL_CHECK(write_cpu_act_chunk(buf, NULL, n, ESSL_TRUE));
	}

	return MEM_OK;

}

static memerr write_cpu_attributes_chunk(output_buffer *buf, translation_unit *tu)
{
	symbol_list *sl;
	WRITE_HEADER(buf, "VSOP");
	for(sl = tu->cpu_processed; sl != 0; sl = sl->next)
	{
		symbol *s = sl->sym;
		if(s->body != NULL)
		{
			ESSL_CHECK(write_cpu_act_chunk(buf, s, NULL, ESSL_FALSE));
			ESSL_CHECK(write_cpu_sym_body(buf, s->body));
			ESSL_CHECK(write_cpu_final_assign_chunk(buf, s));
		}

	}
	
	WRITE_LEN(buf);
	return MEM_OK;
}

static memerr write_cpu_attributes(output_buffer *buf, translation_unit *tu)
{
	if(tu->cpu_processed != NULL)
	{
		ESSL_CHECK(write_cpu_attributes_chunk(buf, tu));
	}
	return MEM_OK;
}
static memerr write_fragment_shader(mempool *pool, error_context *e_ctx, output_buffer *buf, translation_unit *tu, 
						shader_binary_writer writer,
						shader_binary_func_writer func_writer)
{
	WRITE_HEADER(buf, "CFRA");
	IGNORE_PARAM(pool);

	ESSL_CHECK(APPEND_INT32(buf, tu->desc->core));

	ESSL_CHECK(write_opt_blocks(buf, tu));

	ESSL_CHECK(write_discard_flags(buf, tu));
	ESSL_CHECK(write_buffer_usage_flags(buf, tu));

	ESSL_CHECK(write_uniform_set(buf, tu));
	ESSL_CHECK(write_varying_set(buf, tu));

	ESSL_CHECK_NONFATAL(write_shader_binary(e_ctx, buf, tu, writer));

	if(tu->proactive_funcs != NULL)
	{
		ESSL_CHECK_NONFATAL(write_pro_shader_binary(e_ctx, buf, tu, func_writer));
	}

	WRITE_LEN(buf);
	return MEM_OK;
}



static memerr write_vertex_shader(mempool *pool, error_context *e_ctx, output_buffer *buf, translation_unit *tu, 
							shader_binary_writer writer,
							shader_binary_func_writer func_writer)
{
	WRITE_HEADER(buf, "CVER");
	IGNORE_PARAM(pool);

	ESSL_CHECK(APPEND_INT32(buf, tu->desc->core));

	ESSL_CHECK(write_opt_blocks(buf, tu));
	ESSL_CHECK(write_uniform_set(buf, tu));
	ESSL_CHECK(write_attribute_set(buf, tu));
	ESSL_CHECK(write_varying_set(buf, tu));

	ESSL_CHECK_NONFATAL(write_shader_binary(e_ctx, buf, tu, writer));

	if(tu->proactive_funcs != NULL)
	{
		ESSL_CHECK_NONFATAL(write_pro_shader_binary(e_ctx, buf, tu, func_writer));
	}

	ESSL_CHECK(write_cpu_attributes(buf, tu));
	WRITE_LEN(buf);
	return MEM_OK;
}

memerr _essl_serialize_translation_unit(mempool *pool, error_context *e_ctx, output_buffer *buf, 
									translation_unit *tu, 
									shader_binary_writer writer, 
									shader_binary_func_writer func_writer,
									serializer_endianness endianness)
{
	WRITE_HEADER(buf, "MBS1");

	switch(tu->desc->kind)
	{
	case TARGET_UNKNOWN:
		assert(0);
		break;

	case TARGET_VERTEX_SHADER:
		ESSL_CHECK_NONFATAL(write_vertex_shader(pool, e_ctx, buf, tu, writer, func_writer));
		break;
	case TARGET_FRAGMENT_SHADER:
		ESSL_CHECK_NONFATAL(write_fragment_shader(pool, e_ctx, buf, tu, writer, func_writer));
		break;
	}

	WRITE_LEN(buf);
	if(endianness == SERIALIZER_LITTLE_ENDIAN)
	{
		_essl_output_buffer_native_to_le_byteswap(buf);
	}
	return MEM_OK;
}
