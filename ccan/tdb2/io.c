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

	tdb->map_ptr = mmap(NULL, tdb->map_size, 
			    PROT_READ|(tdb->read_only? 0:PROT_WRITE), 
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

	if (fstat(tdb->fd, &st) == -1) {
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

static void *tdb_direct(struct tdb_context *tdb, tdb_off_t off, size_t len)
{
	if (unlikely(!tdb->map_ptr))
		return NULL;

	/* FIXME: We can do a subset of this! */
	if (tdb->transaction)
		return NULL;

	if (unlikely(tdb_oob(tdb, off + len, true) == -1))
		return NULL;
	return (char *)tdb->map_ptr + off;
}

/* Either make a copy into pad and return that, or return ptr into mmap. */
/* Note: pad has to be a real object, so we can't get here if len
 * overflows size_t */
void *tdb_get(struct tdb_context *tdb, tdb_off_t off, void *pad, size_t len)
{
	if (likely(!(tdb->flags & TDB_CONVERT))) {
		void *ret = tdb_direct(tdb, off, len);
		if (ret)
			return ret;
	}

	if (unlikely(tdb_oob(tdb, off + len, false) == -1))
		return NULL;

	if (tdb->methods->read(tdb, off, pad, len) == -1)
		return NULL;
	return tdb_convert(tdb, pad, len);
}

/* Endian conversion: we only ever deal with 8 byte quantities */
void *tdb_convert(const struct tdb_context *tdb, void *buf, tdb_len_t size)
{
	if (unlikely((tdb->flags & TDB_CONVERT))) {
		uint64_t i, *p = (uint64_t *)buf;
		for (i = 0; i < size / 8; i++)
			p[i] = bswap_64(p[i]);
	}
	return buf;
}

/* Return first non-zero offset in num offset array, or num. */
/* FIXME: Return the off? */
uint64_t tdb_find_nonzero_off(struct tdb_context *tdb, tdb_off_t off,
			      uint64_t num)
{
	uint64_t i, *val;
	bool alloc = false;

	val = tdb_direct(tdb, off, num * sizeof(tdb_off_t));
	if (!unlikely(val)) {
		val = tdb_alloc_read(tdb, off, num * sizeof(tdb_off_t));
		if (!val)
			return num;
		alloc = true;
	}

	for (i = 0; i < num; i++) {
		if (val[i])
			break;
	}
	if (unlikely(alloc))
		free(val);
	return i;
}

/* Return first zero offset in num offset array, or num. */
uint64_t tdb_find_zero_off(struct tdb_context *tdb, tdb_off_t off,
			   uint64_t num)
{
	uint64_t i, *val;
	bool alloc = false;

	val = tdb_direct(tdb, off, num * sizeof(tdb_off_t));
	if (!unlikely(val)) {
		val = tdb_alloc_read(tdb, off, num * sizeof(tdb_off_t));
		if (!val)
			return num;
		alloc = true;
	}

	for (i = 0; i < num; i++) {
		if (!val[i])
			break;
	}
	if (unlikely(alloc))
		free(val);
	return i;
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

int zero_out(struct tdb_context *tdb, tdb_off_t off, tdb_len_t len)
{
	void *p = tdb_direct(tdb, off, len);
	if (p) {
		memset(p, 0, len);
		return 0;
	} else {
		char buf[8192] = { 0 };
		return fill(tdb, buf, sizeof(buf), len, off);
	}
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
				 off, len, strerror(errno));
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
		      void *rec, size_t len)
{
	return tdb->methods->write(tdb, off, tdb_convert(tdb, rec, len), len);
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

/* read a lump of data, allocating the space for it */
void *tdb_alloc_read(struct tdb_context *tdb, tdb_off_t offset, tdb_len_t len)
{
	void *buf;

	/* some systems don't like zero length malloc */
	buf = malloc(len ? len : 1);
	if (unlikely(!buf)) {
		tdb->ecode = TDB_ERR_OOM;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_alloc_read malloc failed len=%lld\n",
			 (long long)len);
	} else if (unlikely(tdb->methods->read(tdb, offset, buf, len))) {
		free(buf);
		buf = NULL;
	}
	return buf;
}

uint64_t hash_record(struct tdb_context *tdb, tdb_off_t off)
{
	struct tdb_used_record pad, *r;
	void *key;
	uint64_t klen, hash;

	r = tdb_get(tdb, off, &pad, sizeof(pad));
	if (!r)
		/* FIXME */
		return 0;

	klen = rec_key_length(r);
	key = tdb_direct(tdb, off + sizeof(pad), klen);
	if (likely(key))
		return tdb_hash(tdb, key, klen);

	key = tdb_alloc_read(tdb, off + sizeof(pad), klen);
	if (unlikely(!key))
		return 0;
	hash = tdb_hash(tdb, key, klen);
	free(key);
	return hash;
}

/* Give a piece of tdb data to a parser */
int tdb_parse_data(struct tdb_context *tdb, TDB_DATA key,
		   tdb_off_t offset, tdb_len_t len,
		   int (*parser)(TDB_DATA key, TDB_DATA data,
				 void *private_data),
		   void *private_data)
{
	TDB_DATA data;
	int result;
	bool allocated = false;

	data.dsize = len;
	data.dptr = tdb_direct(tdb, offset, len);
	if (unlikely(!data.dptr)) {
		if (!(data.dptr = tdb_alloc_read(tdb, offset, len))) {
			return -1;
		}
		allocated = true;
	}
	result = parser(key, data, private_data);
	if (unlikely(allocated))
		free(data.dptr);
	return result;
}

/* expand a file.  we prefer to use ftruncate, as that is what posix
  says to use for mmap expansion */
static int tdb_expand_file(struct tdb_context *tdb,
			   tdb_len_t size, tdb_len_t addition)
{
	char buf[8192];

	if (tdb->read_only) {
		tdb->ecode = TDB_ERR_RDONLY;
		return -1;
	}

	/* If this fails, we try to fill anyway. */
	if (ftruncate(tdb->fd, size+addition))
		;

	/* now fill the file with something. This ensures that the
	   file isn't sparse, which would be very bad if we ran out of
	   disk. This must be done with write, not via mmap */
	memset(buf, 0x43, sizeof(buf));
	return fill(tdb, buf, sizeof(buf), addition, size);
}

const void *tdb_access_read(struct tdb_context *tdb,
			    tdb_off_t off, tdb_len_t len)
{
	const void *ret = tdb_direct(tdb, off, len);

	if (!ret)
		ret = tdb_alloc_read(tdb, off, len);
	return ret;
}

void tdb_access_release(struct tdb_context *tdb, const void *p)
{
	if (!tdb->map_ptr
	    || (char *)p < (char *)tdb->map_ptr
	    || (char *)p >= (char *)tdb->map_ptr + tdb->map_size)
		free((void *)p);
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


/* expand the database by expanding the underlying file and doing the
   mmap again if necessary */
int tdb_expand(struct tdb_context *tdb)
{
	struct tdb_record rec;
	tdb_off_t offset, new_size;	

	/* We have to lock every hash bucket and every free list. */
	do {
		

	if (tdb_lock(tdb, -1, F_WRLCK) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "lock failed in tdb_expand\n"));
		return -1;
	}

	/* must know about any previous expansions by another process */
	tdb->methods->tdb_oob(tdb, tdb->map_size + 1, 1);

	/* always make room for at least 100 more records, and at
           least 25% more space. Round the database up to a multiple
           of the page size */
	new_size = MAX(tdb->map_size + size*100, tdb->map_size * 1.25);
	size = TDB_ALIGN(new_size, tdb->page_size) - tdb->map_size;

	if (!(tdb->flags & TDB_INTERNAL))
		tdb_munmap(tdb);

	/*
	 * We must ensure the file is unmapped before doing this
	 * to ensure consistency with systems like OpenBSD where
	 * writes and mmaps are not consistent.
	 */

	/* expand the file itself */
	if (!(tdb->flags & TDB_INTERNAL)) {
		if (tdb->methods->tdb_expand_file(tdb, tdb->map_size, size) != 0)
			goto fail;
	}

	tdb->map_size += size;

	if (tdb->flags & TDB_INTERNAL) {
		char *new_map_ptr = (char *)realloc(tdb->map_ptr,
						    tdb->map_size);
		if (!new_map_ptr) {
			tdb->map_size -= size;
			goto fail;
		}
		tdb->map_ptr = new_map_ptr;
	} else {
		/*
		 * We must ensure the file is remapped before adding the space
		 * to ensure consistency with systems like OpenBSD where
		 * writes and mmaps are not consistent.
		 */

		/* We're ok if the mmap fails as we'll fallback to read/write */
		tdb_mmap(tdb);
	}

	/* form a new freelist record */
	memset(&rec,'\0',sizeof(rec));
	rec.rec_len = size - sizeof(rec);

	/* link it into the free list */
	offset = tdb->map_size - size;
	if (tdb_free(tdb, offset, &rec) == -1)
		goto fail;

	tdb_unlock(tdb, -1, F_WRLCK);
	return 0;
 fail:
	tdb_unlock(tdb, -1, F_WRLCK);
	return -1;
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

static const struct tdb_methods io_methods = {
	tdb_read,
	tdb_write,
	tdb_oob,
	tdb_expand_file,
};

/*
  initialise the default methods table
*/
void tdb_io_init(struct tdb_context *tdb)
{
	tdb->methods = &io_methods;
}
