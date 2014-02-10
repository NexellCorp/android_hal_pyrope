/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "gles_base.h"
#include "gles2_program_object.h"
#include "../gles_common_state/gles_program_rendering_state.h"

#include "gles2_shader_object.h"
#include "../gles_util.h"
#include "../gles_context.h"
#include "../gles_convert.h"
#include "gles2_state.h"
#include "../gles_gb_interface.h"
#include "../gles_fb_interface.h"

#include <base/mali_context.h>
#include <shared/binary_shader/bs_symbol.h>
#include <shared/binary_shader/bs_error.h>
#include <shared/binary_shader/bs_loader.h>
#include <shared/binary_shader/bs_loader_internal.h>

/**
 * Global const array holding all the glGetActive* filters. These filters decide which
 * symbols should NOT be returned through the gl API. The spec require all symbols
 * starting with "gl_" to be filtered, the compiler spec also require all symbols
 * starting with "?" to be filtered away. More filters may be added here
 */
const char* _gles_active_filters[GLES_ACTIVE_FILTER_SIZE] = { "gl_", "?" };


/* Returns true if the shader is attached to a program object */
MALI_STATIC mali_bool _gles2_is_shader_attached(gles2_program_object * program_obj, GLuint shader )
{
	mali_bool found = MALI_FALSE;
	mali_linked_list_entry* it;
	it = __mali_linked_list_get_first_entry(&program_obj->shaders);
	while(it != NULL)
	{
		if((u32)it->data == shader)
		{
			found = MALI_TRUE;
			break;
		}
		it = __mali_linked_list_get_next_entry(it);
	}

	return found;
}

gles2_program_object*_gles2_program_internal_alloc(void)
{
	gles2_program_object* po = _mali_sys_malloc(sizeof(gles2_program_object));
	mali_err_code err = MALI_ERR_NO_ERROR;

	if(NULL == po) /* malloc failed */
	{
		return NULL;
	}

	/* initialize entire state */
	_mali_sys_memset(po, 0, sizeof(gles2_program_object));

	/* initialize bindings */
	err = __mali_linked_list_init(&po->attrib_bindings);
	if( err != MALI_ERR_NO_ERROR)
	{
		_mali_sys_free(po);
		return NULL;
	}

	err = __mali_linked_list_init(&po->shaders);
	if(err != MALI_ERR_NO_ERROR)
	{
		__mali_linked_list_deinit(&po->attrib_bindings);
		_mali_sys_free(po);
		return NULL;
	}

	/* create a dummy render state block; this will never be used for anything except
	 * act as an empty non-linked state struct. It's a bit of waste of memory,
	 * but only if the program is never linked. Which is highly unlikely.  */
	po->render_state = _gles_program_rendering_state_alloc();
	if( po->render_state == NULL )
	{
		__mali_linked_list_deinit(&po->shaders);
		__mali_linked_list_deinit(&po->attrib_bindings);
		_mali_sys_free(po);
		return NULL;
	}

	return po;
}

void _gles2_program_internal_free(gles2_program_object* po)
{
	MALI_DEBUG_ASSERT_POINTER(po);
	/* delete shader attachments */
	/* Of course, by this point, all shaders should already be detached*/
	MALI_DEBUG_ASSERT(po->attached_shaders == 0, ("Not all shaders are detached from program object"));

	_gles_program_rendering_state_deref(po->render_state);

	/* delete attrib bindings */
	_gles2_clear_attrib_bindings(po);
	__mali_linked_list_deinit(&po->attrib_bindings);

	/* deinit shaders linkedlist */
	__mali_linked_list_deinit(&po->shaders);

	_mali_sys_free(po);
}

void _gles2_program_env_init( gles2_program_environment *pe)
{

	MALI_DEBUG_ASSERT_POINTER(pe);
	pe->current_program = 0;
}

/************************************************************************************/
/*  Helper Functions                                                                */
/************************************************************************************/


/* helper function to find type of program */
void* _gles2_program_internal_get_type(mali_named_list *program_object_list, GLuint program, GLenum* type)
{
	gles2_program_object_wrapper *pw;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	/* assert that input is a valid object */
	pw = __mali_named_list_get( program_object_list, program);
	if(pw == NULL) /* not a defined name */
	{
		if(type) *type = GL_INVALID_VALUE;
		return NULL;
	}

	/* return type */
	if(type) *type = pw->type;
	return pw->content;

}


void _gles2_program_internal_unattach(mali_named_list* program_object_list, gles2_program_object* po,  gles2_shader_object* so, GLuint shader_name)
{

	mali_linked_list_entry* entry;

	MALI_DEBUG_ASSERT_POINTER(program_object_list);
	MALI_DEBUG_ASSERT_POINTER(po);
	if(shader_name == 0) return;

	MALI_DEBUG_ASSERT_POINTER(so);

	/* retrieve the entry - should always exist */
	entry = __mali_linked_list_get_first_entry(&po->shaders);
	while(entry != NULL)
	{
		if(((GLuint)entry->data) == shader_name) break;
		entry = __mali_linked_list_get_next_entry(entry);
	}

	MALI_DEBUG_ASSERT(entry, ("Attempting to deattach a shader which didn't exist. That is soo wrong. "));

	__mali_linked_list_remove_entry(&po->shaders, entry);

	po->attached_shaders--;
	_gles2_shader_object_deref( so );
	if(so->delete_status == GL_TRUE && _mali_sys_atomic_get( &so->references ) == 0)
	{
		gles2_program_object_wrapper* pow;
		_gles2_shader_internal_free(so);
		pow = __mali_named_list_remove(program_object_list, shader_name);
		MALI_DEBUG_ASSERT(pow, ("program object wrapper should exist at this point - otherwise the data struct is corrupted"));
		_mali_sys_free(pow);
	}

}

/* Internal function to check program and shader types and return the proper GL error based on their type.
 * Basically it abstracts some of the error handling required.
 * Used by attach shader and detach shader */
