/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1998
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include <afs/param.h>
#include <sys/types.h>
#include <netinet/in.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include <afs/auth.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <des.h>
#include <rx/rxkad.h>

#include "AFS_component_version_number.c"

main(argc, argv)
int argc;
char **argv; {
    struct ktc_token token;
    struct ktc_principal sname;
    register afs_int32 code;
    struct afsconf_dir *dir;
    afs_int32 now;
    char skey[9];
    char cellName[MAXKTCNAMELEN];
    char session[8];

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
#endif
    if (argc != 3) {
	printf("kseal: usage is 'kseal <username> <server key>\n");
	exit(1);
    }

    /* lookup configuration info */
    dir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
    if (!dir) {
	printf("kseal: can't open config dir (%s)\n", AFSDIR_CLIENT_ETC_DIRPATH);
	exit(1);
    }
    code = afsconf_GetLocalCell(dir, cellName, sizeof(cellName));
    if (code) {
	printf("kseal: failed to get local cell name, code %d\n", code);
	exit(1);
    }

    /* setup key for sealing */
    string_to_key(argv[2], skey);

    now = time(0);
    bcopy(&now,	session, 4);	/* but this is only a test pgm */
    bcopy(&now, session+4, 4);
    code = tkt_MakeTicket(token.ticket, &token.ticketLen, skey, argv[1], "", cellName,
	now-300, now+25*3600, session, /* host */ 0, "afs", "");
    if (code) {
	printf("kseal: could not seal ticket, code %d!\n", code);
	exit(1);
    }
    
    /* now send the ticket to the ticket cache */
    strcpy(sname.name, "afs");
    strcpy(sname.instance, "");
    strcpy(sname.cell, cellName);
    token.startTime = 0;
    token.endTime = 0x7fffffff;
    bcopy(session, &token.sessionKey, 8);
    token.kvno = 0;
    code = ktc_SetToken (&sname, &token, (char *) 0, 0);
    if (code) {
	printf("kseal: could not install newly-sealed ticket, code %d\n", code);
	exit(1);
    }

    /* all done */
    afsconf_Close(dir);
    exit(0);
}
