/* Copyright (C) 1990 Transarc Corporation - All rights reserved */

#include <afs/param.h>
#include <afs/stds.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <stdio.h>
#include "ptserver.h"


#ifdef PR_REMEMBER_TIMES

# include <time.h>


static char *pr_TimeToString (time_t clock)
{   struct tm *tm;
    static char buffer[32];
    static int this_year = 0;
    char year[6];

    if (clock == 0) return "time-not-set  ";
    if (!this_year) {
	time_t now = time(0);
	tm = localtime (&now);
	this_year = tm->tm_year;
    }
    tm = localtime(&clock);
    if (tm->tm_year != this_year)
        strftime (buffer, 32, "%m/%d/%Y %T", tm);
    else
        strftime (buffer, 32, "%m/%d %T", tm);
    return buffer;
}
#endif

#define host(a) (hostOrder ? (a) : ntohl(a))
 
static void PrintEntries (f, hostOrder, indent, e, n)
  FILE *f;
  int hostOrder;			/* structures in host order */
  int indent;
  struct prentry *e;
  int n;
{
    int i;
    int newline;

    newline = 0;
    for (i=0; i<n; i++) {
	if (e->entries[i] == 0) break;

	if (i==0) fprintf (f, "%*sids ", indent, "");
	else if (newline == 0) fprintf (f, "%*s", indent+4, "");

	if (host(e->entries[i]) == PRBADID) fprintf (f, " EMPTY");
	else fprintf (f, "%6d", host(e->entries[i]));
	newline = 1;
	if (i%10 == 9) {
	    fprintf (f, "\n");
	    newline = 0;
	}
	else fprintf (f, " ");
    }
    if (newline) fprintf (f, "\n");
}

int pr_PrintEntry (f, hostOrder, ea, e, indent)
  FILE *f;
  int hostOrder;			/* structures in host order */
  afs_int32 ea;
  struct prentry *e;
  int indent;
{
    int i;

    if (e->cellid) fprintf (f, "cellid == %d\n", host(e->cellid));
    for (i=0; i<sizeof(e->reserved)/sizeof(e->reserved[0]); i++)
	if (e->reserved[i])
	    fprintf (f, "reserved field [%d] not zero: %d\n",
		     i, host(e->reserved[i]));

    fprintf (f, "%*s", indent, "");
    fprintf (f, "Entry at %d: flags 0x%x, id %di, next %d.\n",
	     ea, host(e->flags), host(e->id), host(e->next));
#ifdef PR_REMEMBER_TIMES
    fprintf (f, "%*s", indent, "");
    fprintf (f, "c:%s ", pr_TimeToString(host(e->createTime)));
    fprintf (f, "a:%s ", pr_TimeToString(host(e->addTime)));
    fprintf (f, "r:%s ", pr_TimeToString(host(e->removeTime)));
    fprintf (f, "n:%s\n", pr_TimeToString(host(e->changeTime)));
#endif
    if (host(e->flags) & PRCONT)
	PrintEntries (f, hostOrder, indent, e, COSIZE);
    else { /* regular entry */
	PrintEntries (f, hostOrder, indent, e, PRSIZE);
	fprintf (f, "%*s", indent, "");
	fprintf (f, "hash (id %d name %d).  Owner %di, creator %di\n",
		 host(e->nextID), host(e->nextName),
		 host(e->owner), host(e->creator));
	fprintf (f, "%*s", indent, "");
	fprintf (f, "quota groups %d, foreign users %d.  Mem: %d, inst: %d\n",
		 host(e->ngroups), host(e->nusers),
		 host(e->count), host(e->instance));
	fprintf (f, "%*s", indent, "");
	fprintf (f, "Owned chain %d, next owned %d, inst ptrs(%d %d %d).\n",
		 host(e->owned), host(e->nextOwned),
		 host(e->parent), host(e->sibling), host(e->child));
	fprintf (f, "%*s", indent, "");
	if (strlen (e->name) >= PR_MAXNAMELEN)
	    fprintf (f, "NAME TOO LONG: ");
	fprintf (f, "Name is '%.*s'\n", PR_MAXNAMELEN, e->name);
    }
    return 0;
}
