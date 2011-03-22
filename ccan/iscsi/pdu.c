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
#include <strings.h>
#include <string.h>
#include <arpa/inet.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "dlinklist.h"

struct iscsi_pdu *iscsi_allocate_pdu(struct iscsi_context *iscsi, enum iscsi_opcode opcode, enum iscsi_opcode response_opcode)
{
	struct iscsi_pdu *pdu;

	pdu = malloc(sizeof(struct iscsi_pdu));
	if (pdu == NULL) {
		printf("failed to allocate pdu\n");
		return NULL;
	}
	bzero(pdu, sizeof(struct iscsi_pdu));

	pdu->outdata.size = ISCSI_HEADER_SIZE;
	pdu->outdata.data = malloc(pdu->outdata.size);

	if (pdu->outdata.data == NULL) {
		printf("failed to allocate pdu header\n");
		free(pdu);
		pdu = NULL;
		return NULL;
	}
	bzero(pdu->outdata.data, pdu->outdata.size);

	/* opcode */
	pdu->outdata.data[0] = opcode;
	pdu->response_opcode = response_opcode;

	/* isid */
	if (opcode ==ISCSI_PDU_LOGIN_REQUEST) {
		memcpy(&pdu->outdata.data[8], &iscsi->isid[0], 6);
	}

	/* itt */
	*(uint32_t *)&pdu->outdata.data[16] = htonl(iscsi->itt);
	pdu->itt = iscsi->itt;

	iscsi->itt++;

	return pdu;
}

void iscsi_free_pdu(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	if (pdu == NULL) {
		printf("trying to free NULL pdu\n");
		return;
	}
	if (pdu->outdata.data) {
		free(pdu->outdata.data);
		pdu->outdata.data = NULL;
	}
	if (pdu->indata.data) {
		free(pdu->indata.data);
		pdu->indata.data = NULL;
	}
	if (pdu->scsi_cbdata) {
		iscsi_free_scsi_cbdata(pdu->scsi_cbdata);
		pdu->scsi_cbdata = NULL;
	}

	free(pdu);
}


int iscsi_add_data(struct iscsi_data *data, const unsigned char *dptr, int dsize, int pdualignment)
{
	int len, aligned;
	unsigned char *buf;

	if (dsize == 0) {
		printf("Trying to append zero size data to iscsi_data\n");
		return -1;
	}

	len = data->size + dsize;
	aligned = len;
	if (pdualignment) {
		aligned = (aligned+3)&0xfffffffc;
	}
	buf = malloc(aligned);
	if (buf == NULL) {
		printf("failed to allocate buffer for %d bytes\n", len);
		return -2;
	}

	memcpy(buf, data->data, data->size);
	memcpy(buf + data->size, dptr, dsize);
	if (len != aligned) {
		/* zero out any padding at the end */
	       bzero(buf+len, aligned-len);
	}

	free(data->data);
	data->data  = buf;
	data->size = len;

	return 0;
}

int iscsi_pdu_add_data(struct iscsi_context *iscsi, struct iscsi_pdu *pdu, const unsigned char *dptr, int dsize)
{
	if (pdu == NULL) {
		printf("trying to add data to NULL pdu\n");
		return -1;
	}
	if (dsize == 0) {
		printf("Trying to append zero size data to pdu\n");
		return -2;
	}

	if (iscsi_add_data(&pdu->outdata, dptr, dsize, 1) != 0) {
		printf("failed to add data to pdu buffer\n");
		return -3;
	}

	/* update data segment length */
	*(uint32_t *)&pdu->outdata.data[4] = htonl(pdu->outdata.size-ISCSI_HEADER_SIZE);

	return 0;
}

int iscsi_get_pdu_size(const unsigned char *hdr)
{
	int size;

	size = (ntohl(*(uint32_t *)&hdr[4])&0x00ffffff) + ISCSI_HEADER_SIZE;
	size = (size+3)&0xfffffffc;

	return size;
}


