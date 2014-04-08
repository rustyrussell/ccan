#include <ccan/io/io.h>
/* Include the C files directly. */
#include <ccan/io/poll.c>
#include <ccan/io/io.c>
#include <ccan/tap/tap.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>

#ifndef PORT
#define PORT "65015"
#endif

struct data {
	int state;
	int timeout_usec;
	bool timed_out;
	char buf[4];
};


static struct io_plan no_timeout(struct io_conn *conn, struct data *d)
{
	ok1(d->state == 1);
	d->state++;
	return io_close();
}

static struct io_plan timeout(struct io_conn *conn, struct data *d)
{
	ok1(d->state == 1);
	d->state++;
	d->timed_out = true;
	return io_close();
}

static void finish_ok(struct io_conn *conn, struct data *d)
{
	ok1(d->state == 2);
	d->state++;
	io_break(d, io_never());
}

static void init_conn(int fd, struct data *d)
{
	struct io_conn *conn;

	ok1(d->state == 0);
	d->state++;

	conn = io_new_conn(fd, io_read(d->buf, sizeof(d->buf), no_timeout, d));
	io_set_finish(conn, finish_ok, d);
	io_timeout(conn, time_from_usec(d->timeout_usec), timeout, d);
}

static int make_listen_fd(const char *port, struct addrinfo **info)
{
	int fd, on = 1;
	struct addrinfo *addrinfo, hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;

	if (getaddrinfo(NULL, port, &hints, &addrinfo) != 0)
		return -1;

	fd = socket(addrinfo->ai_family, addrinfo->ai_socktype,
		    addrinfo->ai_protocol);
	if (fd < 0)
		return -1;

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (bind(fd, addrinfo->ai_addr, addrinfo->ai_addrlen) != 0) {
		close(fd);
		return -1;
	}
	if (listen(fd, 1) != 0) {
		close(fd);
		return -1;
	}
	*info = addrinfo;
	return fd;
}

int main(void)
{
	struct data *d = malloc(sizeof(*d));
	struct addrinfo *addrinfo;
	struct io_listener *l;
	int fd, status;

	/* This is how many tests you plan to run */
	plan_tests(20);
	d->state = 0;
	d->timed_out = false;
	d->timeout_usec = 100000;
	fd = make_listen_fd(PORT, &addrinfo);
	ok1(fd >= 0);
	l = io_new_listener(fd, init_conn, d);
	ok1(l);
	fflush(stdout);

	if (!fork()) {
		int i;

		io_close_listener(l);
		fd = socket(addrinfo->ai_family, addrinfo->ai_socktype,
			    addrinfo->ai_protocol);
		if (fd < 0)
			exit(1);
		if (connect(fd, addrinfo->ai_addr, addrinfo->ai_addrlen) != 0)
			exit(2);
		signal(SIGPIPE, SIG_IGN);
		usleep(500000);
		for (i = 0; i < strlen("hellothere"); i++) {
			if (write(fd, "hellothere" + i, 1) != 1)
				break;
		}
		close(fd);
		freeaddrinfo(addrinfo);
		free(d);
		exit(i);
	}
	ok1(io_loop() == d);
	ok1(d->state == 3);
	ok1(d->timed_out == true);
	ok1(wait(&status));
	ok1(WIFEXITED(status));
	ok1(WEXITSTATUS(status) < sizeof(d->buf));

	/* This one shouldn't time out. */
	d->state = 0;
	d->timed_out = false;
	d->timeout_usec = 500000;
	fflush(stdout);

	if (!fork()) {
		int i;

		io_close_listener(l);
		fd = socket(addrinfo->ai_family, addrinfo->ai_socktype,
			    addrinfo->ai_protocol);
		if (fd < 0)
			exit(1);
		if (connect(fd, addrinfo->ai_addr, addrinfo->ai_addrlen) != 0)
			exit(2);
		signal(SIGPIPE, SIG_IGN);
		usleep(100000);
		for (i = 0; i < strlen("hellothere"); i++) {
			if (write(fd, "hellothere" + i, 1) != 1)
				break;
		}
		close(fd);
		freeaddrinfo(addrinfo);
		free(d);
		exit(i);
	}
	ok1(io_loop() == d);
	ok1(d->state == 3);
	ok1(d->timed_out == false);
	ok1(wait(&status));
	ok1(WIFEXITED(status));
	ok1(WEXITSTATUS(status) >= sizeof(d->buf));

	io_close_listener(l);
	freeaddrinfo(addrinfo);
	free(d);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
