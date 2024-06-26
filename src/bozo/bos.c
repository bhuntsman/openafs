/* Copyright (C) 1998 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include <afs/param.h>
#include <afs/stds.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <io.h>
#include <fcntl.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <strings.h>
#endif /* AFS_NT40_ENV */
#include <string.h>
#include <afs/procmgmt.h>  /* signal(), kill(), wait(), etc. */
#include <time.h>
#include "bnode.h"
#include <errno.h>
#include <afs/afsutil.h>
#include <afs/cellconfig.h>
#include <rx/rx.h>
#include <sys/stat.h>
#include <rx/xdr.h>
#include <afs/auth.h>
#include <rx/rxkad.h>
#include <afs/cellconfig.h>
#include <stdio.h>
#include <afs/cmd.h>
#include <afs/com_err.h>
#include <ubik.h>
#include <afs/ktime.h>



extern char *volutil_PartitionName();
extern struct rx_call *rx_NewCall();
extern char *volutil_PartitionName();
extern struct hostent *hostutil_GetHostByName();

static IStatServer();
static DoStat();

#include "bosint.h"
#include "../permit_xprt.h"

/* dummy routine for the audit work.  It should do nothing since audits */
/* occur at the server level and bos is not a server. */
osi_audit() {return 0;}

/* keep those lines small */
static char *em(acode)
afs_int32 acode; {
    if (acode == -1)
	return "communications failure (-1)";
    else if (acode == -3)
	return "communications timeout (-3)";
    else return (char *) error_message(acode);
}

/* get partition id from a name */
static afs_int32 GetPartitionID(aname)
char *aname; {
    register char tc;
    char ascii[3];

    tc = *aname;
    if (tc == 0) return -1;	/* unknown */
    /* numbers go straight through */
    if (tc >= '0' && tc <= '9') {
	return atoi(aname);
    }
    /* otherwise check for vicepa or /vicepa, or just plain "a" */
    ascii[2] = 0;
    if (strlen(aname) <= 2) {
	strcpy(ascii, aname);
    }
    else if (!strncmp(aname, "/vicep", 6)) {
	strncpy(ascii, aname+6, 2);
    }
    else if (!strncmp(aname, "vicep", 5)) {
	strncpy(ascii, aname+5, 2);
    }
    else return	-1;	/* bad partition name */
    /* now partitions are named /vicepa ... /vicepz, /vicepaa, /vicepab,
     * .../vicepzz, and are numbered from 0.  Do the appropriate conversion */
    if (ascii[1] == 0) {
	/* one char name, 0..25 */
	if (ascii[0] <	'a' || ascii[0]	> 'z')	return -1;  /* wrongo */
	return ascii[0] - 'a';
    }
    else {
	/* two char name, 26 .. <whatever> */
	if (ascii[0] <	'a' || ascii[0]	> 'z')	return -1;  /* wrongo */
	if (ascii[1] <	'a' || ascii[1]	> 'z')	return -1;  /* just as bad */
	return (ascii[0] - 'a') * 26 + (ascii[1] - 'a') + 26;
    }
}

/* make ctime easier to use */
static char *DateOf(atime)
afs_int32 atime; {
    static char tbuffer[30];
    register char *tp;
    tp=ctime((time_t *)&atime);
    if (tp) {
	strcpy(tbuffer, tp);
	tbuffer[24] = 0;    /* get rid of new line */
    }
    else
	strcpy(tbuffer, "BAD TIME");
    return tbuffer;
}

/* global stuff from main for communicating with GetConn */
static struct rx_securityClass *sc[3];
static int scIndex;

/* use the syntax descr to get a connection, authenticated appropriately.
 * aencrypt is set if we want to encrypt the data on the wire.
 */
static struct rx_connection *GetConn(as, aencrypt)
int aencrypt;
struct cmd_syndesc *as; {
    struct hostent *th;
    char *hostname;
    register afs_int32 code;
    register struct rx_connection *tconn;
    afs_int32 addr;
    register struct afsconf_dir *tdir;
    int encryptLevel;
    struct ktc_principal sname;
    struct ktc_token ttoken;
    int localauth;
    const char *confdir;

    hostname = as->parms[0].items->data;
    th = hostutil_GetHostByName(hostname);
    if (!th) {
	printf("bos: can't find address for host '%s'\n", hostname);
	exit(1);
    }
    bcopy(th->h_addr, &addr, sizeof(afs_int32));

    /* get tokens for making authenticated connections */
    localauth = (as->parms[14].items != 0);
    confdir = (localauth ? AFSDIR_SERVER_ETC_DIRPATH : AFSDIR_CLIENT_ETC_DIRPATH);
    tdir = afsconf_Open (confdir);
    if (tdir) {
	struct afsconf_cell info;
	char *tname;

	if (as->parms[12].items) tname = as->parms[12].items->data;
	else tname = (char *) 0;
	/* next call expands cell name abbrevs for us and handles looking up
         * local cell */
	code = afsconf_GetCellInfo(tdir, tname, (char *) 0, &info);
	if (code) {
	    com_err ("bos", code, "(can't find cell '%s' in cell database)",
		   (tname? tname : "<default>"));
	    exit(1);
	}
	else strcpy(sname.cell, info.name);
    }
    else {
	printf("bos: can't open cell database (%s)\n", confdir);
	exit(1);
    }
    sname.instance[0] = 0;
    strcpy(sname.name, "afs");
    sc[0] = (struct rx_securityClass *) rxnull_NewClientSecurityObject();
    sc[1] = 0;
    sc[2] = 0;
    scIndex = 0;

    if (!as->parms[13].items) {	/* not -noauth */
	if (as->parms[14].items) { /* -localauth */
	    code = afsconf_GetLatestKey (tdir, 0,0);
	    if (code) com_err ("bos", code, "(getting key from local KeyFile)");
	    else {
		if (aencrypt)
		    code = afsconf_ClientAuthSecure (tdir, &sc[2], &scIndex);
		else
		    code = afsconf_ClientAuth (tdir, &sc[2], &scIndex);
		if (code) com_err ("bos", code, "(calling ClientAuth)");
		else if (scIndex != 2)	/* this shouldn't happen */
		    sc[scIndex] = sc[2];
	    }
	} else { /* not -localauth, check for tickets */
	    code = ktc_GetToken(&sname, &ttoken, sizeof(ttoken), (char *)0);
	    if (code == 0) {
		/* have tickets, will travel */
		if (ttoken.kvno >= 0 && ttoken.kvno <= 255) ;
		else {
		    fprintf (stderr,
			     "bos: funny kvno (%d) in ticket, proceeding\n",
			     ttoken.kvno);
		}
		/* kerberos tix */
		if (aencrypt) encryptLevel = rxkad_crypt;
		else encryptLevel = rxkad_clear;
		sc[2] = (struct rx_securityClass *)
		    rxkad_NewClientSecurityObject
			(encryptLevel, &ttoken.sessionKey,
			 ttoken.kvno, ttoken.ticketLen, ttoken.ticket);
		scIndex = 2;
	    } else com_err ("bos", code, "(getting tickets)");
	}
	if ((scIndex == 0) || (sc[scIndex] == 0)) {
	    fprintf (stderr, "bos: running unauthenticated\n");
	    scIndex = 0;
	}
    }
    tconn = rx_NewConnection (addr, htons(AFSCONF_NANNYPORT), 1,
			      sc[scIndex], scIndex);
    if (!tconn) {
	fprintf (stderr, "bos: could not create rx connection\n");
	exit(1);
    }
    rxs_Release (sc[scIndex]);

