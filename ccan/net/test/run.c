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

static bool we_faked_double = false;

/* Get a localhost on ipv4 and IPv6.  Fake it if we have to. */
static struct addrinfo* double_addr_lookup(char* buf)
{
	struct addrinfo *addr, *addr2;

	addr = net_client_lookup("localhost", buf, AF_UNSPEC, SOCK_STREAM);
	if (!addr)
		return addr;

	/* If we only got one, we need to fake up the other one. */
	if (addr->ai_next) {
		addr2 = addr->ai_next;
	} else {
		we_faked_double = true;

		/* OK, IPv4 only? */
		if (addr->ai_family == AF_INET) {
			/* These are the names I found on my Ubuntu system. */
			addr2 = net_client_lookup("ip6-localhost", buf,
						  AF_UNSPEC, SOCK_STREAM);
			if (!addr2)
				addr2 = net_client_lookup("localhost6", buf,
							  AF_UNSPEC,
							  SOCK_STREAM);
		} else if (addr->ai_family == AF_INET6)
			/* IPv6 only?  This is a guess... */
			addr2 = net_client_lookup("ip4-localhost", buf,
						  AF_UNSPEC, SOCK_STREAM);
		if (!addr2)
			return NULL;
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

static void double_addr_free(struct addrinfo* addr)
{
	struct addrinfo *addr2;
	if (we_faked_double) {
		addr2 = addr->ai_next;
		addr->ai_next = NULL;
	}
	freeaddrinfo(addr);
	if (we_faked_double)
		freeaddrinfo(addr2);
}

int main(void)
{
	struct addrinfo *addr;
	int fd, status;
	struct sockaddr saddr;
	socklen_t slen;
	char buf[20];
	unsigned int port;

	plan_tests(14);
	port = server(AF_INET, SOCK_STREAM);
	sprintf(buf, "%u", port);

	addr = double_addr_lookup(buf);
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
	double_addr_free(addr);

	port = server(AF_INET6, SOCK_STREAM);
	sprintf(buf, "%u", port);

	addr = double_addr_lookup(buf);
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
	double_addr_free(addr);

	wait(&status);
	ok1(WIFEXITED(status));
	ok1(WEXITSTATUS(status) == 0);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
