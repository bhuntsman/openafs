/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 *
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <osi.h>

#include "afsd.h"

#include "smb.h"

smb_ioctlProc_t *smb_ioctlProcsp[SMB_IOCTL_MAXPROCS];

void smb_InitIoctl(void)
{
	smb_ioctlProcsp[VIOCGETAL] = cm_IoctlGetACL;
        smb_ioctlProcsp[VIOC_FILE_CELL_NAME] = cm_IoctlGetFileCellName;
        smb_ioctlProcsp[VIOCSETAL] = cm_IoctlSetACL;
        smb_ioctlProcsp[VIOC_FLUSHVOLUME] = cm_IoctlFlushVolume;
        smb_ioctlProcsp[VIOCFLUSH] = cm_IoctlFlushFile;
        smb_ioctlProcsp[VIOCSETVOLSTAT] = cm_IoctlSetVolumeStatus;
        smb_ioctlProcsp[VIOCGETVOLSTAT] = cm_IoctlGetVolumeStatus;
        smb_ioctlProcsp[VIOCWHEREIS] = cm_IoctlWhereIs;
        smb_ioctlProcsp[VIOC_AFS_STAT_MT_PT] = cm_IoctlStatMountPoint;
        smb_ioctlProcsp[VIOC_AFS_DELETE_MT_PT] = cm_IoctlDeleteMountPoint;
        smb_ioctlProcsp[VIOCCKSERV] = cm_IoctlCheckServers;
        smb_ioctlProcsp[VIOC_GAG] = cm_IoctlGag;
        smb_ioctlProcsp[VIOCCKBACK] = cm_IoctlCheckVolumes;
        smb_ioctlProcsp[VIOCSETCACHESIZE] = cm_IoctlSetCacheSize;
        smb_ioctlProcsp[VIOCGETCACHEPARMS] = cm_IoctlGetCacheParms;
        smb_ioctlProcsp[VIOCGETCELL] = cm_IoctlGetCell;
        smb_ioctlProcsp[VIOCNEWCELL] = cm_IoctlNewCell;
        smb_ioctlProcsp[VIOC_GET_WS_CELL] = cm_IoctlGetWsCell;
        smb_ioctlProcsp[VIOC_AFS_SYSNAME] = cm_IoctlSysName;
        smb_ioctlProcsp[VIOC_GETCELLSTATUS] = cm_IoctlGetCellStatus;
        smb_ioctlProcsp[VIOC_SETCELLSTATUS] = cm_IoctlSetCellStatus;
        smb_ioctlProcsp[VIOC_SETSPREFS] = cm_IoctlSetSPrefs;
        smb_ioctlProcsp[VIOC_GETSPREFS] = cm_IoctlGetSPrefs;
        smb_ioctlProcsp[VIOC_STOREBEHIND] = cm_IoctlStoreBehind;
        smb_ioctlProcsp[VIOC_AFS_CREATE_MT_PT] = cm_IoctlCreateMountPoint;
        smb_ioctlProcsp[VIOC_TRACECTL] = cm_IoctlTraceControl;
	smb_ioctlProcsp[VIOCSETTOK] = cm_IoctlSetToken;
	smb_ioctlProcsp[VIOCGETTOK] = cm_IoctlGetTokenIter;
	smb_ioctlProcsp[VIOCNEWGETTOK] = cm_IoctlGetToken;
	smb_ioctlProcsp[VIOCDELTOK] = cm_IoctlDelToken;
	smb_ioctlProcsp[VIOCDELALLTOK] = cm_IoctlDelAllToken;
	smb_ioctlProcsp[VIOC_SYMLINK] = cm_IoctlSymlink;
	smb_ioctlProcsp[VIOC_LISTSYMLINK] = cm_IoctlListlink;
	smb_ioctlProcsp[VIOC_DELSYMLINK] = cm_IoctlDeletelink;
	smb_ioctlProcsp[VIOC_MAKESUBMOUNT] = cm_IoctlMakeSubmount;
}

/* called to make a fid structure into an IOCTL fid structure */
void smb_SetupIoctlFid(smb_fid_t *fidp, cm_space_t *prefix)
{
	smb_ioctl_t *iop;
	cm_space_t *copyPrefix;

	lock_ObtainMutex(&fidp->mx);
	fidp->flags |= SMB_FID_IOCTL;
	fidp->scp = &cm_fakeSCache;
        if (fidp->ioctlp == NULL) {
		iop = malloc(sizeof(*iop));
                memset(iop, 0, sizeof(*iop));
                fidp->ioctlp = iop;
        }
	if (prefix) {
		copyPrefix = cm_GetSpace();
		strcpy(copyPrefix->data, prefix->data);
		fidp->ioctlp->prefix = copyPrefix;
	}
	lock_ReleaseMutex(&fidp->mx);
}

/* called when we receive a read call, does the send of the received data if
 * this is the first read call.  This is the function that actually makes the
 * call to the ioctl code.
 */
