/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef BACKEND_LIVENESS_PRINTER_H
#define BACKEND_LIVENESS_PRINTER_H

#ifndef NEEDS_STDIO
#define NEEDS_STDIO
#endif

#include "common/essl_system.h"
#include "backend/liveness.h"
#include "common/unique_names.h"

memerr _essl_liveness_printer(FILE *f, live_range *range, unique_name_context *names, unsigned register_width);

#endif
