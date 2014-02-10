/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
 
#define NEEDS_STDIO  /* for snprintf */

#include "common/essl_system.h"
#include "common/type_name.h"

const char* _essl_get_type_name(mempool *pool, const type_specifier* type)
{
	size_t buf_size;
	char* str;

	if (type->basic_type == TYPE_STRUCT)
	{
		if (type->name.len > 0)
		{
			/* longest name can now be struct name + [2147483647] */
			buf_size = type->name.len + 13;
		}
		else
		{
			/* longest name can now be "unnamed struct[2147483647]" */
			buf_size = 27;
		}
	}
	else
	{
		/* longenst name can now be "samplerShadownD[2147483647]" */
		buf_size = 28;
	}

	ESSL_CHECK(str = _essl_mempool_alloc(pool, buf_size));

	switch (type->basic_type)
	{
	case TYPE_VOID:
		snprintf(str, buf_size, "void");
		break;
	case TYPE_FLOAT:
		if (type->u.basic.vec_size == 1)
		{
			snprintf(str, buf_size, "float");
		}
		else
		{
			snprintf(str, buf_size, "vec%u", type->u.basic.vec_size);
		}
		break;
	case TYPE_INT:
		if (type->u.basic.vec_size == 1)
		{
			snprintf(str, buf_size, "int");
		}
		else
		{
			snprintf(str, buf_size, "ivec%u", type->u.basic.vec_size);
		}
		break;
	case TYPE_BOOL:
		if (type->u.basic.vec_size == 1)
		{
			snprintf(str, buf_size, "bool");
		}
		else
		{
			snprintf(str, buf_size, "bvec%u", type->u.basic.vec_size);
		}
		break;
	case TYPE_MATRIX_OF:
	{
		unsigned n_columns = GET_MATRIX_N_COLUMNS(type);
		unsigned n_rows = GET_MATRIX_N_ROWS(type);
		if(n_columns == n_rows)
		{
			snprintf(str, buf_size, "mat%u", n_columns);
		} else {
			snprintf(str, buf_size, "mat%ux%u", n_rows, n_columns);
		}
		break;
	}
	case TYPE_SAMPLER_2D:
		snprintf(str, buf_size, "sampler2D");
		break;
	case TYPE_SAMPLER_3D:
		snprintf(str, buf_size, "sampler3D");
		break;
	case TYPE_SAMPLER_CUBE:
		snprintf(str, buf_size, "samplerCube");
		break;
	case TYPE_SAMPLER_EXTERNAL:
		snprintf(str, buf_size, "samplerExternalOES");
		break;
	case TYPE_SAMPLER_2D_SHADOW:
		snprintf(str, buf_size, "sampler2DShadow");
		break;
	case TYPE_STRUCT:
		if (type->name.len > 0)
		{
			memcpy(str, "struct ", 7);
			memcpy(str + 7, type->name.ptr, type->name.len);
			str[7 + type->name.len] = '\0';
		}
		else
		{
			memcpy(str, "unnamed struct", 15);
		}
		break;
	case TYPE_ARRAY_OF:
		{
			const char *child_str = _essl_get_type_name(pool, type->child_type);
			snprintf(str, buf_size, "%s[%u]", child_str, type->u.array_size);
		}
		break;

	default:
		assert(0);
		break;
	};


	return str;
}
