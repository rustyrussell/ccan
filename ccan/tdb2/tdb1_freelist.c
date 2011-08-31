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

/* read a freelist record and check for simple errors */
int tdb1_rec_free_read(struct tdb_context *tdb, tdb1_off_t off, struct tdb1_record *rec)
{
	if (tdb->tdb1.io->tdb1_read(tdb, off, rec, sizeof(*rec),TDB1_DOCONV()) == -1)
		return -1;

	if (rec->magic == TDB1_MAGIC) {
		/* this happens when a app is showdown while deleting a record - we should
		   not completely fail when this happens */
		tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_WARNING,
			   "tdb1_rec_free_read non-free magic 0x%x at offset=%d - fixing\n",
			   rec->magic, off);
		rec->magic = TDB1_FREE_MAGIC;
		if (tdb->tdb1.io->tdb1_write(tdb, off, rec, sizeof(*rec)) == -1)
			return -1;
	}

	if (rec->magic != TDB1_FREE_MAGIC) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
					"tdb1_rec_free_read bad magic 0x%x at offset=%d\n",
					rec->magic, off);
		return -1;
	}
	if (tdb->tdb1.io->tdb1_oob(tdb, rec->next+sizeof(*rec), 0) != 0)
		return -1;
	return 0;
}


/* update a record tailer (must hold allocation lock) */
static int update_tailer(struct tdb_context *tdb, tdb1_off_t offset,
			 const struct tdb1_record *rec)
{
	tdb1_off_t totalsize;

	/* Offset of tailer from record header */
	totalsize = sizeof(*rec) + rec->rec_len;
	return tdb1_ofs_write(tdb, offset + totalsize - sizeof(tdb1_off_t),
			 &totalsize);
}

/* Add an element into the freelist. Merge adjacent records if
   necessary. */
int tdb1_free(struct tdb_context *tdb, tdb1_off_t offset, struct tdb1_record *rec)
{
	/* Allocation and tailer lock */
	if (tdb1_lock(tdb, -1, F_WRLCK) != 0)
		return -1;

	/* set an initial tailer, so if we fail we don't leave a bogus record */
	if (update_tailer(tdb, offset, rec) != 0) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb_free: update_tailer failed!\n");
		goto fail;
	}

	/* Look left */
	if (offset - sizeof(tdb1_off_t) > TDB1_DATA_START(tdb->tdb1.header.hash_size)) {
		tdb1_off_t left = offset - sizeof(tdb1_off_t);
		struct tdb1_record l;
		tdb1_off_t leftsize;

		/* Read in tailer and jump back to header */
		if (tdb1_ofs_read(tdb, left, &leftsize) == -1) {
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_free: left offset read failed at %u", left);
			goto update;
		}

		/* it could be uninitialised data */
		if (leftsize == 0 || leftsize == TDB1_PAD_U32) {
			goto update;
		}

		left = offset - leftsize;

		if (leftsize > offset ||
		    left < TDB1_DATA_START(tdb->tdb1.header.hash_size)) {
			goto update;
		}

		/* Now read in the left record */
		if (tdb->tdb1.io->tdb1_read(tdb, left, &l, sizeof(l), TDB1_DOCONV()) == -1) {
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_free: left read failed at %u (%u)", left, leftsize);
			goto update;
		}

		/* If it's free, expand to include it. */
		if (l.magic == TDB1_FREE_MAGIC) {
			/* we now merge the new record into the left record, rather than the other
			   way around. This makes the operation O(1) instead of O(n). This change
			   prevents traverse from being O(n^2) after a lot of deletes */
			l.rec_len += sizeof(*rec) + rec->rec_len;
			if (tdb1_rec_write(tdb, left, &l) == -1) {
				tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
					   "tdb1_free: update_left failed at %u", left);
				goto fail;
			}
			if (update_tailer(tdb, left, &l) == -1) {
				tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
					   "tdb1_free: update_tailer failed at %u", offset);
				goto fail;
			}
			tdb1_unlock(tdb, -1, F_WRLCK);
			return 0;
		}
	}

update:

	/* Now, prepend to free list */
	rec->magic = TDB1_FREE_MAGIC;

	if (tdb1_ofs_read(tdb, TDB1_FREELIST_TOP, &rec->next) == -1 ||
	    tdb1_rec_write(tdb, offset, rec) == -1 ||
	    tdb1_ofs_write(tdb, TDB1_FREELIST_TOP, &offset) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_free record write failed at offset=%d",
			   offset);
		goto fail;
	}

	/* And we're done. */
	tdb1_unlock(tdb, -1, F_WRLCK);
	return 0;

 fail:
	tdb1_unlock(tdb, -1, F_WRLCK);
	return -1;
}



