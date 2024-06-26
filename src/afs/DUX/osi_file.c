/* Copyright (C) 1995, 1989 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h"  /* afs statistics */


int afs_osicred_initialized=0;
struct  AFS_UCRED afs_osi_cred;
afs_lock_t afs_xosi;		/* lock is for tvattr */
extern struct osi_dev cacheDev;
extern struct mount *afs_cacheVfsp;


void *osi_UFSOpen(ainode)
    afs_int32 ainode;
{
    struct inode *ip;
    register struct osi_file *afile = NULL;
    extern int cacheDiskType;
    afs_int32 code = 0;
    int dummy;
    AFS_STATCNT(osi_UFSOpen);
    if(cacheDiskType != AFS_FCACHE_TYPE_UFS) {
	osi_Panic("UFSOpen called for non-UFS cache\n");
    }
    if (!afs_osicred_initialized) {
	/* valid for alpha_osf, SunOS, Ultrix */
	bzero((char *)&afs_osi_cred, sizeof(struct AFS_UCRED));
	afs_osi_cred.cr_ref++;
	afs_osicred_initialized = 1;
    }
    afile = (struct osi_file *) osi_AllocSmallSpace(sizeof(struct osi_file));
    AFS_GUNLOCK();
    code = igetinode(afs_cacheVfsp, (dev_t) cacheDev.dev, (ino_t)ainode, &ip, &dummy);
    AFS_GLOCK();
    if (code) {
	osi_FreeSmallSpace(afile);
	osi_Panic("UFSOpen: igetinode failed");
    }
    IN_UNLOCK(ip);
    afile->vnode = ITOV(ip);
    afile->size = VTOI(afile->vnode)->i_size;
    afile->offset = 0;
    afile->proc = (int (*)()) 0;
    afile->inum = ainode;        /* for hint validity checking */
    return (void *)afile;
}

afs_osi_Stat(afile, astat)
    register struct osi_file *afile;
    register struct osi_stat *astat; {
    register afs_int32 code;
    struct vattr tvattr;
    AFS_STATCNT(osi_Stat);
    MObtainWriteLock(&afs_xosi,320);
    AFS_GUNLOCK();
    VOP_GETATTR(afile->vnode, &tvattr, &afs_osi_cred, code);
    AFS_GLOCK();
    if (code == 0) {
	astat->size = tvattr.va_size;
	astat->blksize = tvattr.va_blocksize;
	astat->mtime = tvattr.va_mtime.tv_sec;
	astat->atime = tvattr.va_atime.tv_sec;
    }
    MReleaseWriteLock(&afs_xosi);
    return code;
}

osi_UFSClose(afile)
     register struct osi_file *afile;
  {
      AFS_STATCNT(osi_Close);
      if(afile->vnode) {
	AFS_RELE(afile->vnode);
      }
      
      osi_FreeSmallSpace(afile);
      return 0;
  }

osi_UFSTruncate(afile, asize)
    register struct osi_file *afile;
    afs_int32 asize; {
    struct AFS_UCRED *oldCred;
    struct vattr tvattr;
    register afs_int32 code;
    struct osi_stat tstat;
    AFS_STATCNT(osi_Truncate);

    /* This routine only shrinks files, and most systems
     * have very slow truncates, even when the file is already
     * small enough.  Check now and save some time.
     */
    code = afs_osi_Stat(afile, &tstat);
    if (code || tstat.size <= asize) return code;
    MObtainWriteLock(&afs_xosi,321);    
    VATTR_NULL(&tvattr);
    /* note that this credential swapping stuff is only necessary because
	of ufs's references directly to u.u_cred instead of to
	credentials parameter.  Probably should fix ufs some day. */
    oldCred = u.u_cred;	    /* remember old credentials pointer  */
    u.u_cred = &afs_osi_cred;   /* temporarily use superuser credentials */
    tvattr.va_size = asize;
    AFS_GUNLOCK();
    VOP_SETATTR(afile->vnode, &tvattr, &afs_osi_cred, code);
    AFS_GLOCK();
    u.u_cred = oldCred;	    /* restore */
    MReleaseWriteLock(&afs_xosi);
    return code;
}

void osi_DisableAtimes(avp)
struct vnode *avp;
{
   struct inode *ip = VTOI(avp);
   ip->i_flag &= ~IACC;
}


/* Generic read interface */
afs_osi_Read(afile, offset, aptr, asize)
    register struct osi_file *afile;
    int offset;
    char *aptr;
    afs_int32 asize; {
    struct AFS_UCRED *oldCred;
    unsigned int resid;
    register afs_int32 code;
    register afs_int32 cnt1=0;
    AFS_STATCNT(osi_Read);

    /**
      * If the osi_file passed in is NULL, panic only if AFS is not shutting
      * down. No point in crashing when we are already shutting down
      */
    if ( !afile ) {
	if ( !afs_shuttingdown )
	    osi_Panic("osi_Read called with null param");
	else
	    return EIO;
    }

    if (offset != -1) afile->offset = offset;
    AFS_GUNLOCK();
    code = gop_rdwr(UIO_READ, afile->vnode, (caddr_t) aptr, asize, afile->offset,
                  AFS_UIOSYS, IO_UNIT, &afs_osi_cred, &resid);
    AFS_GLOCK();
    if (code == 0) {
	code = asize - resid;
	afile->offset += code;
	osi_DisableAtimes(afile->vnode);
    }
    else {
	afs_Trace2(afs_iclSetp, CM_TRACE_READFAILED, ICL_TYPE_INT32, resid,
		 ICL_TYPE_INT32, code);
	code = -1;
    }
    return code;
}

/* Generic write interface */
afs_osi_Write(afile, offset, aptr, asize)
    register struct osi_file *afile;
    char *aptr;
    afs_int32 offset;
    afs_int32 asize; {
    struct AFS_UCRED *oldCred;
    unsigned int resid;
    register afs_int32 code;
    AFS_STATCNT(osi_Write);
    if ( !afile )
        osi_Panic("afs_osi_Write called with null param");
    if (offset != -1) afile->offset = offset;
    {
	struct ucred *tmpcred = u.u_cred;
	u.u_cred = &afs_osi_cred;
	AFS_GUNLOCK();
	code = gop_rdwr(UIO_WRITE, afile->vnode, (caddr_t) aptr, asize, afile->offset,
                  AFS_UIOSYS, IO_UNIT, &afs_osi_cred, &resid);
	AFS_GLOCK();
	u.u_cred = tmpcred;
    }
    if (code == 0) {
	code = asize - resid;
	afile->offset += code;
    }
    else {
	code = -1;
    }
    if (afile->proc) {
	(*afile->proc)(afile, code);
    }
    return code;
}


/*  This work should be handled by physstrat in ca/machdep.c.
    This routine written from the RT NFS port strategy routine.
    It has been generalized a bit, but should still be pretty clear. */
int afs_osi_MapStrategy(aproc, bp)
	int (*aproc)();
	register struct buf *bp;
{
    afs_int32 returnCode;

    AFS_STATCNT(osi_MapStrategy);
    returnCode = (*aproc) (bp);

    return returnCode;
}



void
shutdown_osifile()
{
  extern int afs_cold_shutdown;

  AFS_STATCNT(shutdown_osifile);
  if (afs_cold_shutdown) {
    afs_osicred_initialized = 0;
  }
}

