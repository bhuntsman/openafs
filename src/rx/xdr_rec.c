/*  * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
#ifndef	NeXT
#ifndef lint
static char sccsid[] = "@(#)xdr_rec.c 1.1 86/02/03 Copyr 1984 Sun Micro";
#endif

/*  * xdr_rec.c, Implements TCP/IP based XDR streams with a "record marking"
 * layer above tcp (for rpc's use).
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * These routines interface XDRSTREAMS to a tcp/ip connection.
 * There is a record marking layer between the xdr stream
 * and the tcp transport level.  A record is composed on one or more
 * record fragments.  A record fragment is a thirty-two bit header followed
 * by n bytes of data, where n is contained in the header.  The header
 * is represented as a htonl(afs_uint32).  Thegh order bit encodes
 * whether or not the fragment is the last fragment of the record
 * (1 => fragment is last, 0 => more fragments to follow. 
 * The other 31 bits encode the byte length of the fragment.
 */

#include <afs/param.h>
#include <stdio.h>
#include "xdr.h"
#ifndef AFS_NT40_ENV
#include <sys/time.h>
#include <netinet/in.h>
#endif

#ifndef osi_alloc
char *osi_alloc();
#endif
extern afs_int32 lseek();

static u_int	fix_buf_size();

static bool_t	flush_out();
static bool_t	get_input_bytes();
static bool_t	set_input_fragment();
static bool_t	skip_input_bytes();

static bool_t	xdrrec_getint32();
static bool_t	xdrrec_putint32();
static bool_t	xdrrec_getbytes();
static bool_t	xdrrec_putbytes();
static u_int	xdrrec_getpos();
static bool_t	xdrrec_setpos();
static afs_int32 *	xdrrec_inline();
static void	xdrrec_destroy();

static struct  xdr_ops xdrrec_ops = {
	xdrrec_getint32,
	xdrrec_putint32,
	xdrrec_getbytes,
	xdrrec_putbytes,
	xdrrec_getpos,
	xdrrec_setpos,
	xdrrec_inline,
	xdrrec_destroy
};

/*  * A record is composed of one or more record fragments.
 * A record fragment is a two-byte header followed by zero to
 * 2**32-1 bytes.  The header is treated as an afs_int32 unsigned and is
 * encode/decoded to the network via htonl/ntohl.  The low order 31 bits
 * are a byte count of the fragment.  The highest order bit is a boolean:
 * 1 => this fragment is the last fragment of the record,
 * 0 => this fragment is followed by more fragment(s).
 *
 * The fragment/record machinery is not general;  it is constructed to
 * meet the needs of xdr and rpc based on tcp.
 */

#define LAST_FRAG ((afs_uint32)(1 << 31))

typedef struct rec_strm {
	caddr_t tcp_handle;
	/* 	 * out-goung bits
	 */
	int (*writeit)();
	caddr_t out_base;	/* output buffer (points to frag header) */
	caddr_t out_finger;	/* next output position */
	caddr_t out_boundry;	/* data cannot up to this address */
	afs_uint32 *frag_header;	/* beginning of curren fragment */
	bool_t frag_sent;	/* true if buffer sent in middle of record */
	/* 	 * in-coming bits
	 */
	int (*readit)();
	afs_uint32 in_size;	/* fixed size of the input buffer */
	caddr_t in_base;
	caddr_t in_finger;	/* location of next byte to be had */
	caddr_t in_boundry;	/* can read up to this location */
	afs_int32 fbtbc;		/* fragment bytes to be consumed */
	bool_t last_frag;
	u_int sendsize;
	u_int recvsize;
} RECSTREAM;


/*  * Create an xdr handle for xdrrec
 * xdrrec_create fills in xdrs.  Sendsize and recvsize are
 * send and recv buffer sizes (0 => use default).
 * tcp_handle is an opaque handle that is passed as the first parameter to
 * the procedures readit and writeit.  Readit and writeit are read and
 * write respectively.   They are like the system
 * calls expect that they take an opaque handle rather than an fd.
 */
