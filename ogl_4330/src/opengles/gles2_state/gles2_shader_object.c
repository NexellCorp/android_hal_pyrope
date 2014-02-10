/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles2_shader_object.h"

#include <mali_system.h>

#include "gles2_program_object.h"
#include "../gles_util.h"
#include <shared/binary_shader/bs_object.h>
#include <shared/binary_shader/online_compiler_integration.h>
#include <shared/binary_shader/bs_loader.h>
#include <shared/binary_shader/bs_error.h>


gles2_shader_object* _gles2_shader_internal_alloc( GLenum type )
{
	gles2_shader_object* so = _mali_sys_malloc( sizeof( gles2_shader_object ) );
	if( !so ) return NULL;

	so->shader_type = type;
	so->delete_status = GL_FALSE;

	so->combined_source_length = 0;
	so->combined_source = NULL;
	so->source_string_count = 0;
	so->source_string_lengths_array = NULL;

	_mali_sys_atomic_initialize(&so->references, 0);

	__mali_shader_binary_state_init( &so->binary );

	return so;
}


void _gles2_shader_internal_free( gles2_shader_object *so )
{

	MALI_DEBUG_ASSERT_POINTER( so );
	MALI_DEBUG_ASSERT( (_mali_sys_atomic_get( &so->references ) == 0), ("Reference count %d is not zero, deleting object will lead to crashes!",
					   _mali_sys_atomic_get( &so->references )));

	__mali_shader_binary_state_reset( &so->binary );

	if(so->combined_source) _mali_sys_free( so->combined_source );
	if(so->source_string_lengths_array) _mali_sys_free( so->source_string_lengths_array );

	_mali_sys_free( so );
}

GLenum _gles2_create_shader(  mali_named_list *program_object_list, GLenum type, GLuint* name )
{
	gles2_shader_object *so;
	gles2_program_object_wrapper *pw;
	GLuint iname;
	mali_err_code error_code;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	/* assert proper shadertype */
	if( type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER )
	{
		return GL_INVALID_ENUM;
	}

	/* create shader object */
	so = _gles2_shader_internal_alloc( type );
	if( NULL == so )  /* out of memory, abort */
	{
		return GL_OUT_OF_MEMORY;
	}

	/* create wrapper struct */
	pw = _mali_sys_malloc( sizeof( gles2_program_object_wrapper ) );
	if( !pw )
	{
		_gles2_shader_internal_free( so );
		return GL_OUT_OF_MEMORY;
	}

	pw->content = so;
	pw->type = GLES_SHADER;

	/* place object in named list */
	iname = __mali_named_list_get_unused_name( program_object_list );
	if( 0 == iname ) /* we're out of names; somebody allocated 2^32-1 programs.  Abort and return */
	{
		_mali_sys_free( pw );
		_gles2_shader_internal_free( so );
		return GL_OUT_OF_MEMORY;
	}

	error_code = __mali_named_list_insert( program_object_list, iname, pw );

	if( error_code != MALI_ERR_NO_ERROR ) /* out of memory */
	{
		_mali_sys_free( pw );
		_gles2_shader_internal_free( so );
		return GL_OUT_OF_MEMORY;
	}

	*name = iname;
	return GL_NO_ERROR;
}

GLenum _gles2_delete_shader(mali_named_list *program_object_list, GLuint name )
{
	gles2_shader_object* so;

	gles2_program_object_wrapper* pw;

	if( 0 == name ) return GL_NO_ERROR; /* Silently ignore the name ZERO */

	pw = __mali_named_list_get( program_object_list, name );

	/* assert object exist */
	MALI_CHECK_NON_NULL( pw, GL_INVALID_VALUE );

	/* assert object is a shader */
	MALI_CHECK( pw->type == GLES_SHADER, GL_INVALID_OPERATION );

	so = pw->content;

	/* if shader has bindings, bail out */
	if( _mali_sys_atomic_get( &so->references ) > 0 )
	{
		so->delete_status = GL_TRUE; /* flag for later deletion */
		return GL_NO_ERROR;
	}

	/* delete the object and remove from list*/
	_gles2_shader_internal_free( so );

	_mali_sys_free( pw );
	__mali_named_list_remove( program_object_list, name );

	return GL_NO_ERROR;
}

