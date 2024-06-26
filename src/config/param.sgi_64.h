#ifndef	_PARAM_SGI_64_H
#define	_PARAM_SGI_64_H


#define AFS_VFS_ENV		1
#define AFS_VFSINCL_ENV		1
#define AFS_ENV			1	/* NOBODY uses this.... */
#define CMUSTD_ENV		1	/* NOBODY uses this.... */
#define AFS_SGI_ENV		1
#define AFS_SGI51_ENV		1	/* Dist from 5.0.1 */
#define	AFS_SGI52_ENV		1
#define	AFS_SGI53_ENV		1
#define AFS_SGI61_ENV		1
#define AFS_SGI62_ENV		1
#define AFS_SGI63_ENV		1
#define AFS_SGI64_ENV		1
#define AFS_SGI_EXMAG		1	/* use magic fields in extents for AFS extra fields */
/* AFS_SGI_SHORTSTACK not required since we have a 16K stack. */	

#define _ANSI_C_SOURCE		1	/* rx_user.h */

#define AFS_64BIT_ENV		1	/* Defines afs_int32 as int, not long. */
#define AFS_64BITPOINTER_ENV	1	/* pointers are 64 bits. */
#define AFS_HAVE_FFS		1	/* Use system's ffs. */
#define AFS_HAVE_STATVFS	1	/* System supports statvfs */

#include <afs/afs_sysnames.h>

#define AFS_GLOBAL_SUNLOCK 1

#define AFS_PIOCTL	64+1000
#define AFS_SETPAG	65+1000
#define AFS_IOPEN	66+1000
#define AFS_ICREATE	67+1000
#define AFS_IREAD	68+1000
#define AFS_IWRITE	69+1000
#define AFS_IINC	70+1000
#define AFS_IDEC	71+1000
#define AFS_IOPEN64	72+1000 /* was never-used aux call. */
#define AFS_SYSCALL	73+1000

/* For the XFS fileserver */
#define AFS_SGI_XFS_IOPS_ENV 	1 /* turns on XFS inode ops. */
#define AFS_64BIT_IOPS_ENV	1 /* inode ops expect 64 bit inodes */

/* Vnode size and access differs between Octane and Origin. The number
 * can be used to indicate which altername vnodeX_t to use for future
 * changes.
 */
#define AFS_SGI_VNODE_GLUE	1

/* File system entry (used if mount.h doesn't define MOUNT_AFS */
#define AFS_MOUNT_AFS	 "afs"

/* Machine / Operating system information */
#define sys_sgi_64	1
#define SYS_NAME	"sgi_64"
#define SYS_NAME_ID	SYS_NAME_ID_sgi_64
#define AFSBIG_ENDIAN	1
#define AFS_VM_RDWR_ENV	1

/* Extra kernel definitions (from kdefs file) */
#ifdef KERNEL
#if _MIPS_SIM == _ABI64
#define _K64U64
#endif
/* definitions here */
#define	AFS_VFS34		1	/* afs/afs_vfsops.c (afs_vget), afs/afs_vnodeops.c (afs_lockctl, afs_noop) */
#define	AFS_UIOFMODE		1	/* Only in afs/afs_vnodeops.c (afs_ustrategy) */
#define	AFS_SYSVLOCK		1	/* sys v locking supported */
/*#define	AFS_USEBUFFERS	1*/
#define	afsio_iov		uio_iov
#define	afsio_iovcnt	uio_iovcnt
#define	afsio_offset	uio_offset
#define	afsio_seg		uio_segflg
#define	afsio_fmode	uio_fmode
#define	afsio_resid	uio_resid
#define	AFS_UIOSYS	UIO_SYSSPACE
#define	AFS_UIOUSER	UIO_USERSPACE
#define	AFS_CLBYTES	MCLBYTES
#define	AFS_MINCHANGE	2
#ifdef _K64U64
#define	osi_GetTime(x)	irix5_microtime((struct __irix5_timeval*)(x))
#else
#define	osi_GetTime(x)	microtime(x)
#endif
#define	AFS_KALLOC(n)	kmem_alloc(n, KM_SLEEP)
#define	AFS_KALLOC_NOSLEEP(n)	kmem_alloc(n, KM_NOSLEEP)
#define	AFS_KFREE	kmem_free
#define	VATTR_NULL	vattr_null
#define	DEBUG		1

#define AFS_EVENT_LOCK  1 /* osi_Sleep/osi_Wakeup use spinlock. */
#define AFS_SGI_DUAL_FS_CACHE 1 /* implies supports EFS or XFS cache */

#endif /* KERNEL */

#ifndef CMSERVERPREF
#define CMSERVERPREF
#endif
#endif	/* _PARAM_SGI_64_H_ */
