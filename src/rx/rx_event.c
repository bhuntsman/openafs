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

#ifdef	KERNEL
#include "../afs/param.h"
#ifndef UKERNEL
#include "../afs/afs_osi.h"
#else /* !UKERNEL */
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#endif /* !UKERNEL */
#include "../rx/rx_clock.h"
#include "../rx/rx_queue.h"
#include "../rx/rx_event.h"
#include "../rx/rx_kernel.h"
#include "../rx/rx_kmutex.h"
#ifdef RX_ENABLE_LOCKS
#include "../rx/rx.h"
#endif /* RX_ENABLE_LOCKS */
#include "../rx/rx_globals.h"
#if defined(AFS_SGI_ENV)
#include "../sys/debug.h"
/* These are necessary to get curproc (used by GLOCK asserts) to work. */
#include "../h/proc.h"
#if !defined(AFS_SGI64_ENV) && !defined(UKERNEL)
#include "../h/user.h"
#endif
extern void *osi_Alloc();
#endif
#else /* KERNEL */
#include "afs/param.h"
#include <stdio.h>
#include "rx_clock.h"
#include "rx_queue.h"
#include "rx_event.h"
#include "rx_user.h"
#ifdef AFS_PTHREAD_ENV
#include <rx/rx_pthread.h>
#else
#include "rx_lwp.h"
#endif
#ifdef RX_ENABLE_LOCKS
#include "rx.h"
#endif /* RX_ENABLE_LOCKS */
#include "rx_globals.h"
#ifdef AFS_NT40_ENV
#include <malloc.h>
#endif
#endif /* KERNEL */


/* All event processing is relative to the apparent current time given by clock_GetTime */

/* This should be static, but event_test wants to look at the free list... */
struct rx_queue rxevent_free;	   /* It's somewhat bogus to use a doubly-linked queue for the free list */
struct rx_queue rxepoch_free;	   /* It's somewhat bogus to use a doubly-linked queue for the free list */
static struct rx_queue rxepoch_queue; /* list of waiting epochs */
static int rxevent_allocUnit = 10;   /* Allocation unit (number of event records to allocate at one time) */
static int rxepoch_allocUnit = 10;   /* Allocation unit (number of epoch records to allocate at one time) */
int rxevent_nFree;		   /* Number of free event records */
int rxevent_nPosted;	   /* Current number of posted events */
int rxepoch_nFree;		   /* Number of free epoch records */
static int (*rxevent_ScheduledEarlierEvent)(); /* Proc to call when an event is scheduled that is earlier than all other events */
struct xfreelist { 
    void *mem;
    int size;
    struct xfreelist *next; 
};
static struct xfreelist *xfreemallocs = 0, *xsp = 0;

struct clock rxevent_nextRaiseEvents;	/* Time of next call to raise events */
int rxevent_raiseScheduled;		/* true if raise events is scheduled */

#ifdef RX_ENABLE_LOCKS
#ifdef RX_LOCKS_DB
/* rxdb_fileID is used to identify the lock location, along with line#. */
static int rxdb_fileID = RXDB_FILE_RX_EVENT;
#endif /* RX_LOCKS_DB */
#define RX_ENABLE_LOCKS  1
afs_kmutex_t rxevent_lock;
#endif /* RX_ENABLE_LOCKS */

#ifdef AFS_PTHREAD_ENV
/*
 * This mutex protects the following global variables:
 * rxevent_initialized
 */

#include <assert.h>
pthread_mutex_t rx_event_mutex;
#define LOCK_EV_INIT assert(pthread_mutex_lock(&rx_event_mutex)==0);
#define UNLOCK_EV_INIT assert(pthread_mutex_unlock(&rx_event_mutex)==0);
#else
#define LOCK_EV_INIT
#define UNLOCK_EV_INIT
#endif /* AFS_PTHREAD_ENV */


