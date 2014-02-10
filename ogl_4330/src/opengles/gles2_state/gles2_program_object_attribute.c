/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>

#include "gles2_program_object.h"
#include "gles2_shader_object.h"
#include "../gles_util.h"
#include <shared/binary_shader/bs_symbol.h>
#include <shared/binary_shader/bs_error.h>
#include <shared/binary_shader/link_gp2.h>
#include "gles_convert.h"

GLenum _gles2_bind_attrib_location( mali_named_list* program_object_list, GLuint program, GLuint index, const char* name )
{
	gles2_program_object *po;
	GLenum object_type;
	gles2_attrib_binding *newentry;
	int namelen;
	char* newentry_string;
	mali_err_code error;
	mali_linked_list_entry* entry;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	/* assert that index is valid */
	if( index >= GLES_VERTEX_ATTRIB_COUNT ) return GL_INVALID_VALUE;

	/* assert that name is not one of the reserved names */
	/* (uses strlen and memcmp instead of strncmp) */
	if ((_mali_sys_strlen(name) > 2) && (_mali_sys_memcmp(name, "gl_", 3) == 0)) return GL_INVALID_OPERATION;/*gl_ names are reserved */

	po = _gles2_program_internal_get_type(program_object_list, program, &object_type);
	/* assert that name is valid */
	MALI_CHECK( object_type != GL_INVALID_VALUE, GL_INVALID_VALUE );

	/* assert that object is a program object */
	MALI_CHECK( object_type == GLES_PROGRAM, GL_INVALID_OPERATION );

	/* create a new linked list object - this will be added to the linked list later */
	newentry = _mali_sys_malloc(sizeof(gles2_attrib_binding));
	MALI_CHECK_NON_NULL( newentry, GL_OUT_OF_MEMORY );

	namelen = _mali_sys_strlen(name);
	newentry_string = _mali_sys_malloc(namelen+1); /* +1 for terminator character */
	if(newentry_string == NULL)
	{
		_mali_sys_free(newentry);
		return GL_OUT_OF_MEMORY;
	}

	_mali_sys_memcpy(newentry_string, name, namelen);
	newentry_string[namelen] = '\0';
	newentry->name = newentry_string;
	newentry->index = index;

	/* check whether the name is already present in the bindings list. If so, replace the old one */
	entry = __mali_linked_list_get_first_entry(&po->attrib_bindings);

	while(entry != NULL)
	{
		gles2_attrib_binding* data = entry->data;
		if(_mali_sys_strcmp(data->name, name) == 0)
		{
			/* was already present, simply replace entry */
			_mali_sys_free(data->name);
			_mali_sys_free(data);
			entry->data = newentry;
			return GL_NO_ERROR;
		}

		entry = __mali_linked_list_get_next_entry(entry);
	}

	/* If we got to this point, insert newentry into list */
	error = __mali_linked_list_insert_data(&po->attrib_bindings, newentry);
	if( MALI_ERR_NO_ERROR != error )
	{
		_mali_sys_free(newentry->name);
		_mali_sys_free(newentry);
		return _gles_convert_mali_err( error );
	}

	/* and, we're done! */
	return GL_NO_ERROR;
}
GLenum _gles2_get_active_attrib(mali_named_list* program_object_list, GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint *size, GLenum* type, char* name)
{
	gles2_program_object *po;
	GLenum object_type;

	/* assert bufsize positive */
	if(bufsize < 0) return GL_INVALID_VALUE;
	/* assert that index is valid */
	if( index >= GLES_VERTEX_ATTRIB_COUNT ) return GL_INVALID_VALUE;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );

	po = _gles2_program_internal_get_type(program_object_list, program, &object_type);

	/* assert that name is valid */
	MALI_CHECK( object_type != GL_INVALID_VALUE, GL_INVALID_VALUE );

	/* assert that object is a program object */
	MALI_CHECK( object_type == GLES_PROGRAM, GL_INVALID_OPERATION );
	MALI_DEBUG_ASSERT_POINTER( po->render_state );

	/* check if index is too high (failed link or unlinked programs may not have attribute symbol tables, and thus the index is always too high) */
	MALI_CHECK_NON_NULL( po->render_state->binary.attribute_symbols, GL_INVALID_VALUE );
	MALI_CHECK( index < bs_symbol_count_actives(po->render_state->binary.attribute_symbols, _gles_active_filters, GLES_ACTIVE_FILTER_SIZE), GL_INVALID_VALUE );

	/* lookup values */
	{
		struct bs_symbol* symbol = NULL;

		symbol = bs_symbol_get_nth_active(po->render_state->binary.attribute_symbols, index, name, bufsize,
		                                  _gles_active_filters, GLES_ACTIVE_FILTER_SIZE);

		MALI_DEBUG_ASSERT_POINTER( symbol );

		/* write outputs */

		if(NULL != length)
		{
			if(NULL != name && bufsize > 0)
			{
				*length = _mali_sys_strlen(name);
			}
			else
			{
				*length = 0;
			}
		}

		if(size)
		{
			*size = symbol->array_size;
			if(!symbol->array_size) (*size) = 1;
		}
		if(type) *type = _gles2_convert_datatype_to_gltype(symbol->datatype, symbol->type.primary.vector_size);

		/* and, we're done. */
	}

	return GL_NO_ERROR;
}


