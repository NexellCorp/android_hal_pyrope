/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2009, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#include <stdio.h>
#include <ctype.h>
#include "common/essl_common.h"
#include "frontend/data_type.h"
/* This file contains an implementation of functionality similar to what is done by 'strtod'
 *
 * Ported from C++
 */

/* FP64 description */
#define FP64_BITS 11
#define FP64_DECIMALS 52


typedef unsigned int		u32_t;
typedef signed int 			s32_t;
typedef unsigned long long	u64_t;
typedef signed long long	s64_t;

typedef struct frontend_bigint
{
	u32_t *data;
	unsigned size;
} frontend_bigint;

/**
 * Copy from src to dst
 */
static memerr bigint_copy(mempool *pool, frontend_bigint *src, frontend_bigint *dst)
{
	unsigned i;

	if(dst->size < src->size)
	{
		ESSL_CHECK(dst->data = _essl_mempool_alloc(pool,  (src->size) * sizeof(u32_t)));
	}
	dst->size = src->size;
	for(i = 0; i < src->size; i++)
	{
		dst->data[i] = src->data[i];
	}

	return MEM_OK;
}

static memerr bigint_resize(mempool *pool, frontend_bigint *bigint, unsigned new_size)
{
	unsigned int i;

	if(new_size != bigint->size)
	{
		if(new_size > bigint->size)
		{
			u32_t *new_data_ptr;
			ESSL_CHECK(new_data_ptr =  _essl_mempool_alloc(pool, new_size * sizeof(u32_t)));
			for(i = 0; i < bigint->size; i++)
			{
				new_data_ptr[i] = bigint->data[i];
			}
			bigint->data = new_data_ptr;
		}else
		{
			for(i = new_size; i < bigint->size; i++)
			{
				bigint->data[i] = 0;
			}
		}
	}
	bigint->size = new_size;

	return MEM_OK;
}

/**
 * Helper function that truncates mantissa to contain only the necessary words.
 */
static memerr bigint_trunc(mempool *pool, frontend_bigint *bigint)
{
	/* Can the result be truncated?*/
	u32_t trunc = 0;
	u32_t mr;
	int i;

	/* Current most significant word*/
	for ( i = bigint->size - 1; i > 0; i-- )
	{
		/* Current most significant word*/
		mr = bigint->data[i];
		if((mr == 0 && (bigint->data[i - 1] & 0x80000000) == 0) ||
		   (mr == 0xffffffff && (bigint->data[i - 1] & 0x80000000) != 0) )
		{
			trunc = i;
		}else
		{
			break;
		}
	}

	if(trunc != 0)
	{
		ESSL_CHECK(bigint_resize(pool,  bigint, trunc));
	}
	return MEM_OK;
}

static memerr bigint_init(mempool *pool, frontend_bigint *bigint, s64_t a)
{
	unsigned i;
	u64_t u = (u64_t)(a);

	ESSL_CHECK(bigint_resize(pool, bigint, 2));
	for(i = 0; i < bigint->size; i++)
	{
		bigint->data[i] = 0;
	}

	/* Is the value negative (This code is used in order not to assume the s64 bit-representation)?*/
	assert(a >= 0 );

	/* Set the words*/
	bigint->data[0] = (u32_t)( u & 0x00000000ffffffffULL );
	bigint->data[1] = (u32_t)( u >> 32 );

	/* Truncate*/
	ESSL_CHECK(bigint_trunc(pool, bigint));
	return MEM_OK;
}

/**
 * Returns the number of bits needed to represent the bigint value. Note that the bigint is considered
 * a signed 2's complement value, so the sign-bit is included in this value. The value 127 will therefore
 * return 8 bits as size.
 *
 * @return	The number of bits needed to represent the value.
 */
static u32_t bigint_bit_size(frontend_bigint *a)
{
	unsigned i;
	u32_t msw = a->data[a->size - 1];/* Most significant word*/

	/* Get the sign bit*/
	u32_t sgn = msw & 0x80000000;
	u32_t n = 0;/* Number of redundant bits in the most significant word*/

	/* Left-shift the msw so that bit 30 can be checked*/
	msw <<= 1;

	/* Find the first non-sign bit*/
	for(i = 0; i < 31 && (msw & 0x80000000) == sgn; i++ )
	{
		/* Count number of extra bits*/
		msw <<= 1;
		n++;
	}

	/* Return the number of bits in the value*/
	return 32*(a->size) - n;
}

static frontend_bigint * new_frontend_bigint(mempool *pool)
{
	frontend_bigint *res;
	ESSL_CHECK(res = _essl_mempool_alloc(pool, sizeof(frontend_bigint)));
	ESSL_CHECK(res->data = _essl_mempool_alloc(pool, 2 * sizeof(u32_t)));
	res->size = 2;
	return res;
}

/**
 * Shifts this left by the given number of bits.
 * bigint <<= b
 * Result is returned in bigint
 */
static frontend_bigint * bigint_lshift(mempool *pool, frontend_bigint *bigint, unsigned b)
{
	unsigned w;
	unsigned asize;
	unsigned r;
	unsigned sign;
	unsigned i,j;

	/* Compute the number of whole words to shift left*/
	w = b >> 5;

	/* Ensure that there is enough room in the result*/
	asize = bigint->size;
	ESSL_CHECK(bigint_resize(pool,   bigint, asize + w + 1 ));

	/* Compute the remainder of the shift amount*/
	r = b & 0x0000001f;

	/* Compute the sign*/
	sign = (bigint->data[asize - 1] & 0x80000000) != 0 ? 0xffffffff : 0;

	/* Shift by this number of bits*/
	i = asize;
	for(j = 0; j <= asize; j++)
	{
		bigint->data[i + w] = (u32_t)( ((i != asize ? bigint->data[i] : sign) << r) );
		if(i > 0 && r != 0)
		{
			bigint->data[i + w] |= (u32_t)( (bigint->data[i - 1] >> (32 - r)) );
		}
		i--;
	}

	/* Clear the shifted-in values*/
	for(i = 0; i < w; i++ )
	{
		bigint->data[i] = 0;
	}

	/* Truncate*/
	ESSL_CHECK(bigint_trunc(pool,bigint));

	return bigint;
}

/**
 * Shifts this right by the given number of bits. The shifting is arithmetic, preserving the sign bit.
 *
 * @param	pool Memory pool for allocations
 * @param	a	Input value
 * @param	b	The number of bits to shift right.
 * @return	inpute value after shifting.
 * Result is returned in a
 */
