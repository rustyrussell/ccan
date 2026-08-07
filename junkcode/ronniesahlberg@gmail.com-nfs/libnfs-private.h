#ifndef CCAN_NFS_LIBNFS_PRIVATE_H
#define CCAN_NFS_LIBNFS_PRIVATE_H
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

#include <rpc/auth.h>

struct rpc_context {
	int fd;
	int is_connected;

	char *error_string;

	rpc_cb connect_cb;
	void *connect_data;

	AUTH *auth;
	unsigned long xid;

       /* buffer used for encoding RPC PDU */
       char *encodebuf;
       int encodebuflen;

       struct rpc_pdu *outqueue;
       struct rpc_pdu *waitpdu;

       int insize;
       int inpos;
       char *inbuf;
};

struct rpc_pdu {
	struct rpc_pdu *prev, *next;

	unsigned long xid;
	XDR xdr;

	int written;
	struct rpc_data outdata;

	rpc_cb cb;
	void *private_data;

	/* function to decode the xdr reply data and buffer to decode into */
	xdrproc_t xdr_decode_fn;
	caddr_t xdr_decode_buf;
	int xdr_decode_bufsize;
};

struct rpc_pdu *rpc_allocate_pdu(struct rpc_context *rpc, int program, int version, int procedure, rpc_cb cb, void *private_data, xdrproc_t xdr_decode_fn, int xdr_bufsize);
void rpc_free_pdu(struct rpc_context *rpc, struct rpc_pdu *pdu);
int rpc_queue_pdu(struct rpc_context *rpc, struct rpc_pdu *pdu);
int rpc_get_pdu_size(char *buf);
int rpc_process_pdu(struct rpc_context *rpc, char *buf, int size);
void rpc_error_all_pdus(struct rpc_context *rpc, char *error);

#endif /* CCAN_NFS_LIBNFS_PRIVATE_H */