    return tconn;
}

static SetAuth(as)
struct cmd_syndesc *as; {
    register afs_int32 code;
    register struct rx_connection *tconn;
    afs_int32 flag;
    register char *tp;
    
    tconn = GetConn(as, 0);
    tp = as->parms[1].items->data;
    if (strcmp(tp, "on")==0) flag = 0;	/* auth req.: noauthflag is false */
    else if (strcmp(tp, "off") == 0) flag = 1;
    else {
	printf("bos: illegal authentication specifier '%s', must be 'off' or 'on'.\n", tp);
	return 1;
    }
    code = BOZO_SetNoAuthFlag(tconn, flag);
    if (code) com_err ("bos", code, "(failed to set authentication flag)");
    return 0;
}

/* take a name (e.g. foo/bar, and a dir e.g. /usr/afs/bin, and construct
 * /usr/afs/bin/bar */
static ComputeDestDir(aname, adir, aresult, alen)
char *aname, *adir, *aresult;
afs_int32 alen; {
    register char *tp;

    strcpy(aresult, adir);
    tp = rindex(aname, '/');
    if (!tp) {
	/* no '/' in name */
	strcat(aresult, "/");
	strcat(aresult, aname);
    }
    else {
	/* tp points at the / character */
	strcat(aresult, tp);
    }
    return 0;
}

/* copy data from fd afd to rx call acall */
static CopyBytes(afd, acall)
int afd;
register struct rx_call *acall; {
    register afs_int32 code;
    register afs_int32 len;
    char tbuffer[256];

    while(1) {
	len = read(afd, tbuffer, sizeof(tbuffer));
	if (len < 0) return errno;
	if (len == 0) return 0;    /* all done */
	code = rx_Write(acall, tbuffer, len);
	if (code != len) return -1;
    }
}

static Prune(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    register struct rx_connection *tconn;
    register afs_int32 flags;
    
    tconn = GetConn(as, 0);
    flags = 0;
    if (as->parms[1].items) flags |= BOZO_PRUNEBAK;
    if (as->parms[2].items) flags |= BOZO_PRUNEOLD;
    if (as->parms[3].items) flags |= BOZO_PRUNECORE;
    if (as->parms[4].items) flags |= 0xff;
    code = BOZO_Prune(tconn, flags);
    if (code) com_err ("bos", code, "(failed to prune server files)");
    return code;
}

static Exec(as)
register struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register afs_int32 code;

    tconn = GetConn(as, 0);
    code = BOZO_Exec(tconn, as->parms[1].items->data);
    if (code) printf("bos: failed to execute command (%s)\n",
		     em(code));
    return code;
}

static GetDate(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    char tbuffer[256];
    char destDir[256];
    afs_int32 time, bakTime, oldTime;
    register struct rx_connection *tconn;
    register struct cmd_item *ti;

    tconn = GetConn(as, 0);
    if (!as->parms[1].items) {
	printf("bos: no files to check\n");
	return 1;
    }

    /* compute dest dir or file; default MUST be canonical form of dir path */
    if (as->parms[2].items) strcpy(destDir, as->parms[2].items->data);
    else strcpy(destDir, AFSDIR_CANONICAL_SERVER_BIN_DIRPATH);

    for(ti=as->parms[1].items; ti; ti=ti->next) {
	/* check date for each file */
	ComputeDestDir(ti->data, destDir, tbuffer, sizeof(tbuffer));
	code = BOZO_GetDates(tconn, tbuffer, &time, &bakTime, &oldTime);
	if (code) {
	    printf("bos: failed to check date on %s (%s)\n", ti->data,
		   em(code));
	    return 1;
	}
	else {
	    printf("File %s ", tbuffer);
	    if (time == 0) printf("does not exist, ");
	    else printf("dated %s, ", DateOf(time));
	    if (bakTime == 0) printf("no .BAK file, ");
	    else printf(".BAK file dated %s, ", DateOf(bakTime));
	    if (oldTime == 0) printf("no .OLD file.");
	    else printf(".OLD file dated %s.", DateOf(oldTime));
	    printf("\n");
	}
    }
}

static UnInstall(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    char tbuffer[256];
    char destDir[256];
    register struct cmd_item *ti;
    register struct rx_connection *tconn;

    tconn = GetConn(as, 0);
    if (!as->parms[1].items) {
	printf("bos: no files to uninstall\n");
	return 1;
    }

    /* compute dest dir or file; default MUST be canonical form of dir path */
    if (as->parms[2].items) strcpy(destDir, as->parms[2].items->data);
    else strcpy(destDir, AFSDIR_CANONICAL_SERVER_BIN_DIRPATH);

    for(ti=as->parms[1].items; ti; ti=ti->next) {
	/* uninstall each file */
	ComputeDestDir(ti->data, destDir, tbuffer, sizeof(tbuffer));
	code = BOZO_UnInstall(tconn, tbuffer);
	if (code) {
	    printf("bos: failed to uninstall %s (%s)\n", ti->data,
		   em(code));
	    return 1;
	}
	else printf("bos: uninstalled file %s\n", ti->data);
    }
    return 0;
}

static afs_int32 GetServerGoal(aconn, aname)
char *aname;
struct rx_connection *aconn; {
    char buffer[500];
    char *tp;
    register afs_int32 code;
    struct bozo_status istatus;

    tp = buffer;
    code = BOZO_GetInstanceInfo(aconn, aname, &tp, &istatus);
    if (code) {
	printf("bos: failed to get instance info for '%s' (%s)\n",
	       aname, em(code));
	/* if we can't get the answer, assume its running */
	return BSTAT_NORMAL; 
    }
    if (istatus.goal == 0)
	return BSTAT_SHUTDOWN;
    else
	return BSTAT_NORMAL;
}

static Install(as)
struct cmd_syndesc *as; {
    struct rx_connection *tconn;
    register afs_int32 code;
    register struct cmd_item *ti;
    struct stat tstat;
    char tbuffer[256];
    int fd;
    struct rx_call *tcall;
    char destDir[256];

    tconn = GetConn(as, 0);
    if (!as->parms[1].items) {
	printf("bos: no files to install\n");
	return 1;
    }

    /* compute dest dir or file; default MUST be canonical form of dir path */
    if (as->parms[2].items) strcpy(destDir, as->parms[2].items->data);
    else strcpy(destDir, AFSDIR_CANONICAL_SERVER_BIN_DIRPATH);

    for(ti=as->parms[1].items; ti; ti=ti->next) {
	/* install each file */
	fd = open(ti->data, O_RDONLY);
	if (fd < 0) {
	    /* better to quit on error than continue for install command */
	    printf("bos: can't find file '%s', quitting\n", ti->data);
	    return 1;
	}
	code = fstat(fd, &tstat);
	if (code) {
	    printf("bos: failed to stat file %s, errno is %d\n", ti->data,
		   errno);
	    return 1;
	}
	/* compute destination dir */
	ComputeDestDir(ti->data, destDir, tbuffer, sizeof(tbuffer));
	tcall = rx_NewCall(tconn);
	code = StartBOZO_Install(tcall, tbuffer, tstat.st_size, (afs_int32)tstat.st_mode,
				 tstat.st_mtime);
	if (code == 0) {
	    code = CopyBytes(fd, tcall);
	}
	code = rx_EndCall(tcall, code);
	if (code) {
	    printf("bos: failed to install %s (%s)\n", ti->data, em(code));
	    return 1;
	}
	else printf("bos: installed file %s\n", ti->data);
    }
    return 0;
}

