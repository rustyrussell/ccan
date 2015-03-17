 /*
   Trivial Database 2: fetch, store and misc routines.
   Copyright (C) Rusty Russell 2010

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
#include "private.h"
#ifndef HAVE_LIBREPLACE
#include <stdarg.h>
#endif

static enum NTDB_ERROR update_rec_hdr(struct ntdb_context *ntdb,
				     ntdb_off_t off,
				     ntdb_len_t keylen,
				     ntdb_len_t datalen,
				     struct ntdb_used_record *rec)
{
	uint64_t dataroom = rec_data_length(rec) + rec_extra_padding(rec);
	enum NTDB_ERROR ecode;

	ecode = set_header(ntdb, rec, NTDB_USED_MAGIC, keylen, datalen,
			   keylen + dataroom);
	if (ecode == NTDB_SUCCESS) {
		ecode = ntdb_write_convert(ntdb, off, rec, sizeof(*rec));
	}
	return ecode;
}

static enum NTDB_ERROR replace_data(struct ntdb_context *ntdb,
				   struct hash_info *h,
				   NTDB_DATA key, NTDB_DATA dbuf,
				   ntdb_off_t old_off, ntdb_len_t old_room,
				   bool growing)
{
	ntdb_off_t new_off;
	enum NTDB_ERROR ecode;

	/* Allocate a new record. */
	new_off = alloc(ntdb, key.dsize, dbuf.dsize, NTDB_USED_MAGIC, growing);
	if (NTDB_OFF_IS_ERR(new_off)) {
		return NTDB_OFF_TO_ERR(new_off);
	}

	/* We didn't like the existing one: remove it. */
	if (old_off) {
		ntdb->stats.frees++;
		ecode = add_free_record(ntdb, old_off,
					sizeof(struct ntdb_used_record)
					+ key.dsize + old_room,
					NTDB_LOCK_WAIT, true);
		if (ecode == NTDB_SUCCESS)
			ecode = replace_in_hash(ntdb, h, new_off);
	} else {
		ecode = add_to_hash(ntdb, h, new_off);
	}
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	new_off += sizeof(struct ntdb_used_record);
	ecode = ntdb->io->twrite(ntdb, new_off, key.dptr, key.dsize);
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	new_off += key.dsize;
	ecode = ntdb->io->twrite(ntdb, new_off, dbuf.dptr, dbuf.dsize);
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	if (ntdb->flags & NTDB_SEQNUM)
		ntdb_inc_seqnum(ntdb);

	return NTDB_SUCCESS;
}

static enum NTDB_ERROR update_data(struct ntdb_context *ntdb,
				  ntdb_off_t off,
				  NTDB_DATA dbuf,
				  ntdb_len_t extra)
{
	enum NTDB_ERROR ecode;

	ecode = ntdb->io->twrite(ntdb, off, dbuf.dptr, dbuf.dsize);
	if (ecode == NTDB_SUCCESS && extra) {
		/* Put a zero in; future versions may append other data. */
		ecode = ntdb->io->twrite(ntdb, off + dbuf.dsize, "", 1);
	}
	if (ntdb->flags & NTDB_SEQNUM)
		ntdb_inc_seqnum(ntdb);

	return ecode;
}

_PUBLIC_ enum NTDB_ERROR ntdb_store(struct ntdb_context *ntdb,
			 NTDB_DATA key, NTDB_DATA dbuf, int flag)
{
	struct hash_info h;
	ntdb_off_t off;
	ntdb_len_t old_room = 0;
	struct ntdb_used_record rec;
	enum NTDB_ERROR ecode;

	off = find_and_lock(ntdb, key, F_WRLCK, &h, &rec, NULL);
	if (NTDB_OFF_IS_ERR(off)) {
		return NTDB_OFF_TO_ERR(off);
	}

	/* Now we have lock on this hash bucket. */
	if (flag == NTDB_INSERT) {
		if (off) {
			ecode = NTDB_ERR_EXISTS;
			goto out;
		}
	} else {
		if (off) {
			old_room = rec_data_length(&rec)
				+ rec_extra_padding(&rec);
			if (old_room >= dbuf.dsize) {
				/* Can modify in-place.  Easy! */
				ecode = update_rec_hdr(ntdb, off,
						       key.dsize, dbuf.dsize,
						       &rec);
				if (ecode != NTDB_SUCCESS) {
					goto out;
				}
				ecode = update_data(ntdb,
						    off + sizeof(rec)
						    + key.dsize, dbuf,
						    old_room - dbuf.dsize);
				if (ecode != NTDB_SUCCESS) {
					goto out;
				}
				ntdb_unlock_hash(ntdb, h.h, F_WRLCK);
				return NTDB_SUCCESS;
			}
		} else {
			if (flag == NTDB_MODIFY) {
				/* if the record doesn't exist and we
				   are in NTDB_MODIFY mode then we should fail
				   the store */
				ecode = NTDB_ERR_NOEXIST;
				goto out;
			}
		}
	}

	/* If we didn't use the old record, this implies we're growing. */
	ecode = replace_data(ntdb, &h, key, dbuf, off, old_room, off);
out:
	ntdb_unlock_hash(ntdb, h.h, F_WRLCK);
	return ecode;
}

