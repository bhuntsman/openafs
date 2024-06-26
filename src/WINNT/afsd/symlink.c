/* Copyright (C) 1997 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <winsock2.h>
#include <errno.h>

#include <osi.h>
#include <afsint.h>

typedef long afs_int32;

#include "fs_utils.h"
#include "cmd.h"

#define MAXNAME 100
#define	MAXSIZE	2048
#define MAXINSIZE 1300    /* pioctl complains if data is larger than this */

static char space[MAXSIZE];
static char tspace[1024];

#ifndef WIN32
static struct ubik_client *uclient;
#endif /* not WIN32 */


static char pn[] = "symlink";
static int rxInitDone = 0;

void Die();

foldcmp (a, b)
    register char *a;
    register char *b; {
    register char t, u;
    while (1) {
        t = *a++;
        u = *b++;
        if (t >= 'A' && t <= 'Z') t += 0x20;
        if (u >= 'A' && u <= 'Z') u += 0x20;
        if (t != u) return 1;
        if (t == 0) return 0;
    }
}

/* this function returns TRUE (1) if the file is in AFS, otherwise false (0) */
static int InAFS(apath)
register char *apath; {
    struct ViceIoctl blob;
    register afs_int32 code;

    blob.in_size = 0;
    blob.out_size = MAXSIZE;
    blob.out = space;

    code = pioctl(apath, VIOC_FILE_CELL_NAME, &blob, 1);
    if (code) {
	if ((errno == EINVAL) || (errno == ENOENT)) return 0;
    }
    return 1;
}

/* return a static pointer to a buffer */
static char *Parent(apath)
char *apath; {
    register char *tp;
    strcpy(tspace, apath);
    tp = strrchr(tspace, '\\');
    if (tp) {
	*(tp+1) = 0;	/* lv trailing slash so Parent("k:\foo") is "k:\" not "k:" */
    }
    else {
	fs_ExtractDriveLetter(apath, tspace);
    	strcat(tspace, ".");
    }
    return tspace;
}


static ListLinkCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;
    int error;
    register struct cmd_item *ti;
    char orig_name[1024];		/*Original name, may be modified*/
    char true_name[1024];		/*``True'' dirname (e.g., symlink target)*/
    char parent_dir[1024];		/*Parent directory of true name*/
    register char *last_component;	/*Last component of true name*/
#ifndef WIN32
    struct stat statbuff;		/*Buffer for status info*/
#endif /* not WIN32 */
#ifndef WIN32
    int link_chars_read;		/*Num chars read in readlink()*/
#endif /* not WIN32 */
    int	thru_symlink;			/*Did we get to a mount point via a symlink?*/
    
    error = 0;
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	thru_symlink = 0;
#ifdef WIN32
	strcpy(orig_name, ti->data);
#else /* not WIN32 */
	sprintf(orig_name, "%s%s",
		(ti->data[0] == '/') ? "" : "./",
		ti->data);
#endif /* not WIN32 */

#ifndef WIN32
	if (lstat(orig_name, &statbuff) < 0) {
	    /* if lstat fails, we should still try the pioctl, since it
		may work (for example, lstat will fail, but pioctl will
		    work if the volume of offline (returning ENODEV). */
	    statbuff.st_mode = S_IFDIR; /* lie like pros */
	}

	/*
	 * The lstat succeeded.  If the given file is a symlink, substitute
	 * the file name with the link name.
	 */
	if ((statbuff.st_mode & S_IFMT) == S_IFLNK) {
	    thru_symlink = 1;
	    /*
	     * Read name of resolved file.
	     */
	    link_chars_read = readlink(orig_name, true_name, 1024);
	    if (link_chars_read <= 0) {
		fprintf(stderr,"%s: Can't read target name for '%s' symbolic link!\n",
		       pn, orig_name);
		exit(1);
	    }

	    /*
	     * Add a trailing null to what was read, bump the length.
	     */
	    true_name[link_chars_read++] = 0;

	    /*
	     * If the symlink is an absolute pathname, we're fine.  Otherwise, we
	     * have to create a full pathname using the original name and the
	     * relative symlink name.  Find the rightmost slash in the original
	     * name (we know there is one) and splice in the symlink value.
	     */
	    if (true_name[0] != '\\') {
		last_component = (char *) strrchr(orig_name, '\\');
		strcpy(++last_component, true_name);
		strcpy(true_name, orig_name);
	    }
	}
	else
	    strcpy(true_name, orig_name);
