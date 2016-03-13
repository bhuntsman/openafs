/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*  serverLog.c     - Server logging                                      */
/*                                                                        */
/*  Information Technology Center                                         */
/*  Date: 05/21/97                                                        */
/*                                                                        */
/*  Function    - These routiens implement logging from the servers       */
/*                                                                        */
/* ********************************************************************** */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <afs/procmgmt.h>	/* signal(), kill(), wait(), etc. */

#include <roken.h>		/* Must come after procmgmt.h */
#ifdef AFS_PTHREAD_ENV
 #include <opr/softsig.h>
 #include <afs/procmgmt_softsig.h>	/* Must come after softsig.h */
#endif
#include <afs/opr.h>
#include "afsutil.h"
#include "fileutil.h"
#include <lwp.h>

#if defined(AFS_PTHREAD_ENV)
#include <pthread.h>
static pthread_once_t serverLogOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t serverLogMutex;
#define LOCK_SERVERLOG() opr_Verify(pthread_mutex_lock(&serverLogMutex) == 0)
#define UNLOCK_SERVERLOG() opr_Verify(pthread_mutex_unlock(&serverLogMutex) == 0)

#ifdef AFS_NT40_ENV
#define NULLDEV "NUL"
#else
#define NULLDEV "/dev/null"
#endif

#else /* AFS_PTHREAD_ENV */
#define LOCK_SERVERLOG()
#define UNLOCK_SERVERLOG()
#endif /* AFS_PTHREAD_ENV */

#ifdef AFS_NT40_ENV
#define F_OK 0
#define O_NONBLOCK 0
#endif

static int
dummyThreadNum(void)
{
    return -1;
}
static int (*threadNumProgram) (void) = dummyThreadNum;

static int serverLogFD = -1;

#ifndef AFS_NT40_ENV
int serverLogSyslog = 0;
int serverLogSyslogFacility = LOG_DAEMON;
char *serverLogSyslogTag = 0;
#endif

int LogLevel;
int mrafsStyleLogs = 0;
static int threadIdLogs = 0;
int printLocks = 0;
static int resetSignals = 0;
static char ourName[MAXPATHLEN];

void
SetLogThreadNumProgram(int (*func) (void) )
{
    threadNumProgram = func;
}

void
WriteLogBuffer(char *buf, afs_uint32 len)
{
    LOCK_SERVERLOG();
    if (serverLogFD >= 0) {
	if (write(serverLogFD, buf, len) < 0)
	    ; /* don't care */
    }
    UNLOCK_SERVERLOG();
}

int
LogThreadNum(void)
{
  return (*threadNumProgram) ();
}

void
vFSLog(const char *format, va_list args)
{
    time_t currenttime;
    char tbuffer[1024];
    char *info;
    size_t len;
    struct tm tm;
    int num;

    currenttime = time(NULL);
    len = strftime(tbuffer, sizeof(tbuffer), "%a %b %d %H:%M:%S %Y ",
		   localtime_r(&currenttime, &tm));
    info = &tbuffer[len];

    if (mrafsStyleLogs || threadIdLogs) {
	num = (*threadNumProgram) ();
        if (num > -1) {
	    snprintf(info, (sizeof tbuffer) - strlen(tbuffer), "[%d] ",
		     num);
	    info += strlen(info);
	}
    }

    vsnprintf(info, (sizeof tbuffer) - strlen(tbuffer), format, args);

    len = strlen(tbuffer);
    LOCK_SERVERLOG();
#ifndef AFS_NT40_ENV
    if (serverLogSyslog) {
	syslog(LOG_INFO, "%s", info);
    } else
#endif
    if (serverLogFD >= 0) {
	if (write(serverLogFD, tbuffer, len) < 0)
	    ; /* don't care */
    }
    UNLOCK_SERVERLOG();

#if !defined(AFS_PTHREAD_ENV) && !defined(AFS_NT40_ENV)
    if (!serverLogSyslog) {
	fflush(stdout);
	fflush(stderr);		/* in case they're sharing the same FD */
    }
#endif
}				/*vFSLog */

/* VARARGS1 */
/*@printflike@*/
void
FSLog(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vFSLog(format, args);
    va_end(args);
}				/*FSLog */

void
LogCommandLine(int argc, char **argv, const char *progname,
	       const char *version, const char *logstring,
	       void (*log) (const char *format, ...))
{
    int i, l;
    char *commandLine, *cx;

    opr_Assert(argc > 0);

    for (l = i = 0; i < argc; i++)
	l += strlen(argv[i]) + 1;
    if ((commandLine = malloc(l))) {
	for (cx = commandLine, i = 0; i < argc; i++) {
	    strcpy(cx, argv[i]);
	    cx += strlen(cx);
	    *(cx++) = ' ';
	}
	commandLine[l-1] = '\0';
	(*log)("%s %s %s%s(%s)\n", logstring, progname,
		    version, strlen(version)>0?" ":"", commandLine);
	free(commandLine);
    } else {
	/* What, we're out of memory already!? */
	(*log)("%s %s%s%s\n", logstring,
	      progname, strlen(version)>0?" ":"", version);
    }
}

