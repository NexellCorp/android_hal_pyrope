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

#include "common/essl_common.h"
#include "common/essl_mem.h"
#include "common/essl_stringbuffer.h"
#include "common/compiler_options.h"
#include "common/essl_random.h"
#include "compiler.h"
#include "commandline.h"
#include "compiler_version.h"

#define HW_REV_UNSET ((unsigned int)-1)

static char *readfile(mempool *pool, const char *filename, int *size_out)
{
	FILE *f;
	char *data;
	long int nbytes;

	f = fopen(filename, "rb");
	if (f == 0) {
		fprintf(stderr, "Error reading file %s\n", filename);
		return 0;
	}

	if (fseek(f, 0, SEEK_END) >= 0 &&
		(nbytes = ftell(f)) >= 0 &&
		fseek(f, 0, SEEK_SET) >= 0 &&
		(data = _essl_mempool_alloc(pool, (size_t)nbytes)) != 0 &&
		(long int)fread(data, 1, (size_t)nbytes, f) == nbytes)
	{
		(void)fclose(f);
		*size_out = nbytes;
		return data;
	} else {
		(void)fclose(f);
		fprintf(stderr, "Error reading file %s\n", filename);
		return 0;
	}
}

typedef enum {
	HW_REV_FOUND,
	HW_REV_FOUND_ERROR,
	HW_REV_NOT_FOUND
} hw_rev_parse_ret;

static hw_rev_parse_ret parse_hardware_revision(int argc, const char **argv, int *i, unsigned int *hw_core, unsigned int *hw_rp)
{

	const char *arg = argv[*i];
	if (!strncmp(arg, "--revision", 10) || !strncmp(arg, "-r", 2)) {
		/* find option argument (text) */
		const char *text;
			
		if (arg[1] == '-' && arg[10] != '\0') /* option argument follows --revision in same argv */
		{
			text = arg + 10;
		}
		else if (arg[1] != '-' && arg[2] != '\0') /* option argument follows -r in same argv */
		{
			text = arg + 2;
		}
		else if ((*i + 1) < argc)
		{
			text = argv[++*i]; /* option argument is in next argv element */
		}
		else
		{
			fprintf(stderr, "Missing mandatory option argument for %s\n", arg);
			return HW_REV_FOUND_ERROR;
		}

		{
			unsigned int release = 0;
			unsigned int patch = 0;
			char dummy;
			if(text[0] == '=') ++text;

			if (sscanf(text, "r%up%u%c", &release, &patch, &dummy) == 2)
			{
				if ((release & 0xFF) == release && (patch & 0xFF) == patch)
				{
					if (*hw_rp != HW_REV_UNSET && (HW_REV_RELEASE(*hw_rp) != release || HW_REV_PATCH(*hw_rp) != patch))
					{
						fprintf(stderr, "Hardware revision specified more than once\n");
						return HW_REV_FOUND_ERROR;
					}
		
					*hw_rp = (release << 8) | patch;
					return HW_REV_FOUND;
				}
			}
			fprintf(stderr, "Malformed revision argument specified: '%s'\n", text);
			return HW_REV_FOUND_ERROR;
		}
	} else if (!strcmp(arg, "--testchip")) {
		if (*hw_core != HW_REV_UNSET && *hw_core != HW_REV_CORE_MALI200)
		{
			fprintf(stderr, "Core specified more than once\n");
			return HW_REV_FOUND_ERROR;
		}
		*hw_rp = HW_REV_MALI200_R0P1;
		*hw_core = HW_REV_CORE_MALI200;
		return HW_REV_FOUND;
	} else if (!strncmp(arg, "--core", 6)) {
		/* find option argument (text) */
		const char *text;
		unsigned int core = HW_REV_UNSET;

		if (arg[6] != '\0') /* option argument follows --revision in same argv */
		{
			text = arg + 6;
		}
		else if ((*i + 1) < argc)
		{
			text = argv[++*i]; /* option argument is in next argv element */
		}
		else
		{
			fprintf(stderr, "Missing mandatory option argument for %s\n", arg);
			return HW_REV_FOUND_ERROR;
		}

		if(text[0] == '=') ++text;

		{
			essl_bool known_core = ESSL_FALSE;
			/* Case-insensitive comparison against "mali" */
			if ((text[0] == 'M' || text[0] == 'm') &&
				(text[1] == 'A' || text[1] == 'a') &&
				(text[2] == 'L' || text[2] == 'l') &&
				(text[3] == 'I' || text[3] == 'i'))
			{
				if (!strncmp(&text[4], "-200", 4))
				{
					core = HW_REV_CORE_MALI200;
					known_core = ESSL_TRUE;
				}
				else if (!strncmp(&text[4], "-400", 4))
				{
					core = HW_REV_CORE_MALI400;
					known_core = ESSL_TRUE;
				}
				else if (!strncmp(&text[4], "-300", 4))
				{
					core = HW_REV_CORE_MALI300;
					known_core = ESSL_TRUE;
				}
#ifdef ENABLE_MALI450_SUPPORT
				else if (!strncmp(&text[4], "-450", 4))
				{
					core = HW_REV_CORE_MALI450;
					known_core = ESSL_TRUE;
				}
#endif

			}
			if (!known_core)
			{
				fprintf(stderr, "Invalid core name specified for %s\n", arg);
				return HW_REV_FOUND_ERROR;
			}
		}

		if (*hw_core != HW_REV_UNSET && *hw_core != core)
		{
			fprintf(stderr, "Core specified more than once\n");
			return HW_REV_FOUND_ERROR;
		}

		*hw_core = core;
		return HW_REV_FOUND;
	} else {
		return HW_REV_NOT_FOUND;
	}

}

