/* Copyright (C) 1995, 1989 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#ifndef _AFS_OSI_
#define _AFS_OSI_

#include "../afs/param.h"
#include "../h/types.h"
#include "../h/param.h"

#ifdef AFS_LINUX20_ENV
#ifndef _LINUX_CODA_FS_I
#define _LINUX_CODA_FS_I
#define _CODA_HEADER_
struct coda_inode_info {};
#endif
#include "../h/fs.h"
#include "../h/mm.h"
#endif


/* this is just a dummy type decl, we're really using struct sockets here */
struct osi_socket {
    int junk;
};

struct osi_stat {
    afs_int32 size;	    /* file size in bytes */
    afs_int32 blksize;   /* optimal transfer size in bytes */
    afs_int32 mtime;	    /* modification date */
    afs_int32 atime;	    /* access time */
};

struct osi_file {
    afs_int32 size;	    	/* file size in bytes XXX Must be first field XXX */
#ifdef AFS_LINUX22_ENV
    struct dentry dentry; /* merely to hold the pointer to the inode. */
    struct file file; /* May need this if we really open the file. */
#else
    struct vnode *vnode;
#endif
#if	defined(AFS_HPUX102_ENV)
    k_off_t offset;
#else
    afs_int32 offset;
#endif
    int	(*proc)();	/* proc, which, if not null, is called on writes */
    char *rock;		/* rock passed to proc */
    ino_t inum;         /* guarantee validity of hint */
#if defined(UKERNEL)
    int fd;		/* file descriptor for user space files */
#endif /* defined(UKERNEL) */
};

struct osi_dev {
    afs_int32 dev;
};

struct afs_osi_WaitHandle {
    caddr_t proc;	/* process waiting */
};

#define	osi_SetFileProc(x,p)	((x)->proc=(p))
#define	osi_SetFileRock(x,r)	((x)->rock=(r))
#define	osi_GetFileProc(x)	((x)->proc)
#define	osi_GetFileRock(x)	((x)->rock)

#ifdef	AFS_TEXT_ENV
#define osi_FlushText(vp) if (hcmp((vp)->m.DataVersion, (vp)->flushDV) > 0) \
			    osi_FlushText_really(vp)
#else
#define osi_FlushText(vp)
#endif


#define AFSOP_STOP_RXEVENT   214 /* stop rx event deamon */
#define AFSOP_STOP_COMPLETE  215 /* afs has been shutdown */
#define AFSOP_STOP_RXK_LISTENER   217 /* stop rx listener daemon */


#define	osi_NPACKETS	20		/* number of cluster pkts to alloc */
extern struct osi_socket *osi_NewSocket();


/*
 * Alloc declarations.
 */
extern void *afs_osi_Alloc(size_t size);
extern void  afs_osi_Free(void *x, size_t size);
#define afs_osi_Alloc_NoSleep afs_osi_Alloc

extern void *osi_AllocSmallSpace(size_t size);
extern void  osi_FreeSmallSpace(void *x);
extern void *osi_AllocMediumSpace(size_t size);
extern void  osi_FreeMediumSpace(void *x);
extern void *osi_AllocLargeSpace(size_t size);
extern void  osi_FreeLargeSpace(void *x);

/*
 * Vnode related macros
 */
extern struct vnodeops *afs_ops;
#define	vType(vc)	    (vc)->v.v_type
#define	vSetType(vc,type)   (vc)->v.v_type = (type)
#define	IsAfsVnode(vc)	    ((vc)->v_op == afs_ops)
#define	SetAfsVnode(vc)	    (vc)->v_op = afs_ops
#define	vSetVfsp(vc,vfsp)   (vc)->v.v_vfsp = (vfsp)

#ifdef AFS_SGI65_ENV
#define	gop_lookupname(fnamep,segflg,followlink,dirvpp,compvpp) \
             lookupname((fnamep),(segflg),(followlink),(dirvpp),(compvpp),\
			NULL)
#else
#define	gop_lookupname(fnamep,segflg,followlink,dirvpp,compvpp) \
             lookupname((fnamep),(segflg),(followlink),(dirvpp),(compvpp))
#endif

/*
 * In IRIX 6.5 we cannot have DEBUG turned on since certain
 * system-defined structures are a different size with DEBUG on, the
 * kernel is compiled without DEBUG on, and the resulting differences
 * would break our ability to interact with the rest of the kernel.
 *
 * Is DEBUG only for turning the ASSERT() macro?  If so, we should
 * be able to eliminate DEBUG entirely.
 */
#if !defined(AFS_SGI65_ENV)
#ifndef	DEBUG
#define	DEBUG	1	/* Default is to enable debugging/logging */
#endif
#endif

/* 
 * Time related macros
 */