GLenum _gles2_release_shader_compiler( void )
{
	return GL_NO_ERROR;
}

GLboolean _gles2_is_shader( mali_named_list *program_object_list, GLuint program )
{
	GLenum object_type;
	GLboolean retval;
	MALI_DEBUG_ASSERT_POINTER( program_object_list );
	_gles2_program_internal_get_type( program_object_list, program, &object_type );
	retval = (object_type == GLES_SHADER)?GL_TRUE:GL_FALSE;
	return retval;
}


GLenum _gles2_shader_source( mali_named_list *program_object_list, GLuint shadername, GLsizei count, const char** string, const GLint* length )
{
	gles2_shader_object* so = NULL;
	GLenum object_type 		= GL_INVALID_VALUE;
	GLsizei comb_lengths 	= 0;
	s32* length_array 		= NULL;
	char* combined_source 	= NULL;
	GLsizei i;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	/* assert count >= 0 */
	if( count < 0 ) return GL_INVALID_VALUE;

	so = _gles2_program_internal_get_type( program_object_list, shadername, &object_type );

	/* assert object exist */
	MALI_CHECK( GL_INVALID_VALUE != object_type, GL_INVALID_VALUE );

	/* assert object is a shader */
	MALI_CHECK( GLES_SHADER == object_type, GL_INVALID_OPERATION );

	/* process/merge lengths of inputted strings */
	length_array = _mali_sys_malloc( sizeof(s32)*count );
	MALI_CHECK_NON_NULL( length_array, GL_OUT_OF_MEMORY );

	/* If an element in length is negative, its accompanying string is nullterminated.
	 * If length is NULL, all strings in the string argument are considered nullterminated. */
	for(i = 0; i < count; i++)
	{
		if( length && length[i] >= 0 )
		{
			comb_lengths += length[i];
			length_array[i] = length[i];
		}
		else
		{
			u32 len =  _mali_sys_strlen(string[i]);
			comb_lengths += len;
			length_array[i] = len;
		}
	}

	/* Need to add the final null terminator */
	comb_lengths = comb_lengths + 1;

	/* create combined source string */
	combined_source = _mali_sys_malloc( ( comb_lengths ) * sizeof(char) );
	if(combined_source == NULL)
	{ /* out of memory. */
		_mali_sys_free( length_array );
		return GL_OUT_OF_MEMORY;
	}

	/* build source string */
	{
		u32 combined_source_length_so_far = 0;
		combined_source[0] = '\0'; /* terminate buffer - required as an init condition for the loop below */
		for( i = 0; i < count; i++ )
		{
			_mali_sys_strncpy(&combined_source[combined_source_length_so_far], string[i], length_array[i]);
			combined_source_length_so_far += length_array[i];
			combined_source[combined_source_length_so_far] = '\0';
		}
	}

	/* clean old shaderstrings */
	if( so->combined_source ) _mali_sys_free( so->combined_source );
	if( so->source_string_lengths_array) _mali_sys_free( so->source_string_lengths_array );

	so->combined_source_length = comb_lengths;
	so->source_string_count = count;
	so->source_string_lengths_array = length_array;
	so->combined_source = combined_source;

	return GL_NO_ERROR;
}

