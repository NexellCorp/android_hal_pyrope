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

#include "common/essl_node.h"
#include "backend/arithmetic.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif


scalar_type _essl_backend_constant_fold(const type_specifier *type, expression_operator op, scalar_type sa, scalar_type sb, scalar_type sc)
{
	float res = 0.0, a = sa.mali200_float, b = sb.mali200_float, c = sc.mali200_float;
	scalar_type sres;
	switch(op)
	{
	case EXPR_OP_NOT: 
		res = (float)(a == 0);
		break;
	case EXPR_OP_NEGATE:
		res = -a;
		break;
	case EXPR_OP_PLUS:
		res = a;
		break;


	case EXPR_OP_ADD:
		res = a + b;
		break;

	case EXPR_OP_SUB:
		res = a - b;
		break;
	case EXPR_OP_MUL:
		res = a * b;
		break;
	case EXPR_OP_DIV:
		if (type != 0 && type->basic_type == TYPE_INT)
		{
			res = (float)((int)(a / b));
		}
		else
		{
			res = a / b;
		}
		break;

	case EXPR_OP_LT:
		res = (float)(a < b);
		break;
	case EXPR_OP_LE:
		res = (float)(a <= b);
		break;
	case EXPR_OP_EQ:
		res = (float)(a == b);
		break;
	case EXPR_OP_NE:
		res = (float)(a != b);
		break;
	case EXPR_OP_GE:
		res = (float)(a >= b);
		break;
	case EXPR_OP_GT:
		res = (float)(a > b);
		break;

	case EXPR_OP_LOGICAL_AND:
		res = (float)(a != 0 && b != 0);
		break;
	case EXPR_OP_LOGICAL_OR:
		res = (float)(a != 0 || b != 0);
		break;
	case EXPR_OP_LOGICAL_XOR:
		res = (float)((a != 0) ^ (b != 0));
		break;

	case EXPR_OP_FUN_RADIANS:
		res = (float)(a * (M_PI / 180.0));
		break;
	case EXPR_OP_FUN_DEGREES:
		res = (float)(a * (180.0 / M_PI));
		break;
	case EXPR_OP_FUN_SIN:
		res = (float)sin(a);
		break;
	case EXPR_OP_FUN_COS:
		res = (float)cos(a);
		break;
	case EXPR_OP_FUN_TAN:
		res = (float)tan(a);
		break;
	case EXPR_OP_FUN_ASIN:
		res = (float)asin(a);
		break;
	case EXPR_OP_FUN_ACOS:
		res = (float)acos(a);
		break;

	case EXPR_OP_FUN_EXP:
		res = (float)exp(a);
		break;
	case EXPR_OP_FUN_LOG:
		res = (float)log(a);
		break;
	case EXPR_OP_FUN_EXP2:
		res = (float)exp(a * M_LN2);
		break;
	case EXPR_OP_FUN_LOG2:
		res = (float)(log(a) / M_LN2);
		break;
	case EXPR_OP_FUN_SQRT:
		res = (float)sqrt(a);
		break;
	case EXPR_OP_FUN_INVERSESQRT:
		res = 1.0f /(float)sqrt(a);
		break;
	case EXPR_OP_FUN_ABS:
		res = (float)fabs(a);
		break;
	case EXPR_OP_FUN_SIGN:
		if (a < 0)
		{
			res = -1.0;
		}
		else if (a > 0)
		{
			res = 1.0;
		}
		else
		{
			res = 0.0;
		}
		break;
	case EXPR_OP_FUN_FLOOR:
		res = (float)floor(a);
		break;
	case EXPR_OP_FUN_CEIL:
		res = (float)ceil(a);
		break;
	case EXPR_OP_FUN_FRACT:
		res = a - (float)floor(a);
		break;
	case EXPR_OP_FUN_NOT:
		res = a != 0.0f ? 0.0f : 1.0f;
		break;
	case EXPR_OP_FUN_DFDX:
		res = 0.0;
		break;
	case EXPR_OP_FUN_DFDY:
		res = 0.0;
		break;
	case EXPR_OP_FUN_FWIDTH:
		res = 0.0;
		break;

	case EXPR_OP_FUN_ATAN:
		res = (float)atan2(a, b);
		break;
	case EXPR_OP_FUN_POW:
		res = (float)pow(a, b);
		break;
	case EXPR_OP_FUN_MOD:
		res = a - b * (float)floor(a / b);

		break;
	case EXPR_OP_FUN_MIN:
		/* min(nan, x) = x */
		if(a != a) res = b;
		else if(b != b) res = a;
		else res = b < a ? b : a;
		break;
	case EXPR_OP_FUN_MAX:
		/* max(nan, x) = x */
		if(a != a) res = b;
		else if(b != b) res = a;
		else res = a < b ? b : a;
		break;
	case EXPR_OP_FUN_STEP:
		res = b < a ? 0.0f : 1.0f;
		break;
	case EXPR_OP_FUN_LESSTHAN:
		res = a < b ? 1.0f : 0.0f;
		break;
	case EXPR_OP_FUN_LESSTHANEQUAL:
		res = a <= b ? 1.0f : 0.0f;
		break;
	case EXPR_OP_FUN_GREATERTHAN:
		res = a > b ? 1.0f : 0.0f;
		break;
	case EXPR_OP_FUN_GREATERTHANEQUAL:
		res = a >= b ? 1.0f : 0.0f;
		break;
	case EXPR_OP_FUN_EQUAL:
		res = a == b ? 1.0f : 0.0f;
		break;
	case EXPR_OP_FUN_NOTEQUAL:
		res = a != b ? 1.0f : 0.0f;
		break;

	case EXPR_OP_FUN_CLAMP:
		if (a < b)
		{
			res = b;
		}
		else if (a > c)
		{
			res = c;
		}
		else
		{
			res = a;
		}
		break;
	case EXPR_OP_FUN_MIX:
		res = a * (1 - c) + b * c;
		break;
	case EXPR_OP_FUN_SMOOTHSTEP:
		res = (c - a) / (b - a);
		if (res < 0) res = 0;
		else if (res > 1) res = 1;
		res = res * res * (3 - 2 * res);

		break;
	case EXPR_OP_FUN_RCP:
		res = 1.0f / a;
		break;

	default:
		assert(0); /* please don't give us invalid ops */
		break;
		
	}
	sres.mali200_float = res;
	return sres;
}

