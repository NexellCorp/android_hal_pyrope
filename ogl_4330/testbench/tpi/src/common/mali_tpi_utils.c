/*
 * $Copyright:
 * ----------------------------------------------------------------------------
 *  This confidential and proprietary software may be used only as authorized
 *  by a licensing agreement from ARM Limited.
 *          (C) COPYRIGHT 2005-2012 ARM Limited , ALL RIGHTS RESERVED
 *  The entire notice above must be reproduced on all authorized copies and
 *  copies may only be made to the extent permitted by a licensing agreement
 *  from ARM Limited.
 * ----------------------------------------------------------------------------
 * $
 */

/* ============================================================================
	Includes
============================================================================ */
#define MALI_TPI_API MALI_TPI_EXPORT
#include "tpi/mali_tpi.h"

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <float.h>

/* ============================================================================
	Function Like Macros
============================================================================ */
/* Many of the local functions exit if a sub-function returns less than zero.
 * This macro encapsulates that behavior.
 */
#define EXIT_LTZ( call )   \
{                          \
	int ltzstatus = call;  \
	if( ltzstatus < 0 )    \
	{                      \
		return ltzstatus;  \
	}                      \
} while(0)

/* ============================================================================
	Local defines: constants and and structures
============================================================================ */
static const unsigned int flag_alternate = 0x01; /**< printf flag # */
static const unsigned int flag_zero_pad  = 0x02; /**< printf flag 0 */
static const unsigned int flag_left      = 0x04; /**< printf flag - */
static const unsigned int flag_space     = 0x08; /**< printf flag ' ' */
static const unsigned int flag_sign      = 0x10; /**< printf flag + */

/**
 * @brief Internal state for @c mali_tpi_format_string
 */
typedef struct
{
	/** Total number of characters that have passed through */
	size_t written;
	/** Callback function given to @c mali_tpi_format_string */
	int (*output)(char ch, void *arg);
	/** User argument to be passed to @c output */
	void *arg;
} format_string_state;

/**
 * @brief Modifier characters for mali_tpi_format_string
 */
typedef enum
{
	modifier_none,
	modifier_hh,
	modifier_h,
	modifier_l,
	modifier_ll,
	modifier_z,
	modifier_t
} format_string_modifier;

/* An encoding representing value * 10^exponent, for float formatting */
typedef struct
{
	mali_tpi_uint64 value;
	int exponent;
} decimal;

static const int float_type_neg = 0x1;
static const int float_type_inf = 0x2;
static const int float_type_nan = 0x4;

typedef struct
{
	char *str;
	size_t space;
} string_formatter;

/* ============================================================================
	Private Functions
============================================================================ */
/**
 * @brief Output a single character from string formatting
 *
 * @param fss      The callback state
 * @param c        The character to output
 *
 * @return Zero on success, negative value on failure
 */
static int format_string_emit_char( format_string_state *fss, char c )
{
	EXIT_LTZ( fss->output( c, fss->arg ) );
	++fss->written;
	return 0;
}

/* ------------------------------------------------------------------------- */
/**
 * @brief Output a sequence of characters from string formatting
 *
 * @param fss      The callback state
 * @param len      The length of @c s
 * @param s        The string to output (need not be null-terminated)
 *
 * @return Zero on success, negative value on failure
 */
static int format_string_emit_chars(
	format_string_state *fss,
	size_t len,
	const char *s )
{
	size_t i;
	for( i = 0; i < len; i++ )
	{
		EXIT_LTZ( format_string_emit_char(fss, s[i]) );
	}
	return 0;
}

/* ------------------------------------------------------------------------- */
/**
 * @brief Output a single character multiple times
 *
 * @param fss      The callback state
 * @param repeat   The number of times to output the character (may be zero)
 * @param c        The character to output (may be null)
 *
 * @return Zero on success, negative value on failure
 */
static int format_string_emit_char_repeat(
	format_string_state *fss,
	size_t repeat,
	char c )
{
	size_t i;
	for( i = 0; i < repeat; i++ )
	{
		EXIT_LTZ( format_string_emit_char( fss, c ) );
	}
	return 0;
}

