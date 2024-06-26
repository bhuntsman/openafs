/* Copyright (C) 1995 Transarc Corporation - All rights reserved. */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h"   /* statistics stuff */


int afs_mount();
int afs_unmount();
int afs_root();
int afs_statfs();
int afs_sync();

struct vfsops Afs_vfsops = {
    afs_mount,
    afs_unmount,
    afs_root,
    afs_statfs,
    afs_sync,
};

struct vfs *afs_globalVFS = 0;
struct vcache *afs_globalVp = 0;
int afs_rootCellIndex = 0;

#if !defined(AFS_USR_AIX_ENV)
#include "../sys/syscall.h"
#endif

afs_mount(afsp, path, data)
    char *path;
    caddr_t data; 
    struct vfs *afsp;
{
    AFS_STATCNT(afs_mount);

    if (afs_globalVFS) {
	/* Don't allow remounts since some system (like AIX) don't handle it well */
 	return (setuerror(EBUSY));
    }
    afs_globalVFS = afsp;
    afsp->vfs_bsize = 8192;
    afsp->vfs_fsid.val[0] = AFS_VFSMAGIC; /* magic */
    afsp->vfs_fsid.val[1] = AFS_VFSFSID; 

    return 0;
}

afs_unmount (afsp)
    struct vfs *afsp; {
    AFS_STATCNT(afs_unmount);
    afs_globalVFS = 0;
    afs_shutdown();
    return 0;
}

afs_root (OSI_VFS_ARG(afsp), avpp)
    OSI_VFS_DECL(afsp);
    struct vnode **avpp; {
    register afs_int32 code = 0;
    struct vrequest treq;
    register struct vcache *tvp=0;
    OSI_VFS_CONVERT(afsp)

    AFS_STATCNT(afs_root);
    if (afs_globalVp && (afs_globalVp->states & CStatd)) {
	tvp = afs_globalVp;
    } else {
	if (!(code = afs_InitReq(&treq, u.u_cred)) &&
	    !(code = afs_CheckInit())) {
	    tvp = afs_GetVCache(&afs_rootFid, &treq, (afs_int32 *)0,
				(struct vcache*)0, WRITE_LOCK);
	    /* we really want this to stay around */
	    if (tvp) {
		afs_globalVp = tvp;
	    } else
		code = ENOENT;
	}
    }
    if (tvp) {
	VN_HOLD((struct vnode *)tvp);

	tvp->v.v_flag |= VROOT;	    /* No-op on Ultrix 2.2 */
	afs_globalVFS = afsp;
	*avpp = (struct vnode *) tvp;
    }

    afs_Trace3(afs_iclSetp, CM_TRACE_GOPEN, ICL_TYPE_POINTER, *avpp,
	       ICL_TYPE_INT32, 0, ICL_TYPE_INT32, code);
    return code;
}

afs_sync(afsp)
    struct vfs *afsp; 
{
    AFS_STATCNT(afs_sync);
    return 0;
}

afs_statfs(afsp, abp)
    register struct vfs *afsp;
    struct statfs *abp;
 {
	AFS_STATCNT(afs_statfs);
	abp->f_type = 0;
	abp->f_bsize = afsp->vfs_bsize;
	abp->f_fsid.val[0] = AFS_VFSMAGIC; /* magic */
	abp->f_fsid.val[1] = AFS_VFSFSID;
	return 0;
}

afs_mountroot()
{
    AFS_STATCNT(afs_mountroot);
    return(EINVAL);
}

afs_swapvp() 
{
    AFS_STATCNT(afs_swapvp);
    return(EINVAL);
}
