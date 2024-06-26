/* Copyright (C) 1998  Transarc Corporation.  All rights reserved. */
#ifdef KERNEL
#include "../afs/param.h"
#if defined(UKERNEL)
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#include "../rx/rx_kcommon.h"
#include "../rx/rx_clock.h"
#include "../rx/rx_queue.h"
#include "../rx/rx_packet.h"
#else /* defined(UKERNEL) */
#include "../h/types.h"
#ifndef AFS_LINUX20_ENV
#include "../h/systm.h"
#endif
#if defined(AFS_SGI_ENV) || defined(AFS_HPUX110_ENV)
#include "../afs/sysincludes.h"
#endif
#include "../h/socket.h"
#include "../netinet/in.h"
#include "../afs/afs_osi.h"
#include "../rx/rx_kmutex.h"
#include "../rx/rx_clock.h"
#include "../rx/rx_queue.h"
#ifdef	AFS_SUN5_ENV
#include <sys/sysmacros.h>
#endif
#include "../rx/rx_packet.h"
#if !defined(AFS_SUN5_ENV) &&  !defined(AFS_LINUX20_ENV)
#if	!defined(AFS_OSF_ENV) && !defined(AFS_AIX41_ENV)
#include "../sys/mount.h"   /* it gets pulled in by something later anyway */
#endif
#include "../h/mbuf.h"
#endif
#endif /* defined(UKERNEL) */
#include "../rx/rx_globals.h"
#else /* KERNEL */
#include <afs/param.h>
#include "sys/types.h"
#include <sys/stat.h>
#include <errno.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include "rx_xmit_nt.h"
#include <stdlib.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include "rx_clock.h"
#include "rx.h"
#include "rx_queue.h"
#ifdef	AFS_SUN5_ENV
#include <sys/sysmacros.h>
#endif
#include "rx_packet.h"
#include "rx_globals.h"
#include <lwp.h>
#include "rx_internal.h"
#endif /* KERNEL */


#ifdef RX_LOCKS_DB
/* rxdb_fileID is used to identify the lock location, along with line#. */
static int rxdb_fileID = RXDB_FILE_RX_PACKET;
#endif /* RX_LOCKS_DB */
struct rx_packet *rx_mallocedP = 0;

extern char cml_version_number[];
extern int (*rx_almostSent)();

void rxi_FreePacketNoLock(struct rx_packet *p);
static void rxi_SendDebugPacket(struct rx_packet *apacket, osi_socket asocket,
			       afs_int32 ahost, short aport, afs_int32 istack);

extern char cml_version_number[];
extern int (*rx_almostSent)();
/* some rules about packets:
 * 1.  When a packet is allocated, the final iov_buf contains room for
 * a security trailer, but iov_len masks that fact.  If the security
 * package wants to add the trailer, it may do so, and then extend
 * iov_len appropriately.  For this reason, packet's niovecs and
 * iov_len fields should be accurate before calling PreparePacket.
*/

/* Preconditions:
 *        all packet buffers (iov_base) are integral multiples of 
 *	  the word size.
 *        offset is an integral multiple of the word size.
 */ 
afs_int32 rx_SlowGetInt32(struct rx_packet *packet, size_t offset)
{
  unsigned int i;
  size_t l;
  for (l=0, i=1; i< packet->niovecs ; i++ ) {
    if (l + packet->wirevec[i].iov_len > offset) {
      return *((afs_int32 *)((char*)(packet->wirevec[i].iov_base) + (offset-l)));
    }      
    l += packet->wirevec[i].iov_len;
  }
  
  return 0;
}

/* Preconditions:
 *        all packet buffers (iov_base) are integral multiples of the word size.
 *        offset is an integral multiple of the word size.
 */ 
afs_int32 rx_SlowPutInt32(struct rx_packet *packet, size_t offset, afs_int32 data)
{
  unsigned int i;
  size_t l;
  for (l=0, i=1; i< packet->niovecs ; i++ ) {
    if (l + packet->wirevec[i].iov_len > offset) {
      *((afs_int32 *)((char*)(packet->wirevec[i].iov_base) + (offset - l))) =
	  data;
      return 0;
    }      
    l += packet->wirevec[i].iov_len;
  }
  
  return 0;
}

/* Preconditions:
 *        all packet buffers (iov_base) are integral multiples of the
 *        word size.
 *        offset is an integral multiple of the word size.
 * Packet Invariants:
 *         all buffers are contiguously arrayed in the iovec from 0..niovecs-1
 */ 
afs_int32 rx_SlowReadPacket(struct rx_packet *packet, unsigned int offset,
			int resid, char *out)
{
  unsigned int i, j, l, r;
  for (l=0, i=1; i< packet->niovecs ; i++ ) {
    if (l + packet->wirevec[i].iov_len > offset) {
      break;
    }      
    l += packet->wirevec[i].iov_len;
  }
  
  /* i is the iovec which contains the first little bit of data in which we
   * are interested.  l is the total length of everything prior to this iovec.
   * j is the number of bytes we can safely copy out of this iovec.
   */
  r = resid;
  while ((resid > 0) && (i < packet->niovecs)) {
    j = MIN (resid, packet->wirevec[i].iov_len - (offset - l));
    bcopy ((char *)(packet->wirevec[i].iov_base) + (offset - l), out, j);
    resid -= j;
    l += packet->wirevec[i].iov_len;
    i++;  
  }
  
  return (resid ? (r - resid) : r);
}


/* Preconditions:
 *        all packet buffers (iov_base) are integral multiples of the
 *        word size.
 *        offset is an integral multiple of the word size.
 */ 
afs_int32 rx_SlowWritePacket(struct rx_packet *packet, int offset, int resid,
			 char *in)
{
  int i, j, l, r;
  char * b;
  
  for (l=0, i=1; i < packet->niovecs; i++ ) {
    if (l + packet->wirevec[i].iov_len > offset) {
      break;
    }      
    l += packet->wirevec[i].iov_len;
  }
  
  /* i is the iovec which contains the first little bit of data in which we
   * are interested.  l is the total length of everything prior to this iovec.
   * j is the number of bytes we can safely copy out of this iovec.
   */
  r = resid;
  while ((resid > 0) && (i < RX_MAXWVECS)) {
    if (i >= packet->niovecs) 
      if (rxi_AllocDataBuf(packet, resid, RX_PACKET_CLASS_SEND_CBUF) >0) /* ++niovecs as a side-effect */
	break;
    
    b = (char*)(packet->wirevec[i].iov_base) + (offset - l);
    j = MIN (resid, packet->wirevec[i].iov_len - (offset - l));
    bcopy (in, b, j);
    resid -= j;
    l += packet->wirevec[i].iov_len;
    i++;  
  }
  
  return (resid ? (r - resid) : r);
}

static struct rx_packet * allocCBuf(int class)
{
  struct rx_packet *c;
#ifndef KERNEL
  extern void rxi_MorePacketsNoLock();
#endif /* !KERNEL */
  SPLVAR;
  
  NETPRI;
  MUTEX_ENTER(&rx_freePktQ_lock);

#ifdef KERNEL
  if (rxi_OverQuota(class)) {
    c = NULL;
    rxi_NeedMorePackets = TRUE;
    MUTEX_ENTER(&rx_stats_mutex);
    switch(class) {
	case RX_PACKET_CLASS_RECEIVE:
	    rx_stats.receivePktAllocFailures++;
	    break;
	case RX_PACKET_CLASS_SEND:
	    rx_stats.sendPktAllocFailures++;
	    break;
	case RX_PACKET_CLASS_SPECIAL:
	    rx_stats.specialPktAllocFailures++;
	    break;
	case RX_PACKET_CLASS_RECV_CBUF:
	    rx_stats.receiveCbufPktAllocFailures++;
	    break;
	case RX_PACKET_CLASS_SEND_CBUF:
	    rx_stats.sendCbufPktAllocFailures++;
	    break;
    }
    MUTEX_EXIT(&rx_stats_mutex);
    goto done;
  }
  
  if (queue_IsEmpty(&rx_freePacketQueue)) {
    c = NULL;
    rxi_NeedMorePackets = TRUE;
    goto done;
  }
#else /* KERNEL */
  if (queue_IsEmpty(&rx_freePacketQueue)) {
    rxi_MorePacketsNoLock(rx_initSendWindow);
  }
#endif /* KERNEL */
  
  rx_nFreePackets--;
  c = queue_First(&rx_freePacketQueue, rx_packet);
  queue_Remove(c);
  if (c->header.flags != RX_FREE_PACKET)
    osi_Panic("rxi_AllocPacket: packet not free\n");
  c->header.flags = 0;
  
 done:
  MUTEX_EXIT(&rx_freePktQ_lock);
  
  USERPRI;
  return c;
}

/*
 * Free a packet currently used as a continuation buffer
 */
