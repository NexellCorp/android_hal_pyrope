/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file gles1_stubs.h
 * @brief Contains stub versions of the second layer functions called by all the
 *	OpenGL ES 1.1 API/entrypoint functions that are not present in OpenGL ES 2.0.
 *
 * @note These functions are used to populate the OpenGL ES 1.x function pointers in
 *       the vtable when it is used by an OpenGL ES 2.x context and will silently fail
 *       with an instrumented error.
 */

#ifndef _GLES1_STUBS_H_
#define _GLES1_STUBS_H_

#include "gles_base.h"
#include "gles_ftype.h"
#include <mali_system.h>

#include "mali_instrumented_context.h"
#include "mali_log.h"

struct gles1_alpha_test;
struct gles_blend;
struct gles_framebuffer_control;
struct gles1_transform;
struct gles_current;
struct gles_vertex_array;
struct gles1_transform;
struct gles_viewport;


MALI_STATIC void _gles1_log_incorrect_api_error(const char *function)
{
	MALI_IGNORE(function);
	MALI_INSTRUMENTED_LOG((_instrumented_get_context(), MALI_LOG_LEVEL_ERROR, "Calling %s while an OpenGL ES 1.x context is set as current", function));
}

GLenum _gles1_alpha_func_stub( struct gles_context *ctx, GLenum func, GLftype ref)
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( func );
	MALI_IGNORE( ref );

	_gles1_log_incorrect_api_error("glAlphaFunc");

	return GL_NO_ERROR;
}

GLenum _gles1_alpha_funcx_stub( struct gles_context *ctx, GLenum func, GLftype ref )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( func );
	MALI_IGNORE( ref );

	_gles1_log_incorrect_api_error("glAlphaFuncx");

	return GL_NO_ERROR;
}


void _gles1_clear_colorx_stub( struct gles_framebuffer_control *fb_control, GLftype red, GLftype green, GLftype blue, GLftype alpha )
{
	MALI_IGNORE( fb_control );
	MALI_IGNORE( red );
	MALI_IGNORE( green );
	MALI_IGNORE( blue );
	MALI_IGNORE( alpha );

	_gles1_log_incorrect_api_error("glClearColorx");

	return;
}

void _gles1_clear_depthx_stub( struct gles_framebuffer_control *fb_control, GLftype depth )
{
	MALI_IGNORE( fb_control );
	MALI_IGNORE( depth );

	_gles1_log_incorrect_api_error("glClearDepthx");

	return;
}

GLenum _gles1_client_active_texture_stub( struct gles_vertex_array *vertex_array, GLenum texture )
{
	MALI_IGNORE( vertex_array );
	MALI_IGNORE( texture );

	_gles1_log_incorrect_api_error("glClientActiveTexture");

	return GL_NO_ERROR;
}

GLenum _gles1_clip_planef_stub( struct gles_context *ctx, GLenum plane, const GLvoid *equation, gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( plane );
	MALI_IGNORE( equation );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glClipPlanef");

	return GL_NO_ERROR;
}

GLenum _gles1_clip_planex_stub( struct gles_context *ctx, GLenum plane, const GLvoid *equation, gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( plane );
	MALI_IGNORE( equation );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glClipPlanex");

	return GL_NO_ERROR;
}

GLenum _gles1_color4f_stub( struct gles_context *ctx, GLftype red, GLftype green, GLftype blue, GLftype alpha )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( red );
	MALI_IGNORE( green );
	MALI_IGNORE( blue );
	MALI_IGNORE( alpha );

	_gles1_log_incorrect_api_error("glColor4f");

	return GL_NO_ERROR;
}

GLenum _gles1_color4ub_stub( struct gles_context *ctx, GLftype red, GLftype green, GLftype blue, GLftype alpha )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( red );
	MALI_IGNORE( green );
	MALI_IGNORE( blue );
	MALI_IGNORE( alpha );

	_gles1_log_incorrect_api_error("glColor4ub");

	return GL_NO_ERROR;
}

