/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_ESSL_RELOCATION_H
#define COMMON_ESSL_RELOCATION_H

#include "common/essl_list.h"

struct symbol;
struct basic_block;
struct output_buffer;

typedef enum
{
	/* generic relocations */
	REL_MIDGARD_ABS8  = 1, /**< 8-bit absolute relocation at the given byte address */
	REL_MIDGARD_ABS16 = 2, /**< 16-bit absolute relocation at the given byte address */
	REL_MIDGARD_ABS32 = 3, /**< 32-bit absolute relocation at the given byte address */
	REL_MIDGARD_ABS64 = 4, /**< 64-bit absolute relocation at the given byte address */

	/* load/store specific relocations */
	REL_MIDGARD_ATTR_INDEX_LS0 = 5, /**< absolute attribute index for load store instruction 0, bit offset  8+51, 9 bits */
	REL_MIDGARD_ATTR_INDEX_LS1 = 6, /**< absolute attribute index for load store instruction 0, bit offset 68+51, 9 bits */
	REL_MIDGARD_UNIFORM_INDEX_LS0 = 7, /**< absolute uniform index for load store instruction 0, bit offset  8+44, 17 bits. Address to use is uniform address & 0xFFFF. */
	REL_MIDGARD_UNIFORM_INDEX_LS1 = 8, /**< absolute uniform index for load store instruction 1, bit offset  8+44, 17 bits. Address to use is uniform address & 0xFFFF. */
	REL_MIDGARD_UNIFORM_BUFFER_INDEX_LS0 =  9, /**< absolute uniform buffer index for load store instruction 0, bit offset  8+25,  8 bits. Address to use is uniform address >> 16. */
	REL_MIDGARD_UNIFORM_BUFFER_INDEX_LS1 = 10, /**< absolute uniform buffer index for load store instruction 1, bit offset  8+25,  8 bits. Address to use is uniform address >> 16. */
	REL_MIDGARD_GENERAL_LOAD_DISPLACEMENT_LS0    = 11, /**< absolute relocation for general purpose load displacement, load store instruction 0, bit offset  8+42, 18 bits. */
	REL_MIDGARD_GENERAL_LOAD_DISPLACEMENT_LS1    = 12, /**< absolute relocation for general purpose load displacement, load store instruction 1, bit offset 68+42, 18 bits. */

	/* texturing specifc relocations */
	REL_MIDGARD_IMAGE_SAMPLER_IDX = 13, /**< 16-bit absolute relocation at the given byte address. . Address to use is (address & 0xFFFF) >> 2  */

	/* branch specific relocations */
	REL_MIDGARD_PC_INSTR_TYPE = 14, /**< absolute relocation for instruction type of a branch target, bit offset 3, 4 bits, source used is destination pc % 16 */

	/** PC-relative relocations. Address to use is (dest_pc - (section_address + bytes_from_start_of_section + extra_byte_offset))/16 */

	REL_MIDGARD_PC_BRANCH_LONG_UNCONDITIONAL = 15, /**< pc-relative relocation for branch target, bit offset 9, 39 bits. */
	REL_MIDGARD_PC_BRANCH_LONG_CONDITIONAL = 16, /**< pc-relative relocation for branch target, bit offset 9, 23 bits. */
	REL_MIDGARD_PC_BRANCH_SHORT_UNCONDITIONAL = 17, /**< pc-relative relocation for branch target, bit offset 9, 7 bits. */
	REL_MIDGARD_PC_BRANCH_SHORT_CONDITIONAL = 18 /**< pc-relative relocation for branch target, bit offset 7, 7 bits. */

} relocation_kind;


typedef struct relocation
{
	struct relocation *next;

	relocation_kind kind; /**< what type of relocation */
	unsigned int section; /**< which section to write the relocation */
	unsigned int bytes_from_start_of_section; /**< where to write the relocation, subject to the bit offsets from the table above */
	int extra_byte_offset; /**< for pc-relative branch, the additional number of bytes from bytes_from_start_of_section to the end of the instruction word, so we can calculate PC-relative addresses correctly */
	struct _tag_symbol *sym; /**< which symbol to rewrite with */
	struct _tag_basic_block *block; /**< If function-local branch (sym == NULL), this holds the basic block we're branching to */
} relocation;

ASSUME_ALIASING(generic_list, relocation);


void _essl_apply_relocation(relocation *reloc, long long address_difference, struct output_buffer *buf, unsigned section_buffer_locations[], long long section_load_locations[]);
void _essl_apply_relocation_list(relocation *reloc, long long address_difference, struct output_buffer *buf, unsigned section_buffer_locations[], long long section_load_locations[]);

#endif /* COMMON_ESSL_RELOCATION_H */

