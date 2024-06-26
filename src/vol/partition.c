
#ifndef lint
#endif

/*
 * (C) COPYRIGHT IBM CORPORATION 1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/*

	System:		VICE-TWO
	Module:		partition.c
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#include <afs/param.h>
#include <ctype.h>
#ifdef AFS_NT40_ENV
#include <windows.h>
#include <winbase.h>
#include <winioctl.h>
#else
#include <sys/param.h>

#if AFS_HAVE_STATVFS
#include <sys/statvfs.h>
#endif /* AFS_HAVE_STATVFS */

#if !defined(AFS_SGI_ENV)
#ifdef	AFS_OSF_ENV
#include <sys/mount.h>
#include <ufs/fs.h>
#else	/* AFS_OSF_ENV */
#ifdef AFS_VFSINCL_ENV
#define VFS
#ifdef	AFS_SUN5_ENV
#include <sys/fs/ufs_fs.h>
#else
#include <ufs/fs.h>
#endif
#else /* AFS_VFSINCL_ENV */
#if !defined(AFS_AIX_ENV) && !defined(AFS_LINUX22_ENV)
#include <sys/fs.h>
#endif
#endif /* AFS_VFSINCL_ENV */
#endif	/* AFS_OSF_ENV */
#include <sys/errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>
#ifdef	AFS_AIX_ENV
#include <sys/vfs.h>
#include <sys/lockf.h>
#else
#ifdef	AFS_HPUX_ENV
#include <sys/vfs.h>
#include <unistd.h>
#include <fcntl.h>
#include <checklist.h>
#else
#if	defined(AFS_SUN_ENV)
#include <sys/vfs.h>
#endif
#ifdef AFS_SUN5_ENV
#include <unistd.h>
#include <sys/mnttab.h>
#include <sys/mntent.h>
#else
#ifdef AFS_LINUX22_ENV
#include <mntent.h>
#include <linux/fs.h>
#include <linux/ext2_fs.h>
#else
#include <fstab.h>
#endif
#endif
#endif
#endif
#endif	/* AFS_SGI_ENV */
#endif /* AFS_NT40_ENV */
#if defined(AFS_SGI_ENV)
#include <sys/errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/file.h>
#include <mntent.h>
#endif

#include <rx/xdr.h>
#include <afs/afsint.h>
#include "nfs.h"
#include <afs/errors.h>
#include "lock.h"
#include "lwp.h"
#include <afs/afssyscalls.h>
#include "ihandle.h"
#ifdef AFS_NAMEI_ENV
#ifdef AFS_NT40_ENV
#include "ntops.h"
#else
#include "namei_ops.h"
#endif
#endif /* AFS_NAMEI_ENV */
#include "vnode.h"
#include "volume.h"
#include "partition.h"
#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#else /* AFS_PTHREAD_ENV */
#include <afs/assert.h>
#endif /* AFS_PTHREAD_ENV */

#if defined(AFS_HPUX_ENV)
#include <sys/types.h>
#include <sys/privgrp.h>
#endif /* defined(AFS_HPUX_ENV) */

#ifdef AFS_AIX42_ENV
#include <jfs/filsys.h>
#endif

int aixlow_water = 8;	/* default 8% */
struct DiskPartition *DiskPartitionList;

#ifdef AFS_SGI_XFS_IOPS_ENV
/* Verify that the on disk XFS inodes on the partition are large enough to
 * hold the AFS attribute. Returns -1 if the attribute can't be set or is
 * too small to fit in the inode. Returns 0 if the attribute does fit in
 * the XFS inode.
 */