GLenum _gles1_color4x_stub( struct gles_context *ctx, GLftype red, GLftype green, GLftype blue, GLftype alpha )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( red );
	MALI_IGNORE( green );
	MALI_IGNORE( blue );
	MALI_IGNORE( alpha );

	_gles1_log_incorrect_api_error("glColor4x");

	return GL_NO_ERROR;
}

GLenum _gles1_color_pointer_stub( struct gles_context *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( size );
	MALI_IGNORE( type );
	MALI_IGNORE( stride );
	MALI_IGNORE( pointer );

	_gles1_log_incorrect_api_error("glColorPointer");

	return GL_NO_ERROR;
}

GLenum _gles1_current_palette_matrix_oes_stub( struct gles1_transform *transform, GLint palette_index )
{
	MALI_IGNORE( transform );
	MALI_IGNORE( palette_index );

	_gles1_log_incorrect_api_error("glCurrentPaletteMatrixOES");

	return GL_NO_ERROR;
}

void _gles1_depth_rangex_stub( struct gles_context *ctx, GLftype z_near, GLftype z_far )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( z_near );
	MALI_IGNORE( z_far );

	_gles1_log_incorrect_api_error("glDepthRangex");
}

GLenum _gles1_disable_client_state_stub( struct gles_context *ctx, GLenum cap, GLboolean state )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( cap );
	MALI_IGNORE( state );

	_gles1_log_incorrect_api_error("glDisableClientState");

	return GL_NO_ERROR;
}

GLenum _gles1_draw_tex_oes_stub( gles_context * ctx, GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( x );
	MALI_IGNORE( y );
	MALI_IGNORE( z );
	MALI_IGNORE( width );
	MALI_IGNORE( height );

	GL_SUCCESS;
}
GLenum _gles1_enable_client_state_stub( struct gles_context *ctx, GLenum cap, GLboolean state )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( cap );
	MALI_IGNORE( state );

	_gles1_log_incorrect_api_error("glEnableClientState");

	return GL_NO_ERROR;
}

GLenum _gles1_fogf_stub( struct gles_context *ctx, GLenum pname, const GLvoid *param, gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( pname );
	MALI_IGNORE( param );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glFogf");

	return GL_NO_ERROR;
}

GLenum _gles1_fogfv_stub( struct gles_context *ctx, GLenum pname, const GLvoid *params, gles_datatype type )
{
	MALI_IGNORE( ctx);
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glFogfv");

	return GL_NO_ERROR;
}

GLenum _gles1_fogx_stub( struct gles_context *ctx, GLenum pname, const GLvoid *param, gles_datatype type )
{
	MALI_IGNORE( ctx);
	MALI_IGNORE( pname );
	MALI_IGNORE( param );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glFogx");

	return GL_NO_ERROR;
}

GLenum _gles1_fogxv_stub( struct gles_context *ctx, GLenum pname, const GLvoid *params, gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glFogxv");

	return GL_NO_ERROR;
}

GLenum _gles1_frustumf_stub( struct gles_context *ctx, GLftype l, GLftype r, GLftype b, GLftype t, GLftype n, GLftype f )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( l );
	MALI_IGNORE( r );
	MALI_IGNORE( b );
	MALI_IGNORE( t );
	MALI_IGNORE( n );
	MALI_IGNORE( f );

	_gles1_log_incorrect_api_error("glFrustumf");

	return GL_NO_ERROR;
}

GLenum _gles1_frustumx_stub( struct gles_context *ctx, GLftype l, GLftype r, GLftype b, GLftype t, GLftype n, GLftype f )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( l );
	MALI_IGNORE( r );
	MALI_IGNORE( b );
	MALI_IGNORE( t );
	MALI_IGNORE( n );
	MALI_IGNORE( f );

	_gles1_log_incorrect_api_error("glFrustumx");

	return GL_NO_ERROR;
}

