
/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

/* Server test program */

#include <afs/param.h>
#include <sys/types.h>
#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <io.h>
#include <winsock2.h>
#include <fcntl.h>
#include <stdlib.h>
#else
#include <netinet/in.h>
#include <netdb.h>
#include <sys/file.h>
#include <sys/time.h>
#endif
#include <signal.h>
#include <sys/stat.h>
#include "../rx_clock.h"
#include "../rx.h"
#include "../rx_globals.h"
#include "../rx_null.h"

int error;  /* Return this error number on a call */
int print = 0, eventlog = 0, rxlog = 0;
extern rx_intentionallyDroppedPacketsPer100;
extern char rxi_tracename[];
extern int rx_initSendWindow, rx_initReceiveWindow;
extern int rxi_nSendFrags, rxi_nRecvFrags;
extern int (*rxi_syscallp)();
struct clock computeTime, waitTime;
FILE *debugFile;
char *rcvFile;
int logstdout = 0;

void intSignal(int ignore) {
    Quit("Interrupted");
}

void quitSignal(int ignore) {
    static int quitCount = 0;
    if (++quitCount > 1) Quit("rx_ctest: second quit signal, aborting");
    rx_debugFile = debugFile = fopen("rx_stest.db", "w");
    if (debugFile) rx_PrintStats(debugFile);
}

int SimpleRequest(), FileRequest();

#if !defined(AFS_NT40_ENV) && !defined(AFS_LINUX20_ENV)
int test_syscall(a3, a4, a5)
afs_uint32 a3, a4;
void * a5;
{
  afs_uint32 rcode;
  void (*old)(int);
	
  old = signal(SIGSYS, SIG_IGN);	
  rcode = syscall (31 /* AFS_SYSCALL */, 28 /* AFSCALL_CALL */, a3, a4, a5);
  signal(SIGSYS, old);	

return rcode;
}
#endif

main(argc, argv)
char **argv;
{
    struct rx_service *service;
    struct rx_securityClass *(secobjs[1]);
    int err = 0;
    int setFD = 0;

#if !defined(AFS_NT40_ENV) && !defined(AFS_LINUX20_ENV)
    setlinebuf(stdout);
    rxi_syscallp = test_syscall;
#endif
    argv++; argc--;
    while (argc && **argv == '-') {
	if (strcmp(*argv, "-verbose") == 0) print = 1;
	else if (strcmp(*argv, "-rxlog") == 0) rxlog = 1;
	else if (strcmp(*argv, "-trace") == 0) strcpy(rxi_tracename, *(++argv)), argc--;
	else if (strcmp(*argv, "-logstdout") == 0) logstdout = 1;
	else if (strcmp(*argv, "-eventlog") == 0) eventlog = 1;
	else if (strcmp(*argv, "-npb") == 0) rx_nPackets = atoi(*++argv), argc--;
	else if (!strcmp(*argv, "-nsf")) 
  	     rxi_nSendFrags =  atoi(*++argv), argc--;
	else if (!strcmp(*argv, "-nrf")) 
  	     rxi_nRecvFrags =  atoi(*++argv), argc--;
	else if (strcmp(*argv, "-twind") == 0) 
	     rx_initSendWindow = atoi(*++argv), argc--;
	else if (strcmp(*argv, "-rwind") == 0) 
	     rx_initReceiveWindow = atoi(*++argv), argc--;
	else if (strcmp(*argv, "-file") == 0) rcvFile = *++argv, argc--;
	else if (strcmp(*argv, "-drop") == 0) rx_intentionallyDroppedPacketsPer100 = atoi(*++argv), argc--;
	else if (strcmp(*argv, "-err") == 0) error = atoi(*++argv), argc--;
	else if (strcmp(*argv, "-compute") == 0) {
	    /* Simulated "compute" time for each call--to test acknowledgement protocol.  This is simulated by doing an iomgr_select:  imperfect, admittedly. */
	    computeTime.sec = atoi(*++argv), argc--;
	    computeTime.usec = atoi(*++argv), argc--;
	}
	else if (strcmp(*argv, "-wait") == 0) {
	    /* Wait time between calls--to test lastack code */
	    waitTime.sec = atoi(*++argv), argc--;
	    waitTime.usec = atoi(*++argv), argc--;
	}
	else if (strcmp(*argv, "-fd") == 0) {
	    /* Open at least this many fd's. */
	    setFD = atoi(*++argv), argc--;
	}
	else {err++; break;}
	argv++, argc--;
    }
    if (err || argc != 0)
	Quit("usage: rx_stest [-silent] [-rxlog] [-eventlog]");

    if (rxlog || eventlog) {
	if (logstdout) debugFile = stdout;
	else debugFile = fopen("rx_stest.db", "w");
	if (debugFile == NULL) Quit("Couldn't open rx_stest.db");
	if (rxlog) rx_debugFile = debugFile;
	if (eventlog) rxevent_debugFile = debugFile;
    }

    signal(SIGINT, intSignal); /* Changed to SIGQUIT since dbx is broken right now */
#ifndef AFS_NT40_ENV
    signal(SIGQUIT, quitSignal);
#endif

    if (setFD>0)
	OpenFD(setFD);
    if (rx_Init(htons(2500)) != 0 ) {
	printf("RX initialization failed, exiting.\n");
	exit(1);
    }
    if (setFD>0) {
	printf("rx_socket=%d\n", rx_socket);
    }

    secobjs[0] = rxnull_NewServerSecurityObject();
    service = rx_NewService(/*port*/0, /*service*/3, "test", secobjs, /*nsec*/1, /*Execute request*/ rcvFile? FileRequest: SimpleRequest);
    if (!service) Abort("rx_NewService returned 0!\n");
    printf("Using %d packet buffers\n", rx_nPackets);
    rx_StartServer(1);
}

