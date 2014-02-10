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
#include "common/essl_profiling.h"

#ifdef TIME_PROFILING

#define MAX_TIME_PROFILES 100

struct TimeProfDat {
	char *name;
	double last_start_time;
	double total_time;
} time_profile_data[MAX_TIME_PROFILES];

int next_profdat;

/*#define GET_TIME (clock()/(double)CLOCKS_PER_SEC)*/
#define GET_TIME timeofday()

static double timeofday() {
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + tv.tv_usec/(double)1000000;
}

void _essl_time_profile_init(void) {
	int i;
	for (i = 0 ; i < MAX_TIME_PROFILES ; i++) {
		time_profile_data[i].name = 0;
		time_profile_data[i].last_start_time = 0;
		time_profile_data[i].total_time = 0;
	}
	next_profdat = 0;
}

struct TimeProfDat *find_prof(char *name) {
	struct TimeProfDat *prof;
	int i;
	for (i = next_profdat-1 ; i >= 0 ; i--) {
		if (strcmp(name, time_profile_data[i].name) == 0) {
			return &time_profile_data[i];
		}
	}
	prof = &time_profile_data[next_profdat];
	if (next_profdat < MAX_TIME_PROFILES-1) {
		prof->name = name;
		next_profdat++;
	}
	return prof;
}

void _essl_time_profile_start(char *s) {
	struct TimeProfDat *prof = find_prof(s);
	double time = GET_TIME;
	prof->last_start_time = time;
}

void _essl_time_profile_stop(char *s) {
	double time = GET_TIME;
	struct TimeProfDat *prof = find_prof(s);
	prof->total_time += time - prof->last_start_time;
}

void _essl_time_profile_dump(FILE *f) {
	int i;
	for (i = 0 ; i < next_profdat ; i++) {
		fprintf(f, "%s %f\n", time_profile_data[i].name, time_profile_data[i].total_time);
	}
	if (time_profile_data[MAX_TIME_PROFILES-1].total_time != 0) {
		fprintf(f, "rest %f\n", time_profile_data[MAX_TIME_PROFILES-1].total_time);
	}
}
#else
int _essl_dummy; /* can't have an empty translation unit */
#endif