_PUBLIC_ enum NTDB_ERROR ntdb_append(struct ntdb_context *ntdb,
			  NTDB_DATA key, NTDB_DATA dbuf)
{
	struct hash_info h;
	ntdb_off_t off;
	struct ntdb_used_record rec;
	ntdb_len_t old_room = 0, old_dlen;
	unsigned char *newdata;
	NTDB_DATA new_dbuf;
	enum NTDB_ERROR ecode;

	off = find_and_lock(ntdb, key, F_WRLCK, &h, &rec, NULL);
	if (NTDB_OFF_IS_ERR(off)) {
		return NTDB_OFF_TO_ERR(off);
	}

	if (off) {
		old_dlen = rec_data_length(&rec);
		old_room = old_dlen + rec_extra_padding(&rec);

		/* Fast path: can append in place. */
		if (rec_extra_padding(&rec) >= dbuf.dsize) {
			ecode = update_rec_hdr(ntdb, off, key.dsize,
					       old_dlen + dbuf.dsize, &rec);
			if (ecode != NTDB_SUCCESS) {
				goto out;
			}

			off += sizeof(rec) + key.dsize + old_dlen;
			ecode = update_data(ntdb, off, dbuf,
					    rec_extra_padding(&rec));
			goto out;
		}

		/* Slow path. */
		newdata = ntdb->alloc_fn(ntdb, key.dsize + old_dlen + dbuf.dsize,
				     ntdb->alloc_data);
		if (!newdata) {
			ecode = ntdb_logerr(ntdb, NTDB_ERR_OOM, NTDB_LOG_ERROR,
					   "ntdb_append:"
					   " failed to allocate %zu bytes",
					   (size_t)(key.dsize + old_dlen
						    + dbuf.dsize));
			goto out;
		}
		ecode = ntdb->io->tread(ntdb, off + sizeof(rec) + key.dsize,
				       newdata, old_dlen);
		if (ecode != NTDB_SUCCESS) {
			goto out_free_newdata;
		}
		memcpy(newdata + old_dlen, dbuf.dptr, dbuf.dsize);
		new_dbuf.dptr = newdata;
		new_dbuf.dsize = old_dlen + dbuf.dsize;
	} else {
		newdata = NULL;
		new_dbuf = dbuf;
	}

	/* If they're using ntdb_append(), it implies they're growing record. */
	ecode = replace_data(ntdb, &h, key, new_dbuf, off, old_room, true);

out_free_newdata:
	ntdb->free_fn(newdata, ntdb->alloc_data);
out:
	ntdb_unlock_hash(ntdb, h.h, F_WRLCK);
	return ecode;
}

_PUBLIC_ enum NTDB_ERROR ntdb_fetch(struct ntdb_context *ntdb, NTDB_DATA key,
				    NTDB_DATA *data)
{
	ntdb_off_t off;
	struct ntdb_used_record rec;
	struct hash_info h;
	enum NTDB_ERROR ecode;
	const char *keyp;

	off = find_and_lock(ntdb, key, F_RDLCK, &h, &rec, &keyp);
	if (NTDB_OFF_IS_ERR(off)) {
		return NTDB_OFF_TO_ERR(off);
	}

	if (!off) {
		ecode = NTDB_ERR_NOEXIST;
	} else {
		data->dsize = rec_data_length(&rec);
		data->dptr = ntdb->alloc_fn(ntdb, data->dsize, ntdb->alloc_data);
		if (unlikely(!data->dptr)) {
			ecode = NTDB_ERR_OOM;
		} else {
			memcpy(data->dptr, keyp + key.dsize, data->dsize);
			ecode = NTDB_SUCCESS;
		}
		ntdb_access_release(ntdb, keyp);
	}

	ntdb_unlock_hash(ntdb, h.h, F_RDLCK);
	return ecode;
}

