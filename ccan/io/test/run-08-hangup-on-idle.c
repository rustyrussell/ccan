#include <ccan/io/io.h>
/* Include the C files directly. */
#include <ccan/io/poll.c>
#include <ccan/io/io.c>
#include <ccan/tap/tap.h>
#include <sys/wait.h>
#include <stdio.h>

static int fds2[2];

static struct io_plan timeout_wakeup(struct io_conn *conn, char *buf)
{
	/* This kills the dummy connection. */
	close(fds2[1]);
	return io_read(buf, 16, io_close_cb, NULL);
}

static struct io_plan never(struct io_conn *conn, void *unused)
{
	abort();
}

int main(void)
{
	int fds[2];
	struct io_conn *conn;
	char buf[16];

	plan_tests(4);

	ok1(pipe(fds) == 0);

	/* Write then close. */
	io_new_conn(fds[1], io_write("hello there world", 16,
				     io_close_cb, NULL));
	conn = io_new_conn(fds[0], io_wait(buf, never, NULL));

	/* To avoid assert(num_waiting) */
	ok1(pipe(fds2) == 0);
	io_new_conn(fds2[0], io_read(buf, 16, io_close_cb, NULL));

	/* After half a second, it will read. */
	io_timeout(conn, time_from_msec(500), timeout_wakeup, buf);

	ok1(io_loop() == NULL);
	ok1(memcmp(buf, "hello there world", 16) == 0);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
