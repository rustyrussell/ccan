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
		tdb->log(tdb, TDB_DEBUG_WARNING, tdb->log_priv,
			 "tdb_mmap failed for size %lld (%s)\n", 
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
	int ret;

	/* We can't hold pointers during this: we could unmap! */
	assert(!tdb->direct_access
	       || (tdb->flags & TDB_NOLOCK)
	       || tdb_has_expansion_lock(tdb));

	if (len <= tdb->map_size)
		return 0;
	if (tdb->flags & TDB_INTERNAL) {
		if (!probe) {
			/* Ensure ecode is set for log fn. */
			tdb->ecode = TDB_ERR_IO;
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "tdb_oob len %lld beyond internal"
				 " malloc size %lld\n",
				 (long long)len,
				 (long long)tdb->map_size);
		}
		return -1;
	}

	if (tdb_lock_expand(tdb, F_RDLCK) != 0)
		return -1;

	ret = fstat(tdb->fd, &st);

	tdb_unlock_expand(tdb, F_RDLCK);

	if (ret == -1) {
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}

	if (st.st_size < (size_t)len) {
		if (!probe) {
			/* Ensure ecode is set for log fn. */
			tdb->ecode = TDB_ERR_IO;
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "tdb_oob len %lld beyond eof at %lld\n",
				 (long long)len, (long long)st.st_size);
		}
		return -1;
	}

	/* Unmap, update size, remap */
	tdb_munmap(tdb);

	tdb->map_size = st.st_size;
	tdb_mmap(tdb);
	return 0;
}

/* Either make a copy into pad and return that, or return ptr into mmap. */
/* Note: pad has to be a real object, so we can't get here if len
 * overflows size_t */
void *tdb_get(struct tdb_context *tdb, tdb_off_t off, void *pad, size_t len)
{
	if (likely(!(tdb->flags & TDB_CONVERT))) {
		void *ret = tdb->methods->direct(tdb, off, len);
		if (ret)
			return ret;
	}
	return tdb_read_convert(tdb, off, pad, len) == -1 ? NULL : pad;
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
	void *p = tdb->methods->direct(tdb, off, len);
	if (p) {
		memset(p, 0, len);
		return 0;
	}
	while (len) {
		unsigned todo = len < sizeof(buf) ? len : sizeof(buf);
		if (tdb->methods->write(tdb, off, buf, todo) == -1)
			return -1;
		len -= todo;
		off += todo;
	}
	return 0;
}

tdb_off_t tdb_read_off(struct tdb_context *tdb, tdb_off_t off)
{
	tdb_off_t pad, *ret;

	ret = tdb_get(tdb, off, &pad, sizeof(pad));
	if (!ret) {
		return TDB_OFF_ERR;
	}
	return *ret;
}

/* Even on files, we can get partial writes due to signals. */
bool tdb_pwrite_all(int fd, const void *buf, size_t len, tdb_off_t off)
{
	while (len) {
		ssize_t ret;
		ret = pwrite(fd, buf, len, off);
		if (ret < 0)
			return false;
		if (ret == 0) {
			errno = ENOSPC;
			return false;
		}
		buf = (char *)buf + ret;
		off += ret;
		len -= ret;
	}
	return true;
}

/* Even on files, we can get partial reads due to signals. */
bool tdb_pread_all(int fd, void *buf, size_t len, tdb_off_t off)
{
	while (len) {
		ssize_t ret;
		ret = pread(fd, buf, len, off);
		if (ret < 0)
			return false;
		if (ret == 0) {
			/* ETOOSHORT? */
			errno = EWOULDBLOCK;
			return false;
		}
		buf = (char *)buf + ret;
		off += ret;
		len -= ret;
	}
	return true;
}

bool tdb_read_all(int fd, void *buf, size_t len)
{
	while (len) {
		ssize_t ret;
		ret = read(fd, buf, len);
		if (ret < 0)
			return false;
		if (ret == 0) {
			/* ETOOSHORT? */
			errno = EWOULDBLOCK;
			return false;
		}
		buf = (char *)buf + ret;
		len -= ret;
	}
	return true;
}

