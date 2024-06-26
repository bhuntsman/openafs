/* Copyright 1998 Transarc Corporation - All Rights Reserved.
 *
 * rx_knet.c - RX kernel send, receive and timer routines.
 *
 * Linux implementation.
 */
#include "../afs/param.h"
#ifdef AFS_LINUX22_ENV
#include "../rx/rx_kcommon.h"
#include <asm/uaccess.h>

/* rxk_NewSocket
 * open and bind RX socket
 */
struct osi_socket *rxk_NewSocket(short aport)
{
    struct socket *sockp;
    struct sockaddr_in myaddr;
    int code;

    
    code = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &sockp);
    if (code<0)
	return NULL;

    /* Bind socket */
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = aport;
    code = sockp->ops->bind(sockp, (struct sockaddr*)&myaddr, sizeof(myaddr));

    if (code<0) {
	sock_release(sockp);
	return NULL;
    }

    return (struct osi_socket*)sockp;
}


/* free socket allocated by osi_NetSocket */
int rxk_FreeSocket(asocket)
    register struct socket *asocket;
{
    AFS_STATCNT(osi_FreeSocket);
    return 0;
}


/* osi_NetSend
 *
 * Return codes:
 * 0 = success
 * non-zero = failure
 */
int osi_NetSend(struct socket *sop, struct sockaddr_in *to,
		struct iovec *iov, int iovcnt, int size, int istack)
{
    KERNEL_SPACE_DECL;
    struct msghdr msg;
    int code;
    struct iovec tmpvec[RX_MAXWVECS+2];

    if (iovcnt > RX_MAXWVECS+2) {
	osi_Panic("Too many (%d) iovecs passed to osi_NetSend\n", iovcnt);
    }

    if (iovcnt <= 2) { /* avoid pointless uiomove */
	tmpvec[0].iov_base = iov[0].iov_base;
	tmpvec[0].iov_len = size;
	msg.msg_iovlen = 1;
    }
    else {
	memcpy(tmpvec, iov, iovcnt * sizeof(struct iovec));
	msg.msg_iovlen = iovcnt;
    }
    msg.msg_iov = tmpvec;
    msg.msg_name = to;
    msg.msg_namelen = sizeof(*to);
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;

    TO_USER_SPACE();
    code = sock_sendmsg(sop, &msg, size);
    TO_KERNEL_SPACE();
    return (code<0) ? code : 0;
}


/* osi_NetReceive
 * OS dependent part of kernel RX listener thread.
 *
 * Arguments:
 *	so      socket to receive on, typically rx_socket
 *	from    pointer to a sockaddr_in. 
 *	iov     array of iovecs to fill in.
 *	iovcnt  how many iovecs there are.
 *	lengthp IN/OUT in: total space available in iovecs. out: size of read.
 *
 * Return
 * 0 if successful
 * error code (such as EINTER) if not
 *
 * Environment
 *	Note that the maximum number of iovecs is 2 + RX_MAXWVECS. This is
 *	so we have a little space to look for packets larger than 
 *	rx_maxReceiveSize.
 */
int rxk_lastSocketError;
int rxk_nSocketErrors;
int osi_NetReceive(osi_socket so, struct sockaddr_in *from, 
		   struct iovec *iov, int iovcnt, int *lengthp)
{
    KERNEL_SPACE_DECL;
    struct msghdr msg;
    int code;
    struct iovec tmpvec[RX_MAXWVECS+2];
    struct socket *sop = (struct socket*)so;

    if (iovcnt > RX_MAXWVECS+2) {
	osi_Panic("Too many (%d) iovecs passed to osi_NetReceive\n", iovcnt);
    }
    memcpy(tmpvec, iov, iovcnt * sizeof(struct iovec));
    msg.msg_name = from;
    msg.msg_iov = tmpvec;
    msg.msg_iovlen = iovcnt;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;

    TO_USER_SPACE();
    code = sock_recvmsg(sop, &msg, *lengthp, 0);
    TO_KERNEL_SPACE();

    if (code<0) {
	/* Clear the error before using the socket again.
	 * Oh joy, Linux has hidden header files as well. It appears we can
	 * simply call again and have it clear itself via sock_error().
	 */
#ifdef AFS_LINUX22_ENV
	flush_signals(current); /* We don't want no stinkin' signals. */
#else
	current->signal = 0; /* We don't want no stinkin' signals. */
#endif
	rxk_lastSocketError = code;
	rxk_nSocketErrors ++ ;
    }
    else {
	*lengthp = code;
	code = 0;
    }

    return code;
}

void osi_StopListener(void)
{
    extern int (*sys_killp)();
    extern int rxk_ListenerPid;

    if (rxk_ListenerPid) {
	(void) (*sys_killp)(rxk_ListenerPid, 9);
	rxk_ListenerPid = 0;
    }
    sock_release(rx_socket);
    rx_socket = NULL;
}

#endif /* AFS_LINUX22_ENV */
