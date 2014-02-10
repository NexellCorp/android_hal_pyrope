/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#define __USE_BSD 1

#include <mali_system.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "../../base_arch_timer.h"

u32 *_mali_sys_timer;

/* Address and size of 24MHz timer on RealView devboards */

#define REALVIEW_SYS_24MHZ_TIMER_OFFSET 0x5c

#define REALVIEW_SYS_BASE 0x10000000

static const u32 mem_length = 4;

static int _fd = -1;
static u32 zero = 0;

void arch_init_timer(void)
{
	u8 *mapped_sys_base;

	_fd = open("/dev/mem", O_RDONLY|O_SYNC);
	MALI_CHECK_GOTO(-1 != _fd, fail);

	mapped_sys_base = mmap(NULL, REALVIEW_SYS_24MHZ_TIMER_OFFSET + mem_length, PROT_READ, MAP_SHARED, _fd, REALVIEW_SYS_BASE);
	MALI_CHECK_GOTO(mapped_sys_base != MAP_FAILED, fail);

	_mali_sys_timer = MALI_REINTERPRET_CAST(u32*)(mapped_sys_base + REALVIEW_SYS_24MHZ_TIMER_OFFSET);

	return;

fail:
	MALI_DEBUG_PRINT(0, ("Failed to open /dev/mem for timer reads, timer will always return 0"));
	_mali_sys_timer = &zero;
}

void arch_cleanup_timer(void)
{
	if (_mali_sys_timer != NULL && _mali_sys_timer != &zero)
	{
		munmap(MALI_REINTERPRET_CAST(void*)REALVIEW_SYS_BASE, REALVIEW_SYS_24MHZ_TIMER_OFFSET + mem_length);
	}

	if (_fd == -1) return;

	close(_fd);
}

