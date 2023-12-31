/*-
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
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
 *
 * @(#)print.c	8.4 (Berkeley) 4/17/94
 * $FreeBSD: src/bin/ls/print.c,v 1.73 2004/06/08 09:27:42 das Exp $
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <langinfo.h>
#include <libutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>
#ifdef COLORLS
#include <ctype.h>
#include <termcap.h>
#include <signal.h>
#endif

#include "ls.h"
#include "extern.h"

static int	printaname(const FTSENT *, u_long, u_long);
static void	printlink(const FTSENT *);
static void	printtime(const struct timespec);
static int	printtype(u_int);
static void	printsize(size_t, off_t);
#ifdef COLORLS
static void	endcolor(int);
static int	colortype(mode_t);
#endif

#define	IS_NOPRINT(p)	((p)->fts_number == NO_PRINT)

#ifdef COLORLS
/* Most of these are taken from <sys/stat.h> */
typedef enum Colors {
	C_DIR,			/* directory */
	C_LNK,			/* symbolic link */
	C_SOCK,			/* socket */
	C_FIFO,			/* pipe */
	C_EXEC,			/* executable */
	C_BLK,			/* block special */
	C_CHR,			/* character special */
	C_SUID,			/* setuid executable */
	C_SGID,			/* setgid executable */
	C_WSDIR,		/* directory writeble to others, with sticky
				 * bit */
	C_WDIR,			/* directory writeble to others, without
				 * sticky bit */
	C_NUMCOLORS		/* just a place-holder */
} Colors;

static const char *defcolors = "exfxcxdxbxegedabagacad";

/* colors for file types */
static struct {
	int	num[2];
	int	bold;
} colors[C_NUMCOLORS];
#endif

void
printscol(const DISPLAY *dp)
{
	FTSENT *p;

	for (p = dp->list; p; p = p->fts_link) {
		if (IS_NOPRINT(p))
			continue;
		printaname(p, dp->s_inode, dp->s_block);
		putchar('\n');
	}
}

/*
 * print name in current style
 */
int
printname(const char *name)
{
	if (f_octal || f_octal_escape)
		return prn_octal(name);
	else if (f_nonprint)
		return prn_printable(name);
	else
		return prn_normal(name);
}

void
printlong(const DISPLAY *dp)
{
	struct stat *sp;
	FTSENT *p;
	NAMES *np;
	char buf[20];
#ifdef COLORLS
	int color_printed = 0;
#endif

	if ((dp->list == NULL || dp->list->fts_level != FTS_ROOTLEVEL) &&
	    (f_longform || f_size)) {
		printf("total %lu\n", howmany(dp->btotal, blocksize));
	}

	for (p = dp->list; p; p = p->fts_link) {
		if (IS_NOPRINT(p))
			continue;
		sp = p->fts_statp;
		if (f_inode)
			printf("%*ju ", dp->s_inode, (uintmax_t)sp->st_ino);
		if (f_size)
			printf("%*jd ",
			    dp->s_block, howmany(sp->st_blocks, blocksize));
		strmode(sp->st_mode, buf);
		np = p->fts_pointer;
		printf("%s %*u %-*s  %-*s  ", buf, dp->s_nlink,
		    sp->st_nlink, dp->s_user, np->user, dp->s_group,
		    np->group);
		if (f_flags)
			printf("%-*s ", dp->s_flags, np->flags);
		if (S_ISCHR(sp->st_mode) || S_ISBLK(sp->st_mode))
			printf("%3d, 0x%08x ",
			       major(sp->st_rdev), (u_int)minor(sp->st_rdev));
		else if (dp->bcfile)
			printf("%*s%*jd ",
			    8 - dp->s_size, "", dp->s_size, (intmax_t)sp->st_size);
		else
			printsize(dp->s_size, sp->st_size);
		if (f_accesstime)
			printtime(sp->st_atim);
		else if (f_statustime)
			printtime(sp->st_ctim);
		else
			printtime(sp->st_mtim);
		putchar(' ');
#ifdef COLORLS
		if (f_color)
			color_printed = colortype(sp->st_mode);
#endif
		printname(p->fts_name);
#ifdef COLORLS
		if (f_color && color_printed)
			endcolor(0);
#endif
		if (f_type)
			printtype(sp->st_mode);
		if (S_ISLNK(sp->st_mode))
			printlink(p);
		putchar('\n');
	}
}

