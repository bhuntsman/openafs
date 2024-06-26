/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include <afs/param.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <io.h>
#include <fcntl.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#endif
#include <sys/stat.h>
#include <lwp.h>
#include <lock.h>
#include <errno.h>

#define	UBIK_INTERNALS 1
#include "ubik.h"

/* these routines are called via the proc ptr in the ubik_dbase structure.  They provide access to
 * the physical disk, by converting the file numbers being processed (>= 0 for user data space, < 0
 * for ubik system files, such as the log) to actual pathnames to open, read, write, truncate, sync,
 * etc.
 */

#define	MAXFDCACHE  4
static struct fdcache {
    int fd;
    int fileID;
    int refCount;
} fdcache[MAXFDCACHE];

static char pbuffer[1024];

/* beware, when using this function, of the header in front of most files */
static int uphys_open(adbase, afid)
    register struct ubik_dbase *adbase;
    afs_int32 afid; {
    char temp[20];
    register int fd;
    static int initd;
    register int i;
    register struct fdcache *tfd;
    struct fdcache *bestfd;

    /* initialize package */
    if (!initd) {
	initd=1;
	tfd=fdcache;
	for(i=0;i<MAXFDCACHE;tfd++,i++) {
	    tfd->fd = -1;	    /* invalid value */
	    tfd->fileID	= -10000;   /* invalid value */
	    tfd->refCount = 0;
	}
    }

    /* scan file descr cache */
    for(tfd=fdcache,i=0; i<MAXFDCACHE; i++,tfd++) {
	if (afid == tfd->fileID	&& tfd->refCount == 0) {    /* don't use open fd */
	    lseek(tfd->fd, 0, 0);   /* reset ptr just like open would have */
	    tfd->refCount++;
	    return tfd->fd;
	}
    }

    /* not found, open it and try to enter in cache */
    strcpy(pbuffer, adbase->pathName);
    strcat(pbuffer, ".DB");
    if (afid < 0) {
	i = -afid;
	strcat(pbuffer, "SYS");
    }
    else i = afid;
    sprintf(temp, "%d", i);
    strcat(pbuffer, temp);
    fd = open(pbuffer, O_CREAT | O_RDWR, 0600);
    if (fd < 0) {
	/* try opening read-only */
	fd = open(pbuffer, O_RDONLY, 0);
	if (fd < 0) return fd;
    }
    
    /* enter it in the cache */
    tfd = fdcache;
    bestfd = (struct fdcache *) 0;
    for(i=0;i<MAXFDCACHE;i++,tfd++) {	/* look for empty slot */
	if (tfd->fd == -1) {
	    bestfd = tfd;
	    break;
	}
    }
    if (!bestfd) {			/* look for reclaimable slot */
	tfd = fdcache;
	for(i=0;i<MAXFDCACHE;i++,tfd++) {
	    if (tfd->refCount == 0) {
		bestfd = tfd;
		break;
	    }
	}
    }
    if (bestfd)	{	    /* found a usable slot */
	tfd = bestfd;
	if (tfd->fd >= 0) close(tfd->fd);
	tfd->fd = fd;
	tfd->refCount = 1;	/* us */
	tfd->fileID = afid;
    }

    /* finally, we're done */
    return fd;
}

/* close the file, maintaining ref count in cache structure */
uphys_close (afd)
register int afd; {
    register int i;
    register struct fdcache *tfd;

    if (afd < 0) return EBADF;
    tfd = fdcache;
    for(i=0;i<MAXFDCACHE;i++,tfd++) {
	if (tfd->fd == afd) {
	    tfd->refCount--;
	    return 0;
	}
    }
    return close(afd);
}

uphys_stat(adbase, afid, astat)
    struct ubik_stat *astat;
    afs_int32 afid;
    struct ubik_dbase *adbase; {
    register int fd;
    struct stat tstat;
    register afs_int32 code;
    
    fd = uphys_open(adbase, afid);
    if (fd < 0) return fd;
    code = fstat(fd, &tstat);
    uphys_close(fd);
    if (code < 0) {
	return code;
    }
    astat->mtime = tstat.st_mtime;
    code = tstat.st_size-HDRSIZE;
    if (code < 0) astat->size = 0;
    else astat->size = code;
    return 0;
}

uphys_read(adbase, afile, abuffer, apos, alength)
    register struct ubik_dbase *adbase;
    register char *abuffer;
    afs_int32 apos;
    afs_int32 afile;
    afs_int32 alength; {
    register int fd;
    register afs_int32 code;

    fd = uphys_open(adbase, afile);
    if (fd < 0) return -1;
    code = lseek(fd, apos+HDRSIZE, 0);
    if (code < 0) {
	uphys_close(fd);
	return -1;
    }
    code = read(fd, abuffer, alength);
    uphys_close(fd);
    return code;
}

uphys_write(adbase, afile, abuffer, apos, alength)
    register struct ubik_dbase *adbase;
    register char *abuffer;
    afs_int32 apos;
    afs_int32 afile;
    afs_int32 alength; {
    register int fd;
    register afs_int32 code;
    afs_int32 length;

    fd = uphys_open(adbase, afile);
    if (fd < 0) return -1;
    code = lseek(fd, apos+HDRSIZE, 0);
    if (code < 0) {
	uphys_close(fd);
	return -1;
    }
    length = write(fd, abuffer, alength);
    code = uphys_close(fd);
    if (code) return -1;
    else return length;
}

uphys_truncate(adbase, afile, asize)
    afs_int32 asize;
    register struct ubik_dbase *adbase;
    afs_int32 afile; {
    register afs_int32 code, fd;
    fd = uphys_open(adbase, afile);
    if (fd < 0) return UNOENT;
    code = ftruncate(fd, asize+HDRSIZE);
    uphys_close(fd);
    return code;
}

/* get number of dbase files */
uphys_getnfiles(adbase) {
    /* really should scan dir for data */
    return 1;
}

/* get database label, with aversion in host order */
uphys_getlabel(adbase, afile, aversion)
    register struct ubik_dbase *adbase;
    afs_int32 afile;
    struct ubik_version *aversion; {
    struct ubik_hdr thdr;
    register afs_int32 code, fd;

    fd = uphys_open(adbase, afile);
    if (fd < 0) return UNOENT;
    code = read(fd, &thdr, sizeof(thdr));
    if (code != sizeof(thdr)) {
	uphys_close(fd);
	return EIO;
    }
    aversion->epoch = ntohl(thdr.version.epoch);
    aversion->counter=ntohl(thdr.version.counter);
    uphys_close(fd);
    return 0;
}

/* label database, with aversion in host order */
uphys_setlabel(adbase, afile, aversion)
    register struct ubik_dbase *adbase;
    afs_int32 afile;
    struct ubik_version *aversion; {
    struct ubik_hdr thdr;
    register afs_int32 code, fd;

    fd = uphys_open(adbase, afile);
    if (fd < 0) return UNOENT;
    thdr.version.epoch = htonl(aversion->epoch);
    thdr.version.counter = htonl(aversion->counter);
    thdr.magic = htonl(UBIK_MAGIC);
    thdr.size = htonl(HDRSIZE);
    code = write(fd, &thdr, sizeof(thdr));
    fsync(fd);	    /* preserve over crash */
    uphys_close(fd);
    if (code != sizeof(thdr)) {
	return EIO;
    }
    return 0;
}

uphys_sync(adbase, afile)
    register struct ubik_dbase *adbase;
    afs_int32 afile; {
    register afs_int32 code, fd;
    fd = uphys_open(adbase, afile);
    code = fsync(fd);
    uphys_close(fd);
    return code;
}
