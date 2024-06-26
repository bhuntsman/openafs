/*
 * (C) Copyright 1990 Transarc Corporation
 * Licensed materials - property of Transarc
 * All rights reserved.
 *
 * For copyright information, see IPL which you accepted in order to
 * download this software.
 *
 *
 * afs_callback.c:
 *	Exported routines (and their private support) to implement
 *	the callback RPC interface.
 */

#include "../afs/param.h"       /*Should be always first*/
#include "../afs/sysincludes.h" /*Standard vendor system headers*/
#include "../afs/afsincludes.h" /*AFS-based standard headers*/
#include "../afs/afs_stats.h"	/*Cache Manager stats*/
#include "../afs/afs_args.h"

extern struct volume *afs_volumes[NVOLS];   /* volume hash table */
extern void afs_DequeueCallback();
extern void afs_ComputePAGStats();
afs_int32 afs_allCBs	= 0;		/*Break callbacks on all objects */
afs_int32 afs_oddCBs	= 0;		/*Break callbacks on dirs*/
afs_int32 afs_evenCBs = 0;		/*Break callbacks received on files*/
afs_int32 afs_allZaps = 0;		/*Objects entries deleted */
afs_int32 afs_oddZaps = 0;		/*Dir cache entries deleted*/
afs_int32 afs_evenZaps = 0;		/*File cache entries deleted*/
afs_int32 afs_connectBacks = 0;
extern struct rx_service *afs_server;

extern afs_lock_t afs_xvcb, afs_xbrs, afs_xdcache;
extern afs_rwlock_t afs_xvcache, afs_xserver,  afs_xcell,  afs_xuser;
extern afs_rwlock_t afs_xvolume, afs_puttofileLock, afs_ftf, afs_xinterface;
extern afs_rwlock_t afs_xconn;
extern struct afs_lock afs_xaxs;
extern afs_lock_t afs_xcbhash;
extern struct srvAddr *afs_srvAddrs[NSERVERS];
extern struct afs_q CellLRU;
extern struct cm_initparams cm_initParams;

/*
 * Some debugging aids.
 */
static struct ltable {
    char *name;
    char *addr;
} ltable []= {
    "afs_xvcache", (char *)&afs_xvcache,
    "afs_xdcache", (char *)&afs_xdcache,
    "afs_xserver", (char *)&afs_xserver,
    "afs_xvcb",    (char *)&afs_xvcb,
    "afs_xbrs",    (char *)&afs_xbrs,
    "afs_xcell",   (char *)&afs_xcell,
    "afs_xconn",   (char *)&afs_xconn,
    "afs_xuser",   (char *)&afs_xuser,
    "afs_xvolume", (char *)&afs_xvolume,
    "puttofile",   (char *)&afs_puttofileLock,
    "afs_ftf",     (char *)&afs_ftf,
    "afs_xcbhash", (char *)&afs_xcbhash,
    "afs_xaxs",    (char *)&afs_xaxs,
    "afs_xinterface", (char *)&afs_xinterface,
};
unsigned long  lastCallBack_vnode;
unsigned int   lastCallBack_dv;
osi_timeval_t  lastCallBack_time;

/* these are for storing alternate interface addresses */
struct interfaceAddr afs_cb_interface;

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetCE
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement pulling out the contents of the i'th cache entry.
 *
 * Arguments:
 *	a_call   : Ptr to Rx call on which this request came in.
 *	a_index  : Index of desired cache entry.
 *	a_result : Ptr to a buffer for the given cache entry.
 *
 * Returns:
 *	0 if everything went fine,
 *	1 if we were given a bad index.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_GetCE(a_call, a_index, a_result)
    struct rx_call *a_call;
    afs_int32 a_index;
    struct AFSDBCacheEntry *a_result;

{ /*SRXAFSCB_GetCE*/

    register int i;			/*Loop variable*/
    register struct vcache *tvc;	/*Ptr to current cache entry*/
    int code;				/*Return code*/
    XSTATS_DECLS;

#ifdef RX_ENABLE_LOCKS
    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */

    XSTATS_START_CMTIME(AFS_STATS_CM_RPCIDX_GETCE);

