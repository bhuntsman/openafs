/* -*- C -*- Automatically set mode to C-mode since that's closest to right */

/* RX Authentication Stress test: rxgen file */

/* Copyright (C) 1991 Transarc Corporation - All rights reserved */

%#define	RXKST_SERVICEPORT	7100 /* test server's port */
%#define	RXKST_SERVICEID		0xeb81

package RXKST_

/* so client & server stubs don't clash */
prefix S

proc Fast (IN  u_long n,
	   OUT u_long *inc_n) = 1;
proc Slow (IN  u_long tag,
	   OUT u_long *time) = 2;
proc Copious (IN  u_long inlen, u_long insum, u_long outlen,
	      OUT u_long *outsum) split = 3;
proc Kill () = 4;
