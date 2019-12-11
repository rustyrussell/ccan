/* Licensed under LGPLv2+ - see LICENSE file for details */
#ifndef _CCAN_READ_WRITE_H
#define _CCAN_READ_WRITE_H
#include <stddef.h>
#include <stdbool.h>

/**
 * Write `size` bytes from `data` to the file descriptor `fd`, retrying on
 * transient errors.
 * If the data cannot be fully written, then false is returned and errno is set
 * to indicate the error.
 */
bool write_all(int fd, const void *data, size_t size);

/**
 * Read `size` bytes from the file descriptor `fd` into `data`, retrying on
 * transient errors.
 * If `size` bytes cannot be read then false is returned and errno is set
 * to indicate the error, in which case the contents of `data` is undefined.
 * If EOF occurs before `size` bytes were read, then errno is set to EBADMSG.
 */
bool read_all(int fd, void *data, size_t size);

#endif /* _CCAN_READ_WRITE_H */