MALI_STATIC GLenum _gles2_internal_program_shader_valid(GLenum vp, GLenum vs)
{
	/* ensure program inputs are valid objects. */
	if(vp == GL_INVALID_VALUE) return GL_INVALID_VALUE;

	/* ensure that program input names are of their required type */
	if(vp != GLES_PROGRAM ) return GL_INVALID_OPERATION;

	/* ensure shader inputs are valid objects. */
	if(vs == GL_INVALID_VALUE) return GL_INVALID_VALUE;

	/* ensure that shader input names are of their required type */
	if(vs != GLES_SHADER) return GL_INVALID_OPERATION;

	return GL_NO_ERROR;
}

/***
 * Serialize the current set of attribute bindings into a BATT block. 
 * The BATT block will be returned as a memory block allocated
 * with mali_sys_malloc. 
 *
 * For a description of the BATT block format, check out the file format at
 * http://wiki.trondheim.arm.com/wiki/OpenGL_ES_2.x_Compiler_Integration
 *
 * @param po - The program object to extract the BATT block from
 * @param ptr - OUTPUT PARAM - The pointer to a malloced block holding the data
 * @param size - OUTPUT PARAM - The size of the output block given by ptr
 * @return MALI_ERR_NO_ERROR or MALI_ERR_OUT_OF_MEMORY
 */
MALI_STATIC mali_err_code serialize_attrib_bindings(gles2_program_object *po, char** ptr, u32* size)
{
	mali_linked_list_entry* e = NULL;
	u32 offset = 0;
	u32 num_bindings = 0;

	MALI_DEBUG_ASSERT_POINTER(po);
	MALI_DEBUG_ASSERT_POINTER(ptr);
	MALI_DEBUG_ASSERT_POINTER(size);

	*ptr = NULL;
	*size = 0;

	/* Figure out the size of the memory block. 
	 * The block is given by:
	 *  - 8 byte header (ID and size)
	 *  - 4 bytes "num attributes"
	 *  - "num attributes" entries each holding: 
	 *      - 8 bytes header for the STRI block
	 *      - 4-byte-aligned size of string bytes holding the binding name
	 *      - 4 bytes "binding value"
	 */
	*size = 8 /* header BATT block */
	      + 4 /* "num attributes */
		  ;

	e = __mali_linked_list_get_first_entry(&po->attrib_bindings);
	while (e != NULL)
	{
		gles2_attrib_binding* data = e->data;

		MALI_DEBUG_ASSERT_POINTER(data);

		*size += 8 /* header STRI block */
		      +  MALI_ALIGN(_mali_sys_strlen(data->name)+1, 4) /* 4byte aligned string name */
			  + 4; /* binding value */
		num_bindings++;
		e = __mali_linked_list_get_next_entry(e);
	}

	/* allocate a memory block holding all this data */
	*ptr = _mali_sys_malloc( *size );
	if( NULL == (*ptr) ) return MALI_ERR_OUT_OF_MEMORY;

#define WRITE_STREAM_U8(ptr, offset, size, byteval)                          \
	MALI_DEBUG_ASSERT((offset) < (size), ("Writing outside of stream"));     \
	MALI_DEBUG_ASSERT(!((byteval) & ~0xFF), ("Writing more than one byte")); \
	(ptr)[(offset)++] = (byteval)

#define WRITE_STREAM_U32(ptr, offset, size, intval)                          \
	WRITE_STREAM_U8(ptr, offset, size, (((intval) & 0xFF)));                 \
	WRITE_STREAM_U8(ptr, offset, size, (((intval) & 0xFF00)>> 8));           \
	WRITE_STREAM_U8(ptr, offset, size, (((intval) & 0xFF0000)>> 16));        \
	WRITE_STREAM_U8(ptr, offset, size, (((intval) & 0xFF000000)>> 24))     

#define WRITE_STREAM_HEADER(ptr, offset, size, a,b,c,d,blocksize)            \
	WRITE_STREAM_U8(ptr, offset, size, a);                                   \
	WRITE_STREAM_U8(ptr, offset, size, b);                                   \
	WRITE_STREAM_U8(ptr, offset, size, c);                                   \
	WRITE_STREAM_U8(ptr, offset, size, d);                                   \
	WRITE_STREAM_U32(ptr, offset, size, blocksize)
	
	/* build the memory block content */
	WRITE_STREAM_HEADER(*ptr, offset, *size, 'B', 'A', 'T', 'T', (*size) - 8 );
	WRITE_STREAM_U32(*ptr, offset, *size, num_bindings);

	e = __mali_linked_list_get_first_entry(&po->attrib_bindings);
	while (e != NULL)
	{
		u32 nameLength;
		gles2_attrib_binding* data = e->data;

		MALI_DEBUG_ASSERT_POINTER(data);
		nameLength = MALI_ALIGN(_mali_sys_strlen(data->name) + 1, 4);

		WRITE_STREAM_HEADER(*ptr, offset, *size, 'S', 'T', 'R', 'I', nameLength );

		_mali_sys_memset((*ptr)+offset, 0, nameLength);
		_mali_sys_memcpy((*ptr)+offset, data->name, _mali_sys_strlen(data->name));

		offset += nameLength;
		WRITE_STREAM_U32(*ptr, offset, *size, data->index);
		
		e = __mali_linked_list_get_next_entry(e);
	}

	MALI_DEBUG_ASSERT(offset == *size, ("Serialized too much or too little. Written: %d bytes, buffersize: %d bytes", offset, *size));

#undef WRITE_STREAM_HEADER
#undef WRITE_STREAM_U32
#undef WRITE_STREAM_U8
	return MALI_ERR_NO_ERROR;
}

