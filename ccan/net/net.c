/* Licensed under BSD-MIT - see LICENSE file for details */
#include <ccan/net/net.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <netinet/in.h>

struct addrinfo *net_client_lookup(const char *hostname,
				   const char *service,
				   int family,
				   int socktype)
{
	struct addrinfo hints;
	struct addrinfo *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = socktype;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	if (getaddrinfo(hostname, service, &hints, &res) != 0)
		return NULL;

	return res;
}

static bool set_nonblock(int fd, bool nonblock)
{
	long flags;

	flags = fcntl(fd, F_GETFL);
	if (flags == -1)
		return false;

	if (nonblock)
		flags |= O_NONBLOCK;
	else
		flags &= ~(long)O_NONBLOCK;

	return (fcntl(fd, F_SETFL, flags) == 0);
}

/* We only handle IPv4 and IPv6 */
#define MAX_PROTOS 2

static void remove_fd(struct pollfd pfd[],
		      const struct addrinfo *addr[],
		      socklen_t slen[],
		      unsigned int *num,
		      unsigned int i)
{
	memmove(pfd + i, pfd + i + 1, (*num - i - 1) * sizeof(pfd[0]));
	memmove(addr + i, addr + i + 1, (*num - i - 1) * sizeof(addr[0]));
	memmove(slen + i, slen + i + 1, (*num - i - 1) * sizeof(slen[0]));
	(*num)--;
}

int net_connect(const struct addrinfo *addrinfo)
{
	int sockfd = -1, saved_errno;
	unsigned int i, num;
	const struct addrinfo *ipv4 = NULL, *ipv6 = NULL;
	const struct addrinfo *addr[MAX_PROTOS];
	socklen_t slen[MAX_PROTOS];
	struct pollfd pfd[MAX_PROTOS];

	for (; addrinfo; addrinfo = addrinfo->ai_next) {
		switch (addrinfo->ai_family) {
		case AF_INET:
			if (!ipv4)
				ipv4 = addrinfo;
			break;
		case AF_INET6:
			if (!ipv6)
				ipv6 = addrinfo;
			break;
		}
	}

	num = 0;
	/* We give IPv6 a slight edge by connecting it first. */
	if (ipv6) {
		addr[num] = ipv6;
		slen[num] = sizeof(struct sockaddr_in6);
		pfd[num].fd = socket(AF_INET6, ipv6->ai_socktype,
				     ipv6->ai_protocol);
		if (pfd[num].fd != -1)
			num++;
	}
	if (ipv4) {
		addr[num] = ipv4;
		slen[num] = sizeof(struct sockaddr_in);
		pfd[num].fd = socket(AF_INET, ipv4->ai_socktype,
				     ipv4->ai_protocol);
		if (pfd[num].fd != -1)
			num++;
	}

	for (i = 0; i < num; i++) {
		if (!set_nonblock(pfd[i].fd, true)) {
			remove_fd(pfd, addr, slen, &num, i--);
			continue;
		}
		/* Connect *can* be instant. */
		if (connect(pfd[i].fd, addr[i]->ai_addr, slen[i]) == 0)
			goto got_one;
		if (errno != EINPROGRESS) {
			/* Remove dead one. */
			remove_fd(pfd, addr, slen, &num, i--);
		}
		pfd[i].events = POLLOUT;
	}

	while (num && poll(pfd, num, -1) != -1) {
		for (i = 0; i < num; i++) {
			int err;
			socklen_t errlen = sizeof(err);
			if (!pfd[i].revents)
				continue;
			if (getsockopt(pfd[i].fd, SOL_SOCKET, SO_ERROR, &err,
				       &errlen) != 0)
				goto out;
			if (err == 0)
				goto got_one;

			/* Remove dead one. */
			errno = err;
			remove_fd(pfd, addr, slen, &num, i--);
		}
	}

got_one:
	/* We don't want to hand them a non-blocking socket! */
	if (set_nonblock(pfd[i].fd, false))
		sockfd = pfd[i].fd;

out:
	saved_errno = errno;
	for (i = 0; i < num; i++)
		if (pfd[i].fd != sockfd)
			close(pfd[i].fd);
	errno = saved_errno;
	return sockfd;
}
