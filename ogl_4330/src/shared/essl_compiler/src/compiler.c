/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "compiler.h"
#include "compiler_internal.h"
#include "compiler_context.h"
#include "common/essl_mem.h"
#include "common/essl_target.h"
#include "frontend/frontend.h"
#include "middle/middle.h"
#include "common/error_reporting.h"
#include "common/output_buffer.h"
#include "common/essl_profiling.h"
#include "common/compiler_options.h"
#include "frontend/ast_to_lir.h"

#ifdef DEBUGPRINT
#include "common/middle_printer.h"
#endif


static compiler_context *allocate_compiler_context(alloc_func alloc, free_func free)
{
	compiler_context *ctx;

	ctx = alloc(sizeof(*ctx));
	if (ctx == NULL)
	{
		return NULL;
	}

	_essl_mempool_tracker_init(&ctx->mem_tracker, alloc, free);

	if ((_essl_mempool_init(&ctx->pool, 0, &ctx->mem_tracker)) == MEM_ERROR)
	{
		free(ctx);
		return NULL;
	}

	if ((ctx->options = _essl_new_compiler_options(&ctx->pool)) == 0)
	{
		_essl_mempool_destroy(&ctx->pool);
		free(ctx);
		return NULL;
	}

	if ((_essl_output_buffer_init(&ctx->out_buf, &ctx->pool)) == 0)
	{
		_essl_mempool_destroy(&ctx->pool);
		free(ctx);
		return NULL;
	}

	return ctx;
}

/**
   Allocates and initializes a new compiler context.
   @param kind The shader kind, i.e. a vertex shader or a fragment shader
   @param concatenated_input_string The source strings concatenated together to a single input string
   @param source_string_lengths Array of source string lengths. The size of the array is given by n_source_strings.
   @param n_source_strings Number of source strings
   @param hw_ver the hardware revision to compile for
   @returns A new compiler context, or null if there was an out of memory error.
*/

compiler_context *_essl_new_compiler(shader_kind kind, const char *concatenated_input_string, const int *source_string_lengths, unsigned int n_source_strings, unsigned int hw_rev, alloc_func alloc, free_func free)
{
	compiler_context *ctx;
	target_kind t_kind;

	TIME_PROFILE_START("_total");

	if ((ctx = allocate_compiler_context(alloc, free)) == NULL)
	{
		return NULL;
	}

	/* from now on the context is fully set up memory-wise, and _essl_destroy_compiler can serve as a destructor */

	switch(kind)
	{
	case SHADER_KIND_VERTEX_SHADER:
		t_kind = TARGET_VERTEX_SHADER;
		break;
	case SHADER_KIND_FRAGMENT_SHADER:
		t_kind = TARGET_FRAGMENT_SHADER;
		break;
	default:
		assert(0);
		t_kind = TARGET_FRAGMENT_SHADER;
		break;
	}

	_essl_set_compiler_options_for_hw_rev(ctx->options, hw_rev);

	TIME_PROFILE_START("target_descriptor");
	ctx->desc = _essl_new_target_descriptor(&ctx->pool, t_kind, ctx->options);
	TIME_PROFILE_STOP("target_descriptor");
	if(ctx->desc == 0)
	{
		_essl_destroy_compiler(ctx);
		return NULL;
	}


	TIME_PROFILE_START("_new_frontend");
	ctx->frontend_ctx = _essl_new_frontend(&ctx->pool, ctx->desc, concatenated_input_string, source_string_lengths, n_source_strings);
	TIME_PROFILE_STOP("_new_frontend");
	if(ctx->frontend_ctx == 0)
	{
		_essl_destroy_compiler(ctx);
		return NULL;
	}

   	return ctx;
}


compiler_context *_essl_new_compiler_for_target(target_descriptor *desc, const char *concatenated_input_string, const int *source_string_lengths, unsigned int n_source_strings, alloc_func alloc, free_func free)
{
	compiler_context *ctx;

	if ((ctx = allocate_compiler_context(alloc, free)) == NULL)
	{
		return NULL;
	}

	/* from now on the context is fully set up memory-wise, and _essl_destroy_compiler can serve as a destructor */

	ctx->desc = desc;
	ctx->options = desc->options;

	TIME_PROFILE_START("_new_frontend");
	ctx->frontend_ctx = _essl_new_frontend(&ctx->pool, ctx->desc, concatenated_input_string, source_string_lengths, n_source_strings);
	TIME_PROFILE_STOP("_new_frontend");
	if(ctx->frontend_ctx == NULL)
	{
		_essl_destroy_compiler(ctx);
		return NULL;
	}

	return ctx;
}

