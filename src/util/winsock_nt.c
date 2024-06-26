/* Copyright (C)  1998  Transarc Corporation.  All rights reserved. */

/* winsock.c contains winsock related routines. */

#include <afs/param.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <sys/timeb.h>
#include <afs/afsutil.h>

/* afs_wsInit - LWP version
 * Initialize the winsock 2 environment.
 * 
 * Returns 0 on success, -1 on error.
 */
int afs_winsockInit(void)
{
    static int once = 1;

    if (once) {
	int code ;
	WSADATA data;
	WORD sockVersion;

	once = 0;

	sockVersion = 2;
	code = WSAStartup(sockVersion, &data);
	if (code)
	    return -1;

	if (data.wVersion != 2)
	    return -1;
    }
    return 0;
}

int afs_gettimeofday(struct timeval *tv, struct timezone *tz)
{
    struct _timeb myTime;

    _ftime(&myTime);
    tv->tv_sec = myTime.time;
    tv->tv_usec = myTime.millitm * 1000;
    if (tz) {
	tz->tz_minuteswest = myTime.timezone;
	tz->tz_dsttime = myTime.dstflag;
    }
    return 0;
}
#endif
