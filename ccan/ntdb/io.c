 /*
   Unix SMB/CIFS implementation.

   trivial database library

   Copyright (C) Andrew Tridgell              1999-2005
   Copyright (C) Paul `Rusty' Russell		   2000
   Copyright (C) Jeremy Allison			   2000-2003
   Copyright (C) Rusty Russell			   2010

     ** NOTE! The following LGPL license applies to the ntdb
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

static void free_old_mmaps(struct ntdb_context *ntdb)
{
	struct ntdb_old_mmap *i;

	assert(ntdb->file->direct_count == 0);

	while ((i = ntdb->file->old_mmaps) != NULL) {
		ntdb->file->old_mmaps = i->next;
		if (ntdb->flags & NTDB_INTERNAL) {
			ntdb->free_fn(i->map_ptr, ntdb->alloc_data);
		} else {
			munmap(i->map_ptr, i->map_size);
		}
		ntdb->free_fn(i, ntdb->alloc_data);
	}
}

static enum NTDB_ERROR save_old_map(struct ntdb_context *ntdb)
{
	struct ntdb_old_mmap *old;

	assert(ntdb->file->direct_count);

	old = ntdb->alloc_fn(ntdb->file, sizeof(*old), ntdb->alloc_data);
	if (!old) {
		return ntdb_logerr(ntdb, NTDB_ERR_OOM, NTDB_LOG_ERROR,
				   "save_old_map alloc failed");
	}
	old->next = ntdb->file->old_mmaps;
	old->map_ptr = ntdb->file->map_ptr;
	old->map_size = ntdb->file->map_size;
	ntdb->file->old_mmaps = old;

	return NTDB_SUCCESS;
}

enum NTDB_ERROR ntdb_munmap(struct ntdb_context *ntdb)
{
	if (ntdb->file->fd == -1) {
		return NTDB_SUCCESS;
	}

	if (!ntdb->file->map_ptr) {
		return NTDB_SUCCESS;
	}

	/* We can't unmap now if there are accessors. */
	if (ntdb->file->direct_count) {
		return save_old_map(ntdb);
	} else {
		munmap(ntdb->file->map_ptr, ntdb->file->map_size);
		ntdb->file->map_ptr = NULL;
	}
	return NTDB_SUCCESS;
}

enum NTDB_ERROR ntdb_mmap(struct ntdb_context *ntdb)
{
	int mmap_flags;

	if (ntdb->flags & NTDB_INTERNAL)
		return NTDB_SUCCESS;

#ifndef HAVE_INCOHERENT_MMAP
	if (ntdb->flags & NTDB_NOMMAP)
		return NTDB_SUCCESS;
#endif

	if ((ntdb->open_flags & O_ACCMODE) == O_RDONLY)
		mmap_flags = PROT_READ;
	else
		mmap_flags = PROT_READ | PROT_WRITE;

	/* size_t can be smaller than off_t. */
	if ((size_t)ntdb->file->map_size == ntdb->file->map_size) {
		ntdb->file->map_ptr = mmap(NULL, ntdb->file->map_size,
					  mmap_flags,
					  MAP_SHARED, ntdb->file->fd, 0);
	} else
		ntdb->file->map_ptr = MAP_FAILED;

	/*
	 * NB. When mmap fails it returns MAP_FAILED *NOT* NULL !!!!
	 */
	if (ntdb->file->map_ptr == MAP_FAILED) {
		ntdb->file->map_ptr = NULL;
#ifdef HAVE_INCOHERENT_MMAP
		/* Incoherent mmap means everyone must mmap! */
		return ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
				  "ntdb_mmap failed for size %lld (%s)",
				  (long long)ntdb->file->map_size,
				  strerror(errno));
#else
		ntdb_logerr(ntdb, NTDB_SUCCESS, NTDB_LOG_WARNING,
			   "ntdb_mmap failed for size %lld (%s)",
			   (long long)ntdb->file->map_size, strerror(errno));
#endif
	}
	return NTDB_SUCCESS;
}

