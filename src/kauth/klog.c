/* Copyright (C) 1991, 1989 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1988, 1989
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

/*
 * Revision 2.6  1991/04/15  10:44:53
 * Initialize "explicit" local variable in false case.
 *
 * Revision 2.5  90/10/02  15:48:55
 * Change -x parm to CMD_FLAG.
 * 
 * Revision 2.4  90/10/01  11:28:01
 * Cleaups for HC compiler.
 * Add the -x option back as a noop for existing programs/scripts.
 * 
 * Revision 2.3  90/09/26  14:19:23
 * Remove support for -x option.  This controlled checking the /etc/passwd file.
 *   We no longer do this under an circumstances.
 * 
 * Revision 2.2  90/08/09  08:41:24
 * Check for lifetimes longer than 30 days and reject them.  Otherwise such
 *   bogus lifetimes make the kaserver think the password is wrong.
 * 
 * Revision 2.1  90/08/07  19:11:58
 * Start with clean version to sync test and dev trees.
 * */
/* See RCS log for older history. */

    /* These two needed for rxgen output to work */
#include <afs/param.h>
#include <afs/stds.h>
#include <sys/types.h>
#include <rx/xdr.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif

#include <lock.h>
#include <ubik.h>

#include <stdio.h>
#include <pwd.h>
#include <afs/com_err.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/cmd.h>
#include "kauth.h"
#include "kautils.h"
#include "assert.h"


/* This code borrowed heavily from the previous version of log.  Here is the
   intro comment for that program: */

/*
	log -- tell the Andrew Cache Manager your password
	5 June 1985
	modified
	February 1986

	Further modified in August 1987 to understand cell IDs.
 */

/* Current Usage:
     klog [principal [password]] [-t] [-c cellname] [-servers <hostlist>]

     where:
       principal is of the form 'name' or 'name@cell' which provides the
	  cellname.  See the -c option below.
       password is the user's password.  This form is NOT recommended for
	  interactive users.
       -t advises klog to write a Kerberos style ticket file in /tmp.
       -c identifies cellname as the cell in which authentication is to take
	  place.
       -servers allows the explicit specification of the hosts providing
	  authentication services for the cell being used for authentication.
 */

#define KLOGEXIT(code) assert(!code || code >= KAMINERROR); \
                       rx_Finalize(); \
                       (!code ? exit(0) : exit((code)-KAMINERROR+1))
extern int CommandProc (
  struct cmd_syndesc *as,
  char *arock
);

static int zero_argc;
static char **zero_argv;

int osi_audit(void)
{
    return 0;
}

int main (
  int   argc,
  char *argv[])
{
    struct cmd_syndesc *ts;
    afs_int32 code;
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
    zero_argc = argc;
    zero_argv = argv;

    ts = cmd_CreateSyntax((char *) 0, CommandProc, 0, "obtain Kerberos authentication");

#define aXFLAG 0
#define aPRINCIPAL 1
#define aPASSWORD 2
#define aCELL 3
#define aSERVERS 4
#define aPIPE 5
#define aSILENT 6
#define aLIFETIME 7
#define aSETPAG 8
#define aTMP 9


    cmd_AddParm(ts, "-x", CMD_FLAG, CMD_OPTIONAL, "(obsolete, noop)");
    cmd_Seek(ts, aPRINCIPAL);
    cmd_AddParm(ts, "-principal", CMD_SINGLE, CMD_OPTIONAL, "user name");
    cmd_AddParm(ts, "-password", CMD_SINGLE, CMD_OPTIONAL, "user's password");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_AddParm(ts, "-servers", CMD_LIST, CMD_OPTIONAL, "explicit list of servers");
    cmd_AddParm(ts, "-pipe", CMD_FLAG, CMD_OPTIONAL, "read password from stdin");
    cmd_AddParm(ts, "-silent", CMD_FLAG, CMD_OPTIONAL, "silent operation");
    cmd_AddParm(ts, "-lifetime", CMD_SINGLE, CMD_OPTIONAL, "ticket lifetime in hh[:mm[:ss]]");
    cmd_AddParm(ts, "-setpag", CMD_FLAG, CMD_OPTIONAL, "Create a new setpag before authenticating");
    cmd_AddParm(ts, "-tmp", CMD_FLAG, CMD_OPTIONAL, "write Kerberos-style ticket file in /tmp");

    code = cmd_Dispatch(argc, argv);
    KLOGEXIT(code);
}