void
xdrrec_create(xdrs, sendsize, recvsize, tcp_handle, readit, writeit)
	register XDR *xdrs;
	u_int sendsize;
	u_int recvsize;
	caddr_t tcp_handle;
	int (*readit)();  /* like read, but pass it a tcp_handle, not sock */
	int (*writeit)();  /* like write, but pass it a tcp_handle, not sock */
{
	register RECSTREAM *rstrm =
	    (RECSTREAM *)osi_alloc(sizeof(RECSTREAM));

	if (rstrm == NULL) {
		/* 
		 *  This is bad.  Should rework xdrrec_create to 
		 *  return a handle, and in this case return NULL
		 */
		return;
	}
	xdrs->x_ops = &xdrrec_ops;
	xdrs->x_private = (caddr_t)rstrm;
	rstrm->tcp_handle = tcp_handle;
	rstrm->readit = readit;
	rstrm->writeit = writeit;
	sendsize = fix_buf_size(sendsize);
	if ((rstrm->out_base = rstrm->out_finger = rstrm->out_boundry =
	    osi_alloc(sendsize)) == NULL) {
		return;
	}
	rstrm->frag_header = (afs_uint32 *)rstrm->out_base;
	rstrm->out_finger += sizeof(afs_uint32);
	rstrm->out_boundry += sendsize;
	rstrm->frag_sent = FALSE;
	rstrm->in_size = recvsize = fix_buf_size(recvsize);
	if ((rstrm->in_base = rstrm->in_boundry=osi_alloc(recvsize)) == NULL) {
		return;
	}
	rstrm->in_finger = (rstrm->in_boundry += recvsize);
	rstrm->fbtbc = 0;
	rstrm->last_frag = TRUE;
	rstrm->sendsize = sendsize;
	rstrm->recvsize = recvsize;
}


/*  * The reoutines defined below are the xdr ops which will go into the
 * xdr handle filled in by xdrrec_create.
 */

static bool_t
xdrrec_getint32(xdrs, lp)
	XDR *xdrs;
	afs_int32 *lp;
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register afs_int32 *buflp = (afs_int32 *)(rstrm->in_finger);
	afs_int32 myint32;

	/* first try the inline, fast case */
	if ((rstrm->fbtbc >= sizeof(afs_int32)) &&
	    (((int)rstrm->in_boundry - (int)buflp) >= sizeof(afs_int32))) {
		*lp = ntohl(*buflp);
		rstrm->fbtbc -= sizeof(afs_int32);
		rstrm->in_finger += sizeof(afs_int32);
	} else {
		if (! xdrrec_getbytes(xdrs, (caddr_t)&myint32, sizeof(afs_int32)))
			return (FALSE);
		*lp = ntohl(myint32);
	}
	return (TRUE);
}

static bool_t
xdrrec_putint32(xdrs, lp)
	XDR *xdrs;
	afs_int32 *lp;
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register afs_int32 *dest_lp = ((afs_int32 *)(rstrm->out_finger));

	if ((rstrm->out_finger += sizeof(afs_int32)) > rstrm->out_boundry) {
		/*
		 * this case should almost never happen so the code is
		 * inefficient
		 */
		rstrm->out_finger -= sizeof(afs_int32);
		rstrm->frag_sent = TRUE;
		if (! flush_out(rstrm, FALSE))
			return (FALSE);
		dest_lp = ((afs_int32 *)(rstrm->out_finger));
		rstrm->out_finger += sizeof(afs_int32);
	}
	*dest_lp = htonl(*lp);
	return (TRUE);
}

static bool_t  /* must manage buffers, fragments, and records */
xdrrec_getbytes(xdrs, addr, len)
	XDR *xdrs;
	register caddr_t addr;
	register u_int len;
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register int current;

	while (len > 0) {
		current = rstrm->fbtbc;
		if (current == 0) {
			if (rstrm->last_frag)
				return (FALSE);
			if (! set_input_fragment(rstrm))
				return (FALSE);
			continue;
		}
		current = (len < current) ? len : current;
		if (! get_input_bytes(rstrm, addr, current))
			return (FALSE);
		addr += current; 
		rstrm->fbtbc -= current;
		len -= current;
	}
	return (TRUE);
}

static bool_t
xdrrec_putbytes(xdrs, addr, len)
	XDR *xdrs;
	register caddr_t addr;
	register u_int len;
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register int current;

	while (len > 0) {
		current = (u_int)rstrm->out_boundry - (u_int)rstrm->out_finger;
		current = (len < current) ? len : current;
		bcopy(addr, rstrm->out_finger, current);
		rstrm->out_finger += current;
		addr += current;
		len -= current;
		if (rstrm->out_finger == rstrm->out_boundry) {
			rstrm->frag_sent = TRUE;
			if (! flush_out(rstrm, FALSE))
				return (FALSE);
		}
	}
	return (TRUE);
}