/**
   Deallocate and clean up a compiler context
   @param context The context to clean up
*/
void _essl_destroy_compiler(compiler_context *context)
{
	_essl_mempool_destroy(&context->pool);
	context->mem_tracker.free(context);

	TIME_PROFILE_STOP("_total");
}


/**
   Set an option for the compiler.
   @param option the option to set
   @param value the value to set it to
   @returns
            MALI_ERR_NO_ERROR if the option was understood
            MALI_ERR_FUNCTION_FAILED if the option was not understood
*/
mali_err_code _essl_set_compiler_option(compiler_context *context, compiler_option option, int value)
{
	if (_essl_set_compiler_option_value(context->options, option, value))
	{
		return MALI_ERR_NO_ERROR;
	} else {
		return MALI_ERR_FUNCTION_FAILED;
	}
}

static mali_err_code examine_error(compiler_context *ctx)
{
	error_context *err_ctx = ctx->frontend_ctx->err_context;
	if(_essl_error_get_n_errors(err_ctx) > 0) return MALI_ERR_FUNCTION_FAILED;

	/* okay, no errors reported, we need to synthesize one */
	if(ctx->mem_tracker.out_of_memory_encountered)
	{
		REPORT_OOM(err_ctx);
		return MALI_ERR_OUT_OF_MEMORY;
	}

	/* okay, only thing left is an internal compiler error */
	REPORT_ERROR(err_ctx, ERR_INTERNAL_COMPILER_ERROR, 0, "Internal compiler error.\n");
	return MALI_ERR_FUNCTION_FAILED;
}

/**
   Perform a compilation. 
   @param context A compiler context created by _essl_create_compiler
   @returns 
            MALI_ERR_NO_ERROR if everything is okay. A binary shader will be available. There might still be a nonempty compile log, but it will only contain warnings.
			MALI_ERR_OUT_OF_MEMORY if an out of memory condition was encountered. No binary shader available, and the compile log will contain at least one out of memory error.
			MALI_ERR_FUNCTION_FAILED if there was an error in the shader. No binary shader available, and the compile log will contain at least one error.
*/
mali_err_code _essl_run_compiler(compiler_context *ctx)
{
	translation_unit *tu;
	frontend_context *frontend_ctx = ctx->frontend_ctx;
	memerr ret;
	ctx->tu = tu = _essl_run_frontend(ctx->frontend_ctx);

	if(_essl_error_get_n_errors(frontend_ctx->err_context))
	{
		return MALI_ERR_FUNCTION_FAILED;
	}
	
	if(!tu)
	{
		return examine_error(ctx);
	}

	ret = _essl_ast_to_lir(&ctx->pool, frontend_ctx->err_context, frontend_ctx->typestor_context, ctx->desc, ctx->options, tu);

	if (_essl_error_get_n_errors(frontend_ctx->err_context))
	{
		return MALI_ERR_FUNCTION_FAILED;
	}

	if (!ret)
	{
		return examine_error(ctx);
	}


	ret = _essl_middle_transform(&ctx->pool, frontend_ctx->err_context, frontend_ctx->typestor_context, ctx->desc, ctx->options, tu);

	if(_essl_error_get_n_errors(frontend_ctx->err_context)) return MALI_ERR_FUNCTION_FAILED;

	if(!ret)
	{
		return examine_error(ctx);
	}

#ifdef DEBUGPRINT
	{
		FILE *f2 = fopen("out.dot", "w");
		if(f2)
		{
			if(!_essl_debug_translation_unit_to_dot(f2, &ctx->pool, ctx->desc, tu, 0))
			{
				fprintf(stderr, "Error during LIR output\n");
				return examine_error(ctx);
			}
			(void)fclose(f2);
		}
	}
#endif

	ret = ctx->desc->driver(&ctx->pool, frontend_ctx->err_context, frontend_ctx->typestor_context, ctx->desc, tu, &ctx->out_buf, ctx->options);


	if(_essl_error_get_n_errors(frontend_ctx->err_context)) return MALI_ERR_FUNCTION_FAILED;

	if(!ret)
	{
		return examine_error(ctx);
	}

#ifdef DEBUGPRINT
	{
		FILE *f2 = fopen("out-target.dot", "w");
		if(f2)
		{
			if(!_essl_debug_translation_unit_to_dot(f2, &ctx->pool, ctx->desc, tu, 1))
			{
				fprintf(stderr, "Error during LIR output\n");
				tu = 0;

			}
			(void)fclose(f2);
		}
	}
#endif

	return MALI_ERR_NO_ERROR;
}