GLenum _gles2_get_attrib_location( mali_named_list* program_object_list, GLuint program, const char* name, GLint *retval )
{
	gles2_program_object *po;
	GLenum object_type;
	s32 vertex_position;
	struct bs_symbol* symbol;

	MALI_DEBUG_ASSERT_POINTER( program_object_list );
	if(retval) *retval = -1;

	po = _gles2_program_internal_get_type(program_object_list, program, &object_type);

	/* assert that name is valid */
	MALI_CHECK( object_type != GL_INVALID_VALUE, GL_INVALID_VALUE );

	/* assert that object is a program object */
	MALI_CHECK( object_type == GLES_PROGRAM, GL_INVALID_OPERATION );

	/* assert program has been linked */
	MALI_DEBUG_ASSERT_POINTER( po->render_state );
	MALI_CHECK( MALI_TRUE == po->render_state->binary.linked, GL_INVALID_OPERATION );
	MALI_CHECK_NON_NULL( retval, GL_NO_ERROR );

	/* search through all active uniforms, locate the required uniform and return its position */
	symbol = bs_symbol_lookup(po->render_state->binary.attribute_symbols, name, &vertex_position, NULL, NULL);

	/* not present? */
	MALI_CHECK_NON_NULL( symbol, GL_NO_ERROR );

	/* vertex_position always positive if symbol is non NULL */
	*retval = po->render_state->reverse_attribute_remap_table[ vertex_position / 4 ];

	return GL_NO_ERROR;
}

MALI_STATIC void attrib_binding_free(mali_linked_list_cb_param sact)
{
	gles2_attrib_binding* act = (gles2_attrib_binding*) sact;
	_mali_sys_free(act->name);
	_mali_sys_free(act);
}

void _gles2_clear_attrib_bindings(gles2_program_object* po)
{

	MALI_DEBUG_ASSERT_POINTER( po );
	if( &po->attrib_bindings != NULL )
	{
		__mali_linked_list_empty(&po->attrib_bindings, &attrib_binding_free);
	}

}

/* this function is the one to change if we want better auto-location allocation
 * Current implementation locates the smallest fitting block. This is a greedy algorithm.
 * */
