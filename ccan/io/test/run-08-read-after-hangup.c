#include <ccan/io/io.h>
/* Include the C files directly. */
#include <ccan/io/poll.c>
#include <ccan/io/io.c>
#include <ccan/tap/tap.h>
#include <sys/wait.h>
#include <stdio.h>
#include <signal.h>

static char inbuf[8];

static struct io_plan wake_it(struct io_conn *conn, struct io_conn *reader)
{
	io_wake(inbuf);
	return io_close();
}

static struct io_plan read_buf(struct io_conn *conn, void *unused)
{
	return io_read(inbuf, 8, io_close_cb, NULL);
}

int main(void)
{
	int fds[2];
	struct io_conn *conn;

	plan_tests(3);

	ok1(pipe(fds) == 0);
	conn = io_new_conn(fds[0], io_wait(inbuf, read_buf, NULL));
	io_new_conn(fds[1], io_write("EASYTEST", 8, wake_it, conn));

	ok1(io_loop() == NULL);
	ok1(memcmp(inbuf, "EASYTEST", sizeof(inbuf)) == 0);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
