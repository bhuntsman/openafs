/* Copyright (C) 1995, 1989, 1998 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/*
 * afs_vnop_access.c - access vop ccess mode bit support for vnode operations.
 *
 * Implements:
 * afs_GetAccessBits
 * afs_AccessOK
 * afs_access
 *
 * Local:
 * fileModeMap (table)
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h" /* statistics */
#include "../afs/afs_cbqueue.h"
#include "../afs/nfsclient.h"
#include "../afs/afs_osidnlc.h"

#ifndef ANONYMOUSID
#define ANONYMOUSID     32766 /* make sure this is same as in ptserver.h */
#endif




/* access bits to turn off for various owner Unix mode values */
static char fileModeMap[8] = {
    PRSFS_READ | PRSFS_WRITE,
    PRSFS_READ | PRSFS_WRITE,
    PRSFS_READ,
    PRSFS_READ,
    PRSFS_WRITE,
    PRSFS_WRITE,
    0,
    0
};

/* avc must be held.  Returns bit map of mode bits.  Ignores file mode bits */
afs_int32 afs_GetAccessBits (avc, arights, areq)
    register struct vcache *avc;
    register afs_int32 arights;
    register struct vrequest *areq; 
{
    AFS_STATCNT(afs_GetAccessBits);
    /* see if anyuser has the required access bits */
    if ((arights & avc->anyAccess) == arights) {
	return arights;
    }

    /* look in per-pag cache */
    if (avc->Access) {  /* not beautiful, but Sun's cc will tolerate it */
      struct axscache *ac;

      ac = afs_FindAxs(avc->Access, areq->uid);
      if (ac) {
	return (arights & ac->axess);
      }
    }

    if (!(avc->states & CForeign)) { 
      /* If there aren't any bits cached for this user (but the vnode
       * _is_ cached, obviously), make sure this user has valid tokens
       * before bothering with the RPC.  */
      struct unixuser *tu;
      extern struct unixuser *afs_FindUser();
      tu = afs_FindUser( areq->uid, avc->fid.Cell, READ_LOCK );
      if (!tu) {
	return (arights & avc->anyAccess);
      }
      if ((tu->vid == UNDEFVID) || !(tu->states & UHasTokens) ||
	  (tu->states & UTokensBad)) {
	afs_PutUser(tu, READ_LOCK);
	return (arights & avc->anyAccess);
      }
      else {
	afs_PutUser(tu, READ_LOCK);
      }
    }

    { /* Ok, user has valid tokens, go ask the server. */
      struct AFSFetchStatus OutStatus;
      afs_int32 code;
    
      code = afs_FetchStatus(avc, &avc->fid, areq, &OutStatus);
      return (code ? 0 : OutStatus.CallerAccess & arights);
    }
}


/* the new access ok function.  AVC must be held but not locked. if avc is a
 * file, its parent need not be held, and should not be locked. */

afs_AccessOK(avc, arights, areq, check_mode_bits)
struct vcache *avc;
afs_int32 arights, check_mode_bits;
struct vrequest *areq; {
    register struct vcache *tvc;
    struct VenusFid dirFid;
    register afs_int32 mask;
    afs_int32 dirBits;
    register afs_int32 fileBits;

    AFS_STATCNT(afs_AccessOK);

    if ((vType(avc) == VDIR) || (avc->states & CForeign)) {
      /* rights are just those from acl */
      return (arights == afs_GetAccessBits(avc, arights, areq));
    }
    else {
	/* some rights come from dir and some from file.  Specifically, you 
	 * have "a" rights to a file if you are its owner, which comes
	 * back as "a" rights to the file. You have other rights just
	 * from dir, but all are restricted by the file mode bit. Now,
	 * if you have I and A rights to a file, we throw in R and W
	 * rights for free. These rights will then be restricted by
	 * the access mask. */
	dirBits = 0;
	if (avc->parentVnode) {
	    dirFid.Cell = avc->fid.Cell;
	    dirFid.Fid.Volume = avc->fid.Fid.Volume;
	    dirFid.Fid.Vnode = avc->parentVnode;
	    dirFid.Fid.Unique = avc->parentUnique;
	    /* Avoid this GetVCache call */
	    tvc = afs_GetVCache(&dirFid, areq, (afs_int32 *)0,
				(struct vcache*)0, WRITE_LOCK);
	    if (tvc) {
		dirBits = afs_GetAccessBits(tvc, arights, areq);
		afs_PutVCache(tvc, WRITE_LOCK);
	    }
	}
	else
	    dirBits = 0xffffffff;	/* assume OK; this is a race condition */
	if (arights & PRSFS_ADMINISTER)
	    fileBits = afs_GetAccessBits(avc, arights, areq);
	else
	    fileBits = 0;	/* don't make call if results don't matter */

	/* compute basic rights in fileBits, taking A from file bits */
	fileBits = (fileBits & PRSFS_ADMINISTER) | (dirBits & ~PRSFS_ADMINISTER);

	/* for files, throw in R and W if have I and A (owner).  This makes
	   insert-only dirs work properly */
	if (vType(avc) != VDIR && (fileBits & (PRSFS_ADMINISTER | PRSFS_INSERT)) ==
	    (PRSFS_ADMINISTER | PRSFS_INSERT))
	    fileBits |= (PRSFS_READ | PRSFS_WRITE);

	if (check_mode_bits & CHECK_MODE_BITS) {
	    /* owner mode bits are further restrictions on the access mode
             * The mode bits are mapped to protection bits through the
             * fileModeMap. If CMB_ALLOW_EXEC_AS_READ is set, it's from the
             * NFS translator and we don't know if it's a read or execute
             * on the NFS client, but both need to read the data.
             */
	    mask = (avc->m.Mode & 0700) >> 6;	/* file restrictions to use */
	    fileBits &= ~fileModeMap[mask];
	    if (check_mode_bits & CMB_ALLOW_EXEC_AS_READ) {
		 if (avc->m.Mode & 0100) fileBits |= PRSFS_READ;
	    }
	}
	return ((fileBits & arights) == arights);	/* true if all rights bits are on */
    }
}