MALI_STATIC int find_best_location(const struct bs_symbol *symb, int attribute_occupants[])
{
	int i;
	int best_location = -1;
	int best_location_size = 0;
	int one_before_this_location = -1;
	int free_locations = 0;
	int needed_streams = (symb->block_size.vertex + 3 ) / 4;

	/* find the best location */
	for (i = 0; i < GLES_VERTEX_ATTRIB_COUNT; ++i)
	{
		if (attribute_occupants[i] < 0)
		{
			free_locations++;
		}
		else
		{
			/* the block we just found, was it large enough? */
			if(free_locations >= needed_streams)
			{
				/* was it a better fit than what we had? */
				if(best_location == -1 || best_location_size > free_locations)
				{
					best_location_size = free_locations;
					best_location = one_before_this_location + 1;
				}
			}

			/* reset tracking variables */
			free_locations =  0;
			one_before_this_location  =  i;
		}
	}

	/* Check if the final block was large enough */
	if(free_locations >= needed_streams)
	{
		/* was it a better fit than what we had? */
		if(best_location == -1 || best_location_size > free_locations)
		{
			best_location_size = free_locations;
			best_location = one_before_this_location + 1;
		}
	}

	return best_location;
}

/**
*	Setup a binary shader remap table so that they match the streams required from GL.
*	Bindings are given maximum priority, and non-bound symbols are attempted packed around the bound ones.
*	bs_remap[old_bs_location] = new_bs_location
*/
MALI_STATIC mali_err_code setup_bs_remap_table(gles2_program_object *po, int bs_remap[])
{
	unsigned int i, j;
	mali_linked_list_entry* e;

	/* a remapping table, mapping locations to attribute indices. used to make sure no attributes are overwritten */
	int occupants[MALIGP2_MAX_VS_INPUT_REGISTERS];

	/* reset all attribute locations and occupants */
	for (i = 0; i < MALIGP2_MAX_VS_INPUT_REGISTERS; ++i)
	{
		bs_remap[i] = -1;
		occupants[i] = -1;
	}

	MALI_DEBUG_ASSERT_POINTER( po->render_state );

	/* iterate through all bindings, and assign them locations */
	e = __mali_linked_list_get_first_entry(&po->attrib_bindings);
	while (e != NULL)
	{
		struct bs_symbol *symb = NULL;
		gles2_attrib_binding* binding = (gles2_attrib_binding*)e->data;
		int bs_location = -1;

		/* look for attribute (only active attributes are assigned locations) */
		for (j = 0; j < po->render_state->binary.attribute_symbols->member_count; ++j)
		{
			symb = po->render_state->binary.attribute_symbols->members[j];
			if (0 == _mali_sys_strcmp(symb->name, binding->name))
			{
				bs_location = po->render_state->binary.attribute_symbols->members[j]->location.vertex;
				MALI_DEBUG_ASSERT(bs_location >= 0, ("All existing symbols should exist by definition"));
				bs_location = bs_location / 4;
				break;
			}
		}

		if (bs_location >= 0)
		{
			/* setup occupant list */
			for (j = 0; j < ( symb->block_size.vertex + 3 ) / 4; ++j)
			{
				int pos = binding->index + j;

				/* stupid user, tried to bind an attribute outside of the valid range */
				if (pos >= GLES_VERTEX_ATTRIB_COUNT)
				{
					bs_set_program_link_error_attribute_bound_outsize_of_legal_range( &po->render_state->binary, symb->name, pos, GLES_VERTEX_ATTRIB_COUNT-1);
					if(bs_is_error_log_set_to_out_of_memory(&po->render_state->binary.log)) return MALI_ERR_OUT_OF_MEMORY;
					return MALI_ERR_FUNCTION_FAILED;
				}
				occupants[binding->index + j] = bs_location + j;
				bs_remap[bs_location + j] = binding->index + j;
			}
		}

		/* goto next element */
		e = __mali_linked_list_get_next_entry(e);
	}

	/* iterate through all attributes and insert them into the first fitting location */
	for (i = 0; i < po->render_state->binary.attribute_symbols->member_count ; ++i)
	{
		int best_location;
		struct bs_symbol *symb = po->render_state->binary.attribute_symbols->members[i];
		int bs_location = symb->location.vertex;
		MALI_DEBUG_ASSERT(bs_location >= 0, ("All existing symbols should exist by definition"));
		bs_location = bs_location / 4;

		if (bs_remap[bs_location] >= 0) continue; /* don't process already-bound attribs */

		/* search for best location to put the attribute on */
		best_location = find_best_location(symb, occupants);

		/* check for success */
		if (-1 == best_location)
		{
			/* we failed, no location found */
			bs_set_error( &po->render_state->binary.log, BS_ERR_LINK_TOO_MANY_ATTRIBUTES,
			                              "Not enough attribute locations available");
			if(bs_is_error_log_set_to_out_of_memory(&po->render_state->binary.log)) return MALI_ERR_OUT_OF_MEMORY;
			return MALI_ERR_FUNCTION_FAILED;
		}

		/* tag all locations for use by the attribute */
		for (j = 0; j < ( symb->block_size.vertex + 3 )/ 4; ++j)
		{
			int pos = best_location + j;

			MALI_DEBUG_ASSERT_RANGE(pos, 0, GLES_VERTEX_ATTRIB_COUNT - 1);
			MALI_DEBUG_ASSERT(-1 == occupants[pos], ("location %d already assigned to attribute %d", pos, occupants[pos]));

			occupants[pos] = bs_location + j; /* tag stream as in use */
			bs_remap[bs_location + j] = pos;
		}
	}

	return MALI_ERR_NO_ERROR;
}

