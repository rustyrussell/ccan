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

void tdb_munmap(struct tdb_context *tdb)
{
	if (tdb->flags & TDB_INTERNAL)
		return;

	if (tdb->map_ptr) {
		munmap(tdb->map_ptr, tdb->map_size);
		tdb->map_ptr = NULL;
	}
}

void tdb_mmap(struct tdb_context *tdb)
{
	if (tdb->flags & TDB_INTERNAL)
		return;

	if (tdb->flags & TDB_NOMMAP)
		return;

	tdb->map_ptr = mmap(NULL, tdb->map_size, tdb->mmap_flags,
			    MAP_SHARED, tdb->fd, 0);

	/*
	 * NB. When mmap fails it returns MAP_FAILED *NOT* NULL !!!!
	 */
	if (tdb->map_ptr == MAP_FAILED) {
		tdb->map_ptr = NULL;
		tdb_logerr(tdb, TDB_SUCCESS, TDB_LOG_WARNING,
			   "tdb_mmap failed for size %lld (%s)",
			   (long long)tdb->map_size, strerror(errno));
	}
}

/* check for an out of bounds access - if it is out of bounds then
   see if the database has been expanded by someone else and expand
   if necessary
   note that "len" is the minimum length needed for the db
*/
static int tdb_oob(struct tdb_context *tdb, tdb_off_t len, bool probe)
{
	struct stat st;
	enum TDB_ERROR ecode;

	/* We can't hold pointers during this: we could unmap! */
	assert(!tdb->direct_access
	       || (tdb->flags & TDB_NOLOCK)
	       || tdb_has_expansion_lock(tdb));

	if (len <= tdb->map_size)
		return 0;
	if (tdb->flags & TDB_INTERNAL) {
		if (!probe) {
			tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				 "tdb_oob len %lld beyond internal"
				 " malloc size %lld",
				 (long long)len,
				 (long long)tdb->map_size);
		}
		return -1;
	}

	ecode = tdb_lock_expand(tdb, F_RDLCK);
	if (ecode != TDB_SUCCESS) {
		tdb->ecode = ecode;
		return -1;
	}

	if (fstat(tdb->fd, &st) != 0) {
		tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
			   "Failed to fstat file: %s", strerror(errno));
		tdb_unlock_expand(tdb, F_RDLCK);
		return -1;
	}

	tdb_unlock_expand(tdb, F_RDLCK);

	if (st.st_size < (size_t)len) {
		if (!probe) {
			tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				   "tdb_oob len %zu beyond eof at %zu",
				   (size_t)len, st.st_size);
		}
		return -1;
	}

	/* Unmap, update size, remap */
	tdb_munmap(tdb);

	tdb->map_size = st.st_size;
	tdb_mmap(tdb);
	return 0;
}

/* Endian conversion: we only ever deal with 8 byte quantities */
void *tdb_convert(const struct tdb_context *tdb, void *buf, tdb_len_t size)
{
	if (unlikely((tdb->flags & TDB_CONVERT)) && buf) {
		uint64_t i, *p = (uint64_t *)buf;
		for (i = 0; i < size / 8; i++)
			p[i] = bswap_64(p[i]);
	}
	return buf;
}

/* FIXME: Return the off? */
uint64_t tdb_find_nonzero_off(struct tdb_context *tdb,
			      tdb_off_t base, uint64_t start, uint64_t end)
{
	uint64_t i;
	const uint64_t *val;

	/* Zero vs non-zero is the same unconverted: minor optimization. */
	val = tdb_access_read(tdb, base + start * sizeof(tdb_off_t),
			      (end - start) * sizeof(tdb_off_t), false);
	if (!val)
		return end;

	for (i = 0; i < (end - start); i++) {
		if (val[i])
			break;
	}
	tdb_access_release(tdb, val);
	return start + i;
}

/* Return first zero offset in num offset array, or num. */
uint64_t tdb_find_zero_off(struct tdb_context *tdb, tdb_off_t off,
			   uint64_t num)
{
	uint64_t i;
	const uint64_t *val;

	/* Zero vs non-zero is the same unconverted: minor optimization. */
	val = tdb_access_read(tdb, off, num * sizeof(tdb_off_t), false);
	if (!val)
		return num;

	for (i = 0; i < num; i++) {
		if (!val[i])
			break;
	}
	tdb_access_release(tdb, val);
	return i;
}

int zero_out(struct tdb_context *tdb, tdb_off_t off, tdb_len_t len)
{
	char buf[8192] = { 0 };
	void *p = tdb->methods->direct(tdb, off, len, true);

	assert(!tdb->read_only);
	if (p) {
		memset(p, 0, len);
		return 0;
	}
	while (len) {
		unsigned todo = len < sizeof(buf) ? len : sizeof(buf);
		if (tdb->methods->twrite(tdb, off, buf, todo) == -1)
			return -1;
		len -= todo;
		off += todo;
	}
	return 0;
}

