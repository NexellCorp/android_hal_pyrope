/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_osu_misc.c
 * File implements the user side of the OS interface
 */

#define _XOPEN_SOURCE 600
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#elif _POSIX_C_SOURCE < 200112L
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#ifndef HAVE_ANDROID_OS
#include <syscall.h>
#include <sys/syscall.h>
#endif

#include <mali_system.h>
#include "mali_osu.h"

void _mali_osu_cssleep(u32 csec)
{
	while (csec > ((u32)99))
	{
		usleep((u32)990000);
		csec -= (u32)99;
	}

	usleep(csec*10000);
}

void _mali_osu_yield(void)
{
	int status;
	status = sched_yield();
	/* It's non-fatal if this fails. Warn instead */
	MALI_DEBUG_CODE(
					if ( 0 != status)
					{
						MALI_DEBUG_PRINT(1,
										 ("sched_yield failed, ret=%.8X\n", status));
					}
					);

	MALI_IGNORE( status );
}

void _mali_osu_qsort(void * base, u32 num, u32 size, int (*compare)(const void *, const void *))
{
	qsort(base, num, size, compare);
}

void _mali_osu_load_config_strings(void)
{
#ifdef MALI_EXTRA_ENVIRONMENT_FILE
#define MALI_ENV_LINE_LENGTH 4096
	/* Open file with extra environment variables */
	FILE* env_file;

	env_file = fopen(MALI_STRINGIZE_MACRO(MALI_EXTRA_ENVIRONMENT_FILE), "r");
	if (NULL != env_file)
	{
		char cmdline_file_name[32];
		FILE* cmdline_file;

		_mali_sys_snprintf(cmdline_file_name, 32, "/proc/%u/cmdline", (unsigned int)getpid());
		cmdline_file = fopen(cmdline_file_name, "r");
		if (NULL != cmdline_file)
		{
			char *envline = _mali_sys_malloc(sizeof(char) * MALI_ENV_LINE_LENGTH);
			if (NULL != envline)
			{
				char cmdline[512];
				size_t size;

				size = fread(cmdline, 1, 512, cmdline_file);
				cmdline[511] = '\0';

				if (0 < size)
				{
					/*
					 * Each parameter in cmdline is null-terminated. We are only interrested in the first one
					 * (which is typically com.company.app).
					 * Look thorugh each line in the mali extra environment file and see if the first token
					 * matches this.
					 */

					size_t token_size = strlen(cmdline);
					while (NULL != fgets(envline, MALI_ENV_LINE_LENGTH, env_file))
					{
						/* Remove end-of-line character (if found) */
						char* eol_char = strrchr(envline, '\n');
						if (NULL != eol_char)
						{
							*eol_char = '\0';
						}

						if (0 == strncmp(envline, cmdline, token_size) && envline[token_size] == ' ')
						{
							/* Now, lets figure out where the first parameter starts. Look for the '=' first */
							char* begin = strchr(envline, '=');
							if (NULL != begin)
							{
								/* Now that we found the first '=', go backwards untill we find a space */
								while (begin > envline && *begin != ' ')
								{
									begin--;
								}

								if (begin > envline)
								{
									/* We have now found the start of the parameters, now go through them all and put then into the environment */
									const char* space_sep = " ";
									char* token = strtok(begin, space_sep);

									while (NULL != token)
									{
										/* Now we have token, which is name=value, find the '=' and split it */

										char* value = strchr(token, '=');

										if (NULL != value)
										{
											*value = '\0';
											value++;
											_mali_sys_config_string_set(token, value);
										}

										token = strtok(NULL, space_sep);
									}
								}
							}
							break;
						}
					}
				}
				_mali_sys_free(envline);
			}

			fclose(cmdline_file);
		}

		fclose(env_file);
	}
#endif /* MARKER for B&R */
}

const char * _mali_osu_config_string_get(const char * name)
{
	return getenv(name);
}

_mali_osk_errcode_t _mali_osu_config_string_set(const char * name, const char *value)
{
    if (0 == setenv(name, value, 1)) return _MALI_OSK_ERR_OK;
    return _MALI_OSK_ERR_FAULT;
}

void _mali_osu_config_string_release(const char * env_var)
{
	/* No need to do anything here */
	MALI_IGNORE(env_var);
}

void _mali_osu_break(void)
{
	/*
	SIGTRAP will send a signal to the parent process.
	If the parent isn't waiting for the signal we will terminate.
	*/
	kill(0, SIGTRAP);
}

void _mali_osu_abort(void)
{
	*MALI_REINTERPRET_CAST(u32*)0 = 0;
	abort();
}

u64 _mali_osu_get_time_usec(void)
{
	int result;
	struct timeval tod;

	result = gettimeofday(&tod, NULL);

	/* gettimeofday returns non-null on error*/
	if (0 != result) return 0;

	return (MALI_REINTERPRET_CAST(u64)tod.tv_sec) * 1000000ULL + tod.tv_usec;
}

u32 _mali_osu_rand(void)
{
    return rand();
}

u32 _mali_osu_get_pid(void)
{
	return getpid();
}

u32 _mali_osu_get_tid(void)
{
#ifdef HAVE_ANDROID_OS
	return gettid();
#else
	return syscall(SYS_gettid);
#endif
}
