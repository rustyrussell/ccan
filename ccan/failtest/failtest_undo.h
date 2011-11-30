/* Licensed under LGPL - see LICENSE file for details */
#ifndef CCAN_FAILTEST_RESTORE_H
#define CCAN_FAILTEST_RESTORE_H
/* This file undoes the effect of failtest_override.h. */

#undef calloc
#define calloc(nmemb, size)	\
	failtest_calloc((nmemb), (size), NULL, 0)

#undef malloc
#define malloc(size)	\
	failtest_malloc((size), NULL, 0)

#undef realloc
#define realloc(ptr, size)					\
	failtest_realloc((ptr), (size), NULL, 0)

#undef open
#define open(pathname, ...) \
	failtest_open((pathname), NULL, 0, __VA_ARGS__)

#undef pipe
#define pipe(pipefd) \
	failtest_pipe((pipefd), NULL, 0)

#undef read
#define read(fd, buf, count) \
	failtest_read((fd), (buf), (count), NULL, 0)

#undef write
#define write(fd, buf, count) \
	failtest_write((fd), (buf), (count), NULL, 0)

#undef mmap
#define mmap(addr, length, prot, flags, fd, offset) \
	failtest_mmap((addr), (length), (prot), (flags), (fd), (offset), NULL, 0)

#undef lseek
#define lseek(fd, off, whence) \
	failtest_lseek((fd), (off), (whence), NULL, 0)

#undef close
#define close(fd) failtest_close(fd)

#undef fcntl
#define fcntl(fd, ...) \
	failtest_fcntl((fd), NULL, 0, __VA_ARGS__)

#endif /* CCAN_FAILTEST_RESTORE_H */
