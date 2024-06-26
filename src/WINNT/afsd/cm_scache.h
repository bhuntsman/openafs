/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 *
 */
#ifndef __CM_SCACHE_H_ENV__
#define __CM_SCACHE_H_ENV__ 1

typedef struct cm_fid {
	unsigned long cell;
        unsigned long volume;
        unsigned long vnode;
        unsigned long unique;
} cm_fid_t;

typedef struct cm_accessCache {
	osi_queue_t q;			/* queue header */
        struct cm_user *userp;		/* user having access rights */
        long rights;			/* rights */
} cm_accessCache_t;

typedef struct cm_file_lock {
	osi_queue_t q;			/* list of all locks */
	osi_queue_t fileq;		/* per-file list of locks */
	cm_user_t *userp;
	LARGE_INTEGER LOffset;
	LARGE_INTEGER LLength;
	cm_fid_t fid;
	unsigned char LockType;
	unsigned char flags;
} cm_file_lock_t;

#define CM_FILELOCK_FLAG_INVALID	0x1
#define CM_FILELOCK_FLAG_WAITING	0x2

typedef struct cm_prefetch {		/* last region scanned for prefetching */
	osi_hyper_t base;		/* start of region */
        osi_hyper_t end;		/* first char past region */
} cm_prefetch_t;

typedef struct cm_scache {
	osi_queue_t q;			/* lru queue; cm_scacheLock */
	struct cm_scache *nextp;	/* next in hash; cm_scacheLock */
	cm_fid_t fid;
        long flags;			/* flags; locked by mx */

	/* synchronization stuff */
        osi_mutex_t mx;			/* mutex for this structure */
        osi_rwlock_t bufCreateLock;	/* read-locked during buffer creation;
        				 * write-locked to prevent buffers from
                                         * being created during a truncate op, etc.
                                         */
        int refCount;			/* reference count; cm_scacheLock */
        osi_queueData_t *bufReadsp;	/* queue of buffers being read */
        osi_queueData_t *bufWritesp;	/* queue of buffers being written */

	/* parent info for ACLs */
        long parentVnode;		/* parent vnode for ACL callbacks */
        long parentUnique;		/* for ACL callbacks */

	/* local modification stat */
        long mask;			/* for clientModTime, length and
					 * truncPos */

	/* file status */
	int fileType;			/* file type */
	unsigned long clientModTime;	/* mtime */
        unsigned long serverModTime;	/* at server, for concurrent call
					 * comparisons */
        osi_hyper_t length;		/* file length */
	cm_prefetch_t prefetch;		/* prefetch info structure */
        int unixModeBits;		/* unix protection mode bits */
        int linkCount;			/* link count */
        long dataVersion;		/* data version */
        long owner;			/* file owner */
        long group;			/* file owning group */

	/* pseudo file status */
	osi_hyper_t serverLength;	/* length known to server */

	/* aux file status */
        osi_hyper_t truncPos;		/* file size to truncate to before
					 * storing data */

	/* symlink and mount point info */
        char *mountPointStringp;	/* the string stored in a mount point;
        				 * first char is type, then vol name.
                                         * If this is a normal symlink, we store
					 * the link contents here.
                                         */
	cm_fid_t *mountRootFidp;	/* mounted on root */
	unsigned int mountRootGen;	/* time to update mountRootFidp? */
	cm_fid_t *dotdotFidp;		/* parent of volume root */

	/* callback info */
        struct cm_server *cbServerp;	/* server granting callback */
        long cbExpires;			/* time callback expires */

	/* access cache */
        long anyAccess;			/* anonymous user's access */
        struct cm_aclent *randomACLp;	/* access cache entries */

	/* file locks */
	osi_queue_t *fileLocks;
	
	/* volume info */
        struct cm_volume *volp;		/* volume info; held reference */
  
        /* bulk stat progress */
        osi_hyper_t bulkStatProgress;	/* track bulk stats of large dirs */

        /* open state */
        short openReads;		/* opens for reading */
        short openWrites;		/* open for writing */
        short openShares;		/* open for read excl */
        short openExcls;		/* open for exclusives */
} cm_scache_t;

/* mask field - tell what has been modified */
#define CM_SCACHEMASK_CLIENTMODTIME	1	/* client mod time */
#define CM_SCACHEMASK_LENGTH		2	/* length */
#define CM_SCACHEMASK_TRUNCPOS		4	/* truncation position */

/* fileType values */
#define CM_SCACHETYPE_FILE		1	/* a file */
#define CM_SCACHETYPE_DIRECTORY		2	/* a dir */
#define CM_SCACHETYPE_SYMLINK		3	/* a symbolic link */
#define CM_SCACHETYPE_MOUNTPOINT	4	/* a mount point */

/* flag bits */
#define CM_SCACHEFLAG_STATD		1	/* status info is valid */
#define CM_SCACHEFLAG_DELETED		2	/* file has been deleted */
#define CM_SCACHEFLAG_CALLBACK		4	/* have a valid callback */
#define CM_SCACHEFLAG_STORING		8	/* status being stored back */
#define CM_SCACHEFLAG_FETCHING		0x10	/* status being fetched */
#define CM_SCACHEFLAG_SIZESTORING	0x20	/* status being stored that
						 * changes the data; typically,
						 * this is a truncate op. */
#define CM_SCACHEFLAG_INHASH		0x40	/* in the hash table */
#define CM_SCACHEFLAG_BULKSTATTING	0x80	/* doing a bulk stat */
#define CM_SCACHEFLAG_WAITING		0x200	/* waiting for fetch/store
						 * state to change */
