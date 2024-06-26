/* ticket caching code */

/* Copyright (C) 1990, 1989 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 */

#include <afs/param.h>
#include <afs/stds.h>
#include <stdio.h>
#include <afs/pthread_glock.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <rpc.h>
#include <afs/smb_iocons.h>
#include <afs/pioctl_nt.h>
#include "../WINNT/afsd/afsrpc.h"
#include <afs/vice.h>
#include "auth.h"
#include <afs/afsutil.h>


/* Forward declarations for local token cache. */
static int SetLocalToken(struct ktc_principal *aserver,
			 struct ktc_token *atoken,
			 struct ktc_principal *aclient,
			 afs_int32 flags);
static int GetLocalToken(struct ktc_principal *aserver,
			 struct ktc_token *atoken,
			 int atokenLen,
			 struct ktc_principal *aclient);
static int ForgetLocalTokens();
static int ForgetOneLocalToken(struct ktc_principal *aserver);


static char AFSConfigKeyName[] =
	"SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters";

/*
 * Support for RPC's to send and receive session keys
 *
 * Session keys must not be sent and received in the clear.  We have no
 * way to piggyback encryption on SMB, so we use a separate RPC, using
 * packet privacy (when available).  In SetToken, the RPC is done first;
 * in GetToken, the pioctl is done first.
 */

char rpcErr[256];

void __RPC_FAR * __RPC_USER midl_user_allocate (size_t cBytes)
{
	return ((void __RPC_FAR *) malloc(cBytes));
}

void __RPC_USER midl_user_free(void __RPC_FAR * p)
{
	free(p);
}

/*
 * Determine the server name to be used in the RPC binding.  If it is
 * the same as the client (i.e. standalone, non-gateway), NULL can be
 * used, so it is not necessary to call gethostbyname().
 */
void getservername(char **snp, unsigned int snSize)
{
	HKEY parmKey;
	long code;

	code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSConfigKeyName,
			    0, KEY_QUERY_VALUE, &parmKey);
	if (code != ERROR_SUCCESS)
		goto nogateway;
	code = RegQueryValueEx(parmKey, "Gateway", NULL, NULL,
				*snp, &snSize);
	RegCloseKey (parmKey);
	if (code == ERROR_SUCCESS)
		return;
nogateway:
	/* No gateway name in registry; use ourself */
	*snp = NULL;
}

RPC_STATUS send_key(afs_uuid_t uuid, char sessionKey[8])
{
	RPC_STATUS status;
	char *stringBinding = NULL;
	ULONG authnLevel, authnSvc;
	char serverName[256];
	char *serverNamep = serverName;
	char encrypt[32];
	BOOL encryptionOff = FALSE;
	char protseq[32];

	/* Encryption on by default */
	if (GetEnvironmentVariable("AFS_RPC_ENCRYPT",
				   encrypt, sizeof(encrypt)))
		if (!strcmpi(encrypt, "OFF"))
			encryptionOff = TRUE;

	/* Protocol sequence is named pipe by default */
	if (!GetEnvironmentVariable("AFS_RPC_PROTSEQ",
				    protseq, sizeof(protseq)))
		strcpy(protseq, "ncacn_np");

	/* Server name */
	getservername(&serverNamep, sizeof(serverName));

	status = RpcStringBindingCompose("",	/* obj uuid */
					 protseq,
					 serverNamep,
					 "",	/* endpoint */
					 "",	/* protocol options */
					 &stringBinding);
	if (status != RPC_S_OK)
		goto cleanup;

	status = RpcBindingFromStringBinding(stringBinding, &hAfsHandle);
	if (status != RPC_S_OK)
		goto cleanup;

	/*
	 * On Windows 95/98, we must resolve the binding before calling
	 * SetAuthInfo.  On Windows NT, we don't have to resolve yet,
	 * but it does no harm.
	 */
	status = RpcEpResolveBinding(hAfsHandle, afsrpc_v1_0_c_ifspec);
	if (status != RPC_S_OK)
		goto cleanup;

	if (encryptionOff) {
		authnLevel = RPC_C_AUTHN_LEVEL_NONE;
		authnSvc = RPC_C_AUTHN_WINNT;
	} else {
		authnLevel = RPC_C_AUTHN_LEVEL_PKT_PRIVACY;
		authnSvc = RPC_C_AUTHN_WINNT;
	}

	status = RpcBindingSetAuthInfo(hAfsHandle, NULL, authnLevel, authnSvc,
					NULL, RPC_C_AUTHZ_NONE);
	if (status != RPC_S_OK)
		goto cleanup;

	RpcTryExcept {
		status = AFSRPC_SetToken(uuid, sessionKey);
	}
	RpcExcept(1) {
		status = RpcExceptionCode();
	}
	RpcEndExcept

cleanup:
	if (stringBinding)
		RpcStringFree(&stringBinding);

	if (hAfsHandle != NULL)
		RpcBindingFree(&hAfsHandle);

	return status;
}

