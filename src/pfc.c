#include "config.h"
#include "minilzo.h"

#ifdef __MINGW32__
#define Win32_Winsock
#include <windows.h>
#include <io.h>			/* for chdir and umask */
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ASSERT_H
#include <assert.h>
#endif
#ifdef STDC_HEADERS
#include <stdio.h>
#include <string.h>
#include <assert.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#else
#define EX_USAGE 64
#endif

#ifdef __MINGW32__

#ifdef ECONNREFUSED
#undef ECONNREFUSED
#endif
#define ECONNREFUSED WSAECONNREFUSED

#ifdef EINVAL
#undef EINVAL
#endif
#define EINVAL WSAEINVAL

#ifdef EBADF
#undef EBADF
#endif
#define EBADF WSAEBADF

#define close(s) closesocket(s)
#define read(s,b,l) recv((s),(b),(l),0)
#define write(s,b,l) send((s),(b),(l),0)
#define perror(s) fprintf(stderr, "%s: %d\n", (s), WSAGetLastError())

#else /* Unix or Cygwin */

typedef int SOCKET;

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#endif

#ifndef NULL
#define NULL 0
#endif

#if !defined(HAVE_MEMCMP)
#undef memcmp
#define memcmp    lzo_memcmp
#endif
#if !defined(HAVE_MEMCPY)
#undef memcpy
#define memcpy    lzo_memcpy
#endif
#if !defined(HAVE_MEMMOVE)
#undef memmove
#define memmove   lzo_memmove
#endif
#if !defined(HAVE_MEMSET)
#undef memset
#define memset    lzo_memset
#endif

/* Set by command line args */
int listen_port = 0;
int remote_port = 0;
const char *remote_host = NULL;
int server_side = 0;

/* Maximum packet size */
#define MAX_PACKET_DATA (1024 * 63)

/* A packet we are accumulating */
typedef struct
{
	int p_length;		/* length we're expecting */
	int p_sofar;		/* what we've read so far */
	int p_compressed;	/* is the packet compressed? */
	char p_buf[MAX_PACKET_DATA + 3]; /* buffer holding the packet */
}
Packet;

#ifdef HAVE_FORK
Packet packet =
{
	-1, 0, 0
};
#endif

/* Statistics on the data compressed by this process */
int c_total;
int c_compr;

#ifndef HAVE_FORK

#define MAXCONN	10
typedef struct
{
	SOCKET s1;
	SOCKET s2;
	Packet packet;
}
Connection;
Connection connects[MAXCONN];

fd_set sfds;			/* FDs we're watching */
fd_set cfds;			/* FDs to compress */
int numfds;			/* Highest fd in the set */

#endif				/* !HAVE_FORK */

static int decompress(SOCKET s, SOCKET d, Packet * p);
static int compress(SOCKET s, SOCKET d);

static SOCKET
server_establish()
{
	SOCKET s;
	struct sockaddr_in sa;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) <= 0)
	{
		return -1;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(listen_port);
	if (bind(s, (struct sockaddr *) &sa, sizeof(sa)) < 0)
	{
		close(s);
		return -1;
	}
	listen(s, 3);		/* max # of queued connects */
	return s;
}

static SOCKET
get_connection(SOCKET s)
{
	struct sockaddr_in isa;
	int i;
	SOCKET t;

	i = sizeof(isa);
	t = accept(s, (struct sockaddr *) &isa, &i);
	if (t < 0) {
		t = -1;
	}
	return t;
}

#if defined(HAVE_FORK) && defined(SIGTSTP)
static void
reaper(int ignored)
{
	union wait wstatus;

	signal(SIGCHLD, reaper);	/* re-register signal handler */
	while (wait3((void *) &wstatus, WNOHANG, NULL) > 0)
		;
}
#endif				/* HAVE_FORK && SIGTSTP */