void rxi_freeCBuf(struct rx_packet *c)
{
  extern void rxi_PacketsUnWait();
  SPLVAR;
  
  NETPRI;
  MUTEX_ENTER(&rx_freePktQ_lock);
  
  rxi_FreePacketNoLock(c);
  /* Wakeup anyone waiting for packets */
  rxi_PacketsUnWait();
  
  MUTEX_EXIT(&rx_freePktQ_lock);
  USERPRI;
}

/* this one is kind of awful.
 * In rxkad, the packet has been all shortened, and everything, ready for 
 * sending.  All of a sudden, we discover we need some of that space back.
 * This isn't terribly general, because it knows that the packets are only
 * rounded up to the EBS (userdata + security header).
 */
int rxi_RoundUpPacket(p, nb)
     struct rx_packet * p;
     unsigned int nb;
{
  int i;
  i = p->niovecs - 1;
  if (p->wirevec[i].iov_base == (caddr_t) p->localdata) {
    if (p->wirevec[i].iov_len <= RX_FIRSTBUFFERSIZE - nb) {
      p->wirevec[i].iov_len += nb;
      return 0;
    }
  }
  else {
    if (p->wirevec[i].iov_len <= RX_CBUFFERSIZE - nb) {
      p->wirevec[i].iov_len += nb;
      return 0;
    }
  }

return 0;
}
/* get sufficient space to store nb bytes of data (or more), and hook
 * it into the supplied packet.  Return nbytes<=0 if successful, otherwise
 * returns the number of bytes >0 which it failed to come up with.
 * Don't need to worry about locking on packet, since only
 * one thread can manipulate one at a time. Locking on continution
 * packets is handled by allocCBuf */
/* MTUXXX don't need to go throught the for loop if we can trust niovecs */
int rxi_AllocDataBuf(struct rx_packet *p, int nb, int class) 
{
  int i;
  
  for (i=p->niovecs; nb>0 && i<RX_MAXWVECS; i++) {  
      register struct rx_packet *cb;
      if (cb = allocCBuf(class)) {
	  p->wirevec[i].iov_base = (caddr_t) cb->localdata;
	  p->wirevec[i].iov_len = RX_CBUFFERSIZE;
	  nb -= RX_CBUFFERSIZE;
	  p->length += RX_CBUFFERSIZE;
	  p->niovecs++;
      }
      else break;
  }
  
  return nb; 
}

/* Add more packet buffers */
void rxi_MorePackets(int apackets)
{
  extern void rxi_PacketsUnWait();
  struct rx_packet *p, *e;
  int getme;
  SPLVAR;

  getme = apackets * sizeof(struct rx_packet);
  p = rx_mallocedP = (struct rx_packet *) osi_Alloc(getme);

  PIN(p, getme);	/* XXXXX */
  bzero((char *)p, getme);
  NETPRI;
  AFS_RXGLOCK();
  MUTEX_ENTER(&rx_freePktQ_lock);

  for (e = p + apackets; p<e; p++) {
    p->wirevec[0].iov_base = (char *) (p->wirehead);
    p->wirevec[0].iov_len  = RX_HEADER_SIZE;
    p->wirevec[1].iov_base = (char *) (p->localdata);
    p->wirevec[1].iov_len  = RX_FIRSTBUFFERSIZE;
    p->header.flags = RX_FREE_PACKET;
    p->niovecs = 2;
    
    queue_Append(&rx_freePacketQueue, p);
  }
  rx_nFreePackets += apackets;
  rxi_NeedMorePackets = FALSE;
  rxi_PacketsUnWait();
  
  AFS_RXGUNLOCK();
  MUTEX_EXIT(&rx_freePktQ_lock);
  USERPRI;
}

#ifndef KERNEL
/* Add more packet buffers */
void rxi_MorePacketsNoLock(int apackets)
{
  extern void rxi_PacketsUnWait();
  struct rx_packet *p, *e;
  int getme;

  /* allocate enough packets that 1/4 of the packets will be able
   * to hold maximal amounts of data */ 
  apackets += (apackets/4)
	      * ((rx_maxJumboRecvSize - RX_FIRSTBUFFERSIZE)/RX_CBUFFERSIZE);
  getme = apackets * sizeof(struct rx_packet);
  p = rx_mallocedP = (struct rx_packet *) osi_Alloc(getme);

  bzero((char *)p, getme);

  for (e = p + apackets; p<e; p++) {
    p->wirevec[0].iov_base = (char *) (p->wirehead);
    p->wirevec[0].iov_len  = RX_HEADER_SIZE;
    p->wirevec[1].iov_base = (char *) (p->localdata);
    p->wirevec[1].iov_len  = RX_FIRSTBUFFERSIZE;
    p->header.flags = RX_FREE_PACKET;
    p->niovecs = 2;
    
    queue_Append(&rx_freePacketQueue, p);
  }
  rx_nFreePackets += apackets;
  rxi_NeedMorePackets = FALSE;
  rxi_PacketsUnWait();
}
#endif /* !KERNEL */

void rxi_FreeAllPackets(void)
{
  /* must be called at proper interrupt level, etcetera */
  /* MTUXXX need to free all Packets */
  osi_Free(rx_mallocedP, (rx_maxReceiveWindow+2) * sizeof(struct rx_packet));    
  UNPIN(rx_mallocedP, (rx_maxReceiveWindow+2) * sizeof(struct rx_packet));    
}

/* Allocate more packets iff we need more continuation buffers */
/* In kernel, can't page in memory with interrupts disabled, so we
 * don't use the event mechanism. */
void rx_CheckPackets()
{
  if (rxi_NeedMorePackets) {
    rxi_MorePackets(rx_initSendWindow); 
  }
}

/* In the packet freeing routine below, the assumption is that
   we want all of the packets to be used equally frequently, so that we
   don't get packet buffers paging out.  It would be just as valid to
   assume that we DO want them to page out if not many are being used.
   In any event, we assume the former, and append the packets to the end
   of the free list.  */
/* This explanation is bogus.  The free list doesn't remain in any kind of
   useful order for afs_int32: the packets in use get pretty much randomly scattered 
   across all the pages.  In order to permit unused {packets,bufs} to page out, they
   must be stored so that packets which are adjacent in memory are adjacent in the 
   free list.  An array springs rapidly to mind.
   */

/* Actually free the packet p. */
void rxi_FreePacketNoLock(struct rx_packet *p)
{
  dpf(("Free %x\n", p));

  if (p->header.flags & RX_FREE_PACKET)
    osi_Panic("rxi_FreePacketNoLock: packet already free\n");
  rx_nFreePackets++;
  p->header.flags = RX_FREE_PACKET;
  queue_Append(&rx_freePacketQueue, p);
}

int rxi_FreeDataBufsNoLock(p, first)
     struct rx_packet * p;
     int first;
{
  struct iovec *iov, *end;
  
  if (first != 1)          /* MTUXXX */
      osi_Panic("FreeDataBufs 1: first must be 1");
  iov = &p->wirevec[1];
  end = iov + (p->niovecs-1);
  if (iov->iov_base != (caddr_t) p->localdata) /* MTUXXX */
	osi_Panic("FreeDataBufs 2: vec 1 must be localdata");
  for (iov++ ; iov < end ; iov++) {
    if (!iov->iov_base)
	osi_Panic("FreeDataBufs 3: vecs 2-niovecs must not be NULL");
    rxi_FreePacketNoLock(RX_CBUF_TO_PACKET(iov->iov_base, p));
  }
  p->length = 0;
  p->niovecs = 0;

  return 0; 
}

int rxi_nBadIovecs = 0;

/* rxi_RestoreDataBufs 
 *
 * Restore the correct sizes to the iovecs. Called when reusing a packet
 * for reading off the wire.
 */
void rxi_RestoreDataBufs(struct rx_packet *p)
{
    int i;
    struct iovec *iov = &p->wirevec[2];

    p->wirevec[0].iov_base = (char *) (p->wirehead);
    p->wirevec[0].iov_len  = RX_HEADER_SIZE;
    p->wirevec[1].iov_base = (char *) (p->localdata);
    p->wirevec[1].iov_len  = RX_FIRSTBUFFERSIZE;

    for (i=2, iov = &p->wirevec[2]; i < p->niovecs; i++, iov++) {
	if (!iov->iov_base) {
	    rxi_nBadIovecs ++;
	    p->niovecs = i;
	    break;
	}
	iov->iov_len = RX_CBUFFERSIZE;
    }
}

int rxi_TrimDataBufs(p, first)
     struct rx_packet * p;
     int first;
{
  extern void rxi_PacketsUnWait();
  int length;
  struct iovec *iov, *end;
  SPLVAR;
  
  if (first != 1)
      osi_Panic("TrimDataBufs 1: first must be 1");

  /* Skip over continuation buffers containing message data */
  iov = &p->wirevec[2];
  end = iov + (p->niovecs-2);
  length = p->length - p->wirevec[1].iov_len;
  for (; iov < end && length > 0 ; iov++) {
    if (!iov->iov_base)
	osi_Panic("TrimDataBufs 3: vecs 1-niovecs must not be NULL");
    length -= iov->iov_len;
  }

  /* iov now points to the first empty data buffer. */
  if (iov >= end)
    return 0;
  
  NETPRI;
  MUTEX_ENTER(&rx_freePktQ_lock);

  for (; iov < end ; iov++) {
    if (!iov->iov_base)
	osi_Panic("TrimDataBufs 4: vecs 2-niovecs must not be NULL");
    rxi_FreePacketNoLock(RX_CBUF_TO_PACKET(iov->iov_base, p));
    p->niovecs--;
  }
  rxi_PacketsUnWait();
  
  MUTEX_EXIT(&rx_freePktQ_lock);
  USERPRI;

  return 0; 
}