#include <afs/xfsattrs.h>
static int VerifyXFSInodeSize(char *part, char *fstype)
{
    afs_xfs_attr_t junk;
    int length = SIZEOF_XFS_ATTR_T;
    int fd = 0;
    int code = -1;
    struct fsxattr fsx;

    if (strcmp("xfs", fstype))
	return 0;

    if (attr_set(part, AFS_XFS_ATTR, &junk, length, ATTR_ROOT) == 0) {
	if (((fd=open(part, O_RDONLY, 0)) != -1)
	    && (fcntl(fd, F_FSGETXATTRA, &fsx) == 0)) {
	
	    if (fsx.fsx_nextents) {
		Log("Partition %s: XFS inodes too small, exiting.\n", part);
		Log("Run xfs_size_check utility and remake partitions.\n");
	    }
	    else
		code = 0;
	}

	if (fd > 0)
	    close(fd);
	(void) attr_remove(part, AFS_XFS_ATTR, ATTR_ROOT);
    }
    return code;
}
#endif


static void VInitPartition_r(char *path, char *devname, Device dev)
{
    struct DiskPartition *dp, *op;
    dp = (struct DiskPartition *) malloc(sizeof (struct DiskPartition));
    /* Add it to the end, to preserve order when we print statistics */
    for (op = DiskPartitionList; op; op = op->next) {
	if (!op->next)
	    break;
    }
    if (op)
	op->next = dp;
    else
	DiskPartitionList = dp;
    dp->next = 0;
    strcpy(dp->name, path);
#if defined(AFS_NAMEI_ENV) && !defined(AFS_NT40_ENV)
    strcpy(dp->devName, path);
    dp->device = volutil_GetPartitionID(path);
#else
    strcpy(dp->devName, devname);
    dp->device = dev;
#endif
    dp->lock_fd = -1;
    dp->flags = 0;
    dp->f_files = 1;	/* just a default value */
#if defined(AFS_NAMEI_ENV) && !defined(AFS_NT40_ENV)
    if (programType == fileServer)
	(void) namei_ViceREADME(VPartitionPath(dp));
#endif
    VSetPartitionDiskUsage_r(dp);
}

static void VInitPartition(char *path, char *devname, Device dev)
{
    VOL_LOCK
    VInitPartition_r(path, devname, dev);
    VOL_UNLOCK
}

#ifndef AFS_NT40_ENV
/* VAttachPartitions() finds the vice partitions on this server. Calls
 * VCheckPartition() to do some basic checks on the partition. If the partition
 * is a valid vice partition, VCheckPartition will add it to the DiskPartition
 * list.
 * Returns the number of errors returned by VCheckPartition. An error in
 * VCheckPartition means that partition is a valid vice partition but the
 * fileserver should not start because of the error found on that partition.
 *
 * AFS_NAMEI_ENV
 * No specific user space file system checks, since we don't know what
 * is being used for vice partitions.
 *
 * Use partition name as devname.
 */
int VCheckPartition(part, devname)
     char *part;
     char *devname;
{
    struct stat status;

    /* Only keep track of "/vicepx" partitions since it can get hairy
     * when NFS mounts are involved.. */
    if (strncmp(part, VICE_PARTITION_PREFIX, VICE_PREFIX_SIZE)) {
	return 0;
    }
    if (stat(part, &status) < 0) {
	Log("VInitVnodes: Couldn't find file system %s; ignored\n", part);
	return 0;
    }
    
#ifndef AFS_AIX32_ENV
    if (programType == fileServer) {
	char salvpath[MAXPATHLEN];
	strcpy(salvpath, part);
	strcat(salvpath, "/FORCESALVAGE");
	if (stat(salvpath, &status) == 0) {
	    Log("VInitVnodes: Found %s; aborting\n", salvpath);
	    return -1;
	}
    }
#endif

#ifdef AFS_SGI_XFS_IOPS_ENV
    if (VerifyXFSInodeSize(part, status.st_fstype) < 0)
	return -1;
#endif

#ifdef AFS_DUX40_ENV
    if (status.st_ino != ROOTINO) {
	Log("%s is not a mounted file system; ignored.\n", part);
	return 0;
    }
#endif

    VInitPartition(part, devname, status.st_dev);

    return 0;
}
#endif /* AFS_NT40_ENV */

