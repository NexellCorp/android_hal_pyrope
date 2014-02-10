/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "mali200/mali200_type.h"
#include "common/essl_mem.h"
#include "common/essl_str.h"
#include "common/essl_common.h"


static unsigned int internal_type_alignment(target_descriptor *desc, const type_specifier *t, address_space_kind kind)
{
	unsigned int alignment = 1;

	if(t->basic_type == TYPE_ARRAY_OF) return internal_type_alignment(desc, t->child_type, kind);
	else if(t->basic_type == TYPE_STRUCT)
	{
		single_declarator *memb;
		alignment = 1;
		for(memb = t->members; memb != 0; memb = memb->next)
		{
			unsigned int member_alignment = internal_type_alignment(desc, memb->type, kind);
			if(member_alignment > alignment) alignment = member_alignment;
		}

	} else if(t->basic_type == TYPE_MATRIX_OF)
	{
		return internal_type_alignment(desc, t->child_type, kind);
	} else {
		alignment = GET_VEC_SIZE(t);
		if(alignment == 3) alignment = 4;
	}
	

	if (desc->options->mali200_store_workaround) {
		/* Workaround for bugzilla 3592 */
		if (kind != ADDRESS_SPACE_UNIFORM && kind != ADDRESS_SPACE_FRAGMENT_VARYING)
		{
			alignment = (alignment + 3) & ~3; /* align every single variable to a vec4 */
		}
	}
	return alignment;
}

unsigned int _essl_mali200_get_type_alignment(target_descriptor *desc, const type_specifier *t, address_space_kind kind)
{
	unsigned int alignment = internal_type_alignment(desc, t, kind);
	return alignment;
}

unsigned int _essl_mali200_get_type_size(target_descriptor *desc, const type_specifier *t, address_space_kind kind)
{
	unsigned int elem_size = 0;

	if(t->basic_type == TYPE_ARRAY_OF) 
	{
		return (unsigned)(t->u.array_size * _essl_mali200_get_array_stride(desc, t->child_type, kind));
	} else if(t->basic_type == TYPE_MATRIX_OF)
	{
		elem_size = _essl_mali200_get_type_size(desc, t->child_type, kind);
		if(elem_size == 3) elem_size = 4; /* pad vec3s */ 
		elem_size *= GET_MATRIX_N_COLUMNS(t);
 	} else if(t->basic_type == TYPE_STRUCT)
	{
		single_declarator *memb;
		elem_size = 0;
		for (memb = t->members; memb != 0; memb = memb->next)
		{
			unsigned alignment = internal_type_alignment(desc, memb->type, kind);
			elem_size = (elem_size + alignment - 1) & ~(alignment - 1);
			elem_size += _essl_mali200_get_type_size(desc, memb->type, kind);
		}
	} else if(t->basic_type == TYPE_SAMPLER_EXTERNAL)
	{
		/* one EXTERNAL sampler expands to 3 regular samplers, handle this */
		elem_size = 3;
	} else {

		elem_size = GET_VEC_SIZE(t);
		
		/* writeable vec3s need the space of a vec4, as mali200 cannot store a vec3 without also writing the last component */
		switch(kind)
		{
		case ADDRESS_SPACE_THREAD_LOCAL:
		case ADDRESS_SPACE_GLOBAL:
		case ADDRESS_SPACE_FRAGMENT_OUT:
			if(elem_size == 3) elem_size = 4; /* pad vec3s */ 
		
			
		case ADDRESS_SPACE_UNIFORM:
		case ADDRESS_SPACE_FRAGMENT_VARYING:
		case ADDRESS_SPACE_FRAGMENT_SPECIAL:
		case ADDRESS_SPACE_REGISTER:
			break;
		case ADDRESS_SPACE_UNKNOWN:
		case ADDRESS_SPACE_ATTRIBUTE:
		case ADDRESS_SPACE_VERTEX_VARYING:
			assert(0);
			break;
		}
	}


	return elem_size;
}


unsigned int _essl_mali200_get_address_multiplier(target_descriptor *desc, const type_specifier *t, address_space_kind kind)
{
	unsigned alignment;
	type_basic bt;
	bt = t->basic_type;
	if(bt == TYPE_SAMPLER_2D 
	   || bt == TYPE_SAMPLER_3D 
	   || bt == TYPE_SAMPLER_CUBE
	   || bt == TYPE_SAMPLER_2D_SHADOW
	   || bt == TYPE_SAMPLER_EXTERNAL)
	{
		return 1; /* special handling */
	}
	alignment = internal_type_alignment(desc, t, kind);
	return (_essl_mali200_get_type_size(desc, t, kind) + alignment-1) & ~(alignment-1);

}

unsigned int _essl_mali200_get_array_stride(target_descriptor *desc, const type_specifier *t, address_space_kind kind)
{
	unsigned stride = _essl_mali200_get_address_multiplier(desc, t, kind);
	if(kind == ADDRESS_SPACE_FRAGMENT_VARYING && t->basic_type == TYPE_ARRAY_OF)
	{
		/* ESSL packing rules dictate array stride a multiple of 4 for
		   varyings and uniforms.  Uniforms we don't care about, as we
		   have unlimited of them. However, varyings need to be packed
		   exactly according to the rules, and so we modify the
		   stride. */
		/* Note: the alignment is not necessary for matrices */
		stride = ((stride+3)/4)*4;
	}
	return stride; 
}

int _essl_mali200_get_type_member_offset(target_descriptor *desc, const single_declarator *sd, address_space_kind kind)
{
	unsigned int cur_offset = 0;
	single_declarator *memb;
	assert(sd->parent_type != NULL && sd->parent_type->basic_type == TYPE_STRUCT);

	for (memb = sd->parent_type->members; memb != 0; memb = memb->next)
	{
		unsigned alignment = internal_type_alignment(desc, memb->type, kind);
		cur_offset = (cur_offset + alignment - 1) & ~(alignment - 1);
		if (memb == sd)
		{
			return (int)cur_offset;
		}
		cur_offset += _essl_mali200_get_type_size(desc, memb->type, kind);
	}

	return -1;
}
