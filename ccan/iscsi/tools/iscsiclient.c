/* This is an example of using libiscsi.
 * It basically logs in to the the target and performs a discovery.
 * It then selects the last target in the returned list and
 * starts a normal login to that target.
 * Once logged in it issues a REPORTLUNS call and selects the last returned lun in the list.
 * This LUN is then used to send INQUIRY, READCAPACITY10 and READ10 test calls to.
 */
/* The reason why we have to specify an allocation length and sometimes probe, starting with a small value, probing how big the buffer 
 * should be, and asking again with a bigger buffer.
 * Why not just always ask with a buffer that is big enough?
 * The reason is that a lot of scsi targets are "sensitive" and ""buggy""
 * many targets will just fail the operation completely if they thing alloc len is unreasonably big.
 */

/* This is the host/port we connect to.*/
#define TARGET "10.1.1.27:3260"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <poll.h>
#include <ccan/iscsi/iscsi.h>
#include <ccan/iscsi/scsi-lowlevel.h>

struct client_state {
       char *message;
       int has_discovered_target;
       char *target_name;
       char *target_address;
       int lun;
       int block_size;
};

void nop_out_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct iscsi_data *data = command_data;

	printf("NOP-IN status:%d\n", status);
	if (data->size > 0) {
		printf("NOP-IN data:%s\n", data->data);
	}
	exit(10);
}


void write10_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct scsi_task *task = command_data;
	int i;

	if (status == ISCSI_STATUS_CHECK_CONDITION) {

		printf("Write10 failed with sense key:%d ascq:%04x\n", task->sense.key, task->sense.ascq);
		exit(10);
	}

	printf("Write successful\n");
	exit(10);
}


void read10_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct scsi_task *task = command_data;
	int i;

	if (status == ISCSI_STATUS_CHECK_CONDITION) {
		printf("Read10 failed with sense key:%d ascq:%04x\n", task->sense.key, task->sense.ascq);
		exit(10);
	}

	printf("READ10 successful. Block content:\n");
	for (i=0;i<task->datain.size;i++) {
		printf("%02x ", task->datain.data[i]);
		if (i%16==15)
			printf("\n");
		if (i==69)
			break;
	}
	printf("...\n");

	printf("Finished,   wont try to write data since that will likely destroy your LUN :-(\n");
	printf("Send NOP-OUT\n");
	if (iscsi_nop_out_async(iscsi, nop_out_cb, "Ping!", 6, private_data) != 0) {
		printf("failed to send nop-out\n");
		exit(10);
	}
//	printf("write the block back\n");
//	if (iscsi_write10_async(iscsi, clnt->lun, write10_cb, task->data.datain, task->datain.size, 0, 0, 0, clnt->block_size, private_data) != 0) {
//		printf("failed to send write10 command\n");
//		exit(10);
//	}
}

void readcapacity10_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct scsi_task *task = command_data;
	struct scsi_readcapacity10 *rc10;
	int full_size;

	if (status == ISCSI_STATUS_CHECK_CONDITION) {
		printf("Readcapacity10 failed with sense key:%d ascq:%04x\n", task->sense.key, task->sense.ascq);
		exit(10);
	}

	full_size = scsi_datain_getfullsize(task);
	if (full_size < task->datain.size) {
		printf("not enough data for full size readcapacity10\n");
		exit(10);
	}

	rc10 = scsi_datain_unmarshall(task);
	if (rc10 == NULL) {
		printf("failed to unmarshall readcapacity10 data\n");
		exit(10);
	}
	clnt->block_size = rc10->block_size;
	printf("READCAPACITY10 successful. Size:%d blocks  blocksize:%d. Read first block\n", rc10->lba, rc10->block_size);
	free(rc10);

	if (iscsi_read10_async(iscsi, clnt->lun, read10_cb, 0, clnt->block_size, clnt->block_size, private_data) != 0) {
		printf("failed to send read10 command\n");
		exit(10);
	}
}