/* check for an out of bounds access - if it is out of bounds then
   see if the database has been expanded by someone else and expand
   if necessary
   note that "len" is the minimum length needed for the db.

   If probe is true, len being too large isn't a failure.
*/
static enum NTDB_ERROR ntdb_normal_oob(struct ntdb_context *ntdb,
				       ntdb_off_t off, ntdb_len_t len,
				       bool probe)
{
	struct stat st;
	enum NTDB_ERROR ecode;

	if (len + off < len) {
		if (probe)
			return NTDB_SUCCESS;

		return ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
				  "ntdb_oob off %llu len %llu wrap\n",
				  (long long)off, (long long)len);
	}

	if (ntdb->flags & NTDB_INTERNAL) {
		if (probe)
			return NTDB_SUCCESS;

		ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
			   "ntdb_oob len %lld beyond internal"
			   " alloc size %lld",
			   (long long)(off + len),
			   (long long)ntdb->file->map_size);
		return NTDB_ERR_IO;
	}

	ecode = ntdb_lock_expand(ntdb, F_RDLCK);
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	if (fstat(ntdb->file->fd, &st) != 0) {
		ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
			   "Failed to fstat file: %s", strerror(errno));
		ntdb_unlock_expand(ntdb, F_RDLCK);
		return NTDB_ERR_IO;
	}

	ntdb_unlock_expand(ntdb, F_RDLCK);

	if (st.st_size < off + len) {
		if (probe)
			return NTDB_SUCCESS;

		ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
			   "ntdb_oob len %llu beyond eof at %llu",
			   (long long)(off + len), (long long)st.st_size);
		return NTDB_ERR_IO;
	}

	/* Unmap, update size, remap */
	ecode = ntdb_munmap(ntdb);
	if (ecode) {
		return ecode;
	}

	ntdb->file->map_size = st.st_size;
	return ntdb_mmap(ntdb);
}

/* Endian conversion: we only ever deal with 8 byte quantities */
void *ntdb_convert(const struct ntdb_context *ntdb, void *buf, ntdb_len_t size)
{
	assert(size % 8 == 0);
	if (unlikely((ntdb->flags & NTDB_CONVERT)) && buf) {
		uint64_t i, *p = (uint64_t *)buf;
		for (i = 0; i < size / 8; i++)
			p[i] = bswap_64(p[i]);
	}
	return buf;
}

/* Return first non-zero offset in offset array, or end, or -ve error. */
/* FIXME: Return the off? */
uint64_t ntdb_find_nonzero_off(struct ntdb_context *ntdb,
			      ntdb_off_t base, uint64_t start, uint64_t end)
{
	uint64_t i;
	const uint64_t *val;

	/* Zero vs non-zero is the same unconverted: minor optimization. */
	val = ntdb_access_read(ntdb, base + start * sizeof(ntdb_off_t),
			      (end - start) * sizeof(ntdb_off_t), false);
	if (NTDB_PTR_IS_ERR(val)) {
		return NTDB_ERR_TO_OFF(NTDB_PTR_ERR(val));
	}

	for (i = 0; i < (end - start); i++) {
		if (val[i])
			break;
	}
	ntdb_access_release(ntdb, val);
	return start + i;
}

/* Return first zero offset in num offset array, or num, or -ve error. */
uint64_t ntdb_find_zero_off(struct ntdb_context *ntdb, ntdb_off_t off,
			   uint64_t num)
{
	uint64_t i;
	const uint64_t *val;

	/* Zero vs non-zero is the same unconverted: minor optimization. */
	val = ntdb_access_read(ntdb, off, num * sizeof(ntdb_off_t), false);
	if (NTDB_PTR_IS_ERR(val)) {
		return NTDB_ERR_TO_OFF(NTDB_PTR_ERR(val));
	}

	for (i = 0; i < num; i++) {
		if (!val[i])
			break;
	}
	ntdb_access_release(ntdb, val);
	return i;
}

enum NTDB_ERROR zero_out(struct ntdb_context *ntdb, ntdb_off_t off, ntdb_len_t len)
{
	char buf[8192] = { 0 };
	void *p = ntdb->io->direct(ntdb, off, len, true);
	enum NTDB_ERROR ecode = NTDB_SUCCESS;

