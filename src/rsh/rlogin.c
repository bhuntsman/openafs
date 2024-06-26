/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rlogin.c	5.6 (Berkeley) 9/18/85";
#endif /* not lint */

/*
 * rlogin - remote login
 */
#include <afs/param.h>
#if	!defined(AFS_HPUX_ENV) && !defined(AFS_SUN5_ENV) && !defined(AFS_SGI_ENV) && !defined(AFS_LINUX20_ENV)
#include <sys/param.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <stdio.h>
#include <sgtty.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <netdb.h>

# ifndef TIOCPKT_WINDOW
# define TIOCPKT_WINDOW 0x80
# endif /* TIOCPKT_WINDOW */

char	*index(), *rindex(), *malloc(), *getenv();
struct	passwd *getpwuid();
char	*name;
int	rem;
char	cmdchar = '~';
int	eight;
int	litout;
char	*speeds[] =
    { "0", "50", "75", "110", "134", "150", "200", "300",
      "600", "1200", "1800", "2400", "4800", "9600", "19200", "38400" };
char	term[256] = "network";
extern	int errno;
int	lostpeer();
#ifndef	COMPAT
int	dosigwinch = 0;
int	nosigwin = 0;
struct	winsize winsize;
int	sigwinch(), oob();
#else /* COMPAT */
#define	sigmask(s)	(1 << (s-1))
int	oob();
#endif /* COMPAT */

#ifdef	AFS_AIX32_ENV
#if defined(NLS) || defined(KJI)
#include <locale.h>
#endif
#endif

#include "AFS_component_version_number.c"

main(argc, argv)
	int argc;
	char **argv;
{
	char *host, *cp;
	struct sgttyb ttyb;
	struct passwd *pwd;
	struct servent *sp;
	int uid, options = 0, oldmask;
	int on = 1;

#if defined(AFS_AIX32_ENV) && (defined(NLS) || defined(KJI))
	setlocale(LC_ALL,"");
#endif

	host = rindex(argv[0], '/');
	if (host)
		host++;
	else
		host = argv[0];
	argv++, --argc;
	if (!strcmp(host, "rlogin"))
		host = *argv++, --argc;
another:
	if (argc > 0 && !strcmp(*argv, "-d")) {
		argv++, argc--;
		options |= SO_DEBUG;
		goto another;
	}
	if (argc > 0 && !strcmp(*argv, "-l")) {
		argv++, argc--;
		if (argc == 0)
			goto usage;
		name = *argv++; argc--;
		goto another;
	}
	if (argc > 0 && !strncmp(*argv, "-e", 2)) {
		cmdchar = argv[0][2];
		argv++, argc--;
		goto another;
	}
	if (argc > 0 && !strcmp(*argv, "-8")) {
		eight = 1;
		argv++, argc--;
		goto another;
	}
	if (argc > 0 && !strcmp(*argv, "-L")) {
		litout = 1;
		argv++, argc--;
		goto another;
	}
#ifndef	COMPAT
	if (argc > 0 && !strcmp(*argv, "-w")) {
		nosigwin++;
		argv++, argc--;
		goto another;
	}
#endif /* COMPAT */
	if (host == 0)
		goto usage;
	if (argc > 0)
		goto usage;
	pwd = getpwuid(getuid());
	if (pwd == 0) {
		fprintf(stderr, "Who are you?\n");
		exit(1);
	}
	sp = getservbyname("login", "tcp");
	if (sp == 0) {
		fprintf(stderr, "rlogin: login/tcp: unknown service\n");
		exit(2);
	}
	cp = getenv("TERM");
	if (cp)
		strcpy(term, cp);
	if (ioctl(0, TIOCGETP, &ttyb) == 0) {
		strcat(term, "/");
		strcat(term, speeds[ttyb.sg_ospeed]);
	}
#ifndef	COMPAT
#ifdef TIOCGWINSZ
	(void) ioctl(0, TIOCGWINSZ, &winsize);
#else
	(void) ioctl(0, TIOCGSIZE, &winsize);
#endif
#endif /* COMPAT */
	signal(SIGPIPE, lostpeer);
	signal(SIGURG, oob);
	oldmask = sigblock(sigmask(SIGURG));
        rem = rcmd(&host, sp->s_port, pwd->pw_name,
#ifdef	AFS_AIX32_ENV
	    name ? name : pwd->pw_name, term, 0, 0);
#else
	    name ? name : pwd->pw_name, term, 0);
#endif
        if (rem < 0)
                exit(1);
	if (options & SO_DEBUG &&
	    setsockopt(rem, SOL_SOCKET, SO_DEBUG, &on, sizeof (on)) < 0)
		perror("rlogin: setsockopt (SO_DEBUG)");
	uid = getuid();
	if (setuid(uid) < 0) {
		perror("rlogin: setuid");
		exit(1);
	}
	doit(oldmask);
	/*NOTREACHED*/