MALI_STATIC mali_err_code deserialize_attrib_bindings( gles2_program_object *po, const char *binary, GLint length)
{
	struct bs_stream stream; 
	u32 size; 
	u32 num_bindings;
	u32 i;

	MALI_DEBUG_ASSERT_POINTER(po);

	stream.data = binary;
	stream.size = length;
	stream.position = 0;

	/* load MBS1 block */
	size = bs_read_or_skip_header(&stream, MBS1);
	if(size == 0)
	{
		bs_set_error(&po->render_state->binary.log, BS_ERR_WRONG_ATTACHMENT_COUNT,
		             "Input to glProgramBinary is corrupted - no MBS1 block found");
		return MALI_ERR_FUNCTION_FAILED;
	}

	/* search all MBS1 subblocks for BATT */
	size = 0;
	while( bs_stream_bytes_remaining(&stream) > 0 )
	{
		size = bs_read_or_skip_header(&stream, BATT);
		if(size == 0) continue; /* no BATT block found at this stage, try the next one */
		if(size < 4) 
		{
			bs_set_error(&po->render_state->binary.log, BS_ERR_WRONG_ATTACHMENT_COUNT,
			             "Input to glProgramBinary is corrupted - invalid BATT block found");
			return MALI_ERR_FUNCTION_FAILED;
		}
		break;
	}

	/* If no BATT block found, early out. No bindings needed */
	if(bs_stream_bytes_remaining(&stream) == 0) return MALI_ERR_NO_ERROR; 

	/* we have a BATT block in the stream */

	num_bindings = load_u32_value(&stream); 

	for(i = 0; i < num_bindings; i++)
	{
		char* str;
		u32 binding;
		mali_err_code error;
		gles2_attrib_binding *newentry;

		error = bs_read_and_allocate_string(&stream, &str);
		if(error != MALI_ERR_NO_ERROR) return error;
		if(bs_stream_bytes_remaining(&stream) < 4) 
		{
			_mali_sys_free(str);
			bs_set_error(&po->render_state->binary.log, BS_ERR_WRONG_ATTACHMENT_COUNT,
		             "Input to glProgramBinary is corrupted - incorrect BATT block found");
			return MALI_ERR_FUNCTION_FAILED;
		}
		binding = load_u32_value(&stream);

		/* store the binding values */
		newentry = _mali_sys_malloc(sizeof(gles2_attrib_binding));
		if(!newentry) 
		{
			_mali_sys_free(str);
			return MALI_ERR_OUT_OF_MEMORY;
		}

		newentry->name = str;
		newentry->index = binding;
		
		error = __mali_linked_list_insert_data(&po->attrib_bindings, newentry);
		if(error != MALI_ERR_NO_ERROR)
		{
			_mali_sys_free(str);
			_mali_sys_free(newentry);
			return MALI_ERR_OUT_OF_MEMORY;
		}

	}

	return MALI_ERR_NO_ERROR;		
}


/**
 * After linking, there are a lot of things that need to be set up in the rpogram object
 * These must be done both in glLinkprogram and in glProgramBinary. 
 * All that work is set in this function, for easy access in both paths. 
 */
MALI_STATIC mali_err_code _gles2_program_object_post_link_ops(struct gles_context* ctx, gles2_program_object* po, GLuint program, mali_named_list *program_object_list)
{

	gles_program_rendering_state* prs;
	mali_err_code error_code;

	MALI_DEBUG_ASSERT_POINTER(po);
	MALI_DEBUG_ASSERT_POINTER(ctx);

	prs = po->render_state;
	MALI_DEBUG_ASSERT_POINTER(prs);
	MALI_DEBUG_ASSERT(prs->binary.linked, ("Post link setup should only be done if the program is linked"));

	/* link attributes */
	error_code = _gles2_link_attributes(po);
	if (error_code != MALI_ERR_NO_ERROR)
	{
		prs->binary.linked = MALI_FALSE;
		if(error_code == MALI_ERR_OUT_OF_MEMORY) return MALI_ERR_OUT_OF_MEMORY;
		return MALI_ERR_NO_ERROR; /* error log is already set */
	}

	/* set up the magic uniforms */
	_gles2_setup_magic_uniforms(po);

	/* set up glUniformLocations */
	error_code= _gles2_create_gl_uniform_location_table(po);
	if (error_code != MALI_ERR_NO_ERROR)
	{
		prs->binary.linked = MALI_FALSE;
		return MALI_ERR_OUT_OF_MEMORY;
	}

	/* set up the FB specific part of the program rendering state */
	prs->fb_prs = _gles_fb_alloc_program_rendering_state( prs );
	if( prs->fb_prs == NULL )
	{
		prs->binary.linked = MALI_FALSE;
		return MALI_ERR_OUT_OF_MEMORY;
	}
	prs->gb_prs = _gles_gb_alloc_program_rendering_state( prs );
	if( prs->gb_prs == NULL )
	{
		prs->binary.linked = MALI_FALSE;
		return MALI_ERR_OUT_OF_MEMORY;
	}

	error_code = _gles2_create_fp16_fs_uniform_cache(prs);
	if (error_code != MALI_ERR_NO_ERROR)
	{
		prs->binary.linked = MALI_FALSE;
		return MALI_ERR_OUT_OF_MEMORY;
	}

	/* If this is done on the current_program, we need to run use_program to update the program_rendering_state with the new shader */
	if ( (ctx->state.api.gles2->program_env).current_program == program )
	{
		error_code = _gles2_use_program( &(ctx->state), program_object_list, program );
		MALI_CHECK( MALI_ERR_NO_ERROR == error_code, _gles_convert_mali_err( error_code ) );
	}

	error_code = _gles2_fill_fp16_fs_uniform_cache(prs);
	MALI_DEBUG_ASSERT(error_code == MALI_ERR_NO_ERROR, ("Error converting uniforms for the uniform cache"));

	return MALI_ERR_NO_ERROR;
}




/************************************************************************************/
/*   API Functions                                                                  */
/************************************************************************************/


