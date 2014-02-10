/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*
  Internal compiler options are specified here. The syntax is:

  COMPILER_OPTION(name, field name, type, default value)

  Every option must also be declared in the compiler_option enum in
  compiler.h.
*/


/* Resource limits */
COMPILER_OPTION(NUM_MALIGP2_INSTRUCTIONS, n_maligp2_instruction_words, int, 512)
COMPILER_OPTION(NUM_MALIGP2_WORK_REGISTERS, n_maligp2_work_registers, int, 16)
COMPILER_OPTION(NUM_MALIGP2_CONSTANT_REGISTERS, n_maligp2_constant_registers, int, 304)
COMPILER_OPTION(MAX_MALIGP2_REGALLOC_ITERATIONS, max_maligp2_regalloc_iterations, int, 20)
COMPILER_OPTION(NUM_MALI200_REGISTERS, n_mali200_registers, int, 6)

/* Optimisations */
COMPILER_OPTION(ENABLE_FUNCTION_INLINING, optimise_inline_functions, essl_bool, ESSL_TRUE)
COMPILER_OPTION(OPTIMISE_CONDITIONAL_SELECT, optimise_conditional_select, essl_bool, ESSL_TRUE)
COMPILER_OPTION(OPTIMISE_GLOBAL_VARIABLES, optimise_global_variables, essl_bool, ESSL_TRUE)
COMPILER_OPTION(OPTIMISE_LOOP_ENTRY, optimise_loop_entry, essl_bool, ESSL_TRUE)
COMPILER_OPTION(OPTIMISE_STORE_LOAD_FORWARDING, optimise_store_load_forwarding, essl_bool, ESSL_TRUE)

/* Workarounds */

/* r0p1 causes incorrect results when subtracting the largest representable 
   value smaller than a power of two from that power of two (bugzilla 3400).
   Currently no workaround implemented.
*/
COMPILER_OPTION(MALIGP2_ADD_WORKAROUND, maligp2_add_workaround, essl_bool, ESSL_FALSE)

/* r0p2 causes exp2(-1) to return minus infinity (bugzilla 3607).
*/
COMPILER_OPTION(MALIGP2_EXP2_WORKAROUND, maligp2_exp2_workaround, essl_bool, ESSL_FALSE)

/* r0p2 causes stores to read input values from different register components 
   depending on the destination address (bugzilla 3592).
*/
COMPILER_OPTION(MALI200_STORE_WORKAROUND, mali200_store_workaround, essl_bool, ESSL_FALSE)

/* r0p2 causes a constant register store to always store both components
   of stored half-registers, ignoring the output selectors (bugzilla 3688).
*/
COMPILER_OPTION(MALIGP2_CONSTANT_STORE_WORKAROUND, maligp2_constant_store_workaround, essl_bool, ESSL_FALSE)

/* r0p2 causes mali200 to sometimes lose stores and potentially hang the core (bugzilla 3558).
   Not a workaround, just an option to report unsafe stores.
*/
COMPILER_OPTION(MALI200_UNSAFE_STORE_REPORT, mali200_unsafe_store_report, essl_bool, ESSL_FALSE)

/* r0p2 causes mali200 to sometimes lose stores and potentially hang the core (bug 3558).
   Decides whether unsafe stores should be reported as errors(ESSL_TRUE) or warnings(ESSL_FALSE)
*/
COMPILER_OPTION(MALI200_UNSAFE_STORE_ERROR, mali200_unsafe_store_error, essl_bool, ESSL_TRUE)

/* Bug in some memory implementations which causes the result of a work register read
   at the same time as a write to the same half-register to be undefined.
*/
COMPILER_OPTION(MALIGP2_WORK_REG_READWRITE_WORKAROUND, maligp2_work_reg_readwrite_workaround, essl_bool, ESSL_FALSE)

/* Bug in some memory implementations which causes the result of a constant register read
   at the same time as a write to the same register to be undefined.
*/
COMPILER_OPTION(MALIGP2_CONSTANT_REG_READWRITE_WORKAROUND, maligp2_constant_reg_readwrite_workaround, essl_bool, ESSL_FALSE)


/* Missing feature in all Mali200 revisions to flip point sprites when rendering upside down
   and to scale point coordinates depending on framebuffer downscaling.
*/
COMPILER_OPTION(MALI200_POINTCOORD_SCALEBIAS, mali200_pointcoord_scalebias, essl_bool, ESSL_FALSE)

/* Missing feature in all Mali200 revisions to scale gl_FragCoord depending on
   framebuffer downscaling.
*/
COMPILER_OPTION(MALI200_FRAGCOORD_SCALE, mali200_fragcoord_scale, essl_bool, ESSL_FALSE)

/* Missing feature in all Mali200 revisions to scale derivatives depending on
   framebuffer downscaling.
*/
COMPILER_OPTION(MALI200_DERIVATIVE_SCALE, mali200_derivative_scale, essl_bool, ESSL_FALSE)


/* Mali200 bug where add with post-scale would give the wrong result in overflow situations.
*/
COMPILER_OPTION(MALI200_ADD_WITH_SCALE_OVERFLOW_WORKAROUND, mali200_add_with_scale_overflow_workaround, essl_bool, ESSL_FALSE)
