/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements:
 * setgroups (syscall)
 * setpag
 *
 */
#include <afsconfig.h>
#include "afs/param.h"
#ifdef LINUX_KEYRING_SUPPORT
#include <linux/seq_file.h>
#endif


#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/afs_stats.h"	/* statistics */
#include "afs/nfsclient.h"
#include <linux/smp_lock.h>

#ifdef AFS_LINUX26_ONEGROUP_ENV
#define NUMPAGGROUPS 1
#else
#define NUMPAGGROUPS 2
#endif

static int
afs_setgroups(cred_t **cr, struct group_info *group_info, int change_parent)
{
    struct group_info *old_info;

    AFS_STATCNT(afs_setgroups);

    old_info = afs_cr_group_info(*cr);
    get_group_info(group_info);
    afs_set_cr_group_info(*cr, group_info);
    put_group_info(old_info);

    crset(*cr);

#if defined(STRUCT_TASK_STRUCT_HAS_PARENT) && !defined(STRUCT_TASK_HAS_CRED)
    if (change_parent) {
	old_info = current->parent->group_info;
	get_group_info(group_info);
	current->parent->group_info = group_info;
	put_group_info(old_info);
    }
#endif

    return (0);
}
/* Returns number of groups. And we trust groups to be large enough to
 * hold all the groups.
 */
static struct group_info *
afs_getgroups(cred_t * cr)
{
    AFS_STATCNT(afs_getgroups);

    get_group_info(afs_cr_group_info(cr));
    return afs_cr_group_info(cr);
}

int
__setpag(cred_t **cr, afs_uint32 pagvalue, afs_uint32 *newpag,
         int change_parent)
{
    struct group_info *group_info;
#ifndef AFS_LINUX26_ONEGROUP_ENV
    gid_t g0, g1;
#endif
    struct group_info *tmp;
    int i;
#ifdef AFS_LINUX26_ONEGROUP_ENV
    int j;
#endif
    int need_space = 0;

    group_info = afs_getgroups(*cr);
    if (group_info->ngroups < NUMPAGGROUPS
	||  afs_get_pag_from_groups(
#ifdef AFS_LINUX26_ONEGROUP_ENV
	    group_info
#else
	    GROUP_AT(group_info, 0) ,GROUP_AT(group_info, 1)
#endif
				    ) == NOPAG) 
	/* We will have to make sure group_info is big enough for pag */
	need_space = NUMPAGGROUPS;

    tmp = groups_alloc(group_info->ngroups + need_space);
    
    *newpag = (pagvalue == -1 ? genpag() : pagvalue);
#ifdef AFS_LINUX26_ONEGROUP_ENV
    for (i = 0, j = 0; i < group_info->ngroups; ++i) {
	int ths = GROUP_AT(group_info, i);
	int last = i > 0 ? GROUP_AT(group_info, i-1) : 0;
	if ((ths >> 24) == 'A')
	    continue;
	if (last <= *newpag && ths > *newpag) {
	   GROUP_AT(tmp, j) = *newpag;
	   j++;
	}
	GROUP_AT(tmp, j) = ths;
	j++;
    }
    if (j != i + need_space)
        GROUP_AT(tmp, j) = *newpag;
#else
    for (i = 0; i < group_info->ngroups; ++i)
      GROUP_AT(tmp, i + need_space) = GROUP_AT(group_info, i);
#endif
    put_group_info(group_info);
    group_info = tmp;

#ifndef AFS_LINUX26_ONEGROUP_ENV
    afs_get_groups_from_pag(*newpag, &g0, &g1);
    GROUP_AT(group_info, 0) = g0;
    GROUP_AT(group_info, 1) = g1;
#endif

    afs_setgroups(cr, group_info, change_parent);

    put_group_info(group_info);

    return 0;
}

#ifdef LINUX_KEYRING_SUPPORT
extern struct key_type key_type_keyring __attribute__((weak));
static struct key_type *__key_type_keyring = &key_type_keyring;