static Shutdown(as)
struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register afs_int32 code;
    register struct cmd_item *ti;
    
    tconn = GetConn(as, 0);
    if (as->parms[1].items == 0) {
	code = BOZO_ShutdownAll(tconn);
	if (code) printf("bos: failed to shutdown servers (%s)\n", em(code));
    }
    else {
	for(ti = as->parms[1].items; ti; ti=ti->next) {
	    code = BOZO_SetTStatus(tconn, ti->data, BSTAT_SHUTDOWN);
	    if (code)
		printf("bos: failed to shutdown instance %s (%s)\n",
		       ti->data, em(code));
	}
    }
    if (as->parms[8].items) {
	code = BOZO_WaitAll(tconn);
	if (code)
	    printf("bos: can't wait for processes to shutdown (%s)\n",
		   em(code));
    }
    return 0;
}

static GetRestartCmd(as, arock)
struct cmd_syndesc *as;
char *arock; {
    register afs_int32 code;
    struct ktime generalTime, newBinaryTime;
    char messageBuffer[256];
    struct rx_connection *tconn;
    char *hostp;

    hostp = as->parms[0].items->data;	/* host name for messages */
    tconn = GetConn(as, 0);

    code = BOZO_GetRestartTime(tconn, 1, &generalTime);
    if (code) {
	printf("bos: failed to retrieve restart information (%s)\n", em(code));
	return code;
    }
    code = BOZO_GetRestartTime(tconn, 2, &newBinaryTime);
    if (code) {
	printf("bos: failed to retrieve restart information (%s)\n", em(code));
	return code;
    }

    code = ktime_DisplayString(&generalTime, messageBuffer);
    if (code) {
	printf("bos: failed to decode restart time (%s)\n", em(code));
	return code;
    }
    printf("Server %s restarts %s\n", hostp, messageBuffer);

    code = ktime_DisplayString(&newBinaryTime, messageBuffer);
    if (code) {
	printf("bos: failed to decode restart time (%s)\n", em(code));
	return code;
    }
    printf("Server %s restarts for new binaries %s\n", hostp, messageBuffer);

    /* all done now */
    return 0;
}

static SetRestartCmd(as, arock)
struct cmd_syndesc *as;
char *arock; {
    afs_int32 count;
    register afs_int32 code;
    struct ktime restartTime;
    afs_int32 type;
    struct rx_connection *tconn;

    count = 0;
    tconn = GetConn(as, 0);
    if (as->parms[2].items) {count++; type = 1;}
    if (as->parms[3].items) {count++; type = 2;}
    if (count > 1) {
	printf("bos: can't specify more than one restart time at a time\n");
	return -1;
    }
    if (count == 0) type = 1;	/* by default set general restart time */

    if (code = ktime_ParsePeriodic(as->parms[1].items->data, &restartTime)) {
	printf("bos: failed to parse '%s' as periodic restart time(%s)\n",
	       as->parms[1].items->data, em(code));
	return code;
    }

    code = BOZO_SetRestartTime(tconn, type, &restartTime);
    if (code) {
	printf("bos: failed to set restart time at server (%s)\n", em(code));
	return code;
    }
    return 0;
}

static Startup(as)
struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register afs_int32 code;
    register struct cmd_item *ti;
    
    tconn = GetConn(as, 0);
    if (as->parms[1].items == 0) {
	code = BOZO_StartupAll(tconn);
	if (code)
	    printf("bos: failed to startup servers (%s)\n", em(code));
    }
    else {
	for(ti = as->parms[1].items; ti; ti=ti->next) {
	    code = BOZO_SetTStatus(tconn, ti->data, BSTAT_NORMAL);
	    if (code)
		printf("bos: failed to start instance %s (%s)\n",
		       ti->data, em(code));
	}
    }
    return 0;
}

static Restart(as)
struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register afs_int32 code;
    register struct cmd_item *ti;
    
    tconn = GetConn(as, 0);
    if (as->parms[2].items) {
	/* this is really a rebozo command */
	if (as->parms[1].items) {
	    /* specified specific things to restart, can't do this at the same
             * time */
	    printf("bos: can't specify both '-bos' and specific servers to restart.\n");
	    return 1;
	}
	/* otherwise do a rebozo */
	code = BOZO_ReBozo(tconn);
	if (code)
	    printf("bos: failed to restart bosserver (%s)\n", em(code));
	return code;
    }
    if (as->parms[1].items == 0) {
	if (as->parms[3].items) {		/* '-all' */
	    code = BOZO_RestartAll(tconn);
	    if (code) printf("bos: failed to restart servers (%s)\n", em(code));
	} else
	    printf("bos: To restart all processes please specify '-all'\n");
    }
    else {
	if (as->parms[3].items) {
	    printf("bos: Can't use '-all' along with individual instances\n");
	} else {
	    for(ti = as->parms[1].items; ti; ti=ti->next) {
		code = BOZO_Restart(tconn, ti->data);
		if (code)
		    printf("bos: failed to restart instance %s (%s)\n", ti->data,
			   em(code));
	    }
	}
    }
    return 0;
}

static SetCellName(as)
struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register afs_int32 code;
    
    tconn = GetConn(as, 0);
    code = BOZO_SetCellName(tconn, as->parms[1].items->data);
    if (code) printf("bos: failed to set cell (%s)\n", em(code));
    return 0;
}

static AddHost(as)
register struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register afs_int32 code;
    register struct cmd_item *ti;
    
    tconn = GetConn(as, 0);
    for(ti = as->parms[1].items; ti; ti=ti->next) {
	code = BOZO_AddCellHost(tconn, ti->data);
	if (code)
	    printf("bos: failed to add host %s (%s)\n", ti->data, em(code));
    }
    return 0;
}

static RemoveHost(as)
register struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register afs_int32 code;
    register struct cmd_item *ti;
    
    tconn = GetConn(as, 0);
    for(ti=as->parms[1].items; ti; ti=ti->next) {
	code = BOZO_DeleteCellHost(tconn, ti->data);
	if (code)
	    printf("bos: failed to delete host %s (%s)\n", ti->data, em(code));
    }
    return 0;
}

static ListHosts(as)
register struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register afs_int32 code;
    char tbuffer[256];
    char *tp;
    register afs_int32 i;

    tp = tbuffer;
    tconn = GetConn(as, 0);
    code = BOZO_GetCellName(tconn, &tp);
    if (code) {
	printf("bos: failed to get cell name (%s)\n", em(code));
	exit(1);
    }
    printf("Cell name is %s\n", tbuffer);
    for(i=0;;i++) {
	code = BOZO_GetCellHost(tconn, i, &tp);
	if (code == BZDOM) break;
	if (code != 0) {
	    printf("bos: failed to get cell host %d (%s)\n", i, em(code));
	    exit(1);
	}
	printf("    Host %d is %s\n", i+1, tbuffer);
    }
    return 0;
}

