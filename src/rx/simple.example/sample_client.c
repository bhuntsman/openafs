#include <sys/types.h>
#include <netdb.h>
#include <stdio.h>
#include "sample.h"

/* Bogus procedure to get internet address of host */
static u_long GetIpAddress(hostname)
    char *hostname;
{
    struct hostent *hostent;
    u_long host;
    hostent = gethostbyname(hostname);
    if (!hostent) {printf("host %s not found", hostname);exit(1);}
    if (hostent->h_length != sizeof(u_long)) {
	printf("host address is disagreeable length (%d)", hostent->h_length);
	exit(1);
    }
    bcopy(hostent->h_addr, (char *)&host, sizeof(host));
    return host;
}

main(argc, argv)
    char **argv;
{
    struct rx_connection *conn;
    u_long host;
    struct rx_securityClass *null_securityObject;
    int i;

    rx_Init(0);
    host = GetIpAddress(argv[1]);
    null_securityObject = rxnull_NewClientSecurityObject();
    conn = rx_NewConnection(host, SAMPLE_SERVER_PORT, SAMPLE_SERVICE_ID, null_securityObject, SAMPLE_NULL);
    for(i=1;i<10;i++) {
	int error, result;
	printf("add(%d,%d)",i,i*2);
	error = TEST_Add(conn, i,i*2,&result);
	printf(" ==> %d, error %d\n", result, error);
	printf("sub(%d,%d)",i,i*2);
	error = TEST_Sub(conn, i, i*2, &result);
	printf(" ==> %d, error %d\n", result, error);
    }
}
