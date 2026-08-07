#ifndef CCAN_NFS_LIBNFS_RAW_H
#define CCAN_NFS_LIBNFS_RAW_H
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
 * This is the lowlevel interface to access NFS resources.
 * Through this interface you have access to the full gamut of nfs and nfs related
 * protocol as well as the XDR encoded/decoded structures.
 */
#include <stdint.h>

struct rpc_data {
       int size;
       unsigned char *data;
};

struct rpc_context;
struct rpc_context *rpc_init_context(void);
void rpc_destroy_context(struct rpc_context *rpc);

struct AUTH;
void rpc_set_auth(struct rpc_context *rpc, struct AUTH *auth);

int rpc_get_fd(struct rpc_context *rpc);
int rpc_which_events(struct rpc_context *rpc);
int rpc_service(struct rpc_context *rpc, int revents);
char *rpc_get_error(struct rpc_context *rpc);


#define RPC_STATUS_SUCCESS	   	0
#define RPC_STATUS_ERROR		1
#define RPC_STATUS_CANCEL		2

typedef void (*rpc_cb)(struct rpc_context *rpc, int status, void *data, void *private_data);

/*
 * Async connection to the tcp port at server:port.
 * Function returns
 *  0 : The connection was initiated. Once the connection establish finishes, the callback will be invoked.
 * <0 : An error occurred when trying to set up the connection. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : The tcp connection was successfully established.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The connection failed to establish.
 *                      data is the erro string.
 * RPC_STATUS_CANCEL  : The connection attempt was aborted before it could complete.
 *                    : data is NULL.
 */
int rpc_connect_async(struct rpc_context *rpc, const char *server, int port, int use_privileged_port, rpc_cb cb, void *private_data);
/*
 * When disconnecting a connection in flight. All commands in flight will be called with the callback
 * and status RPC_STATUS_ERROR. Data will be the error string for the disconnection.
 */
int rpc_disconnect(struct rpc_context *rpc, char *error);

void rpc_set_error(struct rpc_context *rpc, char *error_string, ...);


/*
 * PORTMAP FUNCTIONS
 */

/*
 * Call PORTMAPPER/NULL
 * Function returns
 *  0 : The connection was initiated. Once the connection establish finishes, the callback will be invoked.
 * <0 : An error occurred when trying to set up the connection. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the portmapper daemon.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the portmapper.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_pmap_null_async(struct rpc_context *rpc, rpc_cb cb, void *private_data);


/*
 * Call PORTMAPPER/GETPORT.
 * Function returns
 *  0 : The connection was initiated. Once the connection establish finishes, the callback will be invoked.
 * <0 : An error occurred when trying to set up the connection. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the portmapper daemon.
 *                      data is a (uint32_t *), containing the port returned.
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the portmapper.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_pmap_getport_async(struct rpc_context *rpc, int program, int version, rpc_cb cb, void *private_data);



/*
 * MOUNT FUNCTIONS
 */
char *mountstat3_to_str(int stat);
int mountstat3_to_errno(int error);

/*
 * Call MOUNT/NULL
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the mount daemon.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the mount daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_mount_null_async(struct rpc_context *rpc, rpc_cb cb, void *private_data);

/*
 * Call MOUNT/MNT
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the mount daemon.
 *                      data is  mountres3 *.
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the mount daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_mount_mnt_async(struct rpc_context *rpc, rpc_cb cb, char *export, void *private_data);

/*
 * Call MOUNT/DUMP
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the mount daemon.
 *                      data is a mountlist.
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the mount daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_mount_dump_async(struct rpc_context *rpc, rpc_cb cb, void *private_data);

/*
 * Call MOUNT/UMNT
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the mount daemon.
 *                      data NULL.
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the mount daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_mount_umnt_async(struct rpc_context *rpc, rpc_cb cb, char *export, void *private_data);

/*
 * Call MOUNT/UMNTALL
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the mount daemon.
 *                      data NULL.
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the mount daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_mount_umntall_async(struct rpc_context *rpc, rpc_cb cb, void *private_data);

/*
 * Call MOUNT/EXPORT
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the mount daemon.
 *                      data is an exports.
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the mount daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_mount_export_async(struct rpc_context *rpc, rpc_cb cb, void *private_data);




/*
 * NFS FUNCTIONS
 */
struct nfs_fh3;
char *nfsstat3_to_str(int error);
int nfsstat3_to_errno(int error);