void
LogDesWarning(void)
{
    /* The blank newlines help this stand out a bit more in the log. */
    ViceLog(0, ("\n"));
    ViceLog(0, ("WARNING: You are using single-DES keys in a KeyFile. Using single-DES\n"));
    ViceLog(0, ("WARNING: long-term keys is considered insecure, and it is strongly\n"));
    ViceLog(0, ("WARNING: recommended that you migrate to stronger encryption. See\n"));
    ViceLog(0, ("WARNING: OPENAFS-SA-2013-003 on http://www.openafs.org/security/\n"));
    ViceLog(0, ("WARNING: for details.\n"));
    ViceLog(0, ("\n"));
}

static void*
DebugOn(void *param)
{
    int loglevel = (intptr_t)param;
    if (loglevel == 0) {
	ViceLog(0, ("Reset Debug levels to 0\n"));
    } else {
	ViceLog(0, ("Set Debug On level = %d\n", loglevel));
    }
    return 0;
}				/*DebugOn */



void
SetDebug_Signal(int signo)
{
    if (LogLevel > 0) {
	LogLevel *= 5;

#if defined(AFS_PTHREAD_ENV)
        if (LogLevel > 1 && threadNumProgram != NULL &&
            threadIdLogs == 0) {
            threadIdLogs = 1;
        }
#endif
    } else {
	LogLevel = 1;

#if defined(AFS_PTHREAD_ENV)
        if (threadIdLogs == 1)
            threadIdLogs = 0;
#endif
    }
    printLocks = 2;
#if defined(AFS_PTHREAD_ENV)
    DebugOn((void *)(intptr_t)LogLevel);
#else /* AFS_PTHREAD_ENV */
    IOMGR_SoftSig(DebugOn, (void *)(intptr_t)LogLevel);
#endif /* AFS_PTHREAD_ENV */

    if (resetSignals) {
	/* When pthreaded softsig handlers are not in use, some platforms
	 * require this signal handler to be set again. */
	(void)signal(signo, SetDebug_Signal);
    }
}				/*SetDebug_Signal */

void
ResetDebug_Signal(int signo)
{
    LogLevel = 0;

    if (printLocks > 0)
	--printLocks;
#if defined(AFS_PTHREAD_ENV)
    DebugOn((void *)(intptr_t)LogLevel);
#else /* AFS_PTHREAD_ENV */
    IOMGR_SoftSig(DebugOn, (void *)(intptr_t)LogLevel);
#endif /* AFS_PTHREAD_ENV */

    if (resetSignals) {
	/* When pthreaded softsig handlers are not in use, some platforms
	 * require this signal handler to be set again. */
	(void)signal(signo, ResetDebug_Signal);
    }
#if defined(AFS_PTHREAD_ENV)
    if (threadIdLogs == 1)
        threadIdLogs = 0;
#endif
    if (mrafsStyleLogs)
	OpenLog((char *)&ourName);
}				/*ResetDebug_Signal */


#ifdef AFS_PTHREAD_ENV
/*!
 * Register pthread-safe signal handlers for server log management.
 *
 * \note opr_softsig_Init() must be called before this function.
 */
void
SetupLogSoftSignals(void)
{
    opr_softsig_Register(SIGHUP, ResetDebug_Signal);
    opr_softsig_Register(SIGTSTP, SetDebug_Signal);
#ifndef AFS_NT40_ENV
    (void)signal(SIGPIPE, SIG_IGN);
#endif
}
#endif /* AFS_PTHREAD_ENV */

/*!
 * Register signal handlers for server log management.
 *
 * \note This function is deprecated and should not be used
 *       in new code. This function should be removed when
 *       all the servers have been converted to pthreads
 *       and lwp has been removed.
 */
void
SetupLogSignals(void)
{
    resetSignals = 1;
    (void)signal(SIGHUP, ResetDebug_Signal);
    /* Note that we cannot use SIGUSR1 -- Linux stole it for pthreads! */
    (void)signal(SIGTSTP, SetDebug_Signal);
#ifndef AFS_NT40_ENV
    (void)signal(SIGPIPE, SIG_IGN);
#endif
}

#if defined(AFS_PTHREAD_ENV)
static void
InitServerLogMutex(void)
{
    opr_Verify(pthread_mutex_init(&serverLogMutex, NULL) == 0);
}
#endif /* AFS_PTHREAD_ENV */

