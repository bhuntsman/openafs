/* Copyright (C) 1995, 1989, 1998 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/*
 * afs_mariner.c - fetch/store monitoring facility.
 */
/*
 * Implements:
 */
#include "../afs/param.h"       /*Should be always first*/
#include "../afs/sysincludes.h" /*Standard vendor system headers*/
#include "../afs/afsincludes.h" /*AFS-based standard headers*/
#include "../afs/afs_stats.h"  /* statistics */

/* Exported variables */
struct rx_service *afs_server;


#define	SMAR	    20			/* size of a mariner name */
#define	NMAR	    10			/* number of mariner names */
static char marinerNames[NMAR][SMAR];
static struct vcache *marinerVCs[NMAR];
static int marinerPtr = 0;	/* pointer to next mariner slot to use */

/* Exported variables */
afs_int32 afs_mariner = 0;
afs_int32 afs_marinerHost = 0;

afs_AddMarinerName(aname, avc)
    register char *aname;
    register struct vcache *avc; {
    register int i;
    register char *tp;

    AFS_STATCNT(afs_AddMarinerName);
    i = marinerPtr++;
    if (i >= NMAR) {
	i = 0;
	marinerPtr = 1;
    }
    tp = marinerNames[i];
    strncpy(tp, aname, SMAR);
    tp[SMAR-1] = 0;
    marinerVCs[i] = avc;
    return 0;
}

char *afs_GetMariner(avc)
    register struct vcache *avc; {
    register int i;
    AFS_STATCNT(afs_GetMariner);
    for(i=0; i<NMAR; i++) {
	if (marinerVCs[i] == avc) {
	    return marinerNames[i];
	}
    }
    return "a file";
}

void afs_MarinerLogFetch(avc, off, bytes, idx)
    register struct vcache *avc;
    register afs_int32 off, bytes, idx;
{
    struct sockaddr_in taddr;
    register char *tp, *tp1, *tp2;
    struct iovec dvec;
    int len;


    AFS_STATCNT(afs_MarinerLog);
    taddr.sin_family = AF_INET;
    taddr.sin_addr.s_addr = afs_marinerHost;
    taddr.sin_port = htons(2106);
#ifdef  AFS_OSF_ENV
    taddr.sin_len = sizeof(taddr);
#endif  /* AFS_OSF_ENV */
    tp = tp1 = (char *) osi_AllocSmallSpace(AFS_SMALLOCSIZ);
    strcpy(tp, "fetch$Fetching ");
    tp += 15; /* change it if string changes */
    tp2 = afs_GetMariner(avc);
    strcpy(tp, tp2);
    tp += strlen(tp2);
    *tp++ = '\n';
    /* note, console doesn't want a terminating null */
    len = strlen(tp1) - 1;
    /* I don't care if mariner packets fail to be sent */
    dvec.iov_base = tp1;
    dvec.iov_len = len;
    AFS_GUNLOCK();
    (void) osi_NetSend(afs_server->socket, &taddr, &dvec, 1, len, 0);
    AFS_GLOCK();
    osi_FreeSmallSpace(tp1);
} /*afs_MarinerLogFetch*/

void afs_MarinerLog(astring, avc)
    register struct vcache *avc;
    register char *astring;
{
    struct sockaddr_in taddr;
    register char *tp, *tp1, *buf;
    struct iovec dvec;

    AFS_STATCNT(afs_MarinerLog);
    taddr.sin_family = AF_INET;
    taddr.sin_addr.s_addr = afs_marinerHost;
    taddr.sin_port = htons(2106);
    tp = buf = (char *) osi_AllocSmallSpace(AFS_SMALLOCSIZ);

    strcpy(tp, astring);
    tp += strlen(astring);
    *tp++ = ' ';
    tp1 = afs_GetMariner(avc);
    strcpy(tp, tp1);
    tp += strlen(tp1);
    *tp++ = '\n';
    /* note, console doesn't want a terminating null */
    /* I don't care if mariner packets fail to be sent */
    dvec.iov_base = buf;
    dvec.iov_len = tp-buf;
    AFS_GUNLOCK();
    (void) osi_NetSend(afs_server->socket, &taddr, &dvec, 1, tp-buf, 0);
    AFS_GLOCK();
    osi_FreeSmallSpace(buf);
} /*afs_MarinerLog*/

void shutdown_mariner(void)
{
    int i;

    marinerPtr = 0;
    afs_mariner = 0;

    for (i=0; i<NMAR; i++)
	marinerVCs[i] = 0;
}


























