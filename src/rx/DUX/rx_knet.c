/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*        Copyright Transarc Corporation 1989 - All Rights Reserved         *
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

#include "../afs/param.h"
#ifdef AFS_DUX40_ENV
#include "../rx/rx_kcommon.h"


static struct protosw parent_proto;	/* udp proto switch */
static void rxk_input (struct mbuf *am, int iphlen);
static void rxk_fasttimo (void);

/* start intercepting basic calls */
rxk_init() {
    register struct protosw *tpro, *last;
    if (rxk_initDone) return 0;

    last = inetdomain.dom_protoswNPROTOSW;
    for (tpro = inetdomain.dom_protosw; tpro < last; tpro++)
      if (tpro->pr_protocol == IPPROTO_UDP) {
	/* force UDP checksumming on for AFS	*/
	 extern int udpcksum;
	 udpcksum = 1;	
          bcopy(tpro, &parent_proto, sizeof(parent_proto));
          tpro->pr_input = rxk_input;
          tpro->pr_fasttimo = rxk_fasttimo;
          /*
           * don't bother with pr_drain and pr_ctlinput
           * until we have something to do
           */
          rxk_initDone = 1;
          return 0;
      }
    osi_Panic("inet:no udp");
}


static void rxk_input (struct mbuf *am, int iphlen)
{
    void (*tproc)();
    register unsigned short *tsp;
    int hdr;
    struct udphdr *tu;
    register struct ip *ti;
    struct udpiphdr *tvu;
    register int i;
    char *phandle;
    afs_int32 code;
    struct sockaddr_in taddr;
    int tlen;
    short port;
    int data_len, comp_sum;

    SPLVAR;
    NETPRI;

    /* make sure we have base ip and udp headers in first mbuf */
    if (iphlen > sizeof (struct ip)) {
        ip_stripoptions(am, (struct mbuf *)0, (struct ipoption *)0);
        iphlen = sizeof (struct ip);
    }

    if (am->m_len < sizeof(struct udpiphdr)) {
        am = m_pullup(am, sizeof(struct udpiphdr));
        if (!am) {
	    USERPRI;
	    return;
        }
    }

    ti = mtod(am, struct ip *);
    /* skip basic ip hdr */
    tu = (struct udphdr *)(((char *)ti) + sizeof(struct ip)); 
    
    /* now read the port out */
    port = tu->uh_dport;

    if (port) {
	for(tsp=rxk_ports, i=0; i<MAXRXPORTS;i++) {
	    if (*tsp++ == port) {
		/* checksum the packet */
		/*
		 * Make mbuf data length reflect UDP length.
		 * If not enough data to reflect UDP length, drop.
		 */
		tvu = (struct udpiphdr *)ti;
		tlen = ntohs((u_short)tvu->ui_ulen);
		if ((int)ti->ip_len != tlen) {
		    if (tlen > (int)ti->ip_len) {
			m_free(am);
			USERPRI;
			return;
		    }
		    m_adj(am, tlen - (int)ti->ip_len);
		}
		/* deliver packet to rx */
		taddr.sin_family = AF_INET;	    /* compute source address */
		taddr.sin_port = tu->uh_sport;
		taddr.sin_addr.s_addr = ti->ip_src.s_addr;
		taddr.sin_len = sizeof(taddr);
		tvu = (struct udpiphdr *) ti;	/* virtual udp structure, for cksum */
		/* handle the checksum.  Note that this code damages the actual ip
		   header (replacing it with the virtual one, which is the same size),
		   so we must ensure we get everything out we need, first */
		if ( tu->uh_sum != 0) {
			/* if the checksum is there, always check it. It's crazy not
			 * to, unless you can really be sure that your
			 * underlying network (and interfaces and drivers and
			 * DMA hardware, etc!) is error-free. First, fill
			 * in entire virtual ip header. */
			tvu->ui_i.fill[0] = 0;
			tvu->ui_i.fill[1] = 0;
			tvu->ui_x1 = 0;
			tvu->ui_len = tvu->ui_ulen;
			tlen = ntohs((unsigned short)(tvu->ui_ulen));
			if (in_cksum(am, sizeof(struct ip) + tlen)) {
			    /* checksum, including cksum field, doesn't come out 0, so
			       this packet is bad */
			    m_freem(am);
			    USERPRI;
			    return;
			}
		      }

		/*
		 * 28 is IP (20) + UDP (8) header.  ulen includes
		 * udp header, and we *don't* tell RX about udp
		 * header either.  So, we remove those 8 as well.
		 */
		data_len = ntohs(tu->uh_ulen);
		data_len -= 8;
		AFS_RXGLOCK();
		if (!(*rxk_GetPacketProc)(&phandle, data_len)) {
		  if (rx_mb_to_packet(am, m_freem, 28, data_len, phandle)) {
		    /* XXX should just increment counter here.. */
		    printf("rx: truncated UDP packet\n");
		    rxi_FreePacket(phandle);
		  }
		  else 
		    (*rxk_PacketArrivalProc)(phandle, &taddr,
					     rxk_portRocks[i], data_len);
		}else m_freem(am);
		AFS_RXGUNLOCK();
		USERPRI;
		return;
		}
	    }
	}

    /* if we get here, try to deliver packet to udp */
    if (tproc = parent_proto.pr_input) (*tproc)(am,iphlen);
    USERPRI;
    return;
}


