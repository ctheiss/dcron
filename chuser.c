
/*
 * CHUSER.C
 *
 * Copyright 1994 Matthew Dillon (dillon@apollo.backplane.com)
 * Copyright 2009-2010 James Pryor <profjim@jimpryor.net>
 * May be distributed under the GNU General Public License
 */

#include "defs.h"

Prototype /*@maynotreturn@*/ uid_t ChangeUser(const char *user, STRING dochdir, const char *when, const char *desc);

static /*@noreturn@*/ void ChangeUserFailed(const char *reason, STRING post_reason, const char *user, const char *when, const char *desc);



void
ChangeUserFailed(const char *reason, STRING post_reason, const char *user, const char *when, const char *desc)
{
	char buf[LINE_BUF];
	/* [v]snprintf always \0-terminate; we don't care here if result was truncated */
	(void)snprintf(buf, sizeof(buf), "%s user %%s%s\n", reason, when);
	if (post_reason)
		logger(LOG_ERR, buf, post_reason, user, desc);
	else
		logger(LOG_ERR, buf, user, desc);
	exit(EXIT_FAILURE);
}


uid_t
ChangeUser(const char *user, STRING dochdir, const char *when, const char *desc)
{
	struct passwd *pas;

	/*
	 * Obtain password entry and change privilages
	 */
	if ((pas = getpwnam(user)) == NULL)
		ChangeUserFailed("could not change to unknown", NULL, user, when, desc);

	(void)setenv("USER", pas->pw_name, 1);
	(void)setenv("HOME", pas->pw_dir, 1);
	(void)setenv("SHELL", "/bin/sh", 1);

	/*
	 * Change running state to the user in question
	 */

	if (initgroups(user, pas->pw_gid) < 0) {
		char buf[SMALL_BUF];
		(void)snprintf(buf, sizeof(buf), " gid %d%s", pas->pw_gid, when);
		ChangeUserFailed("could not initgroups for", NULL, user, buf, desc);
	}
	if (setregid(pas->pw_gid, pas->pw_gid) < 0) {
		char buf[SMALL_BUF];
		size_t k;
		(void)snprintf(buf, sizeof(buf), " gid %d", pas->pw_gid);
		k = strlen(buf);
		ChangeUserFailed("could not setregid for", NULL, user, strncat(buf + k, when, sizeof(buf) - k - 1), desc);
	}
	if (setreuid(pas->pw_uid, pas->pw_uid) < 0) {
		char buf[SMALL_BUF];
		size_t k;
		(void)snprintf(buf, sizeof(buf), " uid %d", pas->pw_uid);
		k = strlen(buf);
		ChangeUserFailed("could not setreuid for", NULL, user, strncat(buf + k, when, sizeof(buf) - k - 1), desc);
	}
	if (dochdir) {
		/* first try to cd $HOME */
		if (chdir(pas->pw_dir) < 0) {
			/* if that fails, complain then cd to the backup dochdir, usually /tmp */
			char buf[LINE_BUF];
			(void)snprintf(buf, sizeof(buf), "could not chdir to %s for user %%s%s\n", pas->pw_dir, when);
			logger(LOG_ERR, buf, user, desc);
			if (chdir(dochdir) < 0)
				ChangeUserFailed("could not chdir to %s for", dochdir, user, when, desc);
		}
	}
	return pas->pw_uid;
}