GLenum _gles1_get_clip_planef_stub( struct gles_state *state, GLenum plane, GLvoid *equation, gles_datatype type )
{
	MALI_IGNORE( state );
	MALI_IGNORE( plane );
	MALI_IGNORE( equation );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glGetClipPlanef");

	return GL_NO_ERROR;
}

GLenum _gles1_get_clip_planex_stub( struct gles_state *state, GLenum plane, GLvoid *equation, gles_datatype type )
{
	MALI_IGNORE( state );
	MALI_IGNORE( plane );
	MALI_IGNORE( equation );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glGetClipPlanex");

	return GL_NO_ERROR;
}

GLenum _gles1_get_fixedv_stub( struct gles_context *ctx, GLenum pname, GLvoid *params, gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glGetFixedv");

	return GL_NO_ERROR;
}

GLenum _gles1_get_lightfv_stub( struct gles_state *state, GLenum light, GLenum pname, GLvoid *params, gles_datatype type )
{
	MALI_IGNORE( state );
	MALI_IGNORE( light );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glGetLightfv");

	return GL_NO_ERROR;
}

GLenum _gles1_get_lightxv_stub( struct gles_state *state, GLenum light, GLenum pname, GLvoid *params, gles_datatype type )
{
	MALI_IGNORE( state );
	MALI_IGNORE( light );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glGetLightxv");

	return GL_NO_ERROR;
}

GLenum _gles1_get_materialfv_stub( struct gles_state *state, GLenum face, GLenum pname, GLvoid *params, gles_datatype type )
{
	MALI_IGNORE( state );
	MALI_IGNORE( face );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glGetMaterialfv");

	return GL_NO_ERROR;
}

GLenum _gles1_get_materialxv_stub( struct gles_state *state, GLenum face, GLenum pname, GLvoid *params, gles_datatype type )
{
	MALI_IGNORE( state );
	MALI_IGNORE( face );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glGetMaterialxv");

	return GL_NO_ERROR;
}

GLenum _gles1_get_pointerv_stub( struct gles_state *state, GLenum pname, GLvoid **pointer )
{
	MALI_IGNORE( state );
	MALI_IGNORE( pname );
	MALI_IGNORE( pointer );

	_gles1_log_incorrect_api_error("glGetPointerv");

	return GL_NO_ERROR;
}

GLenum _gles1_get_tex_envfv_stub( struct gles_state *state, GLenum target, GLenum pname, GLvoid *params, gles_datatype type )
{
	MALI_IGNORE( state );
	MALI_IGNORE( target );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glGetTexEnvfv");

	return GL_NO_ERROR;
}

GLenum _gles1_get_tex_enviv_stub( struct gles_state *state, GLenum target, GLenum pname, GLvoid *params, gles_datatype type )
{
	MALI_IGNORE( state );
	MALI_IGNORE( target );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glGetTexEnviv");

	return GL_NO_ERROR;
}

GLenum _gles1_get_tex_envxv_stub( struct gles_state *state, GLenum target, GLenum pname, GLvoid *params, gles_datatype type )
{
	MALI_IGNORE( state );
	MALI_IGNORE( target );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glGetTexEnvxv");

	return GL_NO_ERROR;
}

GLenum _gles1_get_tex_parameterxv_stub( struct gles_common_state *state, GLenum target, GLenum pname, GLvoid *params, gles_datatype type )
{
	MALI_IGNORE( state );
	MALI_IGNORE( target );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glGetTexParameterxv");

	return GL_NO_ERROR;
}

GLenum _gles1_lightf_stub( struct gles_context *ctx, GLenum light, GLenum pname, const GLvoid *param, const gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( light );
	MALI_IGNORE( pname );
	MALI_IGNORE( param );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glLightf");

	return GL_NO_ERROR;
}

