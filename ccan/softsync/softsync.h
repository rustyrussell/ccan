#ifndef CCAN_SOFTSYNC_H
#define CCAN_SOFTSYNC_H
#include <ccan/md4/md4.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

struct softsync {
	int base_fd, journal_fd;
	bool needs_recovery;
	off_t journal_len, base_len;
	struct md4_ctx csum;
};

int softsync_open(struct softsync *s,
		  const char *pathname, int flags, mode_t mode);
int softsync_init_fd(struct softsync *s,
		     int fd, const char *pathname, mode_t mode);

ssize_t softsync_pwrite(struct softsync *s,
			const void *buf, size_t count, off_t off);
ssize_t softsync_pread(struct softsync *s, void *buf, size_t count, off_t off);
int softsync_ftruncate(struct softsync *s, off_t len);
int softsync_fdatasync(struct softsync *s);
int softsync_close(struct softsync *s);
bool softsync_check(const struct softsync *s, const char *abortstr);
#endif /* CCAN_SOFTSYNC_H */