/* write a lump of data at a specified offset */
static int tdb_write(struct tdb_context *tdb, tdb_off_t off, 
		     const void *buf, tdb_len_t len)
{
	if (len == 0) {
		return 0;
	}

	if (tdb->read_only) {
		tdb->ecode = TDB_ERR_RDONLY;
		return -1;
	}

	if (tdb->methods->oob(tdb, off + len, 0) != 0)
		return -1;

	if (tdb->map_ptr) {
		memcpy(off + (char *)tdb->map_ptr, buf, len);
	} else {
		if (!tdb_pwrite_all(tdb->fd, buf, len, off)) {
			tdb->ecode = TDB_ERR_IO;
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "tdb_write failed at %llu len=%llu (%s)\n",
				 (long long)off, (long long)len,
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
		if (!tdb_pread_all(tdb->fd, buf, len, off)) {
			/* Ensure ecode is set for log fn. */
			tdb->ecode = TDB_ERR_IO;
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "tdb_read failed at %lld "
				 "len=%lld (%s) map_size=%lld\n",
				 (long long)off, (long long)len,
				 strerror(errno),
				 (long long)tdb->map_size);
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
			tdb->ecode = TDB_ERR_OOM;
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "tdb_write: no memory converting %zu bytes\n",
				 len);
			return -1;
		}
		memcpy(conv, rec, len);
		ret = tdb->methods->write(tdb, off,
					  tdb_convert(tdb, conv, len), len);
		free(conv);
	} else
		ret = tdb->methods->write(tdb, off, rec, len);

	return ret;
}

int tdb_read_convert(struct tdb_context *tdb, tdb_off_t off,
		      void *rec, size_t len)
{
	int ret = tdb->methods->read(tdb, off, rec, len);
	tdb_convert(tdb, rec, len);
	return ret;
}

int tdb_write_off(struct tdb_context *tdb, tdb_off_t off, tdb_off_t val)
{
	return tdb_write_convert(tdb, off, &val, sizeof(val));
}

static void *_tdb_alloc_read(struct tdb_context *tdb, tdb_off_t offset,
			     tdb_len_t len, unsigned int prefix)
{
	void *buf;

	/* some systems don't like zero length malloc */
	buf = malloc(prefix + len ? prefix + len : 1);
	if (unlikely(!buf)) {
		tdb->ecode = TDB_ERR_OOM;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_alloc_read malloc failed len=%lld\n",
			 (long long)prefix + len);
	} else if (unlikely(tdb->methods->read(tdb, offset, buf+prefix, len))) {
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

		if (!tdb_pwrite_all(tdb->fd, buf, n, off)) {
			tdb->ecode = TDB_ERR_IO;
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "fill write failed: giving up!\n");
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
		tdb->ecode = TDB_ERR_RDONLY;
		return -1;
	}

	if (tdb->flags & TDB_INTERNAL) {
		char *new = realloc(tdb->map_ptr, tdb->map_size + addition);
		if (!new) {
			tdb->ecode = TDB_ERR_OOM;
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
		if (fill(tdb, buf, sizeof(buf), tdb->map_size, addition) == -1)
			return -1;
		tdb->map_size += addition;
		tdb_mmap(tdb);
	}
	return 0;
}

/* This is only neded for tdb_access_commit, but used everywhere to simplify. */
struct tdb_access_hdr {
	tdb_off_t off;
	tdb_len_t len;
	bool convert;
};

