%{

/*
 * Copyright (C) 1990 Transarc Corporation
 * Licensed Materials - Property of Transarc
 * All Rights Reserved
 *
 * lex.l:
 *	Lexical definitions recognized by package, the AFS
 *	workstation configuration facility.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "globals.h"
#include "package.h"

#include "y.tab.h"

char *stash();
char *emalloc();

%}

/* definitions */
COM	#[^\n]*
W	[ \t]
D	[0-9]
L	[A-Za-z]
P	("./"|"../"|"/")[^ \t\n#=\^\|\(\)\{\};]+
%%

{W}*{COM}	{ dbgprint((stderr, "COMMENT\n"));
		 return(COMMENT); }

^{W}*\n		{ dbgprint((stderr, "BLANKLINE\n"));
		 return(BLANKLINE); }

{W}+		{ dbgprint((stderr, "WHITESPACE\n"));
		 return(WHITESPACE); }

^{W}*{L}		{
		  dbgprint((stderr, "FILETYPE %c\n", yytext[0]));
		  switch(yytext[0])
		    {
		      case 'F' :
			return(REGTYPE); 	/* regular file */
			break;
		      case 'D' :
			return(DIRTYPE); 	/* directory */
			break;
		      case 'L' :
			return(LNKTYPE);	/* symbolic link */
			break;
		      case 'B' :
			return(BLKTYPE);	/* block device */
			break;
		      case 'C' :
			return(CHRTYPE); 	/* character device */
			break;
		      case 'S' :
			return(SOCKTYPE);	/* socket */
			break;
		      case 'P' :
			return(PIPETYPE);	/* named pipe */
			break;
		    }
		}

{L}		{ dbgprint((stderr, "LETTER %c\n", yytext[0]));
		 yylval.chval = yytext[0]; return(LETTER); }

{P}		{ dbgprint((stderr, "PATHNAME %s\n", yytext));
		 yylval.strval = stash(yytext); return(PATHNAME); }

\/		{ dbgprint((stderr, "PATHNAME %s\n", yytext));
		 yylval.strval = stash(yytext); return(PATHNAME); }

{D}		{ dbgprint((stderr, "DIGIT %c\n", yytext[0]));
		 yylval.usval = (u_short)(yytext[0] - '0'); return(DIGIT); }

\n		{ dbgprint((stderr, "NEWLINE\n"));
		 return(NEWLINE);}

%%

char *stash(s)
char *s;
/*
 * Save string away in memory and return a pointer to it.  If
 * no space could be found, quit with a fatal error.
 */
{
    char *strcpy();
    char *ptr;

    ptr = emalloc((unsigned)(strlen(s) + 1));
    return(strcpy(ptr, s));
}
