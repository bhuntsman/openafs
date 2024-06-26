/* The rxkad security object.  Authentication using a DES-encrypted
 * Kerberos-style ticket.  These are the client-only routines.  They do not
 * make any use of DES. */

/* Copyright (C) 1991, 1989 Transarc Corporation - All rights reserved */
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


#ifdef KERNEL

#include "../afs/param.h"
#include "../afs/stds.h"
#ifndef UKERNEL
#include "../h/types.h"
#include "../h/time.h"
#ifdef AFS_LINUX20_ENV
#include "../h/socket.h"
#endif
#include "../netinet/in.h"
#else /* !UKERNEL */
#include "../afs/sysincludes.h"
#endif /* !UKERNEL */
#ifndef AFS_LINUX22_ENV
#include "../rpc/types.h"
#include "../rpc/xdr.h"
#endif
#include "../rx/rx.h"
#else /* KERNEL */

#include <afs/param.h>
#include <afs/stds.h>
#include <sys/types.h>
#include <time.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <rx/rx.h>
#include <rx/xdr.h>
#ifdef AFS_PTHREAD_ENV
#include "../rxkad/rxkad.h"
#endif /* AFS_PTHREAD_ENV */

#endif /* KERNEL */

#include "private_data.h"
#define XPRT_RXKAD_CLIENT
#ifdef KERNEL
#include "../afs/permit_xprt.h"
#else
#include "../permit_xprt.h"
#endif


char *rxi_Alloc();

#ifndef max
#define	max(a,b)    ((a) < (b)? (b) : (a))
#endif /* max */

static struct rx_securityOps rxkad_client_ops = {
    rxkad_Close,
    rxkad_NewConnection,		/* every new connection */
    rxkad_PreparePacket,		/* once per packet creation */
    0,					/* send packet (once per retrans.) */
    0,
    0,
    0,
    rxkad_GetResponse,			/* respond to challenge packet */
    0,
    rxkad_CheckPacket,			/* check data packet */
    rxkad_DestroyConnection,
    rxkad_GetStats,
    0,
    0,
    0,
};

/* To minimize changes to epoch, we set this Cuid once, and everyone (including
 * rxnull) uses it after that.  This means that the Ksession of the first
 * authencticated connection should be a good one. */

#ifdef AFS_PTHREAD_ENV
/*
 * This mutex protects the following global variables:
 * Cuid
 * counter
 * rxkad_EpochWasSet
 */
#include <assert.h>
pthread_mutex_t rxkad_client_uid_mutex;
#define LOCK_CUID assert(pthread_mutex_lock(&rxkad_client_uid_mutex)==0);
#define UNLOCK_CUID assert(pthread_mutex_unlock(&rxkad_client_uid_mutex)==0);
#else
#define LOCK_CUID
#define UNLOCK_CUID
#endif /* AFS_PTHREAD_ENV */

static afs_int32 Cuid[2];			/* set once and shared by all */
int rxkad_EpochWasSet = 0;		/* TRUE => we called rx_SetEpoch */

/* allocate a new connetion ID in place */
rxkad_AllocCID(aobj, aconn)
  struct rx_securityClass *aobj;
  struct rx_connection *aconn;
{
    struct rxkad_cprivate *tcp;
    struct rxkad_cidgen tgen;
    static afs_int32 counter = 0;		/* not used anymore */

