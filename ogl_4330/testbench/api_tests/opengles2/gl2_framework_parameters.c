/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2001-2002, 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gl2_framework_parameters.h"

/* this is the default config list run on all api tests (which doesn't specify their own) */
static struct gles_framework_config gles_default_config_list[] =
{
	{ "Commandline", {0}, 128, 128, 5, 6, 5, 0, 0, CONFIG_FLAG_CUSTOM },
	{ "128x128x16bit", {0} , 128, 128, 5, 6, 5, 0, 0, 0 },
	{ "128x128x32bit", {0}, 128, 128, 8, 8, 8, 8, 0, 0 },
	{ "Khronos_non_standard_resolution", {0}, 47, 113, 5, 6, 5, 0, 0, 0 },
	{ "4x", {0}, 128, 128, 5, 6, 5, 0, 4, 0 },
	{ "16x", {0}, 128, 128, 5, 6, 5, 0, 16, 0 },
	/*{ "720p", {0}, 1280, 720, 5, 6, 5, 0, 0, 0 },*//* Removed for now due to defect 7158 and testfarm timing considerations */
	{ "FBO", {0}, 128, 128, 5, 6, 5, 0, 0, CONFIG_FLAG_RENDER_TO_FBO },
	{ "32bit_16x", {0}, 128, 128, 8, 8, 8, 8, 16, 0 },
	/*
	{ "1024x512x16bit", {0}, 1024, 512, 5, 6, 5, 0, 0, 0 },
	{ "1024x512x32bit", {0}, 1024, 512, 8, 8, 8, 8, 0, 0 },
	{ "2048x2048x16bit", {0}, 2048, 2048, 5, 6, 5, 0, 0, 0 },
	{ "2048x2048x32bit", {0}, 2048, 2048, 8, 8, 8, 8, 0, 0 },
	{ "4096x2048x16bit", {0}, 4096, 2048, 5, 6, 5, 0, 0, 0 },
	{ "4096x2048x32bit", {0}, 4096, 2048, 8, 8, 8, 8, 0, 0 },
*/
};

/**
 * @brief returns the number of configs defined
 */
static int gl2_get_num_configs(void) { return sizeof(gles_default_config_list) / sizeof(gles_default_config_list[0]); }


int gl2_get_config_information(unsigned int index, const char** name,
                          int* width, int* height, int* rgba,
					 	  int* multisample, unsigned int* flags)
{
	if(index >= (unsigned int)gl2_get_num_configs()) return 1;

	if(name) *name = gles_default_config_list[index].name;
	if(width) *width =  gles_default_config_list[index].width;
	if(height) *height =  gles_default_config_list[index].height;
	if(rgba)
	{
		rgba[0] =  gles_default_config_list[index].red_bits;
		rgba[1] =  gles_default_config_list[index].green_bits;
		rgba[2] =  gles_default_config_list[index].blue_bits;
		rgba[3] =  gles_default_config_list[index].alpha_bits;
	}
	if(multisample) *multisample =  gles_default_config_list[index].fsaa;
	if(flags) *flags =  gles_default_config_list[index].pattern_flags;

	return 0;
}


void gl2_set_config_exclusions(suite* test_suite, unsigned int exclusion_bitfield)
{
	test_suite->api_specific_fixture_filters = exclusion_bitfield;
}


/**
 * @brief modifies the flags of all the configs declared above, populating them with the proper flags
 * @note Albeit largely static output, this cannot be done "manually" as the custom config might change.
 */
