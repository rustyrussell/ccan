#include <ccan/io/io.h>
/* Include the C files directly. */
#include <ccan/io/poll.c>
#include <ccan/io/io.c>
#include <ccan/tap/tap.h>
#include <sys/wait.h>
#include <stdio.h>

#ifndef PORT
#define PORT "65001"
#endif
static int expected_fd;

static void finish_ok(struct io_conn *conn, int *state)
{
	ok1(*state == 1);
	ok1(io_conn_fd(conn) == expected_fd);
	(*state)++;
	io_break(state + 1, io_never());
}

static void init_conn(int fd, int *state)
{
	ok1(*state == 0);
	(*state)++;
	expected_fd = fd;
	io_set_finish(io_new_conn(fd, io_close()), finish_ok, state);
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
	int state = 0;
	struct addrinfo *addrinfo;
	struct io_listener *l;
	int fd;

	/* This is how many tests you plan to run */
	plan_tests(10);
	fd = make_listen_fd(PORT, &addrinfo);
	ok1(fd >= 0);
	l = io_new_listener(fd, init_conn, &state);
	ok1(l);
	fflush(stdout);
	if (!fork()) {
		io_close_listener(l);
		fd = socket(addrinfo->ai_family, addrinfo->ai_socktype,
			    addrinfo->ai_protocol);
		if (fd < 0)
			exit(1);
		if (connect(fd, addrinfo->ai_addr, addrinfo->ai_addrlen) != 0)
			exit(2);
		close(fd);
		freeaddrinfo(addrinfo);
		exit(0);
	}
	freeaddrinfo(addrinfo);
	ok1(io_loop() == &state + 1);
	ok1(state == 2);
	io_close_listener(l);
	ok1(wait(&state));
	ok1(WIFEXITED(state));
	ok1(WEXITSTATUS(state) == 0);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
