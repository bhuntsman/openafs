/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1998
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include <afs/param.h>
#include <sys/types.h>
#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <rx/xdr.h>
#include <afs/vlserver.h>   /*Misc server-side Volume Location stuff*/
#include <ubik.h>
#include "volser.h"
#include "bc.h"
#include <afs/volint.h>

extern int VL_GetEntryByID();
extern char *whoami;

/* ********************************************************************* */
/* Volserver routines */
/* ********************************************************************* */

afs_int32 bc_GetEntryByID(uclient, volID, volType, vldbEntryPtr)
    struct ubik_client *uclient;
    afs_int32              volID;
    afs_int32              volType;
    struct vldbentry   *vldbEntryPtr;
{
    afs_int32 code = 0;

    code = ubik_Call(VL_GetEntryByID, uclient, 0, volID, volType, vldbEntryPtr);
    return(code);
}

/* volImageTime
 *	Determine the time stamp to be recorded with the backup of this
 *	volume. For backup and r/o volumes this is the clone time, for
 *	r/w volumes, this is the current time. This timestamp is stored
 *	directly into the cloneDate field of the bc_volumeDump structure
 * exit:
 *	0 - success
 *	-1 - failed to get information. Sets cloneDate to 0.
 */

afs_int32
volImageTime (serv, part, volid, voltype, clDatePtr)
    afs_int32 serv;
    afs_int32 part;
    afs_int32 volid;
    afs_int32 voltype;
    afs_int32 *clDatePtr;
{
    afs_int32 code = 0;
    struct volintInfo *viptr;

    if (voltype == RWVOL)
    {
        *clDatePtr = time(0);
	return(0);
    }

    code = UV_ListOneVolume(htonl(serv), part, volid, &viptr);
    if (code)
    {
        com_err(whoami, code,
		"Warning: Can't get clone time of volume %u - using 0", volid);
	*clDatePtr = 0;
	return(0);
    }

    /* volume types from vol/voldefs.h */
    switch ( viptr->type )
    {
      case RWVOL:
	/* For a r/w volume there may not be any foolproof way of
	 * preventing anomalies in the backups. Use the current time;
	 */
	*clDatePtr = time(0);
	break;

      case ROVOL:
      case BACKVOL:
	*clDatePtr = viptr->creationDate;     /* use the creation time */
	break;
	
      default:
        com_err(whoami, 0,
		"Can't get clone time of volume %u - unknown volume type", volid);
	return(-1);
    }
    return(0);
}

