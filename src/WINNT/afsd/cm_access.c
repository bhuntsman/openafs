/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 *
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <winsock2.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <nb30.h>
#include <osi.h>

#include "afsd.h"

/* called with scp write-locked, check to see if we have the ACL info we need
 * and can get it w/o blocking for any locks.
 *
 * Never drops the scp lock, but may fail if the access control info comes from
 * the parent directory, and the parent's scache entry can't be found, or it
 * can't be locked.  Thus, this must always be called in a while loop to stabilize
 * things, since we can always lose the race condition getting to the parent vnode.
 */
int cm_HaveAccessRights(struct cm_scache *scp, struct cm_user *up, long rights,
	long *outRightsp)
{
	cm_scache_t *aclScp;
        long code;
        cm_fid_t tfid;
        int didLock;
        long trights;
	
	didLock = 0;
	if (scp->fileType == CM_SCACHETYPE_DIRECTORY) {
        	aclScp = scp;
                cm_HoldSCache(scp);
	}
        else {
		tfid.cell = scp->fid.cell;
		tfid.volume = scp->fid.volume;
                tfid.vnode = scp->parentVnode;
                tfid.unique = scp->parentUnique;
		aclScp = cm_FindSCache(&tfid);
                if (!aclScp) return 0;
                if (aclScp != scp) {
	                code = lock_TryMutex(&aclScp->mx);
	                if (code == 0) {
				/* can't get lock safely and easily */
				cm_ReleaseSCache(aclScp);
	                        return 0;
        	        }

			/* check that we have a callback, too */
                        if (!cm_HaveCallback(aclScp)) {
				/* can't use it */
                                lock_ReleaseMutex(&aclScp->mx);
                                cm_ReleaseSCache(aclScp);
                                return 0;
                        }
                        didLock = 1;
		}
	}

	lock_AssertMutex(&aclScp->mx);
        
        /* now if rights is a subset of the public rights, we're done.
	 * Otherwise, if we an explicit acl entry, we're also in good shape,
	 * and can definitively answer.
         */
	if ((~aclScp->anyAccess & rights) == 0) {
		*outRightsp = rights;
        }
        else {
		/* we have to check the specific rights info */
                code = cm_FindACLCache(aclScp, up, &trights);
                if (code) {
			code = 0;
                        goto done;
                }
                *outRightsp = trights;
        }

	/* check mode bits */
	if (!(scp->unixModeBits & 0400))
		*outRightsp &= ~PRSFS_READ;
	if (!(scp->unixModeBits & 0200))
		*outRightsp &= ~PRSFS_WRITE;

        code = 1;
        /* fall through */
        
done:
	if (didLock) lock_ReleaseMutex(&aclScp->mx);
        cm_ReleaseSCache(aclScp);
        return code;
}

/* called with locked scp; ensures that we have an ACL cache entry for the
 * user specified by the parameter "up."
 * In pathological race conditions, this function may return success without
 * having loaded the entry properly (due to a racing callback revoke), so this
 * function must also be called in a while loop to make sure that we eventually
 * succeed.
 */
long cm_GetAccessRights(struct cm_scache *scp, struct cm_user *up,
	struct cm_req *reqp)
{
	long code;
	cm_fid_t tfid;
        cm_scache_t *aclScp;

	/* pretty easy: just force a pass through the fetch status code */
        
	osi_Log2(afsd_logp, "GetAccess scp %x user %x", scp, up);

        /* first, start by finding out whether we have a directory or something
	 * else, so we can find what object's ACL we need.
         */
        code = cm_SyncOp(scp, NULL, up, reqp, 0, CM_SCACHESYNC_GETSTATUS
        		| CM_SCACHESYNC_NEEDCALLBACK);
                        
        if (code) return code;
        
        if (scp->fileType != CM_SCACHETYPE_DIRECTORY) {
		/* not a dir, use parent dir's acl */
		tfid.cell = scp->fid.cell;
	        tfid.volume = scp->fid.volume;
	        tfid.vnode = scp->parentVnode;
	        tfid.unique = scp->parentUnique;
		lock_ReleaseMutex(&scp->mx);
		code = cm_GetSCache(&tfid, &aclScp, up, reqp);
                if (code) {
			lock_ObtainMutex(&scp->mx);
                        return code;
                }
                
		osi_Log1(afsd_logp, "GetAccess parent %x", aclScp);
		lock_ObtainMutex(&aclScp->mx);
                code = cm_GetCallback(aclScp, up, reqp, 1);
                lock_ReleaseMutex(&aclScp->mx);
                cm_ReleaseSCache(aclScp);
                lock_ObtainMutex(&scp->mx);
        }
        else {
		code = cm_GetCallback(scp, up, reqp, 1);
        }

	return code;
}
