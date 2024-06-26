/* Copyright (C) 1998 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT TRANSARC CORPORATION 1989
 * LICENSED MATERIALS - PROPERTY OF TRANSARC
 * ALL RIGHTS RESERVED
 */

typedef struct {
    time_t last_use;
    afs_int32 host;
} kalog_elt;

#define KALOG_DB_MODE 		0600

/* types of operations that we log */
#define LOG_GETTICKET 		0
#define LOG_CHPASSWD		1
#define	LOG_CRUSER		2
#define	LOG_AUTHENTICATE	3
#define	LOG_DELUSER		4
#define	LOG_SETFIELDS		5
#define	LOG_UNLOCK              6
#define	LOG_AUTHFAILED	        7

#ifdef AUTH_DBM_LOG
#ifdef AFS_LINUX20_ENV
#include <gdbm.h>
#define dbm_store	gdbm_store
#define dbm_firstkey	gdbm_firstkey
#define dbm_fetch	gdbm_fetch
#define dbm_close	gdbm_close
#define dbm_open(F, L, M)	gdbm_open(F, 512, L, M, 0)
#define afs_dbm_nextkey(d, k)	gdbm_nextkey(d, k)
#define DBM GDBM_FILE
#define DBM_REPLACE GDBM_REPLACE

#else /* AFS_LINUX20_ENV */
#include <ndbm.h>
#define afs_dbm_nextkey(d, k) dbm_nextkey(d)
#endif
#endif /* AUTH_DBM_LOG */

#ifdef AUTH_DBM_LOG
#define KALOG(a,b,c,d,e,f,g) kalog_log(a,b,c,d,e,f,g)
#else
#define KALOG(a,b,c,d,e,f,g) ka_log(a,b,c,d,e,f,g)
#endif