    AFS_STATCNT(SRXAFSCB_GetCE);
    for (i = 0; i < VCSIZE; i++) {
	for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
	    if (a_index == 0)
		goto searchDone;
	    a_index--;
	} /*Zip through current hash chain*/
    } /*Zip through hash chains*/

  searchDone:
    if (tvc == (struct vcache *) 0) {
	/*Past EOF*/
	code = 1;
	goto fcnDone;
    }

    /*
     * Copy out the located entry.
     */
    a_result->addr = afs_data_pointer_to_int32(tvc);
    a_result->cell = tvc->fid.Cell;
    a_result->netFid.Volume = tvc->fid.Fid.Volume;
    a_result->netFid.Vnode = tvc->fid.Fid.Vnode;
    a_result->netFid.Unique = tvc->fid.Fid.Unique;
    a_result->lock.waitStates = tvc->lock.wait_states;
    a_result->lock.exclLocked = tvc->lock.excl_locked;
    a_result->lock.readersReading = tvc->lock.readers_reading;
    a_result->lock.numWaiting = tvc->lock.num_waiting;
#if defined(INSTRUMENT_LOCKS)
    a_result->lock.pid_last_reader = tvc->lock.pid_last_reader;
    a_result->lock.pid_writer = tvc->lock.pid_writer;
    a_result->lock.src_indicator = tvc->lock.src_indicator;
#else
    /* On osf20 , the vcache does not maintain these three fields */
    a_result->lock.pid_last_reader = 0;
    a_result->lock.pid_writer = 0;
    a_result->lock.src_indicator = 0;
#endif /* AFS_OSF20_ENV */
    a_result->Length = tvc->m.Length;
    a_result->DataVersion = hgetlo(tvc->m.DataVersion);
    a_result->callback = afs_data_pointer_to_int32(tvc->callback);		/* XXXX Now a pointer; change it XXXX */
    a_result->cbExpires = tvc->cbExpires;
    a_result->refCount = tvc->vrefCount;
    a_result->opens = tvc->opens;
    a_result->writers = tvc->execsOrWriters;
    a_result->mvstat = tvc->mvstat;
    a_result->states = tvc->states;
    code = 0;

    /*
     * Return our results.
     */
fcnDone:
    XSTATS_END_TIME;

#ifdef RX_ENABLE_LOCKS
    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */

    return(code);

} /*SRXAFSCB_GetCE*/


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetLock
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement pulling out the contents of a lock in the lock
 *	table.
 *
 * Arguments:
 *	a_call   : Ptr to Rx call on which this request came in.
 *	a_index  : Index of desired lock.
 *	a_result : Ptr to a buffer for the given lock.
 *
 * Returns:
 *	0 if everything went fine,
 *	1 if we were given a bad index.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_GetLock (a_call, a_index, a_result)
    struct rx_call *a_call;
    afs_int32 a_index;
    struct AFSDBLock *a_result;

{ /*SRXAFSCB_GetLock*/

    struct ltable *tl;		/*Ptr to lock table entry*/
    int nentries;		/*Num entries in table*/
    int code;			/*Return code*/
    XSTATS_DECLS;

#ifdef RX_ENABLE_LOCKS
    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */

    XSTATS_START_CMTIME(AFS_STATS_CM_RPCIDX_GETLOCK);
    
    AFS_STATCNT(SRXAFSCB_GetLock);
    nentries = sizeof(ltable)/sizeof(struct ltable);
    if (a_index < 0 || a_index >= nentries) {
	/*
	  * Past EOF
	  */
	code = 1;
    }
    else {
	/*
	 * Found it - copy out its contents.
	 */
	tl = &ltable[a_index];
	strcpy(a_result->name, tl->name);
	a_result->lock.waitStates = ((struct afs_lock *)(tl->addr))->wait_states;
	a_result->lock.exclLocked = ((struct afs_lock *)(tl->addr))->excl_locked;
	a_result->lock.readersReading = ((struct afs_lock *)(tl->addr))->readers_reading;
	a_result->lock.numWaiting = ((struct afs_lock *)(tl->addr))->num_waiting;
	a_result->lock.pid_last_reader = ((struct afs_lock *)(tl->addr))->pid_last_reader;
	a_result->lock.pid_writer = ((struct afs_lock *)(tl->addr))->pid_writer;
	a_result->lock.src_indicator = ((struct afs_lock *)(tl->addr))->src_indicator;
	code = 0;
    }

    XSTATS_END_TIME;

#ifdef RX_ENABLE_LOCKS
    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */

    return(code);

}  /*SRXAFSCB_GetLock*/


