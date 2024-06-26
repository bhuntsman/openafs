/* Copyright (C) 1998 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include <afs/param.h>
#include <afs/stds.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <direct.h>
#include <io.h>
#include <WINNT/afsevent.h>
#else
#include <unistd.h>
#include <netinet/in.h>
#endif /* AFS_NT40_ENV */
#include <afs/cellconfig.h>
#include <rx/rx.h>
#include <rx/xdr.h>
#include <rx/rx_globals.h>
#include "bosint.h"
#include "bnode.h"
#include <afs/auth.h>
#include <afs/keys.h>
#include <afs/ktime.h>
#include <afs/afsutil.h>
#include <afs/fileutil.h>
#include <afs/procmgmt.h>  /* signal(), kill(), wait(), etc. */
#if defined(AFS_SGI_ENV)
#include <afs/afs_args.h>
#endif


#define BOZO_LWP_STACKSIZE	16000
extern int BOZO_ExecuteRequest();
extern int RXSTATS_ExecuteRequest();
extern int afsconf_GetKey();
extern struct bnode_ops fsbnode_ops, ezbnode_ops, cronbnode_ops;
struct afsconf_dir *bozo_confdir = 0;	/* bozo configuration dir */
static char *bozo_pid;
struct rx_securityClass *bozo_rxsc[3];
const char *bozo_fileName;
FILE *bozo_logFile;
extern int rx_stackSize;    /* for rx_SetStackSize macro */

int DoLogging = 0;
static afs_int32 nextRestart;
static afs_int32 nextDay;

struct ktime bozo_nextRestartKT, bozo_nextDayKT;
int bozo_newKTs;

struct bztemp {
    FILE *file;
};

/* check whether caller is authorized to manage RX statistics */
int bozo_rxstat_userok(call)
    struct rx_call *call;
{
    return afsconf_SuperUser(bozo_confdir, call, (char *)0);
}

/* restart bozo process */
bozo_ReBozo() {
#ifdef AFS_NT40_ENV
    /* exit with restart code; SCM integrator process will restart bosserver */
    int status = BOSEXIT_RESTART;

    /* if noauth flag is set, pass "-noauth" to new bosserver */
    if (afsconf_GetNoAuthFlag(bozo_confdir)) {
	status |= BOSEXIT_NOAUTH_FLAG;
    }
    /* if logging is on, pass "-log" to new bosserver */
    if (DoLogging) {
	status |= BOSEXIT_LOGGING_FLAG;
    }
    exit(status);
#else
    /* exec new bosserver process */
    char *argv[4];
    int i = 0;

    argv[i] = (char *)AFSDIR_SERVER_BOSVR_FILEPATH;
    i++;

    /* if noauth flag is set, pass "-noauth" to new bosserver */
    if (afsconf_GetNoAuthFlag(bozo_confdir)) {
	argv[i] = "-noauth";
	i++;
    }
    /* if logging is on, pass "-log" to new bosserver */
    if (DoLogging) {
	argv[i] = "-log";
	i++;
    }

    /* null-terminate argument list */
    argv[i] = NULL;

    /* close random fd's */
    for (i = 3; i < 64; i++) {
	close(i);
    }

    execv(argv[0], argv);  /* should not return */
    _exit(1);
#endif /* AFS_NT40_ENV */
}

/* make sure a dir exists */
static MakeDir(adir)
register char *adir; {
    struct stat tstat;
    register afs_int32 code;
    if (stat(adir, &tstat) < 0 || (tstat.st_mode & S_IFMT) != S_IFDIR) {
	int reqPerm;
	unlink(adir);
	reqPerm = GetRequiredDirPerm (adir);
	if (reqPerm == -1) reqPerm = 0777;
#ifdef AFS_NT40_ENV
	/* underlying filesystem may not support directory protection */
	code = mkdir(adir);
#else
	code = mkdir(adir, reqPerm);
#endif
	return code;
    }
    return 0;
}

