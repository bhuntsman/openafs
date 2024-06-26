/* 
 *Copyright (C) 1998  Transarc Corporation.  All rights reserved.
 */

/* rx_pthread.h defines the lock and cv primitives required for a thread
 * safe user mode rx. The current implemenation is only tested on Solaris.
 */
 
#ifndef RX_PTHREAD_H
#define RX_PTHREAD_H

#ifndef RX_ENABLE_LOCKS
#define RX_ENABLE_LOCKS 1
#endif

/* This turns out to be necessary even for fine grain locking. */
#ifndef AFS_GLOBAL_RXLOCK_KERNEL
#define AFS_GLOBAL_RXLOCK_KERNEL 1
#endif

/* Block signals to child threads. */
#include <afs/pthread_nosigs.h>

#ifdef AFS_NT40_ENV
#include <wtypes.h>
#include <winbase.h>
#include <winsock2.h>
#include <pthread.h>

typedef pthread_mutex_t afs_kmutex_t;
typedef pthread_cond_t afs_kcondvar_t;
#define MUTEX_ISMINE
#define pthread_yield() Sleep(0)

#else /* AFS_NT40_ENV */

#include <pthread.h>
typedef pthread_mutex_t afs_kmutex_t;
typedef pthread_cond_t afs_kcondvar_t;

#if !defined(pthread_yield) && defined(AFS_SUN5_ENV)
#define pthread_yield() thr_yield()
#endif
#if !defined(pthread_yield) && !defined(AFS_AIX_ENV)
#define pthread_yield() sleep(0)
#endif
#if !defined(pthread_yield) && (_XOPEN_SOURCE + 0) >= 500
#define pthread_yield() sched_yield()
#endif


#ifndef MUTEX_ISMINE
/* Only used for debugging. */
#ifdef AFS_SUN5_ENV
#include <synch.h>
#define MUTEX_ISMINE(l) MUTEX_HELD(l)
#else /* AFS_SUN5_ENV */
#define MUTEX_ISMINE(l) (1)
#endif /* AFS_SUN5_ENV */
#endif /* !MUTEX_ISMINE */
#endif /* AFS_NT40_ENV */

extern void osirx_AssertMine(afs_kmutex_t *lockaddr, char *msg);

#ifdef AFS_PTHREAD_ENV
#ifdef MUTEX_INIT
#undef MUTEX_INIT
#endif
#define MUTEX_INIT(a, b, c, d) pthread_mutex_init(a, NULL)

#ifdef MUTEX_DESTROY
#undef MUTEX_DESTROY
#endif
#define MUTEX_DESTROY(l) pthread_mutex_destroy(l)

#ifdef MUTEX_ENTER
#undef MUTEX_ENTER
#endif
#define MUTEX_ENTER(l) pthread_mutex_lock(l)

#ifdef MUTEX_TRYENTER
#undef MUTEX_TRYENTER
#endif
#define MUTEX_TRYENTER(l) pthread_mutex_trylock(l) ? 0 : 1

#ifdef MUTEX_EXIT
#undef MUTEX_EXIT
#endif
#define MUTEX_EXIT(l) pthread_mutex_unlock(l)

#ifdef RXObtainWriteLock
#undef RXObtainWriteLock
#endif
#define RXObtainWriteLock(l) MUTEX_ENTER(l)

#ifdef RXReleaseWriteLock
#undef RXReleaseWriteLock
#endif
#define RXReleaseWriteLock(l) MUTEX_EXIT(l)

#ifdef CV_INIT
#undef CV_INIT
#endif
#define CV_INIT(cv, a, b, c) pthread_cond_init(cv, NULL)

#ifdef CV_DESTROY
#undef CV_DESTROY
#endif
#define CV_DESTROY(cv) pthread_cond_destroy(cv)

#ifdef CV_WAIT
#undef CV_WAIT
#endif
#define CV_WAIT(cv, l) pthread_cond_wait(cv, l)

#ifdef CV_SIGNAL
#undef CV_SIGNAL
#endif
#define CV_SIGNAL(cv) pthread_cond_signal(cv)

#ifdef CV_BROADCAST
#undef CV_BROADCAST
#endif
#define CV_BROADCAST(cv) pthread_cond_broadcast(cv)

#endif /* AFS_PTHREAD_ENV */


#endif /* RX_PTHREAD_H */
