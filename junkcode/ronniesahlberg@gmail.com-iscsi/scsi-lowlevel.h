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
#ifndef CCAN_ISCSI_SCSI_LOWLEVEL_H
#define CCAN_ISCSI_SCSI_LOWLEVEL_H

#define SCSI_CDB_MAX_SIZE			16

enum scsi_opcode {SCSI_OPCODE_TESTUNITREADY=0x00,
		  SCSI_OPCODE_INQUIRY=0x12,
		  SCSI_OPCODE_MODESENSE6=0x1a,
		  SCSI_OPCODE_READCAPACITY10=0x25,
		  SCSI_OPCODE_READ10=0x28,
		  SCSI_OPCODE_WRITE10=0x2A,
		  SCSI_OPCODE_REPORTLUNS=0xA0};

/* sense keys */
#define SCSI_SENSE_KEY_ILLEGAL_REQUEST			0x05
#define SCSI_SENSE_KEY_UNIT_ATTENTION			0x06

/* ascq */
#define SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB		0x2400
#define SCSI_SENSE_ASCQ_BUS_RESET			0x2900

enum scsi_xfer_dir {SCSI_XFER_NONE=0,
     		    SCSI_XFER_READ=1,
		    SCSI_XFER_WRITE=2};

struct scsi_reportluns_params {
       int report_type;
};
struct scsi_readcapacity10_params {
       int lba;
       int pmi;
};
struct scsi_inquiry_params {
       int evpd;
       int page_code;
};
struct scsi_modesense6_params {
       int dbd;
       int pc;
       int page_code;
       int sub_page_code;
};

struct scsi_sense {
       unsigned char error_type;
       unsigned char key;
       int           ascq;
};

struct scsi_data {
       int size;
       unsigned char *data;
};

struct scsi_allocated_memory {
       struct scsi_allocated_memory *prev, *next;
       void *ptr;
};

struct scsi_task {
       int cdb_size;
       int xfer_dir;
       int expxferlen;
       unsigned char cdb[SCSI_CDB_MAX_SIZE];
       union {
       	     struct scsi_readcapacity10_params readcapacity10;
       	     struct scsi_reportluns_params     reportluns;
       	     struct scsi_inquiry_params        inquiry;
	     struct scsi_modesense6_params     modesense6;
       } params;

       struct scsi_sense sense;
       struct scsi_data datain;
       struct scsi_allocated_memory *mem;
};

void scsi_free_scsi_task(struct scsi_task *task);


/*
 * TESTUNITREADY
 */
struct scsi_task *scsi_cdb_testunitready(void);


/*
 * REPORTLUNS
 */
#define SCSI_REPORTLUNS_REPORT_ALL_LUNS				0x00
#define SCSI_REPORTLUNS_REPORT_WELL_KNOWN_ONLY			0x01
#define SCSI_REPORTLUNS_REPORT_AVAILABLE_LUNS_ONLY		0x02

struct scsi_reportluns_list {
       uint32_t num;
       uint16_t luns[0];
};

struct scsi_task *scsi_reportluns_cdb(int report_type, int alloc_len);

/*
 * READCAPACITY10
 */
struct scsi_readcapacity10 {
       uint32_t lba;
       uint32_t block_size;
};
struct scsi_task *scsi_cdb_readcapacity10(int lba, int pmi);


/*
 * INQUIRY
 */
enum scsi_inquiry_peripheral_qualifier {SCSI_INQUIRY_PERIPHERAL_QUALIFIER_CONNECTED=0x00,
     				        SCSI_INQUIRY_PERIPHERAL_QUALIFIER_DISCONNECTED=0x01,
     				        SCSI_INQUIRY_PERIPHERAL_QUALIFIER_NOT_SUPPORTED=0x03};

enum scsi_inquiry_peripheral_device_type {SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS=0x00,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_SEQUENTIAL_ACCESS=0x01,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_PRINTER=0x02,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_PROCESSOR=0x03,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_WRITE_ONCE=0x04,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_MMC=0x05,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_SCANNER=0x06,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_OPTICAL_MEMORY=0x07,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_MEDIA_CHANGER=0x08,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_COMMUNICATIONS=0x09,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_STORAGE_ARRAY_CONTROLLER=0x0c,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_ENCLOSURE_SERVICES=0x0d,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_SIMPLIFIED_DIRECT_ACCESS=0x0e,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_OPTICAL_CARD_READER=0x0f,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_BRIDGE_CONTROLLER=0x10,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_OSD=0x11,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_AUTOMATION=0x12,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_SEQURITY_MANAGER=0x13,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_WELL_KNOWN_LUN=0x1e,
     					  SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_UNKNOWN=0x1f};

struct scsi_inquiry_standard {
       enum scsi_inquiry_peripheral_qualifier periperal_qualifier;
       enum scsi_inquiry_peripheral_device_type periperal_device_type;
       int rmb;
       int version;
       int normaca;
       int hisup;
       int response_data_format;

       char vendor_identification[8+1];
       char product_identification[16+1];
       char product_revision_level[4+1];
};

struct scsi_task *scsi_cdb_inquiry(int evpd, int page_code, int alloc_len);



/*
 * MODESENSE6
 */
enum scsi_modesense_page_control {SCSI_MODESENSE_PC_CURRENT=0x00,
     				  SCSI_MODESENSE_PC_CHANGEABLE=0x01,
				  SCSI_MODESENSE_PC_DEFAULT=0x02,
				  SCSI_MODESENSE_PC_SAVED=0x03};

enum scsi_modesense_page_code {SCSI_MODESENSE_PAGECODE_RETURN_ALL_PAGES=0x3f};

struct scsi_task *scsi_cdb_modesense6(int dbd, enum scsi_modesense_page_control pc, enum scsi_modesense_page_code page_code, int sub_page_code, unsigned char alloc_len);




int scsi_datain_getfullsize(struct scsi_task *task);
void *scsi_datain_unmarshall(struct scsi_task *task);

struct scsi_task *scsi_cdb_read10(int lba, int xferlen, int blocksize);
struct scsi_task *scsi_cdb_write10(int lba, int xferlen, int fua, int fuanv, int blocksize);

#endif /* CCAN_ISCSI_SCSI_LOWLEVEL_H */
