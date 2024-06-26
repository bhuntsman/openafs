/*
 * (C) COPYRIGHT IBM CORPORATION 1997
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#ifndef __AFS_USROPS_H__
#define __AFS_USROPS_H__ 1

#if !defined(UKERNEL)
#include <afs/sysincludes.h>
#endif /* !defined(UKERNEL) */

/*
 * Macros to manipulate doubly linked lists
 */
#define DLL_INIT_LIST(_HEAD, _TAIL) \
    { _HEAD = NULL ; _TAIL = NULL; }

#define DLL_INSERT_TAIL(_ELEM, _HEAD, _TAIL, _NEXT, _PREV) \
{ \
    if (_HEAD == NULL) { \
	_ELEM->_NEXT = NULL; \
	_ELEM->_PREV = NULL; \
	_HEAD = _ELEM; \
	_TAIL = _ELEM; \
    } else { \
	_ELEM->_NEXT = NULL; \
	_ELEM->_PREV = _TAIL; \
	_TAIL->_NEXT = _ELEM; \
	_TAIL = _ELEM; \
    } \
}

#define DLL_DELETE(_ELEM, _HEAD, _TAIL, _NEXT, _PREV) \
{ \
    if (_ELEM->_NEXT == NULL) { \
	_TAIL = _ELEM->_PREV; \
    } else { \
	_ELEM->_NEXT->_PREV = _ELEM->_PREV; \
    } \
    if (_ELEM->_PREV == NULL) { \
	_HEAD = _ELEM->_NEXT; \
    } else { \
	_ELEM->_PREV->_NEXT = _ELEM->_NEXT; \
    } \
    _ELEM->_NEXT = NULL; \
    _ELEM->_PREV = NULL; \
}

extern struct afsconf_dir *afs_cdir;
extern char afs_LclCellName[64];

extern int afs_osicred_Initialized;

extern struct usr_vnode *afs_RootVnode;

extern struct usr_vnode *afs_CurrentDir;
extern struct usr_vnode *afs_FileTable[];
extern int afs_FileFlags[];
extern int afs_FileOffsets[];

extern char afs_mountDir[];
extern int afs_mountDirLen;

extern void uafs_InitClient(void);
extern void uafs_InitThread(void);
extern void uafs_Init(char *, char *, char *, char *, int, int, int, int,
		      int, int, int, int, int, int, char *);
extern void uafs_RxServerProc(void);
extern int uafs_LookupLink(struct usr_vnode *vp, struct usr_vnode *parentP,
			   struct usr_vnode **vpp);
extern int uafs_LookupName(char *path, struct usr_vnode *parentP,
			   struct usr_vnode **vpp, int follow);
extern int uafs_LookupParent(char *path, struct usr_vnode **vpp);
extern int uafs_GetAttr(struct usr_vnode *vp, struct stat *stats);

extern int uafs_SetTokens(char *buf, int len);
extern int uafs_SetTokens_r(char *buf, int len);
extern int uafs_mkdir(char *path, int mode);
extern int uafs_mkdir_r(char *path, int mode);
extern int uafs_chdir(char *path);
extern int uafs_chdir_r(char *path);
extern int uafs_open(char *path, int flags, int mode);
extern int uafs_open_r(char *path, int flags, int mode);
extern int uafs_creat(char *path, int mode);
extern int uafs_creat_r(char *path, int mode);
extern int uafs_write(int fd, char *buf, int len);
extern int uafs_write_r(int fd, char *buf, int len);
extern int uafs_read(int fd, char *buf, int len);
extern int uafs_read_r(int fd, char *buf, int len);
extern int uafs_fsync(int fd);
extern int uafs_fsync_r(int fd);
extern int uafs_close(int fd);
extern int uafs_close_r(int fd);
extern int uafs_stat(char *path, struct stat *stats);
extern int uafs_stat_r(char *path, struct stat *stats);
extern int uafs_lstat(char *path, struct stat *stats);
extern int uafs_lstat_r(char *path, struct stat *stats);
extern int uafs_fstat(int fd, struct stat *stats);
extern int uafs_fstat_r(int fd, struct stat *stats);
extern int uafs_truncate(char *path, int len);
extern int uafs_truncate_r(char *path, int len);
extern int uafs_ftruncate(int fd, int len);
extern int uafs_ftruncate_r(int fd, int len);
extern int uafs_chmod(char *path, int mode);
extern int uafs_chmod_r(char *path, int mode);
extern int uafs_fchmod(int fd, int mode);
extern int uafs_fchmod_r(int fd, int mode);
extern int uafs_symlink(char *target, char *source);
extern int uafs_symlink_r(char *target, char *source);
extern int uafs_unlink(char *path);
extern int uafs_unlink_r(char *path);
extern int uafs_rmdir(char *path);
extern int uafs_rmdir_r(char *path);
extern int uafs_readlink(char *path, char *buf, int len);
extern int uafs_readlink_r(char *path, char *buf, int len);
extern int uafs_link(char *existing, char *new);
extern int uafs_link_r(char *existing, char *new);
extern int uafs_rename(char *old, char *new);
extern int uafs_rename_r(char *old, char *new);
extern int uafs_FlushFile(char *path);
extern int uafs_FlushFile_r(char *path);
extern usr_DIR *uafs_opendir(char *path);
extern usr_DIR *uafs_opendir_r(char *path);
extern struct usr_dirent *uafs_readdir(usr_DIR *dirp);
extern struct usr_dirent *uafs_readdir_r(usr_DIR *dirp);
extern int uafs_getdents(int fd, struct min_direct *buf, int len);
extern int uafs_getdents_r(int fd, struct min_direct *buf, int len);
extern int uafs_closedir(usr_DIR *dirp);
extern int uafs_closedir_r(usr_DIR *dirp);
extern void uafs_ThisCell(char *namep);
extern void uafs_ThisCell_r(char *namep);
extern int uafs_klog(char *user,char *cell,char *passwd,char **reason);
extern int uafs_klog_r(char *user,char *cell,char *passwd,char **reason);
extern int uafs_unlog(void);
extern int uafs_unlog_r(void);
extern void uafs_SetRxPort(int);
extern char *uafs_afsPathName(char *);
extern int uafs_RPCStatsEnableProc(void);
extern int uafs_RPCStatsDisableProc(void);
extern int uafs_RPCStatsEnablePeer(void);
extern int uafs_RPCStatsDisablePeer(void);

#endif /* __AFS_USROPS_H__ */
