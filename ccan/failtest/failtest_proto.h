/* Licensed under LGPL - see LICENSE file for details */
#ifndef CCAN_FAILTEST_PROTO_H
#define CCAN_FAILTEST_PROTO_H
#include <stdlib.h>

/* Potentially-failing versions of routines; #defined in failtest.h */
void *failtest_calloc(size_t nmemb, size_t size,
		      const char *file, unsigned line);
void *failtest_malloc(size_t size, const char *file, unsigned line);
void *failtest_realloc(void *ptr, size_t size,
		       const char *file, unsigned line);
void failtest_free(void *ptr);
int failtest_open(const char *pathname,
		  const char *file, unsigned line, ...);
int failtest_pipe(int pipefd[2], const char *file, unsigned line);
ssize_t failtest_read(int fd, void *buf, size_t count,
		      const char *file, unsigned line);
ssize_t failtest_write(int fd, const void *buf, size_t count,
		       const char *file, unsigned line);
ssize_t failtest_pread(int fd, void *buf, size_t count, off_t offset,
		       const char *file, unsigned line);
ssize_t failtest_pwrite(int fd, const void *buf, size_t count, off_t offset,
			const char *file, unsigned line);
void *failtest_mmap(void *addr, size_t length, int prot, int flags,
		    int fd, off_t offset, const char *file, unsigned line);
void *failtest_mmap_noloc(void *addr, size_t length, int prot, int flags,
			  int fd, off_t offset);
off_t failtest_lseek(int fd, off_t offset, int whence,
		     const char *file, unsigned line);
int failtest_close(int fd, const char *file, unsigned line);
int failtest_fcntl(int fd, const char *file, unsigned line, int cmd, ...);
pid_t failtest_getpid(const char *file, unsigned line);
#endif /* CCAN_FAILTEST_PROTO_H */