static frontend_bigint * bigint_rshift(mempool *pool, frontend_bigint *a, unsigned b)
{
	unsigned w, r, sign,hw;
	unsigned i;
	/* Special-case for right-shifting more than the width of a bigint*/
	assert(!((u64_t)(b) >= (u64_t)(a->size)*32 ));

	/* Compute the number of whole words to shift right*/
	w = b >> 5;

	/* Compute the remainder of the shift amount*/
	r = b & 0x0000001f;

	/* Compute the sign*/
	sign = (a->data[a->size - 1] & 0x80000000) != 0 ? 0xffffffff : 0;

	/* Shift by this number of bits*/
	for(i = w; i < a->size; i++)
	{
		a->data[i - w] = (u32_t)(a->data[i] >> r);
		hw = (i + 1) >= a->size ? sign : a->data[i + 1];
		if(r != 0)
		{
			a->data[i - w] |= (u32_t)( hw << (32 - r) );
		}
	}

	/*Resize the remove the words that are not needed any more*/
	ESSL_CHECK(bigint_resize(pool,  a, a->size - w ));

	/* Truncate*/
	ESSL_CHECK(bigint_trunc(pool,a));

	return a;
}


/**
 * Bigint or
 * Result is returned in a
 */
static frontend_bigint * bigint_or(mempool *pool, frontend_bigint *a, frontend_bigint * b)
{
	unsigned xa;
	unsigned xb;
	unsigned i;
	unsigned wa;
	unsigned wb;
	/* Compute the number of bits*/
	unsigned n = ESSL_MAX(a->size, b->size );

	/* Compute the signs of the inputs*/
	xa = (a->data[a->size - 1] & 0x80000000) ? 0xffffffff : 0;
	xb = (b->data[b->size - 1] & 0x80000000) ? 0xffffffff : 0;

	/* Resize the result*/
	ESSL_CHECK(bigint_resize(pool,  a, n));

	/* Loop through each word, oring it*/
	for(i = 0; i < n; i++)
	{
		wa = i >= a->size ? xa : a->data[i];
		wb = i >= b->size ? xb : b->data[i];

		/* Set the and result*/
		a->data[i] = wa | wb;
	}

	/* Truncate the result*/
	ESSL_CHECK(bigint_trunc(pool,a));

	return a;
}

/*
 * Result is returned in a
 */
static frontend_bigint * bigint_or_by_int(mempool *pool, frontend_bigint *a, int b)
{
	frontend_bigint *bi;

	ESSL_CHECK(bi = new_frontend_bigint(pool));

	ESSL_CHECK(bigint_init(pool, bi, b));

	ESSL_CHECK(a = bigint_or(pool, a, bi));
	return a;
}

/**
 * Unary invert.
 * Result is returned in a
 */
static void bigint_invert(frontend_bigint *a)
{
	unsigned i;
	/* Negate all the bits of the bigint*/
	for(i = 0; i < a->size; i++ )
	{
		a->data[i] = ~a->data[i];
	}
}


/**
 * Postfix increment. Increments the bigint with 1.
 *
 * Result is returned in a
 */
static memerr bigint_postfix_incr(frontend_bigint *a)
{
	unsigned i;
	u64_t sum;
	u64_t carry = 1;

	/* Loop through all words in the bigint*/
	for(i = 0; i < a->size; i++)
	{
		sum = (u64_t)(a->data[i] ) + carry;
		a->data[i] = (u32_t)( sum & 0xffffffffULL );
		carry = sum >> 32;
	}

	/* Is the carry nonzero? In this case we need to increase the size of the bigint*/
	assert ( carry == 0 );

	return MEM_OK;
}


/**
 * Unary negate.
 * Result is returned in a
 */
static memerr bigint_negate(frontend_bigint *a )
{
	bigint_invert(a);
	/* Add 1 to the result*/
	ESSL_CHECK(bigint_postfix_incr(a));
	return MEM_OK;
}


/**
 * Multiplies two bigints together.
 */
static frontend_bigint * bigint_mul(mempool *pool, frontend_bigint *a, frontend_bigint *b)
{
	frontend_bigint *mb, *ma;
	frontend_bigint *r;
	unsigned i, j, k;
	u64_t t;

	/* Are any of the parameters 0? */
	if(a->size == 1 && a->data[0] == 0)
	{
		return a;
	}

	/* Are any of the parameters negative?*/
	assert( (a->data[a->size - 1] & 0x80000000) == 0);
	ESSL_CHECK(ma = new_frontend_bigint(pool));
	ESSL_CHECK(bigint_copy(pool, a, ma));
	ESSL_CHECK(mb = new_frontend_bigint(pool));
	assert((b->data[b->size - 1] & 0x80000000) == 0);
	ESSL_CHECK(bigint_copy(pool, b, mb));

	/* Initialize the result bigint*/
	ESSL_CHECK(r = new_frontend_bigint(pool));
	ESSL_CHECK(bigint_resize(pool,   r, ma->size + mb->size ));

	/* Compute the multiplication*/
	for(j = 0; j < mb->size; j++)
	{
		k = 0;
		for(i = 0; i < ma->size; i++)
		{
			t = (u64_t)( ma->data[i] )*(u64_t)( mb->data[j] ) + (u64_t)( r->data[i + j] ) + (u64_t)( k );
			r->data[i + j] = (u32_t)( t & 0x00000000ffffffffULL );
			k = (u32_t)( t >> 32 );
		}
		r->data[j + ma->size] = k;
	}

	/* Truncate the result*/
	ESSL_CHECK(bigint_trunc(pool,r));

	/* Return the result*/
	return r;
}

static frontend_bigint * bigint_mul_by_int(mempool *pool, frontend_bigint *a, int b)
{
	frontend_bigint *res;
	frontend_bigint *bi;

	ESSL_CHECK(bi = new_frontend_bigint(pool));
	ESSL_CHECK(bigint_init(pool, bi, b));

	ESSL_CHECK(res = bigint_mul(pool, a, bi));
	return res;
}

/**
 * Adds a bigint to bigint.
 * Result is returned in a
 */
static memerr bigint_add(mempool *pool, frontend_bigint *a, frontend_bigint *b)
{
	u32_t xa, xb, ai, bi;
	u32_t sr;
	u32_t xr;
	u64_t sum;
	unsigned i;
	unsigned carry = 0;
	unsigned length = ESSL_MAX(a->size, b->size );

	ESSL_CHECK(bigint_resize(pool,  a, length ));

	/* Sign-extend a and b*/
	xa = (a->data[a->size - 1] & 0x80000000) ? 0xffffffff : 0;
	xb = (b->data[b->size - 1] & 0x80000000) ? 0xffffffff : 0;

	/* Loop through all words in the inputs*/
	for(i = 0; i < length; i++ )
	{
		ai = i >= a->size ? xa : a->data[i];
		bi = i >= b->size ? xb : b->data[i];

		/* Compute the sum*/
		sum = (u64_t)( ai ) + (u64_t)( bi ) + (u64_t)( carry );
		a->data[i] = (u32_t)( sum & 0xffffffff);

		/* Compute the carry out*/
		carry = (sum & 0xffffffff00000000ULL) != 0 ? 1 : 0;
	}

	/* Compute the unextended result sign*/
	sr = (a->data[a->size - 1] & 0x80000000) ? 0xffffffff : 0;

	/* Compute the extended result*/
	xr = (xa + xb + carry) & 0xffffffff;

	/* Does the result have to be extended?*/
	if ( xr != sr )
	{
		ESSL_CHECK(bigint_resize(pool,  a, a->size + 1 ));
		a->data[a->size - 1] = xr;
	}

	/* Truncate the result*/
	ESSL_CHECK(bigint_trunc(pool,a));
	return MEM_OK;
}

