#ifndef TRANSARC_AFS_ADMIN_H
#define TRANSARC_AFS_ADMIN_H

/*
 * Copyright (C)  1998  Transarc Corporation.  All rights reserved.
 *
 */

#include <afs/param.h>
#include <afs/afs_args.h>
#include <rx/rx.h>

#ifdef AFS_NT40_ENV
/* NT definitions */
#define ADMINAPI __cdecl

#ifndef ADMINEXPORT
#define ADMINEXPORT  __declspec(dllimport)
#endif

#else
/* Unix definitions */
#define ADMINAPI
#define ADMINEXPORT
#endif /* AFS_NT40_ENV */


typedef unsigned int afs_status_t, *afs_status_p;

typedef enum {
  AFS_RPC_STATS_DISABLED,
  AFS_RPC_STATS_ENABLED
} afs_RPCStatsState_t, *afs_RPCStatsState_p;

typedef afs_uint32 afs_RPCStatsVersion_t, *afs_RPCStatsVersion_p;

#define AFS_RX_STATS_CLEAR_ALL 			 	0xffffffff
#define AFS_RX_STATS_CLEAR_INVOCATIONS 			0x1
#define AFS_RX_STATS_CLEAR_TIME_SUM 			0x2
#define AFS_RX_STATS_CLEAR_TIME_SQUARE 			0x4
#define AFS_RX_STATS_CLEAR_TIME_MIN 			0x8
#define AFS_RX_STATS_CLEAR_TIME_MAX 			0x10

typedef afs_uint32 afs_RPCStatsClearFlag_t, *afs_RPCStatsClearFlag_p;

typedef union {
  rx_function_entry_v1_t stats_v1;
  /* add new stat structures here when required */
} afs_RPCUnion_t, *afs_RPCUnion_p;

typedef struct afs_RPCStats {
  afs_uint32 clientVersion;
  afs_uint32 serverVersion;
  afs_uint32 statCount;
  afs_RPCUnion_t s;
} afs_RPCStats_t, *afs_RPCStats_p;

typedef union {
  cm_initparams_v1 config_v1;
  /* add new client config structures here when required */
} afs_ClientConfigUnion_t, *afs_ClientConfigUnion_p;

typedef struct afs_ClientConfig {
  afs_uint32 clientVersion;
  afs_uint32 serverVersion;
  afs_ClientConfigUnion_t c;
} afs_ClientConfig_t, *afs_ClientConfig_p;

#if AFS_NT40_ENV
typedef HANDLE rxdebugSocket_t;
#else /* AFS_NT40_ENV */
typedef int rxdebugSocket_t;
#endif /* AFS_NT40_ENV */
#define INVALID_RXDEBUG_SOCKET	((rxdebugSocket_t)-1)

typedef struct {
  rxdebugSocket_t sock;
  int ipAddr;
  int udpPort;
  int firstFlag;
  afs_uint32 supportedStats;
} rxdebugHandle_t, *rxdebugHandle_p;

#define AFS_STATUS_OK 0

#endif /* TRANSARC_AFS_ADMIN_H */
