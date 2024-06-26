/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * Copyright TRANSARC CORPORATION 1990
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 * uss_vol.h
 *	Interface to the volume operations used by the AFS user
 *	account facility.
 */

#ifndef _USS_VOL_H_
#define _USS_VOL_H_ 1
#include <afs/param.h>

/*
 * ------------------------ Exported functions  -----------------------
 */
extern afs_int32 uss_vol_GetServer();
    /*
     * Summary:
     *    Given the string name of a desired host, find its address.
     *
     * Args:
     *	  a_name : String name of desired host.
     *
     * Returns:
     *	  Host address in network byte order.
     */

extern afs_int32 uss_vol_GetPartitionID();
    /*
     * Summary:
     *    Get partition id from a name.
     *
     * Args:
     *	  a_name : Name of the partition ID.
     *
     * Returns:
     *	  Numeric partition name, or -1 on failure.
     */

extern afs_int32 uss_vol_CreateVol();
    /*
     * Summary:
     *    Create a volume, set its disk quota, and mount it at the
     *	  given place.  Also, set the mountpoint's ACL.
     *
     * Args:
     *	  char *a_volname   : Volume name to mount.
     *	  char *a_server    : FileServer housing the volume
     *	  char *a_partition : Partition housing the volume
     *	  char *a_quota     : Initial quota
     *	  char *a_mpoint    : Mountpoint to assign it
     *	  char *a_owner	    : Name of mountpoint's owner
     *	  char *a_acl	    : ACL for mountpoint.
     *
     * Returns:
     *	  0 if everything went well,
     *	  1 if there was a problem in the routine itself, or
     *	  Other error code if problem occurred in lower-level call.
     */

extern afs_int32 uss_vol_DeleteVol();
    /*
     * Summary:
     *    Delete the given volume.
     *
     * Args:
     *	  char *a_volName  : Name of the volume to delete.
     *	  afs_int32 a_volID     : Numerical volume ID.
     *	  char *a_servName : Name of the server hosting the volume.
     *	  afs_int32 a_servID    : Numerical server ID.
     *	  char *a_partName : Name of the home server partition.
     *	  afs_int32 a_volID     : Numerical partition ID.
     *
     * Returns:
     *	  0 if everything went well,
     *	  1 if there was a problem in the routine itself, or
     *	  Other error code if problem occurred in lower-level call.
     */

extern afs_int32 uss_vol_GetVolInfoFromMountPoint();
    /*
     * Summary:
     *    Given a mountpoint, pull out the name of the volume mounted
     *	  there, along with the name of the FileServer and partition
     *	  hosting it, putting them all in common locations.
     *
     * Args:
     *	  char *a_mountpoint : Name of the mountpoint.
     *
     * Returns:
     *	  0 if everything went well,
     *	  1 if there was a problem in the routine itself, or
     *	  Other error code if problem occurred in lower-level call.
     */

extern afs_int32 uss_vol_DeleteMountPoint();
    /*
     * Summary:
     *    Given a mountpoint, nuke it.
     *
     * Args:
     *	  char *a_mountpoint : Name of the mountpoint.
     *
     * Returns:
     *	  0 if everything went well,
     *	  1 if there was a problem in the routine itself, or
     *	  Other error code if problem occurred in lower-level call.
     */

#endif /* _USS_VOL_H_ */