void modesense6_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct scsi_task *task = command_data;
	int full_size;

	if (status == ISCSI_STATUS_CHECK_CONDITION) {
		printf("Modesense6 failed with sense key:%d ascq:%04x\n", task->sense.key, task->sense.ascq);
		exit(10);
	}

	full_size = scsi_datain_getfullsize(task);
	if (full_size > task->datain.size) {
		printf("did not get enough data for mode sense, sening modesense again asking for bigger buffer\n");
		if (iscsi_modesense6_async(iscsi, clnt->lun, modesense6_cb, 0, SCSI_MODESENSE_PC_CURRENT, SCSI_MODESENSE_PAGECODE_RETURN_ALL_PAGES, 0, full_size, private_data) != 0) {
			printf("failed to send modesense6 command\n");
			exit(10);
		}
		return;
	}

	printf("MODESENSE6 successful.\n");
	printf("Send READCAPACITY10\n");
	if (iscsi_readcapacity10_async(iscsi, clnt->lun, readcapacity10_cb, 0, 0, private_data) != 0) {
		printf("failed to send readcapacity command\n");
		exit(10);
	}
}

void inquiry_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct scsi_task *task = command_data;
	struct scsi_inquiry_standard *inq;

	if (status == ISCSI_STATUS_CHECK_CONDITION) {
		printf("Inquiry failed with sense key:%d ascq:%04x\n", task->sense.key, task->sense.ascq);
		exit(10);
	}

	printf("INQUIRY successful for standard data.\n");
	inq = scsi_datain_unmarshall(task);
	if (inq == NULL) {
		printf("failed to unmarshall inquiry datain blob\n");
		exit(10);
	}

	printf("Device Type is %d. VendorId:%s ProductId:%s\n", inq->periperal_device_type, inq->vendor_identification, inq->product_identification);
	printf("Send MODESENSE6\n");
	if (iscsi_modesense6_async(iscsi, clnt->lun, modesense6_cb, 0, SCSI_MODESENSE_PC_CURRENT, SCSI_MODESENSE_PAGECODE_RETURN_ALL_PAGES, 0, 4, private_data) != 0) {
		printf("failed to send modesense6 command\n");
		exit(10);
	}

}

void testunitready_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct scsi_task *task = command_data;

	if (status == ISCSI_STATUS_CHECK_CONDITION) {
		printf("First testunitready failed with sense key:%d ascq:%04x\n", task->sense.key, task->sense.ascq);
		if (task->sense.key == SCSI_SENSE_KEY_UNIT_ATTENTION && task->sense.ascq == SCSI_SENSE_ASCQ_BUS_RESET) {
			printf("target device just came online, try again\n");

			if (iscsi_testunitready_async(iscsi, clnt->lun, testunitready_cb, private_data) != 0) {
				printf("failed to send testunitready command\n");
				exit(10);
			}
		}
		return;
	}

	printf("TESTUNITREADY successful, do an inquiry on lun:%d\n", clnt->lun);
	if (iscsi_inquiry_async(iscsi, clnt->lun, inquiry_cb, 0, 0, 64, private_data) != 0) {
		printf("failed to send inquiry command\n");
		exit(10);
	}
}


void reportluns_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct scsi_task *task = command_data;
	struct scsi_reportluns_list *list;
	uint32_t full_report_size;
	int i;

	if (status != ISCSI_STATUS_GOOD) {
		printf("Reportluns failed with unknown status code :%d\n", status);
		return;
	}

	full_report_size = scsi_datain_getfullsize(task);

	printf("REPORTLUNS status:%d   data size:%d,   full reports luns data size:%d\n", status, task->datain.size, full_report_size);
	if (full_report_size > task->datain.size) {
		printf("We did not get all the data we need in reportluns, ask again\n");
		if (iscsi_reportluns_async(iscsi, reportluns_cb, 0, full_report_size, private_data) != 0) {
			printf("failed to send reportluns command\n");
			exit(10);
		}
		return;
	}

	
	list = scsi_datain_unmarshall(task);
	if (list == NULL) {
		printf("failed to unmarshall reportluns datain blob\n");
		exit(10);
	}
	for (i=0; i < list->num; i++) {
		printf("LUN:%d found\n", list->luns[i]);
		clnt->lun = list->luns[i];
	}

	printf("Will use LUN:%d\n", clnt->lun);
	printf("Send testunitready to lun %d\n", clnt->lun);
	if (iscsi_testunitready_async(iscsi, clnt->lun, testunitready_cb, private_data) != 0) {
		printf("failed to send testunitready command\n");
		exit(10);
	}
}


