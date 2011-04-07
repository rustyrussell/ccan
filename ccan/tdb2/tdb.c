#include "private.h"
#include <ccan/asprintf/asprintf.h>
#include <stdarg.h>

static enum TDB_ERROR update_rec_hdr(struct tdb_context *tdb,
				     tdb_off_t off,
				     tdb_len_t keylen,
				     tdb_len_t datalen,
				     struct tdb_used_record *rec,
				     uint64_t h)
{
	uint64_t dataroom = rec_data_length(rec) + rec_extra_padding(rec);
	enum TDB_ERROR ecode;

	ecode = set_header(tdb, rec, TDB_USED_MAGIC, keylen, datalen,
			   keylen + dataroom, h);
	if (ecode == TDB_SUCCESS) {
		ecode = tdb_write_convert(tdb, off, rec, sizeof(*rec));
	}
	return ecode;
}

static enum TDB_ERROR replace_data(struct tdb_context *tdb,
				   struct hash_info *h,
				   struct tdb_data key, struct tdb_data dbuf,
				   tdb_off_t old_off, tdb_len_t old_room,
				   bool growing)
{
	tdb_off_t new_off;
	enum TDB_ERROR ecode;

	/* Allocate a new record. */
	new_off = alloc(tdb, key.dsize, dbuf.dsize, h->h, TDB_USED_MAGIC,
			growing);
	if (TDB_OFF_IS_ERR(new_off)) {
		return new_off;
	}

	/* We didn't like the existing one: remove it. */
	if (old_off) {
		add_stat(tdb, frees, 1);
		ecode = add_free_record(tdb, old_off,
					sizeof(struct tdb_used_record)
					+ key.dsize + old_room);
		if (ecode == TDB_SUCCESS)
			ecode = replace_in_hash(tdb, h, new_off);
	} else {
		ecode = add_to_hash(tdb, h, new_off);
	}
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	new_off += sizeof(struct tdb_used_record);
	ecode = tdb->methods->twrite(tdb, new_off, key.dptr, key.dsize);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	new_off += key.dsize;
	ecode = tdb->methods->twrite(tdb, new_off, dbuf.dptr, dbuf.dsize);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	if (tdb->flags & TDB_SEQNUM)
		tdb_inc_seqnum(tdb);

	return TDB_SUCCESS;
}

static enum TDB_ERROR update_data(struct tdb_context *tdb,
				  tdb_off_t off,
				  struct tdb_data dbuf,
				  tdb_len_t extra)
{
	enum TDB_ERROR ecode;

	ecode = tdb->methods->twrite(tdb, off, dbuf.dptr, dbuf.dsize);
	if (ecode == TDB_SUCCESS && extra) {
		/* Put a zero in; future versions may append other data. */
		ecode = tdb->methods->twrite(tdb, off + dbuf.dsize, "", 1);
	}
	if (tdb->flags & TDB_SEQNUM)
		tdb_inc_seqnum(tdb);

	return ecode;
}

enum TDB_ERROR tdb_store(struct tdb_context *tdb,
			 struct tdb_data key, struct tdb_data dbuf, int flag)
{
	struct hash_info h;
	tdb_off_t off;
	tdb_len_t old_room = 0;
	struct tdb_used_record rec;
	enum TDB_ERROR ecode;

	off = find_and_lock(tdb, key, F_WRLCK, &h, &rec, NULL);
	if (TDB_OFF_IS_ERR(off)) {
		return tdb->last_error = off;
	}

	/* Now we have lock on this hash bucket. */
	if (flag == TDB_INSERT) {
		if (off) {
			ecode = TDB_ERR_EXISTS;
			goto out;
		}
	} else {
		if (off) {
			old_room = rec_data_length(&rec)
				+ rec_extra_padding(&rec);
			if (old_room >= dbuf.dsize) {
				/* Can modify in-place.  Easy! */
				ecode = update_rec_hdr(tdb, off,
						       key.dsize, dbuf.dsize,
						       &rec, h.h);
				if (ecode != TDB_SUCCESS) {
					goto out;
				}
				ecode = update_data(tdb,
						    off + sizeof(rec)
						    + key.dsize, dbuf,
						    old_room - dbuf.dsize);
				if (ecode != TDB_SUCCESS) {
					goto out;
				}
				tdb_unlock_hashes(tdb, h.hlock_start,
						  h.hlock_range, F_WRLCK);
				return tdb->last_error = TDB_SUCCESS;
			}
		} else {
			if (flag == TDB_MODIFY) {
				/* if the record doesn't exist and we
				   are in TDB_MODIFY mode then we should fail
				   the store */
				ecode = TDB_ERR_NOEXIST;
				goto out;
			}
		}
	}

	/* If we didn't use the old record, this implies we're growing. */
	ecode = replace_data(tdb, &h, key, dbuf, off, old_room, off);
out:
	tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range, F_WRLCK);
	return tdb->last_error = ecode;
}

