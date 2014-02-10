/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "common/essl_system.h"
#include "common/essl_common.h"

/* NOTE: Does not handle overflows */
/** Converts a string into an integer using the specified base.
    If 'base' is the special value 0, the base is automatically detected based on a 0/0x prefix.
*/
int _essl_string_to_integer(const string instr, int base, /*@out@*/ int *retval)
{
	int curval;
	unsigned int i;
	char c;
	int charval;
	string str = instr;
	assert(str.ptr != 0);
	*retval = 0;
	if (base == 0)
	{
		/* autodetect base */
		if (str.len >= 2 && str.ptr[0] == '0')
		{
			if (str.len >= 3 && str.ptr[1] == 'x')
			{
				/* hex */
				string newstr;
				newstr.ptr = instr.ptr + 2;
				newstr.len = instr.len - 2;
				str = newstr;
				base = 16;
			} else {
				/* oct */
				string newstr;
				newstr.ptr = instr.ptr + 1;
				newstr.len = instr.len - 1;
				str = newstr;
				base = 8;
			}
		} else {
			/* dec */
			base = 10;
		}
	}

	curval = 0;
	assert(str.ptr != 0);
	for (i = 0; i < (unsigned)str.len; ++i)
	{
		/* Assumes continuous 0-9, a-f and A-F in charset */
		c = str.ptr[i];
		if (c >= '0' && c <= '9')
		{
			charval = c - '0';
		}
		else if (c >= 'a' && c <= 'f')
		{
			charval = c - 'a' + 10;
		}
		else if (c >= 'A' && c <= 'F')
		{
			charval = c - 'A' + 10;
		}
		else
		{
			/* Unknown char */
			return 0;
		}

		if (charval >= base)
		{
			/* Base error */
			return 0;
		}

		curval = curval * base + charval;
	}
	*retval = curval;
	return 1;
}
