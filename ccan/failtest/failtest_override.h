/* Licensed under LGPL - see LICENSE file for details */
#ifndef CCAN_FAILTEST_OVERRIDE_H
#define CCAN_FAILTEST_OVERRIDE_H
/* This file is included before the source file to test. */
#include "config.h"
#if HAVE_FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

/* Replacement of allocators. */
#include <stdlib.h>

#undef calloc
#define calloc(nmemb, size)	\
	failtest_calloc((nmemb), (size), __FILE__, __LINE__)

#undef malloc
#define malloc(size)	\
	failtest_malloc((size), __FILE__, __LINE__)

#undef realloc
#define realloc(ptr, size)					\
	failtest_realloc((ptr), (size), __FILE__, __LINE__)

#undef free
#define free(ptr) \
	failtest_free(ptr)

/* Replacement of I/O. */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#undef open
#define open(pathname, ...) \
	failtest_open((pathname), __FILE__, __LINE__, __VA_ARGS__)

#undef pipe
#define pipe(pipefd) \
	failtest_pipe((pipefd), __FILE__, __LINE__)

#undef read
#define read(fd, buf, count) \
	failtest_read((fd), (buf), (count), __FILE__, __LINE__)

#undef write
#define write(fd, buf, count) \
	failtest_write((fd), (buf), (count), __FILE__, __LINE__)

#undef pread
#define pread(fd, buf, count, off)				\
	failtest_pread((fd), (buf), (count), (off), __FILE__, __LINE__)

#undef pwrite
#define pwrite(fd, buf, count, off)					\
	failtest_pwrite((fd), (buf), (count), (off), __FILE__, __LINE__)

#undef close
#define close(fd) failtest_close(fd, __FILE__, __LINE__)

#undef fcntl
#define fcntl(fd, ...) failtest_fcntl((fd), __FILE__, __LINE__, __VA_ARGS__)

#undef mmap
/* OpenBSD doesn't idempotent-protect sys/mman.h, so we can't add args. */
#ifdef __OpenBSD__
#define mmap(addr, length, prot, flags, fd, offset)			\
	failtest_mmap_noloc((addr), (length), (prot), (flags), (fd), (offset))
#else
#define mmap(addr, length, prot, flags, fd, offset)			\
	failtest_mmap((addr), (length), (prot), (flags), (fd), (offset), \
		      __FILE__, __LINE__)
#endif /* !__OpenBSD__ */

#undef lseek
#define lseek(fd, offset, whence)					\
	failtest_lseek((fd), (offset), (whence), __FILE__, __LINE__)

/* Replacement of getpid (since failtest will fork). */
#undef getpid
#define getpid() failtest_getpid(__FILE__, __LINE__)

#include <ccan/failtest/failtest_proto.h>

#endif /* CCAN_FAILTEST_OVERRIDE_H */
