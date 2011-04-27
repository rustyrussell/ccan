 /*
   Unix SMB/CIFS implementation.

   trivial database library

   Copyright (C) Andrew Tridgell              1999-2005
   Copyright (C) Paul `Rusty' Russell		   2000
   Copyright (C) Jeremy Allison			   2000-2003
   Copyright (C) Rusty Russell			   2010

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
#include "private.h"
#include <assert.h>
#include <ccan/likely/likely.h>

void tdb_munmap(struct tdb_file *file)
{
	if (file->fd == -1)
		return;

	if (file->map_ptr) {
		munmap(file->map_ptr, file->map_size);
		file->map_ptr = NULL;
	}
}

void tdb_mmap(struct tdb_context *tdb)
{
	if (tdb->flags & TDB_INTERNAL)
		return;

	if (tdb->flags & TDB_NOMMAP)
		return;

	/* size_t can be smaller than off_t. */
	if ((size_t)tdb->file->map_size == tdb->file->map_size) {
		tdb->file->map_ptr = mmap(NULL, tdb->file->map_size,
					  tdb->mmap_flags,
					  MAP_SHARED, tdb->file->fd, 0);
	} else
		tdb->file->map_ptr = MAP_FAILED;

	/*
	 * NB. When mmap fails it returns MAP_FAILED *NOT* NULL !!!!
	 */
	if (tdb->file->map_ptr == MAP_FAILED) {
		tdb->file->map_ptr = NULL;
		tdb_logerr(tdb, TDB_SUCCESS, TDB_LOG_WARNING,
			   "tdb_mmap failed for size %lld (%s)",
			   (long long)tdb->file->map_size, strerror(errno));
	}
}

/* check for an out of bounds access - if it is out of bounds then
   see if the database has been expanded by someone else and expand
   if necessary
   note that "len" is the minimum length needed for the db
*/
static enum TDB_ERROR tdb_oob(struct tdb_context *tdb, tdb_off_t len,
			      bool probe)
{
	struct stat st;
	enum TDB_ERROR ecode;

	/* We can't hold pointers during this: we could unmap! */
	assert(!tdb->direct_access
	       || (tdb->flags & TDB_NOLOCK)
	       || tdb_has_expansion_lock(tdb));

	if (len <= tdb->file->map_size)
		return 0;
	if (tdb->flags & TDB_INTERNAL) {
		if (!probe) {
			tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				 "tdb_oob len %lld beyond internal"
				 " malloc size %lld",
				 (long long)len,
				 (long long)tdb->file->map_size);
		}
		return TDB_ERR_IO;
	}

	ecode = tdb_lock_expand(tdb, F_RDLCK);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	if (fstat(tdb->file->fd, &st) != 0) {
		tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
			   "Failed to fstat file: %s", strerror(errno));
		tdb_unlock_expand(tdb, F_RDLCK);
		return TDB_ERR_IO;
	}

	tdb_unlock_expand(tdb, F_RDLCK);

	if (st.st_size < (size_t)len) {
		if (!probe) {
			tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				   "tdb_oob len %zu beyond eof at %zu",
				   (size_t)len, st.st_size);
		}
		return TDB_ERR_IO;
	}

	/* Unmap, update size, remap */
	tdb_munmap(tdb->file);

	tdb->file->map_size = st.st_size;
	tdb_mmap(tdb);
	return TDB_SUCCESS;
}

/* Endian conversion: we only ever deal with 8 byte quantities */
void *tdb_convert(const struct tdb_context *tdb, void *buf, tdb_len_t size)
{
	assert(size % 8 == 0);
	if (unlikely((tdb->flags & TDB_CONVERT)) && buf) {
		uint64_t i, *p = (uint64_t *)buf;
		for (i = 0; i < size / 8; i++)
			p[i] = bswap_64(p[i]);
	}
	return buf;
}

/* Return first non-zero offset in offset array, or end, or -ve error. */
/* FIXME: Return the off? */
uint64_t tdb_find_nonzero_off(struct tdb_context *tdb,
			      tdb_off_t base, uint64_t start, uint64_t end)
{
	uint64_t i;
	const uint64_t *val;

	/* Zero vs non-zero is the same unconverted: minor optimization. */
	val = tdb_access_read(tdb, base + start * sizeof(tdb_off_t),
			      (end - start) * sizeof(tdb_off_t), false);
	if (TDB_PTR_IS_ERR(val)) {
		return TDB_PTR_ERR(val);
	}

	for (i = 0; i < (end - start); i++) {
		if (val[i])
			break;
	}
	tdb_access_release(tdb, val);
	return start + i;
}