enum TDB_ERROR tdb_append(struct tdb_context *tdb,
			  struct tdb_data key, struct tdb_data dbuf)
{
	struct hash_info h;
	tdb_off_t off;
	struct tdb_used_record rec;
	tdb_len_t old_room = 0, old_dlen;
	unsigned char *newdata;
	struct tdb_data new_dbuf;
	enum TDB_ERROR ecode;

	off = find_and_lock(tdb, key, F_WRLCK, &h, &rec, NULL);
	if (TDB_OFF_IS_ERR(off)) {
		return tdb->last_error = off;
	}

	if (off) {
		old_dlen = rec_data_length(&rec);
		old_room = old_dlen + rec_extra_padding(&rec);

		/* Fast path: can append in place. */
		if (rec_extra_padding(&rec) >= dbuf.dsize) {
			ecode = update_rec_hdr(tdb, off, key.dsize,
					       old_dlen + dbuf.dsize, &rec,
					       h.h);
			if (ecode != TDB_SUCCESS) {
				goto out;
			}

			off += sizeof(rec) + key.dsize + old_dlen;
			ecode = update_data(tdb, off, dbuf,
					    rec_extra_padding(&rec));
			goto out;
		}

		/* Slow path. */
		newdata = malloc(key.dsize + old_dlen + dbuf.dsize);
		if (!newdata) {
			ecode = tdb_logerr(tdb, TDB_ERR_OOM, TDB_LOG_ERROR,
					   "tdb_append:"
					   " failed to allocate %zu bytes",
					   (size_t)(key.dsize + old_dlen
						    + dbuf.dsize));
			goto out;
		}
		ecode = tdb->methods->tread(tdb, off + sizeof(rec) + key.dsize,
					    newdata, old_dlen);
		if (ecode != TDB_SUCCESS) {
			goto out_free_newdata;
		}
		memcpy(newdata + old_dlen, dbuf.dptr, dbuf.dsize);
		new_dbuf.dptr = newdata;
		new_dbuf.dsize = old_dlen + dbuf.dsize;
	} else {
		newdata = NULL;
		new_dbuf = dbuf;
	}

	/* If they're using tdb_append(), it implies they're growing record. */
	ecode = replace_data(tdb, &h, key, new_dbuf, off, old_room, true);

out_free_newdata:
	free(newdata);
out:
	tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range, F_WRLCK);
	return tdb->last_error = ecode;
}

enum TDB_ERROR tdb_fetch(struct tdb_context *tdb, struct tdb_data key,
			 struct tdb_data *data)
{
	tdb_off_t off;
	struct tdb_used_record rec;
	struct hash_info h;
	enum TDB_ERROR ecode;

	off = find_and_lock(tdb, key, F_RDLCK, &h, &rec, NULL);
	if (TDB_OFF_IS_ERR(off)) {
		return tdb->last_error = off;
	}

	if (!off) {
		ecode = TDB_ERR_NOEXIST;
	} else {
		data->dsize = rec_data_length(&rec);
		data->dptr = tdb_alloc_read(tdb, off + sizeof(rec) + key.dsize,
					    data->dsize);
		if (TDB_PTR_IS_ERR(data->dptr)) {
			ecode = TDB_PTR_ERR(data->dptr);
		} else
			ecode = TDB_SUCCESS;
	}

	tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range, F_RDLCK);
	return tdb->last_error = ecode;
}

bool tdb_exists(struct tdb_context *tdb, TDB_DATA key)
{
	tdb_off_t off;
	struct tdb_used_record rec;
	struct hash_info h;

	off = find_and_lock(tdb, key, F_RDLCK, &h, &rec, NULL);
	if (TDB_OFF_IS_ERR(off)) {
		tdb->last_error = off;
		return false;
	}
	tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range, F_RDLCK);

	tdb->last_error = TDB_SUCCESS;
	return off ? true : false;
}

enum TDB_ERROR tdb_delete(struct tdb_context *tdb, struct tdb_data key)
{
	tdb_off_t off;
	struct tdb_used_record rec;
	struct hash_info h;
	enum TDB_ERROR ecode;

	off = find_and_lock(tdb, key, F_WRLCK, &h, &rec, NULL);
	if (TDB_OFF_IS_ERR(off)) {
		return tdb->last_error = off;
	}

	if (!off) {
		ecode = TDB_ERR_NOEXIST;
		goto unlock;
	}

	ecode = delete_from_hash(tdb, &h);
	if (ecode != TDB_SUCCESS) {
		goto unlock;
	}

	/* Free the deleted entry. */
	add_stat(tdb, frees, 1);
	ecode = add_free_record(tdb, off,
				sizeof(struct tdb_used_record)
				+ rec_key_length(&rec)
				+ rec_data_length(&rec)
				+ rec_extra_padding(&rec));

	if (tdb->flags & TDB_SEQNUM)
		tdb_inc_seqnum(tdb);

unlock:
	tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range, F_WRLCK);
	return tdb->last_error = ecode;
}

unsigned int tdb_get_flags(struct tdb_context *tdb)
{
	return tdb->flags;
}