/* create all the bozo dirs */
static CreateDirs() {
    
    MakeDir(AFSDIR_USR_DIRPATH);
    MakeDir(AFSDIR_SERVER_AFS_DIRPATH);
    MakeDir(AFSDIR_SERVER_BIN_DIRPATH);
    MakeDir(AFSDIR_SERVER_ETC_DIRPATH); 
    MakeDir(AFSDIR_SERVER_LOCAL_DIRPATH);
    MakeDir(AFSDIR_SERVER_DB_DIRPATH); 
    MakeDir(AFSDIR_SERVER_LOGS_DIRPATH);
#ifndef AFS_NT40_ENV
    MakeDir(AFSDIR_CLIENT_VICE_DIRPATH);
    MakeDir(AFSDIR_CLIENT_ETC_DIRPATH);

    symlink(AFSDIR_SERVER_THISCELL_FILEPATH, AFSDIR_CLIENT_THISCELL_FILEPATH); 
    symlink(AFSDIR_SERVER_CELLSERVDB_FILEPATH, AFSDIR_CLIENT_CELLSERVDB_FILEPATH);
#endif /* AFS_NT40_ENV */
    return 0;
}

/* strip the \\n from the end of the line, if it is present */
static StripLine(abuffer)
register char *abuffer; {
    register char *tp;
    
    tp = abuffer + strlen(abuffer); /* starts off pointing at the null  */
    if(tp == abuffer) return 0;	    /* null string, no last character to check */
    tp--;	/* aim at last character */
    if (*tp == '\n') *tp = 0;
    return 0;
}

/* write one bnode's worth of entry into the file */
static bzwrite(abnode, at)
register struct bnode *abnode;
register struct bztemp *at; {
    register int i;
    char tbuffer[BOZO_BSSIZE];
    register afs_int32 code;

    if (abnode->notifier)
	fprintf(at->file, "bnode %s %s %d %s\n", 
		abnode->type->name, abnode->name, abnode->fileGoal, abnode->notifier);
    else
	fprintf(at->file, "bnode %s %s %d\n", abnode->type->name, abnode->name, abnode->fileGoal);
    for(i=0;;i++) {
	code = bnode_GetParm(abnode, i, tbuffer, BOZO_BSSIZE);
	if (code) {
	    if (code != BZDOM) return code;
	    break;
	}
	fprintf(at->file, "parm %s\n", tbuffer);
    }
    fprintf(at->file, "end\n");
    return 0;
}

