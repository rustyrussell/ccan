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
#include <ccan/compiler/compiler.h>
#include "iscsi.h"
#include "iscsi-private.h"

int iscsi_login_async(struct iscsi_context *iscsi, iscsi_command_cb cb, void *private_data)
{
	struct iscsi_pdu *pdu;
	const char *str;
	char *astr;
	int ret;

	if (iscsi == NULL) {
		printf("trying to login on NULL context\n");
		return -1;
	}

	if (iscsi->is_loggedin != 0) {
		printf("trying to login while already logged in\n");
		return -2;
	}

	switch (iscsi->session_type) {
	case ISCSI_SESSION_DISCOVERY:
	case ISCSI_SESSION_NORMAL:
		break;
	default:
		printf("trying to login without setting session type\n");
		return -3;
	}

	pdu = iscsi_allocate_pdu(iscsi, ISCSI_PDU_LOGIN_REQUEST, ISCSI_PDU_LOGIN_RESPONSE);
	if (pdu == NULL) {
		printf("Failed to allocate login pdu\n");
		return -4;
	}

	/* login request */
	iscsi_pdu_set_immediate(pdu);

	/* flags */
	iscsi_pdu_set_pduflags(pdu, ISCSI_PDU_LOGIN_TRANSIT|ISCSI_PDU_LOGIN_CSG_OPNEG|ISCSI_PDU_LOGIN_NSG_FF);


	/* initiator name */
	if (asprintf(&astr, "InitiatorName=%s", iscsi->initiator_name) == -1) {
		printf("asprintf failed\n");
		iscsi_free_pdu(iscsi, pdu);
		return -5;
	}
	ret = iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)astr, strlen(astr)+1);
	free(astr);
	if (ret != 0) {
		printf("pdu add data failed\n");
		iscsi_free_pdu(iscsi, pdu);
		return -6;
	}

	/* optional alias */
	if (iscsi->alias) {
		if (asprintf(&astr, "InitiatorAlias=%s", iscsi->alias) == -1) {
			printf("asprintf failed\n");
			iscsi_free_pdu(iscsi, pdu);
			return -7;
		}
		ret = iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)astr, strlen(astr)+1);
		free(astr);
		if (ret != 0) {
			printf("pdu add data failed\n");
			iscsi_free_pdu(iscsi, pdu);
			return -8;
		}
	}

	/* target name */
	if (iscsi->session_type == ISCSI_SESSION_NORMAL) {
		if (iscsi->target_name == NULL) {
			printf("trying normal connect but target name not set\n");
			iscsi_free_pdu(iscsi, pdu);
			return -9;
		}

		if (asprintf(&astr, "TargetName=%s", iscsi->target_name) == -1) {
			printf("asprintf failed\n");
			iscsi_free_pdu(iscsi, pdu);
			return -10;
		}
		ret = iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)astr, strlen(astr)+1);
		free(astr);
		if (ret != 0) {
			printf("pdu add data failed\n");
			iscsi_free_pdu(iscsi, pdu);
			return -11;
		}
	}

	/* session type */
	switch (iscsi->session_type) {
	case ISCSI_SESSION_DISCOVERY:
		str = "SessionType=Discovery";
		break;
	case ISCSI_SESSION_NORMAL:
		str = "SessionType=Normal";
		break;
	default:
		printf("can not handle sessions %d yet\n", iscsi->session_type);
		return -12;
	}
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1) != 0) {
		printf("pdu add data failed\n");
		iscsi_free_pdu(iscsi, pdu);
		return -13;
	}

	str = "HeaderDigest=None";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1) != 0) {
		printf("pdu add data failed\n");
		iscsi_free_pdu(iscsi, pdu);
		return -14;
	}
	str = "DataDigest=None";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1) != 0) {
		printf("pdu add data failed\n");
		iscsi_free_pdu(iscsi, pdu);
		return -15;
	}
	str = "InitialR2T=Yes";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1) != 0) {
		printf("pdu add data failed\n");
		iscsi_free_pdu(iscsi, pdu);
		return -16;
	}
	str = "ImmediateData=Yes";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1) != 0) {
		printf("pdu add data failed\n");
		iscsi_free_pdu(iscsi, pdu);
		return -17;
	}
	str = "MaxBurstLength=262144";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1) != 0) {
		printf("pdu add data failed\n");
		iscsi_free_pdu(iscsi, pdu);
		return -18;
	}
	str = "FirstBurstLength=262144";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1) != 0) {
		printf("pdu add data failed\n");
		iscsi_free_pdu(iscsi, pdu);
		return -19;
	}
	str = "MaxRecvDataSegmentLength=262144";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1) != 0) {
		printf("pdu add data failed\n");
		iscsi_free_pdu(iscsi, pdu);
		return -20;
	}
	str = "DataPDUInOrder=Yes";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1) != 0) {
		printf("pdu add data failed\n");
		iscsi_free_pdu(iscsi, pdu);
		return -21;
	}
	str = "DataSequenceInOrder=Yes";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1) != 0) {
		printf("pdu add data failed\n");
		iscsi_free_pdu(iscsi, pdu);
		return -22;
	}


	pdu->callback     = cb;
	pdu->private_data = private_data;

	if (iscsi_queue_pdu(iscsi, pdu) != 0) {
		printf("failed to queue iscsi login pdu\n");
		iscsi_free_pdu(iscsi, pdu);
		return -23;
	}

	return 0;
}

