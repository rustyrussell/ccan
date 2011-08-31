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

uint64_t tdb1_incompatible_hash(const void *key, size_t len, uint64_t seed, void *);

/* @} ******************************************************************/

/* wipe and repack */
int tdb1_wipe_all(struct tdb_context *tdb);
int tdb1_repack(struct tdb_context *tdb);

extern TDB_DATA tdb1_null;

#endif /* tdb1.h */