/*------------------------------------------------------------------------
 * static ClearCallBack
 *
 * Description:
 *	Clear out callback information for the specified file, or
 *	even a whole volume.  Used to worry about callback was from 
 *      within the particular cell or not.  Now we don't bother with
 *      that anymore; it's not worth the time.
 *
 * Arguments:
 *	a_conn : Ptr to Rx connection involved.
 *	a_fid  : Ptr to AFS fid being cleared.
 *
 * Returns:
 *	0 (always)
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static ClearCallBack(a_conn, a_fid)
    register struct rx_connection *a_conn;
    register struct AFSFid *a_fid;

{ /*ClearCallBack*/

    register struct vcache *tvc;
    register int i;
    struct VenusFid localFid;
    struct volume * tv;

    AFS_STATCNT(ClearCallBack);

    /*
     * XXXX Don't hold any server locks here because of callback protocol XXX
     */
    localFid.Cell = 0;
    localFid.Fid.Volume = a_fid->Volume;
    localFid.Fid.Vnode = a_fid->Vnode;
    localFid.Fid.Unique = a_fid->Unique;

    /*
      * Volume ID of zero means don't do anything.
      */
    if (a_fid->Volume != 0) {
	if (a_fid->Vnode == 0) {
	    /*
	     * Clear callback for the whole volume.  Zip through the
	     * hash chain, nullifying entries whose volume ID matches.
	     */
	    for (i = 0; i < VCSIZE; i++)
		for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
		    if (tvc->fid.Fid.Volume == a_fid->Volume) {
			tvc->callback = (struct server *)0;
			tvc->quick.stamp = 0; 
			if (!localFid.Cell)
			    localFid.Cell = tvc->fid.Cell;
			tvc->h1.dchint = NULL; /* invalidate hints */
			ObtainWriteLock(&afs_xcbhash, 449);
			afs_DequeueCallback(tvc);
			tvc->states &= ~(CStatd | CUnique | CBulkFetching);
			afs_allCBs++;
			if (tvc->fid.Fid.Vnode & 1)
			  afs_oddCBs++;	
			else
			  afs_evenCBs++; 
			ReleaseWriteLock(&afs_xcbhash);
			if (tvc->fid.Fid.Vnode & 1 || (vType(tvc) == VDIR))
			    osi_dnlc_purgedp(tvc);
			afs_Trace3(afs_iclSetp, CM_TRACE_CALLBACK,
				   ICL_TYPE_POINTER, tvc, 
				   ICL_TYPE_INT32, tvc->states,
				   ICL_TYPE_INT32, a_fid->Volume);
		    } else if ((tvc->states & CMValid) && (tvc->mvid->Fid.Volume == a_fid->Volume)) {
			tvc->states &= ~CMValid;
			if (!localFid.Cell)
			    localFid.Cell = tvc->mvid->Cell;
		    }
		}

	    /*
	     * XXXX Don't hold any locks here XXXX
	     */
	    tv = afs_FindVolume(&localFid, 0);
	    if (tv) {
	      afs_ResetVolumeInfo(tv);
	      afs_PutVolume(tv, 0);
	      /* invalidate mtpoint? */
	    }
	} /*Clear callbacks for whole volume*/
	else {
	    /*
	     * Clear callbacks just for the one file.
	     */
	    afs_allCBs++;
	    if (a_fid->Vnode & 1)
		afs_oddCBs++;	/*Could do this on volume basis, too*/
	    else
		afs_evenCBs++; /*A particular fid was specified*/
	    i = VCHash(&localFid);
	    for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
		if (tvc->fid.Fid.Vnode == a_fid->Vnode
		    && tvc->fid.Fid.Volume == a_fid->Volume
		    && tvc->fid.Fid.Unique == a_fid->Unique ) {
		    tvc->callback = (struct server *)0;
		    tvc->quick.stamp = 0; 
		    tvc->h1.dchint = NULL; /* invalidate hints */
		    ObtainWriteLock(&afs_xcbhash, 450);
		    afs_DequeueCallback(tvc);
		    tvc->states &= ~(CStatd | CUnique | CBulkFetching);
		    ReleaseWriteLock(&afs_xcbhash);
		    if (a_fid->Vnode & 1 || (vType(tvc) == VDIR))
			osi_dnlc_purgedp(tvc);
		    afs_Trace3(afs_iclSetp, CM_TRACE_CALLBACK,
			       ICL_TYPE_POINTER, tvc, 
			       ICL_TYPE_INT32, tvc->states, ICL_TYPE_LONG, 0);
#ifdef CBDEBUG
		    lastCallBack_vnode = afid->Vnode;
		    lastCallBack_dv = tvc->mstat.DataVersion.low;
		    osi_GetuTime(&lastCallBack_time);
#endif /* CBDEBUG */
		}
	    } /*Walk through hash table*/
	} /*Clear callbacks for one file*/
    } /*Fid has non-zero volume ID*/

    /*
     * Always return a predictable value.
     */
    return(0);

} /*ClearCallBack*/


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_CallBack
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement passing in callback information.
 *	table.
 *
 * Arguments:
 *	a_call      : Ptr to Rx call on which this request came in.
 *	a_fids      : Ptr to array of fids involved.
 *	a_callbacks : Ptr to matching callback info for the fids.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_CallBack(a_call, a_fids, a_callbacks)
    struct rx_call *a_call;
    register struct AFSCBFids *a_fids;
    struct AFSCBs *a_callbacks;
    
