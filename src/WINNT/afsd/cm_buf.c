/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 *
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <osi.h>
#include <malloc.h>
#include <stdio.h>
#include <assert.h>

#include "afsd.h"

void afsi_log();

/* This module implements the buffer package used by the local transaction
 * system (cm).  It is initialized by calling cm_Init, which calls buf_Init;
 * it must be initalized before any of its main routines are called.
 *
 * Each buffer is hashed into a hash table by file ID and offset, and if its
 * reference count is zero, it is also in a free list.
 *
 * There are two locks involved in buffer processing.  The global lock
 * buf_globalLock protects all of the global variables defined in this module,
 * the reference counts and hash pointers in the actual cm_buf_t structures,
 * and the LRU queue pointers in the buffer structures.
 *
 * The mutexes in the buffer structures protect the remaining fields in the
 * buffers, as well the data itself.
 * 
 * The locking hierarchy here is this:
 * 
 * - resv multiple simul. buffers reservation
 * - lock buffer I/O flags
 * - lock buffer's mutex
 * - lock buf_globalLock
 *
 */

/* global debugging log */
osi_log_t *buf_logp = NULL;

/* Global lock protecting hash tables and free lists */
osi_rwlock_t buf_globalLock;

/* ptr to head of the free list (most recently used) and the
 * tail (the guy to remove first).  We use osi_Q* functions
 * to put stuff in buf_freeListp, and maintain the end
 * pointer manually
 */
cm_buf_t *buf_freeListp;
cm_buf_t *buf_freeListEndp;

/* a pointer to a list of all buffers, just so that we can find them
 * easily for debugging, and for the incr syncer.  Locked under
 * the global lock.
 */
cm_buf_t *buf_allp;

/* defaults setup; these variables may be manually assigned into
 * before calling cm_Init, as a way of changing these defaults.
 */
long buf_nbuffers = CM_BUF_BUFFERS;
long buf_nOrigBuffers;
long buf_bufferSize = CM_BUF_SIZE;
long buf_hashSize = CM_BUF_HASHSIZE;

static
HANDLE CacheHandle;

static
SYSTEM_INFO sysInfo;

/* buffer reservation variables */
long buf_reservedBufs;
long buf_maxReservedBufs;
int buf_reserveWaiting;

/* callouts for reading and writing data, etc */
cm_buf_ops_t *cm_buf_opsp;

/* pointer to hash table; size computed dynamically */
cm_buf_t **buf_hashTablepp;

/* another hash table */
cm_buf_t **buf_fileHashTablepp;

/* hold a reference to an already held buffer */
void buf_Hold(cm_buf_t *bp)
{
	lock_ObtainWrite(&buf_globalLock);
	bp->refCount++;
	lock_ReleaseWrite(&buf_globalLock);
}

/* incremental sync daemon.  Writes 1/10th of all the buffers every 5000 ms */
void buf_IncrSyncer(long parm)
{
	cm_buf_t *bp;			/* buffer we're hacking on; held */
        long i;				/* counter */
        long nAtOnce;			/* how many to do at once */
	cm_req_t req;

	lock_ObtainWrite(&buf_globalLock);
	bp = buf_allp;
        bp->refCount++;
        lock_ReleaseWrite(&buf_globalLock);
        nAtOnce = buf_nbuffers / 10;
	while (1) {
		i = SleepEx(5000, 1);
                if (i != 0) continue;
                
                /* now go through our percentage of the buffers */
                for(i=0; i<nAtOnce; i++) {
			/* don't want its identity changing while we're
                         * messing with it, so must do all of this with
                         * bp held.
			 */

			/* start cleaning the buffer; don't touch log pages since
                         * the log code counts on knowing exactly who is writing
                         * a log page at any given instant.
                         */
			cm_InitReq(&req);
			req.flags |= CM_REQ_NORETRY;
			buf_CleanAsync(bp, &req);

			/* now advance to the next buffer; the allp chain never changes,
                         * and so can be followed even when holding no locks.
                         */
			lock_ObtainWrite(&buf_globalLock);
			buf_LockedRelease(bp);
                        bp = bp->allp;
                        if (!bp) bp = buf_allp;
                        bp->refCount++;
			lock_ReleaseWrite(&buf_globalLock);
                }	/* for loop over a bunch of buffers */
        }		/* whole daemon's while loop */
}

/* Create a security attribute structure suitable for use when the cache file
 * is created.  What we mainly want is that only the administrator should be
 * able to do anything with the file.  We create an ACL with only one entry,
 * an entry that grants all rights to the administrator.
 */
PSECURITY_ATTRIBUTES CreateCacheFileSA()
{
	PSECURITY_ATTRIBUTES psa;
	PSECURITY_DESCRIPTOR psd;
	SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
	PSID AdminSID;
	DWORD AdminSIDlength;
	PACL AdminOnlyACL;
	DWORD ACLlength;

	/* Get Administrator SID */
	AllocateAndInitializeSid(&authority, 2,
				 SECURITY_BUILTIN_DOMAIN_RID,
				 DOMAIN_ALIAS_RID_ADMINS,
				 0, 0, 0, 0, 0, 0,
				 &AdminSID);

	/* Create Administrator-only ACL */
	AdminSIDlength = GetLengthSid(AdminSID);
	ACLlength = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE)
			+ AdminSIDlength - sizeof(DWORD);
	AdminOnlyACL = GlobalAlloc(GMEM_FIXED, ACLlength);
	InitializeAcl(AdminOnlyACL, ACLlength, ACL_REVISION);
	AddAccessAllowedAce(AdminOnlyACL, ACL_REVISION,
			    STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL,
			    AdminSID);

	/* Create security descriptor */
	psd = GlobalAlloc(GMEM_FIXED, sizeof(SECURITY_DESCRIPTOR));
	InitializeSecurityDescriptor(psd, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(psd, TRUE, AdminOnlyACL, FALSE);

	/* Create security attributes structure */
	psa = GlobalAlloc(GMEM_FIXED, sizeof(SECURITY_ATTRIBUTES));
	psa->nLength = sizeof(SECURITY_ATTRIBUTES);
	psa->lpSecurityDescriptor = psd;
	psa->bInheritHandle = TRUE;

	return psa;
}