	assert(!(ntdb->flags & NTDB_RDONLY));
	if (NTDB_PTR_IS_ERR(p)) {
		return NTDB_PTR_ERR(p);
	}
	if (p) {
		memset(p, 0, len);
		return ecode;
	}
	while (len) {
		unsigned todo = len < sizeof(buf) ? len : sizeof(buf);
		ecode = ntdb->io->twrite(ntdb, off, buf, todo);
		if (ecode != NTDB_SUCCESS) {
			break;
		}
		len -= todo;
		off += todo;
	}
	return ecode;
}

/* write a lump of data at a specified offset */
static enum NTDB_ERROR ntdb_write(struct ntdb_context *ntdb, ntdb_off_t off,
				const void *buf, ntdb_len_t len)
{
	enum NTDB_ERROR ecode;

	if (ntdb->flags & NTDB_RDONLY) {
		return ntdb_logerr(ntdb, NTDB_ERR_RDONLY, NTDB_LOG_USE_ERROR,
				  "Write to read-only database");
	}

	ecode = ntdb_oob(ntdb, off, len, false);
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	if (ntdb->file->map_ptr) {
		memcpy(off + (char *)ntdb->file->map_ptr, buf, len);
	} else {
#ifdef HAVE_INCOHERENT_MMAP
		return NTDB_ERR_IO;
#else
		ssize_t ret;
		ret = pwrite(ntdb->file->fd, buf, len, off);
		if (ret != len) {
			/* This shouldn't happen: we avoid sparse files. */
			if (ret >= 0)
				errno = ENOSPC;

			return ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
					  "ntdb_write: %zi at %zu len=%zu (%s)",
					  ret, (size_t)off, (size_t)len,
					  strerror(errno));
		}
#endif
	}
	return NTDB_SUCCESS;
}

/* read a lump of data at a specified offset */
static enum NTDB_ERROR ntdb_read(struct ntdb_context *ntdb, ntdb_off_t off,
			       void *buf, ntdb_len_t len)
{
	enum NTDB_ERROR ecode;

	ecode = ntdb_oob(ntdb, off, len, false);
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	if (ntdb->file->map_ptr) {
		memcpy(buf, off + (char *)ntdb->file->map_ptr, len);
	} else {
#ifdef HAVE_INCOHERENT_MMAP
		return NTDB_ERR_IO;
#else
		ssize_t r = pread(ntdb->file->fd, buf, len, off);
		if (r != len) {
			return ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
					  "ntdb_read failed with %zi at %zu "
					  "len=%zu (%s) map_size=%zu",
					  r, (size_t)off, (size_t)len,
					  strerror(errno),
					  (size_t)ntdb->file->map_size);
		}
#endif
	}
	return NTDB_SUCCESS;
}

enum NTDB_ERROR ntdb_write_convert(struct ntdb_context *ntdb, ntdb_off_t off,
				 const void *rec, size_t len)
{
	enum NTDB_ERROR ecode;

	if (unlikely((ntdb->flags & NTDB_CONVERT))) {
		void *conv = ntdb->alloc_fn(ntdb, len, ntdb->alloc_data);
		if (!conv) {
			return ntdb_logerr(ntdb, NTDB_ERR_OOM, NTDB_LOG_ERROR,
					  "ntdb_write: no memory converting"
					  " %zu bytes", len);
		}
		memcpy(conv, rec, len);
		ecode = ntdb->io->twrite(ntdb, off,
					 ntdb_convert(ntdb, conv, len), len);
		ntdb->free_fn(conv, ntdb->alloc_data);
	} else {
		ecode = ntdb->io->twrite(ntdb, off, rec, len);
	}
	return ecode;
}

enum NTDB_ERROR ntdb_read_convert(struct ntdb_context *ntdb, ntdb_off_t off,
				void *rec, size_t len)
{
	enum NTDB_ERROR ecode = ntdb->io->tread(ntdb, off, rec, len);
	ntdb_convert(ntdb, rec, len);
	return ecode;
}