_PUBLIC_ bool ntdb_exists(struct ntdb_context *ntdb, NTDB_DATA key)
{
	ntdb_off_t off;
	struct ntdb_used_record rec;
	struct hash_info h;

	off = find_and_lock(ntdb, key, F_RDLCK, &h, &rec, NULL);
	if (NTDB_OFF_IS_ERR(off)) {
		return false;
	}
	ntdb_unlock_hash(ntdb, h.h, F_RDLCK);

	return off ? true : false;
}

_PUBLIC_ enum NTDB_ERROR ntdb_delete(struct ntdb_context *ntdb, NTDB_DATA key)
{
	ntdb_off_t off;
	struct ntdb_used_record rec;
	struct hash_info h;
	enum NTDB_ERROR ecode;

	off = find_and_lock(ntdb, key, F_WRLCK, &h, &rec, NULL);
	if (NTDB_OFF_IS_ERR(off)) {
		return NTDB_OFF_TO_ERR(off);
	}

	if (!off) {
		ecode = NTDB_ERR_NOEXIST;
		goto unlock;
	}

	ecode = delete_from_hash(ntdb, &h);
	if (ecode != NTDB_SUCCESS) {
		goto unlock;
	}

	/* Free the deleted entry. */
	ntdb->stats.frees++;
	ecode = add_free_record(ntdb, off,
				sizeof(struct ntdb_used_record)
				+ rec_key_length(&rec)
				+ rec_data_length(&rec)
				+ rec_extra_padding(&rec),
				NTDB_LOCK_WAIT, true);

	if (ntdb->flags & NTDB_SEQNUM)
		ntdb_inc_seqnum(ntdb);

unlock:
	ntdb_unlock_hash(ntdb, h.h, F_WRLCK);
	return ecode;
}

_PUBLIC_ unsigned int ntdb_get_flags(struct ntdb_context *ntdb)
{
	return ntdb->flags;
}

static bool inside_transaction(const struct ntdb_context *ntdb)
{
	return ntdb->transaction != NULL;
}

static bool readonly_changable(struct ntdb_context *ntdb, const char *caller)
{
	if (inside_transaction(ntdb)) {
		ntdb_logerr(ntdb, NTDB_ERR_EINVAL, NTDB_LOG_USE_ERROR,
			    "%s: can't change"
			    " NTDB_RDONLY inside transaction",
			    caller);
		return false;
	}
	return true;
}

_PUBLIC_ void ntdb_add_flag(struct ntdb_context *ntdb, unsigned flag)
{
	if (ntdb->flags & NTDB_INTERNAL) {
		ntdb_logerr(ntdb, NTDB_ERR_EINVAL, NTDB_LOG_USE_ERROR,
			    "ntdb_add_flag: internal db");
		return;
	}
	switch (flag) {
	case NTDB_NOLOCK:
		ntdb->flags |= NTDB_NOLOCK;
		break;
	case NTDB_NOMMAP:
		if (ntdb->file->direct_count) {
			ntdb_logerr(ntdb, NTDB_ERR_EINVAL, NTDB_LOG_USE_ERROR,
				    "ntdb_add_flag: Can't get NTDB_NOMMAP from"
				    " ntdb_parse_record!");
			return;
		}
		ntdb->flags |= NTDB_NOMMAP;
#ifndef HAVE_INCOHERENT_MMAP
		ntdb_munmap(ntdb);
#endif
		break;
	case NTDB_NOSYNC:
		ntdb->flags |= NTDB_NOSYNC;
		break;
	case NTDB_SEQNUM:
		ntdb->flags |= NTDB_SEQNUM;
		break;
	case NTDB_ALLOW_NESTING:
		ntdb->flags |= NTDB_ALLOW_NESTING;
		break;
	case NTDB_RDONLY:
		if (readonly_changable(ntdb, "ntdb_add_flag"))
			ntdb->flags |= NTDB_RDONLY;
		break;
	default:
		ntdb_logerr(ntdb, NTDB_ERR_EINVAL, NTDB_LOG_USE_ERROR,
			    "ntdb_add_flag: Unknown flag %u", flag);
	}
}

