/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef COMMON_RANDOM_GENERATOR_H
#define COMMON_RANDOM_GENERATOR_H

#ifdef RANDOMIZED_COMPILATION

void _essl_set_random_seed(int val);

/** return an integer in the range [lower_bound, upper_bound) */
int _essl_get_random_int(int lower_bound, int upper_bound);



#endif


#endif /* COMMON_RANDOM_GENERATOR_H */