static AddKey(as)
register struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register afs_int32 code;
    struct ktc_encryptionKey tkey;
    afs_int32 temp;
    char *tcell;
    char cellBuffer[256];
    char buf[BUFSIZ], ver[BUFSIZ];
    
    tconn = GetConn(as, 1);
    bzero(&tkey, sizeof(struct ktc_encryptionKey));

	if(as->parms[1].items)
		strcpy(buf,as->parms[1].items->data);
	else
	{
		/* prompt for key */
		code=des_read_pw_string(buf,sizeof(buf),"input key: ",0);
		if(code || strlen(buf)==0)
		{
			fprintf(stderr,"Bad key: \n");
			exit(1);
		}
		code=des_read_pw_string(ver,sizeof(ver),"Retype input key: ",0);
		if(code || strlen(ver)==0)
		{
			fprintf(stderr,"Bad key: \n");
			exit(1);
		}
		if (strcmp (ver, buf) != 0) {
		    fprintf (stderr, "\nInput key mismatch\n");
		    exit(1);
		}

	}

    temp = atoi(as->parms[2].items->data);
    if (temp == 999) {
	/* bcrypt key */
/*
	strcpy((char *)&tkey, as->parms[1].items->data);
*/
	strcpy((char *)&tkey, buf);
    }
    else {    /* kerberos key */
	if (as->parms[12].items) {
	    strcpy(cellBuffer, as->parms[12].items->data);

	    /* string to key needs upper-case cell names */

	    /* I don't believe this is true.  The string to key function
             * actually expands the cell name, then LOWER-CASES it.  Perhaps it
             * didn't use to??? */
	    ucstring(cellBuffer, cellBuffer, strlen(cellBuffer));
	    tcell = cellBuffer;
	}
	else tcell = (char *) 0;    /* no cell specified, use current */
/*
	ka_StringToKey(as->parms[1].items->data, tcell, &tkey);
*/
	ka_StringToKey(buf,tcell,&tkey);
    }
    tconn = GetConn(as, 1);
    code = BOZO_AddKey(tconn, temp, &tkey);
    if (code) {
	printf("bos: failed to set key %d (%s)\n", temp, em(code));
	exit(1);
    }
    return 0;
}

static RemoveKey(as)
register struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register afs_int32 code;
    afs_int32 temp;
    register struct cmd_item *ti;
    
    tconn = GetConn(as, 0);
    for(ti=as->parms[1].items; ti; ti=ti->next) {
	temp = atoi(ti->data);
	code = BOZO_DeleteKey(tconn, temp);
	if (code) {
	    printf("bos: failed to delete key %d (%s)\n", temp, em(code));
	    exit(1);
	}
    }
    return 0;
}

static ListKeys(as)
  IN register struct cmd_syndesc *as;
{
    register struct rx_connection *tconn;
    register afs_int32 code;
    struct ktc_encryptionKey tkey;
    afs_int32 kvno;
    struct bozo_keyInfo keyInfo;
    int everWorked;
    register afs_int32 i;
    
    tconn = GetConn(as, 1);
    everWorked = 0;
    for(i=0;;i++) {
	code = BOZO_ListKeys(tconn, i, &kvno, &tkey, &keyInfo);
	if (code) break;
	everWorked = 1;
	/* first check if key is returned */
	if ((!ka_KeyIsZero (&tkey, sizeof(tkey))) && (as->parms[1].items)) {
	    printf ("key %d is '", kvno);
	    ka_PrintBytes (&tkey, sizeof(tkey));
	    printf ("'\n");
	} else {
	    if (keyInfo.keyCheckSum == 0) /* shouldn't happen */
		printf ("key version is %d\n", kvno);
	    else
		printf ("key %d has cksum %u\n", kvno, keyInfo.keyCheckSum);
	}
    }
    if (everWorked) {
	printf("Keys last changed on %s.\n", DateOf(keyInfo.mod_sec));
    }
    if (code != BZDOM)
	printf("bos: %s error encountered while listing keys\n", em(code));
    else
	printf("All done.\n");
    return 0;
}

static AddSUser(as)
register struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register afs_int32 code;
    int failed;
    register struct cmd_item *ti;

    failed = 0;
    tconn = GetConn(as, 0);
    for(ti=as->parms[1].items; ti; ti=ti->next) {
	code = BOZO_AddSUser(tconn, ti->data);
	if (code) {
	    printf("bos: failed to add user '%s' (%s)\n", ti->data, em(code));
	    failed = 1;
	}
    }
    return failed;
}

static RemoveSUser(as)
register struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register struct cmd_item *ti;
    register afs_int32 code;
    int failed;

    failed = 0;
    tconn = GetConn(as, 0);
    for(ti=as->parms[1].items; ti; ti=ti->next) {
	code = BOZO_DeleteSUser(tconn, ti->data);
	if (code) {
	    printf("bos: failed to delete user '%s', ", ti->data);
	    if (code == ENOENT)
		printf("(no such user)\n");
	    else
		printf("(%s)\n", em(code));
	    failed = 1;
	}
    }
    return failed;
}

#define	NPERLINE    10	    /* dudes to print per line */
static ListSUsers(as)
register struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register int i;
    register afs_int32 code;
    char tbuffer[256];
    char *tp;
    int lastNL, printGreeting;
    
    tconn = GetConn(as, 0);
    lastNL = 0;
    printGreeting = 1;
    for(i=0;;i++) {
	tp = tbuffer;
	code = BOZO_ListSUsers(tconn, i, &tp);
	if (code) break;
	if (printGreeting) {
	    printGreeting = 0;	/* delay until after first call succeeds */
	    printf("SUsers are: ");
	}
	printf("%s ", tbuffer);
	if ((i % NPERLINE) == NPERLINE-1) {
	    printf("\n");
	    lastNL = 1;
	}
	else lastNL = 0;
    }
    if (code != 1) {
	/* a real error code, instead of scanned past end */
	printf("bos: failed to retrieve super-user list (%s)\n", em(code));
	return code;
    }
    if (lastNL == 0) printf("\n");
    return 0;
}

static StatServer(as)
register struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register afs_int32 code;
    register int i;
    char ibuffer[BOZO_BSSIZE];
    char *tp;
    int int32p;
    
    /* int32p==1 is obsolete, smaller, printout */
    int32p = (as->parms[2].items != 0? 2 : 0);

    /* no parms does something pretty different */
    if (as->parms[1].items) return IStatServer(as, int32p);

    tconn = GetConn(as, 0);
    for(i=0;;i++) {
	/* for each instance */
	tp = ibuffer;
	code = BOZO_EnumerateInstance(tconn, i, &tp);
	if (code == BZDOM) break;
	if (code) {
	    printf("bos: failed to contact host's bosserver (%s).\n",
		   em(code));
	    break;
	}
	DoStat(ibuffer,	tconn, int32p, (i==0)); /* print status line */
    }
    return 0;
}