#define	MAXPARMS    20
ReadBozoFile(aname)
char *aname; {
    register FILE *tfile;
    char tbuffer[BOZO_BSSIZE];
    register char *tp;
    char *instp, *typep, *notifier, *notp;
    register afs_int32 code;
    afs_int32 ktmask, ktday, kthour, ktmin, ktsec;
    afs_int32 i, goal;
    struct bnode *tb;
    char *parms[MAXPARMS];

    /* rename BozoInit to BosServer for the user */
    if (!aname) {
	/* if BozoInit exists and BosConfig doesn't, try a rename */
	if (access(AFSDIR_SERVER_BOZINIT_FILEPATH, 0) == 0
	    && access(AFSDIR_SERVER_BOZCONF_FILEPATH, 0) != 0) {
	    code = renamefile(AFSDIR_SERVER_BOZINIT_FILEPATH, AFSDIR_SERVER_BOZCONF_FILEPATH);
	    if (code < 0)
		perror("bosconfig rename");
	}
    }

    /* setup default times we want to do restarts */
    bozo_nextRestartKT.mask = KTIME_HOUR | KTIME_MIN | KTIME_DAY;
    bozo_nextRestartKT.hour = 4; /* 4 am */
    bozo_nextRestartKT.min = 0;
    bozo_nextRestartKT.day =	0;  /* Sunday */
    bozo_nextDayKT.mask = KTIME_HOUR | KTIME_MIN;
    bozo_nextDayKT.hour = 5;
    bozo_nextDayKT.min = 0;

    for(code=0;code<MAXPARMS;code++)
	parms[code] = (char *) 0;
    instp = typep = notifier = (char *) 0;
    tfile = (FILE *) 0;
    if (!aname) aname = (char *)bozo_fileName;
    tfile = fopen(aname, "r");
    if (!tfile) 
	return 0; 	/* -1 */
    instp = (char *) malloc(BOZO_BSSIZE);
    typep = (char *) malloc(BOZO_BSSIZE);
    notifier = notp = (char *) malloc(BOZO_BSSIZE);
    while (1) {
	/* ok, read lines giving parms and such from the file */
	tp = fgets(tbuffer, sizeof(tbuffer), tfile);
	if (tp == (char	*) 0) break;	/* all done */

	if (strncmp(tbuffer, "restarttime", 11) == 0) {
	    code = sscanf(tbuffer, "restarttime %d %d %d %d %d",
			  &ktmask, &ktday, &kthour, &ktmin, &ktsec);
	    if (code != 5) {
		code = -1;
		goto fail;
	    }
	    /* otherwise we've read in the proper ktime structure; now assign
	       it and continue processing */
	    bozo_nextRestartKT.mask = ktmask;
	    bozo_nextRestartKT.day = ktday;
	    bozo_nextRestartKT.hour = kthour;
	    bozo_nextRestartKT.min = ktmin;
	    bozo_nextRestartKT.sec = ktsec;
	    continue;
	}

	if (strncmp(tbuffer, "checkbintime", 12) == 0) {
	    code = sscanf(tbuffer, "checkbintime %d %d %d %d %d",
			  &ktmask, &ktday, &kthour, &ktmin, &ktsec);
	    if (code != 5) {
		code = -1;
		goto fail;
	    }
	    /* otherwise we've read in the proper ktime structure; now assign
	       it and continue processing */
	    bozo_nextDayKT.mask = ktmask;	/* time to restart the system */
	    bozo_nextDayKT.day = ktday;
	    bozo_nextDayKT.hour = kthour;
	    bozo_nextDayKT.min = ktmin;
	    bozo_nextDayKT.sec = ktsec;
	    continue;
	}

	if (strncmp("bnode", tbuffer, 5) != 0) {
	    code = -1;
	    goto fail;
	}
	notifier = notp;
	code = sscanf(tbuffer, "bnode %s %s %d %s", typep, instp, &goal, notifier);
	if (code < 3) {
	    code = -1;
	    goto fail;
	} else if (code == 3)
	    notifier = (char *)0;
	
	for(i=0;i<MAXPARMS;i++) {
	    /* now read the parms, until we see an "end" line */
	    tp = fgets(tbuffer, sizeof(tbuffer), tfile);
	    if (!tp) {
		code = -1;
		goto fail;
	    }
	    StripLine(tbuffer);
	    if (!strncmp(tbuffer, "end", 3)) break;
	    if (strncmp(tbuffer, "parm ", 5)) {
		code = -1;
		goto fail;    /* no "parm " either */
	    }
	    if (!parms[i])  /* make sure there's space */
		parms[i] = (char *) malloc(BOZO_BSSIZE);
	    strcpy(parms[i], tbuffer+5);    /* remember the parameter for later */
	}

	/* ok, we have the type and parms, now create the object */
	code = bnode_Create(typep, instp, &tb, parms[0], parms[1], parms[2],
			  parms[3], parms[4], notifier,
			  goal ? BSTAT_NORMAL : BSTAT_SHUTDOWN);
	if (code) goto fail;

	/* bnode created in 'temporarily shutdown' state;
	   check to see if we are supposed to run this guy,
	    and if so, start the process up */
	if (goal) {
	    bnode_SetStat(tb, BSTAT_NORMAL);	/* set goal, taking effect immediately */
	}
	else {
	    bnode_SetStat(tb, BSTAT_SHUTDOWN);
	}
    }
    /* all done */
    code = 0;

fail:
    if (instp) free(instp);
    if (typep) free(typep);
    for(i=0;i<MAXPARMS;i++) if (parms[i]) free(parms[i]);
    if (tfile) fclose(tfile);
    return code;
}

