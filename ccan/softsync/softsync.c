#include "softsync.h"
#include <ccan/noerr/noerr.h>
#include <ccan/short_types/short_types.h>
#include <ccan/read_write_all/read_write_all.h>
#include <ccan/str/str.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#define SOFTSYNC_EXT ".softsync"

enum ss_op {
	SS_WRITE,
	SS_TRUNCATE,
	SS_SYNC
};

/* Read and csum bytes. */
static bool csum_read(int ssfd, struct md4_ctx *csum, void *buf, size_t bytes)
{
	if (!read_all(ssfd, buf, bytes))
		return false;

	md4_hash(csum, buf, bytes);
	return true;
}

/* Discard and csum bytes. */
static bool csum_consume(int ssfd, struct md4_ctx *csum, size_t bytes)
{
	while (bytes) {
		size_t len;
		char buf[1024];

		if (bytes < sizeof(buf))
			len = bytes;
		else
			len = sizeof(buf);

		if (!csum_read(ssfd, csum, buf, len))
			return false;
		bytes -= len;
	}
	return true;
}

/*
 * We go up to first incomplete or incorrect transaction, returning
 * the last valid offset.  We also tell you the size of the base file
 * for convenience.
 *
 * Normally we ignore incomplete (ie. unchecksummed) records, but it's
 * useful for checking.
 */
static off_t validate_journal(int journal_fd, off_t *eof, bool incomplete)
{
	struct md4_ctx csum;
	off_t transaction_len, curr_eof, journal_len;

	md4_init(&csum);
	journal_len = transaction_len = curr_eof = *eof = 0;
	for (;;) {
		u32 jtype;
		u64 jlen, joff;
		char expect[sizeof(csum.hash.bytes)];

		if (!csum_read(journal_fd, &csum, &jtype, sizeof(jtype)))
			break;
		transaction_len += sizeof(jtype);

		switch (jtype) {
		case SS_WRITE:
			if (!csum_read(journal_fd, &csum, &jlen, sizeof(jlen))
			    || !csum_read(journal_fd, &csum, &joff,sizeof(joff))
			    || !csum_consume(journal_fd, &csum, jlen))
				break;
			transaction_len += sizeof(jlen) + sizeof(joff) + jlen;
			/* Note expanded file */
			if (joff + jlen > curr_eof)
				curr_eof = joff + jlen;
			break;
		case SS_TRUNCATE:
			if (!csum_read(journal_fd, &csum, &jlen,
				       sizeof(jlen)))
				break;
			transaction_len += sizeof(jlen);
			curr_eof = jlen;
			break;
		case SS_SYNC:
			md4_finish(&csum);
			if (!read_all(journal_fd, expect, sizeof(expect)))
				break;
			if (memcmp(expect, csum.hash.bytes, sizeof(expect)))
				break;
			md4_init(&csum);
			journal_len += transaction_len;
			*eof = curr_eof;
			transaction_len = 0;
		default:
			break;
		}
	}

	/* If they want to know about unchecksummed entries, too. */
	if (incomplete) {
		journal_len += transaction_len;
		*eof = curr_eof;
	}

	return journal_len;
}

static bool skip_bytes(int fd, off_t len)
{
	return lseek(fd, len, SEEK_CUR) != (off_t)-1;
}