GLenum _gles2_create_program(mali_named_list *program_object_list, GLuint *program )
{
	gles2_program_object *po;
	gles2_program_object_wrapper *pw;
	u32 name;
	mali_err_code error_code;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );
	MALI_DEBUG_ASSERT_POINTER( program );

	*program = 0;

	/* allocate new program object */
	po = _gles2_program_internal_alloc();
	if(NULL == po)  /* out of memory, abort */
	{
		return GL_OUT_OF_MEMORY;
	}

	/* and, allocate wrapper structure */
	pw = _mali_sys_malloc(sizeof(gles2_program_object_wrapper));
	if(NULL == pw)
	{
		_gles2_program_internal_free(po);
		return GL_OUT_OF_MEMORY;
	}
	pw->type = GLES_PROGRAM;
	pw->content = po;

	/* place object in named list */
	name = __mali_named_list_get_unused_name( program_object_list );
	if(0 == name) /* we're out of names; somebody allocated 2^32-1 programs.  Abort and return */
	{
		_mali_sys_free(pw);
		_gles2_program_internal_free(po);
		return GL_OUT_OF_MEMORY;
	}
	error_code = __mali_named_list_insert(program_object_list, name, pw);
	if( error_code != MALI_ERR_NO_ERROR ) /* out of memory */
	{
		_mali_sys_free(pw);
		_gles2_program_internal_free(po);
		return GL_OUT_OF_MEMORY;
	}

	/* return program object name */
	*program = name;

	return GL_NO_ERROR;
}

GLenum _gles2_delete_program(mali_named_list *program_object_list, GLuint name )
{
	mali_linked_list_entry* entry;
	gles2_program_object* po = NULL;
	GLenum vp;
	gles2_program_object_wrapper* pw = NULL;

	MALI_DEBUG_ASSERT_POINTER(program_object_list);

	/* a value of 0 will be silently ignored */
	if(name == 0)
	{
		return GL_NO_ERROR;
	}

	/* retrieve object */
	pw = __mali_named_list_get(program_object_list, name);
	MALI_CHECK_NON_NULL( pw, GL_INVALID_VALUE );

	vp = pw->type;
	/* if input object is a shader, give an error */
	MALI_CHECK( vp == GLES_PROGRAM, GL_INVALID_OPERATION );
	po = pw->content;

	/* if asked to delete current program in some context. Spec says, wait until not part of any current context */
	if(po->bound_context_count != 0)
	{
		po->delete_status = GL_TRUE;
		return GL_NO_ERROR;
	}

	/* We need to detach the shaders before deleting the object. */
	entry = __mali_linked_list_get_first_entry(&po->shaders);
	while(entry != NULL)
	{
		gles2_shader_object * shader_obj;
		shader_obj = _gles2_program_internal_get_type(program_object_list, (GLuint) entry->data, NULL);
		_gles2_program_internal_unattach(program_object_list, po, shader_obj, (GLuint) entry->data);
		entry = __mali_linked_list_get_first_entry(&po->shaders); /* get new first entry in shader bindings list */
	}
	/* Delete object from program list */
	_mali_sys_free(pw); /* deletes program wrapper */
	_gles2_program_internal_free(po);
	__mali_named_list_remove( program_object_list, name);
	return GL_NO_ERROR;
}


void _gles2_program_object_list_entry_delete( struct gles2_program_object_wrapper* wrapper )
{

	MALI_DEBUG_ASSERT_POINTER(wrapper);
	switch(wrapper->type)
	{
	case GLES_SHADER:
	{
		gles2_shader_object* so = wrapper->content;
		/* we don't care about the number of programs this shader is attached to; they are all
		 * going to be deleted anyway. Just set the refcount to zero. This is okay as long as the entire list
		 * is deleted, and a lot faster. */
		_mali_sys_atomic_set( &so->references, 0 );
		/* with no more references, free the object */
		_gles2_shader_internal_free( so );
	}
	break;
	case GLES_PROGRAM:
	{
		gles2_program_object* po = wrapper->content;
		/* we don't care about any attached shaders, they will be deleted by other means. Just set to 0,
		 * this is okay as long as the entire list is deleted, and a lot faster.
		 */
		po->attached_shaders = 0;
		/* free the program object. It is okay to delete the program object outright here, as this function
		 * is only called when the (shared) context is deleted. */
		_gles2_program_internal_free(po);
	}
	break;
	default:
		MALI_DEBUG_ASSERT( 0, ("Illegal object attempted to be deleted"));
		break;
	}

	_mali_sys_free(wrapper);
}

GLenum _gles2_use_program( struct gles_state* state, mali_named_list *program_object_list, GLuint program )
{
	mali_bool del= MALI_FALSE;
	GLuint old_program;
	gles2_program_environment *pstate;
	gles_program_rendering_state *old_program_rendering_state;
	gles_program_rendering_state *program_rendering_state;


	MALI_DEBUG_ASSERT_POINTER( state );

	pstate = &(state->api.gles2->program_env);

	MALI_DEBUG_ASSERT_POINTER( pstate );
	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	/* save old program - in case we need to delete it */
	old_program = pstate->current_program;
	old_program_rendering_state = state->common.current_program_rendering_state;

	/* fetch new program object's rendering state */
	if(program == 0)
	{
		program_rendering_state = NULL;
	} else {
		gles2_program_object_wrapper* pw;
		gles2_program_object* po;

		/* get the requested program, see if it exists*/
		pw = __mali_named_list_get( program_object_list, program);
		MALI_CHECK_NON_NULL( pw, GL_INVALID_VALUE ); /* requested program does not exist */

		MALI_CHECK( pw->type == GLES_PROGRAM, GL_INVALID_OPERATION ); /* requested program was not a program */

		po = pw->content;
		MALI_DEBUG_ASSERT_POINTER( po );

		program_rendering_state = po->render_state;
		MALI_DEBUG_ASSERT_POINTER( program_rendering_state );

		/* we cannot use a non-linked program */
		MALI_CHECK( MALI_TRUE == program_rendering_state->binary.linked, GL_INVALID_OPERATION );

		/* addref new program object */
		po->bound_context_count++;
		MALI_DEBUG_ASSERT(po->bound_context_count > 0, ("Should be at least 1 bound context by now"));
	}

	/* set the program by derefing the old bound program rendering state and addrefing the new one, and update the state */
	if( NULL != program_rendering_state ) _gles_program_rendering_state_addref(program_rendering_state);
	if( NULL != old_program_rendering_state ) _gles_program_rendering_state_deref(old_program_rendering_state);
	pstate->current_program = program;
	state->common.current_program_rendering_state = program_rendering_state;
	mali_statebit_set( & state->common, MALI_STATE_VS_PRS_UPDATE_PENDING );

	/* lastly, check if we should delete the old program. This will be done if that program's delete flag is set */
	if(0 != old_program) /* program 0 should never be deleted */
	{
		/* first fetch the old program object */
		gles2_program_object_wrapper* pw;
		gles2_program_object* po;

		/* the old program should be present and a program */
		pw = __mali_named_list_get( program_object_list, old_program);
		MALI_DEBUG_ASSERT_POINTER(pw);
		MALI_DEBUG_ASSERT(pw->type == GLES_PROGRAM, ("Old program should've been a program"));
		po = pw->content;

		/* deref old program object */
		po->bound_context_count--;

		/* delete object if delete set and context count == 0 */
		if( po->delete_status == GL_TRUE && po->bound_context_count == 0 )
		{
			del = MALI_TRUE;
		}
	}

	/* maybe delete old program object */
	if(del == MALI_TRUE )
	{
		GLenum err = _gles2_delete_program(program_object_list, old_program);
		MALI_IGNORE( err ); /* NOTE: we ignore this error because it can never logically fail */
	}

	return GL_NO_ERROR;
}

