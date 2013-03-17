/* Licensed under BSD-MIT - see LICENSE file for details */
#ifndef CCAN_NET_H
#define CCAN_NET_H
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
 *	#include <sys/types.h>
 *	#include <sys/socket.h>
 *	#include <stdio.h>
 *	#include <netdb.h>
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
 * net_server_lookup - look up a service name to bind to.
 * @service: the service to look up
 * @family: Usually AF_UNSPEC, otherwise AF_INET or AF_INET6.
 * @socktype: SOCK_DGRAM or SOCK_STREAM.
 *
 * This will do a synchronous lookup of a given name, returning a linked list
 * of results, or NULL on error.  You should use freeaddrinfo() to free it.
 *
 * Example:
 *	#include <sys/types.h>
 *	#include <sys/socket.h>
 *	#include <stdio.h>
 *	#include <netdb.h>
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