static void *_ntdb_alloc_read(struct ntdb_context *ntdb, ntdb_off_t offset,
			     ntdb_len_t len, unsigned int prefix)
{
	unsigned char *buf;
	enum NTDB_ERROR ecode;

	/* some systems don't like zero length malloc */
	buf = ntdb->alloc_fn(ntdb, prefix + len ? prefix + len : 1,
			  ntdb->alloc_data);
	if (!buf) {
		ntdb_logerr(ntdb, NTDB_ERR_OOM, NTDB_LOG_ERROR,
			   "ntdb_alloc_read alloc failed len=%zu",
			   (size_t)(prefix + len));
		return NTDB_ERR_PTR(NTDB_ERR_OOM);
	} else {
		ecode = ntdb->io->tread(ntdb, offset, buf+prefix, len);
		if (unlikely(ecode != NTDB_SUCCESS)) {
			ntdb->free_fn(buf, ntdb->alloc_data);
			return NTDB_ERR_PTR(ecode);
		}
	}
	return buf;
}

/* read a lump of data, allocating the space for it */
void *ntdb_alloc_read(struct ntdb_context *ntdb, ntdb_off_t offset, ntdb_len_t len)
{
	return _ntdb_alloc_read(ntdb, offset, len, 0);
}

static enum NTDB_ERROR fill(struct ntdb_context *ntdb,
			   const void *buf, size_t size,
			   ntdb_off_t off, ntdb_len_t len)
{
	while (len) {
		size_t n = len > size ? size : len;
		ssize_t ret = pwrite(ntdb->file->fd, buf, n, off);
		if (ret != n) {
			if (ret >= 0)
				errno = ENOSPC;

			return ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
					  "fill failed:"
					  " %zi at %zu len=%zu (%s)",
					  ret, (size_t)off, (size_t)len,
					  strerror(errno));
		}
		len -= n;
		off += n;
	}
	return NTDB_SUCCESS;
}

/* expand a file.  we prefer to use ftruncate, as that is what posix
  says to use for mmap expansion */
static enum NTDB_ERROR ntdb_expand_file(struct ntdb_context *ntdb,
				      ntdb_len_t addition)
{
	char buf[8192];
	enum NTDB_ERROR ecode;

	assert((ntdb->file->map_size + addition) % NTDB_PGSIZE == 0);
	if (ntdb->flags & NTDB_RDONLY) {
		return ntdb_logerr(ntdb, NTDB_ERR_RDONLY, NTDB_LOG_USE_ERROR,
				  "Expand on read-only database");
	}

	if (ntdb->flags & NTDB_INTERNAL) {
		char *new;

		/* Can't free it if we have direct accesses. */
		if (ntdb->file->direct_count) {
			ecode = save_old_map(ntdb);
			if (ecode) {
				return ecode;
			}
			new = ntdb->alloc_fn(ntdb->file,
					     ntdb->file->map_size + addition,
					     ntdb->alloc_data);
			if (new) {
				memcpy(new, ntdb->file->map_ptr,
				       ntdb->file->map_size);
			}
		} else {
			new = ntdb->expand_fn(ntdb->file->map_ptr,
					      ntdb->file->map_size + addition,
					      ntdb->alloc_data);
		}
		if (!new) {
			return ntdb_logerr(ntdb, NTDB_ERR_OOM, NTDB_LOG_ERROR,
					  "No memory to expand database");
		}
		ntdb->file->map_ptr = new;
		ntdb->file->map_size += addition;
		return NTDB_SUCCESS;
	} else {
		/* Unmap before trying to write; old NTDB claimed OpenBSD had
		 * problem with this otherwise. */
		ecode = ntdb_munmap(ntdb);
		if (ecode) {
			return ecode;
		}

		/* If this fails, we try to fill anyway. */
		if (ftruncate(ntdb->file->fd, ntdb->file->map_size + addition))
			;

		/* now fill the file with something. This ensures that the
		   file isn't sparse, which would be very bad if we ran out of
		   disk. This must be done with write, not via mmap */
		memset(buf, 0x43, sizeof(buf));
		ecode = fill(ntdb, buf, sizeof(buf), ntdb->file->map_size,
			     addition);
		if (ecode != NTDB_SUCCESS)
			return ecode;
		ntdb->file->map_size += addition;
		return ntdb_mmap(ntdb);
	}
}

