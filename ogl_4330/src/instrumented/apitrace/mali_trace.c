/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_system.h"
#include "mali_trace.h"

#define MAX_USER_BUFFERS_TO_WATCH 128

static mali_bool trace_inited = MALI_FALSE;
static unsigned int init_count = 0;
static mali_bool deinit_after_func_call = MALI_FALSE;
static mali_bool in_a_func_call = MALI_FALSE;
static enum mali_api_trace_flags trace_flags = 0;
static mali_file* xml_file = NULL;
static u32 param_id = 0;

static void mali_api_trace_write_xml(const char* str, u32 count)
{
	_mali_sys_fwrite(str, 1, count, xml_file);
}

static void mali_api_trace_write_xml_as_hex(const char* str, u32 count)
{
	const char ch_tbl[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

	char obuf[1024]; /* must be a multiple of two */
	u32 dest_counter = 0;
	u32 i;

	for (i = 0; i < count; i++)
	{
		obuf[dest_counter++] = ch_tbl[(str[i] >> 4) & 0xF];
		obuf[dest_counter++] = ch_tbl[(str[i]) & 0xF];

		if (sizeof(obuf) == dest_counter)
		{
			mali_api_trace_write_xml(obuf, dest_counter);
			dest_counter = 0;
		}
	}

	if (0 < dest_counter)
	{
		mali_api_trace_write_xml(obuf, dest_counter);
	}
}

static void mali_api_trace_init(void)
{
	if (trace_inited == MALI_TRUE)
	{
		return;
	}

	if (0 == init_count)
	{
		/*
		 * Tracing will start even before a base context is created,
		 * so make sure extra environment variables are loaded.
		 */
		_mali_sys_load_config_strings();
	}

	if (_mali_sys_config_string_get_bool("API_TRACE_ALL", MALI_FALSE))
	{
		trace_flags = MALI_API_TRACE_FUNCTION_CALLS | MALI_API_TRACE_PARAMETERS | MALI_API_TRACE_OUTPUT;
	}
	else if (_mali_sys_config_string_get_bool("API_TRACE_PARAMETERS", MALI_FALSE))
	{
		trace_flags = MALI_API_TRACE_FUNCTION_CALLS | MALI_API_TRACE_PARAMETERS;
	}
	else if (_mali_sys_config_string_get_bool("API_TRACE_FUNCTION_CALLS", MALI_FALSE))
	{
		trace_flags = MALI_API_TRACE_FUNCTION_CALLS;
	}

	if (_mali_sys_config_string_get_bool("MALI_API_TRACE_TESTING", MALI_FALSE))
	{
		trace_flags |= MALI_API_TRACE_TESTING;
	}

	if (trace_flags & MALI_API_TRACE_FUNCTION_CALLS)
	{
		const char* filename_env = _mali_sys_config_string_get("API_TRACE_FILE"); /* release this if != NULL */
		char* filename_alloc = NULL; /* free this if != NULL */
		const char* filename = NULL; /* this is the one used, but we never free this pointer in any way */
		
		if (0 == init_count)
		{
			if (NULL != filename_env)
			{
				filename = filename_env;
			}
			else
			{
				/* default to trace.trc */
				filename = "trace.trc";
			}
		}
		else
		{
			if (NULL != filename_env)
			{
				unsigned int buf_size = _mali_sys_strlen(filename_env) + 6;
				filename_alloc = _mali_sys_malloc(buf_size);
				if (NULL != filename_alloc)
				{
					_mali_sys_snprintf(filename_alloc, buf_size, "%s.%04u", filename_env, init_count);
				}
			}
			else
			{
				unsigned int buf_size = 15;
				filename_alloc = _mali_sys_malloc(buf_size);
				if (NULL != filename_alloc)
				{
					_mali_sys_snprintf(filename_alloc, buf_size, "trace_%04u.trc", init_count);
				}
			}
			filename = filename_alloc;
		}

		if (NULL != filename)
		{
			_mali_sys_printf("Starting API tracing to file: %s\n", filename);
			xml_file = _mali_sys_fopen(filename, "w");
			init_count++;
			if (NULL != xml_file)
			{
				_mali_sys_fprintf(xml_file, "<?xml version='1.0' encoding='UTF-8'?>\n");
				_mali_sys_fprintf(xml_file, "<trace version='2'>\n");
			}
		}

		if (NULL != filename_env)
		{
			_mali_sys_config_string_release(filename_env);
		}

		if (NULL != filename_alloc)
		{
			_mali_sys_free(filename_alloc);
		}

		filename = NULL;

		if (NULL == xml_file)
		{
			_mali_sys_printf("Failed to open trace file\n");
		}
	}

	deinit_after_func_call = MALI_FALSE;
	in_a_func_call = MALI_FALSE;
	trace_inited = MALI_TRUE;
}

static void mali_api_trace_deinit(void)
{
	if (trace_inited == MALI_TRUE)
	{
		if (NULL != xml_file)
		{
			_mali_sys_fprintf(xml_file, "</trace>\n");
			_mali_sys_fclose(xml_file);
			xml_file = NULL;
		}

		deinit_after_func_call = MALI_FALSE;
		trace_inited = MALI_FALSE;
	}
}

MALI_EXPORT
void mali_api_trace_mark_for_deinit(void)
{
	if (trace_inited == MALI_TRUE)
	{
		deinit_after_func_call = MALI_TRUE;
	}

	if (in_a_func_call == MALI_FALSE)
	{
		/* It is safe to do deinit now */
		mali_api_trace_deinit();
	}
}

MALI_EXPORT
mali_bool mali_api_trace_flag_enabled(enum mali_api_trace_flags flag)
{
	mali_api_trace_init();

	return (xml_file != NULL && (trace_flags & flag)) ? MALI_TRUE : MALI_FALSE;
}

u32 mali_api_trace_gen_param_id()
{
	return ++param_id;
}

MALI_EXPORT
void mali_api_trace_func_begin(const char* func_name)
{
	in_a_func_call = MALI_TRUE;
	param_id = 0;

	if (mali_api_trace_flag_enabled(MALI_API_TRACE_FUNCTION_CALLS))
	{
		_mali_sys_fprintf(xml_file, " <func name='%s'>\n", func_name);
	}
}

MALI_EXPORT
void mali_api_trace_func_end(const char* func_name)
{
	MALI_IGNORE(func_name);

	if (mali_api_trace_flag_enabled(MALI_API_TRACE_FUNCTION_CALLS))
	{
		_mali_sys_fprintf(xml_file, " </func>\n");
	}

	in_a_func_call = MALI_FALSE;

	if (deinit_after_func_call == MALI_TRUE)
	{
		mali_api_trace_deinit();
	}
}

MALI_EXPORT
void mali_api_trace_param_signed_integer(s32 value, const char* type)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		_mali_sys_fprintf(xml_file, "  <param id='%u' type='%s'>\n", mali_api_trace_gen_param_id(), type);
		_mali_sys_fprintf(xml_file, "   <value>%d</value>\n", value);
		_mali_sys_fprintf(xml_file, "  </param>\n");
	}
}

