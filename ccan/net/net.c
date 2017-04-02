/* Licensed under BSD-MIT - see LICENSE file for details */
#include <ccan/net/net.h>
#include <ccan/noerr/noerr.h>
#include <poll.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <assert.h>

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

static int start_connect(const struct addrinfo *addr, bool *immediate)
{
	int fd;

	*immediate = false;

	fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	if (fd == -1)
		return fd;

	if (!set_nonblock(fd, true))
		goto close;

	if (connect(fd, addr->ai_addr, addr->ai_addrlen) == 0) {
		/* Immediate connect. */
		*immediate = true;
		return fd;
	}

	if (errno == EINPROGRESS)
		return fd;

close:
	close_noerr(fd);
	return -1;
}


int net_connect_async(const struct addrinfo *addrinfo, struct pollfd pfds[2])
{
	const struct addrinfo *addr[2] = { NULL, NULL };
	unsigned int i;

	pfds[0].fd = pfds[1].fd = -1;
	pfds[0].events = pfds[1].events = POLLOUT;

	/* Give IPv6 a slight advantage, by trying it first. */
	for (; addrinfo; addrinfo = addrinfo->ai_next) {
		switch (addrinfo->ai_family) {
		case AF_INET:
			addr[1] = addrinfo;
			break;
		case AF_INET6:
			addr[0] = addrinfo;
			break;
		default:
			continue;
		}
	}

	/* In case we found nothing. */
	errno = ENOENT;
	for (i = 0; i < 2; i++) {
		bool immediate;

		if (!addr[i])
			continue;

		pfds[i].fd = start_connect(addr[i], &immediate);
		if (immediate) {
			if (pfds[!i].fd != -1)
				close(pfds[!i].fd);
			if (!set_nonblock(pfds[i].fd, false)) {
				close_noerr(pfds[i].fd);
				return -1;
			}
			return pfds[i].fd;
		}
	}

	if (pfds[0].fd != -1 || pfds[1].fd != -1)
		errno = EINPROGRESS;
	return -1;
}

void net_connect_abort(struct pollfd pfds[2])
{
	unsigned int i;

	for (i = 0; i < 2; i++) {
		if (pfds[i].fd != -1)
			close_noerr(pfds[i].fd);
		pfds[i].fd = -1;
	}
}

int net_connect_complete(struct pollfd pfds[2])
{
	unsigned int i;

	assert(pfds[0].fd != -1 || pfds[1].fd != -1);

	for (i = 0; i < 2; i++) {
		int err;
		socklen_t errlen = sizeof(err);

		if (pfds[i].fd == -1)
			continue;
		if (pfds[i].revents & POLLHUP) {
			/* Linux gives this if connecting to local
			 * non-listening port */
			close(pfds[i].fd);
			pfds[i].fd = -1;
			if (pfds[!i].fd == -1) {
				errno = ECONNREFUSED;
				return -1;
			}
			continue;
		}
		if (!(pfds[i].revents & POLLOUT))
			continue;

		if (getsockopt(pfds[i].fd, SOL_SOCKET, SO_ERROR, &err,
			       &errlen) != 0) {
			net_connect_abort(pfds);
			return -1;
		}
		if (err == 0) {
			/* Don't hand them non-blocking fd! */
			if (!set_nonblock(pfds[i].fd, false)) {
				net_connect_abort(pfds);
				return -1;
			}
			/* Close other one. */
			if (pfds[!i].fd != -1)
				close(pfds[!i].fd);
			return pfds[i].fd;
		}
	}

	/* Still going... */
	errno = EINPROGRESS;
	return -1;
}

int net_connect(const struct addrinfo *addrinfo)
{
	struct pollfd pfds[2];
	int sockfd;

	sockfd = net_connect_async(addrinfo, pfds);
	/* Immediate connect or error is easy. */
	if (sockfd >= 0 || errno != EINPROGRESS)
		return sockfd;

	while (poll(pfds, 2, -1) != -1) {
		sockfd = net_connect_complete(pfds);
		if (sockfd >= 0 || errno != EINPROGRESS)
			return sockfd;
	}

	net_connect_abort(pfds);
	return -1;
}

struct addrinfo *net_server_lookup(const char *service,
				   int family,
				   int socktype)
{
	struct addrinfo *res, hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = socktype;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;

	if (getaddrinfo(NULL, service, &hints, &res) != 0)
		return NULL;

	return res;
}

static bool should_listen(const struct addrinfo *addrinfo)
{
#ifdef SOCK_SEQPACKET
	if (addrinfo->ai_socktype == SOCK_SEQPACKET)
		return true;
#endif
	return (addrinfo->ai_socktype == SOCK_STREAM);
}

static int make_listen_fd(const struct addrinfo *addrinfo)
{
	int fd, on = 1;

	fd = socket(addrinfo->ai_family, addrinfo->ai_socktype,
		    addrinfo->ai_protocol);
	if (fd < 0)
		return -1;

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0)
		goto fail;

	if (bind(fd, addrinfo->ai_addr, addrinfo->ai_addrlen) != 0)
		goto fail;

	if (should_listen(addrinfo) && listen(fd, 5) != 0)
		goto fail;
	return fd;

fail:
	close_noerr(fd);
	return -1;
}

int net_bind(const struct addrinfo *addrinfo, int fds[2])
{
	const struct addrinfo *ipv6 = NULL;
	const struct addrinfo *ipv4 = NULL;
	unsigned int num;

	if (addrinfo->ai_family == AF_INET)
		ipv4 = addrinfo;
	else if (addrinfo->ai_family == AF_INET6)
		ipv6 = addrinfo;

	if (addrinfo->ai_next) {
		if (addrinfo->ai_next->ai_family == AF_INET)
			ipv4 = addrinfo->ai_next;
		else if (addrinfo->ai_next->ai_family == AF_INET6)
			ipv6 = addrinfo->ai_next;
	}

	num = 0;
	/* Take IPv6 first, since it might bind to IPv4 port too. */
	if (ipv6) {
		if ((fds[num] = make_listen_fd(ipv6)) >= 0)
			num++;
		else
			ipv6 = NULL;
	}
	if (ipv4) {
		if ((fds[num] = make_listen_fd(ipv4)) >= 0)
			num++;
		else
			ipv4 = NULL;
	}
	if (num == 0)
		return -1;

	return num;
}