/* Pass in the number of events to allocate at a time */
int rxevent_initialized = 0;
void
rxevent_Init(nEvents, scheduler)
    int nEvents;
    int (*scheduler)();
{
    LOCK_EV_INIT
    if (rxevent_initialized) {
	UNLOCK_EV_INIT
	return;
    }
    MUTEX_INIT(&rxevent_lock, "rxevent_lock", MUTEX_DEFAULT, 0);
    clock_Init();
    if (nEvents) rxevent_allocUnit = nEvents;
    queue_Init(&rxevent_free);
    queue_Init(&rxepoch_free);
    queue_Init(&rxepoch_queue);
    rxevent_nFree = rxevent_nPosted = 0;
    rxepoch_nFree = 0;
    rxevent_ScheduledEarlierEvent = scheduler;
    rxevent_initialized = 1;
    clock_Zero(&rxevent_nextRaiseEvents);
    rxevent_raiseScheduled = 0;
    UNLOCK_EV_INIT
}

/* Create and initialize new epoch structure */
struct rxepoch *rxepoch_Allocate(struct clock *when)
{
    struct rxepoch *ep;
    int i;

    /* If we are short on free epoch entries, create a block of new oned
     * and add them to the free queue */
    if (queue_IsEmpty(&rxepoch_free)) {
#if    defined(AFS_AIX32_ENV) && defined(KERNEL)
	ep = (struct rxepoch *) rxi_Alloc(sizeof(struct rxepoch));
	queue_Append(&rxepoch_free, &ep[0]), rxepoch_nFree++;
#else
	ep = (struct rxepoch *)
	     osi_Alloc(sizeof(struct rxepoch) * rxepoch_allocUnit);
	xsp = xfreemallocs;
	xfreemallocs = (struct xfreelist *) osi_Alloc(sizeof(struct xfreelist));
	xfreemallocs->mem = (void *)ep;
	xfreemallocs->size = sizeof(struct rxepoch) * rxepoch_allocUnit;
	xfreemallocs->next = xsp;
	for (i = 0; i<rxepoch_allocUnit; i++)
	    queue_Append(&rxepoch_free, &ep[i]), rxepoch_nFree++;
#endif
    }
    ep = queue_First(&rxepoch_free, rxepoch);
    queue_Remove(ep);
    rxepoch_nFree--;
    ep->epochSec = when->sec;
    queue_Init(&ep->events);
    return ep;
}

/* Add the indicated event (function, arg) at the specified clock time.  The
 * "when" argument specifies when "func" should be called, in clock (clock.h)
 * units. */

