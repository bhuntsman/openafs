
#ifndef lint
#endif

/* Copyright (C) 1989 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

/*******************************************************************\
* 								    *
* 	Information Technology Center				    *
* 	Carnegie-Mellon University				    *
* 								    *
* 								    *
* 								    *
\*******************************************************************/


/*
	Locking routines for Vice.

*/

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h" /* afs statistics */


#ifndef FALSE
#define FALSE	0
#endif
#ifndef TRUE
#define TRUE	1
#endif

int afs_trclock=0;

void Lock_Obtain();
void Lock_ReleaseR();
void Lock_ReleaseW();

void Lock_Init(lock)
    register struct afs_lock *lock;
{

    AFS_STATCNT(Lock_Init);
    lock -> readers_reading = 0;
    lock -> excl_locked = 0;
    lock -> wait_states = 0;
    lock -> num_waiting = 0;
#if defined(INSTRUMENT_LOCKS)
    lock->pid_last_reader = 0;
    lock->pid_writer = 0;
    lock->src_indicator = 0;
#endif /* INSTRUMENT_LOCKS */
    lock->time_waiting.tv_sec = 0;
    lock->time_waiting.tv_usec = 0;
}

void ObtainLock(lock, how, src_indicator)
    register struct afs_lock *lock;
    int how;
    unsigned int src_indicator;
{
    switch (how) {
      case READ_LOCK:		
	if (!((lock)->excl_locked & WRITE_LOCK))
	    (lock) -> readers_reading++;
	else
	    Afs_Lock_Obtain(lock, READ_LOCK);
#if defined(INSTRUMENT_LOCKS)
	(lock)->pid_last_reader = MyPidxx;
#endif /* INSTRUMENT_LOCKS */
	break;
      case WRITE_LOCK:		
	if (!(lock)->excl_locked && !(lock)->readers_reading)
	    (lock) -> excl_locked = WRITE_LOCK;
	else
	    Afs_Lock_Obtain(lock, WRITE_LOCK);
#if defined(INSTRUMENT_LOCKS)
	(lock)->pid_writer = MyPidxx;
	(lock)->src_indicator = src_indicator;
#endif /* INSTRUMENT_LOCKS */
	break;
      case SHARED_LOCK:		
	if (!(lock)->excl_locked)
	    (lock) -> excl_locked = SHARED_LOCK;
	else
	    Afs_Lock_Obtain(lock, SHARED_LOCK);
#if defined(INSTRUMENT_LOCKS)
	(lock)->pid_writer = MyPidxx;
	(lock)->src_indicator = src_indicator;
#endif /* INSTRUMENT_LOCKS */
	break;
    }	
}

void ReleaseLock(lock, how)
    register struct afs_lock *lock;
    int how;
{
    if (how == READ_LOCK) {
	if (!--lock->readers_reading && lock->wait_states)
	{
#if defined(INSTRUMENT_LOCKS)
	    if ( lock->pid_last_reader == MyPidxx )
		lock->pid_last_reader = 0;
#endif /* INSTRUMENT_LOCKS */
	    Afs_Lock_ReleaseW(lock); 
	}
    } else if (how == WRITE_LOCK) {
	lock->excl_locked &= ~WRITE_LOCK;
#if defined(INSTRUMENT_LOCKS)
	lock->pid_writer = 0;
#endif /* INSTRUMENT_LOCKS */
	if (lock->wait_states) Afs_Lock_ReleaseR(lock);
    } else if (how == SHARED_LOCK) {
	lock->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK);
#if defined(INSTRUMENT_LOCKS)
	lock->pid_writer = 0;
#endif /* INSTRUMENT_LOCKS */
	if (lock->wait_states) Afs_Lock_ReleaseR(lock);
    }
}