/*
	Compress the bs_stream mapping.
	The output is a gles_remap table telling how GL streams map onto BS streams

	@param bs_remap - inout parameter. input holds the remap table as GL wants the BS streams to look.
	                  Output is how they look after the compression. Program can be rewritten using these
	@param gl_remap - output parameter. Before compression, GL holds an assumed identity mapping.
	                  After compression, this holds the remap table GL has to use
	@param reverse_gl_remap - Output parameter. Since we're already computing it, no need to recompute later.
	                  Might as well return it here.

*/
MALI_STATIC void compress_streams_on_bs_side(int bs_remap[], int gl_remap[], int reverse_gl_remap[])
{
	int i;
	int reverse_bs_remap[GLES_VERTEX_ATTRIB_COUNT];       /* yes, this is the correct size */

	/* init output and temp storage */
	for (i = 0; i < GLES_VERTEX_ATTRIB_COUNT; ++i)
	{
		gl_remap[i] = -1;         /* clear */
		reverse_bs_remap[i] = -1; /* clear */
	}
	for (i = 0; i < MALIGP2_MAX_VS_INPUT_REGISTERS; ++i)
	{
		reverse_gl_remap[i] = -1;
	}

	/**
	 * This problem contains three domains.
	 * The locations as given by the compiler, the locations as required by GL, and the locations as we want it in the HW.
	 *
	 * We assume there are MALIGP2_MAX_VS_INPUT_REGISTERS streams in the compiler and HW domains.
	 * We assume there are GLES_VERTEX_ATTRIB_COUNT streams in the GL domain. Probably the same number.
	 *
	 * Our input data is given as the compiler to GL remap table, called bs_remap. It specifies how we want to rewrite the shader
	 * if we want to strictly adhere to GL's position requirements.  Reversing the bs_remap table shows us how to get from a
	 * GL location to a compiler location.
	 *
	 * If we look at this inverse remap table, it is basically representing the layout of the streams as seen by GL.
	 * Any cell at -1 is a stream not in use. We want to eliminate the number of unused streams.
	 *
	 * By traversing the reverse_bs_remap table, and re-labelling all streams we stumble over to the first available one,
	 * we basically get a transform from the GL domain to the HW domain. This table is our required output, called the gl_remap.
	 *
	 * Reversing the gl_remap table gives us a transform back from the HW to the GL domain. This is also needed as an output, called reverse_gl_remap.
	 *
	 * And finally, we want to go from the compiler domain to the HW domain. This can be achieved by simply transforming each element in the bs_remap
	 * table with the gl_remap table. This transform will be returned as the bs_remap array.
	 *
	 */

	/* set up GL to compiler remap table. This is basically the inverse of the bs_remap table.   */
	for (i = 0; i < MALIGP2_MAX_VS_INPUT_REGISTERS; ++i)
	{
		if( bs_remap[i] > -1 )
			{
				reverse_bs_remap[bs_remap[i]] = i;
			}
	}

	/* walk through the reverse_bs_remap, relabel streams as we find them */
	{
		int next_stream_id = 0;
		for (i = 0; i < GLES_VERTEX_ATTRIB_COUNT; ++i)
		{
			if( reverse_bs_remap[i] >= 0 )
			{
				 gl_remap[i] = next_stream_id;
				 next_stream_id++;
			}
		}
	}

	/* reverse the gl remap table. This is a required output variable  */
	for (i = 0; i < GLES_VERTEX_ATTRIB_COUNT; ++i)
	{
		if( gl_remap[i] > -1 )
			{
				reverse_gl_remap[gl_remap[i]] = i;
			}
	}

	/* Modify the bs_remap table so that it now yields a transform from the compiler to HW domains */
	for (i = 0; i < MALIGP2_MAX_VS_INPUT_REGISTERS; ++i)
	{
		if( bs_remap[i] > -1 )
			{
				bs_remap[i] = gl_remap[bs_remap[i]];
			}
	}

}