#define osi_GetuTime(x) osi_GetTime(x)

/* osi_timeval_t exists because SGI 6.x has two sizes of timeval. */
/** In 64 bit Solaris the timeval structure has members that are 64 bit
  * In the GetTime() interface we expect pointers to afs_int32. So the need to
  * define osi_timeval_t to have 32 bit members. To make this less ambiguous
  * we now use 32 bit quantities consistently all over the code.
  * In 64 bit HP-UX the timeval structure has a 64 bit member.
  */


#if defined(AFS_HPUX_ENV) || defined(AFS_SUN57_ENV) || (defined(AFS_SGI61_ENV) && defined(KERNEL) && defined(_K64U64))
typedef struct {
    afs_int32 tv_sec;
    afs_int32 tv_usec;
} osi_timeval_t;
#else
typedef struct timeval osi_timeval_t;
#endif /* AFS_SGI61_ENV */

/*
 * The following three routines provide the fid routines used by the buffer
 * and directory packages.
 */
#define dirp_Zap(afid)    (*(afid) = -1)
#define dirp_Eq(afid, bfid) (*(afid) == *(bfid))
#define dirp_Cpy(dfid,sfid) (*(dfid) = *(sfid))


/*
 * osi_ThreadUnique() should yield a value that can be found in ps
 * output in order to draw correspondences between ICL traces and what
 * is going on in the system.  So if ps cannot show thread IDs it is
 * likely to be the process ID instead.
 */
#define osi_ThreadUnique()	getpid()



#ifdef AFS_GLOBAL_SUNLOCK
#define AFS_ASSERT_GLOCK() \
    (ISAFS_GLOCK() || (osi_Panic("afs global lock not held"), 0))
#define AFS_ASSERT_RXGLOCK() \
    (ISAFS_RXGLOCK() || (osi_Panic("rx global lock not held"), 0))
#endif /* AFS_GLOBAL_SUNLOCK */



#ifndef KERNEL
#define AFS_GLOCK()
#define AFS_GUNLOCK()
#define ISAFS_GLOCK() 1
#define AFS_ASSERT_GLOCK()
#define AFS_RXGLOCK()
#define AFS_RXGUNLOCK()
#define ISAFS_RXGLOCK() 1
#define AFS_ASSERT_RXGLOCK()
#endif

/* On an MP that uses multithreading, splnet is not sufficient to provide
 * mutual exclusion because the other processors will not see it.  On some
 * early multiprocessors (SunOS413 & SGI5.2) splnet actually obtains a global
 * mutex, which this works in the UP expected way, it means that the whole MP
 * can only take one interrupt at a time; a serious performance penalty. */

#if ((defined(AFS_GLOBAL_SUNLOCK) || defined(RX_ENABLE_LOCKS)) && !defined(AFS_HPUX_ENV)) || !defined(KERNEL)
#define SPLVAR
#define NETPRI
#define USERPRI
#endif

/*
 * vnode/vcache ref count manipulation
 */
#if defined(UKERNEL)
#define AFS_RELE(vp) do { VN_RELE(vp); } while (0)
#else /* defined(UKERNEL) */
#define AFS_RELE(vp) do { AFS_GUNLOCK(); VN_RELE(vp); AFS_GLOCK(); } while (0)
#endif /* defined(UKERNEL) */
/*
 * For some reason we do bare refcount manipulation in some places, for some
 * platforms.  The assumption is apparently that either we wouldn't call
 * afs_inactive anyway (because we know the ref count is high), or that it's
 * OK not to call it (because we don't expect CUnlinked or CDirty).
 * (Also, of course, the vnode is assumed to be one of ours.  Can't use this
 * macro for V-file vnodes.)
 */
#define AFS_FAST_HOLD(vp) VN_HOLD(&(vp)->v)
#define AFS_FAST_RELE(vp) AFS_RELE(&(vp)->v)

/*
 * MP safe versions of routines to copy memory between user space
 * and kernel space. Call these to avoid taking page faults while
 * holding the global lock.
 */
#ifdef AFS_GLOBAL_SUNLOCK

#define AFS_COPYIN(SRC,DST,LEN,CODE)				\
	do {							\
	    int haveGlock = ISAFS_GLOCK();			\
	    if (haveGlock)					\
		AFS_GUNLOCK();					\
	    CODE = copyin((SRC),(DST),(LEN));			\
	    if (haveGlock)					\
		AFS_GLOCK();					\
	} while(0)

#define AFS_COPYINSTR(SRC,DST,LEN,CNT,CODE)			\
	do {							\
	    int haveGlock = ISAFS_GLOCK();			\
	    if (haveGlock)					\
		AFS_GUNLOCK();					\
	    CODE = copyinstr((SRC),(DST),(LEN),(CNT));		\
	    if (haveGlock)					\
		AFS_GLOCK();					\
	} while(0)

