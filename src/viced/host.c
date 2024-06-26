/* Copyright (C) 1991, 1990 Transarc Corporation - All rights reserved */

/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1998
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include <afs/param.h>
#include <stdio.h>
#include <errno.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <winsock2.h>
#else
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#endif
#include <afs/stds.h>
#include <rx/xdr.h>
#include <afs/assert.h>
#include <lwp.h>
#include <lock.h>
#include <afs/afsint.h>
#include <afs/rxgen_consts.h>
#include <afs/nfs.h>
#include <afs/errors.h>
#include <afs/ihandle.h>
#include <afs/vnode.h>
#include <afs/volume.h>
#ifdef AFS_ATHENA_STDENV
#include <krb.h>
#endif
#include <afs/acl.h>
#include <afs/ptclient.h>
#include <afs/prs_fs.h>
#include <afs/auth.h>
#include <afs/afsutil.h>
#include <rx/rx.h>
#include <afs/cellconfig.h>
#include <stdlib.h>
#include "viced.h"
#include "host.h"


#ifdef AFS_PTHREAD_ENV
pthread_mutex_t host_glock_mutex;
#endif /* AFS_PTHREAD_ENV */

extern	int	Console;
extern  int	CurrentConnections;
extern	int	SystemId;
extern  int     AnonymousID;
extern  prlist  AnonCPS;
extern  int     LogLevel;
extern	struct afsconf_dir *confDir; /* config dir object */
extern  int     lwps; 	/* the max number of server threads */
extern  afsUUID FS_HostUUID;

int	CEs = 0;	    /* active clients */
int	CEBlocks = 0;	    /* number of blocks of CEs */
struct  client *CEFree = 0; /* first free client */
struct  host *hostList = 0; /* linked list of all hosts */
int     hostCount = 0;      /* number of hosts in hostList */
int	rxcon_ident_key;
int	rxcon_client_key;

#define CESPERBLOCK 73
struct CEBlock		    /* block of CESPERBLOCK file entries */
{
    struct client entry[CESPERBLOCK];
};

/*
 * Make sure the subnet macros have been defined.
 */
#ifndef IN_SUBNETA
#define	IN_SUBNETA(i)		((((afs_int32)(i))&0x80800000)==0x00800000)
#endif

#ifndef IN_CLASSA_SUBNET
#define	IN_CLASSA_SUBNET	0xffff0000
#endif

#ifndef IN_SUBNETB
#define	IN_SUBNETB(i)		((((afs_int32)(i))&0xc0008000)==0x80008000)
#endif

#ifndef IN_CLASSB_SUBNET
#define	IN_CLASSB_SUBNET	0xffffff00
#endif

#define rxr_GetEpoch(aconn) (((struct rx_connection *)(aconn))->epoch)

#define rxr_CidOf(aconn) (((struct rx_connection *)(aconn))->cid)

#define rxr_PortOf(aconn) \
    rx_PortOf(rx_PeerOf(((struct rx_connection *)(aconn))))

#define rxr_HostOf(aconn) \
    rx_HostOf(rx_PeerOf((struct rx_connection *)(aconn)))


/* get a new block of CEs and chain it on CEFree */
static void GetCEBlock()
{
    register struct CEBlock *block;
    register int i;

    block = (struct CEBlock *)malloc(sizeof(struct CEBlock));
    if (!block)
	ShutDownAndCore(PANIC);

    for(i = 0; i < (CESPERBLOCK -1); i++) {
	Lock_Init(&block->entry[i].lock);
	block->entry[i].next = &(block->entry[i+1]);
    }
    block->entry[CESPERBLOCK-1].next = 0;
    Lock_Init(&block->entry[CESPERBLOCK-1].lock);
    CEFree = (struct client *)block;
    CEBlocks++;

} /*GetCEBlock*/


/* get the next available CE */
static struct client *GetCE()

{
    register struct client *entry;

    if (CEFree == 0)
	GetCEBlock();
    if (CEFree == 0)
	ShutDownAndCore(PANIC);

    entry = CEFree;
    CEFree = entry->next;
    CEs++;
    bzero((char *)entry, CLIENT_TO_ZERO(entry));
    return(entry);

} /*GetCE*/


/* return an entry to the free list */
static void FreeCE(entry)
    register struct client *entry;

{
    entry->next = CEFree;
    CEFree = entry;
    CEs--;

} /*FreeCE*/

/*
 * The HTs and HTBlocks variables were formerly static, but they are
 * now referenced elsewhere in the FileServer.
 */
int HTs = 0;				/* active file entries */
int HTBlocks = 0;			/* number of blocks of HTs */
static struct host *HTFree = 0;         /* first free file entry */

/*
 * Hash tables of host pointers. We need two tables, one
 * to map IP addresses onto host pointers, and another
 * to map host UUIDs onto host pointers.
 */
static struct h_hashChain *hostHashTable[h_HASHENTRIES];
static struct h_hashChain *hostUuidHashTable[h_HASHENTRIES];
#define h_HashIndex(hostip) ((hostip) & (h_HASHENTRIES-1))
#define h_UuidHashIndex(uuidp) (((int)(afs_uuid_hash(uuidp))) & (h_HASHENTRIES-1))

struct HTBlock		/* block of HTSPERBLOCK file entries */
{
    struct host entry[h_HTSPERBLOCK];
};


/* get a new block of HTs and chain it on HTFree */
static void GetHTBlock()

{
    register struct HTBlock *block;
    register int i;
    static int index = 0;

    block = (struct HTBlock *)malloc(sizeof(struct HTBlock));
    if (!block)
	ShutDownAndCore(PANIC);

#ifdef AFS_PTHREAD_ENV
    for(i=0; i < (h_HTSPERBLOCK); i++)
	assert(pthread_cond_init(&block->entry[i].cond, NULL) == 0);
#endif /* AFS_PTHREAD_ENV */
    for(i=0; i < (h_HTSPERBLOCK); i++)
	Lock_Init(&block->entry[i].lock);
    for(i=0; i < (h_HTSPERBLOCK -1); i++)
	block->entry[i].next = &(block->entry[i+1]);
    for (i=0; i< (h_HTSPERBLOCK); i++)
        block->entry[i].index = index++;
    block->entry[h_HTSPERBLOCK-1].next = 0;
    HTFree = (struct host *)block;
    hosttableptrs[HTBlocks++] = block->entry;

} /*GetHTBlock*/


/* get the next available HT */
static struct host *GetHT()

{
    register struct host *entry;

    if (HTFree == 0)
	GetHTBlock();
    assert(HTFree != 0);
    entry = HTFree;
    HTFree = entry->next;
    HTs++;
    bzero((char *)entry, HOST_TO_ZERO(entry));
    return(entry);

} /*GetHT*/


/* return an entry to the free list */
static void FreeHT(entry)
    register struct host *entry; 

{
    entry->next = HTFree;
    HTFree = entry;
    HTs--;

} /*FreeHT*/


static short consolePort = 0;

int h_Release(host)
    register struct host *host;
{
    H_LOCK
    h_Release_r(host);
    H_UNLOCK
    return 0;
}

/**
 * If this thread does not have a hold on this host AND
 * if other threads also dont have any holds on this host AND
 * If either the HOSTDELETED or CLIENTDELETED flags are set
 * then toss the host
 */
int h_Release_r(host)
    register struct host *host;
{	
    
    if (!((host)->holds[h_holdSlot()] &= ~h_holdbit()) ) {
	if (! h_OtherHolds_r(host) ) {
	    if ( (host->hostFlags & HOSTDELETED) || 
		(host->hostFlags & CLIENTDELETED) ) {
		h_TossStuff_r(host);
	    }		
	}
    }
    return 0;
}

int h_Held(host)
    register struct host *host;
{
    int retVal;
    H_LOCK
    retVal = h_Held_r(host);
    H_UNLOCK
    return retVal;
}

int h_OtherHolds_r(host)
    register struct host *host;
{
    register int i, bit, slot;
    bit = h_holdbit();
    slot = h_holdSlot();
    for (i = 0 ; i < h_maxSlots ; i++) {
	if (host->holds[i] != ((i == slot) ? bit : 0)) {
	    return 1;
	}
    }
    return 0;
}

int h_OtherHolds(host)
    register struct host *host;
{
    int retVal;
    H_LOCK
    retVal = h_OtherHolds_r(host);
    H_UNLOCK
    return retVal;
}

int h_Lock_r(host)
    register struct host *host;
{
    H_UNLOCK
    h_Lock(host);
    H_LOCK
    return 0;
}

/**
  * Non-blocking lock
  * returns 1 if already locked
  * else returns locks and returns 0
  */