static char *getpipepass(void)
{
    static char gpbuf[BUFSIZ];
    /* read a password from stdin, stop on \n or eof */
    register int i, tc;
    bzero(gpbuf, sizeof(gpbuf));
    for(i=0; i<(sizeof(gpbuf)-1); i++) {
	tc = fgetc(stdin);
	if (tc == '\n' || tc == EOF) break;
	gpbuf[i] = tc;
    }
    return gpbuf;
}

int CommandProc (
  struct cmd_syndesc *as,
  char *arock)
{
    char  name[MAXKTCNAMELEN];
    char  instance[MAXKTCNAMELEN];
    char  cell[MAXKTCREALMLEN];
    char  realm[MAXKTCREALMLEN];
    afs_int32  serverList[MAXSERVERS];
    char *lcell;			/* local cellname */
    char  lrealm[MAXKTCREALMLEN];	/* uppercase copy of local cellname */
    int	  code;
    int   i, dosetpag;
    Date  lifetime;			/* requested ticket lifetime */

    struct passwd pwent;
    struct passwd *pw = &pwent;
    struct passwd *lclpw = &pwent;
    char passwd[BUFSIZ];

    static char	rn[] = "klog";		/*Routine name*/
    static int Pipe = 0;		/* reading from a pipe */
    static int Silent = 0;		/* Don't want error messages */

    int explicit;			/* servers specified explicitly */
    int local;				/* explicit cell is same a local one */
    int	foundPassword =	0;		/*Not yet, anyway*/
    int	foundExplicitCell = 0;		/*Not yet, anyway*/
    int writeTicketFile = 0;          /* write ticket file to /tmp */
    afs_int32 password_expires = -1;

    char *reason;			/* string describing errors */

    /* blow away command line arguments */
    for (i=1; i<zero_argc; i++) bzero (zero_argv[i], strlen(zero_argv[i]));
    zero_argc = 0;

    /* first determine quiet flag based on -silent switch */
    Silent = (as->parms[aSILENT].items ? 1 : 0);
    Pipe = (as->parms[aPIPE].items ? 1 : 0);

    /* Determine if we should also do a setpag based on -setpag switch */
    dosetpag = (as->parms[aSETPAG].items ? 1 : 0);

    if (as->parms[aTMP].items) {
       writeTicketFile = 1;
    }

    if (as->parms[aCELL].items) {
	/*
	 * cell name explicitly mentioned; take it in if no other cell name
	 * has already been specified and if the name actually appears.  If
	 * the given cell name differs from our own, we don't do a lookup.
	 */
	foundExplicitCell = 1;
	strncpy (realm, as->parms[aCELL].items->data, sizeof(realm));
	/* XXX the following is just a hack to handle the afscell environment XXX */
	(void) afsconf_GetCellInfo((struct afsconf_dir *)0, realm, 0, (struct afsconf_cell *)0);
    }

    code = ka_Init(0);
    if (code ||
	!(lcell = ka_LocalCell())) {
      nocell:
	if (!Silent) 
	   com_err (rn, code, "Can't get local cell name!");
	KLOGEXIT(code);
    }
    if (code = ka_CellToRealm (lcell, lrealm, 0)) goto nocell;

    strcpy (instance, "");

    /* Parse our arguments. */

    if (as->parms[aCELL].items) {
	/*
	 * cell name explicitly mentioned; take it in if no other cell name
	 * has already been specified and if the name actually appears.  If
	 * the given cell name differs from our own, we don't do a lookup.
	 */
	foundExplicitCell = 1;
	strncpy (realm, as->parms[aCELL].items->data, sizeof(realm));
    }

    if (as->parms[aSERVERS].items) {
	/* explicit server list */
	int i;
	struct cmd_item *ip;
	char *ap[MAXSERVERS+2];

	for (ip = as->parms[aSERVERS].items, i=2; ip; ip=ip->next, i++)
	    ap[i] = ip->data;
	ap[0] = "";
	ap[1] = "-servers";
	code = ubik_ParseClientList(i, ap, serverList);
	if (code) {
	    if (!Silent) {
	       com_err (rn, code, "could not parse server list");
	     }
	    return code;
	}
	explicit = 1;
    } else explicit = 0;

    if (as->parms[aPRINCIPAL].items) {
	ka_ParseLoginName (as->parms[aPRINCIPAL].items->data,
			   name, instance, cell);
	if (strlen (instance) > 0)
	    if (!Silent) {
		fprintf (stderr, 
			 "Non-null instance (%s) may cause strange behavior.\n",
			 instance);
	      }
	if (strlen(cell) > 0) {
	    if (foundExplicitCell) {
		if (!Silent) {
		    fprintf (stderr, 
		          "%s: May not specify an explicit cell twice.\n", rn);
		  }
		return -1;
	    }
	    foundExplicitCell = 1;
	    strncpy (realm, cell, sizeof(realm));
	}
	lclpw->pw_name = name;
    } else {
	/* No explicit name provided: use Unix uid. */
	pw = getpwuid(getuid());
	if (pw == 0) {
	    if (!Silent) {
		fprintf (stderr, "Can't figure out your name in local cell %s from your user id.\n", lcell);
		fprintf (stderr, "Try providing the user name.\n");
	    }
	    KLOGEXIT( KABADARGUMENT );
	}
	lclpw = pw;
    }

    if (as->parms[aPASSWORD].items) {
	/*
	 * Current argument is the desired password string.  Remember it in
	 * our local buffer, and zero out the argument string - anyone can
	 * see it there with ps!
	 */
	foundPassword = 1;
	strncpy (passwd, as->parms[aPASSWORD].items->data, sizeof(passwd));
	bzero (as->parms[aPASSWORD].items->data,
	       strlen(as->parms[aPASSWORD].items->data));
    }

    if (as->parms[aLIFETIME].items) {
	char *life = as->parms[aLIFETIME].items->data;
	char *sp;			/* string ptr to rest of life */
	lifetime = 3600*strtol (life, &sp, 0); /* hours */
	if (sp == life) {
bad_lifetime:
	    if (!Silent) fprintf (stderr, "%s: translating '%s' to lifetime failed\n",
			       rn, life);
	    return KABADARGUMENT;
	}
	if (*sp == ':') {
	    life = sp+1;		/* skip the colon */
	    lifetime += 60*strtol (life, &sp, 0); /* minutes */
	    if (sp == life) goto bad_lifetime;
	    if (*sp == ':') {
		life = sp+1;
		lifetime += strtol (life, &sp, 0); /* seconds */
		if (sp == life) goto bad_lifetime;
		if (*sp) goto bad_lifetime;
	    } else if (*sp) goto bad_lifetime;
	} else if (*sp) goto bad_lifetime;
	if (lifetime > MAXKTCTICKETLIFETIME) {
	    if (!Silent) fprintf (stderr, "%s: a lifetime of %.2f hours is too long, must be less than %d.\n", rn, (double)lifetime/3600.0, MAXKTCTICKETLIFETIME/3600);
	    KLOGEXIT( KABADARGUMENT );
	}
    } else lifetime = 0;

    if (!foundExplicitCell) strcpy (realm, lcell);
    if (code = ka_CellToRealm (realm, realm, &local)) {
	if (!Silent) com_err (rn, code, "Can't convert cell to realm");
	KLOGEXIT(code);
    }

    /* Get the password if it wasn't provided. */
    if (!foundPassword) {
	if (Pipe) {
	    strncpy(passwd, getpipepass(), sizeof(passwd));
	}
	else {
	    if (ka_UserReadPassword
		("Password:", passwd, sizeof(passwd), &reason)) {
		fprintf (stderr, "Unable to login because %s.\n", reason);
		KLOGEXIT( KABADARGUMENT );
	    }
	}
    }

    if (explicit) ka_ExplicitCell (realm, serverList);

    code = ka_UserAuthenticateGeneral (KA_USERAUTH_VERSION + (dosetpag ? KA_USERAUTH_DOSETPAG2:0), pw->pw_name,
	     instance, realm, passwd, lifetime, &password_expires, 0, &reason);
    bzero (passwd, sizeof(passwd));
    if (code) {
	if (!Silent) {
	    fprintf (stderr,
		     "Unable to authenticate to AFS because %s.\n", reason);
	  }
	KLOGEXIT( code );
      }

#ifndef AFS_KERBEROS_ENV
    if (writeTicketFile) {
       code = krb_write_ticket_file (realm);
       if (!Silent) {
          if (code) 
              com_err (rn, code, "writing Kerberos ticket file");
          else fprintf (stderr, "Wrote ticket file to /tmp\n");
      }
   }
#endif
 
#ifdef DEBUGEXPIRES
       if (password_expires >= 0) {
	 printf ("password expires at %ld\n", password_expires);
       }
#endif /* DEBUGEXPIRES */

    return 0;
}