{ /*SRXAFSCB_CallBack*/

    register int i;			    /*Loop variable*/
    struct AFSFid *tfid;		    /*Ptr to current fid*/
    register struct rx_connection *tconn;   /*Call's connection*/
    int code=0;
    XSTATS_DECLS;

#ifdef RX_ENABLE_LOCKS
    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */

    XSTATS_START_CMTIME(AFS_STATS_CM_RPCIDX_CALLBACK);

    AFS_STATCNT(SRXAFSCB_CallBack);
    if (!(tconn = rx_ConnectionOf(a_call))) return;
    tfid = (struct AFSFid *) a_fids->AFSCBFids_val;
    
    /*
     * For now, we ignore callbacks, since the File Server only *breaks*
     * callbacks at present.
     */
    for (i = 0; i < a_fids->AFSCBFids_len; i++) 
	ClearCallBack(tconn, &tfid[i]);

    XSTATS_END_TIME;

#ifdef RX_ENABLE_LOCKS
    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
    
    return(0);

} /*SRXAFSCB_CallBack*/


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_Probe
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement ``probing'' the Cache Manager, just making sure it's
 *	still there.
 *
 * Arguments:
 *	a_call : Ptr to Rx call on which this request came in.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_Probe(a_call)
    struct rx_call *a_call;

{ /*SRXAFSCB_Probe*/
    int code = 0;
    XSTATS_DECLS;

#ifdef RX_ENABLE_LOCKS
    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
    AFS_STATCNT(SRXAFSCB_Probe);

    XSTATS_START_CMTIME(AFS_STATS_CM_RPCIDX_PROBE);
    XSTATS_END_TIME;

#ifdef RX_ENABLE_LOCKS
    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */

    return(0);

} /*SRXAFSCB_Probe*/


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_InitCallBackState
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement clearing all callbacks from this host.
 *
 * Arguments:
 *	a_call : Ptr to Rx call on which this request came in.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_InitCallBackState(a_call)
    struct rx_call *a_call;

{ /*SRXAFSCB_InitCallBackState*/

    register int i;
    register struct vcache *tvc;
    register struct rx_connection *tconn;
    register struct rx_peer *peer;
    struct server *ts;
    int code = 0;
    extern int osi_dnlc_purge();
    XSTATS_DECLS;

#ifdef RX_ENABLE_LOCKS
    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */

    XSTATS_START_CMTIME(AFS_STATS_CM_RPCIDX_INITCALLBACKSTATE);
    AFS_STATCNT(SRXAFSCB_InitCallBackState);

    /*
     * Find the address of the host making this call
     */
    if ((tconn = rx_ConnectionOf(a_call)) && (peer = rx_PeerOf(tconn))) {

	afs_allCBs++;
	afs_oddCBs++;	/*Including any missed via create race*/
	afs_evenCBs++;	/*Including any missed via create race*/

	ts = afs_FindServer(rx_HostOf(peer), rx_PortOf(peer), (afsUUID *)0, 0);
	if (ts) {
	    for (i = 0; i < VCSIZE; i++)
		for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
		    if (tvc->callback == ts) {
			ObtainWriteLock(&afs_xcbhash, 451);
			afs_DequeueCallback(tvc);
			tvc->callback = (struct server *)0;
			tvc->states &= ~(CStatd | CUnique | CBulkFetching);
			ReleaseWriteLock(&afs_xcbhash);
		    }
		}
	}

      
 
	/* find any volumes residing on this server and flush their state */
	{
	    register struct volume *tv;
	    register int j;
        
	    for (i=0;i<NVOLS;i++) 
		for (tv = afs_volumes[i]; tv; tv=tv->next) {
		    for (j=0; j<MAXHOSTS; j++)
			if (tv->serverHost[j] == ts)
			    afs_ResetVolumeInfo(tv);
		}
	}
	osi_dnlc_purge(); /* may be a little bit extreme */
    }

    XSTATS_END_TIME;

#ifdef RX_ENABLE_LOCKS
    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */

    return(0);

} /*SRXAFSCB_InitCallBackState*/


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_XStatsVersion
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement pulling out the xstat version number for the Cache
 *	Manager.
 *
 * Arguments:
 *	a_versionP : Ptr to the version number variable to set.
 *
 * Returns:
 *	0 (always)
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_XStatsVersion(a_call, a_versionP)
    struct rx_call *a_call;
    afs_int32 *a_versionP;

