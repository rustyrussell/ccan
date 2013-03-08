/*
   Copyright (C) 2010 by Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "dlinklist.h"
#if HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

static void set_nonblocking(int fd)
{
	unsigned v;
	v = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, v | O_NONBLOCK);
}

int iscsi_connect_async(struct iscsi_context *iscsi, const char *target, iscsi_command_cb cb, void *private_data)
{
	int port = 3260;
	char *str;
	char *addr;
	union {
		struct sockaddr sa;
		struct sockaddr_storage ss;
		struct sockaddr_in sin;
	} s;
	int socksize;

	if (iscsi == NULL) {
		printf("Trying to connect NULL context\n");
		return -1;
	}
	if (iscsi->fd != -1) {
		printf("Trying to connect but already connected\n");
		return -2;
	}

	addr = strdup(target);
	if (addr == NULL) {
		printf("failed to strdup target address\n");
		return -3;
	}
	
	/* check if we have a target portal group tag */
	if ((str = rindex(addr, ',')) != NULL) {
		str[0] = 0;
	}

	/* XXX need handling for {ipv6 addresses} */
	/* for now, assume all is ipv4 */
	if ((str = rindex(addr, ':')) != NULL) {
		port = atoi(str+1);
		str[0] = 0;
	}

	s.sin.sin_family = AF_INET;
	s.sin.sin_port   = htons(port);
	if (inet_pton(AF_INET, addr, &s.sin.sin_addr) != 1) {
		printf("failed to convert to ip address\n");
		free(addr);
		return -4;
	}
	free(addr);

	switch (s.ss.ss_family) {
	case AF_INET:
		iscsi->fd = socket(AF_INET, SOCK_STREAM, 0);
		socksize = sizeof(struct sockaddr_in);
		break;
	default:
		printf("Unknown family :%d\n", s.ss.ss_family);
		return -5;

	}

	if (iscsi->fd == -1) {
		printf("Failed to open socket\n");
		return -6;

	}

	iscsi->connect_cb  = cb;
	iscsi->connect_data = private_data;

	set_nonblocking(iscsi->fd);

	if (connect(iscsi->fd, &s.sa, socksize) != 0 && errno != EINPROGRESS) {
		printf("Connect failed errno : %s (%d)\n", strerror(errno), errno);
		return -7;
	}

	return 0;
}

int iscsi_disconnect(struct iscsi_context *iscsi)
{
	if (iscsi == NULL) {
		printf("Trying to disconnect NULL context\n");
		return -1;
	}
	if (iscsi->is_loggedin != 0) {
		printf("Trying to disconnect while logged in\n");
		return -2;
	}
	if (iscsi->fd == -1) {
		printf("Trying to disconnect but not connected\n");
		return -3;
	}

	close(iscsi->fd);
	iscsi->fd  = -1;

	iscsi->is_connected = 0;

	return 0;
}

int iscsi_get_fd(struct iscsi_context *iscsi)
{
	if (iscsi == NULL) {
		printf("Trying to get fd for NULL context\n");
		return -1;
	}

	return iscsi->fd;
}

int iscsi_which_events(struct iscsi_context *iscsi)
{
	int events = POLLIN;

	if (iscsi->is_connected == 0) {
		events |= POLLOUT;
	}

	if (iscsi->outqueue) {
		events |= POLLOUT;
	}
	return events;
}

