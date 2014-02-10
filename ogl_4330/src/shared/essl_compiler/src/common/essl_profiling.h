/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_PROFILING_H
#define COMMON_PROFILING_H

#include "common/essl_system.h"

#ifdef TIME_PROFILING

#define TIME_PROFILE_INIT() _essl_time_profile_init()
#define TIME_PROFILE_START(s) _essl_time_profile_start(s)
#define TIME_PROFILE_STOP(s) _essl_time_profile_stop(s)
#define TIME_PROFILE_DUMP(f) _essl_time_profile_dump(f)

void _essl_time_profile_init(void);
void _essl_time_profile_start(char *s);
void _essl_time_profile_stop(char *s);
void _essl_time_profile_dump(FILE *f);

#else

#define TIME_PROFILE_INIT()
#define TIME_PROFILE_START(s)
#define TIME_PROFILE_STOP(s)
#define TIME_PROFILE_DUMP(f)

#endif

#endif
