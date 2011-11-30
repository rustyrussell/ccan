/* Include the C files directly. */
#include <ccan/failtest/failtest.c>
#include <stdlib.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ccan/tap/tap.h>

#define SIZE 8

/* We don't want to fork and fail; we're just testing lock recording. */
static enum failtest_result dont_fail(struct tlist_calls *history)
{
	return FAIL_DONT_FAIL;
}

static bool place_lock(int fd, char lockarr[], unsigned pos, unsigned size,
		       int type)
{
	struct flock fl;

	/* Update record keeping. */
	if (type == F_RDLCK)
		memset(lockarr+pos, 1, size);
	else if (type == F_WRLCK)
		memset(lockarr+pos, 2, size);
	else
		memset(lockarr+pos, 0, size);

	fl.l_whence = SEEK_SET;
	fl.l_type = type;
	fl.l_start = pos;
	fl.l_len = size;
	return failtest_fcntl(fd, "run-locking.c", 1, F_SETLK, &fl) == 0;
}

static char lock_lookup(int fd, unsigned pos)
{
	char ret = 0;
	unsigned int i;
	struct lock_info *l;

	for (i = 0; i < lock_num; i++) {
		l = &locks[i];

		if (l->fd != fd)
			continue;

		if (pos >= l->start && pos <= l->end) {
			if (ret)
				ret = 3;
			else if (l->type == F_RDLCK)
				ret = 1;
			else
				ret = 2;
		}
	}
	return ret;
}

static bool test(int fd,
		 unsigned p1, unsigned s1,
		 unsigned p2, unsigned s2,
		 unsigned p3, unsigned s3)
{
	unsigned int i;
	char lockarr[SIZE];

	memset(lockarr, 0, sizeof(lockarr));

	if (!place_lock(fd, lockarr, p1, s1, F_WRLCK))
		return false;

	if (!place_lock(fd, lockarr, p2, s2, F_RDLCK))
		return false;

	if (!place_lock(fd, lockarr, p3, s3, F_UNLCK))
		return false;

	for (i = 0; i < SIZE; i++) {
		if (lock_lookup(fd, i) != lockarr[i])
			return false;
	}

	/* Reset lock info. */
	lock_num = 0;
	return true;
}

int main(void)
{
	int fd;
	long flags;
	unsigned int isize;

	plan_tests(5835);
	failtest_init(0, NULL);
	failtest_hook = dont_fail;

	fd = open("run-locking-scratch", O_RDWR|O_CREAT, 0600);
	/* GETFL and SETFL wrappers should pass through. */
	flags = fcntl(fd, F_GETFL);
	ok1(failtest_fcntl(fd, "run-locking.c", 1, F_GETFL) == flags);
	flags |= O_NONBLOCK;
	ok1(failtest_fcntl(fd, "run-locking.c", 1, F_SETFL, flags) == 0);
	ok1(failtest_fcntl(fd, "run-locking.c", 1, F_GETFL) == flags);

	for (isize = 1; isize < 4; isize++) {
		unsigned int ipos;
		for (ipos = 0; ipos + isize < SIZE; ipos++) {
			unsigned int jsize;
			for (jsize = 1; jsize < 4; jsize++) {
				unsigned int jpos;
				for (jpos = 0; jpos + jsize < SIZE; jpos++) {
					unsigned int ksize;
					for (ksize = 1; ksize < 4; ksize++) {
						unsigned int kpos;
						for (kpos = 0;
						     kpos + ksize < SIZE;
						     kpos++) {
							ok1(test(fd,
								 ipos, isize,
								 jpos, jsize,
								 kpos, ksize));
						}
					}
				}
			}
		}
	}

	return exit_status();
}