/*
 * Call NFS/NULL
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfs_null_async(struct rpc_context *rpc, rpc_cb cb, void *private_data);

/*
 * Call NFS/GETATTR
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is GETATTR3res
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfs_getattr_async(struct rpc_context *rpc, rpc_cb cb, struct nfs_fh3 *fh, void *private_data);

/*
 * Call NFS/LOOKUP
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is LOOKUP3res
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfs_lookup_async(struct rpc_context *rpc, rpc_cb cb, struct nfs_fh3 *fh, char *name, void *private_data);

/*
 * Call NFS/ACCESS
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is ACCESS3res
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfs_access_async(struct rpc_context *rpc, rpc_cb cb, struct nfs_fh3 *fh, int access, void *private_data);

/*
 * Call NFS/READ
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is ACCESS3res
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfs_read_async(struct rpc_context *rpc, rpc_cb cb, struct nfs_fh3 *fh, nfs_off_t offset, size_t count, void *private_data);

/*
 * Call NFS/WRITE
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is WRITE3res *
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfs_write_async(struct rpc_context *rpc, rpc_cb cb, struct nfs_fh3 *fh, char *buf, nfs_off_t offset, size_t count, int stable_how, void *private_data);

/*
 * Call NFS/COMMIT
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is COMMIT3res *
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfs_commit_async(struct rpc_context *rpc, rpc_cb cb, struct nfs_fh3 *fh, void *private_data);


/*
 * Call NFS/SETATTR
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is SETATTR3res *
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
struct SETATTR3args;
int rpc_nfs_setattr_async(struct rpc_context *rpc, rpc_cb cb, struct SETATTR3args *args, void *private_data);



/*
 * Call NFS/MKDIR
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is MKDIR3res *
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfs_mkdir_async(struct rpc_context *rpc, rpc_cb cb, struct nfs_fh3 *fh, char *dir, void *private_data);





/*
 * Call NFS/RMDIR
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is RMDIR3res *
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfs_rmdir_async(struct rpc_context *rpc, rpc_cb cb, struct nfs_fh3 *fh, char *dir, void *private_data);




/*
 * Call NFS/CREATE
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is CREATE3res *
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfs_create_async(struct rpc_context *rpc, rpc_cb cb, struct nfs_fh3 *fh, char *name, int mode, void *private_data);




/*
 * Call NFS/REMOVE
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is REMOVE3res *
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfs_remove_async(struct rpc_context *rpc, rpc_cb cb, struct nfs_fh3 *fh, char *name, void *private_data);



/*
 * Call NFS/REMOVE
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is READDIR3res *
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfs_readdir_async(struct rpc_context *rpc, rpc_cb cb, struct nfs_fh3 *fh, uint64_t cookie, char *cookieverf, int count, void *private_data);

/*
 * Call NFS/FSSTAT
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is FSSTAT3res
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfs_fsstat_async(struct rpc_context *rpc, rpc_cb cb, struct nfs_fh3 *fh, void *private_data);




/*
 * Call NFS/READLINK
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is READLINK3res *
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfs_readlink_async(struct rpc_context *rpc, rpc_cb cb, struct nfs_fh3 *fh, void *private_data);



/*
 * Call NFS/SYMLINK
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is SYMLINK3res *
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfs_symlink_async(struct rpc_context *rpc, rpc_cb cb, struct nfs_fh3 *fh, char *newname, char *oldpath, void *private_data);


/*
 * Call NFS/RENAME
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is RENAME3res *
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfs_rename_async(struct rpc_context *rpc, rpc_cb cb, struct nfs_fh3 *olddir, char *oldname, struct nfs_fh3 *newdir, char *newname, void *private_data);



/*
 * Call NFS/LINK
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is LINK3res *
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfs_link_async(struct rpc_context *rpc, rpc_cb cb, struct nfs_fh3 *file, struct nfs_fh3 *newdir, char *newname, void *private_data);







/*
 * NFSACL FUNCTIONS
 */
/*
 * Call NFSACL/NULL
 * Function returns
 *  0 : The call was initiated. The callback will be invoked when the call completes.
 * <0 : An error occurred when trying to set up the call. The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the nfs daemon.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : An error occurred when trying to contact the nfs daemon.
 *                      data is the error string.
 * RPC_STATUS_CANCEL : The connection attempt was aborted before it could complete.
 *                     data is NULL.
 */
int rpc_nfsacl_null_async(struct rpc_context *rpc, rpc_cb cb, void *private_data);

#endif /* CCAN_NFS_LIBNFS_RAW_H */