RPC_STATUS receive_key(afs_uuid_t uuid, char sessionKey[8])
{
	RPC_STATUS status;
	char *stringBinding = NULL;
	ULONG authnLevel, authnSvc;
	char serverName[256];
	char *serverNamep = serverName;
	char encrypt[32];
	BOOL encryptionOff = FALSE;
	char protseq[32];

	/* Encryption on by default */
	if (GetEnvironmentVariable("AFS_RPC_ENCRYPT",
				   encrypt, sizeof(encrypt)))
		if (!strcmpi(encrypt, "OFF"))
			encryptionOff = TRUE;

	/* Protocol sequence is named pipe by default */
	if (!GetEnvironmentVariable("AFS_RPC_PROTSEQ",
				    protseq, sizeof(protseq)))
		strcpy(protseq, "ncacn_np");

	/* Server name */
	getservername(&serverNamep, sizeof(serverName));

	status = RpcStringBindingCompose("",	/* obj uuid */
					 protseq,
					 serverNamep,
					 "",	/* endpoint */
					 "",	/* protocol options */
					 &stringBinding);
	if (status != RPC_S_OK)
		goto cleanup;

	status = RpcBindingFromStringBinding(stringBinding, &hAfsHandle);
	if (status != RPC_S_OK)
		goto cleanup;

	/*
	 * On Windows 95/98, we must resolve the binding before calling
	 * SetAuthInfo.  On Windows NT, we don't have to resolve yet,
	 * but it does no harm.
	 */
	status = RpcEpResolveBinding(hAfsHandle, afsrpc_v1_0_c_ifspec);
	if (status != RPC_S_OK)
		goto cleanup;

	if (encryptionOff) {
		authnLevel = RPC_C_AUTHN_LEVEL_NONE;
		authnSvc = RPC_C_AUTHN_WINNT;
	} else {
		authnLevel = RPC_C_AUTHN_LEVEL_PKT_PRIVACY;
		authnSvc = RPC_C_AUTHN_WINNT;
	}

	status = RpcBindingSetAuthInfo(hAfsHandle, NULL, authnLevel, authnSvc,
					NULL, RPC_C_AUTHZ_NONE);
	if (status != RPC_S_OK)
		goto cleanup;

	RpcTryExcept {
		status = AFSRPC_GetToken(uuid, sessionKey);
	}
	RpcExcept(1) {
		status = RpcExceptionCode();
	}
	RpcEndExcept

cleanup:
	if (stringBinding)
		RpcStringFree(&stringBinding);

	if (hAfsHandle != NULL)
		RpcBindingFree(&hAfsHandle);

	return status;
}

