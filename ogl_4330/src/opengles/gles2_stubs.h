/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * ( C ) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles2_stubs.h
 * @brief Contains stub versions of the second layer functions called by all the
 *        OpenGL ES 2.x API/entrypoint functions that are not present in OpenGL ES 1.x.
 *
 * @note These functions are used to populate the OpenGL ES 2.x function pointers in
 *       the vtable when it is used by an OpenGL ES 1.x context and will silently fail
 *       with an instrumented error.
 */

#ifndef _GLES2_STUBS_H_
#define _GLES2_STUBS_H_

#include "gles_base.h"
#include "gles_ftype.h"
#include <mali_system.h>

#include "mali_instrumented_context.h"
#include "mali_log.h"

#include <shared/mali_named_list.h>

struct gles_context;
struct gles_blend;
struct gles2_program_environment;
struct gles_program_rendering_state;

MALI_STATIC void _gles2_log_incorrect_api_error(const char *function)
{
	MALI_IGNORE(function);
	
	MALI_INSTRUMENTED_LOG((_instrumented_get_context(), MALI_LOG_LEVEL_ERROR, "Calling %s while an OpenGL ES 2.x context is set as current", function));
}

GLenum _gles2_attach_shader_stub( mali_named_list *program_object_list, GLuint program, GLuint shader )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );
	MALI_IGNORE( shader );

	_gles2_log_incorrect_api_error("glAttachShader");

	return GL_NO_ERROR;
}

GLenum _gles2_bind_attrib_location_stub( mali_named_list* program_object_list, GLuint program, GLuint index, const char* name )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );
	MALI_IGNORE( index );
	MALI_IGNORE( name );

	_gles2_log_incorrect_api_error("glBindAttribLocation");

	return GL_NO_ERROR;
}

GLenum _gles2_blend_color_stub( struct gles_context *ctx, GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( red );
	MALI_IGNORE( green );
	MALI_IGNORE( blue );
	MALI_IGNORE( alpha );

	_gles2_log_incorrect_api_error("glBlendColor");

	return GL_NO_ERROR;
}

GLenum _gles2_blend_equation_stub( struct gles_context *ctx, GLenum mode_rgb, GLenum mode_alpha )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( mode_rgb );
	MALI_IGNORE( mode_alpha );

	_gles2_log_incorrect_api_error("glBlendEquation");

	return GL_NO_ERROR;
}

GLenum _gles2_blend_equation_separate_stub( struct gles_context *ctx, GLenum mode_rgb, GLenum mode_alpha )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( mode_rgb );
	MALI_IGNORE( mode_alpha );

	_gles2_log_incorrect_api_error("glBlendEquationSeparate");

	return GL_NO_ERROR;
}

GLenum _gles2_blend_func_separate_stub( struct gles_context *ctx, GLenum src_rgb, GLenum dst_rgb, GLenum src_alpha, GLenum dst_alpha )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( src_rgb );
	MALI_IGNORE( dst_rgb );
	MALI_IGNORE( src_alpha );
	MALI_IGNORE( dst_alpha );

	_gles2_log_incorrect_api_error("glBlendFuncSeparate");

	return GL_NO_ERROR;
}

GLenum _gles2_compile_shader_stub( mali_named_list *program_object_list, GLuint shadername )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( shadername );

	_gles2_log_incorrect_api_error("glCompileShader");

	return GL_NO_ERROR;
}

GLenum _gles2_create_program_stub( mali_named_list *program_object_list, GLuint *program )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );

	_gles2_log_incorrect_api_error("glCreateProgram");

	return GL_NO_ERROR;
}

GLenum _gles2_create_shader_stub( mali_named_list *program_object_list, GLenum type, GLuint* name )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( type );
	MALI_IGNORE( name );

	_gles2_log_incorrect_api_error("glCreateShader");

	return GL_NO_ERROR;
}

GLenum _gles2_delete_program_stub( mali_named_list *program_object_list, GLuint name )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( name );

	_gles2_log_incorrect_api_error("glDeleteProgram");

	return GL_NO_ERROR;
}

GLenum _gles2_delete_shader_stub( mali_named_list *program_object_list, GLuint name )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( name );

	_gles2_log_incorrect_api_error("glDeleteShader");

	return GL_NO_ERROR;
}

GLenum _gles2_detach_shader_stub( mali_named_list *program_object_list, struct gles2_program_environment *program_env, GLuint program, GLuint shader )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program_env );
	MALI_IGNORE( program );
	MALI_IGNORE( shader );

	_gles2_log_incorrect_api_error("glDetachShader");

	return GL_NO_ERROR;
}

