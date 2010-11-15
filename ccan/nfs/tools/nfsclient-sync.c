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

/* Example program using the highlevel sync interface
 */

#define SERVER "10.1.1.27"
#define EXPORT "/VIRTUAL"
#define NFSFILE "/BOOKS/Classics/Dracula.djvu"
#define NFSFILER "/BOOKS/Classics/Dracula.djvu.renamed"
#define NFSFILEW "/BOOKS/Classics/foo"
#define NFSDIR "/BOOKS/Classics/"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <fcntl.h>
#include <ccan/nfs/nfs.h>
#include <rpc/rpc.h>            /* for authunix_create() */

struct client {
       char *server;
       char *export;
       uint32_t mount_port;
       int is_finished;
};


int main(int argc, char *argv[])
{
	struct nfs_context *nfs;
	int i, ret;
	struct client client;
	struct stat st;
	struct nfsfh  *nfsfh;
	struct nfsdir *nfsdir;
	struct nfsdirent *nfsdirent;
	client.server = SERVER;
	client.export = EXPORT;
	client.is_finished = 0;
	char buf[16];
	nfs_off_t offset;
	struct statvfs svfs;

	nfs = nfs_init_context();
	if (nfs == NULL) {
		printf("failed to init context\n");
		exit(10);
	}

	ret = nfs_mount_sync(nfs, client.server, client.export);
	if (ret != 0) {
 		printf("Failed to mount nfs share : %s\n", nfs_get_error(nfs));
		exit(10);
	}
	printf("mounted share successfully\n");
exit(10);

	ret = nfs_stat_sync(nfs, NFSFILE, &st);
	if (ret != 0) {
		printf("Failed to stat(%s) %s\n", NFSFILE, nfs_get_error(nfs));
		exit(10);
	}
	printf("Mode %04o\n", st.st_mode);
	printf("Size %d\n", (int)st.st_size);
	printf("Inode %04o\n", (int)st.st_ino);

	ret = nfs_open_sync(nfs, NFSFILE, O_RDONLY, &nfsfh);
	if (ret != 0) {
		printf("Failed to open(%s) %s\n", NFSFILE, nfs_get_error(nfs));
		exit(10);
	}

	ret = nfs_read_sync(nfs, nfsfh, 16, buf);
	if (ret < 0) {
		printf("Failed to pread(%s) %s\n", NFSFILE, nfs_get_error(nfs));
		exit(10);
	}
	printf("read %d bytes\n", ret);
	for (i=0;i<16;i++) {
		printf("%02x ", buf[i]&0xff);
	}
	printf("\n");
	ret = nfs_read_sync(nfs, nfsfh, 16, buf);
	if (ret < 0) {
		printf("Failed to pread(%s) %s\n", NFSFILE, nfs_get_error(nfs));
		exit(10);
	}
	printf("read %d bytes\n", ret);
	for (i=0;i<16;i++) {
		printf("%02x ", buf[i]&0xff);
	}
	printf("\n");

	ret = (int)nfs_lseek_sync(nfs, nfsfh, 0, SEEK_CUR, &offset);
	if (ret < 0) {
		printf("Failed to lseek(%s) %s\n", NFSFILE, nfs_get_error(nfs));
		exit(10);
	}
	printf("File position is %d\n", (int)offset);

	printf("seek to end of file\n");
	ret = (int)nfs_lseek_sync(nfs, nfsfh, 0, SEEK_END, &offset);
	if (ret < 0) {
		printf("Failed to lseek(%s) %s\n", NFSFILE, nfs_get_error(nfs));
		exit(10);
	}
	printf("File position is %d\n", (int)offset);

	ret = nfs_fstat_sync(nfs, nfsfh, &st);
	if (ret != 0) {
		printf("Failed to stat(%s) %s\n", NFSFILE, nfs_get_error(nfs));
		exit(10);
	}
	printf("Mode %04o\n", st.st_mode);
	printf("Size %d\n", (int)st.st_size);
	printf("Inode %04o\n", (int)st.st_ino);


	ret = nfs_close_sync(nfs, nfsfh);
	if (ret < 0) {
		printf("Failed to close(%s)\n", NFSFILE, nfs_get_error(nfs));
		exit(10);
	}

	ret = nfs_opendir_sync(nfs, NFSDIR, &nfsdir);
	if (ret != 0) {
		printf("Failed to open(%s) %s\n", NFSFILE, nfs_get_error(nfs));
		exit(10);
	}
	while((nfsdirent = nfs_readdir(nfs, nfsdir)) != NULL) {
		printf("Inode:%d Name:%s\n", (int)nfsdirent->inode, nfsdirent->name);
	}
	nfs_closedir(nfs, nfsdir);


	ret = nfs_open_sync(nfs, NFSFILEW, O_WRONLY, &nfsfh);
	if (ret != 0) {
		printf("Failed to open(%s) %s\n", NFSFILEW, nfs_get_error(nfs));
		exit(10);
	}
	ret = nfs_pwrite_sync(nfs, nfsfh, 0, 16, buf);
	if (ret < 0) {
		printf("Failed to pwrite(%s) %s\n", NFSFILEW, nfs_get_error(nfs));
		exit(10);
	}
	ret = nfs_fsync_sync(nfs, nfsfh);
	if (ret < 0) {
		printf("Failed to fsync(%s) %s\n", NFSFILEW, nfs_get_error(nfs));
		exit(10);
	}
	ret = nfs_close_sync(nfs, nfsfh);
	if (ret < 0) {
		printf("Failed to close(%s)\n", NFSFILEW, nfs_get_error(nfs));
		exit(10);
	}


	ret = nfs_statvfs_sync(nfs, NFSDIR, &svfs);
	if (ret < 0) {
		printf("Failed to statvfs(%s)\n", NFSDIR, nfs_get_error(nfs));
		exit(10);
	}
	printf("files %d/%d/%d\n", (int)svfs.f_files, (int)svfs.f_ffree, (int)svfs.f_favail);


	ret = nfs_access_sync(nfs, NFSFILE, R_OK);
	if (ret != 0) {
		printf("Failed to access(%s) %s\n", NFSFILE, nfs_get_error(nfs));
	}

	/* become root */
	nfs_set_auth(nfs, authunix_create("Ronnies-Laptop", 0, 0, 0, NULL));

	ret = nfs_link_sync(nfs, NFSFILE, NFSFILER);
	if (ret != 0) {
		printf("Failed to link(%s) %s\n", NFSFILE, nfs_get_error(nfs));
	}


	nfs_destroy_context(nfs);
	printf("nfsclient finished\n");
	return 0;
}