/* write a new bozo file */
WriteBozoFile(aname)
char *aname; {
    register FILE *tfile;
    char tbuffer[AFSDIR_PATH_MAX];
    register afs_int32 code;
    struct bztemp btemp;

    if (!aname) aname = (char *)bozo_fileName;
    strcpy(tbuffer, aname);
    strcat(tbuffer, ".NBZ");
    tfile = fopen(tbuffer, "w");
    if (!tfile) return -1;
    btemp.file = tfile;
    fprintf(tfile, "restarttime %d %d %d %d %d\n", bozo_nextRestartKT.mask,
	    bozo_nextRestartKT.day, bozo_nextRestartKT.hour, bozo_nextRestartKT.min,
	    bozo_nextRestartKT.sec);
    fprintf(tfile, "checkbintime %d %d %d %d %d\n", bozo_nextDayKT.mask,
	    bozo_nextDayKT.day, bozo_nextDayKT.hour, bozo_nextDayKT.min,
	    bozo_nextDayKT.sec);
    code = bnode_ApplyInstance(bzwrite, &btemp);
    if (code || (code = ferror(tfile))) {	/* something went wrong */
	fclose(tfile);
	unlink(tbuffer);
	return code;
    }
    /* close the file, check for errors and snap new file into place */
    if (fclose(tfile) == EOF) {
	unlink(tbuffer);
	return -1;
    }
    code = renamefile(tbuffer, aname);
    if (code) {
	unlink(tbuffer);
	return -1;
    }
    return 0;
}

static bdrestart(abnode, arock)
register struct bnode *abnode;
char *arock; {
    register afs_int32 code;
    
    if (abnode->fileGoal != BSTAT_NORMAL || abnode->goal != BSTAT_NORMAL)
	return 0;	/* don't restart stopped bnodes */
    bnode_Hold(abnode);
    code = bnode_RestartP(abnode);
    if (code) {
	/* restart the dude */
	bnode_SetStat(abnode, BSTAT_SHUTDOWN);
	bnode_WaitStatus(abnode, BSTAT_SHUTDOWN);
	bnode_SetStat(abnode, BSTAT_NORMAL);
    }
    bnode_Release(abnode);
    return 0;	/* keep trying all bnodes */
}

#define	BOZO_MINSKIP 3600	    /* minimum to advance clock */
/* lwp to handle system restarts */
static BozoDaemon() {
    register afs_int32 now;
    
    /* now initialize the values */
    bozo_newKTs = 1;
    while (1) {
	IOMGR_Sleep(60);
	now = FT_ApproxTime();

	if (bozo_newKTs) {	/* need to recompute restart times */
	    bozo_newKTs = 0;	/* done for a while */
	    nextRestart = ktime_next(&bozo_nextRestartKT, BOZO_MINSKIP);
	    nextDay = ktime_next(&bozo_nextDayKT, BOZO_MINSKIP);
	}

	/* see if we should do a restart */
	if (now > nextRestart) {
	    BOZO_ReBozo(0);	/* doesn't come back */
	}
	
	/* see if we should restart a server */
	if (now > nextDay) {
	    nextDay = ktime_next(&bozo_nextDayKT, BOZO_MINSKIP);
	    
	    /* call the bnode restartp function, and restart all that require it */
	    bnode_ApplyInstance(bdrestart, 0);
	}
    }
}

#ifdef AFS_AIX32_ENV
static tweak_config()
{
    FILE *f;
    char c[80];
    int s, sb_max, ipfragttl;

    sb_max = 131072;
    ipfragttl = 20;
    f = popen("/usr/sbin/no -o sb_max", "r");
    s = fscanf(f, "sb_max = %d", &sb_max);
    fclose(f);
    if (s < 1) 
	return;
    f = popen("/usr/sbin/no -o ipfragttl", "r");
    s = fscanf(f, "ipfragttl = %d", &ipfragttl);
    fclose(f);
    if (s < 1) 
	ipfragttl = 20;

    if (sb_max < 131072) 
	sb_max = 131072;
    if (ipfragttl > 20)
	ipfragttl = 20;
    
    sprintf(c, "/usr/sbin/no -o sb_max=%d -o ipfragttl=%d", sb_max, ipfragttl);
    f = popen(c, "r");
    fclose(f);
}
#endif