void
printstream(const DISPLAY *dp)
{
	FTSENT *p;
	int chcnt;

	for (p = dp->list, chcnt = 0; p; p = p->fts_link) {
		if (p->fts_number == NO_PRINT)
			continue;
		/* XXX strlen does not take octal escapes into account. */
		if (strlen(p->fts_name) + chcnt +
		    (p->fts_link ? 2 : 0) >= (unsigned)termwidth) {
			putchar('\n');
			chcnt = 0;
		}
		chcnt += printaname(p, dp->s_inode, dp->s_block);
		if (p->fts_link) {
			printf(", ");
			chcnt += 2;
		}
	}
	if (chcnt)
		putchar('\n');
}

void
printcol(const DISPLAY *dp)
{
	static FTSENT **array;
	static int lastentries = -1;
	FTSENT *p;
	FTSENT **narray;
	int base;
	int chcnt;
	int cnt;
	int col;
	int colwidth;
	int endcol;
	int num;
	int numcols;
	int numrows;
	int row;
	int tabwidth;

	if (f_notabs)
		tabwidth = 1;
	else
		tabwidth = 8;

	/*
	 * Have to do random access in the linked list -- build a table
	 * of pointers.
	 */
	if (dp->entries > lastentries) {
		lastentries = dp->entries;
		if ((narray =
		    realloc(array, dp->entries * sizeof(FTSENT *))) == NULL) {
			warn(NULL);
			printscol(dp);
			return;
		}
		lastentries = dp->entries;
		array = narray;
	}
	for (p = dp->list, num = 0; p; p = p->fts_link)
		if (p->fts_number != NO_PRINT)
			array[num++] = p;

	colwidth = dp->maxlen;
	if (f_inode)
		colwidth += dp->s_inode + 1;
	if (f_size)
		colwidth += dp->s_block + 1;
	if (f_type)
		colwidth += 1;

	colwidth = rounddown2(colwidth + tabwidth, tabwidth);
	if (termwidth < 2 * colwidth) {
		printscol(dp);
		return;
	}
	numcols = termwidth / colwidth;
	numrows = num / numcols;
	if (num % numcols)
		++numrows;

	if ((dp->list == NULL || dp->list->fts_level != FTS_ROOTLEVEL) &&
	    (f_longform || f_size)) {
		printf("total %lu\n", howmany(dp->btotal, blocksize));
	}

	/* counter if f_sortacross, else case-by-case */
	base = 0;

	for (row = 0; row < numrows; ++row) {
		endcol = colwidth;
		if (!f_sortacross)
			base = row;
		for (col = 0, chcnt = 0; col < numcols; ++col) {
			chcnt += printaname(array[base], dp->s_inode,
			    dp->s_block);
			if (f_sortacross)
				base++;
			else
				base += numrows;
			if (base >= num)
				break;
			while ((cnt = (rounddown2(chcnt + tabwidth, tabwidth)))
			    <= endcol) {
				if (f_sortacross && col + 1 >= numcols)
					break;
				putchar(f_notabs ? ' ' : '\t');
				chcnt = cnt;
			}
			endcol += colwidth;
		}
		putchar('\n');
	}
}

/*
 * print [inode] [size] name
 * return # of characters printed, no trailing characters.
 */
