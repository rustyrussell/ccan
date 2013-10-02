#include <ccan/net/net.h>
#include <ccan/tap/tap.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <err.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

static int ipv6_only;

#ifdef IPV6_V6ONLY
static int my_setsockopt(int sockfd, int level, int optname,
			 const void *optval, socklen_t optlen)
{
	int ret;
	setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6_only,
		   sizeof(ipv6_only));
	ret = setsockopt(sockfd, level, optname, optval, optlen);
	return ret;
}
#define setsockopt my_setsockopt
#endif

#include <ccan/net/net.c>

#define TEST_PORT "65001"

static void do_connect(int family, int type)
{
	int fd, ret;
	struct addrinfo *addr;
	char buf[8];

	/* Just in case... */
	alarm(5);
	addr = net_client_lookup(NULL, TEST_PORT, family, type);
	fd = net_connect(addr);
	if (fd < 0)
		err(1, "Failed net_connect");
	freeaddrinfo(addr);

	ret = write(fd, "Yay!", strlen("Yay!"));
	if (ret != strlen("Yay!"))
		err(1, "Write returned %i", ret);
	ret = read(fd, buf, sizeof(buf));
	if (ret != 5)
		err(1, "Read returned %i", ret);
	if (memcmp(buf, "metoo", ret) != 0)
		err(1, "Read returned '%.*s'", ret, buf);
	close(fd);
}

static int wait_for_readable(int fds[2], int num_fds)
{
	int i, max_fd = -1;
	fd_set set;

	FD_ZERO(&set);
	for (i = 0; i < num_fds; i++) {
		if (fds[i] > max_fd)
			max_fd = fds[i];
		FD_SET(fds[i], &set);
	}

	select(max_fd+1, &set, NULL, NULL, NULL);
	for (i = 0; i < num_fds; i++) {
		if (FD_ISSET(fds[i], &set))
			return i;
	}
	return num_fds+1;
}

int main(void)
{
	struct addrinfo *addr;
	int fds[2], num_fds, i, fd, status, ret;
	char buf[20];
	union {
		struct sockaddr addr;
		struct sockaddr_in ipv4;
		struct sockaddr_in6 ipv6;
	} remote_addr;
	socklen_t addlen = sizeof(remote_addr);

	plan_tests(35);

	/* Simple TCP test. */
	addr = net_server_lookup(TEST_PORT, AF_UNSPEC, SOCK_STREAM);
	ok1(addr);
	num_fds = net_bind(addr, fds);
	ok1(num_fds == 1 || num_fds == 2);

	if (!fork()) {
		for (i = 0; i < num_fds; i++)
			close(fds[i]);
		do_connect(AF_UNSPEC, SOCK_STREAM);
		exit(0);
	}

	i = wait_for_readable(fds, num_fds);
	ok1(i < num_fds);
	fd = accept(fds[i], NULL, NULL);
	ok1(fd >= 0);

	ret = read(fd, buf, strlen("Yay!"));
	ok1(ret == strlen("Yay!"));
	ok1(memcmp(buf, "Yay!", ret) == 0);
	ret = write(fd, "metoo", strlen("metoo"));
	ok1(ret == strlen("metoo"));
	ok1(wait(&status) != -1);
	ok1(WIFEXITED(status));
	ok1(WEXITSTATUS(status) == 0);
	close(fd);
	for (i = 0; i < num_fds; i++)
		close(fds[i]);
	freeaddrinfo(addr);

	/* Simple UDP test. */
	addr = net_server_lookup(TEST_PORT, AF_UNSPEC, SOCK_DGRAM);
	ok1(addr);
	num_fds = net_bind(addr, fds);
	ok1(num_fds == 1 || num_fds == 2);

	if (!fork()) {
		for (i = 0; i < num_fds; i++)
			close(fds[i]);
		do_connect(AF_UNSPEC, SOCK_DGRAM);
		exit(0);
	}

	i = wait_for_readable(fds, num_fds);
	ok1(i < num_fds);
	fd = fds[i];

	ret = recvfrom(fd, buf, strlen("Yay!"), 0,
		       (void *)&remote_addr, &addlen);
	ok1(ret == strlen("Yay!"));
	ok1(memcmp(buf, "Yay!", ret) == 0);
	ret = sendto(fd, "metoo", strlen("metoo"), 0,
		     (void *)&remote_addr, addlen);
	ok1(ret == strlen("metoo"));
	ok1(wait(&status) >= 0);
	ok1(WIFEXITED(status));
	ok1(WEXITSTATUS(status) == 0);
	close(fd);
	for (i = 0; i < num_fds; i++)
		close(fds[i]);

/* This seems like a Linux-only extension */
#ifdef IPV6_V6ONLY
	/* Try to force separate sockets for IPv4/IPv6, if we can. */
	if (addr->ai_next)
		ipv6_only = true;
#endif
	freeaddrinfo(addr);

	if (ipv6_only) {
		int j;

		addr = net_server_lookup(TEST_PORT, AF_UNSPEC, SOCK_STREAM);
		ok1(addr);
		num_fds = net_bind(addr, fds);
		ok1(num_fds == 2);
		freeaddrinfo(addr);

		if (!fork()) {
			for (i = 0; i < num_fds; i++)
				close(fds[i]);
			do_connect(AF_INET, SOCK_STREAM);
			do_connect(AF_INET6, SOCK_STREAM);
			exit(0);
		}

		i = wait_for_readable(fds, num_fds);
		ok1(i < num_fds);
		fd = accept(fds[i], NULL, NULL);
		ok1(fd >= 0);

		ret = read(fd, buf, strlen("Yay!"));
		ok1(ret == strlen("Yay!"));
		ok1(memcmp(buf, "Yay!", ret) == 0);
		ret = write(fd, "metoo", strlen("metoo"));
		ok1(ret == strlen("metoo"));
		close(fd);

		j = wait_for_readable(fds, num_fds);
		ok1(j < num_fds);
		ok1(j != i);
		fd = accept(fds[j], NULL, NULL);
		ok1(fd >= 0);

		ret = read(fd, buf, strlen("Yay!"));
		ok1(ret == strlen("Yay!"));
		ok1(memcmp(buf, "Yay!", ret) == 0);
		ret = write(fd, "metoo", strlen("metoo"));
		ok1(ret == strlen("metoo"));

		ok1(wait(&status) >= 0);
		ok1(WIFEXITED(status));
		ok1(WEXITSTATUS(status) == 0);
		close(fd);
	} else
		skip(16, "No support for IPv6-only binding");

	for (i = 0; i < num_fds; i++)
		close(fds[i]);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