#ifdef AFS_SUN5_ENV
int VAttachPartitions(void)
{
    int errors = 0;
    struct mnttab mnt;
    FILE *mntfile;

    if (!(mntfile = fopen(MNTTAB, "r"))) {
	Log("Can't open %s\n", MNTTAB);
	perror(MNTTAB);
	exit(-1);
    }
    while (!getmntent(mntfile, &mnt)) {
	/* Ignore non ufs or non read/write partitions */
	if ((strcmp(mnt.mnt_fstype, "ufs") !=0) ||
	    (strncmp(mnt.mnt_mntopts, "ro,ignore",9) ==0)) 
	    continue; 

	if (VCheckPartition(mnt.mnt_mountp, mnt.mnt_special) < 0 )
	    errors ++;
    }

   (void) fclose(mntfile);

    return errors ;
}

#endif /* AFS_SUN5_ENV */
#if defined(AFS_SGI_ENV) || (defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV)) || defined(AFS_HPUX_ENV)
int VAttachPartitions(void)
{
    int errors = 0;
    FILE *mfd;
    struct mntent *mntent;
    
    if ((mfd = setmntent(MOUNTED, "r")) == NULL) {
	Log("Problems in getting mount entries(setmntent)\n");
	exit(-1);
    }
    while (mntent = getmntent(mfd)) {
	if (!hasmntopt(mntent, MNTOPT_RW)) continue;
	
	if (VCheckPartition(mntent->mnt_dir, mntent->mnt_fsname) < 0 )
	    errors ++;
    }

    endmntent(mfd);

    return errors ;
}
#endif
#ifdef AFS_AIX_ENV
/*
 * (This function was grabbed from df.c)
 */
int
getmount(vmountpp)
register struct vmount	**vmountpp;	/* place to tell where buffer is */
{
	int			size;
	register struct vmount	*vm;
	int			nmounts;

	/* set initial size of mntctl buffer to a MAGIC NUMBER */
	size = BUFSIZ;

	/* try the operation until ok or a fatal error */
	while (1) {
		if ((vm = (struct vmount *)malloc(size)) == NULL) {
			/* failed getting memory for mount status buf */
			perror("FATAL ERROR: get_stat malloc failed\n");
			exit(-1);
		}

		/*
		 * perform the QUERY mntctl - if it returns > 0, that is the
		 * number of vmount structures in the buffer.  If it returns
		 * -1, an error occured.  If it returned 0, then look in
		 * first word of buffer for needed size.
		 */
		if ((nmounts = mntctl(MCTL_QUERY, size, (caddr_t)vm)) > 0) {
			/* OK, got it, now return */
			*vmountpp = vm;
			return(nmounts);

		} else if (nmounts == 0) {
			/* the buffer wasn't big enough .... */
			/* .... get required buffer size */
			size = *(int *)vm;
			free(vm);

		} else {
			/* some other kind of error occurred */
			free(vm);
			return(-1);
		}
	}
}