static unsigned int get_hardware_revision(unsigned int hw_core, unsigned int hw_rp)
{
	if (hw_core == HW_REV_UNSET)
	{
		hw_core = HW_REV_CORE_DEFAULT;
	}
	if (hw_rp == HW_REV_UNSET)
	{
		switch (hw_core)
		{
		case HW_REV_CORE_MALI200:
			return HW_REV_MALI200_DEFAULT;
		case HW_REV_CORE_MALI400:
		case HW_REV_CORE_MALI300:
		case HW_REV_CORE_MALI450:
			return HW_REV_MALI400_DEFAULT;
		default:
			assert(0);
			return 0;
		}
	} else {
		return (hw_core << 16) | hw_rp;
	}
}

commandline_ret _essl_parse_commandline(mempool *pool, int argc, const char **argv, char **shader_source, int **lengths, int *nsources, int *source_string_report_offset, shader_kind *kind, const char **out_filename, essl_bool *verbose, unsigned int *hw_rev) 
{
	int i;
	string_buffer *db; /* #define buffer */
	string_buffer *sb; /* source buffer */
	essl_bool seen_vertex = ESSL_FALSE;
	essl_bool seen_fragment = ESSL_FALSE;
	int n = 1; /* start at index 1 with the source files, 0 is reserved for a dummy block with #defines from cmdline */
	essl_bool read_source = ESSL_FALSE;
	unsigned int hw_core = HW_REV_UNSET;
	unsigned int hw_rp = HW_REV_UNSET;

	*verbose = ESSL_FALSE;
	ESSL_CHECK(*lengths = _essl_mempool_alloc(pool, argc*sizeof(int)));
	ESSL_CHECK(db = _essl_new_string_buffer(pool));
	ESSL_CHECK(sb = _essl_new_string_buffer(pool));

	(*lengths)[0] = 0;

	for (i = 1 ; i < argc ; i++) {
		const char *arg = argv[i];

		hw_rev_parse_ret ret = parse_hardware_revision(argc, argv, &i, &hw_core, &hw_rp);
		if(ret == HW_REV_FOUND) continue;
		if(ret == HW_REV_FOUND_ERROR) return CMD_OPTION_ERROR;

		if (!strcmp(arg, "-vert") || !strcmp(arg, "--vert")) {
			seen_vertex = ESSL_TRUE;
		} else if (!strcmp(arg, "-frag") || !strcmp(arg, "--frag")) {
			seen_fragment = ESSL_TRUE;
		} else if (!strcmp(arg, "--verbose") || !strcmp(arg, "-v")) {
			*verbose = ESSL_TRUE;
		} else if (arg[0] == '-' && arg[1] == 'o') {
			if (arg[2] != '\0')
			{
				*out_filename = arg + 2; /* -ofilename */
			}
			else if ((i + 1) < argc)
			{
				*out_filename = argv[++i]; /* option argument is in next argv element */
			}
			else
			{
				fprintf(stderr, "Missing mandatory option argument for %s\n", arg);
				return CMD_OPTION_ERROR;
			}
		} else if (arg[0] == '-' && arg[1] == 'D') {
			/* -DNAME or -DNAME=VALUE */
			const char *name = &arg[2];
			const char *ptr = name;
			ESSL_CHECK(_essl_string_buffer_put_str(db, "#define "));
			while (*ptr != 0 && *ptr != '=') ptr++;
			if (*ptr == 0) {
				/* Define with no value */
				ESSL_CHECK(_essl_string_buffer_put_str(db, name));
				(*lengths)[0] += 9+strlen(name);
			} else {
				/* Define with value */
				const char *value = &ptr[1];
				string name_str;
				name_str.ptr = name;
				name_str.len = ptr-name;
				ESSL_CHECK(_essl_string_buffer_put_string(db, &name_str));
				ESSL_CHECK(_essl_string_buffer_put_str(db, " "));
				ESSL_CHECK(_essl_string_buffer_put_str(db, value));
				(*lengths)[0] += 10+name_str.len+strlen(value);
			}
			ESSL_CHECK(_essl_string_buffer_put_str(db, "\n"));
		} else if (arg[0] == '-' && arg[1] == 'X') {
			/* -XOPTION or -XOPTION=VALUE - check that the option is supported */
			const char *name = &arg[2];
			int value;
			compiler_option option = _essl_parse_compiler_option(name, &value);
			if (option == 0) {
				fprintf(stderr, "Unknown internal option %s\n", arg);
				return CMD_OPTION_ERROR;
			}
#ifdef RANDOMIZED_COMPILATION
		} else if (!strncmp(arg, "--random-seed=", 14)) {
			_essl_set_random_seed(atoi(&arg[14]));
#endif
		} else if (arg[0] == '-') {
			fprintf(stderr, "Unknown option %s\n", arg);
			return CMD_OPTION_ERROR;
		} else {
			/* File name */
			string source;
			source.ptr = readfile(pool, argv[i], &source.len);
			if (source.ptr == 0) {
				/* error reported by readfile() */
				return CMD_FILE_ERROR;
			}
			ESSL_CHECK(_essl_string_buffer_put_string(sb, &source));
			(*lengths)[n] = source.len;
			
			if (strlen(argv[i]) >= 5 && !strcmp(argv[i] + strlen(argv[i]) - 5, ".vert")) {
				seen_vertex = ESSL_TRUE;
			}
			if (strlen(argv[i]) >= 5 && !strcmp(argv[i] + strlen(argv[i]) - 5, ".frag")) {
				seen_fragment = ESSL_TRUE;
			}
			n++;
			read_source = ESSL_TRUE;
		}
	}

	if (!read_source) {
		fprintf(stderr, "Source file not specified\n");
		return CMD_OPTION_ERROR;
	}

	if (seen_vertex && seen_fragment) {
		return CMD_KIND_ERROR;
	}

	if (seen_vertex) {
		*kind = SHADER_KIND_VERTEX_SHADER;
	}

	if (seen_fragment) {
		*kind = SHADER_KIND_FRAGMENT_SHADER;
	}

	*hw_rev = get_hardware_revision(hw_core, hw_rp);
	if (!_essl_validate_hw_rev(*hw_rev))
	{
		fprintf(stderr, "Illegal or unknown core/revision combination specified\n");
		return CMD_OPTION_ERROR;
	}

	ESSL_CHECK(*shader_source = _essl_string_buffers_to_string(db, sb, pool));
	*nsources = n;
	*source_string_report_offset = -1; /* always adding define buffer */
	return CMD_OK;
}

