/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */


#include <shared/binary_shader/bs_error.h>

#include <mali_system.h>

#include <base/mali_runtime.h>

static const char *out_of_memory_error = "F0001 Out of memory while setting error log\n";

MALI_EXPORT void bs_get_log(struct bs_log *log, u32 bufsize, int *length, char *infolog)
{
	const char *log_string;
	unsigned int ilength, loglen;

	MALI_DEBUG_ASSERT_POINTER(log);

	/* prefer the proper log to the out of memory error */
	log_string = (log->log != NULL) ? log->log : log->log_out_of_memory;

	if( NULL == log_string )
	{
		if(bufsize > 0 && infolog != NULL) infolog[0] = '\0';

		if( NULL != length )
		{
			*length = 0;
		}
		return;
	}

	/* If bufsize is ZERO then silently return with length=0, if the user wishes to know the number of chars in the info log then
	 * he/she should be using GetShaderiv or GetProgramiv with INFO_LOG_LENGTH instead */
	if( 0 == bufsize )
	{
		if( NULL != length )
		{
			*length = 0;
		}
		return;
	}

	/* copy the log to the infolog */
	loglen = _mali_sys_strlen( log_string ) + 1; /* length of log including termchar */
	ilength = (bufsize < loglen)?(bufsize):(loglen); /* length written, including termchar */

	if( NULL != infolog )
	{
		_mali_sys_memcpy( infolog, log_string, ilength -1);
		infolog[ilength-1] = '\0';
	}

	/* Returns the number of chars written in the infolog excluding the null terminator */
	if( NULL != length )
	{
		*length = ilength-1;
	}
}

MALI_EXPORT void bs_get_log_length(struct bs_log *log, int *length)
{
	const char *log_string;

	MALI_DEBUG_ASSERT_POINTER(log);
	MALI_DEBUG_ASSERT_POINTER(length);

	/* prefer the proper log to the out of memory error */
	log_string = (log->log != NULL) ? log->log : log->log_out_of_memory;

	if ( log_string != NULL )
	{
		*length = _mali_sys_strlen( log_string );
	} else
	{
		*length = 0;
	}
}

MALI_EXPORT void bs_clear_error(bs_log *log)
{
	MALI_DEBUG_ASSERT_POINTER(log);

	/* this error message should not be deleted */
	if( NULL != log->log_out_of_memory )
	{
		/* if we ran out of memory, the dynamic log should have been cleared */
		MALI_DEBUG_ASSERT( NULL == log->log, ("Inconsistent error log") );
		log->log_out_of_memory = NULL;
		return;
	}

	if( NULL != log->log )
	{
		/* free log */
		MALI_DEBUG_ASSERT( NULL == log->log_out_of_memory, ("Inconsistent error log") );
		_mali_sys_free( log->log );
		log->log = NULL;
	}
}

MALI_EXPORT void bs_set_error_out_of_memory(bs_log *log)
{
	MALI_DEBUG_ASSERT_POINTER(log);
	bs_clear_error(log);
	log->log_out_of_memory = out_of_memory_error;
}

MALI_EXPORT mali_bool bs_is_error_log_set_to_out_of_memory(bs_log *log)
{
	MALI_DEBUG_ASSERT_POINTER(log);
	return log->log_out_of_memory != NULL;
}

MALI_EXPORT void bs_set_error(bs_log* log, char* errortype, char* message )
{
	char* buffer;
	const char *oldlog = NULL;
	unsigned int length, errortypelength, messagelength;
	unsigned int oldlength = 0;

	MALI_DEBUG_ASSERT_POINTER(log);
	MALI_DEBUG_ASSERT_POINTER(errortype);
	MALI_DEBUG_ASSERT_POINTER(message);

	if( NULL != log->log )
	{
		oldlength = _mali_sys_strlen(log->log);
		oldlog = log->log;
	} else if ( NULL != log->log_out_of_memory )
	{
		oldlength = _mali_sys_strlen(log->log_out_of_memory);
		oldlog = log->log_out_of_memory;
	}

	/*	Error is à la: "ERRORTYPE ERRORMESSAGE\n", consisting of a errortype, a space, the message and a newline. */
	errortypelength = _mali_sys_strlen(errortype);
	messagelength = _mali_sys_strlen(message);
	length = errortypelength + 1 + messagelength + 1 ;

	/* allocate new error log buffer */
	buffer = _mali_sys_malloc( length + oldlength + 1); /* +1 for termchar */
	if( NULL == buffer )
	{
		bs_set_error_out_of_memory(log);
		return;
	}

	/* populate buffer. This would've been so much easier with an snprintf */
	if( oldlength > 0 ) _mali_sys_memcpy(buffer, oldlog, oldlength);
	_mali_sys_memcpy(&buffer[oldlength], errortype, errortypelength);
	_mali_sys_memcpy(&buffer[oldlength + errortypelength], " ", 1);
	_mali_sys_memcpy(&buffer[oldlength + errortypelength + 1], message, messagelength);
	_mali_sys_memcpy(&buffer[oldlength + errortypelength + 1 + messagelength], "\n", 1);
	buffer[oldlength + length] = '\0'; /* terminate */

	bs_clear_error(log);
	log->log = buffer;
}