int VAttachPartitions(void)
{
    int errors = 0;
    int nmounts;
    struct vmount *vmountp;

    if ((nmounts = getmount(&vmountp)) <= 0)	{   
	Log("Problems in getting # of mount entries(getmount)\n");
	exit(-1);
    }
    for (; nmounts; nmounts--,
	 vmountp = (struct vmount *)((int)vmountp + vmountp->vmt_length)) {
	char *part = vmt2dataptr(vmountp, VMT_STUB);

	if (vmountp->vmt_flags & (MNT_READONLY|MNT_REMOVABLE|MNT_REMOTE))
	    continue; /* Ignore any "special" partitions */

#ifdef AFS_AIX42_ENV
	{
	    struct superblock fs;
	    /* The Log statements are non-sequiters in the SalvageLog and don't
	     * even appear in the VolserLog, so restrict them to the FileLog.
	     */
	    if (ReadSuper(&fs, vmt2dataptr(vmountp, VMT_OBJECT))<0) {
		if (programType == fileServer)
		    Log("Can't read superblock for %s, ignoring it.\n", part);
		continue;
	    }
	    if (IsBigFilesFileSystem(&fs)) {
		if (programType == fileServer)
		    Log("%s is a big files filesystem, ignoring it.\n", part);
		continue;
	    }
	}
#endif

	if (VCheckPartition(part, vmt2dataptr(vmountp, VMT_OBJECT)) < 0 )
	    errors ++;
    }
    return errors ;

}
#endif
#ifdef AFS_DUX40_ENV
int VAttachPartitions(void)
{
    int errors = 0;
    struct fstab *fsent;

    if (setfsent() < 0) {
	Log("Error listing filesystems.\n");
	exit(-1);
    }

    while (fsent = getfsent()) {
	if (strcmp(fsent->fs_type, "rw") != 0) continue;

	if (VCheckPartition(fsent->fs_file, fsent->fs_spec) < 0 )
	    errors ++;
    }
    endfsent();
    
    return errors ;
}
#endif

#ifdef AFS_NT40_ENV
#include <string.h>
#include <sys/stat.h>
/* VValidVPTEntry
 *
 * validate names in vptab.
 *
 * Return value:
 * 1 valid entry
 * 0 invalid entry
 */

int VValidVPTEntry(struct vptab *vpe)
{
    int len = strlen(vpe->vp_name);
    int i;

    if (len < VICE_PREFIX_SIZE+1 || len > VICE_PREFIX_SIZE + 2)
	return 0;
    if (strncmp(vpe->vp_name, VICE_PARTITION_PREFIX, VICE_PREFIX_SIZE))
	return 0;
    
    for (i=VICE_PREFIX_SIZE; i<len; i++) {
	if (vpe->vp_name[i] < 'a' || vpe->vp_name[i] > 'z') {
	    Log("Invalid partition name %s in registry, ignoring it.\n",
		vpe->vp_name);
	    return 0;
	}
    }
    if (len == VICE_PREFIX_SIZE + 2) {
	i = (int)(vpe->vp_name[VICE_PREFIX_SIZE]-'a') * 26 +
	    (int)(vpe->vp_name[VICE_PREFIX_SIZE+1]-'a') ;
	if (i>255) {
	    Log("Invalid partition name %s in registry, ignoring it.\n",
		vpe->vp_name);
	    return 0;
	}
    }

    len = strlen(vpe->vp_dev);
    if (len != 2 || vpe->vp_dev[1] != ':'  || vpe->vp_dev[0] < 'A' ||
	vpe->vp_dev[0] > 'Z') {
	Log("Invalid device name %s in registry, ignoring it.\n",
	    vpe->vp_dev);
	return 0;
    }

    return 1;
}

int VCheckPartition(char *partName)
{
    char volRoot[4];
    char volFsType[64];
    DWORD dwDummy;
    int err;

    /* partName is presumed to be of the form "X:" */
    (void) sprintf(volRoot, "%c:\\", *partName);

    if (!GetVolumeInformation(volRoot,    /* volume root directory */
			      NULL,       /* volume name buffer */
			      0,          /* volume name size */
			      NULL,       /* volume serial number */
			      &dwDummy,   /* max component length */
			      &dwDummy,   /* file system flags */
			      volFsType,  /* file system name */
			      sizeof(volFsType))) {
	err = GetLastError();
	Log("VCheckPartition: Failed to get partition information for %s, ignoring it.\n",
	    partName);
	return -1;
    }

    if (strcmp(volFsType, "NTFS")) {
	Log("VCheckPartition: Partition %s is not an NTFS partition, ignoring it.\n", partName);
	return -1;
    }

    return 0;
}