/*
  Set source string position that errors in the first source string should be reported at. Handy if you inject extra source strings in order to handle -D on the command line.
 */
void _essl_set_source_string_report_offset(compiler_context *ctx, int source_string_report_offset)
{
	int i;
	error_context *e_ctx = ctx->frontend_ctx->err_context;
	preprocessor_context *pre_ctx = &ctx->frontend_ctx->prep_context;

	_essl_error_set_source_string_report_offset(e_ctx, source_string_report_offset);

	pre_ctx->source_body_start = 0;
	for (i = 0 ; i < -source_string_report_offset ; i++)
	{
		pre_ctx->source_body_start += e_ctx->source_string_lengths[i];
	}
}

/**
   Get the size of the error log in bytes. This includes space for null termination.
   @param context The compiler context
   @returns error log size
*/

essl_size_t _essl_get_error_log_size(compiler_context *context)
{
	return (essl_size_t)_essl_error_get_log_size(context->frontend_ctx->err_context);

}

/**
   Copy the error log to memory provided by the caller.
   @param context               The compiler context
   @param error_log_buffer      Memory to copy the error log to. Must have room for at least error_log_buffer_size bytes
   @param error_log_buffer_size The size of the error log buffer
   @returns The number of bytes copied into the buffer. Will be min(_essl_get_error_log_size(context), error_log_buffer_size) bytes.
*/
essl_size_t _essl_get_error_log(compiler_context *context, /*@out@*/ char *error_log_buffer, essl_size_t error_log_buffer_size)
{
	return (essl_size_t)_essl_error_get_log(context->frontend_ctx->err_context, error_log_buffer, error_log_buffer_size);

}

/**
   Get the number of error messages produces by the compilation.
   @param context               The compiler context
   @returns The number of errors.
*/
int _essl_get_n_errors(compiler_context *context)
{
	return _essl_error_get_n_errors(context->frontend_ctx->err_context);
}

/**
   Get the number of warning messages produces by the compilation.
   @param context               The compiler context
   @returns The number of warnings.
*/
int _essl_get_n_warnings(compiler_context *context)
{
	return _essl_error_get_n_warnings(context->frontend_ctx->err_context);
}

/**
   Get the size of the resulting binary shader in bytes.
   @param context The compiler context
   @returns binary shader size or 0 if there is no binary shader available.
*/
essl_size_t _essl_get_binary_shader_size(compiler_context *context)
{
	return (essl_size_t)_essl_output_buffer_get_size(&context->out_buf) * sizeof(u32);

}

/**
   Copy the binary shader to memory provided by the caller.
   @param context                   The compiler context
   @param binary_shader_buffer      Memory to copy the binary shader to. Must have room for at least binary_shader_buffer_size bytes
   @param binary_shader_buffer_size The size of the binary shader buffer
   @returns The number of bytes copied into the buffer. Will be min(_essl_get_binary_shader_size(context), binary_shader_buffer_size) bytes.
*/

essl_size_t _essl_get_binary_shader(compiler_context *context, /*@out@*/ void *binary_shader_buffer, essl_size_t binary_shader_buffer_size)
{
	essl_size_t n_bytes = (essl_size_t)_essl_output_buffer_get_size(&context->out_buf) * sizeof(u32);
	if(binary_shader_buffer_size < n_bytes) n_bytes = binary_shader_buffer_size;
	memcpy(binary_shader_buffer, _essl_output_buffer_get_raw_pointer(&context->out_buf), n_bytes);
	return n_bytes;

}




