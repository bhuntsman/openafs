/* Copyright Transarc Corporation 1998 - All Rights Reserved */

/* I/O operations for the Unix open by name (namei) interface. */

#include <afs/param.h>
#ifdef AFS_NAMEI_ENV
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <afs/assert.h>
#include <string.h>
#include <sys/file.h>
#include <sys/param.h>
#include <lock.h>
#include <afs/afsutil.h>
#include <lwp.h>
#include "nfs.h"
#include <afs/afsint.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "viceinode.h"
#include "voldefs.h"
#include "partition.h"

extern char *volutil_PartitionName_r(int volid, char *buf, int buflen);

int namei_iread(IHandle_t *h, int offset, char *buf, int size)
{
    int nBytes;
    FdHandle_t *fdP;

    fdP = IH_OPEN(h);
    if (fdP == NULL)
	return -1;

    if (FDH_SEEK(fdP, offset, SEEK_SET)<0) {
	FDH_REALLYCLOSE(fdP);
	return -1;
    }

    nBytes = FDH_READ(fdP, buf, size);
    FDH_CLOSE(fdP);
    return nBytes;
}

int namei_iwrite(IHandle_t *h, int offset, char *buf, int size)
{
    int nBytes;
    FdHandle_t *fdP;

    fdP = IH_OPEN(h);
    if (fdP == NULL)
	return -1;

    if (FDH_SEEK(fdP, offset, SEEK_SET)<0) {
	FDH_REALLYCLOSE(fdP);
	return -1;
    }
    nBytes = FDH_WRITE(fdP, buf, size);
    FDH_CLOSE(fdP);
    return nBytes;
}



/* Inode number format:
 * low 26 bits - vnode number - all 1's if volume special file.
 * next 3 bits - tag 
 * next 3 bits spare (0's)
 * high 32 bits - uniquifier (regular) or type if spare
 */
#define NAMEI_VNODEMASK    0x003ffffff
#define NAMEI_TAGMASK      0x7
#define NAMEI_TAGSHIFT     26
#define NAMEI_UNIQMASK     0xffffffff
#define NAMEI_UNIQSHIFT    32
#define NAMEI_INODESPECIAL ((Inode)NAMEI_VNODEMASK)
#define NAMEI_VNODESPECIAL NAMEI_VNODEMASK

/* dir1 is the high 8 bits of the 26 bit vnode */
#define VNO_DIR1(vno) ((vno >> 14) & 0xff)
/* dir2 is the next 9 bits */
#define VNO_DIR2(vno) ((vno >> 9) & 0x1ff)
/* "name" is the low 9 bits of the vnode, the 3 bit tag and the uniq */

#define NAMEI_SPECDIR "special"
#define NAMEI_SPECDIRLEN (sizeof(NAMEI_SPECDIR)-1)

#define NAMEI_MAXVOLS 5 /* Maximum supported number of volumes per volume
		      * group, not counting temporary (move) volumes.
		      * This is the number of separate files, all having
		      * the same vnode number, which can occur in a volume
		      * group at once.
		      */

		       
typedef struct {
    int ogm_owner;
    int ogm_group;
    int ogm_mode;
} namei_ogm_t;

int namei_SetLinkCount(FdHandle_t *h, Inode ino, int count, int locked);
static int GetFreeTag(IHandle_t *ih, int vno);

/* namei_HandleToInodeDir
 *
 * Construct the path name of the directory holding the inode data.
 * Format: /<vicepx>/INODEDIR
 *
 */
#define PNAME_BLEN 64
static void namei_HandleToInodeDir(namei_t *name, IHandle_t *ih)
{
    char *tmp = name->n_base;

    memset(name, '\0', sizeof(*name));

    (void) volutil_PartitionName_r(ih->ih_dev, tmp, NAMEI_LCOMP_LEN);
    tmp += VICE_PREFIX_SIZE;
    tmp += ih->ih_dev > 25 ? 2 : 1;
    *tmp = '/'; tmp ++;
    (void) strcpy(tmp, INODEDIR);
    (void) strcpy(name->n_path, name->n_base);
}

#define addtoname(N, C) \
do { \
	 strcat((N)->n_path, "/"); strcat((N)->n_path, C); \
} while(0)


static void namei_HandleToVolDir(namei_t *name, IHandle_t *ih)
{
    lb64_string_t tmp;

    namei_HandleToInodeDir(name, ih);
    (void) int32_to_flipbase64(tmp, (int64_t)(ih->ih_vid & 0xff));
    (void) strcpy(name->n_voldir1, tmp);
    addtoname(name, name->n_voldir1);
    (void) int32_to_flipbase64(tmp, (int64_t)ih->ih_vid);
    (void) strcpy(name->n_voldir2, tmp);
    addtoname(name, name->n_voldir2);
}

/* namei_HandleToName
 *
 * Constructs a file name for the fully qualified handle.
 * Note that special files end up in /vicepX/InodeDir/Vxx/V*.data/special
 */
void namei_HandleToName(namei_t *name, IHandle_t *ih)
{
    lb64_string_t str;
    int vno = (int)(ih->ih_ino & NAMEI_VNODEMASK);
    int tmp;
	
    namei_HandleToVolDir(name, ih);

    if (vno == NAMEI_VNODESPECIAL) {
	(void) strcpy(name->n_dir1, NAMEI_SPECDIR);
	addtoname(name, name->n_dir1);
	name->n_dir2[0] = '\0';
    }
    else {
	(void) int32_to_flipbase64(str, VNO_DIR1(vno));
	(void) strcpy(name->n_dir1, str);
	addtoname(name, name->n_dir1);
	(void) int32_to_flipbase64(str, VNO_DIR2(vno));
	(void) strcpy(name->n_dir2, str);
	addtoname(name, name->n_dir2);
    }
    (void) int64_to_flipbase64(str, (int64_t)ih->ih_ino);
    (void) strcpy(name->n_inode, str);
    addtoname(name, name->n_inode);
}

