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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "iscsi.h"
#include "iscsi-private.h"

int iscsi_discovery_async(struct iscsi_context *iscsi, iscsi_command_cb cb, void *private_data)
{
	struct iscsi_pdu *pdu;
	const char *str;

	if (iscsi == NULL) {
		printf("trying to send text on NULL context\n");
		return -1;
	}

	if (iscsi->session_type != ISCSI_SESSION_DISCOVERY) {
		printf("Trying to do discovery on non-discovery session\n");
		return -2;
	}

	pdu = iscsi_allocate_pdu(iscsi, ISCSI_PDU_TEXT_REQUEST, ISCSI_PDU_TEXT_RESPONSE);
	if (pdu == NULL) {
		printf("Failed to allocate text pdu\n");
		return -3;
	}

	/* immediate */
	iscsi_pdu_set_immediate(pdu);

	/* flags */
	iscsi_pdu_set_pduflags(pdu, ISCSI_PDU_TEXT_FINAL);

	/* target transfer tag */
	iscsi_pdu_set_ttt(pdu, 0xffffffff);

	/* sendtargets */
	str = "SendTargets=All";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1) != 0) {
		printf("pdu add data failed\n");
		iscsi_free_pdu(iscsi, pdu);
		return -4;
	}

	pdu->callback     = cb;
	pdu->private_data = private_data;

	if (iscsi_queue_pdu(iscsi, pdu) != 0) {
		printf("failed to queue iscsi text pdu\n");
		iscsi_free_pdu(iscsi, pdu);
		return -5;
	}

	return 0;
}

static void iscsi_free_discovery_addresses(struct iscsi_discovery_address *addresses)
{
	while (addresses != NULL) {
		struct iscsi_discovery_address *next = addresses->next;

		if (addresses->target_name != NULL) {
			free(discard_const(addresses->target_name));
			addresses->target_name = NULL;
		}
		if (addresses->target_address != NULL) {
			free(discard_const(addresses->target_address));
			addresses->target_address = NULL;
		}
		addresses->next = NULL;
		free(addresses);
		addresses = next;
	}
}

int iscsi_process_text_reply(struct iscsi_context *iscsi, struct iscsi_pdu *pdu, const unsigned char *hdr, int size)
{
	struct iscsi_discovery_address *targets = NULL;

	/* verify the response looks sane */
	if (hdr[1] != ISCSI_PDU_TEXT_FINAL) {
		printf("unsupported flags in text reply %02x\n", hdr[1]);
		pdu->callback(iscsi, ISCSI_STATUS_ERROR, NULL, pdu->private_data);
		return -1;
	}

	/* skip past the header */
	hdr  += ISCSI_HEADER_SIZE;
	size -= ISCSI_HEADER_SIZE;

	while (size > 0) {
		int len;

		len = strlen((char *)hdr);

		if (len == 0) {
			break;
		}

		if (len > size) {
			printf("len > size when parsing discovery data %d>%d\n", len, size);
			pdu->callback(iscsi, ISCSI_STATUS_ERROR, NULL, pdu->private_data);
			iscsi_free_discovery_addresses(targets);
			return -1;
		}

		/* parse the strings */
		if (!strncmp((char *)hdr, "TargetName=", 11)) {
			struct iscsi_discovery_address *target;

			target = malloc(sizeof(struct iscsi_discovery_address));
			if (target == NULL) {
				printf("Failed to allocate data for new discovered target\n");
				pdu->callback(iscsi, ISCSI_STATUS_ERROR, NULL, pdu->private_data);
				iscsi_free_discovery_addresses(targets);
				return -1;
			}
			bzero(target, sizeof(struct iscsi_discovery_address));
			target->target_name = strdup((char *)hdr+11);
			if (target->target_name == NULL) {
				printf("Failed to allocate data for new discovered target name\n");
				pdu->callback(iscsi, ISCSI_STATUS_ERROR, NULL, pdu->private_data);
				free(target);
				target = NULL;
				iscsi_free_discovery_addresses(targets);
				return -1;
			}
			target->next = targets;
			targets = target;
		} else if (!strncmp((char *)hdr, "TargetAddress=", 14)) {
			targets->target_address = strdup((char *)hdr+14);
			if (targets->target_address == NULL) {
				printf("Failed to allocate data for new discovered target address\n");
				pdu->callback(iscsi, ISCSI_STATUS_ERROR, NULL, pdu->private_data);
				iscsi_free_discovery_addresses(targets);
				return -1;
			}
		} else {
			printf("Don't know how to handle discovery string : %s\n", hdr);
			pdu->callback(iscsi, ISCSI_STATUS_ERROR, NULL, pdu->private_data);
			iscsi_free_discovery_addresses(targets);
			return -1;
		}

		hdr  += len + 1;
		size -= len + 1;
	}

	pdu->callback(iscsi, ISCSI_STATUS_GOOD, targets, pdu->private_data);
	iscsi_free_discovery_addresses(targets);

	return 0;
}
