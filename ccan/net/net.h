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
#endif /* CCAN_NET_H */