int h_NBLock_r(host)
    register struct host *host;
{
    struct Lock *hostLock = &host->lock;
    int locked = 0;

    H_UNLOCK
    LOCK_LOCK(hostLock)
    if ( !(hostLock->excl_locked) && !(hostLock->readers_reading) )
	hostLock->excl_locked = WRITE_LOCK;
    else
	locked = 1;

    LOCK_UNLOCK(hostLock)
    H_LOCK
    if ( locked )
	return 1;
    else
	return 0;
}


#if FS_STATS_DETAILED
/*------------------------------------------------------------------------
 * PRIVATE h_AddrInSameNetwork
 *
 * Description:
 *	Given a target IP address and a candidate IP address (both
 *	in host byte order), return a non-zero value (1) if the
 *	candidate address is in a different network from the target
 *	address.
 *
 * Arguments:
 *	a_targetAddr	   : Target address.
 *	a_candAddr	   : Candidate address.
 *
 * Returns:
 *	1 if the candidate address is in the same net as the target,
 *	0 otherwise.
 *
 * Environment:
 *	The target and candidate addresses are both in host byte
 *	order, NOT network byte order, when passed in.  We return
 *	our value as a character, since that's the type of field in
 *	the host structure, where this info will be stored.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static char h_AddrInSameNetwork(a_targetAddr, a_candAddr)
    afs_uint32 a_targetAddr;
    afs_uint32 a_candAddr;

{ /*h_AddrInSameNetwork*/

    afs_uint32 targetNet;
    afs_uint32 candNet;

    /*
     * Pull out the network and subnetwork numbers from the target
     * and candidate addresses.  We can short-circuit this whole
     * affair if the target and candidate addresses are not of the
     * same class.
     */
    if (IN_CLASSA(a_targetAddr)) {
	if (!(IN_CLASSA(a_candAddr))) {
	    return(0);
	}
	targetNet = a_targetAddr & IN_CLASSA_NET;
	candNet   = a_candAddr   & IN_CLASSA_NET;
    }
    else
	if (IN_CLASSB(a_targetAddr)) {
	    if (!(IN_CLASSB(a_candAddr))) {
		return(0);
	    }
	    targetNet = a_targetAddr & IN_CLASSB_NET;
	    candNet   = a_candAddr   & IN_CLASSB_NET;
	} /*Class B target*/
	else
	    if (IN_CLASSC(a_targetAddr)) {
		if (!(IN_CLASSC(a_candAddr))) {
		    return(0);
		}
		targetNet = a_targetAddr & IN_CLASSC_NET;
		candNet   = a_candAddr   & IN_CLASSC_NET;
	    } /*Class C target*/
	    else {
		targetNet = a_targetAddr;
		candNet = a_candAddr;
	    } /*Class D address*/
    
    /*
     * Now, simply compare the extracted net values for the two addresses
     * (which at this point are known to be of the same class)
     */
    if (targetNet == candNet)
	return(1);
    else
	return(0);

} /*h_AddrInSameNetwork*/
#endif /* FS_STATS_DETAILED */



h_gethostcps_r(host,now)
    register struct host *host;
    register afs_int32	  now;
{
    register int code;
    int  slept=0, held;

    /* at this point, the host might not be locked, nor held */
    /* make sure that we do not disappear behind the RPC     */
    if ( !(held = h_Held_r(host)) )
		h_Hold_r(host);

    	/* wait if somebody else is already doing the getCPS call */
    while ( host->hostFlags & HPCS_INPROGRESS ) 
    {
	slept = 1;		/* I did sleep */
	host->hostFlags |= HPCS_WAITING; /* I am sleeping now */
#ifdef AFS_PTHREAD_ENV
	pthread_cond_wait(&host->cond, &host_glock_mutex);
#else /* AFS_PTHREAD_ENV */
        if (( code = LWP_WaitProcess( &(host->hostFlags ))) != LWP_SUCCESS)
		ViceLog(0, ("LWP_WaitProcess returned %d\n", code));
#endif /* AFS_PTHREAD_ENV */
    }


    host->hostFlags |= HPCS_INPROGRESS;	/* mark as CPSCall in progress */
    if (host->hcps.prlist_val)
	free(host->hcps.prlist_val);    /* this is for hostaclRefresh */
    host->hcps.prlist_val = (afs_int32 *)0;
    host->hcps.prlist_len = 0;
    slept? (host->cpsCall = FT_ApproxTime()): (host->cpsCall = now );

    H_UNLOCK
    code = pr_GetHostCPS(htonl(host->host), &host->hcps);
    H_LOCK
    if (code) {
	/*
	 * Although ubik_Call (called by pr_GetHostCPS) traverses thru all protection servers
	 * and reevaluates things if no sync server or quorum is found we could still end up
	 * with one of these errors. In such case we would like to reevaluate the rpc call to
	 * find if there's cps for this guy. We treat other errors (except network failures
	 * ones - i.e. code < 0) as an indication that there is no CPS for this host. Ideally
	 * we could like to deal this problem the other way around (i.e. if code == NOCPS 
	 * ignore else retry next time) but the problem is that there're other errors (i.e.
	 * EPERM) for which we don't want to retry and we don't know the whole code list!
	 */
	if (code < 0 || code == UNOQUORUM || code == UNOTSYNC) {
	    /* 
	     * We would have preferred to use a while loop and try again since ops in protected
	     * acls for this host will fail now but they'll be reevaluated on any subsequent
	     * call. The attempt to wait for a quorum/sync site or network error won't work
	     * since this problems really should only occurs during a complete fileserver 
	     * restart. Since the fileserver will start before the ptservers (and thus before
	     * quorums are complete) clients will be utilizing all the fileserver's lwps!!
	     */
	    host->hcpsfailed = 1;
	    ViceLog(0, ("Warning:  GetHostCPS failed (%d) for %x; will retry\n", code, host->host));
	} else {
	    host->hcpsfailed = 0;
	    ViceLog(1, ("gethost:  GetHostCPS failed (%d) for %x; ignored\n", code, host->host));
	}
	if (host->hcps.prlist_val)
	    free(host->hcps.prlist_val);
	host->hcps.prlist_val = (afs_int32 *)0;
	host->hcps.prlist_len = 0;	/* Make sure it's zero */
    } else
	host->hcpsfailed = 0;

    host->hostFlags &=  ~HPCS_INPROGRESS;
    					/* signal all who are waiting */
    if ( host->hostFlags & HPCS_WAITING) /* somebody is waiting */
    {
        host->hostFlags &= ~HPCS_WAITING;
#ifdef AFS_PTHREAD_ENV
	assert(pthread_cond_broadcast(&host->cond) == 0);
#else /* AFS_PTHREAD_ENV */
    	if ( (code = LWP_NoYieldSignal( &(host->hostFlags) )) != LWP_SUCCESS )
		ViceLog(0, ("LWP_NoYieldSignal returns %d\n", code));
#endif /* AFS_PTHREAD_ENV */
    }

    /* if we had held the  host, release it now */
    if ( !held ) 
	h_Release_r(host);
}

void h_flushhostcps(hostaddr, hport)
    register afs_uint32  hostaddr, hport;  /* net byte order */
{
    register struct host *host;
    
    H_LOCK
    host = h_Lookup_r(hostaddr, hport);
    if (host) {
      host->hcpsfailed = 1;
    }
    H_UNLOCK

return;
}


/*
 * Allocate a host.  It will be identified by the peer (ip,port) info in the
 * rx connection provided.  The host is returned un-held and un-locked
 */
#define	DEF_ROPCONS 2115

struct host *h_Alloc(r_con)
    register struct rx_connection *r_con;
{
    struct host *retVal;
    H_LOCK
    retVal = h_Alloc_r(r_con);
    H_UNLOCK
    return retVal;
}

struct host *h_Alloc_r(r_con)
    register struct rx_connection *r_con;