/**
 * Adds a bigint to integer.
 * Result is returned in a
 */
static memerr bigint_add_by_int(mempool *pool, frontend_bigint *a, int b)
{
	frontend_bigint *bi;

	ESSL_CHECK(bi = new_frontend_bigint(pool));

	ESSL_CHECK(bigint_init(pool,bi, b));

	ESSL_CHECK(bigint_add(pool, a, bi));

	return MEM_OK;
}


/**
 * Subtracts a bigint from bigint.
 * Result is returned in a
 */
static memerr bigint_sub(mempool *pool, frontend_bigint *a, frontend_bigint *b)
{
	/* make copy and negate b */
	frontend_bigint *negb;
	ESSL_CHECK(negb = new_frontend_bigint(pool));
	ESSL_CHECK(bigint_copy(pool, b, negb));
	ESSL_CHECK(bigint_negate(negb));

	ESSL_CHECK(bigint_add(pool, a, negb));

	return MEM_OK;
}


/**
 * Converts the bigint to a 64-bit unsigned integer if it is small enough. Negative values will not be converted.
 *
 * @param	a		input vaule
 * @param	value	Will receive the converted value.
 * @return	true if successful, false if the bigint is too large or negative.
 * In case of failure, the value variable will be unchanged.
 */
static essl_bool bigint_to_u64(frontend_bigint *a, u64_t * value)
{
	/* Is the bigint too large?*/
	assert(a->size <= 3 && !(a->size == 3 && a->data[2] != 0) );
	/* Is the value negative?*/
	assert(!(a->data[a->size - 1] & 0x80000000 ));

	/* Successful*/
	*value = (((u64_t)(a->data[1] ) << 32) | (u64_t)(a->data[0] ));
	return ESSL_TRUE;
}

/**
 * Compares a bigint 'a' to a bigint 'b' for equality.
 *
 * @param	a	First input value to compare
 * @param	b	Second input value to compare.
 * @return	true if this == b, false otherwise.
 */
static essl_bool bigint_equal( frontend_bigint *a, frontend_bigint *b )
{
	unsigned i;
	/* First compute the size of the bigints*/
	if(a->size != b->size )
	{
		return ESSL_FALSE;
	}

	/* Loop through each word of the bigints*/
	for(i = 0; i < a->size; i++ )
	{
		if(a->data[i] != b->data[i])
		{
			return ESSL_FALSE;
		}
	}

	/* The bigints are equal*/
	return ESSL_TRUE;
}

static memerr bigint_equal_to_int(mempool *pool, frontend_bigint *a, int b, essl_bool *res)
{
	frontend_bigint *bi;

	*res = ESSL_FALSE;
	ESSL_CHECK(bi = new_frontend_bigint(pool));
	ESSL_CHECK(bigint_init(pool,bi, b));

	*res = bigint_equal(a, bi);
	return MEM_OK;
}

/**
 * Compares bigints for greater than.
 */
static memerr bigint_gt(mempool *pool, frontend_bigint *a, frontend_bigint *b, essl_bool *res)
{
	/* Compute difference*/
	frontend_bigint *nega;
	frontend_bigint *cb;
	*res = ESSL_FALSE;
	ESSL_CHECK(nega = new_frontend_bigint(pool));
	ESSL_CHECK(cb = new_frontend_bigint(pool));
	ESSL_CHECK(bigint_copy(pool, a, nega));
	ESSL_CHECK(bigint_negate(nega));
	ESSL_CHECK(bigint_copy(pool, b, cb));
	ESSL_CHECK(bigint_add(pool, cb, nega));

	/* If the difference is negative, return true*/
	*res = (cb->data[cb->size - 1] & 0x80000000) != 0;
	return MEM_OK;
}


/**
 * Compares  bigints for greater than or equal.
 *
 * @param	pool	Memory pool for allocations
 * @param	a	Input value
 * @param	b	The bigint to compare to.
 * @param	res	result of comparison
 * @return	MEM_OK if all memory allocations are ok.
 */
static memerr bigint_ge(mempool *pool, frontend_bigint *a, frontend_bigint *b, essl_bool *res)
{
	/* Compute difference*/
	frontend_bigint *nega;
	frontend_bigint *cb;
	*res = ESSL_FALSE;
	ESSL_CHECK(nega = new_frontend_bigint(pool));
	ESSL_CHECK(cb = new_frontend_bigint(pool));
	ESSL_CHECK(bigint_copy(pool, a, nega));
	ESSL_CHECK(bigint_negate(nega));
	ESSL_CHECK(bigint_copy(pool, b, cb));
	ESSL_CHECK(bigint_add(pool, cb, nega));

	/* If the difference is negative or zero, return true*/
	*res = ((cb->data[cb->size - 1] & 0x80000000) != 0);
	return MEM_OK;
}



/**
 * Division/modulo algorithm. Division is done per bit, so it is rather slow.
 * A faster version based on built-in 32-bit divides should be used.
 *
 * @param	pool Memeory pool for allocations
 * @param	a	The dividend.
 * @param	b	The divisor.
 * @param	q	Will receive the quotient.
 * @param	r	Will receive the remainder.
 * @return	true if successful, false if dividing by zero.
 */