#else	/* WIN32 */
	strcpy(true_name, orig_name);
#endif /* WIN32 */

	/*
	 * Find rightmost slash, if any.
	 */
	last_component = (char *) strrchr(true_name, '\\');
	if (!last_component)
	    last_component = (char *) strrchr(true_name, '/');
	if (last_component) {
	    /*
	     * Found it.  Designate everything before it as the parent directory,
	     * everything after it as the final component.
	     */
	    strncpy(parent_dir, true_name, last_component - true_name + 1);
	    parent_dir[last_component - true_name + 1] = 0;
	    last_component++;   /*Skip the slash*/
	}
	else {
	    /*
	     * No slash appears in the given file name.  Set parent_dir to the current
	     * directory, and the last component as the given name.
	     */
	    fs_ExtractDriveLetter(true_name, parent_dir);
	    strcat(parent_dir, ".");
	    last_component = true_name;
            fs_StripDriveLetter(true_name, true_name, sizeof(true_name));
	}

	if (strcmp(last_component, ".") == 0 || strcmp(last_component, "..") == 0) {
	    fprintf(stderr,"%s: you may not use '.' or '..' as the last component\n", pn);
	    fprintf(stderr,"%s: of a name in the 'symlink list' command.\n", pn);
	    error = 1;
	    continue;
	}
	blob.in = last_component;
	blob.in_size = strlen(last_component)+1;
	blob.out_size = MAXSIZE;
	blob.out = space;
	memset(space, 0, MAXSIZE);

	code = pioctl(parent_dir, VIOC_LISTSYMLINK, &blob, 1);

	if (code == 0)
	    printf("'%s' is a %ssymlink to '%s'\n",
		   ti->data,
		   (thru_symlink ? "symbolic link, leading to a " : ""),
		   space);

	else {
	    error = 1;
	    if (errno == EINVAL)
		fprintf(stderr,"'%s' is not a symlink.\n",
		       ti->data);
	    else {
		Die(errno, (ti->data ? ti->data : parent_dir));
	    }
	}
    }
    return error;
}

static MakeLinkCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code;
    struct ViceIoctl blob;

    if (!InAFS(Parent(as->parms[0].items->data))) {
	fprintf(stderr,"%s: symlinks must be created within the AFS file system\n", pn);
	return 1;
    }

    strcpy(space, as->parms[1].items->data);
#ifdef WIN32
    /* create symlink with a special pioctl for Windows NT, since it doesn't
     * have a symlink system call.
     */
    blob.out_size = 0;
    blob.in_size = 1 + strlen(space);
    blob.in = space;
    blob.out = NULL;
    code = pioctl(as->parms[0].items->data, VIOC_SYMLINK, &blob, 0);
#else /* not WIN32 */
    code = symlink(space, as->parms[0].items->data);
#endif /* not WIN32 */
    if (code) {
	Die(errno, as->parms[0].items->data);
	return 1;
    }
    return 0;
}

/*
 * Delete AFS symlinks.  Variables are used as follows:
 *       tbuffer: Set to point to the null-terminated directory name of the
 *	    symlink (or ``.'' if none is provided)
 *      tp: Set to point to the actual name of the symlink to nuke.
 */
