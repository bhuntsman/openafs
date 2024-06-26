/* Copyright (C) 1990, 1989 Transarc Corporation - All rights reserved */
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


/* NOTE: fc_cbc_encrypt now modifies its 5th argument, to permit chaining over
 * scatter/gather vectors.
 */

#define DEBUG 0

#ifdef KERNEL
#include "../afs/param.h"
#ifndef UKERNEL
#include "../afs/stds.h"
#include "../h/types.h"
#ifndef AFS_LINUX20_ENV
#include "../netinet/in.h"
#endif
#else /* UKERNEL */
#include "../afs/sysincludes.h"
#include "../afs/stds.h"
#endif /* UKERNEL */
#ifdef AFS_LINUX22_ENV
#include <asm/byteorder.h>
#endif

#include "../afs/longc_procs.h"

#else /* KERNEL */

#include <afs/param.h>
#include <afs/stds.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <rx/rx.h>
#endif /* KERNEL */

#include "sboxes.h"
#include "fcrypt.h"
#include "rxkad.h"


#ifdef TCRYPT
int ROUNDS = 16;
#else
#define ROUNDS 16
#endif

#define XPRT_FCRYPT
#ifdef KERNEL
#include "../afs/permit_xprt.h"
#else
#include "../permit_xprt.h"
#endif

int fc_keysched (key, schedule)
  IN struct ktc_encryptionKey *key;
  OUT fc_KeySchedule    schedule;
{   unsigned char *keychar = (unsigned char *)key;
    afs_uint32  kword[2];

    unsigned int   temp;
    int		   i;

    /* first, flush the losing key parity bits. */
    kword[0] = (*keychar++) >> 1;
    kword[0] <<= 7;
    kword[0] += (*keychar++) >> 1;
    kword[0] <<= 7;
    kword[0] += (*keychar++) >> 1;
    kword[0] <<= 7;
    kword[0] += (*keychar++) >> 1;
    kword[1] = kword[0] >> 4;		/* get top 24 bits for hi word */
    kword[0] &= 0xf;
    kword[0] <<= 7;
    kword[0] += (*keychar++) >> 1;
    kword[0] <<= 7;
    kword[0] += (*keychar++) >> 1;
    kword[0] <<= 7;
    kword[0] += (*keychar++) >> 1;
    kword[0] <<= 7;
    kword[0] += (*keychar) >> 1;

    schedule[0] = kword[0];
    for (i=1; i<ROUNDS; i++) {
	/* rotate right 3 */
	temp = kword[0] & ((1<<11)-1);	/* get 11 lsb */
	kword[0] = (kword[0] >> 11) | ((kword[1] & ((1<<11)-1)) << (32-11));
	kword[1] = (kword[1] >> 11) | (temp << (56-32-11));
	schedule[i] = kword[0];
    }
    LOCK_RXKAD_STATS
    rxkad_stats.fc_key_scheds++;
    UNLOCK_RXKAD_STATS
    return 0;
}

