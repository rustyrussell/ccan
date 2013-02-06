/* Licensed under BSD-MIT - see LICENSE file for details */
#include "read_write_all.h"
#include <unistd.h>
#include <errno.h>

/* GCC inlines this quite well on -O2. */
static inline bool do_all(int fd, void *data, size_t size,
			  ssize_t (*op)(int, void *, size_t))
{
	while (size) {
		ssize_t done;

		done = op(fd, data, size);
		if (done < 0 && errno == EINTR)
			continue;
		if (done <= 0)
			return false;
		data = (char *)data + done;
		size -= done;
	}

	return true;
}

bool write_all(int fd, const void *data, size_t size)
{
	return do_all(fd, (void *)data, size, (void *)write);
}

bool read_all(int fd, void *data, size_t size)
{
	return do_all(fd, data, size, read);
}