void _essl_backend_constant_fold_sized( expression_operator op, scalar_type *ret, unsigned int size, scalar_type *sa, scalar_type *sb, scalar_type *sc, const type_specifier *atype, const type_specifier *btype)
{
	switch(op)
	{
	case EXPR_OP_EQ:
	case EXPR_OP_NE:
		{
			unsigned int equal = 1;
			unsigned int i;
			for (i = 0; i < size; ++i)
			{
				if (sa[i].mali200_float != sb[i].mali200_float)
				{
					equal = 0;
				}
			}
			if ((op == EXPR_OP_EQ && equal == 1) || (op == EXPR_OP_NE && equal == 0))
			{
				ret[0].mali200_float = 1.0;
			}
			else
			{
				ret[0].mali200_float = 0.0;
			}
		}
		break;
	case EXPR_OP_FUN_LENGTH:
	case EXPR_OP_FUN_NORMALIZE:
		{
			float len = 0;
			unsigned int i;
			for (i = 0; i < size; ++i)
			{
				float v = sa[i].mali200_float;
				len += v * v;
			}
			len = (float)sqrt(len);
			if (op == EXPR_OP_FUN_LENGTH)
			{
				ret[0].mali200_float = len;
			}
			else
			{
				for (i = 0; i < size; ++i)
				{
					ret[i].mali200_float = sa[i].mali200_float / len;
				}
			}
		}
		break;

	case EXPR_OP_FUN_ANY:
	case EXPR_OP_FUN_ALL:
		{
			unsigned int i;
			float res = (op == EXPR_OP_FUN_ANY) ? 0.0f : 1.0f;
			for (i = 0; i < size; ++i)
			{
				float v = sa[i].mali200_float;
				if (v != res)
				{
					ret[0].mali200_float = v;
					return;
				}
			}
			ret[0].mali200_float = res;
		}
		break;

	case EXPR_OP_FUN_DISTANCE:
		{
			float len = 0;
			unsigned int i;
			assert(sb != 0);
			for (i = 0; i < size; ++i)
			{
				float v = sa[i].mali200_float - sb[i].mali200_float;
				len += v * v;
			}
			ret[0].mali200_float = (float)sqrt(len);
		}
		break;
	case EXPR_OP_FUN_DOT:
		{
			float sum = 0;
			unsigned int i;
			assert(sb != 0);
			for (i = 0; i < size; ++i)
			{
				sum += sa[i].mali200_float * sb[i].mali200_float;
			}
			ret[0].mali200_float = sum;
		}
		break;
	case EXPR_OP_FUN_CROSS:
		{
			float x0, x1, x2, y0, y1, y2;
			assert(size == 3);
			assert(sb != 0);
			x0 = sa[0].mali200_float;
			x1 = sa[1].mali200_float;
			x2 = sa[2].mali200_float;
			y0 = sb[0].mali200_float;
			y1 = sb[1].mali200_float;
			y2 = sb[2].mali200_float;
			ret[0].mali200_float = x1 * y2 - y1 * x2;
			ret[1].mali200_float = x2 * y0 - y2 * x0;
			ret[2].mali200_float = x0 * y1 - y0 * x1;
		}
		break;
	case EXPR_OP_FUN_FACEFORWARD:
		{
			float sum = 0;
			unsigned int i;
			assert(sb != 0);
			assert(sc != 0);
			for (i = 0; i < size; ++i)
			{
				sum += sc[i].mali200_float * sb[i].mali200_float;
			}
			if (sum < 0)
			{
				for (i = 0; i < size; ++i)
				{
					ret[i].mali200_float = sa[i].mali200_float;
				}
			}
			else
			{
				for (i = 0; i < size; ++i)
				{
					ret[i].mali200_float = -sa[i].mali200_float;
				}
			}
		}
		break;
	case EXPR_OP_FUN_REFLECT:
		{
			float sum = 0;
			unsigned int i;
			assert(sb != 0);
			for (i = 0; i < size; ++i)
			{
				sum += sb[i].mali200_float * sa[i].mali200_float;
			}
			for (i = 0; i < size; ++i)
			{
				ret[i].mali200_float = sa[i].mali200_float - 2 * sum * sb[i].mali200_float;
			}
		}
		break;
	case EXPR_OP_FUN_REFRACT:
		{
			float sum = 0;
			unsigned int i;
			float k;
			float eta;
			assert(sb != 0);
			assert(sc != 0);
			eta = sc[0].mali200_float;
			for (i = 0; i < size; ++i)
			{
				sum += sb[i].mali200_float * sa[i].mali200_float;
			}
			k = 1.0f - eta * eta * (1.0f - sum * sum);
			if (k < 0.0)
			{
				for (i = 0; i < size; ++i)
				{
					ret[i].mali200_float = 0;
				}
			}
			else
			{
				k = (float)sqrt(k);
				for (i = 0; i < size; ++i)
				{
					ret[i].mali200_float = eta * sa[i].mali200_float - (eta * sum + k) * sb[i].mali200_float;
				}
			}
		}
		break;
	case EXPR_OP_FUN_MATRIXCOMPMULT:
		{
			unsigned int i;
			assert(sb != 0);
			for (i = 0; i < size; ++i)
			{
				ret[i].mali200_float = sa[i].mali200_float * sb[i].mali200_float;
			}
		}
		break;
	case EXPR_OP_MUL:
	    {
			/* either matrix-matrix, vector-matrix or matrix-vector multiply */
			assert(sa != 0);
			assert(sb != 0);
			assert(atype != 0);
			assert(btype != 0);
			if(atype->basic_type == TYPE_MATRIX_OF && btype->basic_type == TYPE_MATRIX_OF)
			{
				unsigned i, j, k;
				unsigned A_n_rows = GET_MATRIX_N_ROWS(atype);
				unsigned A_n_columns = GET_MATRIX_N_COLUMNS(atype);
				unsigned B_n_rows = GET_MATRIX_N_ROWS(btype);
				unsigned B_n_columns = GET_MATRIX_N_COLUMNS(btype);
				assert(A_n_columns == B_n_rows);

				/* matrix*matrix */
				for(i = 0; i < B_n_columns; ++i)
				{
					unsigned B_idx = i*B_n_rows;
					unsigned res_idx = i*A_n_rows;
					for(j = 0; j < A_n_rows; ++j)
					{
						float acc = 0.0;
						for(k = 0; k < A_n_columns; ++k)
						{
							acc += sa[k*A_n_rows+j].mali200_float * sb[B_idx+k].mali200_float;

						}
						ret[res_idx+j].mali200_float = acc;
					}
				}

			} else if(atype->basic_type == TYPE_MATRIX_OF)
			{
				unsigned i, j;
				unsigned mat_n_rows = GET_MATRIX_N_ROWS(atype);
				unsigned mat_n_columns = GET_MATRIX_N_COLUMNS(atype);
				assert(mat_n_columns == GET_VEC_SIZE(btype));
				/* matrix*vector */
				for(i = 0; i < mat_n_rows; ++i)
				{
					float acc = 0.0;
					for(j = 0; j < mat_n_columns; ++j)
					{
						acc += sa[j*mat_n_rows+i].mali200_float * sb[j].mali200_float;
					}
					ret[i].mali200_float = acc;
				}

			} else if(btype->basic_type == TYPE_MATRIX_OF)
			{
				unsigned i, j;
				unsigned mat_n_rows = GET_MATRIX_N_ROWS(btype);
				unsigned mat_n_columns = GET_MATRIX_N_COLUMNS(btype);

				assert(GET_VEC_SIZE(atype) == mat_n_rows);

				/* vector*matrix */
				for(i = 0; i < mat_n_columns; ++i)
				{
					unsigned idx = i*mat_n_rows;
					float acc = 0.0;
					for(j = 0; j < mat_n_rows; ++j)
					{
						acc += sa[j].mali200_float * sb[idx+j].mali200_float;
					}
					ret[i].mali200_float = acc;
				}
			} else
			{
				assert(0);
			}
			

		}
		break;

	default:
		assert(0); /* please don't give us invalid ops */
		break;
		
	}
}