#define CM_SCACHEFLAG_PURERO		0x400	/* read-only (not even backup);
						 * for mount point eval */
#define CM_SCACHEFLAG_RO		0x800	/* read-only
						 * (can't do write ops) */
#define CM_SCACHEFLAG_GETCALLBACK	0x1000	/* we're getting a callback */
#define CM_SCACHEFLAG_DATASTORING	0x2000	/* data being stored */
#define CM_SCACHEFLAG_PREFETCHING	0x4000	/* somebody is prefetching */
#define CM_SCACHEFLAG_OVERQUOTA		0x8000	/* over quota */
#define CM_SCACHEFLAG_OUTOFSPACE	0x10000	/* out of space */
#define CM_SCACHEFLAG_ASYNCSTORING	0x20000	/* scheduled to store back */
#define CM_SCACHEFLAG_LOCKING		0x40000	/* setting/clearing file lock */
#define CM_SCACHEFLAG_WATCHED		0x80000	/* directory being watched */
#define CM_SCACHEFLAG_WATCHEDSUBTREE	0x100000 /* dir subtree being watched */
#define CM_SCACHEFLAG_ANYWATCH \
			(CM_SCACHEFLAG_WATCHED | CM_SCACHEFLAG_WATCHEDSUBTREE)

/* sync flags for calls to the server.  The CM_SCACHEFLAG_FETCHING,
 * CM_SCACHEFLAG_STORING and CM_SCACHEFLAG_SIZESTORING flags correspond to the
 * below, except for FETCHDATA and STOREDATA, which correspond to non-null
 * buffers in bufReadsp and bufWritesp.
 * These flags correspond to individual RPCs that we may be making, and at most
 * one can be set in any one call to SyncOp.
 */
#define CM_SCACHESYNC_FETCHSTATUS	1	/* fetching status info */
#define CM_SCACHESYNC_STORESTATUS	2	/* storing status info */
#define CM_SCACHESYNC_FETCHDATA		4	/* fetch data */
#define CM_SCACHESYNC_STOREDATA		8	/* store data */
#define CM_SCACHESYNC_STORESIZE		0x10	/* store new file size */
#define CM_SCACHESYNC_GETCALLBACK	0x20	/* fetching a callback */
#define CM_SCACHESYNC_STOREDATA_EXCL	0x40	/* store data */
#define CM_SCACHESYNC_ASYNCSTORE	0x80	/* schedule data store */
#define CM_SCACHESYNC_LOCK		0x100	/* set/clear file lock */

/* sync flags for calls within the client; there are no corresponding flags
 * in the scache entry, because we hold the scache entry locked during the
 * operations below.
 */
#define CM_SCACHESYNC_GETSTATUS		0x1000	/* read the status */
#define CM_SCACHESYNC_SETSTATUS		0x2000	/* e.g. utimes */
#define CM_SCACHESYNC_READ		0x4000	/* read data from a chunk */
#define CM_SCACHESYNC_WRITE		0x8000	/* write data to a chunk */
#define CM_SCACHESYNC_SETSIZE		0x10000	/* shrink the size of a file,
						 * e.g. truncate */
#define CM_SCACHESYNC_NEEDCALLBACK	0x20000	/* need a callback on the file */
#define CM_SCACHESYNC_CHECKRIGHTS	0x40000	/* check that user has desired
						 * access rights */
#define CM_SCACHESYNC_BUFLOCKED		0x80000	/* the buffer is locked */
#define CM_SCACHESYNC_NOWAIT		0x100000/* don't wait for the state,
						 * just fail */

/* flags for cm_MergeStatus */
#define CM_MERGEFLAG_FORCE		1	/* check mtime before merging;
						 * used to see if we're merging
						 * in old info.
                                                 */

/* hash define.  Must not include the cell, since the callback revocation code
 * doesn't necessarily know the cell in the case of a multihomed server
 * contacting us from a mystery address.
 */
#define CM_SCACHE_HASH(fidp)	(((unsigned long)	\
				   ((fidp)->volume +	\
				    (fidp)->vnode +	\
				    (fidp)->unique))	\
					% cm_hashTableSize)

extern cm_scache_t cm_fakeSCache;

extern void cm_InitSCache(long);

extern long cm_GetSCache(cm_fid_t *, cm_scache_t **, struct cm_user *,
	struct cm_req *);

extern void cm_PutSCache(cm_scache_t *);

extern cm_scache_t *cm_GetNewSCache(void);

extern int cm_FidCmp(cm_fid_t *, cm_fid_t *);

extern long cm_SyncOp(cm_scache_t *, struct cm_buf *, struct cm_user *,
	struct cm_req *, long, long);

extern void cm_SyncOpDone(cm_scache_t *, struct cm_buf *, long);

extern void cm_MergeStatus(cm_scache_t *, struct AFSFetchStatus *, struct AFSVolSync *,
	struct cm_user *, int flags);

extern void cm_AFSFidFromFid(struct AFSFid *, cm_fid_t *);

extern void cm_HoldSCache(cm_scache_t *);

extern void cm_ReleaseSCache(cm_scache_t *);

extern cm_scache_t *cm_FindSCache(cm_fid_t *fidp);

extern long cm_hashTableSize;

extern osi_rwlock_t cm_scacheLock;

extern osi_queue_t *cm_allFileLocks;

extern cm_scache_t **cm_hashTablep;

extern void cm_DiscardSCache(cm_scache_t *scp);

#endif /*  __CM_SCACHE_H_ENV__ */
