/* Copyright (C) 1998 by Transarc Corporation */

#ifndef _PARAM_NT35_H_
#define _PARAM_NT35_H_


#define AFS_NT40_ENV        1
#define AFSLITTLE_ENDIAN    1
#define AFS_64BIT_IOPS_ENV  1
#define AFS_NAMEI_ENV       1   /* User space interface to file system */
#define AFS_HAVE_STATVFS    0	/* System doesn't support statvfs */

#include <afs/afs_sysnames.h>
#define SYS_NAME_ID	SYS_NAME_ID_i386_nt35

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

/*
 * NT makes size_t a typedef for unsigned int (e.g. in <stddef.h>)
 * and has no typedef for ssize_t (a signed size_t).
 * So, we make our own.
 */
typedef int ssize_t;

/* these macros define Unix-style functions missing in  VC++5.0/NT4.0 */
#define MAXPATHLEN _MAX_PATH

#define bzero(A, S) memset((void*)(A), 0, (size_t)(S))
#define bcopy(A, B, S) memcpy((void*)(B), (void*)(A), (size_t)(S))
/* There is a minor syntactic difference between memcmp and bcmp... */
#define bcmp(A,B,S) (memcmp((void*)(A), (void*)(B), (size_t)(S)) ? 1 : 0)
#define strcasecmp(s1,s2)       _stricmp(s1,s2) 
#define strncasecmp(s1,s2,n)    _strnicmp(s1,s2,n)
#define index(s, c)             strchr(s, c)
#define rindex(s, c)            strrchr(s, c)
#define sleep(seconds)          Sleep((seconds) * 1000)
#define fsync(fileno)           _commit(fileno)
#define ftruncate(fd, size)     _chsize((fd), (long)(size))
#define strtoll(str, cp, base)  strtoi64((str), (cp), (base))
#define strtoull(str, cp, base) strtoui64((str), (cp), (base))

#define random()                rand()
#define srandom(a)              srand(a)

#define popen(cmd, mode)        _popen((cmd), (mode))
#define pclose(stream)          _pclose(stream)
typedef char * caddr_t;

#define pipe(fdp)               _pipe(fdp, 4096, _O_BINARY)
#endif /* _PARAM_NT35_H_ */