/* 
 * UDP fast timer to raise events for all but Solaris and NCR. 
 * Called about 5 times per second (at unknown priority?).  Must go to
 * splnet or obtain global lock before touching anything significant.
 */
static void rxk_fasttimo (void)
{
    void (*tproc)();
    struct clock temp;

    /* do rx fasttimo processing here */
    rxevent_RaiseEvents(&temp);
    if (tproc = parent_proto.pr_fasttimo) (*tproc)();
}


/* rx_NetSend - send asize bytes at adata from asocket to host at addr.
 *
 * Now, why do we allocate a new buffer when we could theoretically use the one
 * pointed to by adata?  Because PRU_SEND returns after queueing the message,
 * not after sending it.  If the sender changes the data after queueing it,
 * we'd see the already-queued data change.  One attempt to fix this without
 * adding a copy would be to have this function wait until the datagram is
 * sent; however this doesn't work well.  In particular, if a host is down, and
 * an ARP fails to that host, this packet will be queued until the ARP request
 * comes back, which could be hours later.  We can't block in this routine that
 * long, since it prevents RPC timeouts from happening.
 */
/* XXX In the brave new world, steal the data bufs out of the rx_packet iovec,
 * and just queue those.  XXX
 */

/* set lock on sockbuf sb; can't call sblock since we're at interrupt level
 * sometimes */
static trysblock(sb)    
register struct sockbuf *sb; {
    AFS_STATCNT(trysblock);
    if (sb->sb_flags & SB_LOCK){
	return -1;  /* can't lock socket */
    }
    sb->sb_flags |= SB_LOCK;
    return 0;
}