int iscsi_process_login_reply(struct iscsi_context *iscsi, struct iscsi_pdu *pdu, const unsigned char *hdr, int size)
{
	int status;

	if (size < ISCSI_HEADER_SIZE) {
		printf("don't have enough data to read status from login reply\n");
		return -1;
	}

	/* XXX here we should parse the data returned in case the target renegotiated some
	 *     some parameters.
	 *     we should also do proper handshaking if the target is not yet prepared to transition
	 *     to the next stage
	 */
	status = ntohs(*(uint16_t *)&hdr[36]);
	if (status != 0) {
		pdu->callback(iscsi, ISCSI_STATUS_ERROR, NULL, pdu->private_data);
		return 0;
	}

	iscsi->statsn = ntohs(*(uint16_t *)&hdr[24]);

	iscsi->is_loggedin = 1;
	pdu->callback(iscsi, ISCSI_STATUS_GOOD, NULL, pdu->private_data);

	return 0;
}


int iscsi_logout_async(struct iscsi_context *iscsi, iscsi_command_cb cb, void *private_data)
{
	struct iscsi_pdu *pdu;

	if (iscsi == NULL) {
		printf("trying to logout on NULL context\n");
		return -1;
	}

	if (iscsi->is_loggedin == 0) {
		printf("trying to logout while not logged in\n");
		return -2;
	}

	pdu = iscsi_allocate_pdu(iscsi, ISCSI_PDU_LOGOUT_REQUEST, ISCSI_PDU_LOGOUT_RESPONSE);
	if (pdu == NULL) {
		printf("Failed to allocate logout pdu\n");
		return -3;
	}

	/* logout request has the immediate flag set */
	iscsi_pdu_set_immediate(pdu);

	/* flags : close the session */
	iscsi_pdu_set_pduflags(pdu, 0x80);


	pdu->callback     = cb;
	pdu->private_data = private_data;

	if (iscsi_queue_pdu(iscsi, pdu) != 0) {
		printf("failed to queue iscsi logout pdu\n");
		iscsi_free_pdu(iscsi, pdu);
		return -4;
	}

	return 0;
}

int iscsi_process_logout_reply(struct iscsi_context *iscsi, struct iscsi_pdu *pdu, const unsigned char *hdr, int size UNUSED)
{
	iscsi->is_loggedin = 0;
	pdu->callback(iscsi, ISCSI_STATUS_GOOD, NULL, pdu->private_data);

	return 0;
}

int iscsi_set_session_type(struct iscsi_context *iscsi, enum iscsi_session_type session_type)
{
	if (iscsi == NULL) {
		printf("Trying to set sesssion type on NULL context\n");
		return -1;
	}
	if (iscsi->is_loggedin) {
		printf("trying to set session type while logged in\n");
		return -2;
	}
	
	iscsi->session_type = session_type;

	return 0;
}
