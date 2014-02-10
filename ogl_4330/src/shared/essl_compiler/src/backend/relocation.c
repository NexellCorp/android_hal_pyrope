/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "backend/relocation.h"
#include "common/output_buffer.h"

void normalize_address(unsigned *word_pos, unsigned *bit_pos)
{
	while(*bit_pos >= 32)
	{
		++*word_pos;
		*bit_pos -= 32;
	}

}

unsigned long long read_location(struct output_buffer *buf, unsigned word_pos, unsigned bit_offset, unsigned n_bits)
{
	unsigned long long res = 0;
	unsigned res_shift = 0;
	assert(n_bits <= 64);
	while(n_bits > 0)
	{
		unsigned n_bits_to_read = n_bits;
		if(n_bits_to_read > 32-bit_offset) n_bits_to_read = 32-bit_offset;
		
		res |= _essl_output_buffer_retrieve_bits(buf, word_pos, bit_offset, n_bits_to_read) << res_shift;
		
		++word_pos;
		bit_offset = 0;
		n_bits -= n_bits_to_read;
		res_shift += n_bits_to_read;
	}
	return res;
}


void write_location(struct output_buffer *buf, unsigned word_pos, unsigned bit_offset, unsigned n_bits, unsigned long long data)
{
	while(n_bits > 0)
	{
		unsigned n_bits_to_write = n_bits;
		if(n_bits_to_write > 32-bit_offset) n_bits_to_write = 32-bit_offset;
		
		_essl_output_buffer_replace_bits(buf, word_pos, bit_offset, n_bits_to_write, data);
		
		++word_pos;
		bit_offset = 0;
		n_bits -= n_bits_to_write;
		data >>= n_bits_to_write;

	}
}


void _essl_apply_relocation(relocation *reloc, long long address_difference, struct output_buffer *buf, unsigned section_buffer_locations[], long long section_load_locations[])
{
	long long address_to_apply = address_difference;
	unsigned bit_offset = 0;
	long long end_of_word = section_load_locations[reloc->section] + reloc->bytes_from_start_of_section + reloc->extra_byte_offset;
	int n_bits = -1;
	unsigned byte_pos = 0;
	unsigned word_pos = 0;

	long long data = 0;

	switch(reloc->kind)
	{
	case REL_MIDGARD_ABS8:
		n_bits = 8;
		break;
	case REL_MIDGARD_ABS16:
		n_bits = 16;
		break;
	case REL_MIDGARD_ABS32:
		n_bits = 32;
		break;
	case REL_MIDGARD_ABS64:
		n_bits = 64;
		break;

	case REL_MIDGARD_ATTR_INDEX_LS0:
		bit_offset =  8+51;
		n_bits = 9;
		break;
	case REL_MIDGARD_ATTR_INDEX_LS1:
		bit_offset = 68+51;
		n_bits = 9;
		break;

	case REL_MIDGARD_UNIFORM_INDEX_LS0:
		bit_offset =  8+44;
		n_bits = 17;
		address_to_apply = address_difference & 0xFFFF;
		break;
	case REL_MIDGARD_UNIFORM_INDEX_LS1:
		bit_offset = 68+44;
		n_bits = 17;
		address_to_apply = address_difference & 0xFFFF;
		break;

	case REL_MIDGARD_UNIFORM_BUFFER_INDEX_LS0:
		bit_offset =  8+25;
		n_bits = 8;
		address_to_apply = address_difference >> 16;
		break;
	case REL_MIDGARD_UNIFORM_BUFFER_INDEX_LS1:
		bit_offset = 68+25;
		n_bits = 8;
		address_to_apply = address_difference >> 16;
		break;

	case REL_MIDGARD_GENERAL_LOAD_DISPLACEMENT_LS0:
		bit_offset =  8+42;
		n_bits = 18;
		break;
	case REL_MIDGARD_GENERAL_LOAD_DISPLACEMENT_LS1:
		bit_offset = 68+42;
		n_bits = 18;
		break;

	case REL_MIDGARD_IMAGE_SAMPLER_IDX:
		n_bits = 16;
		address_to_apply = (address_difference & 0xFFFF) >> 2;
		break;
		
	case REL_MIDGARD_PC_INSTR_TYPE:
		bit_offset = 32;
		n_bits = 4;
		address_to_apply = address_difference & 0xF;
		break;
	
	case REL_MIDGARD_PC_BRANCH_LONG_UNCONDITIONAL:
		bit_offset = 9;
		n_bits = 39;
		address_to_apply = (address_difference - end_of_word) >> 4;
		break;

	case REL_MIDGARD_PC_BRANCH_LONG_CONDITIONAL:
		bit_offset = 9;
		n_bits = 23;
		address_to_apply = (address_difference - end_of_word) >> 4;
		break;
	case REL_MIDGARD_PC_BRANCH_SHORT_UNCONDITIONAL:
		bit_offset = 9;
		n_bits = 7;
		address_to_apply = (address_difference - end_of_word) >> 4;
		break;
	case REL_MIDGARD_PC_BRANCH_SHORT_CONDITIONAL:
		bit_offset = 7;
		n_bits = 7;
		address_to_apply = (address_difference - end_of_word) >> 4;
		break;
	

	}
	assert(n_bits != -1);
	
	byte_pos = section_buffer_locations[reloc->section] + reloc->bytes_from_start_of_section;

	word_pos = byte_pos >> 2;
	bit_offset += (byte_pos&3)*8;
	
	normalize_address(&word_pos, &bit_offset);

	
	data = read_location(buf, word_pos, bit_offset, n_bits);


	data += address_to_apply;
	
	write_location(buf, word_pos, bit_offset, n_bits, data);
}


void _essl_apply_relocation_list(relocation *reloc, long long address_difference, struct output_buffer *buf, unsigned section_buffer_locations[], long long section_load_locations[])
{
	for( ; reloc != NULL; reloc = reloc->next)
	{
		_essl_apply_relocation(reloc, address_difference, buf, section_buffer_locations, section_load_locations);
	}

}