/* ------------------------------------------------------------------------- */
/**
 * @brief Output a number from string formatting
 *
 * @param fss            The callback state
 * @param prefix         An optional prefix before the number (e.g. sign)
 * @param digits_len     The number of characters in @c digits
 * @param digits         The actual digits of the number
 * @param flags          The format flags (only @c flag_left and
 *                       @c flag_zero_pad matter)
 * @param width          The total field width, including prefix
 * @param precision      The minimum number of digits to output
 * @param trailing_zeros Number of extra zeros to output after the number
 *                       itself
 *
 * @pre At most one of @c flag_left and @c flag_zero_pad may be set
 * @pre @c precision must be non-negative
 */
static int format_string_emit_number(
	format_string_state *fss,
	const char *prefix,
	int digits_len,
	const char *digits,
	unsigned int flags,
	int width,
	int precision,
	int trailing_zeros)
{
	const int prefix_len = strlen( prefix );
	int total_len = prefix_len + digits_len + trailing_zeros;
	int extra_digits = 0;
	int pad = 0;

	if( digits_len < precision )
	{
		extra_digits = precision - digits_len;
		total_len += extra_digits;
	}

	if( total_len < width )
	{
		if( flags & flag_zero_pad )
		{
			extra_digits += width - total_len;
		}
		else
		{
			pad = width - total_len;
			if( !( flags & flag_left ) )
			{
				EXIT_LTZ( format_string_emit_char_repeat( fss, pad, ' ' ) );
			}
		}
	}

	EXIT_LTZ( format_string_emit_chars( fss, prefix_len, prefix ) );
	EXIT_LTZ( format_string_emit_char_repeat( fss, extra_digits, '0' ) );
	EXIT_LTZ( format_string_emit_chars( fss, digits_len, digits ) );
	EXIT_LTZ( format_string_emit_char_repeat( fss, trailing_zeros, '0' ) );

	if( flags & flag_left )
	{
		EXIT_LTZ( format_string_emit_char_repeat( fss, pad, ' ' ) );
	}
	return 0;
}

/* ------------------------------------------------------------------------- */
static int format_string_format_d(
	format_string_state *fss,
	unsigned int flags,
	int width,
	int precision,
	mali_tpi_int64 value )
{
	mali_tpi_bool neg = MALI_TPI_FALSE;
	mali_tpi_bool max_neg = MALI_TPI_FALSE;

	/* Buffer to hold the formatted number. It is filled in backwards
	 * from the end.
	 */
	char buffer[22];
	char *buffer_end = buffer + sizeof( buffer ) - 1;
	char *buffer_ptr;
	const char *prefix;

	if( precision >= 0 )
	{
		flags &= ~flag_zero_pad;
	}
	else
	{
		precision = 1;
	}

	if( value < 0 )
	{
		neg = MALI_TPI_TRUE;
		/* Handle overflow. Assumes two's complement */
		if( ( value << 1 ) == 0 )
		{
			max_neg = MALI_TPI_TRUE;
			value++;
		}
		value = -value;
	}

	*buffer_end = '\0';
	buffer_ptr = buffer_end;
	while( value > 0 )
	{
		*--buffer_ptr = (char) ( '0' + ( value % 10 ) );
		value /= 10;
	}

	if( max_neg )
	{
		/* We decreased the value by one. This can only happen for a power
		 * of two, so we don't need to worry about carry when correcting this.
		 */
		buffer_end[-1]++;
	}

	if( neg )
	{
		prefix = "-";
	}
	else if( flags & flag_sign )
	{
		prefix = "+";
	}
	else if( flags & flag_space )
	{
		prefix = " ";
	}
	else
	{
		prefix = "";
	}

	return format_string_emit_number( fss, prefix, buffer_end - buffer_ptr,
	    buffer_ptr, flags, width, precision, 0 );
}