/* Return first zero offset in num offset array, or num, or -ve error. */
uint64_t tdb_find_zero_off(struct tdb_context *tdb, tdb_off_t off,
			   uint64_t num)
{
	uint64_t i;
	const uint64_t *val;

	/* Zero vs non-zero is the same unconverted: minor optimization. */
	val = tdb_access_read(tdb, off, num * sizeof(tdb_off_t), false);
	if (TDB_PTR_IS_ERR(val)) {
		return TDB_PTR_ERR(val);
	}

	for (i = 0; i < num; i++) {
		if (!val[i])
			break;
	}
	tdb_access_release(tdb, val);
	return i;
}

enum TDB_ERROR zero_out(struct tdb_context *tdb, tdb_off_t off, tdb_len_t len)
{
	char buf[8192] = { 0 };
	void *p = tdb->methods->direct(tdb, off, len, true);
	enum TDB_ERROR ecode = TDB_SUCCESS;

	assert(!tdb->read_only);
	if (TDB_PTR_IS_ERR(p)) {
		return TDB_PTR_ERR(p);
	}
	if (p) {
		memset(p, 0, len);
		return ecode;
	}
	while (len) {
		unsigned todo = len < sizeof(buf) ? len : sizeof(buf);
		ecode = tdb->methods->twrite(tdb, off, buf, todo);
		if (ecode != TDB_SUCCESS) {
			break;
		}
		len -= todo;
		off += todo;
	}
	return ecode;
}

tdb_off_t tdb_read_off(struct tdb_context *tdb, tdb_off_t off)
{
	tdb_off_t ret;
	enum TDB_ERROR ecode;

	if (likely(!(tdb->flags & TDB_CONVERT))) {
		tdb_off_t *p = tdb->methods->direct(tdb, off, sizeof(*p),
						    false);
		if (TDB_PTR_IS_ERR(p)) {
			return TDB_PTR_ERR(p);
		}
		if (p)
			return *p;
	}

	ecode = tdb_read_convert(tdb, off, &ret, sizeof(ret));
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}
	return ret;
}

/* write a lump of data at a specified offset */
static enum TDB_ERROR tdb_write(struct tdb_context *tdb, tdb_off_t off,
				const void *buf, tdb_len_t len)
{
	enum TDB_ERROR ecode;

	if (tdb->read_only) {
		return tdb_logerr(tdb, TDB_ERR_RDONLY, TDB_LOG_USE_ERROR,
				  "Write to read-only database");
	}

	ecode = tdb->methods->oob(tdb, off + len, 0);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	if (tdb->file->map_ptr) {
		memcpy(off + (char *)tdb->file->map_ptr, buf, len);
	} else {
		ssize_t ret;
		ret = pwrite(tdb->file->fd, buf, len, off);
		if (ret != len) {
			/* This shouldn't happen: we avoid sparse files. */
			if (ret >= 0)
				errno = ENOSPC;

			return tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
					  "tdb_write: %zi at %zu len=%zu (%s)",
					  ret, (size_t)off, (size_t)len,
					  strerror(errno));
		}
	}
	return TDB_SUCCESS;
}

/* read a lump of data at a specified offset */
static enum TDB_ERROR tdb_read(struct tdb_context *tdb, tdb_off_t off,
			       void *buf, tdb_len_t len)
{
	enum TDB_ERROR ecode;

	ecode = tdb->methods->oob(tdb, off + len, 0);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	if (tdb->file->map_ptr) {
		memcpy(buf, off + (char *)tdb->file->map_ptr, len);
	} else {
		ssize_t r = pread(tdb->file->fd, buf, len, off);
		if (r != len) {
			return tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
					  "tdb_read failed with %zi at %zu "
					  "len=%zu (%s) map_size=%zu",
					  r, (size_t)off, (size_t)len,
					  strerror(errno),
					  (size_t)tdb->file->map_size);
		}
	}
	return TDB_SUCCESS;
}

enum TDB_ERROR tdb_write_convert(struct tdb_context *tdb, tdb_off_t off,
				 const void *rec, size_t len)
{
	enum TDB_ERROR ecode;

	if (unlikely((tdb->flags & TDB_CONVERT))) {
		void *conv = malloc(len);
		if (!conv) {
			return tdb_logerr(tdb, TDB_ERR_OOM, TDB_LOG_ERROR,
					  "tdb_write: no memory converting"
					  " %zu bytes", len);
		}
		memcpy(conv, rec, len);
		ecode = tdb->methods->twrite(tdb, off,
					   tdb_convert(tdb, conv, len), len);
		free(conv);
	} else {
		ecode = tdb->methods->twrite(tdb, off, rec, len);
	}
	return ecode;
}

