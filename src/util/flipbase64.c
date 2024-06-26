/*===============================================================
 * Copyright (C) 1997 Transarc Corporation - All rights reserved
 *===============================================================*/


#include <afs/param.h>

#if defined(AFS_NAMEI_ENV)
#include <sys/types.h>
#include "afsutil.h"

/* This version of base64 gets it right and starts converting from the low
 * bits to the high bits. 
 */
/* This table needs to be in lexical order to efficiently map back from
 * characters to the numerical value.
 */
static char c_xlate[80] =
	"+=0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

/* int_to_base64
 * Create a base 64 string representation of a number.
 * The supplied string 's' must be at least 12 bytes long.
 * lb64_string in stds.h provides a typedef to get the length.
 */
char *int64_to_flipbase64(lb64_string_t s, u_int64_t a)
{
    int i, j;
    int64_t n;

    i = 0;
    if (a==0)
	s[i++] = c_xlate[0];
    else {
	for (n = a & 0x3f; a; n = ((a>>=6) & 0x3f)) {
	    s[i++] = c_xlate[n];
	}
    }
    s[i] = '\0';
    return s;
}


/* Mapping: +=0, ==1, 0-9 = 2-11, A-Z = 12-37, a-z = 38-63 */
int64_t flipbase64_to_int64(char *s)
{
    int64_t n = 0;
    int64_t result = 0;
    int shift;

    for (shift = 0; *s; s++, shift += 6) {
	if (*s == '+') n = 0;
	else if (*s == '=') n = 1;
	else if (*s <= '9') {
	    n = 2 + (int)(*s - '0');
	}
	else if (*s <= 'Z') {
	    n = 12 + (int)(*s - 'A');
	}
	else if (*s <= 'z') {
	    n = 38 + (int)(*s - 'a');
	}
	n <<= shift;
	result |= n ;
    }
    return result;
}


#endif /* AFS_NAMEI_ENV */