int ktc_SetToken(
	struct ktc_principal *server,
	struct ktc_token *token,
	struct ktc_principal *client,
	int flags)
{
	struct ViceIoctl iob;
	char tbuffer[1024];
	char *tp;
	struct ClearToken ct;
	int temp;
	int code;
	RPC_STATUS status;
	afs_uuid_t uuid;

	if (token->ticketLen < MINKTCTICKETLEN
	    || token->ticketLen > MAXKTCTICKETLEN)
		return KTC_INVAL;

	if (strcmp(server->name, "afs")) {
	    return SetLocalToken(server, token, client, flags);
	}

	tp = tbuffer;

	/* ticket length */
	memcpy(tp, &token->ticketLen, sizeof(token->ticketLen));
	tp += sizeof(&token->ticketLen);

	/* ticket */
	memcpy(tp, token->ticket, token->ticketLen);
	tp += token->ticketLen;

	/* clear token */
	ct.AuthHandle = token->kvno;
	/*
	 * Instead of sending the session key in the clear, we zero it,
	 * and send it later, via RPC, encrypted.
	 */
	/*
	memcpy(ct.HandShakeKey, &token->sessionKey, sizeof(token->sessionKey));
	 */
	memset(ct.HandShakeKey, 0, sizeof(ct.HandShakeKey));
	ct.BeginTimestamp = token->startTime;
	ct.EndTimestamp = token->endTime;
	if (ct.BeginTimestamp == 0) ct.BeginTimestamp = 1;

	/* We don't know from Vice ID's yet */
	ct.ViceId = 37;			/* XXX */
	if (((ct.EndTimestamp - ct.BeginTimestamp) & 1) == 1)
		ct.BeginTimestamp++;		/* force lifetime to be even */

	/* size of clear token */
	temp = sizeof(struct ClearToken);
	memcpy(tp, &temp, sizeof(temp));
	tp += sizeof(temp);

	/* clear token itself */
	memcpy(tp, &ct, sizeof(ct));
	tp += sizeof(ct);

	/* flags; on NT there is no setpag flag, but there is an
	 * integrated logon flag */
	temp = ((flags & AFS_SETTOK_LOGON) ? PIOCTL_LOGON : 0);
	memcpy(tp, &temp, sizeof(temp));
	tp += sizeof(temp);

	/* cell name */
	temp = strlen(server->cell);
	if (temp >= MAXKTCREALMLEN)
		return KTC_INVAL;
	strcpy(tp, server->cell);
	tp += temp+1;

	/* user name */
	temp = strlen(client->name);
	if (temp >= MAXKTCNAMELEN)
		return KTC_INVAL;
	strcpy(tp, client->name);
	tp += temp+1;

	/* uuid */
	status = UuidCreate((UUID *)&uuid);
	memcpy(tp, &uuid, sizeof(uuid));
	tp += sizeof(uuid);

	/* RPC to send session key */
	status = send_key(uuid, token->sessionKey.data);
	if (status != RPC_S_OK) {
		if (status == 1)
			strcpy(rpcErr, "RPC failure in AFS gateway");
		else
			DceErrorInqText(status, rpcErr);
		if (status == RPC_S_SERVER_UNAVAILABLE
		    || status == EPT_S_NOT_REGISTERED)
			return KTC_NOCMRPC;
		else
			return KTC_RPC;
	}

	/* set up for pioctl */
	iob.in = tbuffer;
	iob.in_size = tp - tbuffer;
	iob.out = tbuffer;
	iob.out_size = sizeof(tbuffer);

	code = pioctl(0, VIOCSETTOK, &iob, 0);
	if (code) {
		if (code == -1) {
			if (errno == ESRCH)
				return KTC_NOCELL;
			else if (errno == ENODEV)
				return KTC_NOCM;
			else if (errno == EINVAL)
				return KTC_INVAL;
			else
				return KTC_PIOCTLFAIL;
		}
		else
			return KTC_PIOCTLFAIL;
	}

	return 0;
}

int ktc_GetToken(
	struct ktc_principal *server,
	struct ktc_token *token,
	int tokenLen,
	struct ktc_principal *client)
{
	struct ViceIoctl iob;
	char tbuffer[1024];
	char *tp, *cp;
	char *ticketP;
	int ticketLen, temp;
	struct ClearToken ct;
	char *cellName;
	int cellNameSize;
	int maxLen;
	int code;
	RPC_STATUS status;
	afs_uuid_t uuid;

	tp = tbuffer;

	/* check to see if the user is requesting tokens for a principal
	 * other than afs.  If so, check the local token cache.
	 */
	if (strcmp(server->name, "afs")) {
	    return GetLocalToken(server, token, tokenLen, client);
	}

	/* cell name */
	strcpy(tp, server->cell);
	tp += strlen(server->cell) + 1;

	/* uuid */
	status = UuidCreate((UUID *)&uuid);
	memcpy(tp, &uuid, sizeof(uuid));
	tp += sizeof(uuid);

	iob.in = tbuffer;
	iob.in_size = tp - tbuffer;
	iob.out = tbuffer;
	iob.out_size = sizeof(tbuffer);

	code = pioctl(0, VIOCNEWGETTOK, &iob, 0);
	if (code) {
		if (code == -1) {
			if (errno == ESRCH)
				return KTC_NOCELL;
			else if (errno == ENODEV)
				return KTC_NOCM;
			else if (errno == EINVAL)
				return KTC_INVAL;
			else if (errno == EDOM)
				return KTC_NOENT;
			else
				return KTC_PIOCTLFAIL;
		}
		else
			return KTC_PIOCTLFAIL;
	}

	/* RPC to receive session key */
	status = receive_key(uuid, token->sessionKey.data);
	if (status != RPC_S_OK) {
		if (status == 1)
			strcpy(rpcErr, "RPC failure in AFS gateway");
		else
			DceErrorInqText(status, rpcErr);
		if (status == RPC_S_SERVER_UNAVAILABLE
		    || status == EPT_S_NOT_REGISTERED)
			return KTC_NOCMRPC;
		else
			return KTC_RPC;
	}