GLboolean _gles2_is_program(mali_named_list *program_object_list, GLuint program)
{
	GLenum object_type;
	GLboolean retval;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	_gles2_program_internal_get_type(program_object_list, program, &object_type);
	retval = (object_type == GLES_PROGRAM)?GL_TRUE:GL_FALSE;

	return retval;
}

GLenum _gles2_attach_shader( mali_named_list *program_object_list, GLuint program, GLuint shader)
{
	gles2_program_object * program_obj;
	gles2_shader_object * shader_obj;
	GLenum vp, vs;
	GLenum error = GL_NO_ERROR;
	mali_linked_list_entry* it;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	program_obj = _gles2_program_internal_get_type(program_object_list, program, &vp);
	shader_obj = _gles2_program_internal_get_type(program_object_list, shader, &vs);

	error = _gles2_internal_program_shader_valid(vp, vs);
	MALI_CHECK( error == GL_NO_ERROR, error );

	/* ensure this shader is not already attached to this object. */
	it = __mali_linked_list_get_first_entry(&program_obj->shaders);
	while(it != NULL)
	{
		MALI_CHECK( ((GLuint)it->data) != shader, GL_INVALID_OPERATION );
		it = __mali_linked_list_get_next_entry(it);
	}

	/* BUGZILLA 2202: According to Khronos meeting 2007.feb.14, two shaders of the same type can no longer be attached to one program
	 * Note: I'll retain support for more than two shaders, by keeping the linked list - never know when this will change again. */
	it = __mali_linked_list_get_first_entry(&program_obj->shaders);
	while(it != NULL)
	{
		GLenum existing_vs;
		gles2_shader_object* existing_shader = _gles2_program_internal_get_type(program_object_list, (GLuint) it->data, &existing_vs);
		MALI_CHECK( existing_shader->shader_type != shader_obj->shader_type, GL_INVALID_OPERATION );
		it = __mali_linked_list_get_next_entry(it);
	}


	/* attach shader to program */
	if(__mali_linked_list_insert_data(&program_obj->shaders, (void*) shader) == MALI_ERR_NO_ERROR)
	{
		_gles2_shader_object_addref( shader_obj );
		program_obj->attached_shaders++;
		return GL_NO_ERROR;

	} /*else*/

	return GL_OUT_OF_MEMORY;

}

GLenum _gles2_detach_shader( mali_named_list *program_object_list, gles2_program_environment *program_env, GLuint program, GLuint shader)
{
	gles2_program_object * program_obj;
	gles2_shader_object * shader_obj;
	GLenum vp;
	GLenum vs;
	GLenum error = GL_NO_ERROR;
	mali_bool found = MALI_FALSE;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );
	MALI_DEBUG_ASSERT_POINTER( program_env );

	MALI_IGNORE( program_env );

	program_obj = _gles2_program_internal_get_type(program_object_list, program, &vp);
	shader_obj = _gles2_program_internal_get_type(program_object_list, shader, &vs);


	error = _gles2_internal_program_shader_valid(vp, vs);
	MALI_CHECK( error == GL_NO_ERROR, error );

	found = _gles2_is_shader_attached( program_obj, shader );
	MALI_CHECK( MALI_TRUE == found, GL_INVALID_OPERATION );

	/* perform the detahcment */
	_gles2_program_internal_unattach(program_object_list, program_obj, shader_obj, shader);

	return GL_NO_ERROR;

}

