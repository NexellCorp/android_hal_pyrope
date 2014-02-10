/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef BACKEND_ARITHMETIC_H
#define BACKEND_ARITHMETIC_H

#include "common/essl_target.h"

scalar_type _essl_backend_constant_fold(const struct _tag_type_specifier *type, enum _tag_expression_operator op, scalar_type a, scalar_type b, scalar_type c);
void _essl_backend_constant_fold_sized(enum _tag_expression_operator op, scalar_type *ret, unsigned int size, scalar_type *a, /*@null@*/ scalar_type *b, /*@null@*/ scalar_type *c, const type_specifier *atype, /*@null@*/ const type_specifier *btype);
scalar_type _essl_backend_float_to_scalar(float a);
scalar_type _essl_backend_int_to_scalar(int a);
scalar_type _essl_backend_bool_to_scalar(int a);
float _essl_backend_scalar_to_float(scalar_type a);
int _essl_backend_scalar_to_int(scalar_type a);
int _essl_backend_scalar_to_bool(scalar_type a);
scalar_type _essl_backend_convert_scalar(const struct _tag_type_specifier *returntype, scalar_type value);
unsigned int _essl_clz32( unsigned int inp);

#endif /* BACKEND_ARITHMETIC_H */