/*
 * This routine causes the calling process to go into the background and
 * to lose its controlling tty.
 *
 * It does not close or otherwise alter the standard file descriptors.
 *
 * It writes warning messages to the standard error output if certain
 * fundamental errors occur.
 *
 * This routine requires
 * 
 * #include <sys/types.h>
 * #include <sys/stat.h>
 * #include <fcntl.h>
 * #include <unistd.h>
 * #include <stdlib.h>
 *
 * and has been tested on:
 *
 * AIX 4.2
 * Digital Unix 4.0D
 * HP-UX 11.0
 * IRIX 6.5
 * Linux 2.1.125
 * Solaris 2.5
 * Solaris 2.6
 */

#ifndef AFS_NT40_ENV
static void
background(void)
{
    /* 
     * A process is a process group leader if its process ID
     * (getpid()) and its process group ID (getpgrp()) are the same.
     */

    /*
     * To create a new session (and thereby lose our controlling
     * terminal) we cannot be a process group leader.
     *
     * To guarantee we are not a process group leader, we fork and
     * let the parent process exit.
     */

    if (getpid() == getpgrp()) {
        pid_t pid;
        pid = fork();
        switch(pid) {
        case -1:
            abort();    /* leave footprints */
            break;
        case 0:         /* child */
            break;
        default:        /* parent */
            exit(0);
            break;
        }
    }

    /*
     * By here, we are not a process group leader, so we can make a
     * new session and become the session leader.
     */

    {
        pid_t sid = setsid();

        if (sid == -1) {
            static char err[] = "bosserver: WARNING: setsid() failed\n";
            write(STDERR_FILENO, err, sizeof err - 1);
        }
    }

    /*
     * Once we create a new session, the current process is a
     * session leader without a controlling tty.
     *
     * On some systems, the first tty device the session leader
     * opens automatically becomes the controlling tty for the
     * session.
     *
     * So, to guarantee we do not acquire a controlling tty, we fork
     * and let the parent process exit.  The child process is not a
     * session leader, and so it will not acquire a controlling tty
     * even if it should happen to open a tty device.
     */

    if (getpid() == getpgrp()) {
        pid_t pid;
        pid = fork();
        switch(pid) {
        case -1:
            abort();    /* leave footprints */
            break;
        case 0:         /* child */
            break;
        default:        /* parent */
            exit(0);
            break;
        }
    }

    /*
     * check that we no longer have a controlling tty
     */

    {
        int fd;

        fd = open("/dev/tty", O_RDONLY);

        if (fd >= 0) {
            static char err[] = "bosserver: WARNING: /dev/tty still attached\n";
            close(fd);
            write(STDERR_FILENO, err, sizeof err - 1);
        }
    }
}
#endif /* ! AFS_NT40_ENV */

/* start a process and monitor it */

#include "AFS_component_version_number.c"