{ /*SRXAFSCB_XStatsVersion*/
   int code=0;

    XSTATS_DECLS;

#ifdef RX_ENABLE_LOCKS
    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
    XSTATS_START_CMTIME(AFS_STATS_CM_RPCIDX_XSTATSVERSION);

    *a_versionP = AFSCB_XSTAT_VERSION;

    XSTATS_END_TIME;

#ifdef RX_ENABLE_LOCKS
    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */

    return(0);
}  /*SRXAFSCB_XStatsVersion*/


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetXStats
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement getting the given data collection from the extended
 *	Cache Manager statistics.
 *
 * Arguments:
 *	a_call		    : Ptr to Rx call on which this request came in.
 *	a_clientVersionNum  : Client version number.
 *	a_opCode	    : Desired operation.
 *	a_serverVersionNumP : Ptr to version number to set.
 *	a_timeP		    : Ptr to time value (seconds) to set.
 *	a_dataArray	    : Ptr to variable array structure to return
 *			      stuff in.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_GetXStats(a_call, a_clientVersionNum, a_collectionNumber, a_srvVersionNumP, a_timeP, a_dataP)
    struct rx_call *a_call;
    afs_int32 a_clientVersionNum;
    afs_int32 a_collectionNumber;
    afs_int32 *a_srvVersionNumP;
    afs_int32 *a_timeP;
    AFSCB_CollData *a_dataP;

{ /*SRXAFSCB_GetXStats*/

    register int code;		/*Return value*/
    afs_int32 *dataBuffP;		/*Ptr to data to be returned*/
    afs_int32 dataBytes;		/*Bytes in data buffer*/
    XSTATS_DECLS;

#ifdef RX_ENABLE_LOCKS
    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */

    XSTATS_START_CMTIME(AFS_STATS_CM_RPCIDX_GETXSTATS);

    /*
     * Record the time of day and the server version number.
     */
    *a_srvVersionNumP = AFSCB_XSTAT_VERSION;
    *a_timeP = osi_Time();

    /*
     * Stuff the appropriate data in there (assume victory)
     */
    code = 0;

#ifdef AFS_NOSTATS
    /*
     * We're not keeping stats, so just return successfully with
     * no data.
     */
    a_dataP->AFSCB_CollData_len = 0;
    a_dataP->AFSCB_CollData_val = (afs_int32 *)0;
#else
    switch(a_collectionNumber) {
      case AFSCB_XSTATSCOLL_CALL_INFO:
	/*
	 * Pass back all the call-count-related data.
	 *
	 * >>> We are forced to allocate a separate area in which to
	 * >>> put this stuff in by the RPC stub generator, since it
	 * >>> will be freed at the tail end of the server stub code.
	 */
	dataBytes = sizeof(struct afs_CMStats);
	dataBuffP = (afs_int32 *)afs_osi_Alloc(dataBytes);
	bcopy((char *)&afs_cmstats, (char *)dataBuffP, dataBytes);
	a_dataP->AFSCB_CollData_len = dataBytes>>2;
	a_dataP->AFSCB_CollData_val = dataBuffP;
	break;

      case AFSCB_XSTATSCOLL_PERF_INFO:
	/*
	 * Update and then pass back all the performance-related data.
	 * Note: the only performance fields that need to be computed
	 * at this time are the number of accesses for this collection
	 * and the current server record info.
	 *
	 * >>> We are forced to allocate a separate area in which to
	 * >>> put this stuff in by the RPC stub generator, since it
	 * >>> will be freed at the tail end of the server stub code.
	 */
	afs_stats_cmperf.numPerfCalls++;
	afs_CountServers();
	dataBytes = sizeof(afs_stats_cmperf);
	dataBuffP = (afs_int32 *)afs_osi_Alloc(dataBytes);
	bcopy((char *)&afs_stats_cmperf, (char *)dataBuffP, dataBytes);
	a_dataP->AFSCB_CollData_len = dataBytes>>2;
	a_dataP->AFSCB_CollData_val = dataBuffP;
	break;

      case AFSCB_XSTATSCOLL_FULL_PERF_INFO:
	/*
	 * Pass back the full range of performance and statistical
	 * data available.  We have to bring the normal performance
	 * data collection up to date, then copy that data into
	 * the full collection.
	 *
	 * >>> We are forced to allocate a separate area in which to
	 * >>> put this stuff in by the RPC stub generator, since it
	 * >>> will be freed at the tail end of the server stub code.
	 */
	afs_stats_cmperf.numPerfCalls++;
	afs_CountServers();
	bcopy((char *)(&afs_stats_cmperf),
	      (char *)(&(afs_stats_cmfullperf.perf)),
	      sizeof(struct afs_stats_CMPerf));
	afs_stats_cmfullperf.numFullPerfCalls++;

	dataBytes = sizeof(afs_stats_cmfullperf);
	dataBuffP = (afs_int32 *)afs_osi_Alloc(dataBytes);
	bcopy((char *)(&afs_stats_cmfullperf), (char *)dataBuffP, dataBytes);
	a_dataP->AFSCB_CollData_len = dataBytes>>2;
	a_dataP->AFSCB_CollData_val = dataBuffP;
	break;

      default:
	/*
	 * Illegal collection number.
	 */
	a_dataP->AFSCB_CollData_len = 0;
	a_dataP->AFSCB_CollData_val = (afs_int32 *)0;
	code = 1;
    } /*Switch on collection number*/
#endif /* AFS_NOSTATS */

    XSTATS_END_TIME;

#ifdef RX_ENABLE_LOCKS
    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */

    return(code);

} /*SRXAFSCB_GetXStats*/