MALI_EXPORT
void mali_api_trace_param_unsigned_integer(u32 value, const char* type)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		_mali_sys_fprintf(xml_file, "  <param id='%u' type='%s'>\n", mali_api_trace_gen_param_id(), type);
		_mali_sys_fprintf(xml_file, "   <value>%u</value>\n", value);
		_mali_sys_fprintf(xml_file, "  </param>\n");
	}
}

MALI_EXPORT
void mali_api_trace_param_float(float value, const char* type)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		_mali_sys_fprintf(xml_file, "  <param id='%u' type='%s'>\n", mali_api_trace_gen_param_id(), type);
		_mali_sys_fprintf(xml_file, "   <value>%.10f</value>\n", (double)value);
		_mali_sys_fprintf(xml_file, "  </param>\n");
	}
}

MALI_EXPORT
void mali_api_trace_param_int8_array(enum mali_api_trace_direction dir, const s8* ptr, u32 count, u32 offset, const char* type)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		if (0 == offset)
		{
			_mali_sys_fprintf(xml_file, "  <param id='%u' type='%s'>\n", mali_api_trace_gen_param_id(), type);
		}
		else
		{
			_mali_sys_fprintf(xml_file, "  <param id='%u' type='%s' offset='%u'>\n", mali_api_trace_gen_param_id(), type, offset);
		}

		if (ptr != NULL)
		{
			u32 i;

			_mali_sys_fprintf(xml_file, "   <values count='%u'>", count);

			if (dir & MALI_API_TRACE_IN || (dir & MALI_API_TRACE_TEST_IN && mali_api_trace_flag_enabled(MALI_API_TRACE_TESTING)))
			{
				for (i = 0; i < count; i++)
				{
					_mali_sys_fprintf(xml_file, "%d ", (s32)ptr[i]);
				}
			}

			_mali_sys_fprintf(xml_file, "</values>\n");
		}

		_mali_sys_fprintf(xml_file, "  </param>\n");
	}
}

