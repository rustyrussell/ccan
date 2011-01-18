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
#define open(pathname, flags, ...) \
	failtest_open((pathname), (flags), NULL, 0, __VA_ARGS__)

#undef pipe
#define pipe(pipefd) \
	failtest_pipe((pipefd), NULL, 0)

#undef read
#define read(fd, buf, count) \
	failtest_read((fd), (buf), (count), NULL, 0)

#undef write
#define write(fd, buf, count) \
	failtest_write((fd), (buf), (count), NULL, 0)

#undef close
#define close(fd) failtest_close(fd)

#endif /* CCAN_FAILTEST_RESTORE_H */
