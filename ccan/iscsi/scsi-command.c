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
#include <arpa/inet.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "dlinklist.h"

struct iscsi_scsi_cbdata {
       struct iscsi_scsi_cbdata *prev, *next;
       iscsi_command_cb  callback;
       void             *private_data;
       struct scsi_task *task;
};

void iscsi_free_scsi_cbdata(struct iscsi_scsi_cbdata *scsi_cbdata)
{
	if (scsi_cbdata == NULL) {
		return;
	}
	if (scsi_cbdata->task == NULL) {
		scsi_free_scsi_task(scsi_cbdata->task);
		scsi_cbdata->task = NULL;
	}
	free(scsi_cbdata);
}

static void iscsi_scsi_response_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct iscsi_scsi_cbdata *scsi_cbdata = (struct iscsi_scsi_cbdata *)private_data;
	struct scsi_task *task = command_data;

	switch (status) {
	case ISCSI_STATUS_CHECK_CONDITION:
		scsi_cbdata->callback(iscsi, ISCSI_STATUS_CHECK_CONDITION, task, scsi_cbdata->private_data);
		return;
	

	case ISCSI_STATUS_GOOD:
		scsi_cbdata->callback(iscsi, ISCSI_STATUS_GOOD, task, scsi_cbdata->private_data);
		return;
	default:
		printf("Cant handle  scsi status %d yet\n", status);
		scsi_cbdata->callback(iscsi, ISCSI_STATUS_ERROR, task, scsi_cbdata->private_data);
	}
}


static int iscsi_scsi_command_async(struct iscsi_context *iscsi, int lun, struct scsi_task *task, iscsi_command_cb cb, struct iscsi_data *data, void *private_data)
{
	struct iscsi_pdu *pdu;
	struct iscsi_scsi_cbdata *scsi_cbdata;
	int flags;

	if (iscsi == NULL) {
		printf("trying to send command on NULL context\n");
		scsi_free_scsi_task(task);
		return -1;
	}

	if (iscsi->session_type != ISCSI_SESSION_NORMAL) {
		printf("Trying to send command on discovery session\n");
		scsi_free_scsi_task(task);
		return -2;
	}

	if (iscsi->is_loggedin == 0) {
		printf("Trying to send command while not logged in\n");
		scsi_free_scsi_task(task);
		return -3;
	}

	scsi_cbdata = malloc(sizeof(struct iscsi_scsi_cbdata));
	if (scsi_cbdata == NULL) {
		printf("failed to allocate scsi cbdata\n");
		scsi_free_scsi_task(task);
		return -4;
	}
	bzero(scsi_cbdata, sizeof(struct iscsi_scsi_cbdata));
	scsi_cbdata->task         = task;
	scsi_cbdata->callback     = cb;
	scsi_cbdata->private_data = private_data;

	pdu = iscsi_allocate_pdu(iscsi, ISCSI_PDU_SCSI_REQUEST, ISCSI_PDU_SCSI_RESPONSE);
	if (pdu == NULL) {
		printf("Failed to allocate text pdu\n");
		iscsi_free_scsi_cbdata(scsi_cbdata);
		return -5;
	}
	pdu->scsi_cbdata = scsi_cbdata;

	/* flags */
	flags = ISCSI_PDU_SCSI_FINAL|ISCSI_PDU_SCSI_ATTR_SIMPLE;
	switch (task->xfer_dir) {
	case SCSI_XFER_NONE:
		break;
	case SCSI_XFER_READ:
		flags |= ISCSI_PDU_SCSI_READ;
		break;
	case SCSI_XFER_WRITE:
		flags |= ISCSI_PDU_SCSI_WRITE;
		if (data == NULL) {
			printf("DATA-OUT command but data == NULL\n");
			iscsi_free_pdu(iscsi, pdu);
			return -5;
		}
		if (data->size != task->expxferlen) {
			printf("data size:%d is not same as expected data transfer length:%d\n", data->size, task->expxferlen);
			iscsi_free_pdu(iscsi, pdu);
			return -7;
		}
		if (iscsi_pdu_add_data(iscsi, pdu, data->data, data->size) != 0) {
			printf("Failed to add outdata to the pdu\n");
			iscsi_free_pdu(iscsi, pdu);
			return -6;
		}

		break;
	}
	iscsi_pdu_set_pduflags(pdu, flags);

	/* lun */
	iscsi_pdu_set_lun(pdu, lun);

	/* expxferlen */
	iscsi_pdu_set_expxferlen(pdu, task->expxferlen);

	/* cmdsn */
	iscsi_pdu_set_cmdsn(pdu, iscsi->cmdsn);
	pdu->cmdsn = iscsi->cmdsn;
	iscsi->cmdsn++;

	/* exp statsn */
	iscsi_pdu_set_expstatsn(pdu, iscsi->statsn+1);
		
	/* cdb */
	iscsi_pdu_set_cdb(pdu, task);

	pdu->callback     = iscsi_scsi_response_cb;
	pdu->private_data = scsi_cbdata;

	if (iscsi_queue_pdu(iscsi, pdu) != 0) {
		printf("failed to queue iscsi scsi pdu\n");
		iscsi_free_pdu(iscsi, pdu);
		return -6;
	}

	return 0;
}


