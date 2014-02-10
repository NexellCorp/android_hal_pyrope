/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_test_system.h"
#include "commandline.h"
#include "compiler.h"
#include "compiler_context.h"
#include "common/essl_profiling.h"
#include "backend/static_cycle_count.h"
#include "compiler_version.h"

static void usage(const char *exe_name)
{
#ifdef NEXELL_FEATURE_ENABLE

#else
		fprintf(stderr, 
				"ARM Mali Offline Shader Compiler " COMPILER_VERSION_STRING "\n"
				"(C) Copyright 2007-2010 ARM Limited.\n"
				"All rights reserved\n"
				"\n"
				"Usage: %s [options] [-o outfile] source-file\n"
				"  -DNAME[=VALUE]             Predefine NAME as a macro, with definition VALUE.\n"
				"  --vert                     Process shader as a vertex shader.\n"
				"  --frag                     Process shader as a fragment shader.\n"
				"  -v, --verbose              Print verbose information about the compiled shader.\n"
				"  -o outfile                 Write output to outfile. Default: output.binshader.\n"
				"  --core=core                Target specified graphics core.\n"
				"                             Supported cores are:\n"
                "                               Mali-200\n"
                "                               Mali-400\n"
				"                               Mali-300\n"
#ifdef ENABLE_MALI450_SUPPORT
				"                               Mali-450\n"
#endif
				"  -r rXpY, --revision=rXpY   Target hardware release X, patch Y.\n"
				"  --testchip                 Same as --core=Mali-200 --revision=r0p1.\n"
				, exe_name);
#endif
}

#ifdef _MSC_VER
__declspec(dllexport)
#endif
int main(int argc, const char **argv)
{
	char *source_string;
	int *input_lengths;
	int n_inputs;
	essl_bool verbose = ESSL_FALSE;
	unsigned int hw_rev;
	mempool_tracker tracker;
	mempool p, *pool = &p;
	shader_kind kind = SHADER_KIND_FRAGMENT_SHADER;
	const char *out_filename = "output.binshader";
	compiler_context *compiler;
	mali_err_code result;
	int source_string_report_offset = 0;
#ifndef NDEBUG
	essl_bool showmemory = ESSL_FALSE;
	int fail_on_allocation = -1;
#endif

	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	if (!_essl_mempool_init(pool, 0, &tracker))
	{
		fprintf(stderr, "Error while allocating memory\n");
		return 1;
	}

	if(argc <= 1)
	{
		usage(argv[0]);
		_essl_mempool_destroy(pool);
		return 1;
	}
#ifndef NDEBUG
	while(argc >= 2)
	{
		if(0 == strcmp(argv[1], "--showmemory"))
		{
			++argv;
			--argc;
			showmemory = ESSL_TRUE;
		} else if(0 == strcmp(argv[1], "--fail-alloc") && argc >= 3)
		{
			fail_on_allocation = atoi(argv[2]);
			argv += 2;
			argc -= 2;
		} else {
			break;
		}
	}
#endif

	switch (_essl_parse_commandline(pool, argc, argv, &source_string, &input_lengths, &n_inputs, &source_string_report_offset, &kind, &out_filename, &verbose, &hw_rev))
	{
	case CMD_MEM_ERROR:
		fprintf(stderr, "Error while allocating memory\n");
		_essl_mempool_destroy(pool);
		return 1;
	case CMD_OPTION_ERROR:
		usage(argv[0]);
		_essl_mempool_destroy(pool);
		return 1;

	case CMD_FILE_ERROR:
		/* Already printed error message */
		_essl_mempool_destroy(pool);
		return 1;
	case CMD_KIND_ERROR:
		fprintf(stderr, "Cannot mix vertex and fragment shaders\n");
		_essl_mempool_destroy(pool);
		return 1;
	case CMD_OK:
		break;
	}

	TIME_PROFILE_INIT();

	compiler = _essl_new_compiler(kind, source_string, input_lengths, n_inputs, hw_rev, _essl_malloc_wrapper, _essl_free_wrapper);
	if(compiler == 0)
	{
		fprintf(stderr, "Error while initializing compiler\n");
		_essl_mempool_destroy(pool);
		return 1;
	}
	_essl_set_source_string_report_offset(compiler, source_string_report_offset);
#ifndef NDEBUG
	if(fail_on_allocation != -1)
	{
		compiler->mem_tracker.fail_on_allocation = compiler->mem_tracker.allocations + 1 + fail_on_allocation;
	}
#endif

	if (_essl_parse_commandline_compiler_options(argc, argv, compiler->options, NULL) == -1)
	{
		_essl_mempool_destroy(pool);
		return 1;
	}

	result = _essl_run_compiler(compiler);

	if(result == MALI_ERR_NO_ERROR)
	{
		FILE *f = fopen(out_filename, "wb");
		if (f) {
			size_t n = _essl_get_binary_shader_size(compiler);
			u32 *ptr = _essl_mempool_alloc(pool, n);
			if(ptr == NULL)
			{
				fprintf(stderr, "Out of memory\n");
			} else {
				n = _essl_get_binary_shader(compiler, ptr, n);
				(void)fwrite(ptr, 1, n, f);
			}
			fclose(f);
		} else {
			fprintf(stderr, "Could not open output file \"%s\"\n", out_filename);
			_essl_destroy_compiler(compiler);

			_essl_mempool_destroy(pool);
			return 1;
		}
	}

	{
		char *error_log = _essl_mempool_alloc(pool, _essl_get_error_log_size(compiler));
		if(error_log)
		{
			assert(error_log != 0);
			(void)_essl_get_error_log(compiler, error_log, _essl_get_error_log_size(compiler));
			fprintf(stderr, "%s", error_log);
			fprintf(stderr, "%u error(s), %u warning(s)\n", _essl_get_n_errors(compiler), _essl_get_n_warnings(compiler));
		} else {
			fprintf(stderr, "Out of memory\n");
		}

		if(result == MALI_ERR_NO_ERROR && verbose)
		{
			if(compiler->desc->custom_cycle_count)
			{
				compiler->desc->custom_cycle_count(pool, compiler->tu);
			} else {
				static_cycle_counts counts;
				if(_essl_calc_static_cycle_counts(pool, compiler->tu, &counts) == MEM_ERROR)
				{
					fprintf(stderr, "Out of memory\n");
				} else {
					fprintf(stdout, "\n");
					fprintf(stdout, "Number of instruction words emitted: %d\n", counts.n_instruction_words);
					fprintf(stdout, "Number of cycles for shortest code path: %d\n", counts.min_number_of_cycles);
					fprintf(stdout, "Number of cycles for longest code path: ");
					
					if(counts.max_cycles_unknown)
					{
						fprintf(stdout, "unknown (the shader contains loops)\n");
					} else {
						fprintf(stdout, "%d\n", counts.max_number_of_cycles);
					}
					if(kind == SHADER_KIND_FRAGMENT_SHADER)
					{
						fprintf(stdout, "Note: The cycle counts do not include possible stalls due to cache misses.\n");
					}
				}
			}
		}
	}

#ifndef NDEBUG
	if(showmemory)
	{
		fprintf(stdout, "Memory allocations: %d\n", compiler->mem_tracker.allocations);
	}
#endif


	_essl_destroy_compiler(compiler);

	TIME_PROFILE_DUMP(stdout);

	_essl_mempool_destroy(pool);
	return result == MALI_ERR_NO_ERROR ? 0 : 1;
}