struct rxevent *rxevent_Post(struct clock *when, void (*func)(),
			     void *arg, void *arg1)
{
    register struct rxevent *ev, *evqe, *evqpr;
    register struct rxepoch *ep, *epqe, *epqpr;
    struct clock ept;
    int isEarliest = 0;

    MUTEX_ENTER(&rxevent_lock);
    AFS_ASSERT_RXGLOCK();
#ifdef RXDEBUG
    if (rx_Log_event) {
	struct clock now;
	clock_GetTime(&now);
	fprintf(rx_Log_event, "%d.%d: rxevent_Post(%d.%d, %x, %x)\n", now.sec, now.usec, when->sec, when->usec, func, arg);
    }
#endif

    /* Get a pointer to the epoch for this event, if none is found then
     * create a new epoch and insert it into the sorted list */
    for (ep = NULL, queue_ScanBackwards(&rxepoch_queue, epqe, epqpr, rxepoch)) {
	if (when->sec == epqe->epochSec) {
	    /* already have an structure for this epoch */
	    ep = epqe;
	    if (ep == queue_First(&rxepoch_queue, rxepoch))
		isEarliest = 1;
	    break;
	} else if (when->sec > epqe->epochSec) {
	    /* Create a new epoch and insert after qe */
	    ep = rxepoch_Allocate(when);
	    queue_InsertAfter(epqe, ep);
	    break;
	}
    }
    if (ep == NULL) {
	/* Create a new epoch and place it at the head of the list */
	ep = rxepoch_Allocate(when);
	queue_Prepend(&rxepoch_queue, ep);
	isEarliest = 1;
    }

    /* If we're short on free event entries, create a block of new ones and add
     * them to the free queue */
    if (queue_IsEmpty(&rxevent_free)) {
	register int i;
#if	defined(AFS_AIX32_ENV) && defined(KERNEL)
	ev = (struct rxevent *) rxi_Alloc(sizeof(struct rxevent));
	queue_Append(&rxevent_free, &ev[0]), rxevent_nFree++;
#else
	ev = (struct rxevent *) osi_Alloc(sizeof(struct rxevent) * rxevent_allocUnit);
	xsp = xfreemallocs;
	xfreemallocs = (struct xfreelist *) osi_Alloc(sizeof(struct xfreelist));
	xfreemallocs->mem = (void *)ev;
	xfreemallocs->size = sizeof(struct rxevent) * rxevent_allocUnit;
	xfreemallocs->next = xsp;
	for (i = 0; i<rxevent_allocUnit; i++)
	    queue_Append(&rxevent_free, &ev[i]), rxevent_nFree++;
#endif
    }

    /* Grab and initialize a new rxevent structure */
    ev = queue_First(&rxevent_free, rxevent);
    queue_Remove(ev);
    rxevent_nFree--;

    /* Record user defined event state */
    ev->eventTime = *when;
    ev->func = func;
    ev->arg = arg;
    ev->arg1 = arg1;
    rxevent_nPosted += 1; /* Rather than ++, to shut high-C up
			   *  regarding never-set variables
			   */

    /* Insert the event into the sorted list of events for this epoch */
    for (queue_ScanBackwards(&ep->events, evqe, evqpr, rxevent)) {
	if (when->usec >= evqe->eventTime.usec) {
	    /* Insert event after evqe */
	    queue_InsertAfter(evqe, ev);
	    MUTEX_EXIT(&rxevent_lock);
	    return ev;
	}
    }
    /* Insert event at head of current epoch */
    queue_Prepend(&ep->events, ev);
    if (isEarliest && rxevent_ScheduledEarlierEvent &&
	(!rxevent_raiseScheduled ||
	 clock_Lt(&ev->eventTime, &rxevent_nextRaiseEvents))) {
	rxevent_raiseScheduled = 1;
	clock_Zero(&rxevent_nextRaiseEvents);
	MUTEX_EXIT(&rxevent_lock);
	/* Notify our external scheduler */
	(*rxevent_ScheduledEarlierEvent)();
	MUTEX_ENTER(&rxevent_lock);
    }
    MUTEX_EXIT(&rxevent_lock);
    return ev;
}

/* Cancel an event by moving it from the event queue to the free list.
 * Warning, the event must be on the event queue!  If not, this should core
 * dump (reference through 0).  This routine should be called using the macro
 * event_Cancel, which checks for a null event and also nulls the caller's
 * event pointer after cancelling the event.
 */
#ifdef RX_ENABLE_LOCKS
#ifdef RX_REFCOUNT_CHECK
int rxevent_Cancel_type = 0;
void rxevent_Cancel_1(ev, call, type)
    register struct rxevent *ev;
    register struct rx_call *call;
    register int type;
#else /* RX_REFCOUNT_CHECK */
void rxevent_Cancel_1(ev, call)
    register struct rxevent *ev;
    register struct rx_call *call;
#endif /* RX_REFCOUNT_CHECK */
#else  /* RX_ENABLE_LOCKS */
void rxevent_Cancel_1(ev)
    register struct rxevent *ev;
