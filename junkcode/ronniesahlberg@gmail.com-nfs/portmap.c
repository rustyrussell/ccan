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
#include <rpc/xdr.h>
#include "nfs.h"
#include "libnfs-raw.h"
#include "libnfs-private.h"
#include "rpc/portmap.h"


int rpc_pmap_null_async(struct rpc_context *rpc, rpc_cb cb, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, PMAP_PROGRAM, PMAP_V2, PMAP_NULL, cb, private_data, (xdrproc_t)xdr_void, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for portmap/null call");
		return -1;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for portmap/null call");
		rpc_free_pdu(rpc, pdu);
		return -2;
	}

	return 0;
}

int rpc_pmap_getport_async(struct rpc_context *rpc, int program, int version, rpc_cb cb, void *private_data)
{
	struct rpc_pdu *pdu;
	struct mapping m;

	pdu = rpc_allocate_pdu(rpc, PMAP_PROGRAM, PMAP_V2, PMAP_GETPORT, cb, private_data, (xdrproc_t)xdr_int, sizeof(uint32_t));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for portmap/getport call");
		return -1;
	}

	m.prog = program;
	m.vers = version;
	m.prot = IPPROTO_TCP;
	m.port = 0;
	if (xdr_mapping(&pdu->xdr, &m) == 0) {
		rpc_set_error(rpc, "XDR error: Failed to encode data for portmap/getport call");
		rpc_free_pdu(rpc, pdu);
		return -2;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		printf("Failed to queue portmap/getport pdu\n");
		rpc_free_pdu(rpc, pdu);
		return -2;
	}

	return 0;
}
