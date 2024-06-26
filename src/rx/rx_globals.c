
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

/* RX:  Globals for internal use, basically */

/* Enable data initialization when the header file is included */
#define INIT(stuff) = stuff
#if defined(AFS_NT40_ENV) && defined(AFS_PTHREAD_ENV)
#define EXT __declspec(dllexport)
#else
#define EXT
#endif

#ifdef KERNEL
#include	"../afs/param.h"
#ifndef UKERNEL
#include "../h/types.h"
#else /* !UKERNEL */
#include	"../afs/sysincludes.h"
#endif /* !UKERNEL */
#else /* KERNEL */
#include	<afs/param.h>
#endif /* KERNEL */

#include "rx_globals.h"
