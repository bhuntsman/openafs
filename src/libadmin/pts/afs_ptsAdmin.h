#ifndef TRANSARC_AFS_PTS_ADMIN_H
#define TRANSARC_AFS_PTS_ADMIN_H

/*
 * Copyright (C)  1998  Transarc Corporation.  All rights reserved.
 *
 */

#include <afs/param.h>
#include <afs/afs_Admin.h>

#define PTS_MAX_NAME_LEN 64
#define PTS_MAX_GROUPS 5000

typedef enum {
  PTS_USER_OWNER_ACCESS,
  PTS_USER_ANYUSER_ACCESS
} pts_userAccess_t, *pts_userAccess_p;

typedef enum {
  PTS_GROUP_OWNER_ACCESS = 10,
  PTS_GROUP_ACCESS,
  PTS_GROUP_ANYUSER_ACCESS
} pts_groupAccess_t, *pts_groupAccess_p;

typedef struct pts_UserEntry {
    int groupCreationQuota;
    int groupMembershipCount;
    int nameUid;
    int ownerUid;
    int creatorUid;
    pts_userAccess_t listStatus;
    pts_userAccess_t listGroupsOwned;
    pts_userAccess_t listMembership;
    char name[PTS_MAX_NAME_LEN];
    char owner[PTS_MAX_NAME_LEN];
    char creator[PTS_MAX_NAME_LEN];
} pts_UserEntry_t, *pts_UserEntry_p;

typedef enum {
  PTS_USER_UPDATE_GROUP_CREATE_QUOTA = 0x1,
  PTS_USER_UPDATE_PERMISSIONS = 0x2
} pts_UserUpdateFlag_t, *pts_UserUpdateFlag_p;

typedef struct pts_UserUpdateEntry {
    pts_UserUpdateFlag_t flag;
    int groupCreationQuota;
    pts_userAccess_t listStatus;
    pts_userAccess_t listGroupsOwned;
    pts_userAccess_t listMembership;
} pts_UserUpdateEntry_t, *pts_UserUpdateEntry_p;

typedef struct pts_GroupEntry {
    int membershipCount;
    int nameUid;
    int ownerUid;
    int creatorUid;
    pts_groupAccess_t listStatus;
    pts_groupAccess_t listGroupsOwned;
    pts_groupAccess_t listMembership;
    pts_groupAccess_t listAdd;
    pts_groupAccess_t listDelete;
    char name[PTS_MAX_NAME_LEN];
    char owner[PTS_MAX_NAME_LEN];
    char creator[PTS_MAX_NAME_LEN];
} pts_GroupEntry_t, *pts_GroupEntry_p;

typedef struct pts_GroupUpdateEntry {
    pts_groupAccess_t listStatus;
    pts_groupAccess_t listGroupsOwned;
    pts_groupAccess_t listMembership;
    pts_groupAccess_t listAdd;
    pts_groupAccess_t listDelete;
} pts_GroupUpdateEntry_t, *pts_GroupUpdateEntry_p;

extern int ADMINAPI pts_GroupMemberAdd(
  const void *cellHandle,
  const char *userName,
  const char *groupName,
  afs_status_p st
);

extern int ADMINAPI pts_GroupOwnerChange(
  const void *cellHandle,
  const char *targetGroup,
  const char *newOwner,
  afs_status_p st
);

extern int ADMINAPI pts_GroupCreate(
  const void *cellHandle,
  const char *newGroup,
  const char *newOwner,
  int *newGroupId,
  afs_status_p st
);

extern int ADMINAPI pts_GroupGet(
  const void *cellHandle,
  const char *groupName,
  pts_GroupEntry_p groupP,
  afs_status_p st
);

extern int ADMINAPI pts_GroupDelete(
  const void *cellHandle,
  const char *groupName,
  afs_status_p st
);

extern int ADMINAPI pts_GroupMaxGet(
  const void *cellHandle,
  int *maxGroupId,
  afs_status_p st
);

extern int ADMINAPI pts_GroupMaxSet(
  const void *cellHandle,
  int maxGroupId,
  afs_status_p st
);

extern int ADMINAPI pts_GroupMemberListBegin(
  const void *cellHandle,
  const char *groupName,
  void **iterationIdP,
  afs_status_p st
);

extern int ADMINAPI pts_GroupMemberListNext(
  const void *iterationId,
  char *memberName,
  afs_status_p st
);

extern int ADMINAPI pts_GroupMemberListDone(
  const void *iterationId,
  afs_status_p st
);

extern int ADMINAPI pts_GroupMemberRemove(
  const void *cellHandle,
  const char *userName,
  const char *groupName,
  afs_status_p st
);

extern int ADMINAPI pts_GroupRename(
  const void *cellHandle,
  const char *oldName,
  const char *newName,
  afs_status_p st
);

extern int ADMINAPI pts_GroupModify(
  const void *cellHandle,
  const char *groupName,
  const pts_GroupUpdateEntry_p newEntryP,
  afs_status_p st
);

extern int ADMINAPI pts_UserCreate(
  const void *cellHandle,
  const char *userName,
  int *newUserId,
  afs_status_p st
);

extern int ADMINAPI pts_UserDelete(
  const void *cellHandle,
  const char *userName,
  afs_status_p st
);

extern int ADMINAPI pts_UserGet(
  const void *cellHandle,
  const char *userName,
  pts_UserEntry_p userP,
  afs_status_p st
);

extern int ADMINAPI pts_UserRename(
  const void *cellHandle,
  const char *oldName,
  const char *newName,
  afs_status_p st
);

extern int ADMINAPI pts_UserModify(
  const void *cellHandle,
  const char *userName,
  const pts_UserUpdateEntry_p newEntryP,
  afs_status_p st
);

extern int ADMINAPI pts_UserMaxGet(
  const void *cellHandle,
  int *maxUserId,
  afs_status_p st
);

extern int ADMINAPI pts_UserMaxSet(
  const void *cellHandle,
  int maxUserId,
  afs_status_p st
);

extern int ADMINAPI pts_UserMemberListBegin(
  const void *cellHandle,
  const char *userName,
  void **iterationIdP,
  afs_status_p st
);

extern int ADMINAPI pts_UserMemberListNext(
  const void *iterationId,
  char *userName,
  afs_status_p st
);

extern int ADMINAPI pts_UserMemberListDone(
  const void *iterationId,
  afs_status_p st
);

extern int ADMINAPI pts_OwnedGroupListBegin(
  const void *cellHandle,
  const char *userName,
  void **iterationIdP,
  afs_status_p st
);

extern int ADMINAPI pts_OwnedGroupListNext(
  const void *iterationId,
  char *groupName,
  afs_status_p st
);

extern int ADMINAPI pts_OwnedGroupListDone(
  const void *iterationId,
  afs_status_p st
);

#endif /* TRANSARC_AFS_PTS_ADMIN_H */