/* Free a security attribute structure created by CreateCacheFileSA() */
VOID FreeCacheFileSA(PSECURITY_ATTRIBUTES psa)
{
	BOOL b1, b2;
	PACL pAcl;

	GetSecurityDescriptorDacl(psa->lpSecurityDescriptor, &b1, &pAcl, &b2);
	GlobalFree(pAcl);
	GlobalFree(psa->lpSecurityDescriptor);
	GlobalFree(psa);
}
	
/* initialize the buffer package; called with no locks
 * held during the initialization phase.
 */
long buf_Init(cm_buf_ops_t *opsp)
{
	static osi_once_t once;
        cm_buf_t *bp;
        long sectorSize;
        HANDLE phandle;
	long i;
        unsigned long pid;
	HANDLE hf, hm;
	char *data;
	PSECURITY_ATTRIBUTES psa;
	long cs;

	/* Get system info; all we really want is the allocation granularity */ 
	GetSystemInfo(&sysInfo);

	/* Have to be able to reserve a whole chunk */
	if (((buf_nbuffers - 3) * buf_bufferSize) < cm_chunkSize)
		return CM_ERROR_TOOFEWBUFS;

	/* recall for callouts */
	cm_buf_opsp = opsp;

        if (osi_Once(&once)) {
		/* initialize global locks */
		lock_InitializeRWLock(&buf_globalLock, "Global buffer lock");

		/*
		 * Cache file mapping constrained by
		 * system allocation granularity;
		 * round up, assuming granularity is a power of two
		 */
		cs = buf_nbuffers * buf_bufferSize;
		cs = (cs + (sysInfo.dwAllocationGranularity - 1))
			& ~(sysInfo.dwAllocationGranularity - 1);
		if (cs != buf_nbuffers * buf_bufferSize) {
			buf_nbuffers = cs / buf_bufferSize;
			afsi_log("Cache size rounded up to %d buffers",
				 buf_nbuffers);
		}

		/* remember this for those who want to reset it */
	        buf_nOrigBuffers = buf_nbuffers;

		/* lower hash size to a prime number */
                buf_hashSize = osi_PrimeLessThan(buf_hashSize);

		/* create hash table */
                buf_hashTablepp = malloc(buf_hashSize * sizeof(cm_buf_t *));
                memset((void *)buf_hashTablepp, 0,
			buf_hashSize * sizeof(cm_buf_t *));

		/* another hash table */
                buf_fileHashTablepp = malloc(buf_hashSize * sizeof(cm_buf_t *));
                memset((void *)buf_fileHashTablepp, 0,
			buf_hashSize * sizeof(cm_buf_t *));
                
		/* min value for which this works */
		sectorSize = 1;

		/* Reserve buffer space by mapping cache file */
		psa = CreateCacheFileSA();
		hf = CreateFile(cm_CachePath,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			psa,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		if (hf == INVALID_HANDLE_VALUE) {
			afsi_log("create file error %d", GetLastError());
			return CM_ERROR_INVAL;
		}
		FreeCacheFileSA(psa);
		CacheHandle = hf;
		hm = CreateFileMapping(hf,
			NULL,
			PAGE_READWRITE,
			0, buf_nbuffers * buf_bufferSize,
			NULL);
		if (hm == NULL) {
			if (GetLastError() == ERROR_DISK_FULL) {
				afsi_log("Error creating cache file mapping: disk full");
				return CM_ERROR_TOOMANYBUFS;
			}
			return CM_ERROR_INVAL;
		}
		data = MapViewOfFile(hm,
			FILE_MAP_ALL_ACCESS,
			0, 0,
			buf_nbuffers * buf_bufferSize);
		if (data == NULL) {
			CloseHandle(hf);
			CloseHandle(hm);
			return CM_ERROR_INVAL;
		}
		CloseHandle(hm);

                /* create buffer headers and put in free list */
		bp = malloc(buf_nbuffers * sizeof(cm_buf_t));
                buf_allp = NULL;
                for(i=0; i<buf_nbuffers; i++) {
			/* allocate and zero some storage */
                        memset(bp, 0, sizeof(cm_buf_t));

			/* thread on list of all buffers */
                        bp->allp = buf_allp;
                        buf_allp = bp;
                        
                        osi_QAdd((osi_queue_t **)&buf_freeListp, &bp->q);
                        bp->flags |= CM_BUF_INLRU;
                        lock_InitializeMutex(&bp->mx, "Buffer mutex");

			/* grab appropriate number of bytes from aligned zone */
                        bp->datap = data;

			/* setup last buffer pointer */
	                if (i == 0)
                        	buf_freeListEndp = bp;

			/* next */
			bp++;
			data += buf_bufferSize;
                }
                
		/* none reserved at first */
                buf_reservedBufs = 0;
                
                /* just for safety's sake */
                buf_maxReservedBufs = buf_nbuffers - 3;
                
                /* init the buffer trace log */
                buf_logp = osi_LogCreate("buffer", 10);

		osi_EndOnce(&once);
                
                /* and create the incr-syncer */
                phandle = CreateThread((SECURITY_ATTRIBUTES *) 0, 0,
	                (LPTHREAD_START_ROUTINE) buf_IncrSyncer, 0, 0, &pid);
		osi_assertx(phandle != NULL, "buf: can't create incremental sync proc");
		CloseHandle(phandle);
        }

	return 0;
}

/* add nbuffers to the buffer pool, if possible.
 * Called with no locks held.
 */
long buf_AddBuffers(long nbuffers)
{
	cm_buf_t *bp;
        int i;
	char *data;
	HANDLE hm;
	long cs;

	/*
	 * Cache file mapping constrained by
	 * system allocation granularity;
	 * round up, assuming granularity is a power of two;
	 * assume existing cache size is already rounded
	 */
	cs = nbuffers * buf_bufferSize;
	cs = (cs + (sysInfo.dwAllocationGranularity - 1))
		& ~(sysInfo.dwAllocationGranularity - 1);
	if (cs != nbuffers * buf_bufferSize) {
		nbuffers = cs / buf_bufferSize;
	}

	/* Reserve additional buffer space by remapping cache file */
	hm = CreateFileMapping(CacheHandle,
		NULL,
		PAGE_READWRITE,
		0, (buf_nbuffers + nbuffers) * buf_bufferSize,
		NULL);
	if (hm == NULL) {
		if (GetLastError() == ERROR_DISK_FULL)
			return CM_ERROR_TOOMANYBUFS;
		else
			return CM_ERROR_INVAL;
	}
	data = MapViewOfFile(hm,
		FILE_MAP_ALL_ACCESS,
		0, buf_nbuffers * buf_bufferSize,
		nbuffers * buf_bufferSize);
	if (data == NULL) {
		CloseHandle(hm);
		return CM_ERROR_INVAL;
	}
	CloseHandle(hm);

	/* Create buffer headers and put in free list */
        bp = malloc(nbuffers * sizeof(*bp));

	for(i=0; i<nbuffers; i++) {
	        memset(bp, 0, sizeof(*bp));
        
	        lock_InitializeMutex(&bp->mx, "cm_buf_t");

		/* grab appropriate number of bytes from aligned zone */
		bp->datap = data;

                bp->flags |= CM_BUF_INLRU;
                
                lock_ObtainWrite(&buf_globalLock);
		/* note that buf_allp chain is covered by buf_globalLock now */
                bp->allp = buf_allp;
                buf_allp = bp;
                osi_QAdd((osi_queue_t **) &buf_freeListp, &bp->q);
                if (!buf_freeListEndp) buf_freeListEndp = bp;
                buf_nbuffers++;
                lock_ReleaseWrite(&buf_globalLock);

		bp++;
		data += buf_bufferSize;
	
        }	 /* for loop over all buffers */

        return 0;
}

/* interface to set the number of buffers to an exact figure.
 * Called with no locks held.
 */
long buf_SetNBuffers(long nbuffers)
{
	if (nbuffers < 10) return CM_ERROR_INVAL;
        if (nbuffers == buf_nbuffers) return 0;
        else if (nbuffers > buf_nbuffers)
		return buf_AddBuffers(nbuffers - buf_nbuffers);
        else return CM_ERROR_INVAL;
}

/* release a buffer.  Buffer must be referenced, but unlocked. */
void buf_Release(cm_buf_t *bp)
{
	lock_ObtainWrite(&buf_globalLock);
	buf_LockedRelease(bp);
	lock_ReleaseWrite(&buf_globalLock);
}

/* wait for reading or writing to clear; called with write-locked
 * buffer, and returns with locked buffer.
 */
void buf_WaitIO(cm_buf_t *bp)
{
	while (1) {
		/* if no IO is happening, we're done */
		if (!(bp->flags & (CM_BUF_READING | CM_BUF_WRITING)))
			break;
		
                /* otherwise I/O is happening, but some other thread is waiting for
                 * the I/O already.  Wait for that guy to figure out what happened,
                 * and then check again.
                 */
                bp->flags |= CM_BUF_WAITING;
                osi_SleepM((long) bp, &bp->mx);
                lock_ObtainMutex(&bp->mx);
		osi_Log1(buf_logp, "buf_WaitIO conflict wait done for 0x%x", bp);
        }
        
        /* if we get here, the IO is done, but we may have to wakeup people waiting for
         * the I/O to complete.  Do so.
         */
        if (bp->flags & CM_BUF_WAITING) {
		bp->flags &= ~CM_BUF_WAITING;
                osi_Wakeup((long) bp);
        }
        osi_Log1(buf_logp, "WaitIO finished wait for bp 0x%x", (long) bp);
}

/* code to drop reference count while holding buf_globalLock */
void buf_LockedRelease(cm_buf_t *bp)
{
	/* ensure that we're in the LRU queue if our ref count is 0 */
	osi_assert(bp->refCount > 0);
	if (--bp->refCount == 0) {
		if (!(bp->flags & CM_BUF_INLRU)) {
			osi_QAdd((osi_queue_t **) &buf_freeListp, &bp->q);

      			/* watch for transition from empty to one element */
                        if (!buf_freeListEndp)
                        	buf_freeListEndp = buf_freeListp;
			bp->flags |= CM_BUF_INLRU;
                }
        }
}

/* find a buffer, if any, for a particular file ID and offset.  Assumes
 * that buf_globalLock is write locked when called.
 */
cm_buf_t *buf_LockedFind(struct cm_scache *scp, osi_hyper_t *offsetp)
{
	long i;
        cm_buf_t *bp;
        
        i = BUF_HASH(&scp->fid, offsetp);
        for(bp = buf_hashTablepp[i]; bp; bp=bp->hashp) {
		if (cm_FidCmp(&scp->fid, &bp->fid) == 0
			&& offsetp->LowPart == bp->offset.LowPart
                	&& offsetp->HighPart == bp->offset.HighPart) {
			bp->refCount++;
			break;
                }
        }
        
	/* return whatever we found, if anything */
        return bp;
}

/* find a buffer with offset *offsetp for vnode *scp.  Called
 * with no locks held.
 */
cm_buf_t *buf_Find(struct cm_scache *scp, osi_hyper_t *offsetp)
{
	cm_buf_t *bp;

	lock_ObtainWrite(&buf_globalLock);
	bp = buf_LockedFind(scp, offsetp);
	lock_ReleaseWrite(&buf_globalLock);

	return bp;
}

/* start cleaning I/O on this buffer.  Buffer must be write locked, and is returned
 * write-locked.
 *
 * Makes sure that there's only one person writing this block
 * at any given time, and also ensures that the log is forced sufficiently far,
 * if this buffer contains logged data.
 */
void buf_LockedCleanAsync(cm_buf_t *bp, cm_req_t *reqp)
{
	long code;

	code = 0;
	while ((bp->flags & (CM_BUF_WRITING | CM_BUF_DIRTY)) == CM_BUF_DIRTY) {
        	lock_ReleaseMutex(&bp->mx);

	        code = (*cm_buf_opsp->Writep)(&bp->fid, &bp->offset,
						buf_bufferSize, 0, bp->userp,
						reqp);
                
                lock_ObtainMutex(&bp->mx);
                if (code) break;
	};

        /* do logging after call to GetLastError, or else */
	osi_Log2(buf_logp, "buf_CleanAsync starts I/O on 0x%x, done=%d", bp, code);
        
	/* if someone was waiting for the I/O that just completed or failed,
         * wake them up.
         */
        if (bp->flags & CM_BUF_WAITING) {
		/* turn off flags and wakeup users */
                bp->flags &= ~CM_BUF_WAITING;
                osi_Wakeup((long) bp);
        }
}

/* Called with a zero-ref count buffer and with the buf_globalLock write locked.
 * recycles the buffer, and leaves it ready for reuse with a ref count of 1.
 * The buffer must already be clean, and no I/O should be happening to it.
 */
void buf_Recycle(cm_buf_t *bp)
{
	int i;
        cm_buf_t **lbpp;
        cm_buf_t *tbp;
	cm_buf_t *prevBp, *nextBp;

	/* if we get here, we know that the buffer still has a 0 ref count,
	 * and that it is clean and has no currently pending I/O.  This is
	 * the dude to return.
	 * Remember that as long as the ref count is 0, we know that we won't
	 * have any lock conflicts, so we can grab the buffer lock out of
	 * order in the locking hierarchy.
	 */
	osi_Log2(buf_logp,
		"buf_Recycle recycles 0x%x, off 0x%x",
		bp, bp->offset.LowPart);

	osi_assert(bp->refCount == 0);
	osi_assert(!(bp->flags & (CM_BUF_READING | CM_BUF_WRITING | CM_BUF_DIRTY)));
	lock_AssertWrite(&buf_globalLock);

	if (bp->flags & CM_BUF_INHASH) {
		/* Remove from hash */

		i = BUF_HASH(&bp->fid, &bp->offset);
		lbpp = &(buf_hashTablepp[i]);
		for(tbp = *lbpp; tbp; lbpp = &tbp->hashp, tbp = *lbpp) {
			if (tbp == bp) break;
		}

		/* we better find it */
		osi_assertx(tbp != NULL, "buf_GetNewLocked: hash table screwup");

		*lbpp = bp->hashp;	/* hash out */

		/* Remove from file hash */

		i = BUF_FILEHASH(&bp->fid);
		prevBp = bp->fileHashBackp;
		nextBp = bp->fileHashp;
		if (prevBp)
			prevBp->fileHashp = nextBp;
		else
			buf_fileHashTablepp[i] = nextBp;
		if (nextBp)
			nextBp->fileHashBackp = prevBp;

		bp->flags &= ~CM_BUF_INHASH;
	}
                        
	/* bump the soft reference counter now, to invalidate softRefs; no
	 * wakeup is required since people don't sleep waiting for this
	 * counter to change.
	 */
	bp->idCounter++;

	/* make the fid unrecognizable */
        memset(&bp->fid, 0, sizeof(bp->fid));
}

/* recycle a buffer, removing it from the free list, hashing in its new identity
 * and returning it write-locked so that no one can use it.  Called without
 * any locks held, and can return an error if it loses the race condition and 
 * finds that someone else created the desired buffer.
 *
 * If success is returned, the buffer is returned write-locked.
 *
 * May be called with null scp and offsetp, if we're just trying to reclaim some
 * space from the buffer pool.  In that case, the buffer will be returned
 * without being hashed into the hash table.
 */
long buf_GetNewLocked(struct cm_scache *scp, osi_hyper_t *offsetp, cm_buf_t **bufpp)
{
	cm_buf_t *bp;		/* buffer we're dealing with */
	cm_buf_t *nextBp;	/* next buffer in file hash chain */
        long i;			/* temp */
	cm_req_t req;

	cm_InitReq(&req);	/* just in case */

	while(1) {
retry:
		lock_ObtainWrite(&buf_globalLock);
		/* check to see if we lost the race */
		if (scp) {
			if (bp = buf_LockedFind(scp, offsetp)) {
				bp->refCount--;
				lock_ReleaseWrite(&buf_globalLock);
	                	return CM_BUF_EXISTS;
	                }
		}
                
		/* for debugging, assert free list isn't empty, although we
		 * really should try waiting for a running tranasction to finish
		 * instead of this; or better, we should have a transaction
		 * throttler prevent us from entering this situation.
                 */
                osi_assertx(buf_freeListEndp != NULL, "buf_GetNewLocked: no free buffers");

		/* look at all buffers in free list, some of which may temp.
		 * have high refcounts and which then should be skipped,
		 * starting cleaning I/O for those which are dirty.  If we find
		 * a clean buffer, we rehash it, lock it and return it.
                 */
                for(bp = buf_freeListEndp; bp; bp=(cm_buf_t *) osi_QPrev(&bp->q)) {
			/* check to see if it really has zero ref count.  This
			 * code can bump refcounts, at least, so it may not be
			 * zero.
                         */
                        if (bp->refCount > 0) continue;
                        
			/* we don't have to lock buffer itself, since the ref
			 * count is 0 and we know it will stay zero as long as
			 * we hold the global lock.
                         */

			/* don't recycle someone in our own chunk */
			if (!cm_FidCmp(&bp->fid, &scp->fid)
			    && (bp->offset.LowPart & (-cm_chunkSize))
				  == (offsetp->LowPart & (-cm_chunkSize)))
				continue;

			/* if this page is being filled (!) or cleaned, see if
			 * the I/O has completed.  If not, skip it, otherwise
			 * do the final processing for the I/O.
                         */
                        if (bp->flags & (CM_BUF_READING | CM_BUF_WRITING)) {
				/* probably shouldn't do this much work while
				 * holding the big lock?  Watch for contention
				 * here.
                                 */
                                continue;
                        }
                        
                        if (bp->flags & CM_BUF_DIRTY) {
				/* if the buffer is dirty, start cleaning it and
				 * move on to the next buffer.  We do this with
				 * just the lock required to minimize contention
				 * on the big lock.
                                 */
				bp->refCount++;
                                lock_ReleaseWrite(&buf_globalLock);

				/* grab required lock and clean; this only
				 * starts the I/O.  By the time we're back,
				 * it'll still be marked dirty, but it will also
				 * have the WRITING flag set, so we won't get
				 * back here.
				 */
                               	buf_CleanAsync(bp, &req);
                                
                                /* now put it back and go around again */
				buf_Release(bp);
                                goto retry;
                        }
                        
                        /* if we get here, we know that the buffer still has a 0
			 * ref count, and that it is clean and has no currently
			 * pending I/O.  This is the dude to return.
                         * Remember that as long as the ref count is 0, we know
			 * that we won't have any lock conflicts, so we can grab
			 * the buffer lock out of order in the locking hierarchy.
                         */
                        buf_Recycle(bp);

			/* clean up junk flags */
			bp->flags &= ~(CM_BUF_EOF | CM_BUF_ERROR);
			bp->dataVersion = -1;	/* unknown so far */

			/* now hash in as our new buffer, and give it the
			 * appropriate label, if requested.
                         */
			if (scp) {
				bp->flags |= CM_BUF_INHASH;
				bp->fid = scp->fid;
				bp->offset = *offsetp;
				i = BUF_HASH(&scp->fid, offsetp);
				bp->hashp = buf_hashTablepp[i];
				buf_hashTablepp[i] = bp;
				i = BUF_FILEHASH(&scp->fid);
				nextBp = buf_fileHashTablepp[i];
				bp->fileHashp = nextBp;
				bp->fileHashBackp = NULL;
				if (nextBp)
					nextBp->fileHashBackp = bp;
				buf_fileHashTablepp[i] = bp;
			}
                        
			/* prepare to return it.  Start by giving it a good
			 * refcount */
			bp->refCount = 1;
                        
			/* and since it has a non-zero ref count, we should move
			 * it from the lru queue.  It better be still there,
			 * since we've held the global (big) lock since we found
			 * it there.
			 */
			osi_assertx(bp->flags & CM_BUF_INLRU,
				    "buf_GetNewLocked: LRU screwup");
			if (buf_freeListEndp == bp) {
				/* we're the last guy in this queue, so maintain it */
				buf_freeListEndp = (cm_buf_t *) osi_QPrev(&bp->q);
			}
			osi_QRemove((osi_queue_t **) &buf_freeListp, &bp->q);
			bp->flags &= ~CM_BUF_INLRU;
                        
			/* finally, grab the mutex so that people don't use it
			 * before the caller fills it with data.  Again, no one	
			 * should have been able to get to this dude to lock it.
			 */
			osi_assertx(lock_TryMutex(&bp->mx),
				    "buf_GetNewLocked: TryMutex failed");

                        lock_ReleaseWrite(&buf_globalLock);
                        *bufpp = bp;
                        return 0;
                } /* for all buffers in lru queue */
		lock_ReleaseWrite(&buf_globalLock);
        }	/* while loop over everything */
        /* not reached */
} /* the proc */

/* get a page, returning it held but unlocked.  Doesn't fill in the page
 * with I/O, since we're going to write the whole thing new.
 */
long buf_GetNew(struct cm_scache *scp, osi_hyper_t *offsetp, cm_buf_t **bufpp)
{
	cm_buf_t *bp;
        long code;
        osi_hyper_t pageOffset;
        int created;

	created = 0;
        pageOffset.HighPart = offsetp->HighPart;
        pageOffset.LowPart = offsetp->LowPart & ~(buf_bufferSize-1);
	while (1) {
		lock_ObtainWrite(&buf_globalLock);
		bp = buf_LockedFind(scp, &pageOffset);
		lock_ReleaseWrite(&buf_globalLock);
                if (bp) {
			/* lock it and break out */
                	lock_ObtainMutex(&bp->mx);
                        break;
                }
                
                /* otherwise, we have to create a page */
                code = buf_GetNewLocked(scp, &pageOffset, &bp);

		/* check if the buffer was created in a race condition branch.
		 * If so, go around so we can hold a reference to it. 
                 */
		if (code == CM_BUF_EXISTS) continue;
                
		/* something else went wrong */
                if (code != 0) return code;
                
                /* otherwise, we have a locked buffer that we just created */
                created = 1;
                break;
        } /* big while loop */
        
	/* wait for reads */
	if (bp->flags & CM_BUF_READING)
        	buf_WaitIO(bp);

        /* once it has been read once, we can unlock it and return it, still
	 * with its refcount held.
         */
        lock_ReleaseMutex(&bp->mx);
        *bufpp = bp;
        osi_Log3(buf_logp, "buf_GetNew returning bp 0x%x for file 0x%x, offset 0x%x",
        	bp, (long) scp, offsetp->LowPart);
        return 0;
}

/* get a page, returning it held but unlocked.  Make sure it is complete */
long buf_Get(struct cm_scache *scp, osi_hyper_t *offsetp, cm_buf_t **bufpp)
{
	cm_buf_t *bp;
        long code;
        osi_hyper_t pageOffset;
        unsigned long tcount;
        int created;

	created = 0;
        pageOffset.HighPart = offsetp->HighPart;
        pageOffset.LowPart = offsetp->LowPart & ~(buf_bufferSize-1);
	while (1) {
		lock_ObtainWrite(&buf_globalLock);
		bp = buf_LockedFind(scp, &pageOffset);
		lock_ReleaseWrite(&buf_globalLock);
                if (bp) {
			/* lock it and break out */
                	lock_ObtainMutex(&bp->mx);
                        break;
                }
                
                /* otherwise, we have to create a page */
                code = buf_GetNewLocked(scp, &pageOffset, &bp);

		/* check if the buffer was created in a race condition branch.
		 * If so, go around so we can hold a reference to it. 
                 */
		if (code == CM_BUF_EXISTS) continue;
                
		/* something else went wrong */
                if (code != 0) return code;
                
                /* otherwise, we have a locked buffer that we just created */
                created = 1;
                break;
        } /* big while loop */
        
        /* if we get here, we have a locked buffer that may have just been
	 * created, in which case it needs to be filled with data.
         */
        if (created) {
		/* load the page; freshly created pages should be idle */
		osi_assert(!(bp->flags & (CM_BUF_READING | CM_BUF_WRITING)));

		/* setup offset, event */
	        bp->over.Offset = bp->offset.LowPart;
	        bp->over.OffsetHigh = bp->offset.HighPart;

		/* start the I/O; may drop lock */
                bp->flags |= CM_BUF_READING;
               	code = (*cm_buf_opsp->Readp)(bp, buf_bufferSize, &tcount, NULL);
		if (code != 0) {
			/* failure or queued */
                        if (code != ERROR_IO_PENDING) {
				bp->error = code;
                                bp->flags |= CM_BUF_ERROR;
                                bp->flags &= ~CM_BUF_READING;
                                if (bp->flags & CM_BUF_WAITING) {
					bp->flags &= ~CM_BUF_WAITING;
                                        osi_Wakeup((long) bp);
                                }
				lock_ReleaseMutex(&bp->mx);
                                buf_Release(bp);
                                return code;
                        }
                } else {
	                /* otherwise, I/O completed instantly and we're done, except
                         * for padding the xfr out with 0s and checking for EOF
                         */
			if (tcount < (unsigned long) buf_bufferSize) {
				memset(bp->datap+tcount, 0, buf_bufferSize - tcount);
                                if (tcount == 0)
                                	bp->flags |= CM_BUF_EOF;
                        }
			bp->flags &= ~CM_BUF_READING;
			if (bp->flags & CM_BUF_WAITING) {
				bp->flags &= ~CM_BUF_WAITING;
                                osi_Wakeup((long) bp);
                        }
                }
                        
        } /* if created */
        
	/* wait for reads, either that which we started above, or that someone
	 * else started.  We don't care if we return a buffer being cleaned.
         */
	if (bp->flags & CM_BUF_READING)
        	buf_WaitIO(bp);

        /* once it has been read once, we can unlock it and return it, still
	 * with its refcount held.
         */
        lock_ReleaseMutex(&bp->mx);
        *bufpp = bp;

	/* now remove from queue; will be put in at the head (farthest from
	 * being recycled) when we're done in buf_Release.
         */
        lock_ObtainWrite(&buf_globalLock);
	if (bp->flags & CM_BUF_INLRU) {
		if (buf_freeListEndp == bp)
                	buf_freeListEndp = (cm_buf_t *) osi_QPrev(&bp->q);
		osi_QRemove((osi_queue_t **) &buf_freeListp, &bp->q);
                bp->flags &= ~CM_BUF_INLRU;
        }
        lock_ReleaseWrite(&buf_globalLock);

        osi_Log3(buf_logp, "buf_Get returning bp 0x%x for file 0x%x, offset 0x%x",
        	bp, (long) scp, offsetp->LowPart);
        return 0;
}

/* count # of elements in the free list;
 * we don't bother doing the proper locking for accessing dataVersion or flags
 * since it is a pain, and this is really just an advisory call.  If you need
 * to do better at some point, rewrite this function.
 */
long buf_CountFreeList(void)
{
	long count;
        cm_buf_t *bufp;

	count = 0;
	lock_ObtainRead(&buf_globalLock);
	for(bufp = buf_freeListp; bufp; bufp = (cm_buf_t *) osi_QNext(&bufp->q)) {
		/* if the buffer doesn't have an identity, or if the buffer
                 * has been invalidate (by having its DV stomped upon), then
                 * count it as free, since it isn't really being utilized.
                 */
		if (!(bufp->flags & CM_BUF_INHASH) || bufp->dataVersion <= 0)
                	count++;
        }
	lock_ReleaseRead(&buf_globalLock);
        return count;
}

/* clean a buffer synchronously */
void buf_CleanAsync(cm_buf_t *bp, cm_req_t *reqp)
{
	lock_ObtainMutex(&bp->mx);
	buf_LockedCleanAsync(bp, reqp);
	lock_ReleaseMutex(&bp->mx);
}

/* wait for a buffer's cleaning to finish */
void buf_CleanWait(cm_buf_t *bp)
{
	lock_ObtainMutex(&bp->mx);
	if (bp->flags & CM_BUF_WRITING) {
		buf_WaitIO(bp);
        }
	lock_ReleaseMutex(&bp->mx);
}

/* set the dirty flag on a buffer, and set associated write-ahead log,
 * if there is one.  Allow one to be added to a buffer, but not changed.
 *
 * The buffer must be locked before calling this routine.
 */
void buf_SetDirty(cm_buf_t *bp)
{
	osi_assert(bp->refCount > 0);
	
	osi_Log1(buf_logp, "buf_SetDirty 0x%x", bp);

        /* set dirty bit */
	bp->flags |= CM_BUF_DIRTY;

	/* and turn off EOF flag, since it has associated data now */
        bp->flags &= ~CM_BUF_EOF;
}

/* clean all buffers, reset log pointers and invalidate all buffers.
 * Called with no locks held, and returns with same.
 *
 * This function is guaranteed to clean and remove the log ptr of all the
 * buffers that were dirty or had non-zero log ptrs before the call was
 * made.  That's sufficient to clean up any garbage left around by recovery,
 * which is all we're counting on this for; there may be newly created buffers
 * added while we're running, but that should be OK.
 *
 * In an environment where there are no transactions (artificially imposed, for
 * example, when switching the database to raw mode), this function is used to
 * make sure that all updates have been written to the disk.  In that case, we don't
 * really require that we forget the log association between pages and logs, but
 * it also doesn't hurt.  Since raw mode I/O goes through this buffer package, we don't
 * have to worry about invalidating data in the buffers.
 *
 * This function is used at the end of recovery as paranoia to get the recovered
 * database out to disk.  It removes all references to the recovery log and cleans
 * all buffers.
 */
long buf_CleanAndReset(void)
{
	long i;
        cm_buf_t *bp;
	cm_req_t req;

	lock_ObtainWrite(&buf_globalLock);
        for(i=0; i<buf_hashSize; i++) {
        	for(bp = buf_hashTablepp[i]; bp; bp = bp->hashp) {
			bp->refCount++;
                        lock_ReleaseWrite(&buf_globalLock);
			
                        /* now no locks are held; clean buffer and go on */
			cm_InitReq(&req);
                        buf_CleanAsync(bp, &req);
                        buf_CleanWait(bp);
                        
                        /* relock and release buffer */
                        lock_ObtainWrite(&buf_globalLock);
                        buf_LockedRelease(bp);
                } /* over one bucket */
	}	/* for loop over all hash buckets */
        
        /* release locks */
	lock_ReleaseWrite(&buf_globalLock);

	/* and we're done */
        return 0;
}

/* called without global lock being held, reserves buffers for callers
 * that need more than one held (not locked) at once.
 */
void buf_ReserveBuffers(long nbuffers)
{
	lock_ObtainWrite(&buf_globalLock);
	while (1) {
		if (buf_reservedBufs + nbuffers > buf_maxReservedBufs) {
			buf_reserveWaiting = 1;
                        osi_Log1(buf_logp, "buf_ReserveBuffers waiting for %d bufs", nbuffers);
                        osi_SleepW((long) &buf_reservedBufs, &buf_globalLock);
                        lock_ObtainWrite(&buf_globalLock);
                }
                else {
			buf_reservedBufs += nbuffers;
                	break;
                }
        }
	lock_ReleaseWrite(&buf_globalLock);
}

int buf_TryReserveBuffers(long nbuffers)
{
	int code;

	lock_ObtainWrite(&buf_globalLock);
	if (buf_reservedBufs + nbuffers > buf_maxReservedBufs) {
		code = 0;
	}
	else {
		buf_reservedBufs += nbuffers;
                code = 1;
	}
	lock_ReleaseWrite(&buf_globalLock);
	return code;
}

/* called without global lock held, releases reservation held by
 * buf_ReserveBuffers.
 */
void buf_UnreserveBuffers(long nbuffers)
{
	lock_ObtainWrite(&buf_globalLock);
	buf_reservedBufs -= nbuffers;
        if (buf_reserveWaiting) {
		buf_reserveWaiting = 0;
                osi_Wakeup((long) &buf_reservedBufs);
        }
	lock_ReleaseWrite(&buf_globalLock);
}

/* truncate the buffers past sizep, zeroing out the page, if we don't
 * end on a page boundary.
 *
 * Requires cm_bufCreateLock to be write locked.
 */
long buf_Truncate(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp,
	osi_hyper_t *sizep)
{
	cm_buf_t *bufp;
	cm_buf_t *nbufp;			/* next buffer, if didRelease */
        osi_hyper_t bufEnd;
        long code;
        long bufferPos;
        int didRelease;
	long i;
        
	/* assert that cm_bufCreateLock is held in write mode */
        lock_AssertWrite(&scp->bufCreateLock);

	i = BUF_FILEHASH(&scp->fid);

	lock_ObtainWrite(&buf_globalLock);
	bufp = buf_fileHashTablepp[i];
	if (bufp == NULL) {
		lock_ReleaseWrite(&buf_globalLock);
		return 0;
	}

        bufp->refCount++;
	lock_ReleaseWrite(&buf_globalLock);
        for(; bufp; bufp = nbufp) {
                didRelease = 0;
		lock_ObtainMutex(&bufp->mx);

		bufEnd.HighPart = 0;
                bufEnd.LowPart = buf_bufferSize;
                bufEnd = LargeIntegerAdd(bufEnd, bufp->offset);

		if (cm_FidCmp(&bufp->fid, &scp->fid) == 0 &&
                	LargeIntegerLessThan(*sizep, bufEnd)) {
	                buf_WaitIO(bufp);
		}
	        lock_ObtainMutex(&scp->mx);
	
		/* make sure we have a callback (so we have the right value for
		 * the length), and wait for it to be safe to do a truncate.
	         */
		code = cm_SyncOp(scp, bufp, userp, reqp, 0,
				 CM_SCACHESYNC_NEEDCALLBACK
                		 | CM_SCACHESYNC_GETSTATUS
                		 | CM_SCACHESYNC_SETSIZE
				 | CM_SCACHESYNC_BUFLOCKED);
		/* if we succeeded in our locking, and this applies to the right
		 * file, and the truncate request overlaps the buffer either
		 * totally or partially, then do something.
                 */
		if (code == 0 && cm_FidCmp(&bufp->fid, &scp->fid) == 0
                	&& LargeIntegerLessThan(*sizep, bufEnd)) {
                        
                        lock_ObtainWrite(&buf_globalLock);

			/* destroy the buffer, turning off its dirty bit, if
			 * we're truncating the whole buffer.  Otherwise, set
			 * the dirty bit, and clear out the tail of the buffer
			 * if we just overlap some.
                         */
                        if (LargeIntegerLessThanOrEqualTo(*sizep, bufp->offset)) {
				/* truncating the entire page */
                                bufp->flags &= ~CM_BUF_DIRTY;
                                bufp->dataVersion = -1;	/* known bad */
                                bufp->dirtyCounter++;
                        }
			else {
				/* don't set dirty, since dirty implies
				 * currently up-to-date.  Don't need to do this,
				 * since we'll update the length anyway.
				 *
				 * Zero out remainder of the page, in case we
				 * seek and write past EOF, and make this data
				 * visible again.
                                 */
                                bufferPos = sizep->LowPart & (buf_bufferSize - 1);
                                osi_assert(bufferPos != 0);
                                memset(bufp->datap + bufferPos, 0,
					buf_bufferSize - bufferPos);
			}

                        lock_ReleaseWrite(&buf_globalLock);

                }
		
                lock_ReleaseMutex(&scp->mx);
		lock_ReleaseMutex(&bufp->mx);
		if (!didRelease) {
			lock_ObtainWrite(&buf_globalLock);
			nbufp = bufp->fileHashp;
                        if (nbufp) nbufp->refCount++;
                        buf_LockedRelease(bufp);
			lock_ReleaseWrite(&buf_globalLock);
		}

		/* bail out early if we fail */
                if (code) {
			/* at this point, nbufp is held; bufp has already been
                         * released.
                         */
                        if (nbufp) buf_Release(nbufp);
                	return code;
		}
	}
	
        /* success */
        return 0;
}

long buf_FlushCleanPages(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp)
{
	long code;
	cm_buf_t *bp;		/* buffer we're hacking on */
        cm_buf_t *nbp;
        int didRelease;
	long i;

	i = BUF_FILEHASH(&scp->fid);

	code = 0;
	lock_ObtainWrite(&buf_globalLock);
        bp = buf_fileHashTablepp[i];
        if (bp) bp->refCount++;
        lock_ReleaseWrite(&buf_globalLock);
	for(; bp; bp = nbp) {
		didRelease = 0;	/* haven't released this buffer yet */

		/* clean buffer synchronously */
		if (cm_FidCmp(&bp->fid, &scp->fid) == 0) {
                        lock_ObtainMutex(&bp->mx);

			/* start cleaning the buffer, and wait for it to finish */
			buf_LockedCleanAsync(bp, reqp);
                        buf_WaitIO(bp);
                        lock_ReleaseMutex(&bp->mx);

                        code = (*cm_buf_opsp->Stabilizep)(scp, userp, reqp);
                        if (code) goto skip;

			lock_ObtainWrite(&buf_globalLock);
			/* actually, we only know that buffer is clean if ref
			 * count is 1, since we don't have buffer itself locked.
                         */
			if (!(bp->flags & CM_BUF_DIRTY)) {
				if (bp->refCount == 1) {	/* bp is held above */
                                	buf_LockedRelease(bp);
                                        nbp = bp->fileHashp;
                                        if (nbp) nbp->refCount++;
                                        didRelease = 1;
                                	buf_Recycle(bp);
				}
                        }
			lock_ReleaseWrite(&buf_globalLock);

                        (*cm_buf_opsp->Unstabilizep)(scp, userp);
		}

skip:
		if (!didRelease) {
			lock_ObtainWrite(&buf_globalLock);
                        if (nbp = bp->fileHashp) nbp->refCount++;
                	buf_LockedRelease(bp);
                        lock_ReleaseWrite(&buf_globalLock);
		}
	}	/* for loop over a bunch of buffers */
	
        /* done */
	return code;
}

long buf_CleanVnode(struct cm_scache *scp, cm_user_t *userp, cm_req_t *reqp)
{
	long code;
	cm_buf_t *bp;		/* buffer we're hacking on */
        cm_buf_t *nbp;		/* next one */
	long i;

	i = BUF_FILEHASH(&scp->fid);

	code = 0;
	lock_ObtainWrite(&buf_globalLock);
        bp = buf_fileHashTablepp[i];
        if (bp) bp->refCount++;
        lock_ReleaseWrite(&buf_globalLock);
	for(; bp; bp = nbp) {
		/* clean buffer synchronously */
		if (cm_FidCmp(&bp->fid, &scp->fid) == 0) {
			if (userp) {
				lock_ObtainMutex(&bp->mx);
				if (bp->userp) cm_ReleaseUser(bp->userp);
                                bp->userp = userp;
				lock_ReleaseMutex(&bp->mx);
                                cm_HoldUser(userp);
                        }
			buf_CleanAsync(bp, reqp);
	                buf_CleanWait(bp);
                        lock_ObtainMutex(&bp->mx);
			if (bp->flags & CM_BUF_ERROR) {
				if (code == 0 || code == -1) code = bp->error;
                                if (code == 0) code = -1;
                        }
                        lock_ReleaseMutex(&bp->mx);
		}

		lock_ObtainWrite(&buf_globalLock);
		buf_LockedRelease(bp);
                nbp = bp->fileHashp;
                if (nbp) nbp->refCount++;
		lock_ReleaseWrite(&buf_globalLock);
	}	/* for loop over a bunch of buffers */
	
        /* done */
	return code;
}
