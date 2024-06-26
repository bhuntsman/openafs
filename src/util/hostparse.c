/*
 * (C) COPYRIGHT TRANSARC CORPORATION 1989
 * LICENSED MATERIALS - PROPERTY OF TRANSARC
 * ALL RIGHTS RESERVED
 */

#include <afs/param.h>
#ifdef UKERNEL
#include "../afs/sysincludes.h"
#include "../afs/afsutil.h"
#else /* UKERNEL */
#include <stdio.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <stdlib.h>
#include <direct.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include <errno.h>
#include "afsutil.h"
#endif /* UKERNEL */


/* also parse a.b.c.d addresses */
struct hostent *hostutil_GetHostByName(ahost)
register char *ahost; {
    register int tc;
    static struct hostent thostent;
    static char *addrp[2];
    static char addr[4];
    register char *ptr = ahost;
    afs_int32 tval, numeric=0;
    int dots=0;

    tc = *ahost;    /* look at the first char */
    if (tc >= '0' && tc <= '9') {
	numeric = 1;
	while (tc = *ptr++) {
	    if (tc == '.') {
		if (dots >= 3) {
		    numeric = 0;
		    break;
		}
		dots++;
	    } else if (tc > '9' || tc < '0') {
		numeric = 0;
		break;
	    } 
	}
    }
    if (numeric) {
	tc = *ahost;    /* look at the first char */
	/* decimal address, return fake hostent with only hostaddr field good */
	tval = 0;
	dots = 0;
	bzero(addr, sizeof(addr));
	while (tc = *ahost++) {
	    if (tc == '.') {
		if (dots >= 3) return (struct hostent *) 0; /* too many dots */
		addr[dots++] = tval;
		tval = 0;
	    }
	    else if (tc > '9' || tc < '0') return (struct hostent *) 0;
	    else {
		tval *= 10;
		tval += tc - '0';
	    }
	}
	addr[dots] = tval;
#ifdef h_addr
	/* 4.3 system */
	addrp[0] = addr;
	addrp[1] = (char *) 0;
	thostent.h_addr_list = &addrp[0];
#else /* h_addr */
	/* 4.2 and older systems */
	thostent.h_addr = addr;
#endif /* h_addr */
	return &thostent;
    }
    else {
#ifdef AFS_NT40_ENV
	if (afs_winsockInit()<0) 
	    return (struct hostent *)0;
#endif
	return gethostbyname(ahost);
    }
}

/* Translate an internet address into a nice printable string. The
 * variable addr is in network byte order.
 */
char *hostutil_GetNameByINet(addr)
  afs_int32 addr;
{
  struct hostent *th;
  static char    tbuffer[256];

#ifdef AFS_NT40_ENV
	if (afs_winsockInit()<0) 
	    return (char *)0;
#endif
  th = gethostbyaddr((void *)&addr, sizeof(addr), AF_INET);
  if (th) {
     strcpy(tbuffer, th->h_name);
  } else {
     addr = ntohl(addr);
     sprintf(tbuffer, "%d.%d.%d.%d", 
	     ((addr>>24) & 0xff), ((addr>>16) & 0xff),
	     ((addr>>8)  & 0xff), ( addr      & 0xff));
  }
  
    return tbuffer;
}

