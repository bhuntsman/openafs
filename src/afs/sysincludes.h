/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#ifndef __AFS_SYSINCLUDESH__
#define __AFS_SYSINCLUDESH__ 1

#ifdef AFS_LINUX22_ENV
#include <linux/version.h>
#include <linux/config.h>
#include <linux/linkage.h>
#include <linux/limits.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/vfs.h>
#include <linux/net.h>
#include <linux/kdev_t.h>
#include <linux/ioctl.h>
/* Avoid conflicts with coda overloading AFS type namespace. Must precede
 * inclusion of uaccess.h.
 */
#define _LINUX_CODA_FS_I
#define _CFS_HEADER_
struct coda_inode_info {};
#include <asm/uaccess.h>
#include <linux/list.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/quota.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <asm/semaphore.h>
#include <linux/errno.h>

#else /* AFS_LINUX22_ENV */
#if	!defined(AFS_OSF_ENV)
#include "../h/errno.h"
#include "../h/types.h"
#include "../h/param.h"

#ifdef	AFS_AUX_ENV
#ifdef	PAGING
#include "../h/mmu.h"
#include "../h/seg.h"
#include "../h/page.h"
#include "../h/region.h"
#endif /* PAGING */
#include "../h/sysmacros.h"
#include "../h/signal.h"
#include "../h/var.h"
#endif /* AFS_AUX_ENV */

#include "../h/systm.h"
#include "../h/time.h"

#ifdef	AFS_AIX_ENV
#ifdef AFS_AIX41_ENV
#include "sys/statfs.h"
#endif
#include "../h/file.h"
#include "../h/fullstat.h"
#include "../h/vattr.h"
#include "../h/var.h"
#include "../h/access.h"
#endif /* AFS_AIX_ENV */

#if defined(AFS_SGI_ENV)
#include "values.h"
#include "../sys/sema.h"
#include "../sys/cmn_err.h"
#ifdef AFS_SGI64_ENV
#include <ksys/behavior.h>
#endif /* AFS_SGI64_ENV */
#include "../sgiefs/efs.h"
#include "../sys/kmem.h"
#include "../sys/cred.h"
#include "../sys/resource.h"

/*
 * ../sys/debug.h defines ASSERT(), but it is nontrivial only if DEBUG
 * is on (see afs_osi.h).
 * In IRIX 6.5 we cannot have DEBUG turned on since certain
 * system-defined structures have different members with DEBUG on, and
 * this breaks our ability to interact with the rest of the kernel.
 *
 * Instead of using ASSERT(), we use our own osi_Assert().
 */
#if defined(AFS_SGI65_ENV) && !defined(DEBUG)
#define DEBUG
#include "../sys/debug.h"
#undef DEBUG
#else
#include "../sys/debug.h"
#endif

#include "../sys/statvfs.h"
#include "../sys/sysmacros.h"
#include "../sys/fs_subr.h"
#include "../sys/siginfo.h"
#endif  /* AFS_SGI_ENV */

#if !defined(AFS_AIX_ENV) && !defined(AFS_SUN5_ENV) && !defined(AFS_SGI_ENV)
#  include "../h/kernel.h"
#endif /* !SUN5 && !SGI */

#ifdef	AFS_SUN5_ENV
#include <sys/cmn_err.h>	/* for kernel printf() prototype */
#endif

#if	defined(AFS_SUN56_ENV)
#include "../h/vfs.h"		/* stops SUN56 socketvar.h warnings */
#include "../h/stropts.h"	/* stops SUN56 socketvar.h warnings */
#include "../h/stream.h"	/* stops SUN56 socketvar.h errors */
#endif

#include "../h/socket.h"
#include "../h/socketvar.h"
#include "../h/protosw.h"

#if defined(AFS_SGI_ENV) || defined(AFS_SUN_ENV) || defined(AFS_HPUX_ENV) || defined(AFS_SUN5_ENV)
#  include "../h/dirent.h"
#  ifdef	AFS_SUN5_ENV
#    include "../h/sysmacros.h"
#    include "../h/fs/ufs_fsdir.h"
#  endif /* AFS_SUN5_ENV */
#else
#  include "../h/dir.h"
#endif /* SGI || SUN || HPUX */

#ifdef AFS_DEC_ENV
#  include "../h/smp_lock.h"
#endif /* AFS_DEC_ENV */


#ifndef AFS_SGI64_ENV
#include "../h/user.h"
#endif /* AFS_SGI64_ENV */
#define	MACH_USER_API	1
#include "../h/file.h"
#include "../h/uio.h"
#include "../h/buf.h"
#include "../h/stat.h"


/* ----- The following mainly deal with vnodes/inodes stuff ------ */
#ifdef	AFS_DEC_ENV
#  include "../h/mount.h"
#  include "../machine/psl.h"
#  include "../afs/gfs_vnode.h"
#endif

