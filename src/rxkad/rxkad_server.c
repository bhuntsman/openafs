/* The rxkad security object.  Authentication using a DES-encrypted
 * Kerberos-style ticket.  These are the server-only routines. */

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
#include <des.h>
#include <afs/afsutil.h>
#include "private_data.h"
#define XPRT_RXKAD_SERVER
#include "../permit_xprt.h"


/*
 * This can be set to allow alternate ticket decoding.
 * Currently only used by the AFS/DFS protocol translator to recognize
 * Kerberos V5 tickets. The actual code to do that is provided externally.
 */
afs_int32 (*rxkad_AlternateTicketDecoder)();

static struct rx_securityOps rxkad_server_ops = {
    rxkad_Close,
    rxkad_NewConnection,
    rxkad_PreparePacket,		/* once per packet creation */
    0,					/* send packet (once per retrans) */
    rxkad_CheckAuthentication,
    rxkad_CreateChallenge,
    rxkad_GetChallenge,
    0,
    rxkad_CheckResponse,
    rxkad_CheckPacket,			/* check data packet */
    rxkad_DestroyConnection,
    rxkad_GetStats,
};
extern afs_uint32 rx_MyMaxSendSize;

/* Miscellaneous random number routines that use the fcrypt module and the
 * timeofday. */

static fc_KeySchedule random_int32_schedule;

#ifdef AFS_PTHREAD_ENV
/*
 * This mutex protects the following global variables:
 * random_int32_schedule
 * seed
 */

#include <assert.h>
pthread_mutex_t rxkad_random_mutex;
#define LOCK_RM assert(pthread_mutex_lock(&rxkad_random_mutex)==0);
#define UNLOCK_RM assert(pthread_mutex_unlock(&rxkad_random_mutex)==0);
#else
#define LOCK_RM
#define UNLOCK_RM
#endif /* AFS_PTHREAD_ENV */

static void init_random_int32 ()
{   struct timeval key;

    gettimeofday (&key, (char *) 0);
    LOCK_RM
    fc_keysched (&key, random_int32_schedule);
    UNLOCK_RM
}

static afs_int32 get_random_int32 ()
{   static struct timeval seed;
    afs_int32 rc;

    LOCK_RM
    fc_ecb_encrypt (&seed, &seed, random_int32_schedule, ENCRYPT);
    rc = seed.tv_sec;
    UNLOCK_RM
    return rc;
}

/* Called with four parameters.  The first is the level of encryption, as
   defined in the rxkad.h file.  The second and third are a rock and a
   procedure that is called with the key version number that accompanies the
   ticket and returns a pointer to the server's decryption key.  The fourth
   argument, if not NULL, is a pointer to a function that will be called for
   every new connection with the name, instance and cell of the client.  The
   routine should return zero if the user is NOT acceptible to the server.  If
   this routine is not supplied, the server can call rxkad_GetServerInfo with
   the rx connection pointer passed to the RPC routine to obtain information
   about the client. */

struct rx_securityClass *
rxkad_NewServerSecurityObject (level, get_key_rock, get_key, user_ok)
  rxkad_level      level;		/* minimum level */
  char		  *get_key_rock;	/* rock for get_key implementor */
  int		 (*get_key)();		/* passed kvno & addr(key) to fill */
  int		 (*user_ok)();		/* passed name, inst, cell => bool */
{   struct rx_securityClass *tsc;
    struct rxkad_sprivate   *tsp;
    int size;

    if (!get_key) return 0;
    
    size = sizeof(struct rx_securityClass);
    tsc = (struct rx_securityClass *) osi_Alloc (size);
    bzero (tsc, size);
    tsc->refCount = 1;			/* caller has one reference */
    tsc->ops = &rxkad_server_ops;
    size = sizeof(struct rxkad_sprivate);
    tsp = (struct rxkad_sprivate *) osi_Alloc (size);
    bzero (tsp, size);
    tsc->privateData = (char *) tsp;

    tsp->type |= rxkad_server;		/* so can identify later */
    tsp->level = level;			/* level of encryption */
    tsp->get_key_rock = get_key_rock;
    tsp->get_key = get_key;		/* to get server ticket */
    tsp->user_ok = user_ok;		/* to inform server of client id. */
    init_random_int32 ();

    LOCK_RXKAD_STATS
    rxkad_stats_serverObjects++;
    UNLOCK_RXKAD_STATS
    return tsc;
}

