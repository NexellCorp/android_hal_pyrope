/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_ERROR_CODES_H
#define COMMON_ERROR_CODES_H


typedef enum {
#define PROCESS(v, n) v,
#include "common/error_names.h"
#undef PROCESS
	ERR_END
} error_code;


#endif /* COMMON_ERROR_CODES_H */
