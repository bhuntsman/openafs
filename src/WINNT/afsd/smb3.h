/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 *
 */
#ifndef __SMB3_H_ENV__
#define __SMB3_H_ENV__ 1

typedef struct smb_tran2Packet {
	osi_queue_t q;			/* queue of all packets */
        int totalData;			/* total # of expected data bytes */
        int totalParms;			/* total # of expected parm bytes */
	int oldTotalParms;		/* initial estimate of parm bytes */
        int curData;			/* current # of received data bytes */
        int curParms;			/* current # of received parm bytes */
        int maxReturnData;		/* max # of returned data bytes */
        int maxReturnParms;		/* max # of returned parm bytes */
        int opcode;			/* subopcode we're handling */
        long flags;			/* flags */
        smb_vc_t *vcp;			/* virtual circuit we're dealing with */
        unsigned short tid;		/* tid to match */
        unsigned short mid;		/* mid to match */
        unsigned short pid;		/* pid to remember */
        unsigned short uid;		/* uid to remember */
	unsigned short res[6];		/* contains PidHigh */
        unsigned short *parmsp;		/* parms */
        unsigned char *datap;		/* data bytes */
} smb_tran2Packet_t;

/* for flags field */
#define SMB_TRAN2PFLAG_ALLOC	1

typedef struct smb_tran2Dispatch {
	long (*procp)(smb_vc_t *, smb_tran2Packet_t *, smb_packet_t *);
        long flags;
} smb_tran2Dispatch_t;

typedef struct smb_tran2QFSInfo {
	union {
		struct {
			long FSID;			/* file system ID */
                        long sectorsPerAllocUnit;
                        long totalAllocUnits;		/* on the disk */
                        long availAllocUnits;		/* free blocks */
                        unsigned short bytesPerSector;	/* bytes per sector */
                } allocInfo;
                struct {
			long vsn;	/* volume serial number */
                        char vnCount;	/* count of chars in label, incl null */
                        char label[12];	/* pad out with nulls */
                } volumeInfo;
		struct {
			FILETIME vct;	/* volume creation time */
			long vsn;	/* volume serial number */
			long vnCount;	/* length of volume label in bytes */
			char res[2];	/* reserved */
			char label[10];	/* volume label */
		} FSvolumeInfo;
		struct {
			osi_hyper_t totalAllocUnits;	/* on the disk */
			osi_hyper_t availAllocUnits;	/* free blocks */
			long sectorsPerAllocUnit;
			long bytesPerSector;		/* bytes per sector */
		} FSsizeInfo;
		struct {
			long devType;	/* device type */
			long characteristics;
		} FSdeviceInfo;
		struct {
			long attributes;
			long maxCompLength;	/* max path component length */
			long FSnameLength;	/* length of file system name */
			char FSname[12];
		} FSattributeInfo;
        } u;
} smb_tran2QFSInfo_t;

/* more than enough opcodes for today, anyway */
#define SMB_TRAN2_NOPCODES		20

extern smb_tran2Dispatch_t smb_tran2DispatchTable[SMB_TRAN2_NOPCODES];

extern long smb_ReceiveV3SessionSetupX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3TreeConnectX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3Tran2A(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveTran2Open(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2FindFirst(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2SearchDir(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2FindNext(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2QFSInfo(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2SetFSInfo(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2QPathInfo(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2SetPathInfo(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2QFileInfo(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2SetFileInfo(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2FSCTL(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2IOCTL(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2FindNotifyFirst(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2FindNotifyNext(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveTran2MKDir(smb_vc_t *vcp, smb_tran2Packet_t *p,
	smb_packet_t *outp);

extern long smb_ReceiveV3FindClose(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3FindNotifyClose(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3UserLogoffX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3OpenX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3LockingX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3GetAttributes(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3ReadX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveV3SetAttributes(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveNTCreateX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern long smb_ReceiveNTTransact(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern void smb_NotifyChange(DWORD action, DWORD notifyFilter,
	cm_scache_t *dscp, char *filename, char *otherFilename,
	BOOL isDirectParent);

extern long smb_ReceiveNTCancel(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp);

extern int smb_V3MatchMask(char *namep, char *maskp, int flags);

extern void smb3_Init();

#endif /*  __SMB3_H_ENV__ */
