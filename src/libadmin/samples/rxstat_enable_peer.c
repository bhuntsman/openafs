/* Copyright (C) 1999 IBM Corporation - All rights reserved.
 */

/*
 * This file contains sample code for the rxstats interface 
 */

#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <pthread.h>
#endif
#include <afs/afs_Admin.h>
#include <afs/afs_clientAdmin.h>
#include <afs/afs_utilAdmin.h>

extern int RXSTATS_EnablePeerRPCStats();

void Usage()
{
    fprintf(stderr,
	    "Usage: rxstat_enable_peer <cell> <host> <port>\n");
    exit(1);
}

void ParseArgs(
    int argc,
    char *argv[],
    char **cellName,
    char **srvrName,
    long *srvrPort)
{
    char **argp = argv;

    if (!*(++argp))
	Usage();
    *cellName = *(argp++);
    if (!*(argp))
	Usage();
    *srvrName = *(argp++);
    if (!*(argp))
	Usage();
    *srvrPort = strtol(*(argp++), NULL, 0);
    if (*srvrPort <= 0 || *srvrPort >= 65536)
	Usage();
    if (*(argp))
	Usage();
}

int main(int argc, char *argv[])
{
    int rc;
    afs_status_t st = 0;
    struct rx_connection *conn;
    char *srvrName;
    long srvrPort;
    char *cellName;
    void *tokenHandle;
    void *cellHandle;

    ParseArgs(argc, argv, &cellName, &srvrName, &srvrPort);

    rc = afsclient_Init(&st);
    if (!rc) {
	fprintf(stderr, "afsclient_Init, status %d\n", st);
	exit(1);
    }

    rc = afsclient_TokenGetExisting(cellName, &tokenHandle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_TokenGetExisting, status %d\n", st);
	exit(1);
    }

    rc = afsclient_CellOpen(cellName, tokenHandle, &cellHandle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_CellOpen, status %d\n", st);
	exit(1);
    }

    rc = afsclient_RPCStatOpenPort(cellHandle, srvrName, srvrPort, &conn, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_RPCStatOpenPort, status %d\n", st);
	exit(1);
    }

    rc = util_RPCStatsStateEnable(conn, RXSTATS_EnablePeerRPCStats, &st);
    if (!rc) {
	fprintf(stderr, "util_RPCStatsStateEnable, status %d\n", st);
	exit(1);
    }

    rc = afsclient_RPCStatClose(conn, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_RPCStatClose, status %d\n", st);
	exit(1);
    }

    rc = afsclient_CellClose(cellHandle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_CellClose, status %d\n", st);
	exit(1);
    }

    rc = afsclient_TokenClose(tokenHandle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_TokenClose, status %d\n", st);
	exit(1);
    }

    exit(0);
}