GLenum _gles2_get_shader_source (mali_named_list * program_object_list, GLuint shadername, GLsizei bufSize, GLsizei *length, char *source)
{
	gles2_shader_object* so = NULL;
	GLenum object_type = GL_INVALID_VALUE;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	/* Shader name ZERO is reserved and shared with program objects.  */
	if( 0 == shadername ) return GL_INVALID_OPERATION;

	if( bufSize < 0 ) return GL_INVALID_VALUE;

	so = _gles2_program_internal_get_type( program_object_list, shadername, &object_type );

	/* assert object exist */
	MALI_CHECK( GL_INVALID_VALUE != object_type, GL_INVALID_VALUE );

	/* assert object is a shader */
	MALI_CHECK( GLES_SHADER == object_type, GL_INVALID_OPERATION );

	/* if no buffer, abort */
	if ( NULL == so->combined_source || 0 == bufSize || NULL == source )
	{
		if( length ) *length = 0; 								/* update length if possible */
		if( bufSize > 0 && source != NULL ) source[0] = '\0'; 	/* nullterminate buffer if possible */
		return GL_NO_ERROR;
	}

	/* copy out string to external buffer*/
	_mali_sys_strncpy(source, so->combined_source, bufSize);
	source[bufSize-1] = '\0';

	/* If length is non-NULL then returns the number of characters written into source, excluding the null terminator */
	if( NULL != length )
	{
		if ((u32)bufSize >= so->combined_source_length)
		{
			*length = so->combined_source_length - 1;
		}
		else
		{
			*length = bufSize - 1;
		}
	}
	return GL_NO_ERROR;
}

GLenum _gles2_compile_shader( mali_named_list * program_object_list, GLuint shadername )
{
	gles2_shader_object* so = NULL;
	GLenum object_type		= GL_INVALID_VALUE;
	mali_err_code error_code;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	so = _gles2_program_internal_get_type( program_object_list, shadername, &object_type );

	/* assert object exist */
	MALI_CHECK( GL_INVALID_VALUE != object_type, GL_INVALID_VALUE );

	/* assert object is a shader */
	MALI_CHECK( GLES_SHADER == object_type, GL_INVALID_OPERATION );

	/* clear out the binary struct */
	__mali_shader_binary_state_reset(&so->binary);

	/* compile. Sends shaderdata to the compiler, and set the compile status flag accordingly */
	error_code = __mali_compile_essl_shader( &so->binary, so->shader_type, so->combined_source, so->source_string_lengths_array, so->source_string_count );

	/* if out of memory, we have to set a GL_OUT_OF_MEMORY_ERROR */
	MALI_CHECK( MALI_ERR_OUT_OF_MEMORY != error_code, GL_OUT_OF_MEMORY );
	/* all other return values are inconsequential */

	return GL_NO_ERROR;
}

GLenum _gles2_get_shader_info_log( mali_named_list* program_object_list, GLuint shader, GLsizei bufsize, GLsizei *length,  char *infolog )
{
	gles2_shader_object *so;
	GLenum object_type;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	/* assert that bufsize >= 0 */
	if( bufsize < 0 )
	{
		return GL_INVALID_VALUE;
	}

	/* assert that input is a valid object */
	so = _gles2_program_internal_get_type( program_object_list, shader, &object_type );
	MALI_CHECK( GL_INVALID_VALUE != object_type, GL_INVALID_VALUE );

	/* assert that input is a program object */
	MALI_CHECK( GLES_SHADER == object_type, GL_INVALID_OPERATION );

	bs_get_log(&so->binary.log, bufsize, length, infolog);

	return GL_NO_ERROR;

}