/*------------------------------------------------------------------------
 * EXPORTED afs_RXCallBackServer
 *
 * Description:
 *	Body of the thread supporting callback services.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int afs_RXCallBackServer()

{ /*afs_RXCallBackServer*/
    AFS_STATCNT(afs_RXCallBackServer);

    while (1) {
	if (afs_server)
	    break;
	afs_osi_Sleep(&afs_server);
    }

    /*
     * Donate this process to Rx.
     */
    rx_ServerProc();
    return(0);

} /*afs_RXCallBackServer*/


/*------------------------------------------------------------------------
 * EXPORTED shutdown_CB
 *
 * Description:
 *	Zero out important Cache Manager data structures.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int shutdown_CB() 

{ /*shutdown_CB*/

  extern int afs_cold_shutdown;

  AFS_STATCNT(shutdown_CB);

  if (afs_cold_shutdown) {
    afs_oddCBs = afs_evenCBs = afs_allCBs = afs_allZaps = afs_oddZaps = afs_evenZaps =
	afs_connectBacks = 0;
  }

  return(0);

} /*shutdown_CB*/

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_InitCallBackState2
 *
 * Description:
 *      This routine was used in the AFS 3.5 beta release, but not anymore.
 *      It has since been replaced by SRXAFSCB_InitCallBackState3.
 *
 * Arguments:
 *      a_call : Ptr to Rx call on which this request came in.
 *
 * Returns:
 *      RXGEN_OPCODE (always). 
 *
 * Environment:
 *      Nothing interesting.
 *
 * Side Effects:
 *      None
 *------------------------------------------------------------------------*/

int SRXAFSCB_InitCallBackState2(a_call, addr)
struct rx_call *a_call;
struct interfaceAddr * addr;
{
	return RXGEN_OPCODE;
}

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_WhoAreYou
 *
 * Description:
 *      Routine called by the server-side callback RPC interface to
 *      obtain a unique identifier for the client. The server uses
 *      this identifier to figure out whether or not two RX connections
 *      are from the same client, and to find out which addresses go
 *      with which clients.
 *
 * Arguments:
 *      a_call : Ptr to Rx call on which this request came in.
 *      addr: Ptr to return the list of interfaces for this client.
 *
 * Returns:
 *      0 (Always)
 *
 * Environment:
 *      Nothing interesting.
 *
 * Side Effects:
 *      As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_WhoAreYou(a_call, addr)
struct rx_call *a_call;
struct interfaceAddr *addr;
{
    int i;
    int code = 0;
    XSTATS_DECLS;

#ifdef RX_ENABLE_LOCKS
    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */

    AFS_STATCNT(SRXAFSCB_WhoAreYou);

    ObtainReadLock(&afs_xinterface);

    /* return all network interface addresses */
    addr->numberOfInterfaces = afs_cb_interface.numberOfInterfaces;
    addr->uuid = afs_cb_interface.uuid;
    for ( i=0; i < afs_cb_interface.numberOfInterfaces; i++) {
	addr->addr_in[i] = ntohl(afs_cb_interface.addr_in[i]);
	addr->subnetmask[i] = ntohl(afs_cb_interface.subnetmask[i]);
	addr->mtu[i] = ntohl(afs_cb_interface.mtu[i]);
    }

    ReleaseReadLock(&afs_xinterface);

#ifdef RX_ENABLE_LOCKS
    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */

    return code;
}


/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_InitCallBackState3
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement clearing all callbacks from this host.
 *
 * Arguments:
 *	a_call : Ptr to Rx call on which this request came in.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_InitCallBackState3(a_call, a_uuid)