/* ------------------------------------------------------------------------- */
static int format_string_format_u(
	format_string_state *fss,
	unsigned int flags,
	int width,
	int precision,
	mali_tpi_uint64 value )
{
	/* Buffer to hold the formatted number. It is filled in backwards
	 * from the end.
	 */
	char buffer[22];
	char *buffer_end = buffer + sizeof( buffer ) - 1;
	char *buffer_ptr;

	if( precision >= 0 )
	{
		flags &= ~flag_zero_pad;
	}
	else
	{
		precision = 1;
	}

	*buffer_end = '\0';
	buffer_ptr = buffer_end;
	while( value > 0 )
	{
		*--buffer_ptr = (char) ( '0' + ( value % 10 ) );
		value /= 10;
	}

	return format_string_emit_number( fss, "", buffer_end - buffer_ptr,
	    buffer_ptr, flags, width, precision, 0 );
}

/* ------------------------------------------------------------------------- */
static int format_string_format_binary(
	format_string_state *fss,
	const char *prefix,
	mali_tpi_bool prefix_zero,
	int shift,
	const char *char_table,
	unsigned int flags,
	int width,
	int precision,
	mali_tpi_uint64 value )
{
	unsigned int mask = ( 1U << shift ) - 1;

	/* Buffer to hold the formatted number. It is filled in backwards
	 * from the end. */
	char buffer[65]; /* big enough for binary 64-bit in future */
	char *buffer_end = buffer + sizeof( buffer ) - 1;
	char *buffer_ptr;

	*buffer_end = '\0';
	buffer_ptr = buffer_end;
	if( ( value == 0 ) && ( !prefix_zero ) )
	{
		prefix = "";
	}

	while( value > 0 )
	{
		*--buffer_ptr = char_table[value & mask];
		value >>= shift;
	}

	return format_string_emit_number( fss, prefix, buffer_end - buffer_ptr,
	    buffer_ptr, flags, width, precision, 0 );
}

/* ------------------------------------------------------------------------- */
static int format_string_format_o(
	format_string_state *fss,
	unsigned int flags,
	int width,
	int precision,
	mali_tpi_uint64 value )
{
	const char *prefix = "";
	static const char char_table[] = "01234567";

	if( precision >= 0 )
	{
		flags &= ~flag_zero_pad;
	}
	else
	{
		precision = 1;
	}

	if( flags & flag_alternate )
	{
		prefix = "0";
		/* This prefix counts against the precision, so we need to reduce
		 * the precision such that "%#.3o", 0 produces 000 not 0000. */
		if( precision > 0 )
		{
			precision--;
		}
	}

	return format_string_format_binary( fss, prefix, MALI_TPI_TRUE, 3,
		char_table, flags, width, precision, value );
}

/* ------------------------------------------------------------------------- */
static int format_string_format_hex(
	format_string_state *fss,
	mali_tpi_bool upper_case,
	mali_tpi_bool prefix_zero,
	unsigned int flags,
	int width,
	int precision,
	mali_tpi_uint64 value)
{
	const char *prefix = "";
	const char *char_table = upper_case ? "0123456789ABCDEF"
	                                    : "0123456789abcdef";

	if( precision >= 0 )
	{
		flags &= ~flag_zero_pad;
	}
	else
	{
		precision = 1;
	}

	if( flags & flag_alternate )
	{
		prefix = upper_case ? "0X" : "0x";
	}

	return format_string_format_binary( fss, prefix, prefix_zero, 4,
	    char_table, flags, width, precision, value );
}

/* ------------------------------------------------------------------------- */
static int format_string_format_p(
	format_string_state *fss,
	unsigned int flags,
	int width,
	int precision,
	mali_tpi_uint64 value)
{
	/* Force the 0x prefix */
	flags |= flag_alternate;

	/* Default to a full-width output for the size of pointers */
	if( precision >= 0 )
	{
		flags &= ~flag_zero_pad;
	}
	else
	{
		precision = sizeof(void *) * 2; /* Two hex digits per byte */
	}

	return format_string_format_hex( fss, MALI_TPI_FALSE, MALI_TPI_TRUE,
	    flags, width, precision, value );
}