    LOCK_CUID
    if (Cuid[0] == 0) {
	afs_uint32 xor[2];
	tgen.ipAddr = rxi_getaddr();	/* comes back in net order */
	clock_GetTime(&tgen.time);	/* changes time1 and time2 */
	tgen.time.sec = htonl(tgen.time.sec);
	tgen.time.usec = htonl(tgen.time.usec);
	tgen.counter = htonl(counter++);
#ifdef KERNEL
	tgen.random1 = afs_random() & 0x7fffffff;	/* was "80000" */
	tgen.random2 = afs_random() & 0x7fffffff;	/* was "htonl(100)" */
#else
	tgen.random1 = htonl(getpid());
	tgen.random2 = htonl(100);
#endif
	if (aobj) {
	    /* block is ready for encryption with session key, let's go for it. */
	    tcp = (struct rxkad_cprivate *) aobj->privateData;
	    bcopy((void *)tcp->ivec, (void *)xor, 2*sizeof(afs_int32));
	    fc_cbc_encrypt((char *) &tgen, (char *) &tgen, sizeof(tgen),
			   tcp->keysched, xor, ENCRYPT);
	} else {
	    /* Create a session key so that we can encrypt it */

	}
	bcopy(((char *)&tgen) + sizeof(tgen) - ENCRYPTIONBLOCKSIZE,
	      (void *)Cuid, ENCRYPTIONBLOCKSIZE);
	Cuid[0] = (Cuid[0] & ~0x40000000) | 0x80000000;
	Cuid[1] &= RX_CIDMASK;
	rx_SetEpoch (Cuid[0]);		/* for future rxnull connections */
	rxkad_EpochWasSet++;
    }

    if (!aconn) {
	UNLOCK_CUID
	return 0;
    }
    aconn->epoch = Cuid[0];
    aconn->cid = Cuid[1];
    Cuid[1] += 1<<RX_CIDSHIFT;
    UNLOCK_CUID
    return 0;
}

/* Allocate a new client security object.  Called with the encryption level,
 * the session key and the ticket for the other side obtained from the
 * AuthServer.  Refers to export control to determine level. */

struct rx_securityClass *
rxkad_NewClientSecurityObject(level, sessionkey, kvno, ticketLen, ticket)
  rxkad_level      level;
  struct ktc_encryptionKey *sessionkey;
  afs_int32		   kvno;
  int		   ticketLen;
  char		  *ticket;
{   struct rx_securityClass *tsc;
    struct rxkad_cprivate   *tcp;
    int			     code;
    int			     size;

    size = sizeof(struct rx_securityClass);
    tsc = (struct rx_securityClass *) rxi_Alloc (size);
    bzero ((void *)tsc, size);
    tsc->refCount = 1;			/* caller gets one for free */
    tsc->ops = &rxkad_client_ops;

    size = sizeof(struct rxkad_cprivate);
    tcp = (struct rxkad_cprivate *) rxi_Alloc (size);
    bzero ((void *)tcp, size);
    tsc->privateData = (char *) tcp;
    tcp->type |= rxkad_client;
    tcp->level = xprt_CoerceLevel(level);
    code = fc_keysched (sessionkey, tcp->keysched);
    if (code) return 0;			/* bad key */
    bcopy ((void *)sessionkey, (void *)tcp->ivec, sizeof(tcp->ivec));
    tcp->kvno = kvno;			/* key version number */
    tcp->ticketLen = ticketLen;		/* length of ticket */
    bcopy(ticket, tcp->ticket, ticketLen);

    LOCK_RXKAD_STATS
    rxkad_stats_clientObjects++;
    UNLOCK_RXKAD_STATS
    return tsc;
}

/* client: respond to a challenge packet */

