/* Copyright (C) 1995, 1989, 1998 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/*
 * osi_groups.c
 *
 * Implements:
 * Afs_xsetgroups (syscall)
 * setpag
 *
 */
#include "../afs/param.h"
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#include "../afs/afs_stats.h"  /* statistics */

static int
afs_getgroups(
    struct ucred *cred,
    int ngroups,
    gid_t *gidset);

static int
afs_setgroups(
    struct proc *proc,
    struct ucred **cred,
    int ngroups,
    gid_t *gidset,
    int change_parent);

int
Afs_xsetgroups(p, args, retval)
    struct proc *p;
    void *args;
    int *retval;
{
    int code = 0;
    struct vrequest treq;

    AFS_STATCNT(afs_xsetgroups);
    AFS_GLOCK();

    code = afs_InitReq(&treq, u.u_cred);
    AFS_GUNLOCK();
    if (code) return code;

    code = setgroups(p, args, retval);
    /* Note that if there is a pag already in the new groups we don't
     * overwrite it with the old pag.
     */
    if (PagInCred(u.u_cred) == NOPAG) {
	if (((treq.uid >> 24) & 0xff) == 'A') {
	    AFS_GLOCK();
	    /* we've already done a setpag, so now we redo it */
	    AddPag(p, treq.uid, &p->p_rcred);
	    AFS_GUNLOCK();
	}
    }
    return code;
}


int
setpag(proc, cred, pagvalue, newpag, change_parent)
    struct proc *proc;
    struct ucred **cred;
    afs_uint32 pagvalue;
    afs_uint32 *newpag;
    afs_uint32 change_parent;
{
    gid_t gidset[NGROUPS];
    int ngroups, code;
    int j;

    AFS_STATCNT(setpag);
    ngroups = afs_getgroups(*cred, NGROUPS, gidset);
    if (afs_get_pag_from_groups(gidset[0], gidset[1]) == NOPAG) {
	/* We will have to shift grouplist to make room for pag */
	if (ngroups + 2 > NGROUPS) {
	    return (E2BIG);
	}
	for (j = ngroups -1; j >= 0; j--) {
 	    gidset[j+2] = gidset[j];
 	}
	ngroups += 2;
    }
    *newpag = (pagvalue == -1 ? genpag(): pagvalue);
    afs_get_groups_from_pag(*newpag, &gidset[0], &gidset[1]);
    code = afs_setgroups(proc, cred, ngroups, gidset, change_parent);
    return code;
}


static int
afs_getgroups(
    struct ucred *cred,
    int ngroups,
    gid_t *gidset)
{
    int ngrps, savengrps;
    gid_t *gp;

    AFS_STATCNT(afs_getgroups);
    savengrps = ngrps = MIN(ngroups, cred->cr_ngroups);
    gp = cred->cr_groups;
    while (ngrps--)
	*gidset++ = *gp++;   
    return savengrps;
}



static int
afs_setgroups(
    struct proc *proc,
    struct ucred **cred,
    int ngroups,
    gid_t *gidset,
    int change_parent)
{
    int ngrps;
    int i;
    gid_t *gp;
    struct ucred *newcr, *cr;

    AFS_STATCNT(afs_setgroups);
    /*
     * The real setgroups() call does this, so maybe we should too.
     *
     */
    if (ngroups > NGROUPS)
	return EINVAL;
    cr = *cred;
    if (!change_parent) {
	crhold(cr);
	newcr = crcopy(cr);
    } else
	newcr = cr;
    newcr->cr_ngroups = ngroups;
    gp = newcr->cr_groups;
    while (ngroups--)
	*gp++ = *gidset++;
    if (!change_parent) {
	substitute_real_creds(proc, NOUID, NOUID, NOGID, NOGID, newcr);
    }
    *cred = newcr;
    return(0);
}