_PUBLIC_ void ntdb_remove_flag(struct ntdb_context *ntdb, unsigned flag)
{
	if (ntdb->flags & NTDB_INTERNAL) {
		ntdb_logerr(ntdb, NTDB_ERR_EINVAL, NTDB_LOG_USE_ERROR,
			    "ntdb_remove_flag: internal db");
		return;
	}
	switch (flag) {
	case NTDB_NOLOCK:
		ntdb->flags &= ~NTDB_NOLOCK;
		break;
	case NTDB_NOMMAP:
		ntdb->flags &= ~NTDB_NOMMAP;
#ifndef HAVE_INCOHERENT_MMAP
		/* If mmap incoherent, we were mmaping anyway. */
		ntdb_mmap(ntdb);
#endif
		break;
	case NTDB_NOSYNC:
		ntdb->flags &= ~NTDB_NOSYNC;
		break;
	case NTDB_SEQNUM:
		ntdb->flags &= ~NTDB_SEQNUM;
		break;
	case NTDB_ALLOW_NESTING:
		ntdb->flags &= ~NTDB_ALLOW_NESTING;
		break;
	case NTDB_RDONLY:
		if ((ntdb->open_flags & O_ACCMODE) == O_RDONLY) {
			ntdb_logerr(ntdb, NTDB_ERR_EINVAL, NTDB_LOG_USE_ERROR,
				    "ntdb_remove_flag: can't"
				    " remove NTDB_RDONLY on ntdb"
				    " opened with O_RDONLY");
			break;
		}
		if (readonly_changable(ntdb, "ntdb_remove_flag"))
			ntdb->flags &= ~NTDB_RDONLY;
		break;
	default:
		ntdb_logerr(ntdb, NTDB_ERR_EINVAL, NTDB_LOG_USE_ERROR,
			    "ntdb_remove_flag: Unknown flag %u",
			    flag);
	}
}

_PUBLIC_ const char *ntdb_errorstr(enum NTDB_ERROR ecode)
{
	/* Gcc warns if you miss a case in the switch, so use that. */
	switch (NTDB_ERR_TO_OFF(ecode)) {
	case NTDB_ERR_TO_OFF(NTDB_SUCCESS): return "Success";
	case NTDB_ERR_TO_OFF(NTDB_ERR_CORRUPT): return "Corrupt database";
	case NTDB_ERR_TO_OFF(NTDB_ERR_IO): return "IO Error";
	case NTDB_ERR_TO_OFF(NTDB_ERR_LOCK): return "Locking error";
	case NTDB_ERR_TO_OFF(NTDB_ERR_OOM): return "Out of memory";
	case NTDB_ERR_TO_OFF(NTDB_ERR_EXISTS): return "Record exists";
	case NTDB_ERR_TO_OFF(NTDB_ERR_EINVAL): return "Invalid parameter";
	case NTDB_ERR_TO_OFF(NTDB_ERR_NOEXIST): return "Record does not exist";
	case NTDB_ERR_TO_OFF(NTDB_ERR_RDONLY): return "write not permitted";
	}
	return "Invalid error code";
}

enum NTDB_ERROR COLD ntdb_logerr(struct ntdb_context *ntdb,
			       enum NTDB_ERROR ecode,
			       enum ntdb_log_level level,
			       const char *fmt, ...)
{
	char *message;
	va_list ap;
	size_t len;
	/* ntdb_open paths care about errno, so save it. */
	int saved_errno = errno;

	if (!ntdb->log_fn)
		return ecode;

	va_start(ap, fmt);
	len = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	message = ntdb->alloc_fn(ntdb, len + 1, ntdb->alloc_data);
	if (!message) {
		ntdb->log_fn(ntdb, NTDB_LOG_ERROR, NTDB_ERR_OOM,
			    "out of memory formatting message:", ntdb->log_data);
		ntdb->log_fn(ntdb, level, ecode, fmt, ntdb->log_data);
	} else {
		va_start(ap, fmt);
		vsnprintf(message, len+1, fmt, ap);
		va_end(ap);
		ntdb->log_fn(ntdb, level, ecode, message, ntdb->log_data);
		ntdb->free_fn(message, ntdb->alloc_data);
	}
	errno = saved_errno;
	return ecode;
}

