/*
   Copyright (C) by Ronnie Sahlberg <ronniesahlberg@gmail.com> 2010
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <rpc/xdr.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include "nfs.h"
#include "libnfs-raw.h"
#include "libnfs-private.h"
#include "dlinklist.h"

static void set_nonblocking(int fd)
{
	unsigned v;
	v = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, v | O_NONBLOCK);
}

int rpc_get_fd(struct rpc_context *rpc)
{
	return rpc->fd;
}

int rpc_which_events(struct rpc_context *rpc)
{
	int events = POLLIN;

	if (rpc->is_connected == 0) {
		events |= POLLOUT;
	}

	if (rpc->outqueue) {
		events |= POLLOUT;
	}
	return events;
}

static int rpc_write_to_socket(struct rpc_context *rpc)
{
	ssize_t count;

	if (rpc == NULL) {
		printf("trying to write to socket for NULL context\n");
		return -1;
	}
	if (rpc->fd == -1) {
		printf("trying to write but not connected\n");
		return -2;
	}

	while (rpc->outqueue != NULL) {
		ssize_t total;

		total = rpc->outqueue->outdata.size;

		count = write(rpc->fd, rpc->outqueue->outdata.data + rpc->outqueue->written, total - rpc->outqueue->written);
		if (count == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				printf("socket would block, return from write to socket\n");
				return 0;
			}
			printf("Error when writing to socket :%s(%d)\n", strerror(errno), errno);
			return -3;
		}

		rpc->outqueue->written += count;
		if (rpc->outqueue->written == total) {
			struct rpc_pdu *pdu = rpc->outqueue;

	       	    	DLIST_REMOVE(rpc->outqueue, pdu);
			DLIST_ADD_END(rpc->waitpdu, pdu, NULL);
		}
	}
	return 0;
}

static int rpc_read_from_socket(struct rpc_context *rpc)
{
	int available;
	int size;
	unsigned char *buf;
	ssize_t count;

	if (ioctl(rpc->fd, FIONREAD, &available) != 0) {
		rpc_set_error(rpc, "Ioctl FIONREAD returned error : %d. Closing socket.", errno);
		return -1;
	}
	if (available == 0) {
		rpc_set_error(rpc, "Socket has been closed");
		return -2;
	}
	size = rpc->insize - rpc->inpos + available;
	buf = malloc(size);
	if (buf == NULL) {
		rpc_set_error(rpc, "Out of memory: failed to allocate %d bytes for input buffer. Closing socket.", size);
		return -3;
	}
	if (rpc->insize > rpc->inpos) {
		memcpy(buf, rpc->inbuf + rpc->inpos, rpc->insize - rpc->inpos);
		rpc->insize -= rpc->inpos;
		rpc->inpos   = 0;
	}

	count = read(rpc->fd, buf + rpc->insize, available);
	if (count == -1) {
		if (errno == EINTR) {
			free(buf);
			buf = NULL;
			return 0;
		}
		rpc_set_error(rpc, "Read from socket failed, errno:%d. Closing socket.", errno);
		free(buf);
		buf = NULL;
		return -4;
	}

	if (rpc->inbuf != NULL) {
		free(rpc->inbuf);
	}
	rpc->inbuf   = (char *)buf;
	rpc->insize += count;

	while (1) {
		if (rpc->insize - rpc->inpos < 4) {
			return 0;
		}
		count = rpc_get_pdu_size(rpc->inbuf + rpc->inpos);
		if (rpc->insize + rpc->inpos < count) {
			return 0;
		}
		if (rpc_process_pdu(rpc, rpc->inbuf + rpc->inpos, count) != 0) {
			rpc_set_error(rpc, "Invalid/garbage pdu received from server. Closing socket");
			return -5;
		}
		rpc->inpos += count;
		if (rpc->inpos == rpc->insize) {
			free(rpc->inbuf);
			rpc->inbuf = NULL;
			rpc->insize = 0;
			rpc->inpos = 0;
		}
	}
	return 0;
}



int rpc_service(struct rpc_context *rpc, int revents)
{
	if (revents & POLLERR) {
		printf("rpc_service: POLLERR, socket error\n");
		if (rpc->is_connected == 0) {
			rpc_set_error(rpc, "Failed to connect to server socket.");
		} else {
			rpc_set_error(rpc, "Socket closed with POLLERR");
		}
		rpc->connect_cb(rpc, RPC_STATUS_ERROR, rpc->error_string, rpc->connect_data);
		return -1;
	}
	if (revents & POLLHUP) {
		printf("rpc_service: POLLHUP, socket error\n");
		rpc_set_error(rpc, "Socket failed with POLLHUP");
		rpc->connect_cb(rpc, RPC_STATUS_ERROR, rpc->error_string, rpc->connect_data);
		return -2;
	}

	if (rpc->is_connected == 0 && rpc->fd != -1 && revents&POLLOUT) {
		rpc->is_connected = 1;
		rpc->connect_cb(rpc, RPC_STATUS_SUCCESS, NULL, rpc->connect_data);
		return 0;
	}

	if (revents & POLLOUT && rpc->outqueue != NULL) {
		if (rpc_write_to_socket(rpc) != 0) {
			printf("write to socket failed\n");
			return -3;
		}
	}

	if (revents & POLLIN) {
		if (rpc_read_from_socket(rpc) != 0) {
			rpc_disconnect(rpc, rpc_get_error(rpc));
			return -4;
		}
	}

	return 0;
}


int rpc_connect_async(struct rpc_context *rpc, const char *server, int port, int use_privileged_port, rpc_cb cb, void *private_data)
{
	struct sockaddr_storage s;
	struct sockaddr_in *sin = (struct sockaddr_in *)&s;
	int socksize;

	if (rpc->fd != -1) {
		rpc_set_error(rpc, "Trying to connect while already connected");
		printf("%s\n", rpc->error_string);
		return -1;
	}

	sin->sin_family = AF_INET;
	sin->sin_port   = htons(port);
	if (inet_pton(AF_INET, server, &sin->sin_addr) != 1) {
		rpc_set_error(rpc, "Not a valid server ip address");
		printf("%s\n", rpc->error_string);
		return -2;
	}

	switch (s.ss_family) {
	case AF_INET:
		rpc->fd = socket(AF_INET, SOCK_STREAM, 0);
		socksize = sizeof(struct sockaddr_in);
		break;
	}

	if (rpc->fd == -1) {
		rpc_set_error(rpc, "Failed to open socket");
		printf("%s\n", rpc->error_string);
		return -3;
	}

	/* if we are root, try to find a privileged port to use (512 - 1023) */
	if (geteuid() == 0 && use_privileged_port != 0) {
		struct sockaddr_storage ls;
		int ret, count = 0;
		static int local_port = 0;

		if (local_port == 0) {
			srandom(getpid() ^ time(NULL));
			local_port = random()%512 + 512;
		}

		do {
			count ++;
			if (local_port >= 1024) {
				local_port = 512;
			}
			switch (s.ss_family) {
			case AF_INET:
				bzero(&ls, socksize);
				((struct sockaddr_in *)&ls)->sin_family      = AF_INET;
				((struct sockaddr_in *)&ls)->sin_addr.s_addr = INADDR_ANY;
				((struct sockaddr_in *)&ls)->sin_port        = htons(local_port++);
				break;
			}

			ret  = bind(rpc->fd, (struct sockaddr *)&ls, socksize);
		} while (ret != 0  && count < 50);
	}

	rpc->connect_cb  = cb;
	rpc->connect_data = private_data;

	set_nonblocking(rpc->fd);

	if (connect(rpc->fd, (struct sockaddr *)&s, socksize) != 0 && errno != EINPROGRESS) {
		rpc_set_error(rpc, "connect() to server failed");
		printf("%s\n", rpc->error_string);
		return -4;
	}

	return 0;
}

int rpc_disconnect(struct rpc_context *rpc, char *error)
{
	if (rpc->fd != -1) {
		close(rpc->fd);
	}
	rpc->fd  = -1;

	rpc->is_connected = 0;

	rpc_error_all_pdus(rpc, error);

	return 0;
}