static int
printaname(const FTSENT *p, u_long inodefield, u_long sizefield)
{
	struct stat *sp;
	int chcnt;
#ifdef COLORLS
	int color_printed = 0;
#endif

	sp = p->fts_statp;
	chcnt = 0;
	if (f_inode) {
		chcnt += printf("%*ju ", (int)inodefield,
				(uintmax_t)sp->st_ino);
	}
	if (f_size)
		chcnt += printf("%*jd ",
		    (int)sizefield, howmany(sp->st_blocks, blocksize));
#ifdef COLORLS
	if (f_color)
		color_printed = colortype(sp->st_mode);
#endif
	chcnt += printname(p->fts_name);
#ifdef COLORLS
	if (f_color && color_printed)
		endcolor(0);
#endif
	if (f_type)
		chcnt += printtype(sp->st_mode);
	return (chcnt);
}

static void
printtime(const struct timespec stt)
{
	char longstring[80];
	static time_t now;
	const struct tm *ptime;
	const char *format;
	static char *lc_time;
	static int posix_time;

#define	SIXMONTHS	((365 / 2) * 86400)

	if (now == 0)
		now = time(NULL);

	if (lc_time == NULL) {
		lc_time = setlocale(LC_TIME, NULL);
		posix_time = (strcmp(lc_time, "C") == 0);
	}

	if (f_timeformat) {
		/*
		 * User specified format
		 */
		format = f_timeformat;
	} else if (f_sectime) {
		/*
		 * POSIX: Not covered by standard.  If C/POSIX locale used
		 *        the old convention of mmm dd hh:mm:ss yyyy is kept.
		 * Named locales use the extended ISO 8601 format without T:
		 *        YYYY-DD-MM hh:mm:ss
		 */
		format = posix_time ? "%b %e %T %Y" : "%F %T";
	} else if (posix_time) {
		/*
		 * POSIX: Future or older than 6 months returns equivalent of
		 *        date "+%b %e  %Y"
		 *        If file was modified within the past 6 months,
		 *        return equivalent of: date "+%b %e %H:%M"
		 */
		if ((stt.tv_sec > now) || (stt.tv_sec < now - SIXMONTHS))
			format = "%b %e  %Y";
		else
			format = "%b %e %R";
	} else {
		/*
		 * Named locales use format DD-MMM-YYYY hh:mm
		 * Regardless of locale, English is used for month field
		 */
		format = "%d-%b-%Y %H:%M";
	}

	/*
	 * If printing the nanotime field, always use GMT time
	 */
	if (f_nanotime)
		ptime = gmtime(&(stt.tv_sec));
	else
		ptime = localtime(&(stt.tv_sec));

	strftime_l(longstring, sizeof(longstring), format, ptime, NULL);
	fputs(longstring, stdout);
	if (f_nanotime)
		printf(".%09ju", stt.tv_nsec);
}

static int
printtype(u_int mode)
{

	if (f_slash) {
		if ((mode & S_IFMT) == S_IFDIR) {
			putchar('/');
			return (1);
		}
		return (0);
	}

	switch (mode & S_IFMT) {
	case S_IFDIR:
		putchar('/');
		return (1);
	case S_IFIFO:
		putchar('|');
		return (1);
	case S_IFLNK:
		putchar('@');
		return (1);
	case S_IFSOCK:
		putchar('=');
		return (1);
	case S_IFWHT:
		putchar('%');
		return (1);
	default:
		break;
	}
	if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
		putchar('*');
		return (1);
	}
	return (0);
}

#ifdef COLORLS
static int
putch(int c)
{
	putchar(c);
	return 0;
}

static int
writech(int c)
{
	char tmp = (char)c;

	write(STDOUT_FILENO, &tmp, 1);
	return 0;
}

static void
printcolor(Colors c)
{
	char *ansiseq;

	if (colors[c].bold)
		tputs(enter_bold, 1, putch);

	if (colors[c].num[0] != -1) {
		ansiseq = tgoto(ansi_fgcol, 0, colors[c].num[0]);
		if (ansiseq)
			tputs(ansiseq, 1, putch);
	}
	if (colors[c].num[1] != -1) {
		ansiseq = tgoto(ansi_bgcol, 0, colors[c].num[1]);
		if (ansiseq)
			tputs(ansiseq, 1, putch);
	}
}