smb_IoctlPrepareRead(smb_fid_t *fidp, smb_ioctl_t *ioctlp, cm_user_t *userp)
{
	long opcode;
        smb_ioctlProc_t *procp;
        long code;

	if (ioctlp->flags & SMB_IOCTLFLAG_DATAIN) {
		ioctlp->flags &= ~SMB_IOCTLFLAG_DATAIN;
                
                /* do the call now, or fail if we didn't get an opcode, or
                 * enough of an opcode.
                 */
                if (ioctlp->inCopied < sizeof(long)) return CM_ERROR_INVAL;
                memcpy(&opcode, ioctlp->inDatap, sizeof(long));
                ioctlp->inDatap += sizeof(long);

                osi_Log1(afsd_logp, "Ioctl opcode %d", opcode);

		/* check for opcode out of bounds */
                if (opcode < 0 || opcode >= SMB_IOCTL_MAXPROCS)
                	return CM_ERROR_TOOBIG;
		
                /* check for no such proc */
                procp = smb_ioctlProcsp[opcode];
                if (procp == NULL) return CM_ERROR_BADOP;

		/* otherwise, make the call */
		ioctlp->outDatap += sizeof(long);	/* reserve room for return code */
                code = (*procp)(ioctlp, userp);

		osi_Log1(afsd_logp, "Ioctl return code %d", code);

		/* copy in return code */
                memcpy(ioctlp->outAllocp, &code, sizeof(long));
        }
        return 0;
}

/* called when we receive a write call.  If this is the first write call after
 * a series of reads (or the very first call), then we start a new call.
 * We also ensure that things are properly initialized for the start of a call.
 */
void smb_IoctlPrepareWrite(smb_fid_t *fidp, smb_ioctl_t *ioctlp)
{
	/* make sure the buffer(s) are allocated */
	if (!ioctlp->inAllocp) ioctlp->inAllocp = malloc(SMB_IOCTL_MAXDATA);
        if (!ioctlp->outAllocp) ioctlp->outAllocp = malloc(SMB_IOCTL_MAXDATA);

	/* and make sure that we've reset our state for the new incoming request */
	if (!(ioctlp->flags & SMB_IOCTLFLAG_DATAIN)) {
	        ioctlp->inCopied = 0;
	        ioctlp->outCopied = 0;
		ioctlp->inDatap = ioctlp->inAllocp;
		ioctlp->outDatap = ioctlp->outAllocp;
	        ioctlp->flags |= SMB_IOCTLFLAG_DATAIN;
	}
}

/* called from smb_ReceiveCoreRead when we receive a read on the ioctl fid */
long smb_IoctlRead(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp,
	smb_packet_t *outp)
{
	smb_ioctl_t *iop;
        long count;
        long leftToCopy;
        char *op;
        long code;
        cm_user_t *userp;

        iop = fidp->ioctlp;
        count = smb_GetSMBParm(inp, 1);
        userp = smb_GetUser(vcp, inp);

	/* Identify tree */
	iop->tidPathp = smb_GetTIDPath(vcp, ((smb_t *)inp)->tid);

	/* turn the connection around, if required */
	code = smb_IoctlPrepareRead(fidp, iop, userp);

	if (code) {
		cm_ReleaseUser(userp);
		return code;
        }

	if (iop->flags & SMB_IOCTLFLAG_LOGON) {
		vcp->logonDLLUser = userp;
		userp->flags |= CM_USERFLAG_WASLOGON;
	}

	leftToCopy = (iop->outDatap - iop->outAllocp) - iop->outCopied;
        if (count > leftToCopy) count = leftToCopy;
        
        /* now set the parms for a read of count bytes */
        smb_SetSMBParm(outp, 0, count);
        smb_SetSMBParm(outp, 1, 0);
        smb_SetSMBParm(outp, 2, 0);
        smb_SetSMBParm(outp, 3, 0);
        smb_SetSMBParm(outp, 4, 0);

	smb_SetSMBDataLength(outp, count+3);

        op = smb_GetSMBData(outp, NULL);
        *op++ = 1;
        *op++ = count & 0xff;
        *op++ = (count >> 8) & 0xff;
        
	/* now copy the data into the response packet */
        memcpy(op, iop->outCopied + iop->outAllocp, count);

        /* and adjust the counters */
        iop->outCopied += count;
        
        cm_ReleaseUser(userp);
        smb_ReleaseFID(fidp);

        return 0;
}

/* called from smb_ReceiveCoreWrite when we receive a write call on the IOCTL
 * file descriptor.
 */
long smb_IoctlWrite(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
	smb_ioctl_t *iop;
        long count;
        long code;
        char *op;
        long inDataBlockCount;

	code = 0;
	count = smb_GetSMBParm(inp, 1);
        iop = fidp->ioctlp;
        
	smb_IoctlPrepareWrite(fidp, iop);

        op = smb_GetSMBData(inp, NULL);
	op = smb_ParseDataBlock(op, NULL, &inDataBlockCount);
	
        if (count + iop->inCopied > SMB_IOCTL_MAXDATA) {
		code = CM_ERROR_TOOBIG;
                goto done;
        }
        
	/* copy data */
        memcpy(iop->inDatap + iop->inCopied, op, count);
        
        /* adjust counts */
        iop->inCopied += count;

done:
	/* return # of bytes written */
	if (code == 0) {
		smb_SetSMBParm(outp, 0, count);
                smb_SetSMBDataLength(outp, 0);
        }

        smb_ReleaseFID(fidp);
        return code;
}