/* The following is a warning to tell sys-admins to not muck about in this
 * name space.
 */
#define VICE_README "These files and directories are a part of the AFS \
namespace. Modifying them\nin any way will result in loss of AFS data.\n"
int namei_ViceREADME(char *partition)
{
   char filename[32];
   int fd;

   sprintf(filename, "%s/%s/README", partition, INODEDIR);
   fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0444);
   if (fd >= 0) {
      write(fd, VICE_README, strlen(VICE_README));
      close(fd);
   }
   return(errno);
}


#define create_dir() \
do { \
    if (mkdir(tmp, 0700)<0) { \
	if (errno != EEXIST) \
	    return -1; \
    } \
    else { \
	*created = 1; \
    } \
} while (0) 

#define create_nextdir(A) \
do { \
	 strcat(tmp, "/"); strcat(tmp, A); create_dir();  \
} while(0)

/* namei_CreateDataDirectories
 *
 * If creating the file failed because of ENOENT or ENOTDIR, try
 * creating all the directories first.
 */
static int namei_CreateDataDirectories(namei_t *name, int *created)
{
    char tmp[256];
    char *s;
    int i;

    *created = 0;

    (void) strcpy(tmp, name->n_base);
    create_dir();

    create_nextdir(name->n_voldir1);
    create_nextdir(name->n_voldir2);
    create_nextdir(name->n_dir1);
    if (name->n_dir2[0]) {
	create_nextdir(name->n_dir2);
    }
    return 0;
}	

/* delTree(): Deletes an entire tree of directories (no files)
 * Input:
 *   root : Full path to the subtree. Should be big enough for PATH_MAX
 *   tree : the subtree to be deleted is rooted here. Specifies only the
 *          subtree beginning at tree (not the entire path). It should be
 *          a pointer into the "root" buffer.
 * Output:
 *  errp : errno of the first error encountered during the directory cleanup.
 *         *errp should have been initialized to 0.
 * 
 * Return Values:
 *  -1  : If errors were encountered during cleanup and error is set to 
 *        the first errno.
 *   0  : Success.
 *
 * If there are errors, we try to work around them and delete as many
 * directories as possible. We don't attempt to remove directories that still
 * have non-dir entries in them.
 */
static int
delTree(char *root, char *tree, int *errp)
{
  char *cp;
  DIR *ds;
  struct dirent *dirp;
  struct stat st;
  int er;

  if (*tree) {
    /* delete the children first */
    cp = strchr(tree, '/');
    if (cp) {
      delTree(root, cp+1, errp);
      *cp = '\0';
    }
    else
      cp = tree + strlen(tree);  /* move cp to the end of string tree */
    
    /* now delete all entries in this dir */
    if ( (ds = opendir(root)) != (DIR *)NULL) {
      errno = 0;
      while (dirp = readdir(ds)) {
	/* ignore . and .. */
	if (!strcmp(dirp->d_name, ".") || !strcmp(dirp->d_name, ".."))
	  continue;
	/* since root is big enough, we reuse the space to
	 * concatenate the dirname to the current tree 
	 */
	strcat(root, "/");
	strcat(root, dirp->d_name);
	if (  stat(root, &st) == 0 && S_ISDIR(st.st_mode)) {
	  /* delete this subtree */
	  delTree(root, cp+1, errp); 
	} else
	  *errp = *errp ? *errp : errno;
	  
	/* recover path to our cur tree by truncating it to 
	 * its original len 
	 */
	*cp = 0; 
      }
      if (!errno)
	closedir(ds);
    } 
    
    /* finally axe the current dir */
    if (rmdir(root))
      *errp = *errp ? *errp : errno;

#ifndef AFS_PTHREAD_ENV   /* let rx get some work done */
    IOMGR_Poll();
#endif /* !AFS_PTHREAD_ENV */

  } /* if valid tree */
  
  /* if we encountered errors during cleanup, we return a -1 */
  if (*errp)
    return -1;

  return 0;
  
}

/* namei_RemoveDataDirectories
 * Return Values:
 * Returns 0 on success.
 * Returns -1 on error. Typically, callers ignore this error bcause we
 * can continue running if the removes fail. The salvage process will
 * finish tidying up for us. We only use the n_base and n_voldir1 entries
 * and only do rmdir's.
 */

static int namei_RemoveDataDirectories(namei_t *name)
{
      char pbuf[MAXPATHLEN], *path = pbuf;
      int prefixlen = strlen(name->n_base), err = 0;
      
      strcpy(path, name->n_path);

      /* move past the prefix */
      path = path+prefixlen+1; /* skip over the trailing / */

      /* now delete all dirs upto path */
      return delTree(pbuf, path, &err);
      
}	

/* Create the file in the name space.
 *
 * Parameters stored as follows:
 * Regular files:
 * p1 - volid - implied in containing directory.
 * p2 - vnode - name is <vno:31-23>/<vno:22-15>/<vno:15-0><uniq:31-5><tag:2-0>
 * p3 - uniq -- bits 4-0 are in mode bits 4-0
 * p4 - dv ---- dv:15-0 in uid, dv:29-16 in gid, dv:31-30 in mode:6-5
 * Special files:
 * p1 - volid - creation time - dwHighDateTime
 * p2 - vnode - -1 means special, file goes in "S" subdirectory.
 * p3 - type -- name is <type>.<tag> where tag is a file name unqiquifier.
 * p4 - parid - parent volume id - implied in containing directory.
 *
 * Return value is the inode number or (Inode)-1 if error.
 * We "know" there is only one link table, so return EEXIST if there already
 * is a link table. It's up to the calling code to test errno and increment
 * the link count.
 */