#define AFS_COPYOUT(SRC,DST,LEN,CODE)				\
	do {							\
	    int haveGlock = ISAFS_GLOCK();			\
	    if (haveGlock)					\
		AFS_GUNLOCK();					\
	    CODE = copyout((SRC),(DST),(LEN));			\
	    if (haveGlock)					\
		AFS_GLOCK();					\
	} while(0)

#ifdef AFS_OSF_ENV
#define AFS_UIOMOVE(SRC,LEN,RW,UIO,CODE)			\
	do {							\
	    int haveGlock = ISAFS_GLOCK();			\
	    if (haveGlock)					\
		AFS_GUNLOCK();					\
	    (UIO)->uio_rw = (RW);				\
	    CODE = uiomove((SRC),(LEN),(UIO));			\
	    if (haveGlock)					\
		AFS_GLOCK();					\
	} while(0)
#else /* AFS_OSF_ENV */
#define AFS_UIOMOVE(SRC,LEN,RW,UIO,CODE)			\
	do {							\
	    int haveGlock = ISAFS_GLOCK();			\
	    if (haveGlock)					\
		AFS_GUNLOCK();					\
	    CODE = uiomove((SRC),(LEN),(RW),(UIO));		\
	    if (haveGlock)					\
		AFS_GLOCK();					\
	} while(0)
#endif /* AFS_OSF_ENV */

#else /* AFS_GLOBAL_SUNLOCK */

#define AFS_COPYIN(SRC,DST,LEN,CODE)				\
	do {							\
	    CODE = copyin((SRC),(DST),(LEN));			\
	} while(0)

#define AFS_COPYINSTR(SRC,DST,LEN,CNT,CODE)			\
	do {							\
	    CODE = copyinstr((SRC),(DST),(LEN),(CNT));		\
	} while(0)

#define AFS_COPYOUT(SRC,DST,LEN,CODE)				\
	do {							\
	    CODE = copyout((SRC),(DST),(LEN));			\
	} while(0)

#ifdef AFS_OSF_ENV
#define AFS_UIOMOVE(SRC,LEN,RW,UIO,CODE)			\
	do {							\
	    (UIO)->uio_rw = (RW);				\
	    CODE = uiomove((SRC),(LEN),(UIO));			\
	} while(0)
#else /* AFS_OSF_ENV */
#define AFS_UIOMOVE(SRC,LEN,RW,UIO,CODE)			\
	do {							\
	    CODE = uiomove((SRC),(LEN),(RW),(UIO));		\
	} while(0)
#endif /* AFS_OSF_ENV */

#endif /* AFS_GLOBAL_SUNLOCK */

/*
 * encapsulation of kernel data structure accesses
 */
#define setuerror(erval)	u.u_error = (erval)
#define getuerror()		u.u_error

/* Macros for vcache/vnode and vfs arguments to vnode and vfs ops.
 * These are required for IRIX 6.4 and later, which pass behavior pointers.
 * Note that the _CONVERT routines get the ";" here so that argument lists
 * can have arguments after the OSI_x_CONVERT macro is called.
 */
#define OSI_VN_ARG(V) V
#define OSI_VN_DECL(V) struct vnode *V
#define OSI_VN_CONVERT(V)
#define OSI_VC_ARG(V) V
#define OSI_VC_DECL(V) struct vcache *V
#define OSI_VC_CONVERT(V)
#define OSI_VFS_ARG(V) V
#define OSI_VFS_DECL(V) struct vfs *V
#define OSI_VFS_CONVERT(V)


/*
** Macro for Solaris 2.6 returns 1 if file is larger than 2GB; else returns 0 
*/
#define AfsLargeFileUio(uio)       0
#define AfsLargeFileSize(pos, off) 0


/* VM function prototypes.
 */
#if defined(AFS_SUN5_ENV)
extern int osi_VM_GetDownD();
extern void osi_VM_PreTruncate();
#endif
#if defined(AFS_SGI_ENV)
extern void osi_VM_FSyncInval();
#endif
extern int osi_VM_FlushVCache();
extern void osi_VM_StoreAllSegments();
extern void osi_VM_TryToSmush();
extern void osi_VM_FlushPages();
extern void osi_VM_Truncate();

extern void osi_ReleaseVM();

/* Now include system specific OSI header file. It will redefine macros
 * defined here as required by the OS.
 */
#include "../afs/osi_machdep.h"

/* Declare any structures which use these macros after the OSI implementation
 * has had the opportunity to redefine them.
 */
extern struct AFS_UCRED afs_osi_cred;

#endif /* _AFS_OSI_ */


