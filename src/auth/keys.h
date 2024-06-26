/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#ifndef __KEYS_AFS_INCL_
#define	__KEYS_AFS_INCL_    1

/* include file for getting keys from the server's key file in /usr/afs/etc/ServerKeys.
    The format of server keys is a long count, followed by that many
    structures containing a kvno long and a key.  The key is treated
    as an array of 8 chars, but the longs should be in network byte order.
*/

#define	AFSCONF_MAXKEYS	    8

struct afsconf_key {
    afs_int32 kvno;
    char key[8];
};

struct afsconf_keys {
    afs_int32 nkeys;
    struct afsconf_key key[AFSCONF_MAXKEYS];
};

#define	AFSCONF_KEYINUSE	512	/* Random error code - only "locally" unique */

#endif /* __KEYS_AFS_INCL_ */
