#ifndef TDB1_H
#define TDB1_H

/*
   Unix SMB/CIFS implementation.

   trivial database library (version 1 compat functions)

   Copyright (C) Andrew Tridgell 1999-2004
   Copyright (C) Rusty Russell 2011

     ** NOTE! The following LGPL license applies to the tdb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#include "tdb2.h"

#ifndef _SAMBA_BUILD_
/* For mode_t */
#include <sys/types.h>
/* For O_* flags. */
#include <sys/stat.h>
#endif


void tdb1_set_max_dead(struct tdb_context *tdb, int max_dead);

int tdb1_parse_record(struct tdb_context *tdb, TDB_DATA key,
			      int (*parser)(TDB_DATA key, TDB_DATA data,
					    void *private_data),
			      void *private_data);

TDB_DATA tdb1_firstkey(struct tdb_context *tdb);

TDB_DATA tdb1_nextkey(struct tdb_context *tdb, TDB_DATA key);

int tdb1_lockall(struct tdb_context *tdb);

int tdb1_unlockall(struct tdb_context *tdb);

int tdb1_lockall_read(struct tdb_context *tdb);

int tdb1_unlockall_read(struct tdb_context *tdb);

int tdb1_transaction_start(struct tdb_context *tdb);

int tdb1_transaction_prepare_commit(struct tdb_context *tdb);

int tdb1_transaction_commit(struct tdb_context *tdb);

int tdb1_get_seqnum(struct tdb_context *tdb);

void tdb1_increment_seqnum_nonblock(struct tdb_context *tdb);

uint64_t tdb1_incompatible_hash(const void *key, size_t len, uint64_t seed, void *);

int tdb1_check(struct tdb_context *tdb,
	      int (*check) (TDB_DATA key, TDB_DATA data, void *private_data),
	      void *private_data);

/* @} ******************************************************************/

/* Low level locking functions: use with care */
int tdb1_chainlock(struct tdb_context *tdb, TDB_DATA key);
int tdb1_chainunlock(struct tdb_context *tdb, TDB_DATA key);
int tdb1_chainlock_read(struct tdb_context *tdb, TDB_DATA key);
int tdb1_chainunlock_read(struct tdb_context *tdb, TDB_DATA key);


/* wipe and repack */
int tdb1_wipe_all(struct tdb_context *tdb);
int tdb1_repack(struct tdb_context *tdb);

/* Debug functions. Not used in production. */
char *tdb1_summary(struct tdb_context *tdb);

extern TDB_DATA tdb1_null;

#endif /* tdb1.h */