GLenum _gles1_lightfv_stub(
	struct gles_context *ctx,
	GLenum light,
	const GLenum pname,
	const GLvoid *params,
	const gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( light );
	MALI_IGNORE( params );
	MALI_IGNORE( pname );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glLightfv");

	return GL_NO_ERROR;
}

GLenum _gles1_light_modelf_stub( struct gles_context *ctx, GLenum pname, const GLvoid *param, gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( pname );
	MALI_IGNORE( param );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glLightModelf");

	return GL_NO_ERROR;
}

GLenum _gles1_light_modelfv_stub( struct gles_context *ctx, GLenum pname, const GLvoid *params, gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glLightModelfv");

	return GL_NO_ERROR;
}

GLenum _gles1_light_modelx_stub( struct gles_context *ctx, GLenum pname, const GLvoid *param, gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( pname );
	MALI_IGNORE( param );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glLightModelx");

	return GL_NO_ERROR;
}

GLenum _gles1_light_modelxv_stub( struct gles_context *ctx, GLenum pname, const GLvoid *params, gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glLightModelxv");

	return GL_NO_ERROR;
}

GLenum _gles1_lightx_stub( struct gles_context *ctx, GLenum light, GLenum pname, const GLvoid *param, const gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( light );
	MALI_IGNORE( pname );
	MALI_IGNORE( param );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glLightx");

	return GL_NO_ERROR;
}

GLenum _gles1_lightxv_stub( struct gles_context *ctx, GLenum light, const GLenum pname, const GLvoid *params, const gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( light );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glLightxv");

	return GL_NO_ERROR;
}

GLenum _gles1_line_widthx_stub( struct gles_context *ctx, GLftype width )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( width );

	_gles1_log_incorrect_api_error("glLineWidthx");

	return GL_NO_ERROR;
}

void _gles1_load_identity_stub( struct gles_context *ctx )
{
	MALI_IGNORE( ctx );

	_gles1_log_incorrect_api_error("glLoadIdentity");

	return;
}

void _gles1_load_matrixf_stub( struct gles_context *ctx, const GLfloat *m )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( m );

	_gles1_log_incorrect_api_error("glLoadMatrixf");

	return;
}

void _gles1_load_matrixx_stub( struct gles_context *ctx, const GLfixed *m )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( m );

	_gles1_log_incorrect_api_error("glLoadMatrixx");

	return;
}

void _gles1_load_palette_from_model_view_matrix_oes_stub( struct gles_context *ctx, struct gles1_transform *transform )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( transform );

	_gles1_log_incorrect_api_error("glLoadPaletteFromModelViewMatrixOES");

	return;
}

GLenum _gles1_logic_op_stub( struct gles_context *ctx, GLenum opcode )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( opcode );

	_gles1_log_incorrect_api_error("glLogicOp");

	return GL_NO_ERROR;
}

GLenum _gles1_materialf_stub( struct gles_context *ctx, GLenum face, GLenum pname, const GLvoid *param, const gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( face );
	MALI_IGNORE( pname );
	MALI_IGNORE( param );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glMaterialf");

	return GL_NO_ERROR;
}

GLenum _gles1_materialfv_stub( struct gles_context *ctx, GLenum face, GLenum pname, const GLvoid *params, const gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( face );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glMaterialfv");

	return GL_NO_ERROR;
}

GLenum _gles1_materialx_stub( struct gles_context *ctx, GLenum face, GLenum pname, const GLvoid *param, const gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( face );
	MALI_IGNORE( pname );
	MALI_IGNORE( param );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glMaterialx");

	return GL_NO_ERROR;
}

GLenum _gles1_materialxv_stub( struct gles_context *ctx, GLenum face, GLenum pname, const GLvoid *params, const gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( face );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glMaterialxv");

	return GL_NO_ERROR;
}

