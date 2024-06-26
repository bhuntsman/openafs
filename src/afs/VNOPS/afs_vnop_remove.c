/* Copyright (C) 1995, 1989, 1998 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/*
 * afs_vnop_remove.c
 *
 * Implements:
 * afs_Wire (DUX)
 * FetchWholeEnchilada
 * afs_IsWired (DUX)
 * afsremove
 * afs_remove
 *
 * Local:
 * newname
 *
 */
#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h" /* statistics */
#include "../afs/afs_cbqueue.h"
#include "../afs/nfsclient.h"
#include "../afs/afs_osidnlc.h"


extern afs_rwlock_t afs_xvcache;
extern afs_rwlock_t afs_xcbhash;


#ifdef	AFS_OSF_ENV
/*
 *  Wire down file in cache: prefetch all data, and turn on CWired flag
 *  so that callbacks/callback expirations are (temporarily) ignored
 *  and cache file(s) are kept in cache. File will be unwired when
 *  afs_inactive is called (ie no one has VN_HOLD on vnode), or when
 *  afs_IsWired notices that the file is no longer Active.
 */
afs_Wire(avc, areq)
#else	/* AFS_OSF_ENV */
static void FetchWholeEnchilada(avc, areq)
#endif
struct vrequest *areq;
register struct vcache *avc; {
    register afs_int32 nextChunk, pos;
    register struct dcache *tdc;
    afs_int32 offset, len;

    AFS_STATCNT(FetchWholeEnchilada);
    if ((avc->states & CStatd) == 0) return;	/* don't know size */
    for(nextChunk=0;nextChunk<1024;nextChunk++)	{   /* sanity check on N chunks */
	pos = AFS_CHUNKTOBASE(nextChunk);
#if	defined(AFS_OSF_ENV)
	if (pos	>= avc->m.Length) break;	/* all done */
#else	/* AFS_OSF_ENV */
	if (pos	>= avc->m.Length) return;	/* all done */
#endif
	tdc = afs_GetDCache(avc, pos, areq, &offset, &len, 0);
	if (!tdc) 
#if	defined(AFS_OSF_ENV)
	    break;
#else	/* AFS_OSF_ENV */
	    return;
#endif
	afs_PutDCache(tdc);
    }
#if defined(AFS_OSF_ENV)
    avc->states |= CWired;
#endif	/* AFS_OSF_ENV */
}

#if	defined(AFS_OSF_ENV)
/*
 *  Tests whether file is wired down, after unwiring the file if it
 *  is found to be inactive (ie not open and not being paged from).
 */
afs_IsWired(avc)
    register struct vcache *avc; {
    if (avc->states & CWired) {
	if (osi_Active(avc)) {
	    return 1;
	}
	avc->states &= ~CWired;
    }
    return 0;
}
#endif	/* AFS_OSF_ENV */

afsremove(adp, tdc, tvc, aname, acred, treqp)
    register struct vcache *adp;
    register struct dcache *tdc;
    register struct vcache *tvc;
    char *aname;
    struct vrequest *treqp;
    struct AFS_UCRED *acred; {
    register afs_int32 code;
    register struct conn *tc;
    afs_int32 offset, len;
    struct AFSFetchStatus OutDirStatus;
    struct AFSVolSync tsync;
    XSTATS_DECLS

    do {
	tc = afs_Conn(&adp->fid, treqp, SHARED_LOCK);
	if (tc) {
          XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_REMOVEFILE);
#ifdef RX_ENABLE_LOCKS
	    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	    code = RXAFS_RemoveFile(tc->id, (struct AFSFid *) &adp->fid.Fid,
				    aname, &OutDirStatus, &tsync);
#ifdef RX_ENABLE_LOCKS
	    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
          XSTATS_END_TIME;
      }
	else code = -1;
    } while
      (afs_Analyze(tc, code, &adp->fid, treqp,
		   AFS_STATS_FS_RPCIDX_REMOVEFILE, SHARED_LOCK, (struct cell *)0));

    osi_dnlc_remove (adp, aname, tvc);
    if (tvc) afs_symhint_inval(tvc);   /* XXX: don't really need to be so extreme */

    if (code) {
	if (tdc) afs_PutDCache(tdc);
	if (tvc) afs_PutVCache(tvc, WRITE_LOCK);

	if (code < 0) {
	  ObtainWriteLock(&afs_xcbhash, 497);
	  afs_DequeueCallback(adp);
	  adp->states &= ~CStatd;
	  ReleaseWriteLock(&afs_xcbhash);
	  osi_dnlc_purgedp(adp);
	}
	ReleaseWriteLock(&adp->lock);
	code = afs_CheckCode(code, treqp, 21);
	return code;
    }
    if (afs_LocalHero(adp, tdc, &OutDirStatus, 1)) {
	/* we can do it locally */
	code = afs_dir_Delete(&tdc->f.inode, aname);
	if (code) {
	    ZapDCE(tdc);	/* surprise error -- invalid value */
	    DZap(&tdc->f.inode);
	}
    }
    if (tdc) {
	afs_PutDCache(tdc);	/* drop ref count */
    }
    ReleaseWriteLock(&adp->lock);

    /* now, get vnode for unlinked dude, and see if we should force it
     * from cache.  adp is now the deleted files vnode.  Note that we
     * call FindVCache instead of GetVCache since if the file's really
     * gone, we won't be able to fetch the status info anyway.  */
    if (tvc) {
#if	defined(AFS_SUN_ENV) || defined(AFS_ALPHA_ENV) || defined(AFS_SUN5_ENV)
        afs_BozonLock(&tvc->pvnLock, tvc);	
	/* Since afs_TryToSmush will do a pvn_vptrunc */
#endif
	ObtainWriteLock(&tvc->lock,141);
	/* note that callback will be broken on the deleted file if there are
	 * still >0 links left to it, so we'll get the stat right */
	tvc->m.LinkCount--;
	tvc->states &= ~CUnique;		/* For the dfs xlator */
	if (tvc->m.LinkCount == 0 && !osi_Active(tvc)) {
	    if (!AFS_NFSXLATORREQ(acred)) afs_TryToSmush(tvc, acred, 0);
	} 
	ReleaseWriteLock(&tvc->lock);
#if	defined(AFS_SUN_ENV) || defined(AFS_ALPHA_ENV) || defined(AFS_SUN5_ENV)
	afs_BozonUnlock(&tvc->pvnLock, tvc);
#endif
	afs_PutVCache(tvc, WRITE_LOCK);
    }
    return (0);
}

