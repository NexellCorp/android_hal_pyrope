/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef MALIGP2_SLOT_H
#define MALIGP2_SLOT_H

#include "common/essl_system.h"
#include "maligp2/maligp2_scheduler.h"

typedef enum
{
	MALIGP2_RESERVATION_DELETE,
	MALIGP2_RESERVATION_FULFILL,
	MALIGP2_RESERVATION_KEEP
} reservation_style;

typedef enum
{
	MALIGP2_DONT_PERFORM_CSE,
	MALIGP2_PERFORM_CSE
} cse_enable;


essl_bool _essl_maligp2_allocate_slots(control_flow_graph *cfg, target_descriptor *desc, maligp2_instruction_word *word, int earliest_use_subcycle, essl_bool same_cycle, int *slots, node **transfer_nodes, reservation_style res_style, cse_enable cse_enabled, int *address, va_list arglist);
maligp2_schedule_classes _essl_maligp2_allocate_move(control_flow_graph *cfg, target_descriptor *desc, maligp2_instruction_word *word,
													 reservation_style res_style, maligp2_opcode opcode, node *src, node *dst);
essl_bool _essl_maligp2_reserve_move(control_flow_graph *cfg, target_descriptor *desc, maligp2_instruction_word *word, node *n);

void _essl_maligp2_demote_misc_move_reservation(control_flow_graph *cfg, target_descriptor *desc, maligp2_instruction_word *word);


#endif /* MALIGP2_SLOT_H */
