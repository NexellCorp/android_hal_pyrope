/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_osu_file.c
 * File implements the user side of the OS interface
 */

#include <mali_system.h>
#include "mali_osu.h"

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

typedef enum
{
    _MALI_FILE_TYPE_NORMAL,
    _MALI_FILE_TYPE_STDOUT,
    _MALI_FILE_TYPE_STDERR,
} _mali_file_type_t;

/* Private declaration of the OSU file type */
struct _mali_file_t_struct
{
    FILE* file;
    _mali_file_type_t type;
};

static _mali_file_t MALI_STDOUT_STRUCT = { NULL, _MALI_FILE_TYPE_STDOUT };
static _mali_file_t MALI_STDERR_STRUCT = { NULL, _MALI_FILE_TYPE_STDERR };

_mali_file_t *MALI_STDOUT = &MALI_STDOUT_STRUCT;
_mali_file_t *MALI_STDERR = &MALI_STDERR_STRUCT;

static FILE *_mali_map_file_type(_mali_file_t *file);

_mali_file_t * _mali_osu_fopen(const char * path, _mali_file_mode_t mode)
{
    const char *file_mode_mapping[6] = { "r", "w", "a", "rb", "wb", "ab" };
    _mali_file_t *mali_file;

    MALI_DEBUG_ASSERT_POINTER( path );
    MALI_DEBUG_ASSERT_RANGE( (int)mode, 0, (int)((sizeof(file_mode_mapping) / sizeof(file_mode_mapping[0])) - 1) );

    mali_file = _mali_osu_malloc(sizeof(_mali_file_t));
    if (NULL == mali_file) return NULL;

    mali_file->file = fopen(path, file_mode_mapping[mode]);
    if (NULL == mali_file->file)
    {
        _mali_osu_free(mali_file);
        return NULL;
    }

    mali_file->type = _MALI_FILE_TYPE_NORMAL;
    return mali_file;
}

void _mali_osu_fclose(_mali_file_t * file)
{
	MALI_DEBUG_ASSERT_POINTER( file );
	MALI_DEBUG_ASSERT( file->type == _MALI_FILE_TYPE_NORMAL, ("_mali_osu_fclose called on stdout or stderr file!") );
    fclose(file->file);
    free(file);
}

int _mali_osu_remove(const char *filename)
{
	MALI_DEBUG_ASSERT_POINTER( filename );
    return remove(filename);
}

int _mali_osu_fwrite(const void *ptr, u32 element_size, u32 num_elements, _mali_file_t * file)
{
    int elements_written_successfully;
    FILE *fp;
    MALI_DEBUG_ASSERT_POINTER( file );
    MALI_DEBUG_ASSERT_POINTER( ptr );

    fp = _mali_map_file_type(file);
    elements_written_successfully = fwrite(ptr, element_size, num_elements, fp);

    /* flush stdout and stderr, not file based operations for efficiency */
    if (file->type != _MALI_FILE_TYPE_NORMAL)
    {
        fflush(file->file);
    }
    return elements_written_successfully;
}

int _mali_osu_fread(void *ptr, u32 element_size, u32 num_elements, _mali_file_t * file)
{
    FILE *fp;
	int elements_read_successfully;
    MALI_DEBUG_ASSERT_POINTER( file );
    MALI_DEBUG_ASSERT_POINTER( ptr );

    fp = _mali_map_file_type(file);
    elements_read_successfully = fread(ptr, element_size, num_elements, fp);
    return elements_read_successfully;
}

int _mali_osu_feof(_mali_file_t *file)
{
	FILE *fp;
    MALI_DEBUG_ASSERT_POINTER( file );

    fp = _mali_map_file_type(file);
	return feof(fp);
}

_mali_file_t *_mali_osu_stderr(void)
{
    return MALI_STDOUT;
}

_mali_file_t *_mali_osu_stdout(void)
{
    return MALI_STDERR;
}

static FILE *_mali_map_file_type(_mali_file_t *file)
{
    switch(file->type)
    {
        case _MALI_FILE_TYPE_NORMAL: return file->file;
        case _MALI_FILE_TYPE_STDOUT:
            MALI_DEBUG_ASSERT( file->file == NULL, ("Invalid stdout file descriptor"));
            return stdout;
        case _MALI_FILE_TYPE_STDERR:
            MALI_DEBUG_ASSERT( file->file == NULL, ("Invalid stderr file descriptor"));
            return stderr;
        default:
            MALI_DEBUG_ASSERT( MALI_TRUE, ("Invalid file type 0x%x in file descriptor 0x%x.", file->type, file));
            return stderr;
   }
}
