/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2007-2010, 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/*
 * Segfault handler with backtrace.
 * Enable ADVANCED_BACKTRACE to get a backtrace that has the correct
 * addresses set.
 * The addresses retrieved by the simple backtrace is relative to
 * the loaded address of the libraries, making it hard to extract the
 * actual function and line number.
 * The advanced version manages to grab the correct addresses.
 * Use addr2line to get the codeline number
 */

#if defined(__linux__) || defined(__arm__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif /* _GNU_SOURCE */
#define __USE_GNU
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>
#include <sys/wait.h>

#define GDB_BACKTRACE 1

/**
 * Will disable a given signal handler (ie. set it to SIG_DFL)
 */
void suite_signal_handler_disable( int signal )
{
	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	sa.sa_flags = 0;
	sa.sa_restorer = NULL;
	sigaction( signal, &sa, NULL );
}

/**
 * Will enable a given signal handler, and use func as callback
 */
void suite_signal_handler_enable( int signal, void (*func)(int) )
{
	struct sigaction sa;
	sa.sa_handler = func;
	sa.sa_flags = 0;
	sa.sa_restorer = NULL;
	sigemptyset(&sa.sa_mask);
	sigaction( signal, &sa, NULL );
}

#ifdef GDB_BACKTRACE

void suite_segfault_handler( int signo )
{
	char cmd[1024];
	pid_t pid;
	int wait_status;

	/* Disable segfault handler early. If we crash in here we crash hard */
	suite_signal_handler_disable( SIGSEGV );

	/* Make sure the previous test output is flushed, for logging purposes */
	(void)fflush( stdout );
	(void)fflush( stderr );

	fputs( "\nSEGFAULT\n", stderr );

	/*
	 * Fork off a new process, and invoke gdb on the parent process in order to output a backtrace
	 */
	pid = getpid();
	if ( 0 == fork() )
	{
		snprintf(cmd, 1024, "gdb -batch -ex 'set confirm off' -ex 'set pagination off' -ex 'set height 0' -ex 'set width 0' -ex 'echo *** BACKTRACE ***\\n' -ex 'thread apply all bt' -ex 'quit' /proc/%i/exe %i 1>&2", pid, pid);
		fprintf(stderr, "Executing \"%s\"\n", cmd);
		system(cmd);
	}
	else
	{
		/* make sure we don't exit until gdb is finished */
		wait(&wait_status);
	}

	(void)fflush( stdout );
	(void)fflush( stderr );

	/* Re-raise the segfault to abort the test-run */
	raise( SIGSEGV );
}

#else

/**
 * A simple segfault handler, using backtrace from libc.
 */
void suite_segfault_handler( int status )
{
	void *bt[30];
	char **strings;
	size_t size;
	size_t i;

	/* Make sure the previous test output is flushed */
	(void)fflush( stdout );
	(void)fflush( stderr );

	size = backtrace( bt, sizeof(bt) / sizeof(void*) );
	strings = backtrace_symbols( bt, size );

	if ( NULL == strings )
	{
		fprintf( stderr, "*** BACKTRACE (not possible due to out of mem) ***\n" );
		goto sigexit;
	}

	fprintf( stderr, "*** BACKTRACE (%zd stack frames found) ***\n", size );

	for ( i=0; i<size; i++ )
	{
		fprintf( stderr, "%s\n", strings[i] );
	}

	free( strings );

sigexit:
	suite_signal_handler_disable( SIGSEGV );
	raise( SIGSEGV );
}
#endif

void suite_enable_backtrace()
{
	suite_signal_handler_enable( SIGSEGV, suite_segfault_handler );
}

#else /* defined(__linux__) || defined(__arm__) */

void suite_enable_backtrace( void )
{
	/* not support on non-linux os-es */

}

#endif