int 
osi_NetSend(asocket, addr, dvec, nvec, asize, istack)
     register struct socket *asocket;
     struct iovec *dvec;
     int nvec;
     register afs_int32 asize;
     struct sockaddr_in *addr;
     int istack;
{
    register struct mbuf *tm, *um;
    register afs_int32 code;
    int s;
    struct mbuf *top = 0;
    register struct mbuf *m, **mp;
    int len;
    char *tdata;
    caddr_t tpa;
    int i,tl,rlen;
    int mlen;
    int haveGlock;

    AFS_STATCNT(osi_NetSend);

/* Actually, the Ultrix way is as good as any for us, so we don't bother with
 * special mbufs any more.  Used to think we could get away with not copying
 * the data to the interface, but there's no way to tell the caller not to
 * reuse the buffers after sending, so we lost out on that trick anyway */

    s = splnet();
    mp = &top;
    i = 0;
    tdata = dvec[i].iov_base;
    tl = dvec[i].iov_len;
    while (1) {
	mlen = MLEN;
        if (top == 0) {
            MGETHDR(m, M_DONTWAIT, MT_DATA);
            if (!m) {
                splx(s);
                return 1;
            }
            mlen = MHLEN;
            m->m_pkthdr.len = 0;
            m->m_pkthdr.rcvif = (struct ifnet *)0;
        } else
	MGET(m, M_DONTWAIT, MT_DATA);
	if (!m) {
	    /* can't get an mbuf, give up */
	    if (top) m_freem(top);	/* free mbuf list we're building */
	    splx(s);
	    return 1;
	}
	/*
	 * WARNING: the `4 * MLEN' is somewhat dubious.  It is better than
	 * `NBPG', which may have no relation to `CLBYTES'.  Also, `CLBYTES'
	 * may be so large that we never use clusters, resulting in far
	 * too many mbufs being used.  It is often better to briefly use
	 * a cluster, even if we are only using a portion of it.  Since
	 * we are on the xmit side, it shouldn't end up sitting on a queue
	 * for a potentially unbounded time (except perhaps if we are talking
	 * to ourself).
	 */
	if (asize >= 4 * MLEN) {	/* try to get cluster mbuf */
	    register struct mbuf *p;

	    /* different algorithms for getting cluster mbuf */
            MCLGET(m, M_DONTWAIT);
            if ((m->m_flags & M_EXT) == 0)
                goto nopages;
            mlen = MCLBYTES;

	    /* now compute usable size */
            len = MIN(mlen, asize);
/* Should I look at MAPPED_MBUFS??? */
	} else {
nopages:
            len = MIN(mlen, asize);
	}
	m->m_len = 0;
        *mp = m;	/* XXXX */
        top->m_pkthdr.len += len;
	tpa = mtod(m, caddr_t);
	while (len) {
	  rlen = MIN(len, tl);
	  bcopy(tdata, tpa, rlen);
	  asize -= rlen;
	  len -= rlen;
	  tpa += rlen;
	  m->m_len += rlen;
	  tdata += rlen;
	  tl -= rlen;
	  if (tl <= 0) {
	    i++;
	    if (i > nvec) {
	      /* shouldn't come here! */
	      asize = 0;   /* so we make progress toward completion */
	      break;
	    }
	    tdata = dvec[i].iov_base;
	    tl = dvec[i].iov_len;
	  }
	}
	*mp = m;
	mp = &m->m_next;
	if (asize <= 0)
	  break;
    }
    tm = top;

    tm->m_act = (struct mbuf *) 0;

    /* setup mbuf corresponding to destination address */
    um = m_get(M_DONTWAIT, MT_SONAME);
    if (!um) {
	if (top) m_freem(top);	/* free mbuf chain */
	/* if this were vfs40, we'd do sbunlock(asocket, &asocket->so_snd), but
	   we don't do the locking at all for vfs40 systems */
	splx(s);
	return 1;
    }
    bcopy(addr, mtod(um, caddr_t), sizeof(*addr));
    um->m_len = sizeof(*addr);
    /* note that udp_usrreq frees funny mbuf.  We hold onto data, but mbuf
     * around it is gone.  we free address ourselves.  */
    haveGlock = ISAFS_GLOCK();
    if (haveGlock) {
	AFS_GUNLOCK();
    }
    SOCKET_LOCK(asocket);
    code = (*asocket->so_proto->pr_usrreq)(asocket, PRU_SEND, tm, um, 0);
    SOCKET_UNLOCK(asocket);
    if (haveGlock) {
	AFS_GLOCK();
    }
    splx(s);
    m_free(um);

    return code;
}

#endif /* AFS_DUX40_ENV */