GLenum _gles2_disable_vertex_attrib_array_stub( struct gles_context* ctx, GLuint index )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( index );

	_gles2_log_incorrect_api_error("glDisableVertexAttribArray");

	return GL_NO_ERROR;
}

GLenum _gles2_enable_vertex_attrib_array_stub( struct gles_context* ctx, GLuint index )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( index );

	_gles2_log_incorrect_api_error("glEnableVertexAttribArray");

	return GL_NO_ERROR;
}

GLenum _gles2_get_active_attrib_stub(
	mali_named_list* program_object_list,
	GLuint program,
	GLuint index,
	GLsizei bufsize,
	GLsizei* length,
	GLint *size,
	GLenum* type,
	char* name )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );
	MALI_IGNORE( index );
	MALI_IGNORE( bufsize );
	MALI_IGNORE( length );
	MALI_IGNORE( size );
	MALI_IGNORE( type );
	MALI_IGNORE( name );

	_gles2_log_incorrect_api_error("glGetActiveAttrib");

	return GL_NO_ERROR;
}

GLenum _gles2_get_active_uniform_stub(
	mali_named_list* program_object_list,
	GLuint program,
	GLuint index,
	GLsizei bufsize,
	GLsizei* length,
	GLint *size,
	GLenum* type,
	char* name )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );
	MALI_IGNORE( index );
	MALI_IGNORE( bufsize );
	MALI_IGNORE( length );
	MALI_IGNORE( size );
	MALI_IGNORE( type );
	MALI_IGNORE( name );

	_gles2_log_incorrect_api_error("glGetActiveUniform");

	return GL_NO_ERROR;
}

GLenum _gles2_get_attached_shaders_stub(mali_named_list* program_object_list, GLuint program, GLsizei maxcount, GLsizei *count, GLuint *shaders )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );
	MALI_IGNORE( maxcount );
	MALI_IGNORE( count );
	MALI_IGNORE( shaders );

	_gles2_log_incorrect_api_error("glGetAttachedShaders");

	return GL_NO_ERROR;
}

GLenum _gles2_get_attrib_location_stub( mali_named_list* program_object_list, GLuint program, const char* name, GLint *retval )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );
	MALI_IGNORE( name );
	MALI_IGNORE( retval );

	_gles2_log_incorrect_api_error("glGetAttribLocation");

	return GL_NO_ERROR;
}

GLenum _gles2_get_program_info_log_stub( mali_named_list *program_object_list, GLuint program, GLsizei bufsize, GLsizei *length, char *infolog )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );
	MALI_IGNORE( bufsize );
	MALI_IGNORE( length );
	MALI_IGNORE( infolog );

	_gles2_log_incorrect_api_error("glGetProgramInfoLog");

	return GL_NO_ERROR;
}

GLenum _gles2_get_programiv_stub( mali_named_list* program_object_list, GLuint program, GLenum pname, GLint* params )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );

	_gles2_log_incorrect_api_error("glGetProgramiv");

	return GL_NO_ERROR;
}

GLenum _gles2_get_shader_info_log_stub( mali_named_list* program_object_list, GLuint shader, GLsizei bufsize, GLsizei *length, char *infolog )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( shader );
	MALI_IGNORE( bufsize );
	MALI_IGNORE( length );
	MALI_IGNORE( infolog );

	_gles2_log_incorrect_api_error("glGetShaderInfoLog");

	return GL_NO_ERROR;
}

GLenum _gles2_get_shaderiv_stub( mali_named_list *program_object_list, GLuint shader, GLenum pname, GLint* params )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( shader );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );

	_gles2_log_incorrect_api_error("glGetShaderiv");

	return GL_NO_ERROR;
}

GLenum _gles2_get_shader_precision_format_stub( GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision )
{
	MALI_IGNORE( shadertype );
	MALI_IGNORE( precisiontype );
	MALI_IGNORE( range );
	MALI_IGNORE( precision );

	_gles2_log_incorrect_api_error("glGetShaderPrecisionFormat");

	return GL_NO_ERROR;
}

GLenum _gles2_get_shader_source_stub( mali_named_list *program_object_list, GLuint shadername, GLsizei buf_size, GLsizei *length, char *source )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( shadername );
	MALI_IGNORE( buf_size );
	MALI_IGNORE( length );
	MALI_IGNORE( source );

	_gles2_log_incorrect_api_error("glGetShaderSource");

	return GL_NO_ERROR;
}

