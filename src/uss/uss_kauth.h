/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * Copyright TRANSARC CORPORATION 1990
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 * uss_kauth.h
 *	Interface to the authentication-related procedures for the
 *	AFS user account facility.
 *
 */

#ifndef _USS_KAUTH_H_
#define _USS_KAUTH_H_ 1
#include <afs/param.h>
/*
 * --------------------- Required definitions ---------------------
 */


/*
 * --------------------- Exported definitions ---------------------
 */


/*
 * ------------------------ Exported functions  -----------------------
 */
extern afs_int32 uss_kauth_InitAccountCreator();
    /*
     * Summary:
     *    Initialize the variable uss_AccountCreator().
     *
     * Args:
     *    None.
     *
     * Returns:
     *    0 if everything went well,
     *    1 if couldn't get user name from getpwuid().
     */

extern afs_int32 uss_kauth_AddUser();
    /*
     * Summary:
     *    Register the given user with the Authentication Server.
     *
     * Args:
     *	  char *a_user   : Name of the user to register.
     *	  char *a_passwd : User's (cleartext) password.
     *
     * Returns:
     *	  0 if everything went well,
     *	  1 if there was a problem encountered in this function, or
     *	  Code returned from a lower-level call.
     */

extern afs_int32 uss_kauth_DelUser();
    /*
     * Summary:
     *    Delete the given user from the Authentication Database.
     *
     * Args:
     *	  char *a_user : Name of the user to delete.
     *
     * Returns:
     *	  0 if everything went well,
     *	  1 if there was a problem encountered in this function, or
     *	  Code returned from a lower-level call.
     */

extern afs_int32 uss_kauth_CheckUserName();
    /*
     * Summary:
     *    Make sure the parsed user name is a legal one.
     *
     * Args:
     *	  None.
     *
     * Returns:
     *	  0 if everything went well,
     *	  1 if the user name is not legal.
     */

#endif /* _USS_KAUTH_H_ */