static RemoveLinkCmd(as)
register struct cmd_syndesc *as; {
    register afs_int32 code=0;
    struct ViceIoctl blob;
    register struct cmd_item *ti;
    char tbuffer[1024];
    char lsbuffer[1024];
    register char *tp;
    
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	tp = (char *) strrchr(ti->data, '\\');
	if (!tp)
	    tp = (char *) strrchr(ti->data, '/');
	if (tp) {
	    strncpy(tbuffer, ti->data, code=tp-ti->data+1);  /* the dir name */
            tbuffer[code] = 0;
	    tp++;   /* skip the slash */
	}
	else {
	    fs_ExtractDriveLetter(ti->data, tbuffer);
	    strcat(tbuffer, ".");
	    tp = ti->data;
            fs_StripDriveLetter(tp, tp, 0);
	}
	blob.in = tp;
	blob.in_size = strlen(tp)+1;
	blob.out = lsbuffer;
	blob.out_size = sizeof(lsbuffer);
	code = pioctl(tbuffer, VIOC_LISTSYMLINK, &blob, 0);
	if (code) {
	    if (errno == EINVAL)
		fprintf(stderr,"fs: '%s' is not a symlink.\n", ti->data);
	    else {
		Die(errno, ti->data);
	    }
	    continue;	/* don't bother trying */
	}
	blob.out_size = 0;
	blob.in = tp;
	blob.in_size = strlen(tp)+1;
	code = pioctl(tbuffer, VIOC_DELSYMLINK, &blob, 0);
	if (code) {
	    Die(errno, ti->data);
	}

    }
    return code;
}

static    struct ViceIoctl gblob;
static int debug = 0;

main(argc, argv)
int argc;
char **argv; {
    register afs_int32 code;
    register struct cmd_syndesc *ts;
    
#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a 
     * crash (i.e. core is generated) we can include the user's data section 
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;
    
    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
#endif

#ifdef WIN32
    WSADATA WSAjunk;
    WSAStartup(0x0101, &WSAjunk);
#endif /* WIN32 */

    /* try to find volume location information */
    

    osi_Init();

    ts = cmd_CreateSyntax("list", ListLinkCmd, 0, "list symlink");    
    cmd_AddParm(ts, "-name", CMD_LIST, 0, "name");
    
    ts = cmd_CreateSyntax("make", MakeLinkCmd, 0, "make symlink");
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "name");
    cmd_AddParm(ts, "-to", CMD_SINGLE, 0, "target");

    ts = cmd_CreateSyntax("rm", RemoveLinkCmd, 0, "remove symlink");
    cmd_AddParm(ts, "-name", CMD_LIST, 0, "name");

    code = cmd_Dispatch(argc, argv);

#ifndef WIN32
    if (rxInitDone) rx_Finalize();
#endif /* not WIN32 */
    
    return code;
}

void Die(code, filename)
    int   code;
    char *filename;
{ /*Die*/

    if (code == EINVAL) {
	if (filename)
	    fprintf(stderr,"%s: Invalid argument; it is possible that %s is not in AFS.\n", pn, filename);
	else fprintf(stderr,"%s: Invalid argument.\n", pn);
    }
    else if (code == ENOENT) {
	if (filename) fprintf(stderr,"%s: File '%s' doesn't exist\n", pn, filename);
	else fprintf(stderr,"%s: no such file returned\n", pn);
    }
    else if (code == EROFS)  fprintf(stderr,"%s: You can not change a backup or readonly volume\n", pn);
    else if (code == EACCES || code == EPERM) {
	if (filename) fprintf(stderr,"%s: You don't have the required access rights on '%s'\n", pn, filename);
	else fprintf(stderr,"%s: You do not have the required rights to do this operation\n", pn);
    }
    else if (code == ENODEV) {
	fprintf(stderr,"%s: AFS service may not have started.\n", pn);
    }
    else if (code == ESRCH) {
	fprintf(stderr,"%s: Cell name not recognized.\n", pn);
    }
    else if (code == EPIPE) {
	fprintf(stderr,"%s: Volume name or ID not recognized.\n", pn);
    }
    else if (code == EFBIG) {
	fprintf(stderr,"%s: Cache size too large.\n", pn);
    }
    else if (code == ETIMEDOUT) {
	if (filename)
	    fprintf(stderr,"%s:'%s': Connection timed out", pn, filename);
	else
	    fprintf(stderr,"%s: Connection timed out", pn);
    }
    else {
	if (filename) fprintf(stderr,"%s:'%s'", pn, filename);
	else fprintf(stderr,"%s", pn);
#ifdef WIN32
	fprintf(stderr, ": code 0x%x\n", code);
#else /* not WIN32 */
	fprintf(stderr,": %s\n", error_message(code));
#endif /* not WIN32 */
    }
} /*Die*/