static void set_config_list_flags(void)
{
	int size = gl2_get_num_configs();
	int i;

	for(i = 0; i < size; i++)
	{
		struct gles_framework_config* cfg = &gles_default_config_list[i];

		/* limit bits to legal values */
		if(cfg->red_bits > 8) cfg->red_bits = 8;
		if(cfg->blue_bits > 8) cfg->blue_bits = 8;
		if(cfg->green_bits > 8) cfg->green_bits = 8;
		if(cfg->alpha_bits > 8) cfg->alpha_bits = 8;

		if(cfg->fsaa != 0)  cfg->pattern_flags |= CONFIG_FLAG_FSAA;
		if(cfg->width < 16 || cfg->height < 16)	cfg->pattern_flags |= CONFIG_FLAG_DIMENSION_SMALLER_THAN_16x16;
		if(cfg->width < 32 || cfg->height < 32)	cfg->pattern_flags |= CONFIG_FLAG_DIMENSION_SMALLER_THAN_32x32;
		if(cfg->width < 64 || cfg->height < 64)	cfg->pattern_flags |= CONFIG_FLAG_DIMENSION_SMALLER_THAN_64x64;
		if(cfg->width < 128 || cfg->height < 128) cfg->pattern_flags |= CONFIG_FLAG_DIMENSION_SMALLER_THAN_128x128;
		if(cfg->width != 128 || cfg->height != 128) cfg->pattern_flags |= CONFIG_FLAG_DIMENSION_NOT_128x128;
		if(cfg->width % 16 != 0 || cfg->height % 16 != 0) cfg->pattern_flags |= CONFIG_FLAG_DIMENSION_NOT_DIVISIBLE_BY_16;
		if(cfg->width != cfg->height) cfg->pattern_flags |= CONFIG_FLAG_HEIGHT_DIFFERFROM_WIDTH;

	}
}

static void set_config_names(void)
{
	int size = gl2_get_num_configs();
	int i;
	for(i = 0; i < size; i++)
	{
		struct gles_framework_config* cfg = &gles_default_config_list[i];
		sprintf(cfg->name, "'%s' ( %dx%d %d%d%d%d %s%s)", cfg->tagname, cfg->width, cfg->height,
			cfg->red_bits, cfg->green_bits, cfg->blue_bits, cfg->alpha_bits,
			(cfg->fsaa == 0) ? "" : (cfg->fsaa == 4) ? "4xFSAA " : (cfg->fsaa == 16) ? "16xFSAA " : "??FSAA ",
			(cfg->pattern_flags & CONFIG_FLAG_RENDER_TO_FBO) ? "FBO " : ""
		 );
	}

}

static void gles2_print_framework_usage(void)
{
	/**
	 * These puts calls have been split up so that the string length stays within C89 limits
	 * NOTE: Please use only spaces, not tab stops, to format these strings.
	 */
	puts(
		"\n"
		"This testbench contains a set of test suites. Each test suite contains \n"
		"a series of functional tests, which in turn contains a series of asserts.\n"
		"Any failing asserts will be logged and presented at the end of the program.\n"
		"\n"
		"Each suite can have a series of fixtures containing different data sets. \n"
		"The testbench also defines a series of configs on which all tests should be run.\n"
		"\n"
		"  Output arguments:\n"
		"\n"
		"    --help         Show this help message\n"
		"\n"
		);
	puts(
		"  Execution arguments: \n"
		"\n"
		"    --suite=s      Run only the suite named 's'\n"
		"    --test=t       Run only the test named 't'\n"
		"    --config=c     Use only the config named 'c'\n"
		"    --fixture=n    Run only fixture number 'n'\n"
		"\n"
		);
	puts(
		"  Enumeration arguments: \n"
		"\n"
		"     --list-suites\n"
		"     --list-tests\n"
		"     --list-fixtures\n"
		"                   Instead of running the tests, list them. Each option\n"
		"                   lists at a different level of granularity. Each line\n"
		"                   of output is a fully-qualified command line to run\n"
		"                   that suite, test, or fixture (aka config).\n"
		"\n"
		);
	puts(
		"  Filtering options: \n\n"
		"       --include=filter\n"
		"                     Include tests that match the given filter:\n"
		"                       all [default], smoketest, performance, none\n"
		"                     If this option is omitted, all tests will be included,\n"
		"                     subject to other options\n"
		"       --exclude=filter \n"
		"                     Exclude tests that match the given filter:\n"
		"                       all, smoketest, performance, none [default]\n"
		"                     If this option is omitted, no tests will be excluded,\n"
		"                     subject to other options\n"
		);
	puts(
		"  Config arguments: \n"
		"\n"
		"     --list-configs\n"
		"                   Lists all supported predefined configs in this suite \n"
		"\n"
		"     Using any of the following parameters will run the tests using a custom config\n"
		"     This is initially set to width=128 height=128 format=5650 nofsaa\n"
		"\n"
		"     --width=w     Set the rendertarget width \n"
		"     --height=h    Set the rendertarget height \n"
		"     --format=rgba Set the rendertarget pixel format, 4 digits (ex 5650)\n"
		"     --4x          Use 4x FSAA\n"
		"     --16x         Use 16x FSAA\n"
		"     --fbo         Render to an FBO instead of the screen. \n"
		"\n"
		);

}