mali_err_code _gles2_link_attributes(gles2_program_object *po)
{
	mali_err_code merr;

	/* a remapping table, mapping attribute indices to locations
	 * This is the one that will be used for rewriting the shader*/
	int bs_remap[MALIGP2_MAX_VS_INPUT_REGISTERS];

	MALI_DEBUG_ASSERT_POINTER(po);
	MALI_DEBUG_ASSERT_POINTER(po->render_state);
	MALI_DEBUG_ASSERT_POINTER(po->render_state->binary.attribute_symbols);

	/* At this stage, the program has just been linked. Attribute streams are set up as given by the compiler.
	 * The function setup_bs_remap_table will generate a remap table to rewrite it to the requirements of
	 * glBindAttribLocation. This operation may alias streams.
	 */
	merr = setup_bs_remap_table(po, bs_remap);
	if (MALI_ERR_NO_ERROR != merr) return merr;

	/* If we rewrote the program at this point, everything should work perfectly. But, the new setup may have a
	 * lot of holes in it. This is not optimal on MaliGP, so we'll try to eliminate the empty streams by moving
	 * up streams on the binary shader side whenever we encounter one. This function will modify the
	 * existing bs_remap, and generate a new gl_remap table which translates how GL sees it into how GP sees it.
	 */
	compress_streams_on_bs_side(bs_remap,
					po->render_state->attribute_remap_table,
					po->render_state->reverse_attribute_remap_table);

	/* finally, we can perform the actual rewrite of the vertex shader. */
	merr = _mali_gp2_link_attribs(&po->render_state->binary, bs_remap, MALI_TRUE);
	if (MALI_ERR_NO_ERROR != merr)
	{
		MALI_DEBUG_ASSERT(MALI_ERR_OUT_OF_MEMORY == merr, ("unexpected error, god 0x%X", merr));
		return MALI_ERR_OUT_OF_MEMORY;
	}

#if HARDWARE_ISSUE_4326
	{
		/* This workaround will double the locations of all streams, but not update the symbol info */
		mali_err_code err = __mali_gp2_rewrite_attributes_for_bug_4326(&po->render_state->binary);
		if(err != MALI_ERR_NO_ERROR) return err;
	}
#endif

	return MALI_ERR_NO_ERROR;
}

