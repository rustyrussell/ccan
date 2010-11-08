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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "dlinklist.h"


struct iscsi_context *iscsi_create_context(const char *initiator_name)
{
	struct iscsi_context *iscsi;

	iscsi = malloc(sizeof(struct iscsi_context));
	if (iscsi == NULL) {
		printf("Failed to allocate iscsi context\n");
		return NULL;
	}

	bzero(iscsi, sizeof(struct iscsi_context));

	iscsi->initiator_name = strdup(initiator_name);
	if (iscsi->initiator_name == NULL) {
		printf("Failed to allocate initiator name context\n");
		free(iscsi);
		return NULL;
	}

	iscsi->fd = -1;

	/* use a "random" isid */
	srandom(getpid() ^ time(NULL));
	iscsi_set_random_isid(iscsi);

	return iscsi;
}

int iscsi_set_random_isid(struct iscsi_context *iscsi)
{
	iscsi->isid[0] = 0x80;
	iscsi->isid[1] = random()&0xff;
	iscsi->isid[2] = random()&0xff;
	iscsi->isid[3] = random()&0xff;
	iscsi->isid[4] = 0;
	iscsi->isid[5] = 0;

	return 0;
}

int iscsi_set_alias(struct iscsi_context *iscsi, const char *alias)
{
	if (iscsi == NULL) {
		printf("Context is NULL when adding alias\n");
		return -1;
	}
	if (iscsi->is_loggedin != 0) {
		printf("Already logged in when adding alias\n");
		return -2;
	}

	if (iscsi->alias != NULL) {
		free(discard_const(iscsi->alias));
		iscsi->alias = NULL;
	}

	iscsi->alias = strdup(alias);
	if (iscsi->alias == NULL) {
		printf("Failed to allocate alias name\n");
		return -3;
	}

	return 0;
}

int iscsi_set_targetname(struct iscsi_context *iscsi, const char *target_name)
{
	if (iscsi == NULL) {
		printf("Context is NULL when adding targetname\n");
		return -1;
	}
	if (iscsi->is_loggedin != 0) {
		printf("Already logged in when adding targetname\n");
		return -2;
	}

	if (iscsi->target_name != NULL) {
		free(discard_const(iscsi->target_name));
		iscsi->target_name= NULL;
	}

	iscsi->target_name = strdup(target_name);
	if (iscsi->target_name == NULL) {
		printf("Failed to allocate target name\n");
		return -3;
	}

	return 0;
}

int iscsi_destroy_context(struct iscsi_context *iscsi)
{
	struct iscsi_pdu *pdu;

	if (iscsi == NULL) {
		return 0;
	}
	if (iscsi->initiator_name != NULL) {
		free(discard_const(iscsi->initiator_name));
		iscsi->initiator_name = NULL;
	}
	if (iscsi->alias != NULL) {
		free(discard_const(iscsi->alias));
		iscsi->alias = NULL;
	}
	if (iscsi->is_loggedin != 0) {
		printf("deswtroying context while logged in\n");
	}
	if (iscsi->fd != -1) {
		iscsi_disconnect(iscsi);
	}

	if (iscsi->inbuf != NULL) {
		free(iscsi->inbuf);
		iscsi->inbuf = NULL;
		iscsi->insize = 0;
		iscsi->inpos = 0;
	}

	while ((pdu = iscsi->outqueue)) {
	      	DLIST_REMOVE(iscsi->outqueue, pdu);
		pdu->callback(iscsi, ISCSI_STATUS_CANCELLED, NULL, pdu->private_data);
		iscsi_free_pdu(iscsi, pdu);
	}
	while ((pdu = iscsi->waitpdu)) {
	      	DLIST_REMOVE(iscsi->waitpdu, pdu);
		pdu->callback(iscsi, ISCSI_STATUS_CANCELLED, NULL, pdu->private_data);
		iscsi_free_pdu(iscsi, pdu);
	}

	free(iscsi);

	return 0;
}