tdb_off_t tdb_read_off(struct tdb_context *tdb, tdb_off_t off)
{
	tdb_off_t ret;

	if (likely(!(tdb->flags & TDB_CONVERT))) {
		tdb_off_t *p = tdb->methods->direct(tdb, off, sizeof(*p),
						    false);
		if (p)
			return *p;
	}

	if (tdb_read_convert(tdb, off, &ret, sizeof(ret)) == -1)
		return TDB_OFF_ERR;
	return ret;
}

/* write a lump of data at a specified offset */
static int tdb_write(struct tdb_context *tdb, tdb_off_t off,
		     const void *buf, tdb_len_t len)
{
	if (tdb->read_only) {
		tdb_logerr(tdb, TDB_ERR_RDONLY, TDB_LOG_USE_ERROR,
			   "Write to read-only database");
		return -1;
	}

	/* FIXME: Bogus optimization? */
	if (len == 0) {
		return 0;
	}

	if (tdb->methods->oob(tdb, off + len, 0) != 0)
		return -1;

	if (tdb->map_ptr) {
		memcpy(off + (char *)tdb->map_ptr, buf, len);
	} else {
		ssize_t ret;
		ret = pwrite(tdb->fd, buf, len, off);
		if (ret < len) {
			/* This shouldn't happen: we avoid sparse files. */
			if (ret >= 0)
				errno = ENOSPC;

			tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				   "tdb_write: %zi at %zu len=%zu (%s)",
				   ret, (size_t)off, (size_t)len,
				   strerror(errno));
			return -1;
		}
	}
	return 0;
}

/* read a lump of data at a specified offset */
static int tdb_read(struct tdb_context *tdb, tdb_off_t off, void *buf,
		    tdb_len_t len)
{
	if (tdb->methods->oob(tdb, off + len, 0) != 0) {
		return -1;
	}

	if (tdb->map_ptr) {
		memcpy(buf, off + (char *)tdb->map_ptr, len);
	} else {
		ssize_t r = pread(tdb->fd, buf, len, off);
		if (r != len) {
			tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				   "tdb_read failed with %zi at %zu "
				   "len=%zu (%s) map_size=%zu",
				   r, (size_t)off, (size_t)len,
				   strerror(errno),
				   (size_t)tdb->map_size);
			return -1;
		}
	}
	return 0;
}

int tdb_write_convert(struct tdb_context *tdb, tdb_off_t off,
		      const void *rec, size_t len)
{
	int ret;
	if (unlikely((tdb->flags & TDB_CONVERT))) {
		void *conv = malloc(len);
		if (!conv) {
			tdb_logerr(tdb, TDB_ERR_OOM, TDB_LOG_ERROR,
				   "tdb_write: no memory converting"
				   " %zu bytes", len);
			return -1;
		}
		memcpy(conv, rec, len);
		ret = tdb->methods->twrite(tdb, off,
					   tdb_convert(tdb, conv, len), len);
		free(conv);
	} else
		ret = tdb->methods->twrite(tdb, off, rec, len);

	return ret;
}

int tdb_read_convert(struct tdb_context *tdb, tdb_off_t off,
		      void *rec, size_t len)
{
	int ret = tdb->methods->tread(tdb, off, rec, len);
	tdb_convert(tdb, rec, len);
	return ret;
}

int tdb_write_off(struct tdb_context *tdb, tdb_off_t off, tdb_off_t val)
{
	if (tdb->read_only) {
		tdb_logerr(tdb, TDB_ERR_RDONLY, TDB_LOG_USE_ERROR,
			   "Write to read-only database");
		return -1;
	}

	if (likely(!(tdb->flags & TDB_CONVERT))) {
		tdb_off_t *p = tdb->methods->direct(tdb, off, sizeof(*p),
						    true);
		if (p) {
			*p = val;
			return 0;
		}
	}
	return tdb_write_convert(tdb, off, &val, sizeof(val));
}

static void *_tdb_alloc_read(struct tdb_context *tdb, tdb_off_t offset,
			     tdb_len_t len, unsigned int prefix)
{
	void *buf;

	/* some systems don't like zero length malloc */
	buf = malloc(prefix + len ? prefix + len : 1);
	if (!buf) {
		tdb_logerr(tdb, TDB_ERR_OOM, TDB_LOG_USE_ERROR,
			   "tdb_alloc_read malloc failed len=%zu",
			   (size_t)(prefix + len));
	} else if (unlikely(tdb->methods->tread(tdb, offset, buf+prefix, len)
			    == -1)) {
		free(buf);
		buf = NULL;
	}
	return buf;
}