#ifdef	AFS_MACH_ENV
#    include <vfs/vfs.h>
#    include <vfs/vnode.h>
#    include <sys/inode.h>
#    include <sys/mount.h>
#    include <vm/vm_pager.h>
#    include <kern/mfs.h>
#    include <mach/vm_param.h>
#    include <kern/parallel.h>
#endif /* AFS_MACH_ENV */

#ifndef AFS_DEC_ENV
#  ifdef	AFS_SUN5_ENV
#    include "../h/statvfs.h"
#  endif /* AFS_SUN5_ENV */
#  ifdef AFS_HPUX_ENV
struct vfspage;			/* for vnode.h compiler warnings */
#    include "../h/swap.h"	/* for struct swpdbd, for vnode.h compiler warnings */
#    include "../h/dbd.h"	/* for union idbd, for vnode.h compiler warnings */
#  endif /* AFS_HPUX_ENV */
#  include "../h/vfs.h"
#  include "../h/vnode.h"
#  ifdef	AFS_SUN5_ENV
#    include "../h/fs/ufs_inode.h"
#    include "../h/fs/ufs_mount.h"
#  else
#    if !defined(AFS_SGI_ENV) && !defined(AFS_AIX32_ENV)
#      include "../ufs/inode.h"
#      if !defined(AFS_SGI_ENV) && !defined(AFS_HPUX_ENV)
#        include "../ufs/mount.h"
#      endif /* !AFS_HPUX_ENV */
#    endif /* !AFS_AIX32_ENV */
#  endif /* AFS_SUN5_ENV */
#endif /* AFS_DEC_ENV */

/* These mainly deal with networking and rpc headers */
#include "../netinet/in.h"
#undef	MFREE	/* defined at mount.h for AIX */
#ifdef	AFS_SUN5_ENV
#  include "../h/time.h"
#else
#  include "../h/mbuf.h"
#endif /* AFS_SUN5_ENV */

#include "../rpc/types.h"
#include "../rpc/xdr.h"

#ifdef AFS_AIX32_ENV
#  include "net/spl.h"
#endif

/* Miscellaneous headers */
#include "../h/proc.h"
#include "../h/ioctl.h"

#if	defined(AFS_HPUX101_ENV)
#include "../h/proc_iface.h"
#include "../h/vas.h"
#endif

#if	defined(AFS_HPUX102_ENV)
#include "../h/unistd.h"
#include "../h/tty.h"
#endif

#if !defined(AFS_SGI_ENV) && !defined(AFS_SUN_ENV) && !defined(AFS_MACH_ENV) && !defined(AFS_AIX32_ENV) && !defined(AFS_HPUX_ENV) && !defined(AFS_SUN5_ENV)
#  include "../h/text.h"
#endif 


#if	defined(AFS_AIX_ENV) || defined(AFS_DEC_ENV)
#  include "../h/flock.h"	/* fcntl.h is a user-level include in aix */
#else
#  include "../h/fcntl.h"
#endif /* AIX || DEC */

#if defined(AFS_SGI_ENV) || defined(AFS_SUN_ENV) || defined(AFS_SUN5_ENV)
#  include "../h/unistd.h"
#endif /* SGI || SUN */

#ifdef	AFS_AIX32_ENV
#  include "../h/vmuser.h"
#endif /* AFS_AIX32_ENV */

#if	defined(AFS_SUN5_ENV)
#include <sys/tiuser.h>
#include <sys/t_lock.h>
#include <sys/mutex.h>
#include <sys/vtrace.h>
#include <sys/pathname.h>
#include <sys/debug.h>
#endif

#else	/* ! AFS_OSF_ENV */
/* All of the OSF/1 stuff is here */
#include <net/net_globals.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <ufs/dir.h>
#include <sys/user.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <ufs/inode.h>
#include <sys/mount.h>
#include <vm/vm_page.h>
#include <mach/vm_param.h>
#include <kern/parallel.h>
#include <mach/mach_types.h>
#ifndef	AFS_OSF30_ENV
#include <kern/mfs.h>
#endif
#include <mach/vm_param.h>
#include <kern/parallel.h>

/* These mainly deal with networking and rpc headers */
#include <netinet/in.h>
#include <sys/mbuf.h>
#include <rpc/types.h>

#ifdef	AFS_ALPHA_ENV
#undef kmem_alloc
#undef kmem_free
#undef mem_alloc
#undef mem_free
#undef register
#endif	/* AFS_ALPHA_ENV */

#include <rpc/xdr.h>
#include <sys/proc.h>
#include <sys/ioctl.h>

#endif	/* AFS_OSF_ENV */
#endif /* AFS_LINUX22_ENV */

#endif /* __AFS_SYSINCLUDESH__  so idempotent */