GLenum _gles1_matrix_index_pointer_oes_stub( struct gles_context *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( size );
	MALI_IGNORE( type );
	MALI_IGNORE( stride );
	MALI_IGNORE( pointer );

	_gles1_log_incorrect_api_error("glMatrixIndexPointerOES");

	return GL_NO_ERROR;
}

GLenum _gles1_matrix_mode_stub( struct gles_state *state, GLenum mode )
{
	MALI_IGNORE( state );
	MALI_IGNORE( mode );

	_gles1_log_incorrect_api_error("glMatrixMode");

	return GL_NO_ERROR;
}

GLenum _gles1_multi_tex_coord4b_stub( struct gles1_current *current, GLenum target, GLftype s, GLftype t, GLftype r, GLftype q )
{
	MALI_IGNORE( current );
	MALI_IGNORE( target );
	MALI_IGNORE( s );
	MALI_IGNORE( t );
	MALI_IGNORE( r );
	MALI_IGNORE( q );

	_gles1_log_incorrect_api_error("glMultiTexCoord4b");

	return GL_NO_ERROR;
}

GLenum _gles1_multi_tex_coord4f_stub( struct gles1_current *current, GLenum target, GLftype s, GLftype t, GLftype r, GLftype q )
{
	MALI_IGNORE( current );
	MALI_IGNORE( target );
	MALI_IGNORE( s );
	MALI_IGNORE( t );
	MALI_IGNORE( r );
	MALI_IGNORE( q );

	_gles1_log_incorrect_api_error("glMultiTexCoord4f");

	return GL_NO_ERROR;
}

GLenum _gles1_multi_tex_coord4x_stub( struct gles1_current *current, GLenum target, GLftype s, GLftype t, GLftype r, GLftype q )
{
	MALI_IGNORE( current );
	MALI_IGNORE( target );
	MALI_IGNORE( s );
	MALI_IGNORE( t );
	MALI_IGNORE( r );
	MALI_IGNORE( q );

	_gles1_log_incorrect_api_error("glMultiTexCoord4x");

	return GL_NO_ERROR;
}

void _gles1_mult_matrixf_stub( struct gles_context *ctx, const GLfloat *m )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( m );

	_gles1_log_incorrect_api_error("glMultMatrixf");

	return;
}

void _gles1_mult_matrixx_stub( struct gles_context *ctx, const GLfixed *m )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( m );

	_gles1_log_incorrect_api_error("glMultMatrixx");

	return;
}

GLenum _gles1_normal3f_stub( struct gles1_current *current, GLftype nx, GLftype ny, GLftype nz )
{
	MALI_IGNORE( current );
	MALI_IGNORE( nx );
	MALI_IGNORE( ny );
	MALI_IGNORE( nz );

	_gles1_log_incorrect_api_error("glNormal3f");

	return GL_NO_ERROR;
}

GLenum _gles1_normal3x_stub( struct gles1_current *current, GLftype nx, GLftype ny, GLftype nz )
{
	MALI_IGNORE( current );
	MALI_IGNORE( nx );
	MALI_IGNORE( ny );
	MALI_IGNORE( nz );

	_gles1_log_incorrect_api_error("glNormal3x");

	return GL_NO_ERROR;
}

GLenum _gles1_normal_pointer_stub( struct gles_context *ctx, GLenum type, GLsizei stride, const GLvoid *pointer )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( type );
	MALI_IGNORE( stride );
	MALI_IGNORE( pointer );

	_gles1_log_incorrect_api_error("glNormalPointer");

	return GL_NO_ERROR;
}

GLenum _gles1_orthof_stub( struct gles_context *ctx, GLftype l, GLftype r, GLftype b, GLftype t, GLftype n, GLftype f )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( l );
	MALI_IGNORE( r );
	MALI_IGNORE( b );
	MALI_IGNORE( t );
	MALI_IGNORE( n );
	MALI_IGNORE( f );

	_gles1_log_incorrect_api_error("glOrthof");

	return GL_NO_ERROR;
}