AFS_HIDE
afs_int32 fc_ecb_encrypt(clear, cipher, schedule, encrypt)
  IN afs_uint32  *clear;
  OUT afs_uint32 *cipher;
  IN fc_KeySchedule  schedule;
  IN int	     encrypt;	/* 0 ==> decrypt, else encrypt */
{   afs_uint32  L,R;
    afs_uint32  S,P;
    unsigned char *Pchar = (unsigned char *)&P;
    unsigned char *Schar = (unsigned char *)&S;
    int i;

#if defined(vax) || (defined(mips) && defined(MIPSEL)) || defined(AFSLITTLE_ENDIAN)
#define Byte0 3 
#define Byte1 2
#define Byte2 1
#define Byte3 0
#else
#define Byte0 0
#define Byte1 1
#define Byte2 2
#define Byte3 3
#endif

#if 0
    bcopy (clear, &L, sizeof(afs_int32));
    bcopy (clear+1, &R, sizeof(afs_int32));
#else
    L = ntohl(*clear);
    R = ntohl(*(clear+1));
#endif

    if (encrypt) {
	LOCK_RXKAD_STATS
	rxkad_stats.fc_encrypts[ENCRYPT]++;
	UNLOCK_RXKAD_STATS
	for (i=0; i<(ROUNDS/2); i++) {
	    S = *schedule++ ^ R;	/* xor R with key bits from schedule */
	    Pchar[Byte2] = sbox0[Schar[Byte0]];	/* do 8-bit S Box subst. */
	    Pchar[Byte3] = sbox1[Schar[Byte1]];	/* and permute the result */
	    Pchar[Byte1] = sbox2[Schar[Byte2]];
	    Pchar[Byte0] = sbox3[Schar[Byte3]];
	    P = (P >> 5) | ((P & ((1<<5)-1)) << (32-5)); /* right rot 5 bits */
	    L ^= P;			/* we're done with L, so save there */
	    S = *schedule++ ^ L;	/* this time xor with L */
	    Pchar[Byte2] = sbox0[Schar[Byte0]];
	    Pchar[Byte3] = sbox1[Schar[Byte1]];
	    Pchar[Byte1] = sbox2[Schar[Byte2]];
	    Pchar[Byte0] = sbox3[Schar[Byte3]];
	    P = (P >> 5) | ((P & ((1<<5)-1)) << (32-5)); /* right rot 5 bits */
	    R ^= P;
	}
    }
    else {
	LOCK_RXKAD_STATS
	rxkad_stats.fc_encrypts[DECRYPT]++;
	UNLOCK_RXKAD_STATS
	schedule = &schedule[ROUNDS-1];	/* start at end of key schedule */
	for (i=0; i<(ROUNDS/2); i++) {
	    S = *schedule-- ^ L;	/* xor R with key bits from schedule */
	    Pchar[Byte2] = sbox0[Schar[Byte0]];	/* do 8-bit S Box subst. and */
	    Pchar[Byte3] = sbox1[Schar[Byte1]];	/* permute the result */
	    Pchar[Byte1] = sbox2[Schar[Byte2]];
	    Pchar[Byte0] = sbox3[Schar[Byte3]];
	    P = (P >> 5) | ((P & ((1<<5)-1)) << (32-5)); /* right rot 5 bits */
	    R ^= P;			/* we're done with L, so save there */
	    S = *schedule-- ^ R;	/* this time xor with L */
	    Pchar[Byte2] = sbox0[Schar[Byte0]];
	    Pchar[Byte3] = sbox1[Schar[Byte1]];
	    Pchar[Byte1] = sbox2[Schar[Byte2]];
	    Pchar[Byte0] = sbox3[Schar[Byte3]];
	    P = (P >> 5) | ((P & ((1<<5)-1)) << (32-5)); /* right rot 5 bits */
	    L ^= P;
	}
    }
#if 0
    bcopy (&L, cipher, sizeof(afs_int32));
    bcopy (&R, cipher+1, sizeof(afs_int32));
#else
    *cipher = htonl(L);
    *(cipher+1) = htonl(R);
#endif
    return 0;
}

/* Crypting can be done in segments by recycling xor.  All but the final segment must
 * be multiples of 8 bytes.
 * NOTE: fc_cbc_encrypt now modifies its 5th argument, to permit chaining over
 * scatter/gather vectors.
 */
AFS_HIDE
afs_int32 fc_cbc_encrypt (input, output, length, key, xor, encrypt)
  char		*input;
  char		*output;
  afs_int32		 length;		/* in bytes */
  int		 encrypt;		/* 0 ==> decrypt, else encrypt */
  fc_KeySchedule key;			/* precomputed key schedule */
  afs_uint32 *xor;			/* 8 bytes of initialization vector */
{   afs_uint32 i,j;
    afs_uint32 t_input[2];
    afs_uint32 t_output[2];
    unsigned char *t_in_p = (unsigned char *) t_input;

    if (encrypt) {
	for (i = 0; length > 0; i++, length -= 8) {
	    /* get input */
	    bcopy (input, t_input, sizeof(t_input));
	    input += sizeof(t_input);

	    /* zero pad */
	    for (j = length; j <= 7; j++)
		*(t_in_p+j)= 0;

	    /* do the xor for cbc into the temp */
	    xor[0] ^= t_input[0] ;
	    xor[1] ^= t_input[1] ;
	    /* encrypt */
	    fc_ecb_encrypt (xor, t_output, key, encrypt);

	    /* copy temp output and save it for cbc */
	    bcopy (t_output, output, sizeof(t_output));
	    output += sizeof(t_output);

	    /* calculate xor value for next round from plain & cipher text */
	    xor[0] = t_input[0] ^ t_output[0];
	    xor[1] = t_input[1] ^ t_output[1];


	}
	t_output[0] = 0;
	t_output[1] = 0;
    }
    else {
	/* decrypt */
	for (i = 0; length > 0; i++, length -= 8) {
	    /* get input */
	    bcopy (input, t_input, sizeof(t_input));
	    input += sizeof(t_input);

	    /* no padding for decrypt */
	    fc_ecb_encrypt(t_input, t_output, key, encrypt);

	    /* do the xor for cbc into the output */
	    t_output[0] ^= xor[0] ;
	    t_output[1] ^= xor[1] ;

	    /* copy temp output */
	    bcopy (t_output, output, sizeof(t_output));
	    output += sizeof(t_output);

	    /* calculate xor value for next round from plain & cipher text */
	    xor[0] = t_input[0] ^ t_output[0];
	    xor[1] = t_input[1] ^ t_output[1];
	}
    }
    return 0;
}
