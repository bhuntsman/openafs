/* Copyright (C) 1998 Transarc Corporation - All rights reserved. */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h"  /* statistics */

/* Try to discard pages, in order to recycle a vcache entry.
 *
 * We also make some sanity checks:  ref count, open count, held locks.
 *
 * We also do some non-VM-related chores, such as releasing the cred pointer
 * (for AIX and Solaris) and releasing the gnode (for AIX).
 *
 * Locking:  afs_xvcache lock is held.  If it is dropped and re-acquired,
 *   *slept should be set to warn the caller.
 *
 * Formerly, afs_xvcache was dropped and re-acquired for Solaris, but now it
 * is not dropped and re-acquired for any platform.  It may be that *slept is
 * therefore obsolescent.
 */
int
osi_VM_FlushVCache(avc, slept)
    struct vcache *avc;
    int *slept;
{
    if (avc->vrefCount > 1)
	return EBUSY;

    if (avc->opens)
	return EBUSY;

    return 0;
}

/* Try to store pages to cache, in order to store a file back to the server.
 *
 * Locking:  the vcache entry's lock is held.  It will usually be dropped and
 * re-obtained.
 */
void
osi_VM_StoreAllSegments(avc)
    struct vcache *avc;
{
    ;	/* Nothing here yet */
}

/* Try to invalidate pages, for "fs flush" or "fs flushv"; or
 * try to free pages, when deleting a file.
 *
 * Locking:  the vcache entry's lock is held.  It may be dropped and 
 * re-obtained.
 */
void
osi_VM_TryToSmush(avc, acred, sync)
    struct vcache *avc;
    struct AFS_UCRED *acred;
    int sync;
{
    struct vnode *vp = (struct vnode *)avc;

    /* Flush the delayed write blocks associated with this vnode
     * from the buffer cache
     */
    if ((vp->v_flag & VTEXT) == 0) {
        mpurge(vp);
    }
    /* Mark the cached blocks on the free list as invalid; it invalidates blocks
     * associated with vp which are on the freelist.
     */
    binvalfree(vp);
    mpurge(vp);
}

/* Purge VM for a file when its callback is revoked.
 *
 * Locking:  No lock is held, not even the global lock.
 */
void
osi_VM_FlushPages(avc, credp)
    struct vcache *avc;
    struct AFS_UCRED *credp;
{
    ;	/* Nothing here yet */
}

/* Purge pages beyond end-of-file, when truncating a file.
 *
 * Locking:  no lock is held, not even the global lock.
 * activeV is raised.  This is supposed to block pageins, but at present
 * it only works on Solaris.
 */
void
osi_VM_Truncate(avc, alen, acred)
    struct vcache *avc;
    int alen;
    struct AFS_UCRED *acred;
{
    ;	/* Nothing here yet */
}