#endif /* RX_ENABLE_LOCKS */
{
#ifdef RXDEBUG
    if (rx_Log_event) {
	struct clock now;
	clock_GetTime(&now);
	fprintf(rx_Log_event, "%d.%d: rxevent_Cancel_1(%d.%d, %x, %x)\n", now.sec,
		now.usec, ev->eventTime.sec, ev->eventTime.usec, ev->func,
		ev->arg);
    }
#endif
    /* Append it to the free list (rather than prepending) to keep the free
     * list hot so nothing pages out
     */
    AFS_ASSERT_RXGLOCK();
    MUTEX_ENTER(&rxevent_lock);
    if (!ev) {
	MUTEX_EXIT(&rxevent_lock);
	return;
    }
#ifdef RX_ENABLE_LOCKS
    /* It's possible we're currently processing this event. */
    if (queue_IsOnQueue(ev)) {
	queue_MoveAppend(&rxevent_free, ev);
	rxevent_nPosted--;
	rxevent_nFree++;
	if (call) {
	    call->refCount--;
#ifdef RX_REFCOUNT_CHECK
	    call->refCDebug[type]--;
	    if (call->refCDebug[type]<0) {
		rxevent_Cancel_type = type;
		osi_Panic("rxevent_Cancel: call refCount < 0");
	    }
#endif /* RX_REFCOUNT_CHECK */
	}
    }
#else /* RX_ENABLE_LOCKS */
    queue_MoveAppend(&rxevent_free, ev);
    rxevent_nPosted--;
    rxevent_nFree++;
#endif /* RX_ENABLE_LOCKS */
    MUTEX_EXIT(&rxevent_lock);
}

/* Process all epochs that have expired relative to the current clock time
 * (which is not re-evaluated unless clock_NewTime has been called).  The
 * relative time to the next epoch is returned in the output parameter next
 * and the function returns 1.  If there are is no next epoch, the function
 * returns 0.
 */
int rxevent_RaiseEvents(next)
    struct clock *next;
{
    register struct rxepoch *ep;
    register struct rxevent *ev;
    struct clock now;

    MUTEX_ENTER(&rxevent_lock);

    AFS_ASSERT_RXGLOCK();

    /* Events are sorted by time, so only scan until an event is found that has
     * not yet timed out */

    clock_Zero(&now);
    while (queue_IsNotEmpty(&rxepoch_queue)) {
	ep = queue_First(&rxepoch_queue, rxepoch);
	if (queue_IsEmpty(&ep->events)) {
	    queue_Remove(ep);
	    queue_Append(&rxepoch_free, ep);
	    rxepoch_nFree++;
	    continue;
	}
	do {
	    ev = queue_First(&ep->events, rxevent);
	    if (clock_Lt(&now, &ev->eventTime)) {
		clock_GetTime(&now);
		if (clock_Lt(&now, &ev->eventTime)) {
		    *next = rxevent_nextRaiseEvents = ev->eventTime;
		    rxevent_raiseScheduled = 1;
		    clock_Sub(next, &now);
		    MUTEX_EXIT(&rxevent_lock);
		    return 1;
		}
	    }
	    queue_Remove(ev);
	    rxevent_nPosted--;
	    MUTEX_EXIT(&rxevent_lock);
	    ev->func(ev, ev->arg, ev->arg1);
	    MUTEX_ENTER(&rxevent_lock);
	    queue_Append(&rxevent_free, ev);
	    rxevent_nFree++;
	} while (queue_IsNotEmpty(&ep->events));
    }
#ifdef RXDEBUG
    if (rx_Log_event) fprintf(rx_Log_event, "rxevent_RaiseEvents(%d.%d)\n", now.sec, now.usec);
#endif
    rxevent_raiseScheduled = 0;
    MUTEX_EXIT(&rxevent_lock);
    return 0;
}

void shutdown_rxevent(void) 
{
    struct xfreelist *xp, *nxp;

    LOCK_EV_INIT
    if (!rxevent_initialized) {
	UNLOCK_EV_INIT
	return;
    }
    rxevent_initialized = 0;
    UNLOCK_EV_INIT
    MUTEX_DESTROY(&rxevent_lock);
#if	defined(AFS_AIX32_ENV) && defined(KERNEL)
    /* Everything is freed in afs_osinet.c */
#else
    xp = xfreemallocs;
    while (xp) {
	nxp = xp->next;
	osi_Free((char *)xp->mem, xp->size);
	osi_Free((char *)xp, sizeof(struct xfreelist));
	xp = nxp;
    }
#endif

}