usage:
	fprintf(stderr,
	    "usage: rlogin host [ -ex ] [ -l username ] [ -8 ] [ -L ] [ -w ]\n");
	exit(1);
}

#define CRLF "\r\n"

int	child;
int	catchild();
int	writeroob();

int	defflags, tabflag;
int	deflflags;
char	deferase, defkill;
struct	tchars deftc;
struct	ltchars defltc;
struct	tchars notc =	{ -1, -1, -1, -1, -1, -1 };
struct	ltchars noltc =	{ -1, -1, -1, -1, -1, -1 };

doit(oldmask)
{
	int exit();
	struct sgttyb sb;

	ioctl(0, TIOCGETP, (char *)&sb);
	defflags = sb.sg_flags;
	tabflag = defflags & TBDELAY;
	defflags &= ECHO | CRMOD;
	deferase = sb.sg_erase;
	defkill = sb.sg_kill;
	ioctl(0, TIOCLGET, (char *)&deflflags);
	ioctl(0, TIOCGETC, (char *)&deftc);
	notc.t_startc = deftc.t_startc;
	notc.t_stopc = deftc.t_stopc;
	ioctl(0, TIOCGLTC, (char *)&defltc);
	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, exit);
	signal(SIGQUIT, exit);
	child = fork();
	if (child == -1) {
		perror("rlogin: fork");
		done();
	}
	if (child == 0) {
		mode(1);
		sigsetmask(oldmask);
		reader();
		sleep(1);
		prf("\007Connection closed.");
		exit(3);
	}
#ifndef	COMPAT
	signal(SIGURG, writeroob);
#endif /* COMPAT */
	sigsetmask(oldmask);
	signal(SIGCHLD, catchild);
#ifndef	COMPAT
	if (!nosigwin)
		signal(SIGWINCH, sigwinch);
#endif /* COMPAT */
	writer();
	prf("Closed connection.");
	done();
}

done()
{

	mode(0);
	if (child > 0 && kill(child, SIGKILL) >= 0)
		wait((int *)0);
	exit(0);
}
#ifndef	COMPAT
/*
 * This is called when the reader process gets the out-of-band (urgent)
 * request to turn on the window-changing protocol.
 */
writeroob()
{

	if (dosigwinch == 0)
		sendwindow();
	dosigwinch = 1;
}
#endif /* COMPAT */
catchild()
{
	int status;
	int pid;

again:
	pid = wait3(&status, WNOHANG|WUNTRACED, 0);
	if (pid == 0)
		return;
	/*
	 * if the child (reader) dies, just quit
	 */
	if (pid < 0 || pid == child && !WIFSTOPPED(status))
		done();
	goto again;
}

/*
 * writer: write to remote: 0 -> line.
 * ~.	terminate
 * ~^Z	suspend rlogin process.
 * ~^Y  suspend rlogin process, but leave reader alone.
 */
writer()
{
	char c;
	register n;
	register bol = 1;               /* beginning of line */
	register local = 0;

	for (;;) {
		n = read(0, &c, 1);
		if (n <= 0) {
			if (n < 0 && errno == EINTR)
				continue;
			break;
		}
		/*
		 * If we're at the beginning of the line
		 * and recognize a command character, then
		 * we echo locally.  Otherwise, characters
		 * are echo'd remotely.  If the command
		 * character is doubled, this acts as a 
		 * force and local echo is suppressed.
		 */
		if (bol) {
			bol = 0;
			if (c == cmdchar) {
				bol = 0;
				local = 1;
				continue;
			}
		} else if (local) {
			local = 0;
			if (c == '.' || c == deftc.t_eofc) {
				echo(c);
				break;
			}
			if (c == defltc.t_suspc || c == defltc.t_dsuspc) {
				bol = 1;
				echo(c);
				stop(c);
				continue;
			}
			if (c != cmdchar)
				write(rem, &cmdchar, 1);
		}
		if (write(rem, &c, 1) == 0) {
			prf("line gone");
			break;
		}
		bol = c == defkill || c == deftc.t_eofc ||
		    c == '\r' || c == '\n';
	}
}

echo(c)
register char c;
{
	char buf[8];
	register char *p = buf;

	c &= 0177;
	*p++ = cmdchar;
	if (c < ' ') {
		*p++ = '^';
		*p++ = c + '@';
	} else if (c == 0177) {
		*p++ = '^';
		*p++ = '?';
	} else
		*p++ = c;
	*p++ = '\r';
	*p++ = '\n';
	write(1, buf, p - buf);
}

stop(cmdc)
	char cmdc;
{
	mode(0);
	signal(SIGCHLD, SIG_IGN);
	kill(cmdc == defltc.t_suspc ? 0 : getpid(), SIGTSTP);
	signal(SIGCHLD, catchild);
	mode(1);
#ifndef	COMPAT
	sigwinch();			/* check for size changes */
#endif /* COMPAT */
}
#ifndef	COMPAT
sigwinch()
{
	struct winsize ws;

	if (dosigwinch && !nosigwin && ioctl(0, TIOCGWINSZ, &ws) == 0 &&
	    bcmp(&ws, &winsize, sizeof (ws))) {
		winsize = ws;
		sendwindow();
	}
}