GLenum _gles2_get_uniformfv_stub( mali_named_list* program_object_list, GLuint program, GLint location, void* output_array, GLenum output_type )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );
	MALI_IGNORE( location );
	MALI_IGNORE( output_array );
	MALI_IGNORE( output_type );

	_gles2_log_incorrect_api_error("glGetUniformfv");

	return GL_NO_ERROR;
}

GLenum _gles2_get_uniformiv_stub( mali_named_list* program_object_list, GLuint program, GLint location, void* output_array, GLenum output_type )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );
	MALI_IGNORE( location );
	MALI_IGNORE( output_array );
	MALI_IGNORE( output_type );

	_gles2_log_incorrect_api_error("glGetUniformiv");

	return GL_NO_ERROR;
}

GLenum _gles2_get_uniform_location_stub( mali_named_list* program_object_list, GLuint program, const char* name, GLint *retval )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );
	MALI_IGNORE( name );
	MALI_IGNORE( retval );

	_gles2_log_incorrect_api_error("glGetUniformLocation");

	return GL_NO_ERROR;
}

GLenum _gles2_get_vertex_attribfv_stub( struct gles_vertex_array *vertex_array, GLuint index, GLenum pname, gles_datatype type, void* param )
{
	MALI_IGNORE( vertex_array );
	MALI_IGNORE( index );
	MALI_IGNORE( pname );
	MALI_IGNORE( type );
	MALI_IGNORE( param );

	_gles2_log_incorrect_api_error("glGetVertexAttribfv");

	return GL_NO_ERROR;
}

GLenum _gles2_get_vertex_attribiv_stub( struct gles_vertex_array *vertex_array, GLuint index, GLenum pname, gles_datatype type, void* param )
{
	MALI_IGNORE( vertex_array );
	MALI_IGNORE( index );
	MALI_IGNORE( pname );
	MALI_IGNORE( type );
	MALI_IGNORE( param );

	_gles2_log_incorrect_api_error("glGetVertexAttribiv");

	return GL_NO_ERROR;
}

GLenum _gles2_get_vertex_attrib_pointerv_stub( struct gles_vertex_array *vertex_array, GLuint index, GLenum pname, void** param )
{
	MALI_IGNORE( vertex_array );
	MALI_IGNORE( index );
	MALI_IGNORE( pname );
	MALI_IGNORE( param );

	_gles2_log_incorrect_api_error("glGetVertexAttribPointerv");

	return GL_NO_ERROR;
}

GLboolean _gles2_is_program_stub( mali_named_list *program_object_list, GLuint program )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );

	_gles2_log_incorrect_api_error("glIsProgram");

	return GL_FALSE;
}

GLboolean _gles2_is_shader_stub( mali_named_list *program_object_list, GLuint program )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );

	_gles2_log_incorrect_api_error("glIsShader");

	return GL_FALSE;
}

GLenum _gles2_link_program_stub( struct gles_context *ctx, mali_named_list *program_object_list, GLuint program )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );

	_gles2_log_incorrect_api_error("glLinkProgram");

	return GL_NO_ERROR;
}

GLenum _gles2_release_shader_compiler_stub( void )
{
	_gles2_log_incorrect_api_error("glReleaseShaderCompiler");

	return GL_NO_ERROR;
}

GLenum _gles2_shader_binary_stub(
	mali_named_list *program_object_list,
	GLsizei n,
	const GLuint *shaders,
	GLenum binaryformat,
	const void *binary,
	GLsizei length )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( n );
	MALI_IGNORE( shaders );
	MALI_IGNORE( binaryformat );
	MALI_IGNORE( binary );
	MALI_IGNORE( length );

	_gles2_log_incorrect_api_error("glShaderBinary");

	return GL_NO_ERROR;
}

GLenum _gles2_shader_source_stub(
		mali_named_list *program_object_list,
		GLuint shadername,
		GLsizei count,
		const char** string,
		const GLint* length )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( shadername );
	MALI_IGNORE( count );
	MALI_IGNORE( string );
	MALI_IGNORE( length );

	_gles2_log_incorrect_api_error("glShaderSource");

	return GL_NO_ERROR;
}