static void print_all_configs(void)
{
		int size = gl2_get_num_configs();
		int i;
		printf("Available configs: \n================\n");
		for(i = 1; i < size; i++)
		{
			printf("Fixture %i: = %s\n", i, gles_default_config_list[i].name);
		}
}

int gles2_parse_args( test_specifier* test_spec, mempool* pool, const char* string)
{

	if(string == NULL)
	{
		test_spec->first_fixture = 1;
		test_spec->last_fixture = gl2_get_num_configs()-1;
		test_spec->api_parameters = gles_default_config_list;
		set_config_list_flags();
		set_config_names();
		return 1; /* no params to parse, just run ahead with default spec */
	}

	if(strcmp(string, "--list-configs") == 0)
	{
		print_all_configs();
		exit(0);
	} else
	if(strncmp(string,"--width=",8)==0)
	{
		gles_default_config_list[0].width = atoi(string+8);
		test_spec->first_fixture = test_spec->last_fixture = 0;
    } else
	if(strncmp(string,"--height=",9)==0)
	{
		gles_default_config_list[0].height = atoi(string+9);
		test_spec->first_fixture = test_spec->last_fixture = 0;
    } else
	if(strcmp(string,"--fbo") == 0)
	{
		gles_default_config_list[0].pattern_flags |= CONFIG_FLAG_RENDER_TO_FBO;
		test_spec->first_fixture = test_spec->last_fixture = 0;
	} else
	if(strncmp(string,"--format=",9)==0)
	{
		char buffer[2] = {0, 0};
		if(strlen(string+9) != 4)
		{
			printf("Format parameters need to be 4 digits, one for each bitplane. \n");
			printf("Examples:  \n\n");
			printf("  --format=5650   (5 red, 6 blue, 5 green, 0 alpha; standard 16 bit setup) \n");
			printf("  --format=8888   (8 red, 8 blue, 8 green, 8 alpha; standard 32 bit setup) \n");
			exit(1);
		}
		buffer[0] =  string[9];
		gles_default_config_list[0].red_bits = atoi(buffer);
		buffer[0] =  string[11];
		gles_default_config_list[0].green_bits = atoi(buffer);
		buffer[0] =  string[10];
		gles_default_config_list[0].blue_bits = atoi(buffer);
		buffer[0] =  string[12];
		gles_default_config_list[0].alpha_bits = atoi(buffer);

		test_spec->first_fixture = test_spec->last_fixture = 0;
    } else
	if(strcmp(string,"--4x")==0)
	{
		 gles_default_config_list[0].fsaa = 4;
		 test_spec->first_fixture = test_spec->last_fixture = 0;
    } else
	if(strcmp(string,"--16x")==0)
	{
		 gles_default_config_list[0].fsaa = 16;
		 test_spec->first_fixture = test_spec->last_fixture = 0;
    } else
	if(strncmp(string,"--config=",9)==0)
	{
		int size = gl2_get_num_configs();
		int match = -1;
		int i;
		for(i = 1; i < size; i++)
		{
			if( strcmp(string+9,  gles_default_config_list[i].tagname ) == 0 )
			{
				match = i;
			}
		}

		if(match == -1) /* no match */
		{
	    	printf("No match on config '%s'\n\n", string);
			print_all_configs();
			exit(1);
		}

		test_spec->first_fixture = test_spec->last_fixture = match;
	}else
	{
		gles2_print_framework_usage();
		exit(1);
	}
	set_config_list_flags();
	set_config_names();
	return 1;
}