int iscsi_process_pdu(struct iscsi_context *iscsi, const unsigned char *hdr, int size)
{
	uint32_t itt;
	enum iscsi_opcode opcode;
	struct iscsi_pdu *pdu;
	uint8_t	ahslen;

	opcode = hdr[0] & 0x3f;
	ahslen = hdr[4];
	itt = ntohl(*(uint32_t *)&hdr[16]);

	if (ahslen != 0) {
		printf("cant handle expanded headers yet\n");
		return -1;
	}

	for (pdu = iscsi->waitpdu; pdu; pdu = pdu->next) {
		enum iscsi_opcode expected_response = pdu->response_opcode;
		int is_finished = 1;

		if (pdu->itt != itt) {
			continue;
		}

		/* we have a special case with scsi-command opcodes, the are replied to by either a scsi-response
		 * or a data-in, or a combination of both.
		 */
		if (opcode == ISCSI_PDU_DATA_IN && expected_response == ISCSI_PDU_SCSI_RESPONSE) {
			expected_response = ISCSI_PDU_DATA_IN;
		}
				
		if (opcode != expected_response) {
			printf("Got wrong opcode back for itt:%d  got:%d expected %d\n", itt, opcode, pdu->response_opcode);
			return -1;
		}
		switch (opcode) {
		case ISCSI_PDU_LOGIN_RESPONSE:
			if (iscsi_process_login_reply(iscsi, pdu, hdr, size) != 0) {
				DLIST_REMOVE(iscsi->waitpdu, pdu);
				iscsi_free_pdu(iscsi, pdu);
				printf("iscsi login reply failed\n");
				return -2;
			}
			break;
		case ISCSI_PDU_TEXT_RESPONSE:
			if (iscsi_process_text_reply(iscsi, pdu, hdr, size) != 0) {
				DLIST_REMOVE(iscsi->waitpdu, pdu);
				iscsi_free_pdu(iscsi, pdu);
				printf("iscsi text reply failed\n");
				return -2;
			}
			break;
		case ISCSI_PDU_LOGOUT_RESPONSE:
			if (iscsi_process_logout_reply(iscsi, pdu, hdr, size) != 0) {
				DLIST_REMOVE(iscsi->waitpdu, pdu);
				iscsi_free_pdu(iscsi, pdu);
				printf("iscsi logout reply failed\n");
				return -3;
			}
			break;
		case ISCSI_PDU_SCSI_RESPONSE:
			if (iscsi_process_scsi_reply(iscsi, pdu, hdr, size) != 0) {
				DLIST_REMOVE(iscsi->waitpdu, pdu);
				iscsi_free_pdu(iscsi, pdu);
				printf("iscsi response reply failed\n");
				return -4;
			}
			break;
		case ISCSI_PDU_DATA_IN:
			if (iscsi_process_scsi_data_in(iscsi, pdu, hdr, size, &is_finished) != 0) {
				DLIST_REMOVE(iscsi->waitpdu, pdu);
				iscsi_free_pdu(iscsi, pdu);
				printf("iscsi data in failed\n");
				return -4;
			}
			break;
		case ISCSI_PDU_NOP_IN:
			if (iscsi_process_nop_out_reply(iscsi, pdu, hdr, size) != 0) {
				DLIST_REMOVE(iscsi->waitpdu, pdu);
				iscsi_free_pdu(iscsi, pdu);
				printf("iscsi nop-in failed\n");
				return -5;
			}
			break;
		default:
			printf("Don't know how to handle opcode %d\n", opcode);
			return -2;
		}

		if (is_finished) {
			DLIST_REMOVE(iscsi->waitpdu, pdu);
			iscsi_free_pdu(iscsi, pdu);
		} else {
			printf("pdu is not yet finished, let it remain\n");
		}
		return 0;
	}

	return 0;
}

void iscsi_pdu_set_pduflags(struct iscsi_pdu *pdu, unsigned char flags)
{
	pdu->outdata.data[1] = flags;
}

void iscsi_pdu_set_immediate(struct iscsi_pdu *pdu)
{
	pdu->outdata.data[0] |= ISCSI_PDU_IMMEDIATE;
}

void iscsi_pdu_set_ttt(struct iscsi_pdu *pdu, uint32_t ttt)
{
	*(uint32_t *)&pdu->outdata.data[20] = htonl(ttt);
}

void iscsi_pdu_set_cmdsn(struct iscsi_pdu *pdu, uint32_t cmdsn)
{
	*(uint32_t *)&pdu->outdata.data[24] = htonl(cmdsn);
}

void iscsi_pdu_set_expstatsn(struct iscsi_pdu *pdu, uint32_t expstatsnsn)
{
	*(uint32_t *)&pdu->outdata.data[28] = htonl(expstatsnsn);
}

void iscsi_pdu_set_cdb(struct iscsi_pdu *pdu, struct scsi_task *task)
{
	bzero(&pdu->outdata.data[32], 16);
	memcpy(&pdu->outdata.data[32], task->cdb, task->cdb_size);
}

void iscsi_pdu_set_lun(struct iscsi_pdu *pdu, uint32_t lun)
{
	pdu->outdata.data[9] = lun;
}

void iscsi_pdu_set_expxferlen(struct iscsi_pdu *pdu, uint32_t expxferlen)
{
	*(uint32_t *)&pdu->outdata.data[20] = htonl(expxferlen);
}