MALI_EXPORT
void mali_api_trace_param_uint8_array(enum mali_api_trace_direction dir, const u8* ptr, u32 count, u32 offset, const char* type)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		if (0 == offset)
		{
			_mali_sys_fprintf(xml_file, "  <param id='%u' type='%s'>\n", mali_api_trace_gen_param_id(), type);
		}
		else
		{
			_mali_sys_fprintf(xml_file, "  <param id='%u' type='%s' offset='%u'>\n", mali_api_trace_gen_param_id(), type, offset);
		}

		if (ptr != NULL)
		{
			u32 i;
			
			_mali_sys_fprintf(xml_file, "   <values count='%u'>", count);

			if (dir & MALI_API_TRACE_IN || (dir & MALI_API_TRACE_TEST_IN && mali_api_trace_flag_enabled(MALI_API_TRACE_TESTING)))
			{
				for (i = 0; i < count; i++)
				{
					_mali_sys_fprintf(xml_file, "%02x", (u32)ptr[i]);
				}
			}

			_mali_sys_fprintf(xml_file, "</values>\n");
		}

		_mali_sys_fprintf(xml_file, "  </param>\n");
	}
}

MALI_EXPORT
void mali_api_trace_param_int16_array(enum mali_api_trace_direction dir, const s16* ptr, u32 count, const char* type)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		/* Check alignment of pointer */
		if ((((u32)ptr) & 0x1) == 0)
		{
			/* aligned correctly for a 16 bit value */
			_mali_sys_fprintf(xml_file, "  <param id='%u' type='%s'>\n", mali_api_trace_gen_param_id(), type);
		}
		else
		{
			/* Aligned incorrectly, so make sure we include alignment info */
			_mali_sys_fprintf(xml_file, "  <param id='%u' type='%s' misalign='%u'>\n", mali_api_trace_gen_param_id(), type, ((u32)ptr) & 0x1);
		}

		if (ptr != NULL)
		{
			u32 i;
			
			_mali_sys_fprintf(xml_file, "   <values count='%u'>", count);

			if (dir & MALI_API_TRACE_IN || (dir & MALI_API_TRACE_TEST_IN && mali_api_trace_flag_enabled(MALI_API_TRACE_TESTING)))
			{
				for (i = 0; i < count; i++)
				{
					_mali_sys_fprintf(xml_file, "%d ", (s32)ptr[i]);
				}
			}

			_mali_sys_fprintf(xml_file, "</values>\n");
		}

		_mali_sys_fprintf(xml_file, "  </param>\n");
	}
}

