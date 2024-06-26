/* Copyright (C) 1998  Transarc Corporation.  All Rights Reserved.
 *
 */

#include <afs/param.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <windows.h>

#include "pmgtprivate.h"

/* Implements native-signal redirection.  Basically, NT generated signals
 * are caught and passed through to the signal functions implemented by
 * this process management library.
 *
 * Note that signals are passed through by name, since procmgmt.h can't
 * be included here (given that signal(), raise(), etc. are redefined).
 */


/* Program must have FP code to trap SIGFPE; MS suggests the following def. */
static volatile double dummyDouble = 0.0f;


/*
 * NativeSignalHandler() -- handles (redirects) NT-generated signals.
 */
static void __cdecl
NativeSignalHandler(int signo)
{
    const char *signame = NULL;
    int libSigno;

    /* Reinstall signal handler for signo; no reliable signals on NT */
    (void) signal(signo, NativeSignalHandler);

    /* NT defines few signals, and doesn't really generate all of these */
    switch (signo) {
      case SIGINT:
	signame = "SIGINT";
	break;
      case SIGILL:
	signame = "SIGILL";
	break;
      case SIGFPE:
	signame = "SIGFPE";
	break;
      case SIGSEGV:
	signame = "SIGSEGV";
	break;
      case SIGTERM:
	signame = "SIGTERM";
	break;
      case SIGABRT:
	signame = "SIGABRT";
	break;
      default:
	/* unexpect signo value */
	signame = NULL;
	break;
    }

    if (signame != NULL) {
	/* Redirect NT signal into process management library */
	if (pmgt_SignalRaiseLocalByName(signame, &libSigno) == 0 &&
	    signo == SIGABRT) {
	    /* SIGABRT is a special case.  It is generated by NT when abort()
	     * is called.  Upon returning from the signal handler, abort()
	     * will terminate the process with an exit code of 3.  In order
	     * to make an understandable termination status available to the
	     * process management library's waitpid() function, we exit
	     * the process here with a more appropriate exit code.
	     */
	    ExitProcess(PMGT_SIGSTATUS_ENCODE(libSigno));
	}
    }
}


/*
 * pmgt_RedirectNativeSignals() -- initialize native signal redirection.
 */
int
pmgt_RedirectNativeSignals(void)
{
    if (signal(SIGINT, NativeSignalHandler) == SIG_ERR ||
	signal(SIGILL, NativeSignalHandler) == SIG_ERR ||
	signal(SIGFPE, NativeSignalHandler) == SIG_ERR ||
	signal(SIGSEGV, NativeSignalHandler) == SIG_ERR ||
	signal(SIGTERM, NativeSignalHandler) == SIG_ERR ||
	signal(SIGABRT, NativeSignalHandler) == SIG_ERR) {
	errno = EINVAL;
	return -1;
    } else {
	return 0;
    }
}
