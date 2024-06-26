
#ifndef lint
#endif

/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

/*
	(Multiple) readers & writers test of LWP stuff.

Created: 11/1/83, J. Rosenberg

*/

#include <afs/param.h>
#ifdef AFS_NT40_ENV
#include <malloc.h>
#include <stdlib.h>
#else
#include <sys/time.h>
extern char *calloc();
#endif
#include <stdio.h>

#include "lwp.h"
#include "lock.h"
#include "preempt.h"
#include <afs/assert.h>

#define DEFAULT_READERS	5

#define STACK_SIZE	(16*1024)

/* The shared queue */
typedef struct QUEUE {
    struct QUEUE	*prev, *next;
    char		*data;
    struct Lock		lock;
} queue;

queue *init()
{
    queue *q;

    q = (queue *) malloc(sizeof(queue));
    q -> prev = q -> next = q;
    return(q);
}

char empty(q)
    queue *q;
{
    return (q->prev == q && q->next == q);
}

void insert(queue *q, char *s)
{
    queue *new;

    new = (queue *) malloc(sizeof(queue));
    new -> data = s;
    new -> prev = q -> prev;
    q -> prev -> next = new;
    q -> prev = new;
    new -> next = q;
}

char *Remove(q)
    queue *q;
{
    queue *old;
    char *s;

    if (empty(q)) {
	printf("Remove from empty queue");
	assert(0);
    }

    old = q -> next;
    q -> next = old -> next;
    q -> next -> prev = q;
    s = old -> data;
    free(old);
    return(s);
}

queue *q;

int asleep;	/* Number of processes sleeping -- used for
		   clean termination */

static int read_process(id)
    int id;
{
    printf("\t[Reader %d]\n", id);
    LWP_DispatchProcess();		/* Just relinquish control for now */

    PRE_PreemptMe();
    for (;;) {
        register int i;

	/* Wait until there is something in the queue */
	asleep++;
	ObtainReadLock(&q->lock);
	while (empty(q)) {
	    ReleaseReadLock(&q->lock);
	    LWP_WaitProcess(q);
	    ObtainReadLock(&q->lock);
	}
	asleep--;
	for (i=0; i<10000; i++) ;
	PRE_BeginCritical();
	printf("[%d: %s]\n", id, Remove(q));
	PRE_EndCritical();
	ReleaseReadLock(&q->lock);
	LWP_DispatchProcess();
    }
    return 0;
}

static int write_process()
{
    static char *messages[] =
    {
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	0
    };
    char **mesg;

    printf("\t[Writer]\n");
    PRE_PreemptMe();

    /* Now loop & write data */
    for (mesg=messages; *mesg!=0; mesg++) {
	ObtainWriteLock(&q->lock);
	insert(q, *mesg);
	ReleaseWriteLock(&q->lock);
	LWP_SignalProcess(q);
    }

    asleep++;
    return 0;
}

/*
	Arguments:
		0:	Unix junk, ignore
		1:	Number of readers to create (default is DEFAULT_READERS)
		2:	# msecs for interrupt (to satisfy Larry)
		3:	Present if lwp_debug to be set
*/

#include "AFS_component_version_number.c"

main(argc, argv)
   int argc; char **argv;
{
    int nreaders, i;
    afs_int32 interval;	/* To satisfy Brad */
    PROCESS *readers;
    PROCESS writer;
    struct timeval tv;

    printf("\n*Readers & Writers*\n\n");
    setbuf(stdout, 0);

    /* Determine # readers */
    if (argc == 1)
	nreaders = DEFAULT_READERS;
    else
	sscanf(*++argv, "%d", &nreaders);
    printf("[There will be %d readers]\n", nreaders);

    interval = (argc >= 3 ? atoi(*++argv)*1000 : 50000);

    if (argc == 4) lwp_debug = 1;
    LWP_InitializeProcessSupport(0, (PROCESS*)&i);
    printf("[Support initialized]\n");
    tv.tv_sec = 0;
    tv.tv_usec = interval;
    PRE_InitPreempt(&tv);

    /* Initialize queue */
    q = init();

    /* Initialize lock */
    Lock_Init(&q->lock);

    asleep = 0;
    /* Now create readers */
    printf("[Creating Readers...\n");
    readers = (PROCESS *) calloc(nreaders, sizeof(PROCESS));
    for (i=0; i<nreaders; i++)
	LWP_CreateProcess(read_process, STACK_SIZE, 0, (void*)i, "Reader",
			  &readers[i]);
    printf("done]\n");

    printf("\t[Creating Writer...\n");
    LWP_CreateProcess(write_process, STACK_SIZE, 1, 0, "Writer", &writer);
    printf("done]\n");

    /* Now loop until everyone's done */
    while (asleep != nreaders+1) LWP_DispatchProcess();
    /* Destroy the readers */
    for (i=nreaders-1; i>=0; i--) LWP_DestroyProcess(readers[i]);
    printf("\n*Exiting*\n");
}