/* Free the packet p.  P is assumed not to be on any queue, i.e.
 * remove it yourself first if you call this routine. */
void rxi_FreePacket(struct rx_packet *p)
{
  extern void rxi_PacketsUnWait();
  SPLVAR;
  
  NETPRI;
  MUTEX_ENTER(&rx_freePktQ_lock);
  
  rxi_FreeDataBufsNoLock(p,1);
  rxi_FreePacketNoLock(p);
  /* Wakeup anyone waiting for packets */
  rxi_PacketsUnWait();
  
  MUTEX_EXIT(&rx_freePktQ_lock);
  USERPRI;
}


/* rxi_AllocPacket sets up p->length so it reflects the number of 
 * bytes in the packet at this point, **not including** the header.
 * The header is absolutely necessary, besides, this is the way the
 * length field is usually used */
struct rx_packet *rxi_AllocPacketNoLock(class)
     int class;
{
  register struct rx_packet *p;
  
#ifdef KERNEL
  if (rxi_OverQuota(class)) {
    rxi_NeedMorePackets = TRUE;
    MUTEX_ENTER(&rx_stats_mutex);
    switch(class) {
	case RX_PACKET_CLASS_RECEIVE:
	    rx_stats.receivePktAllocFailures++;
	    break;
	case RX_PACKET_CLASS_SEND:
	    rx_stats.sendPktAllocFailures++;
	    break;
	case RX_PACKET_CLASS_SPECIAL:
	    rx_stats.specialPktAllocFailures++;
	    break;
	case RX_PACKET_CLASS_RECV_CBUF:
	    rx_stats.receiveCbufPktAllocFailures++;
	    break;
	case RX_PACKET_CLASS_SEND_CBUF:
	    rx_stats.sendCbufPktAllocFailures++;
	    break;
    }
    MUTEX_EXIT(&rx_stats_mutex);
    return (struct rx_packet *) 0;
  }
#endif /* KERNEL */

  MUTEX_ENTER(&rx_stats_mutex);
  rx_stats.packetRequests++;
  MUTEX_EXIT(&rx_stats_mutex);
  
#ifdef KERNEL
  if (queue_IsEmpty(&rx_freePacketQueue)) 
    osi_Panic("rxi_AllocPacket error");
#else /* KERNEL */
  if (queue_IsEmpty(&rx_freePacketQueue)) 
    rxi_MorePacketsNoLock(rx_initSendWindow);
#endif /* KERNEL */
  
  rx_nFreePackets--;
  p = queue_First(&rx_freePacketQueue, rx_packet);
  if (p->header.flags != RX_FREE_PACKET)
    osi_Panic("rxi_AllocPacket: packet not free\n");
  
  dpf(("Alloc %x, class %d\n", p, class));
  
  queue_Remove(p);
  p->header.flags = 0;
  
  /* have to do this here because rx_FlushWrite fiddles with the iovs in
   * order to truncate outbound packets.  In the near future, may need 
   * to allocate bufs from a static pool here, and/or in AllocSendPacket
   */
  p->wirevec[0].iov_base = (char *) (p->wirehead);
  p->wirevec[0].iov_len  = RX_HEADER_SIZE;
  p->wirevec[1].iov_base = (char *) (p->localdata);
  p->wirevec[1].iov_len  = RX_FIRSTBUFFERSIZE;
  p->niovecs = 2;
  p->length = RX_FIRSTBUFFERSIZE;
  return p;
}

struct rx_packet *rxi_AllocPacket(class)
     int class;
{
    register struct rx_packet *p;

    MUTEX_ENTER(&rx_freePktQ_lock);
    p = rxi_AllocPacketNoLock(class);
    MUTEX_EXIT(&rx_freePktQ_lock);
    return p;
}

/* This guy comes up with as many buffers as it {takes,can get} given
 * the MTU for this call. It also sets the packet length before
 * returning.  caution: this is often called at NETPRI
 * Called with call locked.
 */
struct rx_packet *rxi_AllocSendPacket(call, want)
register struct rx_call *call;
int want;
{
    register struct rx_packet *p = (struct rx_packet *) 0;
    register int mud;
    register unsigned delta;

    SPLVAR;
    mud = call->MTU - RX_HEADER_SIZE;
    delta = rx_GetSecurityHeaderSize(rx_ConnectionOf(call)) + 
	rx_GetSecurityMaxTrailerSize(rx_ConnectionOf(call));

    while (!(call->error)) {
      MUTEX_ENTER(&rx_freePktQ_lock);
      /* if an error occurred, or we get the packet we want, we're done */
      if (p = rxi_AllocPacketNoLock(RX_PACKET_CLASS_SEND)) {
	MUTEX_EXIT(&rx_freePktQ_lock);

	want += delta;
	want = MIN(want, mud);

	if ((unsigned) want > p->length) 
	  (void) rxi_AllocDataBuf(p, (want - p->length),
				  RX_PACKET_CLASS_SEND_CBUF);

	if ((unsigned) p->length > mud)
	    p->length = mud;

	if (delta >= p->length) {
	  rxi_FreePacket(p);
	  p = NULL; 
	} else {
	    p->length -= delta;
	}
	break; 
      }

      /* no error occurred, and we didn't get a packet, so we sleep.
       * At this point, we assume that packets will be returned
       * sooner or later, as packets are acknowledged, and so we
       * just wait.  */
      NETPRI;
      call->flags |= RX_CALL_WAIT_PACKETS;
      CALL_HOLD(call, RX_CALL_REFCOUNT_PACKET);
      MUTEX_EXIT(&call->lock);
      rx_waitingForPackets = 1;
      
#ifdef	RX_ENABLE_LOCKS
      CV_WAIT(&rx_waitingForPackets_cv, &rx_freePktQ_lock);
#else
      osi_rxSleep(&rx_waitingForPackets);
#endif
      MUTEX_EXIT(&rx_freePktQ_lock);
      MUTEX_ENTER(&call->lock);
      CALL_RELE(call, RX_CALL_REFCOUNT_PACKET);
      call->flags &= ~RX_CALL_WAIT_PACKETS;
      USERPRI;
    }

    return p;
}

#ifndef KERNEL

/* count the number of used FDs */
static int CountFDs(amax)
register int amax; {
    struct stat tstat;
    register int i, code;
    register int count;

    count = 0;
    for(i=0;i<amax;i++) {
	code = fstat(i, &tstat);
	if (code == 0) count++;
    }
    return count;
}

#else /* KERNEL */

#define CountFDs(amax) amax

#endif /* KERNEL */

#if !defined(KERNEL) || defined(UKERNEL)

/* This function reads a single packet from the interface into the
 * supplied packet buffer (*p).  Return 0 if the packet is bogus.  The
 * (host,port) of the sender are stored in the supplied variables, and
 * the data length of the packet is stored in the packet structure.
 * The header is decoded. */
