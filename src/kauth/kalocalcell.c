
/*
 * (C) COPYRIGHT IBM CORPORATION 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

/*
 * Revision 1.5  89/02/14  16:32:55
 * Move ka_Init yet again!
 * 
 * Revision 1.4  89/02/14  16:10:58
 * Moved ka_Init here from authclient.c.
 * 
 * Revision 1.3  89/01/11  14:46:35
 * Collection of minor fixes so that afsconf_Open is called with the correct
 *   CLIENT/SERVER argument.
 * 
 * Revision 1.2  89/01/11  14:16:20
 * Added ka_CellConfig call to specify client or server operation.
 * 
 * Revision 1.1  89/01/11  11:05:08
 * Initial revision
 *  */

#if defined(UKERNEL)
#include "../afs/param.h"
#include "../afs/pthread_glock.h"
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#include "../afs/afs_usrops.h"
#include "../afs/cellconfig.h"
#include "../afs/pthread_glock.h"
#include "../rx/xdr.h"
#include "../rx/rx.h"
#include "../afsint/kauth.h"
#include "../afs/kautils.h"
#include "../afs/afsutil.h"
#else /* defined(UKERNEL) */
#include <afs/param.h>
#include <afs/pthread_glock.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <afs/cellconfig.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include "kauth.h"
#include "kautils.h"
#include <afs/afsutil.h>
#endif /* defined(UKERNEL) */

/* This is a utility routine that many parts of kauth use but it invokes the
   afsconf package so its best to have it in a separate .o file to make the
   linker happy. */

static struct afsconf_dir *conf = 0;
static char cell_name[MAXCELLCHARS];

int ka_CellConfig (char *dir)
{
    int code;
#ifdef UKERNEL
    conf = afs_cdir;
    strcpy(cell_name, afs_LclCellName);
    return 0;
#else /* UKERNEL */
    LOCK_GLOBAL_MUTEX
    if (conf) afsconf_Close(conf);
    conf = afsconf_Open (dir);
    if (!conf) {
	UNLOCK_GLOBAL_MUTEX
	return KANOCELLS;
    }
    code = afsconf_GetLocalCell (conf, cell_name, sizeof(cell_name));
    UNLOCK_GLOBAL_MUTEX
    return code;
#endif /* UKERNEL */
}

char *ka_LocalCell (void)
{   
  int code;

    LOCK_GLOBAL_MUTEX
    if (conf) {
	UNLOCK_GLOBAL_MUTEX
	return cell_name;
    }

#ifdef UKERNEL
    conf = afs_cdir;
    strcpy(cell_name, afs_LclCellName);
#else /* UKERNEL */
    if (conf = afsconf_Open (AFSDIR_CLIENT_ETC_DIRPATH)) {
	code = afsconf_GetLocalCell (conf, cell_name, sizeof(cell_name));
/* leave conf open so we can lookup other cells */
/* afsconf_Close (conf); */
    }
    if (!conf || code) {
	printf("** Can't determine local cell name!\n");
	conf = 0;
	UNLOCK_GLOBAL_MUTEX
	return 0;
    }
#endif /* UKERNEL */
    UNLOCK_GLOBAL_MUTEX
    return cell_name;
}

int ka_ExpandCell (
  char *cell,
  char *fullCell,
  int  *alocal)
{
    int  local = 0;
    int  code;
    char cellname[MAXKTCREALMLEN];
    struct afsconf_cell cellinfo;	/* storage for cell info */

    LOCK_GLOBAL_MUTEX
    ka_LocalCell();			/* initialize things */
    if (!conf) {
	UNLOCK_GLOBAL_MUTEX
	return KANOCELLS;
    }

    if ((cell == 0) || (strlen(cell) == 0)) {
	local = 1;
	cell = cell_name;
    } else {
	cell = lcstring (cellname, cell, sizeof(cellname));
	code = afsconf_GetCellInfo (conf, cell, 0, &cellinfo);
	if (code) {
	    UNLOCK_GLOBAL_MUTEX
	    return KANOCELL;
	}
	cell = cellinfo.name;
    }
    if (strcmp (cell, cell_name) == 0) local = 1;

    if (fullCell) strcpy (fullCell, cell);
    if (alocal) *alocal = local;
    UNLOCK_GLOBAL_MUTEX
    return 0;
}    

int ka_CellToRealm (
  char *cell,
  char *realm,
  int  *local)
{
    int code;

    LOCK_GLOBAL_MUTEX
    code = ka_ExpandCell (cell, realm, local);
    ucstring (realm, realm, MAXKTCREALMLEN);
    UNLOCK_GLOBAL_MUTEX
    return code;
}