GLenum _gles1_orthox_stub( struct gles_context *ctx, GLftype l, GLftype r, GLftype b, GLftype t, GLftype n, GLftype f )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( l );
	MALI_IGNORE( r );
	MALI_IGNORE( b );
	MALI_IGNORE( t );
	MALI_IGNORE( n );
	MALI_IGNORE( f );

	_gles1_log_incorrect_api_error("glOrthox");

	return GL_NO_ERROR;
}

GLenum _gles1_point_parameterf_stub( struct gles_context *ctx, GLenum pname, const GLvoid *param, gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( pname );
	MALI_IGNORE( param );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glPointParameterf");

	return GL_NO_ERROR;
}

GLenum _gles1_point_parameterfv_stub( struct gles_context *ctx, GLenum pname, const GLvoid *params, gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glPointParameterfv");

	return GL_NO_ERROR;
}

GLenum _gles1_point_parameterx_stub( struct gles_context *ctx, GLenum pname, const GLvoid *param, gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( pname );
	MALI_IGNORE( param );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glPointParameterx");

	return GL_NO_ERROR;
}

GLenum _gles1_point_parameterxv_stub( struct gles_context *ctx, GLenum pname, const GLvoid *params, gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glPointParameterxv");

	return GL_NO_ERROR;
}

GLenum _gles1_point_size_stub( struct gles_rasterization *raster, GLftype size )
{
	MALI_IGNORE( raster );
	MALI_IGNORE( size );

	_gles1_log_incorrect_api_error("glPointSize");

	return GL_NO_ERROR;
}

GLenum _gles1_point_size_pointer_oes_stub( struct gles_context *ctx, GLenum type, GLsizei stride, const GLvoid *pointer )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( type );
	MALI_IGNORE( stride );
	MALI_IGNORE( pointer );

	_gles1_log_incorrect_api_error("glPointSizePointerOES");

	return GL_NO_ERROR;
}

GLenum _gles1_point_sizex_stub( struct gles_rasterization *raster, GLftype size )
{
	MALI_IGNORE( raster );
	MALI_IGNORE( size );

	_gles1_log_incorrect_api_error("glPointSizex");

	return GL_NO_ERROR;
}

GLenum _gles1_polygon_offsetx_stub( struct gles_context *ctx, GLftype factor, GLftype units )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( factor );
	MALI_IGNORE( units );

	_gles1_log_incorrect_api_error("glPolygonOffsetx");

	return GL_NO_ERROR;
}

GLenum _gles1_pop_matrix_stub( struct gles_context *ctx )
{
	MALI_IGNORE( ctx );

	_gles1_log_incorrect_api_error("glPopMatrix");

	return GL_NO_ERROR;
}

GLenum _gles1_push_matrix_stub( struct gles_context *ctx )
{
	MALI_IGNORE( ctx );

	_gles1_log_incorrect_api_error("glPushMatrix");

	return GL_NO_ERROR;
}

GLbitfield _gles1_query_matrixx_oes_stub( struct gles_context *ctx, GLfixed *mantissa, GLint *exponent )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( mantissa );
	MALI_IGNORE( exponent );

	_gles1_log_incorrect_api_error("glQueryMatrixxOES");

	return 0;
}

void _gles1_rotatef_stub( struct gles_context *ctx, GLftype angle, GLftype x, GLftype y, GLftype z )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( angle );
	MALI_IGNORE( x );
	MALI_IGNORE( y );
	MALI_IGNORE( z );

	_gles1_log_incorrect_api_error("glRotatef");

	return;
}

void _gles1_rotatex_stub( struct gles_context *ctx, GLftype angle, GLftype x, GLftype y, GLftype z )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( angle );
	MALI_IGNORE( x );
	MALI_IGNORE( y );
	MALI_IGNORE( z );

	_gles1_log_incorrect_api_error("glRotatex");

	return;
}

