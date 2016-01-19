/* Licensed under Apache License v2.0 - see LICENSE file for details */
#include "config.h"
#include "rszshm.h"

#define _XOPEN_SOURCE 700
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/file.h>

#define pgup(x, pgsz) (((x) + (pgsz) - 1) & ~((pgsz) - 1))

void *rszshm_mk(struct rszshm *r, size_t flen, const char *fname, struct rszshm_scan scan)
{
	long pgsz = sysconf(_SC_PAGE_SIZE);
	int i, errno_;
	char *m, *tgt, *p = NULL;

	if (!r || flen == 0 || scan.len < flen + sizeof(*r->hdr) ||
	    !scan.start || scan.len == 0 || scan.hop == 0 || scan.iter == 0 ||
	    (fname && strnlen(fname, RSZSHM_PATH_MAX) == RSZSHM_PATH_MAX)) {
		errno = EINVAL;
		return NULL;
	}

	*r = (typeof(*r)) { -1, 0, "", NULL, NULL };
	strcpy(r->fname, fname ? fname : RSZSHM_DFLT_FNAME);

	flen = pgup(flen + sizeof(*r->hdr), pgsz);
	scan.len = pgup(scan.len, pgsz);

	for (i = 1, tgt = scan.start; i <= scan.iter; i++) {
		m = mmap(tgt, scan.len, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_NORESERVE, -1, 0);
		if (m == MAP_FAILED)
			return NULL;
		if (m == tgt)
			break;
		munmap(m, scan.len);
		m = NULL;
		tgt += (i % 2 == 0 ? 1 : -1) * i * scan.hop;
	}
	if (!m) {
		errno = ENOSPC;
		return NULL;
	}

	if ((p = strstr(r->fname, "XXXXXX/")) != NULL) {
		p += 6;
		*p = '\0';
		if (!mkdtemp(r->fname))
			goto err;
		*p = '/';
	}

	if ((r->fd = open(r->fname, O_CREAT|O_EXCL|O_RDWR, p ? 0600 : 0666)) == -1)
		goto err;

	if (ftruncate(r->fd, flen) == -1)
		goto err;

	if (mmap(m, flen, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, r->fd, 0) == MAP_FAILED)
		goto err;

	*(r->hdr = (typeof(r->hdr)) m) = (typeof(*r->hdr)) { flen, scan.len, m };

	if (msync(m, sizeof(*r->hdr), MS_SYNC) == -1)
		goto err;

	r->flen = flen;
	r->cap = flen - sizeof(*r->hdr);
	r->dat = m + sizeof(*r->hdr);

	return r->dat;

err:
	errno_ = errno;
	if (m && m != MAP_FAILED)
		munmap(m, scan.len);
	if (r->fd != -1) {
		close(r->fd);
		unlink(r->fname);
	}
	if (p) {
		*p = '\0';
		rmdir(r->fname);
		*p = '/';
	}
	errno = errno_;
	return NULL;
}

void *rszshm_at(struct rszshm *r, const char *fname)
{
	struct rszshm_hdr h;
	int fd = -1, ret, errno_;
	char *m = NULL;

	if (!r || !fname || !fname[0] ||
	    strnlen(fname, RSZSHM_PATH_MAX) == RSZSHM_PATH_MAX) {
		errno = EINVAL;
		return NULL;
	}

	if ((fd = open(fname, O_RDWR)) == -1)
		return NULL;

	if ((ret = read(fd, &h, sizeof(h))) == -1)
		goto err;

	if (ret != sizeof(h) || !h.addr || h.flen == 0 || h.max == 0) {
		errno = ENODATA;
		goto err;
	}

	m = mmap(h.addr, h.max, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_NORESERVE, -1, 0);
	if (m == MAP_FAILED)
		goto err;
	if (m != h.addr) {
		errno = ENOSPC;
		goto err;
	}

	if (mmap(m, h.flen, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, fd, 0) == MAP_FAILED)
		goto err;

	*r = (typeof(*r)) { .fd = fd, .flen = h.flen, .hdr = (typeof(r->hdr)) m,
			    .dat = m + sizeof(h), .cap = h.flen - sizeof(h) };
	strcpy(r->fname, fname);

	return r->dat;

err:
	errno_ = errno;
	if (m && m != MAP_FAILED)
		munmap(m, h.max);
	close(fd);
	errno = errno_;
	return NULL;
}

#undef rszshm_up
int rszshm_up(struct rszshm *r)
{
	size_t flen;

	assert(r);

	flen = r->hdr->flen;
	if (r->flen == flen)
		return 0;
	if (mmap(r->hdr, flen, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, r->fd, 0) == MAP_FAILED)
		return -1;

	r->flen = flen;
	r->cap = flen - sizeof(*r->hdr);
	return 1;
}

int rszshm_grow(struct rszshm *r)
{
	int ret;
	assert(r);

	if ((ret = rszshm_up(r)) != 0)
		return ret;

	if (r->flen == r->hdr->max) {
		errno = ENOMEM;
		return -1;
	}

	if (flock(r->fd, LOCK_EX) == -1)
		return -1;

	if ((ret = rszshm_up(r)) == 0) {
		int flen = r->hdr->flen * 2 < r->hdr->max ? r->hdr->flen * 2 : r->hdr->max;

		if (ftruncate(r->fd, flen) != -1 &&
		    mmap(r->hdr, flen, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, r->fd, 0) != MAP_FAILED) {
			r->flen = r->hdr->flen = flen;
			r->cap = flen - sizeof(*r->hdr);
			ret = 1;
		}
		else
			ret = -1;
	}

	flock(r->fd, LOCK_UN);
	return ret;
}

int rszshm_dt(struct rszshm *r)
{
	int ret[3];
	assert(r);

	/* ok to call twice, since free macro calls this */
	if (r->fd == -1)
		return 0;

	ret[0] = msync(r->hdr, r->flen, MS_SYNC);
	ret[1] = munmap(r->hdr, r->hdr->max);
	ret[2] = close(r->fd);

	r->fd = -1;
	r->flen = 0;
	r->hdr = NULL;
	r->dat = NULL;
	r->cap = 0;

	return ret[0] == 0 && ret[1] == 0 && ret[2] == 0 ? 0 : -1;
}

int rszshm_unlink(struct rszshm *r)
{
	assert(r);
	return unlink(r->fname);
}

int rszshm_rmdir(struct rszshm *r)
{
	int ret;
	char *p;

	assert(r);

	if ((p = strrchr(r->fname, '/')) == NULL) {
		errno = ENOTDIR;
		return -1;
	}

	*p = '\0';
	ret = rmdir(r->fname);
	*p = '/';
	return ret;
}
