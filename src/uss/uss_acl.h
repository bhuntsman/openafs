/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * Copyright TRANSARC CORPORATION 1990
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 * uss_acl.h
 *	Interface to the ACL and quota-related operations used by
 *	the AFS user account facility.
 */

#ifndef _USS_ACL_H_
#define _USS_ACL_H_ 1
#include <afs/param.h>
/*
 * ------------------------ Exported functions  -----------------------
 */
extern afs_int32 uss_acl_SetAccess();
    /*
     * Summary:
     *    Set the value of the given ACL.
     *
     * Args:
     *	  a_access   : Ptr to the pathname & ACL to set.
     *	  a_clear    : Should we clear out the ACL first?
     *	  a_negative : Set the negative list?
     *
     * Returns:
     *	  0 if everything went well,
     *	  Lower-level code otherwise.
     */

extern afs_int32 uss_acl_SetDiskQuota();
    /*
     * Summary:
     *    Set the initial disk quota for a user.
     *
     * Args:
     *	  a_path : Pathname for volume mountpoint.
     *	  a_q    : Quota value.
     *
     * Returns:
     *	  0 if everything went well,
     *	  Lower-level code otherwise.
     */

extern afs_int32 uss_acl_CleanUp();
    /*
     * Summary:
     *    Remove the uss_AccountCreator from the various ACLs s/he
     *    had to wiggle into in order to carry out the account
     *    manipulation.
     *
     * Args:
     *	  None.
     *
     * Returns:
     *	  0 (always)
     */

#endif /* _USS_ACL_H_ */