int rxi_ReadPacket(socket, p, host, port)
     int socket;
     register struct rx_packet *p;
     afs_uint32 *host;
     u_short *port;
{
    struct sockaddr_in from;
    int nbytes;
    afs_int32 rlen;
    register afs_int32 tlen, savelen;
    struct msghdr msg;
    rx_computelen(p, tlen);
    rx_SetDataSize(p, tlen);  /* this is the size of the user data area */

    tlen += RX_HEADER_SIZE;   /* now this is the size of the entire packet */
    rlen = rx_maxJumboRecvSize; /* this is what I am advertising.  Only check
			         * it once in order to avoid races.  */
    tlen = rlen - tlen;
    if (tlen > 0) {
      tlen = rxi_AllocDataBuf(p, tlen, RX_PACKET_CLASS_SEND_CBUF);
      if (tlen >0) {
	tlen = rlen - tlen;
      }
      else tlen = rlen;
    }
    else tlen = rlen;

   /* Extend the last iovec for padding, it's just to make sure that the 
    * read doesn't return more data than we expect, and is done to get around
    * our problems caused by the lack of a length field in the rx header.
    * Use the extra buffer that follows the localdata in each packet
    * structure. */
    savelen = p->wirevec[p->niovecs].iov_len;
    p->wirevec[p->niovecs].iov_len += RX_EXTRABUFFERSIZE;

    bzero((char *)&msg, sizeof(msg));
    msg.msg_name = (char *) &from;
    msg.msg_namelen = sizeof(struct sockaddr_in);
    msg.msg_iov = p->wirevec;
    msg.msg_iovlen = p->niovecs;
    nbytes = rxi_Recvmsg(socket, &msg, 0);

   /* restore the vec to its correct state */
    p->wirevec[p->niovecs].iov_len = savelen;

    p->length = (nbytes - RX_HEADER_SIZE);
    if ((nbytes > tlen) || (p->length  & 0x8000)) {  /* Bogus packet */
      if (nbytes > 0)
	rxi_MorePackets(rx_initSendWindow);
#ifndef AFS_NT40_ENV
      else if (nbytes < 0 && errno == EWOULDBLOCK) {
	MUTEX_ENTER(&rx_stats_mutex);
	rx_stats.noPacketOnRead++;
	MUTEX_EXIT(&rx_stats_mutex);
      }
#endif
      else {
	MUTEX_ENTER(&rx_stats_mutex);
	rx_stats.bogusPacketOnRead++;
	rx_stats.bogusHost = from.sin_addr.s_addr;
	MUTEX_EXIT(&rx_stats_mutex);
	dpf(("B: bogus packet from [%x,%d] nb=%d", from.sin_addr.s_addr,
	     from.sin_port,nbytes));
      }
      return  0;
    }
    else {
      /* Extract packet header. */
      rxi_DecodePacketHeader(p);
      
      *host = from.sin_addr.s_addr;
      *port = from.sin_port;
      if (p->header.type > 0 && p->header.type < RX_N_PACKET_TYPES) {
	struct rx_peer *peer;
	MUTEX_ENTER(&rx_stats_mutex);
	rx_stats.packetsRead[p->header.type-1]++;
	MUTEX_EXIT(&rx_stats_mutex);
	/*
	 * Try to look up this peer structure.  If it doesn't exist,
	 * don't create a new one - 
	 * we don't keep count of the bytes sent/received if a peer
	 * structure doesn't already exist.
	 *
	 * The peer/connection cleanup code assumes that there is 1 peer
	 * per connection.  If we actually created a peer structure here
	 * and this packet was an rxdebug packet, the peer structure would
	 * never be cleaned up.
	 */
	peer = rxi_FindPeer(*host, *port, 0, 0);
	if (peer) {
	    MUTEX_ENTER(&peer->peer_lock);
	    hadd32(peer->bytesReceived, p->length);
	    MUTEX_EXIT(&peer->peer_lock);
	}
      }

      /* Free any empty packet buffers at the end of this packet */
      rxi_TrimDataBufs(p, 1);
      
      return  1;
    }
}

#endif /* !KERNEL || UKERNEL */

/* This function splits off the first packet in a jumbo packet.
 * As of AFS 3.5, jumbograms contain more than one fixed size
 * packet, and the RX_JUMBO_PACKET flag is set in all but the
 * last packet header. All packets (except the last) are padded to
 * fall on RX_CBUFFERSIZE boundaries.
 * HACK: We store the length of the first n-1 packets in the
 * last two pad bytes. */

struct rx_packet *rxi_SplitJumboPacket(p, host, port, first)
     register struct rx_packet *p;
     afs_int32 host;
     short port;
     int first;
{
    struct rx_packet *np;
    struct rx_jumboHeader *jp;
    int niov, i;
    struct iovec *iov;
    int length;
    afs_uint32 temp;

    /* All but the last packet in each jumbogram are RX_JUMBOBUFFERSIZE
     * bytes in length. All but the first packet are preceded by
     * an abbreviated four byte header. The length of the last packet
     * is calculated from the size of the jumbogram. */
    length = RX_JUMBOBUFFERSIZE + RX_JUMBOHEADERSIZE;

    if ((int)p->length < length) {
	dpf(("rxi_SplitJumboPacket: bogus length %d\n", p->length));
	return NULL;
    }
    niov = p->niovecs - 2;
    if (niov < 1) {
	dpf(("rxi_SplitJumboPacket: bogus niovecs %d\n", p->niovecs));
	return NULL;
    }
    iov = &p->wirevec[2];
    np = RX_CBUF_TO_PACKET(iov->iov_base, p);

    /* Get a pointer to the abbreviated packet header */
    jp = (struct rx_jumboHeader *)
	 ((char *)(p->wirevec[1].iov_base) + RX_JUMBOBUFFERSIZE);

    /* Set up the iovecs for the next packet */
    np->wirevec[0].iov_base = (char *)(&np->wirehead[0]);
    np->wirevec[0].iov_len = sizeof(struct rx_header);
    np->wirevec[1].iov_base = (char *)(&np->localdata[0]);
    np->wirevec[1].iov_len = length - RX_JUMBOHEADERSIZE;
    np->niovecs = niov+1;
    for (i = 2 , iov++ ; i <= niov ; i++ , iov++) {
	np->wirevec[i] = *iov;
    }
    np->length = p->length - length;
    p->length = RX_JUMBOBUFFERSIZE;
    p->niovecs = 2;

    /* Convert the jumbo packet header to host byte order */
    temp = ntohl(*(afs_uint32 *)jp);
    jp->flags = (u_char)(temp >> 24);
    jp->cksum = (u_short)(temp);

    /* Fill in the packet header */
    np->header = p->header;
    np->header.serial = p->header.serial + 1;
    np->header.seq = p->header.seq + 1;
    np->header.flags = jp->flags;
    np->header.spare = jp->cksum;
      
    return np;
}

#ifndef KERNEL
/* Send a udp datagram */
int osi_NetSend(socket, addr, dvec, nvecs, length, istack)
    osi_socket socket;
    char * addr;
    struct iovec *dvec;
    int nvecs;
    int length;
    int istack;
{
    struct msghdr msg;

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = dvec;
    msg.msg_iovlen = nvecs;
    msg.msg_name = addr;
    msg.msg_namelen = sizeof(struct sockaddr_in);

    rxi_Sendmsg(socket, &msg, 0);

    return 0;
}
#elif !defined(UKERNEL)
/* osi_NetSend is defined in afs/afs_osinet.c
 * message receipt is done in rxk_input or rx_put.
 */

#ifdef AFS_SUN5_ENV
/*
 * Copy an mblock to the contiguous area pointed to by cp.
 * MTUXXX Supposed to skip <off> bytes and copy <len> bytes,
 * but it doesn't really.
 * Returns the number of bytes not transferred.
 * The message is NOT changed.
 */
static int cpytoc(mp, off, len, cp)
    mblk_t *mp;
    register int off, len;
    register char * cp;
{
    register int n;

    for (;mp && len > 0; mp = mp->b_cont) {
	if (mp->b_datap->db_type != M_DATA) {
	    return -1;
	}
	n = MIN(len, (mp->b_wptr - mp->b_rptr));
	bcopy((char *)mp->b_rptr, cp, n);
	cp += n;
	len -= n;
	mp->b_rptr += n;
    }
    return (len);
}

/* MTUXXX Supposed to skip <off> bytes and copy <len> bytes,
 * but it doesn't really.  
 * This sucks, anyway, do it like m_cpy.... below 
 */
static int cpytoiovec(mp, off, len, iovs, niovs)
    mblk_t *mp;
    int off, len, niovs;
    register struct iovec *iovs;
{
    register int m,n,o,t,i;

    for (i = -1, t = 0; i < niovs && mp && len > 0; mp = mp->b_cont) {
	if (mp->b_datap->db_type != M_DATA) {
	    return -1;
	}
	n = MIN(len, (mp->b_wptr - mp->b_rptr));
	len -= n;
	while (n) {
	  if (!t) {
	    o=0;
	    i++;
	    t = iovs[i].iov_len;
	  }
	  m = MIN(n,t);
	  bcopy((char *)mp->b_rptr, iovs[i].iov_base + o, m);
	  mp->b_rptr += m;
	  o += m;
	  t -= m;
	  n -= m;
	}
    }
    return (len);
}
#define m_cpytoc(a, b, c, d)  cpytoc(a, b, c, d)
#define m_cpytoiovec(a, b, c, d, e) cpytoiovec(a, b, c, d, e)
#else
#if !defined(AFS_LINUX20_ENV)
static int m_cpytoiovec(m, off, len, iovs, niovs)
     struct mbuf *m;
     int off, len, niovs;
     struct iovec iovs[];
{
  caddr_t p1, p2;
  unsigned int l1, l2, i, t;
  
  if (m == NULL || off < 0 || len < 0 || iovs == NULL)
    osi_Panic("m_cpytoiovec");  /* MTUXXX probably don't need this check */
  
  while (off && m)
    if (m->m_len <= off) {
      off -= m->m_len;
      m = m->m_next;
      continue;
    } else
      break;
  
  if (m == NULL)
    return len;
  
  p1 = mtod(m, caddr_t)+off;
  l1 = m->m_len - off;
  i = 0;
  p2 = iovs[0].iov_base;
  l2 = iovs[0].iov_len;
  