/* namei_MakeSpecIno
 *
 * This function is called by VCreateVolume to hide the implementation
 * details of the inode numbers. This only allows for 7 volume special
 * types, but if we get that far, this could should be dead by then.
 */
Inode namei_MakeSpecIno(int volid, int type)
{
    Inode ino;
    ino = NAMEI_INODESPECIAL;
    type &= NAMEI_TAGMASK;
    ino |= ((Inode)type) << NAMEI_TAGSHIFT;
    ino |= ((Inode)volid) << NAMEI_UNIQSHIFT;
    return ino;
}

/* SetOGM - set owner group and mode bits from parm and tag
 *
 * owner - low 15 bits of parm.
 * group - next 15 bits of parm.
 * mode - 2 bits of parm, then lowest = 3 bits of tag.
 */
static int SetOGM(int fd, int parm, int tag)
{
    int owner, group, mode;

    owner = parm & 0x7fff;
    group = (parm >> 15) & 0x7fff;
    if (fchown(fd, owner, group)<0)
	return -1;

    mode = (parm >> 27) & 0x18;
    mode |= tag & 0x7;
    if (fchmod(fd, mode)<0)
	return -1;

    return 0;

}

/* GetOGM - get parm and tag from owner, group and mode bits. */
static void GetOGMFromStat(struct stat *status, int *parm, int *tag)
{
    int tmp;

    *parm = status->st_uid | (status->st_gid << 15);
    *parm |= (status->st_mode & 0x18) << 27;
    *tag = status->st_mode & 0x7;
}

static int GetOGM(int fd, int *parm, int *tag)
{
    struct stat status;
    if (fstat(fd, &status)<0) 
	return -1;

    GetOGMFromStat(&status, parm, tag);
    return 0;
}

int big_vno = 0; /* Just in case we ever do 64 bit vnodes. */

/* Derive the name and create it O_EXCL. If that fails we have an error.
 * Get the tag from a free column in the link table.
 */
Inode namei_icreate(IHandle_t *lh, char *part, int p1, int p2, int p3, int p4)
{
    namei_t name;
    namei_ogm_t ogm;
    b32_string_t str1;
    char *p;
    int i;
    int fd = -1;
    int code = 0;
    int created_dir = 0;
    IHandle_t tmp;
    FdHandle_t *fdP;
    FdHandle_t tfd;
    int save_errno;
    int tag;
    int ogm_parm;
    

    memset((void*)&tmp, 0, sizeof(IHandle_t));


    tmp.ih_dev = volutil_GetPartitionID(part);
    if (tmp.ih_dev == -1) {
	errno = EINVAL;
	return -1;
    }

    if (p2 == -1 ) {
	/* Parameters for special file:
	 * p1 - volume id - goes into owner/group/mode
	 * p2 - vnode == -1
	 * p3 - type
	 * p4 - parent volume id
	 */
	ogm_parm = p1;
	tag = p3;

	tmp.ih_vid = p4; /* Use parent volume id, where this file will be.*/
	tmp.ih_ino = namei_MakeSpecIno(p1, p3);
    }
    else {
	int vno = p2 & NAMEI_VNODEMASK; 
	/* Parameters for regular file:
	 * p1 - volume id
	 * p2 - vnode
	 * p3 - uniq
	 * p4 - dv
	 */

	if (vno != p2) {
	    big_vno ++;
	    errno = EINVAL;
	    return -1;
	}
	/* If GetFreeTag succeeds, it atomically sets link count to 1. */
	tag = GetFreeTag(lh, p2);
	if (tag < 0)
	    goto bad;

	/* name is <uniq(p3)><tag><vno(p2)> */
	tmp.ih_vid = p1;
	tmp.ih_ino = (Inode)p2;
	tmp.ih_ino |= ((Inode)tag)<<NAMEI_TAGSHIFT;
	tmp.ih_ino |= ((Inode)p3)<<NAMEI_UNIQSHIFT;

	ogm_parm = p4;
    }

    namei_HandleToName(&name, &tmp);
    fd = open(name.n_path, O_CREAT|O_EXCL|O_TRUNC|O_RDWR, 0);
    if (fd < 0) {
	if (errno == ENOTDIR || errno == ENOENT) {
	    if (namei_CreateDataDirectories(&name, &created_dir)<0)
		goto bad;
	    fd = open(name.n_path, O_CREAT|O_EXCL|O_TRUNC|O_RDWR, 0);
	    if (fd < 0)
		goto bad;
	}
	else {
	    goto bad;
	}
    }
    if (SetOGM(fd, ogm_parm, tag)<0) {
	close(fd);
	fd = -1;
	goto bad;
    }

    if (p2 == -1 && p3 == VI_LINKTABLE) {
	/* hack at tmp to setup for set link count call. */
	tfd.fd_fd = fd;
	code = namei_SetLinkCount(&tfd, (Inode)0, 1, 0);
    }

bad:
    if (fd >= 0)
	close(fd);


    if (code || (fd < 0)) {
	if (p2 != -1) {
	    fdP = IH_OPEN(lh);
	    if (fdP) {
		namei_SetLinkCount(fdP, tmp.ih_ino, 0, 0);
		FDH_CLOSE(fdP);
	    }
	}
    }
    return (code || (fd<0)) ? (Inode)-1 : tmp.ih_ino;
}