static void
endcolor(int sig)
{
	tputs(ansi_coloff, 1, sig ? writech : putch);
	tputs(attrs_off, 1, sig ? writech : putch);
}

static int
colortype(mode_t mode)
{
	switch (mode & S_IFMT) {
	case S_IFDIR:
		if (mode & S_IWOTH)
			if (mode & S_ISTXT)
				printcolor(C_WSDIR);
			else
				printcolor(C_WDIR);
		else
			printcolor(C_DIR);
		return (1);
	case S_IFLNK:
		printcolor(C_LNK);
		return (1);
	case S_IFSOCK:
		printcolor(C_SOCK);
		return (1);
	case S_IFIFO:
		printcolor(C_FIFO);
		return (1);
	case S_IFBLK:
		printcolor(C_BLK);
		return (1);
	case S_IFCHR:
		printcolor(C_CHR);
		return (1);
	default:;
	}
	if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
		if (mode & S_ISUID)
			printcolor(C_SUID);
		else if (mode & S_ISGID)
			printcolor(C_SGID);
		else
			printcolor(C_EXEC);
		return (1);
	}
	return (0);
}

void
parsecolors(const char *cs)
{
	int i;
	int j;
	size_t len;
	char c[2];
	short legacy_warn = 0;

	if (cs == NULL)
		cs = "";	/* LSCOLORS not set */
	len = strlen(cs);
	for (i = 0; i < C_NUMCOLORS; i++) {
		colors[i].bold = 0;

		if (len <= 2 * (size_t)i) {
			c[0] = defcolors[2 * i];
			c[1] = defcolors[2 * i + 1];
		} else {
			c[0] = cs[2 * i];
			c[1] = cs[2 * i + 1];
		}
		for (j = 0; j < 2; j++) {
			/* Legacy colours used 0-7 */
			if (c[j] >= '0' && c[j] <= '7') {
				colors[i].num[j] = c[j] - '0';
				if (!legacy_warn) {
					warnx("LSCOLORS should use "
					      "characters a-h instead of 0-9 ("
					      "see the manual page)\n");
				}
				legacy_warn = 1;
			} else if (c[j] >= 'a' && c[j] <= 'h')
				colors[i].num[j] = c[j] - 'a';
			else if (c[j] >= 'A' && c[j] <= 'H') {
				colors[i].num[j] = c[j] - 'A';
				colors[i].bold = 1;
			} else if (tolower((unsigned char)c[j]) == 'x')
				colors[i].num[j] = -1;
			else {
				warnx("invalid character '%c' in LSCOLORS"
				      " env var\n", c[j]);
				colors[i].num[j] = -1;
			}
		}
	}
}

void
colorquit(int sig)
{
	endcolor(sig);

	signal(sig, SIG_DFL);
	kill(getpid(), sig);
}

#endif /* COLORLS */

static void
printlink(const FTSENT *p)
{
	int lnklen;
	char name[MAXPATHLEN + 1];
	char path[MAXPATHLEN + 1];

	if (p->fts_level == FTS_ROOTLEVEL)
		snprintf(name, sizeof(name), "%s", p->fts_name);
	else
		snprintf(name, sizeof(name),
		    "%s/%s", p->fts_parent->fts_accpath, p->fts_name);
	if ((lnklen = readlink(name, path, sizeof(path) - 1)) == -1) {
		fprintf(stderr, "\nls: %s: %s\n", name, strerror(errno));
		return;
	}
	path[lnklen] = '\0';
	printf(" -> ");
	printname(path);
}

static void
printsize(size_t width, off_t bytes)
{
	if (f_humanval) {
		char buf[5];

		humanize_number(buf, sizeof(buf), (int64_t)bytes, "",
				HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);
		printf("%5s ", buf);
	} else
		printf("%*jd ", (int)width, (uintmax_t)bytes);
}
