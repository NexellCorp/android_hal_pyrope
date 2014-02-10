/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef _GLES2_FRAMEWORK_PARAMETERS_
#define _GLES2_FRAMEWORK_PARAMETERS_

/* list of configs we want to test */

#include "suite.h"
#include "test_helpers.h"
#include "gles_helpers.h"

enum gles_framework_config_pattern_flags
{
	CONFIG_FLAG_CUSTOM = 1 << 0, /**< This flag is set for the commandline config only, to mark it as the commandline config */
	CONFIG_FLAG_FSAA = 1 << 1, /**< Configs using FSAA are flagged with this flag. Some tests cannot run in FSAA */
	CONFIG_FLAG_DIMENSION_SMALLER_THAN_16x16 = 1 << 2, /**< Most tests need at least 16x16 pixels resolution to work, this flag is set on configs not matching that */
	CONFIG_FLAG_DIMENSION_SMALLER_THAN_32x32 = 1 << 3, /**< Most tests need at least 32x32 pixels resolution to work, this flag is set on configs not matching that */
	CONFIG_FLAG_DIMENSION_SMALLER_THAN_64x64 = 1 << 4, /**< Similarly, for 64x64. All configs should run on resolutions above this */
	CONFIG_FLAG_DIMENSION_SMALLER_THAN_128x128 = 1 << 5, /** Similarly for 128x128. All configs should run on resolutions above this  */
	CONFIG_FLAG_DIMENSION_NOT_128x128 = 1 << 6, /**< Some tests are written to only work in 128x128. While not optimal, this flag marks all configs not that size */
	CONFIG_FLAG_DIMENSION_NOT_DIVISIBLE_BY_16 = 1 << 7, /**< Some config dimensions are not block aligned. Some tests will fail on those */
	CONFIG_FLAG_RENDER_TO_FBO = 1 << 8, /**< Configs with this flag render directly to an FBO instead of the monitor */
	CONFIG_FLAG_HEIGHT_DIFFERFROM_WIDTH = 1 << 9 /**< Some tests want square output resolutions */
};


#define GLES_FRAMEWORK_CONFIG_NAME_LENGTH 256
struct gles_framework_config
{
	const char* tagname;
	char name[GLES_FRAMEWORK_CONFIG_NAME_LENGTH];
	unsigned int width, height;
	unsigned int red_bits, green_bits, blue_bits, alpha_bits;
	unsigned int fsaa;
	unsigned int pattern_flags; /* These flags parameterize the capabilities of this config */
};

/**
 * @brief Returns information about a builtin config
 * @param index The index of the config we want to examine.
 * @param name Output parameter yielding the name of the config
 * @param width Output parameter yielding the width of the config
 * @param height Output parameter yielding the height of the config
 * @param rgba Output array of four elements yielding the bit depths of this config
 * @param multisample Output value yielding either 1, 4 or 16
 * @param flags Output value yielding the config flags
 * @return 0 on success, 1 if index was too high.
 * @note typically called in the gl2_create_fixture function.
 */
int gl2_get_config_information(unsigned int index, const char** name,
			int* width, int* height, int* rgba,
			int* multisample, unsigned int* flags);



/**
 * @brief disables certain configs for a suite
 * @param suite the suite to exclude configs for
 * @param exclusion_bitfield a bitfield of values defined in the gles_framework_config_pattern_flags enum.
 * @note typically called in each test suite
 * @example gl2_set_config_exclusions(mysuite, CONFIG_SMALL | CONFIG_FSAA); -- disables all small or FSAA-enabled configs for this suite
 */
void gl2_set_config_exclusions(suite* test_suite, unsigned int exclusion_bitfield);


/**
 * @brief parser function for gles2 api testbenches.
 * @param test_spec the testspec structure containing the parsed arguments in binary form
 * @param pool The current memory pool
 * @param string An input string containing all the parameters
 * @note This function is used by the defect and API test suite and passed as a custom parameter parser
 */
int gles2_parse_args( test_specifier* test_spec, mempool* pool, const char* string);




#endif


