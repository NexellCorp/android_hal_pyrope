/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_counters_dump.h"
#include "mali_counters.h"

#include "mali_system.h"
#include "base/mali_runtime.h"
#include "base/mali_macros.h"
#include "base/mali_debug.h"
#include "util/mali_names.h"

#include "mali_instrumented_context.h"
#include "mali_instrumented_context_types.h"

static char* _instrumented_xml_tree(mali_instrumented_context* ctx, const mali_counter_group *group, char* next, char* end, mali_bool calculate_size, unsigned long *size)
{
	unsigned int i;
	const mali_counter_info *info;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT_POINTER(next);
	MALI_DEBUG_ASSERT_POINTER(end);
	MALI_DEBUG_ASSERT((!calculate_size && NULL == size) || (calculate_size && NULL != size),
		("Invalid combination of calculate_size and size parameter"));

	if (group == ctx->header_root)
	{
		_mali_sys_snprintf(next, end-next, "\n<countertree>\n");
		if (calculate_size)
		{
			unsigned long cur_size = _mali_sys_strlen(next);
			*size += cur_size;
			MALI_DEBUG_ASSERT((next + cur_size + 1) < end, ("Buffer to small"));
		}
		else
		{
			next += _mali_sys_strlen(next);
		}
	}
	else
	{
		_mali_sys_snprintf(next, end-next, "<group>\n<name>%s</name>\n", group->name);
		if (calculate_size)
		{
			unsigned long cur_size = _mali_sys_strlen(next);
			*size += cur_size;
			MALI_DEBUG_ASSERT((next + cur_size + 1) < end, ("Buffer to small"));
		}
		else
		{
			next += _mali_sys_strlen(next);
		}

		if (_mali_sys_strlen(group->description) > 0)
		{
			_mali_sys_snprintf(next, end-next, "<description>%s</description>\n", group->description);
			if (calculate_size)
			{
				unsigned long cur_size = _mali_sys_strlen(next);
				*size += cur_size;
				MALI_DEBUG_ASSERT((next + cur_size + 1) < end, ("Buffer to small"));
			}
			else
			{
				next += _mali_sys_strlen(next);
			}
		}
		if (group->frequency > 0)
		{
			_mali_sys_snprintf(next, end-next, "<frequency>%d</frequency>\n", group->frequency);
			if (calculate_size)
			{
				unsigned long cur_size = _mali_sys_strlen(next);
				*size += cur_size;
				MALI_DEBUG_ASSERT((next + cur_size + 1) < end, ("Buffer to small"));
			}
			else
			{
				next += _mali_sys_strlen(next);
			}
		}
	}
	for (i = 0; i < group->num_children; i++)
	{
		if (group->leaf)
		{
			info = _instrumented_get_counter_information(ctx, group->children.counter_indices[i]);

			_mali_sys_snprintf(next, end-next,
				"<counter>\n\t<id>%u</id>\n\t<name>%s</name>\n\t<description>%s</description>\n\t<index>%u</index>\n",
				info->id, info->name, info->description, group->children.counter_indices[i]);
			if (calculate_size)
			{
				unsigned long cur_size = _mali_sys_strlen(next);
				*size += cur_size;
				MALI_DEBUG_ASSERT((next + cur_size + 1) < end, ("TBuffer to small"));
			}
			else
			{
				next += _mali_sys_strlen(next);
			}

			if ('\0' != info->unit[0])
			{
				_mali_sys_snprintf(next, end-next, "\t<unit>%s</unit>\n", info->unit);
				if (calculate_size)
				{
					unsigned long cur_size = _mali_sys_strlen(next);
					*size += cur_size;
					MALI_DEBUG_ASSERT((next + cur_size + 1) < end, ("Buffer to small"));
				}
				else
				{
					next += _mali_sys_strlen(next);
				}
			}

			_mali_sys_snprintf(next, end-next, "\t<scale>%s</scale>\n</counter>\n",
				(info->scale_to_hz == MALI_SCALE_NORMAL)? "normal"
				: ((info->scale_to_hz == MALI_SCALE_INVERSE)? "inverse" : "none") );
			if (calculate_size)
			{
				unsigned long cur_size = _mali_sys_strlen(next);
				*size += cur_size;
				MALI_DEBUG_ASSERT((next + cur_size + 1) < end, ("Buffer to small"));
			}
			else
			{
				next += _mali_sys_strlen(next);
			}
		}
		else
		{
			next = _instrumented_xml_tree(ctx, group->children.groups[i], next, end, calculate_size, size);
		}
	}

	_mali_sys_snprintf(next, end-next, (group == ctx->header_root)? "\n</countertree>\n": "</group>\n");
	if (calculate_size)
	{
		unsigned long cur_size = _mali_sys_strlen(next);
		*size += cur_size;
		MALI_DEBUG_ASSERT((next + cur_size + 1) < end, ("Buffer to small"));
	}
	else
	{
		next += _mali_sys_strlen(next);
	}

	return next;
}

MALI_EXPORT
char* _instrumented_get_xml_tree(mali_instrumented_context* ctx, const mali_counter_group *group, char* next, char* end)
{
	return _instrumented_xml_tree(ctx, group, next, end, MALI_FALSE, NULL);
}