static int
install_session_keyring(struct key *keyring)
{
    struct key *old;
    char desc[20];
    unsigned long not_in_quota;
    int code = -EINVAL;

    if (!__key_type_keyring)
	return code;

    if (!keyring) {

	/* create an empty session keyring */
	not_in_quota = KEY_ALLOC_IN_QUOTA;
	sprintf(desc, "_ses.%u", current->tgid);

#if defined(KEY_ALLOC_NEEDS_STRUCT_TASK)
	keyring = key_alloc(__key_type_keyring, desc,
			    current_uid(), current_gid(), current,
			    (KEY_POS_ALL & ~KEY_POS_SETATTR) | KEY_USR_ALL,
			    not_in_quota);
#elif defined(KEY_ALLOC_NEEDS_CRED)
	keyring = key_alloc(__key_type_keyring, desc,
			    current_uid(), current_gid(), current_cred(),
			    (KEY_POS_ALL & ~KEY_POS_SETATTR) | KEY_USR_ALL,
			    not_in_quota);
#else
	keyring = key_alloc(__key_type_keyring, desc,
			    current_uid(), current_gid(),
			    (KEY_POS_ALL & ~KEY_POS_SETATTR) | KEY_USR_ALL,
			    not_in_quota);
#endif
	if (IS_ERR(keyring)) {
	    code = PTR_ERR(keyring);
	    goto out;
	}
    }

    code = key_instantiate_and_link(keyring, NULL, 0, NULL, NULL);
    if (code < 0) {
	key_put(keyring);
	goto out;
    }

    /* install the keyring */
    spin_lock_irq(&current->sighand->siglock);
    old = task_session_keyring(current);
    smp_wmb();
    task_session_keyring(current) = keyring;
    spin_unlock_irq(&current->sighand->siglock);

    if (old)
	    key_put(old);

out:
    return code;
}
#endif /* LINUX_KEYRING_SUPPORT */

int
setpag(cred_t **cr, afs_uint32 pagvalue, afs_uint32 *newpag,
       int change_parent)
{
    int code;

    AFS_STATCNT(setpag);

    code = __setpag(cr, pagvalue, newpag, change_parent);

#ifdef LINUX_KEYRING_SUPPORT
    if (code == 0 && afs_cr_rgid(*cr) != NFSXLATOR_CRED) {
	(void) install_session_keyring(NULL);

	if (current_session_keyring()) {
	    struct key *key;
	    key_perm_t perm;

	    perm = KEY_POS_VIEW | KEY_POS_SEARCH;
	    perm |= KEY_USR_VIEW | KEY_USR_SEARCH;

#if defined(KEY_ALLOC_NEEDS_STRUCT_TASK)
	    key = key_alloc(&key_type_afs_pag, "_pag", 0, 0, current, perm, 1);
#elif defined(KEY_ALLOC_NEEDS_CRED)
	    key = key_alloc(&key_type_afs_pag, "_pag", 0, 0, current_cred(), perm, 1);
#else
	    key = key_alloc(&key_type_afs_pag, "_pag", 0, 0, perm, 1);
#endif

	    if (!IS_ERR(key)) {
		key_instantiate_and_link(key, (void *) newpag, sizeof(afs_uint32),
					 current_session_keyring(), NULL);
		key_put(key);
	    }
	}
    }
#endif /* LINUX_KEYRING_SUPPORT */

    return code;
}


/* Intercept the standard system call. */
extern asmlinkage long (*sys_setgroupsp) (int gidsetsize, gid_t * grouplist);
asmlinkage long
afs_xsetgroups(int gidsetsize, gid_t * grouplist)
{
    long code;
    cred_t *cr = crref();
    afs_uint32 junk;
    int old_pag;

    lock_kernel();
    old_pag = PagInCred(cr);
    crfree(cr);
    unlock_kernel();

    code = (*sys_setgroupsp) (gidsetsize, grouplist);
    if (code) {
	return code;
    }

    lock_kernel();
    cr = crref();
    if (old_pag != NOPAG && PagInCred(cr) == NOPAG) {
	/* re-install old pag if there's room. */
	code = __setpag(&cr, old_pag, &junk, 0);
    }
    crfree(cr);
    unlock_kernel();

    /* Linux syscall ABI returns errno as negative */
    return (-code);
}

/* Intercept the standard uid32 system call. */
extern asmlinkage long (*sys_setgroups32p) (int gidsetsize, gid_t * grouplist);
asmlinkage long
afs_xsetgroups32(int gidsetsize, gid_t * grouplist)
{
    long code;
    cred_t *cr = crref();
    afs_uint32 junk;
    int old_pag;

    lock_kernel();
    old_pag = PagInCred(cr);
    crfree(cr);
    unlock_kernel();

    code = (*sys_setgroups32p) (gidsetsize, grouplist);

    if (code) {
	return code;
    }

    lock_kernel();
    cr = crref();
    if (old_pag != NOPAG && PagInCred(cr) == NOPAG) {
	/* re-install old pag if there's room. */
	code = __setpag(&cr, old_pag, &junk, 0);
    }
    crfree(cr);
    unlock_kernel();

    /* Linux syscall ABI returns errno as negative */
    return (-code);
}

