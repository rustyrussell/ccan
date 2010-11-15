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
/*
 * High level api to nfs filesystems
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <ccan/compiler/compiler.h>
#include "nfs.h"
#include "libnfs-raw.h"
#include "rpc/mount.h"
#include "rpc/nfs.h"

struct sync_cb_data {
       int is_finished;
       int status;
       nfs_off_t offset;
       void *return_data;
       int return_int;
};


static void wait_for_reply(struct nfs_context *nfs, struct sync_cb_data *cb_data)
{
	struct pollfd pfd;

	for (;;) {
		if (cb_data->is_finished) {
			break;
		}
		pfd.fd = nfs_get_fd(nfs);
		pfd.events = nfs_which_events(nfs);

		if (poll(&pfd, 1, -1) < 0) {
			printf("Poll failed");
			cb_data->status = -EIO;
			break;
		}
		if (nfs_service(nfs, pfd.revents) < 0) {
			printf("nfs_service failed\n");
			cb_data->status = -EIO;
			break;
		}
	}
}






/*
 * connect to the server and mount the export
 */
static void mount_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("mount/mnt call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_mount_sync(struct nfs_context *nfs, const char *server, const char *export)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_mount_async(nfs, server, export, mount_cb, &cb_data) != 0) {
		printf("nfs_mount_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}


/*
 * stat()
 */
static void stat_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("stat call failed with \"%s\"\n", (char *)data);
		return;
	}

	memcpy(cb_data->return_data, data, sizeof(struct stat));
}

int nfs_stat_sync(struct nfs_context *nfs, const char *path, struct stat *st)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;
	cb_data.return_data = st;

	if (nfs_stat_async(nfs, path, stat_cb, &cb_data) != 0) {
		printf("nfs_stat_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}




/*
 * open()
 */
static void open_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;
	struct nfsfh *fh, **nfsfh;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("open call failed with \"%s\"\n", (char *)data);
		return;
	}

	fh    = data;
	nfsfh = cb_data->return_data;
	*nfsfh = fh;
}

int nfs_open_sync(struct nfs_context *nfs, const char *path, int mode, struct nfsfh **nfsfh)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;
	cb_data.return_data = nfsfh;

	if (nfs_open_async(nfs, path, mode, open_cb, &cb_data) != 0) {
		printf("nfs_open_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}




/*
 * pread()
 */
static void pread_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;
	char *buffer;
	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("pread call failed with \"%s\"\n", (char *)data);
		return;
	}

	buffer = cb_data->return_data;
	memcpy(buffer, (char *)data, status);
}