int VAttachPartitions(void)
{
    struct DiskPartition *partP, *prevP, *nextP;
    struct vpt_iter iter;
    struct vptab entry;

    if (vpt_Start(&iter)<0) {
	Log("No partitions to attach.\n");
	return 0;
    }

    while (0==vpt_NextEntry(&iter, &entry)) {
	if (!VValidVPTEntry(&entry)) {
	    continue;
	}

	/* This test for duplicates relies on the fact that the method
	 * of storing the partition names in the NT registry means the same
	 * partition name will never appear twice in the list.
	 */
	for (partP = DiskPartitionList; partP; partP = partP->next) {
	    if (*partP->devName == *entry.vp_dev) {
		Log("Same drive (%s) used for both partition %s and partition %s, ignoring both.\n", entry.vp_dev, partP->name, entry.vp_name);
		partP->flags = PART_DUPLICATE;
		break; /* Only one entry will ever be in this list. */
	    }
	}
	if (partP) continue; /* found a duplicate */

	if (VCheckPartition(entry.vp_dev)<0)
	    continue;
	/* This test allows for manually inserting the FORCESALVAGE flag
	 * and thereby invoking the salvager. scandisk obviously won't be
	 * doing this for us.
	 */
	if (programType == fileServer) {
	    struct stat status;
	    char salvpath[MAXPATHLEN];
	    strcpy(salvpath, entry.vp_dev);
	    strcat(salvpath, "\\FORCESALVAGE");
	    if (stat(salvpath, &status) == 0) {
		Log("VAttachPartitions: Found %s; aborting\n", salvpath);
		exit(1);
	    }
	}
	VInitPartition(entry.vp_name, entry.vp_dev, *entry.vp_dev - 'A');
    }
    vpt_Finish(&iter);

    /* Run through partition list and clear out the dupes. */
    prevP = nextP = NULL;
    for (partP = DiskPartitionList; partP; partP = nextP) {
	nextP = partP->next;
	if (partP->flags == PART_DUPLICATE) {
	    if (prevP)
		prevP->next = partP->next;
	    else
		DiskPartitionList = partP->next;
	    free(partP);
	}
	else
	    prevP = partP;
    }

    return 0;
}
#endif

#ifdef AFS_LINUX22_ENV
int VAttachPartitions(void)
{
    int errors = 0;
    FILE *mfd;
    struct mntent *mntent;
    
    if ((mfd = setmntent("/proc/mounts", "r")) == NULL) {
	if ((mfd = setmntent("/etc/mtab", "r")) == NULL) {
	    Log("Problems in getting mount entries(setmntent)\n");
	    exit(-1);
	}
    }
    while (mntent = getmntent(mfd)) {
	if (VCheckPartition(mntent->mnt_dir, mntent->mnt_fsname) < 0 )
	    errors ++;
    }
    endmntent(mfd);

    return errors ;
}
#endif /* AFS_LINUX22_ENV */

/* This routine is to be called whenever the actual name of the partition
 * is required. The canonical name is still in part->name.
 */
char * VPartitionPath(struct DiskPartition *part)
{
#ifdef AFS_NT40_ENV
    return part->devName;
#else
    return part->name;
#endif    
}

/* get partition structure, abortp tells us if we should abort on failure */
struct DiskPartition *VGetPartition_r(char *name, int abortp)
{
    register struct DiskPartition *dp;
    for (dp = DiskPartitionList; dp; dp = dp->next) {
	if (strcmp(dp->name, name) == 0)
	    break;
    }
    if (abortp)
	assert(dp != NULL);
    return dp;
}

struct DiskPartition *VGetPartition(char *name, int abortp)
{
    struct DiskPartition *retVal;
    VOL_LOCK
    retVal = VGetPartition_r(name, abortp);
    VOL_UNLOCK
    return retVal;
}