/*
 * Send the window size to the server via the magic escape
 */
sendwindow()
{
	char obuf[4 + sizeof (struct winsize)];
	struct winsize *wp = (struct winsize *)(obuf+4);

	obuf[0] = 0377;
	obuf[1] = 0377;
	obuf[2] = 's';
	obuf[3] = 's';
	wp->ws_row = htons(winsize.ws_row);
	wp->ws_col = htons(winsize.ws_col);
	wp->ws_xpixel = htons(winsize.ws_xpixel);
	wp->ws_ypixel = htons(winsize.ws_ypixel);
	(void) write(rem, obuf, sizeof(obuf));
}
#endif /* COMPAT */
oob()
{
	int out = 1+1, atmark;
	char waste[BUFSIZ], mark;
	struct sgttyb sb;
	static int didnotify = 0;

	ioctl(1, TIOCFLUSH, (char *)&out);
	for (;;) {
		int rv;
		if (ioctl(rem, SIOCATMARK, &atmark) < 0) {
			perror("ioctl");
			break;
		}
		if (atmark)
			break;
		rv = read(rem, waste, sizeof (waste));
		if (rv <= 0)
			break;
	}
	recv(rem, &mark, 1, MSG_OOB);
	if (didnotify == 0 && (mark & TIOCPKT_WINDOW)) {
		/*
		 * Let server know about window size changes
		 */
		kill(getppid(), SIGURG);
		didnotify = 1;
	}
	if (eight)
		return;
	if (mark & TIOCPKT_NOSTOP) {
		ioctl(0, TIOCGETP, (char *)&sb);
		sb.sg_flags &= ~CBREAK;
		sb.sg_flags |= RAW;
		ioctl(0, TIOCSETN, (char *)&sb);
		notc.t_stopc = -1;
		notc.t_startc = -1;
		ioctl(0, TIOCSETC, (char *)&notc);
	}
	if (mark & TIOCPKT_DOSTOP) {
		ioctl(0, TIOCGETP, (char *)&sb);
		sb.sg_flags &= ~RAW;
		sb.sg_flags |= CBREAK;
		ioctl(0, TIOCSETN, (char *)&sb);
		notc.t_stopc = deftc.t_stopc;
		notc.t_startc = deftc.t_startc;
		ioctl(0, TIOCSETC, (char *)&notc);
	}
}

/*
 * reader: read from remote: line -> 1
 */
reader()
{
	char rb[BUFSIZ];
	register int cnt;

	signal(SIGTTOU, SIG_IGN);
	{ int pid = getpid();
	  fcntl(rem, F_SETOWN, pid); }
	for (;;) {
		cnt = read(rem, rb, sizeof (rb));
		if (cnt == 0)
			break;
		if (cnt < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		write(1, rb, cnt);
	}
}

mode(f)
{
	struct tchars *tc;
	struct ltchars *ltc;
	struct sgttyb sb;
	int	lflags;

	ioctl(0, TIOCGETP, (char *)&sb);
	ioctl(0, TIOCLGET, (char *)&lflags);
	switch (f) {

	case 0:
		sb.sg_flags &= ~(CBREAK|RAW|TBDELAY);
		sb.sg_flags |= defflags|tabflag;
		tc = &deftc;
		ltc = &defltc;
		sb.sg_kill = defkill;
		sb.sg_erase = deferase;
		lflags = deflflags;
		break;

	case 1:
		sb.sg_flags |= (eight ? RAW : CBREAK);
		sb.sg_flags &= ~defflags;
		/* preserve tab delays, but turn off XTABS */
		if ((sb.sg_flags & TBDELAY) == XTABS)
			sb.sg_flags &= ~TBDELAY;
		tc = &notc;
		ltc = &noltc;
		sb.sg_kill = sb.sg_erase = -1;
		if (litout)
			lflags |= LLITOUT;
		break;

	default:
		return;
	}
	ioctl(0, TIOCSLTC, (char *)ltc);
	ioctl(0, TIOCSETC, (char *)tc);
	ioctl(0, TIOCSETN, (char *)&sb);
	ioctl(0, TIOCLSET, (char *)&lflags);
}

/*VARARGS*/
prf(f, a1, a2, a3, a4, a5)
	char *f;
{
	fprintf(stderr, f, a1, a2, a3, a4, a5);
	fprintf(stderr, CRLF);
}

lostpeer()
{
	signal(SIGPIPE, SIG_IGN);
	prf("\007Connection closed.");
	done();
}
#else

#include "AFS_component_version_number.c"

main(argc, argv)
    int argc;
    char *argv[];
{
    printf("afs rlogin: Not supported for this system ...\n");
}
#endif	/* !defined(AFS_HPUX_ENV) */

