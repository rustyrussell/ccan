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

/* Example program using the lowlevel raw interface.
 * This allow accurate control of the exact commands that are being used.
 */

#define SERVER "10.1.1.27"
#define EXPORT "/VIRTUAL"

#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <ccan/nfs/nfs.h>
#include <ccan/nfs/libnfs-raw.h>
#include <ccan/nfs/rpc/mount.h>

struct client {
       char *server;
       char *export;
       uint32_t mount_port;
       int is_finished;
};

void mount_mnt_cb(struct rpc_context *rpc, int status, void *data, void *private_data)
{
	mountres3 *res;
	struct client *client = private_data;

	if (status == RPC_STATUS_ERROR) {
		printf("mount/mnt call failed with \"%s\"\n", (char *)data);
		exit(10);
	}
	if (status != RPC_STATUS_SUCCESS) {
		printf("mount/mnt call to server %s failed, status:%d\n", client->server, status);
		exit(10);
	}

	res = data;
	if (res->fhs_status != MNT3_OK) {
		printf("RPC error: Mount failed with error %s(%d) %s(%d)", mountstat3_to_str(res->fhs_status), res->fhs_status, strerror(-mountstat3_to_errno(res->fhs_status)), -mountstat3_to_errno(res->fhs_status));
		exit(10);
	}

	printf("Got reply from server for MOUNT/MNT procedure.\n");
	client->is_finished = 1;
}


void mount_null_cb(struct rpc_context *rpc, int status, void *data, void *private_data)
{
	struct client *client = private_data;

	if (status == RPC_STATUS_ERROR) {
		printf("mount null call failed with \"%s\"\n", (char *)data);
		exit(10);
	}
	if (status != RPC_STATUS_SUCCESS) {
		printf("mount null call to server %s failed, status:%d\n", client->server, status);
		exit(10);
	}

	printf("Got reply from server for MOUNT/NULL procedure.\n");
	printf("Send MOUNT/MNT command for %s\n", client->export);
	if (rpc_mount_mnt_async(rpc, mount_mnt_cb, client->export, client) != 0) {
		printf("Failed to send mnt request\n");
		exit(10);
	}

}

void mount_connect_cb(struct rpc_context *rpc, int status, void *data, void *private_data)
{
	struct client *client = private_data;

	if (status != RPC_STATUS_SUCCESS) {
		printf("connection to RPC.MOUNTD on server %s failed\n", client->server);
		exit(10);
	}

	printf("Connected to RPC.MOUNTD on %s:%d\n", client->server, client->mount_port);
	printf("Send NULL request to check if RPC.MOUNTD is actually running\n");
	if (rpc_mount_null_async(rpc, mount_null_cb, client) != 0) {
		printf("Failed to send null request\n");
		exit(10);
	}
}


void pmap_getport_cb(struct rpc_context *rpc, int status, void *data, void *private_data)
{
	struct client *client = private_data;
	uint32_t port;

	if (status == RPC_STATUS_ERROR) {
		printf("portmapper getport call failed with \"%s\"\n", (char *)data);
		exit(10);
	}
       	if (status != RPC_STATUS_SUCCESS) {
		printf("portmapper getport call to server %s failed, status:%d\n", client->server, status);
		exit(10);
	}

	client->mount_port = *(uint32_t *)data;
	printf("GETPORT returned Port:%d\n", client->mount_port);
	if (client->mount_port == 0) {
		printf("RPC.MOUNTD is not available on server : %s\n", client->server, client->mount_port);
		exit(10);
	}		

	printf("Disconnect socket from portmap server\n");
	if (rpc_disconnect(rpc, "normal disconnect") != 0) {
		printf("Failed to disconnect socket to portmapper\n");
		exit(10);
	}

	printf("Connect to RPC.MOUNTD on %s:%d\n", client->server, client->mount_port);
	if (rpc_connect_async(rpc, client->server, client->mount_port, 1, mount_connect_cb, client) != 0) {
		printf("Failed to start connection\n");
		exit(10);
	}
}


void pmap_null_cb(struct rpc_context *rpc, int status, void *data, void *private_data)
{
	struct client *client = private_data;

	if (status == RPC_STATUS_ERROR) {
		printf("portmapper null call failed with \"%s\"\n", (char *)data);
		exit(10);
	}
	if (status != RPC_STATUS_SUCCESS) {
		printf("portmapper null call to server %s failed, status:%d\n", client->server, status);
		exit(10);
	}

	printf("Got reply from server for PORTMAP/NULL procedure.\n");
	printf("Send getport request asking for MOUNT port\n");
	if (rpc_pmap_getport_async(rpc, MOUNT_PROGRAM, MOUNT_V3, pmap_getport_cb, client) != 0) {
		printf("Failed to send getport request\n");
		exit(10);
	}
}

void pmap_connect_cb(struct rpc_context *rpc, int status, void *data, void *private_data)
{
	struct client *client = private_data;

	printf("pmap_connect_cb    status:%d.\n", status);
	if (status != RPC_STATUS_SUCCESS) {
		printf("connection to portmapper on server %s failed\n", client->server);
		exit(10);
	}

	printf("Send NULL request to check if portmapper is actually running\n");
	if (rpc_pmap_null_async(rpc, pmap_null_cb, client) != 0) {
		printf("Failed to send null request\n");
		exit(10);
	}
}


int main(int argc, char *argv[])
{
	struct rpc_context *rpc;
	struct pollfd pfd;
	int ret;
	struct client client;

	rpc = rpc_init_context();
	if (rpc == NULL) {
		printf("failed to init context\n");
		exit(10);
	}

	client.server = SERVER;
	client.export = EXPORT;
	client.is_finished = 0;
	if (rpc_connect_async(rpc, client.server, 111, 0, pmap_connect_cb, &client) != 0) {
		printf("Failed to start connection\n");
		exit(10);
	}

	for (;;) {
		pfd.fd = rpc_get_fd(rpc);
		pfd.events = rpc_which_events(rpc);

		if (poll(&pfd, 1, -1) < 0) {
			printf("Poll failed");
			exit(10);
		}
		if (rpc_service(rpc, pfd.revents) < 0) {
			printf("rpc_service failed\n");
			break;
		}
		if (client.is_finished) {
			break;
		}
	}
	
	rpc_destroy_context(rpc);
	rpc=NULL;
	printf("nfsclient finished\n");
	return 0;
}
