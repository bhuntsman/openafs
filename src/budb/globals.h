 /* Copyright (C) 1998 Transarc Corporation - All rights reserved 
  */
/* ------------------
 * global configuration information for the database server
 * ------------------
 */

#define	DEFAULT_DBPREFIX "bdb"				/* db name prefix */

/* debug and test flags */

#define	DF_NOAUTH		0x1	/* no authentication */
#define	DF_RECHECKNOAUTH	0x2	/* recheck no authentication ? */
#define	DF_SMALLHT		0x4	/* use small hash tables */
#define	DF_TRUNCATEDB		0x8	/* truncate database on startup */

extern int debugging;			/* for controlling debug output */

struct buServerConfS
{
    /* global configuration */
    char *databaseDirectory;		/* where database is placed */
    char *databaseName;			/* database file name */
    char *databaseExtension;		/* extension (depends on ubik) */

    /* error logging */
    FILE *log;				/* log file for status/errors */
    
    /* ubik and comm. related */
    afs_int32 myHost;
    afs_int32 serverList[MAXSERVERS];	/* list of ubik servers */
    char *cellConfigdir;		/* afs cell config. directory */
    struct ubik_dbase	*database;	/* handle for the database */

    /* debug and test */
    afs_uint32	debugFlags;
};

typedef	struct buServerConfS	buServerConfT;
typedef	buServerConfT		*buServerConfP;

extern	buServerConfP	globalConfPtr;

/* for synchronizing the threads dumping the database */

#define	DS_WAITING	1		/* reader/writer sleeping */
#define	DS_DONE		2		/* finished */
#define	DS_DONE_ERROR	4		/* finished with errors */

#define	DUMP_TTL_INC	300		/* secs. before dump times out */

#include <time.h>

struct dumpSyncS
{
    struct Lock	ds_lock;		/* for this struct. */
    afs_int32 	statusFlags;		/* 0, or 1 for dump in progress */
    int	 	pipeFid[2];		/* pipe between threads */
    char 	ds_writerStatus;
    char 	ds_readerStatus;

    PROCESS 	dumperPid;		/* pid of dumper lwp */
    struct ubik_trans *ut;		/* dump db transaction */
    afs_int32 	ds_bytes;		/* no. of bytes buffered */
    time_t 	timeToLive;		/* time. After this, kill the dump */
};

typedef	struct dumpSyncS	dumpSyncT;
typedef	dumpSyncT		*dumpSyncP;

extern dumpSyncP	dumpSyncPtr;	/* defined in dbs_dump.c */
    