	cp = tbuffer;

	/* ticket length */
	memcpy(&ticketLen, cp, sizeof(ticketLen));
	cp += sizeof(ticketLen);

	/* remember where ticket is and skip over it */
	ticketP = cp;
	cp += ticketLen;

	/* size of clear token */
	memcpy(&temp, cp, sizeof(temp));
	cp += sizeof(temp);
	if (temp != sizeof(ct))
		return KTC_ERROR;

	/* clear token */
	memcpy(&ct, cp, temp);
	cp += temp;

	/* skip over primary flag */
	cp += sizeof(temp);

	/* remember cell name and skip over it */
	cellName = cp;
	cellNameSize = strlen(cp);
	cp += cellNameSize + 1;

	/* user name is here */

	/* check that ticket will fit */
	maxLen = tokenLen - sizeof(struct ktc_token) + MAXKTCTICKETLEN;
	if (maxLen < ticketLen)
		return KTC_TOOBIG;

	/* set return values */
	memcpy(token->ticket, ticketP, ticketLen);
	token->startTime = ct.BeginTimestamp;
	token->endTime = ct.EndTimestamp;
	if (ct.AuthHandle == -1) ct.AuthHandle = 999;
	token->kvno = ct.AuthHandle;
	/*
	 * Session key has already been set via RPC
	 */
	/*
	memcpy(&token->sessionKey, ct.HandShakeKey, sizeof(ct.HandShakeKey));
	 */
	token->ticketLen = ticketLen;
	if (client) {
		strcpy(client->name, cp);
		client->instance[0] = '\0';
		strcpy(client->cell, cellName);
	}

	return 0;
}

int ktc_ListTokens(
	int cellNum,
	int *cellNumP,
	struct ktc_principal *server)
{
	struct ViceIoctl iob;
	char tbuffer[1024];
	char *tp, *cp;
	int newIter, ticketLen, temp;
	int code;

	tp = tbuffer;

	/* iterator */
	memcpy(tp, &cellNum, sizeof(cellNum));
	tp += sizeof(cellNum);

	/* do pioctl */
	iob.in = tbuffer;
	iob.in_size = tp - tbuffer;
	iob.out = tbuffer;
	iob.out_size = sizeof(tbuffer);

	code = pioctl(0, VIOCGETTOK, &iob, 0);
	if (code) {
		if (code == -1) {
			if (errno == ESRCH)
				return KTC_NOCELL;
			else if (errno == ENODEV)
				return KTC_NOCM;
			else if (errno == EINVAL)
				return KTC_INVAL;
			else if (errno == EDOM)
				return KTC_NOENT;
			else
				return KTC_PIOCTLFAIL;
		}
		else
			return KTC_PIOCTLFAIL;
	}

	cp = tbuffer;

	/* new iterator */
	memcpy(&newIter, cp, sizeof(newIter));
	cp += sizeof(newIter);

	/* ticket length */
	memcpy(&ticketLen, cp, sizeof(ticketLen));
	cp += sizeof(ticketLen);

	/* skip over ticket */
	cp += ticketLen;

	/* clear token size */
	memcpy(&temp, cp, sizeof(temp));
	cp += sizeof(temp);
	if (temp != sizeof(struct ClearToken))
		return KTC_ERROR;

	/* skip over clear token */
	cp += sizeof(struct ClearToken);

	/* skip over primary flag */
	cp += sizeof(temp);

	/* cell name is here */

	/* set return values */
	strcpy(server->cell, cp);
	server->instance[0] = '\0';
	strcpy(server->name, "afs");

	*cellNumP = newIter;
	return 0;
}

int ktc_ForgetToken(
	struct ktc_principal *server)
{
	struct ViceIoctl iob;
	char tbuffer[1024];
	char *tp;
	int code;

	if (strcmp(server->name, "afs")) {
	    return ForgetOneLocalToken(server);
	}

	tp = tbuffer;

	/* cell name */
	strcpy(tp, server->cell);
	tp += strlen(tp) + 1;

	/* do pioctl */
	iob.in = tbuffer;
	iob.in_size = tp - tbuffer;
	iob.out = tbuffer;
	iob.out_size = sizeof(tbuffer);

	code = pioctl(0, VIOCDELTOK, &iob, 0);
	if (code) {
		if (code == -1) {
			if (errno == ESRCH)
				return KTC_NOCELL;
			else if (errno == EDOM)
				return KTC_NOENT;
			else if (errno == ENODEV)
				return KTC_NOCM;
			else
				return KTC_PIOCTLFAIL;
		}
		else
			return KTC_PIOCTLFAIL;
	}
	return 0;
}

