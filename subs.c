
/*
 * SUBS.C
 *
 * Copyright 1994 Matthew Dillon (dillon@apollo.backplane.com)
 * Copyright 2009 James Pryor <profjim@jimpryor.net>
 * May be distributed under the GNU General Public License
 */

#include "defs.h"

Prototype void logf(int level, const char *ctl, ...);
Prototype void fdlogf(int level, int fd, const char *ctl, ...);
Prototype void fdprintf(int fd, const char *ctl, ...);
Prototype void initsignals(void);
Prototype char Hostname[SMALL_BUFFER];

void vlog(int level, int fd, const char *ctl, va_list va);

char Hostname[SMALL_BUFFER];


void
logf(int level, const char *ctl, ...)
{
	va_list va;

	va_start(va, ctl);
	vlog(level, 2, ctl, va);
	va_end(va);
}

void
fdlogf(int level, int fd, const char *ctl, ...)
{
	va_list va;

	va_start(va, ctl);
	vlog(level, fd, ctl, va);
	va_end(va);
}

void
fdprintf(int fd, const char *ctl, ...)
{
	va_list va;
	char buf[LOG_BUFFER];

	va_start(va, ctl);
	vsnprintf(buf, sizeof(buf), ctl, va);
	write(fd, buf, strlen(buf));
	va_end(va);
}

void
vlog(int level, int fd, const char *ctl, va_list va)
{
	char buf[LOG_BUFFER];
	static short suppressHeader = 0;

	if (level <= LogLevel) {
		if (ForegroundOpt) {
			/*
			 * when -d or -f, we always (and only) log to stderr
			 * fd will be 2 except when 2 is bound to a execing subprocess, then it will be 8
			 * [v]snprintf write at most size including \0; they'll null-terminate, even when they truncate
			 * we don't care here whether it truncates
			 */
			vsnprintf(buf, sizeof(buf), ctl, va);
			write(fd, buf, strlen(buf));
		} else if (SyslogOpt) {
			/* log to syslog */
			vsnprintf(buf, sizeof(buf), ctl, va);
			syslog(level, "%s", buf);

		} else {
			/* log to file */

			time_t t = time(NULL);
			struct tm *tp = localtime(&t);
			int buflen, hdrlen = 0;
			buf[0] = 0; /* in case suppressHeader or strftime fails */
			if (!suppressHeader) {
				/*
				 * run LogHeader through strftime --> [yields hdr] plug in Hostname --> [yields buf]
				 */
				char hdr[SMALL_BUFFER];
				/* strftime returns strlen of result, provided that result plus a \0 fit into buf of size */
				if (strftime(hdr, sizeof(hdr), LogHeader, tp)) {
					if (gethostname(Hostname, sizeof(Hostname))==0)
						/* gethostname successful */
						/* result will be \0-terminated except gethostname doesn't promise to do so if it has to truncate */
						Hostname[sizeof(Hostname)-1] = 0;
					else
						Hostname[0] = 0;   /* gethostname() call failed */
					/* [v]snprintf write at most size including \0; they'll null-terminate, even when they truncate */
					/* return value >= size means result was truncated */
					if ((hdrlen = snprintf(buf, sizeof(hdr), hdr, Hostname)) >= sizeof(hdr))
						hdrlen = sizeof(hdr) - 1;
				}
			}
			if ((buflen = vsnprintf(buf + hdrlen, sizeof(buf) - hdrlen, ctl, va) + hdrlen) >= sizeof(buf))
				buflen = sizeof(buf) - 1;

			write(fd, buf, buflen);
			/* if previous write wasn't \n-terminated, we suppress header on next write */
			suppressHeader = (buf[buflen-1] != '\n');

		}
	}
}

void reopenlogger(int sig) {
	int fd;
	if (getpid() == DaemonPid) {
		/* only daemon handles, children should ignore */
		if ((fd = open(LogFile, O_WRONLY|O_CREAT|O_APPEND, 0600)) < 0) {
			/* can't reopen log file, exit */
			exit(errno);
		}
		dup2(fd, 2);
		close(fd);
	}
}

void
initsignals (void) {
	struct sigaction sa;

	/* save daemon's pid globally */
	DaemonPid = getpid();

	sa.sa_flags = SA_RESTART;
	if (!ForegroundOpt && !SyslogOpt)
		sa.sa_handler = reopenlogger;
	else
		sa.sa_handler = SIG_IGN;
	if (sigaction (SIGHUP, &sa, NULL) != 0) {
		int n = errno;
		fdprintf(2, "failed to start SIGHUP handling, reason: %s", strerror(errno));
		exit(n);
	}
}