/* server: called to tell if a connection authenticated properly */

rxs_return_t rxkad_CheckAuthentication (aobj, aconn)
  struct rx_securityClass *aobj;
  struct rx_connection *aconn;
{   struct rxkad_sconn *sconn;

    /* first make sure the object exists */
    if (!aconn->securityData) return RXKADINCONSISTENCY;

    sconn = (struct rxkad_sconn *) aconn->securityData;
    return !sconn->authenticated;
}

/* server: put the current challenge in the connection structure for later use
   by packet sender */

rxs_return_t rxkad_CreateChallenge(aobj, aconn)
  struct rx_securityClass *aobj;
  struct rx_connection *aconn;
{   struct rxkad_sconn *sconn;
    struct rxkad_sprivate *tsp;

    sconn = (struct rxkad_sconn *) aconn->securityData;
    sconn->challengeID = get_random_int32 ();
    sconn->authenticated = 0;		/* conn unauth. 'til we hear back */
    /* initialize level from object's minimum acceptable level */
    tsp = (struct rxkad_sprivate *)aobj->privateData;
    sconn->level = tsp->level;
    return 0;
}

/* server: fill in a challenge in the packet */

rxs_return_t rxkad_GetChallenge (aobj, aconn, apacket)
  IN struct rx_securityClass *aobj;
  IN struct rx_packet *apacket;
  IN struct rx_connection *aconn;
{   struct rxkad_sconn *sconn;
    afs_int32 temp;
    char *challenge;
    int challengeSize;
    struct rxkad_v2Challenge  c_v2;   /* version 2 */
    struct rxkad_oldChallenge c_old;  /* old style */

    sconn = (struct rxkad_sconn *) aconn->securityData;
    if (rx_IsUsingPktCksum(aconn)) sconn->cksumSeen = 1;

    if (sconn->cksumSeen) {
	bzero (&c_v2, sizeof(c_v2));
	c_v2.version = htonl(RXKAD_CHALLENGE_PROTOCOL_VERSION);
	c_v2.challengeID = htonl(sconn->challengeID);
	c_v2.level = htonl((afs_int32)sconn->level);
	c_v2.spare = 0;
	challenge = (char *)&c_v2;
	challengeSize = sizeof(c_v2);
    } else {
	bzero (&c_old, sizeof(c_old));
	c_old.challengeID = htonl(sconn->challengeID);
	c_old.level = htonl((afs_int32)sconn->level);
	challenge = (char *)&c_old;
	challengeSize = sizeof(c_old);
    }
    if (rx_MyMaxSendSize < challengeSize)
	return RXKADPACKETSHORT;	/* not enough space */

    rx_packetwrite(apacket, 0, challengeSize, challenge);
    rx_SetDataSize (apacket, challengeSize);
    sconn->tried = 1;
    LOCK_RXKAD_STATS
    rxkad_stats.challengesSent++;
    UNLOCK_RXKAD_STATS
    return 0;
}

/* server: process a response to a challenge packet */
/* XXX this does some copying of data in and out of the packet, but I'll bet it
 * could just do it in place, especially if I used rx_Pullup...
 */