MALI_EXPORT
void mali_api_trace_param_int32_array(enum mali_api_trace_direction dir, const s32* ptr, u32 count, const char* type)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		/* Check alignment of pointer */
		if ((((u32)ptr) & 0x3) == 0)
		{
			/* aligned correctly for a 32 bit value */
			_mali_sys_fprintf(xml_file, "  <param id='%u' type='%s'>\n", mali_api_trace_gen_param_id(), type);
		}
		else
		{
			/* Aligned incorrectly, so make sure we include alignment info */
			_mali_sys_fprintf(xml_file, "  <param id='%u' type='%s' misalign='%u'>\n", mali_api_trace_gen_param_id(), type, ((u32)ptr) & 0x3);
		}

		if (ptr != NULL)
		{
			u32 i;
			
			_mali_sys_fprintf(xml_file, "   <values count='%u'>", count);

			if (dir & MALI_API_TRACE_IN || (dir & MALI_API_TRACE_TEST_IN && mali_api_trace_flag_enabled(MALI_API_TRACE_TESTING)))
			{
				for (i = 0; i < count; i++)
				{
					_mali_sys_fprintf(xml_file, "%d ", ptr[i]);
				}
			}

			_mali_sys_fprintf(xml_file, "</values>\n");
		}

		_mali_sys_fprintf(xml_file, "  </param>\n");
	}
}

MALI_EXPORT
void mali_api_trace_param_uint32_array(enum mali_api_trace_direction dir, const u32* ptr, u32 count, const char* type)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		u32 param_id = 0;

		if ((dir & MALI_API_TRACE_ID0) == 0)
		{
			param_id = mali_api_trace_gen_param_id();
		}

		/* Check alignment of pointer */
		if ((((u32)ptr) & 0x3) == 0)
		{
			/* aligned correctly for a 32 bit value */
			_mali_sys_fprintf(xml_file, "  <param id='%u' type='%s'>\n", param_id, type);
		}
		else
		{
			/* Aligned incorrectly, so make sure we include alignment info */
			_mali_sys_fprintf(xml_file, "  <param id='%u' type='%s' misalign='%u'>\n", param_id, type, ((u32)ptr) & 0x3);
		}

		if (ptr != NULL)
		{
			u32 i;
			
			_mali_sys_fprintf(xml_file, "   <values count='%u'>", count);

			if (dir & MALI_API_TRACE_IN || (dir & MALI_API_TRACE_TEST_IN && mali_api_trace_flag_enabled(MALI_API_TRACE_TESTING)))
			{
				for (i = 0; i < count; i++)
				{
					_mali_sys_fprintf(xml_file, "%u ", ptr[i]);
				}
			}

			_mali_sys_fprintf(xml_file, "</values>\n");
		}

		_mali_sys_fprintf(xml_file, "  </param>\n");
	}
}

MALI_EXPORT
void mali_api_trace_param_float_array(enum mali_api_trace_direction dir, const float* ptr, u32 count, const char* type)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		/* Check alignment of pointer */
		if ((((u32)ptr) & 0x3) == 0)
		{
			/* aligned correctly for a 32 bit value */
			_mali_sys_fprintf(xml_file, "  <param id='%u' type='%s'>\n", mali_api_trace_gen_param_id(), type);
		}
		else
		{
			/* Aligned incorrectly, so make sure we include alignment info */
			_mali_sys_fprintf(xml_file, "  <param id='%u' type='%s' misalign='%u'>\n", mali_api_trace_gen_param_id(), type, ((u32)ptr) & 0x3);
		}

		if (ptr != NULL)
		{
			u32 i;
			
			_mali_sys_fprintf(xml_file, "   <values count='%u'>", count);

			if (dir & MALI_API_TRACE_IN || (dir & MALI_API_TRACE_TEST_IN && mali_api_trace_flag_enabled(MALI_API_TRACE_TESTING)))
			{
				for (i = 0; i < count; i++)
				{
					_mali_sys_fprintf(xml_file, "%.10f ", (double)ptr[i]);
				}
			}

			_mali_sys_fprintf(xml_file, "</values>\n");
		}

		_mali_sys_fprintf(xml_file, "  </param>\n");
	}
}

MALI_EXPORT
void mali_api_trace_param_cstr_array(const char** ptr, u32 count)
{
	/*
	 * dumping an array of C strings is a bit harder than for other types, since length of string can vary
	 * thats why we use <string></string> inside <values></values> in this case
	 */

	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		_mali_sys_fprintf(xml_file, "  <param id='%u' type='cstrarray'>\n", mali_api_trace_gen_param_id());

		if (ptr != NULL)
		{
			u32 i;
			
			_mali_sys_fprintf(xml_file, "   <values count='%u'>\n", count);

			for (i = 0; i < count; i++)
			{
				if (NULL != ptr[i])
				{
					_mali_sys_fprintf(xml_file, "    <string>");

					mali_api_trace_write_xml_as_hex(ptr[i], _mali_sys_strlen(ptr[i]));

					_mali_sys_fprintf(xml_file, "</string>\n");
				}
				else
				{
					_mali_sys_fprintf(xml_file, "    <string></string>\n");
				}
			}

			_mali_sys_fprintf(xml_file, "   </values>\n");
		}

		_mali_sys_fprintf(xml_file, "  </param>\n");
	}
}

