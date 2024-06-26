/* Copyright Transarc Corporation 1998 - All Rights Reserved */

#include <afs/param.h>
#ifdef AFS_NAMEI_ENV
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <lock.h>
#include <afs/afsutil.h>
#include "nfs.h"
#include <afs/afsint.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "viceinode.h"
#include "voldefs.h"
#include "partition.h"
#include <dirent.h>
#include <afs/assert.h>



IHandle_t * GetLinkHandle(char *part, int volid)
{
    int dev;
    Inode ino;
    IHandle_t *lh;

    dev = volutil_GetPartitionID(part);
    ino = namei_MakeSpecIno(volid, VI_LINKTABLE);

    IH_INIT(lh, dev, volid, ino);
    return lh;
}

void DFlushVolume(void) {} ;

#endif /* AFS_NAMEI_ENV */