int iscsi_process_scsi_reply(struct iscsi_context *iscsi, struct iscsi_pdu *pdu, const unsigned char *hdr, int size)
{
	int statsn, flags, response, status;
	struct iscsi_scsi_cbdata *scsi_cbdata = pdu->scsi_cbdata;
	struct scsi_task *task = scsi_cbdata->task;

	statsn = ntohl(*(uint32_t *)&hdr[24]);
	if (statsn > (int)iscsi->statsn) {
		iscsi->statsn = statsn;
	}

	flags = hdr[1];
	if ((flags&ISCSI_PDU_DATA_FINAL) == 0) {
		printf("scsi response pdu but Final bit is not set: 0x%02x\n", flags);
		pdu->callback(iscsi, ISCSI_STATUS_ERROR, task, pdu->private_data);
		return -1;
	}
	if ((flags&ISCSI_PDU_DATA_ACK_REQUESTED) != 0) {
		printf("scsi response asked for ACK 0x%02x\n", flags);
		pdu->callback(iscsi, ISCSI_STATUS_ERROR, task, pdu->private_data);
		return -1;
	}
	/* for now, we ignore all overflow/underflow flags. We just print/log them so we can tweak
	 * libiscsi to not generate under/over flows
	 */
	if ((flags&ISCSI_PDU_DATA_BIDIR_OVERFLOW) != 0) {
		printf("scsi response contains bidir overflow 0x%02x\n", flags);
	}
	if ((flags&ISCSI_PDU_DATA_BIDIR_UNDERFLOW) != 0) {
		printf("scsi response contains bidir underflow 0x%02x\n", flags);
	}
	if ((flags&ISCSI_PDU_DATA_RESIDUAL_OVERFLOW) != 0) {
		printf("scsi response contains residual overflow 0x%02x\n", flags);
	}
	if ((flags&ISCSI_PDU_DATA_RESIDUAL_UNDERFLOW) != 0) {
		printf("scsi response contains residual underflow 0x%02x\n", flags);
	}


	response = hdr[2];
	if (response != 0x00) {
		printf("scsi reply response field:%d\n", response);
	}

	status = hdr[3];

	switch (status) {
	case ISCSI_STATUS_GOOD:
		task->datain.data = pdu->indata.data;
		task->datain.size = pdu->indata.size;

		pdu->callback(iscsi, ISCSI_STATUS_GOOD, &pdu->indata, pdu->private_data);
		break;
	case ISCSI_STATUS_CHECK_CONDITION:
		task->datain.data = discard_const(hdr + ISCSI_HEADER_SIZE);
		task->datain.size = size - ISCSI_HEADER_SIZE;

		task->sense.error_type = task->datain.data[2] & 0x7f;
		task->sense.key        = task->datain.data[4] & 0x0f;
		task->sense.ascq       = ntohs(*(uint16_t *)&(task->datain.data[14]));

		pdu->callback(iscsi, ISCSI_STATUS_CHECK_CONDITION, task, pdu->private_data);
		break;
	default:
		printf("Unknown status :%d\n", status);

		pdu->callback(iscsi, ISCSI_STATUS_ERROR, task, pdu->private_data);
		return -1;
	}

	return 0;
}

int iscsi_process_scsi_data_in(struct iscsi_context *iscsi, struct iscsi_pdu *pdu, const unsigned char *hdr, int size, int *is_finished)
{
	int statsn, flags, status;
	struct iscsi_scsi_cbdata *scsi_cbdata = pdu->scsi_cbdata;
	struct scsi_task *task = scsi_cbdata->task;
	int dsl;

	statsn = ntohl(*(uint32_t *)&hdr[24]);
	if (statsn > (int)iscsi->statsn) {
		iscsi->statsn = statsn;
	}

	flags = hdr[1];
	if ((flags&ISCSI_PDU_DATA_ACK_REQUESTED) != 0) {
		printf("scsi response asked for ACK 0x%02x\n", flags);
		pdu->callback(iscsi, ISCSI_STATUS_ERROR, task, pdu->private_data);
		return -1;
	}
	/* for now, we ignore all overflow/underflow flags. We just print/log them so we can tweak
	 * libiscsi to not generate under/over flows
	 */
	if ((flags&ISCSI_PDU_DATA_RESIDUAL_OVERFLOW) != 0) {
		printf("scsi response contains residual overflow 0x%02x\n", flags);
	}
	if ((flags&ISCSI_PDU_DATA_RESIDUAL_UNDERFLOW) != 0) {
		printf("scsi response contains residual underflow 0x%02x\n", flags);
	}

	dsl = ntohl(*(uint32_t *)&hdr[4])&0x00ffffff;

	if (dsl > size - ISCSI_HEADER_SIZE) {
		printf ("dsl is :%d, while buffser size if %d\n", dsl, size - ISCSI_HEADER_SIZE);
	}

	if (iscsi_add_data(&pdu->indata, discard_const(hdr + ISCSI_HEADER_SIZE), dsl, 0) != 0) {
		printf("failed to add data to pdu in buffer\n");
		return -3;
	}


	if ((flags&ISCSI_PDU_DATA_FINAL) == 0) {
		printf("scsi data-in without Final bit: 0x%02x\n", flags);
		*is_finished = 0;
	}
	if ((flags&ISCSI_PDU_DATA_CONTAINS_STATUS) == 0) {
		printf("scsi data-in without Status bit: 0x%02x\n", flags);
		*is_finished = 0;
	}

	if (*is_finished == 0) {
		return 0;
	}


	/* this was the final data-in packet in the sequence and it has the s-bit set, so invoke the
	 * callback.
	 */
	status = hdr[3];
	task->datain.data = pdu->indata.data;
	task->datain.size = pdu->indata.size;

	pdu->callback(iscsi, status, task, pdu->private_data);

	return 0;
}