void mali_api_trace_param_error(const char* msg)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		_mali_sys_fprintf(xml_file, "  <param type='error'>\n");

		if (msg != NULL)
		{
			_mali_sys_fprintf(xml_file, "   <value>%s</value>\n", msg);
		}

		_mali_sys_fprintf(xml_file, "  </param>\n");
	}
}

MALI_EXPORT
void mali_api_trace_return_unsigned_integer(u32 value, const char* type)
{
	/*
	 * There are very few functions that return a value, so logging these will not add much size.
	 * Most of these are handles, which is essensial anyway.
	 */
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		_mali_sys_fprintf(xml_file, "  <return id='0' type='%s'>\n", type);
		_mali_sys_fprintf(xml_file, "   <value>%u</value>\n", (u32)value);
		_mali_sys_fprintf(xml_file, "  </return>\n");
	}
}

MALI_EXPORT
void mali_api_trace_return_signed_integer(s32 value, const char* type)
{
	/*
	 * There are very few functions that return a value, so logging these will not add much size.
	 * Most of these are handles, which is essensial anyway.
	 */
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		_mali_sys_fprintf(xml_file, "  <return id='0' type='%s'>\n", type);
		_mali_sys_fprintf(xml_file, "   <value>%d</value>\n", value);
		_mali_sys_fprintf(xml_file, "  </return>\n");
	}
}

MALI_EXPORT
void mali_api_trace_return_float(float value, const char* type)
{
	/*
	 * There are very few functions that return a value, so logging these will not add much size.
	 * Most of these are handles, which is essensial anyway.
	 */
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		_mali_sys_fprintf(xml_file, "  <return id='0' type='%s'>\n", type);
		_mali_sys_fprintf(xml_file, "   <value>%.10f</value>\n", (double)value);
		_mali_sys_fprintf(xml_file, "  </return>\n");
	}
}

MALI_EXPORT
void mali_api_trace_return_uint8_array(u32 id, const u8* ptr, u32 count, const char* type)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_OUTPUT))
	{
		_mali_sys_fprintf(xml_file, "  <return id='%u' type='%s'>\n", id, type);

		if (ptr != NULL)
		{
			u32 i;
			
			_mali_sys_fprintf(xml_file, "   <values count='%u'>", count);

			for (i = 0; i < count; i++)
			{
				_mali_sys_fprintf(xml_file, "%02x", (u32)ptr[i]);
			}

			_mali_sys_fprintf(xml_file, "</values>\n");
		}

		_mali_sys_fprintf(xml_file, "  </return>\n");
	}
}

MALI_EXPORT
void mali_api_trace_return_int32_array(u32 id, const s32* ptr, u32 count, const char* type)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_OUTPUT))
	{
		_mali_sys_fprintf(xml_file, "  <return id='%u' type='%s'>\n", id, type);

		if (ptr != NULL)
		{
			u32 i;
			
			_mali_sys_fprintf(xml_file, "   <values count='%u'>", count);

			for (i = 0; i < count; i++)
			{
				_mali_sys_fprintf(xml_file, "%d ", ptr[i]);
			}

			_mali_sys_fprintf(xml_file, "</values>\n");
		}

		_mali_sys_fprintf(xml_file, "  </return>\n");
	}
}

static void mali_api_trace_return_uint32_array_helper(u32 id, const u32* ptr, u32 count, const char* type)
{

	_mali_sys_fprintf(xml_file, "  <return id='%u' type='%s'>\n", id, type);

	if (ptr != NULL)
	{
		u32 i;
		
		_mali_sys_fprintf(xml_file, "   <values count='%u'>", count);

		for (i = 0; i < count; i++)
		{
			_mali_sys_fprintf(xml_file, "%u ", ptr[i]);
		}

		_mali_sys_fprintf(xml_file, "</values>\n");
	}

	_mali_sys_fprintf(xml_file, "  </return>\n");
}

