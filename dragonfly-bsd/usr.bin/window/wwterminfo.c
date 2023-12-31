/*	@(#)wwterminfo.c	8.1 (Berkeley) 6/6/93	*/
/*	$NetBSD: wwterminfo.c,v 1.5 2003/08/07 11:17:45 agc Exp $	*/

/*
 * Copyright (c) 1982, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef TERMINFO

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <paths.h>
#include <unistd.h>
#include "local.h"
#include "ww.h"

/*
 * Terminfo support
 *
 * Written by Brian Buhrow
 *
 * Subsequently modified by Edward Wang
 */

/*
 * Initialize the working terminfo directory
 */
int
wwterminfoinit()
{
	FILE *fp;
	char buf[2048];

		/* make the directory */
	(void) sprintf(wwterminfopath, "%swwinXXXXXX", _PATH_TMP);
	if (mkdtemp(wwterminfopath) == NULL ||
	    chmod(wwterminfopath, 0755) < 0) {
		wwerrno = WWE_SYS;
		return -1;
	}
	(void) setenv("TERMINFO", wwterminfopath, 1);
		/* make a termcap entry and turn it into terminfo */
	(void) sprintf(buf, "%s/cap", wwterminfopath);
	if ((fp = fopen(buf, "w")) == NULL) {
		wwerrno = WWE_SYS;
		return -1;
	}
	(void) fprintf(fp, "%sco#%d:li#%d:%s\n",
		WWT_TERMCAP, wwncol, wwnrow, wwwintermcap);
	(void) fclose(fp);
	(void) sprintf(buf,
		"cd %s; %s cap >info 2>/dev/null; %s info >/dev/null 2>&1",
		wwterminfopath, _PATH_CAPTOINFO, _PATH_TIC);
	(void) system(buf);
	return 0;
}

/*
 * Delete the working terminfo directory at shutdown
 */
int
wwterminfoend()
{
	int pstat;
	pid_t pid;

	pid = vfork();
	switch (pid) {
	case -1:
		/* can't really do (or say) anything about errors */
		return -1;
	case 0:
		execl(_PATH_RM, _PATH_RM, "-rf", wwterminfopath, 0);
		_exit(1);
	}
	pid = waitpid(pid, &pstat, 0);
	if (pid == -1 || !WIFEXITED(pstat) || WEXITSTATUS(pstat) != 0)
		return -1;
	return 0;
}

#endif /* TERMINFO */