enum TDB_ERROR tdb_read_convert(struct tdb_context *tdb, tdb_off_t off,
				void *rec, size_t len)
{
	enum TDB_ERROR ecode = tdb->methods->tread(tdb, off, rec, len);
	tdb_convert(tdb, rec, len);
	return ecode;
}

enum TDB_ERROR tdb_write_off(struct tdb_context *tdb,
			     tdb_off_t off, tdb_off_t val)
{
	if (tdb->read_only) {
		return tdb_logerr(tdb, TDB_ERR_RDONLY, TDB_LOG_USE_ERROR,
				  "Write to read-only database");
	}

	if (likely(!(tdb->flags & TDB_CONVERT))) {
		tdb_off_t *p = tdb->methods->direct(tdb, off, sizeof(*p),
						    true);
		if (TDB_PTR_IS_ERR(p)) {
			return TDB_PTR_ERR(p);
		}
		if (p) {
			*p = val;
			return TDB_SUCCESS;
		}
	}
	return tdb_write_convert(tdb, off, &val, sizeof(val));
}

static void *_tdb_alloc_read(struct tdb_context *tdb, tdb_off_t offset,
			     tdb_len_t len, unsigned int prefix)
{
	unsigned char *buf;
	enum TDB_ERROR ecode;

	/* some systems don't like zero length malloc */
	buf = malloc(prefix + len ? prefix + len : 1);
	if (!buf) {
		tdb_logerr(tdb, TDB_ERR_OOM, TDB_LOG_USE_ERROR,
			   "tdb_alloc_read malloc failed len=%zu",
			   (size_t)(prefix + len));
		return TDB_ERR_PTR(TDB_ERR_OOM);
	} else {
		ecode = tdb->methods->tread(tdb, offset, buf+prefix, len);
		if (unlikely(ecode != TDB_SUCCESS)) {
			free(buf);
			return TDB_ERR_PTR(ecode);
		}
	}
	return buf;
}

/* read a lump of data, allocating the space for it */
void *tdb_alloc_read(struct tdb_context *tdb, tdb_off_t offset, tdb_len_t len)
{
	return _tdb_alloc_read(tdb, offset, len, 0);
}

static enum TDB_ERROR fill(struct tdb_context *tdb,
			   const void *buf, size_t size,
			   tdb_off_t off, tdb_len_t len)
{
	while (len) {
		size_t n = len > size ? size : len;
		ssize_t ret = pwrite(tdb->file->fd, buf, n, off);
		if (ret != n) {
			if (ret >= 0)
				errno = ENOSPC;

			return tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
					  "fill failed:"
					  " %zi at %zu len=%zu (%s)",
					  ret, (size_t)off, (size_t)len,
					  strerror(errno));
		}
		len -= n;
		off += n;
	}
	return TDB_SUCCESS;
}

/* expand a file.  we prefer to use ftruncate, as that is what posix
  says to use for mmap expansion */
static enum TDB_ERROR tdb_expand_file(struct tdb_context *tdb,
				      tdb_len_t addition)
{
	char buf[8192];
	enum TDB_ERROR ecode;

	if (tdb->read_only) {
		return tdb_logerr(tdb, TDB_ERR_RDONLY, TDB_LOG_USE_ERROR,
				  "Expand on read-only database");
	}

	if (tdb->flags & TDB_INTERNAL) {
		char *new = realloc(tdb->file->map_ptr,
				    tdb->file->map_size + addition);
		if (!new) {
			return tdb_logerr(tdb, TDB_ERR_OOM, TDB_LOG_ERROR,
					  "No memory to expand database");
		}
		tdb->file->map_ptr = new;
		tdb->file->map_size += addition;
	} else {
		/* Unmap before trying to write; old TDB claimed OpenBSD had
		 * problem with this otherwise. */
		tdb_munmap(tdb->file);

		/* If this fails, we try to fill anyway. */
		if (ftruncate(tdb->file->fd, tdb->file->map_size + addition))
			;

		/* now fill the file with something. This ensures that the
		   file isn't sparse, which would be very bad if we ran out of
		   disk. This must be done with write, not via mmap */
		memset(buf, 0x43, sizeof(buf));
		ecode = fill(tdb, buf, sizeof(buf), tdb->file->map_size,
			     addition);
		if (ecode != TDB_SUCCESS)
			return ecode;
		tdb->file->map_size += addition;
		tdb_mmap(tdb);
	}
	return TDB_SUCCESS;
}