#if defined(AFS_PPC64_LINUX20_ENV)
/* Intercept the uid16 system call as used by 32bit programs. */
extern long (*sys32_setgroupsp)(int gidsetsize, gid_t *grouplist);
asmlinkage long afs32_xsetgroups(int gidsetsize, gid_t *grouplist)
{
    long code;
    cred_t *cr = crref();
    afs_uint32 junk;
    int old_pag;
    
    lock_kernel();
    old_pag = PagInCred(cr);
    crfree(cr);
    unlock_kernel();
    
    code = (*sys32_setgroupsp)(gidsetsize, grouplist);
    if (code) {
	return code;
    }
    
    lock_kernel();
    cr = crref();
    if (old_pag != NOPAG && PagInCred(cr) == NOPAG) {
	/* re-install old pag if there's room. */
	code = __setpag(&cr, old_pag, &junk, 0);
    }
    crfree(cr);
    unlock_kernel();
    
    /* Linux syscall ABI returns errno as negative */
    return (-code);
}
#endif

#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_AMD64_LINUX20_ENV)
/* Intercept the uid16 system call as used by 32bit programs. */
extern long (*sys32_setgroupsp) (int gidsetsize, u16 * grouplist);
asmlinkage long
afs32_xsetgroups(int gidsetsize, u16 * grouplist)
{
    long code;
    cred_t *cr = crref();
    afs_uint32 junk;
    int old_pag;
    
    lock_kernel();
    old_pag = PagInCred(cr);
    crfree(cr);
    unlock_kernel();
    
    code = (*sys32_setgroupsp) (gidsetsize, grouplist);
    if (code) {
	return code;
    }
    
    lock_kernel();
    cr = crref();
    if (old_pag != NOPAG && PagInCred(cr) == NOPAG) {
	/* re-install old pag if there's room. */
	code = __setpag(&cr, old_pag, &junk, 0);
    }
    crfree(cr);
    unlock_kernel();
    
    /* Linux syscall ABI returns errno as negative */
    return (-code);
}

/* Intercept the uid32 system call as used by 32bit programs. */
extern long (*sys32_setgroups32p) (int gidsetsize, gid_t * grouplist);
asmlinkage long
afs32_xsetgroups32(int gidsetsize, gid_t * grouplist)
{
    long code;
    cred_t *cr = crref();
    afs_uint32 junk;
    int old_pag;

    lock_kernel();
    old_pag = PagInCred(cr);
    crfree(cr);
    unlock_kernel();

    code = (*sys32_setgroups32p) (gidsetsize, grouplist);
    if (code) {
	return code;
    }

    lock_kernel();
    cr = crref();
    if (old_pag != NOPAG && PagInCred(cr) == NOPAG) {
	/* re-install old pag if there's room. */
	code = __setpag(&cr, old_pag, &junk, 0);
    }
    crfree(cr);
    unlock_kernel();

    /* Linux syscall ABI returns errno as negative */
    return (-code);
}
#endif


#ifdef LINUX_KEYRING_SUPPORT
static void afs_pag_describe(const struct key *key, struct seq_file *m)
{
    seq_puts(m, key->description);

    seq_printf(m, ": %u", key->datalen);
}

static int afs_pag_instantiate(struct key *key, const void *data, size_t datalen)
{
    int code;
    afs_uint32 *userpag, pag = NOPAG;
#ifndef AFS_LINUX26_ONEGROUP_ENV
    int g0, g1;
#endif

    if (key->uid != 0 || key->gid != 0)
	return -EPERM;

    code = -EINVAL;
    get_group_info(current_group_info());

    if (datalen != sizeof(afs_uint32) || !data)
	goto error;

    if (current_group_info()->ngroups < NUMPAGGROUPS)
	goto error;

    /* ensure key being set matches current pag */
#ifdef AFS_LINUX26_ONEGROUP_ENV
    pag = afs_get_pag_from_groups(current_group_info());
#else
    g0 = GROUP_AT(current_group_info(), 0);
    g1 = GROUP_AT(current_group_info(), 1);

    pag = afs_get_pag_from_groups(g0, g1);
#endif
    if (pag == NOPAG)
	goto error;

    userpag = (afs_uint32 *) data;
    if (*userpag != pag)
	goto error;

    key->payload.value = (unsigned long) *userpag;
    key->datalen = sizeof(afs_uint32);
    code = 0;

error:
    put_group_info(current_group_info());
    return code;
}