static CreateServer(as)
register struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register afs_int32 code;
    char *parms[6];
    register struct cmd_item *ti;
    register int i;
    char *type, *name, *notifier=NONOTIFIER;

    tconn = GetConn(as, 0);
    for(i=0;i<6;i++) parms[i] = "";
    for(i=0,ti=as->parms[3].items; (ti && i<6); ti=ti->next,i++) {
	parms[i] = ti->data;
    }
    name = as->parms[1].items->data;
    type = as->parms[2].items->data;
    if (ti = as->parms[4].items) {
	notifier = ti->data;
    }
    code = BOZO_CreateBnode(tconn, type, name, parms[0], parms[1], parms[2],
			    parms[3], parms[4], notifier);
    if (code) {
	printf("bos: failed to create new server instance %s of type '%s' (%s)\n", 
	       name, type, em(code));
    }
    return code;
}

static DeleteServer(as)
register struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register afs_int32 code;
    register struct cmd_item *ti;

    code = 0;
    tconn = GetConn(as, 0);
    for(ti=as->parms[1].items; ti; ti=ti->next) {
	code = BOZO_DeleteBnode(tconn, ti->data);
	if (code) {
	    if (code == BZBUSY)
		printf("bos: can't delete running instance '%s'\n", ti->data);
	    else
		printf("bos: failed to delete instance '%s' (%s)\n",
		       ti->data, em(code));
	}
    }
    return code;
}

static StartServer(as)
register struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register afs_int32 code;
    register struct cmd_item *ti;

    code = 0;
    tconn = GetConn(as, 0);
    for(ti=as->parms[1].items; ti; ti=ti->next) {
	code = BOZO_SetStatus(tconn, ti->data, BSTAT_NORMAL);
	if (code)
	    printf("bos: failed to start instance '%s' (%s)\n",
		   ti->data, em(code));
    }
    return code;
}

static StopServer(as)
register struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register afs_int32 code;
    register struct cmd_item *ti;

    code = 0;
    tconn = GetConn(as, 0);
    for(ti=as->parms[1].items; ti; ti=ti->next) {
	code = BOZO_SetStatus(tconn, ti->data, BSTAT_SHUTDOWN);
	if (code)
	    printf("bos: failed to change stop instance '%s' (%s)\n",
		   ti->data, em(code));
    }
    if (as->parms[8].items) {
	code = BOZO_WaitAll(tconn);
	if (code)
	    printf("bos: can't wait for processes to shutdown (%s)\n",
		   em(code));
    }
    return code;
}

static DoSalvage(aconn, aparm1, aparm2, aoutName, showlog,parallel,atmpDir,orphans)
struct rx_connection *aconn;
char *aoutName;
char *aparm1;
char *aparm2; 
afs_int32 showlog;
char *parallel;
char *atmpDir;
char *orphans;
{
    register afs_int32 code;
    char *parms[6];
    char buffer;
    char tbuffer[BOZO_BSSIZE];
    struct bozo_status istatus;
    struct rx_call *tcall;
    char *tp;
    FILE *outFile;
    int closeIt;
    char partName[20];	/* canonical name for partition */
    afs_int32 partNumber;
    char *notifier = NONOTIFIER;

    /* if a partition was specified, canonicalize the name, since
       the salvager has a stupid partition ID parser */
    if (aparm1) {
	partNumber = volutil_GetPartitionID(aparm1);
	if (partNumber < 0) {
	    printf("bos: could not parse partition ID '%s'\n", aparm1);
	    return EINVAL;
	}
	tp = volutil_PartitionName(partNumber);
	if (!tp) {
	    printf("bos: internal error parsing partition ID '%s'\n", aparm1);
	    return EINVAL;
	}
	strcpy(partName, tp);
    }
    else partName[0] = 0;

    /* open the file name */
    if (aoutName) {
	outFile = fopen(aoutName, "w");
	if (!outFile) {
	    printf("bos: can't open specified SalvageLog file '%s'\n",
		   aoutName);
	    return ENOENT;
	}
	closeIt = 1;	/* close this file later */
    }
    else {
	outFile = stdout;
	closeIt = 0;	/* don't close this file later */
    }

    for(code=2;code<6;code++) parms[code] = "";
    if (!aparm2) aparm2 = "";
    /* MUST pass canonical (wire-format) salvager path to bosserver */
    strncpy(tbuffer, AFSDIR_CANONICAL_SERVER_SALVAGER_FILEPATH, BOZO_BSSIZE);
    if (*aparm2 != 0) {
        if ( (strlen(tbuffer) + 1 + strlen(partName) + 1 + strlen(aparm2) + 1) > BOZO_BSSIZE ) {
	   printf("bos: command line too big\n");
	   return(E2BIG);
	}
	strcat(tbuffer, " ");
	strcat(tbuffer, partName);
	strcat(tbuffer, " ");
	strcat(tbuffer, aparm2);
    } else {
        if ( (strlen(tbuffer) + 4 + strlen(partName) + 1) > BOZO_BSSIZE ) {
	   printf("bos: command line too big\n");
	   return(E2BIG);
	}
	strcat (tbuffer, " -f ");
	strcat (tbuffer, partName);
    }

    /* add the parallel option if given */
    if (parallel != (char *)0) {
        if ( (strlen(tbuffer) + 11 + strlen(parallel) + 1) > BOZO_BSSIZE ) {
	   printf("bos: command line too big\n");
	   return(E2BIG);
	}
	strcat(tbuffer, " -parallel ");
	strcat(tbuffer, parallel);
    }

    /* add the tmpdir option if given */
    if (atmpDir != (char *)0) {
        if ( (strlen(tbuffer) + 9 + strlen(atmpDir) + 1) > BOZO_BSSIZE ) {
	   printf("bos: command line too big\n");
	   return(E2BIG);
	}
	strcat(tbuffer, " -tmpdir ");
	strcat(tbuffer, atmpDir);
    }

    /* add the orphans option if given */
    if (orphans != (char *)0) {
        if ( (strlen(tbuffer) + 10 + strlen(orphans) + 1) > BOZO_BSSIZE ) {
	   printf("bos: command line too big\n");
	   return(E2BIG);
	}
	strcat(tbuffer, " -orphans ");
	strcat(tbuffer, orphans);
    }

    parms[0] = tbuffer;
    parms[1] = "now";	    /* when to do it */
    code = BOZO_CreateBnode(aconn, "cron", "salvage-tmp", parms[0], parms[1],
			    parms[2], parms[3], parms[4], notifier);
    if (code) goto done;
    /* now wait for bnode to disappear */
    while (1) {
	IOMGR_Sleep(5);
	tp = tbuffer;
	code = BOZO_GetInstanceInfo(aconn, "salvage-tmp", &tp, &istatus);
	if (code) break;
	printf("bos: waiting for salvage to complete.\n");
    }
    if (code != BZNOENT) {
	printf("bos: salvage failed (%s)\n", em(code));
	goto done;
    }

    /* now print the log file to the output file */
    printf("bos: salvage completed\n");
    if (aoutName || showlog) {
	fprintf(outFile, "SalvageLog:\n");
	tcall = rx_NewCall(aconn);
	/* MUST pass canonical (wire-format) salvager log path to bosserver */
	code = StartBOZO_GetLog(tcall, AFSDIR_CANONICAL_SERVER_SLVGLOG_FILEPATH);
	if (code) {
	    rx_EndCall(tcall, code);
	    goto done;
	}
	/* copy data */
	while(1) {
	    code = rx_Read(tcall, &buffer, 1);
	    if (code != 1) break;
	    putc(buffer, outFile);
	    if (buffer == 0) break;	/* the end delimeter */
	}
	code = rx_EndCall(tcall, 0);
	/* fall through into cleanup code */
    }

  done:
    if (closeIt && outFile) fclose(outFile);
    return code;
}