/* namei_iopen */
int namei_iopen(IHandle_t *h)
{
    int fd;
    namei_t name;

    /* Convert handle to file name. */
    namei_HandleToName(&name, h);
    fd = open(name.n_path, O_RDWR, 0666);
    return fd;
}

/* Need to detect vol special file and just unlink. In those cases, the
 * handle passed in _is_ for the inode. We only check p1 for the special
 * files.
 */
int namei_dec(IHandle_t *ih, Inode ino, int p1)
{
    int count = 0;
    namei_t name;
    int code = 0;
    FdHandle_t *fdP;

    if ((ino & NAMEI_INODESPECIAL) == NAMEI_INODESPECIAL) {
	IHandle_t *tmp;
	int was_closed = 0;
	int inode_p1, tag;
	int type = (int)((ino>>NAMEI_TAGSHIFT) & NAMEI_TAGMASK);

	/* Verify this is the right file. */
	IH_INIT(tmp, ih->ih_dev, ih->ih_vid, ino);

	fdP = IH_OPEN(tmp);
	if (fdP == NULL) {
	    IH_RELEASE(tmp);
	    errno = EINVAL;
	    return -1;
	}

	if ((GetOGM(fdP->fd_fd, &inode_p1, &tag)<0) || (inode_p1 != p1)) {
	    FDH_REALLYCLOSE(fdP);
	    IH_RELEASE(tmp);
	    errno = EINVAL;
	    return -1;
	}
	
	/* If it's the link table itself, decrement the link count. */
	if (type == VI_LINKTABLE) {
	    if ((count = namei_GetLinkCount(fdP, (Inode)0, 1))<0) {
		FDH_REALLYCLOSE(fdP);
		IH_RELEASE(tmp);
		return -1;
	    }

	    count --;
	    if (namei_SetLinkCount(fdP, (Inode)0, count<0 ? 0 : count, 1)<0) {
		FDH_REALLYCLOSE(fdP);
		IH_RELEASE(tmp);
		return -1;
	    }

	    if (count>0) {
		FDH_REALLYCLOSE(fdP);
		IH_RELEASE(tmp);
		return 0;
	    }
	}
	
	namei_HandleToName(&name, tmp);
	if ((code = unlink(name.n_path)) == 0) {
	    if (type == VI_LINKTABLE) {
		/* Try to remove directory. If it fails, that's ok.
		 * Salvage will clean up.
		 */
		(void) namei_RemoveDataDirectories(&name);
	    }
	}
	FDH_REALLYCLOSE(fdP);
	IH_RELEASE(tmp);
    }
    else {
	/* Get a file descriptor handle for this Inode */
	fdP = IH_OPEN(ih);
	if (fdP == NULL) {
	    return -1;
	}

	if ((count = namei_GetLinkCount(fdP, ino,  1))<0) {
	    FDH_REALLYCLOSE(fdP);
	    return -1;
	}

	count --;
	if (count >= 0) {
	    if (namei_SetLinkCount(fdP, ino, count, 1)<0) {
		FDH_REALLYCLOSE(fdP);
		return -1;
	    }
	}
	if (count == 0 ) {
	    IHandle_t th = *ih;
	    th.ih_ino = ino;
	    namei_HandleToName(&name, &th);
	    code = unlink(name.n_path);
	}
	FDH_CLOSE(fdP);
    }

    return code;
}

int namei_inc(IHandle_t *h, Inode ino, int p1)
{
    int count;
    int code = 0;
    FdHandle_t *fdP;

    if ((ino & NAMEI_INODESPECIAL) == NAMEI_INODESPECIAL) {
	int type = (int)((ino>>NAMEI_TAGSHIFT) & NAMEI_TAGMASK);
	if (type != VI_LINKTABLE)
	    return 0;
	ino = (Inode)0;
    }

    /* Get a file descriptor handle for this Inode */
    fdP = IH_OPEN(h);
    if (fdP == NULL) {
	return -1;
    }

    if ((count = namei_GetLinkCount(fdP, ino, 1))<0)
	code = -1;
    else {
	count ++;
	if (count > 7) {
	    errno = EINVAL;
	    code = -1;
	    count = 7;
	}
	if (namei_SetLinkCount(fdP, ino, count, 1)<0)
	    code = -1;
    }
    if (code) {
	FDH_REALLYCLOSE(fdP);
    } else {
	FDH_CLOSE(fdP);
    }
    return code;
}