rxs_return_t rxkad_GetResponse (aobj, aconn, apacket)
  IN struct rx_securityClass *aobj;
  IN struct rx_packet *apacket;
  IN struct rx_connection *aconn;
{   struct rxkad_cprivate *tcp;
    char *tp;
    int   v2;				/* whether server is old style or v2 */
    afs_int32  challengeID;
    rxkad_level level;
    char *response;
    int   responseSize, missing;
    struct rxkad_v2ChallengeResponse  r_v2;
    struct rxkad_oldChallengeResponse r_old;

    tcp = (struct rxkad_cprivate *) aobj->privateData;

    if (!(tcp->type & rxkad_client)) return RXKADINCONSISTENCY;

    v2 = (rx_Contiguous(apacket) > sizeof(struct rxkad_oldChallenge));
    tp = rx_DataOf(apacket);

    if (v2) {                                  /* v2 challenge */
	struct rxkad_v2Challenge *c_v2;
	if (rx_GetDataSize(apacket) < sizeof(struct rxkad_v2Challenge))
	   return RXKADPACKETSHORT;
	c_v2 = (struct rxkad_v2Challenge *)tp;
	challengeID = ntohl(c_v2->challengeID);
	level       = ntohl(c_v2->level);
    } else {                                   /* old format challenge */
	struct rxkad_oldChallenge *c_old;
	if (rx_GetDataSize(apacket) < sizeof(struct rxkad_oldChallenge))
	   return RXKADPACKETSHORT;
	c_old = (struct rxkad_oldChallenge *)tp;
	challengeID = ntohl(c_old->challengeID);
	level       = ntohl(c_old->level);
    }

    if (level > tcp->level) return RXKADLEVELFAIL;
    LOCK_RXKAD_STATS
    rxkad_stats.challenges[rxkad_LevelIndex(tcp->level)]++;
    UNLOCK_RXKAD_STATS

    if (v2) {
	int i;
	afs_uint32 xor[2];
	bzero ((void *)&r_v2, sizeof(r_v2));
	r_v2.version = htonl(RXKAD_CHALLENGE_PROTOCOL_VERSION);
	r_v2.spare   = 0;
	(void) rxkad_SetupEndpoint (aconn, &r_v2.encrypted.endpoint);
	(void) rxi_GetCallNumberVector (aconn, r_v2.encrypted.callNumbers);
	for (i=0; i<RX_MAXCALLS; i++) {
	    if (r_v2.encrypted.callNumbers[i] < 0) return RXKADINCONSISTENCY;
	    r_v2.encrypted.callNumbers[i] = htonl(r_v2.encrypted.callNumbers[i]);
	}
	r_v2.encrypted.incChallengeID = htonl(challengeID + 1);
	r_v2.encrypted.level          = htonl((afs_int32)tcp->level);
	r_v2.kvno                     = htonl(tcp->kvno);
	r_v2.ticketLen                = htonl(tcp->ticketLen);
	r_v2.encrypted.endpoint.cksum = rxkad_CksumChallengeResponse (&r_v2);
	bcopy((void *)tcp->ivec, (void *)xor, 2*sizeof(afs_int32));
	fc_cbc_encrypt (&r_v2.encrypted, &r_v2.encrypted, 
			sizeof(r_v2.encrypted), tcp->keysched, xor, ENCRYPT);
	response     = (char *)&r_v2;
	responseSize = sizeof(r_v2);
    } else {
	bzero ((void *)&r_old, sizeof(r_old));
	r_old.encrypted.incChallengeID = htonl(challengeID + 1);
	r_old.encrypted.level          = htonl((afs_int32)tcp->level);
	r_old.kvno                     = htonl(tcp->kvno);
	r_old.ticketLen                = htonl(tcp->ticketLen);
	fc_ecb_encrypt (&r_old.encrypted, &r_old.encrypted, tcp->keysched, ENCRYPT);
	response     = (char *)&r_old;
	responseSize = sizeof(r_old);
    }

    if (RX_MAX_PACKET_DATA_SIZE < responseSize + tcp->ticketLen)
	return RXKADPACKETSHORT;	/* not enough space */

    rx_computelen(apacket, missing);
    missing = responseSize + tcp->ticketLen - missing;
    if (missing > 0) 
       if (rxi_AllocDataBuf(apacket, missing) > 0)
	  return RXKADPACKETSHORT;	/* not enough space */

    /* copy response and ticket into packet */
    rx_packetwrite(apacket, 0, responseSize, response);
    rx_packetwrite(apacket, responseSize, tcp->ticketLen, tcp->ticket);

    rx_SetDataSize (apacket, responseSize + tcp->ticketLen);
    return 0;
}


rxkad_ResetState()
{
    LOCK_CUID
    Cuid[0] = 0;
    rxkad_EpochWasSet=0;
    UNLOCK_CUID
}
