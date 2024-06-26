/* Copyright (C) 1990, 1989 Transarc Corporation - All rights reserved */
/*
 * For copyright information, see IPL which you accepted in order to
 * download this software.
 *
 */

/* These two needed for rxgen output to work */
#include <afs/param.h>
#include <afs/stds.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif

#include <sys/types.h>
#include <rx/xdr.h>

#include <lock.h>
#include <ubik.h>
#ifndef AFS_NT40_ENV
#include <pwd.h>
#else
#include <WINNT/afsevent.h>
#endif
#include <afs/cellconfig.h>
#include <afs/com_err.h>

#include "kauth.h"
#include "kautils.h"


int main (
  int   argc,
  char *argv[])
{
    afs_int32 code;
    char *ap[25];
    int   i;
    char *whoami = argv[0];

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
    sigaction(SIGABRT, &nsa, NULL);
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    initialize_cmd_error_table();
    initialize_ktc_error_table();
    initialize_ka_error_table();
    initialize_acfg_error_table();
    initialize_u_error_table();

#ifdef AFS_NT40_ENV
    /* initialize winsock */
    if (afs_winsockInit()<0) {
      fprintf(stderr, "%s: Couldn't initialize winsock.\n", whoami);
      exit(1);
    }
#endif

    code = ka_Init(0);
    if (code) {
	com_err (whoami, code, "Can't get cell info");
	exit (1);
    }

    /* if there are no arguments or if the first argument is "-cell" or if the
       first argument is clearly a username (it contains a '.' or '@') assume
       the interactive command and splice it into the arglist. */

    ap[0] = argv[0];
    ap[1] = "interactive";
    if (argc == 1) code = ka_AdminInteractive (2, ap);
    else if ((strncmp (argv[1], "-admin_username", strlen(argv[1])) == 0) ||
	     (strncmp (argv[1], "-password_for_admin", strlen(argv[1])) == 0) ||
	     (strncmp (argv[1], "-cell", strlen(argv[1])) == 0) ||
	     (strncmp (argv[1], "-servers", strlen(argv[1])) == 0) ||
	     (strncmp (argv[1], "-noauth", strlen(argv[1])) == 0) ||
	     (strpbrk (argv[1], "@.") != 0)) {
	for (i=1; i<argc; i++) ap[i+1] = argv[i];
	code = ka_AdminInteractive (argc+1, ap);
    }
    else code = ka_AdminInteractive (argc, argv);

    rx_Finalize();
    exit (code != 0);
}
