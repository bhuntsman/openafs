/* Copyright (C) 1999 IBM Corporation - All rights reserved.
 */

/*
 * This file contains sample code for the client admin interface 
 */

#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <pthread.h>
#endif
#include <afs/afs_Admin.h>
#include <afs/afs_clientAdmin.h>
#include <afs/afs_utilAdmin.h>

void Usage()
{
    fprintf(stderr,
	    "Usage: cm_client_config <host> <port>\n");
    exit(1);
}

void ParseArgs(
    int argc,
    char *argv[],
    char **srvrName,
    long *srvrPort)
{
    char **argp = argv;

    if (!*(++argp))
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
    void *cellHandle;
    afs_ClientConfig_t config;

    ParseArgs(argc, argv, &srvrName, &srvrPort);

    rc = afsclient_Init(&st);
    if (!rc) {
	fprintf(stderr, "afsclient_Init, status %d\n", st);
	exit(1);
    }

    rc = afsclient_NullCellOpen(&cellHandle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_NullCellsOpen, status %d\n", st);
	exit(1);
    }

    rc = afsclient_CMStatOpenPort(cellHandle, srvrName, srvrPort, &conn, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_CMStatOpenPort, status %d\n", st);
	exit(1);
    }

    rc = util_CMClientConfig(conn, &config, &st);
    if (!rc) {
	fprintf(stderr, "util_CMClientConfig, status %d\n", st);
	exit(1);
    }

    rc = afsclient_CMStatClose(conn, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_CMStatClose, status %d\n", st);
	exit(1);
    }

    rc = afsclient_CellClose(cellHandle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_CellClose, status %d\n", st);
	exit(1);
    }

    
    printf("\nClient configuration for %s (port %d):\n\n",
	   srvrName, srvrPort);
    printf("    clientVersion:  %d\n", config.clientVersion);
    printf("    serverVersion:  %d\n", config.serverVersion);
    printf("    nChunkFiles:    %d\n", config.c.config_v1.nChunkFiles);
    printf("    nStatCaches:    %d\n", config.c.config_v1.nStatCaches);
    printf("    nDataCaches:    %d\n", config.c.config_v1.nDataCaches);
    printf("    nVolumeCaches:  %d\n", config.c.config_v1.nVolumeCaches);
    printf("    firstChunkSize: %d\n", config.c.config_v1.firstChunkSize);
    printf("    otherChunkSize: %d\n", config.c.config_v1.otherChunkSize);
    printf("    cacheSize:      %d\n", config.c.config_v1.cacheSize);
    printf("    setTime:        %d\n", config.c.config_v1.setTime);
    printf("    memCache:       %d\n", config.c.config_v1.memCache);
    printf("\n");

    exit(0);
}