static int iscsi_read_from_socket(struct iscsi_context *iscsi)
{
	int available;
	int size;
	unsigned char *buf;
	ssize_t count;

	if (ioctl(iscsi->fd, FIONREAD, &available) != 0) {
		printf("ioctl FIONREAD returned error : %d\n", errno);
		return -1;
	}
	if (available == 0) {
		printf("no data readable in socket, socket is closed\n");
		return -2;
	}
	size = iscsi->insize - iscsi->inpos + available;
	buf = malloc(size);
	if (buf == NULL) {
		printf("failed to allocate %d bytes for input buffer\n", size);
		return -3;
	}
	if (iscsi->insize > iscsi->inpos) {
		memcpy(buf, iscsi->inbuf + iscsi->inpos, iscsi->insize - iscsi->inpos);
		iscsi->insize -= iscsi->inpos;
		iscsi->inpos   = 0;
	}

	count = read(iscsi->fd, buf + iscsi->insize, available);
	if (count == -1) {
		if (errno == EINTR) {
			free(buf);
			buf = NULL;
			return 0;
		}
		printf("read from socket failed, errno:%d\n", errno);
		free(buf);
		buf = NULL;
		return -4;
	}

	if (iscsi->inbuf != NULL) {
		free(iscsi->inbuf);
	}
	iscsi->inbuf   = buf;
	iscsi->insize += count;

	while (1) {
		if (iscsi->insize - iscsi->inpos < 48) {
			return 0;
		}
		count = iscsi_get_pdu_size(iscsi->inbuf + iscsi->inpos);
		if (iscsi->insize + iscsi->inpos < count) {
			return 0;
		}
		if (iscsi_process_pdu(iscsi, iscsi->inbuf + iscsi->inpos, count) != 0) {
			printf("failed to process pdu\n");
			return -5;
		}
		iscsi->inpos += count;
		if (iscsi->inpos == iscsi->insize) {
			free(iscsi->inbuf);
			iscsi->inbuf = NULL;
			iscsi->insize = 0;
			iscsi->inpos = 0;
		}
		if (iscsi->inpos > iscsi->insize) {
			printf("inpos > insize. bug!\n");
			return -6;
		}
	}

	return 0;
}

static int iscsi_write_to_socket(struct iscsi_context *iscsi)
{
	ssize_t count;

	if (iscsi == NULL) {
		printf("trying to write to socket for NULL context\n");
		return -1;
	}
	if (iscsi->fd == -1) {
		printf("trying to write but not connected\n");
		return -2;
	}

	while (iscsi->outqueue != NULL) {
		ssize_t total;

		total = iscsi->outqueue->outdata.size;
		total = (total +3) & 0xfffffffc;

		count = write(iscsi->fd, iscsi->outqueue->outdata.data + iscsi->outqueue->written, total - iscsi->outqueue->written);
		if (count == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				printf("socket would block, return from write to socket\n");
				return 0;
			}
			printf("Error when writing to socket :%d\n", errno);
			return -3;
		}

		iscsi->outqueue->written += count;
		if (iscsi->outqueue->written == total) {
			struct iscsi_pdu *pdu = iscsi->outqueue;

	       	    	DLIST_REMOVE(iscsi->outqueue, pdu);
			DLIST_ADD_END(iscsi->waitpdu, pdu, NULL);
		}
	}
	return 0;
}

int iscsi_service(struct iscsi_context *iscsi, int revents)
{
	if (revents & POLLERR) {
		printf("iscsi_service: POLLERR, socket error\n");
		iscsi->connect_cb(iscsi, ISCSI_STATUS_ERROR, NULL, iscsi->connect_data);
		return -1;
	}
	if (revents & POLLHUP) {
		printf("iscsi_service: POLLHUP, socket error\n");
		iscsi->connect_cb(iscsi, ISCSI_STATUS_ERROR, NULL, iscsi->connect_data);
		return -2;
	}

	if (iscsi->is_connected == 0 && iscsi->fd != -1 && revents&POLLOUT) {
		iscsi->is_connected = 1;
		iscsi->connect_cb(iscsi, ISCSI_STATUS_GOOD, NULL, iscsi->connect_data);
		return 0;
	}

	if (revents & POLLOUT && iscsi->outqueue != NULL) {
		if (iscsi_write_to_socket(iscsi) != 0) {
			printf("write to socket failed\n");
			return -3;
		}
	}
	if (revents & POLLIN) {
		if (iscsi_read_from_socket(iscsi) != 0) {
			printf("read from socket failed\n");
			return -4;
		}
	}

	return 0;
}

int iscsi_queue_pdu(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	if (iscsi == NULL) {
		printf("trying to queue to NULL context\n");
		return -1;
	}
	if (pdu == NULL) {
		printf("trying to queue NULL pdu\n");
		return -2;
	}
	DLIST_ADD_END(iscsi->outqueue, pdu, NULL);

	return 0;
}