{
    register int code;
    struct servent *serverentry;
    register index = h_HashIndex(rxr_HostOf(r_con));
    register struct host *host;
    static struct rx_securityClass *sc = 0;
    afs_int32	now;
    struct h_hashChain*	h_hashChain;
#if FS_STATS_DETAILED
    afs_uint32 newHostAddr_HBO;	/*New host IP addr, in host byte order*/
#endif /* FS_STATS_DETAILED */

    host = GetHT();

    h_hashChain = (struct h_hashChain*) malloc(sizeof(struct h_hashChain));
    assert(h_hashChain);
    h_hashChain->hostPtr = host;
    h_hashChain->addr = rxr_HostOf(r_con);
    h_hashChain->next = hostHashTable[index];
    hostHashTable[index] = h_hashChain;

    host->host = rxr_HostOf(r_con);
    host->port = rxr_PortOf(r_con);
    if(consolePort == 0 ) { /* find the portal number for console */
#if	defined(AFS_OSF_ENV)
	serverentry = getservbyname("ropcons", "");
#else
	serverentry = getservbyname("ropcons", 0);
#endif 
	if (serverentry)
	    consolePort = serverentry->s_port;
	else
	    consolePort = DEF_ROPCONS;	/* Use a default */
    }
    if (host->port == consolePort) host->Console = 1;
    /* Make a callback channel even for the console, on the off chance that it
       makes a request that causes a break call back.  It shouldn't. */
    {
	if (!sc)
	    sc = (struct rx_securityClass *) rxnull_NewClientSecurityObject();
	host->callback_rxcon = rx_NewConnection (host->host, host->port,
						 1, sc, 0);
	rx_SetConnDeadTime(host->callback_rxcon, 50);
	rx_SetConnHardDeadTime(host->callback_rxcon, AFS_HARDDEADTIME);
    }
    now = host->LastCall = host->cpsCall = host->ActiveCall = FT_ApproxTime();
    host->hostFlags = 0;
    host->hcps.prlist_val = (afs_int32 *)0;
    host->hcps.prlist_len = 0;
    host->hcps.prlist_val = (afs_int32 *)0;
    host->interface = 0;
    /*host->hcpsfailed = 0; 	/* save cycles */
    /* h_gethostcps(host);      do this under host lock */
    host->FirstClient = 0;      
    h_InsertList_r(host);	/* update global host List */
#if FS_STATS_DETAILED
    /*
     * Compare the new host's IP address (in host byte order) with ours
     * (the File Server's), remembering if they are in the same network.
     */
    newHostAddr_HBO = (afs_uint32)ntohl(host->host);
    host->InSameNetwork = h_AddrInSameNetwork(FS_HostAddr_HBO,
					      newHostAddr_HBO);
#endif /* FS_STATS_DETAILED */
    return host;

} /*h_Alloc*/


/* Lookup a host given an IP address and UDP port number. */
struct host *h_Lookup(hostaddr, hport)
    afs_uint32 hostaddr, hport;     /* network byte order */
{
    struct host *retVal;
    H_LOCK
    retVal = h_Lookup_r(hostaddr, hport);
    H_UNLOCK
    return retVal;
}

struct host *h_Lookup_r(hostaddr, hport)
    afs_uint32 hostaddr, hport;     /* network byte order */
{
    register afs_int32 now;
    register struct host *host=0;
    register struct h_hashChain* chain;
    register index = h_HashIndex(hostaddr);
    extern int hostaclRefresh;

    for (chain=hostHashTable[index]; chain; chain=chain->next) {
	host = chain->hostPtr;
	assert(host);
        if (!(host->hostFlags & HOSTDELETED) && chain->addr == hostaddr
	    && host->port == hport) {
            now = FT_ApproxTime();		/* always evaluate "now" */
	    if (host->hcpsfailed || (host->cpsCall+hostaclRefresh < now )) {
		/*
		 * Every hostaclRefresh period (def 2 hrs) get the new membership list for the host.
		 * Note this could be the first time that the host is added to a group.
		 * Also here we also retry on previous legitimate hcps failures
		 */
		h_gethostcps_r(host,now);
	    }
	    break;
	}
	host = NULL;
    }
    return host;

} /*h_Lookup*/

/* Lookup a host given its UUID. */
struct host *h_LookupUuid_r(uuidp)
    afsUUID *uuidp;
{
    register struct host *host=0;
    register struct h_hashChain* chain;
    register index = h_UuidHashIndex(uuidp);

    for (chain=hostUuidHashTable[index]; chain; chain=chain->next) {
	host = chain->hostPtr;
	assert(host);
        if (!(host->hostFlags & HOSTDELETED) && host->interface
	 && afs_uuid_equal(&host->interface->uuid, uuidp)) {
	    break;
	}
	host = NULL;
    }
    return host;

} /*h_Lookup*/


/*
 * h_Hold: Establish a hold by the current LWP on this host--the host
 * or its clients will not be physically deleted until all holds have
 * been released.
 *
 * NOTE: h_Hold_r is a macro defined in host.h.
 */

int h_Hold(host)
    register struct host *host;
{
    H_LOCK
    h_Hold_r(host);
    H_UNLOCK
    return 0;
}


/* h_TossStuff:  Toss anything in the host structure (the host or
 * clients marked for deletion.  Called from r_Release ONLY.
 * To be called, there must be no holds, and either host->deleted
 * or host->clientDeleted must be set.
 */
h_TossStuff_r(host)
    register struct host *host;

{
    register struct client **cp, *client;
    int		i;

    /* if somebody still has this host held */
    for (i=0; (i<h_maxSlots)&&(!(host)->holds[i]); i++);
    if  (i!=h_maxSlots)
	return;

    /* ASSUMPTION: r_FreeConnection() does not yield */
    for (cp = &host->FirstClient; client = *cp; ) {
	if ((host->hostFlags & HOSTDELETED) || client->deleted) {
	    if ((client->ViceId != ANONYMOUSID) && client->CPS.prlist_val) {
		free(client->CPS.prlist_val);
                client->CPS.prlist_val = (afs_int32 *)0;
	    }
	    if (client->tcon) {
		rx_SetSpecific(client->tcon, rxcon_client_key, (void *)0);
	    }
	    CurrentConnections--;
	    *cp = client->next;
	    FreeCE(client);
	} else cp = &client->next;
    }
    if (host->hostFlags & HOSTDELETED) {
	register struct h_hashChain **hp, *th;
	register struct rx_connection *rxconn;
	afsUUID *uuidp;
    	afs_uint32 hostAddr;
	int i;

	if (host->Console & 1) Console--;
	if (rxconn = host->callback_rxcon) {
	    host->callback_rxcon = (struct rx_connection *)0;
	    /*
	     * If rx_DestroyConnection calls h_FreeConnection we will
	     * deadlock on the host_glock_mutex. Work around the problem
	     * by unhooking the client from the connection before
	     * destroying the connection.
	     */
	    client = rx_GetSpecific(rxconn, rxcon_client_key);
	    if (client && client->tcon == rxconn)
		client->tcon = NULL;
	    rx_SetSpecific(rxconn, rxcon_client_key, (void *)0);
	    rx_DestroyConnection(rxconn);
	}
	if (host->hcps.prlist_val)
	    free(host->hcps.prlist_val);
	host->hcps.prlist_val = (afs_int32 *)0;
	host->hcps.prlist_len = 0;
	DeleteAllCallBacks_r(host);
	host->hostFlags &= ~RESETDONE;	/* just to be safe */

	/* if alternate addresses do not exist */
	if ( !(host->interface) )
	{
		for (hp = &hostHashTable[h_HashIndex(host->host)];
			th = *hp; hp = &th->next) 
		{
	    		assert(th->hostPtr);
			if (th->hostPtr == host) 
			{
				*hp = th->next;
				h_DeleteList_r(host); 
				FreeHT(host);
				break;
			}		
		}
	}
	else 
	{
	    /* delete all hash entries for the UUID */
	    uuidp = &host->interface->uuid;
	    for (hp = &hostUuidHashTable[h_UuidHashIndex(uuidp)];
		 th = *hp; hp = &th->next) {
		assert(th->hostPtr);
		if (th->hostPtr == host)
		{
		    *hp = th->next;
		    free(th);
		    break;
		}
	    }
	    /* delete all hash entries for alternate addresses */
	    assert(host->interface->numberOfInterfaces > 0 );
	    for ( i=0; i < host->interface->numberOfInterfaces; i++)
	    {
		hostAddr = host->interface->addr[i];
		for (hp = &hostHashTable[h_HashIndex(hostAddr)];
	     		th = *hp; hp = &th->next) 
		{
	    		assert(th->hostPtr);
	    		if (th->hostPtr == host) 
			{
				*hp = th->next;
				free(th);
				break;
	    		}
		}
    	    }
	    free(host->interface);
	    host->interface = NULL;
	    h_DeleteList_r(host); /* remove host from global host List */
	    FreeHT(host);
	} 			/* if alternate address exists */
    } 
} /*h_TossStuff_r*/


/* Called by rx when a server connection disappears */
h_FreeConnection(tcon)
    struct rx_connection *tcon;

{
    register struct client *client;

    client = (struct client *) rx_GetSpecific(tcon, rxcon_client_key);
    if (client) {
	H_LOCK
	if (client->tcon == tcon)
	    client->tcon = (struct rx_connection *)0;
	H_UNLOCK
    }
} /*h_FreeConnection*/


/* h_Enumerate: Calls (*proc)(host, held, param) for at least each host in the
 * system at the start of the enumeration (perhaps more).  Hosts may be deleted
 * (have delete flag set); ditto for clients.  (*proc) is always called with
 * host h_held().  The hold state of the host with respect to this lwp is passed
 * to (*proc) as the param held.  The proc should return 0 if the host should be
 * released, 1 if it should be held after enumeration.
 */
h_Enumerate(proc, param)
    int (*proc)();
    char *param;

