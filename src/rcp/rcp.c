/*
 * Copyright (c) 1983 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rcp.c	5.11 (Berkeley) 9/22/88";
#endif /* not lint */

/*
 * rcp
 */
#include <afs/param.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <netinet/in.h>

#include <stdio.h>
#include <signal.h>
#include <pwd.h>
#include <ctype.h>
#include <netdb.h>
#include <errno.h>

#ifdef	AFS_SUN5_ENV
#include <sys/fcntl.h>
#endif
#include <dirent.h>

#if defined(AFS_AIX_ENV)
#define vfork() fork()
#endif

#ifdef	AFS_AIX32_ENV
#ifndef MSG
#define MSGSTR(n,s) s
#define SYSERR(x)	sys_errlist[x]
#define NLfprintf	fprintf
#endif /* MSG */

#ifdef NLS
#include <NLchar.h>
#include <NLctype.h>
#endif
#endif /* AFS_AIX32_ENV */

#ifdef	AFS_OSF_ENV
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#define _PATH_CP        "/bin/cp"
#define _PATH_RSH       "/usr/bin/rsh"
#define _PATH_BSHELL    "/bin/sh"
#endif

int	rem;
char	*colon(), *index(), *rindex(), *malloc(), *strcpy();
int	errs;
#ifndef AFS_LINUX20_ENV
extern char *sys_errlist[];
#endif
void	lostconn();
int	iamremote, targetshouldbedirectory;
int	iamrecursive;
int	pflag;
struct	passwd *pwd;
struct	passwd *getpwuid();
int	userid;
int	port;

struct buffer {
	int	cnt;
	char	*buf;
} *allocbuf();

/*VARARGS*/
int	error();

#define	ga()	 	(void) write(rem, "", 1)

#include "AFS_component_version_number.c"