GLenum _gles2_link_program( struct gles_context *ctx, mali_named_list *program_object_list, GLuint program )
{
	gles2_program_object *po;
	GLenum object_type;
	mali_err_code error_code;
	gles2_shader_object* fragment_object;
	gles2_shader_object* vertex_object;
	struct gles_program_rendering_state* new_rendering_state;
	mali_base_ctx_handle base_ctx;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	base_ctx = ctx->base_ctx;

	/* assert that input is a valid object */
	po = _gles2_program_internal_get_type(program_object_list, program, &object_type);
	MALI_CHECK( object_type != GL_INVALID_VALUE, GL_INVALID_VALUE );

	/* assert that input is a program object */
	MALI_CHECK( object_type == GLES_PROGRAM, GL_INVALID_OPERATION );

	/* create a new rendering state. Must be done because the old one may be in use by another context */
	new_rendering_state = _gles_program_rendering_state_alloc();
	MALI_CHECK_NON_NULL( new_rendering_state, GL_OUT_OF_MEMORY );

	_gles_program_rendering_state_deref(po->render_state);
	po->render_state = new_rendering_state;

	/* Check that only one of each shader is attached, ensure both are compiled */
	if(po->attached_shaders == 0)
	{
		GLenum err;
		bs_set_error(&new_rendering_state->binary.log, BS_ERR_WRONG_ATTACHMENT_COUNT,
		             "A program cannot be linked unless there are any shaders attached to it");
		err = bs_is_error_log_set_to_out_of_memory(&new_rendering_state->binary.log) ? GL_OUT_OF_MEMORY : GL_NO_ERROR;
		return err;
	}

	if(po->attached_shaders != 2)
	{
		GLenum err;
		bs_set_error( &new_rendering_state->binary.log, BS_ERR_WRONG_ATTACHMENT_COUNT,
		              "GLSL allows exactly two attached shaders (one of each type) per program");
		err = bs_is_error_log_set_to_out_of_memory(&new_rendering_state->binary.log) ? GL_OUT_OF_MEMORY : GL_NO_ERROR;
		return err;
	}

	vertex_object = _gles2_program_internal_get_type(program_object_list, (u32)(po->shaders.first->data), NULL);
	if(vertex_object->shader_type == GL_FRAGMENT_SHADER)
	{
		fragment_object = vertex_object;
		vertex_object = _gles2_program_internal_get_type(program_object_list, (u32)(po->shaders.first->next->data), NULL);
	}
	else
	{
		fragment_object = _gles2_program_internal_get_type(program_object_list, (u32)(po->shaders.first->next->data), NULL);
	}

	if(vertex_object->shader_type != GL_VERTEX_SHADER || fragment_object->shader_type != GL_FRAGMENT_SHADER)
	{
		GLenum err;
		bs_set_error( &new_rendering_state->binary.log, BS_ERR_WRONG_ATTACHMENT_COUNT,
		              "A linked program must contain exactly one of each type of shader");
		err = bs_is_error_log_set_to_out_of_memory(&new_rendering_state->binary.log) ? GL_OUT_OF_MEMORY : GL_NO_ERROR;
		return err;
	}

	if(vertex_object->binary.compiled != MALI_TRUE || fragment_object->binary.compiled != MALI_TRUE)
	{
		GLenum err;
		bs_set_error( &new_rendering_state->binary.log, BS_ERR_SHADERS_NOT_COMPILED,
		              "All attached shaders must be compiled prior to linking");
		err = bs_is_error_log_set_to_out_of_memory(&new_rendering_state->binary.log) ? GL_OUT_OF_MEMORY : GL_NO_ERROR;
		return err;
	}

#if EXTENSION_GET_PROGRAM_BINARY_OES_ENABLE
	{
		u32 attrib_size = 0;
		char *attrib_block = NULL;
		mali_err_code err;
		error_code = serialize_attrib_bindings(po, &attrib_block, &attrib_size);	
		if(error_code != MALI_ERR_NO_ERROR) return GL_OUT_OF_MEMORY;
	
		err = __mali_merge_binary_shaders( &new_rendering_state->get_program_binary_data, &new_rendering_state->get_program_binary_size, attrib_block, attrib_size, &vertex_object->binary, &fragment_object->binary);
		_mali_sys_free(attrib_block);
		if(err != MALI_ERR_NO_ERROR) return GL_OUT_OF_MEMORY;

	}
#endif

	/* ALL RIGHT - WE ARE READY TO LINK THE PROGRAM! */
	error_code = __mali_link_binary_shaders( base_ctx, &new_rendering_state->binary, &vertex_object->binary, &fragment_object->binary);
	MALI_CHECK( error_code == MALI_ERR_NO_ERROR, GL_NO_ERROR );

	error_code = _gles2_program_object_post_link_ops(ctx, po, program, program_object_list);
	if(error_code == MALI_ERR_OUT_OF_MEMORY) return GL_OUT_OF_MEMORY;
	MALI_DEBUG_ASSERT(error_code == MALI_ERR_NO_ERROR, ("only legal error code at this point"));

	mali_statebit_set( &ctx->state.common, MALI_STATE_FB_FRAGMENT_UNIFORM_UPDATE_PENDING );


	return GL_NO_ERROR;
}

GLenum _gles2_validate_program (mali_named_list *program_object_list, GLuint program )
{
	GLenum object_type;
	gles2_program_object *po;
	u32 i,j;
	struct bs_sampler_set* samplers;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	/* assert that input is a valid object */
	po = _gles2_program_internal_get_type(program_object_list, program, &object_type);
	MALI_CHECK( object_type != GL_INVALID_VALUE, GL_INVALID_VALUE );

	/* assert that input is a program object */
	MALI_CHECK( object_type == GLES_PROGRAM, GL_INVALID_OPERATION );

	/* validate program - we assume things will fail horribly. */
	po->validate_status = GL_FALSE;

	/* if program isn't linked, it can not be used to render. Early out */
	MALI_DEBUG_ASSERT_POINTER(po->render_state);
	if(po->render_state->binary.linked == MALI_FALSE)
	{
		GLenum err;
		bs_set_error(&po->render_state->binary.log, BS_ERR_VALIDATE_FAILED, "Program is not successfully linked");
		err = bs_is_error_log_set_to_out_of_memory(&po->render_state->binary.log) ? GL_OUT_OF_MEMORY : GL_NO_ERROR;
		return err;
	}

	/* cache the samplers struct */
	MALI_DEBUG_ASSERT_POINTER( po->render_state );
	MALI_DEBUG_ASSERT_POINTER( &po->render_state->binary );
	samplers = &po->render_state->binary.samplers;
	MALI_DEBUG_ASSERT_POINTER( samplers );

	/* things to check for:
	 *
	 * 1: are we using any illegal texture units?
	 *    Only way to test this is to test all the sampler variables and check if any of them values are set too high.
	 * 2: are we using a texture unit for more than one target type (GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP etc)?
	 *    Actually, we have no problem with this, but the spec says this is illegal. Oh well, might as well
	 *    explicitly disallow it. The best way to do this is to
	 *    - for each sampler
	 *      - test all other samplers, ensure they are of the same base type (sampler_2D, sampler_3D, etc).
	 *
	 * 3: Other stuff. This is optional. Not implemented for now, likely never.
	 */

	/* step 1 - illegal texture units */
	for(i = 0; i < samplers->num_samplers; i++)
	{
		int value = samplers->info[i].API_value;
		if( value >= GLES2_MAX_TEXTURE_IMAGE_UNITS )
		{
			GLenum err;
			bs_set_program_validate_error_sampler_out_of_range(
				&po->render_state->binary,
				samplers->info[i].symbol->name,
				value,
				GLES2_MAX_TEXTURE_IMAGE_UNITS );
			err = bs_is_error_log_set_to_out_of_memory(&po->render_state->binary.log) ? GL_OUT_OF_MEMORY : GL_NO_ERROR;
			return err;
		}
	}

	/* step 2 - distinct texture type */
	for(i = 0; i < samplers->num_samplers; i++)
	{
		int ivalue = samplers->info[i].API_value;
		struct bs_symbol *a = samplers->info[i].symbol;
		for(j = 0; j < samplers->num_samplers; j++)
		{
			int jvalue = samplers->info[j].API_value;
			if(i != j && ivalue == jvalue)
			{
				struct bs_symbol *b = samplers->info[j].symbol;

				/* we have found two samplers bound to the same texture units */
				/* test that they are of different types */
				if( a->datatype != b->datatype || a->type.primary.vector_size != b->type.primary.vector_size )
				{
					GLenum err;
					bs_set_program_validate_error_sampler_of_different_types_share_unit(&po->render_state->binary,
					                                                                    a->name, b->name,
					                                                                    ivalue);
					err = bs_is_error_log_set_to_out_of_memory(&po->render_state->binary.log) ? GL_OUT_OF_MEMORY : GL_NO_ERROR;
					return err;
				}
			}
		}
	}

	/* step 3 - other stuff. another day */

	po->validate_status = GL_TRUE;

	return GL_NO_ERROR;
}