{
    register struct host *host, **list;
    register int *held;
    register int i, count;
    
    H_LOCK
    if (hostCount == 0) {
	H_UNLOCK
	return;
    }
    list = (struct host **)malloc(hostCount * sizeof(struct host *));
    assert(list != NULL);
    held = (int *)malloc(hostCount * sizeof(int));
    assert(held != NULL);
    for (count = 0, host = hostList ; host ; host = host->next, count++) {
	list[count] = host;
	if (!(held[count] = h_Held_r(host)))
	    h_Hold_r(host);
    }
    assert(count == hostCount);
    H_UNLOCK
    for ( i = 0 ; i < count ; i++) {
	held[i] = (*proc)(list[i], held[i], param);
	if (!held[i])
	    h_Release(list[i]);/* this might free up the host */
    }
    free((void *)list);
    free((void *)held);
} /*h_Enumerate*/

/* h_Enumerate_r: Calls (*proc)(host, held, param) for at least each host in
 * the at the start of the enumeration (perhaps more).  Hosts may be deleted
 * (have delete flag set); ditto for clients.  (*proc) is always called with
 * host h_held() and the global host lock (H_LOCK) locked.The hold state of the
 * host with respect to this lwp is passed to (*proc) as the param held.
 * The proc should return 0 if the host should be released, 1 if it should
 * be held after enumeration.
 */
h_Enumerate_r(proc, param)
    int (*proc)();
    char *param;

{
    register struct host *host;
    register int held;
    
    if (hostCount == 0) {
	return;
    }
    for (host = hostList ; host ; host = host->next) {
	if (!(held = h_Held_r(host)))
	    h_Hold_r(host);
	held = (*proc)(host, held, param);
	if (!held)
	    h_Release_r(host);/* this might free up the host */
    }
} /*h_Enumerate*/


/* Host is returned held */
struct host *h_GetHost_r(tcon)
    struct rx_connection *tcon;

{
    struct host *host;
    struct host *oldHost;
    int code;
    int held;
    struct interfaceAddr interf;
    int interfValid = 0;
    afs_int32	buffer[AFS_MAX_INTERFACE_ADDR];
    struct Identity *identP = NULL;
    afs_int32 haddr;
    afs_int32 hport;
    int i, j, count;

    haddr = rxr_HostOf(tcon);
    hport = rxr_PortOf(tcon);
retry:
    code = 0;
    identP = (struct Identity *)rx_GetSpecific(tcon, rxcon_ident_key);
    host = h_Lookup_r(haddr, hport);
    if (host && !identP && !(host->Console&1)) {
	/* This is a new connection, and we already have a host
	 * structure for this address. Verify that the identity
	 * of the caller matches the identity in the host structure.
	 */
	if (!(held = h_Held_r(host)))
		h_Hold_r(host);
	h_Lock_r(host);
	if ( !(host->hostFlags & ALTADDR) )
	{
		/* Another thread is doing initialization */
		h_Unlock_r(host);
		if ( !held) h_Release_r(host);
		ViceLog(125, ("Host %x starting h_Lookup again\n", host));
		goto retry;
	}
	host->hostFlags &= ~ALTADDR;
	H_UNLOCK
	code = RXAFSCB_WhoAreYou(host->callback_rxcon, &interf);
	H_LOCK
	if ( code == RXGEN_OPCODE ) {
		identP = (struct Identity *)malloc(1);
		identP->valid = 0;
		rx_SetSpecific(tcon, rxcon_ident_key, identP);
		/* The host on this connection was unable to respond to 
		 * the WhoAreYou. We will treat this as a new connection
		 * from the existing host. The worst that can happen is
		 * that we maintain some extra callback state information */
		if (host->interface) {
		    ViceLog(0,
			    ("Host %x used to support WhoAreYou, deleting.\n",
			    host));
		    host->hostFlags |= HOSTDELETED;
		    h_Unlock_r(host);
		    if (!held) h_Release_r(host);
		    host = NULL;
		    goto retry;
		}
	} else if (code == 0) {
		interfValid = 1;
		identP = (struct Identity *)malloc(sizeof(struct Identity));
		identP->valid = 1;
		identP->uuid = interf.uuid;
		rx_SetSpecific(tcon, rxcon_ident_key, identP);
		/* Check whether the UUID on this connection matches
		 * the UUID in the host structure. If they don't match
		 * then this is not the same host as before. */
		if ( !host->interface
		  || !afs_uuid_equal(&interf.uuid, &host->interface->uuid) ) {
		    ViceLog(25,
			    ("Host %x has changed its identity, deleting.\n",
			    host));
		    host->hostFlags |= HOSTDELETED;
		    h_Unlock_r(host);
		    if (!held) h_Release_r(host);
		    host = NULL;
		    goto retry;
		}
	} else {
	   	ViceLog(0,("CB: WhoAreYou failed for %x.%d, error %d\n", 
			host->host, host->port, code));
	        host->hostFlags |= VENUSDOWN;
	}
	host->hostFlags |= ALTADDR;
	h_Unlock_r(host);
    } else if (host) {
	if (!(held = h_Held_r(host)))
		h_Hold_r(host);
	if ( ! (host->hostFlags & ALTADDR) ) 
	{
        	/* another thread is doing the initialisation */
		ViceLog(125, ("Host %x waiting for host-init to complete\n",
				host));
		h_Lock_r(host);
		h_Unlock_r(host);
		if ( !held) h_Release_r(host);
		ViceLog(125, ("Host %x starting h_Lookup again\n", host));
		goto retry;
	}
	/* We need to check whether the identity in the host structure
	 * matches the identity on the connection. If they don't match
	 * then treat this a new host. */
	if ( !(host->Console&1)
	  && ( ( !identP->valid && host->interface )
	    || ( identP->valid && !host->interface )
	    || ( identP->valid
	      && !afs_uuid_equal(&identP->uuid, &host->interface->uuid) ) ) ) {
		/* The host in the cache is not the host for this connection */
		host->hostFlags |= HOSTDELETED;
		h_Unlock_r(host);
		if (!held) h_Release_r(host);
		ViceLog(0,("CB: new identity for host %x, deleting\n",
			   host->host));
		goto retry;
	}
    } else {
        host = h_Alloc_r(tcon);
        h_Hold_r(host);
        h_Lock_r(host);
	h_gethostcps_r(host,FT_ApproxTime());
        if (!(host->Console&1)) {
	    if (!identP || !interfValid) {
		H_UNLOCK
		code = RXAFSCB_WhoAreYou(host->callback_rxcon, &interf);
		H_LOCK
		if ( code == RXGEN_OPCODE ) {
		    identP = (struct Identity *)malloc(1);
		    identP->valid = 0;
		    rx_SetSpecific(tcon, rxcon_ident_key, identP);
		    ViceLog(25,
			    ("Host %x does not support WhoAreYou.\n",
			    host->host));
		    code = 0;
		} else if (code == 0) {
		    interfValid = 1;
		    identP = (struct Identity *)malloc(sizeof(struct Identity));
		    identP->valid = 1;
		    identP->uuid = interf.uuid;
		    rx_SetSpecific(tcon, rxcon_ident_key, identP);
		    ViceLog(25,("WhoAreYou success on %x\n", host->host));
		}
	    }
	    if (code == 0 && !identP->valid) {
	 	H_UNLOCK
		code = RXAFSCB_InitCallBackState(host->callback_rxcon);
	 	H_LOCK
	    } else if (code == 0) {
		oldHost = h_LookupUuid_r(&identP->uuid);
		if (oldHost) {
		    /* This is a new address for an existing host. Update
		     * the list of interfaces for the existing host and
		     * delete the host structure we just allocated. */
		    if (!(held = h_Held_r(oldHost)))
			h_Hold_r(oldHost);
		    h_Lock_r(oldHost);
		    ViceLog(25,("CB: new addr %x for old host %x\n",
				host->host, oldHost->host));
		    host->hostFlags |= HOSTDELETED;
		    h_Unlock_r(host);
		    h_Release_r(host);
		    host = oldHost;
		    addInterfaceAddr_r(host, haddr);
		} else {
		    /* This really is a new host */
		    hashInsertUuid_r(&identP->uuid, host);
		    H_UNLOCK
		    code = RXAFSCB_InitCallBackState3(host->callback_rxcon,
						      &FS_HostUUID);
		    H_LOCK
		    if (code == 0) {
			ViceLog(25,("InitCallBackState3 success on %x\n",
				host->host));
			assert(interfValid == 1);
			initInterfaceAddr_r(host, &interf);
		    }
		}
	   }
	   if (code) {
	   	ViceLog(0,("CB: RCallBackConnectBack failed for %x.%d\n", 
			host->host, host->port));
	        host->hostFlags |= VENUSDOWN;
	    }
	    else
		host->hostFlags |= RESETDONE;

        }
	host->hostFlags |= ALTADDR;/* host structure iniatilisation complete */
	h_Unlock_r(host);
    }
    return host;

} /*h_GetHost_r*/