scalar_type _essl_backend_float_to_scalar(float a)
{
	scalar_type s;
	s.mali200_float = a;
	return s;
}
scalar_type _essl_backend_int_to_scalar(int a)
{
	scalar_type s;
	s.mali200_float = (float)a;
	return s;
}
scalar_type _essl_backend_bool_to_scalar(int a)
{
	scalar_type s;
	s.mali200_float = (a!=0) ? 1.0f : 0.0f;
	return s;
}
float _essl_backend_scalar_to_float(scalar_type a)
{
	return a.mali200_float;
}
int _essl_backend_scalar_to_int(scalar_type a)
{
	return (int)a.mali200_float;
}
int _essl_backend_scalar_to_bool(scalar_type a)
{
	return (int)(a.mali200_float != 0.0);
}
scalar_type _essl_backend_convert_scalar(const type_specifier *returntype, scalar_type value)
{
	if (returntype->basic_type == TYPE_INT)
	{
		scalar_type s;
		s.mali200_float = (float)((int)value.mali200_float);
		return s;
	} else if(returntype->basic_type == TYPE_BOOL)
	{
		scalar_type s;
		s.mali200_float = value.mali200_float != 0.0f ? 1.0f : 0.0f;
		return s;

	}
	else
	{
		return value;
	}
}