static u_int
xdrrec_getpos(xdrs)
	register XDR *xdrs;
{
	register RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;
	register u_int pos;

	pos = (u_int) lseek((int)rstrm->tcp_handle, 0, 1);
	if ((int)pos != -1)
		switch (xdrs->x_op) {

		case XDR_ENCODE:
			pos += rstrm->out_finger - rstrm->out_base;
			break;

		case XDR_DECODE:
			pos -= rstrm->in_boundry - rstrm->in_finger;
			break;

		default:
			pos = (u_int) -1;
			break;
		}
	return (pos);
}

static bool_t
xdrrec_setpos(xdrs, pos)
	register XDR *xdrs;
	u_int pos;
{
	register RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;
	u_int currpos = xdrrec_getpos(xdrs);
	int delta = currpos - pos;
	caddr_t newpos;

	if ((int)currpos != -1)
		switch (xdrs->x_op) {

		case XDR_ENCODE:
			newpos = rstrm->out_finger - delta;
			if ((newpos > (caddr_t)(rstrm->frag_header)) &&
			    (newpos < rstrm->out_boundry)) {
				rstrm->out_finger = newpos;
				return (TRUE);
			}
			break;

		case XDR_DECODE:
			newpos = rstrm->in_finger - delta;
			if ((delta < (int)(rstrm->fbtbc)) &&
			    (newpos <= rstrm->in_boundry) &&
			    (newpos >= rstrm->in_base)) {
				rstrm->in_finger = newpos;
				rstrm->fbtbc -= delta;
				return (TRUE);
			}
			break;
		}
	return (FALSE);
}

static afs_int32 *
xdrrec_inline(xdrs, len)
	register XDR *xdrs;
	int len;
{
	register RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;
	afs_int32 * buf = NULL;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		if ((rstrm->out_finger + len) <= rstrm->out_boundry) {
			buf = (afs_int32 *) rstrm->out_finger;
			rstrm->out_finger += len;
		}
		break;

	case XDR_DECODE:
		if ((len <= rstrm->fbtbc) &&
		    ((rstrm->in_finger + len) <= rstrm->in_boundry)) {
			buf = (afs_int32 *) rstrm->in_finger;
			rstrm->fbtbc -= len;
			rstrm->in_finger += len;
		}
		break;
	}
	return (buf);
}

static void
xdrrec_destroy(xdrs)
	register XDR *xdrs;
{
	register RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;

	osi_free(rstrm->out_base, rstrm->sendsize);
	osi_free(rstrm->in_base, rstrm->recvsize);
	osi_free((caddr_t)rstrm, sizeof(RECSTREAM));
}


/*
 * Exported routines to manage xdr records
 */

/*
 * Before reading (deserializing from the stream, one should always call
 * this procedure to guarantee proper record alignment.
 */
bool_t
xdrrec_skiprecord(xdrs)
	XDR *xdrs;
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);

	while (rstrm->fbtbc > 0 || (! rstrm->last_frag)) {
		if (! skip_input_bytes(rstrm, rstrm->fbtbc))
			return (FALSE);
		rstrm->fbtbc = 0;
		if ((! rstrm->last_frag) && (! set_input_fragment(rstrm)))
			return (FALSE);
	}
	rstrm->last_frag = FALSE;
	return (TRUE);
}

/*
 * Look ahead fuction.
 * Returns TRUE iff there is no more input in the buffer 
 * after consuming the rest of the current record.
 */
bool_t
xdrrec_eof(xdrs)
	XDR *xdrs;
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);

	while (rstrm->fbtbc > 0 || (! rstrm->last_frag)) {
		if (! skip_input_bytes(rstrm, rstrm->fbtbc))
			return (TRUE);
		rstrm->fbtbc = 0;
		if ((! rstrm->last_frag) && (! set_input_fragment(rstrm)))
			return (TRUE);
	}
	if (rstrm->in_finger == rstrm->in_boundry)
		return (TRUE);
	return (FALSE);
}

/*
 * The client must tell the package when an end-of-record has occurred.
 * The second paraemters tells whether the record should be flushed to the
 * (output) tcp stream.  (This let's the package support batched or
 * pipelined procedure calls.)  TRUE => immmediate flush to tcp connection.
 */
