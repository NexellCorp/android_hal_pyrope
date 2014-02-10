/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */
#ifndef ESSL_COMPILER_H
#define ESSL_COMPILER_H

/*
  ESSL Online compiler interface.

  Warning: If your project uses mali_types.h, please include mali_types.h before this header.
*/


/*
  Typical usage:

  #include "mali_types.h"
  #include "compiler.h"

  ...


  mali_err_code ret;
  compiler_context *ctx;
  int lengths[2];
  size_t error_log_size;
  char *error_buf;
  lengths[0] = first_source_string_length;
  lengths[1] = second_source_string_length;
  ctx = _essl_create_compiler_context(SHADER_KIND_FRAGMENT_SHADER, concatenated_shader_string, lengths, 2);
  if(!ctx) return MALI_ERR_OUT_OF_MEMORY;

  ret = _essl_run_compiler(ctx);

  error_log_size = _essl_get_error_log_size(ctx);
  error_buf = malloc(error_log_size * sizeof(char));
  if(!error_buf)
  {
    ret = MALI_ERR_OUT_OF_MEMORY;
  } else {
    _essl_get_error_log(ctx, error_buf, error_log_size);
  }

  if(ret == MALI_ERR_NO_ERROR)
  {
    size_t binary_shader_size;
    void *binary_shader;

    binary_shader_size = _essl_get_binary_shader_size(ctx);

	binary_shader = malloc(binary_shader_size*sizeof(char));
	if(!binary_shader)
	{
	  ret = MALI_ERR_OUT_OF_MEMORY;
	} else {
	  _essl_get_binary_shader(ctx, binary_shader, binary_shader_size);
	}

  }

  _essl_destroy_compiler(ctx);
  return ret;


*/


#ifndef _MALI_TYPES_H_
/**
 * Error codes from mali:
 */
typedef enum mali_err_code
{
    MALI_ERR_NO_ERROR           =  0,
    MALI_ERR_OUT_OF_MEMORY      = -1,
    MALI_ERR_FUNCTION_FAILED    = -2
} mali_err_code;
#endif /* _MALI_TYPES_H_ */

typedef unsigned int essl_size_t;


typedef struct _compiler_context compiler_context;


/**
   Language variant of the target
*/

typedef enum {
	SHADER_KIND_VERTEX_SHADER, /**< Shader is a vertex shader */
	SHADER_KIND_FRAGMENT_SHADER /**< Shader is a fragment shader */
} shader_kind;


/**
   Compiler option tags
*/