void _gles1_sample_coveragex_stub( struct gles_context *ctx, GLftype value, GLboolean invert )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( value );
	MALI_IGNORE( invert );

	_gles1_log_incorrect_api_error("glSampleCoveragex");

	return;
}

void _gles1_scalef_stub( struct gles_context *ctx, GLftype x, GLftype y, GLftype z )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( x );
	MALI_IGNORE( y );
	MALI_IGNORE( z );

	_gles1_log_incorrect_api_error("glScalef");

	return;
}

void _gles1_scalex_stub( struct gles_context *ctx, GLftype x, GLftype y, GLftype z )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( x );
	MALI_IGNORE( y );
	MALI_IGNORE( z );

	_gles1_log_incorrect_api_error("glScalex");

	return;
}

GLenum _gles1_shade_model_stub( struct gles_context *ctx, GLenum mode )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( mode );

	_gles1_log_incorrect_api_error("glShadeModel");

	return GL_NO_ERROR;
}

GLenum _gles1_tex_coord_pointer_stub( struct gles_context *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( size );
	MALI_IGNORE( type );
	MALI_IGNORE( stride );
	MALI_IGNORE( pointer );

	_gles1_log_incorrect_api_error("glTexCoordPointer");


	return GL_NO_ERROR;
}

GLenum _gles1_tex_envf_stub(
	struct gles_context *ctx,
	GLenum target,
	GLenum pname,
	const GLvoid *param,
	gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( target );
	MALI_IGNORE( pname );
	MALI_IGNORE( param );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glTexEnvf");

	return GL_NO_ERROR;
}

GLenum _gles1_tex_envfv_stub(
	struct gles_context *ctx,
	GLenum target,
	GLenum pname,
	const GLvoid *params,
	gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( target );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glTexEnvfv");

	return GL_NO_ERROR;
}

GLenum _gles1_tex_envi_stub(
	struct gles_context *ctx,
	GLenum target,
	GLenum pname,
	const GLvoid *param,
	gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( target );
	MALI_IGNORE( pname );
	MALI_IGNORE( param );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glTexEnvi");

	return GL_NO_ERROR;
}

GLenum _gles1_tex_enviv_stub(
	struct gles_context *ctx,
	GLenum target,
	GLenum pname,
	const GLvoid *params,
	gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( target );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glTexEnviv");

	return GL_NO_ERROR;
}

GLenum _gles1_tex_envx_stub(
	struct gles_context *ctx,
	GLenum target,
	GLenum pname,
	const GLvoid *param,
	gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( target );
	MALI_IGNORE( pname );
	MALI_IGNORE( param );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glTexEnvx");

	return GL_NO_ERROR;
}

GLenum _gles1_tex_envxv_stub(
	struct gles_context *ctx,
	GLenum target,
	GLenum pname,
	const GLvoid *param,
	gles_datatype type )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( target );
	MALI_IGNORE( pname );
	MALI_IGNORE( param );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glTexEnvxv");

	return GL_NO_ERROR;
}

GLenum _gles1_tex_parameterx_stub( struct gles_texture_environment *texture_env, GLenum target, GLenum pname, const GLvoid *params, gles_datatype type )
{
	MALI_IGNORE( texture_env );
	MALI_IGNORE( target );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glTexParameterx");

	return GL_NO_ERROR;
}

GLenum _gles1_tex_parameterxv_stub( struct gles_texture_environment *texture_env, GLenum target, GLenum pname, const GLvoid *params, gles_datatype type )
{
	MALI_IGNORE( texture_env );
	MALI_IGNORE( target );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );
	MALI_IGNORE( type );

	_gles1_log_incorrect_api_error("glTexParameterxv");

	return GL_NO_ERROR;
}