MALI_EXPORT void  bs_set_program_validate_error_sampler_out_of_range(bs_program* po, char* samplername, unsigned int bound_unit, unsigned int max_unit )
{
	char* buffer;
	int buffersize;

	MALI_DEBUG_ASSERT_POINTER(po);
	MALI_DEBUG_ASSERT_POINTER(samplername);

	buffersize = _mali_sys_strlen(samplername) + 1000; /* big enough for any tempbuffer */
	buffer = _mali_sys_malloc(buffersize);
	if(buffer == NULL)
	{
		bs_set_error_out_of_memory(&po->log);
		return;
	}

	_mali_sys_snprintf(buffer, buffersize, "Sampler '%s' bound to texture unit %i, but max is %i", samplername, bound_unit, max_unit-1);

	bs_set_error(&po->log, BS_ERR_VALIDATE_FAILED, buffer);
	_mali_sys_free(buffer);
}


MALI_EXPORT void bs_set_program_validate_error_sampler_of_different_types_share_unit(bs_program* po, char* samplername1, char* samplername2, unsigned int unit )
{
	char* buffer;
	int buffersize;

	MALI_DEBUG_ASSERT_POINTER(po);
	MALI_DEBUG_ASSERT_POINTER(samplername1);
	MALI_DEBUG_ASSERT_POINTER(samplername2);

	buffersize = _mali_sys_strlen(samplername1) + _mali_sys_strlen(samplername2) + 1000; /* big enough for any tempbuffer */
	buffer = _mali_sys_malloc(buffersize);
	if(buffer == NULL)
	{
		bs_set_error_out_of_memory(&po->log);
		return;
	}

	_mali_sys_snprintf(buffer, buffersize, "Sampler '%s' and '%s' are of different types, but share texture unit %i.",
	                                       samplername1, samplername2, unit);

	bs_set_error(&po->log, BS_ERR_VALIDATE_FAILED, buffer);
	_mali_sys_free(buffer);
}

MALI_EXPORT void bs_set_program_link_error_attribute_bound_outsize_of_legal_range(bs_program* po, char* attributename, unsigned int usedslot, unsigned int maxslot)
{
	char* buffer;
	int buffersize;

	MALI_DEBUG_ASSERT_POINTER(po);
	MALI_DEBUG_ASSERT_POINTER(attributename);

	buffersize = _mali_sys_strlen(attributename) + 1000; /* big enough for any tempbuffer */
	buffer = _mali_sys_malloc(buffersize);
	if(buffer == NULL)
	{
		bs_set_error_out_of_memory(&po->log);
		return;
	}

	_mali_sys_snprintf(buffer, buffersize, "Attribute '%s' bound outside of the valid range; using slot %i but max is %i",
	                                       attributename, usedslot, maxslot);

	bs_set_error(&po->log, BS_ERR_LINK_TOO_MANY_ATTRIBUTES, buffer);
	_mali_sys_free(buffer);
}

MALI_EXPORT void bs_set_program_link_error_missing_vertex_shader_varying(bs_program* po, char* varyingname)
{
	char* buffer;
	int buffersize;

	MALI_DEBUG_ASSERT_POINTER(po);
	MALI_DEBUG_ASSERT_POINTER(varyingname);

	buffersize = _mali_sys_strlen(varyingname) + 1000; /* big enough for any tempbuffer */
	buffer = _mali_sys_malloc(buffersize);
	if(buffer == NULL)
	{
		bs_set_error_out_of_memory(&po->log);
		return;
	}

	_mali_sys_snprintf(buffer, buffersize, "Varying '%s' not found in vertex shader", varyingname);

	bs_set_error(&po->log, BS_ERR_LINK_FRAGMENT_SHADER_UNDECLARED_VARYING, buffer);
	_mali_sys_free(buffer);
}

MALI_EXPORT void bs_set_program_link_error_type_mismatch_varying(bs_program* po, char* reason)
{
	char* buffer;
	int buffersize;

	MALI_DEBUG_ASSERT_POINTER(po);
	MALI_DEBUG_ASSERT_POINTER(reason);

	buffersize = _mali_sys_strlen(reason) + 1000; /* big enough for any tempbuffer */
	buffer = _mali_sys_malloc(buffersize);
	if(buffer == NULL)
	{
		bs_set_error_out_of_memory(&po->log);
		return;
	}

	_mali_sys_snprintf(buffer, buffersize, "Varying %s", reason);

	bs_set_error(&po->log, BS_ERR_LINK_VARYING_TYPE_MISMATCH, buffer);
	_mali_sys_free(buffer);
}