MALI_EXPORT
void mali_api_trace_return_uint32_array(u32 id, const u32* ptr, u32 count, const char* type)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_OUTPUT))
	{
		mali_api_trace_return_uint32_array_helper(id, ptr, count, type);
	}
}

MALI_EXPORT
void mali_api_trace_return_handle_array(u32 id, const u32* ptr, u32 count, const char* type)
{
	/* special thing about returning handles as arrays is that it is needed, thus put into the MALI_API_TRACE_PARAMETERS category */

	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		mali_api_trace_return_uint32_array_helper(id, ptr, count, type);
	}
}

MALI_EXPORT
void mali_api_trace_return_float_array(u32 id, const float* ptr, u32 count, const char* type)
{
	if (mali_api_trace_flag_enabled(MALI_API_TRACE_OUTPUT))
	{
		_mali_sys_fprintf(xml_file, "  <return id='%u' type='%s'>\n", id, type);

		if (ptr != NULL)
		{
			u32 i;
			
			_mali_sys_fprintf(xml_file, "   <values count='%u'>", count);

			for (i = 0; i < count; i++)
			{
				_mali_sys_fprintf(xml_file, "%.10f ", (double)ptr[i]);
			}

			_mali_sys_fprintf(xml_file, "</values>\n");
		}

		_mali_sys_fprintf(xml_file, "  </return>\n");
	}
}

static u32 generate_crc(const char* buffer, u32 size)
{
	/* home made CRC checksum, should consider to replace with a propper one some day */
	u32 i;
	u32 crc = 0;
	char* crc_ptr = (char*)&crc;

	for (i = 0; i < size; i++)
	{
		crc_ptr[i & 3] += buffer[i];
	}

	return crc;
}

MALI_EXPORT
void mali_api_trace_dump_user_buffer(const char* buffer, u32 size)
{
	static struct user_buffer
	{
		const char* ptr;
		u32 size;
		u32 crc;
	} user_buffers[MAX_USER_BUFFERS_TO_WATCH] = { { NULL, 0, 0 }, };
	
	static int insert_pos = 0;

	if (mali_api_trace_flag_enabled(MALI_API_TRACE_PARAMETERS))
	{
		int current_insert_pos = insert_pos; /* this will change if we find the buffer in the list, but with another size or crc */
		mali_bool use_crc = MALI_FALSE;

		if (mali_api_trace_flag_enabled(MALI_API_TRACE_USER_BUFFER_CRC))
		{
			use_crc = MALI_TRUE;
		}

		if (NULL != buffer)
		{
			u32 crc = 0;

			if (MALI_TRUE == use_crc)
			{
				unsigned int i;
				crc = generate_crc(buffer, size);

				for (i = 0; i < MAX_USER_BUFFERS_TO_WATCH; i++)
				{
					if (buffer == user_buffers[i].ptr)
					{
						if (size == user_buffers[i].size && crc == user_buffers[i].crc)
						{
							return; /* early out, we have dumped this already */
						}

						current_insert_pos = i; /* replace this entry */
					}
				}
			}

			_mali_sys_fprintf(xml_file, "  <buffer address='%u' size='%u'>", (u32)buffer, size);

			mali_api_trace_write_xml_as_hex(buffer, size);

			_mali_sys_fprintf(xml_file, "</buffer>\n");

			if (MALI_TRUE == use_crc)
			{
				/* mark this buffer as dumped */
				user_buffers[current_insert_pos].ptr = buffer;
				user_buffers[current_insert_pos].size = size;
				user_buffers[current_insert_pos].crc = crc;

				if (current_insert_pos == insert_pos)
				{
					insert_pos++;
					if (insert_pos >= MAX_USER_BUFFERS_TO_WATCH)
					{
						insert_pos = 0;
					}
				}
			}
		}
	}
}
