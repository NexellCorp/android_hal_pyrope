/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMANDLINE_H
#define COMMANDLINE_H

#include "common/essl_mem.h"
#include "common/essl_common.h"
#include "common/compiler_options.h"
#include "compiler.h"
#include "compiler_context.h"


typedef int commandline_ret;
#define CMD_OK 1
#define CMD_MEM_ERROR 0
#define CMD_FILE_ERROR -1
#define CMD_KIND_ERROR -2
#define CMD_OPTION_ERROR -3

commandline_ret _essl_parse_commandline(mempool *pool, int argc, const char **argv, char **shader_source, int **lengths, int *nsources, int *source_string_report_offset, shader_kind *kind, const char **out_filename, essl_bool *verbose, unsigned int *hw_rev);


compiler_option _essl_parse_compiler_option(const char *name, int *value_out);

/**
   Set options for the compiler based on commandline arguments.
   Each option has the form -XOPTION=VALUE.
   Arguments not starting with -X are ignored.
   @returns the number of options found and set, or -1 if an invalid option name is encountered.
*/
int _essl_parse_commandline_compiler_options(int argc, const char **argv, compiler_options *opts, unsigned int *hw_rev_ret);

#endif