const void *ntdb_access_read(struct ntdb_context *ntdb,
			    ntdb_off_t off, ntdb_len_t len, bool convert)
{
	void *ret = NULL;

	if (likely(!(ntdb->flags & NTDB_CONVERT))) {
		ret = ntdb->io->direct(ntdb, off, len, false);

		if (NTDB_PTR_IS_ERR(ret)) {
			return ret;
		}
	}
	if (!ret) {
		struct ntdb_access_hdr *hdr;
		hdr = _ntdb_alloc_read(ntdb, off, len, sizeof(*hdr));
		if (NTDB_PTR_IS_ERR(hdr)) {
			return hdr;
		}
		hdr->next = ntdb->access;
		ntdb->access = hdr;
		ret = hdr + 1;
		if (convert) {
			ntdb_convert(ntdb, (void *)ret, len);
		}
	} else {
		ntdb->file->direct_count++;
	}

	return ret;
}

void *ntdb_access_write(struct ntdb_context *ntdb,
		       ntdb_off_t off, ntdb_len_t len, bool convert)
{
	void *ret = NULL;

	if (ntdb->flags & NTDB_RDONLY) {
		ntdb_logerr(ntdb, NTDB_ERR_RDONLY, NTDB_LOG_USE_ERROR,
			   "Write to read-only database");
		return NTDB_ERR_PTR(NTDB_ERR_RDONLY);
	}

	if (likely(!(ntdb->flags & NTDB_CONVERT))) {
		ret = ntdb->io->direct(ntdb, off, len, true);

		if (NTDB_PTR_IS_ERR(ret)) {
			return ret;
		}
	}

	if (!ret) {
		struct ntdb_access_hdr *hdr;
		hdr = _ntdb_alloc_read(ntdb, off, len, sizeof(*hdr));
		if (NTDB_PTR_IS_ERR(hdr)) {
			return hdr;
		}
		hdr->next = ntdb->access;
		ntdb->access = hdr;
		hdr->off = off;
		hdr->len = len;
		hdr->convert = convert;
		ret = hdr + 1;
		if (convert)
			ntdb_convert(ntdb, (void *)ret, len);
	} else {
		ntdb->file->direct_count++;
	}
	return ret;
}

static struct ntdb_access_hdr **find_hdr(struct ntdb_context *ntdb, const void *p)
{
	struct ntdb_access_hdr **hp;

	for (hp = &ntdb->access; *hp; hp = &(*hp)->next) {
		if (*hp + 1 == p)
			return hp;
	}
	return NULL;
}

void ntdb_access_release(struct ntdb_context *ntdb, const void *p)
{
	struct ntdb_access_hdr *hdr, **hp = find_hdr(ntdb, p);

	if (hp) {
		hdr = *hp;
		*hp = hdr->next;
		ntdb->free_fn(hdr, ntdb->alloc_data);
	} else {
		if (--ntdb->file->direct_count == 0) {
			free_old_mmaps(ntdb);
		}
	}
}

enum NTDB_ERROR ntdb_access_commit(struct ntdb_context *ntdb, void *p)
{
	struct ntdb_access_hdr *hdr, **hp = find_hdr(ntdb, p);
	enum NTDB_ERROR ecode;

	if (hp) {
		hdr = *hp;
		if (hdr->convert)
			ecode = ntdb_write_convert(ntdb, hdr->off, p, hdr->len);
		else
			ecode = ntdb_write(ntdb, hdr->off, p, hdr->len);
		*hp = hdr->next;
		ntdb->free_fn(hdr, ntdb->alloc_data);
	} else {
		if (--ntdb->file->direct_count == 0) {
			free_old_mmaps(ntdb);
		}
		ecode = NTDB_SUCCESS;
	}

	return ecode;
}