typedef enum {
	COMPILER_OPTION_UNKNOWN = 0,
	/** Size of MaliGP2 instruction space (int, default 512) */
	COMPILER_OPTION_NUM_MALIGP2_INSTRUCTIONS,
	/** Number of MaliGP2 work registers (int, default 16) */
	COMPILER_OPTION_NUM_MALIGP2_WORK_REGISTERS,
	/** Number of MaliGP2 constant registers (int, default 304) */
	COMPILER_OPTION_NUM_MALIGP2_CONSTANT_REGISTERS,
	/** Maximum number of iterations in the MaliGP2 register allocator (int, default 20) */
	COMPILER_OPTION_MAX_MALIGP2_REGALLOC_ITERATIONS,
	/** Number of Mali200 registers (int, default 6) */
	COMPILER_OPTION_NUM_MALI200_REGISTERS,
	/** Function inlining enabled? (essl_bool, default ESSL_TRUE) */
	COMPILER_OPTION_ENABLE_FUNCTION_INLINING,
	/** Conditional select optimisation enabled? (essl_bool, default ESSL_TRUE) */
	COMPILER_OPTION_OPTIMISE_CONDITIONAL_SELECT,
	/** Global variable inlining optimisation enabled? (essl_bool, default ESSL_TRUE) */
	COMPILER_OPTION_OPTIMISE_GLOBAL_VARIABLES,
	/** Loop entry optimisation enabled? (essl_bool, default ESSL_TRUE) */
	COMPILER_OPTION_OPTIMISE_LOOP_ENTRY,
	/** Store->load optimisation enabled? (essl_bool, default ESSL_TRUE) */
	COMPILER_OPTION_OPTIMISE_STORE_LOAD_FORWARDING,

	/** Enable workaround for the MaliGP2 add bug (essl_bool, default ESSL_FALSE) */
	COMPILER_OPTION_MALIGP2_ADD_WORKAROUND,
	/** Enable workaround for the MaliGP2 exp2 bug (essl_bool, default ESSL_FALSE) */
	COMPILER_OPTION_MALIGP2_EXP2_WORKAROUND,
	/** Enable workaround for the Mali200 store bug (essl_bool, default ESSL_FALSE) */
	COMPILER_OPTION_MALI200_STORE_WORKAROUND,
	/** Enable workaround for the MaliGP2 constant register store bug (essl_bool, default ESSL_FALSE) */
	COMPILER_OPTION_MALIGP2_CONSTANT_STORE_WORKAROUND,
	/** Enable error/warning report when the Mali200 backend emits unsafe stores (essl_bool, default ESSL_FALSE) */
	COMPILER_OPTION_MALI200_UNSAFE_STORE_REPORT,
	/** Errors or just warnings when the Mali200 backend emits unsafe stores (essl_bool, default ESSL_TRUE) */
	COMPILER_OPTION_MALI200_UNSAFE_STORE_ERROR,
	/* Enable workaround for work register memories not supporting simultaneous read and write (essl_bool, default ESSL_TRUE) */
	COMPILER_OPTION_MALIGP2_WORK_REG_READWRITE_WORKAROUND,
	/* Enable workaround for constant register memories not supporting simultaneous read and write (essl_bool, default ESSL_TRUE) */
	COMPILER_OPTION_MALIGP2_CONSTANT_REG_READWRITE_WORKAROUND,

	/* Scale and bias gl_PointCoord by a magic uniform (essl_bool, default ESSL_FALSE) */
	COMPILER_OPTION_MALI200_POINTCOORD_SCALEBIAS,
	/* Scale gl_FragCoord by a magic uniform (essl_bool, default ESSL_FALSE) */
	COMPILER_OPTION_MALI200_FRAGCOORD_SCALE,
	/* Scale derivatives by a magic uniform (essl_bool, default ESSL_FALSE) */
	COMPILER_OPTION_MALI200_DERIVATIVE_SCALE,
	/** Enable workaround for the Mali200 add with scale overflow bug (essl_bool, default ESSL_FALSE) */
	COMPILER_OPTION_MALI200_ADD_WITH_SCALE_OVERFLOW_WORKAROUND

} compiler_option;


/**
   Hardware revision, 32 bit integer: (core << 16) | (release << 8) | patch
*/
#define HW_REV_CORE(a) (((a) >> 16) & 0xFF)
#define HW_REV_RELEASE(a) (((a) >> 8) & 0xFF)
#define HW_REV_PATCH(a) ((a) & 0xFF)

#define HW_REV_CORE_MALI200 0
#define HW_REV_CORE_MALI400 1
#define HW_REV_CORE_MALI300 2
#define HW_REV_CORE_MALI450 3
#define HW_REV_MALI200_R0P1 0x000001
#define HW_REV_MALI200_R0P2 0x000002
#define HW_REV_MALI200_R0P3 0x000003
#define HW_REV_MALI200_R0P4 0x000004
#define HW_REV_MALI200_R0P5 0x000005
#define HW_REV_MALI200_R0P6 0x000006
#define HW_REV_MALI400_R0P0 0x010000
#define HW_REV_MALI400_R0P1 0x010001
#define HW_REV_MALI400_R1P0 0x010100
#define HW_REV_MALI400_R1P1 0x010101
#define HW_REV_MALI300_R0P0 0x020000
#define HW_REV_MALI450_R0P0 0x030000

#define HW_REV_CORE_DEFAULT HW_REV_CORE_MALI200
#define HW_REV_MALI200_DEFAULT HW_REV_MALI200_R0P5
#define HW_REV_MALI400_DEFAULT HW_REV_MALI400_R0P0
#define HW_REV_DEFAULT HW_REV_MALI200_R0P5
#define HW_REV_GENPIECES HW_REV_MALI200_R0P1