int
OpenLog(const char *fileName)
{
    /*
     * This function should allow various libraries that inconsistently
     * use stdout/stderr to all go to the same place
     */
    int tempfd, isfifo = 0;
    char oldName[MAXPATHLEN];
    struct timeval Start;
    struct tm *TimeFields;
    char FileName[MAXPATHLEN];

#ifndef AFS_NT40_ENV
    struct stat statbuf;
#endif

#if defined(AFS_PTHREAD_ENV)
    opr_Verify(pthread_once(&serverLogOnce, InitServerLogMutex) == 0);
#endif /* AFS_PTHREAD_ENV */

#ifndef AFS_NT40_ENV
    if (serverLogSyslog) {
	openlog(serverLogSyslogTag, LOG_PID, serverLogSyslogFacility);
	return (0);
    }

    /* Support named pipes as logs by not rotating them */
    if ((lstat(fileName, &statbuf) == 0)  && (S_ISFIFO(statbuf.st_mode))) {
	isfifo = 1;
    }
#endif

    if (mrafsStyleLogs) {
        time_t t;
	struct stat buf;
	gettimeofday(&Start, NULL);
        t = Start.tv_sec;
	TimeFields = localtime(&t);
	if (fileName) {
	    if (strncmp(fileName, (char *)&ourName, strlen(fileName)))
		strcpy((char *)&ourName, (char *)fileName);
	}
    makefilename:
	snprintf(FileName, MAXPATHLEN, "%s.%d%02d%02d%02d%02d%02d",
		 ourName, TimeFields->tm_year + 1900,
		 TimeFields->tm_mon + 1, TimeFields->tm_mday,
		 TimeFields->tm_hour, TimeFields->tm_min,
		 TimeFields->tm_sec);
	if(lstat(FileName, &buf) == 0) {
	    /* avoid clobbering a log */
	    TimeFields->tm_sec++;
	    goto makefilename;
	}
	if (!isfifo)
	    rk_rename(fileName, FileName);	/* don't check error code */
    } else {
	strcpy(oldName, fileName);
	strcat(oldName, ".old");

	/* don't check error */
	if (!isfifo)
	    rk_rename(fileName, oldName);
    }

    tempfd = open(fileName, O_WRONLY | O_TRUNC | O_CREAT | O_APPEND | (isfifo?O_NONBLOCK:0), 0666);
    if (tempfd < 0) {
	printf("Unable to open log file %s\n", fileName);
	return -1;
    }
    /* redirect stdout and stderr so random printf's don't write to data */
    if (freopen(fileName, "a", stdout) == NULL)
	; /* don't care */
    if (freopen(fileName, "a", stderr) != NULL) {
#ifdef HAVE_SETVBUF
	setvbuf(stderr, NULL, _IONBF, 0);
#else
	setbuf(stderr, NULL);
#endif
    }

    serverLogFD = tempfd;

    return 0;
}				/*OpenLog */

int
ReOpenLog(const char *fileName)
{
    int isfifo = 0;
#if !defined(AFS_NT40_ENV)
    struct stat statbuf;
#endif

    if (access(fileName, F_OK) == 0)
	return 0;		/* exists, no need to reopen. */

#if !defined(AFS_NT40_ENV)
    if (serverLogSyslog) {
	return 0;
    }

    /* Support named pipes as logs by not rotating them */
    if ((lstat(fileName, &statbuf) == 0)  && (S_ISFIFO(statbuf.st_mode))) {
	isfifo = 1;
    }
#endif

    LOCK_SERVERLOG();
    if (serverLogFD >= 0)
	close(serverLogFD);
    serverLogFD = open(fileName, O_WRONLY | O_APPEND | O_CREAT | (isfifo?O_NONBLOCK:0), 0666);
    if (serverLogFD >= 0) {
	if (freopen(fileName, "a", stdout) == NULL)
	    ; /* don't care */
	if (freopen(fileName, "a", stderr) != NULL) {
#ifdef HAVE_SETVBUF
	    setvbuf(stderr, NULL, _IONBF, 0);
#else
	    setbuf(stderr, NULL);
#endif
	}

    }
    UNLOCK_SERVERLOG();
    return serverLogFD < 0 ? -1 : 0;
}

/*!
 * Close the server log file.
 *
 * \note Must be preceeded by OpenLog().
 */
void
CloseLog(void)
{
    LOCK_SERVERLOG();
#ifndef AFS_NT40_ENV
    if (serverLogSyslog) {
	closelog();
    } else
#endif
    if (serverLogFD >= 0) {
	close(serverLogFD);
	serverLogFD = -1;
    }
    UNLOCK_SERVERLOG();
}
