/* include file for afsmonitor */

/*
 * (C) Copyright Transarc Corporation 1989
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 */



#define HOST_NAME_LEN	80	/* length of server/cm names */

#define XSTAT_FS_FULLPERF_RESULTS_LEN 424   	/* value of xstat_fs_Results.data.AFS_COllData_len. We use this value before making any xstat calls and hence need this definition. Remember to change this value appropriately if the size of any of the fs related data structures change. Need to find a better way of doing this */

#define XSTAT_CM_FULLPERF_RESULTS_LEN 740   	/* value of xstat_cm_Results.data.AFS_COllData_len. We use this value before making any xstat calls and hence need this definition. Remember to change this value appropriately if the size of any of the cm related data structures change. Need to find a better way of doing this */

/* THRESHOLD STRUCTURE DEFINITIONS */

#define THRESH_VAR_NAME_LEN 80
#define THRESH_VAR_LEN 16

/* structure of each threshold item */
struct Threshold {
	char itemName[THRESH_VAR_NAME_LEN];		/* field name */
	int  index;			/* positional index */
	char threshVal[THRESH_VAR_LEN];	/* user provided threshold value */
	char handler[256]; 		/* user provided ovf handler */
};


/* structures to store info of hosts to be monitored */
static struct afsmon_hostEntry {
	char hostName[HOST_NAME_LEN];	/* fs or cm host name */
	int numThresh;			/* number of thresholds for this host */
	struct Threshold *thresh;	/* ptr to threshold entries */
	struct afsmon_hostEntry *next;
};


#define NUM_FS_STAT_ENTRIES 271		/* number of file server statistics
					entries to display */
#define FS_STAT_STRING_LEN 14		/* max length of each string above */
#define NUM_CM_STAT_ENTRIES 571		/* number of cache manager statistics
					entries to display */
#define CM_STAT_STRING_LEN 14		/* max length of each string above */

/* structures to store statistics in a format convenient to dump to the 
screen */
/* for file servers */
struct fs_Display_Data {
	char hostName[HOST_NAME_LEN];
	int probeOK;	/* 0 => probe failed */
	char data[ NUM_FS_STAT_ENTRIES ][ FS_STAT_STRING_LEN ];
	short threshOvf[ NUM_FS_STAT_ENTRIES ]; /* overflow flags */
	int ovfCount;				/* overflow count */
};

/* for cache managers */
struct cm_Display_Data {
	char hostName[HOST_NAME_LEN];
	int probeOK;	/* 0 => probe failed */
	char data[ NUM_CM_STAT_ENTRIES ][ CM_STAT_STRING_LEN ];
	short threshOvf[ NUM_CM_STAT_ENTRIES ]; /* overflow flags */
	int ovfCount;			/* overflow count */
};

/* GTX related definitions */

/* when an FS or CM probe cycle completes the ovw_refresh() routine is called
to update the overview screen. The FS & CM probes are independent of each
other. Hence we should update only the one that completed. */

#define OVW_UPDATE_FS	1
#define OVW_UPDATE_CM	2
#define OVW_UPDATE_BOTH	3

/* Data is categorized into sections and groups to enable to user to choose
what he wants displayed. */
#define FS_NUM_DATA_CATEGORIES 8	/* # of fs categories */
#define CM_NUM_DATA_CATEGORIES 14	/* # of cm categories */

/* Set this  enable detailed debugging with the -debug switch */
#define DETAILED_DEBUG 0