static essl_bool bigint_div_mod(mempool *pool,
								frontend_bigint *a,
								frontend_bigint *b,
								frontend_bigint *q,
								frontend_bigint *r )
{
	frontend_bigint *da, *db;
	/* Don't allow division by zero*/
	assert(!(b->size == 1 && b->data[0] == 0));

	ESSL_CHECK(da = new_frontend_bigint(pool));
	/* Are any of the parameters negative?*/
	assert((a->data[a->size - 1] & 0x80000000) == 0);
	ESSL_CHECK(bigint_copy(pool, a, da));
	ESSL_CHECK(db = new_frontend_bigint(pool));
	assert((b->data[b->size - 1] & 0x80000000) == 0);
	ESSL_CHECK(bigint_copy(pool, b, db));

	/* Perform division with a single-digit divisor*/
	if(db->size == 1)
	{
		unsigned i, j, k;
		u64_t div_const;
		/* Resize the quotient*/
		ESSL_CHECK(bigint_resize(pool,  q, da->size ));

		k = 0;
		for(i = 0; i < da->size; i++)
		{
			j = da->size - i - 1;

			/* Compute the digit of the quotient*/
			div_const = ((u64_t)( k ) << 32) + (u64_t)( da->data[j] );
			q->data[j] = (u32_t)( div_const / (u64_t)( db->data[0] ) );
			k = (u32_t)( div_const - (u64_t)( q->data[j] )*(u64_t)( db->data[0] ) );
		}

		/* Set the remainder*/
		ESSL_CHECK(bigint_resize(pool,  r, 1));
		r->data[0] = k;

		/* Truncate the quotient*/
		ESSL_CHECK(bigint_trunc(pool,q));
	}/* Perform a full division*/
	else
	{
		unsigned i;
		/* Number of bits of the result*/
		unsigned n = 1;
		essl_bool is_gt;

		/* Compute the shift-amount of the divisor*/
		s32_t sh = (s32_t)(bigint_bit_size(da)) - (s32_t)(bigint_bit_size(db));
		if(sh > 0)
		{
			ESSL_CHECK(bigint_lshift(pool,db, sh));
			n += sh;
		}

		ESSL_CHECK(bigint_gt(pool, db, da, &is_gt));

		/* Right-shift the divisor once if it is larger than the dividend*/
		if(is_gt)
		{
			ESSL_CHECK(bigint_rshift(pool,db, 1));
			n--;
		}

		/* Do the division*/
		ESSL_CHECK(bigint_init(pool,q, 0));
		for(i = 0; i < n; i++)
		{
			essl_bool is_ge;
			/* Left-shift quotient*/
			ESSL_CHECK(q  = bigint_lshift(pool, q, 1));

			ESSL_CHECK(bigint_ge(pool, da, db, &is_ge));
			/* Should a bit be set?*/
			if(is_ge)
			{
				ESSL_CHECK(bigint_sub(pool, da, db));
				ESSL_CHECK(bigint_or_by_int(pool, q, 1));
			}

			/* Right-shift divisor*/
			ESSL_CHECK(bigint_rshift(pool,db, 1));
		}

		/* Set remainder*/
		ESSL_CHECK(bigint_copy(pool, da, r));
	}

	/* Successfully divided*/
	return ESSL_TRUE;
}

/**
 * Function that does sticky right-shifting. The sticky bit is returned as a boolean.
 *
 * @param	pool		Memory pool for allocations
 * @param	a			input value
 * @param	sticky_bit	The sticky-bit after shifting.
 * @param	b			The number of bits to shift the value right.
 * @return	The result of right-shifting this by b bits. this is unchanged.
 */
static frontend_bigint * bigint_sticky_rshift(mempool *pool,
											frontend_bigint *a,
											essl_bool *sticky_bit,
											unsigned b)
{
	frontend_bigint *res;
	unsigned w, r, sign, hw;
	unsigned i, mask;
	/* Special-case for right-shifting more than the width of a bigint*/
	assert(!((u64_t)(b) >= (u64_t)(a->size)*32 ));
	/* Special-case for right-shifting by zero bits*/
	if(b == 0)
	{
		/* We have no sticky-bit in this case.*/
		*sticky_bit = ESSL_TRUE;
		return a;
	}

	/* Compute the number of whole words to shift right*/
	w = b >> 5;

	/* Ensure that there is enough room in the result*/
	ESSL_CHECK(res = new_frontend_bigint(pool));
	ESSL_CHECK(bigint_resize(pool,  res, a->size - w));

	/* Compute the remainder of the shift amount*/
	r = b & 0x0000001f;

	/* Compute the sign*/
	sign = (a->data[a->size - 1] & 0x80000000) != 0 ? 0xffffffff : 0;

	/* Set the sticky bit*/
	*sticky_bit = ESSL_FALSE;
	for(i = 0; i < w; i++)
	{
		*sticky_bit = (*sticky_bit) | (a->data[i] != 0);
	}

	/* Examine the last bits*/
	mask = (1 << r) - 1;
	*sticky_bit = (*sticky_bit) | (a->data[w] & mask);

	/* Shift by this number of bits*/
	for(i = w; i < a->size; i++)
	{
		res->data[i - w] |= (u32_t)(a->data[i] >> r);
		hw = (i + 1) >= a->size ? sign : a->data[i + 1];
		if(r != 0)
		{
			res->data[i - w] |= (u32_t)( hw << (32 - r));
		}
	}

	/*Truncate*/
	ESSL_CHECK(bigint_trunc(pool,res));

	/* Return the result*/
	return res;
}

/**
 * Compute the guard, round and odd bits
 */
static void get_RGObits_from_mantissa(frontend_bigint *mantissa, u32_t *r_bit, u32_t *g_bit, u32_t *o_bit)
{
	assert(mantissa->size > 0);
	*r_bit = (mantissa->data[0] >> 0) & 1;
	*g_bit = (mantissa->data[0] >> 1) & 1;
	*o_bit = (mantissa->data[0] >> 2) & 1;
}


/**
 * Helper function that computes the value 10^abs(exponent).
 * This value is used when changing the exponent from base 10 to base 2.
 *
 * @param	pool		Memory pool for allocations
 * @param	bexp		Will receive the bigint value 10^abs(exponent)
 * @param	exponent	The exponent value to use.
 */
static frontend_bigint * compute_exp(mempool *pool, frontend_bigint *bexp, s64_t exponent)
{
	frontend_bigint *ten;
	unsigned prev_bit;
	unsigned i;
	unsigned j;

	ESSL_CHECK(ten = new_frontend_bigint(pool));

	/* Initially set the bigint exponent to one*/
	ESSL_CHECK(bigint_init(pool,bexp, 1));

	/* Compute the absolute value of the exponent*/
	if(exponent < 0)
	{
		exponent = -exponent;
	}

	/* Loop through every bit in the exponent*/
	ESSL_CHECK(bigint_init(pool,ten, 10));
	prev_bit = 0;
	for(i = 0; i < 63; i++)
	{
		/* Is this bit set?*/
		if(exponent & (1LL << i))
		{
			/* Compute the multiplier for this value*/
			for(j = prev_bit; j < i; j++)
			{
				/* Multiply the multiplier*/
				ESSL_CHECK(ten = bigint_mul(pool, ten, ten));
				prev_bit = i;
			}

			/* Multiply the exponent with this multiplier*/
			ESSL_CHECK(bexp = bigint_mul(pool, bexp, ten));
		}
	}
	return bexp;
}


/**
 * Helper function which encodes a floating point value given a signed exponent and a high-precision mantissa.
 * The mantissa is treated as an integer. The encoded value will get a denormalized mantissa as necessary.
 *
 * @param	res				Resulting encoded bit-pattern
 * @param	exp				The exponent to use.
 * @param	mnt				The mantissa to encode.
 * @return	true if the value could be exactly represented in the target format, false if it had to be rounded or clamped.
 */