static char localcellname[PR_MAXNAMELEN+1];
char local_realm[AFS_REALM_SZ] = "";

/* not reentrant */
void h_InitHostPackage()
{
    afsconf_GetLocalCell (confDir, localcellname, PR_MAXNAMELEN);
    if (!local_realm[0]) {
	if (afs_krb_get_lrealm(local_realm, 0) != 0/*KSUCCESS*/) {
	    ViceLog(0, ("afs_krb_get_lrealm failed, using %s.\n",localcellname));
	    strcpy (local_realm, localcellname);
	}
    }
    rxcon_ident_key = rx_KeyCreate((rx_destructor_t)free);
    rxcon_client_key = rx_KeyCreate((rx_destructor_t)0);
#ifdef AFS_PTHREAD_ENV
    assert(pthread_mutex_init(&host_glock_mutex, NULL) == 0);
#endif /* AFS_PTHREAD_ENV */
}

static MapName_r(aname, acell, aval)
    char *aname;
    char *acell;
    afs_int32 *aval;

{
    namelist lnames;
    idlist lids;
    afs_int32 code;
    afs_int32 anamelen, cnamelen;
    int foreign = 0;
    char *tname;

    anamelen=strlen(aname);
    if (anamelen >= PR_MAXNAMELEN)
	return -1; /* bad name -- caller interprets this as anonymous, but retries later */

    lnames.namelist_len = 1;
    lnames.namelist_val = (prname *) aname;  /* don't malloc in the common case */
    lids.idlist_len = 0;
    lids.idlist_val = (afs_int32 *) 0;

    cnamelen=strlen(acell);
    if (cnamelen) {
	if (strcasecmp(local_realm, acell) && strcasecmp(localcellname, acell))  {
	    ViceLog(2, ("MapName: cell is foreign.  cell=%s, localcell=%s, localrealm=%s\n",
			acell, localcellname, local_realm));
	    if ((anamelen+cnamelen+1) >= PR_MAXNAMELEN) {
		ViceLog(2, ("MapName: Name too long, using AnonymousID for %s@%s\n",
			    aname, acell));
		*aval = AnonymousID;
		return 0;
	    }		    
	    foreign = 1;  /* attempt cross-cell authentication */
	    tname = (char *) malloc(anamelen+cnamelen+2);
	    strcpy(tname, aname);
	    tname[anamelen] = '@';
	    strcpy(tname+anamelen+1, acell);
	    lnames.namelist_val = (prname *) tname;
	}
    }

    H_UNLOCK
    code = pr_NameToId(&lnames, &lids); 
    H_LOCK
    if (code == 0) {
       if (lids.idlist_val) {
	  *aval = lids.idlist_val[0];
	  if (*aval == AnonymousID) {
	     ViceLog(2, ("MapName: NameToId on %s returns anonymousID\n", lnames.namelist_val));
	  }
	  free(lids.idlist_val);  /* return parms are not malloced in stub if server proc aborts */
       } else {
	  ViceLog(0, ("MapName: NameToId on '%s' is unknown\n", lnames.namelist_val));
	  code = -1;
       }
    }

    if (foreign) {
	free(lnames.namelist_val);  /* We allocated this above, so we must free it now. */
    }
    return code;
}
/*MapName*/


/* NOTE: this returns the client with a Shared lock */
struct client *h_ID2Client(vid)
afs_int32 vid;
{
    register struct client *client;
    register struct host *host;

    H_LOCK

      for (host=hostList; host; host=host->next) {
        if (host->hostFlags & HOSTDELETED)
	  continue;
	for (client = host->FirstClient; client; client = client->next) {
	  if (!client->deleted && client->ViceId == vid) {
	    client->refCount++;
	    H_UNLOCK
	    ObtainSharedLock(&client->lock);
	    H_LOCK
	    client->refCount--;
	    H_UNLOCK
	    return client;
	  }
	}
      }

    H_UNLOCK
return 0;
}

/*
 * Called by the server main loop.  Returns a h_Held client, which must be
 * released later the main loop.  Allocates a client if the matching one
 * isn't around. The client is returned with its reference count incremented
 * by one. The caller must call h_ReleaseClient_r when finished with
 * the client.
 */
struct client *h_FindClient_r(tcon)
    struct rx_connection *tcon;

{
    register struct client *client;
    register struct host *host;
    struct client *oldClient;
    afs_int32 viceid;
    afs_int32 expTime;
    afs_int32 code;
    int authClass;
#if (64-MAXKTCNAMELEN)
ticket name length != 64
#endif
    char tname[64];
    char tinst[64];
    char uname[PR_MAXNAMELEN];
    char tcell[MAXKTCREALMLEN];
    int fail = 0;

    client = (struct client *) rx_GetSpecific(tcon, rxcon_client_key);
    if (client && !client->deleted) {
       client->refCount++;
       h_Hold_r(client->host);
       if (client->prfail != 2) {  /* Could add shared lock on client here */
	  /* note that we don't have to lock entry in this path to
	   * ensure CPS is initialized, since we don't call rxr_SetSpecific
	   * until initialization is done, and we only get here if
	   * rx_GetSpecific located the client structure.
	   */
	  return client;
       }
       H_UNLOCK
       ObtainWriteLock(&client->lock); /* released at end */
       H_LOCK
    } else if (client) {
       client->refCount++;
    }

    authClass = rx_SecurityClassOf((struct rx_connection *)tcon);
    ViceLog(5,("FindClient: authenticating connection: authClass=%d\n",
	       authClass));
    if (authClass == 1) {
       /* A bcrypt tickets, no longer supported */
       ViceLog(1, ("FindClient: bcrypt ticket, using AnonymousID\n"));
       viceid = AnonymousID;
       expTime = 0x7fffffff;
    } else if (authClass == 2) {
       afs_int32 kvno;

       /* kerberos ticket */
       code = rxkad_GetServerInfo (tcon, /*level*/0, &expTime,
				   tname, tinst, tcell, &kvno);
       if (code) {
	  ViceLog(1, ("Failed to get rxkad ticket info\n"));
	  viceid = AnonymousID;
	  expTime = 0x7fffffff;
       } else {
	  int ilen = strlen(tinst);
	  ViceLog(5,
		  ("FindClient: rxkad conn: name=%s,inst=%s,cell=%s,exp=%d,kvno=%d\n",
		   tname, tinst, tcell, expTime, kvno));
	  strncpy (uname, tname, sizeof(uname));
	  if (ilen) {
	     if (strlen(uname) + 1 + ilen >= sizeof(uname))
	        goto bad_name;
	     strcat (uname, ".");
	     strcat (uname, tinst);
	  }
	  /* translate the name to a vice id */
	  code = MapName_r(uname, tcell, &viceid);
	  if (code) {
	  bad_name:
	     ViceLog(1, ("failed to map name=%s, cell=%s -> code=%d\n",
			 uname, tcell, code));
	     fail = 1;
	     viceid = AnonymousID;
	     expTime = 0x7fffffff;
	  }
       }
    } else {
       viceid = AnonymousID;	/* unknown security class */
       expTime = 0x7fffffff;
    }

    if (!client) {
       host = h_GetHost_r(tcon); /* Returns it h_Held */

       /* First try to find the client structure */
       for (client = host->FirstClient; client; client = client->next) {
	  if (!client->deleted && (client->sid == rxr_CidOf(tcon)) &&
	                          (client->VenusEpoch == rxr_GetEpoch(tcon))) {
	     if (client->tcon && (client->tcon != tcon)) {
	        ViceLog(0, ("*** Vid=%d, sid=%x, tcon=%x, Tcon=%x ***\n", 
			    client->ViceId, client->sid, client->tcon, tcon));
		client->tcon = (struct rx_connection *)0;
	     }
	     client->refCount++;
	     H_UNLOCK
	     ObtainWriteLock(&client->lock);
	     H_LOCK
	     break;
	  }
       }

       /* Still no client structure - get one */
       if (!client) {
	  client = GetCE();
	  ObtainWriteLock(&client->lock);
	  client->host = host;
	  client->next = host->FirstClient;
	  host->FirstClient = client;
#if FS_STATS_DETAILED
	  client->InSameNetwork = host->InSameNetwork;
#endif /* FS_STATS_DETAILED */
	  client->ViceId = viceid;
	  client->expTime	= expTime;	/* rx only */
	  client->authClass = authClass;	/* rx only */
	  client->sid = rxr_CidOf(tcon);
	  client->VenusEpoch = rxr_GetEpoch(tcon);
	  client->CPS.prlist_val = 0;
	  client->refCount = 1;
	  CurrentConnections++;	/* increment number of connections */
       }
    }
    client->prfail = fail;