static GetLogCmd(as)
register struct cmd_syndesc *as; {
    struct rx_connection *tconn;
    register struct rx_call *tcall;
    register afs_int32 code;
    char buffer;
    int error;

    printf("Fetching log file '%s'...\n", as->parms[1].items->data);
    tconn = GetConn(as, 0);
    tcall = rx_NewCall(tconn);
    code = StartBOZO_GetLog(tcall, as->parms[1].items->data);
    if (code) {
	rx_EndCall(tcall, code);
	goto done;
    }
    /* copy data */
    error = 0;
    while(1) {
	code = rx_Read(tcall, &buffer, 1);
	if (code != 1) {
	    error = EIO;
	    break;
	}
	if (buffer == 0) break;	/* the end delimeter */
	putchar(buffer);
    }
    code = rx_EndCall(tcall, error);
    /* fall through into cleanup code */

  done:
    if (code)
	com_err("bos", code, "(while reading log)");
    return code;
}

static SalvageCmd(as)
struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register afs_int32 code, rc;
    char *outName;
    char tname[65];
    afs_int32 newID;
    extern struct ubik_client *cstruct;
    afs_int32 curGoal, showlog = 0;
    char *parallel;
    char *tmpDir;
    char *orphans;
    
    /* parm 0 is machine name, 1 is partition, 2 is volume, 3 is -all flag */
    tconn = GetConn(as, 0);

    /* we can do a volume, a partition or the whole thing, but not mixtures
     * thereof */
    if (!as->parms[1].items && as->parms[2].items) {
	printf("bos: must specify partition to salvage individual volume.\n");
	return -1;
    }
    if (as->parms[5].items && as->parms[3].items) {
	printf("bos: can not specify both -file and -showlog.\n");
	return -1;
    }
    if (as->parms[4].items && (as->parms[1].items || as->parms[2].items)) {
	printf("bos: can not specify -all with other flags.\n");
	return -1;
    }

    /* get the output file name, if any */
    if (as->parms[3].items)
	outName = as->parms[3].items->data;
    else
	outName = (char *) 0;

    if (as->parms[5].items)
	showlog = 1;

    /* parallel option */
    parallel = (char *) 0;
    if (as->parms[6].items)
	parallel = as->parms[6].items->data;

    /* get the tmpdir filename if any */
    tmpDir = (char *)0;
    if (as->parms[7].items)
	tmpDir = as->parms[7].items->data;

    /* -orphans option */
    orphans = (char *)0;
    if (as->parms[8].items)
	orphans = as->parms[8].items->data;

    if (as->parms[4].items) {
	/* salvage whole enchilada */
	curGoal = GetServerGoal(tconn, "fs");
	if (curGoal == BSTAT_NORMAL) {
	    printf("bos: shutting down fs.\n");
	    code = BOZO_SetTStatus(tconn, "fs", BSTAT_SHUTDOWN);
	    if (code) {
		printf("bos: failed to stop 'fs' (%s)\n", em(code));
		return code;
	    }
	    code = BOZO_WaitAll(tconn);	/* wait for shutdown to complete */
	    if (code)
		printf("bos: failed to wait for file server shutdown, continuing.\n");
	}
	/* now do the salvage operation */
	printf("Starting salvage.\n");
	rc = DoSalvage(tconn, (char *) 0, (char *) 0, outName, showlog,parallel,tmpDir,orphans);
	if (rc) printf("bos: failed to start 'salvager' (%s)\n", em(rc));
	if (curGoal == BSTAT_NORMAL) {
	    printf("bos: restarting fs.\n");
	    code = BOZO_SetTStatus(tconn, "fs", BSTAT_NORMAL);
	    if (code) {
		printf("bos: failed to restart 'fs' (%s)\n", em(code));
		return code;
	    }
	}
	if (rc) return rc;
    }
    else if (!as->parms[2].items) {
	if (!as->parms[1].items) {
	    printf("bos: must specify -all switch to salvage all partitions.\n");
	    return -1;
	}
	if (volutil_GetPartitionID(as->parms[1].items->data) < 0) {
	    /* can't parse volume ID, so complain before shutting down
	     * file server.
	     */
	    printf("bos: can't interpret %s as partition ID.\n",
		   as->parms[1].items->data);
	    return -1;
	}
	curGoal = GetServerGoal(tconn, "fs");
	/* salvage a whole partition (specified by parms[1]) */
	if (curGoal == BSTAT_NORMAL) {
	    printf("bos: shutting down fs.\n");
	    code = BOZO_SetTStatus(tconn, "fs", BSTAT_SHUTDOWN);
	    if (code) {
		printf("bos: can't stop 'fs' (%s)\n", em(code));
		return code;
	    }
	    code = BOZO_WaitAll(tconn);	/* wait for shutdown to complete */
	    if (code)
		printf("bos: failed to wait for file server shutdown, continuing.\n");
	}
	/* now do the salvage operation */
	printf("Starting salvage.\n");
	rc = DoSalvage(tconn, as->parms[1].items->data, (char *) 0,
		       outName, showlog,parallel,tmpDir,orphans);
	if (rc) printf("bos: failed to start 'salvager' (%s)\n", em(rc));
	if (curGoal == BSTAT_NORMAL) {
	    printf("bos: restarting fs.\n");
	    code = BOZO_SetTStatus(tconn, "fs", BSTAT_NORMAL);
	    if (code) {
		printf("bos: failed to restart 'fs' (%s)\n", em(code));
		return code;
	    }
	}
	if (rc) return rc;
    }
    else {
	/* salvage individual volume (don't shutdown fs first), just use
         * single-shot cron bnode.  Must leave server running when using this
         * option, since salvager will ask file server for the volume */
	char *tmpname;
	afs_int32 err;
	const char *confdir;
	int localauth;

	if (as->parms[12].items) tmpname = as->parms[12].items->data;
	else tmpname = (char *) 0;

	localauth = (as->parms[14].items != 0);
	confdir = (localauth ? AFSDIR_SERVER_ETC_DIRPATH : AFSDIR_CLIENT_ETC_DIRPATH);
	code = vsu_ClientInit(/* noauth */ 1, confdir, tmpname,
			      /* server auth */ 0, &cstruct, (int (*)()) 0);
	if (code == 0) {
	    newID = vsu_GetVolumeID(as->parms[2].items->data, cstruct, &err);
	    if (newID == 0) {
		printf("bos: can't interpret %s as volume name or ID\n", as->parms[2].items->data);
		return -1;
	    }
	    sprintf(tname, "%u", newID);
	}
	else {
	    printf("bos: can't initialize volume system client (code %d), trying anyway.\n",
		   as->parms[2].items->data, code);
	    strncpy(tname, as->parms[2].items->data, sizeof(tname));
	}
	if (volutil_GetPartitionID(as->parms[1].items->data) < 0) {
	    /* can't parse volume ID, so complain before shutting down
	     * file server.
	     */
	    printf("bos: can't interpret %s as partition ID.\n",
		   as->parms[1].items->data);
	    return -1;
	}
	printf("Starting salvage.\n");
	rc = DoSalvage(tconn, as->parms[1].items->data, tname, outName,
		       showlog,parallel,tmpDir,orphans);
	if (rc) {
	   printf("bos: failed to start 'salvager' (%s)\n", em(rc));
	   return rc;
	}
    }
    return 0;
}

