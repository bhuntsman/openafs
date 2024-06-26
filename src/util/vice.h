/* $ACIS:vice.h 9.0$ */

#if !defined(lint) && !defined(LOCORE)  && defined(RCS_HDRS)
#endif

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

/*
 * /usr/include/sys/vice.h
 *
 * Definitions required by user processes needing extended vice facilities
 * of the kernel.
 * 
 * NOTE:  /usr/include/sys/remote.h contains definitions common between
 *	    	Venus and the kernel.
 * 	    /usr/local/include/viceioctl.h contains ioctl definitions common
 *	    	between user processes and Venus.
 */
#include <afs/param.h>
#ifdef AFS_SUN5_ENV
#include <sys/ioccom.h>
#endif

#if defined(KERNEL)
/* a fixed size, whether the kernel uses the ILP32 or LP64 data models */
struct ViceIoctl32 {
	unsigned int in, out;	/* Data to be transferred in, or out */
	short in_size;		/* Size of input buffer <= 2K */
	short out_size;		/* Maximum size of output buffer, <= 2K */
};
#endif

#ifndef AFS_NT40_ENV  /* NT decl in sys/pioctl_nt.h with pioctl() decl */
struct ViceIoctl {
	caddr_t in, out;	/* Data to be transferred in, or out */
	short in_size;		/* Size of input buffer <= 2K */
	short out_size;		/* Maximum size of output buffer, <= 2K */
};
#endif

/* The 2K limits above are a consequence of the size of the kernel buffer
   used to buffer requests from the user to venus--2*MAXPATHLEN.
   The buffer pointers may be null, or the counts may be 0 if there
   are no input or output parameters
 */
/*
 * The structure argument to _IOW() is used to add a structure
 * size component to the _IOW() value, to help the kernel code identify
 * how big a structure the calling process is passing into a system
 * call.
 *
 * In user space, struct ViceIoctl32 and struct ViceIoctl are the same
 * except on Digital Unix, where user space code is compiled in 64-bit
 * mode.
 *
 * So, in kernel space, regardless whether it is compiled in 32-bit
 * mode or 64-bit mode, the kernel code can use the struct ViceIoctl32
 * version of _IOW() to check the size of user space arguments -- except
 * on Digital Unix.
 */
#if defined(KERNEL) && !defined(AFS_OSF_ENV)
#define _VICEIOCTL(id)  ((unsigned int ) _IOW('V', id, struct ViceIoctl32))
#else
#define _VICEIOCTL(id)  ((unsigned int ) _IOW('V', id, struct ViceIoctl))
#endif

/* Use this macro to define up to 256 vice ioctl's.  These ioctl's
   all potentially have in/out parameters--this depends upon the
   values in the ViceIoctl structure.  This structure is itself passed
   into the kernel by the normal ioctl parameter passing mechanism.
 */

#define _VALIDVICEIOCTL(com) (com >= _VICEIOCTL(0) && com <= _VICEIOCTL(255))