SimpleRequest(call)
    struct rx_call *call;
{
    int n;
    int nbytes = 1000;
    char buf[1000];
    while ((n = rx_Read(call, buf, nbytes)) > 0)
	if (print) printf("stest: Received %d bytes\n", n);
    if (!rx_Error(call)) {
	/* Fake compute time (use select to lock out everything) */
	struct timeval t;
	if (!clock_IsZero(&computeTime)) {
	    t.tv_sec = computeTime.sec;
	    t.tv_usec = computeTime.usec;
#ifdef AFS_NT40_ENV
	    Sleep(t.tv_sec);
#else
	    if (select(0, 0, 0, 0, &t) != 0) Quit("Select didn't return 0");
#endif
	}
	/* Then wait time (use iomgr_select to allow rx to run) */
	if (!clock_IsZero(&waitTime)) {
	    t.tv_sec = waitTime.sec;
	    t.tv_usec = waitTime.usec;
#ifdef AFS_PTHREAD_ENV
	    select(0, 0, 0, 0, &t);
#else
	    IOMGR_Sleep(t.tv_sec);
#endif
	}
	rx_Write(call, "So long, and thanks for all the fish!\n", strlen("So long, and thanks for all the fish!\n"));
    }
    if (debugFile) rx_PrintPeerStats(debugFile, rx_PeerOf(call->conn));
    rx_PrintPeerStats(stdout, rx_PeerOf(call->conn));
    return 0;
}

FileRequest(call)
struct rx_call *call;
{
    int fd;
    struct stat status;
    int nbytes, blockSize;
    char *buffer;
#ifdef	AFS_AIX_ENV
#include <sys/statfs.h>
    struct statfs tstatfs;
#endif
    /* Open the file ahead of time:  the client timing the operation doesn't have to include the open time */
    fd = open(rcvFile, O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fd < 0) {perror("open"); return;}
    fstat(fd, &status);
#ifdef AFS_NT40_ENV
    blockSize = 1024;
#else
#ifdef	AFS_AIX_ENV
    fstatfs(fd, &tstatfs);
    blockSize = tstatfs.f_bsize;
#else
    blockSize = status.st_blksize;
#endif
#endif /* AFS_NT40_ENV */
    buffer = (char *) malloc(blockSize);

    rx_SetLocalStatus(call, 79);   /* Emulation of file server's old "RCallBackReceivedStore" */
    
    while (nbytes = rx_Read(call, buffer, blockSize)) {
	if (write(fd, buffer, nbytes) != nbytes) {
	    perror("writev");
	    Abort("Write Failed.\n");
	    break;
	}
    }
    rx_Write(call, "So long, and thanks for all the fish!\n", strlen("So long, and thanks for all the fish!\n"));
    printf("Received file %s\n", rcvFile);
    close(fd);
    if (debugFile) rx_PrintPeerStats(debugFile, rx_PeerOf(call->conn));
    rx_PrintPeerStats(stdout, rx_PeerOf(call->conn));
    return 0;
}

Abort(msg, a,b,c,d,e) {
    printf((char *) msg, a, b, c, d, e);
    printf("\n");
    if (debugFile) {
	rx_PrintStats(debugFile);
	fflush(debugFile);
    }
    abort();
}

Quit(msg, a,b,c,d,e) {
    printf((char *) msg, a,b,c,d,e);
    printf("\n");
    if (debugFile) {
	rx_PrintStats(debugFile);
	fflush(debugFile);
    }
    rx_PrintStats(stdout);
    exit(0);
}


/* OpenFD
 *
 * Open file descriptors until file descriptor n or higher is returned.
 */
#include <sys/stat.h>
OpenFD(n)
int n;
{
    int i;
    struct stat sbuf;
    int fd, lfd;

    lfd = -1;
    for (i=0; i<n; i++) {
	if (fstat(i, &sbuf) == 0)
	    continue;
	if ((fd = open("/dev/null", 0, 0)) < 0) {
	    if (lfd >=0) {
		close(lfd);
		return;
	    }
	}
	else {
	    if (fd >= n) {
		close(fd);
		return;
	    }
	    else {
		lfd = fd;
	    }
	}
    }
}