static IStatServer(as, int32p)
int int32p;
register struct cmd_syndesc *as; {
    register struct rx_connection *tconn;
    register struct cmd_item *ti;
    int firstTime = 1;

    tconn = GetConn(as, 0);
    for (ti=as->parms[1].items; ti; ti=ti->next) {
	DoStat(ti->data, tconn, int32p, firstTime);
	firstTime = 0;
    }
    return 0;
}

static DoStat (aname, aconn, aint32p, firstTime)
  IN char *aname;
  IN register struct rx_connection *aconn;
  IN int aint32p;
  IN int firstTime;			/* true iff first instance in cmd */
{
    afs_int32 temp;
    char buffer[500];
    register afs_int32 code;
    register afs_int32 i;
    struct bozo_status istatus;
    char *tp;
    char *is1, *is2, *is3, *is4;	/* instance strings */

    tp = buffer;
    code = BOZO_GetInstanceInfo(aconn, aname, &tp, &istatus);
    if (code) {
	printf("bos: failed to get instance info for '%s' (%s)\n",
	       aname, em(code));
	return -1;
    }
    if (firstTime && aint32p && (istatus.flags & BOZO_BADDIRACCESS))
	printf ("Bosserver reports inappropriate access on server directories\n");
    printf("Instance %s, ", aname);
    if (aint32p) printf("(type is %s) ", buffer);
    if (istatus.fileGoal == istatus.goal) {
	if (!istatus.goal) printf("disabled, ");
    }
    else {
	if (istatus.fileGoal) printf("temporarily disabled, ");
	else printf("temporarily enabled, ");
    }

    if (istatus.flags & BOZO_ERRORSTOP)
	printf("stopped for too many errors, ");
    if (istatus.flags & BOZO_HASCORE)
	printf("has core file, ");

    tp = buffer;
    code = BOZO_GetStatus(aconn, aname, &temp, &tp);
    if (code)
	printf("bos: failed to get status for instance '%s' (%s)\n",
	       aname, em(code));
    else {
	printf("currently ", aname);
	if (temp == BSTAT_NORMAL) printf("running normally.\n");
	else if (temp == BSTAT_SHUTDOWN) printf("shutdown.\n");
	else if (temp == BSTAT_STARTINGUP) printf("starting up.\n");
	else if (temp == BSTAT_SHUTTINGDOWN) printf("shutting down.\n");
	if (buffer[0] != 0) {
	    printf("    Auxiliary status is: %s.\n", buffer);
	}
    }

    /* are we done yet? */
    if (!aint32p) return 0;

    if (istatus.procStartTime)
	printf("    Process last started at %s (%d proc starts)\n",
	       DateOf(istatus.procStartTime), istatus.procStarts);
    if (istatus.lastAnyExit) {
	printf("    Last exit at %s\n", DateOf(istatus.lastAnyExit));
    }
    if (istatus.lastErrorExit) {
	is1 = is2 = is3 = is4 = (char *) 0;
	printf("    Last error exit at %s, ", DateOf(istatus.lastErrorExit));
	code = BOZO_GetInstanceStrings(aconn, aname, &is1, &is2, &is3, &is4);
	/* don't complain about failing call, since could simply mean
	 * interface mismatch.
	 */
	if (code == 0) {
	    if (*is1 != 0) {
		/* non-null instance string */
		printf("by %s, ", is1);
	    }
	    free(is1);
	    free(is2);
	    free(is3);
	    free(is4);
	}
	if (istatus.errorSignal) {
	    if (istatus.errorSignal == SIGTERM)
		printf("due to shutdown request\n");
	    else
		printf("due to signal %d\n", istatus.errorSignal);
	}
	else
	    printf("by exiting with code %d\n", istatus.errorCode);
    }
    
    if (aint32p > 1) {
	/* try to display all the parms */
	for(i=0;;i++) {
	    tp = buffer;
	    code = BOZO_GetInstanceParm(aconn, aname, i, &tp);
	    if (code) break;
	    printf("    Command %d is '%s'\n", i+1, buffer);
	}
	tp = buffer;
	code = BOZO_GetInstanceParm(aconn, aname, 999, &tp);
	if (!code) {
	    /* Any type of failure is treated as not having a notifier program */
	    printf("    Notifier  is '%s'\n", buffer);
	}
	printf("\n");
    }
    return 0;
}

static void add_std_args (ts)
  register struct cmd_syndesc *ts;
{
    cmd_Seek(ts, 12);
    /* 12 */ cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    /* 13 */cmd_AddParm(ts, "-noauth", CMD_FLAG, CMD_OPTIONAL,
			"don't authenticate");
    /* 14 */ cmd_AddParm (ts, "-localauth", CMD_FLAG, CMD_OPTIONAL,
			  "create tickets from KeyFile");
}
  
#include "AFS_component_version_number.c"

main(argc, argv)
  int argc;
  char **argv;
{
    register afs_int32 code;
    register struct cmd_syndesc *ts;
    extern int afsconf_SawCell;

#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a 
     * crash (i.e. core is generated) we can include the user's data section 
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;
    
    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
    sigaction(SIGABRT, &nsa, NULL);
#endif

    /* start up rx */
    code = rx_Init(0);
    if (code) {
	printf("bos: could not initialize rx (%s)\n", em(code));
	exit(1);
    }

    /* To resolve a AFSCELL environment "Note: ..." problem: Because bos calls ka_Init (why?) before any
     * checkup on the existance of a "-cell" option on the particular bos command we don't print the
     * message if a cell is passed in. Luckily the rest of the programs don't call these adhoc routines
     * and things work fine. Note that if the "-cell" isn't passed then we're still ok since later on
     * the proper routine (afsconf_GetCellInfo) is going to be called to do the right thing.
     */
    afsconf_SawCell = 1;	/* Means don't print warning if AFSCELL is set */
    ka_Init(0);
    afsconf_SawCell = 0;	/* Reset it */
    /* don't check error code, since fails sometimes when we're setting up a
     * system */
    initialize_cmd_error_table();
    initialize_bz_error_table();

    ts = cmd_CreateSyntax("start", StartServer, 0, "start running a server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-instance", CMD_LIST, 0, "server process name");
    add_std_args (ts);

    ts = cmd_CreateSyntax("stop", StopServer, 0, "halt a server instance");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-instance", CMD_LIST, 0, "server process name");
    cmd_Seek(ts, 8);
    cmd_AddParm(ts, "-wait", CMD_FLAG, CMD_OPTIONAL,
		"wait for process to stop");
    add_std_args (ts);
    
    ts = cmd_CreateSyntax("status", StatServer, 0,
			  "show server instance status");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-instance", CMD_LIST, CMD_OPTIONAL,
		"server process name");
    cmd_AddParm(ts, "-long", CMD_FLAG, CMD_OPTIONAL, "long status");
    add_std_args (ts);

    ts = cmd_CreateSyntax("shutdown", Shutdown, 0, "shutdown all processes");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-instance", CMD_LIST, CMD_OPTIONAL, "instances");
    cmd_Seek(ts, 8);
    cmd_AddParm(ts, "-wait", CMD_FLAG, CMD_OPTIONAL,
		"wait for process to stop");
    add_std_args (ts);

    ts = cmd_CreateSyntax("startup", Startup, 0, "start all processes");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-instance", CMD_LIST, CMD_OPTIONAL, "instances");
    add_std_args (ts);

    ts = cmd_CreateSyntax("restart", Restart, 0, "restart all processes");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-instance", CMD_LIST, CMD_OPTIONAL, "instances");
    cmd_AddParm(ts, "-bosserver", CMD_FLAG, CMD_OPTIONAL, "restart bosserver");
    cmd_AddParm(ts, "-all", CMD_FLAG, CMD_OPTIONAL, "restart all processes");
    add_std_args (ts);