#ifdef AFS_NT40_ENV
void VSetPartitionDiskUsage_r(register struct DiskPartition *dp)
{
    ULARGE_INTEGER free_user, total, free_total;
    int ufree, tot, tfree;
    
    if (!GetDiskFreeSpaceEx(VPartitionPath(dp), &free_user, &total,
			    &free_total)) {
	printf("Failed to get disk space info for %s, error = %d\n",
	       dp->name, GetLastError());
	return;
    }

    /* Convert to 1K units. */
    ufree = (int) Int64ShraMod32(free_user.QuadPart, 10);
    tot = (int) Int64ShraMod32(total.QuadPart, 10);
    tfree = (int) Int64ShraMod32(free_total.QuadPart, 10);

    dp->minFree = tfree - ufree; /* only used in VPrintDiskStats_r */
    dp->totalUsable = tot;
    dp->free = tfree;
}

#else
void VSetPartitionDiskUsage_r(register struct DiskPartition *dp)
{
    extern int errno;
    int fd, totalblks, free, used, availblks, bsize, code;
    int reserved;
#if AFS_HAVE_STATVFS
    struct statvfs statbuf;
#else
    struct statfs statbuf;
#endif

    if (dp->flags & PART_DONTUPDATE)
	return;
    /* Note:  we don't bother syncing because it's only an estimate, update
       is syncing every 30 seconds anyway, we only have to keep the disk
       approximately 10% from full--you just can't get the stuff in from
       the net fast enough to worry */
#if AFS_HAVE_STATVFS
    code = statvfs(dp->name, &statbuf);
#else
    code = statfs(dp->name, &statbuf);
#endif
    if (code < 0) {
	Log("statfs of %s failed in VSetPartitionDiskUsage (errno = %d)\n", dp->name, errno);
	return;
    }
    if (statbuf.f_blocks == -1)	{   /* Undefined; skip stats.. */   
	Log("statfs of %s failed in VSetPartitionDiskUsage\n", dp->name);
	return;
    }
    totalblks = statbuf.f_blocks;
    free = statbuf.f_bfree;
    reserved = free - statbuf.f_bavail;
#if AFS_HAVE_STATVFS
    bsize = statbuf.f_frsize;
#else
    bsize = statbuf.f_bsize;
#endif
    availblks = totalblks - reserved;
    dp->f_files = statbuf.f_files;      /* max # of files in partition */

    /* Now free and totalblks are in fragment units, but we want them in
     * 1K units.
     */
    if (bsize >= 1024) {
	free *= (bsize/1024);
	totalblks *= (bsize / 1024);
	availblks *= (bsize / 1024 );
	reserved *= (bsize / 1024 );
    }
    else {
	free /= (1024/bsize);
	totalblks /= (1024/bsize);
	availblks /= (1024/bsize);
	reserved /= (1024/bsize);
    }
    /* now compute remaining figures */
    used = totalblks - free;

    dp->minFree = reserved; /* only used in VPrintDiskStats_r */
    dp->totalUsable = availblks;
    dp->free = availblks - used; /* this is exactly f_bavail */
}
#endif /* AFS_NT40_ENV */

void VSetPartitionDiskUsage(register struct DiskPartition *dp)
{
    VOL_LOCK
    VSetPartitionDiskUsage_r(dp);
    VOL_UNLOCK
}

void VResetDiskUsage_r(void)
{
    struct DiskPartition *dp;
    for (dp = DiskPartitionList; dp; dp = dp->next) {
	VSetPartitionDiskUsage_r(dp);
#ifndef AFS_PTHREAD_ENV
	IOMGR_Poll();
#endif /* !AFS_PTHREAD_ENV */
    }
}

void VResetDiskUsage(void)
{
    VOL_LOCK
    VResetDiskUsage_r();
    VOL_UNLOCK
}

