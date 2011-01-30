#include <ccan/net/net.h>
#include <ccan/net/net.c>
#include <ccan/tap/tap.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <err.h>

static unsigned int server(int protocol, int type)
{
	int sock;
	union {
		struct sockaddr addr;
		struct sockaddr_in ipv4;
		struct sockaddr_in6 ipv6;
	} addr;
	socklen_t addlen = sizeof(addr);

	sock = socket(protocol, type, 0);

	/* Bind to free port. */
	listen(sock, 0);

	/* Figure out what port it gave us. */
	getsockname(sock, &addr.addr, &addlen);
	fflush(stdout);

	if (fork() == 0) {
		int ret, fd;

		alarm(3);
		fd = accept(sock, NULL, 0);
		if (fd < 0)
			err(1, "Accepting from socket %i", sock);

		ret = write(fd, "Yay!", strlen("Yay!"));
		if (ret != strlen("Yay!"))
			err(1, "Write returned %i", ret);
		exit(0);
	}
	close(sock);
	return ntohs(protocol == AF_INET
		     ? addr.ipv4.sin_port : addr.ipv6.sin6_port);
}

int main(void)
{
	struct addrinfo *addr, *addr2;
	int fd, status;
	struct sockaddr saddr;
	socklen_t slen = sizeof(saddr);
	char buf[20];
	unsigned int port;

	plan_tests(16);
	port = server(AF_INET, SOCK_STREAM);
	sprintf(buf, "%u", port);

	addr = net_client_lookup("localhost", buf, AF_UNSPEC, SOCK_STREAM);
	addr2 = net_client_lookup("ip6-localhost", buf,
				  AF_UNSPEC, SOCK_STREAM);
	ok1(addr);
	ok1(addr2);
	/* Join them as if they were from one lookup. */
	addr->ai_next = addr2;

	fd = net_connect(addr);
	ok1(fd >= 0);

	ok1(getsockname(fd, &saddr, &slen) == 0);
	ok1(saddr.sa_family == AF_INET);
	status = read(fd, buf, sizeof(buf));
	ok(status == strlen("Yay!"),
	   "Read returned %i (%s)", status, strerror(errno));
	ok1(strncmp(buf, "Yay!", strlen("Yay!")) == 0);
	close(fd);
	addr->ai_next = NULL;
	freeaddrinfo(addr);
	freeaddrinfo(addr2);

	port = server(AF_INET6, SOCK_STREAM);
	sprintf(buf, "%u", port);

	addr = net_client_lookup("localhost", buf, AF_UNSPEC, SOCK_STREAM);
	addr2 = net_client_lookup("ip6-localhost", buf,
				  AF_UNSPEC, SOCK_STREAM);
	ok1(addr);
	ok1(addr2);
	/* Join them as if they were from one lookup. */
	addr->ai_next = addr2;

	fd = net_connect(addr);
	ok1(fd >= 0);

	ok1(getsockname(fd, &saddr, &slen) == 0);
	ok1(saddr.sa_family == AF_INET6);
	status = read(fd, buf, sizeof(buf));
	ok(status == strlen("Yay!"),
	   "Read returned %i (%s)", status, strerror(errno));
	ok1(strncmp(buf, "Yay!", strlen("Yay!")) == 0);
	close(fd);
	addr->ai_next = NULL;
	freeaddrinfo(addr);
	freeaddrinfo(addr2);

	wait(&status);
	ok1(WIFEXITED(status));
	ok1(WEXITSTATUS(status) == 0);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
