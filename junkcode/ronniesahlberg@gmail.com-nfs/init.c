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

#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <rpc/xdr.h>
#include "dlinklist.h"
#include "nfs.h"
#include "libnfs-raw.h"
#include "libnfs-private.h"

struct rpc_context *rpc_init_context(void)
{
	struct rpc_context *rpc;

	rpc = malloc(sizeof(struct rpc_context));
	if (rpc == NULL) {
		printf("Failed to allocate rpc context\n");
		return NULL;
	}
	bzero(rpc, sizeof(struct rpc_context));

	rpc->encodebuflen = 65536;
	rpc->encodebuf = malloc(rpc->encodebuflen);
	if (rpc->encodebuf == NULL) {
		printf("Failed to allocate a buffer for rpc encoding\n");
		free(rpc);
		return NULL;
	}

	rpc->auth = authunix_create_default();
	if (rpc->auth == NULL) {
		printf("failed to create authunix\n");
		free(rpc->encodebuf);
		free(rpc);
		return NULL;
	}
	rpc->xid = 1;
	rpc->fd = -1;

	return rpc;
}


void rpc_set_auth(struct rpc_context *rpc, struct AUTH *auth)
{
	if (rpc->auth != NULL) {
		auth_destroy(rpc->auth);
	}
	rpc->auth = auth;
}


void rpc_set_error(struct rpc_context *rpc, char *error_string, ...)
{
        va_list ap;
	char *str;

	if (rpc->error_string != NULL) {
		free(rpc->error_string);
	}
        va_start(ap, error_string);
	vasprintf(&str, error_string, ap);
	rpc->error_string = str;
        va_end(ap);
}

char *rpc_get_error(struct rpc_context *rpc)
{
	return rpc->error_string;
}

void rpc_error_all_pdus(struct rpc_context *rpc, char *error)
{
	struct rpc_pdu *pdu;

	while((pdu = rpc->outqueue) != NULL) {
		pdu->cb(rpc, RPC_STATUS_ERROR, error, pdu->private_data);
		DLIST_REMOVE(rpc->outqueue, pdu);
		rpc_free_pdu(rpc, pdu);
	}
	while((pdu = rpc->waitpdu) != NULL) {
		pdu->cb(rpc, RPC_STATUS_ERROR, error, pdu->private_data);
		DLIST_REMOVE(rpc->waitpdu, pdu);
		rpc_free_pdu(rpc, pdu);
	}
}


void rpc_destroy_context(struct rpc_context *rpc)
{
	struct rpc_pdu *pdu;

	while((pdu = rpc->outqueue) != NULL) {
		pdu->cb(rpc, RPC_STATUS_CANCEL, NULL, pdu->private_data);
		DLIST_REMOVE(rpc->outqueue, pdu);
		rpc_free_pdu(rpc, pdu);
	}
	while((pdu = rpc->waitpdu) != NULL) {
		pdu->cb(rpc, RPC_STATUS_CANCEL, NULL, pdu->private_data);
		DLIST_REMOVE(rpc->waitpdu, pdu);
		rpc_free_pdu(rpc, pdu);
	}

	auth_destroy(rpc->auth);
	rpc->auth =NULL;

	if (rpc->fd != -1) {
		close(rpc->fd);
	}

	if (rpc->encodebuf != NULL) {
		free(rpc->encodebuf);
		rpc->encodebuf = NULL;
	}

	if (rpc->error_string != NULL) {
		free(rpc->error_string);
		rpc->error_string = NULL;
	}

	free(rpc);
}