GLenum _gles2_get_program_info_log( mali_named_list *program_object_list, GLuint program, GLsizei bufsize, GLsizei *length, char *infolog)
{
	gles2_program_object *po;
	GLenum object_type;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	if(bufsize < 0)
	{
		return GL_INVALID_VALUE;
	}

	/* assert that input is a valid object */
	po = _gles2_program_internal_get_type(program_object_list, program, &object_type);
	MALI_CHECK( object_type != GL_INVALID_VALUE, GL_INVALID_VALUE );

	/* assert that input is a program object */
	MALI_CHECK( object_type == GLES_PROGRAM, GL_INVALID_OPERATION );

	MALI_DEBUG_ASSERT_POINTER(po);
	MALI_DEBUG_ASSERT_POINTER(po->render_state);
	bs_get_log(&po->render_state->binary.log, bufsize, length, infolog);

	return GL_NO_ERROR;
}

GLenum _gles2_get_programiv( mali_named_list* program_object_list, GLuint program, GLenum pname, GLint* params )
{
	gles2_program_object *po;
	GLenum object_type;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	if( program == 0 )return GL_INVALID_VALUE;

	/* assert that input is a valid object */
	po = _gles2_program_internal_get_type(program_object_list, program , &object_type);
	MALI_CHECK( object_type != GL_INVALID_VALUE, GL_INVALID_VALUE );

	/* assert that input is a program object */
	MALI_CHECK( object_type == GLES_PROGRAM, GL_INVALID_OPERATION );

	MALI_CHECK_NON_NULL( params, GL_NO_ERROR );

	MALI_DEBUG_ASSERT_POINTER(po->render_state);

	switch(pname)
	{
	case GL_DELETE_STATUS:
		*params = (GLint) po->delete_status;
		break;
	case GL_VALIDATE_STATUS:
		*params = (GLint) po->validate_status;
		break;
	case GL_INFO_LOG_LENGTH:
		bs_get_log_length(&po->render_state->binary.log, params);
		break;
	case GL_ATTACHED_SHADERS:
		*params = (GLint) po->attached_shaders;
		break;
	case GL_ACTIVE_ATTRIBUTES:
	{
		*params = bs_symbol_count_actives(po->render_state->binary.attribute_symbols, _gles_active_filters , GLES_ACTIVE_FILTER_SIZE);
	}
	break;
	case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
		*params = bs_symbol_longest_location_name_length(po->render_state->binary.attribute_symbols) + 1;
		break;
	case GL_ACTIVE_UNIFORMS:
	{
		*params = bs_symbol_count_actives(po->render_state->binary.uniform_symbols, _gles_active_filters , GLES_ACTIVE_FILTER_SIZE );
	}
	break;
	case GL_ACTIVE_UNIFORM_MAX_LENGTH:
		*params = bs_symbol_longest_location_name_length(po->render_state->binary.uniform_symbols) + 1;
		break;
	case GL_LINK_STATUS:
		*params = po->render_state->binary.linked;
		break;
#if EXTENSION_GET_PROGRAM_BINARY_OES_ENABLE
	case GL_PROGRAM_BINARY_LENGTH_OES:
		*params = po->render_state->get_program_binary_size;
		break;
#endif	

	default:
		return GL_INVALID_ENUM;
	}

	return GL_NO_ERROR;
}


GLenum _gles2_get_attached_shaders (mali_named_list* program_object_list, GLuint program, GLsizei maxcount, GLsizei *count, GLuint *shaders)
{
	gles2_program_object *po;
	GLenum object_type;
	int ncount = 0;
	mali_linked_list_entry* it;

	if(maxcount < 0) return GL_INVALID_VALUE;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	po = _gles2_program_internal_get_type(program_object_list, program, &object_type );

	/* assert that name is valid */
	MALI_CHECK( object_type != GL_INVALID_VALUE, GL_INVALID_VALUE );

	/* assert that object is a program object */
	MALI_CHECK( object_type == GLES_PROGRAM, GL_INVALID_OPERATION );

	it = __mali_linked_list_get_first_entry(&po->shaders);
	while(it != NULL && ncount < maxcount)
	{
		if( shaders != NULL ) shaders[ncount] = (GLuint) it->data;
		ncount++;
		it = __mali_linked_list_get_next_entry(it);
	}
	if(count != NULL) *count = ncount;

	return GL_NO_ERROR;

}

