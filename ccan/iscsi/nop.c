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
#include "iscsi.h"
#include "iscsi-private.h"

int iscsi_nop_out_async(struct iscsi_context *iscsi, iscsi_command_cb cb, unsigned char *data, int len, void *private_data)
{
	struct iscsi_pdu *pdu;

	if (iscsi == NULL) {
		printf("trying to send nop-out on NULL context\n");
		return -1;
	}

	if (iscsi->is_loggedin == 0) {
		printf("trying send nop-out while not logged in\n");
		return -2;
	}

	pdu = iscsi_allocate_pdu(iscsi, ISCSI_PDU_NOP_OUT, ISCSI_PDU_NOP_IN);
	if (pdu == NULL) {
		printf("Failed to allocate nop-out pdu\n");
		return -3;
	}

	/* immediate flag */
	iscsi_pdu_set_immediate(pdu);

	/* flags */
	iscsi_pdu_set_pduflags(pdu, 0x80);

	/* ttt */
	iscsi_pdu_set_ttt(pdu, 0xffffffff);

	/* lun */
	iscsi_pdu_set_lun(pdu, 2);

	/* cmdsn is not increased if Immediate delivery*/
	iscsi_pdu_set_cmdsn(pdu, iscsi->cmdsn);
	pdu->cmdsn = iscsi->cmdsn;
//	iscsi->cmdsn++;

	pdu->callback     = cb;
	pdu->private_data = private_data;

	if (iscsi_pdu_add_data(iscsi, pdu, data, len) != 0) {
		printf("Failed to add outdata to nop-out\n");
		iscsi_free_pdu(iscsi, pdu);
		return -4;
	}


	if (iscsi_queue_pdu(iscsi, pdu) != 0) {
		printf("failed to queue iscsi nop-out pdu\n");
		iscsi_free_pdu(iscsi, pdu);
		return -5;
	}

	return 0;
}

int iscsi_process_nop_out_reply(struct iscsi_context *iscsi, struct iscsi_pdu *pdu, const unsigned char *hdr, int size)
{
	struct iscsi_data data;

	data.data = NULL;
	data.size = 0;

	if (size > ISCSI_HEADER_SIZE) {
		data.data = discard_const(&hdr[ISCSI_HEADER_SIZE]);
		data.size = size - ISCSI_HEADER_SIZE;
	}
	pdu->callback(iscsi, ISCSI_STATUS_GOOD, &data, pdu->private_data);

	return 0;
}