rxs_return_t rxkad_CheckResponse (aobj, aconn, apacket)
  struct rx_securityClass *aobj;
  struct rx_packet *apacket;
  struct rx_connection *aconn;
{   struct rxkad_sconn *sconn;
    struct rxkad_sprivate *tsp;
    struct ktc_encryptionKey serverKey;
    struct rxkad_oldChallengeResponse oldr; /* response format */
    struct rxkad_v2ChallengeResponse v2r;
    afs_int32  tlen;				/* ticket len */
    afs_int32  kvno;				/* key version of ticket */
    char  tix[MAXKTCTICKETLEN];
    afs_int32  incChallengeID;
    rxkad_level level;
    int	  code;
    /* ticket contents */
    struct ktc_principal client;
    struct ktc_encryptionKey sessionkey;
    afs_int32	  host;
    afs_uint32 start;
    afs_uint32 end;
    unsigned int pos;
    struct rxkad_serverinfo *rock;

    sconn = (struct rxkad_sconn *) aconn->securityData;
    tsp = (struct rxkad_sprivate *) aobj->privateData;

    if (sconn->cksumSeen) {
	/* expect v2 response, leave fields in v2r in network order for cksum
         * computation which follows decryption. */
	if (rx_GetDataSize(apacket) < sizeof(v2r)) 
	  return RXKADPACKETSHORT;
	rx_packetread(apacket, 0, sizeof(v2r), &v2r);
	pos = sizeof(v2r);
	/* version == 2 */
	/* ignore spare */
	kvno = ntohl (v2r.kvno);
	tlen = ntohl(v2r.ticketLen);
	if (rx_GetDataSize(apacket) < sizeof(v2r) + tlen)
	    return RXKADPACKETSHORT;
    } else {
	/* expect old format response */
	if (rx_GetDataSize(apacket) < sizeof(oldr)) return RXKADPACKETSHORT;
	rx_packetread(apacket, 0, sizeof(oldr), &oldr);
	pos = sizeof(oldr);

	kvno = ntohl (oldr.kvno);
	tlen = ntohl(oldr.ticketLen);
	if (rx_GetDataSize(apacket) != sizeof(oldr) + tlen)
	    return RXKADPACKETSHORT;
    }
    if ((tlen < MINKTCTICKETLEN) || (tlen > MAXKTCTICKETLEN))
	return RXKADTICKETLEN;

    rx_packetread(apacket, pos, tlen, tix);	/* get ticket */

    /*
     * We allow the ticket to be optionally decoded by an alternate
     * ticket decoder, if the function variable
     * rxkad_AlternateTicketDecoder is set. That function should
     * return a code of -1 if it wants the ticket to be decoded by
     * the standard decoder.
     */
    if (rxkad_AlternateTicketDecoder) {
	code = rxkad_AlternateTicketDecoder
	    (kvno, tix, tlen, client.name, client.instance, client.cell,
	     &sessionkey, &host, &start, &end);
	if (code && code != -1) {
	    return code;
	}
    } else {
	code = -1;    /* No alternate ticket decoder present */
    }

    /*
     * If the alternate decoder is not present, or returns -1, then
     * assume the ticket is of the default style.
     */
    if (code == -1) {
	/* get ticket's key */
	code = (*tsp->get_key)(tsp->get_key_rock, kvno, &serverKey);
	if (code) return RXKADUNKNOWNKEY;	/* invalid kvno */
	code = tkt_DecodeTicket (tix, tlen, &serverKey,
				 client.name, client.instance, client.cell,
				 &sessionkey, &host, &start, &end);
	if (code) return RXKADBADTICKET;
    }
    code = tkt_CheckTimes (start, end, time(0));
    if (code == -1) return RXKADEXPIRED;
    else if (code <= 0) return RXKADNOAUTH;

    code = fc_keysched (&sessionkey, sconn->keysched);
    if (code) return RXKADBADKEY;
    bcopy (&sessionkey, sconn->ivec, sizeof(sconn->ivec));