#ifndef OPBOS

    ts = cmd_CreateSyntax("create", CreateServer, 0,
			  "create a new server instance");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-instance", CMD_SINGLE, 0, "server process name");
    cmd_AddParm(ts, "-type", CMD_SINGLE, 0, "server type");
    cmd_AddParm(ts, "-cmd", CMD_LIST, 0, "command lines");
    cmd_AddParm(ts, "-notifier", CMD_SINGLE, CMD_OPTIONAL, "Notifier program");
    add_std_args (ts);

    ts = cmd_CreateSyntax("delete", DeleteServer, 0,
			  "delete a server instance");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-instance", CMD_LIST, 0, "server process name");
    add_std_args (ts);
    
    ts = cmd_CreateSyntax("adduser", AddSUser, 0,
			  "add users to super-user list");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-user", CMD_LIST, 0, "user names");
    add_std_args (ts);

    ts = cmd_CreateSyntax("removeuser", RemoveSUser, 0,
			  "remove users from super-user list");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-user", CMD_LIST, 0, "user names");
    add_std_args (ts);

    ts = cmd_CreateSyntax("listusers", ListSUsers, 0, "list super-users");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    add_std_args (ts);

    ts = cmd_CreateSyntax("addkey", AddKey, 0,
			   "add keys to key dbase (kvno 999 is bcrypt)");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-key", CMD_SINGLE, CMD_OPTIONAL, "key");
    cmd_AddParm(ts, "-kvno", CMD_SINGLE, 0, "key version number");
    add_std_args (ts);

    ts = cmd_CreateSyntax("removekey", RemoveKey, 0,
			  "remove keys from key dbase");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-kvno", CMD_LIST, 0, "key version number");
    add_std_args (ts);

    ts = cmd_CreateSyntax("listkeys", ListKeys, 0, "list keys");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-showkey", CMD_FLAG, CMD_OPTIONAL, "show the actual key rather than the checksum");
    add_std_args (ts);

    ts = cmd_CreateSyntax("listhosts", ListHosts, 0, "get cell host list");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    add_std_args (ts);
    cmd_CreateAlias(ts, "getcell");

    ts = cmd_CreateSyntax("setcellname", SetCellName, 0, "set cell name");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "cell name");
    add_std_args (ts);

    ts = cmd_CreateSyntax("addhost", AddHost, 0, "add host to cell dbase");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-host", CMD_LIST, 0, "host name");
    add_std_args (ts);

    ts = cmd_CreateSyntax("removehost", RemoveHost, 0,
			  "remove host from cell dbase");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-host", CMD_LIST, 0, "host name");
    add_std_args (ts);

    ts = cmd_CreateSyntax("setauth", SetAuth, 0,
			  "set authentication required flag");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-authrequired", CMD_SINGLE,
		0, "on or off: authentication required for admin requests");
    add_std_args (ts);
    
    ts = cmd_CreateSyntax("install", Install, 0, "install program");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-file", CMD_LIST, 0, "files to install");
    cmd_AddParm(ts, "-dir", CMD_SINGLE, CMD_OPTIONAL, "destination dir");
    add_std_args (ts);

    ts = cmd_CreateSyntax("uninstall", UnInstall, 0, "uninstall program");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-file", CMD_LIST, 0, "files to uninstall");
    cmd_AddParm(ts, "-dir", CMD_SINGLE, CMD_OPTIONAL, "destination dir");
    add_std_args (ts);

    ts = cmd_CreateSyntax("getlog", GetLogCmd, 0, "examine log file");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-file", CMD_SINGLE, 0, "log file to examine");
    add_std_args (ts);

    ts = cmd_CreateSyntax("getdate", GetDate, 0, "get dates for programs");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-file", CMD_LIST, 0, "files to check");
    cmd_AddParm(ts, "-dir", CMD_SINGLE, CMD_OPTIONAL, "destination dir");
    add_std_args (ts);

    ts = cmd_CreateSyntax("exec", Exec, 0, "execute shell command on server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-cmd", CMD_SINGLE, 0, "command to execute");
    add_std_args (ts);

    ts = cmd_CreateSyntax("prune", Prune, 0, "prune server files");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-bak", CMD_FLAG, CMD_OPTIONAL, "delete .BAK files");
    cmd_AddParm(ts, "-old", CMD_FLAG, CMD_OPTIONAL, "delete .OLD files");
    cmd_AddParm(ts, "-core", CMD_FLAG, CMD_OPTIONAL, "delete core files");
    cmd_AddParm(ts, "-all", CMD_FLAG, CMD_OPTIONAL, "delete all junk files");
    add_std_args (ts);

    ts = cmd_CreateSyntax("setrestart", SetRestartCmd, 0, "set restart times");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "machine name");
    cmd_AddParm(ts, "-time", CMD_SINGLE,
		CMD_REQUIRED, "time to restart server");
    cmd_AddParm(ts, "-general", CMD_FLAG,
		CMD_OPTIONAL, "set general restart time");
    cmd_AddParm(ts, "-newbinary", CMD_FLAG,
		CMD_OPTIONAL, "set new binary restart time");
    add_std_args (ts);

    ts = cmd_CreateSyntax("getrestart", GetRestartCmd, 0, "get restart times");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "machine name");
    add_std_args (ts);

    ts = cmd_CreateSyntax("salvage", SalvageCmd, 0,
			  "salvage partition or volumes");
    cmd_AddParm(ts, "-server", CMD_SINGLE, 0, "machine name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE,
		CMD_OPTIONAL, "salvage partition");
    cmd_AddParm(ts, "-volume", CMD_SINGLE,
		CMD_OPTIONAL, "salvage volume number or volume name");
    cmd_AddParm(ts, "-file", CMD_SINGLE,
		CMD_OPTIONAL, "salvage log output file");
    cmd_AddParm(ts, "-all", CMD_FLAG, CMD_OPTIONAL, "salvage whole server");
    cmd_AddParm(ts, "-showlog", CMD_FLAG, CMD_OPTIONAL, "display salvage log");
    cmd_AddParm(ts, "-parallel",CMD_SINGLE, CMD_OPTIONAL, 
 		"# of max parallel partition salvaging");
    cmd_AddParm(ts, "-tmpdir",CMD_SINGLE, CMD_OPTIONAL, 
		"directory to place tmp files");
    cmd_AddParm(ts, "-orphans", CMD_SINGLE, CMD_OPTIONAL, 
		"ignore | remove | attach");
    add_std_args (ts);

#endif

    code = cmd_Dispatch(argc, argv);
    rx_Finalize();
    exit(code);
}