/* ------------------------------------------------------------------------- */
static int format_string_format_s(
	format_string_state *fss,
	unsigned int flags,
	int width,
	int precision,
	const char *str )
{
	size_t len;

	if( precision >= 0 )
	{
		size_t i;
		len = precision;

		/* Check whether the string is actually shorter than len. We cannot
		 * use strlen because the string is not required to be
		 * null-terminated in this case. */
		for( i = 0; i < len; i++ )
		{
			if( str[i] == '\0' )
			{
				len = i;
				break;
			}
		}
	}
	else
	{
		len = strlen( str );
	}

	if( (width >= 0) && ((size_t) width > len) && (!(flags & flag_left)) )
	{
		/* We ignore flag_zero, since it's undefined with %s anyway */
		EXIT_LTZ( format_string_emit_char_repeat(fss, width - len, ' ') );
	}

	EXIT_LTZ( format_string_emit_chars(fss, len, str) );

	if( ( width >= 0 ) && ( (size_t) width > len ) && ( flags & flag_left ) )
	{
		EXIT_LTZ( format_string_emit_char_repeat(fss, width - len, ' ') );
	}
	return 0;
}

/* ------------------------------------------------------------------------- */
static int format_string_format_c(
	format_string_state *fss,
	unsigned int flags,
	int width,
	char ch)
{
	if( ( width > 1 ) && !( flags & flag_left ) )
	{
		/* We ignore flag_zero, since it's undefined with %s anyway */
		EXIT_LTZ( format_string_emit_char_repeat(fss, width - 1, ' ') );
	}

	EXIT_LTZ( format_string_emit_char(fss, ch) );

	if( ( width > 1 ) && ( flags & flag_left ) )
	{
		EXIT_LTZ( format_string_emit_char_repeat(fss, width - 1, ' ') );
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
static void decimal_raise( decimal *d, int exponent )
{
	int error = 0; /* Relative error estimate in units of 1.5/2^64 */
	static const mali_tpi_uint64 max = ~(mali_tpi_uint64) 0;

	assert( exponent >= 0 );
	while( exponent > 0 )
	{
		exponent--;

		if( d->value > max / 2 )
		{
			int mod10 = (int) ( d->value % 10 );
			d->value /= 10;
			d->exponent++;
			if( mod10 >= 5 )
			{
				d->value++;
				mod10 -= 10;
			}

			/* Before dividing by 10, d was in the range [2^63, 2^64). Thus
			 * the relative error is in the range (d/2^64, d/2^63]. Approximate
			 * it as 1.5d/2^64.
			 */
			error += mod10;
		}
		d->value *= 2;
	}
	/* Adjust for relative error. First truncate to 16 bits so that we
	 * can do this calculation in 32 bits without risk of overflow.
	 * The relative error from the truncation has minimal effect on the
	 * correction term.
	 */
	d->value += ( ( (int) ( d->value >> 48 ) ) * error * 3 ) >> 17;
}

/* ------------------------------------------------------------------------- */
static void decimal_lower( decimal *d, int exponent )
{
	int error = 0; /* Relative error estimate in units of 1.5/2^64 */
	int scale;

	assert( exponent <= 0 );
	while( exponent < 0 )
	{
		static const mali_tpi_uint64 max64 = ~(mali_tpi_uint64) 0;

		exponent++;
		/* Avoid losing bits if possible */
		if( ( d->value & 1 ) && ( d->value <= max64 / 10 ) )
		{
			d->value *= 10;
			d->exponent--;
		}

		if( d->value & 1 )
		{
			/* 1/d->value is between (1 << scale)/2^64 and (2 << scale)/2^64 */
			scale = 0;
			while( ( d->value >> ( 63 - scale ) ) == 0 )
			{
				scale++;
			}
			error += 1 << scale;
		}
		d->value /= 2;
	}

	/* Adjust for relative error. First truncate to 16 bits so that we
	 * can do this calculation in 32 bits without risk of overflow.
	 * The relative error from the truncation has minimal effect on the
	 * correction term.
	 */
	d->value += ( ( (int) ( d->value >> 48 ) ) * error * 3 ) >> 17;
}

/* ------------------------------------------------------------------------- */
static void decimal_reduce( decimal *d, int k )
{
	mali_tpi_uint64 tenk = 1; /* 10^k */
	mali_tpi_uint64 mod_tenk;
	int i;

	if( k >= 20 )
	{
		/* 10^20 is bigger than 2^64 */
		d->value = 0;
		d->exponent += k;
		return;
	}

	for( i = 0; i < k; i++ )
	{
		tenk *= 10;
	}

	mod_tenk = d->value % tenk;
	d->value /= tenk;
	d->exponent += k;

	/* Round up if mod_tenk > tenk / 2, or if it's equal and d->value is odd
	 * NB: don't multiple out the / 2, it could overflow */
	if( ( mod_tenk + ( d->value & 1 ) ) > ( tenk / 2 ) )
	{
		d->value++;
	}
}

/* ------------------------------------------------------------------------- */
static int format_string_format_f_get_value(
	decimal *d,
	int precision,
	double value )
{
	mali_tpi_uint64 bits;
	mali_tpi_uint64 sign_bit = ( (mali_tpi_uint64) 1 ) << 63;
	int exponent;
	mali_tpi_uint64 mantissa;
	int type = 0;

	assert( sizeof(bits) == sizeof(value) );
	memcpy( &bits, &value, sizeof( bits ) );
	if( bits & sign_bit )
	{
		type |= float_type_neg;
		bits ^= sign_bit;
	}

	/* -2 because of implicit mantissa bit, and magic denorm value */
	exponent = (int) ( ( bits >> ( DBL_MANT_DIG - 1 ) ) + DBL_MIN_EXP - 2 );
	mantissa = bits & ( ( ( (mali_tpi_uint64) 1 ) << ( DBL_MANT_DIG - 1 ) )
	    - 1 );

	if( exponent == DBL_MAX_EXP )
	{
		type |= ( mantissa == 0 ) ? float_type_inf : float_type_nan;
		return type;
	}

	if( exponent == (DBL_MIN_EXP - 2) )
	{
		/* Denorm or zero - they can be treated the same */
		exponent++;
	}
	else
	{
		mantissa += ( (mali_tpi_uint64) 1 ) << ( DBL_MANT_DIG - 1 );
	}

	/* Treat mantissa as an integer */
	exponent -= DBL_MANT_DIG - 1;
	d->value = mantissa;
	d->exponent = 0;

	/* Apply exponent */
	if( exponent >= 0 )
	{
		decimal_raise( d, exponent );
	}
	else
	{
		decimal_lower( d, exponent );
	}

	/* Round to requested precision if necessary */
	if( d->exponent < -precision )
	{
		decimal_reduce( d, -precision - d->exponent );
	}

	return type;
}

/* ------------------------------------------------------------------------- */
static int format_string_format_f(
	format_string_state *fss,
	unsigned int flags,
	int width,
	int precision,
	double value )
{
	char buffer[512]; /* Maximum double is ~1.8e308, so lots of headroom */
	char *buffer_ptr;
	char *buffer_end;
	decimal dec = { 0, 0 };
	const char *prefix; /* Characters to put in ahead of zero padding */
	int type;
	int trailing_zeros = 0; /* Extra zeros to put after the buffer_ptr */
	int i;

	if( precision < 0 )
	{
		precision = 6;
	}

	type = format_string_format_f_get_value( &dec, precision, value );
	if( type & float_type_neg )
	{
		prefix = "-";
	}
	else if( flags & flag_sign )
	{
		prefix = "+";
	}
	else if( flags & flag_space )
	{
		prefix = " ";
	}
	else
	{
		prefix = "";
	}

	/* Note: 0 flag explicitly does not apply to inf */
	if( type & float_type_inf )
	{
		return format_string_emit_number( fss, prefix, 3, "inf", ( flags
		    & ~flag_zero_pad ), width, 0, 0 );
	}
	if( type & float_type_nan )
	{
		return format_string_emit_number( fss, prefix, 3, "nan", ( flags
		    & ~flag_zero_pad ), width, 0, 0 );
	}

	buffer_end = buffer + sizeof( buffer ) - 1;
	*buffer_end = '\0';
	buffer_ptr = buffer_end;

	i = -precision;
	/* Count trailing zeros off the end of the number. This is necessary to
	 * ensure that buffer is not overrun even if precision is enormous.
	 */
	while( ( i < dec.exponent ) && ( i < 0 ) )
	{
		trailing_zeros++;
		i++;
	}

	while( ( i <= 0 ) || ( dec.value != 0 ) )
	{
		if( ( i == 0 ) && ( precision > 0 || ( flags & flag_alternate ) ) )
		{
			*--buffer_ptr = '.';
		}

		if( i < dec.exponent )
		{
			*--buffer_ptr = '0';
		}
		else
		{
			*--buffer_ptr = (char) ( '0' + dec.value % 10 );
			dec.value /= 10;
			dec.exponent++;
		}
		i++;
	}

	return format_string_emit_number( fss, prefix, buffer_end - buffer_ptr,
	    buffer_ptr, flags, width, 0, trailing_zeros );
}

/* ------------------------------------------------------------------------- */
static unsigned int format_string_parse_flags(const char **format)
{
	unsigned int flags = 0;

	while( MALI_TPI_TRUE )
	{
		switch( **format )
		{
		case '#':
			flags |= flag_alternate;
			break;

		case '0':
			flags |= flag_zero_pad;
			break;

		case '-':
			flags |= flag_left;
			break;

		case ' ':
			flags |= flag_space;
			break;

		case '+':
			flags |= flag_sign;
			break;

		default:
			return flags;
		}
		++*format;
	}
}

/* ------------------------------------------------------------------------- */
/* Parse the width field (possibly empty) from a format string.
 * Returns true if no width was present or one was parsed, false
 * if the width was '*'. If no width was present, *width is set to 0.
 */
static mali_tpi_bool format_string_parse_width(
	const char **format,
	int *width )
{
	char next;

	next = **format;
	/* Parse field width */
	if( next == '*' )
	{
		++*format;
		return MALI_TPI_FALSE;
	}
	else
	{
		int w = 0;
		/* Don't need to worry about a negative width here, since the
		 * - would have been treated as a flag.
		 */
		while( next >= '0' && next <= '9' )
		{
			/* TODO what if this overflows? */
			w = w * 10 + ( next - '0' );
			++*format;
			next = **format;
		}
		*width = w;
		return MALI_TPI_TRUE;
	}
}

/* ------------------------------------------------------------------------- */
/* Parses the precision field (possibly absent), including the '.', from
 * a format string. Returns true if the precision was absent, or was found and
 * parsed. Returns false if the precision was '*'.
 * If the precision was absent, *precision is set to a negative value.
 */
static mali_tpi_bool format_string_parse_precision(
	const char **format,
	int *precision )
{
	char next;

	next = **format;
	if( next == '.' )
	{
		++*format;
		next = **format;
		if( next == '*' )
		{
			++*format;
			return MALI_TPI_FALSE;
		}
		else
		{
			mali_tpi_bool neg = MALI_TPI_FALSE;
			int p = 0; /* C99: precision of just '.' is same as 0 */

			if( next == '-' )
			{
				neg = MALI_TPI_TRUE;
				++*format;
				next = **format;
			}
			while( next >= '0' && next <= '9' )
			{
				p = p * 10 + ( next - '0' );
				++*format;
				next = **format;
			}
			if( neg )
			{
				p = -p;
			}
			*precision = p;
			return MALI_TPI_TRUE;
		}
	}
	else
	{
		*precision = -1;
		return MALI_TPI_TRUE;
	}
}

/* ------------------------------------------------------------------------- */
/* Parses a (possibly absent) length modifier from a format string. */
static format_string_modifier format_string_parse_modifier(
	const char **format )
{
	char next;

	next = **format;
	switch( next )
	{
	case 'h':
		++*format;
		next = **format;
		if( next == 'h' )
		{
			++*format;
			return modifier_hh;
		}
		return modifier_h;

	case 'l':
		++*format;
		next = **format;
		if( next == 'l' )
		{
			++*format;
			return modifier_ll;
		}
		return modifier_l;

	case 'z':
		++*format;
		return modifier_z;

	case 't':
		++*format;
		return modifier_t;

	default:
		return modifier_none;
	}
}

/* ------------------------------------------------------------------------- */
static int string_append( char ch, void *arg )
{
	string_formatter *sf = (string_formatter *) arg;
	if( sf->space > 1 )
	{
		*sf->str++ = ch;
		sf->space--;
	}
	return (unsigned char) ch;
}

/* ============================================================================
	Public Functions internally in the Library, not Exported
============================================================================ */
mali_tpi_bool _mali_tpi_init_internal( void )
{
	return MALI_TPI_TRUE;
}

/* ------------------------------------------------------------------------- */
mali_tpi_bool _mali_tpi_shutdown_internal( void )
{
	return MALI_TPI_TRUE;
}

/* ============================================================================
	Public Functions
============================================================================ */
MALI_TPI_IMPL int mali_tpi_format_string(
	int(*output)(char, void *),
	void *arg,
	const char *format,
	va_list ap )
{
	format_string_state fss;

	fss.written = 0;
	fss.output = output;
	fss.arg = arg;

	while( *format != '\0' )
	{
		char next = *format++;
		if( next != '%' )
		{
			EXIT_LTZ( format_string_emit_char( &fss, next ) ) ;
		}
		else if( *format == '%' )
		{
			format++;
			EXIT_LTZ( format_string_emit_char( &fss, '%' ) );
		}
		else
		{
			unsigned int flags;
			int width, precision;
			format_string_modifier modifier;

			flags = format_string_parse_flags( &format );
			if( !format_string_parse_width( &format, &width ) )
			{
				width = va_arg(ap, int);
			}

			if( !format_string_parse_precision( &format, &precision ) )
			{
				precision = va_arg(ap, int);
			}
			modifier = format_string_parse_modifier( &format );

			/* C99 seems to imply this happens even for asterisk form */
			if( width < 0 )
			{
				flags |= flag_left;
				/* TODO: what if width == INT_MIN? */
				width = -width;
			}

			/* C99 specifies that some flags override others */
			if( flags & flag_sign )
			{
				flags &= ~flag_space;
			}
			if( flags & flag_left )
			{
				flags &= ~flag_zero_pad;
			}

			/* Parse conversion specifier. We also have to deal with all
			 * combinations of length modifier and conversion specifier
			 * in this function, since there is no reliable portable way
			 * to pass off the va_list and then continue to use it here
			 * (in spite of what C99 promises). */
			next = *format++;

			switch( next )
			{
			case 'd':
				/* Coverity Prevent: Expected fallthrough */
			case 'i':
			{
				mali_tpi_int64 value;
				switch( modifier )
				{
				case modifier_hh:
					value = (mali_tpi_int64) (mali_tpi_int8) va_arg(ap, int);
					break;
				case modifier_h:
					value = (mali_tpi_int64) (mali_tpi_int16)va_arg(ap, int);
					break;
				case modifier_none:
					value = va_arg(ap, int);
					break;
				case modifier_l:
					value = va_arg(ap, long int);
					break;
				case modifier_ll:
					value = va_arg(ap, mali_tpi_int64);
					break;
				case modifier_t:
					value = va_arg(ap, ptrdiff_t);
					break;
				default:
					return -1;
				}

				EXIT_LTZ( format_string_format_d( &fss, flags, width,
				    precision, value ) );
			}
				break;
			case 'o':
				/* Coverity Prevent: Expected fallthrough */
			case 'u':
				/* Coverity Prevent: Expected fallthrough */
			case 'x':
				/* Coverity Prevent: Expected fallthrough */
			case 'X':
			{
				mali_tpi_uint64 value;
				switch( modifier )
				{
				case modifier_hh:
					value = (mali_tpi_uint64) (mali_tpi_uint8) va_arg(ap, int);
					break;
				case modifier_h:
					value = (mali_tpi_uint64) (mali_tpi_uint16)va_arg(ap, int);
					break;
				case modifier_none:
					value = va_arg(ap, unsigned int);
					break;
				case modifier_l:
					value = va_arg(ap, unsigned long int);
					break;
				case modifier_ll:
					value = va_arg(ap, mali_tpi_uint64);
					break;
				case modifier_z:
					value = va_arg(ap, size_t);
					break;
				default:
					return -1;
				}

				switch( next )
				{
				case 'u':
					EXIT_LTZ( format_string_format_u( &fss, flags, width,
					    precision, value ) );
					break;
				case 'o':
					EXIT_LTZ( format_string_format_o( &fss, flags, width,
					    precision, value ) );
					break;
				case 'x':
					EXIT_LTZ( format_string_format_hex( &fss, MALI_TPI_FALSE,
					    MALI_TPI_FALSE, flags, width, precision, value ) );
					break;
				case 'X':
					EXIT_LTZ( format_string_format_hex( &fss, MALI_TPI_TRUE,
					    MALI_TPI_FALSE, flags, width, precision, value ) );
					break;
				default:
					assert(0); /* Should not be reachable */
					return -1;
				}
			}
				break;
			case 'p':
			{
				mali_tpi_uint64 value;

				if( modifier != modifier_none )
				{
					return -1;
				}
				value = (mali_tpi_uint64) (size_t) va_arg(ap, void *);
				EXIT_LTZ( format_string_format_p( &fss, flags, width,
				    precision, value ) );
			}
				break;
			case 'n':
			{
				switch( modifier )
				{
				case modifier_ll:
					*( va_arg(ap, mali_tpi_int64 *) ) = fss.written;
					break;
				case modifier_l:
					*( va_arg(ap, long *) ) = fss.written;
					break;
				case modifier_t:
					*( va_arg(ap, ptrdiff_t *) ) = fss.written;
					break;
				case modifier_z:
					*( va_arg(ap, size_t *) ) = fss.written;
					break;
				case modifier_none:
					*( va_arg(ap, int *) ) = fss.written;
					break;
				default:
					return -1;
				}
			}
				break;
			case 's':
				if( modifier != modifier_none )
				{
					return -1;
				}

				EXIT_LTZ( format_string_format_s( &fss, flags, width,
				    precision, va_arg(ap, const char *)) );

				break;
			case 'c':
				if( modifier != modifier_none )
				{
					return -1;
				}
				EXIT_LTZ( format_string_format_c( &fss, flags, width,
				    va_arg(ap, int)) );
				break;
			case 'f':
				EXIT_LTZ( format_string_format_f( &fss, flags, width,
				    precision, va_arg(ap, double)) );
				break;
			default:
				return -1; /* Invalid specifier */
			}
		}
	}
	return fss.written;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL int mali_tpi_vsnprintf(
	char *str,
	size_t size,
	const char *format,
	va_list ap)
{
	string_formatter sf;
	int ret;

	sf.str = str;
	sf.space = size;

	ret = mali_tpi_format_string( string_append, &sf, format, ap );
	if( (sf.space > 0) && ( sf.str!= NULL) )
	{
		*sf.str = '\0';
	}
	return ret;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL int mali_tpi_snprintf(
	char *str,
	size_t size,
	const char *format,
	... )
{
	string_formatter sf;
	int ret;
	va_list ap;

	sf.str = str;
	sf.space = size;

	va_start(ap, format);
	ret = mali_tpi_format_string( string_append, &sf, format, ap );
	va_end(ap);
	if( sf.space > 0 )
	{
		*sf.str = '\0';
	}
	return ret;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL int mali_tpi_puts( const char *str )
{
	return mali_tpi_fputs( str, mali_tpi_stdout() );
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL int mali_tpi_vprintf( const char *format, va_list ap )
{
	mali_tpi_file *mali_stdout = mali_tpi_stdout();
	int ret = mali_tpi_vfprintf( mali_stdout, format, ap );
	if( 0 < ret )
	{
		int flush_ret = mali_tpi_fflush( mali_stdout );
		if( 0 != flush_ret )
		{
			/* Flush failed, so we consider vprintf failed too */
			return -1;
		}
	}
	return ret;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL int mali_tpi_fprintf(
	mali_tpi_file *stream,
	const char *format,
	... )
{
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = mali_tpi_vfprintf( stream, format, ap );
	va_end(ap);
	return ret;
}

/* ------------------------------------------------------------------------- */
MALI_TPI_IMPL int mali_tpi_printf( const char *format, ... )
{
	va_list ap;
	int ret;
	mali_tpi_file *mali_stdout = mali_tpi_stdout();
	assert( NULL != mali_stdout );

	va_start(ap, format);
	ret = mali_tpi_vfprintf( mali_stdout, format, ap );
	va_end(ap);
	if( 0 < ret )
	{
		int flush_ret = mali_tpi_fflush( mali_stdout );
		if( 0 != flush_ret )
		{
			/* Flush failed, so we consider printf failed too */
			return -1;
		}
	}
	return ret;
}