    if (!(client->CPS.prlist_val) || (viceid != client->ViceId)) {
	if (client->CPS.prlist_val && (client->ViceId != ANONYMOUSID)) {
	   free(client->CPS.prlist_val);
	}
	client->CPS.prlist_val = (afs_int32 *)0;
        client->ViceId = viceid;
	client->expTime	= expTime;

	if (viceid == ANONYMOUSID) {
	  client->CPS.prlist_len = AnonCPS.prlist_len;
	  client->CPS.prlist_val = AnonCPS.prlist_val;
	} else {
	  H_UNLOCK
	  code = pr_GetCPS(viceid, &client->CPS);
	  H_LOCK
	  if (code) {
	    ViceLog(0, ("pr_GetCPS failed(%d) for user %d, host %x.%d\n",
		    code, viceid, client->host->host, ntohs(client->host->port)));

	    /* Although ubik_Call (called by pr_GetCPS) traverses thru
	     * all protection servers and reevaluates things if no
	     * sync server or quorum is found we could still end up
	     * with one of these errors. In such case we would like to
	     * reevaluate the rpc call to find if there's cps for this
	     * guy. We treat other errors (except network failures
	     * ones - i.e. code < 0) as an indication that there is no
	     * CPS for this host.  Ideally we could like to deal this
	     * problem the other way around (i.e.  if code == NOCPS
	     * ignore else retry next time) but the problem is that
	     * there're other errors (i.e.  EPERM) for which we don't
	     * want to retry and we don't know the whole code list!
	     */
	    if (code < 0 || code == UNOQUORUM || code == UNOTSYNC) 
		client->prfail = 1;
	  }
	}
	/* the disabling of system:administrators is so iffy and has so many
	 * possible failure modes that we will disable it again */
	/* Turn off System:Administrator for safety  
	   if (AL_IsAMember(SystemId, client->CPS) == 0)
	   assert(AL_DisableGroup(SystemId, client->CPS) == 0); */
    }

    /* Now, tcon may already be set to a rock, since we blocked with no host
     * or client locks set above in pr_GetCPS (XXXX some locking is probably
     * required).  So, before setting the RPC's rock, we should disconnect
     * the RPC from the other client structure's rock.
     */
    if (oldClient = (struct client *) rx_GetSpecific(tcon, rxcon_client_key)) {
	oldClient->tcon = (struct rx_connection *) 0;
	/* rx_SetSpecific will be done immediately below */
    }
    client->tcon = tcon;
    rx_SetSpecific(tcon, rxcon_client_key, client);
    ReleaseWriteLock(&client->lock);

    return client;

} /*h_FindClient_r*/

int h_ReleaseClient_r(client)
    struct client *client;
{
    assert(client->refCount > 0);
    client->refCount--;
    return 0;
}


/*
 * Sigh:  this one is used to get the client AGAIN within the individual
 * server routines.  This does not bother h_Holding the host, since
 * this is assumed already have been done by the server main loop.
 * It does check tokens, since only the server routines can return the
 * VICETOKENDEAD error code
 */
int GetClient(tcon, cp)
    struct rx_connection * tcon;
    struct client **cp;

{
    register struct client *client;

    H_LOCK

    *cp = client = (struct client *) rx_GetSpecific(tcon, rxcon_client_key);
    /* XXXX debug */
    assert(client && client->tcon && rxr_CidOf(client->tcon) == client->sid);
    if (client &&
	client->LastCall > client->expTime && client->expTime) {
	ViceLog(1, ("Token for %s at %x.%d expired %d\n",
		h_UserName(client), client->host->host, client->host->port, client->expTime));
	H_UNLOCK
	return VICETOKENDEAD;
    }

    H_UNLOCK
    return 0;

} /*GetClient*/


/* Client user name for short term use.  Note that this is NOT inexpensive */
char *h_UserName(client)
    struct client *client;

{
    static char User[PR_MAXNAMELEN+1];
    namelist lnames;
    idlist lids;

    lids.idlist_len = 1;
    lids.idlist_val = (afs_int32 *)malloc(1*sizeof(afs_int32));
    lnames.namelist_len = 0;
    lnames.namelist_val = (prname *)0;
    lids.idlist_val[0] = client->ViceId;
    if (pr_IdToName(&lids,&lnames)) {
	/* We need to free id we alloced above! */
	free(lids.idlist_val);
	return "*UNKNOWN USER NAME*";
    }
    strncpy(User,lnames.namelist_val[0],PR_MAXNAMELEN);
    free(lids.idlist_val);
    free(lnames.namelist_val);
    return User;

} /*h_UserName*/


h_PrintStats()

{
    ViceLog(0,
	    ("Total Client entries = %d, blocks = %d; Host entries = %d, blocks = %d\n",
	    CEs, CEBlocks, HTs, HTBlocks));

} /*h_PrintStats*/


static int h_PrintClient(host, held, file)
    register struct host *host;
    int held;
    StreamHandle_t *file;
{
    register struct client *client;
    int i;
    char tmpStr[256];
    char tbuffer[32];

    H_LOCK
    if (host->hostFlags & HOSTDELETED) {
	H_UNLOCK
	return held;
    }
    sprintf(tmpStr,"Host %x.%d down = %d, LastCall %s", host->host,
	    host->port, (host->hostFlags & VENUSDOWN),
	    afs_ctime((time_t *)&host->LastCall, tbuffer, sizeof(tbuffer)));
    STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
    for (client = host->FirstClient; client; client=client->next) {
	if (!client->deleted) {
	    if (client->tcon) {
		sprintf(tmpStr, "    user id=%d,  name=%s, sl=%s till %s",
			client->ViceId, h_UserName(client),
			client->authClass ? "Authenticated" : "Not authenticated",
			client->authClass ?
			afs_ctime((time_t *)&client->expTime, tbuffer, sizeof(tbuffer))
			: "No Limit\n");
		STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
	    }
	    else {
		sprintf(tmpStr, "    user=%s, no current server connection\n",
			h_UserName(client));
		STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
	    }
	    sprintf(tmpStr, "      CPS-%d is [", client->CPS.prlist_len);
	    STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
	    if (client->CPS.prlist_val) {
		for (i=0; i > client->CPS.prlist_len; i++) {
		    sprintf(tmpStr, " %d", client->CPS.prlist_val[i]);
		    STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
		}
	    }
	    sprintf(tmpStr, "]\n");	    
	    STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
	}
    }
    H_UNLOCK
    return held;

} /*h_PrintClient*/



/*
 * Print a list of clients, with last security level and token value seen,
 * if known
 */
h_PrintClients()

{
    time_t now;
    char tmpStr[256];
    char tbuffer[32];

    StreamHandle_t *file = STREAM_OPEN(AFSDIR_SERVER_CLNTDUMP_FILEPATH, "w");

    if (file == NULL) {
	ViceLog(0, ("Couldn't create client dump file %s\n", AFSDIR_SERVER_CLNTDUMP_FILEPATH));
	return;
    }
    now = FT_ApproxTime();
    sprintf(tmpStr, "List of active users at %s\n",
	    afs_ctime(&now, tbuffer, sizeof(tbuffer)));
    STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
    h_Enumerate(h_PrintClient, (char *)file);
    STREAM_REALLYCLOSE(file);
    ViceLog(0, ("Created client dump %s\n", AFSDIR_SERVER_CLNTDUMP_FILEPATH));
}




static int h_DumpHost(host, held, file)
    register struct host *host;
    int held;
    StreamHandle_t *file;

{
    int i;
    char tmpStr[256];

    H_LOCK
    sprintf(tmpStr, "ip:%x holds:%d port:%d hidx:%d cbid:%d lock:%x last:%u active:%u down:%d del:%d cons:%d cldel:%d\n\t hpfailed:%d hcpsCall:%u hcps [",
	    host->host, host->holds, host->port, host->index, host->cblist,
	    CheckLock(&host->lock), host->LastCall, host->ActiveCall, 
	    (host->hostFlags & VENUSDOWN), host->hostFlags&HOSTDELETED, 
	    host->Console, host->hostFlags & CLIENTDELETED, 
	    host->hcpsfailed, host->cpsCall);
    STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
    if (host->hcps.prlist_val)
	for (i=0; i < host->hcps.prlist_len; i++) {
	    sprintf(tmpStr, " %d", host->hcps.prlist_val[i]);
	    STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
	}
    sprintf(tmpStr, "] [");
    STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
    if ( host->interface)
	for (i=0; i < host->interface->numberOfInterfaces; i++) {
	    sprintf(tmpStr, " %x", host->interface->addr[i]);
	    STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
	}
    sprintf(tmpStr, "]\n");
    STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
    H_UNLOCK
    return held;

} /*h_DumpHost*/


h_DumpHosts()

