#include <ccan/io/io.h>
/* Include the C files directly. */
#include <ccan/io/poll.c>
#include <ccan/io/io.c>
#include <ccan/tap/tap.h>
#include <sys/wait.h>
#include <stdio.h>

#ifndef PORT
#define PORT "65009"
#endif

static struct io_listener *l;

struct data {
	int state;
	char buf[10];
};

static struct io_plan closer(struct io_conn *conn, struct data *d)
{
	d->state++;
	return io_close();
}

static struct io_plan connected(struct io_conn *conn, struct data *d2)
{
	ok1(d2->state == 0);
	d2->state++;
	return io_read(d2->buf, sizeof(d2->buf), closer, d2);
}

static void init_conn(int fd, struct data *d)
{
	ok1(d->state == 0);
	d->state++;
	io_new_conn(fd, io_write(d->buf, sizeof(d->buf), closer, d));
	io_close_listener(l);
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
	struct data *d = malloc(sizeof(*d)), *d2 = malloc(sizeof(*d2));
	struct addrinfo *addrinfo;
	int fd;

	/* This is how many tests you plan to run */
	plan_tests(8);
	d->state = 0;
	memset(d->buf, 'a', sizeof(d->buf));
	fd = make_listen_fd(PORT, &addrinfo);
	ok1(fd >= 0);
	l = io_new_listener(fd, init_conn, d);
	ok1(l);

	fd = socket(addrinfo->ai_family, addrinfo->ai_socktype,
		    addrinfo->ai_protocol);
	d2->state = 0;
	ok1(io_new_conn(fd, io_connect(fd, addrinfo, connected, d2)));

	ok1(io_loop() == NULL);
	ok1(d->state == 2);
	ok1(d2->state == 2);

	freeaddrinfo(addrinfo);
	free(d);
	free(d2);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