static SOCKET
call_socket(const char *hostname, short portnum)
{
	struct sockaddr_in sa;
	struct hostent *hp;
	SOCKET s;

	if ((hp = gethostbyname(hostname)) == NULL)
	{
		errno = ECONNREFUSED;
		return (-1);
	}

	/* set address */
	memset(&sa, 0, sizeof(sa));
	memcpy((char *) &sa.sin_addr, hp->h_addr, hp->h_length);

	sa.sin_family = hp->h_addrtype;
	sa.sin_port = htons(portnum);

	if ((s = socket(hp->h_addrtype, SOCK_STREAM, 0)) < 0)
	{
		return -1;
	}
	if (connect(s, (struct sockaddr *) &sa, sizeof(sa)) < 0)
	{
		close(s);
		return -1;
	}
	return s;
}

#ifndef HAVE_FORK
static int
check_socket(SOCKET s)
{
	struct sockaddr_in sin;
	int namelen;

	namelen = sizeof(sin);
	if (getsockname(s, (struct sockaddr *) &sin, &namelen) < 0)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

static void
end_connect(unsigned c)
{
	printf("end connect(%d, %d <-> %d)\n", c, connects[c].s1,
	       connects[c].s2);
	close(connects[c].s1);
	close(connects[c].s2);
	FD_CLR(connects[c].s1, &sfds);
	FD_CLR(connects[c].s1, &cfds);
	FD_CLR(connects[c].s2, &sfds);
	FD_CLR(connects[c].s2, &cfds);
	connects[c].s1 = (SOCKET) - 1;
}

static void
process(SOCKET in, SOCKET out, unsigned c)
{
	if (!FD_ISSET(in, &cfds))
	{
		if (compress(in, out) <= 0)
		{
			end_connect(c);
		}
	}
	else
	{
		if (decompress(in, out, &connects[c].packet) <= 0)
		{
			end_connect(c);
		}
	}
}

static void
reap_sockets(void)
{
	/* Reap a socket that's gone away -- check validity using
	   getsockname */
#ifdef __MINGW32__
	errno = WSAGetLastError();
#endif

	if (errno == EINVAL || errno == EBADF)
	{
		int i;
		for (i = 0; i < MAXCONN; i++)
		{
			if (connects[i].s1 != (SOCKET) - 1)
			{
				if (!check_socket(connects[i].s1) ||
				    !check_socket(connects[i].s2))
				{
					printf("End connect %d.\n", i);
					end_connect(i);
				}
			}
		}
	}
}

static void
new_connection(SOCKET s)
{
	SOCKET in, out;

	in = get_connection(s);

	if (in <= 0)
	{
		return;
	}

	out = call_socket(remote_host, remote_port);

	if (out == (SOCKET) - 1)
	{
		close(in);
	}
	else
	{
		int i;

		if (server_side)
		{
			/* the "in" thing is talking compressed to
			   the client, while "out" is talking
			   uncompressed to perforce */
			FD_SET(in, &cfds);
		}
		else
		{
			/* the "in" thing is talking uncompressed
			   to p4, while "out" is talking
			   compressed to pfc */
			FD_SET(out, &cfds);
		}

		for (i = 0; i < MAXCONN; i++)
		{
			if (connects[i].s1 == (SOCKET) - 1)
			{
				printf("connect(%d, %d <-> %d)\n", 
				       i, in, out);
				connects[i].s1 = in;
				connects[i].s2 = out;
				connects[i].packet.p_length = -1;
				connects[i].packet.p_sofar = 0;
				connects[i].packet.p_compressed = 0;
				break;
			}
		}

		assert(i != MAXCONN);

		FD_SET(in, &sfds);
		FD_SET(out, &sfds);

		if (in >= numfds)
		{
			numfds = in + 1;
		}
		if (out >= numfds)
		{
			numfds = out + 1;
		}
	}
}

static void
process_connections(fd_set* rfds)
{
	int i;
	for (i = 0; i < MAXCONN; i++)
	{
		if (connects[i].s1 == (SOCKET) -1)
		{
			continue;
		}
		if (FD_ISSET(connects[i].s1, rfds))
		{
			process(connects[i].s1, connects[i].s2, i);
		}
		if (FD_ISSET(connects[i].s2, rfds))
		{
			process(connects[i].s2, connects[i].s1, i);
		}
	}
}

static void
server_loop_no_fork(SOCKET s)
{
	unsigned i;
	FD_ZERO(&sfds);
	FD_ZERO(&cfds);
	FD_SET(s, &sfds);
	numfds = s + 1;

	for (i = 0; i < MAXCONN; i++)
	{
		connects[i].s1 = connects[i].s2 = (SOCKET) -1;
	}

	while (1)
	{
		int retval;
		fd_set rfds;

		/* Wait for any socket to become readable */
		rfds = sfds;
		retval = select(numfds, &rfds, NULL, NULL, NULL);

		if (retval > 0)
		{
			if (FD_ISSET(s, &rfds))
			{
				printf("New connection!\n");
				new_connection(s);
			}
			process_connections(&rfds);
		}
		else
		{
			reap_sockets();
		}
	}
}
#endif

#ifdef HAVE_FORK

static void
proxy_connection(SOCKET in)
{
	fd_set rfds;
	int retval;
	int max_plus_one;
	SOCKET out;

	if ((out = call_socket(remote_host, remote_port)) <= 0)
	{
		perror("call_socket");	/* FIXME: don't want perror */
		close(out);
		close(in);
		return;
	}

	/* Watch both sockets for read availability. */
	FD_ZERO(&rfds);
	FD_SET(in, &rfds);
	FD_SET(out, &rfds);
	max_plus_one = out;
	if (in > out)
	{
		max_plus_one = in;
	}
	++max_plus_one;

	if (server_side)
	{
		SOCKET temp = in;

		in = out;
		out = temp;
	}

	while ((retval = select(max_plus_one, &rfds, NULL, NULL, NULL)) > 0)
	{
		if (FD_ISSET(in, &rfds))
		{
			if (compress(in, out) <= 0)
			{
				break;
			}
		}
		if (FD_ISSET(out, &rfds))
		{
			if (decompress(out, in, &packet) <= 0)
			{
				break;
			}
		}

		/* re-initialize rfds */
		FD_SET(in, &rfds);
		FD_SET(out, &rfds);
	}

#ifdef DEBUG
	printf("Compressed %d to %d bytes (%.1f%%).  Bye.\n",
	       c_total, c_compr, (double) c_compr / c_total * 100);
#endif

	close(in);
	close(out);
}

static void
server_loop_fork(SOCKET s)
{
#ifdef SIGTSTP			/* BSD */
	signal(SIGCHLD, reaper);	/* reap zombie child processes */
#else				/* System V */
	signal(SIGCHLD, SIG_IGN);	/* reap zobmie child processes */
#endif				/* BSD -vs- System V */

	while (1)
	{
		SOCKET t;

		if ((t = get_connection(s)) < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			perror("get_connection"); /* FIXME: no perror */
			exit(1);
		}
		switch (fork())
		{
		case -1:
			perror("fork");	/* FIXME: don't want perror */
			close(s);
			close(t);
			exit(1);
		case 0:		/* we're the child */
			proxy_connection(t);
			exit(0);
		default:		/* we're the parent */
			close(t);
		}
	}
}
#endif

static void
server_loop(void)
{
	SOCKET s;

	if ((s = server_establish()) == (SOCKET) - 1)
	{
		perror("establish");
		exit(1);
	}

#ifdef HAVE_FORK
	server_loop_fork(s);
#else
	server_loop_no_fork(s);
#endif
}

static int
decompress(SOCKET s, SOCKET d, Packet * p)
{
	int l;

	l = read(s, p->p_buf + p->p_sofar, sizeof(p->p_buf) - p->p_sofar);
	if (l <= 0)
	{
		return l;
	}

	p->p_sofar += l;

  try_again:

	if (p->p_length < 0 && p->p_sofar > 3)
	{
		if (p->p_buf[0] == 'c')
		{
			p->p_compressed = 1;
		}
		else if (p->p_buf[0] == 'r')
		{
			p->p_compressed = 0;
		}
		else
		{
			return -1;
		}

		p->p_length = p->p_buf[1] | (p->p_buf[2] << 8);
	}

	if (p->p_length > 0 && p->p_sofar >= p->p_length)
	{
		if (p->p_compressed)
		{
			lzo_byte dbuf[MAX_PACKET_DATA];
			lzo_uint dlen;

			lzo1x_decompress((const lzo_byte*)(p->p_buf + 3), 
					 p->p_length - 3,
					 dbuf, &dlen, NULL);
			assert(dlen <= MAX_PACKET_DATA);
			write(d, (const void*)dbuf, dlen);
		}
		else
		{
			write(d, p->p_buf + 3, p->p_length - 3);
		}
		p->p_sofar -= p->p_length;
		memmove(p->p_buf, p->p_buf + p->p_length, p->p_sofar);
		p->p_length = -1;
		goto try_again;
	}
	return l;
}

static int
compress(SOCKET s, SOCKET d)
{
	int l;
	lzo_uint cl;
	unsigned char buf[MAX_PACKET_DATA + 3];
	unsigned char cbuf[MAX_PACKET_DATA + 3 + 
			  (MAX_PACKET_DATA / 64) + 16 + 3];
	unsigned char lzo_wrkmem[LZO1X_1_MEM_COMPRESS];

	if ((l = read(s, (void*)(buf + 3), MAX_PACKET_DATA)) > 0)               
	{
		lzo1x_1_compress((const lzo_byte*)(buf + 3), l,
				 cbuf + 3, &cl,
				 lzo_wrkmem);
		assert(cl < sizeof(cbuf) - 3);	/* FIXME: syslog */
		if (cl < l)
		{
#ifdef DEBUG
			printf("sc %d/%d\n", l, cl);
#endif
			c_total += l;
			c_compr += cl;

			cl += 3;
			cbuf[0] = 'c';
			cbuf[1] = cl & 0xff;
			cbuf[2] = (cl >> 8) & 0xff;
			write(d, (const void*)cbuf, cl);
		}
		else
		{
#ifdef DEBUG
			printf("sr %d\n", l, cl);
#endif
			c_total += l;
			c_compr += l;

			l += 3;
			buf[0] = 'r';
			buf[1] = l & 0xff;
			buf[2] = (l >> 8) & 0xff;
			write(d, (const void*)buf, l);
		}
	}
	return l;
}


static void
usage(const char *prog)
{
	const char *base = strrchr(prog, '/');

	printf("pfc version %s\n", VERSION);

	if (!base)
	{
		base = prog;
	}
	else
	{
		++base;
	}
	fprintf(stderr,
		"usage: %s -S|-C local_port:remote_host:remote_port\n\n"
		"Use -C for the %s that will accept connections from\n"
		"the client side.  Use -S for the server side.\n\n"
		"For example, if you want connections made to port 6666\n"
		"on europe.foo.com to be forwarded and compressed to port\n"
		"6666 on asia.foo.com, do the following:\n\n"
		"On europe.foo.com: pfc -C 6666:asia.foo.com:6667\n"
		"On asia.foo.com:   pfc -S 6667:localhost:6666\n\n"
		"This, of course, means that port 6667 on asia.foo.com must\n"
		"be available for %s's use.\n",
		base, base, base);
	exit(EX_USAGE);
}

static void
parse_args(int argc, char *argv[])
{
	int i;

	for (i = 1; i < argc; i++)
	{
		if ((i + 1 == argc) || argv[i][0] != '-')
		{
			usage(argv[0]);
		}

		switch (argv[i][1])
		{
		case 'S':
			server_side = 1;
			/* fall through */

		case 'C':
			if (argc - i >= 1)
			{
				char *f1;
				char *f2;
				char *f3;

				++i;
				f1 = argv[i];
				if (f1)
				{
					f2 = strchr(f1, ':');
					if (f2)
					{
						f3 = strchr(f2 + 1, ':');
						if (f3)
						{
							*f2++ = '\0';
							*f3++ = '\0';

							listen_port = (short) strtol(f1, NULL, 10);
							remote_host = f2;
							remote_port = (short) strtol(f3, NULL, 10);
						}
					}
				}
			}
			else
			{
				usage(argv[0]);
			}
			break;
		}
	}
	if (!listen_port || !remote_port || !remote_host || (server_side < 0))
	{
		usage(argv[0]);
	}
}

static void
daemon_start(void)
{
#ifdef HAVE_FORK
	int childpid;
#endif
#ifdef TIOCNOTTY
	int fd;
#endif

	/* If started by init, no need to detatch. */
#ifdef HAVE_GETPPID
	if (getppid() != 1)
#endif
	{
		/* Ignore the terminal stop signals */
#ifdef SIGTTOU
		signal(SIGTTOU, SIG_IGN);
#endif
#ifdef SIGTTIN
		signal(SIGTTIN, SIG_IGN);
#endif
#ifdef SIGTSTP
		signal(SIGTSTP, SIG_IGN);
#endif

#ifdef HAVE_FORK
		/* If we're not started in the background, fork and let the
		 * parent exit.  This also guarantees the first child is not a
		 * process group leader. */
		if ((childpid = fork()) < 0)
		{
			perror("Can't fork first child");
			exit(1);		/* fork fails, error case */
		}
		else if (childpid > 0)
		{
			exit(0);		/* parent */
		}
#endif

		/* Now we're the first child process.  Disassociate from the
		 * controlling terminal and process group.  Ensure the process
		 * can't reaquire a new controlling terminal. */

#ifdef TIOCNOTTY		/* BSD */
		if (setpgrp(0, getpid()) == -1)
		{
			perror("Can't change process group");
			exit(2); /* can't change process group */
		}
		if ((fd = open("/dev/tty", O_RDWR)) >= 0)
		{
			/* lose controlling tty */
			ioctl(fd, TIOCNOTTY, (char *) NULL);
			close(fd);
		}

#elif defined(HAVE_SETPGRP) && defined(HAVE_FORK)

		/* System V */

		if (setpgrp() == -1)
		{
			perror("Can't change process group");
      
			exit(2); /* can't change process group */
		}

		signal(SIGHUP, SIG_IGN); /* immune from pgrp leader death */

		if ((childpid = fork()) < 0)
		{
			perror("Can't fork second child");
			exit(3);		/* can't fork second child */
		}
		else if (childpid > 0)
		{
			exit(0);		/* first child */
		}
		/* second child */
#endif				/* BSD -vs- System V */

	}

#if 0				/* skip until real logging is implemented */
	/* 
	 * Close open file descriptors
	 */
	for (fd = 0; fd < NOFILE; fd++)
	{
		close(fd);
	}
#endif

	errno = 0;			/* probably set to EBADF above */

	/* 
	 * Make current directory root to avoid tying up a mounted file
	 * system.
	 */
#ifndef __MINGW32__
	chdir("/");
#endif

	/* 
	 * Clear any inherited file mode creation mask.
	 */
	umask(0);
}

int
main(int argc, char *argv[])
{
#ifdef HAVE_WINSOCK_H
	WSADATA wsaData;

	WSAStartup(0x0002, &wsaData);
#endif

	parse_args(argc, argv);
#if 0
	printf("redirecting localhost:%d to %s:%d\n",
	       listen_port, remote_host, remote_port);
#endif

	daemon_start();
	server_loop();
	return 0;
}

/*
;;; Local Variables: ***
;;; mode:c-mode ***
;;; c-indentation-style:python ***
;;; End: ***
*/
