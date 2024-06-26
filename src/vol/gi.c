
#ifndef lint
#endif

/*
 * (C) COPYRIGHT IBM CORPORATION 1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

int statflag;

#include "AFS_component_version_number.c"

main(argc,argv)
char **argv;
{
	int error=0;
	struct stat status;
	int dev, fd, inode;
	argc--; argv++;
	while (argc && **argv == '-') {
		if (strcmp(*argv, "-stat") == 0)
			statflag = 1;
		else {
			error=1;
			break;
		}
		argc--; argv++;
	}
	if (error || argc!=2) {
		fprintf(stderr, "Usage: gi [-stat] partition inodenumber\n");
		exit(1);
	}
	if (stat(*argv, &status) != 0) {
		fprintf(stderr, "gi: cannot stat %s [should be mounted partition name]\n", *argv);
		exit(1);
	}
	dev = status.st_dev;
	inode = atoi(*++argv);
	fd = iopen(dev, inode, 0);
	if (fd < 0) {
		Perror("Unable to open inode %d", inode);
		exit(1);
	}
	if (statflag) {
		if (fstat(fd, &status) != 0) {
			Perror("Unable to fstat the inode!");			
			exit(1);
		}
		printf("Inode status: dev=%d, ino=%d, mode=%o, nlink=%d, uid=%d, gid=%d, size=%d, mtime=%d, blocks=%d\n", status.st_dev, status.st_ino, status.st_mode, status.st_nlink, status.st_uid, status.st_gid, status.st_size, status.st_mtime);
	}
	else {
		/* Send the inode to standard out */
		char buf[4096]; int n;
		while ((n = read(fd, buf, sizeof(buf))) >0)
			write(1, buf, n);
	}
	exit(0);
}

Perror(err, a1, a2, a3)
   char *err;
{
	char msg[200];
	sprintf(msg, err, a1, a2, a3);
	perror(msg);
}