{
    time_t now;
    StreamHandle_t *file = STREAM_OPEN(AFSDIR_SERVER_HOSTDUMP_FILEPATH, "w");
    char tmpStr[256];
    char tbuffer[32];

    if (file == NULL) {
	ViceLog(0, ("Couldn't create host dump file %s\n", AFSDIR_SERVER_HOSTDUMP_FILEPATH));
	return;
    }
    now = FT_ApproxTime();
    sprintf(tmpStr, "List of active hosts at %s\n",
	    afs_ctime(&now, tbuffer, sizeof(tbuffer)));
    STREAM_WRITE(tmpStr, strlen(tmpStr), 1, file);
    h_Enumerate(h_DumpHost, (char *) file);
    STREAM_REALLYCLOSE(file);
    ViceLog(0, ("Created host dump %s\n", AFSDIR_SERVER_HOSTDUMP_FILEPATH));

} /*h_DumpHosts*/


/*
 * This counts the number of workstations, the number of active workstations,
 * and the number of workstations declared "down" (i.e. not heard from
 * recently).  An active workstation has received a call since the cutoff
 * time argument passed.
 */
h_GetWorkStats(nump, activep, delp, cutofftime)
    int *nump;
    int *activep;
    int *delp;
    afs_int32 cutofftime;

{
    register int i;
    register struct host *host;
    register int num=0, active=0, del=0;

    H_LOCK
    for (host = hostList; host; host = host->next) {
	    if (!(host->hostFlags & HOSTDELETED)) {
		num++;
		if (host->ActiveCall > cutofftime)
		    active++;
		if (host->hostFlags & VENUSDOWN)
		    del++;
	    }
    }
    H_UNLOCK
    if (nump)
	*nump = num;
    if (activep)
	*activep = active;
    if (delp)
	*delp = del;

} /*h_GetWorkStats*/


/*------------------------------------------------------------------------
 * PRIVATE h_ClassifyAddress
 *
 * Description:
 *	Given a target IP address and a candidate IP address (both
 *	in host byte order), classify the candidate into one of three
 *	buckets in relation to the target by bumping the counters passed
 *	in as parameters.
 *
 * Arguments:
 *	a_targetAddr	   : Target address.
 *	a_candAddr	   : Candidate address.
 *	a_sameNetOrSubnetP : Ptr to counter to bump when the two
 *			     addresses are either in the same network
 *			     or the same subnet.
 *	a_diffSubnetP	   : ...when the candidate is in a different
 *			     subnet.
 *	a_diffNetworkP	   : ...when the candidate is in a different
 *			     network.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	The target and candidate addresses are both in host byte
 *	order, NOT network byte order, when passed in.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static void h_ClassifyAddress(a_targetAddr, a_candAddr, a_sameNetOrSubnetP,
		       a_diffSubnetP, a_diffNetworkP)
    afs_uint32 a_targetAddr;
    afs_uint32 a_candAddr;
    afs_int32 *a_sameNetOrSubnetP;
    afs_int32 *a_diffSubnetP;
    afs_int32 *a_diffNetworkP;

{ /*h_ClassifyAddress*/

    register int i;			 /*Iterator thru host hash table*/
    register struct host *hostP;	 /*Ptr to current host entry*/
    register afs_uint32 currHostAddr; /*Current host address*/
    afs_uint32 targetNet;
    afs_uint32 targetSubnet;
    afs_uint32 candNet;
    afs_uint32 candSubnet;

    /*
     * Put bad values into the subnet info to start with.
     */
    targetSubnet = (afs_uint32) 0;
    candSubnet   = (afs_uint32) 0;

    /*
     * Pull out the network and subnetwork numbers from the target
     * and candidate addresses.  We can short-circuit this whole
     * affair if the target and candidate addresses are not of the
     * same class.
     */
    if (IN_CLASSA(a_targetAddr)) {
	if (!(IN_CLASSA(a_candAddr))) {
	    (*a_diffNetworkP)++;
	    return;
	}
	targetNet = a_targetAddr & IN_CLASSA_NET;
	candNet   = a_candAddr   & IN_CLASSA_NET;
	if (IN_SUBNETA(a_targetAddr))
	    targetSubnet = a_targetAddr & IN_CLASSA_SUBNET;
	if (IN_SUBNETA(a_candAddr))
	    candSubnet = a_candAddr & IN_CLASSA_SUBNET;
    }
    else
	if (IN_CLASSB(a_targetAddr)) {
	    if (!(IN_CLASSB(a_candAddr))) {
		(*a_diffNetworkP)++;
		return;
	    }
	    targetNet = a_targetAddr & IN_CLASSB_NET;
	    candNet   = a_candAddr   & IN_CLASSB_NET;
	    if (IN_SUBNETB(a_targetAddr))
		targetSubnet = a_targetAddr & IN_CLASSB_SUBNET;
	    if (IN_SUBNETB(a_candAddr))
		candSubnet = a_candAddr & IN_CLASSB_SUBNET;
	} /*Class B target*/
	else
	    if (IN_CLASSC(a_targetAddr)) {
		if (!(IN_CLASSC(a_candAddr))) {
		    (*a_diffNetworkP)++;
		    return;
		}
		targetNet = a_targetAddr & IN_CLASSC_NET;
		candNet   = a_candAddr   & IN_CLASSC_NET;

		/*
		 * Note that class C addresses can't have subnets,
		 * so we leave the defaults untouched.
		 */
	    } /*Class C target*/
	    else {
		targetNet = a_targetAddr;
		candNet = a_candAddr;
	    } /*Class D address*/
    
    /*
     * Now, simply compare the extracted net and subnet values for
     * the two addresses (which at this point are known to be of the
     * same class)
     */
    if (targetNet == candNet) {
	if (targetSubnet == candSubnet)
	    (*a_sameNetOrSubnetP)++;
	else
	    (*a_diffSubnetP)++;
    }
    else
	(*a_diffNetworkP)++;

} /*h_ClassifyAddress*/


/*------------------------------------------------------------------------
 * EXPORTED h_GetHostNetStats
 *
 * Description:
 *	Iterate through the host table, and classify each (non-deleted)
 *	host entry into ``proximity'' categories (same net or subnet,
 *	different subnet, different network).
 *
 * Arguments:
 *	a_numHostsP	   : Set to total number of (non-deleted) hosts.
 *	a_sameNetOrSubnetP : Set to # hosts on same net/subnet as server.
 *	a_diffSubnetP	   : Set to # hosts on diff subnet as server.
 *	a_diffNetworkP	   : Set to # hosts on diff network as server.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	We only count non-deleted hosts.  The storage pointed to by our
 *	parameters is zeroed upon entry.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void h_GetHostNetStats(a_numHostsP, a_sameNetOrSubnetP, a_diffSubnetP,
		       a_diffNetworkP)
    afs_int32 *a_numHostsP;
    afs_int32 *a_sameNetOrSubnetP;
    afs_int32 *a_diffSubnetP;
    afs_int32 *a_diffNetworkP;

{ /*h_GetHostNetStats*/

    register struct host *hostP;	 /*Ptr to current host entry*/
    register afs_uint32 currAddr_HBO; /*Curr host addr, host byte order*/

    /*
     * Clear out the storage pointed to by our parameters.
     */
    *a_numHostsP	= (afs_int32) 0;
    *a_sameNetOrSubnetP	= (afs_int32) 0;
    *a_diffSubnetP	= (afs_int32) 0;
    *a_diffNetworkP	= (afs_int32) 0;

    H_LOCK
    for (hostP = hostList; hostP; hostP = hostP->next) {
	    if (!(hostP->hostFlags & HOSTDELETED)) {
		/*
		 * Bump the number of undeleted host entries found.
		 * In classifying the current entry's address, make
		 * sure to first convert to host byte order.
		 */
		(*a_numHostsP)++;
		currAddr_HBO = (afs_uint32)ntohl(hostP->host);
		h_ClassifyAddress(FS_HostAddr_HBO,
				  currAddr_HBO,
				  a_sameNetOrSubnetP,
				  a_diffSubnetP,
				  a_diffNetworkP);
	    } /*Only look at non-deleted hosts*/
    } /*For each host record hashed to this index*/
    H_UNLOCK

} /*h_GetHostNetStats*/

static afs_uint32	checktime;
static afs_uint32    clientdeletetime;
static struct AFSFid zerofid;


/*
 * XXXX: This routine could use Multi-R to avoid serializing the timeouts.
 * Since it can serialize them, and pile up, it should be a separate LWP
 * from other events.
 */
int CheckHost(host, held)
    register struct host *host;
    int held;

