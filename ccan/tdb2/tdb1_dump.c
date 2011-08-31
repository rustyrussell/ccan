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

static tdb1_off_t tdb1_dump_record(struct tdb1_context *tdb, int hash,
				 tdb1_off_t offset)
{
	struct tdb1_record rec;
	tdb1_off_t tailer_ofs, tailer;

	if (tdb->methods->tdb1_read(tdb, offset, (char *)&rec,
				   sizeof(rec), TDB1_DOCONV()) == -1) {
		printf("ERROR: failed to read record at %u\n", offset);
		return 0;
	}

	printf(" rec: hash=%d offset=0x%08x next=0x%08x rec_len=%d "
	       "key_len=%d data_len=%d full_hash=0x%x magic=0x%x\n",
	       hash, offset, rec.next, rec.rec_len, rec.key_len, rec.data_len,
	       rec.full_hash, rec.magic);

	tailer_ofs = offset + sizeof(rec) + rec.rec_len - sizeof(tdb1_off_t);

	if (tdb1_ofs_read(tdb, tailer_ofs, &tailer) == -1) {
		printf("ERROR: failed to read tailer at %u\n", tailer_ofs);
		return rec.next;
	}

	if (tailer != rec.rec_len + sizeof(rec)) {
		printf("ERROR: tailer does not match record! tailer=%u totalsize=%u\n",
				(unsigned int)tailer, (unsigned int)(rec.rec_len + sizeof(rec)));
	}
	return rec.next;
}

static int tdb1_dump_chain(struct tdb1_context *tdb, int i)
{
	tdb1_off_t rec_ptr, top;

	top = TDB1_HASH_TOP(i);

	if (tdb1_lock(tdb, i, F_WRLCK) != 0)
		return -1;

	if (tdb1_ofs_read(tdb, top, &rec_ptr) == -1)
		return tdb1_unlock(tdb, i, F_WRLCK);

	if (rec_ptr)
		printf("hash=%d\n", i);

	while (rec_ptr) {
		rec_ptr = tdb1_dump_record(tdb, i, rec_ptr);
	}

	return tdb1_unlock(tdb, i, F_WRLCK);
}

_PUBLIC_ void tdb1_dump_all(struct tdb1_context *tdb)
{
	int i;
	for (i=0;i<tdb->header.hash_size;i++) {
		tdb1_dump_chain(tdb, i);
	}
	printf("freelist:\n");
	tdb1_dump_chain(tdb, -1);
}

_PUBLIC_ int tdb1_printfreelist(struct tdb1_context *tdb)
{
	int ret;
	long total_free = 0;
	tdb1_off_t offset, rec_ptr;
	struct tdb1_record rec;

	if ((ret = tdb1_lock(tdb, -1, F_WRLCK)) != 0)
		return ret;

	offset = TDB1_FREELIST_TOP;

	/* read in the freelist top */
	if (tdb1_ofs_read(tdb, offset, &rec_ptr) == -1) {
		tdb1_unlock(tdb, -1, F_WRLCK);
		return 0;
	}

	printf("freelist top=[0x%08x]\n", rec_ptr );
	while (rec_ptr) {
		if (tdb->methods->tdb1_read(tdb, rec_ptr, (char *)&rec,
					   sizeof(rec), TDB1_DOCONV()) == -1) {
			tdb1_unlock(tdb, -1, F_WRLCK);
			return -1;
		}

		if (rec.magic != TDB1_FREE_MAGIC) {
			printf("bad magic 0x%08x in free list\n", rec.magic);
			tdb1_unlock(tdb, -1, F_WRLCK);
			return -1;
		}

		printf("entry offset=[0x%08x], rec.rec_len = [0x%08x (%d)] (end = 0x%08x)\n",
		       rec_ptr, rec.rec_len, rec.rec_len, rec_ptr + rec.rec_len);
		total_free += rec.rec_len;

		/* move to the next record */
		rec_ptr = rec.next;
	}
	printf("total rec_len = [0x%08x (%d)]\n", (int)total_free,
               (int)total_free);

	return tdb1_unlock(tdb, -1, F_WRLCK);
}