/************************************************************************
 * File Name Structure
 ************************************************************************
 *
 * Each AFS file needs a unique name and it needs to be findable with
 * minimal lookup time. Note that the constraint on the number of files and
 * directories in a volume is the size of the vnode index files and the
 * max file size AFS supports (for internal files) of 2^31. Since a record
 * in the small vnode index file is 64 bytes long, we can have at most
 * (2^31)/64 or 33554432 files. A record in the large index file is
 * 256 bytes long, giving a maximum of (2^31)/256 = 8388608 directories.
 * Another layout parameter is that there is roughly a 16 to 1 ratio between
 * the number of files and the number of directories.
 *
 * Using this information we can see that a layout of 256 directories, each
 * with 512 subdirectories and each of those having 512 files gives us
 * 256*512*512 = 67108864 AFS files and directories. 
 *
 * The volume, vnode, uniquifier and data version, as well as the tag
 * are required, either for finding the file or for salvaging. It's best to 
 * restrict the name to something that can be mapped into 64 bits so the
 * "Inode" is easily comparable (using "==") to other "Inodes". The tag
 * is used to distinguish between different versions of the same file
 * which are currently in the RW and clones of a volume. See "Link Table
 * Organization" below for more information on the tag. The tag is 
 * required in the name of the file to ensure a unique name. 
 *
 * We can store data in the uid, gid and mode bits of the files, provided
 * the directories have root only access. This gives us 15 bits for each
 * of uid and gid (GNU chown considers 65535 to mean "don't change").
 * There are 9 available mode bits. Adn we need to store a total of 
 * 32 (volume id) + 26 (vnode) + 32 (uniquifier) + 32 (data-version) + 3 (tag)
 * or 131 bits somewhere. 
 *
 * The format of a file name for a regular file is:
 * /vicepX/AFSIDat/V1/V2/AA/BB/<tag><uniq><vno>
 * V1 - low 8 bits of RW volume id
 * V2 - all bits of RW volume id
 * AA - high 8 bits of vnode number.
 * BB - next 9 bits of vnode number.
 * <tag><uniq><vno> - file name
 *
 * Volume special files are stored in a separate directory:
 * /vicepX/AFSIDat/V1/V2/special/<tag><uniq><vno>
 *
 *
 * The vnode is hashed into the directory using the high bits of the
 * vnode number. This is so that consecutively created vnodes are in
 * roughly the same area on the disk. This will at least be optimal if
 * the user is creating many files in the same AFS directory. The name
 * should be formed so that the leading characters are different as quickly
 * as possible, leading to faster discards of incorrect matches in the
 * lookup code.
 * 
 */


/************************************************************************
 *  Link Table Organization
 ************************************************************************
 *
 * The link table volume special file is used to hold the link counts that
 * are held in the inodes in inode based AFS vice filesystems. For user
 * space access, the link counts are being kept in a separate
 * volume special file. The file begins with the usual version stamp
 * information and is then followed by one row per vnode number. vnode 0
 * is used to hold the link count of the link table itself. That is because
 * the same link table is shared among all the volumes of the volume group
 * and is deleted only when the last volume of a volume group is deleted.
 *
 * Within each row, the columns are 3 bits wide. They can each hold a 0 based
 * link count from 0 through 7. Each colume represents a unique instance of
 * that vnode. Say we have a file shared between the RW and a RO and a
 * different version of the file (or a different uniquifer) for the BU volume.
 * Then one column would be holding the link count of 2 for the RW and RO
 * and a different column would hold the link count of 1 for the BU volume.
 * Note that we allow only 5 volumes per file, giving 15 bits used in the
 * short.
 */
#define LINKTABLE_WIDTH 2
#define LINKTABLE_SHIFT 1 /* log 2 = 1 */

static void namei_GetLCOffsetAndIndexFromIno(Inode ino, int *offset, int *index)
{
    int toff = (int) (ino & NAMEI_VNODEMASK);
    int tindex = (int)((ino>>NAMEI_TAGSHIFT) & NAMEI_TAGMASK);

    *offset = (toff << LINKTABLE_SHIFT) + 8; /* * 2 + sizeof stamp */
    *index = (tindex << 1) + tindex;
}


/* namei_GetLinkCount
 * If lockit is set, lock the file and leave it locked upon a successful
 * return.
 */
int namei_GetLinkCount(FdHandle_t *h, Inode ino, int lockit)
{
    unsigned short row = 0;
    int junk;
    int offset, index;

    namei_GetLCOffsetAndIndexFromIno(ino, &offset, &index);

    if (lockit) {
	if (flock(h->fd_fd, LOCK_EX)<0)
	    return -1;
    }

    if (lseek(h->fd_fd, offset, SEEK_SET) == -1)
	goto bad_getLinkByte;
    
    if (read(h->fd_fd, (char*)&row, sizeof(row))!=sizeof(row)) {
	goto bad_getLinkByte;
    }
	
    return (int) ((row >> index) & NAMEI_TAGMASK);

 bad_getLinkByte:
    if (lockit)
	flock(h->fd_fd, LOCK_UN);
    return -1;
}

/* Return a free column index for this vnode. */
static int GetFreeTag(IHandle_t *ih, int vno)
{
    FdHandle_t *fdP;
    int offset;
    int col;
    int coldata;
    short row;
    int code;


    fdP = IH_OPEN(ih);
    if (fdP == NULL)
	return -1;

    /* Only one manipulates at a time. */
    if (flock(fdP->fd_fd, LOCK_EX)<0) {
	FDH_REALLYCLOSE(fdP);
	return -1;
    }
    
    offset = (vno <<  LINKTABLE_SHIFT) + 8; /* * 2 + sizeof stamp */
    if (lseek(fdP->fd_fd, offset, SEEK_SET) == -1) {
	goto badGetFreeTag;
    }
	
    code = read(fdP->fd_fd, (char*)&row, sizeof(row));
    if (code != sizeof(row)) {
	if (code != 0)
	    goto badGetFreeTag;
	row = 0;
    }
	
    /* Now find a free column in this row and claim it. */
    coldata = 0x7;
    for (col = 0; col<NAMEI_MAXVOLS; col++) {
	coldata <<= col * 3;
	if ((row & coldata) == 0)
	    break;
    }
    if (col >= NAMEI_MAXVOLS)
	goto badGetFreeTag;

    coldata = 1 << (col * 3);
    row |= coldata;

    if (lseek(fdP->fd_fd, offset, SEEK_SET) == -1) {
	goto badGetFreeTag;
    }
    if (write(fdP->fd_fd, (char*)&row, sizeof(row))!=sizeof(row)) {
	goto badGetFreeTag;
    }
    FDH_SYNC(fdP);
    flock(fdP->fd_fd, LOCK_UN);
    FDH_REALLYCLOSE(fdP);
    return col;;

 badGetFreeTag:
    flock(fdP->fd_fd, LOCK_UN);
    FDH_REALLYCLOSE(fdP);
    return -1;
}



