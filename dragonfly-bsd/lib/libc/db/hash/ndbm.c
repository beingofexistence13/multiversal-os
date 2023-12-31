/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
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
 * @(#)ndbm.c	8.4 (Berkeley) 7/21/94
 * $FreeBSD: head/lib/libc/db/hash/ndbm.c 165903 2007-01-09 00:28:16Z imp $
 */

/*
 * This package provides a dbm compatible interface to the new hashing
 * package described in db(3).
 */

#include "namespace.h"
#include <sys/param.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <ndbm.h>
#include "hash.h"

/*
 * Returns:
 * 	*DBM on success
 *	 NULL on failure
 */
DBM *
dbm_open(const char *file, int flags, mode_t mode)
{
	HASHINFO info;
	char path[MAXPATHLEN];
	DBM *db;

	info.bsize = 4096;
	info.ffactor = 40;
	info.nelem = 1;
	info.cachesize = 0;
	info.hash = NULL;
	info.lorder = 0;

	if( strlen(file) >= sizeof(path) - strlen(DBM_SUFFIX)) {
		errno = ENAMETOOLONG;
		return(NULL);
	}
	strcpy(path, file);
	strcat(path, DBM_SUFFIX);
	db = ((DBM *)__hash_open(path, flags, mode, &info, 0));
	if (db)
		pthread_mutex_init((void *)&db->mutex, NULL);

	return db;
}

void
dbm_close(DBM *db)
{
	if (db)
		pthread_mutex_destroy((void *)&db->mutex);
	(db->close)(db);
}

/*
 * Returns:
 *	DATUM on success
 *	NULL on failure
 */
datum
dbm_fetch(DBM *db, datum key)
{
	datum retdata;
	int status;
	DBT dbtkey, dbtretdata;

	pthread_mutex_lock((void *)&db->mutex);
	dbtkey.data = key.dptr;
	dbtkey.size = key.dsize;
	status = (db->get)(db, &dbtkey, &dbtretdata, 0);
	if (status) {
		dbtretdata.data = NULL;
		dbtretdata.size = 0;
	}
	retdata.dptr = dbtretdata.data;
	retdata.dsize = dbtretdata.size;
	pthread_mutex_unlock((void *)&db->mutex);

	return (retdata);
}

/*
 * Returns:
 *	DATUM on success
 *	NULL on failure
 */
datum
dbm_firstkey(DBM *db)
{
	int status;
	datum retkey;
	DBT dbtretkey, dbtretdata;

	pthread_mutex_lock((void *)&db->mutex);
	status = (db->seq)(db, &dbtretkey, &dbtretdata, R_FIRST);
	if (status)
		dbtretkey.data = NULL;
	retkey.dptr = dbtretkey.data;
	retkey.dsize = dbtretkey.size;
	pthread_mutex_unlock((void *)&db->mutex);

	return (retkey);
}

/*
 * Returns:
 *	DATUM on success
 *	NULL on failure
 */
datum
dbm_nextkey(DBM *db)
{
	int status;
	datum retkey;
	DBT dbtretkey, dbtretdata;

	pthread_mutex_lock((void *)&db->mutex);
	status = (db->seq)(db, &dbtretkey, &dbtretdata, R_NEXT);
	if (status)
		dbtretkey.data = NULL;
	retkey.dptr = dbtretkey.data;
	retkey.dsize = dbtretkey.size;
	pthread_mutex_unlock((void *)&db->mutex);

	return (retkey);
}

/*
 * Returns:
 *	 0 on success
 *	<0 failure
 */
int
dbm_delete(DBM *db, datum key)
{
	int status;
	DBT dbtkey;

	pthread_mutex_lock((void *)&db->mutex);
	dbtkey.data = key.dptr;
	dbtkey.size = key.dsize;
	sigblockall();
	status = (db->del)(db, &dbtkey, 0);
	sigunblockall();
	pthread_mutex_unlock((void *)&db->mutex);

	if (status)
		return (-1);
	else
		return (0);
}

/*
 * Returns:
 *	 0 on success
 *	<0 failure
 *	 1 if DBM_INSERT and entry exists
 */
int
dbm_store(DBM *db, datum key, datum data, int flags)
{
	DBT dbtkey, dbtdata;
	int status;

	pthread_mutex_lock((void *)&db->mutex);
	dbtkey.data = key.dptr;
	dbtkey.size = key.dsize;
	dbtdata.data = data.dptr;
	dbtdata.size = data.dsize;
	sigblockall();
	status = ((db->put)(db, &dbtkey, &dbtdata,
			    (flags == DBM_INSERT) ? R_NOOVERWRITE : 0));
	sigunblockall();
	pthread_mutex_unlock((void *)&db->mutex);

	return status;
}

/*
 * XXX not thread safe
 */
int
dbm_error(DBM *db)
{
	HTAB *hp;

	hp = (HTAB *)db->internal;

	return (hp->error);
}

/*
 * XXX not thread safe
 */
int
dbm_clearerr(DBM *db)
{
	HTAB *hp;

	hp = (HTAB *)db->internal;
	hp->error = 0;

	return (0);
}

int
dbm_dirfno(DBM *db)
{
	return(((HTAB *)db->internal)->fp);
}
