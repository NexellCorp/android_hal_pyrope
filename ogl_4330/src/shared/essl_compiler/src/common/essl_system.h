/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/* All system includes from the compiler go through this file.
   When porting the compiler to a system without a complete C
   standard library, this is the file to modify.
*/

#ifndef COMMON_ESSL_SYSTEM_H
#define COMMON_ESSL_SYSTEM_H

#ifdef UNIT_TEST
# include "common/essl_test_system.h"
#endif

#include <stdarg.h>
#include <assert.h>

#if defined(DEBUGPRINT) || defined(TIME_PROFILING) || defined(_MSC_VER) || defined(__APPLE__)
# ifndef NEEDS_STDIO
#  define NEEDS_STDIO
# endif
#endif

#if defined(TIME_PROFILING)
# include <sys/time.h>
# ifndef NEEDS_TIME
#  define NEEDS_TIME
# endif
#endif

#ifdef NEEDS_STDIO
# include <stdio.h>
#endif

#ifdef NEEDS_CTYPE
# include <ctype.h>
#endif

#ifdef NEEDS_TIME
# include <time.h>
#endif


/* The definitions below this point (excluding malloc and free) are the only
   ones needed when compiling the online compiler in release mode.
*/

#include <stdlib.h>
/*
#ifndef __size_t
typedef int size_t;
#define __size_t size_t
#endif

double strtod(const char *nptr, char **endptr);
double atof(const char *nptr);
int atoi(const char *nptr);

#ifdef NEEDS_MALLOC
void *malloc(size_t size);
void free(void *ptr);
#endif
*/

#include <string.h>
/*
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
size_t strlen(const char *s);
void *memmove(void *dest, const void *src, size_t n);
char *strstr(const char *haystack, const char *needle);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
*/

#include <math.h>
/*
double sin(double);
double cos(double);
double tan(double);
double asin(double);
double acos(double);
double exp(double);
double log(double);
double sqrt(double);
double fabs(double);
double floor(double);
double ceil(double);
double atan2(double,double);
double pow(double,double);
*/

#ifndef essl_bool
typedef unsigned int essl_bool;
#endif

#ifndef ESSL_FALSE
#define ESSL_FALSE 0
#endif

#ifndef ESSL_TRUE
#define ESSL_TRUE 1
#endif


#ifndef NULL
#define NULL ((void*)0)
#endif

#define IGNORE_PARAM(x) (void)x


/* The snprintf and vsnprintf functions are not part of c89.
   If these functions are missing from the platform, custom versions
   should be written and included with the compiler.
*/
#ifdef _MSC_VER
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#else
#if  defined(__SYMBIAN32__)
IMPORT_C int _mali_sys_snprintf(char* str, unsigned int size, const char * format, ... );
IMPORT_C int _mali_sys_vsnprintf(char* str, unsigned int size, const char * format, va_list ap);
#define snprintf _mali_sys_snprintf
#define vsnprintf _mali_sys_vsnprintf
#elif defined (__APPLE__)
/* snprintf and vsnprintf can be used directly */
#else
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
#endif
#endif

/* static inline is declared differently depending on compiler */
#ifdef _MSC_VER
	#define ESSL_STATIC_INLINE static __inline
	#define ESSL_INLINE __inline
	#define ESSL_STATIC_FORCE_INLINE static __forceinline
#else
	/* using a non-MSVC compiler */
	#define ESSL_STATIC_INLINE static __inline__
	#define ESSL_INLINE __inline__

	#if defined(__ARMCC_VERSION)
		#define ESSL_STATIC_FORCE_INLINE static __forceinline
	#elif defined(__GNUC__)
		#define ESSL_STATIC_FORCE_INLINE static __inline__ __attribute__ ((always_inline))
	#else
		#define ESSL_STATIC_FORCE_INLINE ESSL_STATIC_INLINE
	#endif
#endif



#endif
