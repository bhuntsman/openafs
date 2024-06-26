/* Copyright (C) 1998 Transarc Corporation - All rights reserved */

#include <sys/types.h>
#include <afs/param.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif
#include <rx/rx.h>

/* services available on incoming message port */
BC_Print(acall, acode, aflags, amessage)
struct rx_call *acall;
afs_int32 acode, aflags;
char *amessage; {
    struct rx_connection *tconn;
    struct rx_peer *tpeer;
    
    tconn = rx_ConnectionOf(acall);
    tpeer = rx_PeerOf(tconn);
    printf("From %08x: %s <%d>\n", tpeer->host, amessage, acode);
    return 0;
}