/*
count leading zeroes functions. Only used when the input is nonzero.
*/

#if defined(__GNUC__) && (defined(__i386) || defined(__amd64))
#elif defined(__arm__) && defined(__ARMCC_VERSION)
#elif defined(__arm__) && defined(__GNUC__)
#else
/*
table used for the slow default versions.
*/
static const unsigned char clz_table[256] =
    {
    8,7,6,6,5,5,5,5,4,4,4,4,4,4,4,4,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    };
#endif


/*
32-bit count-leading-zeroes function: use the Assembly instruction
whenever possible.
*/
unsigned int _essl_clz32( unsigned int inp )
    {
#if defined(__GNUC__) && (defined(__i386) || defined(__amd64))
    unsigned int bsr;
    __asm__( "bsrl %1, %0" : "=r"(bsr) : "r"(inp | 1) );
    return 31-bsr;
#else
#if defined(__arm__) && defined(__ARMCC_VERSION)
  return __clz(inp); /* armcc builtin */
#else
#if defined(__arm__) && defined(__GNUC__)
    /* this variant will not compile for Thumb 1
    softfloat_uint32_t lz;
    __asm__( "clz %0, %1" : "=r"(lz) : "r"(inp) );
    return lz;
    */
    /* this will possibly not work with rather old versions
     * of gcc and not work well for Thumb 1 */
    return __builtin_clz(inp);
#else
    /* slow default version */
    unsigned int summa = 24;
    if( inp >= (0x10000U) ) { inp >>= 16; summa -= 16; }
    if( inp >= (0x100U) ) { inp >>= 8; summa -= 8; }
    return summa + clz_table[inp];
#endif
#endif
#endif
    }



