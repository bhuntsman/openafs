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
#include <malloc.h>
#include <string.h>

#include <osi.h>
#include <rx/rx.h>

#include "afsd.h"

osi_rwlock_t cm_userLock;

cm_user_t *cm_rootUserp;

void cm_InitUser(void)
{
	static osi_once_t once;
        
        if (osi_Once(&once)) {
		lock_InitializeRWLock(&cm_userLock, "cm_userLock");
                osi_EndOnce(&once);
	}
        
        cm_rootUserp = cm_NewUser();
}

cm_user_t *cm_NewUser(void)
{
	cm_user_t *up;
        
        up = malloc(sizeof(*up));
        memset(up, 0, sizeof(*up));
        up->refCount = 1;
        up->vcRefs = 1;		/* from caller */
        lock_InitializeMutex(&up->mx, "cm_user_t");
        return up;
}

/* must be called with locked userp */
cm_ucell_t *cm_GetUCell(cm_user_t *userp, cm_cell_t *cellp)
{
	cm_ucell_t *ucp;
        
	lock_AssertMutex(&userp->mx);
        for(ucp = userp->cellInfop; ucp; ucp=ucp->nextp) {
		if (ucp->cellp == cellp) break;
        }
        
        if (!ucp) {
		ucp = malloc(sizeof(*ucp));
                memset(ucp, 0, sizeof(*ucp));
                ucp->nextp = userp->cellInfop;
		if (userp->cellInfop)
			ucp->iterator = userp->cellInfop->iterator + 1;
		else
			ucp->iterator = 1;
                userp->cellInfop = ucp;
                ucp->cellp = cellp;
        }
        
        return ucp;
}

cm_ucell_t *cm_FindUCell(cm_user_t *userp, int iterator)
{
	cm_ucell_t *ucp;
	cm_ucell_t *best;

	best = NULL;
	lock_AssertMutex(&userp->mx);
	for (ucp = userp->cellInfop; ucp; ucp = ucp->nextp) {
		if (ucp->iterator >= iterator)
			best = ucp;
		else
			break;
	}
	return best;
}

void cm_HoldUser(cm_user_t *up)
{
	lock_ObtainWrite(&cm_userLock);
	up->refCount++;
	lock_ReleaseWrite(&cm_userLock);
}

void cm_ReleaseUser(cm_user_t *up)
{
	cm_ucell_t *ucp;
        cm_ucell_t *ncp;

	if (up == NULL) return;

	lock_ObtainWrite(&cm_userLock);
	osi_assert(up->refCount-- > 0);
	if (up->refCount == 0) {
		lock_FinalizeMutex(&up->mx);
                for(ucp = up->cellInfop; ucp; ucp = ncp) {
			ncp = ucp->nextp;
			if (ucp->ticketp) free(ucp->ticketp);
                        free(ucp);
                }
                free(up);
        }
	lock_ReleaseWrite(&cm_userLock);
}

/* release the count of the # of connections that use this user structure.
 * When this hits zero, we know we won't be getting an new requests from
 * this user, and thus we can start GC'ing connections.  Ref count on user
 * won't hit zero until all cm_conn_t's have been GC'd, since they hold
 * refCount references to userp.
 */
void cm_ReleaseUserVCRef(cm_user_t *userp)
{
	lock_ObtainMutex(&userp->mx);
	osi_assert(userp->vcRefs-- > 0);
	lock_ReleaseMutex(&userp->mx);
}


/*
 * Check if any users' tokens have expired and if they have then do the 
 * equivalent of unlogging the user for that particular cell for which 
 * the tokens have expired.
 * ref. cm_IoctlDelToken() in cm_ioctl.c 
 * This routine is called by the cm_Daemon() ie. the periodic daemon.
 * every cm_daemonTokenCheckInterval seconds 
 */
void cm_CheckTokenCache(long now)
{
        extern smb_vc_t *smb_allVCsp; /* global vcp list */
	smb_vc_t   *vcp;
	smb_user_t *usersp;
	cm_user_t  *userp;
	cm_ucell_t *ucellp;
	BOOL bExpired=FALSE;
  
	/* 
	 * For every vcp, get the user and check his tokens 
	 */
	lock_ObtainWrite(&smb_rctLock);
	for(vcp=smb_allVCsp; vcp; vcp=vcp->nextp) {
	        for(usersp=vcp->usersp; usersp; usersp=usersp->nextp) {
		        userp=usersp->userp;
			osi_assert(userp);
			lock_ObtainMutex(&userp->mx);
			for(ucellp=userp->cellInfop; ucellp; ucellp=ucellp->nextp) {
			  if(ucellp->flags & CM_UCELLFLAG_RXKAD) {
			    if(ucellp->expirationTime < now) {
				  /* this guy's tokens have expired */
				  osi_Log3(afsd_logp, "cm_CheckTokens: Tokens for user:%s have expired expiration time:0x%x ucellp:%x", ucellp->userName, ucellp->expirationTime, ucellp);
				  if (ucellp->ticketp) {
				          free(ucellp->ticketp);
					  ucellp->ticketp = NULL;
				  }
				  ucellp->flags &= ~CM_UCELLFLAG_RXKAD;
				  ucellp->gen++;
				  bExpired=TRUE;
			    }
			  } 
			}
			lock_ReleaseMutex(&userp->mx);
			if(bExpired) {
			        bExpired=FALSE;
				cm_ResetACLCache(userp);
			}
		}
	}
	lock_ReleaseWrite(&smb_rctLock);
}