main(argc, argv)
	int argc;
	char **argv;
{
	char *targ, *host, *src;
	char *suser, *tuser, *thost;
	int i, tos;
	char buf[BUFSIZ], cmd[16];
	struct servent *sp;

#if defined(AFS_AIX32_ENV) && (defined(NLS) || defined(KJI))
	setlocale(LC_ALL,"");
#endif
	sp = getservbyname("shell", "tcp");
	if (sp == NULL) {
		fprintf(stderr, "rcp: shell/tcp: unknown service\n");
		exit(1);
	}
	port = sp->s_port;
	pwd = getpwuid(userid = getuid());
	if (pwd == 0) {
		fprintf(stderr, "who are you?\n");
		exit(1);
	}

	for (argc--, argv++; argc > 0 && **argv == '-'; argc--, argv++) {
		(*argv)++;
		while (**argv) switch (*(*argv)++) {

		    case 'r':
			iamrecursive++;
			break;

		    case 'p':		/* preserve mtimes and atimes */
			pflag++;
			break;

		    /* The rest of these are not for users. */
		    case 'd':
			targetshouldbedirectory = 1;
			break;

		    case 'f':		/* "from" */
			iamremote = 1;
			(void) response();
			(void) setuid(userid);
			source(--argc, ++argv);
			exit(errs);

		    case 't':		/* "to" */
			iamremote = 1;
			(void) setuid(userid);
			sink(--argc, ++argv);
			exit(errs);

		    default:
			usage();
		}
	}
	if (argc < 2)
		usage();
	if (argc > 2)
		targetshouldbedirectory = 1;
	rem = -1;
	(void) sprintf(cmd, "rcp%s%s%s",
	    iamrecursive ? " -r" : "", pflag ? " -p" : "", 
	    targetshouldbedirectory ? " -d" : "");
	(void) signal(SIGPIPE, lostconn);

	targ = colon(argv[argc - 1]);
	if (targ) {				/* ... to remote */
		*targ++ = 0;
		if (*targ == 0)
			targ = ".";
		thost = index(argv[argc - 1], '@');
		if (thost) {
			*thost++ = 0;
			tuser = argv[argc - 1];
			if (*tuser == '\0')
				tuser = NULL;
			else if (!okname(tuser))
				exit(1);
		} else {
			thost = argv[argc - 1];
			tuser = NULL;
		}
		for (i = 0; i < argc - 1; i++) {
		    char *binpath = "/usr/ucb/rsh";
		    struct stat tstat;

		    if (stat(binpath, &tstat)) {
			/*
			 * Doesn't exist; try another path
			 */
			binpath = "/bin/rsh";
		    }
			src = colon(argv[i]);
			if (src) {		/* remote to remote */
				*src++ = 0;
				if (*src == 0)
					src = ".";
				host = index(argv[i], '@');
				if (host) {
					*host++ = 0;
					suser = argv[i];
					if (*suser == '\0')
						suser = pwd->pw_name;
					else if (!okname(suser))
						continue;
					(void) sprintf(buf, "%s %s -l %s -n %s %s '%s%s%s:%s'",
					    binpath, host, suser, cmd, src,
					    tuser ? tuser : "",
					    tuser ? "@" : "",
					    thost, targ);
				} else
				    (void) sprintf(buf, "%s %s -n %s %s '%s%s%s:%s'",
					    binpath, argv[i], cmd, src,
					    tuser ? tuser : "",
					    tuser ? "@" : "",
					    thost, targ);
				(void) susystem(buf);
			} else {		/* local to remote */
				if (rem == -1) {
					(void) sprintf(buf, "%s -t %s",
					    cmd, targ);
					host = thost;
					rem = rcmd(&host, port, pwd->pw_name,
					    tuser ? tuser : pwd->pw_name,
					    buf, 0);
					if (rem < 0)
						exit(1);
#ifdef	AFS_OSF_ENV
					tos = IPTOS_THROUGHPUT;
					if (setsockopt(rem, IPPROTO_IP, IP_TOS,
						       (char *)&tos, sizeof(int)) < 0)
					    perror("rcp: setsockopt TOS (ignored)");
#endif
					if (response() < 0)
						exit(1);
					(void) setuid(userid);
				}
				source(1, argv+i);
			}
		}
	} else {				/* ... to local */
		if (targetshouldbedirectory)
			verifydir(argv[argc - 1]);
		for (i = 0; i < argc - 1; i++) {
			src = colon(argv[i]);
			if (src == 0) {		/* local to local */
#ifdef	AFS_OSF_ENV
			        (void) sprintf(buf, "%s%s%s %s %s", _PATH_CP,
#else
				(void) sprintf(buf, "/bin/cp%s%s %s %s",
#endif
				    iamrecursive ? " -r" : "", pflag ? " -p" : "",
				    argv[i], argv[argc - 1]);
				(void) susystem(buf);
			} else {		/* remote to local */
				*src++ = 0;
				if (*src == 0)
					src = ".";
				host = index(argv[i], '@');
				if (host) {
					*host++ = 0;
					suser = argv[i];
					if (*suser == '\0')
						suser = pwd->pw_name;
					else if (!okname(suser))
						continue;
				} else {
					host = argv[i];
					suser = pwd->pw_name;
				}
				(void) sprintf(buf, "%s -f %s", cmd, src);
				rem = rcmd(&host, port, pwd->pw_name, suser,
				    buf, 0);
				if (rem < 0)
					continue;
#ifdef AFS_HPUX_ENV
				(void) setuid(userid);
				sink(1, argv+argc-1);
				(void) setuid(0);
#else
#ifdef	AFS_OSF_ENV
				tos = IPTOS_THROUGHPUT;
				if (setsockopt(rem, IPPROTO_IP, IP_TOS,
					       (char *)&tos, sizeof(int)) < 0)
				    perror("rcp: setsockopt TOS (ignored)");
#endif
				(void) setreuid(0, userid);
				sink(1, argv+argc-1);
				(void) setreuid(userid, 0);
#endif
				(void) close(rem);
				rem = -1;
			}
		}
	}
	exit(errs);
}

verifydir(cp)
	char *cp;
{
	struct stat stb;

	if (stat(cp, &stb) >= 0) {
		if ((stb.st_mode & S_IFMT) == S_IFDIR)
			return;
		errno = ENOTDIR;
	}
	error("rcp: %s: %s.\n", cp, sys_errlist[errno]);
	exit(1);
}

char *
colon(cp)
	char *cp;
{

	while (*cp) {
		if (*cp == ':')
			return (cp);
		if (*cp == '/')
			return (0);
		cp++;
	}
	return (0);
}

okname(cp0)
	char *cp0;
{
	register char *cp = cp0;
	register int c;

	do {
		c = *cp;
		if (c & 0200)
			goto bad;
		if (!isalpha(c) && !isdigit(c) && c != '_' && c != '-')
			goto bad;
		cp++;
	} while (*cp);
	return (1);
bad:
	fprintf(stderr, "rcp: invalid user name %s\n", cp0);
	return (0);
}

susystem(s)
	char *s;
{
	int status, pid, w;
#ifdef	AFS_OSF_ENV
	register sig_t istat, qstat;
#else
	register void (*istat)(), (*qstat)();
#endif

	if ((pid = vfork()) == 0) {
		(void) setuid(userid);
#ifdef	AFS_OSF_ENV
		execl(_PATH_BSHELL, "sh", "-c", s, (char *)0);
#else
		execl("/bin/sh", "sh", "-c", s, (char *)0);
#endif
		_exit(127);
	}
	istat = signal(SIGINT, SIG_IGN);
	qstat = signal(SIGQUIT, SIG_IGN);
	while ((w = wait(&status)) != pid && w != -1)
		;
	if (w == -1)
		status = -1;
	(void) signal(SIGINT, istat);
	(void) signal(SIGQUIT, qstat);
	return (status);
}

source(argc, argv)
	int argc;
	char **argv;
{
	char *last, *name;
	struct stat stb;
	static struct buffer buffer;
	struct buffer *bp;
	int x, readerr, f, amt;
	off_t i;
	char buf[BUFSIZ];

	for (x = 0; x < argc; x++) {
		name = argv[x];
		if ((f = open(name, 0)) < 0) {
			error("rcp: %s: %s\n", name, sys_errlist[errno]);
			continue;
		}
		if (fstat(f, &stb) < 0)
			goto notreg;
		switch (stb.st_mode&S_IFMT) {

		case S_IFREG:
			break;

		case S_IFDIR:
			if (iamrecursive) {
				(void) close(f);
				rsource(name, &stb);
				continue;
			}
			/* fall into ... */
		default:
notreg:
			(void) close(f);
			error("rcp: %s: not a plain file\n", name);
			continue;
		}
		last = rindex(name, '/');
		if (last == 0)
			last = name;
		else
			last++;
		if (pflag) {
			/*
			 * Make it compatible with possible future
			 * versions expecting microseconds.
			 */
			(void) sprintf(buf, "T%ld 0 %ld 0\n",
			    stb.st_mtime, stb.st_atime);
			(void) write(rem, buf, strlen(buf));
			if (response() < 0) {
				(void) close(f);
				continue;
			}
		}
		(void) sprintf(buf, "C%04o %ld %s\n",
		    stb.st_mode&07777, stb.st_size, last);
		(void) write(rem, buf, strlen(buf));
		if (response() < 0) {
			(void) close(f);
			continue;
		}
		if ((bp = allocbuf(&buffer, f, BUFSIZ)) == 0) {
			(void) close(f);
			continue;
		}
		readerr = 0;
		for (i = 0; i < stb.st_size; i += bp->cnt) {
			amt = bp->cnt;
			if (i + amt > stb.st_size)
				amt = stb.st_size - i;
			if (readerr == 0 && read(f, bp->buf, amt) != amt)
				readerr = errno;
			(void) write(rem, bp->buf, amt);
		}
		(void) close(f);
		if (readerr == 0)
			ga();
		else
			error("rcp: %s: %s\n", name, sys_errlist[readerr]);
		(void) response();
	}
}


rsource(name, statp)
	char *name;
	struct stat *statp;
{
	DIR *d = opendir(name);
	char *last;
	struct dirent *dp;
	char buf[BUFSIZ];
	char *bufv[1];

	if (d == 0) {
		error("rcp: %s: %s\n", name, sys_errlist[errno]);
		return;
	}
	last = rindex(name, '/');
	if (last == 0)
		last = name;
	else
		last++;
	if (pflag) {
		(void) sprintf(buf, "T%ld 0 %ld 0\n",
		    statp->st_mtime, statp->st_atime);
		(void) write(rem, buf, strlen(buf));
		if (response() < 0) {
			closedir(d);
			return;
		}
	}
	(void) sprintf(buf, "D%04o %d %s\n", statp->st_mode&07777, 0, last);
	(void) write(rem, buf, strlen(buf));
	if (response() < 0) {
		closedir(d);
		return;
	}
	while (dp = readdir(d)) {
		if (dp->d_ino == 0)
			continue;
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;
		if (strlen(name) + 1 + strlen(dp->d_name) >= BUFSIZ - 1) {
			error("%s/%s: Name too long.\n", name, dp->d_name);
			continue;
		}
		(void) sprintf(buf, "%s/%s", name, dp->d_name);
		bufv[0] = buf;
		source(1, bufv);
	}
	closedir(d);
	(void) write(rem, "E\n", 2);
	(void) response();
}

response()
{
	char resp, c, rbuf[BUFSIZ], *cp = rbuf;

	if (read(rem, &resp, 1) != 1)
		lostconn();
	switch (resp) {

	case 0:				/* ok */
		return (0);

	default:
		*cp++ = resp;
		/* fall into... */
	case 1:				/* error, followed by err msg */
	case 2:				/* fatal error, "" */
		do {
			if (read(rem, &c, 1) != 1)
				lostconn();
			*cp++ = c;
		} while (cp < &rbuf[BUFSIZ] && c != '\n');
		if (iamremote == 0)
			(void) write(2, rbuf, cp - rbuf);
		errs++;
		if (resp == 1)
			return (-1);
		exit(1);
	}
	/*NOTREACHED*/
}

void lostconn()
{

	if (iamremote == 0)
		fprintf(stderr, "rcp: lost connection\n");
	exit(1);
}

sink(argc, argv)
	int argc;
	char **argv;
{
	off_t i, j;
	char *targ, *whopp, *cp;
	int of, mode, wrerr, exists, first, count, amt, size;
	struct buffer *bp;
	static struct buffer buffer;
	struct stat stb;
	int targisdir = 0;
	int mask = umask(0);
	char *myargv[1];
	char cmdbuf[BUFSIZ], nambuf[BUFSIZ];
	int setimes = 0;
	struct timeval tv[2];
#define atime	tv[0]
#define mtime	tv[1]
#define	SCREWUP(str)	{ whopp = str; goto screwup; }

	if (!pflag)
		(void) umask(mask);
	if (argc != 1) {
		error("rcp: ambiguous target\n");
		exit(1);
	}
	targ = *argv;
	if (targetshouldbedirectory)
		verifydir(targ);
	ga();
	if (stat(targ, &stb) == 0 && (stb.st_mode & S_IFMT) == S_IFDIR)
		targisdir = 1;
	for (first = 1; ; first = 0) {
		cp = cmdbuf;
		if (read(rem, cp, 1) <= 0)
			return;
		if (*cp++ == '\n')
			SCREWUP("unexpected '\\n'");
		do {
			if (read(rem, cp, 1) != 1)
				SCREWUP("lost connection");
		} while (*cp++ != '\n');
		*cp = 0;
		if (cmdbuf[0] == '\01' || cmdbuf[0] == '\02') {
			if (iamremote == 0)
				(void) write(2, cmdbuf+1, strlen(cmdbuf+1));
			if (cmdbuf[0] == '\02')
				exit(1);
			errs++;
			continue;
		}
		*--cp = 0;
		cp = cmdbuf;
		if (*cp == 'E') {
			ga();
			return;
		}

#define getnum(t) (t) = 0; while (isdigit(*cp)) (t) = (t) * 10 + (*cp++ - '0');
		if (*cp == 'T') {
			setimes++;
			cp++;
			getnum(mtime.tv_sec);
			if (*cp++ != ' ')
				SCREWUP("mtime.sec not delimited");
			getnum(mtime.tv_usec);
			if (*cp++ != ' ')
				SCREWUP("mtime.usec not delimited");
			getnum(atime.tv_sec);
			if (*cp++ != ' ')
				SCREWUP("atime.sec not delimited");
			getnum(atime.tv_usec);
			if (*cp++ != '\0')
				SCREWUP("atime.usec not delimited");
			ga();
			continue;
		}
		if (*cp != 'C' && *cp != 'D') {
			/*
			 * Check for the case "rcp remote:foo\* local:bar".
			 * In this case, the line "No match." can be returned
			 * by the shell before the rcp command on the remote is
			 * executed so the ^Aerror_message convention isn't
			 * followed.
			 */
			if (first) {
				error("%s\n", cp);
				exit(1);
			}
			SCREWUP("expected control record");
		}
		cp++;
		mode = 0;
		for (; cp < cmdbuf+5; cp++) {
			if (*cp < '0' || *cp > '7')
				SCREWUP("bad mode");
			mode = (mode << 3) | (*cp - '0');
		}
		if (*cp++ != ' ')
			SCREWUP("mode not delimited");
		size = 0;
		while (isdigit(*cp))
			size = size * 10 + (*cp++ - '0');
		if (*cp++ != ' ')
			SCREWUP("size not delimited");
		if (targisdir)
			(void) sprintf(nambuf, "%s%s%s", targ,
			    *targ ? "/" : "", cp);
		else
			(void) strcpy(nambuf, targ);
		exists = stat(nambuf, &stb) == 0;
		if (cmdbuf[0] == 'D') {
			if (exists) {
				if ((stb.st_mode&S_IFMT) != S_IFDIR) {
					errno = ENOTDIR;
					goto bad;
				}
				if (pflag)
					(void) chmod(nambuf, mode);
			} else if (mkdir(nambuf, mode) < 0)
				goto bad;
			myargv[0] = nambuf;
			sink(1, myargv);
			if (setimes) {
				setimes = 0;
				if (utimes(nambuf, tv) < 0)
					error("rcp: can't set times on %s: %s\n",
					    nambuf, sys_errlist[errno]);
			}
			continue;
		}
		if ((of = open(nambuf, O_WRONLY|O_CREAT, mode)) < 0) {
	bad:
			error("rcp: %s: %s\n", nambuf, sys_errlist[errno]);
			continue;
		}
		if (exists && pflag)
			(void) fchmod(of, mode);
		ga();
		if ((bp = allocbuf(&buffer, of, BUFSIZ)) == 0) {
			(void) close(of);
			continue;
		}
		cp = bp->buf;
		count = 0;
		wrerr = 0;
		for (i = 0; i < size; i += BUFSIZ) {
			amt = BUFSIZ;
			if (i + amt > size)
				amt = size - i;
			count += amt;
			do {
				j = read(rem, cp, amt);
				if (j <= 0) {
					if (j == 0)
					    error("rcp: dropped connection");
					else
					    error("rcp: %s\n",
						sys_errlist[errno]);
					exit(1);
				}
				amt -= j;
				cp += j;
			} while (amt > 0);
			if (count == bp->cnt) {
				if (wrerr == 0 &&
				    write(of, bp->buf, count) != count)
					wrerr++;
				count = 0;
				cp = bp->buf;
			}
		}
		if (count != 0 && wrerr == 0 &&
		    write(of, bp->buf, count) != count)
			wrerr++;
		if (ftruncate(of, size))
			error("rcp: can't truncate %s: %s\n",
			    nambuf, sys_errlist[errno]);
		(void) close(of);
		(void) response();
		if (setimes) {
			setimes = 0;
			if (utimes(nambuf, tv) < 0)
				error("rcp: can't set times on %s: %s\n",
				    nambuf, sys_errlist[errno]);
		}				   
		if (wrerr)
			error("rcp: %s: %s\n", nambuf, sys_errlist[errno]);
		else
			ga();
	}
screwup:
	error("rcp: protocol screwup: %s\n", whopp);
	exit(1);
}

struct buffer *
allocbuf(bp, fd, blksize)
	struct buffer *bp;
	int fd, blksize;
{
	struct stat stb;
	int size;

	if (fstat(fd, &stb) < 0) {
		error("rcp: fstat: %s\n", sys_errlist[errno]);
		return ((struct buffer *)0);
	}
#if defined(AIX) || defined(AFS_SUN5_ENV)
	size = blksize;
#else
	size = roundup(stb.st_blksize, blksize);
#endif /* defined(AIX) */
	if (size == 0)
		size = blksize;
	if (bp->cnt < size) {
		if (bp->buf != 0)
			free(bp->buf);
		bp->buf = (char *)malloc((unsigned) size);
		if (bp->buf == 0) {
			error("rcp: malloc: out of memory\n");
			return ((struct buffer *)0);
		}
	}
	bp->cnt = size;
	return (bp);
}

/*VARARGS1*/
error(fmt, a1, a2, a3, a4, a5)
	char *fmt;
#if defined(AFS_64BIT_ENV)
	char *a1, *a2, *a3, *a4, *a5;
#else
	int a1, a2, a3, a4, a5;
#endif 
{
	char buf[BUFSIZ], *cp = buf;

	errs++;
	*cp++ = 1;
	(void) sprintf(cp, fmt, a1, a2, a3, a4, a5);
	(void) write(rem, buf, strlen(buf));
	if (iamremote == 0)
		(void) write(2, buf+1, strlen(buf+1));
}

usage()
{
	fputs("usage: rcp [-p] f1 f2; or: rcp [-rp] f1 ... fn d2\n", stderr);
	exit(1);
}
