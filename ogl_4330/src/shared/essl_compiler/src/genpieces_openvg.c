/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_test_system.h"

#include "commandline.h"
#include "compiler.h"
#include "compiler_internal.h"
#include "piecegen_mali200/piecegen_mali200_target.h"
#include "piecegen_mali200/piecegen_mali200_driver.h"

int main(int argc, const char **argv)
{
	char *source_string;
	int *input_lengths;
	int n_inputs;
	mempool_tracker tracker;
	mempool p, *pool = &p;
	shader_kind kind = SHADER_KIND_FRAGMENT_SHADER;
	const char *out_filename = "output.binshader";
	target_descriptor *desc;
	compiler_context *compiler;
	essl_bool verbose = ESSL_FALSE;
	int source_string_report_offset = 0;
	unsigned int hw_rev;
	mali_err_code result;

	_essl_mempool_tracker_init(&tracker, _essl_malloc_wrapper, _essl_free_wrapper);
	if (!_essl_mempool_init(pool, 0, &tracker))
	{
		fprintf(stderr, "Error while allocating memory\n");
		return 1;
	}

	if(argc <= 1)
	{
		fprintf(stderr, "Usage: %s [-DNAME=VALUE] source-files\n", argv[0]);
		_essl_mempool_destroy(pool);
		return 1;
	}

	switch (_essl_parse_commandline(pool, argc, argv, &source_string, &input_lengths, &n_inputs, &source_string_report_offset, &kind, &out_filename, &verbose, &hw_rev))
	{
	case CMD_MEM_ERROR:
		fprintf(stderr, "Error while allocating memory\n");
		_essl_mempool_destroy(pool);
		return 1;
	case CMD_FILE_ERROR:
	case CMD_OPTION_ERROR:
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

	_essl_piecegen_mali200_set_filename(argv[argc-1]);

	if (kind != SHADER_KIND_FRAGMENT_SHADER) {
		fprintf(stderr, "Only fragment shaders are supported by the OpenVG piece generator\n");
		_essl_mempool_destroy(pool);
		return 1;
	}

	desc = _essl_piecegen_mali200_new_target_descriptor(pool, TARGET_FRAGMENT_SHADER);
	if (desc == 0) {
		fprintf(stderr, "Error creating target descriptor\n");
		_essl_mempool_destroy(pool);
		return 1;
	}

	compiler = _essl_new_compiler_for_target(desc, source_string, input_lengths, n_inputs, _essl_malloc_wrapper, _essl_free_wrapper);
	if(compiler == 0)
	{
		fprintf(stderr, "Error while initializing compiler\n");
		_essl_mempool_destroy(pool);
		return 1;

	}
	_essl_set_source_string_report_offset(compiler, source_string_report_offset);

	result = _essl_run_compiler(compiler);
	switch (result) {
	case MALI_ERR_NO_ERROR:
		break;
	case MALI_ERR_OUT_OF_MEMORY:
		fprintf(stderr, "Out of memory\n");
		break;
	case MALI_ERR_FUNCTION_FAILED:
		fprintf(stderr, "Error during compilation\n");
		break;
	}
/*
	if(result == MALI_ERR_NO_ERROR)
	{
		FILE *f = fopen(out_filename, "wb");
		if (f) {
			size_t n = _essl_get_binary_shader_size(compiler);
			u32 *ptr = _essl_mempool_alloc(pool, n);
			n = _essl_get_binary_shader(compiler, ptr, n);
			(void)fwrite(ptr, 1, n, f);
			fclose(f);
		} else {
			fprintf(stderr, "Could not open output file \"%s\"\n", out_filename);

		}
	}
*/
	if(result == MALI_ERR_NO_ERROR || result == MALI_ERR_FUNCTION_FAILED)
	{
		char *error_log = _essl_mempool_alloc(pool, _essl_get_error_log_size(compiler));
		if(error_log == NULL)
		{
			fprintf(stderr, "Out of memory\n");
		} else {
			(void)_essl_get_error_log(compiler, error_log, _essl_get_error_log_size(compiler));
			fprintf(stderr, "%s", error_log);
		}
		fprintf(stdout, "OpenVG shader piece generation: %u error(s), %u warning(s)\n", _essl_get_n_errors(compiler), _essl_get_n_warnings(compiler));
	}

	_essl_destroy_compiler(compiler);

	_essl_mempool_destroy(pool);
	return result == MALI_ERR_NO_ERROR ? 0 : 1;
}