/* the parameter is a pointer to a buffer containing a string of 
** bytes of the form 
** w.x.y.z 	# machineName
** returns the network interface in network byte order 
*/
afs_int32
extractAddr(line, maxSize)
char* line;
int maxSize;
{
	char byte1[32], byte2[32], byte3[32], byte4[32];
	int i=0;
	char*	endPtr;
	afs_int32 val1, val2, val3, val4;
	afs_int32 val=0;

	/* skip empty spaces */
	while ( isspace(*line) && maxSize ) 
	{
		line++;
		maxSize--;
	}
	
	/* skip empty lines */
	if ( !maxSize || !*line ) return  AFS_IPINVALIDIGNORE;
	
	while ( ( *line != '.' ) && maxSize) /* extract first byte */
	{
		if ( !isdigit(*line) ) return AFS_IPINVALID;
		if ( i > 31 ) return AFS_IPINVALID; /* no space */
		byte1[i++] = *line++;
		maxSize--;
	}
	if ( !maxSize ) return AFS_IPINVALID;
	byte1[i] = 0;
	
	i=0, line++;
	while ( ( *line != '.' ) && maxSize) /* extract second byte */
	{
		if ( !isdigit(*line) ) return AFS_IPINVALID;
		if ( i > 31 ) return AFS_IPINVALID; /* no space */
		byte2[i++] = *line++;
		maxSize--;
	}
	if ( !maxSize ) return AFS_IPINVALID;
	byte2[i] = 0;

	i=0, line++;
	while ( ( *line != '.' ) && maxSize)
	{
		if ( !isdigit(*line) ) return AFS_IPINVALID;
		if ( i > 31 ) return AFS_IPINVALID; /* no space */
		byte3[i++] = *line++;
		maxSize--;
	}
	if ( !maxSize ) return AFS_IPINVALID;
	byte3[i] = 0;

	i=0, line++;
	while ( *line && !isspace(*line) && maxSize)
	{
		if ( !isdigit(*line) ) return AFS_IPINVALID;
		if ( i > 31 ) return AFS_IPINVALID; /* no space */
		byte4[i++] = *line++;
		maxSize--;
	}
	if ( !maxSize ) return AFS_IPINVALID;
	byte4[i] = 0;
	
	errno=0;
	val1 = strtol(byte1, &endPtr, 10);
	if (( val1== 0 ) && ( errno != 0 || byte1 == endPtr)) 
		return AFS_IPINVALID;
	
	errno=0;
	val2 = strtol(byte2, &endPtr, 10);
	if (( val2== 0 ) && ( errno !=0 || byte2 == endPtr))/* no conversion */
		return AFS_IPINVALID;

	errno=0;
	val3 = strtol(byte3, &endPtr, 10);
	if (( val3== 0 ) && ( errno !=0 || byte3 == endPtr))/* no conversion */
		return AFS_IPINVALID;

	errno=0;
	val4 = strtol(byte4, &endPtr, 10);
	if (( val4== 0 ) && ( errno !=0 || byte4 == endPtr))/* no conversion */
		return AFS_IPINVALID;

	val = ( val1 << 24 ) | ( val2 << 16 ) | ( val3 << 8 ) | val4;
	val = htonl(val);
	return val;
}

/* 
** converts a 4byte IP address into a static string (e.g. w.x.y.z) 
** On Solaris, if we pass a 4 byte integer directly into inet_ntoa(), it
** causes a memory fault. 
*/
char* afs_inet_ntoa(afs_int32 addr)
{
    struct in_addr temp;
    temp.s_addr = addr;
    return (char *) inet_ntoa(temp);
}

/*
 * gettmpdir() -- Returns pointer to global temporary directory string.
 *     Always succeeds.  Never attempt to deallocate directory string.
 */

char *gettmpdir(void)
{
    char *tmpdirp = NULL;
    
#ifdef AFS_NT40_ENV
    static char *saveTmpDir = NULL;

    if (saveTmpDir == NULL) {
	/* initialize global temporary directory string */
	char *dirp = (char *)malloc(MAX_PATH);
	int freeDirp = 1;

	if (dirp != NULL) {
	    DWORD pathLen = GetTempPath(MAX_PATH, dirp);

	    if (pathLen == 0 || pathLen > MAX_PATH) {
		/* can't get tmp path; get cur work dir */
		pathLen = GetCurrentDirectory(MAX_PATH, dirp);
		if (pathLen == 0 || pathLen > MAX_PATH) {
		    free(dirp);
		    dirp = NULL;
		}
	    }

	    if (dirp != NULL) {
		/* Have a valid dir path; check that actually exists. */
		DWORD fileAttr = GetFileAttributes(dirp);

		if ((fileAttr == 0xFFFFFFFF) ||
		    ((fileAttr & FILE_ATTRIBUTE_DIRECTORY) == 0)) {
		    free(dirp);
		    dirp = NULL;
		}
	    }
	} /* dirp != NULL */
	
	if (dirp != NULL) {
	    FilepathNormalize(dirp);
	} else {
	    /* most likely TMP or TEMP env vars specify a non-existent dir */
	    dirp = "/";
	    freeDirp = 0;
	}

	/* atomically initialize shared buffer pointer IF still null */

#if 0
	if (InterlockedCompareExchange(&saveTmpDir, dirp, NULL) != NULL) {
	    /* shared buffer pointer already initialized by another thread */
	    if (freeDirp) {
		free(dirp);
	    }
	} /* interlock xchng */
#endif

	/* Above is what we really want to do, but Windows 95 does not have
	 * InterlockedCompareExchange().  So we just atomically swap
	 * the buffer pointer values but we do NOT deallocate the
	 * previously installed buffer, if any, in case it is in use.
	 */
	InterlockedExchange((LPLONG)&saveTmpDir, (LONG)dirp);

    } /* if (!saveTmpDir) */
    
    tmpdirp = saveTmpDir;
#else
    tmpdirp = "/tmp";
#endif /* AFS_NT40_ENV */
    
    return tmpdirp;
}