#if	defined(AFS_SUN5_ENV) || (defined(AFS_SGI_ENV) && !defined(AFS_SGI65_ENV))
afs_access(OSI_VC_ARG(avc), amode, flags, acred)
    int flags;		
#else
afs_access(OSI_VC_ARG(avc), amode, acred)
#endif
    OSI_VC_DECL(avc);
    register afs_int32 amode;
    struct AFS_UCRED *acred; {
    register afs_int32 code;
    struct vrequest treq;
    OSI_VC_CONVERT(avc)

    AFS_STATCNT(afs_access);
    afs_Trace3(afs_iclSetp, CM_TRACE_ACCESS, ICL_TYPE_POINTER, avc, 
	       ICL_TYPE_INT32, amode, ICL_TYPE_INT32, avc->m.Length);
    if (code = afs_InitReq(&treq, acred)) return code;

    code = afs_VerifyVCache(avc, &treq);
    if (code) {
      code = afs_CheckCode(code, &treq, 16);
      return code; 
    }

    /* if we're looking for write access and we have a read-only file system, report it */
    if ((amode & VWRITE) && (avc->states & CRO)) {
	return EROFS;
    }
    code = 1;		/* Default from here on in is access ok. */
    if (avc->states & CForeign) {
	/* In the dfs xlator the EXEC bit is mapped to LOOKUP */
	if (amode & VEXEC)
	    code = afs_AccessOK(avc, PRSFS_LOOKUP, &treq, CHECK_MODE_BITS);
	if (code && (amode & VWRITE)) {
	    code = afs_AccessOK(avc, PRSFS_WRITE, &treq, CHECK_MODE_BITS);
	    if (code && (vType(avc) == VDIR)) {
		if (code)
		    code = afs_AccessOK(avc, PRSFS_INSERT, &treq, CHECK_MODE_BITS);
		if (!code) 
		    code = afs_AccessOK(avc, PRSFS_DELETE, &treq, CHECK_MODE_BITS);
	    }	
	}	
	if (code && (amode & VREAD)) 
	    code = afs_AccessOK(avc, PRSFS_READ, &treq, CHECK_MODE_BITS);
    } else {
	if (vType(avc) == VDIR) {
	    if (amode & VEXEC) 
	       code = afs_AccessOK(avc, PRSFS_LOOKUP, &treq, CHECK_MODE_BITS);
	    if (code && (amode & VWRITE)) {
		code = afs_AccessOK(avc, PRSFS_INSERT, &treq, CHECK_MODE_BITS);
		if (!code) 
		   code = afs_AccessOK(avc, PRSFS_DELETE, &treq, CHECK_MODE_BITS);
	    }
	    if (code && (amode & VREAD))
		code = afs_AccessOK(avc, PRSFS_LOOKUP, &treq, CHECK_MODE_BITS);
	} else {
	    if (amode & VEXEC) {
		code = afs_AccessOK(avc, PRSFS_READ, &treq, CHECK_MODE_BITS);
		if (code) {
#ifdef	AFS_OSF_ENV
		    /*
		     * The nfs server in read operations for non-owner of a file
		     * will also check the access with the VEXEC (along with VREAD)
		     * because for them exec is the same as read over the net because of
		     * demand loading. But this means if the mode bit is '-rw' the call
		     * will fail below; so for this particular case where both modes are
		     * specified (only in rfs_read so far) and from the xlator requests
		     * we return succes.
		     */
		    if (!((amode & VREAD) && AFS_NFSXLATORREQ(acred)))
#endif
		    if ((avc->m.Mode & 0100) == 0) code = 0;
		} else if (avc->m.Mode & 0100) code = 1;
	    }
	    if (code && (amode & VWRITE)) 
	    {
	       code = afs_AccessOK(avc, PRSFS_WRITE, &treq, CHECK_MODE_BITS);

	       /* The above call fails when the NFS translator tries to copy
	       ** a file with r--r--r-- permissions into a directory which
               ** has system:anyuser acl. This is because the destination file
	       ** file is first created with r--r--r-- permissions through an
	       ** unauthenticated connectin.  hence, the above afs_AccessOK
	       ** call returns failure. hence, we retry without any file 
	       ** mode bit checking */
	       if ( !code && AFS_NFSXLATORREQ(acred) && avc->m.Owner == ANONYMOUSID)
		code=afs_AccessOK(avc,PRSFS_WRITE, &treq, DONT_CHECK_MODE_BITS);
	    }
	    if (code && (amode & VREAD)) 
	       code = afs_AccessOK(avc, PRSFS_READ, &treq, CHECK_MODE_BITS);
	}
    }
    if (code)
	return 0;		/* if access is ok */
    else {
      code = afs_CheckCode(EACCES, &treq, 17);	     /* failure code */
      return code;
    }
}