void _gles1_translatef_stub( struct gles_context *ctx, GLftype x, GLftype y, GLftype z )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( x );
	MALI_IGNORE( y );
	MALI_IGNORE( z );

	_gles1_log_incorrect_api_error("glTranslatef");

	return;
}

void _gles1_translatex_stub( struct gles_context *ctx, GLftype x, GLftype y, GLftype z )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( x );
	MALI_IGNORE( y );
	MALI_IGNORE( z );

	_gles1_log_incorrect_api_error("glTranslatex");

	return;
}

GLenum _gles1_vertex_pointer_stub( struct gles_context *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( size );
	MALI_IGNORE( type );
	MALI_IGNORE( stride );
	MALI_IGNORE( pointer );

	_gles1_log_incorrect_api_error("glVertexPointer");

	return GL_NO_ERROR;
}

GLenum _gles1_weight_pointer_oes_stub( struct gles_context *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( size );
	MALI_IGNORE( type );
	MALI_IGNORE( stride );
	MALI_IGNORE( pointer );

	_gles1_log_incorrect_api_error("glWeightPointerOES");

	return GL_NO_ERROR;
}

GLenum _gles1_tex_genf_oes_stub( struct gles_context *ctx, GLenum coord, GLenum pname, GLfloat param )
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( coord );
	MALI_IGNORE( pname );
	MALI_IGNORE( param );

	_gles1_log_incorrect_api_error("glTexGenfOES");

	return GL_NO_ERROR;
}

GLenum _gles1_tex_genfv_oes_stub( struct gles_context *ctx, GLenum coord, GLenum pname, const GLfloat *params)
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( coord );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );

	_gles1_log_incorrect_api_error("glTexGenfvOES");

	return GL_NO_ERROR;
}

GLenum _gles1_tex_geni_oes_stub( struct gles_context *ctx, GLenum coord, GLenum pname, GLint param)
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( coord );
	MALI_IGNORE( pname );
	MALI_IGNORE( param );

	_gles1_log_incorrect_api_error("glTexGeniOES");

	return GL_NO_ERROR;
}

GLenum _gles1_tex_geniv_oes_stub( struct gles_context *ctx, GLenum coord, GLenum pname, const GLint *params)
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( coord );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );

	_gles1_log_incorrect_api_error("glTexGenivOES");

	return GL_NO_ERROR;
}

GLenum _gles1_tex_genx_oes_stub( struct gles_context *ctx, GLenum coord, GLenum pname, GLfixed param)
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( coord );
	MALI_IGNORE( pname );
	MALI_IGNORE( param );

	_gles1_log_incorrect_api_error("glTexGenxOES");

	return GL_NO_ERROR;
}

GLenum _gles1_tex_genxv_oes_stub( struct gles_context *ctx, GLenum coord, GLenum pname, const GLfixed *params)
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( coord );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );

	_gles1_log_incorrect_api_error("glTexGenxvOES");

	return GL_NO_ERROR;
}

GLenum _gles1_get_tex_genfv_oes_stub( struct gles_context *ctx, GLenum coord, GLenum pname, GLfloat *params)
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( coord );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );

	_gles1_log_incorrect_api_error("glGetTexGenfvOES");

	return GL_NO_ERROR;
}

GLenum _gles1_get_tex_geniv_oes_stub( struct gles_context *ctx, GLenum coord, GLenum pname, GLint *params)
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( coord );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );

	_gles1_log_incorrect_api_error("glGetTexGenivOES");

	return GL_NO_ERROR;
}

GLenum _gles1_get_tex_genxv_oes_stub( struct gles_context *ctx, GLenum coord, GLenum pname, GLfixed *params)
{
	MALI_IGNORE( ctx );
	MALI_IGNORE( coord );
	MALI_IGNORE( pname );
	MALI_IGNORE( params );

	_gles1_log_incorrect_api_error("glGetTexGenxvOES");

	return GL_NO_ERROR;
}

#endif /* _GLES1_STUBS_H_ */