    if (sconn->cksumSeen) {
	/* using v2 response */
	afs_uint32 cksum;			/* observed cksum */
	struct rxkad_endpoint endpoint;	/* connections endpoint */
	int i;
	afs_uint32 xor[2];

	bcopy(sconn->ivec, xor, 2*sizeof(afs_int32));
	fc_cbc_encrypt (&v2r.encrypted, &v2r.encrypted, sizeof(v2r.encrypted),
			sconn->keysched, xor, DECRYPT);
	cksum = rxkad_CksumChallengeResponse (&v2r);
	if (cksum != v2r.encrypted.endpoint.cksum)
	    return RXKADSEALEDINCON;
	(void) rxkad_SetupEndpoint (aconn, &endpoint);
	v2r.encrypted.endpoint.cksum = 0;
	if (bcmp (&endpoint, &v2r.encrypted.endpoint, sizeof(endpoint)) != 0)
	    return RXKADSEALEDINCON;
	for (i=0; i<RX_MAXCALLS; i++) {
	    v2r.encrypted.callNumbers[i] = ntohl(v2r.encrypted.callNumbers[i]);
	    if (v2r.encrypted.callNumbers[i] < 0) return RXKADSEALEDINCON;
	}

	(void) rxi_SetCallNumberVector (aconn, v2r.encrypted.callNumbers);
	incChallengeID = ntohl(v2r.encrypted.incChallengeID);
	level = ntohl(v2r.encrypted.level);
    } else {
	/* expect old format response */
	fc_ecb_encrypt(&oldr.encrypted, &oldr.encrypted,
		       sconn->keysched, DECRYPT);
	incChallengeID = ntohl(oldr.encrypted.incChallengeID);
	level = ntohl(oldr.encrypted.level);
    }
    if (incChallengeID != sconn->challengeID+1)
	return RXKADOUTOFSEQUENCE;	/* replay attempt */
    if ((level < sconn->level) || (level > rxkad_crypt)) return RXKADLEVELFAIL;
    sconn->level = level;
    rxkad_SetLevel (aconn, sconn->level);
    LOCK_RXKAD_STATS
    rxkad_stats.responses[rxkad_LevelIndex(sconn->level)]++;
    UNLOCK_RXKAD_STATS

    /* now compute endpoint-specific info used for computing 16 bit checksum */
    rxkad_DeriveXORInfo(aconn, sconn->keysched, sconn->ivec, sconn->preSeq);

    /* otherwise things are ok */
    sconn->expirationTime = end;
    sconn->authenticated = 1;

    if (tsp->user_ok) {
	code = tsp->user_ok (client.name, client.instance, client.cell, kvno);
	if (code) return RXKADNOAUTH;
    }
    else {				/* save the info for later retreival */
	int size = sizeof(struct rxkad_serverinfo);
	rock = (struct rxkad_serverinfo *) osi_Alloc (size);
	bzero (rock, size);
	rock->kvno = kvno;
	bcopy (&client, &rock->client, sizeof(rock->client));
	sconn->rock = rock;
    }
    return 0;
}

/* return useful authentication info about a server-side connection */

afs_int32 rxkad_GetServerInfo (aconn, level, expiration,
				 name, instance, cell, kvno)
  struct rx_connection *aconn;
  rxkad_level	 *level;
  afs_uint32  *expiration;
  char		 *name;
  char		 *instance;
  char		 *cell;
  afs_int32		 *kvno;
{
    struct rxkad_sconn *sconn;

    sconn = (struct rxkad_sconn *) aconn->securityData;
    if (sconn && sconn->authenticated && sconn->rock &&
	(time(0) < sconn->expirationTime)) {
	if (level)	*level = sconn->level;
	if (expiration) *expiration = sconn->expirationTime;
	if (name)	strcpy (name, sconn->rock->client.name);
	if (instance)	strcpy (instance, sconn->rock->client.instance);
	if (cell)	strcpy (cell, sconn->rock->client.cell);
	if (kvno)	*kvno = sconn->rock->kvno;
	return 0;
    }
    else return RXKADNOAUTH;
}
