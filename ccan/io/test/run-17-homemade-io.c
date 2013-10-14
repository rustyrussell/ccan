#include <ccan/io/io.h>
/* Include the C files directly. */
#include <ccan/io/poll.c>
#include <ccan/io/io.c>
#include <ccan/tap/tap.h>
#include <sys/wait.h>
#include <stdio.h>

#ifndef PORT
#define PORT "65017"
#endif

struct packet {
	int state;
	size_t len;
	void *contents;
};

static void finish_ok(struct io_conn *conn, struct packet *pkt)
{
	ok1(pkt->state == 3);
	pkt->state++;
	io_break(pkt, io_idle());
}

static bool do_read_packet(int fd, struct io_plan *plan)
{
	struct packet *pkt = plan->u.ptr_len.p;
	char *dest;
	ssize_t ret;
	size_t off, totlen;

	/* Reading len? */
	if (plan->u.ptr_len.len < sizeof(size_t)) {
		ok1(pkt->state == 1);
		pkt->state++;
		dest = (char *)&pkt->len;
		off = plan->u.ptr_len.len;
		totlen = sizeof(pkt->len);
	} else {
		ok1(pkt->state == 2);
		pkt->state++;
		if (pkt->len == 0)
			return true;
		if (!pkt->contents && !(pkt->contents = malloc(pkt->len)))
			goto fail;
		else {
			dest = pkt->contents;
			off = plan->u.ptr_len.len - sizeof(pkt->len);
			totlen = pkt->len;
		}
	}

	ret = read(fd, dest + off, totlen - off);
	if (ret <= 0)
		goto fail;

	plan->u.ptr_len.len += ret;

	/* Finished? */
	return (plan->u.ptr_len.len >= sizeof(pkt->len)
		&& plan->u.ptr_len.len == pkt->len + sizeof(pkt->len));

fail:
	free(pkt->contents);
	/* Override next function to close us. */
	plan->next = io_close;
	return true;
}

static struct io_plan io_read_packet(struct packet *pkt,
				     struct io_plan (*cb)(struct io_conn *, void *),
				     void *arg)
{
	struct io_plan plan;

	assert(cb);
	pkt->contents = NULL;
	plan.u.ptr_len.p = pkt;
	plan.u.ptr_len.len = 0;
	plan.io = do_read_packet;
	plan.next = cb;
	plan.next_arg = arg;
	plan.pollflag = POLLIN;

	io_plan_debug(&plan);
	return plan;
}

static void init_conn(int fd, struct packet *pkt)
{
	ok1(pkt->state == 0);
	pkt->state++;

	if (!io_new_conn(fd, io_read_packet(pkt, io_close, pkt), finish_ok, pkt))
		abort();
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
	struct packet *pkt = malloc(sizeof(*pkt));
	struct addrinfo *addrinfo;
	struct io_listener *l;
	int fd, status;

	/* This is how many tests you plan to run */
	plan_tests(13);
	pkt->state = 0;
	fd = make_listen_fd(PORT, &addrinfo);
	ok1(fd >= 0);
	l = io_new_listener(fd, init_conn, pkt);
	ok1(l);
	fflush(stdout);
	if (!fork()) {
		struct {
			size_t len;
			char data[8];
		} data;

		io_close_listener(l);
		fd = socket(addrinfo->ai_family, addrinfo->ai_socktype,
			    addrinfo->ai_protocol);
		if (fd < 0)
			exit(1);
		if (connect(fd, addrinfo->ai_addr, addrinfo->ai_addrlen) != 0)
			exit(2);
		signal(SIGPIPE, SIG_IGN);

		data.len = sizeof(data.data);
		memcpy(data.data, "hithere!", sizeof(data.data));
		if (write(fd, &data, sizeof(data)) != sizeof(data))
			exit(3);

		close(fd);
		freeaddrinfo(addrinfo);
		free(pkt);
		exit(0);
	}
	freeaddrinfo(addrinfo);
	ok1(io_loop() == pkt);
	ok1(pkt->state == 4);
	ok1(pkt->len == 8);
	ok1(memcmp(pkt->contents, "hithere!", 8) == 0);
	free(pkt->contents);
	free(pkt);
	io_close_listener(l);

	ok1(wait(&status));
	ok1(WIFEXITED(status));
	ok1(WEXITSTATUS(status) == 0);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
