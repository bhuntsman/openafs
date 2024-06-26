/* Copyright (C) 1990 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */


#ifndef __AUTH_AFS_INCL_
#define	__AUTH_AFS_INCL_    1

#include <rx/rxkad.h>			/* to get ticket parameters/contents */

/* super-user pincipal used by servers when talking to other servers */
#define AUTH_SUPERUSER        "afs"

struct ktc_token {
    afs_int32 startTime;
    afs_int32 endTime;
    struct ktc_encryptionKey sessionKey;
    short kvno;  /* XXX UNALIGNED */
    int ticketLen;
    char ticket[MAXKTCTICKETLEN];
};

#ifdef AFS_NT40_ENV
extern int ktc_SetToken(
        struct ktc_principal *server,
        struct ktc_token *token,
        struct ktc_principal *client,
        int flags
);

extern int ktc_GetToken(
        struct ktc_principal *server,
        struct ktc_token *token,
        int tokenLen,
        struct ktc_principal *client
);

extern int ktc_ListTokens(
        int cellNum,
        int *cellNumP,
        struct ktc_principal *serverName
);

extern int ktc_ForgetToken(
        struct ktc_principal *server
);

extern int ktc_ForgetAllTokens(void);

/* Flags for the flag word sent along with a token */
#define PIOCTL_LOGON		0x1	/* invoked from integrated logon */

#endif /* AFS_NT40_ENV */

/* Flags for ktc_SetToken() */
#define AFS_SETTOK_SETPAG	0x1
#define AFS_SETTOK_LOGON	0x2	/* invoked from integrated logon */

#endif /* __AUTH_AFS_INCL_ */
