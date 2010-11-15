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
#include <strings.h>
#include <rpc/xdr.h>
#include <rpc/rpc_msg.h>
#include <ccan/compiler/compiler.h>
#include "dlinklist.h"
#include "nfs.h"
#include "libnfs-raw.h"
#include "libnfs-private.h"

struct rpc_pdu *rpc_allocate_pdu(struct rpc_context *rpc, int program, int version, int procedure, rpc_cb cb, void *private_data, xdrproc_t xdr_decode_fn, int xdr_decode_bufsize)
{
	struct rpc_pdu *pdu;
	struct rpc_msg msg;

	if (rpc == NULL) {
		printf("trying to allocate rpc pdu on NULL context\n");
		return NULL;
	}

	pdu = malloc(sizeof(struct rpc_pdu));
	if (pdu == NULL) {
		printf("Failed to allocate pdu structure\n");
		return NULL;
	}
	bzero(pdu, sizeof(struct rpc_pdu));
	pdu->xid                = rpc->xid++;
	pdu->cb                 = cb;
	pdu->private_data       = private_data;
	pdu->xdr_decode_fn      = xdr_decode_fn;
	pdu->xdr_decode_bufsize = xdr_decode_bufsize;

	xdrmem_create(&pdu->xdr, rpc->encodebuf, rpc->encodebuflen, XDR_ENCODE);
	xdr_setpos(&pdu->xdr, 4); /* skip past the record marker */

	bzero(&msg, sizeof(struct rpc_msg));
	msg.rm_xid = pdu->xid;
        msg.rm_direction = CALL;
	msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	msg.rm_call.cb_prog = program;
	msg.rm_call.cb_vers = version;
	msg.rm_call.cb_proc = procedure;
	msg.rm_call.cb_cred = rpc->auth->ah_cred;
	msg.rm_call.cb_verf = rpc->auth->ah_verf;

	if (xdr_callmsg(&pdu->xdr, &msg) == 0) {
		printf("xdr_callmsg failed\n");
		xdr_destroy(&pdu->xdr);
		free(pdu);
		return NULL;
	}

	return pdu;
}

void rpc_free_pdu(struct rpc_context *rpc UNUSED, struct rpc_pdu *pdu)
{
	if (pdu->outdata.data != NULL) {
		free(pdu->outdata.data);
		pdu->outdata.data = NULL;
	}

	if (pdu->xdr_decode_buf != NULL) {
		xdr_free(pdu->xdr_decode_fn, pdu->xdr_decode_buf);
		free(pdu->xdr_decode_buf);
		pdu->xdr_decode_buf = NULL;
	}

	xdr_destroy(&pdu->xdr);
	free(pdu);
}


int rpc_queue_pdu(struct rpc_context *rpc, struct rpc_pdu *pdu)
{
	int size, recordmarker;

	size = xdr_getpos(&pdu->xdr);

	/* write recordmarker */
	xdr_setpos(&pdu->xdr, 0);
	recordmarker = (size - 4) | 0x80000000;
	xdr_int(&pdu->xdr, &recordmarker);

	pdu->outdata.size = size;
	pdu->outdata.data = malloc(pdu->outdata.size);
	if (pdu->outdata.data == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate buffer for pdu\n");
		rpc_free_pdu(rpc, pdu);
		return -1;
	}

	memcpy(pdu->outdata.data, rpc->encodebuf, pdu->outdata.size);
	DLIST_ADD_END(rpc->outqueue, pdu, NULL);

	return 0;
}

int rpc_get_pdu_size(char *buf)
{
	uint32_t size;

	size = ntohl(*(uint32_t *)buf);

	if ((size & 0x80000000) == 0) {
		printf("cant handle oncrpc fragments\n");
		return -1;
	}

	return (size & 0x7fffffff) + 4;
}