/*
 * SCSI commands
 */

int iscsi_testunitready_async(struct iscsi_context *iscsi, int lun, iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	int ret;

	if ((task = scsi_cdb_testunitready()) == NULL) {
		printf("Failed to create testunitready cdb\n");
		return -1;
	}
	ret = iscsi_scsi_command_async(iscsi, lun, task, cb, NULL, private_data);

	return ret;
}


int iscsi_reportluns_async(struct iscsi_context *iscsi, iscsi_command_cb cb, int report_type, int alloc_len, void *private_data)
{
	struct scsi_task *task;
	int ret;

	if (alloc_len < 16) {
		printf("Minimum allowed alloc len for reportluns is 16. You specified %d\n", alloc_len);
		return -1;
	}

	if ((task = scsi_reportluns_cdb(report_type, alloc_len)) == NULL) {
		printf("Failed to create reportluns cdb\n");
		return -2;
	}
	/* report luns are always sent to lun 0 */
	ret = iscsi_scsi_command_async(iscsi, 0, task, cb, NULL, private_data);

	return ret;
}

int iscsi_inquiry_async(struct iscsi_context *iscsi, int lun, iscsi_command_cb cb, int evpd, int page_code, int maxsize, void *private_data)
{
	struct scsi_task *task;
	int ret;

	if ((task = scsi_cdb_inquiry(evpd, page_code, maxsize)) == NULL) {
		printf("Failed to create inquiry cdb\n");
		return -1;
	}
	ret = iscsi_scsi_command_async(iscsi, lun, task, cb, NULL, private_data);

	return ret;
}

int iscsi_readcapacity10_async(struct iscsi_context *iscsi, int lun, iscsi_command_cb cb, int lba, int pmi, void *private_data)
{
	struct scsi_task *task;
	int ret;

	if ((task = scsi_cdb_readcapacity10(lba, pmi)) == NULL) {
		printf("Failed to create readcapacity10 cdb\n");
		return -1;
	}
	ret = iscsi_scsi_command_async(iscsi, lun, task, cb, NULL, private_data);

	return ret;
}

int iscsi_read10_async(struct iscsi_context *iscsi, int lun, iscsi_command_cb cb, int lba, int datalen, int blocksize, void *private_data)
{
	struct scsi_task *task;
	int ret;

	if (datalen % blocksize != 0) {
		printf("datalen:%d is not a multiple of the blocksize:%d\n", datalen, blocksize);
		return -1;
	}

	if ((task = scsi_cdb_read10(lba, datalen, blocksize)) == NULL) {
		printf("Failed to create read10 cdb\n");
		return -2;
	}
	ret = iscsi_scsi_command_async(iscsi, lun, task, cb, NULL, private_data);

	return ret;
}


int iscsi_write10_async(struct iscsi_context *iscsi, int lun, iscsi_command_cb cb, unsigned char *data, int datalen, int lba, int fua, int fuanv, int blocksize, void *private_data)
{
	struct scsi_task *task;
	struct iscsi_data outdata;
	int ret;

	if (datalen % blocksize != 0) {
		printf("datalen:%d is not a multiple of the blocksize:%d\n", datalen, blocksize);
		return -1;
	}

	if ((task = scsi_cdb_write10(lba, datalen, fua, fuanv, blocksize)) == NULL) {
		printf("Failed to create read10 cdb\n");
		return -2;
	}

	outdata.data = data;
	outdata.size = datalen;

	ret = iscsi_scsi_command_async(iscsi, lun, task, cb, &outdata, private_data);

	return ret;
}

int iscsi_modesense6_async(struct iscsi_context *iscsi, int lun, iscsi_command_cb cb, int dbd, int pc, int page_code, int sub_page_code, unsigned char alloc_len, void *private_data)
{
	struct scsi_task *task;
	int ret;

	if ((task = scsi_cdb_modesense6(dbd, pc, page_code, sub_page_code, alloc_len)) == NULL) {
		printf("Failed to create modesense6 cdb\n");
		return -2;
	}
	ret = iscsi_scsi_command_async(iscsi, lun, task, cb, NULL, private_data);

	return ret;
}