void VAdjustDiskUsage_r(Error *ec, Volume *vp, afs_int32 blocks, afs_int32 checkBlocks)
{
    afs_int32 rem, minavail;
    *ec = 0;
    /* why blocks instead of checkBlocks in the check below?  Otherwise, any check
       for less than BlocksSpare would skip the error-checking path, and we
       could grow existing files forever, not just for another BlocksSpare
       blocks. */
    if (blocks > 0) {
#ifdef	AFS_AIX32_ENV
	if ((rem = vp->partition->free - checkBlocks) < 
	    (minavail = (vp->partition->totalUsable * aixlow_water) / 100))
#else
	if (vp->partition->free - checkBlocks < 0)
#endif
	    *ec = VDISKFULL;
	else if (V_maxquota(vp) && V_diskused(vp) + checkBlocks > V_maxquota(vp))
	    *ec = VOVERQUOTA;
    }    
    vp->partition->free -= blocks;
    V_diskused(vp) += blocks;
}

void VAdjustDiskUsage(Error *ec, Volume *vp, afs_int32 blocks, afs_int32 checkBlocks)
{
    VOL_LOCK
    VAdjustDiskUsage_r(ec, vp, blocks, checkBlocks);
    VOL_UNLOCK
}

int VDiskUsage_r(Volume *vp, afs_int32 blocks)
{
    afs_int32 rem, minavail;
    if (blocks > 0) {
#ifdef	AFS_AIX32_ENV
	if ((rem = vp->partition->free - blocks) < 
	    (minavail = (vp->partition->totalUsable * aixlow_water) / 100))
#else
	if (vp->partition->free - blocks < 0)
#endif
	    return(VDISKFULL);
    }    
    vp->partition->free -= blocks;
    return 0;
}

int VDiskUsage(Volume *vp, afs_int32 blocks)
{
    int retVal;
    VOL_LOCK
    retVal = VDiskUsage_r(vp, blocks);
    VOL_UNLOCK
    return retVal;
}

void VPrintDiskStats_r(void)
{
    struct DiskPartition *dp;
    for (dp = DiskPartitionList; dp; dp = dp->next) {
	Log("Partition %s: %d available 1K blocks (minfree=%d), ",
	    dp->name, dp->totalUsable, dp->minFree);
	if (dp->free < 0)
	    Log("overallocated by %d blocks\n", -dp->free);
	else
	    Log("%d free blocks\n", dp->free);
    }
}

void VPrintDiskStats(void)
{
    VOL_LOCK
    VPrintDiskStats_r();
    VOL_UNLOCK
}

#ifdef AFS_NT40_ENV
/* Need a separate lock file on NT, since NT only has mandatory file locks. */
#define LOCKFILE "LOCKFILE"
void VLockPartition_r(char *name)
{
    struct DiskPartition *dp = VGetPartition_r(name, 0);
    OVERLAPPED lap;
    
    if (!dp) return;
    if (dp->lock_fd == -1) {
	char path[64];
	int rc;
	(void) sprintf(path, "%s\\%s", VPartitionPath(dp), LOCKFILE);
	dp->lock_fd = (int)CreateFile(path, GENERIC_WRITE,
				 FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
				 CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL);
	assert (dp->lock_fd != (int)INVALID_HANDLE_VALUE);

	memset((char*)&lap, 0, sizeof(lap));
	rc = LockFileEx((HANDLE)dp->lock_fd, LOCKFILE_EXCLUSIVE_LOCK,
			0, 1, 0, &lap);
	assert(rc);
    }
}

void VUnlockPartition_r(char *name)
{
    register struct DiskPartition *dp = VGetPartition_r(name, 0);
    OVERLAPPED lap;

    if (!dp) return;	/* no partition, will fail later */
    memset((char*)&lap, 0, sizeof(lap));

    UnlockFileEx((HANDLE)dp->lock_fd, 0, 1, 0, &lap);
    CloseHandle((HANDLE)dp->lock_fd);
    dp->lock_fd = -1;
}
#else /* AFS_NT40_ENV */

#if defined(AFS_HPUX_ENV)
#define BITS_PER_CHAR	(8)
#define BITS(type)	(sizeof(type) * BITS_PER_CHAR)