/* Do a read.  Reconstruct if underlying file needs recovery. */
ssize_t softsync_pread(struct softsync *s, void *buf, size_t count, off_t off)
{
	off_t curr_off;

	/* Normally, we can read straight out of the file. */
	if (!s->needs_recovery)
		return pread(s->base_fd, buf, count, off);

	/* Seek past end and read is legal, returns 0. */
	if (off > s->base_len)
		return 0;

	if (off + count > s->base_len)
		count = s->base_len - off;

	/* Otherwise, we reconstruct the previous operations */

	/* Start by memset 0, since file expansion will do this. */
	memset(buf, 0, count);

	curr_off = 0;
	if (lseek(s->journal_fd, 0, SEEK_SET) != 0)
		goto fail;

	/* Replay journal */
	while (curr_off < s->journal_len) {
		u32 jtype;
		u64 jlen, joff;
		off_t preseek, postseek, readlen;
		size_t prebuf;

		if (!read_all(s->journal_fd, &jtype, sizeof(jtype)))
			goto fail;
		curr_off += sizeof(jtype);

		switch (jtype) {
		case SS_WRITE:
			if (!read_all(s->journal_fd, &jlen, sizeof(jlen))
			    || !read_all(s->journal_fd, &joff, sizeof(joff)))
				goto fail;

			/* Ignore if doesn't overlap what we want to read. */
			if (joff + jlen < off || joff > off + count) {
				if (!skip_bytes(s->journal_fd, jlen))
					goto fail;
				curr_off += sizeof(jlen) + sizeof(joff) + jlen;
				break;
			}

			/* FIXME: Neaten this code. */
			if (off > joff) {
				preseek = off - joff;
				prebuf = 0;
			} else {
				preseek = 0;
				prebuf = joff - off;
			}
			readlen = count;
			if (preseek + readlen > jlen)
				readlen = jlen - preseek;
			if (prebuf + readlen > count)
				readlen = count - prebuf;

			postseek = jlen - (preseek + readlen);
			if (!skip_bytes(s->journal_fd, preseek))
				goto fail;
			if (!read_all(s->journal_fd, (char *)buf + prebuf,
				      readlen))
				goto fail;
			if (!skip_bytes(s->journal_fd, postseek))
				goto fail;
			curr_off += sizeof(jlen) + sizeof(joff) + jlen;
			break;

		case SS_TRUNCATE:
			if (!read_all(s->journal_fd, &jlen, sizeof(jlen)))
				goto fail;
			curr_off += sizeof(jlen);

			/* If we truncate, implies zeroing. */
			if (jlen < off + count) {
				size_t bufoff;

				if (jlen < off)
					bufoff = 0;
				else
					bufoff = jlen - off;
				memset((char *)buf + bufoff, 0, count - bufoff);
			}
			break;
		case SS_SYNC:
			if (!skip_bytes(s->journal_fd,
					sizeof(s->csum.hash.bytes)))
				goto fail;
			curr_off += sizeof(s->csum.hash.bytes);
			break;
		default:
			errno = EINVAL;
			goto fail;
		}
	}
	return count;

fail:
	return -1;
}

/* Reconstruct the file by replaying journal. */
static bool try_recovery(struct softsync *s, const char *pathname)
{
	char *buf;
	off_t old_off;

	/* FIXME: We could mmap, and do huge files in parts. */
	buf = malloc(s->base_len);
	if (!buf)
		goto fail;
	if (softsync_pread(s, buf, s->base_len, 0) != s->base_len)
		goto fail;

	old_off = lseek(s->base_fd, 0, SEEK_SET);
	if (old_off == (off_t)-1)
		goto fail;

	if (!write_all(s->base_fd, buf, s->base_len))
		goto fail;

	if (ftruncate(s->base_fd, s->base_len) != 0)
		goto fail;

	free(buf);
	return true;

fail:
	free(buf);
	return false;
}

/* Write to journal and update checksum */
static bool journal_write(struct softsync *s, const void *data, size_t len)
{
	if (!write_all(s->journal_fd, data, len))
		return false;
	md4_hash(&s->csum, data, len);
	s->journal_len += len;
	return true;
}

static bool needs_recovery(const char *jpath, int fd)
{
#if HAVE_MINCORE_DIRTY_BIT
#error Implement me
#else
	struct stat st;
	struct sysinfo info;
	char path[PATH_MAX];
	struct timeval now;

	/* On any error, we return true: ie. try to do recovery. */
	if (realpath(jpath, path) != path)
		return true;

	/* We assume /usr, /var and /tmp are mounted since boot. */
	if (!strstarts(path, "/usr/")
	    && !strstarts(path, "/tmp/")
	    && !strstarts(path, "/var/"))
		return true;

	if (fstat(fd, &st) != 0)
		return true;

	gettimeofday(&now, NULL);
	if (sysinfo(&info) != 0)
		return true;

	/* If it has been modified since boot, assume it's been recovered,
	 * but be paranoid. */
	if (st.st_mtime > now.tv_sec)
		return true;

	return st.st_mtime < now.tv_sec - info.uptime;
#endif
}

int softsync_init_fd(struct softsync *s,
		     int fd, const char *pathname, mode_t mode)
{
	char jpath[strlen(pathname) + sizeof(SOFTSYNC_EXT)];

	strcpy(jpath, pathname);
	strcat(jpath, SOFTSYNC_EXT);

	s->base_fd = fd;
	s->journal_fd = open(jpath, O_RDWR);

	if (s->journal_fd != -1) {
		s->needs_recovery = needs_recovery(jpath, fd);
		if (s->needs_recovery) {
			s->journal_len = validate_journal(s->journal_fd,
							  &s->base_len, false);

			if (!try_recovery(s, pathname)) {
				close_noerr(s->journal_fd);
				return -1;
			}
			s->needs_recovery = false;
		}
	} else {
		s->needs_recovery = false;

		/* Mode must match, since anyone writing to
		 * file must write to journal. */
		s->journal_fd = open(jpath, O_RDWR|O_CREAT, mode);
		if (s->journal_fd == -1)
			return -1;

		/* FIXME: What if there's already data in pathname? */
		s->journal_len = s->base_len = 0;
	}
	md4_init(&s->csum);
	return 0;
}