/* read a lump of data, allocating the space for it */
void *tdb_alloc_read(struct tdb_context *tdb, tdb_off_t offset, tdb_len_t len)
{
	return _tdb_alloc_read(tdb, offset, len, 0);
}

static int fill(struct tdb_context *tdb,
		const void *buf, size_t size,
		tdb_off_t off, tdb_len_t len)
{
	while (len) {
		size_t n = len > size ? size : len;
		ssize_t ret = pwrite(tdb->fd, buf, n, off);
		if (ret < n) {
			if (ret >= 0)
				errno = ENOSPC;

			tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				   "fill failed: %zi at %zu len=%zu (%s)",
				   ret, (size_t)off, (size_t)len,
				   strerror(errno));
			return -1;
		}
		len -= n;
		off += n;
	}
	return 0;
}

/* expand a file.  we prefer to use ftruncate, as that is what posix
  says to use for mmap expansion */
static int tdb_expand_file(struct tdb_context *tdb, tdb_len_t addition)
{
	char buf[8192];

	if (tdb->read_only) {
		tdb_logerr(tdb, TDB_ERR_RDONLY, TDB_LOG_USE_ERROR,
			   "Expand on read-only database");
		return -1;
	}

	if (tdb->flags & TDB_INTERNAL) {
		char *new = realloc(tdb->map_ptr, tdb->map_size + addition);
		if (!new) {
			tdb_logerr(tdb, TDB_ERR_OOM, TDB_LOG_ERROR,
				   "No memory to expand database");
			return -1;
		}
		tdb->map_ptr = new;
		tdb->map_size += addition;
	} else {
		/* Unmap before trying to write; old TDB claimed OpenBSD had
		 * problem with this otherwise. */
		tdb_munmap(tdb);

		/* If this fails, we try to fill anyway. */
		if (ftruncate(tdb->fd, tdb->map_size + addition))
			;

		/* now fill the file with something. This ensures that the
		   file isn't sparse, which would be very bad if we ran out of
		   disk. This must be done with write, not via mmap */
		memset(buf, 0x43, sizeof(buf));
		if (0 || fill(tdb, buf, sizeof(buf), tdb->map_size, addition) == -1)
			return -1;
		tdb->map_size += addition;
		tdb_mmap(tdb);
	}
	return 0;
}

const void *tdb_access_read(struct tdb_context *tdb,
			    tdb_off_t off, tdb_len_t len, bool convert)
{
	const void *ret = NULL;

	if (likely(!(tdb->flags & TDB_CONVERT)))
		ret = tdb->methods->direct(tdb, off, len, false);

	if (!ret) {
		struct tdb_access_hdr *hdr;
		hdr = _tdb_alloc_read(tdb, off, len, sizeof(*hdr));
		if (hdr) {
			hdr->next = tdb->access;
			tdb->access = hdr;
			ret = hdr + 1;
			if (convert)
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
		return NULL;
	}

	if (likely(!(tdb->flags & TDB_CONVERT)))
		ret = tdb->methods->direct(tdb, off, len, true);

	if (!ret) {
		struct tdb_access_hdr *hdr;
		hdr = _tdb_alloc_read(tdb, off, len, sizeof(*hdr));
		if (hdr) {
			hdr->next = tdb->access;
			tdb->access = hdr;
			hdr->off = off;
			hdr->len = len;
			hdr->convert = convert;
			ret = hdr + 1;
			if (convert)
				tdb_convert(tdb, (void *)ret, len);
		}
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

int tdb_access_commit(struct tdb_context *tdb, void *p)
{
	struct tdb_access_hdr *hdr, **hp = find_hdr(tdb, p);
	int ret = 0;

	if (hp) {
		hdr = *hp;
		if (hdr->convert)
			ret = tdb_write_convert(tdb, hdr->off, p, hdr->len);
		else
			ret = tdb_write(tdb, hdr->off, p, hdr->len);
		*hp = hdr->next;
		free(hdr);
	} else
		tdb->direct_access--;

	return ret;
}

static void *tdb_direct(struct tdb_context *tdb, tdb_off_t off, size_t len,
			bool write_mode)
{
	if (unlikely(!tdb->map_ptr))
		return NULL;

	if (unlikely(tdb_oob(tdb, off + len, true) == -1))
		return NULL;
	return (char *)tdb->map_ptr + off;
}

void add_stat_(struct tdb_context *tdb, uint64_t *s, size_t val)
{
	if ((uintptr_t)s < (uintptr_t)tdb->stats + tdb->stats->size)
		*s += val;
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