  while (len) {
    t = MIN(l1, MIN(l2, (unsigned int)len));
    bcopy (p1, p2, t);
    p1 += t;    p2 += t;
    l1 -= t;    l2 -= t;
    len -= t;
    if (!l1) {
      m = m->m_next; 
      if (!m)
	break;
      p1 = mtod(m, caddr_t);
      l1 = m->m_len;
    }
    if (!l2) {
      if (++i >= niovs)
	break;
      p2 = iovs[i].iov_base;
      l2 = iovs[i].iov_len;
    }
    
  }

return len;  
}
#endif /* LINUX */
#endif /* AFS_SUN5_ENV */

#if !defined(AFS_LINUX20_ENV)
int rx_mb_to_packet(amb, free, hdr_len, data_len, phandle)
#ifdef	AFS_SUN5_ENV
mblk_t *amb;
#else
struct mbuf *amb;
#endif
void (*free)();
struct rx_packet *phandle;
int hdr_len, data_len;
{
  register int code;

  code = m_cpytoiovec(amb, hdr_len, data_len, phandle->wirevec, phandle->niovecs); 
  (*free)(amb);

  return code;
}
#endif /* LINUX */
#endif /*KERNEL && !UKERNEL*/


/* send a response to a debug packet */

struct rx_packet *rxi_ReceiveDebugPacket(ap, asocket, ahost, aport, istack)
  osi_socket asocket;
  afs_int32 ahost;
  short aport;
  register struct rx_packet *ap;
  int istack;
{
    struct rx_debugIn tin;
    afs_int32 tl;
    struct rx_serverQueueEntry *np, *nqe;

    rx_packetread(ap, 0, sizeof(struct rx_debugIn), (char *)&tin);
    /* all done with packet, now set length to the truth, so we can 
     * reuse this packet */
    rx_computelen(ap, ap->length);

    tin.type = ntohl(tin.type);
    tin.index = ntohl(tin.index);
    switch(tin.type) {
	case RX_DEBUGI_GETSTATS: {
	    struct rx_debugStats tstat;

	    /* get basic stats */
	    bzero ((char *)&tstat, sizeof(tstat)); /* make sure spares are zero */
	    tstat.version = RX_DEBUGI_VERSION;
#ifndef	RX_ENABLE_LOCKS
	    tstat.waitingForPackets = rx_waitingForPackets;
#endif
	    tstat.nFreePackets = htonl(rx_nFreePackets);
	    tstat.callsExecuted = htonl(rxi_nCalls);
	    tstat.packetReclaims = htonl(rx_packetReclaims);
	    tstat.usedFDs = CountFDs(64);
	    tstat.nWaiting = htonl(rx_nWaiting);
	    queue_Count( &rx_idleServerQueue, np, nqe, 
				rx_serverQueueEntry, tstat.idleThreads); 
	    tstat.idleThreads = htonl(tstat.idleThreads);
	    tl = sizeof(struct rx_debugStats) - ap->length;
	    if (tl > 0)
	      tl = rxi_AllocDataBuf(ap, tl, RX_PACKET_CLASS_SEND_CBUF);

	    if (tl <= 0) {
	      rx_packetwrite(ap, 0, sizeof(struct rx_debugStats), (char *)&tstat);
	      ap->length = sizeof(struct rx_debugStats);
	      rxi_SendDebugPacket(ap, asocket, ahost, aport, istack);
	      rx_computelen(ap, ap->length);
	    }
	    break;
	}

	case RX_DEBUGI_GETALLCONN:
	case RX_DEBUGI_GETCONN: {
	    int i, j;
	    register struct rx_connection *tc;
	    struct rx_call *tcall;
	    struct rx_debugConn tconn;
	    int all = (tin.type == RX_DEBUGI_GETALLCONN);


	    tl = sizeof(struct rx_debugConn) - ap->length;
	    if (tl > 0)
	      tl = rxi_AllocDataBuf(ap, tl, RX_PACKET_CLASS_SEND_CBUF);
	    if (tl > 0)
	      return ap;

	    bzero ((char *)&tconn, sizeof(tconn)); /* make sure spares are zero */
	    /* get N'th (maybe) "interesting" connection info */
	    for(i=0;i<rx_hashTableSize;i++) {
#if !defined(KERNEL)
		/* the time complexity of the algorithm used here
		 * exponentially increses with the number of connections.
		 */
#ifdef AFS_PTHREAD_ENV
		pthread_yield();
#else
		(void) IOMGR_Poll();
#endif
#endif
		MUTEX_ENTER(&rx_connHashTable_lock);
		/* We might be slightly out of step since we are not 
		 * locking each call, but this is only debugging output.
		 */
		for(tc=rx_connHashTable[i]; tc; tc=tc->next) {
		    if ((all || rxi_IsConnInteresting(tc)) && tin.index-- <= 0) {
			tconn.host = tc->peer->host;
			tconn.port = tc->peer->port;
			tconn.cid = htonl(tc->cid);
			tconn.epoch = htonl(tc->epoch);
			tconn.serial = htonl(tc->serial);
			for(j=0;j<RX_MAXCALLS;j++) {
			    tconn.callNumber[j] = htonl(tc->callNumber[j]);
			    if (tcall=tc->call[j]) {
				tconn.callState[j] = tcall->state;
				tconn.callMode[j] = tcall->mode;
				tconn.callFlags[j] = tcall->flags;
				if (queue_IsNotEmpty(&tcall->rq))
				    tconn.callOther[j] |= RX_OTHER_IN;
				if (queue_IsNotEmpty(&tcall->tq))
				    tconn.callOther[j] |= RX_OTHER_OUT;
			    }
			    else tconn.callState[j] = RX_STATE_NOTINIT;
			}

			tconn.natMTU = htonl(tc->peer->natMTU);
			tconn.error = htonl(tc->error);
			tconn.flags = tc->flags;
			tconn.type = tc->type;
			tconn.securityIndex = tc->securityIndex;
			if (tc->securityObject) {
			    RXS_GetStats (tc->securityObject, tc,
					  &tconn.secStats);
#define DOHTONL(a) (tconn.secStats.a = htonl(tconn.secStats.a))
#define DOHTONS(a) (tconn.secStats.a = htons(tconn.secStats.a))
			    DOHTONL(flags);
			    DOHTONL(expires);
			    DOHTONL(packetsReceived);
			    DOHTONL(packetsSent);
			    DOHTONL(bytesReceived);
			    DOHTONL(bytesSent);
			    for (i=0;
				 i<sizeof(tconn.secStats.spares)/sizeof(short);
				 i++)
				DOHTONS(spares[i]);
			    for (i=0;
				 i<sizeof(tconn.secStats.sparel)/sizeof(afs_int32);
				 i++)
				DOHTONL(sparel[i]);
			}

			MUTEX_EXIT(&rx_connHashTable_lock);
			rx_packetwrite(ap, 0, sizeof(struct rx_debugConn), (char*)&tconn);
			tl = ap->length;
			ap->length = sizeof(struct rx_debugConn);
			rxi_SendDebugPacket(ap, asocket, ahost, aport, istack);
			ap->length = tl;
			return ap;
		    }
		}
		MUTEX_EXIT(&rx_connHashTable_lock);
	    }
	    /* if we make it here, there are no interesting packets */
	    tconn.cid = htonl(0xffffffff); /* means end */
	    rx_packetwrite(ap, 0, sizeof(struct rx_debugConn), (char *)&tconn);
	    tl = ap->length;
	    ap->length = sizeof(struct rx_debugConn);
	    rxi_SendDebugPacket(ap, asocket, ahost, aport, istack);
	    ap->length = tl;
	    break;
	}

	/*
	 * Pass back all the peer structures we have available
	 */

	case RX_DEBUGI_GETPEER: {
	    int i, j;
	    register struct rx_peer *tp;
	    struct rx_debugPeer tpeer;


	    tl = sizeof(struct rx_debugPeer) - ap->length;
	    if (tl > 0)
	      tl = rxi_AllocDataBuf(ap, tl, RX_PACKET_CLASS_SEND_CBUF);
	    if (tl > 0)
	      return ap;

	    bzero ((char *)&tpeer, sizeof(tpeer));
	    for(i=0;i<rx_hashTableSize;i++) {
#if !defined(KERNEL)
		/* the time complexity of the algorithm used here
		 * exponentially increses with the number of peers.
		 *
		 * Yielding after processing each hash table entry
		 * and dropping rx_peerHashTable_lock.
		 * also increases the risk that we will miss a new
		 * entry - but we are willing to live with this
		 * limitation since this is meant for debugging only
		 */
#ifdef AFS_PTHREAD_ENV
		pthread_yield();
#else
		(void) IOMGR_Poll();
#endif
#endif
		MUTEX_ENTER(&rx_peerHashTable_lock);
		for(tp=rx_peerHashTable[i]; tp; tp=tp->next) {
		    if (tin.index-- <= 0) {
			tpeer.host = tp->host;
			tpeer.port = tp->port;
			tpeer.ifMTU = htons(tp->ifMTU);
			tpeer.idleWhen = htonl(tp->idleWhen);
			tpeer.refCount = htons(tp->refCount);
			tpeer.burstSize = tp->burstSize;
			tpeer.burst = tp->burst;
			tpeer.burstWait.sec = htonl(tp->burstWait.sec);
			tpeer.burstWait.usec = htonl(tp->burstWait.usec);
			tpeer.rtt = htonl(tp->rtt);
			tpeer.rtt_dev = htonl(tp->rtt_dev);
			tpeer.timeout.sec = htonl(tp->timeout.sec);
			tpeer.timeout.usec = htonl(tp->timeout.usec);
			tpeer.nSent = htonl(tp->nSent);
			tpeer.reSends = htonl(tp->reSends);
			tpeer.inPacketSkew = htonl(tp->inPacketSkew);
			tpeer.outPacketSkew = htonl(tp->outPacketSkew);
			tpeer.rateFlag = htonl(tp->rateFlag);
			tpeer.natMTU = htons(tp->natMTU);
			tpeer.maxMTU = htons(tp->maxMTU);
			tpeer.maxDgramPackets = htons(tp->maxDgramPackets);
			tpeer.ifDgramPackets = htons(tp->ifDgramPackets);
			tpeer.MTU = htons(tp->MTU);
			tpeer.cwind = htons(tp->cwind);
			tpeer.nDgramPackets = htons(tp->nDgramPackets);
			tpeer.congestSeq = htons(tp->congestSeq);
			tpeer.bytesSent.high = htonl(tp->bytesSent.high);
			tpeer.bytesSent.low = htonl(tp->bytesSent.low);
			tpeer.bytesReceived.high = htonl(tp->bytesReceived.high);
			tpeer.bytesReceived.low = htonl(tp->bytesReceived.low);

			MUTEX_EXIT(&rx_peerHashTable_lock);
			rx_packetwrite(ap, 0, sizeof(struct rx_debugPeer), (char*)&tpeer);
			tl = ap->length;
			ap->length = sizeof(struct rx_debugPeer);
			rxi_SendDebugPacket(ap, asocket, ahost, aport, istack);
			ap->length = tl;
			return ap;
		    }
		}
		MUTEX_EXIT(&rx_peerHashTable_lock);
	    }
	    /* if we make it here, there are no interesting packets */
	    tpeer.host = htonl(0xffffffff); /* means end */
	    rx_packetwrite(ap, 0, sizeof(struct rx_debugPeer), (char *)&tpeer);
	    tl = ap->length;
	    ap->length = sizeof(struct rx_debugPeer);
	    rxi_SendDebugPacket(ap, asocket, ahost, aport, istack);
	    ap->length = tl;
	    break;
	}

	case RX_DEBUGI_RXSTATS: {
	    int i;
	    afs_int32 *s;

	    tl = sizeof(rx_stats) - ap->length;
	    if (tl > 0)
	      tl = rxi_AllocDataBuf(ap, tl, RX_PACKET_CLASS_SEND_CBUF);
	    if (tl > 0)
	      return ap;

	    /* Since its all int32s convert to network order with a loop. */
	    MUTEX_ENTER(&rx_stats_mutex);
	    s = (afs_int32 *)&rx_stats;
	    for (i=0; i<sizeof(rx_stats)/sizeof(afs_int32); i++,s++)
		rx_PutInt32(ap, i*sizeof(afs_int32), htonl(*s));

	    tl = ap->length;
	    ap->length = sizeof(rx_stats);
	    MUTEX_EXIT(&rx_stats_mutex);
	    rxi_SendDebugPacket(ap, asocket, ahost, aport, istack);
	    ap->length = tl;
	    break;
	}

	default:
	    /* error response packet */
	    tin.type = htonl(RX_DEBUGI_BADTYPE);
	    tin.index = tin.type;
	    rx_packetwrite(ap, 0, sizeof(struct rx_debugIn), (char *)&tin);
	    tl = ap->length;
	    ap->length = sizeof(struct rx_debugIn);
	    rxi_SendDebugPacket(ap, asocket, ahost, aport, istack);
	    ap->length = tl;
	    break;
    }
    return ap;
}

