#include <ccan/net/net.h>
#include <ccan/net/net.c>
#include <ccan/tap/tap.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <err.h>

static int server(int protocol, int type)
{
	int sock;
	union {
		struct sockaddr addr;
		struct sockaddr_in ipv4;
		struct sockaddr_in6 ipv6;
	} addr;
	socklen_t addlen = sizeof(addr);

	sock = socket(protocol, type, 0);
	if (sock < 0)
		return -1;

	/* Bind to free port. */
	if (listen(sock, 0) != 0)
		return -1;

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

/* Get a localhost on ipv4 and IPv6.  Fake it if we can. */
static struct addrinfo* double_addr_lookup(char* buf, bool *fake_double)
{
	struct addrinfo *addr, *addr2;

	addr = net_client_lookup("localhost", buf, AF_UNSPEC, SOCK_STREAM);
	if (!addr)
		return addr;

	/* If we only got one, we need to fake up the other one. */
	if (addr->ai_next) {
		addr2 = addr->ai_next;
		*fake_double = false;
	} else {
		/* OK, IPv4 only? */
		if (addr->ai_family == AF_INET) {
			/* These are the names I found on my Ubuntu system. */
			addr2 = net_client_lookup("ip6-localhost", buf,
						  AF_UNSPEC, SOCK_STREAM);
			if (!addr2)
				addr2 = net_client_lookup("localhost6", buf,
							  AF_UNSPEC,
							  SOCK_STREAM);
			if (!addr2)
				addr2 = net_client_lookup("::1", buf,
							  AF_UNSPEC,
							  SOCK_STREAM);
		} else if (addr->ai_family == AF_INET6)
			/* IPv6 only?  This is a guess... */
			addr2 = net_client_lookup("ip4-localhost", buf,
						  AF_UNSPEC, SOCK_STREAM);

		/* Perhaps no support on this system?  Go ahead with one. */
		if (!addr2) {
			*fake_double = false;
			return addr;
		}

		*fake_double = true;
		addr->ai_next = addr2;
	}

	/* More than two? */
	if (addr2->ai_next)
		return NULL;
	/* One IPv4 and one IPv6? */
	if (addr->ai_family == AF_INET && addr2->ai_family == AF_INET6)
		return addr;
	if (addr->ai_family == AF_INET6 && addr2->ai_family == AF_INET)
		return addr;
	return NULL;
}

static void double_addr_free(struct addrinfo* addr, bool fake_double)
{
	if (fake_double) {
		freeaddrinfo(addr->ai_next);
		addr->ai_next = NULL;
	}
	freeaddrinfo(addr);
}

int main(void)
{
	struct addrinfo *addr;
	int fd, status;
	struct sockaddr saddr;
	socklen_t slen;
	char buf[20];
	int port;
	bool fake_double;

	plan_tests(14);

	port = server(AF_INET, SOCK_STREAM);
	if (port == -1) {
		/* No IPv4 support?  Maybe one day this will happen! */
		if (errno == EAFNOSUPPORT)
			skip(6, "No IPv4 socket support");
		else
			fail("Could not create IPv4 listening socket: %s",
			     strerror(errno));
	} else {
		sprintf(buf, "%u", port);

		addr = double_addr_lookup(buf, &fake_double);
		ok1(addr);
		fd = net_connect(addr);
		ok1(fd >= 0);

		slen = sizeof(saddr);
		ok1(getsockname(fd, &saddr, &slen) == 0);
		diag("family = %d", saddr.sa_family);
		ok1(saddr.sa_family == AF_INET);
		status = read(fd, buf, sizeof(buf));
		ok(status == strlen("Yay!"),
		   "Read returned %i (%s)", status, strerror(errno));
		ok1(strncmp(buf, "Yay!", strlen("Yay!")) == 0);
		close(fd);
		double_addr_free(addr, fake_double);
	}

	port = server(AF_INET6, SOCK_STREAM);
	if (port == -1) {
		/* No IPv6 support? */
		if (errno == EAFNOSUPPORT)
			skip(6, "No IPv6 socket support");
		else
			fail("Could not create IPv6 listening socket: %s",
			     strerror(errno));
	} else {
		sprintf(buf, "%u", port);

		addr = double_addr_lookup(buf, &fake_double);
		ok1(addr);
		fd = net_connect(addr);
		ok1(fd >= 0);

		slen = sizeof(saddr);
		ok1(getsockname(fd, &saddr, &slen) == 0);
		ok1(saddr.sa_family == AF_INET6);
		status = read(fd, buf, sizeof(buf));
		ok(status == strlen("Yay!"),
		   "Read returned %i (%s)", status, strerror(errno));
		ok1(strncmp(buf, "Yay!", strlen("Yay!")) == 0);
		close(fd);
		double_addr_free(addr, fake_double);
	}

	wait(&status);
	ok1(WIFEXITED(status));
	ok1(WEXITSTATUS(status) == 0);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