const void *tdb_access_read(struct tdb_context *tdb,
			    tdb_off_t off, tdb_len_t len, bool convert)
{
	const void *ret = NULL;	

	if (likely(!(tdb->flags & TDB_CONVERT)))
		ret = tdb->methods->direct(tdb, off, len);

	if (!ret) {
		struct tdb_access_hdr *hdr;
		hdr = _tdb_alloc_read(tdb, off, len, sizeof(*hdr));
		if (hdr) {
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

	if (likely(!(tdb->flags & TDB_CONVERT)))
		ret = tdb->methods->direct(tdb, off, len);

	if (!ret) {
		struct tdb_access_hdr *hdr;
		hdr = _tdb_alloc_read(tdb, off, len, sizeof(*hdr));
		if (hdr) {
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

void tdb_access_release(struct tdb_context *tdb, const void *p)
{
	if (!tdb->map_ptr
	    || (char *)p < (char *)tdb->map_ptr
	    || (char *)p >= (char *)tdb->map_ptr + tdb->map_size)
		free((struct tdb_access_hdr *)p - 1);
	else
		tdb->direct_access--;
}

int tdb_access_commit(struct tdb_context *tdb, void *p)
{
	int ret = 0;

	if (!tdb->map_ptr
	    || (char *)p < (char *)tdb->map_ptr
	    || (char *)p >= (char *)tdb->map_ptr + tdb->map_size) {
		struct tdb_access_hdr *hdr;

		hdr = (struct tdb_access_hdr *)p - 1;
		if (hdr->convert)
			ret = tdb_write_convert(tdb, hdr->off, p, hdr->len);
		else
			ret = tdb_write(tdb, hdr->off, p, hdr->len);
		free(hdr);
	} else
		tdb->direct_access--;

	return ret;
}

#if 0
/* write a lump of data at a specified offset */
static int tdb_write(struct tdb_context *tdb, tdb_off_t off, 
		     const void *buf, tdb_len_t len)
{
	if (len == 0) {
		return 0;
	}

	if (tdb->read_only || tdb->traverse_read) {
		tdb->ecode = TDB_ERR_RDONLY;
		return -1;
	}

	if (tdb->methods->tdb_oob(tdb, off + len, 0) != 0)
		return -1;

	if (tdb->map_ptr) {
		memcpy(off + (char *)tdb->map_ptr, buf, len);
	} else {
		ssize_t written = pwrite(tdb->fd, buf, len, off);
		if ((written != (ssize_t)len) && (written != -1)) {
			/* try once more */
			tdb->ecode = TDB_ERR_IO;
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_write: wrote only "
				 "%d of %d bytes at %d, trying once more\n",
				 (int)written, len, off));
			written = pwrite(tdb->fd, (const char *)buf+written,
					 len-written,
					 off+written);
		}
		if (written == -1) {
			/* Ensure ecode is set for log fn. */
			tdb->ecode = TDB_ERR_IO;
			TDB_LOG((tdb, TDB_DEBUG_FATAL,"tdb_write failed at %d "
				 "len=%d (%s)\n", off, len, strerror(errno)));
			return -1;
		} else if (written != (ssize_t)len) {
			tdb->ecode = TDB_ERR_IO;
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_write: failed to "
				 "write %d bytes at %d in two attempts\n",
				 len, off));
			return -1;
		}
	}
	return 0;
}



/*
  do an unlocked scan of the hash table heads to find the next non-zero head. The value
  will then be confirmed with the lock held
*/		
static void tdb_next_hash_chain(struct tdb_context *tdb, uint32_t *chain)
{
	uint32_t h = *chain;
	if (tdb->map_ptr) {
		for (;h < tdb->header.hash_size;h++) {
			if (0 != *(uint32_t *)(TDB_HASH_TOP(h) + (unsigned char *)tdb->map_ptr)) {
				break;
			}
		}
	} else {
		uint32_t off=0;
		for (;h < tdb->header.hash_size;h++) {
			if (tdb_ofs_read(tdb, TDB_HASH_TOP(h), &off) != 0 || off != 0) {
				break;
			}
		}
	}
	(*chain) = h;
}

/* read/write a tdb_off_t */
int tdb_ofs_read(struct tdb_context *tdb, tdb_off_t offset, tdb_off_t *d)
{
	return tdb->methods->tdb_read(tdb, offset, (char*)d, sizeof(*d), DOCONV());
}

int tdb_ofs_write(struct tdb_context *tdb, tdb_off_t offset, tdb_off_t *d)
{
	tdb_off_t off = *d;
	return tdb->methods->tdb_write(tdb, offset, CONVERT(off), sizeof(*d));
}


/* read/write a record */
int tdb_rec_read(struct tdb_context *tdb, tdb_off_t offset, struct tdb_record *rec)
{
	if (tdb->methods->tdb_read(tdb, offset, rec, sizeof(*rec),DOCONV()) == -1)
		return -1;
	if (TDB_BAD_MAGIC(rec)) {
		/* Ensure ecode is set for log fn. */
		tdb->ecode = TDB_ERR_CORRUPT;
		TDB_LOG((tdb, TDB_DEBUG_FATAL,"tdb_rec_read bad magic 0x%x at offset=%d\n", rec->magic, offset));
		return -1;
	}
	return tdb->methods->tdb_oob(tdb, rec->next+sizeof(*rec), 0);
}

int tdb_rec_write(struct tdb_context *tdb, tdb_off_t offset, struct tdb_record *rec)
{
	struct tdb_record r = *rec;
	return tdb->methods->tdb_write(tdb, offset, CONVERT(r), sizeof(r));
}
#endif

static void *tdb_direct(struct tdb_context *tdb, tdb_off_t off, size_t len)
{
	if (unlikely(!tdb->map_ptr))
		return NULL;

	if (unlikely(tdb_oob(tdb, off + len, true) == -1))
		return NULL;
	return (char *)tdb->map_ptr + off;
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