void tdb_add_flag(struct tdb_context *tdb, unsigned flag)
{
	if (tdb->flags & TDB_INTERNAL) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_EINVAL,
					     TDB_LOG_USE_ERROR,
					     "tdb_add_flag: internal db");
		return;
	}
	switch (flag) {
	case TDB_NOLOCK:
		tdb->flags |= TDB_NOLOCK;
		break;
	case TDB_NOMMAP:
		tdb->flags |= TDB_NOMMAP;
		tdb_munmap(tdb->file);
		break;
	case TDB_NOSYNC:
		tdb->flags |= TDB_NOSYNC;
		break;
	case TDB_SEQNUM:
		tdb->flags |= TDB_SEQNUM;
		break;
	default:
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_EINVAL,
					     TDB_LOG_USE_ERROR,
					     "tdb_add_flag: Unknown flag %u",
					     flag);
	}
}

void tdb_remove_flag(struct tdb_context *tdb, unsigned flag)
{
	if (tdb->flags & TDB_INTERNAL) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_EINVAL,
					     TDB_LOG_USE_ERROR,
					     "tdb_remove_flag: internal db");
		return;
	}
	switch (flag) {
	case TDB_NOLOCK:
		tdb->flags &= ~TDB_NOLOCK;
		break;
	case TDB_NOMMAP:
		tdb->flags &= ~TDB_NOMMAP;
		tdb_mmap(tdb);
		break;
	case TDB_NOSYNC:
		tdb->flags &= ~TDB_NOSYNC;
		break;
	case TDB_SEQNUM:
		tdb->flags &= ~TDB_SEQNUM;
		break;
	default:
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_EINVAL,
					     TDB_LOG_USE_ERROR,
					     "tdb_remove_flag: Unknown flag %u",
					     flag);
	}
}

const char *tdb_errorstr(enum TDB_ERROR ecode)
{
	/* Gcc warns if you miss a case in the switch, so use that. */
	switch (ecode) {
	case TDB_SUCCESS: return "Success";
	case TDB_ERR_CORRUPT: return "Corrupt database";
	case TDB_ERR_IO: return "IO Error";
	case TDB_ERR_LOCK: return "Locking error";
	case TDB_ERR_OOM: return "Out of memory";
	case TDB_ERR_EXISTS: return "Record exists";
	case TDB_ERR_EINVAL: return "Invalid parameter";
	case TDB_ERR_NOEXIST: return "Record does not exist";
	case TDB_ERR_RDONLY: return "write not permitted";
	}
	return "Invalid error code";
}

enum TDB_ERROR tdb_error(struct tdb_context *tdb)
{
	return tdb->last_error;
}

enum TDB_ERROR COLD tdb_logerr(struct tdb_context *tdb,
			       enum TDB_ERROR ecode,
			       enum tdb_log_level level,
			       const char *fmt, ...)
{
	char *message;
	va_list ap;
	size_t len;
	/* tdb_open paths care about errno, so save it. */
	int saved_errno = errno;

	if (!tdb->log_fn)
		return ecode;

	va_start(ap, fmt);
	len = vasprintf(&message, fmt, ap);
	va_end(ap);

	if (len < 0) {
		tdb->log_fn(tdb, TDB_LOG_ERROR,
			    "out of memory formatting message:", tdb->log_data);
		tdb->log_fn(tdb, level, fmt, tdb->log_data);
	} else {
		tdb->log_fn(tdb, level, message, tdb->log_data);
		free(message);
	}
	errno = saved_errno;
	return ecode;
}

enum TDB_ERROR tdb_parse_record_(struct tdb_context *tdb,
				 TDB_DATA key,
				 enum TDB_ERROR (*parse)(TDB_DATA k,
							 TDB_DATA d,
							 void *data),
				 void *data)
{
	tdb_off_t off;
	struct tdb_used_record rec;
	struct hash_info h;
	enum TDB_ERROR ecode;

	off = find_and_lock(tdb, key, F_RDLCK, &h, &rec, NULL);
	if (TDB_OFF_IS_ERR(off)) {
		return tdb->last_error = off;
	}

	if (!off) {
		ecode = TDB_ERR_NOEXIST;
	} else {
		const void *dptr;
		dptr = tdb_access_read(tdb, off + sizeof(rec) + key.dsize,
				       rec_data_length(&rec), false);
		if (TDB_PTR_IS_ERR(dptr)) {
			ecode = TDB_PTR_ERR(dptr);
		} else {
			TDB_DATA d = tdb_mkdata(dptr, rec_data_length(&rec));

			ecode = parse(key, d, data);
			tdb_access_release(tdb, dptr);
		}
	}

	tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range, F_RDLCK);
	return tdb->last_error = ecode;
}

const char *tdb_name(const struct tdb_context *tdb)
{
	return tdb->name;
}

int64_t tdb_get_seqnum(struct tdb_context *tdb)
{
	tdb_off_t off = tdb_read_off(tdb, offsetof(struct tdb_header, seqnum));
	if (TDB_OFF_IS_ERR(off))
		tdb->last_error = off;
	else
		tdb->last_error = TDB_SUCCESS;
	return off;
}
	

int tdb_fd(const struct tdb_context *tdb)
{
	return tdb->file->fd;
}