static int rpc_process_reply(struct rpc_context *rpc, struct rpc_pdu *pdu, XDR *xdr)
{
	struct rpc_msg msg;

	bzero(&msg, sizeof(struct rpc_msg));
	msg.acpted_rply.ar_verf = _null_auth;
	if (pdu->xdr_decode_bufsize > 0) {
		pdu->xdr_decode_buf = malloc(pdu->xdr_decode_bufsize);
		if (pdu->xdr_decode_buf == NULL) {
			printf("xdr_replymsg failed in portmap_getport_reply\n");
			pdu->cb(rpc, RPC_STATUS_ERROR, "Failed to allocate buffer for decoding of XDR reply", pdu->private_data);
			return 0;
		}
		bzero(pdu->xdr_decode_buf, pdu->xdr_decode_bufsize);
	}
	msg.acpted_rply.ar_results.where = pdu->xdr_decode_buf;
	msg.acpted_rply.ar_results.proc  = pdu->xdr_decode_fn;

	if (xdr_replymsg(xdr, &msg) == 0) {
		printf("xdr_replymsg failed in portmap_getport_reply\n");
		pdu->cb(rpc, RPC_STATUS_ERROR, "Message rejected by server", pdu->private_data);
		if (pdu->xdr_decode_buf != NULL) {
			free(pdu->xdr_decode_buf);
			pdu->xdr_decode_buf = NULL;
		}
		return 0;
	}
	if (msg.rm_reply.rp_stat != MSG_ACCEPTED) {
		pdu->cb(rpc, RPC_STATUS_ERROR, "RPC Packet not accepted by the server", pdu->private_data);
		return 0;
	}
	switch (msg.rm_reply.rp_acpt.ar_stat) {
	case SUCCESS:
		pdu->cb(rpc, RPC_STATUS_SUCCESS, pdu->xdr_decode_buf, pdu->private_data);
		break;
	case PROG_UNAVAIL:
		pdu->cb(rpc, RPC_STATUS_ERROR, "Server responded: Program not available", pdu->private_data);
		break;
	case PROG_MISMATCH:
		pdu->cb(rpc, RPC_STATUS_ERROR, "Server responded: Program version mismatch", pdu->private_data);
		break;
	case PROC_UNAVAIL:
		pdu->cb(rpc, RPC_STATUS_ERROR, "Server responded: Procedure not available", pdu->private_data);
		break;
	case GARBAGE_ARGS:
		pdu->cb(rpc, RPC_STATUS_ERROR, "Server responded: Garbage arguments", pdu->private_data);
		break;
	case SYSTEM_ERR:
		pdu->cb(rpc, RPC_STATUS_ERROR, "Server responded: System Error", pdu->private_data);
		break;
	default:
		pdu->cb(rpc, RPC_STATUS_ERROR, "Unknown rpc response from server", pdu->private_data);
		break;
	}

	return 0;
}

int rpc_process_pdu(struct rpc_context *rpc, char *buf, int size)
{
	struct rpc_pdu *pdu;
	XDR xdr;
	int pos, recordmarker;
	unsigned int xid;

	bzero(&xdr, sizeof(XDR));

	xdrmem_create(&xdr, buf, size, XDR_DECODE);
	if (xdr_int(&xdr, &recordmarker) == 0) {
		printf("xdr_int reading recordmarker failed\n");
		xdr_destroy(&xdr);
		return -1;
	}
	pos = xdr_getpos(&xdr);
	if (xdr_int(&xdr, (int *)&xid) == 0) {
		printf("xdr_int reading xid failed\n");
		xdr_destroy(&xdr);
		return -1;
	}
	xdr_setpos(&xdr, pos);

	for (pdu=rpc->waitpdu; pdu; pdu=pdu->next) {
		if (pdu->xid != xid) {
			continue;
		}
		DLIST_REMOVE(rpc->waitpdu, pdu);
		if (rpc_process_reply(rpc, pdu, &xdr) != 0) {
			printf("rpc_procdess_reply failed\n");
		}
		xdr_destroy(&xdr);
		rpc_free_pdu(rpc, pdu);
		return 0;
	}
	printf("No matching pdu found for xid:%d\n", xid);
	xdr_destroy(&xdr);
	return -1;
}

