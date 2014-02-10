/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_SCALAR_H
#define COMMON_SCALAR_H


/** This union contains all possible scalar types for the enabled targets.
  @note If you add another target, please add its native data types to this union.
*/
typedef union _tag_scalar_type {
	/** Mali 200 data type */
	float mali200_float;

	/** Mali GP data type */
	float maligp_float;

	/** Interpreter data type - integer kind */
	int interpreter_int;
	/** Interpreter data type - float kind */
	float interpreter_float;

} scalar_type;







#endif /* COMMON_SCALAR_H */
