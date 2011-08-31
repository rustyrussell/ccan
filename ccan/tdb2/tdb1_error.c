 /*
   Unix SMB/CIFS implementation.

   trivial database library

   Copyright (C) Andrew Tridgell              1999-2005
   Copyright (C) Paul `Rusty' Russell		   2000
   Copyright (C) Jeremy Allison			   2000-2003

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

#include "tdb1_private.h"

_PUBLIC_ enum TDB1_ERROR tdb1_error(struct tdb1_context *tdb)
{
	return tdb->ecode;
}

static struct tdb1_errname {
	enum TDB1_ERROR ecode; const char *estring;
} emap[] = { {TDB1_SUCCESS, "Success"},
	     {TDB1_ERR_CORRUPT, "Corrupt database"},
	     {TDB1_ERR_IO, "IO Error"},
	     {TDB1_ERR_LOCK, "Locking error"},
	     {TDB1_ERR_OOM, "Out of memory"},
	     {TDB1_ERR_EXISTS, "Record exists"},
	     {TDB1_ERR_NOLOCK, "Lock exists on other keys"},
	     {TDB1_ERR_EINVAL, "Invalid parameter"},
	     {TDB1_ERR_NOEXIST, "Record does not exist"},
	     {TDB1_ERR_RDONLY, "write not permitted"} };

/* Error string for the last tdb error */
_PUBLIC_ const char *tdb1_errorstr(struct tdb1_context *tdb)
{
	uint32_t i;
	for (i = 0; i < sizeof(emap) / sizeof(struct tdb1_errname); i++)
		if (tdb->ecode == emap[i].ecode)
			return emap[i].estring;
	return "Invalid error code";
}