int softsync_open(struct softsync *s,
		  const char *pathname, int flags, mode_t mode)
{
	int fd = open(pathname, flags, mode);
	if (fd < 0)
		return fd;
	if (softsync_init_fd(s, fd, pathname, mode) != 0) {
		close_noerr(fd);
		return -1;
	}
	return fd;
}

ssize_t softsync_pwrite(struct softsync *s,
			const void *buf, size_t count, off_t off)
{
	ssize_t ret;
	u32 jtype = SS_WRITE;
	u64 jlen, joff;

	ret = pwrite(s->base_fd, buf, count, off);
	if (ret == -1)
		return ret;
	/* Short write means out of space, but try to record. */
	count = ret;

	joff = off;
	jlen = count;
	if (!journal_write(s, &jtype, sizeof(jtype))
	    || !journal_write(s, &jlen, sizeof(jlen))
	    || !journal_write(s, &joff, sizeof(joff))
	    || !journal_write(s, buf, count))
		return -1;

	if (joff + jlen > s->base_len)
		s->base_len = joff + jlen;

	return count;
}

int softsync_ftruncate(struct softsync *s, off_t len)
{
	u32 jtype = SS_TRUNCATE;
	u64 jlen = len;

	if (ftruncate(s->base_fd, len) != 0)
		return -1;

	if (!journal_write(s, &jtype, sizeof(jtype))
	    || !journal_write(s, &jlen, sizeof(jlen)))
		return -1;
	s->base_len = jlen;
	return 0;
}

int softsync_fdatasync(struct softsync *s)
{
	u32 jtype = SS_SYNC;
	bool ok;

	if (!journal_write(s, &jtype, sizeof(jtype)))
		ok = false;
	else {
		md4_finish(&s->csum);
		ok = write_all(s->journal_fd, s->csum.hash.bytes,
			       sizeof(s->csum.hash.bytes));
	}
	md4_init(&s->csum);
	return ok ? 0 : -1;
}

int softsync_close(struct softsync *s)
{
	/* close_noerr does the looping on EINTR for us. */
	close_noerr(s->journal_fd);
	errno = close_noerr(s->base_fd);
	s->base_fd = s->journal_fd = -1;
	return errno ? -1 : 0;
}

static void corrupt(const char *abortstr, const char *msg, int errnum)
{
	if (abortstr) {
		if (errnum)
			fprintf(stderr, "%s: %s (%s)\n", abortstr, msg,
				strerror(errnum));
		else
			fprintf(stderr, "%s: %s\n", abortstr, msg);
		abort();
	}
}

bool softsync_check(const struct softsync *s, const char *abortstr)
{
	struct softsync tmp = *s;
	off_t old_off;
	char *buf;
	bool ok = false;

	old_off = lseek(tmp.journal_fd, 0, SEEK_CUR);
	if (old_off == (off_t)-1) {
		corrupt(abortstr, "lseek failed", errno);
		return -1;
	}
	if (lseek(tmp.journal_fd, 0, SEEK_SET) == (off_t)-1) {
		corrupt(abortstr, "lseek failed", errno);
		return -1;
	}

	tmp.journal_len = validate_journal(tmp.journal_fd, &tmp.base_len, true);

	if (s->journal_len != tmp.journal_len) {
		corrupt(abortstr, "journal length incorrect", errno);
		goto restore_off;
	}

	if (s->base_len != tmp.base_len) {
		corrupt(abortstr, "base length incorrect", errno);
		goto restore_off;
	}

	if (tmp.base_len * 2 < tmp.base_len) {
		/* FIXME: This could legitimately fail on sparse files. */
		corrupt(abortstr, "could not check huge file", errno);
		goto restore_off;
	}

	buf = malloc(tmp.base_len * 2);
	if (!buf) {
		/* FIXME: This could legitimately fail on sparse files. */
		corrupt(abortstr, "could not allocate", errno);
		goto restore_off;
	}

	tmp.needs_recovery = true;
	if (softsync_pread(&tmp, buf, tmp.base_len, 0) != tmp.base_len) {
		/* FIXME: This could legitimately fail on sparse files. */
		corrupt(abortstr, "could not reconstruct file", errno);
		goto free_buf;
	}

	/* Compare with actual contents. */
	if (pread(tmp.base_fd, buf + tmp.base_len, tmp.base_len, 0)
	    != tmp.base_len) {
		corrupt(abortstr, "could not read file", errno);
		goto free_buf;
	}

	if (memcmp(buf, buf + tmp.base_len, tmp.base_len) != 0) {
		corrupt(abortstr, "file differed from expected", errno);
		goto free_buf;
	}

	ok = true;

free_buf:
	free(buf);
restore_off:
	lseek(tmp.journal_fd, old_off, SEEK_SET);
	return ok;
}
