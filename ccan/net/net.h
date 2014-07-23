/* Licensed under BSD-MIT - see LICENSE file for details */
#ifndef CCAN_NET_H
#define CCAN_NET_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdbool.h>

struct pollfd;

/**
 * net_client_lookup - look up a network name to connect to.
 * @hostname: the name to look up
 * @service: the service to look up
 * @family: Usually AF_UNSPEC, otherwise AF_INET or AF_INET6.
 * @socktype: SOCK_DGRAM or SOCK_STREAM.
 *
 * This will do a synchronous lookup of a given name, returning a linked list
 * of results, or NULL on error.  You should use freeaddrinfo() to free it.
 *
 * Example:
 *	#include <stdio.h>
 *	#include <poll.h>
 *	#include <err.h>
 *	...
 *	struct addrinfo *addr;
 *
 *	// Get a TCP connection to ccan.ozlabs.org daytime port.
 *	addr = net_client_lookup("ccan.ozlabs.org", "daytime",
 *				 AF_UNSPEC, SOCK_STREAM);
 *	if (!addr)
 *		errx(1, "Failed to look up daytime at ccan.ozlabs.org");
 */
struct addrinfo *net_client_lookup(const char *hostname,
				   const char *service,
				   int family,
				   int socktype);

/**
 * net_connect - connect to a server
 * @addrinfo: linked list struct addrinfo (usually from net_client_lookup).
 *
 * This synchronously connects to a server described by @addrinfo, or returns
 * -1 on error (and sets errno).
 *
 * Example:
 *	int fd;
 *	...
 *	fd = net_connect(addr);
 *	if (fd < 0)
 *		err(1, "Failed to connect to ccan.ozlabs.org");
 *	freeaddrinfo(addr);
 */
int net_connect(const struct addrinfo *addrinfo);

/**
 * net_connect_async - initiate connect to a server
 * @addrinfo: linked list struct addrinfo (usually from net_client_lookup).
 * @pfds: array of two struct pollfd.
 *
 * This begins connecting to a server described by @addrinfo,
 * and places the one or two file descriptors into pfds[0] and pfds[1].
 * It returns a valid file descriptor if connect() returned immediately.
 *
 * Otherwise it returns -1 and sets errno, most likely EINPROGRESS.
 * In this case, poll() on pfds and call net_connect_complete().
 *
 * Example:
 *	struct pollfd pfds[2];
 *	...
 *	fd = net_connect_async(addr, pfds);
 *	if (fd < 0 && errno != EINPROGRESS)
 *		err(1, "Failed to connect to ccan.ozlabs.org");
 */
int net_connect_async(const struct addrinfo *addrinfo, struct pollfd *pfds);

/**
 * net_connect_complete - complete net_connect_async call.
 * @pfds: array of two struct pollfd handed to net_connect_async.
 *
 * When poll() reports some activity, this determines whether a connection
 * has completed.  If so, it cleans up and returns the connected fd.
 * Otherwise, it returns -1, and sets errno (usually EINPROGRESS).
 *
 * Example:
 *	// After net_connect_async.
 *	while (fd < 0 && errno == EINPROGRESS) {
 *		// Wait for activity...
 *		poll(pfds, 2, -1);
 *		fd = net_connect_complete(pfds);
 *	}
 *	if (fd < 0)
 *		err(1, "connecting");
 *	printf("Connected on fd %i!\n", fd);
 */
int net_connect_complete(struct pollfd *pfds);

/**
 * net_connect_abort - abort a net_connect_async call.
 * @pfds: array of two struct pollfd handed to net_connect_async.
 *
 * Closes the file descriptors.
 *
 * Example:
 *	// After net_connect_async.
 *	if (poll(pfds, 2, 1000) == 0) { // Timeout.
 *		net_connect_abort(pfds);
 *		fd = -1;
 *	} else {
 *		fd = net_connect_complete(pfds);
 *		if (fd < 0)
 *			err(1, "connecting");
 *	}
 */
void net_connect_abort(struct pollfd *pfds);

/**
 * net_server_lookup - look up a service name to bind to.
 * @service: the service to look up
 * @family: Usually AF_UNSPEC, otherwise AF_INET or AF_INET6.
 * @socktype: SOCK_DGRAM or SOCK_STREAM.
 *
 * This will do a synchronous lookup of a given name, returning a linked list
 * of results, or NULL on error.  You should use freeaddrinfo() to free it.
 *
 * Example:
 *	#include <stdio.h>
 *	#include <err.h>
 *	...
 *	struct addrinfo *addr;
 *
 *	// Get address(es) to bind for our service.
 *	addr = net_server_lookup("8888", AF_UNSPEC, SOCK_STREAM);
 *	if (!addr)
 *		errx(1, "Failed to look up 8888 to bind to");
 */
struct addrinfo *net_server_lookup(const char *service,
				   int family,
				   int socktype);

/**
 * net_bind - create listening socket(s)
 * @addrinfo: the address(es) to bind to.
 * @fds: array of two fds.
 *
 * This will create one (or if necessary) two sockets, mark them
 * SO_REUSEADDR, bind them to the given address(es), and make them
 * listen() (if the socket type is SOCK_STREAM or SOCK_SEQPACKET).
 *
 * Returns -1 (and sets errno) on error, or 1 or 2 depending on how many
 * @fds are valid.
 *
 * Example:
 *	int fds[2], i, num_fds;
 *
 *	num_fds = net_bind(addr, fds);
 *	if (num_fds < 0)
 *		err(1, "Failed to listen on port 8888");
 *
 *	for (i = 0; i < num_fds; i++)
 *		printf(" Got fd %u/%u: %i\n", i, num_fds, fds[i]);
 */
int net_bind(const struct addrinfo *addrinfo, int fds[2]);
#endif /* CCAN_NET_H */
