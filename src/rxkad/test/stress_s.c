/* Copyright (C) 1991 Transarc Corporation - All rights reserved */

/* RX Authentication Stress test: server side code. */

#include <afs/param.h>
#include <afs/stds.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <rx/rx_null.h>

#include <rx/rxkad.h>

#include "stress.h"
#include "stress_internal.h"


extern RXKST_ExecuteRequest();

struct ktc_encryptionKey serviceKey =
    {0x45, 0xe3, 0x3d, 0x16, 0x29, 0x64, 0x8a, 0x8f};
long serviceKeyVersion = 7;

static long GetKey (rock, kvno, key)
  IN char *rock;
  IN long  kvno;
  OUT struct ktc_encryptionKey *key;
{
    bcopy (&serviceKey, key, sizeof(*key));
    return 0;
}

static int minAuth;

long rxkst_StartServer (parms)
  INOUT struct serverParms *parms;
{
    extern int rx_stackSize;
    struct rx_service *tservice;
    struct rx_securityClass *sc[3];
    rxkad_level minLevel;

    minAuth = parms->authentication;
    if (minAuth == -1) minLevel = rxkad_clear;
    else minLevel = minAuth;

    sc[0] = rxnull_NewServerSecurityObject();
    sc[1] = 0;				/* no rxvab anymore */
    sc[2] = rxkad_NewServerSecurityObject (minLevel, 0, GetKey, 0);
    tservice = rx_NewService(htons(RXKST_SERVICEPORT), RXKST_SERVICEID,
			     "stress test", sc, 3, RXKST_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	fprintf (stderr, "Could not create stress test rx service\n");
	exit(3);
    }
    rx_SetMinProcs(tservice, parms->threads);
    rx_SetMaxProcs(tservice, parms->threads);
    rx_SetStackSize(tservice, 10000);

    rx_StartServer(/*donate me*/1);	/* start handling req. of all types */
    /* never reached */
    return 0;
}

static long CheckAuth (call)
  IN struct rx_call *call;
{
    long code;
    int si;
    rxkad_level level;
    char name[MAXKTCNAMELEN];
    char inst[MAXKTCNAMELEN];
    char cell[MAXKTCREALMLEN];
    long kvno;
    u_long expiration;			/* checked by Security Module */

    si = rx_SecurityClassOf(rx_ConnectionOf(call));
    if (si == 1) {
	printf ("No support for VAB security module.\n");
	return -1;
    } else if (si == 0) {
	if (minAuth > -1) return RXKST_UNAUTH;
	else return 0;
    } else if (si != 2) {
	fprintf (stderr, "Unknown security index %d\n", si);
	return -1;
    }

    code = rxkad_GetServerInfo (rx_ConnectionOf(call), &level, &expiration,
				name, inst, cell, &kvno);
    if (code) return code;
    if (minAuth > level) return -1;
    if (kvno != serviceKeyVersion) return RXKST_BADKVNO;
    if (strcmp (name, RXKST_CLIENT_NAME) ||
	strcmp (inst, RXKST_CLIENT_INST) ||
	cell[0]) return RXKST_BADCLIENT;
    return 0;
}

/* Stop the server.  There isn't a graceful way to do this so just exit. */

long SRXKST_Kill (call)
  IN struct rx_call *call;
{
    long code;
    code = CheckAuth (call);
    if (code) return code;

    /* This is tricky, but since we're never going to return, we end the call
     * here, then rx_Finalize should push out the response/ack. */
    rx_EndCall(call, 0);
    rx_Finalize();

    printf ("Server halted by RPC request.\n");
    exit (0);
    return 0;    
}

long SRXKST_Fast (call, n, inc_nP)
  IN struct rx_call *call;
  IN u_long n;
  OUT u_long *inc_nP;
{
    *inc_nP = n+1;
    return 0;
}

long SRXKST_Slow (call, tag, nowP)
  IN struct rx_call *call;
  IN u_long tag;
  OUT u_long *nowP;
{
    long code;
    code = CheckAuth (call);
    if (code) return code;
#ifdef AFS_PTHREAD_ENV
    sleep(1);
#else
    IOMGR_Sleep (1);
#endif
    time (nowP);
    return 0;
}

#define COPBUFSIZE 10000
static struct buflist {
    struct buflist *next;
} *buflist = 0;
static int bufAllocs = 0;

static u_char *GetBuffer()
{
    u_char *ret;
    if (buflist) {
	ret = (u_char *)buflist;
	buflist = buflist->next;
    } else {
	ret = (u_char *) osi_Alloc (COPBUFSIZE);
	bufAllocs++;
    }
    return ret;
}

static void PutBuffer(b)
  IN u_char *b;
{
    struct buflist *bl = (struct buflist *)b;
    bl->next = buflist;
    buflist = bl;
}

long SRXKST_Copious (call, inlen, insum, outlen, outsum)
  IN struct rx_call *call;
  IN u_long inlen;
  IN u_long insum;
  IN u_long outlen;
  OUT u_long *outsum;
{
    long code;
    long mysum;
    u_char *buf;
    int i;
    long b;
    long bytesTransfered;
    long n;

    code = CheckAuth (call);
    if (code) return code;
    buf = GetBuffer();
    mysum = 0;
    bytesTransfered = 0;
    while (bytesTransfered < inlen) {
	u_long tlen;			/* how much more to do */
	tlen = inlen - bytesTransfered;
	if (tlen > COPBUFSIZE) tlen = COPBUFSIZE;
	n = rx_Read(call, buf, tlen);
	if (n != tlen) {
	    if (n < 0) code = n;
	    else code = RXKST_READSHORT;
	    break;
	}
	for (i=0; i<tlen; i++) mysum += buf[i];
	bytesTransfered += tlen;
    }
    if (code) goto done;
    if (mysum != insum) {
	code = RXKST_BADINPUTSUM;
	goto done;
    }

#define BIG_PRIME 1257056893		/* 0x4AED2A7d */
#if 0
#define NextByte() ((b>24 ? ((seed = seed*BIG_PRIME + BIG_PRIME),b=0) : 0), \
		    (b +=8), ((seed >> (b-8))&0xff))
#else
#define NextByte() (b+=3)
#endif
    b=32;

    mysum = 0;
    bytesTransfered = 0;
    while (bytesTransfered < outlen) {
	u_long tlen;			/* how much more to do */
	tlen = outlen - bytesTransfered;
	if (tlen > COPBUFSIZE) tlen = COPBUFSIZE;
	for (i=0; i<tlen; i++) mysum += (buf[i] = NextByte());
	n = rx_Write(call, buf, tlen);
	if (n != tlen) {
	    if (n < 0) code = n;
	    else code = RXKST_WRITESHORT;
	    break;
	}
	bytesTransfered += tlen;
    }
  done:
    PutBuffer (buf);
    if (code) return code;
    *outsum = mysum;
    return 0;
}