GLenum _gles2_stencil_func_separate_stub( struct gles_context *ctx, GLenum face, GLenum func, GLint ref, GLuint mask )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( face );
	MALI_IGNORE( func );
	MALI_IGNORE( ref );
	MALI_IGNORE( mask );

	_gles2_log_incorrect_api_error("glStencilFuncSeparate");

	return GL_NO_ERROR;
}

GLenum _gles2_stencil_mask_separate_stub( struct gles_context *ctx, GLenum face, GLuint mask )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( face );
	MALI_IGNORE( mask );

	_gles2_log_incorrect_api_error("glStencilMaskSeparate");

	return GL_NO_ERROR;
}

GLenum _gles2_stencil_op_separate_stub( struct gles_context *ctx, GLenum face, GLenum fail, GLenum zfail, GLenum zpass )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( face );
	MALI_IGNORE( fail );
	MALI_IGNORE( zfail );
	MALI_IGNORE( zpass );

	_gles2_log_incorrect_api_error("glStencilOpSeparate");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform1f_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniform1f");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform1fv_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniform1fv");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform1i_stub( struct gles_context *ctx, GLint location, GLint val )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( location );
	MALI_IGNORE( val );

	_gles2_log_incorrect_api_error("glUniform1i");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform1iv_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniform1iv");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform2f_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniform2f");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform2fv_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniform2fv");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform2i_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniform2i");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform2iv_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniform2iv");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform3f_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniform3f");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform3fv_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniform3fv");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform3i_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniform3i");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform3iv_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniform3iv");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform4f_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniform4f");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform4fv_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniform4fv");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform4i_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniform4i");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform4iv_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniform4iv");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform_matrix2fv_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniformMatrix2fv");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform_matrix3fv_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniformMatrix3fv");

	return GL_NO_ERROR;
}

GLenum _gles2_uniform_matrix4fv_stub(
	struct gles_context *ctx,
	gles_datatype input_type,
	GLuint input_width,
	GLuint input_height,
	GLint count,
	GLint location,
	const void* value )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( input_type );
	MALI_IGNORE( input_width );
	MALI_IGNORE( input_height );
	MALI_IGNORE( count );
	MALI_IGNORE( location );
	MALI_IGNORE( value );

	_gles2_log_incorrect_api_error("glUniformMatrix4fv");

	return GL_NO_ERROR;
}

GLenum _gles2_use_program_stub( struct gles_state *state, mali_named_list *program_object_list, GLuint program )
{
	MALI_IGNORE( state );
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );

	_gles2_log_incorrect_api_error("glUseProgram");

	return GL_NO_ERROR;
}

GLenum _gles2_validate_program_stub( mali_named_list *program_object_list, GLuint program )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );

	_gles2_log_incorrect_api_error("glValidateProgram");

	return GL_NO_ERROR;
}

GLenum _gles2_vertex_attrib1f_stub( struct gles_vertex_array *vertex_array, GLuint index, GLuint num_values, const float* values )
{
	MALI_IGNORE( vertex_array );
	MALI_IGNORE( index );
	MALI_IGNORE( num_values );
	MALI_IGNORE( values );

	_gles2_log_incorrect_api_error("glVertexAttrib1f");

	return GL_NO_ERROR;
}

GLenum _gles2_vertex_attrib1fv_stub( struct gles_vertex_array *vertex_array, GLuint index, GLuint num_values, const float* values )
{
	MALI_IGNORE( vertex_array );
	MALI_IGNORE( index );
	MALI_IGNORE( num_values );
	MALI_IGNORE( values );

	_gles2_log_incorrect_api_error("glVertexAttrib1fv");

	return GL_NO_ERROR;
}

GLenum _gles2_vertex_attrib2f_stub( struct gles_vertex_array *vertex_array, GLuint index, GLuint num_values, const float* values )
{
	MALI_IGNORE( vertex_array );
	MALI_IGNORE( index );
	MALI_IGNORE( num_values );
	MALI_IGNORE( values );

	_gles2_log_incorrect_api_error("glVertexAttrib2f");

	return GL_NO_ERROR;
}

GLenum _gles2_vertex_attrib2fv_stub( struct gles_vertex_array *vertex_array, GLuint index, GLuint num_values, const float* values )
{
	MALI_IGNORE( vertex_array );
	MALI_IGNORE( index );
	MALI_IGNORE( num_values );
	MALI_IGNORE( values );

	_gles2_log_incorrect_api_error("glVertexAttrib2fv");

	return GL_NO_ERROR;
}