struct rx_packet *rxi_ReceiveVersionPacket(ap, asocket, ahost, aport, istack)
  osi_socket asocket;
  afs_int32 ahost;
  short aport;
  register struct rx_packet *ap;
  int istack;
{
  afs_int32 tl;
	rx_packetwrite(ap, 0, 65, cml_version_number+4);
        tl = ap->length;
	ap->length = 65;
	rxi_SendDebugPacket(ap, asocket, ahost, aport, istack);
        ap->length = tl;
	return ap;
}


/* send a debug packet back to the sender */
static void rxi_SendDebugPacket(struct rx_packet *apacket, osi_socket asocket,
			       afs_int32 ahost, short aport, afs_int32 istack)
{
    struct sockaddr_in taddr;
    int i;
    int nbytes;
    int saven = 0;
    size_t savelen;
#ifdef KERNEL
    int waslocked = ISAFS_GLOCK();
#endif

    taddr.sin_family = AF_INET;
    taddr.sin_port = aport;
    taddr.sin_addr.s_addr = ahost;


    /* We need to trim the niovecs. */
    nbytes = apacket->length;
    for (i=1; i < apacket->niovecs; i++) {
      if (nbytes <= apacket->wirevec[i].iov_len) {
	savelen = apacket->wirevec[i].iov_len;
	saven = apacket->niovecs;
	apacket->wirevec[i].iov_len = nbytes;
	apacket->niovecs = i+1;   /* so condition fails because i == niovecs */
      }
      else nbytes -= apacket->wirevec[i].iov_len;
    }
    AFS_RXGUNLOCK();
#ifdef KERNEL
    if (waslocked) AFS_GUNLOCK();
#endif
    /* debug packets are not reliably delivered, hence the cast below. */
    (void) osi_NetSend(asocket, &taddr, apacket->wirevec, apacket->niovecs,
		       apacket->length+RX_HEADER_SIZE, istack);
#ifdef KERNEL
    if (waslocked) AFS_GLOCK();
#endif
    AFS_RXGLOCK();
    if (saven) {  /* means we truncated the packet above. */
      apacket->wirevec[i-1].iov_len = savelen;
      apacket->niovecs = saven;
    }

}

/* Send the packet to appropriate destination for the specified
 * connection.  The header is first encoded and placed in the packet.
 */
void rxi_SendPacket(struct rx_connection * conn, struct rx_packet *p,
		    int istack)
{
    int waslocked;
    struct sockaddr_in addr;
    register struct rx_peer *peer = conn->peer;
    osi_socket socket;
#ifdef RXDEBUG
    char deliveryType = 'S';
#endif
    /* The address we're sending the packet to */
    addr.sin_family = AF_INET;
    addr.sin_port = peer->port;
    addr.sin_addr.s_addr = peer->host;

    /* This stuff should be revamped, I think, so that most, if not
     * all, of the header stuff is always added here.  We could
     * probably do away with the encode/decode routines. XXXXX */

    /* Stamp each packet with a unique serial number.  The serial
     * number is maintained on a connection basis because some types
     * of security may be based on the serial number of the packet,
     * and security is handled on a per authenticated-connection
     * basis. */
    /* Pre-increment, to guarantee no zero serial number; a zero
     * serial number means the packet was never sent. */
    MUTEX_ENTER(&conn->conn_data_lock);
    p->header.serial = ++conn->serial; 
    MUTEX_EXIT(&conn->conn_data_lock);
    /* This is so we can adjust retransmit time-outs better in the face of 
     * rapidly changing round-trip times.  RTO estimation is not a la Karn.
     */
    if (p->firstSerial == 0) {
       p->firstSerial = p->header.serial;
     }

#ifdef RXDEBUG
    /* If an output tracer function is defined, call it with the packet and
     * network address.  Note this function may modify its arguments. */
    if (rx_almostSent) {
	int drop = (*rx_almostSent) (p, &addr);
	/* drop packet if return value is non-zero? */
	if (drop) deliveryType = 'D';	/* Drop the packet */
    }
#endif

    /* Get network byte order header */
    rxi_EncodePacketHeader(p);	/* XXX in the event of rexmit, etc, don't need to 
				 * touch ALL the fields */

    /* Send the packet out on the same socket that related packets are being
     * received on */
    socket = (conn->type == RX_CLIENT_CONNECTION
	      ? rx_socket : conn->service->socket);

#ifdef RXDEBUG
    /* Possibly drop this packet,  for testing purposes */
    if ((deliveryType == 'D') ||
	((rx_intentionallyDroppedPacketsPer100 > 0) &&
	 (random() % 100 < rx_intentionallyDroppedPacketsPer100))) {
	deliveryType = 'D';		/* Drop the packet */
    }
    else {
	deliveryType = 'S';		/* Send the packet */
#endif /* RXDEBUG */

	/* Loop until the packet is sent.  We'd prefer just to use a
         * blocking socket, but unfortunately the interface doesn't
         * allow us to have the socket block in send mode, and not
         * block in receive mode */
	AFS_RXGUNLOCK();
#ifdef KERNEL
	waslocked = ISAFS_GLOCK();
	if (waslocked) AFS_GUNLOCK();
#endif
	if (osi_NetSend(socket, &addr, p->wirevec, p->niovecs, 
			p->length+RX_HEADER_SIZE, istack)){
	  /* send failed, so let's hurry up the resend, eh? */
	  MUTEX_ENTER(&rx_stats_mutex);
	  rx_stats.netSendFailures++;	   
	  MUTEX_EXIT(&rx_stats_mutex);
	  p->retryTime = p->timeSent;  /* resend it very soon */
	  clock_Addmsec(&(p->retryTime), 10 + (((afs_uint32) p->backoff) << 8));
	}
#ifdef KERNEL
	if (waslocked) AFS_GLOCK();
#endif
	AFS_RXGLOCK();
#ifdef RXDEBUG    
    }
    dpf(("%c %d %s: %x.%u.%u.%u.%u.%u.%u flags %d, packet %x resend %d.%0.3d len %d",
	 deliveryType, p->header.serial, rx_packetTypes[p->header.type-1],
	 peer->host, peer->port, p->header.serial, p->header.epoch,
	 p->header.cid, p->header.callNumber, p->header.seq, p->header.flags,
	 p, p->retryTime.sec, p->retryTime.usec/1000, p->length));
#endif
    MUTEX_ENTER(&rx_stats_mutex);
    rx_stats.packetsSent[p->header.type-1]++;
    MUTEX_EXIT(&rx_stats_mutex);
    MUTEX_ENTER(&peer->peer_lock);
    hadd32(peer->bytesSent, p->length);
    MUTEX_EXIT(&peer->peer_lock);
}

