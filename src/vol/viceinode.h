
#ifndef _VICEINODE_H_
#define _VICEINODE_H_

#if !defined(lint) && !defined(LOCORE) && defined(RCS_HDRS)
#endif

/*
 * (C) COPYRIGHT IBM CORPORATION 1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/*

	System:		VICE-TWO
	Module:		viceinode.h
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

/* The four inode parameters for most inodes (files, directories,
   symbolic links) */
struct InodeParams {
    VolId	volumeId;
    VnodeId	vnodeNumber;
    Unique	vnodeUniquifier;
    FileVersion	inodeDataVersion;
};

/* The four inode parameters for special inodes, i.e. the descriptive
   inodes for a volume */
struct SpecialInodeParams {
    VolId	volumeId;
    VnodeId	vnodeNumber; /* this must be INODESPECIAL */
#ifdef	AFS_3DISPARES
    VolId	parentId;
    int		type;
#else
    int		type;
    VolId	parentId;
#endif
};

/* Structure of individual records output by fsck.
   When VICEMAGIC inodes are created, they are given four parameters;
   these correspond to the params.fsck array of this record.
 */
struct ViceInodeInfo {
    Inode	inodeNumber;
    int		byteCount;
    int		linkCount;
    union {
        bit32			  param[4];
        struct InodeParams 	  vnode;
	struct SpecialInodeParams special;
    } u;
}; 

#ifdef	AFS_3DISPARES
#define INODESPECIAL	0x1fffffff	
#else
#define INODESPECIAL	0xffffffff	
#endif
/* Special inode types.  Be careful of the ordering.  Must start at 1.
   See vutil.h */
#define VI_VOLINFO	1	/* The basic volume information file */
#define VI_SMALLINDEX	2	/* The index of small vnodes */
#define VI_LARGEINDEX	3	/* The index of large vnodes */
#define VI_ACL		4	/* The volume's access control list */
#define	VI_MOUNTTABLE	5	/* The volume's mount table */
#define VI_LINKTABLE	6	/* The volume's link counts */

#endif /* _VICEINODE_H_ */
