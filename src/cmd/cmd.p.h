
/*
 * (C) COPYRIGHT IBM CORPORATION 1988, 1989
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#ifndef __CMD_INCL__
#define	__CMD_INCL__	    1
#include <afs/param.h>

/* parmdesc types */
#define	CMD_FLAG	1	/* no parms */
#define	CMD_SINGLE	2	/* one parm */
#define	CMD_LIST	3	/* two parms */

/* syndesc flags */
#define	CMD_ALIAS	1	/* this is an alias */
#define CMD_HIDDEN      4       /* A hidden command - similar to CMD_HIDE */

#define CMD_HELPPARM	(CMD_MAXPARMS-1)/* last one is used by -help switch */
#define	CMD_MAXPARMS	64	/* max number of parm types to a cmd line */

/* parse items are here */
struct cmd_item {
    struct cmd_item *next;
    char *data;
};

struct cmd_parmdesc {
    char *name;			/* switch name */
    int	type;			/* flag, single or list */
    struct cmd_item *items;	/* list of cmd items */
    afs_int32 flags;			/* flags */
    char *help;			/* optional help descr */
};

/* cmd_parmdesc flags */
#define	CMD_REQUIRED	    0
#define	CMD_OPTIONAL	    1
#define	CMD_EXPANDS	    2	/* if list, try to eat tokens through eoline, instead of just 1 */
#define CMD_HIDE            4   /* A hidden option */
#define	CMD_PROCESSED	    8

struct cmd_syndesc {
    struct cmd_syndesc *next;	/* next one in system list */
    struct cmd_syndesc *nextAlias;  /* next in alias chain */
    struct cmd_syndesc *aliasOf;    /* back ptr for aliases */
    char *name;		    /* subcommand name */
    char *a0name;	    /* command name from argv[0] */
    char *help;		    /* help description */
    int (*proc)();
    char *rock;
    int	nParms;		    /* number of parms */
    afs_int32 flags;		    /* random flags */
    struct cmd_parmdesc parms[CMD_MAXPARMS];	/* parms themselves */
};

extern struct cmd_syndesc *cmd_CreateSyntax(
  char *namep,
  int (*aprocp)(),
  char *rockp,
  char *helpp
);

extern cmd_SetBeforeProc(
  int (*aproc)(),
  char *arock
);

extern cmd_SetAfterProc(
  int (*aproc)(),
  char *arock
);

extern int cmd_CreateAlias(
  struct cmd_syndesc *as,
  char *aname
);

extern int cmd_Seek(
  struct cmd_syndesc *as,
  int apos
);

extern int cmd_AddParm(
  struct cmd_syndesc *as,
  char *aname,
  int atype,
  afs_int32 aflags,
  char *ahelp
);

extern cmd_Dispatch(
  int argc,
  char **argv
);

extern cmd_FreeArgv(
  char **argv
);

extern cmd_ParseLine(
  char *aline,
  char **argv,
  afs_int32 *an,
  afs_int32 amaxn
);

#endif /* __CMD_INCL__ */
