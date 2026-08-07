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

/* Example program using the highlevel async interface.
 */

#define SERVER "10.1.1.27"
#define EXPORT "/VIRTUAL"
#define NFSFILE "/BOOKS/Classics/Dracula.djvu"
#define NFSDIR "/BOOKS/Classics/"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <ccan/nfs/nfs.h>

struct client {
       char *server;
       char *export;
       uint32_t mount_port;
       struct nfsfh *nfsfh;
       int is_finished;
};

void nfs_opendir_cb(int status, struct nfs_context *nfs, void *data, void *private_data)
{
	struct client *client = private_data;
	struct nfsdir *nfsdir = data;
	struct nfsdirent *nfsdirent;

	if (status < 0) {
		printf("opendir failed with \"%s\"\n", (char *)data);
		exit(10);
	}

	printf("opendir successful\n");
	while((nfsdirent = nfs_readdir(nfs, nfsdir)) != NULL) {
		printf("Inode:%d Name:%s\n", (int)nfsdirent->inode, nfsdirent->name);
	}
	nfs_closedir(nfs, nfsdir);

	client->is_finished = 1;
}

void nfs_close_cb(int status, struct nfs_context *nfs, void *data, void *private_data)
{
	struct client *client = private_data;

	if (status < 0) {
		printf("close failed with \"%s\"\n", (char *)data);
		exit(10);
	}

	printf("close successful\n");
	printf("call opendir(%s)\n", NFSDIR);
	if (nfs_opendir_async(nfs, NFSDIR, nfs_opendir_cb, client) != 0) {
		printf("Failed to start async nfs close\n");
		exit(10);
	}
}

void nfs_fstat_cb(int status, struct nfs_context *nfs, void *data, void *private_data)
{
	struct client *client = private_data;
	struct stat *st;
 
	if (status < 0) {
		printf("fstat call failed with \"%s\"\n", (char *)data);
		exit(10);
	}

	printf("Got reply from server for fstat(%s).\n", NFSFILE);
	st = (struct stat *)data;
	printf("Mode %04o\n", st->st_mode);
	printf("Size %d\n", (int)st->st_size);
	printf("Inode %04o\n", (int)st->st_ino);

	printf("Close file\n");
	if (nfs_close_async(nfs, client->nfsfh, nfs_close_cb, client) != 0) {
		printf("Failed to start async nfs close\n");
		exit(10);
	}
}

void nfs_read_cb(int status, struct nfs_context *nfs, void *data, void *private_data)
{
	struct client *client = private_data;
	char *read_data;
	int i;

	if (status < 0) {
		printf("read failed with \"%s\"\n", (char *)data);
		exit(10);
	}

	printf("read successful with %d bytes of data\n", status);
	read_data = data;
	for (i=0;i<16;i++) {
		printf("%02x ", read_data[i]&0xff);
	}
	printf("\n");
	printf("Fstat file :%s\n", NFSFILE);
	if (nfs_fstat_async(nfs, client->nfsfh, nfs_fstat_cb, client) != 0) {
		printf("Failed to start async nfs fstat\n");
		exit(10);
	}
}

void nfs_open_cb(int status, struct nfs_context *nfs, void *data, void *private_data)
{
	struct client *client = private_data;
	struct nfsfh *nfsfh;

	if (status < 0) {
		printf("open call failed with \"%s\"\n", (char *)data);
		exit(10);
	}

	nfsfh         = data;
	client->nfsfh = nfsfh;
	printf("Got reply from server for open(%s). Handle:%p\n", NFSFILE, data);
	printf("Read first 16 bytes\n");
	if (nfs_pread_async(nfs, nfsfh, 0, 16, nfs_read_cb, client) != 0) {
		printf("Failed to start async nfs open\n");
		exit(10);
	}
}

void nfs_stat_cb(int status, struct nfs_context *nfs, void *data, void *private_data)
{
	struct client *client = private_data;
	struct stat *st;
 
	if (status < 0) {
		printf("stat call failed with \"%s\"\n", (char *)data);
		exit(10);
	}

	printf("Got reply from server for stat(%s).\n", NFSFILE);
	st = (struct stat *)data;
	printf("Mode %04o\n", st->st_mode);
	printf("Size %d\n", (int)st->st_size);
	printf("Inode %04o\n", (int)st->st_ino);

	printf("Open file for reading :%s\n", NFSFILE);
	if (nfs_open_async(nfs, NFSFILE, O_RDONLY, nfs_open_cb, client) != 0) {
		printf("Failed to start async nfs open\n");
		exit(10);
	}
}

void nfs_mount_cb(int status, struct nfs_context *nfs, void *data, void *private_data)
{
	struct client *client = private_data;

	if (status < 0) {
		printf("mount/mnt call failed with \"%s\"\n", (char *)data);
		exit(10);
	}

	printf("Got reply from server for MOUNT/MNT procedure.\n");
	printf("Stat file :%s\n", NFSFILE);
	if (nfs_stat_async(nfs, NFSFILE, nfs_stat_cb, client) != 0) {
		printf("Failed to start async nfs stat\n");
		exit(10);
	}
}



int main(int argc, char *argv[])
{
	struct nfs_context *nfs;
	struct pollfd pfd;
	int ret;
	struct client client;

	client.server = SERVER;
	client.export = EXPORT;
	client.is_finished = 0;

	nfs = nfs_init_context();
	if (nfs == NULL) {
		printf("failed to init context\n");
		exit(10);
	}

	ret = nfs_mount_async(nfs, client.server, client.export, nfs_mount_cb, &client);
	if (ret != 0) {
		printf("Failed to start async nfs mount\n");
		exit(10);
	}

	for (;;) {
		pfd.fd = nfs_get_fd(nfs);
		pfd.events = nfs_which_events(nfs);

		if (poll(&pfd, 1, -1) < 0) {
			printf("Poll failed");
			exit(10);
		}
		if (nfs_service(nfs, pfd.revents) < 0) {
			printf("nfs_service failed\n");
			break;
		}
		if (client.is_finished) {
			break;
		}
	}
	
	nfs_destroy_context(nfs);
	printf("nfsclient finished\n");
	return 0;
}
