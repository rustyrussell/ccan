/* Licensed under BSD-MIT - see LICENSE file for details */
#ifndef CCAN_READ_WRITE_H
#define CCAN_READ_WRITE_H
#include <stddef.h>
#include <stdbool.h>

/**
 * write_all - write all of the given data to an fd.
 * @fd: the file descriptor to write to.
 * @data: the data to write.
 * @size: the amount of data to write (in bytes).
 *
 * This loops, catching EINTR from signals or short writes.  It returns true
 * if everything was written, false (and sets errno) otherwise.
 *
 * Example:
 *	if (!write_all(0, "SUCCESS", strlen("SUCCESS")))
 *		exit(1);
 */
bool write_all(int fd, const void *data, size_t size);

/**
 * read_all - read all of the given data from an fd.
 * @fd: the file descriptor to read from.
 * @data: the buffer to read into.
 * @size: the amount of data to read (in bytes).
 *
 * This loops, catching EINTR from signals or short reads.  It returns true
 * if everything was read, false (and sets errno) otherwise.
 *
 * Example:
 *	char buf[20];
 *	if (!read_all(0, buf, 20))
 *		exit(1);
 */
bool read_all(int fd, void *data, size_t size);

#endif /* CCAN_READ_WRITE_H */