_PUBLIC_ enum NTDB_ERROR ntdb_parse_record_(struct ntdb_context *ntdb,
				 NTDB_DATA key,
				 enum NTDB_ERROR (*parse)(NTDB_DATA k,
							 NTDB_DATA d,
							 void *data),
				 void *data)
{
	ntdb_off_t off;
	struct ntdb_used_record rec;
	struct hash_info h;
	enum NTDB_ERROR ecode;
	const char *keyp;

	off = find_and_lock(ntdb, key, F_RDLCK, &h, &rec, &keyp);
	if (NTDB_OFF_IS_ERR(off)) {
		return NTDB_OFF_TO_ERR(off);
	}

	if (!off) {
		ecode = NTDB_ERR_NOEXIST;
	} else {
		unsigned int old_flags;
		NTDB_DATA d = ntdb_mkdata(keyp + key.dsize,
					  rec_data_length(&rec));

		/*
		 * Make sure they don't try to write db, since they
		 * have read lock!  They can if they've done
		 * ntdb_lockall(): if it was ntdb_lockall_read, that'll
		 * stop them doing a write operation anyway.
		 */
		old_flags = ntdb->flags;
		if (!ntdb->file->allrecord_lock.count &&
		    !(ntdb->flags & NTDB_NOLOCK)) {
			ntdb->flags |= NTDB_RDONLY;
		}
		ecode = parse(key, d, data);
		ntdb->flags = old_flags;
		ntdb_access_release(ntdb, keyp);
	}

	ntdb_unlock_hash(ntdb, h.h, F_RDLCK);
	return ecode;
}

_PUBLIC_ const char *ntdb_name(const struct ntdb_context *ntdb)
{
	return ntdb->name;
}

_PUBLIC_ int64_t ntdb_get_seqnum(struct ntdb_context *ntdb)
{
	return ntdb_read_off(ntdb, offsetof(struct ntdb_header, seqnum));
}


_PUBLIC_ int ntdb_fd(const struct ntdb_context *ntdb)
{
	return ntdb->file->fd;
}

struct traverse_state {
	enum NTDB_ERROR error;
	struct ntdb_context *dest_db;
};

/*
  traverse function for repacking
 */
static int repack_traverse(struct ntdb_context *ntdb, NTDB_DATA key, NTDB_DATA data,
			   struct traverse_state *state)
{
	state->error = ntdb_store(state->dest_db, key, data, NTDB_INSERT);
	if (state->error != NTDB_SUCCESS) {
		return -1;
	}
	return 0;
}

_PUBLIC_ enum NTDB_ERROR ntdb_repack(struct ntdb_context *ntdb)
{
	struct ntdb_context *tmp_db;
	struct traverse_state state;

	state.error = ntdb_transaction_start(ntdb);
	if (state.error != NTDB_SUCCESS) {
		return state.error;
	}

	tmp_db = ntdb_open("tmpdb", NTDB_INTERNAL, O_RDWR|O_CREAT, 0, NULL);
	if (tmp_db == NULL) {
		state.error = ntdb_logerr(ntdb, NTDB_ERR_OOM, NTDB_LOG_ERROR,
					 __location__
					 " Failed to create tmp_db");
		ntdb_transaction_cancel(ntdb);
		return state.error;
	}

	state.dest_db = tmp_db;
	if (ntdb_traverse(ntdb, repack_traverse, &state) < 0) {
		goto fail;
	}

	state.error = ntdb_wipe_all(ntdb);
	if (state.error != NTDB_SUCCESS) {
		goto fail;
	}

	state.dest_db = ntdb;
	if (ntdb_traverse(tmp_db, repack_traverse, &state) < 0) {
		goto fail;
	}

	ntdb_close(tmp_db);
	return ntdb_transaction_commit(ntdb);

fail:
	ntdb_transaction_cancel(ntdb);
	ntdb_close(tmp_db);
	return state.error;
}