/* called from V3 read to handle IOCTL descriptor reads */
long smb_IoctlV3Read(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
	smb_ioctl_t *iop;
        long count;
        long code;
        long leftToCopy;
        char *op;
        cm_user_t *userp;

        iop = fidp->ioctlp;
        count = smb_GetSMBParm(inp, 5);
	
	userp = smb_GetUser(vcp, inp);

	{
		smb_user_t *uidp;

		uidp = smb_FindUID(vcp, ((smb_t *)inp)->uid, 0);
		osi_Log3(afsd_logp, "Ioctl uid %d user %x name %s",
			 uidp->userID, userp,
			 osi_LogSaveString(afsd_logp, uidp->name));
		smb_ReleaseUID(uidp);
	}

	iop->tidPathp = smb_GetTIDPath(vcp, ((smb_t *)inp)->tid);

	code = smb_IoctlPrepareRead(fidp, iop, userp);
        if (code) {
		cm_ReleaseUser(userp);
                smb_ReleaseFID(fidp);
		return code;
        }

	if (iop->flags & SMB_IOCTLFLAG_LOGON) {
		vcp->logonDLLUser = userp;
		userp->flags |= CM_USERFLAG_WASLOGON;
	}

	leftToCopy = (iop->outDatap - iop->outAllocp) - iop->outCopied;
        if (count > leftToCopy) count = leftToCopy;
        
	/* 0 and 1 are reserved for request chaining, were setup by our caller,
         * and will be further filled in after we return.
         */
        smb_SetSMBParm(outp, 2, 0);	/* remaining bytes, for pipes */
        smb_SetSMBParm(outp, 3, 0);	/* resvd */
        smb_SetSMBParm(outp, 4, 0);	/* resvd */
	smb_SetSMBParm(outp, 5, count);	/* # of bytes we're going to read */
        /* fill in #6 when we have all the parameters' space reserved */
        smb_SetSMBParm(outp, 7, 0);	/* resv'd */
        smb_SetSMBParm(outp, 8, 0);	/* resv'd */
        smb_SetSMBParm(outp, 9, 0);	/* resv'd */
        smb_SetSMBParm(outp, 10, 0);	/* resv'd */
	smb_SetSMBParm(outp, 11, 0);	/* reserved */

	/* get op ptr after putting in the last parm, since otherwise we don't
         * know where the data really is.
         */
        op = smb_GetSMBData(outp, NULL);
        
        /* now fill in offset from start of SMB header to first data byte (to op) */
        smb_SetSMBParm(outp, 6, ((int) (op - outp->data)));

	/* set the packet data length the count of the # of bytes */
        smb_SetSMBDataLength(outp, count);
        
	/* now copy the data into the response packet */
        memcpy(op, iop->outCopied + iop->outAllocp, count);

        /* and adjust the counters */
        iop->outCopied += count;
        
        /* and cleanup things */
        cm_ReleaseUser(userp);
        smb_ReleaseFID(fidp);

        return 0;
}

/* called from Read Raw to handle IOCTL descriptor reads */
long smb_IoctlReadRaw(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp,
	smb_packet_t *outp)
{
	smb_ioctl_t *iop;
	long leftToCopy;
	NCB *ncbp;
	long code;
	cm_user_t *userp;

	iop = fidp->ioctlp;

	userp = smb_GetUser(vcp, inp);

	{
		smb_user_t *uidp;

		uidp = smb_FindUID(vcp, ((smb_t *)inp)->uid, 0);
		osi_Log3(afsd_logp, "Ioctl uid %d user %x name %s",
			 uidp->userID, userp,
			 osi_LogSaveString(afsd_logp, uidp->name));
		smb_ReleaseUID(uidp);
	}

	iop->tidPathp = smb_GetTIDPath(vcp, ((smb_t *)inp)->tid);

	code = smb_IoctlPrepareRead(fidp, iop, userp);
	if (code) {
		cm_ReleaseUser(userp);
		smb_ReleaseFID(fidp);
		return code;
	}

	if (iop->flags & SMB_IOCTLFLAG_LOGON) {
		vcp->logonDLLUser = userp;
		userp->flags |= CM_USERFLAG_WASLOGON;
	}

	leftToCopy = (iop->outDatap - iop->outAllocp) - iop->outCopied;

	ncbp = outp->ncbp;
	memset((char *)ncbp, 0, sizeof(NCB));

	ncbp->ncb_length = (unsigned short) leftToCopy;
	ncbp->ncb_lsn = (unsigned char) vcp->lsn;
	ncbp->ncb_command = NCBSEND;
	ncbp->ncb_buffer = iop->outCopied + iop->outAllocp;

	code = Netbios(ncbp);
	if (code != 0)
		osi_Log1(afsd_logp, "ReadRaw send failure code %d", code);

	cm_ReleaseUser(userp);
	smb_ReleaseFID(fidp);

	return 0;
}