static int afs_pag_match(const struct key *key, const void *description)
{
	return strcmp(key->description, description) == 0;
}

static void afs_pag_destroy(struct key *key)
{
    afs_uint32 pag = key->payload.value;
    struct unixuser *pu;
    int locked = ISAFS_GLOCK();

    if (!locked)
	AFS_GLOCK();
    pu = afs_FindUser(pag, -1, READ_LOCK);
    if (pu) {
	pu->ct.EndTimestamp = 0;
	pu->tokenTime = 0;
	afs_PutUser(pu, READ_LOCK);
    }
    if (!locked)
	AFS_GUNLOCK();
}

struct key_type key_type_afs_pag =
{
    .name        = "afs_pag",
    .describe    = afs_pag_describe,
    .instantiate = afs_pag_instantiate,
    .match       = afs_pag_match,
    .destroy     = afs_pag_destroy,
};

#ifdef EXPORTED_TASKLIST_LOCK
extern rwlock_t tasklist_lock __attribute__((weak));
#endif

void osi_keyring_init(void)
{
#if !defined(EXPORTED_KEY_TYPE_KEYRING)
    struct task_struct *p;

    /* If we can't lock the tasklist, either with its explicit lock,
     * or by using the RCU lock, then we can't safely work out the 
     * type of a keyring. So, we have to rely on the weak reference. 
     * If that's not available, then keyring based PAGs won't work.
     */
    
#if defined(EXPORTED_TASKLIST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16) && defined(EXPORTED_RCU_READ_LOCK))
    if (__key_type_keyring == NULL) {
# ifdef EXPORTED_TASKLIST_LOCK
	if (&tasklist_lock)
	    read_lock(&tasklist_lock);
# endif
# if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16) && defined(EXPORTED_RCU_READ_LOCK))
#  ifdef EXPORTED_TASKLIST_LOCK
 	else
#  endif
	    rcu_read_lock();
# endif
#if defined(EXPORTED_FIND_TASK_BY_PID)
	p = find_task_by_pid(1);
#else
	p = find_task_by_vpid(1);
#endif
	if (p && task_user(p)->session_keyring)
	    __key_type_keyring = task_user(p)->session_keyring->type;
# ifdef EXPORTED_TASKLIST_LOCK
	if (&tasklist_lock)
	    read_unlock(&tasklist_lock);
# endif
# if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16) && defined(EXPORTED_RCU_READ_LOCK))
#  ifdef EXPORTED_TASKLIST_LOCK
	else
#  endif
	    rcu_read_unlock();
# endif
    }
#endif
#endif

    register_key_type(&key_type_afs_pag);
}

void osi_keyring_shutdown(void)
{
    unregister_key_type(&key_type_afs_pag);
}

afs_int32
osi_get_keyring_pag(afs_ucred_t *cred)
{
    struct key *key;
    afs_uint32 newpag;
    afs_int32 keyring_pag = NOPAG;

    if (afs_cr_rgid(cred) != NFSXLATOR_CRED) {

#if defined(STRUCT_TASK_HAS_CRED)
	/* If we have a kernel cred, search the passed credentials */
	key = key_ref_to_ptr(keyring_search(make_key_ref(cred->tgcred->session_keyring, 1),
		&key_type_afs_pag, "_pag"));
#else
	/* Search the keyrings of the current process */
	key = request_key(&key_type_afs_pag, "_pag", NULL);
#endif
	if (!IS_ERR(key)) {
	    if (key_validate(key) == 0 && key->uid == 0) {      /* also verify in the session keyring? */
		keyring_pag = key->payload.value;
		/* Only set PAG in groups if needed, and the creds are from the current process */
#if defined(STRUCT_TASK_HAS_CRED)
		if (cred == current_cred() && ((keyring_pag >> 24) & 0xff) == 'A') {
#else
		if (((keyring_pag >> 24) & 0xff) == 'A') {
#endif
		    if (keyring_pag != afs_get_pag_from_groups(current_group_info()))
			__setpag(&cred, keyring_pag, &newpag, 0);
		}
	    }
	    key_put(key);
	}
    }
    return keyring_pag;
}

#else
void osi_keyring_init(void)
{
	return;
}

void osi_keyring_shutdown(void)
{
	return;
}
#endif