int nfs_pread_sync(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_off_t offset, size_t count, char *buffer)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;
	cb_data.return_data = buffer;

	if (nfs_pread_async(nfs, nfsfh, offset, count, pread_cb, &cb_data) != 0) {
		printf("nfs_pread_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}

/*
 * read()
 */
int nfs_read_sync(struct nfs_context *nfs, struct nfsfh *nfsfh, size_t count, char *buffer)
{
	return nfs_pread_sync(nfs, nfsfh, nfs_get_current_offset(nfsfh), count, buffer);
}

/*
 * close()
 */
static void close_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;
	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("close call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_close_sync(struct nfs_context *nfs, struct nfsfh *nfsfh)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_close_async(nfs, nfsfh, close_cb, &cb_data) != 0) {
		printf("nfs_close_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}




/*
 * fstat()
 */
int nfs_fstat_sync(struct nfs_context *nfs, struct nfsfh *nfsfh, struct stat *st)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;
	cb_data.return_data = st;

	if (nfs_fstat_async(nfs, nfsfh, stat_cb, &cb_data) != 0) {
		printf("nfs_fstat_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}


/*
 * pwrite()
 */
static void pwrite_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;
	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("pwrite call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_pwrite_sync(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_off_t offset, size_t count, char *buf)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_pwrite_async(nfs, nfsfh, offset, count, buf, pwrite_cb, &cb_data) != 0) {
		printf("nfs_pwrite_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}

/*
 * write()
 */
int nfs_write_sync(struct nfs_context *nfs, struct nfsfh *nfsfh, size_t count, char *buf)
{
	return nfs_pwrite_sync(nfs, nfsfh, nfs_get_current_offset(nfsfh), count, buf);
}


/*
 * fsync()
 */
static void fsync_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;
	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("fsync call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_fsync_sync(struct nfs_context *nfs, struct nfsfh *nfsfh)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_fsync_async(nfs, nfsfh, fsync_cb, &cb_data) != 0) {
		printf("nfs_fsync_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}




/*
 * ftruncate()
 */
static void ftruncate_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;
	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("ftruncate call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_ftruncate_sync(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_off_t length)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_ftruncate_async(nfs, nfsfh, length, ftruncate_cb, &cb_data) != 0) {
		printf("nfs_ftruncate_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}



/*
 * truncate()
 */
static void truncate_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;
	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("truncate call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_truncate_sync(struct nfs_context *nfs, const char *path, nfs_off_t length)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_truncate_async(nfs, path, length, truncate_cb, &cb_data) != 0) {
		printf("nfs_ftruncate_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}





/*
 * mkdir()
 */
static void mkdir_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;
	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("mkdir call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_mkdir_sync(struct nfs_context *nfs, const char *path)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_mkdir_async(nfs, path, mkdir_cb, &cb_data) != 0) {
		printf("nfs_mkdir_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}





/*
 * rmdir()
 */
static void rmdir_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;
	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("rmdir call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_rmdir_sync(struct nfs_context *nfs, const char *path)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_rmdir_async(nfs, path, rmdir_cb, &cb_data) != 0) {
		printf("nfs_rmdir_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}



/*
 * creat()
 */
static void creat_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;
	struct nfsfh *fh, **nfsfh;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("creat call failed with \"%s\"\n", (char *)data);
		return;
	}

	fh    = data;
	nfsfh = cb_data->return_data;
	*nfsfh = fh;
}

int nfs_creat_sync(struct nfs_context *nfs, const char *path, int mode, struct nfsfh **nfsfh)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;
	cb_data.return_data = nfsfh;

	if (nfs_creat_async(nfs, path, mode, creat_cb, &cb_data) != 0) {
		printf("nfs_creat_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}




/*
 * unlink()
 */
static void unlink_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("unlink call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_unlink_sync(struct nfs_context *nfs, const char *path)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_unlink_async(nfs, path, unlink_cb, &cb_data) != 0) {
		printf("nfs_unlink_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}



/*
 * opendir()
 */
static void opendir_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;
	struct nfsdir *dir, **nfsdir;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("opendir call failed with \"%s\"\n", (char *)data);
		return;
	}

	dir     = data;
	nfsdir  = cb_data->return_data;
	*nfsdir = dir;
}

int nfs_opendir_sync(struct nfs_context *nfs, const char *path, struct nfsdir **nfsdir)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;
	cb_data.return_data = nfsdir;

	if (nfs_opendir_async(nfs, path, opendir_cb, &cb_data) != 0) {
		printf("nfs_opendir_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}


/*
 * lseek()
 */
static void lseek_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("lseek call failed with \"%s\"\n", (char *)data);
		return;
	}

	if (cb_data->return_data != NULL) {
		memcpy(cb_data->return_data, data, sizeof(nfs_off_t));
	}
}

int nfs_lseek_sync(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_off_t offset, int whence, nfs_off_t *current_offset)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;
	cb_data.return_data = current_offset;

	if (nfs_lseek_async(nfs, nfsfh, offset, whence, lseek_cb, &cb_data) != 0) {
		printf("nfs_lseek_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}



/*
 * statvfs()
 */
static void statvfs_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("statvfs call failed with \"%s\"\n", (char *)data);
		return;
	}

	memcpy(cb_data->return_data, data, sizeof(struct statvfs));
}

int nfs_statvfs_sync(struct nfs_context *nfs, const char *path, struct statvfs *svfs)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;
	cb_data.return_data = svfs;

	if (nfs_statvfs_async(nfs, path, statvfs_cb, &cb_data) != 0) {
		printf("nfs_statvfs_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}





/*
 * readlink()
 */
static void readlink_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("readlink call failed with \"%s\"\n", (char *)data);
		return;
	}

	if (strlen(data) > (size_t)cb_data->return_int) {
		printf("Too small buffer for readlink\n");
		cb_data->status = -ENAMETOOLONG;
		return;
	}

	memcpy(cb_data->return_data, data, strlen(data)+1);
}

int nfs_readlink_sync(struct nfs_context *nfs, const char *path, char *buf, int bufsize)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;
	cb_data.return_data = buf;
	cb_data.return_int  = bufsize;

	if (nfs_readlink_async(nfs, path, readlink_cb, &cb_data) != 0) {
		printf("nfs_readlink_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}



/*
 * chmod()
 */
static void chmod_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("chmod call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_chmod_sync(struct nfs_context *nfs, const char *path, int mode)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_chmod_async(nfs, path, mode, chmod_cb, &cb_data) != 0) {
		printf("nfs_chmod_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}




/*
 * fchmod()
 */
static void fchmod_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("fchmod call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_fchmod_sync(struct nfs_context *nfs, struct nfsfh *nfsfh, int mode)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_fchmod_async(nfs, nfsfh, mode, fchmod_cb, &cb_data) != 0) {
		printf("nfs_fchmod_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}




/*
 * chown()
 */
static void chown_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("chown call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_chown_sync(struct nfs_context *nfs, const char *path, int uid, int gid)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_chown_async(nfs, path, uid, gid, chown_cb, &cb_data) != 0) {
		printf("nfs_chown_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}

/*
 * fchown()
 */
static void fchown_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("fchown call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_fchown_sync(struct nfs_context *nfs, struct nfsfh *nfsfh, int uid, int gid)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_fchown_async(nfs, nfsfh, uid, gid, fchown_cb, &cb_data) != 0) {
		printf("nfs_fchown_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}



/*
 * utimes()
 */
static void utimes_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("utimes call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_utimes_sync(struct nfs_context *nfs, const char *path, struct timeval *times)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_utimes_async(nfs, path, times, utimes_cb, &cb_data) != 0) {
		printf("nfs_utimes_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}



/*
 * utime()
 */
static void utime_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("utime call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_utime_sync(struct nfs_context *nfs, const char *path, struct utimbuf *times)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_utime_async(nfs, path, times, utime_cb, &cb_data) != 0) {
		printf("nfs_utimes_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}




/*
 * access()
 */
static void access_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("access call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_access_sync(struct nfs_context *nfs, const char *path, int mode)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_access_async(nfs, path, mode, access_cb, &cb_data) != 0) {
		printf("nfs_access_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}



/*
 * symlink()
 */
static void symlink_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("symlink call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_symlink_sync(struct nfs_context *nfs, const char *oldpath, const char *newpath)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_symlink_async(nfs, oldpath, newpath, symlink_cb, &cb_data) != 0) {
		printf("nfs_symlink_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}



/*
 * rename()
 */
static void rename_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("rename call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_rename_sync(struct nfs_context *nfs, const char *oldpath, const char *newpath)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_rename_async(nfs, oldpath, newpath, rename_cb, &cb_data) != 0) {
		printf("nfs_rename_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}



/*
 * link()
 */
static void link_cb(int status, struct nfs_context *nfs UNUSED, void *data, void *private_data)
{
	struct sync_cb_data *cb_data = private_data;

	cb_data->is_finished = 1;
	cb_data->status = status;

	if (status < 0) {
		printf("link call failed with \"%s\"\n", (char *)data);
		return;
	}
}

int nfs_link_sync(struct nfs_context *nfs, const char *oldpath, const char *newpath)
{
	struct sync_cb_data cb_data;

	cb_data.is_finished = 0;

	if (nfs_link_async(nfs, oldpath, newpath, link_cb, &cb_data) != 0) {
		printf("nfs_link_async failed\n");
		return -1;
	}

	wait_for_reply(nfs, &cb_data);

	return cb_data.status;
}