GLenum _gles2_get_shader( mali_named_list *program_object_list, GLuint shader, GLenum pname, GLint* params )
{
	gles2_shader_object *so;
	GLenum object_type;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	if( shader == 0 )return GL_INVALID_VALUE;

	/* assert that input is a valid object */
	so = _gles2_program_internal_get_type( program_object_list, shader, &object_type );
	MALI_CHECK( object_type != GL_INVALID_VALUE, GL_INVALID_VALUE );

	/* assert that input is a program object */
	MALI_CHECK( object_type == GLES_SHADER, GL_INVALID_OPERATION );

	MALI_CHECK_NON_NULL( params, GL_NO_ERROR );

	switch( pname )
	{
		case GL_SHADER_TYPE:
			*params = (GLint) so->shader_type;
			break;
		case GL_DELETE_STATUS:
			*params = (GLint) so->delete_status;
			break;
		case GL_COMPILE_STATUS:
			*params = (GLint) so->binary.compiled;
			break;
		case GL_INFO_LOG_LENGTH:
			bs_get_log_length(&so->binary.log, params);
			break;
		case GL_SHADER_SOURCE_LENGTH:
			*params = (GLint) so->combined_source_length;
			break;
		default:
			return GL_INVALID_ENUM;
	}

	return GL_NO_ERROR;

}

GLenum _gles2_shader_binary( mali_named_list *program_object_list, GLsizei n, const GLuint *shaders, GLenum binaryformat, const void *binary, GLsizei length )
{
	int i;
	int vertex_shader_count, fragment_shader_count;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	/* If the driver is configured to have no binary formats, we must return INVALID_OPERATION here. */
	if (GLES2_NUM_SHADER_BINARY_FORMATS == 0) return GL_INVALID_OPERATION;

	/* Reset all shaders and count shader object types  */
	vertex_shader_count = 0;
	fragment_shader_count = 0;
	for (i = 0; i < n; ++i)
	{
		gles2_shader_object *so = NULL;
		GLenum object_type = GL_INVALID_ENUM;
		so = (gles2_shader_object *)_gles2_program_internal_get_type(program_object_list, shaders[i], &object_type);
		if (NULL != so && GLES_SHADER == object_type)
		{
			if (GL_VERTEX_SHADER == so->shader_type) vertex_shader_count++;
			else                                     fragment_shader_count++;
			__mali_shader_binary_state_reset(&so->binary);
		}
	}

	/* Check that the format of the binary is correct as defined by the ARM_mali_shader_binary extension */
	if( binaryformat != GL_MALI_SHADER_BINARY_ARM ) return GL_INVALID_ENUM;

	/* Check that there's max 1 shader of each type */
	if( 1 < vertex_shader_count ) return GL_INVALID_OPERATION;
	if( 1 < fragment_shader_count ) return GL_INVALID_OPERATION;

	/* Load shader binaries */
	for (i = 0; i < n; ++i)
	{
		gles2_shader_object *so = NULL;
		GLenum object_type = GL_INVALID_ENUM;
		so = (gles2_shader_object *)_gles2_program_internal_get_type(program_object_list, shaders[i], &object_type);

		/* Verify shader */
		if (NULL == so) return GL_INVALID_VALUE; /* object doesn't exist */
		if (GLES_SHADER != object_type) return GL_INVALID_OPERATION; /* wrong type */

		/* Silently ignore NULL-pointer */
		if (NULL != binary)
		{
			mali_err_code err = __mali_binary_shader_load( &so->binary, so->shader_type, (char*)binary, length );
			if (MALI_ERR_FUNCTION_FAILED == err) return GL_INVALID_VALUE;
			if (MALI_ERR_OUT_OF_MEMORY == err) return GL_OUT_OF_MEMORY;
			MALI_DEBUG_ASSERT(MALI_ERR_NO_ERROR == err, ("Unexpected error code 0x%x", err));
		}
	}

	return GL_NO_ERROR;
}

void _gles2_shader_object_deref(gles2_shader_object *so)
{
	u32 ref_count = 0;
	MALI_DEBUG_ASSERT_POINTER( so );

	MALI_DEBUG_ASSERT((_mali_sys_atomic_get( &so->references ) > 0 ), ("ref_count on framebuffer object is already 0, the object should have been deleted by now!\n"));

	/* dereference */
	ref_count = _mali_sys_atomic_dec_and_return( &so->references );
	if ( ref_count > 0) return;
}



