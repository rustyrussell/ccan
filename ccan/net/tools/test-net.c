#include <ccan/net/net.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <err.h>

static unsigned int count_addrs(const struct addrinfo *addr, int family)
{
	unsigned int num = 0;

	while (addr) {
		if (family == AF_UNSPEC || family == addr->ai_family)
			num++;
		addr = addr->ai_next;
	}
	return num;
}

int main(int argc, char *argv[])
{
	struct addrinfo *addr;
	const char *dest, *port;
	int fd;
	struct sockaddr saddr;
	socklen_t slen = sizeof(saddr);

	if (argc == 2) {
		dest = argv[1];
		port = "http";
	} else if (argc == 3) {
		dest = argv[1];
		port = argv[2];
	} else
		errx(1, "Usage: test-net <target> [<port>]");

	addr = net_client_lookup(dest, port, AF_UNSPEC, SOCK_STREAM);
	if (!addr)
		err(1, "Failed to look up %s", dest);

	printf("Received %u IPv4 addresses, %u IPv6 addresses\n",
	       count_addrs(addr, AF_INET), count_addrs(addr, AF_INET6));

	fd = net_connect(addr);
	if (fd < 0)
		err(1, "Failed to connect to %s", dest);

	if (getsockname(fd, &saddr, &slen) == 0)
		printf("Connected via %s\n",
		       saddr.sa_family == AF_INET6 ? "IPv6"
		       : saddr.sa_family == AF_INET ? "IPv4"
		       : "UNKNOWN??");
	else
		err(1, "Failed to get socket type for connection");
	return 0;
}
