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
/*
 * would be nice if this could grow into a full blown library for scsi to
 * 1, build a CDB
 * 2, check how big a complete data-in structure needs to be
 * 3, unmarshall data-in into a real structure
 * 4, marshall a real structure into a data-out blob
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <ccan/compiler/compiler.h>
#include "scsi-lowlevel.h"
#include "dlinklist.h"


void scsi_free_scsi_task(struct scsi_task *task)
{
	struct scsi_allocated_memory *mem;

	while((mem = task->mem)) {
		   DLIST_REMOVE(task->mem, mem);
		   free(mem);
	}
	free(task);
}

static void *scsi_malloc(struct scsi_task *task, size_t size)
{
	struct scsi_allocated_memory *mem;

	mem = malloc(sizeof(struct scsi_allocated_memory));
	if (mem == NULL) {
		printf("Failed to allocate memory to scsi task\n");
		return NULL;
	}
	bzero(mem, sizeof(struct scsi_allocated_memory));
	mem->ptr = malloc(size);
	if (mem->ptr == NULL) {
		printf("Failed to allocate memory buffer for scsi task\n");
		free(mem);
		return NULL;
	}
	bzero(mem->ptr, size);
	DLIST_ADD(task->mem, mem);
	return mem->ptr;
}

/*
 * TESTUNITREADY
 */
struct scsi_task *scsi_cdb_testunitready(void)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		printf("Failed to allocate scsi task structure\n");
		return NULL;
	}

	bzero(task, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_TESTUNITREADY;

	task->cdb_size   = 6;
	task->xfer_dir   = SCSI_XFER_NONE;
	task->expxferlen = 0;

	return task;
}


/*
 * REPORTLUNS
 */
struct scsi_task *scsi_reportluns_cdb(int report_type, int alloc_len)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		printf("Failed to allocate scsi task structure\n");
		return NULL;
	}

	bzero(task, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_REPORTLUNS;
	task->cdb[2]   = report_type;
	*(uint32_t *)&task->cdb[6] = htonl(alloc_len);

	task->cdb_size = 12;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = alloc_len;

	task->params.reportluns.report_type = report_type;

	return task;
}

/*
 * parse the data in blob and calcualte the size of a full report luns datain structure
 */
static int scsi_reportluns_datain_getfullsize(struct scsi_task *task)
{
	uint32_t list_size;

	list_size = htonl(*(uint32_t *)&(task->datain.data[0])) + 8;
	
	return list_size;
}

/*
 * unmarshall the data in blob for reportluns into a structure
 */
static struct scsi_reportluns_list *scsi_reportluns_datain_unmarshall(struct scsi_task *task)
{
	struct scsi_reportluns_list *list;
	int list_size;
	int i, num_luns;

	if (task->datain.size < 4) {
		printf("not enough data for reportluns list length\n");
		return NULL;
	}

	list_size = htonl(*(uint32_t *)&(task->datain.data[0])) + 8;
	if (list_size < task->datain.size) {
		printf("not enough data to unmarshall reportluns data\n");
		return NULL;
	}

	num_luns = list_size / 8 - 1;
	list = scsi_malloc(task, offsetof(struct scsi_reportluns_list, luns) + sizeof(uint16_t) * num_luns);
	if (list == NULL) {
		printf("Failed to allocate reportluns structure\n");
		return NULL;
	}

	list->num = num_luns;
	for (i=0; i<num_luns; i++) {
		list->luns[i] = htons(*(uint16_t *)&(task->datain.data[i*8+8]));
	}
	
	return list;
}


/*
 * READCAPACITY10
 */
struct scsi_task *scsi_cdb_readcapacity10(int lba, int pmi)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		printf("Failed to allocate scsi task structure\n");
		return NULL;
	}

	bzero(task, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_READCAPACITY10;

	*(uint32_t *)&task->cdb[2] = htonl(lba);

	if (pmi) {
		task->cdb[8] |= 0x01;
	}

	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = 8;

	task->params.readcapacity10.lba = lba;
	task->params.readcapacity10.pmi = pmi;

	return task;
}

/*
 * parse the data in blob and calcualte the size of a full readcapacity10 datain structure
 */
static int scsi_readcapacity10_datain_getfullsize(struct scsi_task *task
						  UNUSED)
{
	return 8;
}

/*
 * unmarshall the data in blob for readcapacity10 into a structure
 */
static struct scsi_readcapacity10 *scsi_readcapacity10_datain_unmarshall(struct scsi_task *task)
{
	struct scsi_readcapacity10 *rc10;

	if (task->datain.size < 8) {
		printf("Not enough data to unmarshall readcapacity10\n");
		return NULL;
	}
	rc10 = malloc(sizeof(struct scsi_readcapacity10));
	if (rc10 == NULL) {
		printf("Failed to allocate readcapacity10 structure\n");
		return NULL;
	}

	rc10->lba        = htonl(*(uint32_t *)&(task->datain.data[0]));
	rc10->block_size = htonl(*(uint32_t *)&(task->datain.data[4]));

	return rc10;
}





/*
 * INQUIRY
 */
struct scsi_task *scsi_cdb_inquiry(int evpd, int page_code, int alloc_len)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		printf("Failed to allocate scsi task structure\n");
		return NULL;
	}

	bzero(task, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_INQUIRY;

	if (evpd) {
		task->cdb[1] |= 0x01;
	}

	task->cdb[2] = page_code;

	*(uint16_t *)&task->cdb[3] = htons(alloc_len);

	task->cdb_size = 6;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = alloc_len;

	task->params.inquiry.evpd      = evpd;
	task->params.inquiry.page_code = page_code;

	return task;
}

