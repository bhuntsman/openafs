/* Copyright Transarc Corporation 1998 - All Rights Reserved.
 *
 *
 * readdir.c - A minimal implementation of readdir to ease porting of AFS to
 * NT. Include dirent.h to pickup the required structs and prototypes.
 *
 * Implemented routines:
 * opendir
 * closedir
 * readdir
 */
#include <afs/param.h>
#include <errno.h>
#include <afs/errmap_nt.h>
#include <windows.h>
#include <winbase.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>

/* opendir() - The case insensitive version of opendir */
DIR *opendir(const char *path)
{
    struct DIR *tDir;
    HANDLE tH;
    char tPath[MAX_PATH];
    WIN32_FIND_DATA tData;
    int ntErr;

    (void) strcpy(tPath, path);
    (void) strcat(tPath, "\\*");
    tH = FindFirstFile(tPath, &tData);

    if (tH == INVALID_HANDLE_VALUE) {
	ntErr = GetLastError();
	switch (ntErr) {
	case ERROR_DIRECTORY:
	    errno = ENOTDIR;
	    return (DIR*)0;
	case ERROR_BAD_PATHNAME:
	    /* AFS NT client returns ERROR_BAD_PATHNAME where it should return
	     * ERROR_DIRECTORY.
	     */
	case ERROR_FILE_NOT_FOUND:
	    /* If at the "root" directory, then this can happen if it's empty.
	     */
	    {
		struct stat status;
		int len = strlen(tPath) - 1;
		tPath[len] = '\0';
		if (len >= 2 && tPath[len-2] != ':') {
		    tPath[len-1] = '\0';
		}
		if (stat(tPath, &status)<0) {
		    errno = nterr_nt2unix(GetLastError(), ENOENT);
		    return (DIR*)0;
		}
		if (!(status.st_mode & _S_IFDIR)) {
		    errno = ENOTDIR;
		    return (DIR*)0;
		}
	    }
	    break;
	default:
	    errno = nterr_nt2unix(GetLastError(), ENOENT);
	    return (DIR*)0;
	}
    }

    tDir = (DIR*)malloc(sizeof(DIR));
    if (!tDir) {
	errno = ENOMEM;
    }
    else {
	memset((void*)tDir, 0, sizeof(*tDir));
	tDir->h = tH;
	tDir->data = tData;
    }
    return tDir;
}

int closedir(DIR *dir)
{
    if (!dir || !dir->h) {
	errno = EINVAL;
	return -1;
    }

    if (dir->h != INVALID_HANDLE_VALUE)
	FindClose(dir->h);
    free((void*)dir);
    return 0;
}

struct dirent *readdir(DIR *dir)
{
    int rc;

    if (!dir) {
	errno = EBADF;
	return (struct dirent*)0;
    }

    errno = 0;
    if (dir->h == INVALID_HANDLE_VALUE)
	return (struct dirent*)0;

    if (dir->first) {
	dir->first = 0;
    }
    else {
      while(rc = FindNextFile(dir->h, &dir->data)) {
	if ((strcmp(dir->data.cFileName, ".") == 0) ||
	    (strcmp(dir->data.cFileName, "..") == 0)) {
	  
	  continue;	/* skip "." and ".." */
	}
	break;  /* found a non '.' or '..' entry. */
      }
      if (rc == 0) { /* FindNextFile() failed */
	if (GetLastError() == ERROR_NO_MORE_FILES)
	  return (struct dirent*)0;
	else {
	  errno = nterr_nt2unix(GetLastError(), EBADF);
		return (struct dirent*)0;
	}
      }
    }

    dir->cdirent.d_name = dir->data.cFileName;
    return &dir->cdirent;
}