#if EXTENSION_GET_PROGRAM_BINARY_OES_ENABLE
GLenum _gles2_get_program_binary ( mali_named_list *program_object_list, GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, GLvoid *binary )
{
	gles2_program_object *po;
	GLenum object_type;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	/* assert that input is a valid object */
	po = _gles2_program_internal_get_type(program_object_list, program, &object_type);
	if( object_type == GL_INVALID_VALUE) return GL_INVALID_VALUE;

	/* assert that input is a program object */
	if( object_type != GLES_PROGRAM) return GL_INVALID_OPERATION;
	if( !po->render_state) return GL_INVALID_OPERATION;

	if( po->render_state->binary.linked == MALI_FALSE || bufSize < (GLsizei)po->render_state->get_program_binary_size )
	{
		if(length) *length = 0;
		return GL_INVALID_OPERATION;
	}

	/* copy the program binary onto the output buffer */
	_mali_sys_memcpy(binary, po->render_state->get_program_binary_data, po->render_state->get_program_binary_size);

	/* other outputs */
	if(length) *length = po->render_state->get_program_binary_size;
	if(binaryFormat) *binaryFormat = GL_MALI_PROGRAM_BINARY_ARM;
	return GL_NO_ERROR;
}

GLenum _gles2_program_binary ( struct gles_context *ctx, mali_named_list *program_object_list, GLuint program, GLenum binaryFormat, const GLvoid *binary, GLint length )
{
	gles2_program_object *po;
	GLenum object_type;
	mali_err_code error_code;
	bs_shader loaded_vertex_shader;
	bs_shader loaded_fragment_shader;
	gles_program_rendering_state * new_rendering_state;

	MALI_DEBUG_ASSERT_POINTER( ctx );
	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	if( binaryFormat != GL_MALI_PROGRAM_BINARY_ARM ) return GL_INVALID_ENUM;

	po = _gles2_program_internal_get_type(program_object_list, program, &object_type);
	if( object_type == GL_INVALID_VALUE) return GL_INVALID_VALUE;
	if( object_type != GLES_PROGRAM) return GL_INVALID_OPERATION;

	/* reset the current shader object by allocating a new PRS. 
	 * Also deref the old one, which probably will be deleted. 
	 * After this point, the old linked shader is gone */
	new_rendering_state = _gles_program_rendering_state_alloc();
	if( !new_rendering_state) return GL_OUT_OF_MEMORY;
	_gles_program_rendering_state_deref(po->render_state);
	po->render_state = new_rendering_state;

	__mali_shader_binary_state_init(&loaded_vertex_shader);	
	__mali_shader_binary_state_init(&loaded_fragment_shader);	

	/* loader code assumed the binary parameter exist. Early out if it does not. 
	 * There are no defined error codes for this scenario */
	if( !binary || length <= 0) 
	{
		bs_set_error(&po->render_state->binary.log, BS_ERR_SHADERS_NOT_COMPILED,
                              "Input data to glProgramBinary is empty");
		return GL_NO_ERROR;
	}

	/* store the binary code in the program object in case we call glGetProgramBinary on this 
	 * program again. In such a case, just return exactly what we were given in to this 
	 * function. */
	new_rendering_state->get_program_binary_data = _mali_sys_malloc(length);
	if(new_rendering_state->get_program_binary_data == NULL) 
	{
		bs_set_error_out_of_memory(&po->render_state->binary.log);
		return GL_OUT_OF_MEMORY;
	}
	new_rendering_state->get_program_binary_size = length;

	/* load the vertex and fragment shader into the local variables on the stack
	 * This code will load the first CVER / CFRA block inside the MBS1 block in the binary. */
	error_code = __mali_binary_shader_load(&loaded_vertex_shader, BS_VERTEX_SHADER, binary, length);
	if(MALI_ERR_NO_ERROR != error_code) 
	{
		bs_set_error(&po->render_state->binary.log, BS_ERR_SHADERS_NOT_COMPILED,
                              "Input data to glProgramBinary is not recognized");
	 __mali_shader_binary_state_reset(&loaded_vertex_shader);
		return GL_NO_ERROR;
	}
	error_code = __mali_binary_shader_load(&loaded_fragment_shader, BS_FRAGMENT_SHADER, binary, length);
	if(MALI_ERR_NO_ERROR != error_code) 
	{
		bs_set_error(&po->render_state->binary.log, BS_ERR_SHADERS_NOT_COMPILED,
                              "Input data to glProgramBinary is not recognized");
	 __mali_shader_binary_state_reset(&loaded_vertex_shader);
	 __mali_shader_binary_state_reset(&loaded_fragment_shader);
		return GL_NO_ERROR;
	}

	/* load all attribute bindings from file 
	 * This code will load the first BATT block inside the MBS1 block in the binary. 
	 * The function will also store the loaded data into the program object so 
	 * that it is available the next time around. */
	error_code = deserialize_attrib_bindings(po, binary, length);
	if ( MALI_ERR_NO_ERROR != error_code )
	{
	 	__mali_shader_binary_state_reset(&loaded_vertex_shader);
	 	__mali_shader_binary_state_reset(&loaded_fragment_shader);
	 	 MALI_ERROR( _gles_convert_mali_err( error_code ) );
	}
	
	/* ALL RIGHT - WE ARE READY TO LINK THE PROGRAM! */
	error_code = __mali_link_binary_shaders( ctx->base_ctx, &new_rendering_state->binary, &loaded_vertex_shader, &loaded_fragment_shader);


	 __mali_shader_binary_state_reset(&loaded_vertex_shader);
	 __mali_shader_binary_state_reset(&loaded_fragment_shader);

	MALI_CHECK( error_code == MALI_ERR_NO_ERROR, GL_NO_ERROR );

	error_code = _gles2_program_object_post_link_ops(ctx, po, program, program_object_list);
	if(error_code == MALI_ERR_OUT_OF_MEMORY) return GL_OUT_OF_MEMORY;
	MALI_DEBUG_ASSERT(error_code == MALI_ERR_NO_ERROR, ("only legal error code at this point"));

	mali_statebit_set( &ctx->state.common, MALI_STATE_FB_FRAGMENT_UNIFORM_UPDATE_PENDING );

	return GL_NO_ERROR;
}

#endif
