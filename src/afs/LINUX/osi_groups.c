/* Copyright (C) 1995, 1989, 1998 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/*
 * osi_groups.c
 *
 * Implements:
 * setgroups (syscall)
 * setpag
 *
 */
#include "../afs/param.h"
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#include "../afs/afs_stats.h"  /* statistics */
#ifdef AFS_LINUX22_ENV
#include "../h/smp_lock.h"
#endif

static int afs_getgroups(cred_t *cr, gid_t *groups);
static int afs_setgroups(cred_t **cr, int ngroups, gid_t *gidset, int change_parent);

/* Only propogate the PAG to the parent process. Unix's propogate to 
 * all processes sharing the cred.
 */
int set_pag_in_parent(int pag, int g0, int g1)
{
    gid_t *gp = current->p_pptr->groups;
    int ngroups;
    int i;

    
    ngroups = current->p_pptr->ngroups;
    gp = current->p_pptr->groups;


    if (afs_get_pag_from_groups(gp[0], gp[1]) == NOPAG) {
	/* We will have to shift grouplist to make room for pag */
	if (ngroups + 2 > NGROUPS) {
	    return EINVAL;
	}
	for (i = ngroups-1; i >= 0; i--) {
 	    gp[i+2] = gp[i];
 	}
	ngroups += 2;
    }
    gp[0] = g0;
    gp[1] = g1;
    if (ngroups < NGROUPS)
	gp[ngroups] = NOGROUP;

    current->p_pptr->ngroups = ngroups;
    return 0;
}

int setpag(cred_t **cr, afs_uint32 pagvalue, afs_uint32 *newpag, int change_parent)
{
    gid_t *gidset;
    afs_int32 ngroups, code = 0;
    int j;

    AFS_STATCNT(setpag);

    gidset = (gid_t *) osi_Alloc(NGROUPS*sizeof(gidset[0]));
    ngroups = afs_getgroups(*cr, gidset);

    if (afs_get_pag_from_groups(gidset[0], gidset[1]) == NOPAG) {
	/* We will have to shift grouplist to make room for pag */
	if (ngroups + 2 > NGROUPS) {
	    osi_Free((char *)gidset, NGROUPS*sizeof(int));
	    return EINVAL;
	}
	for (j = ngroups - 1; j >= 0; j--) {
 	    gidset[j+2] = gidset[j];
 	}
	ngroups += 2;
    }
    *newpag = (pagvalue == -1 ? genpag(): pagvalue);
    afs_get_groups_from_pag(*newpag, &gidset[0], &gidset[1]);
    code = afs_setgroups(cr, ngroups, gidset, change_parent);

    /* If change_parent is set, then we should set the pag in the parent as
     * well.
     */
    if (change_parent && !code) {
	code = set_pag_in_parent(*newpag, gidset[0], gidset[1]);
    }

    osi_Free((char *)gidset, NGROUPS*sizeof(int));
    return code;
}


/* Intercept the standard system call. */
extern int (*sys_setgroupsp)(int gidsetsize, gid_t *grouplist);
asmlinkage int afs_xsetgroups(int gidsetsize, gid_t *grouplist)
{
    int code;
    cred_t *cr = crref();
    int junk;
    int old_pag;

    lock_kernel();
    old_pag = PagInCred(cr);
    crfree(cr);
    unlock_kernel();

    code = (*sys_setgroupsp)(gidsetsize, grouplist);
    if (code) {
	return code;
    }

    lock_kernel();
    cr = crref();
    if (old_pag != NOPAG && PagInCred(cr) == NOPAG) {
	/* re-install old pag if there's room. */
	code = setpag(&cr, old_pag, &junk, 0);
    }
    crfree(cr);
    unlock_kernel();

    return code;
}

static int afs_setgroups(cred_t **cr, int ngroups, gid_t *gidset, int change_parent)
{
    int ngrps;
    int i;
    gid_t *gp;

    AFS_STATCNT(afs_setgroups);

    if (ngroups > NGROUPS)
	return EINVAL;

    gp = (*cr)->cr_groups;
    if (ngroups < NGROUPS)
	gp[ngroups] = (gid_t)NOGROUP;

    for (i = ngroups; i > 0; i--) {
	*gp++ = *gidset++;
    }

    (*cr)->cr_ngroups = ngroups;
    crset(*cr);
    return (0);
}

/* Returns number of groups. And we trust groups to be large enough to
 * hold all the groups.
 */
static int afs_getgroups(cred_t *cr, gid_t *groups)
{
    int i;
    gid_t *gp = cr->cr_groups;
    int n = cr->cr_ngroups;
    AFS_STATCNT(afs_getgroups);

    for (i = 0; (i < n) && (*gp != (gid_t)NOGROUP); i++) {
	*groups++ = *gp++;
    }
    return i;
}