/* Send a list of packets to appropriate destination for the specified
 * connection.  The headers are first encoded and placed in the packets.
 */
void rxi_SendPacketList(struct rx_connection * conn,
			struct rx_packet **list,
			int len,
			int istack)
{
    int waslocked;
    struct sockaddr_in addr;
    register struct rx_peer *peer = conn->peer;
    osi_socket socket;
    struct rx_packet *p;
    struct iovec wirevec[RX_MAXIOVECS];
    int i, length;
    afs_uint32 serial;
    afs_uint32 temp;
    struct rx_jumboHeader *jp;
#ifdef RXDEBUG
    char deliveryType = 'S';
#endif
    /* The address we're sending the packet to */
    addr.sin_family = AF_INET;
    addr.sin_port = peer->port;
    addr.sin_addr.s_addr = peer->host;

    if (len+1 > RX_MAXIOVECS) {
	osi_Panic("rxi_SendPacketList, len > RX_MAXIOVECS\n");
    }

    /*
     * Stamp the packets in this jumbogram with consecutive serial numbers
     */
    MUTEX_ENTER(&conn->conn_data_lock);
    serial = conn->serial; 
    conn->serial += len;
    MUTEX_EXIT(&conn->conn_data_lock);


    /* This stuff should be revamped, I think, so that most, if not
     * all, of the header stuff is always added here.  We could
     * probably do away with the encode/decode routines. XXXXX */

    jp = NULL;
    length = RX_HEADER_SIZE;
    wirevec[0].iov_base = (char *)(&list[0]->wirehead[0]);
    wirevec[0].iov_len = RX_HEADER_SIZE;
    for (i = 0 ; i < len ; i++) {
        p = list[i];

	/* The whole 3.5 jumbogram scheme relies on packets fitting
         * in a single packet buffer. */
	if (p->niovecs > 2) {
	    osi_Panic("rxi_SendPacketList, niovecs > 2\n");
	}

	/* Set the RX_JUMBO_PACKET flags in all but the last packets
	 * in this chunk.  */
	if (i < len-1) {
	    if (p->length != RX_JUMBOBUFFERSIZE) {
		osi_Panic("rxi_SendPacketList, length != jumbo size\n");
	    }
	    p->header.flags |= RX_JUMBO_PACKET;
	    length += RX_JUMBOBUFFERSIZE + RX_JUMBOHEADERSIZE;
	    wirevec[i+1].iov_len = RX_JUMBOBUFFERSIZE + RX_JUMBOHEADERSIZE;
	} else {
	    wirevec[i+1].iov_len = p->length;
	    length += p->length;
	}
	wirevec[i+1].iov_base = (char *)(&p->localdata[0]);
	if (jp != NULL) {
	    /* Convert jumbo packet header to network byte order */
	    temp = (afs_uint32)(p->header.flags) << 24;
	    temp |= (afs_uint32)(p->header.spare);
	    *(afs_uint32 *)jp = htonl(temp);
	}
	jp = (struct rx_jumboHeader *)
	     ((char *)(&p->localdata[0]) + RX_JUMBOBUFFERSIZE);

        /* Stamp each packet with a unique serial number.  The serial
         * number is maintained on a connection basis because some types
         * of security may be based on the serial number of the packet,
         * and security is handled on a per authenticated-connection
         * basis. */
        /* Pre-increment, to guarantee no zero serial number; a zero
         * serial number means the packet was never sent. */
        p->header.serial = ++serial; 
        /* This is so we can adjust retransmit time-outs better in the face of 
         * rapidly changing round-trip times.  RTO estimation is not a la Karn.
         */
        if (p->firstSerial == 0) {
           p->firstSerial = p->header.serial;
        }

#ifdef RXDEBUG
        /* If an output tracer function is defined, call it with the packet and
         * network address.  Note this function may modify its arguments. */
        if (rx_almostSent) {
	    int drop = (*rx_almostSent) (p, &addr);
	    /* drop packet if return value is non-zero? */
	    if (drop) deliveryType = 'D';	/* Drop the packet */
        }
#endif

        /* Get network byte order header */
        rxi_EncodePacketHeader(p);	/* XXX in the event of rexmit, etc, don't need to 
				     * touch ALL the fields */
    }

    /* Send the packet out on the same socket that related packets are being
     * received on */
    socket = (conn->type == RX_CLIENT_CONNECTION
	      ? rx_socket : conn->service->socket);

#ifdef RXDEBUG
    /* Possibly drop this packet,  for testing purposes */
    if ((deliveryType == 'D') ||
	((rx_intentionallyDroppedPacketsPer100 > 0) &&
	 (random() % 100 < rx_intentionallyDroppedPacketsPer100))) {
	deliveryType = 'D';		/* Drop the packet */
    }
    else {
	deliveryType = 'S';		/* Send the packet */
#endif /* RXDEBUG */

	/* Loop until the packet is sent.  We'd prefer just to use a
         * blocking socket, but unfortunately the interface doesn't
         * allow us to have the socket block in send mode, and not
         * block in receive mode */
	AFS_RXGUNLOCK();
#if	defined(AFS_SUN5_ENV) && defined(KERNEL)
	waslocked = ISAFS_GLOCK();
	if (!istack && waslocked) AFS_GUNLOCK();
#endif
	if (osi_NetSend(socket, &addr, &wirevec[0], len+1, length, istack)){
	  /* send failed, so let's hurry up the resend, eh? */
	  MUTEX_ENTER(&rx_stats_mutex);
	  rx_stats.netSendFailures++;	   
	  MUTEX_EXIT(&rx_stats_mutex);
	  for (i = 0 ; i < len ; i++) {
	    p = list[i];
	    p->retryTime = p->timeSent;  /* resend it very soon */
	    clock_Addmsec(&(p->retryTime), 10 + (((afs_uint32) p->backoff) << 8));
	  }
	}
#if	defined(AFS_SUN5_ENV) && defined(KERNEL)
	if (!istack && waslocked) AFS_GLOCK();
#endif
	AFS_RXGLOCK();
#ifdef RXDEBUG    
    }
    dpf(("%c %d %s: %x.%u.%u.%u.%u.%u.%u flags %d, packet %x resend %d.%0.3d len %d",
	 deliveryType, p->header.serial, rx_packetTypes[p->header.type-1],
	 peer->host, peer->port, p->header.serial, p->header.epoch,
	 p->header.cid, p->header.callNumber, p->header.seq, p->header.flags,
	 p, p->retryTime.sec, p->retryTime.usec/1000, p->length));
#endif
    MUTEX_ENTER(&rx_stats_mutex);
    rx_stats.packetsSent[p->header.type-1]++;
    MUTEX_EXIT(&rx_stats_mutex);
    MUTEX_ENTER(&peer->peer_lock);
    hadd32(peer->bytesSent, p->length);
    MUTEX_EXIT(&peer->peer_lock);
}


/* Send a "special" packet to the peer connection.  If call is
 * specified, then the packet is directed to a specific call channel
 * associated with the connection, otherwise it is directed to the
 * connection only. Uses optionalPacket if it is supplied, rather than
 * allocating a new packet buffer.  Nbytes is the length of the data
 * portion of the packet.  If data is non-null, nbytes of data are
 * copied into the packet.  Type is the type of the packet, as defined
 * in rx.h.  Bug: there's a lot of duplication between this and other
 * routines.  This needs to be cleaned up. */