static compiler_option parse_compiler_option_string(const char *op_string, int length)
{

#define COMPILER_OPTION(name,field,type,defval) \
	if (strlen(#name) == length && strncmp(op_string, #name, length) == 0) \
		return COMPILER_OPTION_##name;
#include "common/compiler_option_definitions.h"
#undef COMPILER_OPTION

	return COMPILER_OPTION_UNKNOWN;
}

compiler_option _essl_parse_compiler_option(const char *name, int *value_out) {
	const char *ptr = name;
	compiler_option option;
	while (*ptr != 0 && *ptr != '=') ptr++;
	option = parse_compiler_option_string(name, ptr-name);
	if (option == 0) {
		return COMPILER_OPTION_UNKNOWN;
	}
	
	if (ptr[0] == '=') {
		*value_out = atoi(&ptr[1]);
	} else {
		*value_out = 1;
	}

	return option;
}

int _essl_parse_commandline_compiler_options(int argc, const char **argv, compiler_options *opts, unsigned int *hw_rev_ret) {
	int i;
	int n = 0;
	unsigned int hw_core = HW_REV_UNSET;
	unsigned int hw_rp = HW_REV_UNSET;
	unsigned int hw_rev;

	for (i = 1 ; i < argc ; i++) {
		const char *arg = argv[i];
		if (arg[0] == '-') {
			hw_rev_parse_ret ret = parse_hardware_revision(argc, argv, &i, &hw_core, &hw_rp);
			if(ret == HW_REV_FOUND)
			{
				n++;
			}
			if(ret == HW_REV_FOUND_ERROR) return -1;
		}
	}

	hw_rev = get_hardware_revision(hw_core, hw_rp);
	if (!_essl_validate_hw_rev(hw_rev))
	{
		fprintf(stderr, "Illegal or unknown core/revision combination specified\n");
		return -1;
	}
	_essl_set_compiler_options_for_hw_rev(opts, hw_rev);
	if(hw_rev_ret)
	{
		*hw_rev_ret = hw_rev;
	}

	for (i = 1 ; i < argc ; i++) {
		const char *arg = argv[i];
		if (arg[0] == '-' && arg[1] == 'X') {
			const char *name = &arg[2];
			int value;
			compiler_option option = _essl_parse_compiler_option(name, &value);
			if (option == 0) {
				fprintf(stderr, "Unknown internal option %s\n", arg);
				return -1;
			}
			_essl_set_compiler_option_value(opts, option, value);
			n++;
		}
	}

	return n;
}