/* namei_SetLinkCount
 * If locked is set, assume file is locked. Otherwise, lock file before
 * proceeding to modify it.
 */
int namei_SetLinkCount(FdHandle_t *fdP, Inode ino, int count, int locked)
{
    int offset, index;
    unsigned short row;
    int junk;
    int code = -1;

    namei_GetLCOffsetAndIndexFromIno(ino, &offset, &index);


    if (!locked) {
	if (flock(fdP->fd_fd, LOCK_EX)<0) {
	    return -1;
	}
    }
    if (lseek(fdP->fd_fd, offset, SEEK_SET) == -1) {
	errno = EBADF;
	goto bad_SetLinkCount;
    }

    
    code = read(fdP->fd_fd, (char*)&row, sizeof(row));
    if (code != sizeof(row)) {
	if (code != 0) {
	    errno = EBADF;
	    goto bad_SetLinkCount;
	}
	row = 0;
    }
    
    junk = 7 << index;
    count <<= index;
    row &= (unsigned short)~junk;
    row |= (unsigned short)count;

    if (lseek(fdP->fd_fd, offset, SEEK_SET) == -1) {
	errno =  EBADF;
	goto bad_SetLinkCount;
    }

    if (write(fdP->fd_fd, (char*)&row, sizeof(short)) != sizeof(short)) {
	errno = EBADF;
	goto bad_SetLinkCount;
    }
    FDH_SYNC(fdP);

    code = 0;

    
bad_SetLinkCount:
    flock(fdP->fd_fd, LOCK_UN);

    return code;
}


/* ListViceInodes - write inode data to a results file. */
static int DecodeInode(char *dpath, char *name, struct ViceInodeInfo *info,
		       int volid);
static int DecodeVolumeName(char *name, int *vid);
static int namei_ListAFSSubDirs(IHandle_t *dirIH,
			     int (*write_fun)(FILE *, struct ViceInodeInfo *,
					      char *, char *),
			     FILE *fp,
			     int (*judgeFun)(struct ViceInodeInfo *, int vid),
			     int singleVolumeNumber);


/* WriteInodeInfo
 *
 * Write the inode data to the results file. 
 *
 * Returns -2 on error, 0 on success.
 *
 * This is written as a callback simply so that other listing routines
 * can use the same inode reading code.
 */
static int WriteInodeInfo(FILE *fp, struct ViceInodeInfo *info, char *dir,
			  char *name)
{
    int n;
    n = fwrite(info, sizeof(*info), 1, fp);
    return (n == 1) ? 0 : -2;
}


int mode_errors; /* Number of errors found in mode bits on directories. */
void VerifyDirPerms(char *path)
{
    struct stat status;

    if (stat(path, &status)<0) {
	Log("Unable to stat %s. Please manually verify mode bits for this"
	    " directory\n", path);
    }
    else {
	if (((status.st_mode & 0777) != 0700) || (status.st_uid != 0))
	    mode_errors ++;
    }
}

/* ListViceInodes
 * Fill the results file with the requested inode information.
 *
 * Return values:
 *  0 - success
 * -1 - complete failure, salvage should terminate.
 * -2 - not enough space on partition, salvager has error message for this.
 *
 * This code optimizes single volume salvages by just looking at that one
 * volume's directory. 
 *
 * If the resultFile is NULL, then don't call the write routine.
 */
int ListViceInodes(char *devname, char *mountedOn, char *resultFile,
		   int (*judgeInode)(struct ViceInodeInfo *info, int vid),
		   int singleVolumeNumber, int *forcep,
		   int forceR, char *wpath)
{
    FILE *fp = (FILE*)-1;
    int ninodes;
    struct stat status;

    if (resultFile) {
	fp = fopen(resultFile, "w");
	if (!fp) {
	    Log("Unable to create inode description file %s\n", resultFile);
	    return -1;
	}
    }

    /* Verify protections on directories. */
    mode_errors = 0;
    VerifyDirPerms(mountedOn);

    ninodes = namei_ListAFSFiles(mountedOn, WriteInodeInfo, fp,
			   judgeInode, singleVolumeNumber);

    if (!resultFile)
	return ninodes;

    if (ninodes < 0) {
	fclose(fp);
	return ninodes;
    }

    if (fflush(fp) == EOF) {
	Log("Unable to successfully flush inode file for %s\n", mountedOn);
	fclose(fp);
	return -2;
    }
    if (fsync(fileno(fp)) == -1) {
	Log("Unable to successfully fsync inode file for %s\n", mountedOn);
	fclose(fp);
	return -2;
    }
    if (fclose(fp) == EOF) {
	Log("Unable to successfully close inode file for %s\n", mountedOn);
	return -2;
    }

    /*
     * Paranoia:  check that the file is really the right size
     */
    if (stat(resultFile, &status) == -1) {
	Log("Unable to successfully stat inode file for %s\n", mountedOn);
	return -2;
    }
    if (status.st_size != ninodes * sizeof (struct ViceInodeInfo)) {
	Log("Wrong size (%d instead of %d) in inode file for %s\n", 
	    status.st_size, ninodes * sizeof (struct ViceInodeInfo),
	    mountedOn);
	return -2;
    }
    return 0;
}


/* namei_ListAFSFiles
 *
 * Collect all the matching AFS files on the drive.
 * If singleVolumeNumber is non-zero, just return files for that volume.
 *
 * Returns <0 on error, else number of files found to match.
 */
