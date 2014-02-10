/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_STR_H
#define COMMON_STR_H

#include "common/essl_mem.h"

/** String stuct, contains a pointer to character contents and the length of the string */
typedef struct {
	const char *ptr;
	int len;
} string;

/** Compares the two strings 'a' and 'b', returns 0 if they match and nonzero if they differ. */
int _essl_string_cmp(string a, string b);

/** Compares a string with a c-like string. The specified number of chars are considered */
int _essl_string_cstring_count_cmp(const string a, const char *b, size_t count);

/** Converts a string to a zero-terminated c-like string */
const char *_essl_string_to_cstring(mempool *p, string s);

/** Converts a zero-terminated c-like string to a string, copying contents */
string _essl_cstring_to_string(mempool *p, const char *str);

/** Converts a zero-terminated c-like string to a string, pointing to the same data */
string _essl_cstring_to_string_nocopy(const char *str);

#endif /* COMMON_STR_H */