Afs_Lock_Obtain(lock, how)
    register struct afs_lock *lock;
    int how;
{
    osi_timeval_t tt1, tt2, et;

    AFS_STATCNT(Lock_Obtain);
  
    AFS_ASSERT_GLOCK();
    osi_GetuTime(&tt1);
    
    switch (how) {

	case READ_LOCK:		lock->num_waiting++;
				do {
				    lock -> wait_states |= READ_LOCK;
				    afs_osi_Sleep(&lock->readers_reading);
				} while (lock->excl_locked & WRITE_LOCK);
				lock->num_waiting--;
				lock->readers_reading++;
				break;

	case WRITE_LOCK:	lock->num_waiting++;
				do {
				    lock -> wait_states |= WRITE_LOCK;
				    afs_osi_Sleep(&lock->excl_locked);
				} while (lock->excl_locked || lock->readers_reading);
				lock->num_waiting--;
				lock->excl_locked = WRITE_LOCK;
				break;

	case SHARED_LOCK:	lock->num_waiting++;
				do {
				    lock->wait_states |= SHARED_LOCK;
				    afs_osi_Sleep(&lock->excl_locked);
				} while (lock->excl_locked);
				lock->num_waiting--;
				lock->excl_locked = SHARED_LOCK;
				break;

	case BOOSTED_LOCK:	lock->num_waiting++;
				do {
				    lock->wait_states |= WRITE_LOCK;
				    afs_osi_Sleep(&lock->excl_locked);
				} while (lock->readers_reading);
				lock->num_waiting--;
				lock->excl_locked = WRITE_LOCK;
				break;

	default:		osi_Panic("afs locktype");
    }

    osi_GetuTime(&tt2);
    afs_stats_GetDiff(et, tt1, tt2);
    afs_stats_AddTo((lock->time_waiting), et);

    if (afs_trclock) {
	afs_Trace2(afs_iclSetp, CM_TRACE_LOCKSLEPT, ICL_TYPE_POINTER, lock,
		   ICL_TYPE_INT32, how);
    }
}

/* release a lock, giving preference to new readers */
Afs_Lock_ReleaseR(lock)
    register struct afs_lock *lock;
{
    AFS_STATCNT(Lock_ReleaseR);
    AFS_ASSERT_GLOCK();
    if (lock->wait_states & READ_LOCK) {
	lock->wait_states &= ~READ_LOCK;
	afs_osi_Wakeup(&lock->readers_reading);
    }
    else {
	lock->wait_states &= ~EXCL_LOCKS;
	afs_osi_Wakeup(&lock->excl_locked);
    }
}

/* release a lock, giving preference to new writers */
Afs_Lock_ReleaseW(lock)
    register struct afs_lock *lock;
{
    AFS_STATCNT(Lock_ReleaseW);
    AFS_ASSERT_GLOCK();
    if (lock->wait_states & EXCL_LOCKS) {
	lock->wait_states &= ~EXCL_LOCKS;
	afs_osi_Wakeup(&lock->excl_locked);
    }
    else {
	lock->wait_states &= ~READ_LOCK;
	afs_osi_Wakeup(&lock->readers_reading);
    }
}

/*
Wait for some change in the lock status.
Lock_Wait(lock)
    register struct afs_lock *lock; {
    AFS_STATCNT(Lock_Wait);
    if (lock->readers_reading || lock->excl_locked) return 1;
    lock->wait_states |= READ_LOCK;
    afs_osi_Sleep(&lock->readers_reading);
    return 0;
}
*/

/* These next guys exist to provide an interface to drop a lock atomically with
 * blocking.  They're trivial to do in a non-preemptive LWP environment.
 */

/* release a write lock and sleep on an address, atomically */
afs_osi_SleepR(addr, alock)
register char *addr;
register struct afs_lock *alock; {
    AFS_STATCNT(osi_SleepR);
    ReleaseReadLock(alock);
    afs_osi_Sleep(addr);
}

/* release a write lock and sleep on an address, atomically */
afs_osi_SleepW(addr, alock)
register char *addr;
register struct afs_lock *alock; {
    AFS_STATCNT(osi_SleepW);
    ReleaseWriteLock(alock);
    afs_osi_Sleep(addr);
}