static void *ntdb_direct(struct ntdb_context *ntdb, ntdb_off_t off, size_t len,
			bool write_mode)
{
	enum NTDB_ERROR ecode;

	if (unlikely(!ntdb->file->map_ptr))
		return NULL;

	ecode = ntdb_oob(ntdb, off, len, false);
	if (unlikely(ecode != NTDB_SUCCESS))
		return NTDB_ERR_PTR(ecode);
	return (char *)ntdb->file->map_ptr + off;
}

static ntdb_off_t ntdb_read_normal_off(struct ntdb_context *ntdb,
				       ntdb_off_t off)
{
	ntdb_off_t ret;
	enum NTDB_ERROR ecode;
	ntdb_off_t *p;

	p = ntdb_direct(ntdb, off, sizeof(*p), false);
	if (NTDB_PTR_IS_ERR(p)) {
		return NTDB_ERR_TO_OFF(NTDB_PTR_ERR(p));
	}
	if (likely(p)) {
		return *p;
	}

	ecode = ntdb_read(ntdb, off, &ret, sizeof(ret));
	if (ecode != NTDB_SUCCESS) {
		return NTDB_ERR_TO_OFF(ecode);
	}
	return ret;
}

static ntdb_off_t ntdb_read_convert_off(struct ntdb_context *ntdb,
					ntdb_off_t off)
{
	ntdb_off_t ret;
	enum NTDB_ERROR ecode;

	ecode = ntdb_read_convert(ntdb, off, &ret, sizeof(ret));
	if (ecode != NTDB_SUCCESS) {
		return NTDB_ERR_TO_OFF(ecode);
	}
	return ret;
}

static enum NTDB_ERROR ntdb_write_normal_off(struct ntdb_context *ntdb,
					     ntdb_off_t off, ntdb_off_t val)
{
	ntdb_off_t *p;

	p = ntdb_direct(ntdb, off, sizeof(*p), true);
	if (NTDB_PTR_IS_ERR(p)) {
		return NTDB_PTR_ERR(p);
	}
	if (likely(p)) {
		*p = val;
		return NTDB_SUCCESS;
	}
	return ntdb_write(ntdb, off, &val, sizeof(val));
}

static enum NTDB_ERROR ntdb_write_convert_off(struct ntdb_context *ntdb,
					      ntdb_off_t off, ntdb_off_t val)
{
	return ntdb_write_convert(ntdb, off, &val, sizeof(val));
}

void ntdb_inc_seqnum(struct ntdb_context *ntdb)
{
	ntdb_off_t seq;

	if (likely(!(ntdb->flags & NTDB_CONVERT))) {
		int64_t *direct;

		direct = ntdb->io->direct(ntdb,
					 offsetof(struct ntdb_header, seqnum),
					 sizeof(*direct), true);
		if (likely(direct)) {
			/* Don't let it go negative, even briefly */
			if (unlikely((*direct) + 1) < 0)
				*direct = 0;
			(*direct)++;
			return;
		}
	}

	seq = ntdb_read_off(ntdb, offsetof(struct ntdb_header, seqnum));
	if (!NTDB_OFF_IS_ERR(seq)) {
		seq++;
		if (unlikely((int64_t)seq < 0))
			seq = 0;
		ntdb_write_off(ntdb, offsetof(struct ntdb_header, seqnum), seq);
	}
}

static const struct ntdb_methods io_methods = {
	ntdb_read,
	ntdb_write,
	ntdb_normal_oob,
	ntdb_expand_file,
	ntdb_direct,
	ntdb_read_normal_off,
	ntdb_write_normal_off,
};

static const struct ntdb_methods io_convert_methods = {
	ntdb_read,
	ntdb_write,
	ntdb_normal_oob,
	ntdb_expand_file,
	ntdb_direct,
	ntdb_read_convert_off,
	ntdb_write_convert_off,
};

/*
  initialise the default methods table
*/
void ntdb_io_init(struct ntdb_context *ntdb)
{
	if (ntdb->flags & NTDB_CONVERT)
		ntdb->io = &io_convert_methods;
	else
		ntdb->io = &io_methods;
}