#ifdef UNIT_TEST


int eq(scalar_type a, float b)
{
	return fabs(_essl_backend_scalar_to_float(a) - b) < 1e-7;
}


int main(void)
{
	scalar_type a = _essl_backend_float_to_scalar(3.5);
	scalar_type b = _essl_backend_float_to_scalar(2.5);
	scalar_type c = _essl_backend_float_to_scalar(0.0);
	scalar_type d = _essl_backend_float_to_scalar(-1.0);
	type_specifier *type = 0;
	assert(eq(_essl_backend_constant_fold(type, EXPR_OP_NOT, c, b, b), 1.0));
	assert(eq(_essl_backend_constant_fold(type, EXPR_OP_NEGATE, a, b, b), -3.5));
	assert(eq(_essl_backend_constant_fold(type, EXPR_OP_FUN_ABS, d, b, b), 1.0));

	assert(eq(_essl_backend_constant_fold(type, EXPR_OP_ADD, a, b, b), 6.0));
	assert(eq(_essl_backend_constant_fold(type, EXPR_OP_SUB, a, b, b), 1.0));
	assert(eq(_essl_backend_constant_fold(type, EXPR_OP_MUL, a, b, b), 8.75));
	assert(eq(_essl_backend_constant_fold(type, EXPR_OP_DIV, a, b, b), 1.4f));
	assert(eq(_essl_backend_constant_fold(type, EXPR_OP_ADD, a, b, b), 6.0));


	assert(eq(_essl_backend_constant_fold(type, EXPR_OP_LT, a, b, b), 0.0));
	assert(eq(_essl_backend_constant_fold(type, EXPR_OP_LE, a, b, b), 0.0));
	assert(eq(_essl_backend_constant_fold(type, EXPR_OP_EQ, a, b, b), 0.0));
	assert(eq(_essl_backend_constant_fold(type, EXPR_OP_NE, a, b, b), 1.0));
	assert(eq(_essl_backend_constant_fold(type, EXPR_OP_GE, a, b, b), 1.0));
	assert(eq(_essl_backend_constant_fold(type, EXPR_OP_GT, a, b, b), 1.0));

	assert(eq(_essl_backend_constant_fold(type, EXPR_OP_LOGICAL_AND, a, c, b), 0.0));
	assert(eq(_essl_backend_constant_fold(type, EXPR_OP_LOGICAL_OR, a, c, b), 1.0));
	assert(eq(_essl_backend_constant_fold(type, EXPR_OP_LOGICAL_XOR, a, c, b), 1.0));

	fprintf(stderr, "All tests OK!\n");
	return 0;
}



#endif /* UNIT_TEST */