{
    register struct client *client;
    struct interfaceAddr interf;
    int code;

    /* Host is held by h_Enumerate */
    H_LOCK
    for (client = host->FirstClient; client; client = client->next) {
        if (client->refCount == 0 && client->LastCall < clientdeletetime) {
            client->deleted = 1;
            host->hostFlags  |= CLIENTDELETED;
        }
    }
    if (host->LastCall < checktime) {
	h_Lock_r(host);
	if (!(host->hostFlags & HOSTDELETED)) {
	    if (host->LastCall < clientdeletetime) {
		host->hostFlags |= HOSTDELETED;
		if (!(host->hostFlags & VENUSDOWN)) {
		    host->hostFlags &= ~ALTADDR; /* alternate address invalid*/
		    if (host->interface) {
			H_UNLOCK
			code = RXAFSCB_InitCallBackState3(host->callback_rxcon,
							  &FS_HostUUID);
			H_LOCK
		    } else {
			H_UNLOCK
			code = RXAFSCB_InitCallBackState(host->callback_rxcon);
			H_LOCK
		    }
		    host->hostFlags |= ALTADDR; /* alternate addresses valid */
		    if ( code )
		    {
		    	ViceLog(0,
			    ("CB: RCallBackConnectBack (host.c) failed for host %x.%d\n",
			    host->host, host->port));
		    	host->hostFlags |= VENUSDOWN;
		     }
		    /* Note:  it's safe to delete hosts even if they have call
		     * back state, because break delayed callbacks (called when a
		     * message is received from the workstation) will always send a 
		     * break all call backs to the workstation if there is no
		     *callback.
		     */
		}
	    }
	    else {
		if (!(host->hostFlags & VENUSDOWN) && host->cblist) {
		    if (host->interface) {
			afsUUID uuid = host->interface->uuid;
			H_UNLOCK
			code = RXAFSCB_ProbeUuid(host->callback_rxcon, &uuid);
			H_LOCK
			if(code) {
			    if ( MultiProbeAlternateAddress_r(host) ) {
		    		ViceLog(0,
					("ProbeUuid failed for host %x.%d\n",
					 host->host, host->port));
		        	host->hostFlags |= VENUSDOWN;
			    }
			}
		    } else {
			H_UNLOCK
			code = RXAFSCB_Probe(host->callback_rxcon);
			H_LOCK
			if (code) {
		    	    ViceLog(0, ("ProbeUuid failed for host %x.%d\n",
				    host->host, host->port));
		            host->hostFlags |= VENUSDOWN;
			}
		    }
		}
	    }
	}
	h_Unlock_r(host);
    }
    H_UNLOCK
    return held;

} /*CheckHost*/


/*
 * Set VenusDown for any hosts that have not had a call in 15 minutes and
 * don't respond to a probe.  Note that VenusDown can only be cleared if
 * a message is received from the host (see ServerLWP in file.c).
 * Delete hosts that have not had any calls in 1 hour, clients that
 * have not had any calls in 15 minutes.
 *
 * This routine is called roughly every 5 minutes.
 */
h_CheckHosts() {

    afs_uint32 now = FT_ApproxTime();

    bzero((char *)&zerofid, sizeof(zerofid));
    /*
     * Send a probe to the workstation if it hasn't been heard from in
     * 15 minutes
     */
    checktime = now - 15*60;
    clientdeletetime = now - 120*60;	/* 2 hours ago */
    h_Enumerate(CheckHost, (char *) 0);

} /*h_CheckHosts*/

/*
 * This is called with host locked and held. At this point, the
 * hostHashTable should not be having entries for the alternate
 * interfaces. This function has to insert these entries in the
 * hostHashTable.
 *
 * The addresses in the ineterfaceAddr list are in host byte order.
 */
int
initInterfaceAddr_r(host, interf)
struct host*	host;
struct interfaceAddr *interf;
{
	int i, j;
	int number, count;
	afs_int32		myPort, myHost;
	int found;
	struct Interface *interface;

	assert(host);
	assert(interf);

	ViceLog(125,("initInterfaceAddr : host %x numAddr %d\n",
		host->host, interf->numberOfInterfaces));

	number = interf->numberOfInterfaces;
	myPort   = host->port;
	myHost   = host->host; /* current interface address */

	/* validation checks */
	if ( number < 0 )
        {
		ViceLog(0,("Number of alternate addresses returned is %d\n",
			 number));
		return  -1;
	}

	/*
	 * Convert IP addresses to network byte order, and remove for
	 * duplicate IP addresses from the interface list.
	 */
	for (i = 0, count = 0, found = 0; i < number; i++)
	{
	    interf->addr_in[i] = htonl(interf->addr_in[i]);
	    for (j = 0 ; j < count ; j++) {
		if (interf->addr_in[j] == interf->addr_in[i])
		    break;
	    }
	    if (j == count) {
		interf->addr_in[count] = interf->addr_in[i];
		if (interf->addr_in[count] == myHost)
		    found = 1;
		count++;
	    }
	}

	/*
	 * Allocate and initialize an interface structure for this host.
	 */
	if (found) {
	    interface = (struct Interface *)
			malloc(sizeof(struct Interface) +
			       (sizeof(afs_int32) * (count-1)));
	    assert(interface);
	    interface->numberOfInterfaces = count;
	} else {
	    interface = (struct Interface *)
			malloc(sizeof(struct Interface) +
			       (sizeof(afs_int32) * count));
	    assert(interface);
	    interface->numberOfInterfaces = count + 1;
	    interface->addr[count] = myHost;
	}
	interface->uuid = interf->uuid;
	for (i = 0 ; i < count ; i++)
	    interface->addr[i] = interf->addr_in[i];

	assert(!host->interface);
	host->interface = interface;

	for ( i=0; i < host->interface->numberOfInterfaces; i++)
	{
		ViceLog(125,("--- alt address %x\n", host->interface->addr[i]));
	}

	return 0;
}

/*
 * This is called with host locked and held. At this point, the
 * hostHashTable should not be having entries for the alternate
 * interfaces. This function has to insert these entries in the
 * hostHashTable.
 *
 * All addresses are in network byte order.
 */
int
addInterfaceAddr_r(host, addr)
struct host*	host;
afs_int32 addr;
{
	int i;
	int number;
	int found;
	struct Interface *interface;

	assert(host);
	assert(host->interface);

	ViceLog(125,("addInterfaceAddr : host %x addr %d\n",
		host->host, addr));

	/*
	 * Make sure this address is on the list of known addresses
	 * for this host.
	 */
	number = host->interface->numberOfInterfaces;
	for ( i=0, found=0; i < number && !found; i++)
	{
	    if ( host->interface->addr[i] == addr)
		found = 1;
	}
	if (!found) {
	    interface = (struct Interface *)
			malloc(sizeof(struct Interface) +
			       (sizeof(afs_int32) * number));
	    interface->numberOfInterfaces = number + 1;
	    interface->uuid = host->interface->uuid;
	    for (i = 0 ; i < number ; i++)
		interface->addr[i] = host->interface->addr[i];
	    interface->addr[number] = addr;
	    free(host->interface);
	    host->interface = interface;
	}

	/*
	 * Create a hash table entry for this address
	 */
	hashInsert_r(addr, host);

	return 0;
}

/* inserts  a new HashChain structure corresponding to this address */
hashInsert_r(addr, host)
afs_int32 addr;
struct host* host;
{
	int index;
	struct h_hashChain*	chain;

	/* hash into proper bucket */
	index = h_HashIndex(addr);

        /* insert into beginning of list for this bucket */
	chain = (struct h_hashChain *)malloc(sizeof(struct h_hashChain));
	assert(chain);
	chain->hostPtr = host;
	chain->next = hostHashTable[index];
	chain->addr = addr;
	hostHashTable[index] = chain;

}

/* inserts  a new HashChain structure corresponding to this UUID */
hashInsertUuid_r(uuid, host)
struct afsUUID *uuid;
struct host* host;
{
	int index;
	struct h_hashChain*	chain;

	/* hash into proper bucket */
	index = h_UuidHashIndex(uuid);

        /* insert into beginning of list for this bucket */
	chain = (struct h_hashChain *)malloc(sizeof(struct h_hashChain));
	assert(chain);
	chain->hostPtr = host;
	chain->next = hostUuidHashTable[index];
	hostUuidHashTable[index] = chain;
}

/* deleted a HashChain structure for this address and host */
/* returns 1 on success */
int
hashDelete_r(addr, host)
afs_int32 addr;
struct host* host;
{
	int flag;
	int index;
	register struct h_hashChain **hp, *th;

        for (hp = &hostHashTable[h_HashIndex(addr)]; th = *hp; )
        {
        	assert(th->hostPtr);
        	if (th->hostPtr == host && th->addr == addr)
        	{
                	*hp = th->next;
                	free(th);
                	flag = 1;
			break;
        	} else {
			hp = &th->next;
		}
	}
	return flag;
}


/*
** prints out all alternate interface address for the host. The 'level'
** parameter indicates what level of debugging sets this output
*/
printInterfaceAddr(host, level)
struct host*	host;
int 		level;
{
	int i, number;
        if ( host-> interface )
        {
                /* check alternate addresses */
                number = host->interface->numberOfInterfaces;
                assert( number > 0 );
                for ( i=0; i < number; i++)
			ViceLog(level, ("%x ", host->interface->addr[i]));
        }
	 ViceLog(level, ("\n"));
}