struct rx_call *a_call;
afsUUID *a_uuid;
{
    int code;

    /*
     * TBD: Lookup the server by the UUID instead of its IP address.
     */
    code = SRXAFSCB_InitCallBackState(a_call);

    return code;
}
 

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_ProbeUuid
 *
 * Description:
 *	Routine called by the server-side callback RPC interface to
 *	implement ``probing'' the Cache Manager, just making sure it's
 *	still there is still the same client it used to be.
 *
 * Arguments:
 *	a_call : Ptr to Rx call on which this request came in.
 *	a_uuid : Ptr to UUID that must match the client's UUID.
 *
 * Returns:
 *	0 if a_uuid matches the UUID for this client
 *      Non-zero otherwize
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_ProbeUuid(a_call, a_uuid)
struct rx_call *a_call;
afsUUID *a_uuid;
{
    int code = 0;
    XSTATS_DECLS;

#ifdef RX_ENABLE_LOCKS
    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
    AFS_STATCNT(SRXAFSCB_Probe);

    XSTATS_START_CMTIME(AFS_STATS_CM_RPCIDX_PROBE);
    if (!afs_uuid_equal(a_uuid, &afs_cb_interface.uuid))
	code = 1; /* failure */
    XSTATS_END_TIME;

#ifdef RX_ENABLE_LOCKS
    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */

    return code;
}
 

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetServerPrefs
 *
 * Description:
 *	Routine to list server preferences used by this client.
 *
 * Arguments:
 *	a_call  : Ptr to Rx call on which this request came in.
 *	a_index : Input server index
 *	a_srvr_addr  : Output server address in host byte order
 *                     (0xffffffff on last server)
 *	a_srvr_rank  : Output server rank
 *
 * Returns:
 *	0 on success
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_GetServerPrefs(
    struct rx_call *a_call,
    afs_int32 a_index,
    afs_int32 *a_srvr_addr,
    afs_int32 *a_srvr_rank)
{
    int i, j;
    struct srvAddr *sa;

#ifdef RX_ENABLE_LOCKS
    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
    AFS_STATCNT(SRXAFSCB_GetServerPrefs);

    ObtainReadLock(&afs_xserver);

    /* Search the hash table for the server with this index */
    *a_srvr_addr = 0xffffffff;
    *a_srvr_rank = 0xffffffff;
    for (i=0, j=0; j < NSERVERS && i <= a_index; j++) {
	for (sa = afs_srvAddrs[j]; sa && i <= a_index; sa = sa->next_bkt, i++) {
	    if (i == a_index) {
		*a_srvr_addr = ntohl(sa->sa_ip);
		*a_srvr_rank = sa->sa_iprank;
	    }
	}
    }

    ReleaseReadLock(&afs_xserver);

#ifdef RX_ENABLE_LOCKS
    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */

    return 0;
}
 

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetCellServDB
 *
 * Description:
 *	Routine to list cells configured for this client
 *
 * Arguments:
 *	a_call  : Ptr to Rx call on which this request came in.
 *	a_index : Input cell index
 *	a_name  : Output cell name ("" on last cell)
 *	a_hosts : Output cell database servers in host byte order.
 *
 * Returns:
 *	0 on success
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_GetCellServDB(
    struct rx_call *a_call,
    afs_int32 a_index,
    char **a_name,
    afs_int32 *a_hosts)
{
    afs_int32 i, j;
    struct cell *tcell;
    struct afs_q *cq, *tq;
    char *t_name;

#ifdef RX_ENABLE_LOCKS
    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
    AFS_STATCNT(SRXAFSCB_GetCellServDB);

    t_name = (char *)rxi_Alloc(AFSNAMEMAX);
    if (t_name == NULL) {
#ifdef RX_ENABLE_LOCKS
	AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	return ENOMEM;
    }

    t_name[0] = '\0';
    bzero(a_hosts, AFSMAXCELLHOSTS * sizeof(afs_int32));

    /* search the list for the cell with this index */
    ObtainReadLock(&afs_xcell);
    for (i=0, cq = CellLRU.next; cq != &CellLRU && i<= a_index; cq = tq, i++) {
        tq = QNext(cq);
	if (i == a_index) {
	    tcell = QTOC(cq);
	    strcpy(t_name, tcell->cellName);
	    for (j = 0 ; j < AFSMAXCELLHOSTS && tcell->cellHosts[j] ; j++) {
		a_hosts[j] = ntohl(tcell->cellHosts[j]->addr->sa_ip);
	    }
	}
    }
    ReleaseReadLock(&afs_xcell);

#ifdef RX_ENABLE_LOCKS
    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */

    *a_name = t_name;
    return 0;
}
 

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetLocalCell
 *
 * Description:
 *	Routine to return name of client's local cell
 *
 * Arguments:
 *	a_call  : Ptr to Rx call on which this request came in.
 *	a_name  : Output cell name
 *
 * Returns:
 *	0 on success
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_GetLocalCell(
    struct rx_call *a_call,
    char **a_name)
{
    struct cell *tcell;
    struct afs_q *cq, *tq;
    char *t_name;

#ifdef RX_ENABLE_LOCKS
    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
    AFS_STATCNT(SRXAFSCB_GetLocalCell);

    t_name = (char *)rxi_Alloc(AFSNAMEMAX);
    if (t_name == NULL) {
#ifdef RX_ENABLE_LOCKS
	AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	return ENOMEM;
    }

    t_name[0] = '\0';

    /* Search the list for the primary cell. Cell number 1 is only
     * the primary cell is when no other cell is explicitly marked as
     * the primary cell.  */
    ObtainReadLock(&afs_xcell);
    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
        tq = QNext(cq);
	tcell = QTOC(cq);
	if (tcell->states & CPrimary) {
	    strcpy(t_name, tcell->cellName);
	    break;
	}
        if (tcell->cell == 1) {
	    strcpy(t_name, tcell->cellName);
	}
    }
    ReleaseReadLock(&afs_xcell);