bool_t
xdrrec_endofrecord(xdrs, sendnow)
	XDR *xdrs;
	bool_t sendnow;
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register afs_uint32 len;  /* fragment length */

	if (sendnow || rstrm->frag_sent ||
	    ((afs_uint32)rstrm->out_finger + sizeof(afs_uint32) >=
	    (afs_uint32)rstrm->out_boundry)) {
		rstrm->frag_sent = FALSE;
		return (flush_out(rstrm, TRUE));
	}
	len = (afs_uint32)(rstrm->out_finger) - (afs_uint32)(rstrm->frag_header) -
	   sizeof(afs_uint32);
	*(rstrm->frag_header) = htonl(len | LAST_FRAG);
	rstrm->frag_header = (afs_uint32 *)rstrm->out_finger;
	rstrm->out_finger += sizeof(afs_uint32);
	return (TRUE);
}


/*
 * Internal useful routines
 */
static bool_t
flush_out(rstrm, eor)
	register RECSTREAM *rstrm;
	bool_t eor;
{
	register afs_uint32 eormask = (eor == TRUE) ? LAST_FRAG : 0;
	register afs_uint32 len = (afs_uint32)(rstrm->out_finger) - 
	    (afs_uint32)(rstrm->frag_header) - sizeof(afs_uint32);

	*(rstrm->frag_header) = htonl(len | eormask);
	len = (afs_uint32)(rstrm->out_finger) - (afs_uint32)(rstrm->out_base);
	if ((*(rstrm->writeit))(rstrm->tcp_handle, rstrm->out_base, (int)len)
	    != (int)len)
		return (FALSE);
	rstrm->frag_header = (afs_uint32 *)rstrm->out_base;
	rstrm->out_finger = (caddr_t)rstrm->out_base + sizeof(afs_uint32);
	return (TRUE);
}

static bool_t  /* knows nothing about records!  Only about input buffers */
fill_input_buf(rstrm)
	register RECSTREAM *rstrm;
{
	register caddr_t where = rstrm->in_base;
	register int len = rstrm->in_size;
	u_int adjust = (u_int)rstrm->in_boundry % BYTES_PER_XDR_UNIT;

	/* Bump the current position out to the next alignment boundary*/
	where += adjust;
	len -= adjust;

	if ((len = (*(rstrm->readit))(rstrm->tcp_handle, where, len)) == -1)
		return (FALSE);
	rstrm->in_finger = where;
	where += len;
	rstrm->in_boundry = where;
	return (TRUE);
}

static bool_t  /* knows nothing about records!  Only about input buffers */
get_input_bytes(rstrm, addr, len)
	register RECSTREAM *rstrm;
	register caddr_t addr;
	register int len;
{
	register int current;

	while (len > 0) {
		current = (int)rstrm->in_boundry - (int)rstrm->in_finger;
		if (current == 0) {
			if (! fill_input_buf(rstrm))
				return (FALSE);
			continue;
		}
		current = (len < current) ? len : current;
		bcopy(rstrm->in_finger, addr, current);
		rstrm->in_finger += current;
		addr += current;
		len -= current;
	}
	return (TRUE);
}

static bool_t  /* next two bytes of the input stream are treated as a header */
set_input_fragment(rstrm)
	register RECSTREAM *rstrm;
{
	afs_uint32 header;

	if (! get_input_bytes(rstrm, (caddr_t)&header, sizeof(header)))
		return (FALSE);
	header = ntohl(header);
	rstrm->last_frag = ((header & LAST_FRAG) == 0) ? FALSE : TRUE;
	rstrm->fbtbc = header & (~LAST_FRAG);
	return (TRUE);
}

static bool_t  /* consumes input bytes; knows nothing about records! */
skip_input_bytes(rstrm, cnt)
	register RECSTREAM *rstrm;
	int cnt;
{
	register int current;

	while (cnt > 0) {
		current = (int)rstrm->in_boundry - (int)rstrm->in_finger;
		if (current == 0) {
			if (! fill_input_buf(rstrm))
				return (FALSE);
			continue;
		}
		current = (cnt < current) ? cnt : current;
		rstrm->in_finger += current;
		cnt -= current;
	}
	return (TRUE);
}

static u_int
fix_buf_size(s)
	register u_int s;
{

	if (s < 100)
	    s = 4000;
	return ((s + BYTES_PER_XDR_UNIT - 1) / BYTES_PER_XDR_UNIT)
		* BYTES_PER_XDR_UNIT;

}
#endif /* NeXT */