static char *newname() {
    char *name, *sp, *p = ".__afs";
    afs_int32 rd = afs_random() & 0xffff;

    sp = name = osi_AllocSmallSpace(AFS_SMALLOCSIZ);
    while (*p != '\0') *sp++ = *p++;
    while (rd) {
	*sp++= "0123456789ABCDEF"[rd & 0x0f];
	rd >>= 4;
    }
    *sp = '\0';
    return (name);
}

/* these variables appear to exist for debugging purposes */
struct vcache * Tadp1, * Ttvc;
int Tadpr, Ttvcr;
char *Tnam;
char *Tnam1;

/* Note that we don't set CDirty here, this is OK because the unlink
 * RPC is called synchronously */
#ifdef	AFS_OSF_ENV
afs_remove(ndp)
    struct nameidata *ndp; {
    register struct vcache *adp = (struct vcache *)ndp->ni_dvp;
    char *aname = ndp->ni_dent.d_name;
    struct ucred *acred = ndp->ni_cred;
#else	/* AFS_OSF_ENV */
afs_remove(OSI_VC_ARG(adp), aname, acred)
    OSI_VC_DECL(adp);
    char *aname;
    struct AFS_UCRED *acred; {
#endif
    struct vrequest treq;
    register struct dcache *tdc;
    struct VenusFid unlinkFid;
    register afs_int32 code;
    register struct vcache *tvc;
    afs_int32 offset, len;
    struct AFSFetchStatus OutDirStatus;
    struct AFSVolSync tsync;
    XSTATS_DECLS
    OSI_VC_CONVERT(adp)

    AFS_STATCNT(afs_remove);
    afs_Trace2(afs_iclSetp, CM_TRACE_REMOVE, ICL_TYPE_POINTER, adp,
	       ICL_TYPE_STRING, aname);

    if (code = afs_InitReq(&treq, acred))
      return code;
tagain:
    code = afs_VerifyVCache(adp, &treq);
#ifdef	AFS_OSF_ENV
    tvc = (struct vcache *)ndp->ni_vp;  /* should never be null */
    if (code) {
	afs_PutVCache(adp, 0);
	afs_PutVCache(tvc, 0);
	return afs_CheckCode(code, &treq, 22);
    }
#else	/* AFS_OSF_ENV */
    tvc = (struct vcache *) 0;
    if (code) {
      code = afs_CheckCode(code, &treq, 23);
      return code;
    }
#endif

    /** If the volume is read-only, return error without making an RPC to the
      * fileserver
      */
    if ( adp->states & CRO ) {
        code = EROFS;
	return code;
    }

    tdc	= afs_GetDCache(adp, 0,	&treq, &offset,	&len, 1);  /* test for error below */
    ObtainWriteLock(&adp->lock,142);

    /*
     * Make sure that the data in the cache is current. We may have
     * received a callback while we were waiting for the write lock.
     */
    if (!(adp->states & CStatd)
	|| (tdc && !hsame(adp->m.DataVersion, tdc->f.versionNo))) {
	ReleaseWriteLock(&adp->lock);
	if (tdc)
	    afs_PutDCache(tdc);
	goto tagain;
    }

    unlinkFid.Fid.Vnode = 0;
    if (!tvc) {
      tvc = osi_dnlc_lookup (adp, aname, WRITE_LOCK);
    }
    /* This should not be necessary since afs_lookup() has already
     * done the work */
    if (!tvc)
      if (tdc) {
	code = afs_dir_Lookup(&tdc->f.inode, aname, &unlinkFid.Fid);
	if (code == 0) {	
	    afs_int32 cached=0;

	    unlinkFid.Cell = adp->fid.Cell;
	    unlinkFid.Fid.Volume = adp->fid.Fid.Volume;
	    if (unlinkFid.Fid.Unique == 0) {
		tvc = afs_LookupVCache(&unlinkFid, &treq, &cached, 
				       WRITE_LOCK, adp, aname);
	    } else {
		ObtainReadLock(&afs_xvcache);
		tvc = afs_FindVCache(&unlinkFid, 1, WRITE_LOCK, 
				     0 , DO_STATS );
		ReleaseReadLock(&afs_xvcache);
	    }
	}
    }

    if (tvc && osi_Active(tvc)) {
	/* about to delete whole file, prefetch it first */
	ReleaseWriteLock(&adp->lock);
	ObtainWriteLock(&tvc->lock,143);
#if	defined(AFS_OSF_ENV)
	afs_Wire(tvc, &treq);
#else	/* AFS_OSF_ENV */
	FetchWholeEnchilada(tvc, &treq);
#endif
	ReleaseWriteLock(&tvc->lock);
	ObtainWriteLock(&adp->lock,144);
    }

    osi_dnlc_remove ( adp, aname, tvc);
    if (tvc) afs_symhint_inval(tvc);

    Tadp1 = adp; Tadpr = adp->vrefCount; Ttvc = tvc; Tnam = aname; Tnam1 = 0;
    if (tvc) Ttvcr = tvc->vrefCount;
#ifdef	AFS_AIX_ENV
    if (tvc && (tvc->vrefCount > 2) && tvc->opens > 0 && !(tvc->states & CUnlinked)) {
#else
    if (tvc && (tvc->vrefCount > 1) && tvc->opens > 0 && !(tvc->states & CUnlinked)) {
#endif
	char *unlname = newname();

	ReleaseWriteLock(&adp->lock);
	code = afsrename(adp, aname, adp, unlname, acred);
	Tnam1 = unlname;
	if (!code) {
	    tvc->mvid = (struct VenusFid *)unlname;
	    crhold(acred);
	    if (tvc->uncred) {
		crfree(tvc->uncred);
	    }
	    tvc->uncred = acred;
	    tvc->states |= CUnlinked;
	} else {
	    osi_FreeSmallSpace(unlname);	    
	}
        if ( tdc )
		afs_PutDCache(tdc);
	afs_PutVCache(tvc, WRITE_LOCK);	
    } else {
	code = afsremove(adp, tdc, tvc, aname, acred, &treq);
    }
#ifdef	AFS_OSF_ENV
    afs_PutVCache(adp, WRITE_LOCK);
#endif	/* AFS_OSF_ENV */
    return code;
}


/* afs_remunlink -- This tries to delete the file at the server after it has
 *     been renamed when unlinked locally but now has been finally released.
 *
 * CAUTION -- may be called with avc unheld. */

afs_remunlink(avc, doit)
    register struct vcache *avc;
    register int doit;
{
    struct AFS_UCRED *cred;
    char *unlname;
    struct vcache *adp;
    struct vrequest treq;
    struct VenusFid dirFid;
    register struct dcache *tdc;
    afs_int32 offset, len, code=0;

    if (NBObtainWriteLock(&avc->lock, 423))
	return 0;

    if (avc->mvid && (doit || (avc->states & CUnlinkedDel))) {
        if (code = afs_InitReq(&treq, avc->uncred)) {
	    ReleaseWriteLock(&avc->lock);
        }
	else {
	    /* Must bump the refCount because GetVCache may block.
	     * Also clear mvid so no other thread comes here if we block.
	     */
	    unlname = (char *)avc->mvid;
	    avc->mvid = NULL;
	    cred = avc->uncred;
	    avc->uncred = NULL;

	    VN_HOLD(&avc->v);

	    /* We'll only try this once. If it fails, just release the vnode.
	     * Clear after doing hold so that NewVCache doesn't find us yet.
	     */
	    avc->states  &= ~(CUnlinked | CUnlinkedDel);

	    ReleaseWriteLock(&avc->lock);

	    dirFid.Cell = avc->fid.Cell;
	    dirFid.Fid.Volume = avc->fid.Fid.Volume;
	    dirFid.Fid.Vnode = avc->parentVnode;
	    dirFid.Fid.Unique = avc->parentUnique;
	    adp = afs_GetVCache(&dirFid, &treq, (afs_int32 *)0, 
				(struct vcache *)0, WRITE_LOCK);
	    
	    if (adp) {
		tdc = afs_FindDCache(adp, 0);
		ObtainWriteLock(&adp->lock, 159);

		/* afsremove releases the adp lock, and does vn_rele(avc) */
		code = afsremove(adp, tdc, avc, unlname, cred, &treq);
		afs_PutVCache(adp, WRITE_LOCK);
	    } else {
		/* we failed - and won't be back to try again. */
		afs_PutVCache(avc, WRITE_LOCK);
	    }
	    osi_FreeSmallSpace(unlname);
	    crfree(cred);
        }
    }
    else {
	ReleaseWriteLock(&avc->lock);
    }

    return code;
}