/*
   the core of tdb1_allocate - called when we have decided which
   free list entry to use

   Note that we try to allocate by grabbing data from the end of an existing record,
   not the beginning. This is so the left merge in a free is more likely to be
   able to free up the record without fragmentation
 */
static tdb1_off_t tdb1_allocate_ofs(struct tdb_context *tdb,
				  tdb1_len_t length, tdb1_off_t rec_ptr,
				  struct tdb1_record *rec, tdb1_off_t last_ptr)
{
#define MIN_REC_SIZE (sizeof(struct tdb1_record) + sizeof(tdb1_off_t) + 8)

	if (rec->rec_len < length + MIN_REC_SIZE) {
		/* we have to grab the whole record */

		/* unlink it from the previous record */
		if (tdb1_ofs_write(tdb, last_ptr, &rec->next) == -1) {
			return 0;
		}

		/* mark it not free */
		rec->magic = TDB1_MAGIC;
		if (tdb1_rec_write(tdb, rec_ptr, rec) == -1) {
			return 0;
		}
		return rec_ptr;
	}

	/* we're going to just shorten the existing record */
	rec->rec_len -= (length + sizeof(*rec));
	if (tdb1_rec_write(tdb, rec_ptr, rec) == -1) {
		return 0;
	}
	if (update_tailer(tdb, rec_ptr, rec) == -1) {
		return 0;
	}

	/* and setup the new record */
	rec_ptr += sizeof(*rec) + rec->rec_len;

	memset(rec, '\0', sizeof(*rec));
	rec->rec_len = length;
	rec->magic = TDB1_MAGIC;

	if (tdb1_rec_write(tdb, rec_ptr, rec) == -1) {
		return 0;
	}

	if (update_tailer(tdb, rec_ptr, rec) == -1) {
		return 0;
	}

	return rec_ptr;
}

/* allocate some space from the free list. The offset returned points
   to a unconnected tdb1_record within the database with room for at
   least length bytes of total data

   0 is returned if the space could not be allocated
 */
tdb1_off_t tdb1_allocate(struct tdb_context *tdb, tdb1_len_t length, struct tdb1_record *rec)
{
	tdb1_off_t rec_ptr, last_ptr, newrec_ptr;
	struct {
		tdb1_off_t rec_ptr, last_ptr;
		tdb1_len_t rec_len;
	} bestfit;
	float multiplier = 1.0;

	if (tdb1_lock(tdb, -1, F_WRLCK) == -1)
		return 0;

	/* over-allocate to reduce fragmentation */
	length *= 1.25;

	/* Extra bytes required for tailer */
	length += sizeof(tdb1_off_t);
	length = TDB1_ALIGN(length, TDB1_ALIGNMENT);

 again:
	last_ptr = TDB1_FREELIST_TOP;

	/* read in the freelist top */
	if (tdb1_ofs_read(tdb, TDB1_FREELIST_TOP, &rec_ptr) == -1)
		goto fail;

	bestfit.rec_ptr = 0;
	bestfit.last_ptr = 0;
	bestfit.rec_len = 0;

	/*
	   this is a best fit allocation strategy. Originally we used
	   a first fit strategy, but it suffered from massive fragmentation
	   issues when faced with a slowly increasing record size.
	 */
	while (rec_ptr) {
		if (tdb1_rec_free_read(tdb, rec_ptr, rec) == -1) {
			goto fail;
		}

		if (rec->rec_len >= length) {
			if (bestfit.rec_ptr == 0 ||
			    rec->rec_len < bestfit.rec_len) {
				bestfit.rec_len = rec->rec_len;
				bestfit.rec_ptr = rec_ptr;
				bestfit.last_ptr = last_ptr;
			}
		}

		/* move to the next record */
		last_ptr = rec_ptr;
		rec_ptr = rec->next;

		/* if we've found a record that is big enough, then
		   stop searching if its also not too big. The
		   definition of 'too big' changes as we scan
		   through */
		if (bestfit.rec_len > 0 &&
		    bestfit.rec_len < length * multiplier) {
			break;
		}

		/* this multiplier means we only extremely rarely
		   search more than 50 or so records. At 50 records we
		   accept records up to 11 times larger than what we
		   want */
		multiplier *= 1.05;
	}

	if (bestfit.rec_ptr != 0) {
		if (tdb1_rec_free_read(tdb, bestfit.rec_ptr, rec) == -1) {
			goto fail;
		}

		newrec_ptr = tdb1_allocate_ofs(tdb, length, bestfit.rec_ptr,
					      rec, bestfit.last_ptr);
		tdb1_unlock(tdb, -1, F_WRLCK);
		return newrec_ptr;
	}

	/* we didn't find enough space. See if we can expand the
	   database and if we can then try again */
	if (tdb1_expand(tdb, length + sizeof(*rec)) == 0)
		goto again;
 fail:
	tdb1_unlock(tdb, -1, F_WRLCK);
	return 0;
}