/**
   Memory allocation and deallocation functions
*/
#ifndef ALLOCATION_FUNCTION_TYPES
#define ALLOCATION_FUNCTION_TYPES
typedef void * (*alloc_func)(essl_size_t size);
typedef void (*free_func)(void *address);
#endif


/**
   Allocates and initializes a new compiler context.
   @param kind The shader kind, i.e. a vertex shader or a fragment shader
   @param concatenated_input_string The source strings concatenated together to a single input string
   @param source_string_lengths Array of source string lengths. The size of the array is given by n_source_strings.
   @param n_source_strings Number of source strings
   @param hw_rev the hardware revision to compile for
   @returns A new compiler context, or null if there was an out of memory error.
*/

compiler_context *_essl_new_compiler(shader_kind kind, const char *concatenated_input_string, const int *source_string_lengths, unsigned int n_source_strings, unsigned int hw_rev, alloc_func alloc, free_func free);

/**
   Deallocate and clean up a compiler context
   @param context The context to clean up
*/
void _essl_destroy_compiler(compiler_context *context);


/**
   Set an internal option for the compiler.
   @param option the option to set
   @param value the value to set it to
   @returns
            MALI_ERR_NO_ERROR if the option was understood
            MALI_ERR_FUNCTION_FAILED if the option was not understood
*/
mali_err_code _essl_set_compiler_option(compiler_context *context, compiler_option option, int value);


/**
   Perform a compilation.
   @param context A compiler context created by _essl_create_compiler
   @returns
            MALI_ERR_NO_ERROR if everything is okay. A binary shader will be available. There might still be a nonempty compile log, but it will only contain warnings.
			MALI_ERR_OUT_OF_MEMORY if an out of memory condition was encountered. No binary shader available, and the compile log will contain at least one out of memory error.
			MALI_ERR_FUNCTION_FAILED if there was an error in the shader. No binary shader available, and the compile log will contain at least one error.
*/
mali_err_code _essl_run_compiler(compiler_context *context);


/*
  Set source string position that errors in the first source string should be reported at. Handy if you inject extra source strings in order to handle -D on the command line.
  @param source_string_report_offset position of first source string
 */
void _essl_set_source_string_report_offset(compiler_context *ctx, int source_string_report_offset);

/**
   Get the size of the error log in bytes. This includes space for null termination.
   @param context The compiler context
   @returns error log size
*/

essl_size_t _essl_get_error_log_size(compiler_context *context);

/**
   Copy the error log to memory provided by the caller.
   @param context               The compiler context
   @param error_log_buffer      Memory to copy the error log to. Must have room for at least error_log_buffer_size bytes
   @param error_log_buffer_size The size of the error log buffer
   @returns The number of bytes copied into the buffer. Will be min(_essl_get_error_log_size(context), error_log_buffer_size) bytes.
*/
essl_size_t _essl_get_error_log(compiler_context *context, /*@out@*/ char *error_log_buffer, essl_size_t error_log_buffer_size);

/**
   Get the number of error messages produces by the compilation.
   @param context               The compiler context
   @returns The number of errors.
*/
int _essl_get_n_errors(compiler_context *context);

/**
   Get the number of warning messages produces by the compilation.
   @param context               The compiler context
   @returns The number of warnings.
*/
int _essl_get_n_warnings(compiler_context *context);


/**
   Get the size of the resulting binary shader in bytes.
   @param context The compiler context
   @returns binary shader size or 0 if there is no binary shader available.
*/
essl_size_t _essl_get_binary_shader_size(compiler_context *context);

/**
   Copy the binary shader to memory provided by the caller.
   @param context                   The compiler context
   @param binary_shader_buffer      Memory to copy the binary shader to. Must have room for at least binary_shader_buffer_size bytes
   @param binary_shader_buffer_size The size of the binary shader buffer
   @returns The number of bytes copied into the buffer. Will be min(_essl_get_binary_shader_size(context), binary_shader_buffer_size) bytes.
*/
essl_size_t _essl_get_binary_shader(compiler_context *context, /*@out@*/ void *binary_shader_buffer, essl_size_t binary_shader_buffer_size);



#endif /* ESSL_COMPILER_H */