/*
 * parse the data in blob and calcualte the size of a full inquiry datain structure
 */
static int scsi_inquiry_datain_getfullsize(struct scsi_task *task)
{
	if (task->params.inquiry.evpd != 0) {
		printf("Can not handle extended inquiry yet\n");
		return -1;
	}

	/* standard inquiry*/
	return task->datain.data[4] + 3;
}

/*
 * unmarshall the data in blob for inquiry into a structure
 */
static void *scsi_inquiry_datain_unmarshall(struct scsi_task *task)
{
	struct scsi_inquiry_standard *inq;

	if (task->params.inquiry.evpd != 0) {
		printf("Can not handle extended inquiry yet\n");
		return NULL;
	}

	/* standard inquiry */
	inq = scsi_malloc(task, sizeof(struct scsi_inquiry_standard));
	if (inq == NULL) {
		printf("Failed to allocate standard inquiry structure\n");
		return NULL;
	}

       inq->periperal_qualifier    = (task->datain.data[0]>>5)&0x07;
       inq->periperal_device_type  = task->datain.data[0]&0x1f;
       inq->rmb                    = task->datain.data[1]&0x80;
       inq->version                = task->datain.data[2];
       inq->normaca                = task->datain.data[3]&0x20;
       inq->hisup                  = task->datain.data[3]&0x10;
       inq->response_data_format   = task->datain.data[3]&0x0f;

       memcpy(&inq->vendor_identification[0], &task->datain.data[8], 8);
       memcpy(&inq->product_identification[0], &task->datain.data[16], 16);
       memcpy(&inq->product_revision_level[0], &task->datain.data[32], 4);

       return inq;
}

/*
 * READ10
 */
struct scsi_task *scsi_cdb_read10(int lba, int xferlen, int blocksize)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		printf("Failed to allocate scsi task structure\n");
		return NULL;
	}

	bzero(task, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_READ10;

	*(uint32_t *)&task->cdb[2] = htonl(lba);
	*(uint16_t *)&task->cdb[7] = htons(xferlen/blocksize);

	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = xferlen;

	return task;
}

/*
 * WRITE10
 */
struct scsi_task *scsi_cdb_write10(int lba, int xferlen, int fua, int fuanv, int blocksize)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		printf("Failed to allocate scsi task structure\n");
		return NULL;
	}

	bzero(task, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_WRITE10;

	if (fua) {
		task->cdb[1] |= 0x08;
	}
	if (fuanv) {
		task->cdb[1] |= 0x02;
	}

	*(uint32_t *)&task->cdb[2] = htonl(lba);
	*(uint16_t *)&task->cdb[7] = htons(xferlen/blocksize);

	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_WRITE;
	task->expxferlen = xferlen;

	return task;
}



/*
 * MODESENSE6
 */
struct scsi_task *scsi_cdb_modesense6(int dbd, enum scsi_modesense_page_control pc, enum scsi_modesense_page_code page_code, int sub_page_code, unsigned char alloc_len)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		printf("Failed to allocate scsi task structure\n");
		return NULL;
	}

	bzero(task, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_MODESENSE6;

	if (dbd) {
		task->cdb[1] |= 0x08;
	}
	task->cdb[2] = pc<<6 | page_code;
	task->cdb[3] = sub_page_code;
	task->cdb[4] = alloc_len;

	task->cdb_size = 6;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = alloc_len;

	task->params.modesense6.dbd           = dbd;
	task->params.modesense6.pc            = pc;
	task->params.modesense6.page_code     = page_code;
	task->params.modesense6.sub_page_code = sub_page_code;
 
	return task;
}

/*
 * parse the data in blob and calcualte the size of a full report luns datain structure
 */
static int scsi_modesense6_datain_getfullsize(struct scsi_task *task)
{
	int len;

	len = task->datain.data[0] + 1;

	return len;
}



int scsi_datain_getfullsize(struct scsi_task *task)
{
	switch (task->cdb[0]) {
	case SCSI_OPCODE_TESTUNITREADY:
		return 0;
	case SCSI_OPCODE_INQUIRY:
		return scsi_inquiry_datain_getfullsize(task);
	case SCSI_OPCODE_MODESENSE6:
		return scsi_modesense6_datain_getfullsize(task);
	case SCSI_OPCODE_READCAPACITY10:
		return scsi_readcapacity10_datain_getfullsize(task);
//	case SCSI_OPCODE_READ10:
//	case SCSI_OPCODE_WRITE10:
	case SCSI_OPCODE_REPORTLUNS:
		return scsi_reportluns_datain_getfullsize(task);
	}
	printf("Unknown opcode:%d for datain get full size\n", task->cdb[0]);
	return -1;
}

void *scsi_datain_unmarshall(struct scsi_task *task)
{
	switch (task->cdb[0]) {
	case SCSI_OPCODE_TESTUNITREADY:
		return NULL;
	case SCSI_OPCODE_INQUIRY:
		return scsi_inquiry_datain_unmarshall(task);
	case SCSI_OPCODE_READCAPACITY10:
		return scsi_readcapacity10_datain_unmarshall(task);
//	case SCSI_OPCODE_READ10:
//	case SCSI_OPCODE_WRITE10:
	case SCSI_OPCODE_REPORTLUNS:
		return scsi_reportluns_datain_unmarshall(task);
	}
	printf("Unknown opcode:%d for datain unmarshall\n", task->cdb[0]);
	return NULL;
}