int namei_ListAFSFiles(char *dev,
		    int (*writeFun)(FILE *, struct ViceInodeInfo *, char *,
				     char *),
		    FILE *fp,
		    int (*judgeFun)(struct ViceInodeInfo *, int),
		    int singleVolumeNumber)
{
    IHandle_t ih;
    namei_t name;
    int ninodes = 0;
    DIR *dirp1, *dirp2;
    struct dirent *dp1, *dp2;
    char path2[512];
#ifdef DELETE_ZLC
    static void FreeZLCList(void);
#endif

    memset((void*)&ih, 0, sizeof(IHandle_t));
    ih.ih_dev = volutil_GetPartitionID(dev);

    if (singleVolumeNumber) {
	ih.ih_vid = singleVolumeNumber;
	namei_HandleToVolDir(&name, &ih);
	ninodes = namei_ListAFSSubDirs(&ih, writeFun, fp,
				 judgeFun, singleVolumeNumber);
	if (ninodes < 0)
	    return ninodes;
    }
    else {
	/* Find all volume data directories and descend through them. */
	namei_HandleToInodeDir(&name, &ih);
	ninodes = 0;
	dirp1 = opendir(name.n_path);
	if (!dirp1)
	    return 0;
	while (dp1 = readdir(dirp1)) {
	    if (*dp1->d_name == '.') continue;
	    (void) strcpy(path2, name.n_path);
	    (void) strcat(path2, "/");
	    (void) strcat(path2, dp1->d_name);
	    dirp2 = opendir(path2);
	    if (dirp2) {
		while (dp2 = readdir(dirp2)) {
		    if (*dp2->d_name == '.') continue;
		    if (!DecodeVolumeName(dp2->d_name, &ih.ih_vid)) {
			ninodes += namei_ListAFSSubDirs(&ih, writeFun, fp,
						       judgeFun, 0);
		    }
		}
		closedir(dirp2);
	    }
	}
	closedir(dirp1);
    }
#ifdef DELETE_ZLC
    FreeZLCList();
#endif
    return ninodes;
}



/* namei_ListAFSSubDirs
 *
 *
 * Return values:
 * < 0 - an error
 * > = 0 - number of AFS files found.
 */
static int namei_ListAFSSubDirs(IHandle_t *dirIH,
			     int (*writeFun)(FILE *, struct ViceInodeInfo *,
					      char *, char *),
			     FILE *fp,
			     int (*judgeFun)(struct ViceInodeInfo *, int),
			     int singleVolumeNumber)
{
    int i;
    IHandle_t myIH = *dirIH;
    namei_t name;
    char path1[512], path2[512], path3[512];
    DIR *dirp1, *dirp2, *dirp3;
    struct dirent *dp1, *dp2, *dp3;
    char *s;
    struct ViceInodeInfo info;
    int tag, vno;
    FdHandle_t linkHandle;
    int ninodes = 0;
#ifdef DELETE_ZLC
    static void AddToZLCDeleteList(char dir, char *name);
    static void DeleteZLCFiles(char *path);
#endif

    namei_HandleToVolDir(&name, &myIH);
    (void) strcpy(path1, name.n_path);

    /* Do the directory containing the special files first to pick up link
     * counts.
     */
    (void) strcat(path1, "/");
    (void) strcat(path1, NAMEI_SPECDIR);

    linkHandle.fd_fd = -1;
    dirp1 = opendir(path1);
    if (dirp1) {
	while (dp1 = readdir(dirp1)) {
	    if (*dp1->d_name == '.') continue;
	    if (DecodeInode(path1, dp1->d_name, &info, myIH.ih_vid)<0)
		continue;
	    if (info.u.param[2] != VI_LINKTABLE) {
		info.linkCount = 1;
	    }
	    else {
		/* Open this handle */
		(void) sprintf(path2, "%s/%s", path1, dp1->d_name);
		linkHandle.fd_fd = open(path2, O_RDONLY, 0666);
		info.linkCount = namei_GetLinkCount(&linkHandle, (Inode)0, 0);
	    }
	    if (judgeFun && !(*judgeFun)(&info, singleVolumeNumber))
		continue;

	    if ((*writeFun)(fp, &info, path1, dp1->d_name)<0) {
		if (linkHandle.fd_fd >= 0)
		    close(linkHandle.fd_fd);
		closedir(dirp1);
		return -1;
	    }
	    ninodes ++;
	}
	closedir(dirp1);
    }

    /* Now run through all the other subdirs */
    namei_HandleToVolDir(&name, &myIH);
    (void) strcpy(path1, name.n_path);
    
    dirp1 = opendir(path1);
    if (dirp1) {
	while (dp1 = readdir(dirp1)) {
	    if (*dp1->d_name == '.') continue;
	    if (!strcmp(dp1->d_name, NAMEI_SPECDIR))
		continue;
	    
	    /* Now we've got a next level subdir. */
	    (void) strcpy(path2, path1);
	    (void) strcat(path2, "/");
	    (void) strcat(path2, dp1->d_name);
	    dirp2 = opendir(path2);
	    if (dirp2) {
		while (dp2 = readdir(dirp2)) {
		    if (*dp2->d_name == '.') continue;
		    
		    /* Now we've got to the actual data */
		    (void) strcpy(path3, path2);
		    (void) strcat(path3, "/");
		    (void) strcat(path3, dp2->d_name);
		    dirp3 = opendir(path3);
		    if (dirp3) {
			while (dp3 = readdir(dirp3)) {
			    if (*dp3->d_name == '.') continue;
			    if (DecodeInode(path3, dp3->d_name, &info,
					    myIH.ih_vid)<0)
				continue;
			    info.linkCount = namei_GetLinkCount(&linkHandle,
							       info.inodeNumber, 0);
			    if (info.linkCount == 0) {
#ifdef DELETE_ZLC
				Log("Found 0 link count file %s/%s, deleting it.\n",
				    path3, dp3->d_name);
				AddToZLCDeleteList((char)i, dp3->d_name);
#else
				Log("Found 0 link count file %s/%s.\n",
				    path3, dp3->d_name);
#endif
				continue;
			    }
			    if (judgeFun
				&& !(*judgeFun)(&info, singleVolumeNumber))
				continue;

			    if ((*writeFun)(fp, &info, path3, dp3->d_name)<0) {
				close(linkHandle.fd_fd);
				closedir(dirp3);
				closedir(dirp2);
				closedir(dirp1);
				return -1;
			    }
			    ninodes ++;
			}
			closedir(dirp3);
		    }
		}
		closedir(dirp2);
	    }
	}
	closedir(dirp1);
    }

    if (linkHandle.fd_fd >= 0)
	close(linkHandle.fd_fd);
    if (!ninodes) {
	/* Then why does this directory exist? Blow it away. */
	namei_HandleToVolDir(&name, dirIH);
	namei_RemoveDataDirectories(&name);
    }

    return ninodes;
}

