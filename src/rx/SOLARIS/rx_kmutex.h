/* Copyright Transarc Corporation 1998 - All Rights Reserved
 *
 * rx_kmutex.h - mutex and condition variable macros for kernel environment.
 *
 * Solaris implementation.
 */

#ifndef _RX_KMUTEX_H_
#define _RX_KMUTEX_H_

#if	defined(AFS_SUN5_ENV) && defined(KERNEL) 

#define RX_ENABLE_LOCKS 1
#define AFS_GLOBAL_RXLOCK_KERNEL 1

#include <sys/tiuser.h>
#include <sys/t_lock.h>
#include <sys/mutex.h>

typedef kmutex_t afs_kmutex_t;
typedef kcondvar_t afs_kcondvar_t;

#undef osirx_AssertMine
extern void osirx_AssertMine(afs_kmutex_t *lockaddr, char *msg);

#define MUTEX_DESTROY(a)	mutex_destroy(a)
#define MUTEX_INIT(a,b,c,d)	mutex_init(a, b, c, d)
#define MUTEX_ISMINE(a)		mutex_owned((afs_kmutex_t *)(a))
#define CV_INIT(a,b,c,d)	cv_init(a, b, c, d)
#define CV_DESTROY(a)		cv_destroy(a)
#define CV_SIGNAL(a)		cv_signal(a)
#define CV_BROADCAST(a)		cv_broadcast(a)

#ifdef RX_LOCKS_DB

#define MUTEX_ENTER(a) \
    do { \
	mutex_enter(a); \
	rxdb_grablock((a), osi_ThreadUnique(), rxdb_fileID, __LINE__); \
    } while(0)

#define MUTEX_TRYENTER(a) \
    (mutex_tryenter(a) ? (rxdb_grablock((a), osi_ThreadUnique(), \
     rxdb_fileID, __LINE__), 1) : 0)

#define MUTEX_EXIT(a) \
    do { \
	rxdb_droplock((a), osi_ThreadUnique(), rxdb_fileID, __LINE__); \
	mutex_exit(a); \
    } while(0)

#define CV_WAIT(_cv, _lck) \
    do { \
	int haveGlock = ISAFS_GLOCK(); \
	if (haveGlock) \
	    AFS_GUNLOCK(); \
	rxdb_droplock((_lck), osi_ThreadUnique(), rxdb_fileID, __LINE__); \
	cv_wait(_cv, _lck); \
	rxdb_grablock((_lck), osi_ThreadUnique(), rxdb_fileID, __LINE__); \
	if (haveGlock) { \
	    MUTEX_EXIT(_lck); \
	    AFS_GLOCK(); \
	    MUTEX_ENTER(_lck); \
	} \
    } while (0)

#define CV_TIMEDWAIT(_cv,_lck,_t) \
    do { \
	int haveGlock = ISAFS_GLOCK(); \
	if (haveGlock) \
	    AFS_GUNLOCK(); \
	rxdb_droplock((_lck), osi_ThreadUnique(), rxdb_fileID, __LINE__); \
	cv_timedwait(_cv, _lck, t); \
	rxdb_grablock((_lck), osi_ThreadUnique(), rxdb_fileID, __LINE__); \
	if (haveGlock) { \
	    MUTEX_EXIT(_lck); \
	    AFS_GLOCK(); \
	    MUTEX_ENTER(_lck); \
	} \
     } while (0)

#else /* RX_LOCKS_DB */

#define MUTEX_ENTER(a)		mutex_enter(a)
#define MUTEX_TRYENTER(a)	mutex_tryenter(a)
#define MUTEX_EXIT(a) 		mutex_exit(a)

#define CV_WAIT(_cv, _lck) \
    do { \
	int haveGlock = ISAFS_GLOCK(); \
	if (haveGlock) \
	    AFS_GUNLOCK(); \
	cv_wait(_cv, _lck); \
	if (haveGlock) { \
	    MUTEX_EXIT(_lck); \
	    AFS_GLOCK(); \
	    MUTEX_ENTER(_lck); \
	} \
    } while (0)

#define CV_TIMEDWAIT(_cv,_lck,_t) \
    do { \
	int haveGlock = ISAFS_GLOCK(); \
	if (haveGlock) \
	    AFS_GUNLOCK(); \
	cv_timedwait(_cv, _lck, t); \
	if (haveGlock) { \
	    MUTEX_EXIT(_lck); \
	    AFS_GLOCK(); \
	    MUTEX_ENTER(_lck); \
	} \
     } while (0)

#endif /* RX_LOCKS_DB */

#endif	/* SUN5 && KERNEL */

#endif /* _RX_KMUTEX_H_ */