void normallogin_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	if (status != 0) {
		printf("Failed to log in to target. status :0x%04x\n", status);
		exit(10);
	}

	printf("Logged in normal session, send reportluns\n");
	if (iscsi_reportluns_async(iscsi, reportluns_cb, 0, 16, private_data) != 0) {
		printf("failed to send reportluns command\n");
		exit(10);
	}
}


void normalconnect_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	printf("Connected to iscsi socket\n");

	if (status != 0) {
		printf("normalconnect_cb: connection  failed status:%d\n", status);
		exit(10);
	}

	printf("connected, send login command\n");
	iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);
	if (iscsi_login_async(iscsi, normallogin_cb, private_data) != 0) {
		printf("iscsi_login_async failed\n");
		exit(10);
	}
}



void discoverylogout_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	
	printf("discovery session logged out, Message from main() was:[%s]\n", clnt->message);

	printf("disconnect socket\n");
	if (iscsi_disconnect(iscsi) != 0) {
		printf("Failed to disconnect old socket\n");
		exit(10);
	}

	printf("reconnect with normal login to [%s]\n", clnt->target_address);
	printf("Use targetname [%s] when connecting\n", clnt->target_name);
	if (iscsi_set_targetname(iscsi, clnt->target_name)) {
		printf("Failed to set target name\n");
		exit(10);
	}
	if (iscsi_set_alias(iscsi, "ronnie") != 0) {
		printf("Failed to add alias\n");
		exit(10);
	}
	if (iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL) != 0) {
		printf("Failed to set settion type to normal\n");
		exit(10);
	}

	if (iscsi_connect_async(iscsi, clnt->target_address, normalconnect_cb, clnt) != 0) {
		printf("iscsi_connect failed\n");
		exit(10);
	}
}

void discovery_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct iscsi_discovery_address *addr;

	printf("discovery callback   status:%04x\n", status);
	for(addr=command_data; addr; addr=addr->next) {	
		printf("Target:%s Address:%s\n", addr->target_name, addr->target_address);
	}

	addr=command_data;
	clnt->has_discovered_target = 1;
	clnt->target_name    = strdup(addr->target_name);
	clnt->target_address = strdup(addr->target_address);


	printf("discovery complete, send logout command\n");

	if (iscsi_logout_async(iscsi, discoverylogout_cb, private_data) != 0) {
		printf("iscsi_logout_async failed\n");
		exit(10);
	}
}


void discoverylogin_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	if (status != 0) {
		printf("Failed to log in to target. status :0x%04x\n", status);
		exit(10);
	}

	printf("Logged in to target, send discovery command\n");
	if (iscsi_discovery_async(iscsi, discovery_cb, private_data) != 0) {
		printf("failed to send discovery command\n");
		exit(10);
	}

}

void discoveryconnect_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	printf("Connected to iscsi socket status:0x%08x\n", status);

	if (status != 0) {
		printf("discoveryconnect_cb: connection  failed status:%d\n", status);
		exit(10);
	}

	printf("connected, send login command\n");
	iscsi_set_session_type(iscsi, ISCSI_SESSION_DISCOVERY);
	if (iscsi_login_async(iscsi, discoverylogin_cb, private_data) != 0) {
		printf("iscsi_login_async failed\n");
		exit(10);
	}
}


int main(int argc, char *argv[])
{
	struct iscsi_context *iscsi;
	struct pollfd pfd;
	struct client_state clnt;

	printf("iscsi client\n");

	iscsi = iscsi_create_context("iqn.2002-10.com.ronnie:client");
	if (iscsi == NULL) {
		printf("Failed to create context\n");
		exit(10);
	}

	if (iscsi_set_alias(iscsi, "ronnie") != 0) {
		printf("Failed to add alias\n");
		exit(10);
	}

	clnt.message = "Hello iSCSI";
	clnt.has_discovered_target = 0;
	if (iscsi_connect_async(iscsi, TARGET, discoveryconnect_cb, &clnt) != 0) {
		printf("iscsi_connect failed\n");
		exit(10);
	}

	for (;;) {
		pfd.fd = iscsi_get_fd(iscsi);
		pfd.events = iscsi_which_events(iscsi);

		if (poll(&pfd, 1, -1) < 0) {
			printf("Poll failed");
			exit(10);
		}
		if (iscsi_service(iscsi, pfd.revents) < 0) {
			printf("iscsi_service failed\n");
			break;
		}
	}

printf("STOP\n");
exit(10);

	printf("ok\n");
	return 0;
}