GLenum _gles2_vertex_attrib3f_stub( struct gles_vertex_array *vertex_array, GLuint index, GLuint num_values, const float* values )
{
	MALI_IGNORE( vertex_array );
	MALI_IGNORE( index );
	MALI_IGNORE( num_values );
	MALI_IGNORE( values );

	_gles2_log_incorrect_api_error("glVertexAttrib3f");

	return GL_NO_ERROR;
}

GLenum _gles2_vertex_attrib3fv_stub( struct gles_vertex_array *vertex_array, GLuint index, GLuint num_values, const float* values )
{
	MALI_IGNORE( vertex_array );
	MALI_IGNORE( index );
	MALI_IGNORE( num_values );
	MALI_IGNORE( values );

	_gles2_log_incorrect_api_error("glVertexAttrib3fv");

	return GL_NO_ERROR;
}

GLenum _gles2_vertex_attrib4f_stub( struct gles_vertex_array *vertex_array, GLuint index, GLuint num_values, const float* values )
{
	MALI_IGNORE( vertex_array );
	MALI_IGNORE( index );
	MALI_IGNORE( num_values );
	MALI_IGNORE( values );

	_gles2_log_incorrect_api_error("glVertexAttrib4f");

	return GL_NO_ERROR;
}

GLenum _gles2_vertex_attrib4fv_stub( struct gles_vertex_array *vertex_array, GLuint index, GLuint num_values, const float* values )
{

	MALI_IGNORE( vertex_array );
	MALI_IGNORE( index );
	MALI_IGNORE( num_values );
	MALI_IGNORE( values );

	_gles2_log_incorrect_api_error("glVertexAttrib4fv");

	return GL_NO_ERROR;
}

GLenum _gles2_vertex_attrib_pointer_stub(
	struct gles_context *ctx,
	GLuint index,
	GLint size,
	GLenum type,
	GLboolean normalized,
	GLsizei stride,
	const GLvoid *pointer )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( index );
	MALI_IGNORE( size );
	MALI_IGNORE( type );
	MALI_IGNORE( normalized );
	MALI_IGNORE( stride );
	MALI_IGNORE( pointer );

	_gles2_log_incorrect_api_error("glVertexAttribPointer");

	return GL_NO_ERROR;
}

#if EXTENSION_GET_PROGRAM_BINARY_OES_ENABLE
GLenum _gles2_get_program_binary_stub( 
	mali_named_list *program_object_list, 
	GLuint program, 
	GLsizei bufSize, 
	GLsizei *length, 
	GLenum *binaryFormat, 
	GLvoid *binary )
{
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );
	MALI_IGNORE( bufSize );
	MALI_IGNORE( length );
	MALI_IGNORE( binaryFormat );
	MALI_IGNORE( binary );

	_gles2_log_incorrect_api_error("glGetProgramBinaryOES");

	return GL_NO_ERROR;
}

GLenum _gles2_program_binary_stub( 
	struct gles_context *ctx, 
	mali_named_list *program_object_list, 
	GLuint program, 
	GLenum binaryFormat, 
	const GLvoid *binary, 
	GLint length )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );
	MALI_IGNORE( binaryFormat );
	MALI_IGNORE( binary );
	MALI_IGNORE( length );

	_gles2_log_incorrect_api_error("glGetProgramBinaryOES");

	return GL_NO_ERROR;
}
#endif

#if EXTENSION_ROBUSTNESS_EXT_ENABLE
GLenum _gles2_get_n_uniformfv_ext_stub( struct gles_context *ctx, mali_named_list* program_object_list, GLuint program, GLint location, GLsizei bufSize, void *params, GLenum out_type)
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );
	MALI_IGNORE( location );
	MALI_IGNORE( bufSize );
	MALI_IGNORE( params );
	MALI_IGNORE( out_type );

	_gles2_log_incorrect_api_error("glGetnUniformfvEXT");

	return GL_NO_ERROR;
}

GLenum _gles2_get_n_uniformiv_ext_stub( struct gles_context *ctx, mali_named_list* program_object_list, GLuint program, GLint location, GLsizei bufSize, void *params, GLenum out_type)
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( program_object_list );
	MALI_IGNORE( program );
	MALI_IGNORE( location );
	MALI_IGNORE( bufSize );
	MALI_IGNORE( params );
	MALI_IGNORE( out_type );

	_gles2_log_incorrect_api_error("glGetnUniformivEXT");

	return GL_NO_ERROR;
}
#endif

#endif /* _GLES2_STUBS_H_ */
