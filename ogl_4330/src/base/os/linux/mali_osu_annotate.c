/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>
#include <stdio.h>

/* Define link to annotation interface for framebuffer dumping */
static FILE *gator_annotate = NULL;

mali_bool _mali_osu_annotate_setup(void)
{
	if (NULL == gator_annotate)
	{
		gator_annotate= fopen("/dev/gator/annotate", "wb");
		if (NULL != gator_annotate)
		{
			/* Set non-buffered IO */
			setvbuf(gator_annotate, (char *)NULL, _IONBF, 0);
		}
	}
	return (NULL == gator_annotate ? MALI_FALSE : MALI_TRUE);
}

void _mali_osu_annotate_write(const void* data, u32 length)
{
	MALI_DEBUG_ASSERT_POINTER(data);

	if ((NULL != gator_annotate) && (0 != length))
	{
		u32 pos = 0;
		u32 nBytesWritten;

		/*
		 * Write the data to the annotation channel, looping as required to completely write all the data
		 * in case the underlying write doesn't complete the first time round.  Note that the fwrite is
		 * actually writing to the annotation stream which can cause fwrite to return early. It should set
		 * errno=EWOULDBLOCK/EAGAIN, but this has proved to be unreliable so we just loop.
		 */
		do
		{
			nBytesWritten = fwrite(&((const char*)data)[pos], 1, length - pos, gator_annotate);
			pos += nBytesWritten;
		} while (pos < length);

		fflush(gator_annotate);
	}
}