int ktc_ForgetAllTokens()
{
	struct ViceIoctl iob;
	char tbuffer[1024];
	int code;

	(void) ForgetLocalTokens();

	/* do pioctl */
	iob.in = tbuffer;
	iob.in_size = 0;
	iob.out = tbuffer;
	iob.out_size = sizeof(tbuffer);

	code = pioctl(0, VIOCDELALLTOK, &iob, 0);
	if (code) {
		if (code == -1) {
			if (errno == ENODEV)
				return KTC_NOCM;
			else
				return KTC_PIOCTLFAIL;
		}
		else
			return KTC_PIOCTLFAIL;
	}
	return 0;
}

int ktc_OldPioctl ()
{
    return 1;
}


#define MAXLOCALTOKENS 4

static struct {
    int valid;
    struct ktc_principal server;
    struct ktc_principal client;
    struct ktc_token token;
} local_tokens[MAXLOCALTOKENS] = {0};

static int SetLocalToken(struct ktc_principal *aserver,
			 struct ktc_token *atoken,
			 struct ktc_principal *aclient,
			 afs_int32 flags)
{
    int found = -1;
    int i;

    LOCK_GLOBAL_MUTEX
    for (i = 0; i < MAXLOCALTOKENS; i++)
	if (local_tokens[i].valid) {
	    if ((strcmp(local_tokens[i].server.name, aserver->name) == 0) &&
		(strcmp(local_tokens[i].server.instance,
			aserver->instance) == 0) &&
		(strcmp(local_tokens[i].server.cell, aserver->cell) == 0)) {
		found = i;	/* replace existing entry */
		break;
	    }
	} else if (found == -1)
	    found = i; /* remember empty slot but keep looking for a match */
    if (found == -1) {
	UNLOCK_GLOBAL_MUTEX
	return KTC_NOENT;
    }
    memcpy(&local_tokens[found].token, atoken, sizeof(struct ktc_token));
    memcpy(&local_tokens[found].server, aserver, sizeof(struct ktc_principal));
    memcpy(&local_tokens[found].client, aclient, sizeof(struct ktc_principal));
    local_tokens[found].valid = 1;
    UNLOCK_GLOBAL_MUTEX
    return 0;
}


static int GetLocalToken(struct ktc_principal *aserver,
			 struct ktc_token *atoken,
			 int atokenLen,
			 struct ktc_principal *aclient)
{
    int i;

    LOCK_GLOBAL_MUTEX
    for (i = 0; i < MAXLOCALTOKENS; i++)
	if (local_tokens[i].valid &&
	    (strcmp(local_tokens[i].server.name, aserver->name) == 0) &&
	    (strcmp(local_tokens[i].server.instance,aserver->instance) == 0) &&
	    (strcmp(local_tokens[i].server.cell, aserver->cell) == 0)) {
	    memcpy(atoken, &local_tokens[i].token,
		   min(atokenLen, sizeof(struct ktc_token)));
	    memcpy(aclient, &local_tokens[i].client,
		   sizeof(struct ktc_principal));
	    UNLOCK_GLOBAL_MUTEX
	    return 0;
	}
    UNLOCK_GLOBAL_MUTEX
    return KTC_NOENT;
}


static int ForgetLocalTokens()
{
    int i;

    LOCK_GLOBAL_MUTEX
    for (i = 0; i < MAXLOCALTOKENS; i++) {
	local_tokens[i].valid = 0;
	memset(&local_tokens[i].token.sessionKey, 0,
	       sizeof(struct ktc_encryptionKey));
    }
    UNLOCK_GLOBAL_MUTEX
    return 0;
}


static int ForgetOneLocalToken(struct ktc_principal *aserver)
{
    int i;

    LOCK_GLOBAL_MUTEX
    for (i = 0; i < MAXLOCALTOKENS; i++) {
	if (local_tokens[i].valid &&
	    (strcmp(local_tokens[i].server.name, aserver->name) == 0) &&
	    (strcmp(local_tokens[i].server.instance,aserver->instance) == 0) &&
	    (strcmp(local_tokens[i].server.cell, aserver->cell) == 0)) {
	    local_tokens[i].valid = 0;
	    memset(&local_tokens[i].token.sessionKey, 0,
		   sizeof(struct ktc_encryptionKey));
	    UNLOCK_GLOBAL_MUTEX
	    return 0;
	}
    }
    UNLOCK_GLOBAL_MUTEX
    return KTC_NOENT;
}