/* release a write lock and sleep on an address, atomically */
afs_osi_SleepS(addr, alock)
register char *addr;
register struct afs_lock *alock; {
    AFS_STATCNT(osi_SleepS);
    ReleaseSharedLock(alock);
    afs_osi_Sleep(addr);
}


#ifndef	AFS_NOBOZO_LOCK
/* operations on locks that don't mind if we lock the same thing twice.  I'd like to dedicate
    this function to Sun Microsystems' Version 4.0 virtual memory system, without
    which this wouldn't have been necessary */
void afs_BozonLock(alock, avc)
struct vcache *avc;
struct afs_bozoLock *alock; {
    AFS_STATCNT(afs_BozonLock);
    while (1) {
	if (alock->count == 0) {
	    /* lock not held, we win */
#ifdef	AFS_SUN5_ENV
	    alock->proc = (char *) ttoproc(curthread);
#else
#ifdef AFS_64BITPOINTER_ENV
	    /* To shut up SGI compiler on remark(1413) warnings. */
	    alock->proc = (char *) (long)MyPidxx;
#else /* AFS_64BITPOINTER_ENV */
	    alock->proc = (char *) MyPidxx;
#endif /* AFS_64BITPOINTER_ENV */
#endif
	    alock->count = 1;
	    return;
#ifdef	AFS_SUN5_ENV
	} else if (alock->proc == (char *) ttoproc(curthread)) {
#else
#ifdef AFS_64BITPOINTER_ENV
        /* To shut up SGI compiler on remark(1413) warnings. */
	} else if (alock->proc == (char *) (long)MyPidxx) {
#else /* AFS_64BITPOINTER_ENV */
	} else if (alock->proc == (char *) MyPidxx) {
#endif /* AFS_64BITPOINTER_ENV */
#endif
	    /* lock is held, but by us, so we win anyway */
	    alock->count++;
	    return;
	}
	else {
	    /* lock is held, and not by us; we wait */
	    alock->flags |= AFS_BOZONWAITING;
	    afs_osi_Sleep(alock);
	}
    }
}

/* releasing the same type of lock as defined above */
void afs_BozonUnlock(alock, avc)
struct vcache *avc;
struct afs_bozoLock *alock; {
    AFS_STATCNT(afs_BozonUnlock);
    if (alock->count <= 0)
	osi_Panic("BozoUnlock");
    if ((--alock->count) == 0) {
	if (alock->flags & AFS_BOZONWAITING) {
	    alock->flags &= ~AFS_BOZONWAITING;
	    afs_osi_Wakeup(alock);
	}
    }
}

void afs_BozonInit(alock, avc)
struct vcache *avc;
struct afs_bozoLock *alock; {
    AFS_STATCNT(afs_BozonInit);
    alock->count = 0;
    alock->flags = 0;
    alock->proc = (char *) 0;
}

afs_CheckBozonLock(alock)
struct afs_bozoLock *alock; {
    AFS_STATCNT(afs_CheckBozonLock);
    if (alock->count || (alock->flags & AFS_BOZONWAITING))
	return 1;
    return 0;
}

afs_CheckBozonLockBlocking(alock)
struct afs_bozoLock *alock; {
    AFS_STATCNT(afs_CheckBozonLockBlocking);
    if (alock->count || (alock->flags & AFS_BOZONWAITING))
#ifdef AFS_SUN5_ENV
	if (alock->proc != (char *) ttoproc(curthread))
#else
#ifdef AFS_64BITPOINTER_ENV
	    /* To shut up SGI compiler on remark(1413) warnings. */
	if (alock->proc != (char *) (long)MyPidxx)
#else /* AFS_64BITPOINTER_ENV */
	if (alock->proc != (char *) MyPidxx)
#endif /* AFS_64BITPOINTER_ENV */
#endif
	    return 1;
    return 0;
}
#endif