static int DecodeVolumeName(char *name, int *vid)
{
    if (strlen(name) <= 2)
	return -1;
    *vid = (int) flipbase64_to_int64(name);
    return 0;
}


/* DecodeInode
 *
 * Get the inode number from the name.
 * Get
 */
static int DecodeInode(char *dpath, char *name, struct ViceInodeInfo *info,
		       int volid)
{
    char fpath[512];
    struct stat status;
    int parm, tag;

    (void) strcpy(fpath, dpath);
    (void) strcat(fpath, "/");
    (void) strcat(fpath, name);

    if (stat(fpath, &status)<0) {
	return -1;
    }

    info->byteCount = status.st_size;
    info->inodeNumber = flipbase64_to_int64(name);
    
    GetOGMFromStat(&status, &parm, &tag);
    if ((info->inodeNumber & NAMEI_INODESPECIAL) == NAMEI_INODESPECIAL) {
	/* p1 - vid, p2 - -1, p3 - type, p4 - rwvid */
	info->u.param[0] = parm;
	info->u.param[1] = -1;
	info->u.param[2] = tag;
	info->u.param[3] = volid;
    }
    else {
	/* p1 - vid, p2 - vno, p3 - uniq, p4 - dv */
	info->u.param[0] = volid;
	info->u.param[1] = (int)(info->inodeNumber & NAMEI_VNODEMASK);
	info->u.param[2] = (int)((info->inodeNumber >> NAMEI_UNIQSHIFT)
				 & (Inode)NAMEI_UNIQMASK);
	info->u.param[3] = parm;
    }
    return 0;
}


/* PrintInode
 *
 * returns a static string used to print either 32 or 64 bit inode numbers.
 */
char * PrintInode(char *s, Inode ino)
{
    static afs_ino_str_t result; 
    if (!s)
	s = result;

    (void) sprintf((char*)s, "%Lu", ino);

    return (char*)s;
}


#ifdef DELETE_ZLC
/* Routines to facilitate removing zero link count files. */
#define MAX_ZLC_NAMES 32
#define MAX_ZLC_NAMELEN 16
typedef struct zlcList_s {
    struct zlcList_s *zlc_next;
    int zlc_n;
    char zlc_names[MAX_ZLC_NAMES][MAX_ZLC_NAMELEN];
} zlcList_t;

static zlcList_t *zlcAnchor = NULL;
static zlcList_t *zlcCur = NULL;

static void AddToZLCDeleteList(char dir, char *name)
{
    assert(strlen(name) <= MAX_ZLC_NAMELEN - 3);

    if (!zlcCur || zlcCur->zlc_n >= MAX_ZLC_NAMES) {
	if (zlcCur && zlcCur->zlc_next)
	    zlcCur = zlcCur->zlc_next;
	else {
	    zlcList_t *tmp = (zlcList_t*)malloc(sizeof(zlcList_t));
	    if (!tmp)
		return;
	    if (!zlcAnchor) {
		zlcAnchor = tmp;
	    }
	    else {
		zlcCur->zlc_next = tmp;
	    }
	    zlcCur = tmp;
	    zlcCur->zlc_n = 0;
	    zlcCur->zlc_next = NULL;
	}
    }

    (void) sprintf(zlcCur->zlc_names[zlcCur->zlc_n], "%c\\%s", dir, name);
    zlcCur->zlc_n ++;
}

static void DeleteZLCFiles(char *path)
{
    zlcList_t *z;
    int i;
    char fname[1024];

    for (z = zlcAnchor; z; z = z->zlc_next) {
	for (i=0; i < z->zlc_n; i++) {
	    (void) sprintf(fname, "%s\\%s", path, z->zlc_names[i]);
	    if (namei_unlink(fname)<0) {
		Log("ZLC: Can't unlink %s, dos error = %d\n", fname,
		       GetLastError());
	    }
	}
	z->zlc_n = 0; /* Can reuse space. */
    }
    zlcCur = zlcAnchor;
}

static void FreeZLCList(void)
{
    zlcList_t *tnext;
    zlcList_t *i;

    i = zlcAnchor;
    while (i) {
	tnext = i->zlc_next;
	free(i);
	i = tnext;
    }
    zlcCur = zlcAnchor = NULL;
}
#endif

#endif /* AFS_NAMEI_ENV */