#ifdef RX_ENABLE_LOCKS
    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */

    *a_name = t_name;
    return 0;
}


/*
 * afs_MarshallCacheConfig - marshall client cache configuration
 *
 * PARAMETERS
 *
 * IN callerVersion - the rpc stat version of the caller.
 *
 * IN config - client cache configuration.
 *
 * OUT ptr - buffer where configuration is marshalled.
 *
 * RETURN CODES
 *
 * Returns void.
 */
static void afs_MarshallCacheConfig(
    afs_uint32 callerVersion,
    cm_initparams_v1 *config,
    afs_uint32 *ptr)
{
    AFS_STATCNT(afs_MarshallCacheConfig);
    /*
     * We currently only support version 1.
     */
    *(ptr++) = config->nChunkFiles;
    *(ptr++) = config->nStatCaches;
    *(ptr++) = config->nDataCaches;
    *(ptr++) = config->nVolumeCaches;
    *(ptr++) = config->firstChunkSize;
    *(ptr++) = config->otherChunkSize;
    *(ptr++) = config->cacheSize;
    *(ptr++) = config->setTime;
    *(ptr++) = config->memCache;

}
 

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetCacheConfig
 *
 * Description:
 *	Routine to return parameters used to initialize client cache.
 *      Client may request any format version. Server may not return
 *      format version greater than version requested by client.
 *
 * Arguments:
 *	a_call:        Ptr to Rx call on which this request came in.
 *	callerVersion: Data format version desired by the client.
 *	serverVersion: Data format version of output data.
 *      configCount:   Number bytes allocated for output data.
 *      config:        Client cache configuration.
 *
 * Returns:
 *	0 on success
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_GetCacheConfig(
    struct rx_call *a_call,
    afs_uint32 callerVersion,
    afs_uint32 *serverVersion,
    afs_uint32 *configCount,
    cacheConfig *config)
{
    afs_uint32 *t_config;
    size_t allocsize;
    cm_initparams_v1 cm_config;

#ifdef RX_ENABLE_LOCKS
    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
    AFS_STATCNT(SRXAFSCB_GetCacheConfig);

    /*
     * Currently only support version 1
     */
    allocsize = sizeof(cm_initparams_v1);
    t_config = (afs_uint32 *)rxi_Alloc(allocsize);
    if (t_config == NULL) {
#ifdef RX_ENABLE_LOCKS
	AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	return ENOMEM;
    }

    cm_config.nChunkFiles = cm_initParams.cmi_nChunkFiles;
    cm_config.nStatCaches = cm_initParams.cmi_nStatCaches;
    cm_config.nDataCaches = cm_initParams.cmi_nDataCaches;
    cm_config.nVolumeCaches = cm_initParams.cmi_nVolumeCaches;
    cm_config.firstChunkSize = cm_initParams.cmi_firstChunkSize;
    cm_config.otherChunkSize = cm_initParams.cmi_otherChunkSize;
    cm_config.cacheSize = cm_initParams.cmi_cacheSize;
    cm_config.setTime = cm_initParams.cmi_setTime;
    cm_config.memCache = cm_initParams.cmi_memCache;

    afs_MarshallCacheConfig(callerVersion, &cm_config, t_config);

    *serverVersion = AFS_CLIENT_RETRIEVAL_FIRST_EDITION;
    *configCount = allocsize;
    config->cacheConfig_val = t_config;
    config->cacheConfig_len = allocsize/sizeof(afs_uint32);

#ifdef RX_ENABLE_LOCKS
    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */

    return 0;
}