main (argc, argv,envp)
int argc;
char **argv;
char **envp;
{
    struct rx_service *tservice;
    register afs_int32 code;
    struct afsconf_dir *tdir;
    int noAuth = 0;
    struct ktc_encryptionKey tkey;
    int i;
    pid_t	newSessionID;
    char namebuf[AFSDIR_PATH_MAX];

#ifdef	AFS_AIX32_ENV
    struct sigaction nsa;

    /* for some reason, this permits user-mode RX to run a lot faster.
     * we do it here in the bosserver, so we don't have to do it 
     * individually in each server.
     */
    tweak_config();

    /*
     * The following signal action for AIX is necessary so that in case of a 
     * crash (i.e. core is generated) we can include the user's data section 
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
    sigaction(SIGABRT, &nsa, NULL);
#endif

#ifdef AFS_NT40_ENV
    /* Initialize winsock */
    if (afs_winsockInit() < 0) {
	ReportErrorEventAlt(AFSEVT_SVR_WINSOCK_INIT_FAILED, 0, argv[0], 0);
	fprintf(stderr, "%s: Couldn't initialize winsock.\n", argv[0]);
	exit(2);
    }
#endif

    /* Initialize dirpaths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0], 0);
#endif
	fprintf(stderr, "%s: Unable to obtain AFS server directory.\n", argv[0]);
	exit(2);
    }

    /* some path inits */
    bozo_fileName = AFSDIR_SERVER_BOZCONF_FILEPATH;

    /* initialize the list of dirpaths that the bosserver has
     * an interest in monitoring */
    initBosEntryStats();

#if defined(AFS_SGI_ENV)
    /* offer some protection if AFS isn't loaded */
    if (syscall(AFS_SYSCALL, AFSOP_ENDLOG) < 0 && errno == ENOPKG) {
	printf("bosserver: AFS doesn't appear to be configured in O.S..\n");
	exit(1);
    }
#endif

    /* parse cmd line */
    for(code=1;code<argc;code++) {
	if (strcmp(argv[code], "-noauth")==0) {
	    /* set noauth flag */
	    noAuth = 1;
	}
	else if (strcmp(argv[code], "-log")==0) {
	    /* set extra logging flag */
	    DoLogging = 1;
	}
	else if (strcmp(argv[code], "-enable_peer_stats")==0) {
	    rx_enablePeerRPCStats();
	}
	else if (strcmp(argv[code], "-enable_process_stats")==0) {
	    rx_enableProcessRPCStats();
	}
	else {

	    /* hack to support help flag */

	    printf("Usage: bosserver [-noauth] [-log] "
		   /* "[-enable_peer_stats] [-enable_process_stats] " */
		   "[-help]\n");
	    fflush(stdout);

	    exit(0);
	}
    }

#ifndef AFS_NT40_ENV
    if (geteuid() != 0) {
	printf("bosserver: must be run as root.\n");
	exit(1);
    }
#endif

    code = bnode_Init();
    if (code) {
	printf("bosserver: could not init bnode package, code %d\n", code);
	exit(1);
    }

    bnode_Register("fs", &fsbnode_ops, 3);
    bnode_Register("simple", &ezbnode_ops, 1);
    bnode_Register("cron", &cronbnode_ops, 2);

    /* create useful dirs */
    CreateDirs();

    /* chdir to AFS log directory */
    chdir(AFSDIR_SERVER_LOGS_DIRPATH);

    fputs(AFS_GOVERNMENT_MESSAGE, stdout);
    fflush(stdout);

    /* go into the background and remove our controlling tty */

#ifndef AFS_NT40_ENV
    background();
#endif /* ! AFS_NT40_ENV */

    /* switch to logging information to the BosLog file */
    strcpy(namebuf, AFSDIR_BOZLOG_FILE);
    strcat(namebuf, ".old");
    renamefile(AFSDIR_BOZLOG_FILE, namebuf);	/* try rename first */
    bozo_logFile = fopen(AFSDIR_BOZLOG_FILE, "a");
    if (!bozo_logFile) {
	printf("bosserver: can't initialize log file (%s).\n",
	       AFSDIR_SERVER_BOZLOG_FILEPATH);
	exit(1);
    }

    /* keep log closed normally, so can be removed */

    fclose(bozo_logFile);

    /* Write current state of directory permissions to log file */
    DirAccessOK ();

    for (i=0;i<10;i++) {
	code = rx_Init(htons(AFSCONF_NANNYPORT));
	if (code) {
	    bozo_Log("can't initialize rx: code=%d\n",code);
	    sleep(3);
	}
	else break;
    }
    if (i>=10)
	exit(code);

    code = LWP_CreateProcess(BozoDaemon, BOZO_LWP_STACKSIZE, /* priority */ 1,
			     /* parm */0, "bozo-the-clown", &bozo_pid);

    /* try to read the key from the config file */
    tdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
    if (!tdir) {
	/* try to create local cell config file */
	struct afsconf_cell tcell;
	strcpy(tcell.name, "localcell");
	tcell.numServers = 1;
	code = gethostname(tcell.hostName[0], MAXHOSTCHARS);
	if (code) {
	    bozo_Log("failed to get hostname, code %d\n", errno);
	    exit(1);
	}
	if (tcell.hostName[0][0] == 0) {
	    bozo_Log("host name not set, can't start\n");
	    bozo_Log("try the 'hostname' command\n");
	    exit(1);
	}
	bzero(tcell.hostAddr, sizeof(tcell.hostAddr));	/* not computed */
	code = afsconf_SetCellInfo(bozo_confdir, AFSDIR_SERVER_ETC_DIRPATH, &tcell);
	if (code) {
	    bozo_Log("could not create cell database in '%s' (code %d), quitting\n", AFSDIR_SERVER_ETC_DIRPATH, code);
	    exit(1);
	}
	tdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
	if (!tdir) {
	    bozo_Log("failed to open newly-created cell database, quitting\n");
	    exit(1);
	}
    }

    /* read init file, starting up programs */
    if (code=ReadBozoFile(0)) {
	bozo_Log("bosserver: Something is wrong (%d) with the bos configuration file %s; aborting\n", code, AFSDIR_SERVER_BOZCONF_FILEPATH);
	exit(code);
    }

    /* opened the cell databse */
    bozo_confdir = tdir;
    code = afsconf_GetKey(tdir, 999, &tkey);

    /* allow super users to manage RX statistics */
    rx_SetRxStatUserOk(bozo_rxstat_userok);

    /* have bcrypt key now */

    afsconf_SetNoAuthFlag(tdir, noAuth);

    bozo_rxsc[0] = (struct rx_securityClass *) rxnull_NewServerSecurityObject();
    bozo_rxsc[1] = (struct rx_securityClass *) 0;
    bozo_rxsc[2] = (struct rx_securityClass *) rxkad_NewServerSecurityObject(
					 0, tdir, afsconf_GetKey, (char *) 0);

    /* These two lines disallow jumbograms */
    rx_maxReceiveSize = OLD_MAX_PACKET_SIZE;
    rxi_nSendFrags = rxi_nRecvFrags = 1;

    tservice = rx_NewService(/* port */ 0, /* service id */ 1, 
		  /*service name */ "bozo", /* security classes */ bozo_rxsc,
		  /* numb sec classes */ 3, BOZO_ExecuteRequest);
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, 4);
    rx_SetStackSize(tservice, BOZO_LWP_STACKSIZE); /* so gethostbyname works (in cell stuff) */

    tservice = rx_NewService(0, RX_STATS_SERVICE_ID, "rpcstats", bozo_rxsc,
		  3, RXSTATS_ExecuteRequest);
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, 4);
    rx_StartServer(1);	    /* donate this process */
}

bozo_Log(a,b,c,d,e,f)
char *a, *b, *c, *d, *e, *f; {
    char tdate[26];
    time_t myTime;

    myTime = time(0);
    strcpy(tdate, ctime(&myTime));	/* copy out of static area asap */
    tdate[24] = ':';

    /* log normally closed, so can be removed */

    bozo_logFile=fopen(AFSDIR_SERVER_BOZLOG_FILEPATH, "a");
    if(bozo_logFile == NULL)
    {
	printf("bosserver: WARNING: problem with %s", AFSDIR_SERVER_BOZLOG_FILEPATH);
	fflush(stdout);
    }

    if (bozo_logFile) {
	fprintf(bozo_logFile, "%s ", tdate);
	fprintf(bozo_logFile, a, b, c, d, e, f);
	fflush(bozo_logFile);
    }
    else {
	printf("%s ", tdate);
	printf(a, b, c, d, e, f);
    }

    /* close so rm BosLog works */

    fclose(bozo_logFile);
}