static essl_bool encode_float( u64_t *res, s64_t exp, u64_t mnt)
{
	essl_bool guard_bit, round_bit, sticky_bit;
	s32_t align_shift;
	s64_t exponent_offset;
	u64_t final_exp;
	u64_t mnt_mask;

	/* Is the value zero?*/
	assert ( mnt != 0 );

	/* Align-shift the value*/
	while( (mnt & 0x8000000000000000ULL) == 0 )
	{
		mnt <<= 1;
		exp--;
	}

	/* Right shift to the number of target mantissa bits, generating guard, round and sticky bits*/
	guard_bit = ESSL_FALSE;
	round_bit = ESSL_FALSE;
	sticky_bit = ESSL_FALSE;

	/* Compute the shift-amount to align*/
	align_shift = 63 - (s32_t)( FP64_DECIMALS );

	/* Compute guard bit*/
	/*guard_bit = align_shift - 1 >= 0 ? mnt & (1ULL << (align_shift - 1)) : ESSL_FALSE;*/
	/*round_bit = align_shift - 2 >= 0 ? mnt & (1ULL << (align_shift - 2)) : ESSL_FALSE;*/
	/*sticky_bit = align_shift - 2 >= 0 ? mnt & ((1ULL << (align_shift - 2)) - 1ULL) : ESSL_FALSE;*/
	guard_bit = mnt & (1ULL << (align_shift - 1));
	round_bit =  mnt & (1ULL << (align_shift - 2));
	sticky_bit = mnt & ((1ULL << (align_shift - 2)) - 1ULL);

	/* Right-sift the mantissa to match the target number of bits*/
	mnt >>= align_shift;
	exp += align_shift;

	/* Compute the exponent as if the value is not denormal*/
	exp += FP64_DECIMALS;

	/* Compute the exponent offset*/
	exponent_offset = (s64_t)( (1ULL << (FP64_BITS - 1)) - 1ULL );

	/* Is the value going to be denormal?*/
	final_exp = 0ULL;
	if ( exponent_offset <= -exp )
	{
		/* Should denormals be flushed to zero?*/
		/* Compute the number of bits to shift the mantissa to denormalize it*/
		s64_t denormalize_shift = 1 - exp - exponent_offset;

		/* Special-case 1-step denormalization shift*/
		if ( denormalize_shift == 1 )
		{
			sticky_bit |= round_bit;
			round_bit = guard_bit;
			guard_bit = mnt & 1;
			mnt >>= 1;
		}
		else
		{
			/*Compute the mask to use for generating the new sticky bit*/
			u64_t sticky_mask = (1ULL << (denormalize_shift - 2)) - 1ULL;
			assert(denormalize_shift - 2 < 64 );

			/* Right-shift the mantissa computing the correct guard, round and sticky bit*/
			sticky_bit |= round_bit | guard_bit | (mnt & sticky_mask);
			round_bit = mnt & (1ULL << (denormalize_shift - 2));
			guard_bit = mnt & (1ULL << (denormalize_shift - 1));
			mnt = mnt >> denormalize_shift;
		}

	}else
	{
		assert(exp <= exponent_offset ); /* flaots can't overflow */
		/* Compute the resulting exponent*/
		if ( exp < 0LL )
		{
			/* Add the offset to the exponent and then convert to u64*/
			final_exp = (u64_t)( exp + exponent_offset );
		}
		else
		{
			/* Convert the two values to add to u64 and then add*/
			final_exp = (u64_t)( exp ) + (u64_t)( exponent_offset );
		}
	}

	/* Construct the resulting number*/
	*res = final_exp << FP64_DECIMALS;

	/* Or-in the mantissa*/
	mnt_mask = (1ULL << FP64_DECIMALS) - 1ULL;
	*res = (*res) | (mnt & mnt_mask);

	/* round == ROUND_NEAREST_EVEN*/
	/* Is the rounding ambiguous?*/
	assert(!( guard_bit && !round_bit && !sticky_bit ));
	assert(!( guard_bit && (round_bit || sticky_bit) ));
	return ( !guard_bit && !round_bit && !sticky_bit);
}