const void *tdb_access_read(struct tdb_context *tdb,
			    tdb_off_t off, tdb_len_t len, bool convert)
{
	void *ret = NULL;

	if (likely(!(tdb->flags & TDB_CONVERT))) {
		ret = tdb->methods->direct(tdb, off, len, false);

		if (TDB_PTR_IS_ERR(ret)) {
			return ret;
		}
	}
	if (!ret) {
		struct tdb_access_hdr *hdr;
		hdr = _tdb_alloc_read(tdb, off, len, sizeof(*hdr));
		if (TDB_PTR_IS_ERR(hdr)) {
			return hdr;
		}
		hdr->next = tdb->access;
		tdb->access = hdr;
		ret = hdr + 1;
		if (convert) {
			tdb_convert(tdb, (void *)ret, len);
		}
	} else
		tdb->direct_access++;

	return ret;
}

void *tdb_access_write(struct tdb_context *tdb,
		       tdb_off_t off, tdb_len_t len, bool convert)
{
	void *ret = NULL;

	if (tdb->read_only) {
		tdb_logerr(tdb, TDB_ERR_RDONLY, TDB_LOG_USE_ERROR,
			   "Write to read-only database");
		return TDB_ERR_PTR(TDB_ERR_RDONLY);
	}

	if (likely(!(tdb->flags & TDB_CONVERT))) {
		ret = tdb->methods->direct(tdb, off, len, true);

		if (TDB_PTR_IS_ERR(ret)) {
			return ret;
		}
	}

	if (!ret) {
		struct tdb_access_hdr *hdr;
		hdr = _tdb_alloc_read(tdb, off, len, sizeof(*hdr));
		if (TDB_PTR_IS_ERR(hdr)) {
			return hdr;
		}
		hdr->next = tdb->access;
		tdb->access = hdr;
		hdr->off = off;
		hdr->len = len;
		hdr->convert = convert;
		ret = hdr + 1;
		if (convert)
			tdb_convert(tdb, (void *)ret, len);
	} else
		tdb->direct_access++;

	return ret;
}

static struct tdb_access_hdr **find_hdr(struct tdb_context *tdb, const void *p)
{
	struct tdb_access_hdr **hp;

	for (hp = &tdb->access; *hp; hp = &(*hp)->next) {
		if (*hp + 1 == p)
			return hp;
	}
	return NULL;
}

void tdb_access_release(struct tdb_context *tdb, const void *p)
{
	struct tdb_access_hdr *hdr, **hp = find_hdr(tdb, p);

	if (hp) {
		hdr = *hp;
		*hp = hdr->next;
		free(hdr);
	} else
		tdb->direct_access--;
}

enum TDB_ERROR tdb_access_commit(struct tdb_context *tdb, void *p)
{
	struct tdb_access_hdr *hdr, **hp = find_hdr(tdb, p);
	enum TDB_ERROR ecode;

	if (hp) {
		hdr = *hp;
		if (hdr->convert)
			ecode = tdb_write_convert(tdb, hdr->off, p, hdr->len);
		else
			ecode = tdb_write(tdb, hdr->off, p, hdr->len);
		*hp = hdr->next;
		free(hdr);
	} else {
		tdb->direct_access--;
		ecode = TDB_SUCCESS;
	}

	return ecode;
}

static void *tdb_direct(struct tdb_context *tdb, tdb_off_t off, size_t len,
			bool write_mode)
{
	enum TDB_ERROR ecode;

	if (unlikely(!tdb->file->map_ptr))
		return NULL;

	ecode = tdb_oob(tdb, off + len, true);
	if (unlikely(ecode != TDB_SUCCESS))
		return TDB_ERR_PTR(ecode);
	return (char *)tdb->file->map_ptr + off;
}

void tdb_inc_seqnum(struct tdb_context *tdb)
{
	tdb_off_t seq;

	if (likely(!(tdb->flags & TDB_CONVERT))) {
		int64_t *direct;

		direct = tdb->methods->direct(tdb,
					      offsetof(struct tdb_header,
						       seqnum),
					      sizeof(*direct), true);
		if (likely(direct)) {
			/* Don't let it go negative, even briefly */
			if (unlikely((*direct) + 1) < 0)
				*direct = 0;
			(*direct)++;
			return;
		}
	}

	seq = tdb_read_off(tdb, offsetof(struct tdb_header, seqnum));
	if (!TDB_OFF_IS_ERR(seq)) {
		seq++;
		if (unlikely((int64_t)seq < 0))
			seq = 0;
		tdb_write_off(tdb, offsetof(struct tdb_header, seqnum), seq);
	}
}

static const struct tdb_methods io_methods = {
	tdb_read,
	tdb_write,
	tdb_oob,
	tdb_expand_file,
	tdb_direct,
};

/*
  initialise the default methods table
*/
void tdb_io_init(struct tdb_context *tdb)
{
	tdb->methods = &io_methods;
}
