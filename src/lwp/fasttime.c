
#ifndef lint
#endif

/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/
/*
 *	fasttime.c -- Get the time of day quickly by mapping the kernel's
 *		      time of day variable.
 *
 *	6 January 1986
 *
 *	Modification History
 *	3/21/86:  Added FT_ApproxTime which returns the last time
 * 		  in seconds returned by RT_FastTime.  The intent is to give
 *		  routines which aren't too concerned about the exact time
 *		  fast access to the time, even on kernels without mmap.
 *	4/2/86:   Fixed my previous mod and fixed FT_Init so it doesn't initialize
 *		  a second time if explicitly called after being implicitly called.
 *		  This saves a (precious) file descriptor.
 */

#include <afs/param.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <sys/timeb.h>
#include <winsock2.h>
#else
#include <sys/time.h>
#include <sys/file.h>
#endif
#if !defined(AFS_AIX_ENV) && !defined(AFS_NT40_ENV)
#include <sys/mman.h>
#endif
#include <afs/afsutil.h>

extern char *valloc ();
int ft_debug;

#define TRUE	1
#define FALSE	0

static enum InitState { notTried, tried, done } initState = notTried;

struct timeval FT_LastTime;	/* last time returned by RT_FastTime.  Used to implement
			    	   FT_ApproxTime */


/* Call this to get the memory mapped.  It will return -1 if anything went
   wrong.  In that case, calls to FT_GetTimeOfDay will call gettimeofday
   instead.  If printErrors is true, errors in initialization will cause
   error messages to be printed on stderr.  If notReally is true, then
   things are set up so that all calls to FT_GetTimeOfDay call gettimeofday.
   You might want this if your program won't run too long and the nlist
   call is too expensive.  Yeah, it's pretty horrible.
*/
int FT_Init (int printErrors, int notReally)
{
    if (initState != notTried && !notReally)
	return (initState == done? 0: -1);	/* This is in case explicit initialization
						   occurs after automatic initialization */
    initState = tried;
    if (notReally)
	return 0;		/* fake success, but leave initState
				   wrong. */
    if (printErrors)
	fprintf (stderr, "FT_Init: mmap  not implemented on this kernel\n");
    return (-1);
}

/* Call this to get the time of day.  It will automatically initialize the
   first time you call it.  If you want error messages when you initialize,
   call FT_Init yourself.  If the initialization failed, this will just
   call gettimeofday.  If you ask for the timezone info, this routine will
   punt to gettimeofday. */
int FT_GetTimeOfDay(struct timeval *tv, struct timezone *tz)
{
    register int ret;
    ret = gettimeofday (tv, tz);
    if (!ret) {
	/* need to bounds check 'cause Unix can fail these checks, (esp on Suns)
	    and time package can generate invalid (to select syscall) values
	    for the time until the next interesting event if it encounters
	    out of range microsecond fields */
	if (tv->tv_usec < 0) tv->tv_usec = 0;
	if (tv->tv_usec > 999999) tv->tv_usec = 999999;
	FT_LastTime.tv_sec = tv->tv_sec;
	FT_LastTime.tv_usec = tv->tv_usec;
    }
    return ret;
}


/* For compatibility.  Should go away. */
TM_GetTimeOfDay (tv, tz)
    struct timeval *tv;
    struct timezone *tz;
{
    return FT_GetTimeOfDay(tv, tz);
}

int FT_AGetTimeOfDay(struct timeval *tv, struct timezone *tz)
{
    if (FT_LastTime.tv_sec) {
	tv->tv_sec = FT_LastTime.tv_sec;
	tv->tv_usec = FT_LastTime.tv_usec;
	return 0;
    }
    return FT_GetTimeOfDay(tv, tz);
}

unsigned int FT_ApproxTime(void)
{
    if (!FT_LastTime.tv_sec) {
	FT_GetTimeOfDay(&FT_LastTime, 0);
    }
    return FT_LastTime.tv_sec;
}