struct rx_packet *
rxi_SendSpecial(call, conn, optionalPacket, type, data, nbytes, istack)
    register struct rx_call *call;
    register struct rx_connection *conn;
    struct rx_packet *optionalPacket;
    int type;
    char *data;
    int nbytes, istack;
{
    /* Some of the following stuff should be common code for all
     * packet sends (it's repeated elsewhere) */
    register struct rx_packet *p;
    unsigned int i = 0;
    int savelen, saven = 0;
    int channel, callNumber;
    if (call) {
	channel = call->channel;
	callNumber = *call->callNumber;
	/* BUSY packets refer to the next call on this connection */
	if (type == RX_PACKET_TYPE_BUSY) {
	    callNumber++;
	}
    } else {
	channel = 0;
	callNumber = 0;
    }
    p = optionalPacket;
    if (!p) {
	p = rxi_AllocPacket(RX_PACKET_CLASS_SPECIAL);
	if (!p) osi_Panic("rxi_SendSpecial failure");
    }

    if (nbytes != -1) 
      p->length = nbytes;
    else
      nbytes = p->length;
    p->header.serviceId = conn->serviceId;
    p->header.securityIndex = conn->securityIndex;
    p->header.cid = (conn->cid | channel);
    p->header.callNumber = callNumber;
    p->header.seq = 0;
    p->header.epoch = conn->epoch;
    p->header.type = type;
    p->header.flags = 0;
    if (conn->type == RX_CLIENT_CONNECTION) 
       p->header.flags |= RX_CLIENT_INITIATED;
    if (data) 
      rx_packetwrite(p, 0, nbytes, data);

    for (i=1; i < p->niovecs; i++) {
      if (nbytes <= p->wirevec[i].iov_len) {
	savelen = p->wirevec[i].iov_len;
	saven = p->niovecs;
	p->wirevec[i].iov_len = nbytes;
	p->niovecs = i+1;   /* so condition fails because i == niovecs */
      }
      else nbytes -= p->wirevec[i].iov_len;
    }

    if (call) rxi_Send(call, p, istack);
    else rxi_SendPacket(conn, p, istack);
    if (saven) {  /* means we truncated the packet above.  We probably don't  */
      /* really need to do this, but it seems safer this way, given that  */
      /* sneaky optionalPacket... */
      p->wirevec[i-1].iov_len = savelen;
      p->niovecs = saven;
    }
    if (!optionalPacket) rxi_FreePacket(p);
    return optionalPacket;
}


/* Encode the packet's header (from the struct header in the packet to
 * the net byte order representation in the wire representation of the
 * packet, which is what is actually sent out on the wire) */
void rxi_EncodePacketHeader(p)
register struct rx_packet *p;
{
    register afs_uint32 *buf = (afs_uint32 *)(p->wirevec[0].iov_base);      /* MTUXXX */

    bzero((char *)buf, RX_HEADER_SIZE);
    *buf++ = htonl(p->header.epoch);
    *buf++ = htonl(p->header.cid);
    *buf++ = htonl(p->header.callNumber);
    *buf++ = htonl(p->header.seq);
    *buf++ = htonl(p->header.serial);
    *buf++ = htonl(  (((afs_uint32)p->header.type)<<24) 
		   | (((afs_uint32)p->header.flags)<<16) 
		   | (p->header.userStatus<<8) | p->header.securityIndex);
    /* Note: top 16 bits of this next word were reserved */
    *buf++ = htonl((p->header.spare << 16) | (p->header.serviceId&0xffff));
}

/* Decode the packet's header (from net byte order to a struct header) */
void rxi_DecodePacketHeader(p)
register struct rx_packet *p;
{
    register afs_uint32 *buf = (afs_uint32*)(p->wirevec[0].iov_base);      /* MTUXXX */
    afs_uint32 temp;

    p->header.epoch = ntohl(*buf++);
    p->header.cid = ntohl(*buf++);
    p->header.callNumber = ntohl(*buf++);
    p->header.seq = ntohl(*buf++);
    p->header.serial = ntohl(*buf++);
    temp = ntohl(*buf++);
    /* C will truncate byte fields to bytes for me */
    p->header.type = temp>>24;
    p->header.flags = temp>>16;
    p->header.userStatus = temp>>8;
    p->header.securityIndex = temp>>0;
    temp = ntohl(*buf++);
    p->header.serviceId = (temp&0xffff);
    p->header.spare = temp>>16;
    /* Note: top 16 bits of this last word are the security checksum */
}

void rxi_PrepareSendPacket(call, p, last)
    register struct rx_call *call;
    register struct rx_packet *p;
    register int last;
{
    register struct rx_connection *conn = call->conn;
    int i, j;
    ssize_t len;	/* len must be a signed type; it can go negative */

    p->acked = 0;
    p->header.cid = (conn->cid | call->channel);
    p->header.serviceId = conn->serviceId;
    p->header.securityIndex = conn->securityIndex;
    p->header.callNumber = *call->callNumber;
    p->header.seq = call->tnext++;
    p->header.epoch = conn->epoch;
    p->header.type = RX_PACKET_TYPE_DATA;
    p->header.flags = 0;
    p->header.spare = 0;
    if (conn->type == RX_CLIENT_CONNECTION) 
      p->header.flags |= RX_CLIENT_INITIATED;

    if (last)
      p->header.flags |= RX_LAST_PACKET;

    clock_Zero(&p->retryTime); /* Never yet transmitted */
    clock_Zero(&p->firstSent); /* Never yet transmitted */
    p->header.serial = 0;      /* Another way of saying never transmitted... */
    p->backoff = 0;

    /* Now that we're sure this is the last data on the call, make sure
     * that the "length" and the sum of the iov_lens matches. */
    len = p->length + call->conn->securityHeaderSize;

    for (i=1; i < p->niovecs && len > 0; i++) {
      len -=  p->wirevec[i].iov_len;
    }
    if (len > 0) {
      osi_Panic("PrepareSendPacket 1\n"); /* MTUXXX */
    }
    else {
      /* Free any extra elements in the wirevec */
      for (j = MAX(2,i) ; j < p->niovecs ; j++) {
	rxi_freeCBuf(RX_CBUF_TO_PACKET(p->wirevec[j].iov_base, p));
      }
      p->niovecs = i;
      p->wirevec[i-1].iov_len += len;
    }
    RXS_PreparePacket(conn->securityObject, call, p);
}

/* Given an interface MTU size, calculate an adjusted MTU size that
 * will make efficient use of the RX buffers when the peer is sending
 * either AFS 3.4a jumbograms or AFS 3.5 jumbograms.  */
int rxi_AdjustIfMTU(int mtu)
{
    int adjMTU;
    int frags;

    adjMTU = RX_HEADER_SIZE + RX_JUMBOBUFFERSIZE + RX_JUMBOHEADERSIZE;
    if (mtu <= adjMTU) {
	return mtu;
    }
    mtu -= adjMTU;
    if (mtu <= 0) {
	return adjMTU;
    }
    frags = mtu / (RX_JUMBOBUFFERSIZE + RX_JUMBOHEADERSIZE);
    return (adjMTU + (frags * (RX_JUMBOBUFFERSIZE + RX_JUMBOHEADERSIZE)));
}

/* Given an interface MTU size, and the peer's advertised max receive
 * size, calculate an adjisted maxMTU size that makes efficient use
 * of our packet buffers when we are sending AFS 3.4a jumbograms. */
int rxi_AdjustMaxMTU(int mtu, int peerMaxMTU)
{
    int maxMTU = mtu * rxi_nSendFrags;
    maxMTU = MIN(maxMTU, peerMaxMTU);
    return rxi_AdjustIfMTU(maxMTU);
}

/* Given a packet size, figure out how many datagram packet will fit.
 * The first buffer always contains RX_HEADER_SIZE+RX_JUMBOBUFFERSIZE+
 * RX_JUMBOHEADERSIZE, the middle buffers contain RX_JUMBOBUFFERSIZE+
 * RX_JUMBOHEADERSIZE, and the last buffer contains RX_JUMBOBUFFERSIZE */
int rxi_AdjustDgramPackets(int frags, int mtu)
{
    int maxMTU;
    if (mtu + IPv6_FRAG_HDR_SIZE < RX_JUMBOBUFFERSIZE + RX_HEADER_SIZE) {
	return 1;
    }
    maxMTU = (frags * (mtu + UDP_HDR_SIZE)) - UDP_HDR_SIZE;
    maxMTU = MIN(maxMTU, RX_MAX_PACKET_SIZE);
    /* subtract the size of the first and last packets */
    maxMTU -= RX_HEADER_SIZE + (2 * RX_JUMBOBUFFERSIZE) + RX_JUMBOHEADERSIZE;
    if (maxMTU < 0) {
	return 1;
    }
    return (2 + (maxMTU / (RX_JUMBOBUFFERSIZE + RX_JUMBOHEADERSIZE)));
}