static essl_bool decode( double *dres, u64_t val)
{
	/* Convert the input value to bits*/
	union
	{
		double d;
		u64_t u;
	} conv;

	/* Value to hold the bit-pattern of the double*/
	u64_t res = 0;

	/* Split up the fields into sign, exponent and mantissa*/
	u64_t sgn = (val & (1ULL << (FP64_BITS + FP64_DECIMALS))) >> (FP64_BITS + FP64_DECIMALS);
	u64_t exp_mask = (1ULL << FP64_BITS) - 1ULL;
	u64_t exp;
	u64_t mnt_mask;
	u64_t mnt;
	u64_t implicit;

	exp_mask <<= FP64_DECIMALS;
	exp = (val & exp_mask) >> FP64_DECIMALS;
	mnt_mask = (1ULL << FP64_DECIMALS) - 1ULL;
	mnt = val & mnt_mask;

	/* Add the implicit one if the value is not a denormal or a nan/inf*/
	implicit = 1ULL << FP64_DECIMALS;
	if ( exp != 0 && exp != (exp_mask >> FP64_DECIMALS) )
	{
		mnt |= implicit;
	}

	/* Are all exponent bits set?*/
	if(exp == (exp_mask >> FP64_DECIMALS))
	{
		u64_t signalling;
		u64_t mnt_mask;
		u64_t nan_number;
		u64_t target_mask;
		/* Copy the sign?*/
		res = sgn ? 0x8000000000000000ULL : 0ULL;

		/* Set all bits of the target exponent*/
		res |= 0x7ff0000000000000ULL;

		/* Copy the quiet/signalling bit*/
		signalling = mnt & (1ULL << (FP64_DECIMALS - 1)) ? 1ULL : 0ULL;
		signalling <<= 51;
		res |= signalling;

		/* Copy the rest of the mantissa bits*/
		mnt_mask = (1ULL << (FP64_DECIMALS - 1)) - 1ULL;
		nan_number = mnt & mnt_mask;

		/* Set these bits in the converted mantissa*/
		target_mask = 0x0007ffffffffffffULL;
		res |= nan_number & target_mask;

		/* Set the result*/
		conv.u = res;
		*dres = conv.d;

		/* Was the value representable in the target format?*/
		return (nan_number & target_mask) == nan_number;
	}/* Is this the special value zero?*/
	else if ( exp == 0 && mnt == 0 )
	{
		/* Copy the sign*/
		res = sgn ? 0x8000000000000000ULL : 0ULL;

		/* Set the result*/
		conv.u = res;
		*dres = conv.d;

		/* Was the value representable in the target format*/
		return ESSL_TRUE;
	}/* This is a standard number normalized or denormal*/
	else
	{
		s64_t exponent;
		u64_t mantissa;
		u64_t exp_offset;
		essl_bool exact;
		/* Copy the sign*/
		res = sgn ? 0x8000000000000000ULL : 0ULL;

		/* Set up high precision exponent and mantissa*/
		exponent = exp;
		mantissa = mnt;

		/* Compute the exponent offset based on the number of exponent bits*/
		exp_offset = (1ULL << (FP64_BITS - 1)) - 1ULL;

		/* Is the number denormal?*/
		if(exp == 0)
		{
			/* Un-bias the exponent*/
			exponent = -(s64_t)( exp_offset - 1);
		}else
		{
			/* Un-bias the exponent*/
			exponent -= exp_offset;
		}

		/* Change the exponent to make the mantissa an integer instead of fixed-point*/
		exponent -= FP64_DECIMALS;

		/* Encode the result as a double-precision float*/
		{
			u64_t v = 0;
			exact = encode_float( &v, exponent, mantissa);
			res |= v;
		}

		/* Set the result*/
		conv.u = res;
		*dres = conv.d;
		return exact;
	}
}
/**
* Parser front-end that reads in the number and stores it as a integer/exponent pair. The integer is stored as a bigint
* to allow any number of digits to be used in the stream to be parsed.
*
* @param	pool Memory pool for allocations
* @param	in	string to parse.
* @param	negative	Will be set to true if the parsed value was negative.
* @param	mantissa	The mantissa of the parsed number (as a bigint).
* @param	exponent	The base-10 exponent of the parsed number.
* @return	true if successful, false on error.
*/
static essl_bool parse_front_end(mempool *pool,
								const char * in,
								essl_bool *negative,
								frontend_bigint *mantissa,
								s64_t *exponent)
{
	char *c;
	char *p;
	frontend_bigint *tmp;

	/* This flag is set to true at the decimal point. Each digit after the decimal point will decrement the exponent.*/
	essl_bool decimals = ESSL_FALSE;

	/* This flag is set to true when the first digit is parsed.*/
	essl_bool digits = ESSL_FALSE;

	/* Start by clearing the values to zero*/
	*negative = ESSL_FALSE;
	*exponent = 0;
	ESSL_CHECK(bigint_init(pool,mantissa, 0));


	c = (char *)in;

	if(c == NULL)
	{
		return ESSL_FALSE;
	}

	/* Is this a whitespace character?*/
	while( *c == ' ' || *c == '\n' || *c == '\t' || *c == '\r' || *c == '\v')
	{
		/* Get the next character*/
		c += 1;
		ESSL_CHECK(c);
	}

	/* Is this a plus or minus?*/
	if(*c == '+' || *c == '-')
	{
		/* Set the negative flag if the value is negative*/
		if(*c == '-')
		{
			*negative = ESSL_TRUE;
		}

		/* Read the next character*/
		c += 1;
		ESSL_CHECK(c);
	}

	/* Do we have a digit now?*/
	if(*c >= '0' && *c <= '9')
	{
		/* A digit has been parsed*/
		digits = ESSL_TRUE;

		/* Set the mantissa to this digit*/
		ESSL_CHECK(bigint_init(pool,mantissa, *c - '0'));
	}/* A number can start with a decimal point*/
	else if (*c == '.')
	{
		decimals = ESSL_TRUE;
	}/* This can't be a number*/
	else
	{
		return ESSL_FALSE;
	}

	/* What we can now expect is a digit, a decimal point or an e*/
	p = c;
	p += 1;
	while( (*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E')
	{
		/*Are we encountering a second decimal point?*/
		if ( decimals && *p == '.' )
		{
			/* This decimal point is not part of the number,
			 *but this could be a valid parsed number if any digits have been parsed
			 */
			return digits;
		}

		/* Get the digit*/
		c += 1;
		ESSL_CHECK(c);

		/* Is this a decimal point?*/
		if(*c == '.')
		{
			decimals = ESSL_TRUE;
		}/* Is this a digit?*/
		else if(*c >= '0' && *c <= '9')
		{
			digits = ESSL_TRUE;
			/* Add the number to the mantissa*/
			ESSL_CHECK(tmp = bigint_mul_by_int(pool, mantissa, 10));
			ESSL_CHECK(bigint_copy(pool, tmp, mantissa));
			ESSL_CHECK(bigint_add_by_int(pool, mantissa, *c - '0'));

			/* Should the exponent be corrected?*/
			if(decimals)
			{
				*exponent -= 1;
			}
		}/* Is this an exponent? */
		else if(*c == 'e' || *c == 'E')
		{
			char *p;

			p = c;
			p += 1;
			/* Is the next character a +, - or a digit?*/
			if ( *p == '+' || *p == '-' || (*p >= '0' && *p <= '9') )
			{
				essl_bool pos_exponent = ESSL_TRUE;	/*Assume the exponent is positive for now*/
				s64_t parse_exponent = 0;	/* The exponent actually parsed*/
				essl_bool exponent_digit = ESSL_FALSE;	/* Set to true when the first digit of the exponent is parsed*/

				/* This is potentially a valid exponent*/
				c += 1;
				ESSL_CHECK(c);

				/* Is this a + or a -?*/
				if(*c == '+' || *c == '-')
				{
					/* Is the exponent negative?*/
					if (*c == '-')
					{
						pos_exponent = ESSL_FALSE;
					}
				}/* This is a digit*/
				else
				{
					parse_exponent = *c - '0';
					exponent_digit = ESSL_TRUE;
				}

				p = c;
				p += 1;
				/* Is the next character not a digit?*/
				if(*p < '0' || *p > '9')
				{
					/* This is completely ok if an exponent digit is already parsed*/
					if(exponent_digit)
					{
						/* Add the parsed exponent to the current exponent*/
						assert(pos_exponent);
						(*exponent) += parse_exponent;

						/* Success*/
						return ESSL_TRUE;
					}

					/* We only have e+ or e-, just unget these two characters and signal success*/
					c -= 2;

					/* Successfully parsed a number (that was accidentally followed by e+ or e-*/
					return ESSL_TRUE;
				}/* We have more digits in the exponent number*/
				else
				{
					/* Add all exponent digits*/
					p = c;
					p += 1;
					while(*p >= '0' && *p <= '9' )
					{
						/* Read the character*/
						c += 1;
						ESSL_CHECK(c);

						/* Add the value to the parsed exponent*/
						parse_exponent *= 10;
						parse_exponent += *c - '0';
						p = c;
						p += 1;
					}

					/* Add the parsed exponent to the current exponent*/
					if(pos_exponent)
					{
						(*exponent) += parse_exponent;
					}else
					{
						(*exponent) -= parse_exponent;
					}

					/* Successfully parsed the number*/
					return ESSL_TRUE;
				}
			}/* This can't possibly be a valid exponent*/
			else
			{
				/* Unget the e*/
				c -= 1;

				/* After 'E' or 'e' there must be some digit */
				return ESSL_FALSE;
			}
		}
		p = c;
		p += 1;
	}

	/* An invalid character was encountered. This is a valid parsed number if any digits are encountered*/
	return digits;
}

/**
* Parses a value from a buffer. The value has a data-type described by this object.
* If the number parsing fails, the stream will be unchanged (all all characters read
* will be put back into the stream).
*
* The function is divided into two parts. First the front-end parses the stream and
* and stores the value as a sign, mantissa and exponent, where the mantissa is a bigint.
* The front-end also takes care of special-cases such as inf, nan and snan.
*
* The back-end then takes this value (which is exact) and rounds it to the nearest-even
* representable value in the target format. If the value overflows the range of the target
* format it will be clamped or set to infinity (if that is representable).
*/

essl_bool _essl_convert_string_to_double(mempool *pool, char *in, double *res)
{
	essl_bool negative = ESSL_FALSE;
	frontend_bigint *mantissa;
	essl_bool ret;
	u64_t bit_res;
	s64_t exponent = 0;
	frontend_bigint *remainder;
	frontend_bigint *bexp0;
	frontend_bigint *bexp;
	s64_t lshift;
	essl_bool sticky_bit;
	essl_bool odd_bit;
	essl_bool guard_bit;
	essl_bool round_bit;
	s64_t rshift;
	s64_t exp, exp_offset, offset_exp;
	u64_t umantissa, mask;
	essl_bool eq_to_int;


	ESSL_CHECK(mantissa = new_frontend_bigint(pool));

	/* Run the parser front-end*/
	if (!parse_front_end(pool, in, &negative, mantissa, &exponent))
	{
		/* Failed to parse the number*/
		return ESSL_FALSE;
	}


	/* The value is a floating-point value*/
	/* Is the value negative, but the target format unsigned?*/
	if(negative)
	{
		bit_res = 1ULL << (FP64_BITS + FP64_DECIMALS);
	}/* Just set the value to 0*/
	else
	{
		bit_res = 0ULL;
	}

	ESSL_CHECK(bigint_equal_to_int(pool, mantissa, 0, &eq_to_int));

	/* If the mantissa was zero, we are complete here*/
	if(eq_to_int)
	{
		ret = decode(res, bit_res);
		return ret;
	}

	/* The mantissa remainder*/
	ESSL_CHECK(bexp0 = new_frontend_bigint(pool));
	ESSL_CHECK(bigint_init(pool, bexp0, 1));

	/* The extra left-shift applied before division*/
	lshift = 0;

	ESSL_CHECK(remainder = new_frontend_bigint(pool));
	ESSL_CHECK(bigint_init(pool,remainder, 0));

	/* Is the exponent nonzero?*/
	if(exponent != 0)
	{
		/* Compute the exponent value*/
		ESSL_CHECK(bexp = compute_exp(pool, bexp0, exponent));

		/* Is the exponent positive or negative?*/
		if(exponent > 0)
		{
			/* Multiply the mantissa with the exponent*/
			ESSL_CHECK(mantissa = bigint_mul(pool, mantissa, bexp));
		}else
		{
			frontend_bigint *div_mant;
			/* Make absolutely sure there are enough bits left after division
			 * When dividing a number a with a number b,
			 * the number of digits in the result is at least a.digits - b.digits.
			 * There should be at least decimals mantissa bits, one guard and one round bit present after division
			 */
			lshift = FP64_DECIMALS + 2 - ((s64_t)(bigint_bit_size(mantissa)) - (s64_t)(bigint_bit_size(bexp)));
			assert(lshift >= 0); /* if not just set lshift to zero */
			/* 64-bit for maximum mantissa size + guard + round bit.
			 * Sticky bit will be represented by remainder.*/
			ESSL_CHECK(mantissa = bigint_lshift(pool, mantissa, (u32_t)(lshift)));

			ESSL_CHECK(div_mant = new_frontend_bigint(pool));

			/* Divide the mantiss with the exponent*/
			ESSL_CHECK(bigint_init(pool,div_mant, 0));
			ESSL_CHECK(bigint_init(pool,remainder, 0));
			bigint_div_mod(pool, mantissa, bexp, div_mant, remainder );
			ESSL_CHECK(bigint_copy(pool, div_mant, mantissa));
		}
	}

	/* Normalize the mantissa*/
	sticky_bit = ESSL_FALSE;
	/* Keep two extra bits for guard and round bits.
	 * Also subtract two for the bigint sign-bit and the implicit one.*/
	rshift = (s64_t)(bigint_bit_size(mantissa)) - 2 - (s64_t)(FP64_DECIMALS) - 2;

	/* Right-shift by this amount?*/
	if(rshift > 0)
	{
		ESSL_CHECK(mantissa = bigint_sticky_rshift(pool, mantissa, &sticky_bit, (u32_t)(rshift)));
	}else if(rshift < 0)
	{
		ESSL_CHECK(bigint_lshift(pool,mantissa, (u32_t)(-rshift)));
	}

	ESSL_CHECK(bigint_equal_to_int(pool, remainder, 0, &eq_to_int));

	/* Set the sticky-bit if the remainder is nonzero*/
	sticky_bit |= !eq_to_int;

	/* Compute the guard and round bits*/
	get_RGObits_from_mantissa(mantissa, &round_bit, &guard_bit, &odd_bit);
	ESSL_CHECK(bigint_rshift(pool,mantissa, 2));
	rshift += 2;

	/* Compute the exponent*/
	exp = FP64_DECIMALS + rshift - lshift;

	/* Compute the exponent offset*/
	exp_offset = (1LL << (FP64_BITS - 1)) - 1LL;

	/* Compute the offset exponent*/
	offset_exp = exp + exp_offset;

	/* Is this a denormal case?*/
	if(offset_exp <= 0)
	{
		s64_t rshift;
		/* Check first if the value is so small it will anyway be zero*/
		assert(offset_exp >= -(s64_t)(FP64_DECIMALS + 2));

		/* Compute the ammount of right-shift necessary*/
		rshift = 1LL - offset_exp;

		/* Special-case rshift with one*/
		if(rshift == 1)
		{
			/* Change the guard, round and sticky bits*/
			sticky_bit |= round_bit;
			round_bit = guard_bit;
			guard_bit = odd_bit;

			/* Right-shift mantissa by one*/
			ESSL_CHECK(bigint_rshift(pool,mantissa, 1));
		}/* Special-case also rshift with two*/
		else
		{
			/* Right-shift the value so that it is easy to find the round and guard bits.*/
			essl_bool sh_sticky = ESSL_FALSE;
			assert(rshift >= 2);

			ESSL_CHECK(mantissa = bigint_sticky_rshift(pool, mantissa, &sh_sticky, rshift - 2 ));

			/* Change the guard, round and sticky bits*/
			sticky_bit |= round_bit | guard_bit | sh_sticky;
			get_RGObits_from_mantissa(mantissa, &round_bit, &guard_bit, &odd_bit);
			/* Right-shift the mantissa by two*/
			ESSL_CHECK(bigint_rshift(pool,mantissa, 2));
		}

		/* The offset exponent can now be set to zero*/
		offset_exp = 0;
	}else	/* The value wasn't denormal, but it still may be infinity*/
	{
		/* Compute the exponent that represents infinity, if the computed exponent is equal to,
		 * or larger than this limit, the value should be set to infinity*/
		u64_t max_exp = (1ULL << FP64_BITS) - 1ULL;

		/* Is this an infinity case?*/
		if(offset_exp >= (s64_t)(max_exp))
		{
			offset_exp = max_exp;
			ESSL_CHECK(bigint_init(pool,mantissa, 0));
			odd_bit = guard_bit = round_bit = sticky_bit = ESSL_FALSE;
		}
	}

	/* Or the mantissa into the result*/
	bit_res |= (u64_t)(offset_exp) << FP64_DECIMALS;

	umantissa = 0;
	/* Convert the mantissa to an u64-value and discard the implicit one.*/
	bigint_to_u64(mantissa, &umantissa);

	/* Discard implicit one*/
	mask = (1ULL << FP64_DECIMALS) - 1ULL;
	umantissa &= mask;

	/* Or the mantissa into the result*/
	bit_res |= umantissa;

	/* Is it necessary to round the value up?*/
	if(guard_bit)
	{
		if(round_bit || sticky_bit)
		{
			bit_res++;
		}else if(odd_bit)
		{
			bit_res++;
		}
	}

	ret = decode(res, bit_res);
	/* Successfully parsed the number*/
	return ret;
}


#ifdef UNIT_TEST
#include "softfloat/softfloat.h"
int main (void)
{
	int i;
	struct {
		char *input;
		essl_bool is_success;
		sf64 result;
	} test_cases[] ={
		{"-1.1125369292536007e-308", ESSL_TRUE, 0x8008000000000000},
		{"1.1125369292536007e-308", ESSL_TRUE, 0x8000000000000},
		{"000/01", ESSL_TRUE, 0},
		{"0001", ESSL_TRUE, 0x3ff0000000000000},
		{"0.001", ESSL_TRUE, 0x3f50624dd2f1a9fc},
		{"0e0", ESSL_TRUE, 0},
		{"0E0", ESSL_TRUE, 0},
		{"0EF", ESSL_FALSE, 0},
		{"0eF", ESSL_FALSE, 0},
		{"-0", ESSL_TRUE, 0x8000000000000000},
		{"1.2882297539194267e-231", ESSL_TRUE, 0x1000000000000000},
		{"2.2250738585072014e-308", ESSL_TRUE, 0x10000000000000},
		{"0", ESSL_TRUE, 0},
		{"2.0000000000006821", ESSL_TRUE, 0x4000000000000600},
		{"7.5888483201215469e-321", ESSL_TRUE, 0x600},
		{"3.1415926535897932384626433832795", ESSL_TRUE, 0x400921fb54442d18},
		{"-0.1E+F", ESSL_TRUE, 0xbfb999999999999a},
		{"-0.1E-F", ESSL_TRUE, 0xbfb999999999999a},
		{"-0.1-EF", ESSL_TRUE, 0xbfb999999999999a},
		{"-0.1E-1", ESSL_TRUE, 0xbf847ae147ae147b},
		{"-0.1E", ESSL_FALSE, 0},
		{"4.9406564584124654e-324", ESSL_TRUE, 1},
		{"1.1125369292536002e-308", ESSL_TRUE, 0x7ffffffffffff},
		{"8.9002954340288055e-308", ESSL_TRUE, 0x30000000000000},
		{"-1.1e+400", ESSL_TRUE, 0xfff0000000000000},
		{"1.1e+400", ESSL_TRUE, 0x7FF0000000000000},
		{"-1.1e-235", ESSL_TRUE, 0x8f2662532cdc1b6b},
		{"3E1", ESSL_TRUE, 0x403e000000000000},
		{"1.17549421e-38", ESSL_TRUE, 0x380fffffbfaf0a2a},
		{"1.1e-235", ESSL_TRUE, 0xf2662532cdc1b6b},
		{"-0.1+e13", ESSL_TRUE, 0xbfb999999999999a},
		{"-0.1E-13", ESSL_TRUE, 0xbd06849b86a12b9b},
		{"-0.1E-13F", ESSL_TRUE, 0xbd06849b86a12b9b},
		{"-0.1E+13F", ESSL_TRUE, 0xc26d1a94a2000000},
		{"0..1", ESSL_TRUE, 0},
		{"1..1", ESSL_TRUE, 0x3ff0000000000000},
		{".1.", ESSL_TRUE, 0x3fb999999999999a},
		{"0.1.", ESSL_TRUE, 0x3fb999999999999a},
		{"0.1e10", ESSL_TRUE, 0x41cdcd6500000000},
		{"0.1", ESSL_TRUE, 0x3fb999999999999a},
		{"snnn", ESSL_FALSE, 0},
		{"nan", ESSL_FALSE, 0x7FFFFFFF},
		{"  \t0.1", ESSL_TRUE, 0x3fb999999999999a},
		{"  \n\r\v+0.1", ESSL_TRUE, 0x3fb999999999999a},
		{"  \n\r\v-0.1", ESSL_TRUE, 0xbfb999999999999a},
		{NULL, ESSL_FALSE, 0},
		{"", ESSL_FALSE, 0},

	};

	for(i = 0; i < sizeof(test_cases)/sizeof(test_cases[0]); ++i)
	{
		mempool_tracker tracker;
		mempool pool;
		char *input = test_cases[i].input;
		double res;
		essl_bool success_parsing;
		_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
		_essl_mempool_init(&pool, 0, &tracker);
		success_parsing = _essl_convert_string_to_double(&pool, input, &res);
		if(success_parsing != test_cases[i].is_success)
		{
			fprintf(stderr, "expected parsing result: %s, got %s \n",
									(test_cases[i].is_success) ? "TRUE" : "FALSE",
									(success_parsing) ? "TRUE" : "FALSE");
			assert(success_parsing == test_cases[i].is_success);
		}
		if(!success_parsing)
		{
			continue;
		}
		if(_mali_isnan_sf64(test_cases[i].result) && _mali_isnan_sf64(_mali_double_to_sf64(res)))
		{
			continue;
		}
		if(!_mali_equal_sf64(_mali_double_to_sf64(res), (test_cases[i].result)))
		{
			fprintf(stderr, "returned: '%f', expected '%f' \n", res, _mali_sf64_to_double(test_cases[i].result));
			assert(0);
		}
		_essl_mempool_destroy(&pool);
	}

	fprintf(stderr, "All tests OK!\n");
	return 0;
}
#endif /*UNIT_TEST*/