MALI_EXPORT
unsigned long _instrumented_get_xml_tree_size(mali_instrumented_context* ctx, const mali_counter_group *group)
{
#define TMPBUF_SIZE 16384
	unsigned long size = 0;
	char *tmpbuf = (char*)_mali_sys_malloc(TMPBUF_SIZE);
	if (NULL == tmpbuf)
	{
		return 0;
	}

	_instrumented_xml_tree(ctx, group, tmpbuf, tmpbuf + TMPBUF_SIZE, MALI_TRUE, &size);
	_mali_sys_free(tmpbuf);

	return size + 1; /* +1 for null-term */
#undef TMPBUF_SIZE
}

MALI_EXPORT
void _instrumented_open_dump(mali_instrumented_context* ctx)
{
	const char *filename;
	const char *dump_name;
	const char *pp_name = MALI_NAME_PP;
	const char *gp_name = MALI_NAME_GP;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	if (NULL != ctx->dump_file) return;

	filename = _mali_sys_config_string_get("MALI_DUMP_FILE");
	if (NULL == filename || _mali_sys_strlen(filename) == 0)
	{
		_mali_sys_config_string_release(filename);
		return;
	}

	ctx->dump_file = _mali_sys_fopen(filename, "w");
	_mali_sys_config_string_release(filename);

	if (NULL == ctx->dump_file) return;

	_mali_sys_fprintf(ctx->dump_file,
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<ds2dumpfile version=\"0.2\">\n");

	dump_name = _mali_sys_config_string_get("MALI_DUMP_NAME");
	_mali_sys_fprintf(ctx->dump_file,
			"	<name value=\"%s\" />\n",
			(NULL != dump_name) ? dump_name : "");
	_mali_sys_config_string_release(dump_name);

	if (NULL != gp_name) _mali_sys_fprintf(ctx->dump_file,
			"	<gp value=\"%s\" />\n",
			gp_name);

	if (NULL != pp_name) _mali_sys_fprintf(ctx->dump_file,
			"	<pp value=\"%s\" />\n",
			pp_name);
}

MALI_EXPORT
void _instrumented_close_dump(mali_instrumented_context* ctx)
{
	MALI_DEBUG_ASSERT_POINTER(ctx);
	if (NULL == ctx->dump_file) return;
	if (ctx->options_mask & INSTRUMENT_IS_PAT_DUMPING)
	{
	    ctx->options_mask &= ~INSTRUMENT_IS_PAT_DUMPING; /* stopped PAT dumping */
	    _mali_sys_fprintf(ctx->dump_file, "</frames>\n");
	}
	_mali_sys_fprintf(ctx->dump_file, "</ds2dumpfile>\n");
	_mali_sys_fclose(ctx->dump_file);

	ctx->dump_file = NULL;
}

MALI_EXPORT
void _instrumented_write_headers(mali_instrumented_context* ctx)
{
	char* tree_buffer;
	unsigned long size;

	MALI_DEBUG_ASSERT_POINTER(ctx);
	MALI_DEBUG_ASSERT(0 == (ctx->options_mask & INSTRUMENT_IS_PAT_DUMPING),
		("Attempting to write headers after frames"));  /* has already started PAT dump */

	if (NULL == ctx->dump_file)
	{
		return;
	}

	size = _instrumented_get_xml_tree_size(ctx, ctx->header_root);
	if (size == 0)
	{
		return;
	}

	tree_buffer = _mali_sys_malloc(size);
	if (NULL != tree_buffer)
	{
		_instrumented_get_xml_tree(ctx, ctx->header_root, tree_buffer, tree_buffer + size);
		_mali_sys_fwrite(tree_buffer, size - 1, 1, ctx->dump_file); /* don't write null-term */
		_mali_sys_free(tree_buffer);
	}
}

MALI_EXPORT
void _instrumented_write_frame(mali_instrumented_context* ctx, mali_instrumented_frame *frame)
{
	int counters, i;
	s64 *counter_values;


	MALI_DEBUG_ASSERT_POINTER(ctx);
	if (NULL == ctx->dump_file) return;
	counter_values = frame->counter_values;
	if (NULL == counter_values) return;

	counters = _instrumented_get_number_of_counters(ctx);
	if (0 == (ctx->options_mask & INSTRUMENT_IS_PAT_DUMPING))
	{           /* not dumping already: */
		ctx->options_mask |= INSTRUMENT_IS_PAT_DUMPING; /* start to PAT dump */
		_mali_sys_fprintf(ctx->dump_file, "<frames>\n");
	}
	_mali_sys_fprintf(ctx->dump_file, "<frame id=\"%d\">\n", frame->frame_no + 1);
	for (i = 0; i < counters; i++)
	{
#ifndef __SYMBIAN32__
		_mali_sys_fprintf(ctx->dump_file, "%lld\n", counter_values[i]);
#else
		_mali_sys_fprintf(ctx->dump_file, "%LD\n", counter_values[i]);
#endif
	}
	_mali_sys_fprintf(ctx->dump_file, "</frame>\n");
}