#define LOCKRDONLY_OFFSET	((PRIV_LOCKRDONLY - 1) / BITS(int))
#endif /* defined(AFS_HPUX_ENV) */

void VLockPartition_r(char *name)
{
    register struct DiskPartition *dp = VGetPartition_r(name, 0);
    char *partitionName;
    int retries, code;
    struct timeval pausing;
#if defined(AFS_HPUX_ENV)
    int			lockfRtn;
    struct privgrp_map	privGrpList[PRIV_MAXGRPS];
    unsigned int	*globalMask;
    int			globalMaskIndex;
#endif /* defined(AFS_HPUX_ENV) */
    
    if (!dp) return;	/* no partition, will fail later */
    if (dp->lock_fd != -1) return;

#if    defined(AFS_SUN5_ENV) || defined(AFS_AIX41_ENV)
    partitionName = dp->devName;
    code = O_RDWR;
#else
    partitionName = dp->name;
    code = O_RDONLY;
#endif

    for (retries=25; retries; retries--) {
        dp->lock_fd = open(partitionName, code);
        if (dp->lock_fd != -1) break;
        pausing.tv_sec = 0;
        pausing.tv_usec = 500000;
        select(0, NULL, NULL, NULL, &pausing);
    }
    assert(retries != 0);

#if defined (AFS_HPUX_ENV)

	assert(getprivgrp(privGrpList) == 0);

	/*
	 * In general, it will difficult and time-consuming ,if not impossible,
	 * to try to find the privgroup to which this process belongs that has the
	 * smallest membership, to minimise the security hole.  So, we use the privgrp
	 * to which everybody belongs.
	 */
	/* first, we have to find the global mask */
	for (globalMaskIndex = 0; globalMaskIndex < PRIV_MAXGRPS;
	     globalMaskIndex++) {
	  if (privGrpList[globalMaskIndex].priv_groupno == PRIV_GLOBAL) {
	    globalMask = &(privGrpList[globalMaskIndex].
			   priv_mask[LOCKRDONLY_OFFSET]);
	    break;
	  }
	}

	if (((*globalMask) & privmask(PRIV_LOCKRDONLY)) == 0) {
	  /* allow everybody to set a lock on a read-only file descriptor */
	  (*globalMask) |= privmask(PRIV_LOCKRDONLY);
	  assert(setprivgrp(PRIV_GLOBAL,
			    privGrpList[globalMaskIndex].priv_mask) == 0);

	  lockfRtn = lockf(dp->lock_fd, F_LOCK, 0);

	  /* remove the privilege granted to everybody to lock a read-only fd */
	  (*globalMask) &= ~(privmask(PRIV_LOCKRDONLY));
	  assert(setprivgrp(PRIV_GLOBAL,
			    privGrpList[globalMaskIndex].priv_mask) == 0);
	}
	else {
	  /* in this case, we should be able to do this with impunity, anyway */
	  lockfRtn = lockf(dp->lock_fd, F_LOCK, 0);
	}
	
	assert (lockfRtn != -1); 
#else
#if defined(AFS_AIX_ENV) || defined(AFS_SUN5_ENV)
	assert (lockf(dp->lock_fd, F_LOCK, 0) != -1); 
#else
	assert (flock(dp->lock_fd, LOCK_EX) == 0);
#endif	/* defined(AFS_AIX_ENV) */
#endif
}

void VUnlockPartition_r(char *name)
{
    register struct DiskPartition *dp = VGetPartition_r(name, 0);
    if (!dp) return;	/* no partition, will fail later */
    close(dp->lock_fd);
    dp->lock_fd = -1;
}

#endif /* AFS_NT40_ENV */

void VLockPartition(char *name)
{
    VOL_LOCK
    VLockPartition_r(name);
    VOL_UNLOCK
}

void VUnlockPartition(char *name)
{
    VOL_LOCK
    VUnlockPartition_r(name);
    VOL_UNLOCK
}
